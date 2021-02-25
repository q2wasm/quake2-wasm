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
#include "shared/entity.h"
#include "g_main.h"

#include <wasm_export.h>
}

#include "g_wasm.hh"

// Cvars are handled as heap copies given to the WASM runtime.
// Every now and then, we check to see if we have to update the cvar.
#include <unordered_map>
#include <string>
#include <string_view>

struct wasm_mapped_cvar_t
{
	cvar_t				*native = nullptr;
	wasm_cvar_t			*wasm = nullptr;
	char				*string_ptr = nullptr;
	size_t				string_size = 0;
	char				*latched_string_ptr = nullptr;
	size_t				latched_string_size = 0;
	wasm_app_address_t	wasm_ptr = 0;
};

static std::unordered_map<std::string, wasm_mapped_cvar_t> mapped_cvars;

static wasm_mapped_cvar_t &fetch_mapped_cvar(const char *name)
{
	auto it = mapped_cvars.find(name);

	if (it != mapped_cvars.end())
		return (*it).second;

	return (*mapped_cvars.insert({
		name,
		{}
	}).first).second;
}

static void update_cvar_string(const char *src, char *&dst, size_t &dst_size, uint32_t &dst_ptr)
{
	if (!dst && !src)
		return;
	else if (dst && !src)
	{
		wasm_runtime_module_free(wasm.module_inst, dst_ptr);
		dst = nullptr;
		dst_ptr = 0;
		dst_size = 0;
		return;
	}

	if (dst && strcmp(dst, src) == 0)
		return;

	const size_t length = strlen(src);

	if (length + 1 > dst_size)
	{
		if (dst_ptr)
			wasm_runtime_module_free(wasm.module_inst, dst_ptr);
		
		dst_size = length + 1;
		dst_ptr = wasm_runtime_module_malloc(wasm.module_inst, dst_size, (void **) &dst);

		if (!dst_ptr)
			wasm_error("Out of WASM memory");
	}

	memcpy(dst, src, dst_size);
}

static void update_mapped_cvar(wasm_mapped_cvar_t &cvar)
{
	cvar.wasm->flags = cvar.native->flags;
	cvar.wasm->modified = cvar.native->modified;
	cvar.wasm->value = cvar.native->value;
	cvar.wasm->intVal = (int) cvar.native->value;
	
	update_cvar_string(cvar.native->string, cvar.string_ptr, cvar.string_size, cvar.wasm->string);
	update_cvar_string(cvar.native->latched_string, cvar.latched_string_ptr, cvar.latched_string_size, cvar.wasm->latched_string);
}

// For tracing, we use two maps to keep track of surfaces that we've already seen.
// The address -> native map is used specifically for gi.Pmove, which needs
// to be able to convert them back.
static std::unordered_map<csurface_t*, wasm_surface_address_t> native_surf_to_wasm_surf;
static std::unordered_map<wasm_surface_address_t, csurface_t*> wasm_surf_to_native_surf;

// A pointer to the currently-processing pmove.
static wasm_pmove_t *wasm_pmove_ptr;

static void q2_dprint(wasm_exec_env_t, const char *str)
{
	gi.dprintf("%s", str);
}

static void q2_bprint(wasm_exec_env_t, print_level_t print_level, const char *str)
{
	gi.bprintf(print_level, "%s", str);
}

static uint32_t q2_cvar(wasm_exec_env_t, const char *name, const char *value, const int32_t flags)
{
	auto mapped = fetch_mapped_cvar(name);

	if (!mapped.native)
	{
		mapped.native = gi.cvar(name, value, flags);
		mapped.wasm_ptr = wasm_runtime_module_malloc(wasm.module_inst, sizeof(wasm_cvar_t), (void **) &mapped.wasm);

		if (!mapped.wasm_ptr)
			wasm_error("Out of WASM memory");

		memset(mapped.wasm, 0, sizeof(wasm_cvar_t));
		mapped.wasm->name = wasm_dup_str(mapped.native->name);
	}

	update_mapped_cvar(mapped);
	return mapped.wasm_ptr;
}

static uint32_t q2_cvar_set(wasm_exec_env_t, const char *name, const char *value)
{
	auto mapped = fetch_mapped_cvar(name);

	if (!mapped.native)
	{
		mapped.native = gi.cvar_set(name, value);
		mapped.wasm_ptr = wasm_runtime_module_malloc(wasm.module_inst, sizeof(wasm_cvar_t), (void **) &mapped.wasm);

		if (!mapped.wasm_ptr)
			wasm_error("Out of WASM memory");

		memset(mapped.wasm, 0, sizeof(wasm_cvar_t));
		mapped.wasm->name = wasm_dup_str(mapped.native->name);
	}

	update_mapped_cvar(mapped);
	return mapped.wasm_ptr;
}

static uint32_t q2_cvar_forceset(wasm_exec_env_t, const char *name, const char *value)
{
	auto mapped = fetch_mapped_cvar(name);

	if (!mapped.native)
	{
		mapped.native = gi.cvar_forceset(name, value);
		mapped.wasm_ptr = wasm_runtime_module_malloc(wasm.module_inst, sizeof(wasm_cvar_t), (void **) &mapped.wasm);

		if (!mapped.wasm_ptr)
			wasm_error("Out of WASM memory");

		memset(mapped.wasm, 0, sizeof(wasm_cvar_t));
		mapped.wasm->name = wasm_dup_str(mapped.native->name);
	}

	update_mapped_cvar(mapped);
	return mapped.wasm_ptr;
}

#include <list>
#include <unordered_set>

struct tagged_block
{
	uint32_t memory;
	void *ptr;
	uint32_t tag;
};

static std::list<tagged_block> tagged_blocks;
static std::unordered_map<uint32_t, std::unordered_set<void *>> tagged_entries;
static std::unordered_map<void *, std::list<tagged_block>::iterator> memory_map;

static uint32_t q2_TagMalloc(wasm_exec_env_t, uint32_t size, uint32_t tag)
{
	void *ptr;
	uint32_t loc = wasm_runtime_module_malloc(wasm.module_inst, size, &ptr);

	if (!loc)
		wasm_error("Out of WASM memory");
	
	memset(ptr, 0, size);

	auto it = tagged_blocks.insert(tagged_blocks.begin(), tagged_block {
		.memory = loc,
		.ptr = ptr,
		.tag = tag
	});

	auto hash = tagged_entries.find(tag);

	if (hash == tagged_entries.end())
		tagged_entries.insert({ tag, { ptr } });
	else
		(*hash).second.insert(ptr);

	memory_map.insert({ ptr, it });

	return loc;
}

static void q2_TagFree(wasm_exec_env_t, void *ptr)
{
	wasm_runtime_module_free(wasm.module_inst, wasm_native_to_addr(ptr));

	auto it = memory_map.at(ptr);
	memory_map.erase(ptr);
	(*tagged_entries.find((*it).tag)).second.erase(ptr);
	tagged_blocks.erase(it);
}

static void q2_FreeTags(wasm_exec_env_t, uint32_t tag)
{
	auto blocks = tagged_entries.find(tag);

	if (blocks == tagged_entries.end())
		return;

	for (auto ptr : (*blocks).second)
	{
		tagged_blocks.erase((*memory_map.find(ptr)).second);
		wasm_runtime_module_free(wasm.module_inst, wasm_native_to_addr(ptr));
		memory_map.erase(ptr);
	}

	tagged_entries.erase(tag);
}

static void q2_configstring(wasm_exec_env_t, int32_t id, const char *value)
{
	gi.configstring(id, value);
}

static int32_t q2_modelindex(wasm_exec_env_t, const char *value)
{
	return gi.modelindex(value);
}

static int32_t q2_imageindex(wasm_exec_env_t, const char *value)
{
	return gi.imageindex(value);
}

static int32_t q2_soundindex(wasm_exec_env_t, const char *value)
{
	return gi.soundindex(value);
}

static void q2_cprint(wasm_exec_env_t, wasm_edict_t *ent, print_level_t print_level, const char *str)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");

	gi.cprintf(entity_wnp_to_np(ent), print_level, "%s", str);
}

static void q2_centerprint(wasm_exec_env_t, wasm_edict_t *ent, const char *str)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");

	gi.centerprintf(entity_wnp_to_np(ent), "%s", str);
}

static void q2_error(wasm_exec_env_t, const char *str)
{
	wasm_error(str);
}

static void q2_linkentity(wasm_exec_env_t, wasm_edict_t *wasm_edict)
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

static void q2_unlinkentity(wasm_exec_env_t, wasm_edict_t *wasm_edict)
{
	if (!entity_validate_wnp(wasm_edict))
		wasm_error("Invalid pointer");

	edict_t *native_edict = entity_wnp_to_np(wasm_edict);

	copy_link_wasm_to_native(native_edict, wasm_edict);
	gi.unlinkentity(native_edict);
	copy_link_native_to_wasm(wasm_edict, native_edict);
}

static void q2_setmodel(wasm_exec_env_t, wasm_edict_t *wasm_edict, const char *model)
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

static void q2_trace(wasm_exec_env_t, const vec3_t *start, const vec3_t *mins, const vec3_t *maxs, const vec3_t *end, wasm_edict_t *passent, content_flags_t contentmask, wasm_trace_t *out)
{
	if (!wasm_validate_ptr(start, sizeof(vec3_t)))
		wasm_error("Invalid pointer");
	if (!wasm_validate_ptr(end, sizeof(vec3_t)))
		wasm_error("Invalid pointer");
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
	
	if (wasm_native_to_addr((void *) mins) == 0)
		mins = &zero;
	else if (!wasm_validate_ptr(mins, sizeof(vec3_t)))
		wasm_error("Invalid pointer");

	if (wasm_native_to_addr((void *) maxs) == 0)
		maxs = &zero;
	else if (!wasm_validate_ptr(maxs, sizeof(vec3_t)))
		wasm_error("Invalid pointer");

	edict_t *native_passent;

	if (wasm_native_to_addr(passent) == 0)
		native_passent = globals.edicts;
	else
		native_passent = entity_wnp_to_np(passent);

	static trace_t tr;
	tr = gi.trace(start, mins, maxs, end, native_passent, contentmask);

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
		out->surface = wasm.nullsurf_ptr;
		return;
	}

	auto found = native_surf_to_wasm_surf.find(tr.surface);

	if (found == native_surf_to_wasm_surf.end())
	{
		csurface_t *wasm_surf;
		out->surface = wasm_runtime_module_malloc(wasm.module_inst, sizeof(csurface_t), (void **) &wasm_surf);

		if (!out->surface)
			wasm_error("Out of WASM memory");

		*wasm_surf = *tr.surface;
		native_surf_to_wasm_surf.insert({ tr.surface, out->surface });
		wasm_surf_to_native_surf.insert({ out->surface, tr.surface });
	}
	else
		out->surface = (*found).second;
}

void q2_wasm_clear_surface_cache()
{
	native_surf_to_wasm_surf.clear();
	wasm_surf_to_native_surf.clear();
}

static trace_t q2_wasm_pmove_trace(const vec3_t *start, const vec3_t *mins, const vec3_t *maxs, const vec3_t *end)
{
	wasm.vectors_buf[0] = *start;
	wasm.vectors_buf[1] = *mins;
	wasm.vectors_buf[2] = *maxs;
	wasm.vectors_buf[3] = *end;

	uint32_t args[] = {
		wasm_native_to_addr(wasm_pmove_ptr),
		wasm.vectors_ptr,
		wasm.vectors_ptr + sizeof(vec3_t),
		wasm.vectors_ptr + sizeof(vec3_t) * 2,
		wasm.vectors_ptr + sizeof(vec3_t) * 3,
		wasm.trace_ptr
	};

	wasm_call(wasm.WASM_PmoveTrace, args);

	static trace_t tr;

	tr.allsolid = wasm.trace_buf->allsolid;
	tr.contents = wasm.trace_buf->contents;
	tr.endpos = wasm.trace_buf->endpos;
	tr.ent = entity_wa_to_np(wasm.trace_buf->ent);
	tr.fraction = wasm.trace_buf->fraction;
	tr.plane = wasm.trace_buf->plane;
	tr.startsolid = wasm.trace_buf->startsolid;

	if (wasm.trace_buf->surface == wasm.nullsurf_ptr)
		tr.surface = wasm.nullsurf_native;
	else
		tr.surface = (*wasm_surf_to_native_surf.find(wasm.trace_buf->surface)).second;

	return tr;
}

static content_flags_t q2_wasm_pmove_pointcontents(const vec3_t *start)
{
	wasm.vectors_buf[0] = *start;

	uint32_t args[] = {
		wasm_native_to_addr(wasm_pmove_ptr),
		wasm.vectors_ptr
	};

	wasm_call(wasm.WASM_PmovePointContents, args);

	return args[0];
}

static void q2_Pmove(wasm_exec_env_t, wasm_pmove_t *wasm_pmove)
{
	if (!wasm_validate_ptr(wasm_pmove, sizeof(wasm_pmove_t)))
		wasm_error("Invalid pointer");

	static pmove_t pm;

	pm.s = wasm_pmove->s;
	pm.cmd = wasm_pmove->cmd;
	pm.snapinitial = wasm_pmove->snapinitial;

	pm.trace = q2_wasm_pmove_trace;
	pm.pointcontents = q2_wasm_pmove_pointcontents;

	wasm_pmove_ptr = wasm_pmove;

	gi.Pmove(&pm);

	wasm_pmove->s = pm.s;
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

static content_flags_t q2_pointcontents(wasm_exec_env_t, const vec3_t *p)
{
	if (!wasm_validate_ptr(p, sizeof(vec3_t)))
		wasm_error("Invalid pointer");

	return gi.pointcontents(p);
}

static void q2_WriteAngle(wasm_exec_env_t, vec_t c)
{
	gi.WriteAngle(c);
}

static void q2_WriteByte(wasm_exec_env_t, int32_t c)
{
	gi.WriteByte(c);
}

static void q2_WriteChar(wasm_exec_env_t, int32_t c)
{
	gi.WriteChar(c);
}

static void q2_WriteDir(wasm_exec_env_t, const vec3_t *p)
{
	if (!wasm_validate_ptr(p, sizeof(vec3_t)))
		wasm_error("Invalid pointer");

	gi.WriteDir(p);
}

static void q2_WriteFloat(wasm_exec_env_t, vec_t p)
{
	gi.WriteFloat(p);
}

static void q2_WriteLong(wasm_exec_env_t, int32_t p)
{
	gi.WriteLong(p);
}

static void q2_WritePosition(wasm_exec_env_t, const vec3_t *p)
{
	if (!wasm_validate_ptr(p, sizeof(vec3_t)))
		wasm_error("Invalid pointer");

	gi.WritePosition(p);
}

static void q2_WriteShort(wasm_exec_env_t, int32_t p)
{
	gi.WriteShort(p);
}

static void q2_WriteString(wasm_exec_env_t, const char *p)
{
	gi.WriteString(p);
}

static void q2_unicast(wasm_exec_env_t, wasm_edict_t *ent, qboolean reliable)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");

	gi.unicast(entity_wnp_to_np(ent), reliable);
}

static void q2_multicast(wasm_exec_env_t, const vec3_t *origin, multicast_t to)
{
	if (!wasm_validate_ptr(origin, sizeof(vec3_t)))
		wasm_error("Invalid pointer");

	gi.multicast(origin, to);
}

static int32_t q2_BoxEdicts(wasm_exec_env_t, const vec3_t *mins, const vec3_t *maxs, uint32_t *list, int32_t maxcount, box_edicts_area_t areatype)
{
	if (!wasm_validate_ptr(mins, sizeof(vec3_t)))
		wasm_error("Invalid pointer");
	if (!wasm_validate_ptr(maxs, sizeof(vec3_t)))
		wasm_error("Invalid pointer");
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

	int32_t count = gi.BoxEdicts(mins, maxs, elist, maxcount, areatype);

	for (int32_t i = 0; i < count; i++)
		list[i] = entity_np_to_wa(elist[i]);

	return count;
}

static void q2_sound(wasm_exec_env_t, wasm_edict_t *ent, sound_channel_t channel, int32_t soundindex, vec_t volume, sound_attn_t attenuation, vec_t timeofs)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");

	edict_t *native = entity_wnp_to_np(ent);

	if (native)
		sync_entity(ent, native, false);

	gi.sound(native, channel, soundindex, volume, attenuation, timeofs);
}

static void q2_positioned_sound(wasm_exec_env_t, const vec3_t *origin, wasm_edict_t *ent, sound_channel_t channel, int32_t soundindex, vec_t volume, sound_attn_t attenuation, vec_t timeofs)
{
	if (!entity_validate_wnp(ent))
		wasm_error("Invalid pointer");
	if (!wasm_validate_ptr(origin, sizeof(vec3_t)))
		wasm_error("Invalid pointer");

	edict_t *native = entity_wnp_to_np(ent);

	if (native)
		sync_entity(ent, native, false);

	gi.positioned_sound(origin, native, channel, soundindex, volume, attenuation, timeofs);
}

static int32_t q2_argc(wasm_exec_env_t)
{
	return gi.argc();
}

static uint32_t q2_argv(wasm_exec_env_t, int32_t i)
{
	if (i < 0 || i >= sizeof(wasm.cmd_bufs) / sizeof(*wasm.cmd_bufs))
		return 0;

	return wasm.cmd_ptrs[i];
}

static uint32_t q2_args(wasm_exec_env_t)
{
	return wasm.scmd_ptr;
}

static void q2_AddCommandString(wasm_exec_env_t, const char *str)
{
	gi.AddCommandString(str);
}

static qboolean q2_AreasConnected(wasm_exec_env_t, int32_t a, int32_t b)
{
	return gi.AreasConnected(a, b);
}

static qboolean q2_inPHS(wasm_exec_env_t, const vec3_t *a, const vec3_t *b)
{
	if (!wasm_validate_ptr(a, sizeof(vec3_t)))
		wasm_error("Invalid pointer");
	if (!wasm_validate_ptr(b, sizeof(vec3_t)))
		wasm_error("Invalid pointer");

	return gi.inPHS(a, b);
}

static qboolean q2_inPVS(wasm_exec_env_t, const vec3_t *a, const vec3_t *b)
{
	if (!wasm_validate_ptr(a, sizeof(vec3_t)))
		wasm_error("Invalid pointer");
	if (!wasm_validate_ptr(b, sizeof(vec3_t)))
		wasm_error("Invalid pointer");

	return gi.inPVS(a, b);
}

static void q2_SetAreaPortalState(wasm_exec_env_t, int32_t portal, qboolean state)
{
	gi.SetAreaPortalState(portal, state);
}

#define SYMBOL(name, sig) \
	{ #name, (void *) q2_ ## name, sig, nullptr }

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
	SYMBOL(trace, "(*****i*)"),
	SYMBOL(pointcontents, "(*)i"),
	SYMBOL(WriteAngle, "(f)"),
	SYMBOL(WriteByte, "(i)"),
	SYMBOL(WriteChar, "(i)"),
	SYMBOL(WriteDir, "(*)"),
	SYMBOL(WriteFloat, "(f)"),
	SYMBOL(WriteLong, "(i)"),
	SYMBOL(WritePosition, "(*)"),
	SYMBOL(WriteShort, "(i)"),
	SYMBOL(WriteString, "($)"),
	SYMBOL(unicast, "(*i)"),
	SYMBOL(multicast, "(*i)"),
	SYMBOL(BoxEdicts, "(***ii)i"),
	SYMBOL(sound, "(*iifff)"),
	SYMBOL(positioned_sound, "(**iifff)"),
	SYMBOL(argc, "()i"),
	SYMBOL(argv, "(i)i"),
	SYMBOL(args, "()i"),
	SYMBOL(AddCommandString, "($)"),
	SYMBOL(AreasConnected, "(ii)i"),
	SYMBOL(inPHS, "(**)i"),
	SYMBOL(inPVS, "(**)i"),
	SYMBOL(SetAreaPortalState, "(ii)")
};

int32_t RegisterApiNatives()
{
	return wasm_runtime_register_natives("q2", native_symbols, sizeof(native_symbols) / sizeof(*native_symbols));
}