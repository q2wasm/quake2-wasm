#pragma once

// This file contains structures that are being eaten natively from the
// WASM sandbox, and functions to handle WASM data.

#include <string.h>
#include "game/g_api.h"
#include "shared/entity.h"
#include "shared/client.h"
#include <wasm_export.h>

// Address relative to wasm heap.
typedef uint32_t wasm_addr_t;

// Address relative to wasm heap. Do not use in WASM structs!
typedef void *wasm_app_pointer_t;

// Address relative to wasm heap.
typedef wasm_addr_t wasm_entity_address_t;

// Address relative to wasm heap.
typedef wasm_addr_t wasm_surface_address_t;

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

// WASM data that we use to communicate between ourselves.
// The various buffers below are used to store data that we read/write to
// to communicate to WASM, since we can't pass native pointers through.
typedef struct
{
	usercmd_t		ucmd;
	union {
		char		userinfo[MAX_INFO_STRING];
		char		filename[MAX_INFO_STRING];
	};
	wasm_trace_t	trace;
	char			cmds[16][MAX_INFO_STRING / 8];
	char			scmd[MAX_INFO_STRING];
	csurface_t		nullsurf;
} wasm_buffers_t;

#define GMF_CLIENTNUM               0x00000001
#define GMF_PROPERINUSE             0x00000002
#define GMF_MVDSPEC                 0x00000004
#define GMF_WANT_ALL_DISCONNECTS    0x00000008

#define GMF_ENHANCED_SAVEGAMES      0x00000400
#define GMF_VARIABLE_FPS            0x00000800
#define GMF_EXTRA_USERINFO          0x00001000
#define GMF_IPV6_ADDRESS_AWARE      0x00002000

// WASM environment data. Stored globally.
typedef struct
{
	wasm_module_t wasm_module;
	wasm_module_inst_t module_inst;
	wasm_exec_env_t exec_env;
	uint8_t *assembly;
	char error_buf[128];

	int32_t edict_size, max_edicts;
	wasm_addr_t edicts, num_edicts, edict_end;

	int32_t g_features;

	// address of type wasm_buffers_t
	wasm_surface_address_t	buffers_addr;

	csurface_t	*nullsurf_native;

	// Function pointers from WASM that we store.
	wasm_function_inst_t WASM_PmoveTrace, WASM_PmovePointContents, WASM_GetGameAPI, WASM_Init, WASM_SpawnEntities, WASM_ClientConnect,
		WASM_ClientBegin, WASM_ClientUserinfoChanged, WASM_ClientDisconnect, WASM_ClientCommand, WASM_ClientThink, WASM_RunFrame, WASM_ServerCommand,
		WASM_WriteGame, WASM_ReadGame, WASM_WriteLevel, WASM_ReadLevel, WASM_GetEdicts, WASM_GetEdictSize, WASM_GetNumEdicts,
		WASM_GetMaxEdicts;
} wasm_env_t;

extern wasm_env_t wasm;

// convenience functions
static inline void *wasm_addr_to_native(wasm_addr_t address)
{
	return wasm_runtime_addr_app_to_native(wasm.module_inst, address);
}

static inline void *wasm_pointer_to_native(wasm_app_pointer_t address)
{
	return wasm_runtime_addr_app_to_native(wasm.module_inst, (uint32_t) address);
}

static inline wasm_addr_t wasm_native_to_addr(void *native)
{
	return (wasm_addr_t)wasm_runtime_addr_native_to_app(wasm.module_inst, native);
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

// convenient function to fetch typed buffer data
static inline wasm_buffers_t *wasm_buffers(void)
{
	return (wasm_buffers_t *) wasm_addr_to_native(wasm.buffers_addr);
}

#include <stddef.h>

#define WASM_BUFFERS_OFFSET(n) \
	wasm.buffers_addr + offsetof(wasm_buffers_t, n)

// convenient function to fetch typed buffer data
static inline int32_t wasm_num_edicts(void)
{
	return *(int32_t *) wasm_addr_to_native(wasm.num_edicts);
}

// Address relative to wasm heap.
typedef wasm_addr_t wasm_string_t;

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

typedef struct
{
    uint32_t next, prev;
} wasm_list_t;

typedef struct
{
	int32_t number;         // edict index
	vec3_t origin;
	vec3_t angles;
	vec3_t old_origin;     // for lerping
	int32_t modelindex;
	int32_t modelindex2, modelindex3, modelindex4;  // weapons, CTF flags, etc
	int32_t frame;
	int32_t skinnum;
	entity_effects_t effects;
	render_effects_t renderfx;
	int32_t solid;
	int32_t sound;
	entity_event_t event;
} wasm_entity_state_t;

typedef struct
{
	wasm_entity_state_t	s;
	uint32_t			client;
	qboolean			inuse;
	int32_t				linkcount;

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
} wasm_edict_t;

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
	return wasm.edicts + (number * wasm.edict_size);
}

static inline int32_t entity_wa_to_number(wasm_entity_address_t edict_offset)
{
	return (edict_offset - wasm.edicts) / wasm.edict_size;
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
	wasm_addr_t addr = wasm_native_to_addr(e);
	return addr == 0 || (addr >= wasm.edicts && addr < wasm.edict_end && (wasm_native_to_addr(e) - wasm.edicts) % wasm.edict_size == 0);
}

static inline void copy_link_wasm_to_native(edict_t *native_edict, const wasm_edict_t *wasm_edict)
{
#ifdef KMQUAKE2_ENGINE_MOD
	native_edict->s.angles = wasm_edict->s.angles;
	native_edict->s.effects = wasm_edict->s.effects;
	native_edict->s.event = wasm_edict->s.event;
	native_edict->s.frame = wasm_edict->s.frame;
	native_edict->s.modelindex = wasm_edict->s.modelindex;
	native_edict->s.modelindex2 = wasm_edict->s.modelindex2;
	native_edict->s.modelindex3 = wasm_edict->s.modelindex3;
	native_edict->s.modelindex4 = wasm_edict->s.modelindex4;
	native_edict->s.number = wasm_edict->s.number;
	native_edict->s.old_origin = wasm_edict->s.old_origin;
	native_edict->s.origin = wasm_edict->s.origin;
	native_edict->s.renderfx = wasm_edict->s.renderfx;
	native_edict->s.skinnum = wasm_edict->s.skinnum;
	native_edict->s.solid = wasm_edict->s.solid;
	native_edict->s.sound = wasm_edict->s.sound;
#else
	native_edict->s = *(entity_state_t *)&wasm_edict->s;
#endif
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

#ifndef SIZEOF_MEMBER
#define SIZEOF_MEMBER(s, m) \
	sizeof(((s *)NULL)->m)
#endif

#ifndef lengthof
#define lengthof(v) \
	sizeof(v) / sizeof(*v)
#endif

typedef struct
{
	pmtype_t	pm_type;
	int16_t		origin[3];
	int16_t		velocity[3];
	pmflags_t	pm_flags;
	uint8_t		pm_time;
	int16_t		gravity;
	int16_t		delta_angles[3];
} wasm_pmove_state_t;

typedef struct
{
	wasm_pmove_state_t	pmove;      // for prediction

	vec3_t	viewangles;
	vec3_t	viewoffset;
	vec3_t	kick_angles;

	vec3_t	gunangles;
	vec3_t	gunoffset;
	int32_t	gunindex;
	int32_t	gunframe;

	vec_t	blend[4];

	vec_t	fov;

	refdef_flags_t	rdflags;

	player_stat_t	stats[MAX_VANILLA_STATS];
} wasm_player_state_t;

typedef struct
{
	wasm_player_state_t	ps;
	int32_t				ping;
	int32_t				clientNum;
} wasm_gclient_t;

#ifdef KMQUAKE2_ENGINE_MOD
static inline void sync_pmove_state_wasm_to_native(pmove_state_t *state, const wasm_pmove_state_t *wasm_state)
{
	for (int32_t i = 0; i < 3; i++)
	{
		state->delta_angles[i] = wasm_state->delta_angles[i];
		state->origin[i] = wasm_state->origin[i];
		state->velocity[i] = wasm_state->velocity[i];
	}
	state->gravity = wasm_state->gravity;
	state->pm_flags = wasm_state->pm_flags;
	state->pm_time = wasm_state->pm_time;
	state->pm_type = wasm_state->pm_type;
}

static inline void sync_pmove_state_native_to_wasm(wasm_pmove_state_t *wasm_state, const pmove_state_t *state)
{
	for (int32_t i = 0; i < 3; i++)
	{
		wasm_state->delta_angles[i] = state->delta_angles[i];
		wasm_state->origin[i] = (int16_t) state->origin[i];
		wasm_state->velocity[i] = state->velocity[i];
	}
	wasm_state->gravity = state->gravity;
	wasm_state->pm_flags = state->pm_flags;
	wasm_state->pm_time = state->pm_time;
	wasm_state->pm_type = state->pm_type;
}

extern bool stat_offsets[MAX_STATS];

enum
{
	CS_NAME,
	CS_CDTRACK,
	CS_SKY,
	CS_SKYAXIS,	// %f %f %f format
	CS_SKYROTATE,
	CS_STATUSBAR,	// display program string
	
	CS_AIRACCEL		= 29,	// air acceleration control
	CS_MAXCLIENTS,
	CS_MAPCHECKSUM,			// for catching cheater maps
	
	CS_MODELS,

	MAX_LIGHTSTYLES	= 256,
	MAX_VANILLA_MODELS = 256,
	MAX_VANILLA_SOUNDS = 256,
	MAX_VANILLA_IMAGES = 256,

	MAX_MODELS = 8192,
	MAX_SOUNDS = 8192,
	MAX_IMAGES = 2048,

	MAX_ITEMS = 256,
	MAX_GENERAL = (MAX_CLIENTS * 2),

	CS_SOUNDS		= CS_MODELS + MAX_MODELS,
	CS_IMAGES		= CS_SOUNDS + MAX_SOUNDS,
	CS_LIGHTS		= CS_IMAGES + MAX_IMAGES,
	CS_ITEMS		= CS_LIGHTS + MAX_LIGHTSTYLES,
	CS_PLAYERSKINS		= CS_ITEMS + MAX_ITEMS,
	CS_GENERAL		= CS_PLAYERSKINS + MAX_CLIENTS,
	MAX_CONFIGSTRINGS	= CS_GENERAL + MAX_GENERAL,
	
	CS_VANILLA_SOUNDS		= CS_MODELS + MAX_VANILLA_MODELS,
	CS_VANILLA_IMAGES		= CS_VANILLA_SOUNDS + MAX_VANILLA_SOUNDS,
	CS_VANILLA_LIGHTS		= CS_VANILLA_IMAGES + MAX_VANILLA_IMAGES,
	CS_VANILLA_ITEMS		= CS_VANILLA_LIGHTS + MAX_LIGHTSTYLES,
	CS_VANILLA_PLAYERSKINS		= CS_VANILLA_ITEMS + MAX_ITEMS,
	CS_VANILLA_GENERAL		= CS_VANILLA_PLAYERSKINS + MAX_CLIENTS,
	MAX_VANILLA_CONFIGSTRINGS	= CS_VANILLA_GENERAL + MAX_GENERAL
};

static inline uint16_t wasm_remap_configstring(uint16_t id)
{
	if (id >= CS_VANILLA_GENERAL)
		id = (id - CS_VANILLA_GENERAL) + CS_GENERAL;
	else if (id >= CS_VANILLA_PLAYERSKINS)
		id = (id - CS_VANILLA_PLAYERSKINS) + CS_PLAYERSKINS;
	else if (id >= CS_VANILLA_ITEMS)
		id = (id - CS_VANILLA_ITEMS) + CS_ITEMS;
	else if (id >= CS_VANILLA_LIGHTS)
		id = (id - CS_VANILLA_LIGHTS) + CS_LIGHTS;
	else if (id >= CS_VANILLA_IMAGES)
		id = (id - CS_VANILLA_IMAGES) + CS_IMAGES;
	else if (id >= CS_VANILLA_SOUNDS)
		id = (id - CS_VANILLA_SOUNDS) + CS_SOUNDS;

	return id;
}

#else
#define sync_pmove_state_wasm_to_native(state, wasm_state) \
	*state = *(pmove_state_t *)&wasm_state
#define sync_pmove_state_native_to_wasm(wasm_state, state) \
	*wasm_state = *(wasm_pmove_state_t *)&state
#endif

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

		gclient_t *client = native->client;

#ifdef KMQUAKE2_ENGINE_MOD
		const wasm_gclient_t *wasm_client = (wasm_gclient_t *) wasm_addr_to_native(wasm_edict->client);
		
		for (int32_t i = 0; i < 4; i++)
			client->ps.blend[i] = wasm_client->ps.blend[i];
		client->ps.fov = wasm_client->ps.fov;
		client->ps.gunangles = wasm_client->ps.gunangles;
		client->ps.gunframe = wasm_client->ps.gunframe;
		client->ps.gunindex = wasm_client->ps.gunindex;
		client->ps.gunoffset = wasm_client->ps.gunoffset;
		client->ps.kick_angles = wasm_client->ps.kick_angles;
		sync_pmove_state_wasm_to_native(&client->ps.pmove, &wasm_client->ps.pmove);
		client->ps.rdflags = wasm_client->ps.rdflags;
		for (int32_t i = 0; i < MAX_VANILLA_STATS; i++)
		{
			if (stat_offsets[i])
				client->ps.stats[i] = wasm_remap_configstring(wasm_client->ps.stats[i]);
			else
				client->ps.stats[i] = wasm_client->ps.stats[i];
		}
		client->ps.viewangles = wasm_client->ps.viewangles;
		client->ps.viewoffset = wasm_client->ps.viewoffset;
		client->ping = wasm_client->ping;

		if (wasm.g_features & GMF_CLIENTNUM)
			client->clientNum = wasm_client->clientNum;
#else
		size_t client_struct_size = sizeof(gclient_t);

		if (!(wasm.g_features & GMF_CLIENTNUM))
			client_struct_size -= SIZEOF_MEMBER(gclient_t, clientNum);

		memcpy(client, wasm_addr_to_native(wasm_edict->client), client_struct_size);
#endif
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

typedef wasm_addr_t wasm_function_pointer_t;

void q2_wasm_clear_surface_cache(void);
void q2_wasm_update_cvars();

int32_t RegisterApiNatives(void);

static inline uint32_t wasm_call_args(wasm_function_inst_t func, uint32_t *args, size_t num_args)
{
	if (!func)
		wasm_error("Couldn't call WASM function: function missing");
	else if (!wasm_runtime_call_wasm(wasm.exec_env, func, num_args, args))
		wasm_error(wasm_runtime_get_exception(wasm.module_inst));

	if (args)
		return args[0];

	return 0;
}

static inline void wasm_call(wasm_function_inst_t func)
{
	wasm_call_args(func, NULL, 0);
}

static inline uint32_t ftoui32(float v)
{
	return *(uint32_t *) (&v);
}