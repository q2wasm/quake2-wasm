#pragma once

#include "../game/g_api.h"

typedef uint32_t entity_effects_t;

typedef uint32_t render_effects_t;

typedef int32_t entity_event_t;

// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct
{
	int32_t number;         // edict index

	vec3_t  origin;
	vec3_t  angles;
	vec3_t  old_origin;     // for lerping
	int32_t modelindex;
	int32_t modelindex2, modelindex3, modelindex4;  // weapons, CTF flags, etc
#ifdef KMQUAKE2_ENGINE_MOD //Knightmare- Privater wanted this
	int32_t	modelindex5, modelindex6;	//more attached models
#endif
	int32_t frame;
	int32_t skinnum;
#ifdef KMQUAKE2_ENGINE_MOD //Knightmare- allow the server to set this
	vec_t	alpha;	//entity transparency
#endif
	entity_effects_t	effects;
	render_effects_t	renderfx;
	int32_t solid;			// for client side prediction, 8*(bits 0-4) is x/y radius
							// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
							// gi.linkentity sets this properly
	int32_t  sound;			// for looping sounds, to guarantee shutoff
#ifdef KMQUAKE2_ENGINE_MOD // Knightmare- added sound attenuation
	vec_t	attenuation;
#endif
	entity_event_t	event;	// impulse events -- muzzle flashes, footsteps, etc
							// events only go out for a single frame, they
							// are automatically cleared each frame
} entity_state_t;

typedef struct list_s
{
    struct list_s	*next, *prev;
} list_t;

// edict->svflags
typedef int32_t svflags_t;

// edict->solid values
typedef int32_t solid_t;

enum { MAX_ENT_CLUSTERS	= 16 };

typedef struct edict_s edict_t;

typedef struct edict_s
{
	entity_state_t	s;
	gclient_t		*client;
	qboolean		inuse;
	int32_t			linkcount;

	list_t			area;				// linked to a division node or leaf

	int32_t num_clusters;		// if -1, use headnode instead
	int32_t clusternums[MAX_ENT_CLUSTERS];
	int32_t headnode;			// unused if num_clusters != -1
	int32_t areanum, areanum2;

	//================================

	svflags_t		svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
	vec3_t			mins, maxs;
	vec3_t			absmin, absmax, size;
	solid_t			solid;
	content_flags_t	clipmask;
	edict_t			*owner;
} edict_t;