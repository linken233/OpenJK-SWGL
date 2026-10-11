/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Emission factor of each emissive shader of the world. See rtx_light_emissive.h.
//
// The table lists the shaders of the world emissive lights. A shader name can have several
// shader indices (one per lightmap variant): all of them get the factor of the name.
//
// The polygons of one BSP surface form a group, which is the record of an emissive surface. The group
// keeps the polygons as collected and the slots that hold them in world->light_polys.

#include "../tr_local.h"
#include "rtx_light_edit.h"
#include "rtx_light_emissive.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <map>
#include <array>

#define EMISSIVE_MAX_SCALE	100.0f

struct emissiveShader_t {
	std::string	name;
	int			numPolys;
	float		scale;
};

struct emissiveGroup_t {
	int							record;			// id of the record
	int							shader;			// index in the table of shaders
	int							face;			// world surface
	int							rank;			// rank of the face among the faces of the shader
	vec3_t						mins, maxs;
	vec3_t						check;			// centre of the box
	std::vector<int>			slots;			// slot of each polygon, -1 when it has none
	std::vector<light_poly_t>	base;			// the polygons as collected
	int							numEdges;
	float						edges[RTX_LEMI_MAX_EDGES][6];
};

float g_rtxEmissiveScale[MAX_SHADERS];

static float Com_Clamp( float lo, float hi, float v )
{
	return v < lo ? lo : v > hi ? hi : v;
}

static std::vector<emissiveShader_t>	g_shaders;
static std::vector<emissiveGroup_t>		g_groups;
static rtxEmissiveScales_t				g_kept;			// file entries for shaders that are not in the map
static int								g_generation;	// counts the changes of a factor

static void FillOnes( void )
{
	for ( int i = 0; i < MAX_SHADERS; i++ )
		g_rtxEmissiveScale[i] = 1.0f;
}

static struct emissiveInit_t {
	emissiveInit_t() { FillOnes(); }
} g_emissiveInit;

static int FindByName( const char *name )
{
	for ( size_t i = 0; i < g_shaders.size(); i++ )
	{
		if ( !Q_stricmp( g_shaders[i].name.c_str(), name ) )
			return (int)i;
	}

	return -1;
}

// Gives the factor of an entry to every shader index of that name.
static void ApplyScale( const emissiveShader_t &e )
{
	g_generation++;

	for ( int s = 0; s < tr.numShaders && s < MAX_SHADERS; s++ )
	{
		if ( tr.shaders[s] && !Q_stricmp( tr.shaders[s]->name, e.name.c_str() ) )
			g_rtxEmissiveScale[s] = e.scale;
	}
}

int RTX_LightEmissive_Generation( void )
{
	return g_generation;
}

void RTX_LightEmissive_Reset( void )
{
	g_generation++;
	FillOnes();
	g_shaders.clear();
	g_groups.clear();
	g_kept.clear();
}

typedef std::array<int, 6>	edgeKey_t;

struct edgeInfo_t {
	int		count;
	float	p[6];
};

// Integer key of an edge: two points on a half-unit grid, in a fixed order.
static edgeKey_t MakeEdgeKey( const float *a, const float *b )
{
	edgeKey_t	ka, kb;

	for ( int k = 0; k < 3; k++ )
	{
		ka[k] = (int)floorf( a[k] * 2.0f + 0.5f );
		kb[k] = (int)floorf( b[k] * 2.0f + 0.5f );
	}

	for ( int k = 0; k < 3; k++ )
	{
		ka[3 + k] = kb[k];
		kb[3 + k] = ka[k];
	}

	return ka < kb ? ka : kb;
}

// The outline is the set of edges that one polygon uses. When the set is too large it is the box.
static void BuildOutline( emissiveGroup_t &g )
{
	std::map<edgeKey_t, edgeInfo_t>	edges;

	for ( size_t i = 0; i < g.base.size(); i++ )
	{
		const float *p = g.base[i].positions;

		for ( int e = 0; e < 3; e++ )
		{
			const float *a = p + e * 3;
			const float *b = p + ( ( e + 1 ) % 3 ) * 3;
			edgeInfo_t	&info = edges[MakeEdgeKey( a, b )];

			if ( info.count == 0 )
			{
				VectorCopy( a, info.p );
				VectorCopy( b, info.p + 3 );
			}

			info.count++;
		}
	}

	g.numEdges = 0;

	for ( std::map<edgeKey_t, edgeInfo_t>::const_iterator it = edges.begin(); it != edges.end(); ++it )
	{
		if ( it->second.count != 1 )
			continue;

		if ( g.numEdges == RTX_LEMI_MAX_EDGES )
		{
			g.numEdges = 0;
			break;
		}

		Com_Memcpy( g.edges[g.numEdges++], it->second.p, sizeof(g.edges[0]) );
	}

	if ( g.numEdges > 0 )
		return;

	// The 12 edges of the box: corners numbered by their bits (x, y, z).
	for ( int c = 0; c < 8; c++ )
	{
		for ( int axis = 0; axis < 3; axis++ )
		{
			if ( c & ( 1 << axis ) )
				continue;

			float *e = g.edges[g.numEdges++];

			for ( int k = 0; k < 3; k++ )
			{
				e[k] = ( c & ( 1 << k ) ) ? g.maxs[k] : g.mins[k];
				e[3 + k] = ( ( c | ( 1 << axis ) ) & ( 1 << k ) ) ? g.maxs[k] : g.mins[k];
			}
		}
	}
}

// Name of the surface, at most the length of a record name, with the end of the shader path kept.
static void ShortName( const char *shader, char *out, int size )
{
	const int len = (int)strlen( shader );

	Q_strncpyz( out, len >= size ? shader + len - ( size - 1 ) : shader, size );
}

void RTX_LightEmissive_BuildGroups( world_t &w, const std::vector<char> &owned )
{
	std::map<std::string, int>		counters;
	std::vector<int>				rankOf( MAX( w.numsurfaces, 0 ), 0 );
	std::map<int, int>				faceToGroup;
	std::vector<int>				order;

	g_shaders.clear();
	g_groups.clear();

	// The rank of a face counts the faces of the same shader before it, whether they emit or not.
	for ( int i = 0; i < w.numsurfaces; i++ )
	{
		const shader_t *shader = w.surfaces[i].shader;
		std::string		name = shader ? shader->name : "";

		for ( size_t k = 0; k < name.size(); k++ )
			name[k] = (char)tolower( (unsigned char)name[k] );

		rankOf[i] = counters[name]++;
	}

	for ( int slot = 0; slot < w.num_light_polys; slot++ )
	{
		if ( ( slot < (int)owned.size() && owned[slot] ) )
			continue;

		const light_poly_t &light = w.light_polys[slot];
		const int			face = light.surface;

		if ( !light.material || light.type != LIGHT_POLYGON || face < 0 || face >= w.numsurfaces || !w.surfaces[face].shader )
			continue;

		std::map<int, int>::iterator	it = faceToGroup.find( face );

		if ( it == faceToGroup.end() )
		{
			emissiveGroup_t g = emissiveGroup_t();

			g.face = face;
			g.rank = rankOf[face];
			g.shader = FindByName( w.surfaces[face].shader->name );

			if ( g.shader < 0 )
			{
				emissiveShader_t e;

				e.name = w.surfaces[face].shader->name;
				e.numPolys = 0;
				e.scale = 1.0f;
				g_shaders.push_back( e );
				g.shader = (int)g_shaders.size() - 1;
			}

			g_groups.push_back( g );
			it = faceToGroup.insert( std::make_pair( face, (int)g_groups.size() - 1 ) ).first;
			order.push_back( it->second );
		}

		emissiveGroup_t &g = g_groups[it->second];

		g.slots.push_back( slot );
		g.base.push_back( light );
		g_shaders[g.shader].numPolys++;
	}

	for ( size_t n = 0; n < order.size(); n++ )
	{
		emissiveGroup_t	&g = g_groups[order[n]];
		vec3_t			centre, anchor;
		float			best = 1e30f;

		VectorSet( g.mins, 1e30f, 1e30f, 1e30f );
		VectorSet( g.maxs, -1e30f, -1e30f, -1e30f );

		for ( size_t i = 0; i < g.base.size(); i++ )
		{
			for ( int v = 0; v < 3; v++ )
			{
				for ( int k = 0; k < 3; k++ )
				{
					const float p = g.base[i].positions[v * 3 + k];

					g.mins[k] = MIN( g.mins[k], p );
					g.maxs[k] = MAX( g.maxs[k], p );
				}
			}
		}

		VectorAdd( g.mins, g.maxs, g.check );
		VectorScale( g.check, 0.5f, g.check );

		// The anchor is the centre of the polygon nearest to the box centre: it lies on the surface.
		VectorCopy( g.check, anchor );

		for ( size_t i = 0; i < g.base.size(); i++ )
		{
			VectorClear( centre );

			for ( int v = 0; v < 3; v++ )
				VectorAdd( centre, g.base[i].positions + v * 3, centre );

			VectorScale( centre, 1.0f / 3.0f, centre );

			vec3_t d;

			VectorSubtract( centre, g.check, d );

			if ( VectorLengthSquared( d ) < best )
			{
				best = VectorLengthSquared( d );
				VectorCopy( centre, anchor );
			}
		}

		BuildOutline( g );

		const int			id = RTX_LightEdit_NewRecord( RTX_LSRC_EMISSIVE, g.rank );
		rtxLightRecord_t	*rec = RTX_LightEdit_GetRecordRaw( id );
		vec3_t				half;

		VectorSubtract( g.maxs, g.mins, half );

		g.record = id;
		rec->emiGroup = order[n];
		rec->type = rec->origType = RTX_LTYPE_SURFACE;
		VectorCopy( anchor, rec->origin );
		VectorCopy( anchor, rec->origOrigin );
		rec->intensity = rec->origIntensity = 1.0f;
		rec->radius = rec->origRadius = MAX( 8.0f, 0.5f * VectorLength( half ) );
		ShortName( g_shaders[g.shader].name.c_str(), rec->name, sizeof(rec->name) );
		Q_strncpyz( rec->origName, rec->name, sizeof(rec->origName) );
	}
}

int RTX_LightEmissive_SlotCount( int group )
{
	return group >= 0 && group < (int)g_groups.size() ? (int)g_groups[group].slots.size() : 0;
}

int *RTX_LightEmissive_SlotPtr( int group, int i )
{
	return &g_groups[group].slots[i];
}

int RTX_LightEmissive_BaseCluster( int group, int i )
{
	return g_groups[group].base[i].cluster;
}

int RTX_LightEmissive_NumPolys( void )
{
	int n = 0;

	for ( size_t i = 0; i < g_groups.size(); i++ )
		n += (int)g_groups[i].slots.size();

	return n;
}

void RTX_LightEmissive_Fill( const rtxLightRecord_t *rec, int i, light_poly_t *out )
{
	const emissiveGroup_t &g = g_groups[rec->emiGroup];

	*out = g.base[i];

	// A negative colour marks a light that takes the sky radiance: it has no tint.
	if ( out->color[0] >= 0.0f )
	{
		for ( int k = 0; k < 3; k++ )
			out->color[k] *= rec->color[k] * rec->intensity;
	}

	out->style = rec->style;
}

void RTX_LightEmissive_GroupKey( int group, const char **shader, int *rank, float *check )
{
	const emissiveGroup_t &g = g_groups[group];

	*shader = g_shaders[g.shader].name.c_str();
	*rank = g.rank;
	VectorCopy( g.check, check );
}

int RTX_LightEmissive_FindRecord( const char *shader, int rank )
{
	for ( size_t i = 0; i < g_groups.size(); i++ )
	{
		if ( g_groups[i].rank == rank && !Q_stricmp( g_shaders[g_groups[i].shader].name.c_str(), shader ) )
			return g_groups[i].record;
	}

	return -1;
}

qboolean RTX_LightEmissive_GetSurface( const rtxLightRecord_t *rec, rtxEmissiveSurface_t *out )
{
	if ( rec->emiGroup < 0 || rec->emiGroup >= (int)g_groups.size() )
		return qfalse;

	const emissiveGroup_t &g = g_groups[rec->emiGroup];

	Com_Memset( out, 0, sizeof(*out) );

	out->shader = g.shader;
	Q_strncpyz( out->shaderName, g_shaders[g.shader].name.c_str(), sizeof(out->shaderName) );
	out->shaderScale = g_shaders[g.shader].scale;
	out->face = g.face;
	out->rank = g.rank;
	out->numTris = (int)g.base.size();

	for ( size_t i = 0; i < g.slots.size(); i++ )
		out->numSlots += g.slots[i] >= 0 ? 1 : 0;

	VectorCopy( g.mins, out->mins );
	VectorCopy( g.maxs, out->maxs );
	out->numEdges = g.numEdges;
	Com_Memcpy( out->edges, g.edges, sizeof(g.edges) );

	return qtrue;
}

int RTX_LightEmissive_Count( void )
{
	return RTX_LightEdit_IsReady() ? (int)g_shaders.size() : 0;
}

qboolean RTX_LightEmissive_Get( int shader, char *name, int nameSize, float *scale, int *numPolys )
{
	if ( !RTX_LightEdit_IsReady() || shader < 0 || shader >= (int)g_shaders.size() )
		return qfalse;

	if ( name && nameSize > 0 )
		Q_strncpyz( name, g_shaders[shader].name.c_str(), nameSize );

	if ( scale )
		*scale = g_shaders[shader].scale;

	if ( numPolys )
		*numPolys = g_shaders[shader].numPolys;

	return qtrue;
}

qboolean RTX_LightEmissive_Set( int shader, float scale )
{
	if ( !RTX_LightEdit_IsReady() || shader < 0 || shader >= (int)g_shaders.size() )
		return qfalse;

	scale = Com_Clamp( 0.0f, EMISSIVE_MAX_SCALE, scale );

	if ( g_shaders[shader].scale == scale )
		return qtrue;

	g_shaders[shader].scale = scale;
	ApplyScale( g_shaders[shader] );

	RTX_LightEdit_CountChange( 1 );
	RTX_LightEdit_BumpGeneration();

	return qtrue;
}

void RTX_LightEmissive_SetFromFile( const char *name, float scale )
{
	const int idx = FindByName( name );

	scale = Com_Clamp( 0.0f, EMISSIVE_MAX_SCALE, scale );

	if ( idx >= 0 )
	{
		g_shaders[idx].scale = scale;
		ApplyScale( g_shaders[idx] );
		return;
	}

	for ( size_t i = 0; i < g_kept.size(); i++ )
	{
		if ( !Q_stricmp( g_kept[i].first.c_str(), name ) )
		{
			g_kept[i].second = scale;
			return;
		}
	}

	Com_Printf( "light edit: emissive shader %s is not in this map, kept in the file\n", name );
	g_kept.push_back( std::make_pair( std::string( name ), scale ) );
}

void RTX_LightEmissive_ResetScales( void )
{
	g_generation++;
	FillOnes();

	for ( size_t i = 0; i < g_shaders.size(); i++ )
		g_shaders[i].scale = 1.0f;

	g_kept.clear();
}

void RTX_LightEmissive_GetForSave( rtxEmissiveScales_t &out )
{
	out.clear();

	for ( size_t i = 0; i < g_shaders.size(); i++ )
	{
		if ( g_shaders[i].scale != 1.0f )
			out.push_back( std::make_pair( g_shaders[i].name, g_shaders[i].scale ) );
	}

	for ( size_t i = 0; i < g_kept.size(); i++ )
		out.push_back( g_kept[i] );
}

int RTX_LightEmissive_NumChanged( void )
{
	int n = 0;

	for ( size_t i = 0; i < g_shaders.size(); i++ )
		n += g_shaders[i].scale != 1.0f ? 1 : 0;

	return n;
}

/*
=================
Console commands
=================
*/

// pt_ledit_emissive_shaders: lists the emissive shaders of the world.
void RTX_LightEdit_EmissiveShaders_f( void )
{
	const int count = RTX_LightEmissive_Count();

	for ( int i = 0; i < count; i++ )
	{
		char	name[MAX_QPATH];
		float	scale;
		int		polys;

		if ( RTX_LightEmissive_Get( i, name, sizeof(name), &scale, &polys ) )
			Com_Printf( "%4i: %-48s %5i polygons  scale %g\n", i, name, polys, scale );
	}

	Com_Printf( "%i emissive shaders\n", count );
}

// pt_ledit_emissive_scale <index|name> <scale>: sets the emission factor of a shader.
void RTX_LightEdit_EmissiveScale_f( void )
{
	if ( ri.Cmd_Argc() != 3 )
	{
		Com_Printf( "usage: pt_ledit_emissive_scale <index|name> <scale>\n" );
		return;
	}

	const char	*arg = ri.Cmd_Argv( 1 );
	int			shader = -1;

	if ( arg[0] >= '0' && arg[0] <= '9' )
		shader = atoi( arg );
	else if ( RTX_LightEdit_IsReady() )
		shader = FindByName( arg );

	if ( !RTX_LightEmissive_Set( shader, (float)atof( ri.Cmd_Argv( 2 ) ) ) )
		Com_Printf( "no emissive shader %s\n", arg );
}
