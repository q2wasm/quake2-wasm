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
#include <stdio.h>

#include "shared/entity.h"
#include "shared/client.h"

#include "g_main.h"
}

#include "g_wasm.hh"

#include <algorithm>

#if defined(_WIN32)
inline int32_t strlcpy(char *const dest, const char *src, const size_t len)
{
	return snprintf(dest, len, "%s", src);
}
#endif

game_import_t gi;
game_export_t globals;
wasm_env_t wasm;
char mod_directory[64];
char base_directory[260];
static int32_t max_clients;

static bool wasm_attempt_assembly_load(const char *file)
{
	char path[64];
	snprintf(path, sizeof(path), "%s/%s", mod_directory, file);

	FILE *fp = fopen(path, "rb+");

	if (fp == nullptr)
	{
		gi.dprintf("Couldn't load %s: file does not exist\n", file);
		return false;
	}
	
	fseek(fp, 0, SEEK_END);
	uint32_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	wasm.assembly = (uint8_t *) gi.TagMalloc(size, TAG_GAME);
	fread(wasm.assembly, sizeof(uint8_t), size, fp);
	fclose(fp);

	/* parse the WASM file from buffer and create a WASM module */
	wasm.wasm_module = wasm_runtime_load(wasm.assembly, size, wasm.error_buf, sizeof(wasm.error_buf));

	if (!wasm.wasm_module)
	{
		gi.TagFree(wasm.assembly);
		return false;
	}

	return true;
}

static void pre_sync_entities(void)
{
	for (int32_t i = 0; i < globals.num_edicts; i++)
		copy_frame_native_to_wasm(entity_number_to_wnp(i), entity_number_to_np(i));
}

static void post_sync_entities(void)
{
	const int32_t wasm_num = wasm_num_edicts();

	const int32_t num_sync = std::max(globals.num_edicts, wasm_num);

	globals.num_edicts = wasm_num;

	for (int32_t i = 0; i < globals.num_edicts; i++)
		sync_entity(entity_number_to_wnp(i), entity_number_to_np(i), false);
}

static void wasm_fetch_edict_base(void)
{
	uint32_t args[] = { 0 };

	wasm_call(wasm.WASM_GetEdicts, args, 0);
	wasm.edicts = args[0];

	// Store edict end space, we use it for validation later
	wasm.edict_end = wasm.edicts + (wasm.edict_size * wasm.max_edicts);

	// Validate the entity space
	if (!wasm_runtime_validate_app_addr(wasm.module_inst, wasm.edicts, wasm.edict_end - wasm.edicts))
		wasm_error("InitWASMAPI returned invalid memory");
}

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
	const uint32_t stack_size = 8388608, heap_size = 33554432 * 3;

	cvar_t *game_cvar = gi.cvar("game", "", 0);

	if (game_cvar->string[0])
		snprintf(mod_directory, sizeof(mod_directory), "%s", game_cvar->string);
	else
		snprintf(mod_directory, sizeof(mod_directory), "baseq2");

	snprintf(base_directory, sizeof(base_directory), "%s", gi.cvar("basedir", ".", 0)->string);

	/* initialize the wasm runtime by default configurations */
	wasm_runtime_init();

	if (!RegisterApiNatives())
		wasm_error("Unable to initialize API natives");

	/* read WASM file into a memory buffer */
	if (!wasm_attempt_assembly_load("game.aot") &&
		!wasm_attempt_assembly_load("game.wasm"))
		wasm_error("Unable to load game.aot or game.wasm");

	static const char *dir_list[] = {
		mod_directory
	};

	wasm_runtime_set_wasi_args(wasm.wasm_module, dir_list, sizeof(dir_list) / sizeof(*dir_list), NULL, 0, NULL, 0, NULL, 0);

	/* create an instance of the WASM module (WASM linear memory is ready) */
	wasm.module_inst = wasm_runtime_instantiate(wasm.wasm_module, 1024 * 8, heap_size, wasm.error_buf, sizeof(wasm.error_buf));

	if (!wasm.module_inst)
		wasm_error(wasm.error_buf);

	wasm.exec_env = wasm_runtime_create_exec_env(wasm.module_inst, stack_size);

	if (!wasm.exec_env)
		wasm_error(wasm_runtime_get_exception(wasm.module_inst));
	
	wasm_function_inst_t start_func = wasm_runtime_lookup_function(wasm.module_inst, "_initialize", NULL);

	if (!start_func)
		wasm_error("Unable to find _initialize function");

	wasm_call(start_func);

#define LOAD_FUNC(name, sig) \
	wasm.name = wasm_runtime_lookup_function(wasm.module_inst, #name, sig)

	LOAD_FUNC(WASM_GetAPIVersion, "()i");
	LOAD_FUNC(WASM_GetEdicts, "()i");
	LOAD_FUNC(WASM_GetEdictSize, "()i");
	LOAD_FUNC(WASM_GetNumEdicts, "()i");
	LOAD_FUNC(WASM_GetMaxEdicts, "()i");
	LOAD_FUNC(WASM_PmoveTrace, "(******)");
	LOAD_FUNC(WASM_PmovePointContents, "(**)");
	LOAD_FUNC(WASM_GetGameAPI, nullptr);
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
	LOAD_FUNC(WASM_WriteGame, "($i)");
	LOAD_FUNC(WASM_ReadGame, "($)");
	LOAD_FUNC(WASM_WriteLevel, "($)");
	LOAD_FUNC(WASM_ReadLevel, "($)");

	// allocate buffer data we use for transferring data over to WASM
	wasm.buffers_addr = wasm_runtime_module_malloc(wasm.module_inst, sizeof(wasm_buffers_t), NULL);

	if (!wasm.buffers_addr)
		wasm_error("Unable to allocate WASM buffers memory");

	uint32_t args[1] = {
		0
	};

	wasm_call(wasm.WASM_GetGameAPI);

	wasm_call(wasm.WASM_GetAPIVersion, args, 0);

	wasm_call(wasm.WASM_GetEdictSize, args, 0);
	wasm.edict_size = args[0];

	wasm_call(wasm.WASM_Init);
	
	wasm_call(wasm.WASM_GetMaxEdicts, args, 0);
	wasm.max_edicts = args[0];

	wasm_call(wasm.WASM_GetNumEdicts, args, 0);
	wasm.num_edicts = args[0];

	if (!wasm_validate_addr(wasm.num_edicts, sizeof(int32_t)) ||
		!wasm.edict_size || !wasm.max_edicts)
		wasm_error("InitWASMAPI returned invalid memory");

	// Check to be sure these are set properly.
	if (!wasm.edict_size || !wasm.max_edicts)
		wasm_error("WASM_Init set invalid edict_size/max_edicts");

	wasm_fetch_edict_base();

	// force enhanced savegames on for q2pro.
#define GMF_CLIENTNUM               0x00000001
#define GMF_PROPERINUSE             0x00000002
#define GMF_MVDSPEC                 0x00000004
#define GMF_WANT_ALL_DISCONNECTS    0x00000008

#define GMF_ENHANCED_SAVEGAMES      0x00000400
#define GMF_VARIABLE_FPS            0x00000800
#define GMF_EXTRA_USERINFO          0x00001000
#define GMF_IPV6_ADDRESS_AWARE      0x00002000

	char new_features[64];
	cvar_t *g_features = gi.cvar("g_features", "0", 0);
	snprintf(new_features, sizeof(new_features), "%i", (int32_t)g_features->value | GMF_ENHANCED_SAVEGAMES);
	gi.cvar_forceset("g_features", new_features);

	wasm.g_features = (int32_t)g_features->value;

	post_sync_entities();
}

// copies all of the command buffer data into WASM heap
static void setup_args()
{
	wasm_buffers_t *buffers = wasm_buffers();

	const int32_t n = std::min((sizeof(buffers->cmds) / sizeof(*buffers->cmds)) - 1, (uint32_t) gi.argc());
	int32_t i;

	for (i = 0; i < n; i++)
		strlcpy(buffers->cmds[i], gi.argv(i), sizeof(buffers->cmds[i]));
	buffers->cmds[i][0] = 0;
	strlcpy(buffers->scmd, gi.args(), sizeof(buffers->scmd));
}

static void ShutdownGame(void)
{
	if (wasm.exec_env)
		wasm_runtime_destroy_exec_env(wasm.exec_env);

	if (wasm.module_inst)
		wasm_runtime_deinstantiate(wasm.module_inst);

	if (wasm.wasm_module)
		wasm_runtime_unload(wasm.wasm_module);

	wasm_runtime_destroy();

	gi.FreeTags(TAG_GAME);
	gi.FreeTags(TAG_LEVEL);
}

static void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint)
{
	static uint32_t mapname_str, entities_str, spawnpoint_str;
	
	if (mapname_str)
	{
		q2_wasm_clear_surface_cache();

		wasm_runtime_module_free(wasm.module_inst, mapname_str);
		wasm_runtime_module_free(wasm.module_inst, entities_str);
		wasm_runtime_module_free(wasm.module_inst, spawnpoint_str);
	}

	uint32_t args[] = {
		mapname_str = wasm_dup_str(mapname),
		entities_str = wasm_dup_str(entities),
		spawnpoint_str = wasm_dup_str(spawnpoint)
	};

	wasm_call(wasm.WASM_SpawnEntities, args);

	post_sync_entities();
}

static qboolean ClientConnect(edict_t *e, char *userinfo)
{
	wasm_buffers_t *buffers = wasm_buffers();

	strlcpy(buffers->userinfo, userinfo, sizeof(buffers->userinfo));

	uint32_t edict_offset = entity_np_to_wa(e);

	uint32_t args[] = {
		edict_offset,
		WASM_BUFFERS_OFFSET(userinfo)
	};

	pre_sync_entities();

	wasm_call(wasm.WASM_ClientConnect, args);

	buffers = wasm_buffers();

	strlcpy(userinfo, buffers->userinfo, sizeof(buffers->userinfo));

	post_sync_entities();

	return (qboolean) args[0];
}

static void ClientBegin(edict_t *e)
{
	uint32_t edict_offset = entity_np_to_wa(e);

	uint32_t args[] = {
		edict_offset
	};

	pre_sync_entities();

	wasm_call(wasm.WASM_ClientBegin, args);

	post_sync_entities();
}

static void ClientUserinfoChanged(edict_t *e, char *userinfo)
{
	wasm_buffers_t *buffers = wasm_buffers();

	strlcpy(buffers->userinfo, userinfo, sizeof(buffers->userinfo));

	uint32_t edict_offset = entity_np_to_wa(e);

	uint32_t args[] = {
		edict_offset,
		WASM_BUFFERS_OFFSET(userinfo)
	};

	pre_sync_entities();

	wasm_call(wasm.WASM_ClientUserinfoChanged, args);

	buffers = wasm_buffers();

	strlcpy(userinfo, buffers->userinfo, sizeof(buffers->userinfo));

	post_sync_entities();
}

static void ClientDisconnect(edict_t *e)
{
	uint32_t edict_offset = entity_np_to_wa(e);

	uint32_t args[] = {
		edict_offset
	};
	
	pre_sync_entities();

	wasm_call(wasm.WASM_ClientDisconnect, args);

	post_sync_entities();
}

static void ClientCommand(edict_t *e)
{
	setup_args();

	uint32_t edict_offset = entity_np_to_wa(e);

	uint32_t args[] = {
		edict_offset
	};
	
	pre_sync_entities();

	wasm_call(wasm.WASM_ClientCommand, args);

	post_sync_entities();
}

static void ClientThink(edict_t *e, usercmd_t *ucmd)
{
	wasm_buffers_t *buffers = wasm_buffers();

	buffers->ucmd = *ucmd;
	
	pre_sync_entities();

	uint32_t edict_offset = entity_np_to_wa(e);

	uint32_t args[] = {
		edict_offset,
		WASM_BUFFERS_OFFSET(ucmd)
	};

	wasm_call(wasm.WASM_ClientThink, args);

	post_sync_entities();
}

static void RunFrame(void)
{
	pre_sync_entities();

	wasm_call(wasm.WASM_RunFrame);

	post_sync_entities();
}

static void ServerCommand(void)
{
	setup_args();

	pre_sync_entities();

	wasm_call(wasm.WASM_ServerCommand);

	post_sync_entities();
}

#include <filesystem>
namespace fs = std::filesystem;

std::string get_relative_file(const char *filename)
{
	fs::path path(filename);
	std::string pathname;

	// already relative to mod directory
	if (path.is_relative())
		pathname = filename;
	else
		pathname = path.lexically_relative(base_directory).string();

	for (auto n = pathname.find('\\', 0); n != std::string::npos; n = pathname.find('\\', n + 1))
		pathname[n] = '/';

	return pathname;
}

static void WriteGame(const char *filename, qboolean autosave)
{
	wasm_buffers_t *buffers = wasm_buffers();

	strlcpy(buffers->userinfo, get_relative_file(filename).c_str(), sizeof(buffers->userinfo));

	uint32_t args[] = {
		WASM_BUFFERS_OFFSET(filename),
		(uint32_t) autosave
	};

	wasm_call(wasm.WASM_WriteGame, args);
}

static void ReadGame(const char *filename)
{
	wasm_buffers_t *buffers = wasm_buffers();

	strlcpy(buffers->userinfo, get_relative_file(filename).c_str(), sizeof(buffers->userinfo));

	uint32_t args[] = {
		WASM_BUFFERS_OFFSET(filename)
	};

	wasm_call(wasm.WASM_ReadGame, args);

	wasm_fetch_edict_base();

	post_sync_entities();
}

static void WriteLevel(const char *filename)
{
	// autosaves have a weird setup where they clear inuse,
	// then restore them after writing the level. We have no way of
	// knowing here if this is an autosave or not, but we can infer
	// by checking if any of the native clients had their inuse set.
	// A /save command will have their inuse intact, but an autosave
	// will have all of them set to false.
	std::vector<bool> b;
	bool is_autosave = true;

	for (int32_t i = 0; i < max_clients; i++)
	{
		edict_t *e = entity_number_to_np(i + 1);

		if (e->inuse)
		{
			is_autosave = false;
			break;
		}
	}

	if (is_autosave)
	{
		for (int32_t i = 0; i < max_clients; i++)
		{
			wasm_edict_t *e = entity_number_to_wnp(i + 1);
			b.push_back(e->inuse);
			e->inuse = qfalse;
		}
	}

	wasm_buffers_t *buffers = wasm_buffers();

	strlcpy(buffers->filename, get_relative_file(filename).c_str(), sizeof(buffers->filename));

	uint32_t args[] = {
		WASM_BUFFERS_OFFSET(filename)
	};

	wasm_call(wasm.WASM_WriteLevel, args);
	
	if (is_autosave)
	{
		for (int32_t i = 0; i < max_clients; i++)
		{
			wasm_edict_t *e = entity_number_to_wnp(i + 1);
			e->inuse = b[i] ? qtrue : qfalse;
		}
	}
}

static void ReadLevel(const char *filename)
{
	wasm_buffers_t *buffers = wasm_buffers();

	strlcpy(buffers->filename, get_relative_file(filename).c_str(), sizeof(buffers->filename));

	uint32_t args[] = {
		WASM_BUFFERS_OFFSET(filename)
	};

	wasm_call(wasm.WASM_ReadLevel, args);

	post_sync_entities();
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
	max_clients = (int32_t)gi.cvar("maxclients", "8", CVAR_LATCH)->value;

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
		.num_edicts = max_clients + 1,
		.max_edicts = max_edicts
	};

	return &globals;
}
