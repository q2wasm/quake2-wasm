/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

extern "C"
{
#include "shared/shared.h"

#include "g_main.h"
#include "g_save.h"
}

#include "g_wasm.hh"

game_import_t gi;
game_export_t globals;
wasm_env_t wasm;

/*
============
InitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
static void InitGame(void)
{
	const uint32_t stack_size = (1024 * 1024) * 2, heap_size = (1024 * 1024) * 8;

	/* read WASM file into a memory buffer */
	FILE *fp = fopen("wasm/game.aot", "rb+");

	if (fp == nullptr)
		fp = fopen("wasm/game.wasm", "rb+");
	
	fseek(fp, 0, SEEK_END);
	uint32_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	wasm.assembly = (uint8_t *) gi.TagMalloc(size, TAG_GAME);
	fread(wasm.assembly, sizeof(uint8_t), size, fp);
	fclose(fp);

	/* initialize the wasm runtime by default configurations */
	wasm_runtime_init();

	if (!RegisterWasiNatives())
		gi.error("Unable to initialize WASI natives");

	if (!RegisterApiNatives())
		gi.error("Unable to initialize API natives");

	/* parse the WASM file from buffer and create a WASM module */
	wasm.wasm_module = wasm_runtime_load(wasm.assembly, size, wasm.error_buf, sizeof(wasm.error_buf));

	if (!wasm.wasm_module)
		gi.error("Unable to load WASM runtime from file: %s\n", wasm.error_buf);

	/* create an instance of the WASM module (WASM linear memory is ready) */
	wasm.module_inst = wasm_runtime_instantiate(wasm.wasm_module, stack_size, heap_size, wasm.error_buf, sizeof(wasm.error_buf));

	if (!wasm.module_inst)
		gi.error("Unable to instantiate WASM module: %s\n", wasm.error_buf);

	wasm.exec_env = wasm_runtime_create_exec_env(wasm.module_inst, stack_size);

	if (!wasm.exec_env)
		gi.error("Unable to create WASM execution environment: %s\n", wasm_runtime_get_exception(wasm.module_inst));

#define LOAD_FUNC(name, sig) \
	wasm.name = wasm_runtime_lookup_function(wasm.module_inst, #name, sig)

	LOAD_FUNC(WASM_PmoveTrace, "(******)");
	LOAD_FUNC(WASM_PmovePointContents, "(**)");
	LOAD_FUNC(InitWASMAPI, "()*");
	LOAD_FUNC(WASM_Init, nullptr);
	LOAD_FUNC(WASM_SpawnEntities, "($$$)");
	LOAD_FUNC(WASM_ClientConnect, "(*$)i");
	LOAD_FUNC(WASM_ClientBegin, "(*)");
	LOAD_FUNC(WASM_ClientUserinfoChanged, "(*$)");
	LOAD_FUNC(WASM_ClientCommand, "(*)");
	LOAD_FUNC(WASM_ClientDisconnect, "(*)");
	LOAD_FUNC(WASM_ClientThink, "(**)");
	LOAD_FUNC(WASM_RunFrame, nullptr);
	LOAD_FUNC(WASM_ServerCommand, nullptr);

	wasm_function_inst_t start_func = wasm_runtime_lookup_function(wasm.module_inst, "_start", NULL);

	wasm_call(start_func);

	uint32_t args[1];

	wasm_call(wasm.InitWASMAPI, args, 0);

	// fetch the returned pointer
	void *api_ptr = wasm_runtime_addr_app_to_native(wasm.module_inst, args[0]);

	wasm_call(wasm.WASM_Init);

	if (!wasm_runtime_validate_native_addr(wasm.module_inst, api_ptr, sizeof(int32_t) * 4))
		gi.error("InitWASMAPI returned invalid memory\n");

	wasm.edict_base = ((uint32_t *)api_ptr)[0];
	wasm.edict_size = ((uint32_t *)api_ptr)[1];
	wasm.num_edicts = &((int32_t *)api_ptr)[2];
	wasm.max_edicts = ((uint32_t *)api_ptr)[3];
	wasm.null_ptr = wasm_addr_to_native(0);
	wasm.null_offset = 0;
	
	wasm.ucmd_ptr = wasm_runtime_module_malloc(wasm.module_inst, sizeof(usercmd_t), (void **) &wasm.ucmd_buf);
	wasm.userinfo_ptr = wasm_runtime_module_malloc(wasm.module_inst, MAX_INFO_STRING + 1, (void **) &wasm.userinfo_buf);
	wasm.vectors_ptr = wasm_runtime_module_malloc(wasm.module_inst, sizeof(vec3_t) * 4, (void **) &wasm.vectors_buf);
	wasm.trace_ptr = wasm_runtime_module_malloc(wasm.module_inst, sizeof(wasm_trace_t), (void **) &wasm.trace_buf);

	for (int32_t i = 0; i < sizeof(wasm.cmd_bufs) / sizeof(*wasm.cmd_bufs); i++)
		wasm.cmd_ptrs[i] = wasm_runtime_module_malloc(wasm.module_inst, MAX_INFO_STRING / 8, (void **) &wasm.cmd_bufs[i]);
	wasm.scmd_ptr = wasm_runtime_module_malloc(wasm.module_inst, MAX_INFO_STRING, (void **) &wasm.scmd_buf);
}

// copies all of the command buffer data into WASM heap
static void setup_args()
{
	const int32_t n = gi.argc();

	for (int32_t i = 0; i < n; i++)
		Q_strlcpy(wasm.cmd_bufs[i], gi.argv(i), MAX_INFO_STRING / 8);
	Q_strlcpy(wasm.scmd_buf, gi.args(), MAX_INFO_STRING);
}

static void ShutdownGame(void)
{
	wasm_runtime_destroy_exec_env(wasm.exec_env);

	wasm_runtime_deinstantiate(wasm.module_inst);

	wasm_runtime_destroy();

	gi.FreeTags(TAG_GAME);
	gi.FreeTags(TAG_LEVEL);
}

static void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint)
{
	// FIXME: free on next map change
	uint32_t mapname_str = wasm_dup_str(mapname);
	uint32_t entities_str = wasm_dup_str(entities);
	uint32_t spawnpoint_str = wasm_dup_str(spawnpoint);

	uint32_t args[] = {
		mapname_str,
		entities_str,
		spawnpoint_str
	};

	wasm_call(wasm.WASM_SpawnEntities, args);
}

static qboolean ClientConnect(edict_t *e, char *userinfo)
{
	Q_strlcpy(wasm.userinfo_buf, userinfo, MAX_INFO_STRING * 2);

	uint32_t edict_offset = entity_np_to_wa(e);
	wasm_edict_t *wasm_edict = entity_wa_to_wnp(edict_offset);

	wasm_edict->s.number = e->s.number;

	uint32_t args[] = {
		edict_offset,
		wasm.userinfo_ptr
	};

	wasm_call(wasm.WASM_ClientConnect, args);

	Q_strlcpy(userinfo, wasm.userinfo_buf, MAX_INFO_STRING);

	sync_entity(wasm_edict, e);

	return (qboolean) args[0];
}

static void ClientBegin(edict_t *e)
{
	uint32_t edict_offset = entity_np_to_wa(e);
	wasm_edict_t *wasm_edict = entity_wa_to_wnp(edict_offset);

	wasm_edict->s.number = e->s.number;

	uint32_t args[] = {
		edict_offset
	};

	wasm_call(wasm.WASM_ClientBegin, args);

	sync_entity(wasm_edict, e);
}

static void ClientUserinfoChanged(edict_t *e, char *userinfo)
{
	Q_strlcpy(wasm.userinfo_buf, userinfo, MAX_INFO_STRING * 2);

	uint32_t edict_offset = entity_np_to_wa(e);
	wasm_edict_t *wasm_edict = entity_wa_to_wnp(edict_offset);

	wasm_edict->s.number = e->s.number;

	uint32_t args[] = {
		edict_offset,
		wasm.userinfo_ptr
	};

	wasm_call(wasm.WASM_ClientUserinfoChanged, args);

	Q_strlcpy(userinfo, wasm.userinfo_buf, MAX_INFO_STRING);

	sync_entity(wasm_edict, e);
}

static void ClientDisconnect(edict_t *e)
{
	uint32_t edict_offset = entity_np_to_wa(e);
	wasm_edict_t *wasm_edict = entity_wa_to_wnp(edict_offset);

	wasm_edict->s.number = e->s.number;

	uint32_t args[] = {
		edict_offset
	};

	wasm_call(wasm.WASM_ClientDisconnect, args);

	sync_entity(wasm_edict, e);
}

static void ClientCommand(edict_t *e)
{
	setup_args();

	uint32_t edict_offset = entity_np_to_wa(e);
	wasm_edict_t *wasm_edict = entity_wa_to_wnp(edict_offset);

	wasm_edict->s.number = e->s.number;

	uint32_t args[] = {
		edict_offset
	};

	wasm_call(wasm.WASM_ClientCommand, args);
}

static void ClientThink(edict_t *e, usercmd_t *ucmd)
{
	uint32_t edict_offset = entity_np_to_wa(e);
	wasm_edict_t *wasm_edict = entity_wa_to_wnp(edict_offset);

	wasm_edict->s.number = e->s.number;

	memcpy(wasm.ucmd_buf, ucmd, sizeof(usercmd_t));

	uint32_t args[] = {
		edict_offset,
		wasm.ucmd_ptr
	};

	wasm_call(wasm.WASM_ClientThink, args);

	sync_entity(wasm_edict, e);
}

#include <chrono>

static std::chrono::high_resolution_clock hc;

static void RunFrame(void)
{
	wasm_call(wasm.WASM_RunFrame);

	globals.num_edicts = *wasm.num_edicts;

	for (int32_t i = 0; i < globals.num_edicts; i++)
	{
		wasm_edict_t *e = entity_number_to_wnp(i);
		edict_t *n = &globals.edicts[i];

		sync_entity(e, n);
		// events only last one frame.
		// FIXME: I tried syncing these at the start of RunFrame to no avail.
		// What gives? This should be equivalent, but.. it's weird.
		e->s.event = 0;
	}
}

static void ServerCommand(void)
{
	setup_args();
	wasm_call(wasm.WASM_ServerCommand);
}

/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
extern "C" game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;

	const int32_t max_edicts = (int32_t)gi.cvar("maxentities", "1024", CVAR_LATCH)->value;

	globals = {
		.apiversion = 3,

		.Init = InitGame,
		.Shutdown = ShutdownGame,

		.SpawnEntities = SpawnEntities,

		.WriteGame = WriteGame,
		.ReadGame = ReadGame,

		.WriteLevel = WriteLevel,
		.ReadLevel = ReadLevel,
	
		.ClientConnect = ClientConnect,
		.ClientBegin = ClientBegin,
		.ClientUserinfoChanged = ClientUserinfoChanged,
		.ClientDisconnect = ClientDisconnect,
		.ClientCommand = ClientCommand,
		.ClientThink = ClientThink,

		.RunFrame = RunFrame,

		.ServerCommand = ServerCommand,
		
		.edicts = (edict_t *)gi.TagMalloc(sizeof(edict_t) * max_edicts, TAG_GAME),
		.edict_size = sizeof(edict_t),
		.num_edicts = (int32_t)gi.cvar("maxclients", "8", CVAR_LATCH)->value + 1,
		.max_edicts = max_edicts
	};

	return &globals;
}
