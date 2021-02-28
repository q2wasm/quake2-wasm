#pragma once

// This file contains structures that are being eaten natively from the
// WASM sandbox, and functions to handle WASM data.

#include <string.h>
#include "game/api.h"
#include "shared/entity.h"
#include "shared/client.h"
#include <wasm_export.h>

// Address relative to wasm heap.
typedef uint32_t wasm_app_address_t;

// Address relative to wasm heap. Do not use in WASM structs!
typedef void *wasm_app_pointer_t;

// Address relative to wasm heap.
typedef wasm_app_address_t wasm_entity_address_t;

// Address relative to wasm heap.
typedef wasm_app_address_t wasm_surface_address_t;

// Trace data.
typedef struct
{
	qboolean				allsolid;
	qboolean				startsolid;
	vec_t					fraction;
	vec3_t					endpos;
	cplane_t				plane;
	wasm_surface_address_t	surface;
	content_flags_t			contents;
	wasm_entity_address_t	ent;
} wasm_trace_t;

// WASM environment data. Stored globally.
typedef struct
{
	wasm_module_t wasm_module;
	wasm_module_inst_t module_inst;
	wasm_exec_env_t exec_env;
	uint8_t *assembly;
	char error_buf[128];

	wasm_app_address_t edict_base, edict_end;
	int32_t edict_size;
	int32_t *num_edicts;
	int32_t max_edicts;

	// The various buffers below are used to store data that we read/write to
	// to communicate to WASM, since we can't pass native pointers through.
	usercmd_t			*ucmd_buf;
	wasm_app_address_t	ucmd_ptr;

	char				*userinfo_buf;
	wasm_app_address_t	userinfo_ptr;

	// there's four vectors stored here.
	vec3_t				*vectors_buf;
	wasm_app_address_t	vectors_ptr;

	wasm_trace_t			*trace_buf;
	wasm_app_address_t		trace_ptr;

	char				*cmd_bufs[16];
	wasm_app_address_t	cmd_ptrs[16];
	char				*scmd_buf;
	wasm_app_address_t	scmd_ptr;

	csurface_t				*nullsurf_buf, *nullsurf_native;
	wasm_surface_address_t	nullsurf_ptr;

	// Function pointers from WASM that we store.
	wasm_function_inst_t WASM_PmoveTrace, WASM_PmovePointContents, InitWASMAPI, WASM_Init, WASM_SpawnEntities, WASM_ClientConnect,
		WASM_ClientBegin, WASM_ClientUserinfoChanged, WASM_ClientDisconnect, WASM_ClientCommand, WASM_ClientThink, WASM_RunFrame, WASM_ServerCommand,
		WASM_WriteGame, WASM_ReadGame, WASM_WriteLevel, WASM_ReadLevel;
} wasm_env_t;

extern wasm_env_t wasm;

// convenience functions
static inline void *wasm_addr_to_native(wasm_app_address_t address)
{
	return wasm_runtime_addr_app_to_native(wasm.module_inst, address);
}

static inline void *wasm_pointer_to_native(wasm_app_pointer_t address)
{
	return wasm_runtime_addr_app_to_native(wasm.module_inst, (uint32_t) address);
}

static inline wasm_app_address_t wasm_native_to_addr(void *native)
{
	return (wasm_app_address_t)wasm_runtime_addr_native_to_app(wasm.module_inst, native);
}

static inline wasm_app_pointer_t wasm_native_to_pointer(void *native)
{
	return (wasm_app_pointer_t)wasm_runtime_addr_native_to_app(wasm.module_inst, native);
}

static inline bool wasm_validate_addr(uint32_t addr, uint32_t size)
{
	return wasm_runtime_validate_app_addr(wasm.module_inst, addr, size);
}

static inline bool wasm_validate_ptr(const void *ptr, uint32_t size)
{
	return wasm_runtime_validate_native_addr(wasm.module_inst, (void *) ptr, size);
}

// Address relative to wasm heap.
typedef wasm_app_address_t wasm_string_t;

// duplicates a string from a native address into WASM heap memory
static inline wasm_string_t wasm_dup_str(const char *str)
{
	wasm_string_t s = wasm_runtime_module_dup_data(wasm.module_inst, str, strlen(str) + 1);

	if (!s)
		wasm_error("Out of WASM memory");

	return s;
}

typedef struct
{
	wasm_string_t	name;
	wasm_string_t	string;
	wasm_string_t	latched_string;
	cvar_flags_t	flags;
	qboolean		modified;
	vec_t			value;
	// This is here for compatibility with EGL/q2pro-based mods that
	// have this additional struct member and may have used it.
	int32_t			intVal;
} wasm_cvar_t;

struct wasm_list_t
{
    uint32_t next, prev;
};

struct wasm_edict_t
{
	entity_state_t	s;
	uint32_t		client;
	qboolean		inuse;
	int32_t			linkcount;

	wasm_list_t	area;

	int32_t	num_clusters;
	int32_t	clusternums[MAX_ENT_CLUSTERS];
	int32_t	headnode;
	int32_t	areanum, areanum2;

	svflags_t				svflags;
	vec3_t					mins, maxs;
	vec3_t					absmin, absmax, size;
	solid_t					solid;
	content_flags_t			clipmask;
	wasm_entity_address_t	owner;
};

// for brevity sake, the following functions use acroynms.
// n: entity number
// np: native pointer (edict_t* from globals)
// wnp: wasm native pointer (wasm_edict_t*, absolute from WASM heap)
// wa: wasm address (uint32_t, relative offset from WASM heap)

static inline edict_t *entity_number_to_np(int32_t number)
{
	return &globals.edicts[number];
}

static inline wasm_entity_address_t entity_number_to_wa(int32_t number)
{
	return wasm.edict_base + (number * wasm.edict_size);
}

static inline int32_t entity_wa_to_number(wasm_entity_address_t edict_offset)
{
	return (edict_offset - wasm.edict_base) / wasm.edict_size;
}

static inline wasm_entity_address_t entity_wnp_to_wa(wasm_edict_t *wasm_edict)
{
	return wasm_native_to_addr(wasm_edict);
}

static inline int32_t entity_wnp_to_number(wasm_edict_t *wasm_edict)
{
	return entity_wa_to_number(entity_wnp_to_wa(wasm_edict));
}

inline wasm_edict_t *entity_wa_to_wnp(wasm_entity_address_t e)
{
	return (wasm_edict_t *) wasm_addr_to_native(e);
}

static inline wasm_edict_t *entity_number_to_wnp(int32_t number)
{
	return entity_wa_to_wnp(entity_number_to_wa(number));
}

static inline wasm_entity_address_t entity_np_to_wa(edict_t *e)
{
	return e ? entity_number_to_wa(e - globals.edicts) : 0;
}

inline edict_t *entity_wnp_to_np(wasm_edict_t *e)
{
	return wasm_native_to_addr(e) ? entity_number_to_np(entity_wnp_to_number(e)) : NULL;
}

inline edict_t *entity_wa_to_np(wasm_entity_address_t e)
{
	return e ? entity_number_to_np(entity_wa_to_number(e)) : NULL;
}

static inline bool entity_validate_wnp(wasm_edict_t *e)
{
	wasm_app_address_t addr = wasm_native_to_addr(e);
	return addr == 0 || (addr >= wasm.edict_base && addr < wasm.edict_end && (wasm_native_to_addr(e) - wasm.edict_base) % wasm.edict_size == 0);
}

static inline void copy_link_wasm_to_native(edict_t *native_edict, const wasm_edict_t *wasm_edict)
{
	native_edict->s = wasm_edict->s;
	native_edict->inuse = wasm_edict->inuse;
	native_edict->svflags = wasm_edict->svflags;
	native_edict->mins = wasm_edict->mins;
	native_edict->maxs = wasm_edict->maxs;
	native_edict->clipmask = wasm_edict->clipmask;
	native_edict->solid = wasm_edict->solid;
	native_edict->linkcount = wasm_edict->linkcount;
}

static inline void copy_link_native_to_wasm(wasm_edict_t *wasm_edict, const edict_t *native_edict)
{
	wasm_edict->area.next = native_edict->area.next ? 1 : 0;
	wasm_edict->area.prev = native_edict->area.prev ? 1 : 0;
	wasm_edict->linkcount = native_edict->linkcount;
	wasm_edict->areanum = native_edict->areanum;
	wasm_edict->areanum2 = native_edict->areanum2;
	wasm_edict->absmin = native_edict->absmin;
	wasm_edict->absmax = native_edict->absmax;
	wasm_edict->size = native_edict->size;
	wasm_edict->s.solid = native_edict->s.solid;
	wasm_edict->linkcount = native_edict->linkcount;
}

static inline void copy_frame_native_to_wasm(wasm_edict_t *wasm_edict, const edict_t *native_edict)
{
	wasm_edict->s.number = native_edict->s.number;
	wasm_edict->s.event = native_edict->s.event;
}

static inline bool should_sync_entity(const wasm_edict_t *wasm_edict, const edict_t *native)
{
	return (!wasm_edict->client != !native->client ||
		wasm_edict->inuse != native->inuse ||
		wasm_edict->inuse);
}

static inline void sync_entity(wasm_edict_t *wasm_edict, edict_t *native, bool force)
{
	// Don't bother syncing non-inuse entities.
	if (!force && !should_sync_entity(wasm_edict, native))
		return;

	// sync main data
	copy_link_wasm_to_native(native, wasm_edict);

	// fill owner pointer
	native->owner = entity_wa_to_np(wasm_edict->owner);

	// sync client structure, if it exists
	if (wasm_edict->client)
	{
		if (!native->client)
			native->client = (gclient_t *) gi.TagMalloc(sizeof(gclient_t), TAG_GAME);

		memcpy(native->client, wasm_addr_to_native(wasm_edict->client), sizeof(gclient_t));
	}
	else
	{
		if (native->client)
		{
			gi.TagFree(native->client);
			native->client = NULL;
		}
	}
}

typedef wasm_app_address_t wasm_function_pointer_t;

typedef struct
{
	// state (in / out)
	pmove_state_t	s;

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

void q2_wasm_clear_surface_cache(void);

int32_t RegisterApiNatives(void);
