/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Sky and sun setting of the map. See rtx_light_sky.h.

#include "../tr_local.h"
#include "rtx_light_edit.h"
#include "rtx_light_file.h"
#include "rtx_light_sky.h"

#include <string>
#include <stdlib.h>
#include <math.h>

#define SKY_MIN_BRIGHTNESS	0.0f
#define SKY_MAX_BRIGHTNESS	1000.0f
#define SKY_MIN_ANGLE		1.0f
#define SKY_MAX_ANGLE		10.0f
#define SKY_MAX_AMBIENT		100.0f

static float Com_Clamp( float lo, float hi, float v )
{
	return v < lo ? lo : v > hi ? hi : v;
}

static int Com_Clampi( int lo, int hi, int v )
{
	return v < lo ? lo : v > hi ? hi : v;
}

static rtxSkyDesc_t	g_sky;				// mode GLOBAL when the map has no setting
static qboolean		g_fromFile = qfalse;	// the file holds a global block
static qboolean		g_fromQ3 = qfalse;		// the values come from q3map_sun

static cvar_t *pt_sky_per_map;
static cvar_t *pt_sky_q3map_sun;
static cvar_t *pt_ambient_floor_scale;

static void RegisterCvars( void )
{
	if ( pt_sky_per_map )
		return;

	pt_sky_per_map = ri.Cvar_Get( "pt_sky_per_map", "1", CVAR_ARCHIVE_ND );
	pt_sky_q3map_sun = ri.Cvar_Get( "pt_sky_q3map_sun", "0", CVAR_ARCHIVE_ND );
	pt_ambient_floor_scale = ri.Cvar_Get( "pt_ambient_floor_scale", "1", CVAR_ARCHIVE_ND );
}

static qboolean MapHasSun( void )
{
	return (qboolean)( VectorLengthSquared( tr.sunLight ) > 0.0f );
}

static void SetDefaults( rtxSkyDesc_t *d, int mode )
{
	Com_Memset( d, 0, sizeof(*d) );

	d->mode = mode;
	d->sunAzimuth = 258.0f;
	d->sunElevation = 34.0f;
	VectorSet( d->sunColor, 1.0f, 1.0f, 1.0f );
	d->sunBrightness = 10.0f;
	d->sunAngle = 1.0f;
	VectorSet( d->ambientColor, 1.0f, 1.0f, 1.0f );
}

// The ambient floor does not depend on the mode: a mode change keeps it.
static qboolean HasAmbient( const rtxSkyDesc_t *d )
{
	return (qboolean)( d->ambientIntensity > 0.0f );
}

static void CopyAmbient( rtxSkyDesc_t *to, const rtxSkyDesc_t *from )
{
	VectorCopy( from->ambientColor, to->ambientColor );
	to->ambientIntensity = from->ambientIntensity;
}

void RTX_LightSky_Defaults( rtxSkyDesc_t *d, int mode )
{
	SetDefaults( d, mode );
}

// Values of the cvars, or of the physical sky when it runs.
static void GlobalValues( rtxSkyDesc_t *d )
{
	float az, el;

	SetDefaults( d, RTX_SKY_GLOBAL );

	d->sunAzimuth = sun_azimuth->value;
	d->sunElevation = sun_elevation->value;
	d->sunColor[0] = sun_color[0]->value;
	d->sunColor[1] = sun_color[1]->value;
	d->sunColor[2] = sun_color[2]->value;
	d->sunBrightness = sun_brightness->value;
	d->sunAngle = sun_angle->value;

	if ( vkpt_physical_sky_current_sun( &az, &el ) )
	{
		d->sunAzimuth = az;
		d->sunElevation = el;
	}

	CopyAmbient( d, &g_sky );
}

static void Sanitize( rtxSkyDesc_t *d )
{
	d->mode = Com_Clampi( RTX_SKY_GLOBAL, RTX_SKY_HYBRID, d->mode );
	d->sunAzimuth = fmodf( d->sunAzimuth, 360.0f );

	if ( d->sunAzimuth < 0.0f )
		d->sunAzimuth += 360.0f;

	d->sunElevation = Com_Clamp( -90.0f, 90.0f, d->sunElevation );
	d->sunBrightness = Com_Clamp( SKY_MIN_BRIGHTNESS, SKY_MAX_BRIGHTNESS, d->sunBrightness );
	d->sunAngle = Com_Clamp( SKY_MIN_ANGLE, SKY_MAX_ANGLE, d->sunAngle );
	d->skyDiffuse = Com_Clamp( 0.0f, 1.0f, d->skyDiffuse );
	d->ambientIntensity = Com_Clamp( 0.0f, SKY_MAX_AMBIENT, d->ambientIntensity );

	for ( int k = 0; k < 3; k++ )
	{
		d->sunColor[k] = Com_Clamp( 0.0f, 100.0f, d->sunColor[k] );
		d->ambientColor[k] = Com_Clamp( 0.0f, 1.0f, d->ambientColor[k] );
	}
}

// The sky has to compute again.
static void SkyChanged( qboolean edit )
{
	physical_sky_cvar_changed();

	if ( edit )
	{
		RTX_LightEdit_CountChange( 1 );
		RTX_LightEdit_BumpGeneration();
	}
}

void RTX_LightSky_Clear( void )
{
	RegisterCvars();

	SetDefaults( &g_sky, RTX_SKY_GLOBAL );
	g_fromFile = qfalse;
	g_fromQ3 = qfalse;

	SkyChanged( qfalse );
}

// The q3map_sun direction uses the convention of sun_azimuth and sun_elevation.
void RTX_LightSky_MapLoaded( void )
{
	RegisterCvars();

	if ( g_sky.mode != RTX_SKY_GLOBAL || !pt_sky_q3map_sun->integer || !MapHasSun() )
		return;

	vec3_t dir, color;
	rtxSkyDesc_t ambient = g_sky;

	VectorCopy( tr.sunDirection, dir );
	VectorNormalize( dir );

	const float m = MAX( tr.sunLight[0], MAX( tr.sunLight[1], tr.sunLight[2] ) );

	VectorScale( tr.sunLight, 1.0f / m, color );

	SetDefaults( &g_sky, RTX_SKY_HYBRID );
	g_sky.sunElevation = asinf( Com_Clamp( -1.0f, 1.0f, dir[2] ) ) * ( 180.0f / (float)M_PI );
	g_sky.sunAzimuth = atan2f( dir[1], dir[0] ) * ( 180.0f / (float)M_PI );
	VectorCopy( color, g_sky.sunColor );
	g_sky.sunBrightness = sun_brightness->value;
	g_sky.sunAngle = sun_angle->value;
	CopyAmbient( &g_sky, &ambient );
	Sanitize( &g_sky );

	g_fromQ3 = qtrue;
	SkyChanged( qfalse );
}

qboolean RTX_LightSky_Active( void )
{
	RegisterCvars();

	return (qboolean)( g_sky.mode != RTX_SKY_GLOBAL && pt_sky_per_map->integer );
}

int RTX_LightSky_PhysicalSky( void )
{
	if ( !RTX_LightSky_Active() )
		return physical_sky->integer;

	return g_sky.mode == RTX_SKY_PHYSICAL ? 1 : 0;
}

qboolean RTX_LightSky_Hybrid( void )
{
	return (qboolean)( RTX_LightSky_Active() && g_sky.mode == RTX_SKY_HYBRID );
}

// Only a mode with a sun has a direct sun to turn into sky light.
float RTX_LightSky_Diffuse( void )
{
	if ( !RTX_LightSky_Active() || g_sky.mode == RTX_SKY_SKYBOX )
		return 0.0f;

	return g_sky.skyDiffuse;
}

void RTX_LightSky_AmbientFloor( vec3_t out )
{
	RegisterCvars();

	VectorScale( g_sky.ambientColor, g_sky.ambientIntensity * MAX( 0.0f, pt_ambient_floor_scale->value ), out );
}

qboolean RTX_LightSky_Sun( float *azimuth, float *elevation, vec3_t color, float *brightness, float *angle )
{
	if ( !RTX_LightSky_Active() || g_sky.mode == RTX_SKY_SKYBOX )
		return qfalse;

	*azimuth = g_sky.sunAzimuth;
	*elevation = g_sky.sunElevation;
	VectorCopy( g_sky.sunColor, color );
	*brightness = g_sky.sunBrightness;
	*angle = g_sky.sunAngle;

	return qtrue;
}

void RTX_LightSky_HybridSun( float envScale, vec3_t out )
{
	VectorScale( g_sky.sunColor, g_sky.sunBrightness * envScale * RTX_SKY_HYBRID_TRANSMITTANCE, out );
}

void RTX_LightSky_CvarCheck( void )
{
	RegisterCvars();

	if ( pt_sky_per_map->modified )
	{
		pt_sky_per_map->modified = qfalse;
		SkyChanged( (qboolean)( g_sky.mode != RTX_SKY_GLOBAL ) );
	}
}

/*
=================
File layer
=================
*/

void RTX_LightSky_SetFromFile( const rtxSkyDesc_t *desc )
{
	RegisterCvars();

	g_sky = *desc;
	Sanitize( &g_sky );

	g_fromFile = (qboolean)( g_sky.mode != RTX_SKY_GLOBAL || HasAmbient( &g_sky ) );
	g_fromQ3 = qfalse;

	SkyChanged( qfalse );
}

qboolean RTX_LightSky_GetForSave( rtxSkyDesc_t *out )
{
	if ( g_sky.mode == RTX_SKY_GLOBAL && !HasAmbient( &g_sky ) )
		return qfalse;

	*out = g_sky;
	out->flags = 0;

	return qtrue;
}

// The file now holds the setting.
void RTX_LightSky_Saved( void )
{
	g_fromFile = (qboolean)( g_sky.mode != RTX_SKY_GLOBAL || HasAmbient( &g_sky ) );
	g_fromQ3 = qfalse;
}

/*
=================
API
=================
*/

qboolean RTX_LightSky_Get( rtxSkyDesc_t *out )
{
	if ( !out || !RTX_LightEdit_IsReady() )
		return qfalse;

	RegisterCvars();

	if ( g_sky.mode != RTX_SKY_GLOBAL )
		*out = g_sky;
	else
		GlobalValues( out );

	out->flags = ( g_fromFile ? RTX_SKY_FROM_FILE : 0 ) | ( g_fromQ3 ? RTX_SKY_FROM_Q3MAP_SUN : 0 )
		| ( MapHasSun() ? RTX_SKY_MAP_HAS_SUN : 0 );

	return qtrue;
}

qboolean RTX_LightSky_Set( const rtxSkyDesc_t *desc )
{
	if ( !desc || !RTX_LightEdit_IsReady() )
		return qfalse;

	rtxSkyDesc_t d = *desc;

	// The mode GLOBAL removes the sky setting. The sun values go back to the defaults.
	if ( d.mode == RTX_SKY_GLOBAL )
	{
		SetDefaults( &d, RTX_SKY_GLOBAL );
		CopyAmbient( &d, desc );
	}

	Sanitize( &d );
	d.flags = 0;

	g_sky = d;
	g_fromQ3 = qfalse;

	SkyChanged( qtrue );

	return qtrue;
}

void RTX_LightSky_Reset( void )
{
	if ( !RTX_LightEdit_IsReady() || g_sky.mode == RTX_SKY_GLOBAL )
		return;

	const rtxSkyDesc_t ambient = g_sky;

	SetDefaults( &g_sky, RTX_SKY_GLOBAL );
	CopyAmbient( &g_sky, &ambient );
	g_fromQ3 = qfalse;

	SkyChanged( qtrue );
}

/*
=================
Console commands
=================
*/

static const char *ModeName( int mode )
{
	return mode == RTX_SKY_SKYBOX ? "skybox" : mode == RTX_SKY_PHYSICAL ? "physical" : mode == RTX_SKY_HYBRID ? "hybrid" : "global";
}

// pt_sky_print: prints the block that Save writes and the values in effect.
void RTX_LightSky_Print_f( void )
{
	rtxSkyDesc_t	save;
	std::string		text;

	RegisterCvars();

	if ( RTX_LightSky_GetForSave( &save ) )
	{
		RTX_LightFile_FormatSkyBlock( text, save );
		Com_Printf( "%s", text.c_str() );
	}
	else
		Com_Printf( "no sky setting for this map: the cvars apply\n" );

	Com_Printf( "per map %i, q3map_sun %s, map sun %s, physical_sky %i (cvar %i)\n", pt_sky_per_map->integer,
		g_fromQ3 ? "default" : "no", MapHasSun() ? "yes" : "no", RTX_LightSky_PhysicalSky(), physical_sky->integer );

	float az, el, brightness, angle;
	vec3_t color;

	if ( RTX_LightSky_Sun( &az, &el, color, &brightness, &angle ) )
		Com_Printf( "sun: azimuth %g elevation %g colour %g %g %g brightness %g angle %g\n", az, el,
			color[0], color[1], color[2], brightness, angle );
	else
		Com_Printf( "sun: from the cvars\n" );

	Com_Printf( "sky_diffuse %g (in effect %g), ambient floor colour %g %g %g intensity %g (scale %g)\n", g_sky.skyDiffuse,
		RTX_LightSky_Diffuse(), g_sky.ambientColor[0], g_sky.ambientColor[1], g_sky.ambientColor[2],
		g_sky.ambientIntensity, pt_ambient_floor_scale->value );
}

// pt_sky_set <skybox|physical|hybrid|global> or <key> <values>. For debug.
void RTX_LightSky_Set_f( void )
{
	rtxSkyDesc_t	d;
	const int		argc = ri.Cmd_Argc();
	const char		*key = ri.Cmd_Argv( 1 );

	if ( argc < 2 )
	{
		Com_Printf( "usage: pt_sky_set <skybox|physical|hybrid|global>\n"
			"       pt_sky_set <sky|sun_azimuth|sun_elevation|sun_color|sun_brightness|sun_angle|sky_diffuse> <values>\n"
			"       pt_sky_set <ambient_color r g b | ambient_intensity v>   (any mode)\n" );
		return;
	}

	if ( !RTX_LightSky_Get( &d ) )
	{
		Com_Printf( "pt_sky_set: RTX light edit is not available\n" );
		return;
	}

	if ( !Q_stricmp( key, "sky" ) && argc > 2 )
		key = ri.Cmd_Argv( 2 );

	int mode = -1;

	for ( int m = RTX_SKY_GLOBAL; m <= RTX_SKY_HYBRID; m++ )
	{
		if ( !Q_stricmp( key, ModeName( m ) ) )
			mode = m;
	}

	if ( mode >= 0 )
	{
		d.mode = mode;
		RTX_LightSky_Set( &d );
		return;
	}

	// The ambient floor needs no mode.
	if ( !Q_stricmp( key, "ambient_intensity" ) && argc > 2 )
	{
		d.ambientIntensity = (float)atof( ri.Cmd_Argv( 2 ) );
		RTX_LightSky_Set( &d );
		return;
	}

	if ( !Q_stricmp( key, "ambient_color" ) && argc > 4 )
	{
		for ( int k = 0; k < 3; k++ )
			d.ambientColor[k] = (float)atof( ri.Cmd_Argv( 2 + k ) );

		RTX_LightSky_Set( &d );
		return;
	}

	if ( d.mode == RTX_SKY_GLOBAL )
	{
		Com_Printf( "pt_sky_set: set a mode first\n" );
		return;
	}

	if ( !Q_stricmp( key, "sky_diffuse" ) && argc > 2 )
		d.skyDiffuse = (float)atof( ri.Cmd_Argv( 2 ) );
	else if ( !Q_stricmp( key, "sun_azimuth" ) && argc > 2 )
		d.sunAzimuth = (float)atof( ri.Cmd_Argv( 2 ) );
	else if ( !Q_stricmp( key, "sun_elevation" ) && argc > 2 )
		d.sunElevation = (float)atof( ri.Cmd_Argv( 2 ) );
	else if ( !Q_stricmp( key, "sun_brightness" ) && argc > 2 )
		d.sunBrightness = (float)atof( ri.Cmd_Argv( 2 ) );
	else if ( !Q_stricmp( key, "sun_angle" ) && argc > 2 )
		d.sunAngle = (float)atof( ri.Cmd_Argv( 2 ) );
	else if ( !Q_stricmp( key, "sun_color" ) && argc > 4 )
	{
		for ( int k = 0; k < 3; k++ )
			d.sunColor[k] = (float)atof( ri.Cmd_Argv( 2 + k ) );
	}
	else
	{
		Com_Printf( "pt_sky_set: unknown key or missing values: %s\n", key );
		return;
	}

	RTX_LightSky_Set( &d );
}

// pt_sky_reset: removes the sky setting of the map.
void RTX_LightSky_Reset_f( void )
{
	RTX_LightSky_Reset();
}
