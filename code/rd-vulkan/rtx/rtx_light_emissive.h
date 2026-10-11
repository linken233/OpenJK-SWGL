/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Emission factor of each emissive shader. Include after tr_local.h.
//
// The factor scales the light that the polygons of a shader cast, and the emission of the surface
// that the tracer shows (the emission scale of the material table, see vk_rtx_upload_materials).

#pragma once

#include <string>
#include <vector>
#include <utility>

#include "rtx_light_edit.h"

typedef std::vector< std::pair<std::string, float> >	rtxEmissiveScales_t;

extern float	g_rtxEmissiveScale[MAX_SHADERS];	// by shader index, 1 when unchanged

// Factor of the light that a material casts. O(1), called by copy_light for each light.
static inline float RTX_LightEmissive_Scale( const rtx_material_t *material )
{
	return material->index < MAX_SHADERS ? g_rtxEmissiveScale[material->index] : 1.0f;
}

// Map load. Reset clears the table, the scales and the surfaces. BuildGroups takes the slots of
// world->light_polys that owned[] does not mark and groups them by BSP surface. Each group becomes a
// record of the source RTX_LSRC_EMISSIVE.
void		RTX_LightEmissive_Reset( void );
void		RTX_LightEmissive_BuildGroups( world_t &w, const std::vector<char> &owned );

// Emissive surfaces. A group keeps the polygons as collected; the record keeps the edits.
// SlotCount gives the number of polygons and SlotPtr the slot of polygon i (-1 when it has none).
int			RTX_LightEmissive_SlotCount( int group );
int			*RTX_LightEmissive_SlotPtr( int group, int i );
int			RTX_LightEmissive_BaseCluster( int group, int i );

// Writes the polygon i of the record: the collected one with the factor, the tint and the style of the record.
void		RTX_LightEmissive_Fill( const rtxLightRecord_t *rec, int i, light_poly_t *out );

// File key of a group: the shader, the rank of the face among those of the shader, and the centre of
// the box of its polygons. FindRecord gives the record id of a shader and a rank, or -1.
void		RTX_LightEmissive_GroupKey( int group, const char **shader, int *rank, float *check );
int			RTX_LightEmissive_FindRecord( const char *shader, int rank );
qboolean	RTX_LightEmissive_GetSurface( const rtxLightRecord_t *rec, rtxEmissiveSurface_t *out );
int			RTX_LightEmissive_NumPolys( void );

// Counts each change of a factor. The material table is uploaded again when it changes.
int			RTX_LightEmissive_Generation( void );

// Implementation of the API functions.
int			RTX_LightEmissive_Count( void );
qboolean	RTX_LightEmissive_Get( int shader, char *name, int nameSize, float *scale, int *numPolys );
qboolean	RTX_LightEmissive_Set( int shader, float scale );

// File layer. SetFromFile sets the scale of a shader by name. A name that is not in the map is
// kept for the next Save. ResetScales gives every shader the factor 1 and drops the kept names.
void		RTX_LightEmissive_SetFromFile( const char *name, float scale );
void		RTX_LightEmissive_ResetScales( void );

// Names and scales to write: the shaders of the map with a factor other than 1, then the kept names.
void		RTX_LightEmissive_GetForSave( rtxEmissiveScales_t &out );

// Number of shaders of the map that have a factor other than 1.
int			RTX_LightEmissive_NumChanged( void );

// Console commands.
void		RTX_LightEdit_EmissiveShaders_f( void );
void		RTX_LightEdit_EmissiveScale_f( void );
