// Light edit mode: sky and sun of the map. The commands change the setting of the map in the
// renderer; the renderer saves it with ledit_save. The sun direction uses the convention of the
// renderer: direction to the sun = ( cos az cos el, sin az cos el, sin el ) in world axes.

#include "cg_headers.h"
#include "cg_lightedit.h"
#include "cg_lightedit_local.h"

#include <math.h>

#define LEDIT_SUN_MARK_DIST		2048.0f
#define LEDIT_SUN_ANGLE_MIN		1.0f
#define LEDIT_SUN_ANGLE_MAX		10.0f

static vmCvar_t			s_physSky;		// physical_sky of the renderer: nonzero when the global sky is physical

static const vec4_t		colSun		= { 1.00f, 0.85f, 0.20f, 1.0f };
static const vec4_t		colSunDim	= { 1.00f, 0.85f, 0.20f, 0.55f };
static const vec4_t		colSkyBg	= { 0.00f, 0.00f, 0.00f, 0.45f };
static const vec4_t		colSkyTxt	= { 1.00f, 1.00f, 1.00f, 1.0f };
static const vec4_t		colSkyGrey	= { 0.60f, 0.60f, 0.60f, 1.0f };

static const char		*s_modeNames[] = { "global", "skybox", "physical", "hybrid" };

void LE_SkyInit( void )
{
	cgi_Cvar_Register( &s_physSky, "physical_sky", "0", 0 );
}

static const char *LE_SkyModeName( int mode )
{
	return s_modeNames[Com_Clampi( 0, RTX_SKY_HYBRID, mode )];
}

static qboolean LE_SkyReady( void )
{
	return (qboolean)( s_api && s_api->GetSky && s_api->SetSky && s_api->ResetSky );
}

// Reads the values in effect. Prints the reason when it fails.
static qboolean LE_SkyRead( rtxSkyDesc_t *d )
{
	memset( d, 0, sizeof( *d ) );
	if ( !CG_LightEdit_Active() || !s_api )
	{
		LE_Msg( "light edit: not active" );
		return qfalse;
	}
	if ( !LE_SkyReady() || !s_api->GetSky( d ) )
	{
		LE_Msg( "light edit: the renderer has no sky setting" );
		return qfalse;
	}
	return qtrue;
}

// True when the sky in effect shows a sun.
static qboolean LE_SkyHasSun( const rtxSkyDesc_t *d )
{
	cgi_Cvar_Update( &s_physSky );
	if ( d->mode == RTX_SKY_GLOBAL )
	{
		return (qboolean)( s_physSky.integer != 0 );
	}
	return (qboolean)( d->mode != RTX_SKY_SKYBOX );
}

static void LE_SkyDir( const rtxSkyDesc_t *d, vec3_t out )
{
	const float	az = DEG2RAD( d->sunAzimuth );
	const float	el = DEG2RAD( d->sunElevation );

	out[0] = cosf( az ) * cosf( el );
	out[1] = sinf( az ) * cosf( el );
	out[2] = sinf( el );
}

// Applies a sky setting without an undo entry. The mode GLOBAL removes the sky setting of the map
// and keeps the ambient floor of the desc.
qboolean LE_SkyApply( const rtxSkyDesc_t *desc )
{
	if ( !LE_SkyReady() )
	{
		return qfalse;
	}
	return s_api->SetSky( desc );
}

static void LE_SkyPrint( const char *prefix, const rtxSkyDesc_t *d )
{
	char	extra[64] = "";

	if ( d->flags & RTX_SKY_MAP_HAS_SUN )
	{
		Q_strcat( extra, sizeof( extra ), " map-sun" );
	}
	if ( d->flags & RTX_SKY_FROM_Q3MAP_SUN )
	{
		Q_strcat( extra, sizeof( extra ), " q3map_sun" );
	}
	if ( d->flags & RTX_SKY_FROM_FILE )
	{
		Q_strcat( extra, sizeof( extra ), " from-file" );
	}
	LE_Msg( "%s%s%s | sun az %.1f el %.1f color %.2f %.2f %.2f x%g angle %.1f diffuse %.2f%s", prefix, LE_SkyModeName( d->mode ),
		d->mode == RTX_SKY_GLOBAL ? "" : " (map)", d->sunAzimuth, d->sunElevation, d->sunColor[0], d->sunColor[1],
		d->sunColor[2], d->sunBrightness, d->sunAngle, d->skyDiffuse, extra );
}

static void LE_AmbientPrint( const char *prefix, const rtxSkyDesc_t *d )
{
	if ( d->ambientIntensity > 0.0f )
	{
		LE_Msg( "%sambient floor %.2f %.2f %.2f x%g", prefix, d->ambientColor[0], d->ambientColor[1], d->ambientColor[2],
			d->ambientIntensity );
	}
	else
	{
		LE_Msg( "%sno ambient floor", prefix );
	}
}

// Applies the wanted values, records one undo entry and prints the result.
static void LE_SkyCommit( const rtxSkyDesc_t &before, rtxSkyDesc_t want, const char *label )
{
	rtxSkyDesc_t	after;

	want.flags = 0;
	if ( !LE_SkyApply( &want ) )
	{
		LE_Msg( "light edit: sky change refused (%s)", s_api->LastError() );
		return;
	}
	memset( &after, 0, sizeof( after ) );
	if ( want.mode == RTX_SKY_GLOBAL || !s_api->GetSky( &after ) )
	{
		after = want;
	}
	LE_UndoPushSky( &before, &after, label );
	LE_SkyPrint( "light edit: sky ", &after );
}

// A sun edit needs a setting for the map: the global sky and the skybox without sun switch to one.
static void LE_SkyForSun( rtxSkyDesc_t *d )
{
	if ( d->mode == RTX_SKY_GLOBAL )
	{
		cgi_Cvar_Update( &s_physSky );
		d->mode = s_physSky.integer ? RTX_SKY_PHYSICAL : RTX_SKY_HYBRID;
	}
	else if ( d->mode == RTX_SKY_SKYBOX )
	{
		d->mode = RTX_SKY_HYBRID;
	}
}

static qboolean LE_SkyFloats( int first, int count, float *out )
{
	for ( int i = 0; i < count; i++ )
	{
		const char	*a = CG_Argv( first + i );

		if ( !a[0] )
		{
			return qfalse;
		}
		out[i] = (float)atof( a );
	}
	return qtrue;
}

static float LE_SkyWrap360( float a )
{
	a = fmodf( a, 360.0f );
	return a < 0.0f ? a + 360.0f : a;
}

// ledit_sky [global|skybox|physical|hybrid]
void LE_CmdSky( void )
{
	rtxSkyDesc_t	cur, want;
	const char		*arg = CG_Argv( 1 );
	int				mode = -1;

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	if ( !arg[0] )
	{
		LE_SkyPrint( "light edit: sky ", &cur );
		return;
	}
	for ( int i = 0; i <= RTX_SKY_HYBRID; i++ )
	{
		if ( !Q_stricmp( arg, s_modeNames[i] ) )
		{
			mode = i;
		}
	}
	if ( mode < 0 )
	{
		LE_Msg( "usage: ledit_sky [global | skybox | physical | hybrid]" );
		return;
	}
	if ( mode == cur.mode )
	{
		LE_SkyPrint( "light edit: sky already ", &cur );
		return;
	}
	want = cur;
	want.mode = mode;
	LE_SkyCommit( cur, want, "sky mode" );
}

// ledit_sun <azimuth> <elevation>
void LE_CmdSun( void )
{
	rtxSkyDesc_t	cur, want;
	float			v[2];

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	if ( !LE_SkyFloats( 1, 2, v ) )
	{
		LE_Msg( "usage: ledit_sun <azimuth> <elevation>   (degrees; ledit_sun_here uses the view)" );
		return;
	}
	want = cur;
	LE_SkyForSun( &want );
	want.sunAzimuth = LE_SkyWrap360( v[0] );
	want.sunElevation = Com_Clamp( -90.0f, 90.0f, v[1] );
	LE_SkyCommit( cur, want, "sun direction" );
}

// ledit_sun_here: the sun comes from the direction of the view.
void LE_CmdSunHere( void )
{
	rtxSkyDesc_t	cur, want;
	const float		*f = cg.refdef.viewaxis[0];

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	want = cur;
	LE_SkyForSun( &want );
	want.sunAzimuth = LE_SkyWrap360( RAD2DEG( atan2f( f[1], f[0] ) ) );
	want.sunElevation = Com_Clamp( -90.0f, 90.0f, RAD2DEG( asinf( Com_Clamp( -1.0f, 1.0f, f[2] ) ) ) );
	LE_SkyCommit( cur, want, "sun direction" );
}

// ledit_sun_color <r> <g> <b>
void LE_CmdSunColor( void )
{
	rtxSkyDesc_t	cur, want;
	float			v[3];

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	if ( !LE_SkyFloats( 1, 3, v ) )
	{
		LE_Msg( "usage: ledit_sun_color <r> <g> <b>   (0..10 each, 1 = white)" );
		return;
	}
	want = cur;
	LE_SkyForSun( &want );
	for ( int i = 0; i < 3; i++ )
	{
		want.sunColor[i] = Com_Clamp( 0.0f, 10.0f, v[i] );
	}
	LE_SkyCommit( cur, want, "sun color" );
}

// ledit_sun_brightness <x>
void LE_CmdSunBrightness( void )
{
	rtxSkyDesc_t	cur, want;
	float			v;

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	if ( !LE_SkyFloats( 1, 1, &v ) )
	{
		LE_Msg( "usage: ledit_sun_brightness <x>" );
		return;
	}
	want = cur;
	LE_SkyForSun( &want );
	want.sunBrightness = Q_max( 0.0f, v );
	LE_SkyCommit( cur, want, "sun brightness" );
}

// ledit_sun_angle <deg>
void LE_CmdSunAngle( void )
{
	rtxSkyDesc_t	cur, want;
	float			v;

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	if ( !LE_SkyFloats( 1, 1, &v ) )
	{
		LE_Msg( "usage: ledit_sun_angle <degrees %g..%g>", LEDIT_SUN_ANGLE_MIN, LEDIT_SUN_ANGLE_MAX );
		return;
	}
	want = cur;
	LE_SkyForSun( &want );
	want.sunAngle = Com_Clamp( LEDIT_SUN_ANGLE_MIN, LEDIT_SUN_ANGLE_MAX, v );
	LE_SkyCommit( cur, want, "sun angle" );
}

// ledit_sky_reset: the map goes back to the global sky.
void LE_CmdSkyReset( void )
{
	rtxSkyDesc_t	cur, want;

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	if ( cur.mode == RTX_SKY_GLOBAL )
	{
		LE_Msg( "light edit: the map has no sky setting" );
		return;
	}
	want = cur;
	want.mode = RTX_SKY_GLOBAL;
	LE_SkyCommit( cur, want, "sky reset" );
}

// ledit_sky_diffuse <0-1>: cloud layer. The direct sun scales by (1 - value), the rest becomes sky light.
void LE_CmdSkyDiffuse( void )
{
	rtxSkyDesc_t	cur, want;
	float			v;

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	if ( !CG_Argv( 1 )[0] )
	{
		LE_Msg( "light edit: sky_diffuse %.2f (usage: ledit_sky_diffuse <0-1>)", cur.skyDiffuse );
		return;
	}
	if ( !LE_SkyFloats( 1, 1, &v ) )
	{
		LE_Msg( "usage: ledit_sky_diffuse <0-1>" );
		return;
	}
	want = cur;
	if ( cur.mode != RTX_SKY_SKYBOX )
	{
		LE_SkyForSun( &want );
	}
	want.skyDiffuse = Com_Clamp( 0.0f, 1.0f, v );
	LE_SkyCommit( cur, want, "sky diffuse" );
	if ( want.mode == RTX_SKY_SKYBOX )
	{
		LE_Msg( "light edit: the skybox mode has no sun: sky_diffuse is saved but ignored (use ledit_sky hybrid)" );
	}
}

// ledit_ambient <r> <g> <b> [intensity]: floor of the lighting of a surface point. Any sky mode.
void LE_CmdAmbient( void )
{
	rtxSkyDesc_t	cur, want;
	float			v[4];

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	if ( !CG_Argv( 1 )[0] )
	{
		LE_AmbientPrint( "light edit: ", &cur );
		return;
	}
	if ( !LE_SkyFloats( 1, 3, v ) )
	{
		LE_Msg( "usage: ledit_ambient <r> <g> <b> [intensity]   (colour 0..1, intensity 0..100, 0 = off)" );
		return;
	}
	want = cur;
	for ( int i = 0; i < 3; i++ )
	{
		want.ambientColor[i] = Com_Clamp( 0.0f, 1.0f, v[i] );
	}
	if ( LE_SkyFloats( 4, 1, &v[3] ) )
	{
		want.ambientIntensity = Com_Clamp( 0.0f, 100.0f, v[3] );
	}
	else if ( want.ambientIntensity <= 0.0f )
	{
		want.ambientIntensity = 1.0f;
	}
	LE_SkyCommit( cur, want, "ambient floor" );
	LE_AmbientPrint( "light edit: ", &want );
}

// ledit_ambient_reset: no ambient floor.
void LE_CmdAmbientReset( void )
{
	rtxSkyDesc_t	cur, want;

	if ( !LE_SkyRead( &cur ) )
	{
		return;
	}
	if ( cur.ambientIntensity <= 0.0f )
	{
		LE_Msg( "light edit: the map has no ambient floor" );
		return;
	}
	want = cur;
	want.ambientIntensity = 0.0f;
	LE_SkyCommit( cur, want, "ambient reset" );
}

/*
=================
Display
=================
*/
// Marker at the projected sun direction: a box and a dot.
void LE_SkyDrawSun( void )
{
	rtxSkyDesc_t	d;
	vec3_t			dir, p;
	float			sx, sy;

	if ( !LE_SkyReady() || !s_api->GetSky( &d ) || !LE_SkyHasSun( &d ) )
	{
		return;
	}
	LE_SkyDir( &d, dir );
	if ( DotProduct( dir, cg.refdef.viewaxis[0] ) <= 0.05f )
	{
		return;
	}
	VectorMA( cg.refdef.vieworg, LEDIT_SUN_MARK_DIST, dir, p );
	if ( CG_WorldCoordToScreenCoordFloat( p, &sx, &sy ) && sx > 8 && sx < 632 && sy > 8 && sy < 472 )
	{
		LE_Box( sx, sy, 10.0f, colSunDim );
		LE_Dot( sx, sy, 5.0f, colSun );
	}
}

// Two lines on the panel when no light is selected. y is the top of the panel.
void LE_SkyDrawPanel( int y )
{
	rtxSkyDesc_t	d;
	rtxLightStats_t	st;
	char			l0[64], l1[64], l2[64] = "";

	if ( !LE_SkyReady() || !s_api->GetSky( &d ) )
	{
		return;
	}
	s_api->GetStats( &st );
	cgi_Cvar_Update( &s_physSky );
	Com_sprintf( l0, sizeof( l0 ), "sky: %s%s%s", LE_SkyModeName( d.mode ), d.mode == RTX_SKY_GLOBAL ? "" : " (map)",
		st.unsavedChanges > 0 ? " *" : "" );
	if ( LE_SkyHasSun( &d ) )
	{
		Com_sprintf( l1, sizeof( l1 ), "sun az %.0f el %.0f x%g", d.sunAzimuth, d.sunElevation, d.sunBrightness );
	}
	else
	{
		Com_sprintf( l1, sizeof( l1 ), "no sun" );
	}
	if ( d.skyDiffuse > 0.0f && d.ambientIntensity > 0.0f )
	{
		Com_sprintf( l2, sizeof( l2 ), "diffuse %.2f  floor x%g", d.skyDiffuse, d.ambientIntensity );
	}
	else if ( d.skyDiffuse > 0.0f )
	{
		Com_sprintf( l2, sizeof( l2 ), "diffuse %.2f", d.skyDiffuse );
	}
	else if ( d.ambientIntensity > 0.0f )
	{
		Com_sprintf( l2, sizeof( l2 ), "floor x%g", d.ambientIntensity );
	}

	const int	h = LE_TextH();
	const int	lines = l2[0] ? 3 : 2;
	const int	w = Q_max( Q_max( LE_TextW( l0 ), LE_TextW( l1 ) ), LE_TextW( l2 ) );

	CG_FillRect( 640 - 8 - w - 4, y - 3, w + 10, lines * h + 6, colSkyBg );
	LE_Text( 640 - 8 - LE_TextW( l0 ), y, l0, d.mode == RTX_SKY_GLOBAL ? colSkyGrey : colSkyTxt );
	LE_Text( 640 - 8 - LE_TextW( l1 ), y + h, l1, colSkyGrey );
	if ( l2[0] )
	{
		LE_Text( 640 - 8 - LE_TextW( l2 ), y + 2 * h, l2, colSkyGrey );
	}
}
