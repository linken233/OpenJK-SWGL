/*
===========================================================================
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// g_FastRendererSwitch: two renderers run at the same time, behind one refexport_t.
//
// Renderer 0 is cl_renderer. It owns the ghoul2 instances and answers every query.
// Renderer 1 is the default renderer. It gets every registration and world call too,
// so both hold the same level. Frame calls go to the renderer on screen, and to both
// under g_ShowSplit. Renderer 1 has its own handles, so the calls to it translate them.

#include "../server/exe_headers.h"

#include "client.h"
#include "cl_dualref.h"
#include "sys/sys_public.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#define DR_SCRATCH_CLIENT	15	// scratch image for the split view, clear of the cinematics

static refexport_t	dr_re[2];
static char			dr_names[2][MAX_QPATH];
static qboolean		dr_active;
static int			dr_shown;				// renderer on screen
static qboolean		dr_split;				// the other renderer fills the right half
static int			dr_pendingShown;
static qboolean		dr_pendingSplit;
static int			dr_frameSet[2];			// renderers that get the frame calls
static int			dr_frameCount = 1;
static int			dr_vidWidth, dr_vidHeight;

static qhandle_t	dr_charSet[2];
static qhandle_t	dr_white[2];

static byte			*dr_capture;			// full frame of the other renderer
static byte			*dr_half;				// its right half
static int			dr_halfWidth, dr_halfHeight;

// Renderer 0 handle -> renderer 1 handle.
static std::unordered_map<int, int> dr_modelMap, dr_shaderMap, dr_skinMap, dr_fontMap;

static inline refexport_t &DR( int r )
{
	WIN_SelectSlot( r );
	return dr_re[r];
}

static inline int DR_Map( const std::unordered_map<int, int> &map, int r, int handle )
{
	if ( r == 0 || handle == 0 ) {
		return handle;
	}
	std::unordered_map<int, int>::const_iterator it = map.find( handle );
	return it != map.end() ? it->second : 0;
}

/*
==============================================================================

GHOUL2

Both renderers draw the same ghoul2 instances, from the array of renderer 0. The two
renderers resolve models and build bone caches in different layouts, so each renderer
keeps its own copy of those fields. Around each call to renderer 1 that can draw a
ghoul2 instance, its fields go in and the fields of renderer 0 come out.

==============================================================================
*/

struct G2RendererFields
{
	qhandle_t			mModel;
	CBoneCache			*mBoneCache;
	intptr_t			*mTransformedVertsArray;
	bool				mValid;
	int					mSetupFrame;
	int					mSetupEpoch;
	const model_s		*currentModel;
	int					currentModelSize;
	const model_s		*animModel;
	int					currentAnimModelSize;
	const mdxaHeader_t	*aHeader;
	qhandle_t			mCustomShader;
	qhandle_t			mCustomSkin;
	int					mSkin;
#ifdef _G2_GORE
	int					mGoreSetTag;
#endif
};

struct G2SecondState
{
	char				fileName[MAX_QPATH];
	G2RendererFields	fields;
};

static std::unordered_map<int, std::vector<G2SecondState> > dr_g2Second;	// ghoul2 handle -> state of renderer 1
static std::vector<int>				dr_g2Tracked;		// instances added to the scene of renderer 1
static std::unordered_set<int>		dr_g2TrackedSet;
static std::vector<CGhoul2Info *>	dr_g2Swapped;		// instances with the fields of renderer 1 in them
static std::vector<G2SecondState *>	dr_g2States;
static std::vector<G2RendererFields> dr_g2Saved;

static void G2_ReadFields( const CGhoul2Info &g, G2RendererFields &f )
{
	f.mModel = g.mModel;
	f.mBoneCache = g.mBoneCache;
	f.mTransformedVertsArray = g.mTransformedVertsArray;
	f.mValid = g.mValid;
	f.mSetupFrame = g.mSetupFrame;
	f.mSetupEpoch = g.mSetupEpoch;
	f.currentModel = g.currentModel;
	f.currentModelSize = g.currentModelSize;
	f.animModel = g.animModel;
	f.currentAnimModelSize = g.currentAnimModelSize;
	f.aHeader = g.aHeader;
	f.mCustomShader = g.mCustomShader;
	f.mCustomSkin = g.mCustomSkin;
	f.mSkin = g.mSkin;
#ifdef _G2_GORE
	f.mGoreSetTag = g.mGoreSetTag;
#endif
}

static void G2_WriteFields( CGhoul2Info &g, const G2RendererFields &f )
{
	g.mModel = f.mModel;
	g.mBoneCache = f.mBoneCache;
	g.mTransformedVertsArray = f.mTransformedVertsArray;
	g.mValid = f.mValid;
	g.mSetupFrame = f.mSetupFrame;
	g.mSetupEpoch = f.mSetupEpoch;
	g.currentModel = f.currentModel;
	g.currentModelSize = f.currentModelSize;
	g.animModel = f.animModel;
	g.currentAnimModelSize = f.currentAnimModelSize;
	g.aHeader = f.aHeader;
	g.mCustomShader = f.mCustomShader;
	g.mCustomSkin = f.mCustomSkin;
	g.mSkin = f.mSkin;
#ifdef _G2_GORE
	g.mGoreSetTag = f.mGoreSetTag;
#endif
}

static void G2_ResetSecond( G2SecondState &s, const char *fileName )
{
	// A bone cache of renderer 1 left here leaks: only renderer 1 can delete it.
	memset( &s.fields, 0, sizeof( s.fields ) );
	s.fields.mSetupFrame = -1;
	s.fields.mSetupEpoch = -1;
	Q_strncpyz( s.fileName, fileName, sizeof( s.fileName ) );
}

static inline int G2_Handle( const void *ghoul2 )
{
	// CGhoul2Info_v holds only its handle into the array.
	return ghoul2 ? *(const int *)ghoul2 : 0;
}

static void G2_SwapIn( int handle )
{
	IGhoul2InfoArray &infoArray = dr_re[0].TheGhoul2InfoArray();
	if ( !infoArray.IsValid( handle ) ) {
		return;
	}

	std::vector<CGhoul2Info> &infos = infoArray.Get( handle );
	std::vector<G2SecondState> &second = dr_g2Second[handle];
	if ( second.size() < infos.size() ) {
		const size_t oldSize = second.size();
		second.resize( infos.size() );
		for ( size_t i = oldSize; i < second.size(); i++ ) {
			G2_ResetSecond( second[i], "" );
		}
	}

	for ( size_t i = 0; i < infos.size(); i++ ) {
		CGhoul2Info &g = infos[i];
		G2SecondState &s = second[i];
		G2RendererFields saved;

		if ( strcmp( s.fileName, g.mFileName ) ) {
			G2_ResetSecond( s, g.mFileName );
		}

		G2_ReadFields( g, saved );
		s.fields.mCustomShader = DR_Map( dr_shaderMap, 1, saved.mCustomShader );
		s.fields.mCustomSkin = DR_Map( dr_skinMap, 1, saved.mCustomSkin );
		s.fields.mSkin = DR_Map( dr_skinMap, 1, saved.mSkin );
#ifdef _G2_GORE
		s.fields.mGoreSetTag = 0;	// the gore sets live in renderer 0
#endif
		G2_WriteFields( g, s.fields );

		dr_g2Swapped.push_back( &g );
		dr_g2States.push_back( &s );
		dr_g2Saved.push_back( saved );
	}
}

static void G2_SwapInTracked( void )
{
	for ( size_t i = 0; i < dr_g2Tracked.size(); i++ ) {
		G2_SwapIn( dr_g2Tracked[i] );
	}
}

// Keeps the fields of renderer 1 and puts back those of renderer 0.
static void G2_SwapOut( void )
{
	for ( size_t n = 0; n < dr_g2Swapped.size(); n++ ) {
		G2_ReadFields( *dr_g2Swapped[n], dr_g2States[n]->fields );
		G2_WriteFields( *dr_g2Swapped[n], dr_g2Saved[n] );
	}

	dr_g2Swapped.clear();
	dr_g2States.clear();
	dr_g2Saved.clear();
}

static void G2_Track( int handle )
{
	if ( handle && dr_g2TrackedSet.insert( handle ).second ) {
		dr_g2Tracked.push_back( handle );
	}
}

static void G2_ClearTracked( void )
{
	dr_g2Tracked.clear();
	dr_g2TrackedSet.clear();
}

/*
==============================================================================

REGISTRATION: both renderers, handles of renderer 0 returned

==============================================================================
*/

static qhandle_t DR_Register( qhandle_t (*fn0)( const char * ), qhandle_t (*fn1)( const char * ),
	std::unordered_map<int, int> &map, const char *name )
{
	WIN_SelectSlot( 0 );
	const qhandle_t h0 = fn0( name );
	WIN_SelectSlot( 1 );
	const qhandle_t h1 = fn1( name );
	if ( h0 ) {
		map[h0] = h1;
	}
	return h0;
}

static qhandle_t DR_RegisterModel( const char *name ) { return DR_Register( dr_re[0].RegisterModel, dr_re[1].RegisterModel, dr_modelMap, name ); }
static qhandle_t DR_RegisterSkin( const char *name ) { return DR_Register( dr_re[0].RegisterSkin, dr_re[1].RegisterSkin, dr_skinMap, name ); }
static qhandle_t DR_RegisterShader( const char *name ) { return DR_Register( dr_re[0].RegisterShader, dr_re[1].RegisterShader, dr_shaderMap, name ); }
static qhandle_t DR_RegisterShaderNoMip( const char *name ) { return DR_Register( dr_re[0].RegisterShaderNoMip, dr_re[1].RegisterShaderNoMip, dr_shaderMap, name ); }

static int DR_RegisterFont( const char *name )
{
	const int h0 = DR( 0 ).RegisterFont( name );
	const int h1 = DR( 1 ).RegisterFont( name );
	if ( h0 ) {
		dr_fontMap[h0] = h1;
	}
	return h0;
}

static qhandle_t DR_G2API_PrecacheGhoul2Model( const char *fileName )
{
	return DR_Register( dr_re[0].G2API_PrecacheGhoul2Model, dr_re[1].G2API_PrecacheGhoul2Model, dr_modelMap, fileName );
}

static int DR_G2API_InitGhoul2Model( CGhoul2Info_v &ghoul2, const char *fileName, int modelIndex,
	qhandle_t customSkin, qhandle_t customShader, int modelFlags, int lodBias )
{
	const int ret = DR( 0 ).G2API_InitGhoul2Model( ghoul2, fileName, modelIndex, customSkin, customShader, modelFlags, lodBias );
	if ( fileName && fileName[0] ) {
		DR( 1 ).RegisterModel( fileName );	// load it now, not on the first frame that draws it
	}
	return ret;
}

static void DR_BeginRegistration( glconfig_t *config )
{
	glconfig_t config1;

	DR( 0 ).BeginRegistration( config );
	DR( 1 ).BeginRegistration( &config1 );

	dr_vidWidth = config->vidWidth;
	dr_vidHeight = config->vidHeight;

	for ( int r = 0; r < 2; r++ ) {
		dr_charSet[r] = DR( r ).RegisterShaderNoMip( "gfx/2d/charsgrid_med" );
		dr_white[r] = DR( r ).RegisterShader( "white" );
	}
}

static void DR_LoadWorld( const char *name ) { DR( 0 ).LoadWorld( name ); DR( 1 ).LoadWorld( name ); }
static void DR_SetWorldVisData( const byte *vis ) { DR( 0 ).SetWorldVisData( vis ); DR( 1 ).SetWorldVisData( vis ); }
static void DR_EndRegistration( void ) { DR( 0 ).EndRegistration(); DR( 1 ).EndRegistration(); }

static void DR_RegisterMedia_LevelLoadBegin( const char *psMapName, ForceReload_e eForceReload, qboolean bAllowScreenDissolve )
{
	// The level frees its ghoul2 instances. The bone caches of renderer 1 leak with them.
	dr_g2Second.clear();
	G2_ClearTracked();

	// Only the renderer on screen does the screen dissolve.
	DR( 0 ).RegisterMedia_LevelLoadBegin( psMapName, eForceReload, dr_shown == 0 ? bAllowScreenDissolve : qfalse );
	DR( 1 ).RegisterMedia_LevelLoadBegin( psMapName, eForceReload, dr_shown == 1 ? bAllowScreenDissolve : qfalse );
}

static void DR_RegisterMedia_LevelLoadEnd( void ) { DR( 0 ).RegisterMedia_LevelLoadEnd(); DR( 1 ).RegisterMedia_LevelLoadEnd(); }

static qboolean DR_RegisterModels_LevelLoadEnd( qboolean bDeleteEverythingNotUsedThisLevel )
{
	const qboolean ret = DR( 0 ).RegisterModels_LevelLoadEnd( bDeleteEverythingNotUsedThisLevel );
	DR( 1 ).RegisterModels_LevelLoadEnd( bDeleteEverythingNotUsedThisLevel );
	return ret;
}

static qboolean DR_RegisterImages_LevelLoadEnd( void )
{
	const qboolean ret = DR( 0 ).RegisterImages_LevelLoadEnd();
	DR( 1 ).RegisterImages_LevelLoadEnd();
	return ret;
}

static void DR_SetLightStyle( int style, int color ) { DR( 0 ).SetLightStyle( style, color ); DR( 1 ).SetLightStyle( style, color ); }
static void DR_WorldEffectCommand( const char *command ) { DR( 0 ).WorldEffectCommand( command ); DR( 1 ).WorldEffectCommand( command ); }
static void DR_R_InitWorldEffects( void ) { DR( 0 ).R_InitWorldEffects(); DR( 1 ).R_InitWorldEffects(); }
static void DR_SVModelInit( void ) { DR( 0 ).SVModelInit(); DR( 1 ).SVModelInit(); }
static void DR_AddWeatherZone( vec3_t mins, vec3_t maxs ) { DR( 0 ).AddWeatherZone( mins, maxs ); DR( 1 ).AddWeatherZone( mins, maxs ); }
static void DR_SetRangedFog( float dist ) { DR( 0 ).SetRangedFog( dist ); DR( 1 ).SetRangedFog( dist ); }
static void DR_G2API_SetTime( int currentTime, int clock ) { DR( 0 ).G2API_SetTime( currentTime, clock ); DR( 1 ).G2API_SetTime( currentTime, clock ); }

static void DR_R_ClearStuffToStopGhoul2CrashingThings( void )
{
	DR( 0 ).R_ClearStuffToStopGhoul2CrashingThings();
	DR( 1 ).R_ClearStuffToStopGhoul2CrashingThings();
}

static bool DR_SetTempGlobalFogColor( vec3_t color )
{
	const bool ret = DR( 0 ).SetTempGlobalFogColor( color );
	DR( 1 ).SetTempGlobalFogColor( color );
	return ret;
}

/*
==============================================================================

FRAME: the renderer on screen, and the other one under g_ShowSplit

==============================================================================
*/

#define DR_FOR_FRAME( r ) for ( int dr_n = 0, r = dr_frameSet[0]; dr_n < dr_frameCount; r = dr_frameSet[++dr_n < dr_frameCount ? dr_n : 0] )

static void DR_ClearScene( void )
{
	DR_FOR_FRAME( r ) {
		if ( r == 1 ) {
			G2_ClearTracked();
		}
		DR( r ).ClearScene();
	}
}

static void DR_AddRefEntityToScene( const refEntity_t *ent )
{
	DR_FOR_FRAME( r ) {
		if ( r == 0 ) {
			DR( 0 ).AddRefEntityToScene( ent );
			continue;
		}

		refEntity_t copy = *ent;
		copy.hModel = DR_Map( dr_modelMap, 1, ent->hModel );
		copy.customShader = DR_Map( dr_shaderMap, 1, ent->customShader );
		copy.customSkin = DR_Map( dr_skinMap, 1, ent->customSkin );

		const int handle = G2_Handle( ent->ghoul2 );
		if ( handle ) {
			G2_Track( handle );
			G2_SwapIn( handle );
			DR( 1 ).AddRefEntityToScene( &copy );
			G2_SwapOut();
		} else {
			DR( 1 ).AddRefEntityToScene( &copy );
		}
	}
}

static void DR_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys )
{
	DR_FOR_FRAME( r ) {
		DR( r ).AddPolyToScene( DR_Map( dr_shaderMap, r, hShader ), numVerts, verts, numPolys );
	}
}

static void DR_AddLightToScene( const vec3_t org, float intensity, float red, float green, float blue )
{
	DR_FOR_FRAME( r ) {
		DR( r ).AddLightToScene( org, intensity, red, green, blue );
	}
}

static void DR_RenderScene( const refdef_t *fd )
{
	DR_FOR_FRAME( r ) {
		if ( r == 0 ) {
			DR( 0 ).RenderScene( fd );
			continue;
		}

		// The cgame writes the distortion settings through the pointers of renderer 0.
		if ( dr_re[0].tr_distortionAlpha && dr_re[1].tr_distortionAlpha ) {
			*dr_re[1].tr_distortionAlpha() = *dr_re[0].tr_distortionAlpha();
			*dr_re[1].tr_distortionStretch() = *dr_re[0].tr_distortionStretch();
			*dr_re[1].tr_distortionPrePost() = *dr_re[0].tr_distortionPrePost();
			*dr_re[1].tr_distortionNegate() = *dr_re[0].tr_distortionNegate();
		}

		G2_SwapInTracked();
		DR( 1 ).RenderScene( fd );
		G2_SwapOut();
		G2_ClearTracked();
	}
}

static void DR_SetColor( const float *rgba )
{
	DR_FOR_FRAME( r ) {
		DR( r ).SetColor( rgba );
	}
}

static void DR_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader )
{
	DR_FOR_FRAME( r ) {
		DR( r ).DrawStretchPic( x, y, w, h, s1, t1, s2, t2, DR_Map( dr_shaderMap, r, hShader ) );
	}
}

static void DR_DrawRotatePic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, float a1, qhandle_t hShader, float aspectCorrection )
{
	DR_FOR_FRAME( r ) {
		DR( r ).DrawRotatePic( x, y, w, h, s1, t1, s2, t2, a1, DR_Map( dr_shaderMap, r, hShader ), aspectCorrection );
	}
}

static void DR_DrawRotatePic2( float x, float y, float w, float h, float s1, float t1, float s2, float t2, float a1, qhandle_t hShader, float aspectCorrection )
{
	DR_FOR_FRAME( r ) {
		DR( r ).DrawRotatePic2( x, y, w, h, s1, t1, s2, t2, a1, DR_Map( dr_shaderMap, r, hShader ), aspectCorrection );
	}
}

static void DR_LAGoggles( void )
{
	DR_FOR_FRAME( r ) {
		DR( r ).LAGoggles();
	}
}

static void DR_Scissor( float x, float y, float w, float h )
{
	DR_FOR_FRAME( r ) {
		DR( r ).Scissor( x, y, w, h );
	}
}

static void DR_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty )
{
	DR_FOR_FRAME( r ) {
		DR( r ).DrawStretchRaw( x, y, w, h, cols, rows, data, client, dirty );
	}
}

static void DR_UploadCinematic( int cols, int rows, const byte *data, int client, qboolean dirty )
{
	DR_FOR_FRAME( r ) {
		DR( r ).UploadCinematic( cols, rows, data, client, dirty );
	}
}

static void DR_Font_DrawString( int x, int y, const char *s, const float *rgba, const int iFontHandle, int iMaxPixelWidth, const float scale, const float fAspectCorrection )
{
	DR_FOR_FRAME( r ) {
		DR( r ).Font_DrawString( x, y, s, rgba, DR_Map( dr_fontMap, r, iFontHandle ), iMaxPixelWidth, scale, fAspectCorrection );
	}
}

static qboolean DR_ProcessDissolve( void )
{
	qboolean ret = qfalse;
	DR_FOR_FRAME( r ) {
		const qboolean dissolving = DR( r ).ProcessDissolve();
		if ( r == dr_shown ) {
			ret = dissolving;
		}
	}
	return ret;
}

static qboolean DR_InitDissolve( qboolean bForceCircularExtroWipe )
{
	qboolean ret = qfalse;
	DR_FOR_FRAME( r ) {
		const qboolean started = DR( r ).InitDissolve( bForceCircularExtroWipe );
		if ( r == dr_shown ) {
			ret = started;
		}
	}
	return ret;
}

static void DR_GetScreenShot( byte *data, int w, int h )
{
	DR( dr_shown ).GetScreenShot( data, w, h );
}

static void DR_BeginFrame( stereoFrame_t stereoFrame )
{
	if ( dr_pendingShown != dr_shown ) {
		dr_shown = dr_pendingShown;
		WIN_ShowSlot( dr_shown );
	}
	dr_split = (qboolean)( dr_pendingSplit && dr_re[dr_shown ^ 1].CaptureNextFrame != NULL );

	dr_frameSet[0] = dr_shown;
	dr_frameSet[1] = dr_shown ^ 1;
	dr_frameCount = dr_split ? 2 : 1;

	DR_FOR_FRAME( r ) {
		DR( r ).BeginFrame( stereoFrame );
	}
}

// Draws text at the 640x480 virtual resolution, with the character set of renderer r.
static void DR_DrawLabel( int r, float x, float y, const char *text )
{
	static const float background[4] = { 0.0f, 0.0f, 0.0f, 0.6f };
	static const float foreground[4] = { 1.0f, 0.85f, 0.2f, 1.0f };
	const float charWidth = 8.0f;
	const float charHeight = 12.0f;
	const int length = (int)strlen( text );
	refexport_t &R = DR( r );

	R.SetColor( background );
	R.DrawStretchPic( x - 3.0f, y - 2.0f, length * charWidth + 6.0f, charHeight + 4.0f, 0.0f, 0.0f, 1.0f, 1.0f, dr_white[r] );

	R.SetColor( foreground );
	for ( int i = 0; i < length; i++ ) {
		const int ch = text[i] & 255;
		if ( ch == ' ' ) {
			continue;
		}
		const float fcol = ( ch & 15 ) * 0.0625f;
		const float frow = ( ch >> 4 ) * 0.0625f;
		R.DrawStretchPic( x + i * charWidth, y, charWidth, charHeight, fcol, frow, fcol + 0.03125f, frow + 0.0625f, dr_charSet[r] );
	}
	R.SetColor( NULL );
}

static int DR_Pow2( int size )
{
	int lower = 1;
	while ( lower * 2 <= size ) {
		lower *= 2;
	}
	// Round down when little is lost; the upload of each frame is then 4x smaller.
	return ( lower == size || lower * 10 >= size * 7 ) ? lower : lower * 2;
}

// Makes the image of the other renderer show on screen as it shows in its own window.
// The capture F already holds that renderer's gamma and overbright. The renderer on screen
// shows a raw image x as pow( x / 2^ob, 1 / r_gamma ) * 2^ob, so x is the inverse of that.
static void DR_MatchDisplay( int shown, byte *rgba, int count )
{
	byte lut[256];
	const float gamma = Com_Clamp( 0.5f, 3.0f, Cvar_VariableValue( "r_gamma" ) );
	int ob;

	if ( Q_stristr( dr_names[shown], "vulkan" ) ) {
		// The gamma pass scales by the overbright, except for the tracer's image.
		ob = Cvar_VariableIntegerValue( "r_rtxActive" ) ? 0 : Com_Clampi( 0, 2, Cvar_VariableIntegerValue( "r_overBrightBits" ) );
	} else {
		// rd-vanilla only overbrights in fullscreen, with its gamma table.
		ob = ( Cvar_VariableIntegerValue( "r_fullscreen" ) && !Cvar_VariableIntegerValue( "r_ignorehwgamma" ) )
			? Com_Clampi( 0, 1, Cvar_VariableIntegerValue( "r_overBrightBits" ) ) : 0;
	}

	const float scale = (float)( 1 << ob );
	for ( int v = 0; v < 256; v++ ) {
		const float x = scale * powf( ( v / 255.0f ) / scale, gamma );
		lut[v] = (byte)Com_Clampi( 0, 255, (int)( x * 255.0f + 0.5f ) );
	}

	for ( int i = 0; i < count; i++, rgba += 4 ) {
		rgba[0] = lut[rgba[0]];
		rgba[1] = lut[rgba[1]];
		rgba[2] = lut[rgba[2]];
	}
}

// Renders the other renderer, then draws its right half over the right half of this one.
static void DR_DrawSplit( int shown, int other )
{
	const int halfWidth = DR_Pow2( dr_vidWidth / 2 );
	const int halfHeight = DR_Pow2( dr_vidHeight );

	if ( halfWidth != dr_halfWidth || halfHeight != dr_halfHeight ) {
		if ( dr_capture ) {
			Z_Free( dr_capture );
			Z_Free( dr_half );
		}
		dr_halfWidth = halfWidth;
		dr_halfHeight = halfHeight;
		dr_capture = (byte *)Z_Malloc( halfWidth * 2 * halfHeight * 4, TAG_CLIENTS, qtrue );
		dr_half = (byte *)Z_Malloc( halfWidth * halfHeight * 4, TAG_CLIENTS, qtrue );
	}

	DR( other ).CaptureNextFrame( dr_capture, halfWidth * 2, halfHeight );
	DR( other ).EndFrame( NULL, NULL );

	for ( int y = 0; y < halfHeight; y++ ) {
		memcpy( dr_half + y * halfWidth * 4, dr_capture + ( y * halfWidth * 2 + halfWidth ) * 4, halfWidth * 4 );
	}
	DR_MatchDisplay( shown, dr_half, halfWidth * halfHeight );

	refexport_t &R = DR( shown );
	static const float divider[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	R.SetColor( NULL );
	R.DrawStretchRaw( 320, 0, 320, 480, halfWidth, halfHeight, dr_half, DR_SCRATCH_CLIENT, qtrue );
	R.SetColor( divider );
	R.DrawStretchPic( 319.5f, 0.0f, 1.0f, 480.0f, 0.0f, 0.0f, 1.0f, 1.0f, dr_white[shown] );
	R.SetColor( NULL );
}

const char *CL_DualRef_ShownName( void )
{
	return dr_active ? dr_names[dr_shown] : NULL;
}

// Name of renderer r, with the state of the path tracer for the Vulkan renderer.
static const char *DR_Label( int r )
{
	if ( !Q_stristr( dr_names[r], "vulkan" ) ) {
		return dr_names[r];
	}
	return va( "%s (RTX %s)", dr_names[r], Cvar_VariableIntegerValue( "r_rtxActive" ) ? "enabled" : "disabled" );
}

static void DR_EndFrame( int *frontEndMsec, int *backEndMsec )
{
	const int shown = dr_frameSet[0];
	const int other = shown ^ 1;

	if ( dr_frameCount == 2 ) {
		DR_DrawSplit( shown, other );
	}

	const char *shownLabel = DR_Label( shown );
	DR_DrawLabel( shown, 6.0f, 6.0f, shownLabel );
	if ( dr_frameCount == 2 ) {
		const char *otherLabel = DR_Label( other );
		DR_DrawLabel( shown, 634.0f - strlen( otherLabel ) * 8.0f, 6.0f, otherLabel );
	}

	DR( shown ).EndFrame( frontEndMsec, backEndMsec );
}

// Also runs without destroyWindow, before BeginRegistration after an error drop.
// Both renderers then stay loaded, and the window on screen stays the same.
static void DR_Shutdown( qboolean destroyWindow, qboolean restarting )
{
	if ( destroyWindow && dr_shown != 0 ) {
		// The window of renderer 0 must be the one on screen when it is recreated.
		WIN_ShowSlot( 0 );
		dr_shown = dr_pendingShown = 0;
		dr_frameSet[0] = 0;
	}

	DR( 1 ).Shutdown( destroyWindow, restarting );
	DR( 0 ).Shutdown( destroyWindow, restarting );
	WIN_SelectSlot( 0 );

	if ( dr_capture ) {
		Z_Free( dr_capture );
		Z_Free( dr_half );
		dr_capture = dr_half = NULL;
		dr_halfWidth = dr_halfHeight = 0;
	}

	dr_modelMap.clear();
	dr_shaderMap.clear();
	dr_skinMap.clear();
	dr_fontMap.clear();
	dr_g2Second.clear();
	G2_ClearTracked();
	dr_frameCount = 1;
}

/*
==============================================================================

SETUP AND COMMANDS

==============================================================================
*/

// Only one renderer has a given extension, and it can be either one.
static void *DR_GetExtension( const char *name )
{
	for ( int i = 0; i < 2; i++ ) {
		void *ext = dr_re[i].GetExtension ? dr_re[i].GetExtension( name ) : NULL;
		if ( ext ) {
			return ext;
		}
	}
	return NULL;
}

const refexport_t *CL_DualRef_Init( const refexport_t *first, const char *firstName, const refexport_t *second, const char *secondName )
{
	static refexport_t dual;

	dr_re[0] = *first;
	dr_re[1] = *second;
	Q_strncpyz( dr_names[0], firstName, sizeof( dr_names[0] ) );
	Q_strncpyz( dr_names[1], secondName, sizeof( dr_names[1] ) );

	// Queries, and the ghoul2 API, go to renderer 0 as they are.
	dual = *first;
	dual.CaptureNextFrame = NULL;

	dual.Shutdown = DR_Shutdown;
	dual.GetExtension = DR_GetExtension;
	dual.BeginRegistration = DR_BeginRegistration;
	dual.RegisterModel = DR_RegisterModel;
	dual.RegisterSkin = DR_RegisterSkin;
	dual.RegisterShader = DR_RegisterShader;
	dual.RegisterShaderNoMip = DR_RegisterShaderNoMip;
	dual.RegisterFont = DR_RegisterFont;
	dual.LoadWorld = DR_LoadWorld;
	dual.RegisterMedia_LevelLoadBegin = DR_RegisterMedia_LevelLoadBegin;
	dual.RegisterMedia_LevelLoadEnd = DR_RegisterMedia_LevelLoadEnd;
	dual.RegisterModels_LevelLoadEnd = DR_RegisterModels_LevelLoadEnd;
	dual.RegisterImages_LevelLoadEnd = DR_RegisterImages_LevelLoadEnd;
	dual.SetWorldVisData = DR_SetWorldVisData;
	dual.EndRegistration = DR_EndRegistration;
	dual.SetLightStyle = DR_SetLightStyle;
	dual.WorldEffectCommand = DR_WorldEffectCommand;
	dual.R_InitWorldEffects = DR_R_InitWorldEffects;
	dual.R_ClearStuffToStopGhoul2CrashingThings = DR_R_ClearStuffToStopGhoul2CrashingThings;
	dual.SVModelInit = DR_SVModelInit;
	dual.AddWeatherZone = DR_AddWeatherZone;
	dual.SetTempGlobalFogColor = DR_SetTempGlobalFogColor;
	dual.SetRangedFog = DR_SetRangedFog;
	dual.G2API_SetTime = DR_G2API_SetTime;
	dual.G2API_PrecacheGhoul2Model = DR_G2API_PrecacheGhoul2Model;
	dual.G2API_InitGhoul2Model = DR_G2API_InitGhoul2Model;

	dual.ClearScene = DR_ClearScene;
	dual.AddRefEntityToScene = DR_AddRefEntityToScene;
	dual.AddPolyToScene = DR_AddPolyToScene;
	dual.AddLightToScene = DR_AddLightToScene;
	dual.RenderScene = DR_RenderScene;
	dual.SetColor = DR_SetColor;
	dual.DrawStretchPic = DR_DrawStretchPic;
	dual.DrawRotatePic = DR_DrawRotatePic;
	dual.DrawRotatePic2 = DR_DrawRotatePic2;
	dual.LAGoggles = DR_LAGoggles;
	dual.Scissor = DR_Scissor;
	dual.DrawStretchRaw = DR_DrawStretchRaw;
	dual.UploadCinematic = DR_UploadCinematic;
	dual.Font_DrawString = DR_Font_DrawString;
	dual.ProcessDissolve = DR_ProcessDissolve;
	dual.InitDissolve = DR_InitDissolve;
	dual.GetScreenShot = DR_GetScreenShot;
	dual.BeginFrame = DR_BeginFrame;
	dual.EndFrame = DR_EndFrame;

	dr_active = qtrue;
	dr_shown = dr_pendingShown = 0;
	dr_split = dr_pendingSplit = qfalse;
	dr_frameSet[0] = 0;
	dr_frameCount = 1;

	return &dual;
}

void CL_DualRef_Shutdown( void )
{
	dr_active = qfalse;
	dr_shown = dr_pendingShown = 0;
	dr_split = dr_pendingSplit = qfalse;
	dr_frameSet[0] = 0;
	dr_frameCount = 1;
}

void CL_SwitchRenderer_f( void )
{
	if ( !dr_active ) {
		Com_Printf( "g_SwitchRenderer: needs g_FastRendererSwitch 1 and a cl_renderer that is not the default, then vid_restart.\n" );
		return;
	}

	dr_pendingShown ^= 1;
	Com_Printf( "Renderer on screen: %s\n", dr_names[dr_pendingShown] );
}

void CL_ShowSplit_f( void )
{
	if ( !dr_active ) {
		Com_Printf( "g_ShowSplit: needs g_FastRendererSwitch 1 and a cl_renderer that is not the default, then vid_restart.\n" );
		return;
	}

	if ( !dr_re[0].CaptureNextFrame || !dr_re[1].CaptureNextFrame ) {
		Com_Printf( "g_ShowSplit: one of the renderers cannot capture its frame.\n" );
		return;
	}

	dr_pendingSplit = (qboolean)!dr_pendingSplit;
	Com_Printf( "Split view %s\n", dr_pendingSplit ? "on" : "off" );
}
