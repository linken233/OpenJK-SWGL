/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// The .lgt v2 file layer of the light edit mode.
//
// The file is a list of blocks. A block is a list of keys, and a key has one or three values.
// Blocks are lgt lights (reconstructed from the lightmaps), overrides of BSP `light`
// entities and added lights. A block of an unknown kind is kept and written back unchanged.
// A v1 binary skips the blocks it does not know: they carry a negative intensity.

#include "../tr_local.h"
#include "rtx_light_edit.h"
#include "rtx_light_file.h"
#include "rtx_light_emissive.h"
#include "rtx_light_sky.h"

#include <vector>
#include <string>
#include <algorithm>
#include <stdarg.h>
#include <math.h>

#define LFILE_LGT_RADIUS	4.0f	// as LIGHTGEN_EMITTER_RADIUS in vk_rtx_lightgen.cpp
#define LFILE_MIN_RADIUS	0.5f
#define LFILE_CHECK_RANGE	1.0f
#define LFILE_EPSILON		0.001f
#define LFILE_MAX_FACTOR	100.0f	// factor of an emissive surface, as the factor of a shader

struct fileKV_t {
	std::string					key;
	std::vector<std::string>	vals;
};

struct fileBlock_t {
	std::vector<fileKV_t>	kv;
};

struct fileData_t {
	int							version;
	std::vector<fileBlock_t>	lgt;		// in file order: the index is the lgt key
	std::vector<fileBlock_t>	ent;
	std::vector<fileBlock_t>	added;
	std::vector<fileBlock_t>	other;		// unknown source
	std::vector<fileBlock_t>	emissive;	// factor of an emissive shader
	std::vector<fileBlock_t>	surfaces;	// edits of one emissive surface
	std::vector<fileBlock_t>	global;		// sky and sun of the map
};

typedef struct {
	vec3_t	origin;
	vec3_t	color;
	float	intensity;
	float	radius;
	float	rays;
	float	error;
	int		disabled;
	int		edited;
	char	name[RTX_LIGHTEDIT_NAME_LEN];
} lgtValues_t;

static std::vector<fileBlock_t>	g_passLgt;		// lgt blocks of a map that has entity lights
static std::vector<fileBlock_t>	g_pendEnt;		// read, not applied yet
static std::vector<fileBlock_t>	g_pendAdded;
static std::vector<fileBlock_t>	g_pendEmissive;	// read, not applied yet
static std::vector<fileBlock_t>	g_pendSurfaces;	// read, not applied yet
static std::vector<fileBlock_t>	g_passOther;	// unknown blocks and overrides that do not match
static std::vector<int>			g_disabledLgt;	// ids of the lgt lights to switch off after the load
static qboolean					g_hasEntityLights = qfalse;
static qboolean					g_fileRead = qfalse;
static int						g_numLgt = 0;
static int						g_numOverrides = 0;
static int						g_numAdded = 0;
static int						g_numEmissive = 0;
static int						g_numSurfaces = 0;
static int						g_numIgnored = 0;

/*
=================
Block access
=================
*/

static const fileKV_t *FindKey( const fileBlock_t &b, const char *key )
{
	for ( size_t i = 0; i < b.kv.size(); i++ )
	{
		if ( !Q_stricmp( b.kv[i].key.c_str(), key ) )
			return &b.kv[i];
	}

	return NULL;
}

static float GetF( const fileBlock_t &b, const char *key, int index, float def )
{
	const fileKV_t *kv = FindKey( b, key );

	if ( !kv || index >= (int)kv->vals.size() )
		return def;

	return (float)atof( kv->vals[index].c_str() );
}

static qboolean GetV3( const fileBlock_t &b, const char *key, vec3_t out )
{
	const fileKV_t *kv = FindKey( b, key );

	if ( !kv || kv->vals.size() < 3 )
		return qfalse;

	for ( int k = 0; k < 3; k++ )
		out[k] = (float)atof( kv->vals[k].c_str() );

	return qtrue;
}

static const char *GetS( const fileBlock_t &b, const char *key, const char *def )
{
	const fileKV_t *kv = FindKey( b, key );

	return ( kv && !kv->vals.empty() ) ? kv->vals[0].c_str() : def;
}

/*
=================
Parsing
=================
*/

static void ClassifyBlock( const fileBlock_t &blk, fileData_t &out )
{
	const char *source = GetS( blk, "source", "lgt" );

	if ( !Q_stricmp( source, "lgt" ) )
		out.lgt.push_back( blk );
	else if ( !Q_stricmp( source, "entity" ) )
		out.ent.push_back( blk );
	else if ( !Q_stricmp( source, "added" ) )
		out.added.push_back( blk );
	else if ( !Q_stricmp( source, "emissive" ) )
		out.emissive.push_back( blk );
	else if ( !Q_stricmp( source, "emissive_surface" ) )
		out.surfaces.push_back( blk );
	else if ( !Q_stricmp( source, "global" ) )
		out.global.push_back( blk );
	else
		out.other.push_back( blk );
}

// Tokens outside a block are skipped, as the v1 parser does.
static void ParseFile( const char *text, fileData_t &out )
{
	const char *p = text;

	out.version = 1;

	COM_BeginParseSession( "RTX_LightFile_Parse" );

	while ( 1 )
	{
		const char *token = COM_ParseExt( &p, qtrue );

		if ( !token[0] )
			break;

		if ( token[0] != '{' )
		{
			if ( !Q_stricmp( token, "version" ) )
				out.version = atoi( COM_ParseExt( &p, qfalse ) );

			continue;
		}

		fileBlock_t blk;

		while ( 1 )
		{
			token = COM_ParseExt( &p, qtrue );

			if ( !token[0] || token[0] == '}' )
				break;

			fileKV_t kv;

			kv.key = token;

			while ( 1 )
			{
				token = COM_ParseExt( &p, qfalse );

				if ( !token[0] )
					break;

				kv.vals.push_back( token );
			}

			blk.kv.push_back( kv );
		}

		ClassifyBlock( blk, out );
	}

	COM_EndParseSession();
}

// Reads and parses the file of a map. Gives qfalse when the file is missing or empty.
static qboolean ReadFileData( const char *mapname, fileData_t &out )
{
	char	path[MAX_QPATH];
	void	*buffer = NULL;

	Com_sprintf( path, sizeof(path), "maps/%s.lgt", mapname );

	const long len = ri.FS_ReadFile( path, &buffer );

	if ( len <= 0 || !buffer )
	{
		if ( buffer )
			ri.FS_FreeFile( buffer );

		return qfalse;
	}

	ParseFile( (const char *)buffer, out );

	ri.FS_FreeFile( buffer );

	return qtrue;
}

static void DefaultName( const rtxLightRecord_t *rec, char *out, int size )
{
	Com_sprintf( out, size, "%s_%i", rec->source == RTX_LSRC_ENTITY ? "ent" : "lgt", rec->sourceKey );
}

static void ReadLgtValues( const fileBlock_t &b, int key, lgtValues_t &v )
{
	const float raw = GetF( b, "intensity", 0, 0.0f );
	const char	*name = GetS( b, "name", "" );

	VectorClear( v.origin );
	VectorSet( v.color, 1.0f, 1.0f, 1.0f );
	GetV3( b, "origin", v.origin );
	GetV3( b, "color", v.color );

	v.intensity = fabsf( raw );
	v.radius = GetF( b, "radius", 0, LFILE_LGT_RADIUS );

	if ( v.radius < LFILE_MIN_RADIUS )
		v.radius = LFILE_LGT_RADIUS;

	v.rays = GetF( b, "rays", 0, 0.0f );
	v.error = GetF( b, "error", 0, 0.0f );
	v.disabled = ( GetF( b, "disabled", 0, 0.0f ) != 0.0f || raw <= 0.0f ) ? 1 : 0;
	v.edited = GetF( b, "edited", 0, 0.0f ) != 0.0f ? 1 : 0;

	if ( name[0] )
		Q_strncpyz( v.name, name, sizeof(v.name) );
	else
		Com_sprintf( v.name, sizeof(v.name), "lgt_%i", key );
}

// Reads style, type, dir, the cones of a spot and the size of a rectangle. A key that is absent
// keeps the value of the record.
static void ReadSpot( const fileBlock_t &b, rtxLightRecord_t *rec )
{
	const fileKV_t	*type = FindKey( b, "type" );
	const int		oldType = rec->type;
	const qboolean	wasSpot = (qboolean)( oldType == RTX_LTYPE_SPOT );
	vec3_t			dir;
	float			outer, inner;

	if ( FindKey( b, "style" ) )
		rec->style = MAX( 0, MIN( (int)GetF( b, "style", 0, 0.0f ), RTX_LSTYLE_MAX - 1 ) );

	if ( type )
	{
		const char *name = type->vals.empty() ? "" : type->vals[0].c_str();

		rec->type = !Q_stricmp( name, "spot" ) ? RTX_LTYPE_SPOT : !Q_stricmp( name, "rect" ) ? RTX_LTYPE_RECT : RTX_LTYPE_SPHERE;
	}

	if ( rec->type == RTX_LTYPE_RECT )
	{
		if ( oldType == RTX_LTYPE_RECT || wasSpot )
			VectorCopy( rec->spot + 2, dir );
		else
			VectorSet( dir, 0.0f, 0.0f, -1.0f );

		GetV3( b, "dir", dir );

		RTX_LightEdit_SetRectData( rec, dir, GetF( b, "width", 0, rec->width ), GetF( b, "height", 0, rec->height ),
			GetF( b, "roll", 0, rec->roll ), GetF( b, "two_sided", 0, (float)rec->twoSided ) != 0.0f );
		return;
	}

	if ( rec->type != RTX_LTYPE_SPOT )
		return;

	if ( wasSpot )
		RTX_LightEdit_GetSpotData( rec, dir, &outer, &inner );
	else
	{
		VectorSet( dir, 0.0f, 0.0f, -1.0f );
		outer = 35.0f;
		inner = 25.0f;
	}

	GetV3( b, "dir", dir );
	outer = GetF( b, "cone_outer", 0, outer );
	inner = GetF( b, "cone_inner", 0, inner );

	RTX_LightEdit_SetSpotData( rec, dir, outer, inner );
}

// Makes the current type, spot or rectangle values and style the original ones.
static void SetOriginalSpot( rtxLightRecord_t *rec )
{
	rec->origType = rec->type;
	Com_Memcpy( rec->origSpot, rec->spot, sizeof(rec->origSpot) );
	rec->origWidth = rec->width;
	rec->origHeight = rec->height;
	rec->origRoll = rec->roll;
	rec->origTwoSided = rec->twoSided;
	rec->origStyle = rec->style;
}

// Gives the record the values of a sphere without a style.
static void ResetShape( rtxLightRecord_t *rec )
{
	rec->type = RTX_LTYPE_SPHERE;
	rec->style = 0;
	rec->width = rec->height = RTX_LRECT_DEFAULT_SIZE;
	rec->roll = 0.0f;
	rec->twoSided = 0;
	VectorSet( rec->spot + 2, 0.0f, 0.0f, -1.0f );
}

/*
=================
Load
=================
*/

static light_poly_t *AppendLightPoly( world_t &w )
{
	if ( w.num_light_polys == w.allocated_light_polys )
	{
		w.allocated_light_polys = MAX( w.allocated_light_polys * 2, 128 );
		w.light_polys = (light_poly_t *)realloc( w.light_polys, w.allocated_light_polys * sizeof(light_poly_t) );
	}

	return w.light_polys + w.num_light_polys++;
}

// Makes the record and the light of one lgt block.
static void LoadLgtBlock( world_t &w, const fileBlock_t &b, int key )
{
	lgtValues_t v;

	ReadLgtValues( b, key, v );

	const int id = RTX_LightEdit_RegisterLoaded( RTX_LSRC_LGT, key, v.origin, v.color, v.intensity, -1, v.rays, v.error );
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecordRaw( id );

	rec->radius = rec->origRadius = v.radius;
	Q_strncpyz( rec->name, v.name, sizeof(rec->name) );
	Q_strncpyz( rec->origName, v.name, sizeof(rec->origName) );
	rec->edited = v.edited;

	ReadSpot( b, rec );
	SetOriginalSpot( rec );

	light_poly_t	tmp[RTX_LIGHT_MAX_SLOTS];
	const int		cluster = RTX_LightEdit_Convert( rec, tmp );
	const int		numSlots = RTX_LightEdit_SlotCount( rec );

	if ( cluster < 0 || w.num_light_polys + numSlots > RTX_LEDIT_MAX_SLOTS )
	{
		rec->flags |= RTX_LFLAG_IN_SOLID;
	}
	else
	{
		rec->flags &= ~RTX_LFLAG_IN_SOLID;

		for ( int i = 0; i < numSlots; i++ )
		{
			*RTX_LightEdit_SlotPtr( rec, i ) = w.num_light_polys;
			*AppendLightPoly( w ) = tmp[i];
		}
	}

	// A disabled light keeps its slot until the load ends, so FinalizeLoad reads its class.
	if ( v.disabled )
	{
		rec->flags |= RTX_LFLAG_DISABLED;

		if ( rec->lightIndex >= 0 )
			g_disabledLgt.push_back( id );
	}
}

// Gives the sky of the first global block, or the GLOBAL mode when there is none.
static void ApplyGlobalBlocks( const std::vector<fileBlock_t> &blocks )
{
	rtxSkyDesc_t	d;
	const char		*mode;

	RTX_LightSky_Defaults( &d, RTX_SKY_GLOBAL );

	if ( !blocks.empty() )
	{
		const fileBlock_t &b = blocks[0];

		mode = GetS( b, "sky", "" );
		d.mode = !Q_stricmp( mode, "skybox" ) ? RTX_SKY_SKYBOX : !Q_stricmp( mode, "physical" ) ? RTX_SKY_PHYSICAL
			: !Q_stricmp( mode, "hybrid" ) ? RTX_SKY_HYBRID : RTX_SKY_GLOBAL;

		d.sunAzimuth = GetF( b, "sun_azimuth", 0, d.sunAzimuth );
		d.sunElevation = GetF( b, "sun_elevation", 0, d.sunElevation );
		GetV3( b, "sun_color", d.sunColor );
		d.sunBrightness = GetF( b, "sun_brightness", 0, d.sunBrightness );
		d.sunAngle = GetF( b, "sun_angle", 0, d.sunAngle );
		d.skyDiffuse = GetF( b, "sky_diffuse", 0, d.skyDiffuse );
		GetV3( b, "ambient_color", d.ambientColor );
		d.ambientIntensity = GetF( b, "ambient_intensity", 0, d.ambientIntensity );

		if ( blocks.size() > 1 )
			Com_Printf( "light edit: %i global blocks, only the first one is used\n", (int)blocks.size() );
	}

	RTX_LightSky_SetFromFile( &d );
}

static void ClearState( void )
{
	g_passLgt.clear();
	g_pendEnt.clear();
	g_pendAdded.clear();
	g_pendEmissive.clear();
	g_pendSurfaces.clear();
	g_passOther.clear();
	g_disabledLgt.clear();
	g_hasEntityLights = qfalse;
	g_fileRead = qfalse;
	g_numLgt = 0;
	g_numOverrides = 0;
	g_numAdded = 0;
	g_numEmissive = 0;
	g_numSurfaces = 0;
	g_numIgnored = 0;
}

void RTX_LightFile_Load( world_t &w, qboolean hasEntityLights )
{
	fileData_t data;

	ClearState();

	g_hasEntityLights = hasEntityLights;

	if ( !ReadFileData( w.baseName, data ) )
		return;

	g_fileRead = qtrue;

	if ( data.version > 2 )
		Com_Printf( "light edit: maps/%s.lgt is version %i, read as version 2\n", w.baseName, data.version );

	// A map with entity lights ignores the lgt lights. The blocks stay in the file.
	if ( hasEntityLights )
	{
		g_passLgt = data.lgt;
		g_numIgnored += (int)data.lgt.size();
	}
	else
	{
		for ( size_t k = 0; k < data.lgt.size(); k++ )
			LoadLgtBlock( w, data.lgt[k], (int)k );

		g_numLgt = (int)data.lgt.size();
	}

	g_pendEnt = data.ent;
	g_pendAdded = data.added;
	g_pendEmissive = data.emissive;
	g_pendSurfaces = data.surfaces;
	g_passOther = data.other;
	g_numIgnored += (int)data.other.size();

	ApplyGlobalBlocks( data.global );
}

/*
=================
Overrides and added lights
=================
*/

static rtxLightRecord_t *FindEntityRecord( int ent )
{
	const int count = RTX_LightEdit_NumRecords();

	for ( int id = 0; id < count; id++ )
	{
		rtxLightRecord_t *rec = RTX_LightEdit_GetRecordRaw( id );

		if ( rec->source == RTX_LSRC_ENTITY && rec->sourceKey == ent )
			return rec;
	}

	return NULL;
}

static void SetName( rtxLightRecord_t *rec, const fileBlock_t &b )
{
	const char *name = GetS( b, "name", "" );

	if ( name[0] )
		Q_strncpyz( rec->name, name, sizeof(rec->name) );
}

// Gives qfalse when the block matches no entity light of the BSP.
static qboolean ApplyEntityBlock( const fileBlock_t &b )
{
	const int			ent = (int)GetF( b, "ent", 0, -1.0f );
	rtxLightRecord_t	*rec = FindEntityRecord( ent );
	vec3_t				v;

	if ( rec && GetV3( b, "check", v ) )
	{
		for ( int k = 0; k < 3; k++ )
		{
			if ( fabsf( v[k] - rec->origOrigin[k] ) > LFILE_CHECK_RANGE )
				rec = NULL;

			if ( !rec )
				break;
		}
	}

	if ( !rec )
	{
		Com_Printf( "light edit: entity override %i does not match the BSP, ignored\n", ent );
		return qfalse;
	}

	if ( GetV3( b, "origin", v ) )
		VectorCopy( v, rec->origin );

	if ( GetV3( b, "color", v ) )
		VectorCopy( v, rec->color );

	rec->intensity = MAX( GetF( b, "light", 0, rec->intensity ), 0.0f );

	const float radius = GetF( b, "radius", 0, 0.0f );

	if ( radius > 0.0f )
		rec->radius = MAX( radius, LFILE_MIN_RADIUS );

	if ( GetF( b, "disabled", 0, 0.0f ) != 0.0f )
		rec->flags |= RTX_LFLAG_DISABLED;

	ReadSpot( b, rec );
	SetName( rec, b );
	RTX_LightEdit_ApplyRecord( rec );

	return qtrue;
}

static void ApplyAddedBlock( const fileBlock_t &b )
{
	const int			id = RTX_LightEdit_NewRecord( RTX_LSRC_ADDED, -1 );
	rtxLightRecord_t	*rec = RTX_LightEdit_GetRecordRaw( id );
	vec3_t				v;

	if ( GetV3( b, "origin", v ) )
		VectorCopy( v, rec->origin );

	if ( GetV3( b, "color", v ) )
		VectorCopy( v, rec->color );

	rec->intensity = MAX( GetF( b, "intensity", 0, 300.0f ), 0.0f );

	const float radius = GetF( b, "radius", 0, 0.0f );

	if ( radius > 0.0f )
		rec->radius = MAX( radius, LFILE_MIN_RADIUS );

	SetName( rec, b );
	ReadSpot( b, rec );
	SetOriginalSpot( rec );

	VectorCopy( rec->origin, rec->origOrigin );
	VectorCopy( rec->color, rec->origColor );
	rec->origIntensity = rec->intensity;
	rec->origRadius = rec->radius;
	Q_strncpyz( rec->origName, rec->name, sizeof(rec->origName) );

	RTX_LightEdit_ApplyRecord( rec );
}

// Finds the emissive surface by shader and rank, checks its centre, and gives it the edits of the block.
// Gives qfalse when the block matches no surface of the BSP.
static qboolean ApplySurfaceBlock( const fileBlock_t &b )
{
	const char			*shader = GetS( b, "shader", "" );
	const int			rank = (int)GetF( b, "face", 0, -1.0f );
	const int			id = shader[0] ? RTX_LightEmissive_FindRecord( shader, rank ) : -1;
	rtxLightRecord_t	*rec = id >= 0 ? RTX_LightEdit_GetRecordRaw( id ) : NULL;
	vec3_t				v;

	if ( rec && GetV3( b, "check", v ) )
	{
		const char	*key;
		int			groupRank;
		float		centre[3];

		RTX_LightEmissive_GroupKey( rec->emiGroup, &key, &groupRank, centre );

		for ( int k = 0; k < 3; k++ )
		{
			if ( fabsf( v[k] - centre[k] ) > LFILE_CHECK_RANGE )
				rec = NULL;

			if ( !rec )
				break;
		}
	}

	if ( !rec )
	{
		Com_Printf( "light edit: emissive surface %s face %i does not match the BSP, ignored\n", shader, rank );
		return qfalse;
	}

	rec->intensity = MAX( 0.0f, MIN( GetF( b, "scale", 0, 1.0f ), LFILE_MAX_FACTOR ) );

	if ( GetV3( b, "color", v ) )
	{
		for ( int k = 0; k < 3; k++ )
			rec->color[k] = MAX( 0.0f, MIN( v[k], 1.0f ) );
	}

	rec->style = MAX( 0, MIN( (int)GetF( b, "style", 0, 0.0f ), RTX_LSTYLE_MAX - 1 ) );

	if ( GetF( b, "disabled", 0, 0.0f ) != 0.0f )
		rec->flags |= RTX_LFLAG_DISABLED;

	RTX_LightEdit_ApplyRecord( rec );

	return qtrue;
}

// Applies the pending blocks and empties the lists. Gives the number of lights it changed.
static int ApplyPending( void )
{
	int changed = 0;

	for ( size_t i = 0; i < g_pendEnt.size(); i++ )
	{
		if ( ApplyEntityBlock( g_pendEnt[i] ) )
		{
			g_numOverrides++;
			changed++;
		}
		else
		{
			g_passOther.push_back( g_pendEnt[i] );
			g_numIgnored++;
		}
	}

	for ( size_t i = 0; i < g_pendAdded.size(); i++ )
	{
		ApplyAddedBlock( g_pendAdded[i] );
		g_numAdded++;
		changed++;
	}

	for ( size_t i = 0; i < g_pendEmissive.size(); i++ )
	{
		const char *shader = GetS( g_pendEmissive[i], "shader", "" );

		if ( shader[0] )
		{
			RTX_LightEmissive_SetFromFile( shader, GetF( g_pendEmissive[i], "scale", 0, 1.0f ) );
			g_numEmissive++;
		}
		else
			g_numIgnored++;
	}

	for ( size_t i = 0; i < g_pendSurfaces.size(); i++ )
	{
		if ( ApplySurfaceBlock( g_pendSurfaces[i] ) )
		{
			g_numSurfaces++;
			changed++;
		}
		else
		{
			g_passOther.push_back( g_pendSurfaces[i] );
			g_numIgnored++;
		}
	}

	g_pendEnt.clear();
	g_pendAdded.clear();
	g_pendEmissive.clear();
	g_pendSurfaces.clear();

	return changed;
}

static void PrintSummary( const char *mapname )
{
	Com_Printf( "light edit: maps/%s.lgt: %i lgt, %i overrides, %i added, %i emissive, %i emissive surfaces, %i ignored\n",
		mapname, g_numLgt, g_numOverrides, g_numAdded, g_numEmissive, g_numSurfaces, g_numIgnored );
}

void RTX_LightFile_Apply( world_t &w )
{
	if ( !g_fileRead )
		return;

	int changed = 0;

	RTX_LightEdit_SetLoading( qtrue );

	for ( size_t i = 0; i < g_disabledLgt.size(); i++ )
	{
		RTX_LightEdit_ApplyRecord( RTX_LightEdit_GetRecordRaw( g_disabledLgt[i] ) );
		changed++;
	}

	g_disabledLgt.clear();

	changed += ApplyPending();

	RTX_LightEdit_SetLoading( qfalse );

	// collect_cluster_lights ran before the overlay.
	if ( changed )
		vk_rtx_rebuild_cluster_lights( w, qtrue );

	RTX_LightEdit_CountChange( 0 );

	PrintSummary( w.baseName );
}

/*
=================
Save
=================
*/

static void Appendf( std::string &s, const char *fmt, ... )
{
	char		buf[512];
	va_list		argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( buf, sizeof(buf), fmt, argptr );
	va_end( argptr );

	s += buf;
}

// A number with at most four decimals.
static std::string Num( float v )
{
	char buf[48];

	Com_sprintf( buf, sizeof(buf), "%.4f", v );

	std::string s = buf;

	if ( s.find( '.' ) != std::string::npos )
	{
		while ( !s.empty() && s[s.size() - 1] == '0' )
			s.erase( s.size() - 1 );

		if ( !s.empty() && s[s.size() - 1] == '.' )
			s.erase( s.size() - 1 );
	}

	if ( s == "-0" || s.empty() )
		s = "0";

	return s;
}

static std::string Quote( const std::string &v )
{
	qboolean plain = (qboolean)( !v.empty() );

	for ( size_t i = 0; i < v.size(); i++ )
	{
		const char c = v[i];

		if ( c <= ' ' || c == '"' || c == '{' || c == '}' || c == '/' )
			plain = qfalse;
	}

	if ( plain )
		return v;

	std::string out = "\"";

	for ( size_t i = 0; i < v.size(); i++ )
		out += v[i] == '"' ? '\'' : v[i];

	return out + "\"";
}

static void PutVec( std::string &s, const char *key, const vec3_t v )
{
	s += "\t";
	s += key;
	s += " " + Num( v[0] ) + " " + Num( v[1] ) + " " + Num( v[2] ) + "\n";
}

static void PutNum( std::string &s, const char *key, float v )
{
	s += "\t";
	s += key;
	s += " " + Num( v ) + "\n";
}

static void PutName( std::string &s, const char *name )
{
	if ( name[0] )
	{
		s += "\tname ";
		s += Quote( name );
		s += "\n";
	}
}

// Writes the style when it differs from `base`.
static void PutStyle( std::string &s, const rtxLightRecord_t *rec, int base )
{
	if ( rec->style != base )
		Appendf( s, "\tstyle %i\n", rec->style );
}

// Writes type, dir and the cones of a spot, or the values of a rectangle. `explicitSphere`
// writes `type sphere` for a sphere.
static void PutSpot( std::string &s, const rtxLightRecord_t *rec, qboolean explicitSphere )
{
	if ( rec->type == RTX_LTYPE_RECT )
	{
		vec3_t	dir;
		float	outer, inner;

		RTX_LightEdit_GetSpotData( rec, dir, &outer, &inner );

		s += "\ttype rect\n";
		PutVec( s, "dir", dir );
		PutNum( s, "width", rec->width );
		PutNum( s, "height", rec->height );
		PutNum( s, "roll", rec->roll );
		Appendf( s, "\ttwo_sided %i\n", rec->twoSided ? 1 : 0 );
		return;
	}

	if ( rec->type != RTX_LTYPE_SPOT )
	{
		if ( explicitSphere )
			s += "\ttype sphere\n";

		return;
	}

	vec3_t	dir;
	float	outer, inner;

	RTX_LightEdit_GetSpotData( rec, dir, &outer, &inner );

	s += "\ttype spot\n";
	PutVec( s, "dir", dir );
	PutNum( s, "cone_outer", outer );
	PutNum( s, "cone_inner", inner );
}

static void WriteRawBlock( std::string &s, const fileBlock_t &b )
{
	s += "{\n";

	for ( size_t i = 0; i < b.kv.size(); i++ )
	{
		s += "\t" + b.kv[i].key;

		for ( size_t k = 0; k < b.kv[i].vals.size(); k++ )
			s += " " + Quote( b.kv[i].vals[k] );

		s += "\n";
	}

	s += "}\n";
}

static qboolean NameIsDefault( const rtxLightRecord_t *rec )
{
	char def[RTX_LIGHTEDIT_NAME_LEN];

	DefaultName( rec, def, sizeof(def) );

	return (qboolean)( !strcmp( rec->name, def ) );
}

static qboolean LgtNeedsEditedForm( const rtxLightRecord_t *rec )
{
	return (qboolean)( rec->edited || RTX_LightEdit_IsModified( rec ) || ( rec->flags & RTX_LFLAG_DISABLED )
		|| !NameIsDefault( rec ) || fabsf( rec->radius - LFILE_LGT_RADIUS ) > LFILE_EPSILON );
}

static void WriteLgtRecord( std::string &s, const rtxLightRecord_t *rec )
{
	const qboolean edited = LgtNeedsEditedForm( rec );
	const qboolean disabled = (qboolean)( ( rec->flags & RTX_LFLAG_DISABLED ) != 0 );

	s += "{\n";

	if ( edited )
		s += "\tsource lgt\n\tedited 1\n";

	PutVec( s, "origin", rec->origin );
	PutVec( s, "color", rec->color );
	PutNum( s, "intensity", disabled ? -rec->intensity : rec->intensity );

	if ( edited )
	{
		if ( fabsf( rec->radius - LFILE_LGT_RADIUS ) > LFILE_EPSILON )
			PutNum( s, "radius", rec->radius );

		if ( disabled )
			s += "\tdisabled 1\n";

		if ( !NameIsDefault( rec ) )
			PutName( s, rec->name );

		PutSpot( s, rec, qfalse );
		PutStyle( s, rec, 0 );
	}

	PutNum( s, "rays", rec->rays );
	PutNum( s, "error", rec->error );

	s += "}\n";
}

static void WriteEntityRecord( std::string &s, const rtxLightRecord_t *rec )
{
	s += "{\n\tsource entity\n";
	Appendf( s, "\tent %i\n", rec->sourceKey );

	PutVec( s, "check", rec->origOrigin );
	PutVec( s, "origin", rec->origin );
	PutVec( s, "color", rec->color );
	PutNum( s, "light", rec->intensity );
	PutNum( s, "radius", rec->radius );

	if ( rec->flags & RTX_LFLAG_DISABLED )
		s += "\tdisabled 1\n";

	if ( !NameIsDefault( rec ) )
		PutName( s, rec->name );

	PutSpot( s, rec, (qboolean)( rec->origType == RTX_LTYPE_SPOT ) );
	PutStyle( s, rec, rec->origStyle );

	s += "\tintensity -1\n}\n";
}

static void WriteAddedRecord( std::string &s, const rtxLightRecord_t *rec )
{
	s += "{\n\tsource added\n";

	PutVec( s, "origin", rec->origin );
	PutVec( s, "color", rec->color );
	PutNum( s, "intensity", rec->intensity );
	PutNum( s, "radius", rec->radius );
	PutName( s, rec->name );
	PutSpot( s, rec, qfalse );
	PutStyle( s, rec, 0 );

	s += "}\n";
}

// The global block: one key per line, one or three values, no origin, color or intensity key.
// The mode GLOBAL (a block with an ambient floor only) writes no sun key. A zero sky_diffuse
// and a zero ambient_intensity are not written.
void RTX_LightFile_FormatSkyBlock( std::string &s, const rtxSkyDesc_t &d )
{
	const char *mode = d.mode == RTX_SKY_SKYBOX ? "skybox" : d.mode == RTX_SKY_PHYSICAL ? "physical"
		: d.mode == RTX_SKY_HYBRID ? "hybrid" : "global";

	s += "global\n{\n\tsource global\n";
	Appendf( s, "\tsky %s\n", mode );

	if ( d.mode != RTX_SKY_GLOBAL )
	{
		PutNum( s, "sun_azimuth", d.sunAzimuth );
		PutNum( s, "sun_elevation", d.sunElevation );
		PutVec( s, "sun_color", d.sunColor );
		PutNum( s, "sun_brightness", d.sunBrightness );
		PutNum( s, "sun_angle", d.sunAngle );

		if ( d.skyDiffuse > 0.0f )
			PutNum( s, "sky_diffuse", d.skyDiffuse );
	}

	if ( d.ambientIntensity > 0.0f )
	{
		PutVec( s, "ambient_color", d.ambientColor );
		PutNum( s, "ambient_intensity", d.ambientIntensity );
	}

	s += "}\n";
}

static void WriteEmissiveBlock( std::string &s, const std::string &shader, float scale )
{
	s += "{\n\tsource emissive\n\tshader " + Quote( shader ) + "\n";
	PutNum( s, "scale", scale );
	s += "}\n";
}

// An emissive surface: no origin and no intensity key, so that a v1 parser skips the block.
static void WriteSurfaceRecord( std::string &s, const rtxLightRecord_t *rec )
{
	const char	*shader;
	int			rank;
	float		check[3];

	RTX_LightEmissive_GroupKey( rec->emiGroup, &shader, &rank, check );

	s += "{\n\tsource emissive_surface\n\tshader " + Quote( shader ) + "\n";
	Appendf( s, "\tface %i\n", rank );
	PutVec( s, "check", check );

	if ( fabsf( rec->intensity - 1.0f ) > LFILE_EPSILON )
		PutNum( s, "scale", rec->intensity );

	if ( fabsf( rec->color[0] - 1.0f ) > LFILE_EPSILON || fabsf( rec->color[1] - 1.0f ) > LFILE_EPSILON
		|| fabsf( rec->color[2] - 1.0f ) > LFILE_EPSILON )
		PutVec( s, "color", rec->color );

	PutStyle( s, rec, 0 );

	if ( rec->flags & RTX_LFLAG_DISABLED )
		s += "\tdisabled 1\n";

	s += "}\n";
}

static qboolean SurfaceNeedsBlock( const rtxLightRecord_t *rec )
{
	return (qboolean)( RTX_LightEdit_IsModified( rec ) || ( rec->flags & RTX_LFLAG_DISABLED ) );
}

static qboolean EntityNeedsBlock( const rtxLightRecord_t *rec )
{
	return (qboolean)( RTX_LightEdit_IsModified( rec ) || ( rec->flags & RTX_LFLAG_DISABLED ) || !NameIsDefault( rec ) );
}

static qboolean WriteBytes( const char *path, const void *data, int len )
{
	fileHandle_t f = ri.FS_FOpenFileWrite( path, qtrue );

	if ( !f )
		return qfalse;

	ri.FS_Write( data, len, f );
	ri.FS_FCloseFile( f );

	return qtrue;
}

// Copies maps/<map>.lgt to maps/<map>.lgt.bak when the file exists. Gives qfalse on a write error.
static qboolean BackupFile( const char *mapname )
{
	char	path[MAX_QPATH];
	char	bak[MAX_QPATH];
	void	*old = NULL;

	Com_sprintf( path, sizeof(path), "maps/%s.lgt", mapname );
	Com_sprintf( bak, sizeof(bak), "maps/%s.lgt.bak", mapname );

	const long len = ri.FS_ReadFile( path, &old );
	qboolean ok = qtrue;

	if ( len > 0 && old )
		ok = WriteBytes( bak, old, (int)len );

	if ( old )
		ri.FS_FreeFile( old );

	return ok;
}

qboolean RTX_LightFile_Save( void )
{
	if ( !RTX_LightEdit_IsReady() )
	{
		RTX_LightEdit_SetError( "RTX light edit is not available" );
		return qfalse;
	}

	const char	*mapname = tr.world->baseName;
	char		path[MAX_QPATH];
	const int	count = RTX_LightEdit_NumRecords();

	std::vector<const rtxLightRecord_t *>	lgt, ent, added, surfaces;

	for ( int id = 0; id < count; id++ )
	{
		const rtxLightRecord_t *rec = RTX_LightEdit_GetRecordRaw( id );

		if ( rec->source == RTX_LSRC_LGT )
			lgt.push_back( rec );
		else if ( rec->source == RTX_LSRC_ENTITY )
		{
			if ( EntityNeedsBlock( rec ) )
				ent.push_back( rec );
		}
		else if ( rec->source == RTX_LSRC_EMISSIVE )
		{
			if ( SurfaceNeedsBlock( rec ) )
				surfaces.push_back( rec );
		}
		else if ( !( rec->flags & RTX_LFLAG_DELETED ) )
			added.push_back( rec );
	}

	std::sort( lgt.begin(), lgt.end(), []( const rtxLightRecord_t *a, const rtxLightRecord_t *b ) {
		return a->sourceKey < b->sourceKey;
	} );

	std::string text;

	Appendf( text, "version 2\n// light edit file of %s; lgt blocks, entity overrides, added lights, emissive surfaces\n", mapname );
	Appendf( text, "lights %i\n", (int)( lgt.size() + g_passLgt.size() ) );

	for ( size_t i = 0; i < lgt.size(); i++ )
		WriteLgtRecord( text, lgt[i] );

	for ( size_t i = 0; i < g_passLgt.size(); i++ )
		WriteRawBlock( text, g_passLgt[i] );

	for ( size_t i = 0; i < ent.size(); i++ )
		WriteEntityRecord( text, ent[i] );

	for ( size_t i = 0; i < added.size(); i++ )
		WriteAddedRecord( text, added[i] );

	rtxEmissiveScales_t	scales;
	rtxSkyDesc_t		sky;

	RTX_LightEmissive_GetForSave( scales );

	for ( size_t i = 0; i < scales.size(); i++ )
		WriteEmissiveBlock( text, scales[i].first, scales[i].second );

	for ( size_t i = 0; i < surfaces.size(); i++ )
		WriteSurfaceRecord( text, surfaces[i] );

	if ( RTX_LightSky_GetForSave( &sky ) )
		RTX_LightFile_FormatSkyBlock( text, sky );

	for ( size_t i = 0; i < g_passOther.size(); i++ )
		WriteRawBlock( text, g_passOther[i] );

	if ( !BackupFile( mapname ) )
	{
		RTX_LightEdit_SetError( "cannot write the backup of maps/%s.lgt", mapname );
		return qfalse;
	}

	Com_sprintf( path, sizeof(path), "maps/%s.lgt", mapname );

	if ( !WriteBytes( path, text.c_str(), (int)text.size() ) )
	{
		RTX_LightEdit_SetError( "cannot write %s", path );
		return qfalse;
	}

	// The file is now the source of the lgt lights.
	for ( int id = 0; id < count; id++ )
	{
		rtxLightRecord_t *rec = RTX_LightEdit_GetRecordRaw( id );

		if ( rec->source != RTX_LSRC_LGT )
			continue;

		if ( LgtNeedsEditedForm( rec ) )
			rec->edited = 1;

		VectorCopy( rec->origin, rec->origOrigin );
		VectorCopy( rec->color, rec->origColor );
		rec->origIntensity = rec->intensity;
		rec->origRadius = rec->radius;
		Q_strncpyz( rec->origName, rec->name, sizeof(rec->origName) );
		SetOriginalSpot( rec );
	}

	RTX_LightSky_Saved();
	RTX_LightEdit_CountChange( 0 );

	Com_Printf( "light edit: wrote %s (%i lgt, %i overrides, %i added, %i emissive, %i emissive surfaces, sky %s)\n", path,
		(int)( lgt.size() + g_passLgt.size() ), (int)ent.size(), (int)added.size(), (int)scales.size(), (int)surfaces.size(),
		RTX_LightSky_GetForSave( &sky ) ? "set" : "global" );

	return qtrue;
}

/*
=================
Reload
=================
*/

// Gives the record its original values back, enabled. An added light is deleted.
static void ResetRecord( rtxLightRecord_t *rec )
{
	rec->flags &= ~RTX_LFLAG_MUTED;

	if ( rec->source == RTX_LSRC_ADDED )
	{
		rec->flags |= RTX_LFLAG_DELETED;
		return;
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
	rec->flags &= ~( RTX_LFLAG_DISABLED | RTX_LFLAG_COMPACTED );
	rec->edited = 0;
}

// Sets the current and the original values of an lgt record from its block.
static void SetLgtFromBlock( rtxLightRecord_t *rec, const fileBlock_t &b )
{
	lgtValues_t v;

	ReadLgtValues( b, rec->sourceKey, v );

	VectorCopy( v.origin, rec->origin );
	VectorCopy( v.origin, rec->origOrigin );
	VectorCopy( v.color, rec->color );
	VectorCopy( v.color, rec->origColor );
	rec->intensity = rec->origIntensity = v.intensity;
	rec->radius = rec->origRadius = v.radius;
	rec->rays = v.rays;
	rec->error = v.error;
	rec->edited = v.edited;
	Q_strncpyz( rec->name, v.name, sizeof(rec->name) );
	Q_strncpyz( rec->origName, v.name, sizeof(rec->origName) );

	ResetShape( rec );
	ReadSpot( b, rec );
	SetOriginalSpot( rec );

	if ( v.disabled )
		rec->flags |= RTX_LFLAG_DISABLED;
}

qboolean RTX_LightFile_Reload( void )
{
	if ( !RTX_LightEdit_IsReady() )
	{
		RTX_LightEdit_SetError( "RTX light edit is not available" );
		return qfalse;
	}

	world_t		&w = *tr.world;
	fileData_t	data;
	const int	count = RTX_LightEdit_NumRecords();

	data.version = 1;

	if ( !ReadFileData( w.baseName, data ) )
		Com_Printf( "light edit: maps/%s.lgt not found, the lights go back to the BSP\n", w.baseName );

	g_passLgt.clear();
	g_pendEnt.clear();
	g_pendAdded.clear();
	g_pendEmissive.clear();
	g_pendSurfaces.clear();
	g_passOther.clear();
	g_disabledLgt.clear();
	g_fileRead = qtrue;
	g_numLgt = 0;
	g_numOverrides = 0;
	g_numAdded = 0;
	g_numEmissive = 0;
	g_numSurfaces = 0;
	g_numIgnored = 0;

	RTX_LightEmissive_ResetScales();
	ApplyGlobalBlocks( data.global );
	RTX_LightSky_MapLoaded();

	// Records by lgt key, in the file order of the load.
	std::vector<int> byKey;

	for ( int id = 0; id < count; id++ )
	{
		rtxLightRecord_t *rec = RTX_LightEdit_GetRecordRaw( id );

		ResetRecord( rec );

		if ( rec->source != RTX_LSRC_LGT )
			continue;

		if ( rec->sourceKey >= (int)byKey.size() )
			byKey.resize( rec->sourceKey + 1, -1 );

		byKey[rec->sourceKey] = id;
	}

	if ( g_hasEntityLights )
	{
		g_passLgt = data.lgt;
		g_numIgnored += (int)data.lgt.size();
	}
	else
	{
		for ( size_t k = 0; k < data.lgt.size(); k++ )
		{
			if ( k >= byKey.size() || byKey[k] < 0 )
			{
				Com_Printf( "light edit: lgt block %i has no light in the map, ignored\n", (int)k );
				g_numIgnored++;
				continue;
			}

			SetLgtFromBlock( RTX_LightEdit_GetRecordRaw( byKey[k] ), data.lgt[k] );
			g_numLgt++;
		}
	}

	g_pendEnt = data.ent;
	g_pendAdded = data.added;
	g_pendEmissive = data.emissive;
	g_pendSurfaces = data.surfaces;
	g_passOther = data.other;
	g_numIgnored += (int)data.other.size();

	ApplyPending();

	// Every record goes through its slot once. New records are done already.
	const int total = RTX_LightEdit_NumRecords();

	for ( int id = 0; id < total; id++ )
		RTX_LightEdit_ApplyRecord( RTX_LightEdit_GetRecordRaw( id ) );

	RTX_LightEdit_RebuildClusters();
	RTX_LightEdit_CountChange( 0 );

	PrintSummary( w.baseName );

	return qtrue;
}

/*
=================
pt_lightgen guard
=================
*/

qboolean RTX_LightFile_CheckRegenerate( const char *mapname, qboolean force )
{
	fileData_t data;

	if ( !ReadFileData( mapname, data ) )
		return qtrue;

	qboolean edits = (qboolean)( !data.emissive.empty() || !data.surfaces.empty() || !data.global.empty()
		|| ( data.version >= 2 && ( !data.ent.empty() || !data.added.empty() ) ) );

	for ( size_t i = 0; i < data.lgt.size() && !edits; i++ )
	{
		if ( data.version >= 2 && ( FindKey( data.lgt[i], "edited" ) || FindKey( data.lgt[i], "disabled" ) ) )
			edits = qtrue;
	}

	if ( !edits )
		return qtrue;

	if ( !force )
	{
		Com_Printf( "pt_lightgen: maps/%s.lgt holds light edits; use pt_lightgen %s force\n", mapname, mapname );
		return qfalse;
	}

	if ( !BackupFile( mapname ) )
	{
		Com_Printf( "pt_lightgen: cannot write the backup of maps/%s.lgt\n", mapname );
		return qfalse;
	}

	Com_Printf( "pt_lightgen: copied maps/%s.lgt to maps/%s.lgt.bak\n", mapname, mapname );

	return qtrue;
}

// The emissive and global blocks of the file, for the file that pt_lightgen writes.
std::string RTX_LightFile_KeptBlocks( const char *mapname )
{
	fileData_t	data;
	std::string	text;

	if ( !ReadFileData( mapname, data ) )
		return text;

	for ( size_t i = 0; i < data.emissive.size(); i++ )
		WriteRawBlock( text, data.emissive[i] );

	for ( size_t i = 0; i < data.surfaces.size(); i++ )
		WriteRawBlock( text, data.surfaces[i] );

	if ( !data.global.empty() )
	{
		text += "global\n";
		WriteRawBlock( text, data.global[0] );
	}

	return text;
}
