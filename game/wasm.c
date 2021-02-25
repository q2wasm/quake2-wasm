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

DECLARE_IMPORT(void, bprint, print_level_t printlevel, const char *str);
DECLARE_IMPORT(void, dprint, const char *str);
DECLARE_IMPORT(void, cprint, edict_t *ent, print_level_t printlevel, const char *str);
DECLARE_IMPORT(void, centerprint, edict_t *ent, const char *str);
DECLARE_IMPORT(void, sound, edict_t *ent, sound_channel_t channel, int32_t soundindex, vec_t volume, sound_attn_t attenuation, vec_t timeofs);
DECLARE_IMPORT(void, positioned_sound, const vec3_t *origin, edict_t *ent, sound_channel_t channel, int32_t soundindex, vec_t volume, sound_attn_t attenuation, vec_t timeofs);
DECLARE_IMPORT(void, configstring, int32_t num, const char *string);

DECLARE_IMPORT(void, error, const char *str);

DECLARE_IMPORT(int32_t, modelindex, const char *name);
DECLARE_IMPORT(int32_t, soundindex, const char *name);
DECLARE_IMPORT(int32_t, imageindex, const char *name);

DECLARE_IMPORT(void, setmodel, edict_t *ent, const char *name);

DECLARE_IMPORT(void, trace, const vec3_t *start, const vec3_t *mins, const vec3_t *maxs, const vec3_t *end, edict_t *passent, content_flags_t contentmask, trace_t *out);
DECLARE_IMPORT(content_flags_t, pointcontents, const vec3_t *point);
DECLARE_IMPORT(qboolean, inPVS, const vec3_t *a, const vec3_t *b);
DECLARE_IMPORT(qboolean, inPHS, const vec3_t *a, const vec3_t *b);
DECLARE_IMPORT(void, SetAreaPortalState, int32_t portalnum, qboolean open);
DECLARE_IMPORT(qboolean, AreasConnected, int32_t a, int32_t b);

DECLARE_IMPORT(void, linkentity, edict_t *e);
DECLARE_IMPORT(void, unlinkentity, edict_t *e);
DECLARE_IMPORT(int32_t, BoxEdicts, const vec3_t *mins, const vec3_t *maxs, edict_t **list, int32_t maxcount, box_edicts_area_t areatype);
DECLARE_IMPORT(void, Pmove, pmove_t *pmove);

DECLARE_IMPORT(void, multicast, const vec3_t *origin, multicast_t to);
DECLARE_IMPORT(void, unicast, edict_t *ent, qboolean reliable);

DECLARE_IMPORT(void, WriteChar, int32_t c);
DECLARE_IMPORT(void, WriteByte, int32_t c);
DECLARE_IMPORT(void, WriteShort, int32_t c);
DECLARE_IMPORT(void, WriteLong, int32_t c);
DECLARE_IMPORT(void, WriteFloat, vec_t f);
DECLARE_IMPORT(void, WriteString, const char *s);
DECLARE_IMPORT(void, WritePosition, const vec3_t *p);
DECLARE_IMPORT(void, WriteDir, const vec3_t *p);
DECLARE_IMPORT(void, WriteAngle, vec_t f);

DECLARE_IMPORT(void *, TagMalloc, uint32_t size, uint32_t tag);
DECLARE_IMPORT(void, TagFree, void *block);
DECLARE_IMPORT(void, FreeTags, uint32_t tag);

DECLARE_IMPORT(cvar_t *, cvar, const char *name, const char *value, cvar_flags_t flags);
DECLARE_IMPORT(cvar_t *, cvar_set, const char *name, const char *value);
DECLARE_IMPORT(cvar_t *, cvar_forceset, const char *name, const char *value);

DECLARE_IMPORT(int32_t, argc, void);
DECLARE_IMPORT(char *, argv, int32_t n);
DECLARE_IMPORT(char *, args, void);

DECLARE_IMPORT(void, AddCommandString, const char *);

DECLARE_IMPORT(void, DebugGraph, vec_t, int32_t);

/*	This is the bridge between WASM and Q2. Since WAMR can't "run"
	function pointers, we have to wrap the functions and call them
	using a different method. */
static game_import_t _gi;
static volatile game_export_t *_ge;

extern game_export_t *GetGameAPI (game_import_t *import);

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

#define PARSE_VAR_ARGS \
	va_list argptr; \
	static char	string[1024]; \
	va_start (argptr, format); \
	vsnprintf (string, sizeof(string), format, argptr); \
	va_end (argptr)

static void _bprintf(print_level_t printlevel, const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_bprint(printlevel, string);
}

static void _dprintf(const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_dprint(string);
}

static void _cprintf(edict_t *ent, print_level_t printlevel, const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_cprint(ent, printlevel, string);
}

static void _centerprintf(edict_t *ent, const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_centerprint(ent, string);
}

static void _error(const char *format, ...)
{
	PARSE_VAR_ARGS;
	wasm_error(string);
}

static trace_t _trace(const vec3_t *start, const vec3_t *mins, const vec3_t *maxs, const vec3_t *end, edict_t *passent, content_flags_t contentmask)
{
	static trace_t tr;
	wasm_trace(start, mins, maxs, end, passent, contentmask, &tr);
	return tr;
}

void InitWASMAPI(edict_t ***edicts, int32_t **edict_size, int32_t **num_edicts, int32_t **max_edicts) __WASM_EXPORT__(InitWASMAPI)
{
	_gi.bprintf = _bprintf;
	_gi.dprintf = _dprintf;
	_gi.cprintf = _cprintf;
	_gi.centerprintf = _centerprintf;
	_gi.sound = wasm_sound;
	_gi.positioned_sound = wasm_positioned_sound;

	_gi.configstring = wasm_configstring;

	_gi.error = _error;
	
	_gi.modelindex = wasm_modelindex;
	_gi.soundindex = wasm_soundindex;
	_gi.imageindex = wasm_imageindex;

	_gi.setmodel = wasm_setmodel;

	_gi.trace = _trace;
	_gi.pointcontents = wasm_pointcontents;
	_gi.inPVS = wasm_inPVS;
	_gi.inPHS = wasm_inPHS;
	_gi.SetAreaPortalState = wasm_SetAreaPortalState;
	_gi.AreasConnected = wasm_AreasConnected;
	
	_gi.linkentity = wasm_linkentity;
	_gi.unlinkentity = wasm_unlinkentity;
	_gi.BoxEdicts = wasm_BoxEdicts;
	_gi.Pmove = wasm_Pmove;

	_gi.multicast = wasm_multicast;
	_gi.unicast = wasm_unicast;
	_gi.WriteChar = wasm_WriteChar;
	_gi.WriteByte = wasm_WriteByte;
	_gi.WriteShort = wasm_WriteShort;
	_gi.WriteLong = wasm_WriteLong;
	_gi.WriteFloat = wasm_WriteFloat;
	_gi.WriteString = wasm_WriteString;
	_gi.WritePosition = wasm_WritePosition;
	_gi.WriteDir = wasm_WriteDir;
	_gi.WriteAngle = wasm_WriteAngle;

	_gi.TagMalloc = wasm_TagMalloc;
	_gi.TagFree = wasm_TagFree;
	_gi.FreeTags = wasm_FreeTags;
	
	_gi.cvar = wasm_cvar;
	_gi.cvar_set = wasm_cvar_set;
	_gi.cvar_forceset = wasm_cvar_forceset;

	_gi.argc = wasm_argc;
	_gi.argv = wasm_argv;
	_gi.args = wasm_args;

	_gi.AddCommandString = wasm_AddCommandString;

	_gi.DebugGraph = wasm_DebugGraph;

	_ge = GetGameAPI(&_gi);

	*edicts = &_ge->edicts;
	*edict_size = &_ge->edict_size;
	*num_edicts = &_ge->num_edicts;
	*max_edicts = &_ge->max_edicts;
}

#endif