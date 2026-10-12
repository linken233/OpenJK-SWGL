/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Light edit core of the RTX renderer.
//
// A record is one editable light. Its slot in world->light_polys stays fixed: the shaders use
// the slot as the ReSTIR identity and as the stride of the light stats buffer. A removed light
// becomes a tombstone (cluster -1, colour 0) and keeps its slot until Compact releases it.
// Spare tombstones at the end of the array wait for added lights. The array grows by blocks,
// with a GPU wait.

#include "../tr_local.h"
#include "conversion.h"
#include "rtx_light_edit.h"
#include "rtx_light_file.h"
#include "rtx_light_emissive.h"
#include "rtx_light_sky.h"

#include <vector>
#include <algorithm>
#include <stdarg.h>
#include <math.h>

#define LEDIT_EPSILON			0.001f
#define LEDIT_MIN_SPARE			16
#define LEDIT_GROW_BLOCK		64
#define LEDIT_MAX_SLOTS			RTX_LEDIT_MAX_SLOTS
#define LEDIT_DRAG_REBUILD_MS	100
#define LEDIT_RECT_FLUX			2.5133f	// 0.8 * pi, see Convert
#define LEDIT_ENTITY_RADIUS		16.0f	// as ENTITY_LIGHT_RADIUS in vk_rtx_bsp.cpp
#define LEDIT_LGT_RADIUS		4.0f	// as LIGHTGEN_EMITTER_RADIUS in vk_rtx_lightgen.cpp
#define LEDIT_ADD_RADIUS		8.0f
#define LEDIT_EMISSIVE_MAX_FACTOR	100.0f	// as EMISSIVE_MAX_SCALE in rtx_light_emissive.cpp

static std::vector<rtxLightRecord_t>	g_records;
static world_t		*g_world = NULL;
static qboolean		g_valid = qfalse;
static int			g_firstFree = 0;		// first slot no record uses
static int			g_floor = 0;			// slots below this one may hold a polygon that no record owns
static int			g_overfull = 0;
static int			g_rebuilds = 0;
static int			g_unsaved = 0;
static int			g_dragId = -1;
static int			g_solo = -1;
static int			g_batch = 0;			// depth of BeginBatch calls
static int			g_generation = 0;
static qboolean		g_pendingRebuild = qfalse;
static qboolean		g_loading = qfalse;
static int			g_lastRebuildMs = 0;
static char			g_lastError[128] = "";

void RTX_LightEdit_SetError( const char *fmt, ... )
{
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( g_lastError, sizeof(g_lastError), fmt, argptr );
	va_end( argptr );
}

void RTX_LightEdit_CountChange( int delta )
{
	if ( delta == 0 )
		g_unsaved = 0;
	else
		g_unsaved += delta;
}

qboolean RTX_LightEdit_IsReady( void )
{
	return (qboolean)( vk.rtxActive && tr.world && g_valid && g_world == tr.world && g_world->light_polys );
}

int RTX_LightEdit_NumRecords( void )
{
	return RTX_LightEdit_IsReady() ? (int)g_records.size() : 0;
}

rtxLightRecord_t *RTX_LightEdit_GetRecord( int id )
{
	if ( !RTX_LightEdit_IsReady() || id < 0 || id >= (int)g_records.size() )
	{
		RTX_LightEdit_SetError( "bad light id %i", id );
		return NULL;
	}

	return &g_records[id];
}

void RTX_LightEdit_SetLoading( qboolean loading )
{
	g_loading = loading;
}

rtxLightRecord_t *RTX_LightEdit_GetRecordRaw( int id )
{
	return id >= 0 && id < (int)g_records.size() ? &g_records[id] : NULL;
}

static int PointCluster( const vec3_t p )
{
	vec3_t v;

	VectorCopy( p, v );

	return BSP_PointLeaf( g_world->nodes, v )->cluster;
}

static float ClampF( float lo, float v, float hi )
{
	return v < lo ? lo : v > hi ? hi : v;
}

int RTX_LightEdit_Generation( void )
{
	return g_generation;
}

void RTX_LightEdit_BumpGeneration( void )
{
	g_generation++;
}

static int RecordId( const rtxLightRecord_t *rec )
{
	return (int)( rec - &g_records[0] );
}

// True when Remove switched the light off.
static qboolean IsRemoved( const rtxLightRecord_t *rec )
{
	return (qboolean)( ( rec->flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_DELETED ) ) != 0 );
}

static qboolean IsSoloHidden( const rtxLightRecord_t *rec )
{
	return (qboolean)( g_solo >= 0 && RecordId( rec ) != g_solo );
}

// True when the light does not emit, whatever its position.
static qboolean IsInactive( const rtxLightRecord_t *rec )
{
	return (qboolean)( IsRemoved( rec ) || ( rec->flags & RTX_LFLAG_MUTED ) || IsSoloHidden( rec ) );
}

// A cluster change rebuilds the lists now, or at EndBatch.
static void RequestRebuild( void )
{
	if ( g_batch > 0 )
		g_pendingRebuild = qtrue;
	else
		RTX_LightEdit_RebuildClusters();
}

int RTX_LightEdit_SlotCount( const rtxLightRecord_t *rec )
{
	if ( rec->source == RTX_LSRC_EMISSIVE )
		return RTX_LightEmissive_SlotCount( rec->emiGroup );

	return rec->type == RTX_LTYPE_RECT ? ( rec->twoSided ? 4 : 2 ) : 1;
}

int RTX_LightEdit_SlotCapacity( const rtxLightRecord_t *rec )
{
	return rec->source == RTX_LSRC_EMISSIVE ? RTX_LightEmissive_SlotCount( rec->emiGroup ) : RTX_LIGHT_MAX_SLOTS;
}

int *RTX_LightEdit_SlotPtr( rtxLightRecord_t *rec, int i )
{
	if ( rec->source == RTX_LSRC_EMISSIVE )
		return RTX_LightEmissive_SlotPtr( rec->emiGroup, i );

	return i == 0 ? &rec->lightIndex : &rec->extraSlots[i - 1];
}

static int SlotsOfValues( int type, int twoSided )
{
	return type == RTX_LTYPE_RECT ? ( twoSided ? 4 : 2 ) : 1;
}

static int ClampStyle( int style )
{
	return style < 0 ? 0 : style > RTX_LSTYLE_MAX - 1 ? RTX_LSTYLE_MAX - 1 : style;
}

/*
=================
Spot data
=================
*/

void RTX_LightEdit_SetSpotData( rtxLightRecord_t *rec, const vec3_t dirIn, float outer, float inner )
{
	vec3_t	dir;

	VectorCopy( dirIn, dir );

	if ( VectorNormalize( dir ) < 1e-4f )
		VectorSet( dir, 0.0f, 0.0f, -1.0f );

	outer = ClampF( 1.0f, outer, 89.0f );
	inner = ClampF( 0.0f, inner, outer );

	const float cosOuter = cosf( outer * 0.01745329f );
	const float cosInner = cosf( inner * 0.01745329f );

	rec->spot[0] = uintBitsToFloat( DYNLIGHT_SPOT_EMISSION_PROFILE_FALLOFF );
	rec->spot[1] = uintBitsToFloat( floatToHalf( cosOuter ) | ( floatToHalf( cosInner ) << 16 ) );
	VectorCopy( dir, rec->spot + 2 );
}

void RTX_LightEdit_SetRectData( rtxLightRecord_t *rec, const vec3_t dirIn, float width, float height,
	float roll, int twoSided )
{
	vec3_t	dir;

	VectorCopy( dirIn, dir );

	if ( VectorNormalize( dir ) < 1e-4f )
		VectorSet( dir, 0.0f, 0.0f, -1.0f );

	VectorCopy( dir, rec->spot + 2 );
	rec->width = ClampF( RTX_LRECT_MIN_SIZE, width > 0.0f ? width : RTX_LRECT_DEFAULT_SIZE, RTX_LRECT_MAX_SIZE );
	rec->height = ClampF( RTX_LRECT_MIN_SIZE, height > 0.0f ? height : RTX_LRECT_DEFAULT_SIZE, RTX_LRECT_MAX_SIZE );
	rec->roll = roll;
	rec->twoSided = twoSided ? 1 : 0;
}

void RTX_LightEdit_SetLoadedStyle( int id, int style )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecordRaw( id );

	if ( rec )
		rec->style = rec->origStyle = ClampStyle( style );
}

void RTX_LightEdit_GetSpotData( const rtxLightRecord_t *rec, vec3_t dir, float *outer, float *inner )
{
	if ( rec->type == RTX_LTYPE_RECT )
	{
		VectorCopy( rec->spot + 2, dir );
		*outer = 35.0f;
		*inner = 25.0f;
		return;
	}

	if ( rec->type != RTX_LTYPE_SPOT )
	{
		VectorSet( dir, 0.0f, 0.0f, -1.0f );
		*outer = 35.0f;
		*inner = 25.0f;
		return;
	}

	uint32_t packed;

	Com_Memcpy( &packed, &rec->spot[1], sizeof(packed) );
	VectorCopy( rec->spot + 2, dir );

	*outer = acosf( ClampF( -1.0f, halfToFloat( (uint16_t)( packed & 0xFFFF ) ), 1.0f ) ) * 57.29578f;
	*inner = acosf( ClampF( -1.0f, halfToFloat( (uint16_t)( packed >> 16 ) ), 1.0f ) ) * 57.29578f;
}

/*
=================
Registration at map load
=================
*/

void RTX_LightEdit_Reset( world_t &w )
{
	g_records.clear();
	g_world = &w;
	g_valid = qfalse;
	g_firstFree = 0;
	g_floor = 0;
	g_overfull = 0;
	g_rebuilds = 0;
	g_unsaved = 0;
	g_dragId = -1;
	g_solo = -1;
	g_batch = 0;
	g_generation++;
	g_pendingRebuild = qfalse;
	g_loading = qfalse;
	g_lastRebuildMs = 0;
	g_lastError[0] = 0;

	RTX_LightEmissive_Reset();
	RTX_LightSky_Clear();
}

void RTX_LightEdit_Invalidate( void )
{
	g_valid = qfalse;
	g_world = NULL;
	g_dragId = -1;
	g_solo = -1;
	g_batch = 0;
	g_generation++;
	g_pendingRebuild = qfalse;
}

int RTX_LightEdit_NewRecord( int source, int sourceKey )
{
	rtxLightRecord_t rec;

	Com_Memset( &rec, 0, sizeof(rec) );

	rec.source = source;
	rec.sourceKey = source == RTX_LSRC_ADDED ? -1 : sourceKey;
	rec.type = RTX_LTYPE_SPHERE;
	rec.lightIndex = -1;
	rec.emiGroup = -1;

	for ( int k = 0; k < RTX_LIGHT_MAX_SLOTS - 1; k++ )
		rec.extraSlots[k] = -1;

	rec.width = rec.origWidth = rec.height = rec.origHeight = RTX_LRECT_DEFAULT_SIZE;
	VectorSet( rec.spot + 2, 0.0f, 0.0f, -1.0f );
	VectorSet( rec.origSpot + 2, 0.0f, 0.0f, -1.0f );
	VectorSet( rec.color, 1.0f, 1.0f, 1.0f );
	VectorSet( rec.origColor, 1.0f, 1.0f, 1.0f );
	rec.radius = rec.origRadius = source == RTX_LSRC_ENTITY ? LEDIT_ENTITY_RADIUS
		: source == RTX_LSRC_LGT ? LEDIT_LGT_RADIUS : LEDIT_ADD_RADIUS;
	rec.entClass = source == RTX_LSRC_ADDED ? LIGHT_ENT_EDIT : LIGHT_ENT_AMBIENT;

	g_records.push_back( rec );

	return (int)g_records.size() - 1;
}

int RTX_LightEdit_RegisterLoaded( int source, int sourceKey, const vec3_t origin, const vec3_t color,
	float intensity, int lightIndex, float rays, float error )
{
	const int id = RTX_LightEdit_NewRecord( source, sourceKey );
	rtxLightRecord_t *rec = &g_records[id];

	VectorCopy( origin, rec->origin );
	VectorCopy( origin, rec->origOrigin );
	VectorCopy( color, rec->color );
	VectorCopy( color, rec->origColor );
	rec->intensity = rec->origIntensity = intensity;
	rec->lightIndex = lightIndex;
	rec->rays = rays;
	rec->error = error;

	if ( lightIndex < 0 )
		rec->flags |= RTX_LFLAG_IN_SOLID;

	Com_sprintf( rec->name, sizeof(rec->name), "%s_%i", source == RTX_LSRC_ENTITY ? "ent" : "lgt", sourceKey );
	Q_strncpyz( rec->origName, rec->name, sizeof(rec->origName) );

	return id;
}

// Reads what the load code made of each light: spot data, emitter radius, class.
void RTX_LightEdit_FinalizeLoad( world_t &w )
{
	std::vector<char>	owned( w.num_light_polys, 0 );

	g_world = &w;

	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		rtxLightRecord_t *rec = &g_records[i];

		if ( rec->lightIndex < 0 || rec->lightIndex >= w.num_light_polys )
			continue;

		const light_poly_t *light = w.light_polys + rec->lightIndex;

		for ( int k = 0; k < RTX_LIGHT_MAX_SLOTS; k++ )
		{
			const int slot = *RTX_LightEdit_SlotPtr( rec, k );

			if ( slot >= 0 && slot < w.num_light_polys )
				owned[slot] = 1;
		}

		rec->entClass = light->ent_class;

		// A rectangle of an lgt block keeps the values the file gave it.
		if ( rec->type == RTX_LTYPE_RECT )
			continue;

		rec->type = light->type == LIGHT_SPOT ? RTX_LTYPE_SPOT : RTX_LTYPE_SPHERE;
		rec->radius = rec->origRadius = light->positions[3];

		if ( light->type == LIGHT_SPOT )
		{
			for ( int k = 0; k < 5; k++ )
				rec->spot[k] = light->positions[4 + k];
		}

		rec->origType = rec->type;
		Com_Memcpy( rec->origSpot, rec->spot, sizeof(rec->origSpot) );
	}

	// The slots that no light record holds are emissive polygons: each BSP surface becomes a record.
	RTX_LightEmissive_BuildGroups( w, owned );

	// A live polygon that no record holds keeps its slot: Compact never packs below it.
	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		const int capacity = RTX_LightEdit_SlotCapacity( &g_records[i] );

		for ( int k = 0; k < capacity; k++ )
		{
			const int slot = *RTX_LightEdit_SlotPtr( &g_records[i], k );

			if ( slot >= 0 && slot < w.num_light_polys )
				owned[slot] = 1;
		}
	}

	g_floor = 0;

	for ( int slot = 0; slot < w.num_light_polys; slot++ )
	{
		if ( !owned[slot] && w.light_polys[slot].cluster >= 0 )
			g_floor = slot + 1;
	}

	g_firstFree = w.num_light_polys;
	g_valid = qtrue;

	RTX_LightSky_MapLoaded();
}

/*
=================
Conversion between a record and its light_poly
=================
*/

// Two triangles per side. Seen from the lit side the corners go counter-clockwise, so the
// shader normal cross( p1 - p0, p2 - p0 ) points to the lit side (sample_projected_triangle).
// Corner order: -u-v, +u-v, +u+v, -u+v.
static void ConvertRect( const rtxLightRecord_t *rec, float scale, light_poly_t *out )
{
	static const int tris[4][3] = { { 0, 1, 2 }, { 0, 2, 3 }, { 0, 2, 1 }, { 0, 3, 2 } };
	vec3_t	u, v, corner[4];
	const float	hw = rec->width * 0.5f;
	const float	hh = rec->height * 0.5f;

	RTX_LightRectAxes( rec->spot + 2, rec->roll, u, v );

	for ( int c = 0; c < 4; c++ )
	{
		const float	su = ( c == 0 || c == 3 ) ? -hw : hw;
		const float	sv = c < 2 ? -hh : hh;

		VectorMA( rec->origin, su, u, corner[c] );
		VectorMA( corner[c], sv, v, corner[c] );
	}

	// A polygon gives colour * solid angle, a sphere colour * 0.8 * pi * r^2 / d^2 on its axis
	// (0.8 is the mean of sqrt( cos ) over its disc). So the colour is I * 0.8 * pi / area.
	vec3_t	color;

	VectorScale( rec->color, scale * rec->intensity * LEDIT_RECT_FLUX / ( rec->width * rec->height ), color );

	for ( int i = 0; i < RTX_LightEdit_SlotCount( rec ); i++ )
	{
		light_poly_t *light = out + i;

		Com_Memset( light, 0, sizeof(*light) );

		for ( int k = 0; k < 3; k++ )
			VectorCopy( corner[tris[i][k]], light->positions + k * 3 );

		get_triangle_off_center( light->positions, light->off_center, NULL, 1.0f );
		VectorCopy( color, light->color );
		light->type = LIGHT_POLYGON;
		light->emissive_factor = 1.0f;
		light->material = NULL;
		light->style = ClampStyle( rec->style );
		light->ent_class = rec->entClass;
	}
}

int RTX_LightEdit_Convert( const rtxLightRecord_t *rec, light_poly_t *out )
{
	const float	r = MAX( rec->radius, 0.5f );
	const float	scale = rec->source == RTX_LSRC_LGT ? pt_lightgen_scale->value : 1.0f;
	const int	cluster = PointCluster( rec->origin );

	if ( rec->type == RTX_LTYPE_RECT )
	{
		ConvertRect( rec, scale, out );

		for ( int i = 0; i < RTX_LightEdit_SlotCount( rec ); i++ )
			out[i].cluster = cluster;

		return cluster;
	}

	Com_Memset( out, 0, sizeof(*out) );

	VectorCopy( rec->origin, out->positions + 0 );
	VectorCopy( rec->origin, out->off_center );
	out->positions[3] = rec->radius;

	if ( rec->type == RTX_LTYPE_SPOT )
	{
		for ( int k = 0; k < 5; k++ )
			out->positions[4 + k] = rec->spot[k];

		// A spot's irradiance does not depend on the emitter radius.
		VectorScale( rec->color, scale * rec->intensity * 0.5f, out->color );
		out->type = LIGHT_SPOT;
	}
	else
	{
		// A sphere's irradiance is colour * r^2 / d^2, so the colour holds 1 / r^2.
		VectorScale( rec->color, scale * rec->intensity / ( r * r ), out->color );
		out->type = LIGHT_SPHERE;
	}

	out->emissive_factor = 1.0f;
	out->material = NULL;
	out->style = ClampStyle( rec->style );
	out->ent_class = rec->entClass;
	out->cluster = cluster;

	return cluster;
}

void RTX_LightEdit_Tombstone( int slot, const vec3_t origin )
{
	light_poly_t *light = g_world->light_polys + slot;

	Com_Memset( light, 0, sizeof(*light) );

	VectorCopy( origin, light->positions + 0 );
	VectorCopy( origin, light->off_center );
	light->positions[3] = 1.0f;
	light->cluster = -1;
	light->type = LIGHT_SPHERE;
	light->material = NULL;
	light->ent_class = LIGHT_ENT_AMBIENT;
}

/*
=================
Slots and growth
=================
*/

static void RecreateLightStats( world_t &w )
{
	uint32_t num_stats = w.numClusters * w.num_light_polys * 6 * 2;

	if ( num_stats == 0 )
		num_stats = 1;

	for ( int frame = 0; frame < NUM_LIGHT_STATS_BUFFERS; frame++ )
	{
		vk_rtx_buffer_destroy( vk.buf_light_stats + frame );

		if ( vk_rtx_buffer_create( vk.buf_light_stats + frame, sizeof(uint32_t) * num_stats,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) != VK_SUCCESS )
		{
			Com_Error( ERR_DROP, "light edit: cannot create the light stats buffer" );
		}
	}

	// The shaders accumulate into the buffers, so they start at zero.
	VkCommandBuffer cmd_buf = vkpt_begin_command_buffer( &vk.cmd_buffers_graphics );

	for ( int frame = 0; frame < NUM_LIGHT_STATS_BUFFERS; frame++ )
		qvkCmdFillBuffer( cmd_buf, vk.buf_light_stats[frame].buffer, 0, vk.buf_light_stats[frame].size, 0 );

	vkpt_submit_command_buffer( cmd_buf, vk.queue_graphics, (1 << vk.device_count) - 1, 0, NULL, NULL, NULL, 0, NULL, NULL, NULL );
	qvkDeviceWaitIdle( vk.device );

	// Patch the light stats binding of each set in place.
	for ( uint32_t i = 0; i < vk.swapchain_image_count; i++ )
	{
		vkdescriptor_t *desc = &vk.desc_set_vertex_buffer[i];

		if ( desc->set == VK_NULL_HANDLE )
			continue;

		for ( size_t k = 0; k < desc->size; k++ )
		{
			if ( desc->bindings[k].binding != BINDING_OFFSET_LIGHT_STATS_BUFFER )
				continue;

			for ( int j = 0; j < NUM_LIGHT_STATS_BUFFERS; j++ )
			{
				desc->data[k].buffer[j].buffer = vk.buf_light_stats[j].buffer;
				desc->data[k].buffer[j].offset = 0;
				desc->data[k].buffer[j].range = vk.buf_light_stats[j].size;
			}

			VkWriteDescriptorSet write;

			Com_Memset( &write, 0, sizeof(write) );
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = desc->set;
			write.dstBinding = BINDING_OFFSET_LIGHT_STATS_BUFFER;
			write.dstArrayElement = 0;
			write.descriptorCount = NUM_LIGHT_STATS_BUFFERS;
			write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write.pBufferInfo = desc->data[k].buffer;

			qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
			break;
		}
	}
}

void RTX_LightEdit_RebuildClusters( void )
{
	g_overfull = vk_rtx_rebuild_cluster_lights( *g_world, qtrue );
	g_rebuilds++;
	g_pendingRebuild = qfalse;
	g_lastRebuildMs = ri.Milliseconds();
}

qboolean RTX_LightEdit_GrowSlots( int count )
{
	world_t	&w = *g_world;
	int		newNum = w.num_light_polys + ( ( count + LEDIT_GROW_BLOCK - 1 ) / LEDIT_GROW_BLOCK ) * LEDIT_GROW_BLOCK;

	if ( newNum > LEDIT_MAX_SLOTS )
		newNum = LEDIT_MAX_SLOTS;

	if ( newNum <= w.num_light_polys )
	{
		RTX_LightEdit_SetError( "no free light slot (limit %i)", LEDIT_MAX_SLOTS );
		return qfalse;
	}

	qvkDeviceWaitIdle( vk.device );

	light_poly_t *polys = (light_poly_t *)realloc( w.light_polys, newNum * sizeof(light_poly_t) );

	if ( !polys )
	{
		RTX_LightEdit_SetError( "out of memory" );
		return qfalse;
	}

	w.light_polys = polys;
	w.allocated_light_polys = newNum;

	const int oldNum = w.num_light_polys;
	const vec3_t zero = { 0.0f, 0.0f, 0.0f };

	w.num_light_polys = newNum;

	for ( int i = oldNum; i < newNum; i++ )
		RTX_LightEdit_Tombstone( i, zero );

	RecreateLightStats( w );
	RTX_LightEdit_RebuildClusters();
	g_generation++;

	return qtrue;
}

// Appends one tombstone to light_polys. Only for the map load: no buffer exists yet.
static qboolean AppendSlotAtLoad( void )
{
	world_t	&w = *g_world;

	if ( w.num_light_polys >= LEDIT_MAX_SLOTS )
	{
		RTX_LightEdit_SetError( "no free light slot (limit %i)", LEDIT_MAX_SLOTS );
		return qfalse;
	}

	if ( w.num_light_polys >= w.allocated_light_polys )
	{
		const int		newAlloc = MAX( w.allocated_light_polys * 2, 128 );
		light_poly_t	*polys = (light_poly_t *)realloc( w.light_polys, newAlloc * sizeof(light_poly_t) );

		if ( !polys )
		{
			RTX_LightEdit_SetError( "out of memory" );
			return qfalse;
		}

		w.light_polys = polys;
		w.allocated_light_polys = newAlloc;
	}

	const vec3_t zero = { 0.0f, 0.0f, 0.0f };

	RTX_LightEdit_Tombstone( w.num_light_polys, zero );
	w.num_light_polys++;

	return qtrue;
}

// Gives the record its first `count` slots from the spare ones. Nothing changes on failure.
static qboolean EnsureSlots( rtxLightRecord_t *rec, int count )
{
	int missing = 0;

	for ( int i = 0; i < count; i++ )
	{
		if ( *RTX_LightEdit_SlotPtr( rec, i ) < 0 )
			missing++;
	}

	if ( !missing )
		return qtrue;

	const int spare = g_world->num_light_polys - g_firstFree;

	if ( spare < missing )
	{
		if ( g_loading )
		{
			for ( int i = spare; i < missing; i++ )
			{
				if ( !AppendSlotAtLoad() )
					return qfalse;
			}
		}
		else if ( !RTX_LightEdit_GrowSlots( missing - spare ) )
			return qfalse;

		if ( g_world->num_light_polys - g_firstFree < missing )
		{
			RTX_LightEdit_SetError( "no free light slot (limit %i)", LEDIT_MAX_SLOTS );
			return qfalse;
		}
	}

	for ( int i = 0; i < count; i++ )
	{
		int *slot = RTX_LightEdit_SlotPtr( rec, i );

		if ( *slot < 0 )
			*slot = g_firstFree++;
	}

	return qtrue;
}

qboolean RTX_LightEdit_EnsureSlot( rtxLightRecord_t *rec )
{
	return EnsureSlots( rec, RTX_LightEdit_SlotCount( rec ) );
}

// Emissive surface: each slot takes its polygon with the factor, the tint and the style of the record,
// or becomes a tombstone when the record does not emit. A compacted surface has no slot to write.
static qboolean ApplyEmissive( rtxLightRecord_t *rec )
{
	const int		count = RTX_LightEdit_SlotCount( rec );
	const qboolean	emits = (qboolean)!IsInactive( rec );
	qboolean		changed = qfalse;

	rec->flags &= ~RTX_LFLAG_IN_SOLID;

	if ( emits )
	{
		if ( !EnsureSlots( rec, count ) )
			return qfalse;
	}
	else if ( count == 0 || *RTX_LightEdit_SlotPtr( rec, 0 ) < 0 )
		return qfalse;

	for ( int i = 0; i < count; i++ )
	{
		const int slot = *RTX_LightEdit_SlotPtr( rec, i );

		if ( slot < 0 )
			continue;

		light_poly_t		*light = g_world->light_polys + slot;
		const light_poly_t	old = *light;

		if ( emits )
			RTX_LightEmissive_Fill( rec, i, light );
		else
			RTX_LightEdit_Tombstone( slot, rec->origin );

		if ( light->cluster != old.cluster )
			changed = qtrue;
		else if ( ( old.type == LIGHT_POLYGON || light->type == LIGHT_POLYGON )
			&& memcmp( old.positions, light->positions, sizeof(old.positions) ) != 0 )
			changed = qtrue;
	}

	return changed;
}

// Writes the record to all its slots. A slot the type does not use becomes a tombstone but stays
// the record's. Gives qtrue when the cluster lists need a rebuild: a slot changed cluster, or a
// polygon changed (the lists cull a cluster behind the plane of a polygon).
qboolean RTX_LightEdit_ApplyRecord( rtxLightRecord_t *rec )
{
	if ( rec->source == RTX_LSRC_EMISSIVE )
		return ApplyEmissive( rec );

	light_poly_t	tmp[RTX_LIGHT_MAX_SLOTS];
	const int		cluster = RTX_LightEdit_Convert( rec, tmp );
	const int		used = RTX_LightEdit_SlotCount( rec );

	if ( cluster < 0 )
		rec->flags |= RTX_LFLAG_IN_SOLID;
	else
		rec->flags &= ~RTX_LFLAG_IN_SOLID;

	const qboolean emits = (qboolean)( !IsInactive( rec ) && cluster >= 0 );

	if ( emits )
	{
		if ( !EnsureSlots( rec, used ) )
			return qfalse;
	}
	else if ( rec->lightIndex < 0 )
		return qfalse;

	qboolean changed = qfalse;

	for ( int i = 0; i < RTX_LIGHT_MAX_SLOTS; i++ )
	{
		const int slot = *RTX_LightEdit_SlotPtr( rec, i );

		if ( slot < 0 )
			continue;

		light_poly_t		*light = g_world->light_polys + slot;
		const light_poly_t	old = *light;

		if ( emits && i < used )
			*light = tmp[i];
		else
			RTX_LightEdit_Tombstone( slot, rec->origin );

		if ( light->cluster != old.cluster )
			changed = qtrue;
		else if ( ( old.type == LIGHT_POLYGON || light->type == LIGHT_POLYGON )
			&& memcmp( old.positions, light->positions, sizeof(old.positions) ) != 0 )
			changed = qtrue;
	}

	return changed;
}

// True when a cluster of the PVS of `cluster` has no room for `extra` more lights.
static qboolean ClusterFull( int cluster, int extra = 1 )
{
	const world_t	&w = *g_world;

	if ( cluster < 0 || !w.cluster_light_offsets )
		return qfalse;

	const byte *pvs = BSP_GetPvs( g_world, cluster );

	// A map without a PVS (no vis lump) puts no light in a cluster list.
	if ( !pvs )
		return qfalse;

	for ( int c = 0; c < w.numClusters; c++ )
	{
		if ( !( pvs[c >> 3] & ( 1 << ( c & 7 ) ) ) )
			continue;

		if ( w.cluster_light_offsets[c + 1] - w.cluster_light_offsets[c] + extra > RTX_MAX_LIGHTS_PER_CLUSTER )
			return qtrue;
	}

	return qfalse;
}

// True when the polygons of an emissive surface do not fit in the cluster lists they would join:
// each cluster of the PVS of a polygon takes one more entry for it.
static qboolean GroupClusterFull( const rtxLightRecord_t *rec )
{
	const world_t	&w = *g_world;

	if ( !w.cluster_light_offsets || w.numClusters <= 0 )
		return qfalse;

	const int			group = rec->emiGroup;
	const int			count = RTX_LightEmissive_SlotCount( group );
	std::vector<int>	perCluster( w.numClusters, 0 );
	std::vector<int>	extra( w.numClusters, 0 );

	for ( int i = 0; i < count; i++ )
	{
		const int c = RTX_LightEmissive_BaseCluster( group, i );

		if ( c >= 0 && c < w.numClusters )
			perCluster[c]++;
	}

	for ( int c = 0; c < w.numClusters; c++ )
	{
		if ( !perCluster[c] )
			continue;

		const byte *pvs = BSP_GetPvs( g_world, c );

		if ( !pvs )
			continue;

		for ( int x = 0; x < w.numClusters; x++ )
		{
			if ( pvs[x >> 3] & ( 1 << ( x & 7 ) ) )
				extra[x] += perCluster[c];
		}
	}

	for ( int x = 0; x < w.numClusters; x++ )
	{
		if ( extra[x] && w.cluster_light_offsets[x + 1] - w.cluster_light_offsets[x] + extra[x] > RTX_MAX_LIGHTS_PER_CLUSTER )
			return qtrue;
	}

	return qfalse;
}

// A cluster change rebuilds the lists. During a drag the rebuild runs at most each 100 ms.
static void ClusterChanged( int id )
{
	if ( id == g_dragId )
	{
		g_pendingRebuild = qtrue;

		if ( ri.Milliseconds() - g_lastRebuildMs < LEDIT_DRAG_REBUILD_MS )
			return;
	}

	RequestRebuild();
}

/*
=================
API
=================
*/

static qboolean IsModified( const rtxLightRecord_t *rec )
{
	if ( rec->source == RTX_LSRC_ADDED )
		return qfalse;

	if ( fabsf( rec->intensity - rec->origIntensity ) > LEDIT_EPSILON
		|| fabsf( rec->radius - rec->origRadius ) > LEDIT_EPSILON )
		return qtrue;

	for ( int k = 0; k < 3; k++ )
	{
		if ( fabsf( rec->origin[k] - rec->origOrigin[k] ) > LEDIT_EPSILON
			|| fabsf( rec->color[k] - rec->origColor[k] ) > LEDIT_EPSILON )
			return qtrue;
	}

	if ( rec->type != rec->origType || rec->style != rec->origStyle )
		return qtrue;

	if ( rec->type == RTX_LTYPE_RECT )
	{
		if ( fabsf( rec->width - rec->origWidth ) > LEDIT_EPSILON || fabsf( rec->height - rec->origHeight ) > LEDIT_EPSILON
			|| fabsf( rec->roll - rec->origRoll ) > LEDIT_EPSILON || rec->twoSided != rec->origTwoSided )
			return qtrue;

		for ( int k = 0; k < 3; k++ )
		{
			if ( fabsf( rec->spot[2 + k] - rec->origSpot[2 + k] ) > LEDIT_EPSILON )
				return qtrue;
		}
	}

	if ( rec->type == RTX_LTYPE_SPOT )
	{
		// The cones compare as encoded words: a Get then Set round trip does not change them.
		if ( memcmp( &rec->spot[1], &rec->origSpot[1], sizeof(float) ) != 0 )
			return qtrue;

		for ( int k = 0; k < 3; k++ )
		{
			if ( fabsf( rec->spot[2 + k] - rec->origSpot[2 + k] ) > LEDIT_EPSILON )
				return qtrue;
		}
	}

	return qfalse;
}

qboolean RTX_LightEdit_IsModified( const rtxLightRecord_t *rec )
{
	return IsModified( rec );
}

static void FillDesc( int id, const rtxLightRecord_t *rec, qboolean original, rtxLightDesc_t *out )
{
	Com_Memset( out, 0, sizeof(*out) );

	out->id = id;
	out->source = rec->source;
	out->sourceKey = rec->sourceKey;
	out->type = original ? rec->origType : rec->type;
	out->flags = rec->flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_DELETED | RTX_LFLAG_IN_SOLID | RTX_LFLAG_COMPACTED );

	if ( IsModified( rec ) )
		out->flags |= RTX_LFLAG_MODIFIED;

	if ( ( rec->flags & RTX_LFLAG_MUTED ) || IsSoloHidden( rec ) )
		out->flags |= RTX_LFLAG_MUTED;

	if ( id == g_dragId )
		out->flags |= RTX_LFLAG_DRAGGING;

	VectorCopy( original ? rec->origOrigin : rec->origin, out->origin );
	VectorCopy( original ? rec->origColor : rec->color, out->color );
	out->intensity = original ? rec->origIntensity : rec->intensity;
	out->radius = original ? rec->origRadius : rec->radius;
	Q_strncpyz( out->name, original ? rec->origName : rec->name, sizeof(out->name) );

	out->width = original ? rec->origWidth : rec->width;
	out->height = original ? rec->origHeight : rec->height;
	out->roll = original ? rec->origRoll : rec->roll;
	out->twoSided = original ? rec->origTwoSided : rec->twoSided;
	out->style = original ? rec->origStyle : rec->style;

	rtxLightRecord_t	view = *rec;

	if ( original )
	{
		view.type = rec->origType;
		Com_Memcpy( view.spot, rec->origSpot, sizeof(view.spot) );
	}

	RTX_LightEdit_GetSpotData( &view, out->dir, &out->coneOuter, &out->coneInner );
}

static qboolean LE_IsAvailable( void )
{
	return RTX_LightEdit_IsReady();
}

static qboolean LE_Begin( void )
{
	if ( !RTX_LightEdit_IsReady() )
	{
		RTX_LightEdit_SetError( "RTX light edit is not available" );
		return qfalse;
	}

	const int spare = g_world->num_light_polys - g_firstFree;

	if ( spare < LEDIT_MIN_SPARE )
		RTX_LightEdit_GrowSlots( LEDIT_MIN_SPARE - spare );

	return qtrue;
}

static void LE_EndDrag( int id );

static void LE_End( void )
{
	if ( g_dragId >= 0 )
		LE_EndDrag( g_dragId );
}

static int LE_Count( void )
{
	return RTX_LightEdit_NumRecords();
}

static qboolean LE_Get( int id, rtxLightDesc_t *out )
{
	const rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec || !out )
		return qfalse;

	FillDesc( id, rec, qfalse, out );

	return qtrue;
}

static qboolean LE_GetOriginal( int id, rtxLightDesc_t *out )
{
	const rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec || !out )
		return qfalse;

	FillDesc( id, rec, qtrue, out );

	return qtrue;
}

// Copies the editable values of a descriptor to a record, with limits.
static void CopyValues( rtxLightRecord_t *rec, const rtxLightDesc_t *desc )
{
	VectorCopy( desc->origin, rec->origin );

	for ( int k = 0; k < 3; k++ )
		rec->color[k] = ClampF( 0.0f, desc->color[k], 1.0f );

	rec->intensity = MAX( desc->intensity, 0.0f );
	rec->radius = desc->radius > 0.0f ? MAX( desc->radius, 0.5f ) : rec->radius;
	Q_strncpyz( rec->name, desc->name, sizeof(rec->name) );

	rec->type = desc->type == RTX_LTYPE_SPOT ? RTX_LTYPE_SPOT : desc->type == RTX_LTYPE_RECT ? RTX_LTYPE_RECT : RTX_LTYPE_SPHERE;
	rec->style = ClampStyle( desc->style );

	if ( rec->type == RTX_LTYPE_SPOT )
		RTX_LightEdit_SetSpotData( rec, desc->dir, desc->coneOuter, desc->coneInner );
	else if ( rec->type == RTX_LTYPE_RECT )
		RTX_LightEdit_SetRectData( rec, desc->dir, desc->width > 0.0f ? desc->width : rec->width,
			desc->height > 0.0f ? desc->height : rec->height, desc->roll, desc->twoSided );
}

// Copies the current rectangle values and the style to the original ones.
static void SetOriginalShape( rtxLightRecord_t *rec )
{
	rec->origWidth = rec->width;
	rec->origHeight = rec->height;
	rec->origRoll = rec->roll;
	rec->origTwoSided = rec->twoSided;
	rec->origStyle = rec->style;
}

static int LE_Add( const rtxLightDesc_t *desc )
{
	if ( !RTX_LightEdit_IsReady() )
	{
		RTX_LightEdit_SetError( "RTX light edit is not available" );
		return -1;
	}

	if ( !desc )
	{
		RTX_LightEdit_SetError( "no light description" );
		return -1;
	}

	if ( desc->type == RTX_LTYPE_SURFACE )
	{
		RTX_LightEdit_SetError( "an emissive surface cannot be added" );
		return -1;
	}

	const int cluster = PointCluster( desc->origin );

	if ( cluster < 0 )
	{
		RTX_LightEdit_SetError( "point in solid" );
		return -1;
	}

	if ( ClusterFull( cluster, SlotsOfValues( desc->type, desc->twoSided ) ) )
	{
		RTX_LightEdit_SetError( "cluster full" );
		return -1;
	}

	const int id = RTX_LightEdit_NewRecord( RTX_LSRC_ADDED, -1 );
	rtxLightRecord_t *rec = &g_records[id];

	CopyValues( rec, desc );

	VectorCopy( rec->origin, rec->origOrigin );
	VectorCopy( rec->color, rec->origColor );
	rec->origIntensity = rec->intensity;
	rec->origRadius = rec->radius;
	Q_strncpyz( rec->origName, rec->name, sizeof(rec->origName) );
	rec->origType = rec->type;
	Com_Memcpy( rec->origSpot, rec->spot, sizeof(rec->origSpot) );
	SetOriginalShape( rec );

	if ( !RTX_LightEdit_EnsureSlot( rec ) )
	{
		g_records.pop_back();
		return -1;
	}

	RTX_LightEdit_ApplyRecord( rec );
	RequestRebuild();
	RTX_LightEdit_CountChange( 1 );
	g_generation++;

	return id;
}

static qboolean LE_Set( int id, const rtxLightDesc_t *desc )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec )
		return qfalse;

	if ( !desc )
	{
		RTX_LightEdit_SetError( "no light description" );
		return qfalse;
	}

	if ( rec->flags & RTX_LFLAG_DELETED )
	{
		RTX_LightEdit_SetError( "light %i is deleted", id );
		return qfalse;
	}

	if ( rec->source == RTX_LSRC_EMISSIVE )
	{
		// The cast light changes; the polygons keep their place, so no cluster check is needed.
		rec->intensity = ClampF( 0.0f, desc->intensity, LEDIT_EMISSIVE_MAX_FACTOR );

		for ( int k = 0; k < 3; k++ )
			rec->color[k] = ClampF( 0.0f, desc->color[k], 1.0f );

		rec->style = ClampStyle( desc->style );

		if ( RTX_LightEdit_ApplyRecord( rec ) )
			ClusterChanged( id );

		RTX_LightEdit_CountChange( 1 );
		g_generation++;

		return qtrue;
	}

	if ( desc->type == RTX_LTYPE_SURFACE )
	{
		RTX_LightEdit_SetError( "light %i is not an emissive surface", id );
		return qfalse;
	}

	const int newCluster = PointCluster( desc->origin );

	if ( !IsInactive( rec ) && newCluster >= 0 )
	{
		const int oldCluster = rec->lightIndex >= 0 ? g_world->light_polys[rec->lightIndex].cluster : -1;
		const int newSlots = SlotsOfValues( desc->type, desc->twoSided );
		const int need = newCluster != oldCluster ? newSlots : newSlots - RTX_LightEdit_SlotCount( rec );

		if ( need > 0 && ClusterFull( newCluster, need ) )
		{
			RTX_LightEdit_SetError( "cluster full" );
			return qfalse;
		}

		if ( !EnsureSlots( rec, newSlots ) )
			return qfalse;
	}

	CopyValues( rec, desc );

	if ( RTX_LightEdit_ApplyRecord( rec ) )
		ClusterChanged( id );

	RTX_LightEdit_CountChange( 1 );
	g_generation++;

	return qtrue;
}

static qboolean LE_Remove( int id )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec )
		return qfalse;

	const int flag = rec->source == RTX_LSRC_ADDED ? RTX_LFLAG_DELETED : RTX_LFLAG_DISABLED;

	if ( rec->flags & flag )
	{
		RTX_LightEdit_SetError( "light %i is already removed", id );
		return qfalse;
	}

	rec->flags |= flag;

	if ( RTX_LightEdit_ApplyRecord( rec ) )
		RequestRebuild();

	RTX_LightEdit_CountChange( 1 );
	g_generation++;

	return qtrue;
}

static qboolean LE_Restore( int id )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec )
		return qfalse;

	if ( !IsRemoved( rec ) )
	{
		RTX_LightEdit_SetError( "light %i is not removed", id );
		return qfalse;
	}

	const qboolean	surface = (qboolean)( rec->source == RTX_LSRC_EMISSIVE );
	const int		cluster = surface ? 0 : PointCluster( rec->origin );

	if ( surface ? GroupClusterFull( rec ) : ( cluster >= 0 && ClusterFull( cluster, RTX_LightEdit_SlotCount( rec ) ) ) )
	{
		RTX_LightEdit_SetError( "cluster full" );
		return qfalse;
	}

	// A compacted light has no slots. Take them before the flags change: nothing changes on failure.
	if ( cluster >= 0 && !EnsureSlots( rec, RTX_LightEdit_SlotCount( rec ) ) )
		return qfalse;

	rec->flags &= ~( RTX_LFLAG_DELETED | RTX_LFLAG_DISABLED | RTX_LFLAG_COMPACTED );

	if ( RTX_LightEdit_ApplyRecord( rec ) )
		RequestRebuild();

	RTX_LightEdit_CountChange( 1 );
	g_generation++;

	return qtrue;
}

// Releases the slots of every removed light, then packs the used slots to the front of the pool.
// A live record can move to a lower slot: it loses its ReSTIR history and its light stats.
static int LE_Compact( int *slotsReleased )
{
	struct slotRef_t {
		int		slot;
		int		*ptr;
	};

	std::vector<int>		holes;
	std::vector<slotRef_t>	live;
	const vec3_t			zero = { 0.0f, 0.0f, 0.0f };
	int						lights = 0;

	if ( slotsReleased )
		*slotsReleased = 0;

	if ( !RTX_LightEdit_IsReady() )
	{
		RTX_LightEdit_SetError( "RTX light edit is not available" );
		return -1;
	}

	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		rtxLightRecord_t *rec = &g_records[i];

		if ( !IsRemoved( rec ) || ( rec->flags & RTX_LFLAG_COMPACTED ) )
			continue;

		const int capacity = RTX_LightEdit_SlotCapacity( rec );

		for ( int k = 0; k < capacity; k++ )
		{
			int *slot = RTX_LightEdit_SlotPtr( rec, k );

			if ( *slot < 0 )
				continue;

			RTX_LightEdit_Tombstone( *slot, zero );
			holes.push_back( *slot );
			*slot = -1;
		}

		rec->flags |= RTX_LFLAG_COMPACTED;
		lights++;
	}

	if ( !lights )
		return 0;

	// A drag of a compacted light ends: its record has no slot any more.
	g_dragId = -1;

	std::sort( holes.begin(), holes.end() );

	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		const int capacity = RTX_LightEdit_SlotCapacity( &g_records[i] );

		for ( int k = 0; k < capacity; k++ )
		{
			int *slot = RTX_LightEdit_SlotPtr( &g_records[i], k );

			if ( *slot >= 0 )
			{
				slotRef_t ref = { *slot, slot };

				live.push_back( ref );
			}
		}
	}

	// The highest live slots fill the lowest holes.
	std::sort( live.begin(), live.end(), []( const slotRef_t &a, const slotRef_t &b ) { return a.slot > b.slot; } );

	size_t	next = 0;
	int		moved = 0;

	for ( size_t h = 0; h < holes.size() && next < live.size() && live[next].slot > holes[h]; h++, next++ )
	{
		g_world->light_polys[holes[h]] = g_world->light_polys[live[next].slot];
		RTX_LightEdit_Tombstone( live[next].slot, zero );
		*live[next].ptr = holes[h];
		moved++;
	}

	// Every slot below the new boundary is a live slot of a record, or a polygon that no record holds.
	int first = g_floor;

	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		const int capacity = RTX_LightEdit_SlotCapacity( &g_records[i] );

		for ( int k = 0; k < capacity; k++ )
			first = MAX( first, *RTX_LightEdit_SlotPtr( &g_records[i], k ) + 1 );
	}

	g_firstFree = first;

	// The stats of a moved slot belong to the light that left it.
	if ( moved )
		RecreateLightStats( *g_world );

	RequestRebuild();
	g_generation++;

	if ( slotsReleased )
		*slotsReleased = (int)holes.size();

	return lights;
}

static qboolean LE_Revert( int id )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec )
		return qfalse;

	if ( rec->flags & RTX_LFLAG_DELETED )
	{
		RTX_LightEdit_SetError( "light %i is deleted", id );
		return qfalse;
	}

	const int cluster = rec->source == RTX_LSRC_EMISSIVE ? -1 : PointCluster( rec->origOrigin );

	if ( !IsInactive( rec ) && cluster >= 0 )
	{
		const int oldCluster = rec->lightIndex >= 0 ? g_world->light_polys[rec->lightIndex].cluster : -1;
		const int newSlots = SlotsOfValues( rec->origType, rec->origTwoSided );
		const int need = cluster != oldCluster ? newSlots : newSlots - RTX_LightEdit_SlotCount( rec );

		if ( need > 0 && ClusterFull( cluster, need ) )
		{
			RTX_LightEdit_SetError( "cluster full" );
			return qfalse;
		}

		if ( !EnsureSlots( rec, newSlots ) )
			return qfalse;
	}

	VectorCopy( rec->origOrigin, rec->origin );
	VectorCopy( rec->origColor, rec->color );
	rec->intensity = rec->origIntensity;
	rec->radius = rec->origRadius;
	Q_strncpyz( rec->name, rec->origName, sizeof(rec->name) );
	rec->type = rec->origType;
	Com_Memcpy( rec->spot, rec->origSpot, sizeof(rec->spot) );
	rec->width = rec->origWidth;
	rec->height = rec->origHeight;
	rec->roll = rec->origRoll;
	rec->twoSided = rec->origTwoSided;
	rec->style = rec->origStyle;

	if ( RTX_LightEdit_ApplyRecord( rec ) )
		RequestRebuild();

	RTX_LightEdit_CountChange( 1 );
	g_generation++;

	return qtrue;
}

static void LE_BeginDrag( int id )
{
	if ( !RTX_LightEdit_GetRecord( id ) )
		return;

	g_dragId = id;
	g_pendingRebuild = qfalse;
}

static void LE_EndDrag( int id )
{
	if ( g_dragId != id )
		return;

	g_dragId = -1;

	// A pending rebuild stays pending while a batch is open.
	if ( g_pendingRebuild && g_batch == 0 && RTX_LightEdit_IsReady() )
		RTX_LightEdit_RebuildClusters();
}

static void LE_BeginBatch( void )
{
	g_batch++;
}

static void LE_EndBatch( void )
{
	if ( g_batch > 0 )
		g_batch--;

	if ( g_batch == 0 && g_pendingRebuild && RTX_LightEdit_IsReady() )
		RTX_LightEdit_RebuildClusters();
}

// Mute is a session state: no edit count, no file entry.
static qboolean LE_Mute( int id, qboolean muted )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec )
		return qfalse;

	if ( !!( rec->flags & RTX_LFLAG_MUTED ) == !!muted )
		return qtrue;

	// Unmuting a light that then emits needs room in the cluster lists.
	if ( !muted && !IsRemoved( rec ) && !IsSoloHidden( rec ) )
	{
		const qboolean	surface = (qboolean)( rec->source == RTX_LSRC_EMISSIVE );
		const int		cluster = surface ? 0 : PointCluster( rec->origin );

		if ( surface ? GroupClusterFull( rec ) : ( cluster >= 0 && ClusterFull( cluster, RTX_LightEdit_SlotCount( rec ) ) ) )
		{
			RTX_LightEdit_SetError( "cluster full" );
			return qfalse;
		}
	}

	if ( muted )
		rec->flags |= RTX_LFLAG_MUTED;
	else
		rec->flags &= ~RTX_LFLAG_MUTED;

	if ( RTX_LightEdit_ApplyRecord( rec ) )
		RequestRebuild();

	g_generation++;

	return qtrue;
}

static void LE_Solo( int id )
{
	if ( !RTX_LightEdit_IsReady() )
		return;

	if ( id < 0 || id >= (int)g_records.size() )
		id = -1;

	if ( id == g_solo )
		return;

	g_solo = id;

	const int overfullBefore = g_overfull;
	qboolean changed = qfalse;

	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		if ( RTX_LightEdit_ApplyRecord( &g_records[i] ) )
			changed = qtrue;
	}

	if ( changed )
		RequestRebuild();

	// Solo cannot fail: report the clusters that lost lights. Inside a batch the rebuild is
	// still pending and g_overfull is stale.
	if ( g_overfull > overfullBefore )
		RTX_LightEdit_SetError( "cluster full: %i clusters truncated", g_overfull );

	g_generation++;
}

static int LE_GetSolo( void )
{
	return RTX_LightEdit_IsReady() ? g_solo : -1;
}

static int LE_Generation( void )
{
	return g_generation;
}

// Class scale of copy_light, read live, times pt_lightgen_scale for an lgt light.
static float LE_IntensityScale( int id )
{
	static cvar_t *scaleEdit;

	if ( !scaleEdit )
		scaleEdit = ri.Cvar_Get( "pt_light_scale_edit", "0.1", CVAR_ARCHIVE_ND );

	int	entClass = LIGHT_ENT_EDIT;
	int	source = RTX_LSRC_ADDED;

	if ( id >= 0 )
	{
		const rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

		if ( !rec )
			return 0.0f;

		// The factor of an emissive surface multiplies the polygons as they are.
		if ( rec->source == RTX_LSRC_EMISSIVE )
			return 1.0f;

		entClass = rec->entClass;
		source = rec->source;
	}

	const float scale = entClass == LIGHT_ENT_EDIT ? MAX( 0.0f, scaleEdit->value ) : 1.0f;

	return scale * ( source == RTX_LSRC_LGT ? pt_lightgen_scale->value : 1.0f );
}

static qboolean LE_Save( void )
{
	if ( !RTX_LightEdit_IsReady() )
	{
		RTX_LightEdit_SetError( "RTX light edit is not available" );
		return qfalse;
	}

	return RTX_LightFile_Save();
}

static qboolean LE_Reload( void )
{
	if ( !RTX_LightEdit_IsReady() )
	{
		RTX_LightEdit_SetError( "RTX light edit is not available" );
		return qfalse;
	}

	LE_End();

	g_solo = -1;
	g_generation++;

	return RTX_LightFile_Reload();
}

static void LE_GetStats( rtxLightStats_t *out )
{
	if ( !out )
		return;

	Com_Memset( out, 0, sizeof(*out) );
	out->maxLightPolys = LEDIT_MAX_SLOTS;

	if ( !RTX_LightEdit_IsReady() )
		return;

	out->numRecords = (int)g_records.size();

	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		const rtxLightRecord_t *rec = &g_records[i];

		if ( rec->source == RTX_LSRC_ENTITY )
			out->numEntity++;
		else if ( rec->source == RTX_LSRC_LGT )
			out->numLgt++;
		else if ( rec->source == RTX_LSRC_EMISSIVE )
			out->numEmissive++;
		else if ( !( rec->flags & RTX_LFLAG_DELETED ) )
			out->numAdded++;

		if ( IsModified( rec ) )
			out->numModified++;

		if ( rec->flags & RTX_LFLAG_COMPACTED )
			out->numCompacted++;
		else if ( rec->flags & RTX_LFLAG_DISABLED )
			out->numDisabled++;

		if ( rec->flags & RTX_LFLAG_IN_SOLID )
			out->numInSolid++;
	}

	out->numEmissivePolys = RTX_LightEmissive_NumPolys();
	out->numLightPolys = g_world->num_light_polys;
	out->slotsUsed = g_firstFree;
	out->capacity = g_world->allocated_light_polys;
	out->overfullClusters = g_overfull;
	out->rebuilds = g_rebuilds;
	out->unsavedChanges = g_unsaved;
	Q_strncpyz( out->mapName, g_world->baseName, sizeof(out->mapName) );
}

static const char *LE_LastError( void )
{
	return g_lastError;
}

static qboolean LE_PointInSolid( const vec3_t point )
{
	if ( !RTX_LightEdit_IsReady() )
		return qtrue;

	return (qboolean)( PointCluster( point ) < 0 );
}

// Colour scaled to max 1; a grey 0.5 when all channels are zero.
static void LE_NormalizeColor( const vec3_t in, vec3_t out )
{
	const float m = MAX( in[0], MAX( in[1], in[2] ) );

	if ( m <= 0.0f )
		VectorSet( out, 0.5f, 0.5f, 0.5f );
	else
		VectorScale( in, 1.0f / m, out );
}

static qboolean LE_GetEmissiveSurface( int id, rtxEmissiveSurface_t *out )
{
	const rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec || !out )
		return qfalse;

	if ( rec->source != RTX_LSRC_EMISSIVE )
	{
		RTX_LightEdit_SetError( "light %i is not an emissive surface", id );
		return qfalse;
	}

	return RTX_LightEmissive_GetSurface( rec, out );
}

// Spheres and spots give their position in positions[0..2]; polygon lights in off_center.
static int LE_GetDynamic( int maxCount, vec3_t *origins, vec3_t *colors )
{
	const light_poly_t	*lights;
	const int			num = vk_rtx_get_model_lights( &lights );
	int					n = 0;

	if ( !origins || !colors )
		return 0;

	for ( int i = 0; i < num && n < maxCount; i++ )
	{
		const light_poly_t *l = lights + i;

		if ( l->type == LIGHT_SPHERE || l->type == LIGHT_SPOT )
			VectorCopy( l->positions, origins[n] );
		else
			VectorCopy( l->off_center, origins[n] );

		LE_NormalizeColor( l->color, colors[n] );
		n++;
	}

	return n;
}

// Emissive shader scales (rtx_light_emissive.cpp) and the sky (rtx_light_sky.cpp).
static int LE_CountEmissiveShaders( void )
{
	return RTX_LightEmissive_Count();
}

static qboolean LE_GetEmissiveShader( int shader, char *name, int nameSize, float *scale, int *numPolys )
{
	return RTX_LightEmissive_Get( shader, name, nameSize, scale, numPolys );
}

static qboolean LE_SetEmissiveScale( int shader, float scale )
{
	return RTX_LightEmissive_Set( shader, scale );
}

static qboolean LE_GetSky( rtxSkyDesc_t *out )
{
	return RTX_LightSky_Get( out );
}

static qboolean LE_SetSky( const rtxSkyDesc_t *desc )
{
	return RTX_LightSky_Set( desc );
}

static void LE_ResetSky( void )
{
	RTX_LightSky_Reset();
}

static rtxLightEditAPI_t g_api = {
	RTX_LIGHTEDIT_API_VERSION,
	LE_IsAvailable,
	LE_Begin,
	LE_End,
	LE_Count,
	LE_Get,
	LE_GetOriginal,
	LE_Add,
	LE_Set,
	LE_Remove,
	LE_Restore,
	LE_Revert,
	LE_BeginDrag,
	LE_EndDrag,
	LE_Save,
	LE_Reload,
	LE_GetStats,
	LE_LastError,
	LE_PointInSolid,
	LE_BeginBatch,
	LE_EndBatch,
	LE_Mute,
	LE_Solo,
	LE_GetSolo,
	LE_Generation,
	LE_IntensityScale,
	LE_GetDynamic,
	LE_CountEmissiveShaders,
	LE_GetEmissiveShader,
	LE_SetEmissiveScale,
	LE_GetSky,
	LE_SetSky,
	LE_ResetSky,
	LE_Compact,
	LE_GetEmissiveSurface
};

void *RTX_LightEdit_GetExtension( const char *name )
{
	if ( name && !Q_stricmp( name, RTX_LIGHTEDIT_API_NAME ) )
		return &g_api;

	return NULL;
}

/*
=================
Console commands
=================
*/

static const char *SourceName( int source )
{
	return source == RTX_LSRC_ENTITY ? "entity" : source == RTX_LSRC_LGT ? "lgt" : source == RTX_LSRC_EMISSIVE ? "emissive" : "added";
}

void RTX_LightEdit_List_f( void )
{
	if ( !LE_IsAvailable() )
	{
		Com_Printf( "light edit is not available\n" );
		return;
	}

	for ( int i = 0; i < LE_Count(); i++ )
	{
		rtxLightDesc_t	d;

		LE_Get( i, &d );

		Com_Printf( "%4i %-8s key %-4i %-7s flags 0x%02x origin %.0f %.0f %.0f intensity %.2f radius %.1f style %i slot %i\n",
			i, SourceName( d.source ), d.sourceKey, d.type == RTX_LTYPE_SPOT ? "spot" : d.type == RTX_LTYPE_RECT ? "rect"
			: d.type == RTX_LTYPE_SURFACE ? "surface" : "sphere",
			d.flags, d.origin[0], d.origin[1], d.origin[2], d.intensity, d.radius, d.style,
			RTX_LightEdit_SlotCapacity( &g_records[i] ) > 0 ? *RTX_LightEdit_SlotPtr( &g_records[i], 0 ) : -1 );

		if ( d.type == RTX_LTYPE_RECT )
		{
			Com_Printf( "     rect %.1f x %.1f roll %.1f %s dir %.2f %.2f %.2f slots %i %i %i\n", d.width, d.height, d.roll,
				d.twoSided ? "two-sided" : "one-sided", d.dir[0], d.dir[1], d.dir[2],
				g_records[i].extraSlots[0], g_records[i].extraSlots[1], g_records[i].extraSlots[2] );
		}
	}
}

// True when the argument starts like a number.
static qboolean IsNumberArg( const char *s )
{
	return (qboolean)( ( s[0] >= '0' && s[0] <= '9' ) || ( ( s[0] == '-' || s[0] == '.' ) && s[1] ) );
}

// Reads a rectangle keyword and its values at argument i. Gives the number of arguments it used, 0 when
// the keyword is unknown.
static int ParseShapeArgs( rtxLightDesc_t *d, int i )
{
	const char *key = ri.Cmd_Argv( i );
	const int	argc = ri.Cmd_Argc();

	if ( !Q_stricmp( key, "rect" ) )
	{
		d->type = RTX_LTYPE_RECT;

		if ( i + 2 < argc && IsNumberArg( ri.Cmd_Argv( i + 1 ) ) && IsNumberArg( ri.Cmd_Argv( i + 2 ) ) )
		{
			d->width = atof( ri.Cmd_Argv( i + 1 ) );
			d->height = atof( ri.Cmd_Argv( i + 2 ) );
			return 3;
		}

		return 1;
	}

	if ( !Q_stricmp( key, "dir" ) && i + 3 < argc )
	{
		for ( int k = 0; k < 3; k++ )
			d->dir[k] = atof( ri.Cmd_Argv( i + 1 + k ) );

		return 4;
	}

	if ( i + 1 >= argc )
		return 0;

	if ( !Q_stricmp( key, "roll" ) )
		d->roll = atof( ri.Cmd_Argv( i + 1 ) );
	else if ( !Q_stricmp( key, "twosided" ) )
		d->twoSided = atoi( ri.Cmd_Argv( i + 1 ) ) != 0;
	else if ( !Q_stricmp( key, "style" ) )
		d->style = atoi( ri.Cmd_Argv( i + 1 ) );
	else
		return 0;

	return 2;
}

void RTX_LightEdit_Add_f( void )
{
	if ( ri.Cmd_Argc() < 4 )
	{
		Com_Printf( "usage: pt_ledit_add x y z [intensity] [r g b] [rect w h] [dir x y z] [roll deg] [twosided 0|1] [style n]\n" );
		return;
	}

	rtxLightDesc_t	d;
	float			nums[4];
	int				numCount = 0;
	int				arg = 4;

	Com_Memset( &d, 0, sizeof(d) );

	for ( int k = 0; k < 3; k++ )
		d.origin[k] = atof( ri.Cmd_Argv( 1 + k ) );

	while ( arg < ri.Cmd_Argc() && numCount < 4 && IsNumberArg( ri.Cmd_Argv( arg ) ) )
		nums[numCount++] = atof( ri.Cmd_Argv( arg++ ) );

	d.intensity = numCount > 0 ? nums[0] : 300.0f;
	VectorSet( d.color, 1.0f, 1.0f, 1.0f );

	if ( numCount == 4 )
		VectorSet( d.color, nums[1], nums[2], nums[3] );

	VectorSet( d.dir, 0.0f, 0.0f, -1.0f );

	while ( arg < ri.Cmd_Argc() )
	{
		const int used = ParseShapeArgs( &d, arg );

		if ( !used )
		{
			Com_Printf( "light edit: unknown argument %s\n", ri.Cmd_Argv( arg ) );
			return;
		}

		arg += used;
	}

	d.radius = LEDIT_ADD_RADIUS;

	if ( !LE_Begin() )
	{
		Com_Printf( "light edit: %s\n", g_lastError );
		return;
	}

	const int id = LE_Add( &d );

	if ( id < 0 )
		Com_Printf( "light edit: %s\n", g_lastError );
	else
		Com_Printf( "light %i added\n", id );
}

void RTX_LightEdit_Set_f( void )
{
	if ( ri.Cmd_Argc() < 3 )
	{
		Com_Printf( "usage: pt_ledit_set <id> <origin|color|intensity|radius|spot|sphere|rect [w h]|dir x y z|roll deg|twosided 0|1|style n> <values...>\n" );
		return;
	}

	const int		id = atoi( ri.Cmd_Argv( 1 ) );
	const char		*what = ri.Cmd_Argv( 2 );
	rtxLightDesc_t	d;

	if ( !LE_Get( id, &d ) )
	{
		Com_Printf( "light edit: %s\n", g_lastError );
		return;
	}

	if ( !Q_stricmp( what, "origin" ) && ri.Cmd_Argc() >= 6 )
	{
		for ( int k = 0; k < 3; k++ )
			d.origin[k] = atof( ri.Cmd_Argv( 3 + k ) );
	}
	else if ( !Q_stricmp( what, "color" ) && ri.Cmd_Argc() >= 6 )
	{
		for ( int k = 0; k < 3; k++ )
			d.color[k] = atof( ri.Cmd_Argv( 3 + k ) );
	}
	else if ( !Q_stricmp( what, "intensity" ) )
		d.intensity = atof( ri.Cmd_Argv( 3 ) );
	else if ( !Q_stricmp( what, "radius" ) )
		d.radius = atof( ri.Cmd_Argv( 3 ) );
	else if ( !Q_stricmp( what, "spot" ) && ri.Cmd_Argc() >= 8 )
	{
		d.type = RTX_LTYPE_SPOT;

		for ( int k = 0; k < 3; k++ )
			d.dir[k] = atof( ri.Cmd_Argv( 3 + k ) );

		d.coneOuter = atof( ri.Cmd_Argv( 6 ) );
		d.coneInner = atof( ri.Cmd_Argv( 7 ) );
	}
	else if ( !Q_stricmp( what, "sphere" ) )
		d.type = RTX_LTYPE_SPHERE;
	else if ( !ParseShapeArgs( &d, 2 ) )
	{
		Com_Printf( "usage: pt_ledit_set <id> <origin|color|intensity|radius|spot|sphere|rect [w h]|dir x y z|roll deg|twosided 0|1|style n> <values...>\n" );
		return;
	}

	if ( !LE_Set( id, &d ) )
		Com_Printf( "light edit: %s\n", g_lastError );
}

void RTX_LightEdit_Del_f( void )
{
	if ( ri.Cmd_Argc() < 2 )
	{
		Com_Printf( "usage: pt_ledit_del <id>\n" );
		return;
	}

	if ( !LE_Remove( atoi( ri.Cmd_Argv( 1 ) ) ) )
		Com_Printf( "light edit: %s\n", g_lastError );
}

void RTX_LightEdit_Restore_f( void )
{
	if ( ri.Cmd_Argc() < 2 )
	{
		Com_Printf( "usage: pt_ledit_restore <id>\n" );
		return;
	}

	if ( !LE_Restore( atoi( ri.Cmd_Argv( 1 ) ) ) )
		Com_Printf( "light edit: %s\n", g_lastError );
}

void RTX_LightEdit_Mute_f( void )
{
	if ( ri.Cmd_Argc() < 3 )
	{
		Com_Printf( "usage: pt_ledit_mute <id> <0|1>\n" );
		return;
	}

	if ( !LE_Mute( atoi( ri.Cmd_Argv( 1 ) ), (qboolean)( atoi( ri.Cmd_Argv( 2 ) ) != 0 ) ) )
		Com_Printf( "light edit: %s\n", g_lastError );
}

void RTX_LightEdit_Solo_f( void )
{
	if ( ri.Cmd_Argc() < 2 )
	{
		Com_Printf( "usage: pt_ledit_solo <id|-1>\n" );
		return;
	}

	LE_Solo( atoi( ri.Cmd_Argv( 1 ) ) );
}

void RTX_LightEdit_Stats_f( void )
{
	rtxLightStats_t s;

	LE_GetStats( &s );

	Com_Printf( "map %s: %i records (%i entity, %i lgt, %i added), %i modified, %i disabled, %i compacted, %i in solid, %i emissive\n",
		s.mapName, s.numRecords, s.numEntity, s.numLgt, s.numAdded, s.numModified, s.numDisabled, s.numCompacted, s.numInSolid, s.numEmissive );
	Com_Printf( "slots %i used of %i (entries %i, capacity %i), %i full clusters, %i rebuilds, %i unsaved changes\n",
		s.slotsUsed, s.maxLightPolys, s.numLightPolys, s.capacity, s.overfullClusters, s.rebuilds, s.unsavedChanges );
}

// pt_ledit_compact: releases the slots of the removed lights.
void RTX_LightEdit_Compact_f( void )
{
	int	slots;
	const int lights = LE_Compact( &slots );

	if ( lights < 0 )
	{
		Com_Printf( "light edit: %s\n", g_lastError );
		return;
	}

	if ( lights == 0 )
	{
		Com_Printf( "light edit: no removed light to compact\n" );
		return;
	}

	Com_Printf( "light edit: compacted %i lights, released %i slots, slots used %i/%i\n",
		lights, slots, g_firstFree, LEDIT_MAX_SLOTS );
}

// pt_ledit_emissive [n]: prints the first n emissive surfaces and the count.
void RTX_LightEdit_Emissive_f( void )
{
	int			total = 0;
	const int	limit = ri.Cmd_Argc() > 1 ? atoi( ri.Cmd_Argv( 1 ) ) : 10;

	if ( !LE_IsAvailable() )
	{
		Com_Printf( "light edit is not available\n" );
		return;
	}

	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		rtxEmissiveSurface_t s;

		if ( g_records[i].source != RTX_LSRC_EMISSIVE || !RTX_LightEmissive_GetSurface( &g_records[i], &s ) )
			continue;

		if ( total++ < limit )
			Com_Printf( "%4i: face %i rank %i %s, %i polygons, %i slots, centre %.0f %.0f %.0f, factor %g\n", (int)i, s.face,
				s.rank, s.shaderName, s.numTris, s.numSlots, g_records[i].origin[0], g_records[i].origin[1],
				g_records[i].origin[2], g_records[i].intensity );
	}

	Com_Printf( "%i emissive surfaces\n", total );
}

// pt_ledit_dynamic: prints the dynamic lights of the last frame.
void RTX_LightEdit_Dynamic_f( void )
{
	vec3_t	origins[64], colors[64];
	const int n = LE_GetDynamic( 64, origins, colors );

	for ( int i = 0; i < n; i++ )
		Com_Printf( "%4i: origin %.0f %.0f %.0f colour %.2f %.2f %.2f\n", i,
			origins[i][0], origins[i][1], origins[i][2], colors[i][0], colors[i][1], colors[i][2] );

	Com_Printf( "%i dynamic lights\n", n );
}
