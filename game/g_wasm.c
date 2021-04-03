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

/**
 * This file is only used by the WASM compiler, and is *only* for porting existing codebases over
 * to the WASM codebase with minimal effort. It effectively wraps an existing gamex source into
 * the imports/exports required for WASM. This does add a bit of overhead, but, not enough to
 * be concerned about. This file is unnecessary if you're natively targeting WASM.
 **/

#ifdef __wasm__

#include <stdarg.h>
#include <stdio.h>
#include "g_api.h"

#define WASM_EXPORT(name) \
	__attribute__((export_name(#name)))

#define WASM_IMPORT(name) \
	__attribute__((import_module("q2"), import_name(#name)))

#define DECLARE_IMPORT(r, n, ...) \
	r wasm_ ## n(__VA_ARGS__) WASM_IMPORT(n)

DECLARE_IMPORT(void, bprint, print_level_t, const char *);
DECLARE_IMPORT(void, dprint, const char *);
DECLARE_IMPORT(void, cprint, edict_t *, print_level_t, const char *);
DECLARE_IMPORT(void, centerprint, edict_t *, const char *);
DECLARE_IMPORT(void, sound, edict_t *ent, sound_channel_t, int32_t, vec_t, sound_attn_t, vec_t);
DECLARE_IMPORT(void, positioned_sound, vec_t, vec_t, vec_t, edict_t *, sound_channel_t, int32_t, vec_t, sound_attn_t, vec_t);
DECLARE_IMPORT(void, configstring, int32_t , const char *);

DECLARE_IMPORT(void, error, const char *);

DECLARE_IMPORT(int32_t, modelindex, const char *);
DECLARE_IMPORT(int32_t, soundindex, const char *);
DECLARE_IMPORT(int32_t, imageindex, const char *);

DECLARE_IMPORT(void, setmodel, edict_t *, const char *);

DECLARE_IMPORT(void, trace, vec_t, vec_t, vec_t, vec_t, vec_t, vec_t,
	vec_t, vec_t, vec_t, vec_t, vec_t, vec_t, edict_t *passent,
	content_flags_t, trace_t *);
DECLARE_IMPORT(content_flags_t, pointcontents, vec_t, vec_t, vec_t);
DECLARE_IMPORT(qboolean, inPVS, vec_t, vec_t, vec_t, vec_t, vec_t, vec_t);
DECLARE_IMPORT(qboolean, inPHS, vec_t, vec_t, vec_t, vec_t, vec_t, vec_t);
DECLARE_IMPORT(void, SetAreaPortalState, int32_t, qboolean);
DECLARE_IMPORT(qboolean, AreasConnected, int32_t, int32_t);

DECLARE_IMPORT(void, linkentity, edict_t *);
DECLARE_IMPORT(void, unlinkentity, edict_t *);
DECLARE_IMPORT(int32_t, BoxEdicts, vec_t, vec_t, vec_t, vec_t, vec_t, vec_t, edict_t **, int32_t, box_edicts_area_t);
DECLARE_IMPORT(void, Pmove, pmove_t *pmove);

DECLARE_IMPORT(void, multicast, vec_t, vec_t, vec_t, multicast_t);
DECLARE_IMPORT(void, unicast, edict_t *, qboolean);

DECLARE_IMPORT(void, WriteChar, int32_t);
DECLARE_IMPORT(void, WriteByte, int32_t);
DECLARE_IMPORT(void, WriteShort, int32_t);
DECLARE_IMPORT(void, WriteLong, int32_t);
DECLARE_IMPORT(void, WriteFloat, vec_t);
DECLARE_IMPORT(void, WriteString, const char *);
DECLARE_IMPORT(void, WritePosition, vec_t, vec_t, vec_t);
DECLARE_IMPORT(void, WriteDir, vec_t, vec_t, vec_t);
DECLARE_IMPORT(void, WriteAngle, vec_t);

DECLARE_IMPORT(void *, TagMalloc, uint32_t, uint32_t);
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
	static const vec3_t zero = { 0, 0, 0 };

	if (!mins)
		mins = &zero;
	if (!maxs)
		maxs = &zero;

	wasm_trace(start->x, start->y, start->z, mins->x, mins->y, mins->z, maxs->x, maxs->y, maxs->z,
		end->x, end->y, end->z, passent, contentmask, &tr);

	return tr;
}

static content_flags_t wasm_wrap_pointcontents(const vec3_t *point)
{
	return wasm_pointcontents(point->x, point->y, point->z);
}

static void wasm_wrap_WriteDir(const vec3_t *point)
{
	return wasm_WriteDir(point->x, point->y, point->z);
}

static void wasm_wrap_WritePosition(const vec3_t *point)
{
	return wasm_WritePosition(point->x, point->y, point->z);
}

static void wasm_wrap_positioned_sound(const vec3_t *origin, edict_t *ent, sound_channel_t channel, int soundindex, vec_t volume, sound_attn_t attenuation, vec_t timeofs)
{
	if (!origin)
		wasm_sound(ent, channel, soundindex, volume, attenuation, timeofs);
	else
		wasm_positioned_sound(origin->x, origin->y, origin->z, ent, channel, soundindex, volume, attenuation, timeofs);
}

static qboolean wasm_wrap_inPHS(const vec3_t *p1, const vec3_t *p2)
{
	return wasm_inPHS(p1->x, p1->y, p1->z, p2->x, p2->y, p2->z);
}

static qboolean wasm_wrap_inPVS(const vec3_t *p1, const vec3_t *p2)
{
	return wasm_inPVS(p1->x, p1->y, p1->z, p2->x, p2->y, p2->z);
}

static int32_t wasm_wrap_BoxEdicts(const vec3_t *mins, const vec3_t *maxs, edict_t **list, int32_t maxcount, box_edicts_area_t areatype)
{
	return wasm_BoxEdicts(mins->x, mins->y, mins->z, maxs->x, maxs->y, maxs->z, list, maxcount, areatype);
}

static void wasm_wrap_multicast(const vec3_t *p, multicast_t to)
{
	wasm_multicast(p->x, p->y, p->z, to);
}

game_export_t *GetGameAPI (game_import_t *import);

static game_export_t *_ge;

int32_t WASM_GetGameAPI(int32_t apiversion) WASM_EXPORT(GetGameAPI)
{
	if (apiversion != GAME_API_EXTENDED_VERSION)
		return 0;

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
		MAP_IMPORT_WRAPPED(positioned_sound),

		MAP_IMPORT(configstring),

		MAP_IMPORT_WRAPPED(error),
		
		MAP_IMPORT(modelindex),
		MAP_IMPORT(soundindex),
		MAP_IMPORT(imageindex),

		MAP_IMPORT(setmodel),

		MAP_IMPORT_WRAPPED(trace),
		MAP_IMPORT_WRAPPED(pointcontents),
		MAP_IMPORT_WRAPPED(inPVS),
		MAP_IMPORT_WRAPPED(inPHS),
		MAP_IMPORT(SetAreaPortalState),
		MAP_IMPORT(AreasConnected),
		
		MAP_IMPORT(linkentity),
		MAP_IMPORT(unlinkentity),
		MAP_IMPORT_WRAPPED(BoxEdicts),
		MAP_IMPORT(Pmove),

		MAP_IMPORT_WRAPPED(multicast),
		MAP_IMPORT(unicast),
		MAP_IMPORT(WriteChar),
		MAP_IMPORT(WriteByte),
		MAP_IMPORT(WriteShort),
		MAP_IMPORT(WriteLong),
		MAP_IMPORT(WriteFloat),
		MAP_IMPORT(WriteString),
		MAP_IMPORT_WRAPPED(WritePosition),
		MAP_IMPORT_WRAPPED(WriteDir),
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

	return GAME_API_EXTENDED_VERSION;
}

edict_t *WASM_GetEdicts(void) WASM_EXPORT(GetEdicts)
{
	return _ge->edicts;
}

uint32_t WASM_GetEdictSize(void) WASM_EXPORT(GetEdictSize)
{
	return _ge->edict_size;
}

int32_t *WASM_GetNumEdicts(void) WASM_EXPORT(GetNumEdicts)
{
	return &_ge->num_edicts;
}

uint32_t WASM_GetMaxEdicts(void) WASM_EXPORT(GetMaxEdicts)
{
	return _ge->max_edicts;
}

void WASM_Init(void) WASM_EXPORT(Init)
{
	_ge->Init();
}

void WASM_SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint) WASM_EXPORT(SpawnEntities)
{
	_ge->SpawnEntities(mapname, entities, spawnpoint);
}

qboolean WASM_ClientConnect(edict_t *e, char *userinfo) WASM_EXPORT(ClientConnect)
{
	return _ge->ClientConnect(e, userinfo);
}

void WASM_ClientUserinfoChanged(edict_t *e, char *userinfo) WASM_EXPORT(ClientUserinfoChanged)
{
	return _ge->ClientUserinfoChanged(e, userinfo);
}

void WASM_PmoveTrace(pmove_t *pm, vec_t start_x, vec_t start_y, vec_t start_z, vec_t mins_x, vec_t mins_y, vec_t mins_z,
	vec_t maxs_x, vec_t maxs_y, vec_t maxs_z, vec_t end_x, vec_t end_y, vec_t end_z, trace_t *out) WASM_EXPORT(PmoveTrace)
{
	*out = pm->trace(&(const vec3_t) { start_x, start_y, start_z }, &(const vec3_t) { mins_x, mins_y, mins_z },
		&(const vec3_t) { maxs_x, maxs_y, maxs_z }, &(const vec3_t) { end_x, end_y, end_z });
}

content_flags_t WASM_PmovePointContents(pmove_t *pm, vec_t p_x, vec_t p_y, vec_t p_z) WASM_EXPORT(PmovePointContents)
{
	return pm->pointcontents(&(const vec3_t) { p_x, p_y, p_z });
}

void WASM_ClientThink(edict_t *e, usercmd_t *ucmd) WASM_EXPORT(ClientThink)
{
	_ge->ClientThink(e, ucmd);
}

void WASM_ClientBegin(edict_t *e) WASM_EXPORT(ClientBegin)
{
	_ge->ClientBegin(e);
}

void WASM_RunFrame(void) WASM_EXPORT(RunFrame)
{
	_ge->RunFrame();
}

void WASM_ClientCommand(edict_t *e) WASM_EXPORT(ClientCommand)
{
	_ge->ClientCommand(e);
}

void WASM_ClientDisconnect(edict_t *e) WASM_EXPORT(ClientDisconnect)
{
	_ge->ClientDisconnect(e);
}

void WASM_ServerCommand(void) WASM_EXPORT(ServerCommand)
{
	_ge->ServerCommand();
}

void WASM_WriteGame(const char *filename, qboolean autosave) WASM_EXPORT(WriteGame)
{
	_ge->WriteGame(filename, autosave);
}

void WASM_ReadGame(const char *filename) WASM_EXPORT(ReadGame)
{
	_ge->ReadGame(filename);
}

void WASM_WriteLevel(const char *filename) WASM_EXPORT(WriteLevel)
{
	_ge->WriteLevel(filename);
}

void WASM_ReadLevel(const char *filename) WASM_EXPORT(ReadLevel)
{
	_ge->ReadLevel(filename);
}

game_capability_t WASM_QueryGameCapability(const char *cap) WASM_EXPORT(QueryGameCapability)
{
	return qfalse;
}

#endif