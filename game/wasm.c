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

#ifdef __wasm__

#include <stdarg.h>
#include <stdio.h>
#include "api.h"

#define __WASM_EXPORT__(name) \
	__attribute__((export_name(#name)))
#define __WASM_IMPORT__(name) \
	__attribute__((import_module("q2"), import_name(#name)))

#define DECLARE_IMPORT(r, n, ...) \
	r wasm_ ## n(__VA_ARGS__) __WASM_IMPORT__(n)

DECLARE_IMPORT(void, bprint, print_level_t, const char *);
DECLARE_IMPORT(void, dprint, const char *);
DECLARE_IMPORT(void, cprint, edict_t *, print_level_t, const char *);
DECLARE_IMPORT(void, centerprint, edict_t *, const char *);
DECLARE_IMPORT(void, sound, edict_t *ent, sound_channel_t, int32_t, vec_t, sound_attn_t, vec_t);
DECLARE_IMPORT(void, positioned_sound, const vec3_t *, edict_t *, sound_channel_t, int32_t, vec_t, sound_attn_t, vec_t);
DECLARE_IMPORT(void, configstring, int32_t , const char *);

DECLARE_IMPORT(void, error, const char *);

DECLARE_IMPORT(int32_t, modelindex, const char *);
DECLARE_IMPORT(int32_t, soundindex, const char *);
DECLARE_IMPORT(int32_t, imageindex, const char *);

DECLARE_IMPORT(void, setmodel, edict_t *, const char *);

DECLARE_IMPORT(void, trace, const vec3_t *, const vec3_t *, const vec3_t *, const vec3_t *, edict_t *, content_flags_t, trace_t *);
DECLARE_IMPORT(content_flags_t, pointcontents, const vec3_t *);
DECLARE_IMPORT(qboolean, inPVS, const vec3_t *a, const vec3_t *);
DECLARE_IMPORT(qboolean, inPHS, const vec3_t *a, const vec3_t *);
DECLARE_IMPORT(void, SetAreaPortalState, int32_t, qboolean);
DECLARE_IMPORT(qboolean, AreasConnected, int32_t, int32_t);

DECLARE_IMPORT(void, linkentity, edict_t *);
DECLARE_IMPORT(void, unlinkentity, edict_t *);
DECLARE_IMPORT(int32_t, BoxEdicts, const vec3_t *, const vec3_t *, edict_t **, int32_t, box_edicts_area_t);
DECLARE_IMPORT(void, Pmove, pmove_t *pmove);

DECLARE_IMPORT(void, multicast, const vec3_t *, multicast_t);
DECLARE_IMPORT(void, unicast, edict_t *, qboolean);

DECLARE_IMPORT(void, WriteChar, int32_t);
DECLARE_IMPORT(void, WriteByte, int32_t);
DECLARE_IMPORT(void, WriteShort, int32_t);
DECLARE_IMPORT(void, WriteLong, int32_t);
DECLARE_IMPORT(void, WriteFloat, vec_t);
DECLARE_IMPORT(void, WriteString, const char *);
DECLARE_IMPORT(void, WritePosition, const vec3_t *);
DECLARE_IMPORT(void, WriteDir, const vec3_t *);
DECLARE_IMPORT(void, WriteAngle, vec_t);

DECLARE_IMPORT(void *, TagMalloc, uint32_t , uint32_t);
DECLARE_IMPORT(void, TagFree, void *);
DECLARE_IMPORT(void, FreeTags, uint32_t);

DECLARE_IMPORT(cvar_t *, cvar, const char *, const char *, cvar_flags_t);
DECLARE_IMPORT(cvar_t *, cvar_set, const char *, const char *);
DECLARE_IMPORT(cvar_t *, cvar_forceset, const char *, const char *);

DECLARE_IMPORT(int32_t, argc, void);
DECLARE_IMPORT(char *, argv, int32_t n);
DECLARE_IMPORT(char *, args, void);

DECLARE_IMPORT(void, AddCommandString, const char *);

DECLARE_IMPORT(void, DebugGraph, vec_t, int32_t);

static char	string[1024];

#define PARSE_VAR_ARGS \
	va_list argptr; \
	va_start (argptr, format); \
	vsnprintf (string, sizeof(string), format, argptr); \
	va_end (argptr)

static void wasm_wrap_bprintf(print_level_t printlevel, const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_bprint(printlevel, string);
}

static void wasm_wrap_dprintf(const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_dprint(string);
}

static void wasm_wrap_cprintf(edict_t *ent, print_level_t printlevel, const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_cprint(ent, printlevel, string);
}

static void wasm_wrap_centerprintf(edict_t *ent, const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_centerprint(ent, string);
}

static void wasm_wrap_error(const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_error(string);
}

static trace_t wasm_wrap_trace(const vec3_t *start, const vec3_t *mins, const vec3_t *maxs, const vec3_t *end, edict_t *passent, content_flags_t contentmask)
{
	static trace_t tr;
	wasm_trace(start, mins, maxs, end, passent, contentmask, &tr);
	return tr;
}

volatile game_export_t *GetGameAPI (game_import_t *import);

static volatile game_export_t *volatile _ge;

void WASM_GetGameAPI(void) __WASM_EXPORT__(WASM_GetGameAPI)
{
#define MAP_IMPORT(ei) \
	.ei = wasm_ ## ei
#define MAP_IMPORT_WRAPPED(ei) \
	.ei = wasm_wrap_ ## ei

	/*	This is the bridge between WASM and Q2. Since WAMR can't "run"
		function pointers, we have to wrap the functions and call them
		using a different method. */
	static game_import_t _gi = {
		MAP_IMPORT_WRAPPED(bprintf),
		MAP_IMPORT_WRAPPED(dprintf),
		MAP_IMPORT_WRAPPED(cprintf),
		MAP_IMPORT_WRAPPED(centerprintf),
		MAP_IMPORT(sound),
		MAP_IMPORT(positioned_sound),

		MAP_IMPORT(configstring),

		MAP_IMPORT_WRAPPED(error),
		
		MAP_IMPORT(modelindex),
		MAP_IMPORT(soundindex),
		MAP_IMPORT(imageindex),

		MAP_IMPORT(setmodel),

		MAP_IMPORT_WRAPPED(trace),
		MAP_IMPORT(pointcontents),
		MAP_IMPORT(inPVS),
		MAP_IMPORT(inPHS),
		MAP_IMPORT(SetAreaPortalState),
		MAP_IMPORT(AreasConnected),
		
		MAP_IMPORT(linkentity),
		MAP_IMPORT(unlinkentity),
		MAP_IMPORT(BoxEdicts),
		MAP_IMPORT(Pmove),

		MAP_IMPORT(multicast),
		MAP_IMPORT(unicast),
		MAP_IMPORT(WriteChar),
		MAP_IMPORT(WriteByte),
		MAP_IMPORT(WriteShort),
		MAP_IMPORT(WriteLong),
		MAP_IMPORT(WriteFloat),
		MAP_IMPORT(WriteString),
		MAP_IMPORT(WritePosition),
		MAP_IMPORT(WriteDir),
		MAP_IMPORT(WriteAngle),

		MAP_IMPORT(TagMalloc),
		MAP_IMPORT(TagFree),
		MAP_IMPORT(FreeTags),
		
		MAP_IMPORT(cvar),
		MAP_IMPORT(cvar_set),
		MAP_IMPORT(cvar_forceset),

		MAP_IMPORT(argc),
		MAP_IMPORT(argv),
		MAP_IMPORT(args),

		MAP_IMPORT(AddCommandString),

		MAP_IMPORT(DebugGraph)
	};

	_ge = GetGameAPI(&_gi);
}

int32_t WASM_GetAPIVersion(void) __WASM_EXPORT__(WASM_GetAPIVersion)
{
	return _ge->apiversion;
}

edict_t *WASM_GetEdicts(void) __WASM_EXPORT__(WASM_GetEdicts)
{
	return _ge->edicts;
}

uint32_t WASM_GetEdictSize(void) __WASM_EXPORT__(WASM_GetEdictSize)
{
	return _ge->edict_size;
}

volatile int32_t *WASM_GetNumEdicts(void) __WASM_EXPORT__(WASM_GetNumEdicts)
{
	return &_ge->num_edicts;
}

uint32_t WASM_GetMaxEdicts(void) __WASM_EXPORT__(WASM_GetMaxEdicts)
{
	return _ge->max_edicts;
}

void WASM_Init(void) __WASM_EXPORT__(WASM_Init)
{
	_ge->Init();
}

void WASM_SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint) __WASM_EXPORT__(WASM_SpawnEntities)
{
	_ge->SpawnEntities(mapname, entities, spawnpoint);
}

qboolean WASM_ClientConnect(edict_t *e, char *userinfo) __WASM_EXPORT__(WASM_ClientConnect)
{
	return _ge->ClientConnect(e, userinfo);
}

void WASM_ClientUserinfoChanged(edict_t *e, char *userinfo) __WASM_EXPORT__(WASM_ClientUserinfoChanged)
{
	return _ge->ClientUserinfoChanged(e, userinfo);
}

void WASM_PmoveTrace(pmove_t *pm, const vec3_t *start, const vec3_t *mins, const vec3_t *maxs, const vec3_t *end, trace_t *out) __WASM_EXPORT__(WASM_PmoveTrace)
{
	*out = pm->trace(start, mins, maxs, end);
}

content_flags_t WASM_PmovePointContents(pmove_t *pm, const vec3_t *p) __WASM_EXPORT__(WASM_PmovePointContents)
{
	return pm->pointcontents(p);
}

void WASM_ClientThink(edict_t *e, usercmd_t *ucmd) __WASM_EXPORT__(WASM_ClientThink)
{
	_ge->ClientThink(e, ucmd);
}

void WASM_ClientBegin(edict_t *e) __WASM_EXPORT__(WASM_ClientBegin)
{
	_ge->ClientBegin(e);
}

void WASM_RunFrame(void) __WASM_EXPORT__(WASM_RunFrame)
{
	_ge->RunFrame();
}

void WASM_ClientCommand(edict_t *e) __WASM_EXPORT__(WASM_ClientCommand)
{
	_ge->ClientCommand(e);
}

void WASM_ClientDisconnect(edict_t *e) __WASM_EXPORT__(WASM_ClientDisconnect)
{
	_ge->ClientDisconnect(e);
}

void WASM_ServerCommand(void) __WASM_EXPORT__(WASM_ServerCommand)
{
	_ge->ServerCommand();
}

void WASM_WriteGame(const char *filename, qboolean autosave) __WASM_EXPORT__(WASM_WriteGame)
{
	_ge->WriteGame(filename, autosave);
}

void WASM_ReadGame(const char *filename) __WASM_EXPORT__(WASM_ReadGame)
{
	_ge->ReadGame(filename);
}

void WASM_WriteLevel(const char *filename) __WASM_EXPORT__(WASM_WriteLevel)
{
	_ge->WriteLevel(filename);
}

void WASM_ReadLevel(const char *filename) __WASM_EXPORT__(WASM_ReadLevel)
{
	_ge->ReadLevel(filename);
}

#endif