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

#include <stdio.h>
#include <stdlib.h>

#include "shared/entity.h"
#include "shared/client.h"

#include "g_main.h"
#include "g_wasm.h"

#if defined(_WIN32)
static inline int32_t strlcpy(char *const dest, const char *src, const size_t len)
{
	return snprintf(dest, len, "%s", src);
}
#endif

game_import_t gi;
game_export_t globals;
wasm_env_t wasm;
static char base_directory[260];
static size_t base_directory_len;
static char save_directory[260];
static size_t save_directory_len;
static int32_t max_clients;

static bool wasm_attempt_assembly_load(const char *file)
{
	char path[64];
	snprintf(path, sizeof(path), "%s/%s", base_directory, file);

	FILE *fp = fopen(path, "rb+");

	if (fp == NULL)
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

#ifndef max
#define max(a, b) \
	(a) > (b) ? (a) : (b)
#endif

static void post_sync_entities(void)
{
	const int32_t wasm_num = wasm_num_edicts();

	const int32_t num_sync = max(globals.num_edicts, wasm_num);

	globals.num_edicts = wasm_num;

	for (int32_t i = 0; i < num_sync; i++)
		sync_entity(entity_number_to_wnp(i), entity_number_to_np(i), false);
}

static void wasm_fetch_edict_base(void)
{
	uint32_t args[] = { 0 };

	wasm_call_args(wasm.WASM_GetEdicts, args, 0);
	wasm.edicts = args[0];

	// Store edict end space, we use it for validation later
	wasm.edict_end = wasm.edicts + (wasm.edict_size * wasm.max_edicts);

	// Validate the entity space
	if (!wasm_runtime_validate_app_addr(wasm.module_inst, wasm.edicts, wasm.edict_end - wasm.edicts))
		wasm_error("InitWASMAPI returned invalid memory");
}

#ifdef _WIN32
#define realpath(a, b) \
	_fullpath(a, b, sizeof(b))
#endif

static void InitializeDirectories(void)
{
#ifdef KMQUAKE2_ENGINE_MOD
	snprintf(base_directory, sizeof(base_directory), "%s", gi.FS_GameDir());

	snprintf(save_directory, sizeof(save_directory), "%s", gi.FS_SaveGameDir());
#else
	cvar_t *game_cvar = gi.cvar("game", "", 0);
	char mod_directory[64];

	if (game_cvar->string[0])
		snprintf(mod_directory, sizeof(mod_directory), "%s", game_cvar->string);
	else
		snprintf(mod_directory, sizeof(mod_directory), "baseq2");

	snprintf(base_directory, sizeof(base_directory), "%s/%s", gi.cvar("basedir", ".", 0)->string, mod_directory);

	snprintf(save_directory, sizeof(save_directory), "%s", base_directory);
#endif
	
	realpath(base_directory, base_directory);
	base_directory_len = strlen(base_directory);

	realpath(save_directory, save_directory);
	save_directory_len = strlen(save_directory);

	// swap \\ for / in case of Winders
	for (char *c = base_directory; *c; c++)
		if (*c == '\\')
			*c = '/';
	for (char *c = save_directory; *c; c++)
		if (*c == '\\')
			*c = '/';
}

static void NormalizeSavePath(const char *input_path, char *path, size_t path_size)
{
	snprintf(path, path_size, ".saves/%s", input_path + save_directory_len + 1);
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
	cvar_t *sys_wasmstacksize = gi.cvar("sys_wasmstacksize", "8388608", CVAR_LATCH);
	cvar_t *sys_wasmheapsize = gi.cvar("sys_wasmstacksize", "100663296", CVAR_LATCH);

	InitializeDirectories();

	/* initialize the wasm runtime by default configurations */
	wasm_runtime_init();

	if (!RegisterApiNatives())
		wasm_error("Unable to initialize API natives");

	/* read WASM file into a memory buffer */
	if (!wasm_attempt_assembly_load("game.aot") &&
		!wasm_attempt_assembly_load("game.wasm"))
		wasm_error("Unable to load game.aot or game.wasm");

	static const char *dir_list[2];
	dir_list[0] = gi.cvar("game", "", 0)->string;
	dir_list[1] = ".saves";

	static const char *map_dir_list[2];
	map_dir_list[0] = base_directory;
	map_dir_list[1] = save_directory;

	wasm_runtime_set_wasi_args(wasm.wasm_module, map_dir_list, lengthof(map_dir_list), dir_list, lengthof(dir_list), NULL, 0, NULL, 0);

	/* create an instance of the WASM module (WASM linear memory is ready) */
	wasm.module_inst = wasm_runtime_instantiate(wasm.wasm_module, 1024 * 8, (uint32_t) sys_wasmheapsize->value, wasm.error_buf, sizeof(wasm.error_buf));

	if (!wasm.module_inst)
		wasm_error(wasm.error_buf);

	wasm.exec_env = wasm_runtime_create_exec_env(wasm.module_inst, (uint32_t) sys_wasmstacksize->value);

	if (!wasm.exec_env)
		wasm_error(wasm_runtime_get_exception(wasm.module_inst));
	
	wasm_function_inst_t start_func = wasm_runtime_lookup_function(wasm.module_inst, "_initialize", NULL);

	if (!start_func)
		wasm_error("Unable to find _initialize function");

	wasm_call(start_func);

#define LOAD_FUNC(name, sig) \
	wasm.WASM_ ## name = wasm_runtime_lookup_function(wasm.module_inst, #name, sig)

	LOAD_FUNC(GetEdicts, "()i");
	LOAD_FUNC(GetEdictSize, "()i");
	LOAD_FUNC(GetNumEdicts, "()i");
	LOAD_FUNC(GetMaxEdicts, "()i");
	LOAD_FUNC(PmoveTrace, "(*ffffffffffff*)");
	LOAD_FUNC(PmovePointContents, "(*fff)");
	LOAD_FUNC(GetGameAPI, NULL);
	LOAD_FUNC(Init, NULL);
	LOAD_FUNC(SpawnEntities, "($$$)");
	LOAD_FUNC(ClientConnect, "(*$)i");
	LOAD_FUNC(ClientBegin, "(*)");
	LOAD_FUNC(ClientUserinfoChanged, "(*$)");
	LOAD_FUNC(ClientCommand, "(*)");
	LOAD_FUNC(ClientDisconnect, "(*)");
	LOAD_FUNC(ClientThink, "(**)");
	LOAD_FUNC(RunFrame, NULL);
	LOAD_FUNC(ServerCommand, NULL);
	LOAD_FUNC(WriteGame, "($i)");
	LOAD_FUNC(ReadGame, "($)");
	LOAD_FUNC(WriteLevel, "($)");
	LOAD_FUNC(ReadLevel, "($)");

	// allocate buffer data we use for transferring data over to WASM
	wasm.buffers_addr = wasm_runtime_module_malloc(wasm.module_inst, sizeof(wasm_buffers_t), NULL);

	if (!wasm.buffers_addr)
		wasm_error("Unable to allocate WASM buffers memory");

	uint32_t args[1] = {
		GAME_API_EXTENDED_VERSION
	};

	wasm_call_args(wasm.WASM_GetGameAPI, args, lengthof(args));

	wasm_call_args(wasm.WASM_GetEdictSize, args, 0);
	wasm.edict_size = args[0];

	wasm_call(wasm.WASM_Init);
	
	wasm_call_args(wasm.WASM_GetMaxEdicts, args, 0);
	wasm.max_edicts = args[0];

	wasm_call_args(wasm.WASM_GetNumEdicts, args, 0);
	wasm.num_edicts = args[0];

	if (!wasm_validate_addr(wasm.num_edicts, sizeof(int32_t)) ||
		!wasm.edict_size || !wasm.max_edicts)
		wasm_error("InitWASMAPI returned invalid memory");

	// Check to be sure these are set properly.
	if (!wasm.edict_size || !wasm.max_edicts)
		wasm_error("WASM_Init set invalid edict_size/max_edicts");

	wasm_fetch_edict_base();

	// force enhanced savegames on for q2pro.
#define GMF_ENHANCED_SAVEGAMES      0x00000400

	char new_features[64];
	cvar_t *g_features = gi.cvar("g_features", "0", 0);
	snprintf(new_features, sizeof(new_features), "%i", (int32_t)g_features->value | GMF_ENHANCED_SAVEGAMES);
	gi.cvar_forceset("g_features", new_features);

	wasm.g_features = (int32_t)g_features->value;

	post_sync_entities();
}

#ifndef min
#define min(a, b) \
	(a) < (b) ? (a) : (b)
#endif

// copies all of the command buffer data into WASM heap
static void setup_args()
{
	wasm_buffers_t *buffers = wasm_buffers();

	const int32_t n = min((sizeof(buffers->cmds) / sizeof(*buffers->cmds)) - 1, (uint32_t) gi.argc());
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

	gi.FreeTags(TAG_LEVEL);
	
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

	wasm_call_args(wasm.WASM_SpawnEntities, args, lengthof(args));

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

	wasm_call_args(wasm.WASM_ClientConnect, args, lengthof(args));

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

	wasm_call_args(wasm.WASM_ClientBegin, args, lengthof(args));

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

	wasm_call_args(wasm.WASM_ClientUserinfoChanged, args, lengthof(args));

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

	wasm_call_args(wasm.WASM_ClientDisconnect, args, lengthof(args));

	post_sync_entities();
}

static void ClientCommand(edict_t *e)
{
	q2_wasm_update_cvars();

	setup_args();

	uint32_t edict_offset = entity_np_to_wa(e);

	uint32_t args[] = {
		edict_offset
	};
	
	pre_sync_entities();

	wasm_call_args(wasm.WASM_ClientCommand, args, lengthof(args));

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

	wasm_call_args(wasm.WASM_ClientThink, args, lengthof(args));

	post_sync_entities();
}

static void RunFrame(void)
{
	q2_wasm_update_cvars();

	pre_sync_entities();

	wasm_call(wasm.WASM_RunFrame);

	post_sync_entities();
}

static void ServerCommand(void)
{
	q2_wasm_update_cvars();

	setup_args();

	pre_sync_entities();

	wasm_call(wasm.WASM_ServerCommand);

	post_sync_entities();
}

static void WriteGame(const char *filename, qboolean autosave)
{
	wasm_buffers_t *buffers = wasm_buffers();

	NormalizeSavePath(filename, buffers->filename, sizeof(buffers->filename));

	uint32_t args[] = {
		WASM_BUFFERS_OFFSET(filename),
		(uint32_t) autosave
	};

	wasm_call_args(wasm.WASM_WriteGame, args, lengthof(args));
}

static void ReadGame(const char *filename)
{
	wasm_buffers_t *buffers = wasm_buffers();

	NormalizeSavePath(filename, buffers->filename, sizeof(buffers->filename));

	uint32_t args[] = {
		WASM_BUFFERS_OFFSET(filename)
	};

	wasm_call_args(wasm.WASM_ReadGame, args, lengthof(args));

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
	bool backup[MAX_CLIENTS];
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
			backup[i] = e->inuse;
			e->inuse = qfalse;
		}
	}

	wasm_buffers_t *buffers = wasm_buffers();
	
	NormalizeSavePath(filename, buffers->filename, sizeof(buffers->filename));

	uint32_t args[] = {
		WASM_BUFFERS_OFFSET(filename)
	};

	wasm_call_args(wasm.WASM_WriteLevel, args, lengthof(args));
	
	if (is_autosave)
	{
		for (int32_t i = 0; i < max_clients; i++)
		{
			wasm_edict_t *e = entity_number_to_wnp(i + 1);
			e->inuse = backup[i] ? qtrue : qfalse;
		}
	}
}

static void ReadLevel(const char *filename)
{
	wasm_buffers_t *buffers = wasm_buffers();
	
	NormalizeSavePath(filename, buffers->filename, sizeof(buffers->filename));

	uint32_t args[] = {
		WASM_BUFFERS_OFFSET(filename)
	};

	wasm_call_args(wasm.WASM_ReadLevel, args, lengthof(args));

	post_sync_entities();
}

/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;

	const int32_t max_edicts = (int32_t)gi.cvar("maxentities", "1024", CVAR_LATCH)->value;
	max_clients = (int32_t)gi.cvar("maxclients", "8", CVAR_LATCH)->value;

	globals.apiversion = 3;

	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;

	globals.SpawnEntities = SpawnEntities;

	globals.WriteGame = WriteGame;
	globals.ReadGame = ReadGame;

	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;
	
	globals.ClientConnect = ClientConnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientCommand = ClientCommand;
	globals.ClientThink = ClientThink;

	globals.RunFrame = RunFrame;

	globals.ServerCommand = ServerCommand;
		
	globals.edicts = (edict_t *)gi.TagMalloc(sizeof(edict_t) * max_edicts, TAG_GAME);
	globals.edict_size = sizeof(edict_t);
	globals.num_edicts = max_clients + 1;
	globals.max_edicts = max_edicts;

	return &globals;
}
