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

#include "shared/entity.h"
#include "g_main.h"
#include "g_wasm.h"

#include <wasm_export.h>

// Cvars are handled as heap copies given to the WASM runtime.
// Every now and then, we check to see if we have to update the cvar.
typedef struct wasm_mapped_cvar_s
{
	cvar_t		*native;
	wasm_addr_t	wasm_ptr;
	size_t		string_size, latched_string_size;
	struct wasm_mapped_cvar_s *next;
} wasm_mapped_cvar_t;

static wasm_mapped_cvar_t *mapped_cvars;

static wasm_mapped_cvar_t *fetch_mapped_cvar(const char *name)
{
	for (wasm_mapped_cvar_t *cvar = mapped_cvars; cvar; cvar = cvar->next)
		if (stricmp(cvar->native->name, name) == 0)
			return cvar;

	return NULL;
}

static void update_cvar_string(const char *src, size_t *dst_size, uint32_t *dst_ptr)
{
	if (!*dst_ptr && !src)
		return;
	else if (*dst_ptr && !src)
	{
		wasm_runtime_module_free(wasm.module_inst, *dst_ptr);
		*dst_ptr = 0;
		*dst_size = 0;
		return;
	}

	char *dst = NULL;

	if (*dst_ptr)
	{
		dst = (char *) wasm_addr_to_native(*dst_ptr);

		if (strcmp(dst, src) == 0)
			return;
	}

	const size_t length = strlen(src);

	if (length + 1 > *dst_size)
	{
		if (*dst_ptr)
			wasm_runtime_module_free(wasm.module_inst, *dst_ptr);
		
		*dst_size = length + 1;
		*dst_ptr = wasm_runtime_module_malloc(wasm.module_inst, *dst_size, (void **) &dst);

		if (!*dst_ptr)
			wasm_error("Out of WASM memory");
	}

	if (!dst)
		wasm_error("Not sure how this happen");
	else
		memcpy(dst, src, *dst_size);
}

static void update_mapped_cvar(wasm_cvar_t *wasm_cvar, wasm_mapped_cvar_t *cvar, qboolean modified)
{
	cvar_t *native_cvar = cvar->native;

	wasm_cvar->flags = native_cvar->flags;
	wasm_cvar->value = native_cvar->value;
	wasm_cvar->intVal = (int) native_cvar->value;
	wasm_cvar->modified = modified;
	
	update_cvar_string(native_cvar->string, &cvar->string_size, &wasm_cvar->string);
	update_cvar_string(native_cvar->latched_string, &cvar->latched_string_size, &wasm_cvar->latched_string);
}

void q2_wasm_update_cvars()
{
	for (wasm_mapped_cvar_t *cvar = mapped_cvars; cvar; cvar = cvar->next)
	{
		if (!cvar->native->modified)
			continue;

		wasm_cvar_t *wasm_cvar = (wasm_cvar_t *) wasm_addr_to_native(cvar->wasm_ptr);
		update_mapped_cvar(wasm_cvar, cvar, qtrue);
		cvar->native->modified = qfalse;
	}
}

static wasm_cvar_t *map_cvar(cvar_t *native, wasm_mapped_cvar_t **mapped)
{
	wasm_mapped_cvar_t *m = *mapped = (wasm_mapped_cvar_t *) gi.TagMalloc(sizeof(wasm_mapped_cvar_t), TAG_GAME);

	m->next = mapped_cvars;

	m->native = native;

	wasm_cvar_t *wasm_cvar;

	m->wasm_ptr = wasm_runtime_module_malloc(wasm.module_inst, sizeof(wasm_cvar_t), (void **) &wasm_cvar);

	if (!m->wasm_ptr)
		wasm_error("Out of WASM memory");

	memset(wasm_cvar, 0, sizeof(wasm_cvar_t));

	wasm_cvar->name = wasm_dup_str(native->name);

	mapped_cvars = m;

	return wasm_cvar;
}

static uint32_t q2_cvar(wasm_exec_env_t env, const char *name, const char *value, const int32_t flags)
{
	wasm_mapped_cvar_t *mapped = fetch_mapped_cvar(name);
	wasm_cvar_t *wasm_cvar;

	if (!mapped)
		wasm_cvar = map_cvar(gi.cvar(name, value, flags), &mapped);
	else
		wasm_cvar = (wasm_cvar_t *) wasm_addr_to_native(mapped->wasm_ptr);

	update_mapped_cvar(wasm_cvar, mapped, qfalse);

	return mapped->wasm_ptr;
}

static uint32_t q2_cvar_set(wasm_exec_env_t env, const char *name, const char *value)
{
	wasm_mapped_cvar_t *mapped = fetch_mapped_cvar(name);
	wasm_cvar_t *wasm_cvar;

	if (!mapped)
		wasm_cvar = map_cvar(gi.cvar_set(name, value), &mapped);
	else
		wasm_cvar = (wasm_cvar_t *) wasm_addr_to_native(mapped->wasm_ptr);

	update_mapped_cvar(wasm_cvar, mapped, qfalse);

	return mapped->wasm_ptr;
}

static uint32_t q2_cvar_forceset(wasm_exec_env_t env, const char *name, const char *value)
{
	wasm_mapped_cvar_t *mapped = fetch_mapped_cvar(name);
	wasm_cvar_t *wasm_cvar;

	if (!mapped)
		wasm_cvar = map_cvar(gi.cvar_forceset(name, value), &mapped);
	else
		wasm_cvar = (wasm_cvar_t *) wasm_addr_to_native(mapped->wasm_ptr);

	update_mapped_cvar(wasm_cvar, mapped, qfalse);

	return mapped->wasm_ptr;
}

static void q2_dprint(wasm_exec_env_t env, const char *str)
{
	gi.dprintf("%s", str);
}

static void q2_bprint(wasm_exec_env_t env, print_level_t print_level, const char *str)
{
	gi.bprintf(print_level, "%s", str);
}

typedef struct tagged_block_s
{
	uint32_t memory;
	void *ptr;
	uint32_t tag;
	struct tagged_block_s *next;
} tagged_block_t;

static tagged_block_t *tagged_blocks;

static uint32_t q2_TagMalloc(wasm_exec_env_t env, uint32_t size, uint32_t tag)
{
	void *ptr;
	uint32_t loc = wasm_runtime_module_malloc(wasm.module_inst, size, &ptr);

	if (!loc)
		wasm_error("Out of WASM memory");
	
	memset(ptr, 0, size);

	tagged_block_t *block = (tagged_block_t *) gi.TagMalloc(sizeof(tagged_block_t), TAG_GAME);

	block->memory = loc;
	block->ptr = ptr;
	block->tag = tag;
	block->next = tagged_blocks;
	tagged_blocks = block;

	return loc;
}

static void q2_TagFree(wasm_exec_env_t env, void *ptr)
{
	wasm_runtime_module_free(wasm.module_inst, wasm_native_to_addr(ptr));

	for (tagged_block_t **block = &tagged_blocks; *block; )
	{
		if ((*block)->ptr == ptr)
		{
			tagged_block_t *b = *block;
			*block = b->next;
			gi.TagFree(b);
		}
		else
			block = &(*block)->next;
	}
}

static void q2_FreeTags(wasm_exec_env_t env, uint32_t tag)
{
	for (tagged_block_t **block = &tagged_blocks; *block; )
	{
		if ((*block)->tag == tag)
		{
			tagged_block_t *b = *block;
			*block = b->next;
			gi.TagFree(b);
		}
		else
			block = &(*block)->next;
	}
}

#include <stdlib.h>

#ifdef KMQUAKE2_ENGINE_MOD
bool stat_offsets[MAX_STATS];

#define MAX_TOKEN_CHARS 64
static char     com_token[4][MAX_TOKEN_CHARS];
static int      com_tokidx;

/*
==============
COM_Parse

Parse a token out of a string.
Handles C and C++ comments.
==============
*/
static char *COM_Parse(const char **data_p)
{
    int         c;
    int         len;
    const char  *data;
    char        *s = com_token[com_tokidx++ & 3];

    data = *data_p;
    len = 0;
    s[0] = 0;

    if (!data) {
        *data_p = NULL;
        return s;
    }

// skip whitespace
skipwhite:
    while ((c = *data) <= ' ') {
        if (c == 0) {
            *data_p = NULL;
            return s;
        }
        data++;
    }

// skip // comments
    if (c == '/' && data[1] == '/') {
        data += 2;
        while (*data && *data != '\n')
            data++;
        goto skipwhite;
    }

// skip /* */ comments
    if (c == '/' && data[1] == '*') {
        data += 2;
        while (*data) {
            if (data[0] == '*' && data[1] == '/') {
                data += 2;
                break;
            }
            data++;
        }
        goto skipwhite;
    }

// handle quoted strings specially
    if (c == '\"') {
        data++;
        while (1) {
            c = *data++;
            if (c == '\"' || !c) {
                goto finish;
            }

            if (len < MAX_TOKEN_CHARS - 1) {
                s[len++] = c;
            }
        }
    }

// parse a regular word
    do {
        if (len < MAX_TOKEN_CHARS - 1) {
            s[len++] = c;
        }
        data++;
        c = *data;
    } while (c > 32);

finish:
    s[len] = 0;

    *data_p = data;
    return s;
}
#endif

static void q2_configstring(wasm_exec_env_t env, int32_t id, const char *value)
{
#ifdef KMQUAKE2_ENGINE_MOD
	id = wasm_remap_configstring(id);

	if (id == CS_STATUSBAR)
	{
		memset(stat_offsets, 0, sizeof(stat_offsets));

		char *p = value, *t;

		while ((t = COM_Parse(&p)) && *t && p)
		{
			if (strcmp(t, "stat_string"))
				continue;

			t = COM_Parse(&p);

			if (*t && p)
				stat_offsets[atoi(t)] = true;
		}
	}
#endif

	gi.configstring(id, value);
}

static int32_t q2_modelindex(wasm_exec_env_t env, const char *value)
{
	return gi.modelindex(value);
}

static int32_t q2_imageindex(wasm_exec_env_t env, const char *value)
{
	return gi.imageindex(value);
}

static int32_t q2_soundindex(wasm_exec_env_t env, const char *value)
{
	return gi.soundindex(value);
}

static void q2_cprint(wasm_exec_env_t env, wasm_edict_t *ent, print_level_t print_level, const char *str)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");

	gi.cprintf(entity_wnp_to_np(ent), print_level, "%s", str);
}

static void q2_centerprint(wasm_exec_env_t env, wasm_edict_t *ent, const char *str)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");

	gi.centerprintf(entity_wnp_to_np(ent), "%s", str);
}

static void q2_error(wasm_exec_env_t env, const char *str)
{
	wasm_error(str);
}

static void q2_linkentity(wasm_exec_env_t env, wasm_edict_t *wasm_edict)
{
	if (!entity_validate_wnp(wasm_edict))
		wasm_error("Invalid pointer");

	edict_t *native_edict = entity_wnp_to_np(wasm_edict);

	copy_link_wasm_to_native(native_edict, wasm_edict);
	const bool copy_old_origin = wasm_edict->linkcount == 0;
	gi.linkentity(native_edict);
	if (copy_old_origin)
		wasm_edict->s.old_origin = native_edict->s.old_origin;
	copy_link_native_to_wasm(wasm_edict, native_edict);
}

static void q2_unlinkentity(wasm_exec_env_t env, wasm_edict_t *wasm_edict)
{
	if (!entity_validate_wnp(wasm_edict))
		wasm_error("Invalid pointer");

	edict_t *native_edict = entity_wnp_to_np(wasm_edict);

	copy_link_wasm_to_native(native_edict, wasm_edict);
	gi.unlinkentity(native_edict);
	copy_link_native_to_wasm(wasm_edict, native_edict);
}

static void q2_setmodel(wasm_exec_env_t env, wasm_edict_t *wasm_edict, const char *model)
{
	if (!entity_validate_wnp(wasm_edict))
		wasm_error("Invalid pointer");

	edict_t *native_edict = entity_wnp_to_np(wasm_edict);

	copy_link_wasm_to_native(native_edict, wasm_edict);
	gi.setmodel(native_edict, model);
	copy_link_native_to_wasm(wasm_edict, native_edict);

	// setmodel also sets up mins, maxs, and modelindex
	wasm_edict->s.modelindex = native_edict->s.modelindex;
	wasm_edict->mins = native_edict->mins;
	wasm_edict->maxs = native_edict->maxs;
}

static const vec3_t zero = { 0, 0, 0 };

// For tracing, we use two maps to keep track of surfaces that we've already seen.
// The address -> native map is used specifically for gi.Pmove, which needs
// to be able to convert them back.
#define SURF_CACHE_HASH_SIZE	64

typedef struct surf_cache_entry_s
{
	csurface_t				*native;
	wasm_surface_address_t	wasm;

	struct surf_cache_entry_s	*next_native;
	struct surf_cache_entry_s	*next_wasm;
} surf_cache_entry_t;

typedef struct
{
	surf_cache_entry_t	*native_hash[SURF_CACHE_HASH_SIZE];
	surf_cache_entry_t	*wasm_hash[SURF_CACHE_HASH_SIZE];
} surf_cache_t;

static surf_cache_t surf_cache;

// WASM address to the currently-processing pmove.
static uint32_t wasm_pmove_ptr;

static void q2_trace(wasm_exec_env_t env, const vec_t start_x, const vec_t start_y, const vec_t start_z, const vec_t mins_x, const vec_t mins_y, const vec_t mins_z, const vec_t maxs_x, const vec_t maxs_y, const vec_t maxs_z, const vec_t end_x, const vec_t end_y, const vec_t end_z, wasm_edict_t *passent, content_flags_t contentmask, wasm_trace_t *out)
{
	if (!entity_validate_wnp(passent))
		wasm_error("Invalid pointer");
	if (!wasm_validate_ptr(out, sizeof(wasm_trace_t)))
		wasm_error("Invalid pointer");

	for (int32_t i = 0; i < globals.num_edicts; i++)
	{
		wasm_edict_t *e = entity_number_to_wnp(i);

		if (e->inuse)
			sync_entity(e, entity_number_to_np(i), false);
	}

	edict_t *native_passent;

	if (wasm_native_to_addr(passent) == 0)
		native_passent = globals.edicts;
	else
		native_passent = entity_wnp_to_np(passent);

	static trace_t tr;

	const vec3_t start = { start_x, start_y, start_z };
	const vec3_t mins = { mins_x, mins_y, mins_z };
	const vec3_t maxs = { maxs_x, maxs_y, maxs_z };
	const vec3_t end = { end_x, end_y, end_z };

	tr = gi.trace(&start, &mins, &maxs, &end, native_passent, contentmask);

	out->allsolid = tr.allsolid;
	out->contents = tr.contents;
	out->endpos = tr.endpos;
	out->ent = entity_np_to_wa(tr.ent);
	out->fraction = tr.fraction;
	out->plane = tr.plane;
	out->startsolid = tr.startsolid;

	if (!tr.surface || !tr.surface->name[0])
	{
		wasm.nullsurf_native = tr.surface;
		out->surface = WASM_BUFFERS_OFFSET(nullsurf);
		return;
	}
	
	const int32_t native_hash = ((ptrdiff_t) tr.surface) & (SURF_CACHE_HASH_SIZE - 1);

	for (surf_cache_entry_t *entry = surf_cache.native_hash[native_hash]; entry; entry = entry->next_native)
	{
		if (entry->native == tr.surface)
		{
			out->surface = entry->wasm;
			return;
		}
	}

	surf_cache_entry_t *entry = (surf_cache_entry_t *) gi.TagMalloc(sizeof(surf_cache_entry_t), TAG_LEVEL);
	
	entry->next_native = surf_cache.native_hash[native_hash];
	surf_cache.native_hash[native_hash] = entry;

	entry->native = tr.surface;

	csurface_t *wasm_surf;

	out->surface = entry->wasm = wasm_runtime_module_malloc(wasm.module_inst, sizeof(csurface_t), (void **) &wasm_surf);

	if (!out->surface)
		wasm_error("Out of WASM memory");

	*wasm_surf = *tr.surface;
	
	const int32_t wasm_hash = entry->wasm & (SURF_CACHE_HASH_SIZE - 1);

	entry->next_wasm = surf_cache.wasm_hash[wasm_hash];
	surf_cache.wasm_hash[wasm_hash] = entry;
}

void q2_wasm_clear_surface_cache()
{
	memset(&surf_cache, 0, sizeof(surf_cache));
}

typedef struct
{
	// state (in / out)
	wasm_pmove_state_t	s;

	// command (in)
	usercmd_t	cmd;
	qboolean	snapinitial;    // if s has been changed outside pmove

	// results (out)
	int32_t	numtouch;
	wasm_entity_address_t touchents[MAX_TOUCH];

	vec3_t	viewangles;         // clamped
	vec_t	viewheight;

	vec3_t	mins, maxs;         // bounding box size

	wasm_entity_address_t groundentity;
	int32_t	watertype;
	int32_t	waterlevel;

	// callbacks to test the world
	wasm_function_pointer_t trace;
	wasm_function_pointer_t pointcontents;
} wasm_pmove_t;

static trace_t q2_wasm_pmove_trace(const vec3_t *start, const vec3_t *mins, const vec3_t *maxs, const vec3_t *end)
{
	uint32_t args[] = {
		wasm_pmove_ptr,
		ftoui32(start->x), ftoui32(start->y), ftoui32(start->z),
		ftoui32(mins->x), ftoui32(mins->y), ftoui32(mins->z),
		ftoui32(maxs->x), ftoui32(maxs->y), ftoui32(maxs->z),
		ftoui32(end->x), ftoui32(end->y), ftoui32(end->z),
		WASM_BUFFERS_OFFSET(trace)
	};

	wasm_call_args(wasm.WASM_PmoveTrace, args, lengthof(args));

	const wasm_buffers_t *buffers = wasm_buffers();
	const wasm_trace_t *wtr = &buffers->trace;
	static trace_t tr;

	tr.allsolid = wtr->allsolid;
	tr.contents = wtr->contents;
	tr.endpos = wtr->endpos;
	tr.ent = entity_wa_to_np(wtr->ent);
	tr.fraction = wtr->fraction;
	tr.plane = wtr->plane;
	tr.startsolid = wtr->startsolid;

	if (wtr->surface == WASM_BUFFERS_OFFSET(nullsurf))
		tr.surface = wasm.nullsurf_native;
	else
	{
		const int32_t wasm_hash = wtr->surface & (SURF_CACHE_HASH_SIZE - 1);

		for (surf_cache_entry_t *entry = surf_cache.wasm_hash[wasm_hash]; entry; entry = entry->next_wasm)
		{
			if (entry->wasm == wtr->surface)
			{
				tr.surface = entry->native;
				break;
			}
		}
	}

	return tr;
}

static content_flags_t q2_wasm_pmove_pointcontents(const vec3_t *start)
{
	uint32_t args[] = {
		wasm_pmove_ptr,
		ftoui32(start->x), ftoui32(start->y), ftoui32(start->z)
	};

	wasm_call_args(wasm.WASM_PmovePointContents, args, lengthof(args));

	return args[0];
}

static void q2_Pmove(wasm_exec_env_t env, wasm_pmove_t *wasm_pmove)
{
	if (!wasm_validate_ptr(wasm_pmove, sizeof(wasm_pmove_t)))
		wasm_error("Invalid pointer");

	static pmove_t pm;
	sync_pmove_state_wasm_to_native(&pm.s, &wasm_pmove->s);
	pm.cmd = wasm_pmove->cmd;
	pm.snapinitial = wasm_pmove->snapinitial;

	pm.trace = q2_wasm_pmove_trace;
	pm.pointcontents = q2_wasm_pmove_pointcontents;

	wasm_pmove_ptr = wasm_native_to_addr(wasm_pmove);

	gi.Pmove(&pm);

	sync_pmove_state_native_to_wasm(&wasm_pmove->s, &pm.s);
	wasm_pmove->numtouch = pm.numtouch;
	for (int32_t i = 0; i < wasm_pmove->numtouch; i++)
		wasm_pmove->touchents[i] = entity_np_to_wa(pm.touchents[i]);
	wasm_pmove->viewangles = pm.viewangles;
	wasm_pmove->viewheight = pm.viewheight;
	wasm_pmove->mins = pm.mins;
	wasm_pmove->maxs = pm.maxs;
	wasm_pmove->groundentity = entity_np_to_wa(pm.groundentity);
	wasm_pmove->watertype = pm.watertype;
	wasm_pmove->waterlevel = pm.waterlevel;
}

static content_flags_t q2_pointcontents(wasm_exec_env_t env, const vec_t p_x, const vec_t p_y, const vec_t p_z)
{
	const vec3_t p = { p_x, p_y, p_z };
	return gi.pointcontents(&p);
}

static void q2_WriteAngle(wasm_exec_env_t env, vec_t c)
{
	gi.WriteAngle(c);
}

static void q2_WriteByte(wasm_exec_env_t env, int32_t c)
{
	gi.WriteByte(c);
}

static void q2_WriteChar(wasm_exec_env_t env, int32_t c)
{
	gi.WriteChar(c);
}

static void q2_WriteDir(wasm_exec_env_t env, const vec_t p_x, const vec_t p_y, const vec_t p_z)
{
	const vec3_t p = { p_x, p_y, p_z };
	gi.WriteDir(&p);
}

static void q2_WriteFloat(wasm_exec_env_t env, vec_t p)
{
	gi.WriteFloat(p);
}

static void q2_WriteLong(wasm_exec_env_t env, int32_t p)
{
	gi.WriteLong(p);
}

static void q2_WritePosition(wasm_exec_env_t env, const vec_t p_x, const vec_t p_y, const vec_t p_z)
{
	const vec3_t p = { p_x, p_y, p_z };
	gi.WritePosition(&p);
}

static void q2_WriteShort(wasm_exec_env_t env, int32_t p)
{
	gi.WriteShort(p);
}

static void q2_WriteString(wasm_exec_env_t env, const char *p)
{
	gi.WriteString(p);
}

static void q2_unicast(wasm_exec_env_t env, wasm_edict_t *ent, qboolean reliable)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");

	gi.unicast(entity_wnp_to_np(ent), reliable);
}

static void q2_multicast(wasm_exec_env_t env, const vec_t origin_x, const vec_t origin_y, const vec_t origin_z, multicast_t to)
{
	const vec3_t origin = { origin_x, origin_y, origin_z };
	gi.multicast(&origin, to);
}

static int32_t q2_BoxEdicts(wasm_exec_env_t env, const vec_t mins_x, const vec_t mins_y, const vec_t mins_z, const vec_t maxs_x, const vec_t maxs_y, const vec_t maxs_z, uint32_t *list, int32_t maxcount, box_edicts_area_t areatype)
{
	if (!wasm_validate_ptr(list, sizeof(uint32_t) * maxcount))
		wasm_error("Invalid pointer");

	for (int32_t i = 0; i < globals.num_edicts; i++)
	{
		wasm_edict_t *e = entity_number_to_wnp(i);
		edict_t *n = entity_number_to_np(i);

		if (e->inuse)
			sync_entity(e, n, false);
	}

	static edict_t *elist[MAX_EDICTS];

	const vec3_t mins = { mins_x, mins_y, mins_z };
	const vec3_t maxs = { maxs_x, maxs_y, maxs_z };
	int32_t count = gi.BoxEdicts(&mins, &maxs, elist, maxcount, areatype);

	for (int32_t i = 0; i < count; i++)
		list[i] = entity_np_to_wa(elist[i]);

	return count;
}

static void q2_sound(wasm_exec_env_t env, wasm_edict_t *ent, sound_channel_t channel, int32_t soundindex, vec_t volume, sound_attn_t attenuation, vec_t timeofs)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");

	edict_t *native = entity_wnp_to_np(ent);

	if (native)
		sync_entity(ent, native, false);

	gi.sound(native, channel, soundindex, volume, attenuation, timeofs);
}

static void q2_positioned_sound(wasm_exec_env_t env, vec_t origin_x, vec_t origin_y, vec_t origin_z, wasm_edict_t *ent, sound_channel_t channel, int32_t soundindex, vec_t volume, sound_attn_t attenuation, vec_t timeofs)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");

	edict_t *native = entity_wnp_to_np(ent);

	if (native)
		sync_entity(ent, native, false);

	const vec3_t origin = { origin_x, origin_y, origin_z };
	gi.positioned_sound(&origin, native, channel, soundindex, volume, attenuation, timeofs);
}

static int32_t q2_argc(wasm_exec_env_t env)
{
	return gi.argc();
}

static uint32_t q2_argv(wasm_exec_env_t env, int32_t i)
{
	if (i < 0 || i >= lengthof(wasm_buffers()->cmds))
		return 0;

	return WASM_BUFFERS_OFFSET(cmds[i]);
}

static uint32_t q2_args(wasm_exec_env_t env)
{
	return WASM_BUFFERS_OFFSET(scmd);
}

static void q2_AddCommandString(wasm_exec_env_t env, const char *str)
{
	gi.AddCommandString(str);
}

static qboolean q2_AreasConnected(wasm_exec_env_t env, int32_t a, int32_t b)
{
	return gi.AreasConnected(a, b);
}

static qboolean q2_inPHS(wasm_exec_env_t env, const vec_t a_x, const vec_t a_y, const vec_t a_z, const vec_t b_x, const vec_t b_y, const vec_t b_z)
{
	const vec3_t a = { a_x, a_y, a_z };
	const vec3_t b = { b_x, b_y, b_z };
	return gi.inPHS(&a, &b);
}

static qboolean q2_inPVS(wasm_exec_env_t env, const vec_t a_x, const vec_t a_y, const vec_t a_z, const vec_t b_x, const vec_t b_y, const vec_t b_z)
{
	const vec3_t a = { a_x, a_y, a_z };
	const vec3_t b = { b_x, b_y, b_z };
	return gi.inPVS(&a, &b);
}

static void q2_SetAreaPortalState(wasm_exec_env_t env, int32_t portal, qboolean state)
{
	gi.SetAreaPortalState(portal, state);
}

static void q2_DebugGraph(wasm_exec_env_t env, vec_t a, int32_t b)
{
}

#define SYMBOL(name, sig) \
	{ #name, (void *) q2_ ## name, sig, NULL }

static NativeSymbol native_symbols[] = {
	SYMBOL(dprint, "($)"),
	SYMBOL(bprint, "(i$)"),
	SYMBOL(cprint, "(*i$)"),
	SYMBOL(error, "(*)"),
	SYMBOL(centerprint, "(*$)"),
	SYMBOL(cvar, "($$i)i"),
	SYMBOL(cvar_set, "($$)i"),
	SYMBOL(cvar_forceset, "($$)i"),
	SYMBOL(TagMalloc, "(ii)i"),
	SYMBOL(TagFree, "(*)"),
	SYMBOL(FreeTags, "(i)"),
	SYMBOL(configstring, "(i$)"),
	SYMBOL(modelindex, "($)i"),
	SYMBOL(imageindex, "($)i"),
	SYMBOL(soundindex, "($)i"),
	SYMBOL(linkentity, "(*)"),
	SYMBOL(unlinkentity, "(*)"),
	SYMBOL(setmodel, "(*$)"),
	SYMBOL(Pmove, "(*)"),
	SYMBOL(trace, "(ffffffffffff*i*)"),
	SYMBOL(pointcontents, "(fff)i"),
	SYMBOL(WriteAngle, "(f)"),
	SYMBOL(WriteByte, "(i)"),
	SYMBOL(WriteChar, "(i)"),
	SYMBOL(WriteDir, "(fff)"),
	SYMBOL(WriteFloat, "(f)"),
	SYMBOL(WriteLong, "(i)"),
	SYMBOL(WritePosition, "(fff)"),
	SYMBOL(WriteShort, "(i)"),
	SYMBOL(WriteString, "($)"),
	SYMBOL(unicast, "(*i)"),
	SYMBOL(multicast, "(fffi)"),
	SYMBOL(BoxEdicts, "(ffffff*ii)i"),
	SYMBOL(sound, "(*iifff)"),
	SYMBOL(positioned_sound, "(fff*iifff)"),
	SYMBOL(argc, "()i"),
	SYMBOL(argv, "(i)i"),
	SYMBOL(args, "()i"),
	SYMBOL(AddCommandString, "($)"),
	SYMBOL(AreasConnected, "(ii)i"),
	SYMBOL(inPHS, "(ffffff)i"),
	SYMBOL(inPVS, "(ffffff)i"),
	SYMBOL(SetAreaPortalState, "(ii)"),
	SYMBOL(DebugGraph, "(fi)")
};

int32_t RegisterApiNatives()
{
	return wasm_runtime_register_natives("q2", native_symbols, lengthof(native_symbols));
}