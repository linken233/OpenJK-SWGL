/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Light edit core of the RTX renderer. Include after tr_local.h.
//
// Records describe the editable lights and the emissive surfaces. A record maps to one or more slots of
// world->light_polys. A slot stays with its record until Compact releases the slots of a removed record.
// Compact can move a live record to a lower slot. See rtx_light_edit.cpp.

#pragma once

#include "rd-common/rtx_light_edit_api.h"

// Lights per cluster list. vk_rtx_bsp.cpp truncates a list at this value.
#define RTX_MAX_LIGHTS_PER_CLUSTER	1024

// A record uses up to this many slots: a sphere or a spot one, a rectangle two or four.
// An emissive surface uses one slot per polygon: see RTX_LightEdit_SlotCapacity.
#define RTX_LIGHT_MAX_SLOTS			4

// Slots kept for sabers, dlights and brush model lights. Records use the others.
#define RTX_LEDIT_MAX_SLOTS			( MAX_LIGHT_POLYS - 128 )

// The light of a record in light_polys is built from these values.
typedef struct {
	int			source;				// rtxLightSource_t
	int			sourceKey;			// -1 for added lights
	int			type;				// rtxLightType_t
	int			flags;				// RTX_LFLAG_DISABLED, _DELETED, _IN_SOLID, _MUTED, _COMPACTED only
	int			lightIndex;			// first slot in world->light_polys, or -1
	int			extraSlots[RTX_LIGHT_MAX_SLOTS - 1];	// more slots of a rectangle, -1 when none
	int			emiGroup;			// emissive surface: group of rtx_light_emissive.cpp, which holds the slots; else -1

	vec3_t		origin;
	vec3_t		color;				// 0..1
	float		intensity;
	float		radius;
	char		name[RTX_LIGHTEDIT_NAME_LEN];

	float		spot[5];			// positions[4..8] of a spot: profile, packed cones, direction
								// a rectangle keeps its normal in spot[2..4]

	float		width;				// rectangle only
	float		height;
	float		roll;
	int			twoSided;
	int			style;				// light style, all types

	int			entClass;			// LIGHT_ENT_* used in the light_poly

	vec3_t		origOrigin;			// values at load, or of the Add call
	vec3_t		origColor;
	float		origIntensity;
	float		origRadius;
	char		origName[RTX_LIGHTEDIT_NAME_LEN];
	int			origType;
	float		origSpot[5];
	float		origWidth;
	float		origHeight;
	float		origRoll;
	int			origTwoSided;
	int			origStyle;

	float		rays;				// lgt lights only
	float		error;
	int			edited;				// lgt lights only: the file holds an edited block
} rtxLightRecord_t;

// Hooks of R_PreparePT, vk_rtx_bsp.cpp and vk_rtx_lightgen.cpp.
void	RTX_LightEdit_Reset( world_t &w );
void	RTX_LightEdit_Invalidate( void );
int		RTX_LightEdit_RegisterLoaded( int source, int sourceKey, const vec3_t origin, const vec3_t color,
			float intensity, int lightIndex, float rays, float error );
void	RTX_LightEdit_FinalizeLoad( world_t &w );

// Rebuilds world->cluster_lights and cluster_light_offsets. Returns the number of full clusters.
int		vk_rtx_rebuild_cluster_lights( world_t &worldData, qboolean quiet );

// Console commands and the refexport entry.
void	RTX_LightEdit_List_f( void );
void	RTX_LightEdit_Add_f( void );
void	RTX_LightEdit_Set_f( void );
void	RTX_LightEdit_Del_f( void );
void	RTX_LightEdit_Restore_f( void );
void	RTX_LightEdit_Stats_f( void );
void	RTX_LightEdit_Mute_f( void );
void	RTX_LightEdit_Solo_f( void );
void	RTX_LightEdit_Compact_f( void );
void	*RTX_LightEdit_GetExtension( const char *name );

// Helpers for rtx_light_file.cpp. All of them need RTX_LightEdit_IsReady().
qboolean			RTX_LightEdit_IsReady( void );
void				RTX_LightEdit_SetError( const char *fmt, ... );
int					RTX_LightEdit_NumRecords( void );
rtxLightRecord_t	*RTX_LightEdit_GetRecord( int id );

// Appends a record with default values for its source. Gives its id, or -1. No slot yet.
int					RTX_LightEdit_NewRecord( int source, int sourceKey );

// Gives the record all the slots its type needs (spare ones when it has none). qfalse when no
// slot is free. Slots of a record stay its own until Compact releases them.
qboolean			RTX_LightEdit_EnsureSlot( rtxLightRecord_t *rec );

// Number of slots the record uses now, and the pointer to its slot number i (-1 when none).
// SlotCapacity is the number of pointers that SlotPtr gives: the unused ones stay -1.
int					RTX_LightEdit_SlotCount( const rtxLightRecord_t *rec );
int					RTX_LightEdit_SlotCapacity( const rtxLightRecord_t *rec );
int					*RTX_LightEdit_SlotPtr( rtxLightRecord_t *rec, int i );

// Fills the light_polys from the record: RTX_LightEdit_SlotCount entries, out holds
// RTX_LIGHT_MAX_SLOTS. Gives the cluster of the origin, -1 in solid.
int					RTX_LightEdit_Convert( const rtxLightRecord_t *rec, light_poly_t *out );

// Sets the rectangle values of a record, with limits. A zero dir becomes 0 0 -1. It does not change the type.
void				RTX_LightEdit_SetRectData( rtxLightRecord_t *rec, const vec3_t dir, float width, float height,
						float roll, int twoSided );

// Sets the style of a record and its original style. For the map load.
void				RTX_LightEdit_SetLoadedStyle( int id, int style );

// Makes the slot an inert light, keeping the position.
void				RTX_LightEdit_Tombstone( int slot, const vec3_t origin );

// Writes the record to its slot: a tombstone when disabled, deleted or in solid, else the
// converted light. Sets or clears IN_SOLID. Gives qtrue when the cluster of the slot changed.
qboolean			RTX_LightEdit_ApplyRecord( rtxLightRecord_t *rec );

// Rebuilds the cluster lists now and counts the rebuild.
void				RTX_LightEdit_RebuildClusters( void );

// Grows the static slots by count (rounded up to a block). Waits for the GPU.
qboolean			RTX_LightEdit_GrowSlots( int count );

// Counts an edit for stats.unsavedChanges. Pass 0 to clear it (after a save or reload).
void				RTX_LightEdit_CountChange( int delta );

// While loading, a new slot is appended to light_polys without the GPU growth path.
void				RTX_LightEdit_SetLoading( qboolean loading );

// Record access without the IsReady check, for the load code before FinalizeLoad.
rtxLightRecord_t	*RTX_LightEdit_GetRecordRaw( int id );

// True when the record differs from its original values.
qboolean			RTX_LightEdit_IsModified( const rtxLightRecord_t *rec );

// Spot data of a record (spot[] holds the encoded form). Set clamps the values and gives
// the record the spot encoding of make_entity_spot; it does not change the type.
// Get gives the defaults 0 0 -1, 35 and 25 for a sphere.
void				RTX_LightEdit_SetSpotData( rtxLightRecord_t *rec, const vec3_t dir, float outer, float inner );
void				RTX_LightEdit_GetSpotData( const rtxLightRecord_t *rec, vec3_t dir, float *outer, float *inner );

// Counter of the changes that alter the lighting. The tracer restarts its accumulation when it changes.
int					RTX_LightEdit_Generation( void );
void				RTX_LightEdit_BumpGeneration( void );

// Model lights (dlights, sabers, beams) of the last built frame. Gives the count.
int		vk_rtx_get_model_lights( const light_poly_t **out );

// Console commands pt_ledit_emissive and pt_ledit_dynamic.
void	RTX_LightEdit_Emissive_f( void );
void	RTX_LightEdit_Dynamic_f( void );
