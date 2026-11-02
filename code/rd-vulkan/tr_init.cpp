/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
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

// tr_init.c -- functions that are not called every frame

#include "tr_local.h"

#include <algorithm>
#include <map>
#include <string>
#include "../rd-common/tr_common.h"
#include "tr_WorldEffects.h"
#include "qcommon/MiniHeap.h"
#include "tr_cache.h"
#include "rtx/rtx_light_edit.h"
#include "rtx/rtx_light_emissive.h"
#include "rtx/rtx_light_sky.h"

glconfig_t	glConfig;
glconfigExt_t glConfigExt;
glstate_t	glState;
window_t	window;
glstatic_t	gls;

cvar_t	*r_verbose;
cvar_t	*r_ignore;

cvar_t	*r_detailTextures;

cvar_t	*r_znear;
cvar_t	*r_zproj;

cvar_t	*r_skipBackEnd;

cvar_t	*r_measureOverdraw;

cvar_t	*r_inGameVideo;
cvar_t	*r_fastsky;
cvar_t	*r_drawSun;
cvar_t	*r_dynamiclight;
// rjr - removed for hacking
cvar_t	*r_dlightBacks;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;
cvar_t	*r_autolodscalevalue;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_drawfog;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_visGenerate;
cvar_t	*r_nocull;
cvar_t	*r_facePlaneCull;
cvar_t	*r_cullRoofFaces; //attempted smart method of culling out upwards facing surfaces on roofs for automap shots -rww
cvar_t	*r_roofCullCeilDist; //ceiling distance cull tolerance -rww
cvar_t	*r_roofCullFloorDist; //floor distance cull tolerance -rww
cvar_t	*r_showcluster;
cvar_t	*r_nocurves;

cvar_t	*r_autoMap; //automap renderside toggle for debugging -rww
cvar_t	*r_autoMapBackAlpha; //alpha of automap bg -rww
cvar_t	*r_autoMapDisable; //don't calc it (since it's slow in debug) -rww

cvar_t	*r_dlightStyle;
cvar_t	*r_surfaceSprites;
cvar_t	*r_surfaceWeather;

cvar_t	*r_windSpeed;
cvar_t	*r_windAngle;
cvar_t	*r_windGust;
cvar_t	*r_windDampFactor;
cvar_t	*r_windPointForce;
cvar_t	*r_windPointX;
cvar_t	*r_windPointY;

cvar_t	*r_allowExtensions;

cvar_t	*r_ext_compressed_textures;
cvar_t	*r_ext_compressed_lightmaps;
cvar_t	*r_ext_preferred_tc_method;
cvar_t	*r_ext_gamma_control;
cvar_t	*r_ext_multitexture;
cvar_t	*r_ext_compiled_vertex_array;
cvar_t	*r_ext_texture_env_add;
cvar_t	*r_ext_texture_filter_anisotropic;
cvar_t	*r_gammaShaders;

cvar_t	*r_environmentMapping;

cvar_t	*r_DynamicGlow;
cvar_t	*r_DynamicGlowAllStages;
cvar_t	*r_DynamicGlowPasses;
cvar_t	*r_DynamicGlowDelta;
cvar_t	*r_DynamicGlowIntensity;
cvar_t	*r_DynamicGlowSoft;
cvar_t	*r_DynamicGlowWidth;
cvar_t	*r_DynamicGlowHeight;
cvar_t	*r_DynamicGlowScale;

cvar_t	*r_smartpicmip;
#ifdef USE_VK_PBR
cvar_t	*r_baseNormalX;
cvar_t	*r_baseNormalY;
cvar_t	*r_baseParallax;
cvar_t	*r_baseSpecular;
#endif
#ifdef VK_COMPUTE_NORMALMAP
cvar_t	*r_genNormalMaps;
#endif
#ifdef USE_VK_PBR
cvar_t	*r_normalMapping;
cvar_t	*r_specularMapping;
#ifdef VK_CUBEMAP
cvar_t	*r_cubeMapping;
cvar_t	*r_deluxeMapping;
cvar_t	*r_deluxeSpecular;
#endif
#endif

cvar_t	*r_ignoreGLErrors;
cvar_t	*r_logFile;

cvar_t	*r_primitives;
cvar_t	*r_texturebits;
cvar_t	*r_texturebitslm;

cvar_t	*r_lightmap;
cvar_t	*r_distanceCull;
cvar_t	*r_vertexLight;
cvar_t	*r_uiFullScreen;
cvar_t	*r_shadows;
#ifdef _DEBUG
cvar_t	*r_g2_shadowdebug;	// 1 = traces, 2 = parity map
cvar_t	*r_g2_shadowedges;	// emission filter: bit 0 = the sides, bit 1 = the caps
cvar_t	*r_g2_shadowsurf;	// -1 = every shadow surface, >= 0 = only that surface index
#endif
cvar_t	*r_shadowRange;


cvar_t	*r_flares;
cvar_t	*r_shaderWarnings;
//cvar_t	*r_flareSize;
//cvar_t	*r_flareFade;
//cvar_t	*r_flareCoeff;

cvar_t	*r_nobind;
cvar_t	*r_singleShader;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_showtris;
cvar_t	*r_showsky;
cvar_t	*r_shownormals;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_markcount;
cvar_t	*r_textureMode;
cvar_t	*r_offsetFactor;
cvar_t	*r_offsetUnits;
cvar_t	*r_gamma;
cvar_t	*r_intensity;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;



cvar_t	*r_overBrightBits;
cvar_t	*r_mapOverBrightBits;

cvar_t	*r_debugSurface;
cvar_t	*r_simpleMipMaps;

cvar_t	*r_showImages;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_debugLight;
cvar_t	*r_debugSort;

cvar_t	*r_marksOnTriangleMeshes;

cvar_t	*r_aspectCorrectFonts;
cvar_t	*cl_ratioFix;
cvar_t	*r_patchStitching;

// Vulkan
cvar_t	*r_defaultImage;
cvar_t	*r_device;
cvar_t	*r_swapInterval;
cvar_t	*r_renderThread;
cvar_t	*r_renderThreadSyncLog;
cvar_t	*r_timescale;
cvar_t	*r_stencilbits;
cvar_t	*r_ext_multisample;
cvar_t	*r_ext_supersample;
cvar_t	*r_ext_alpha_to_coverage;
cvar_t	*r_fbo;
cvar_t	*r_hdr;
cvar_t	*r_ext_max_anisotropy;
cvar_t	*r_mapGreyScale;
cvar_t	*r_greyscale;
cvar_t	*r_dither;
cvar_t	*r_presentBits;
cvar_t	*r_bloom;
cvar_t	*r_bloom_threshold;
cvar_t	*r_bloom_intensity;
cvar_t	*r_bloom_threshold_mode;
cvar_t	*r_bloom_modulate;
cvar_t	*r_depthPrepass;
cvar_t	*r_velocityBuffer;
cvar_t	*r_showGBuffer;
cvar_t	*r_distortionStyle;
cvar_t	*r_ssao;
#ifdef USE_RTX
cvar_t	*r_rtx;
cvar_t	*pt_restir;
cvar_t	*pt_caustics;
cvar_t	*pt_dof;
cvar_t	*pt_projection;
cvar_t	*tm_blend_enable;
cvar_t	*pt_debug_poly_lights;
cvar_t	*pt_restir_m_clamp;
cvar_t	*pt_debug_image;
cvar_t	*pt_verbose;
cvar_t	*pt_nrd_max_accum;
cvar_t	*pt_nrd_max_fast_accum;
cvar_t	*pt_nrd_prepass_blur;
cvar_t	*pt_nrd_antifirefly;
cvar_t	*pt_nrd_hitdist_recon;
cvar_t	*pt_nrd_direct;
cvar_t	*pt_nrd_validation;
cvar_t	*pt_accumulation_rendering;
cvar_t	*pt_accumulation_rendering_framenum;
cvar_t	*pt_denoiser;
cvar_t	*pt_dlight_radius;
cvar_t	*pt_dlight_min_dist;
cvar_t	*pt_jobs;
cvar_t	*pt_dlight_lift;
cvar_t	*pt_lightgen_scale;
// Read by the backend: registered here so the backend never calls Cvar_Get.
cvar_t	*pt_light_scale_edit;
cvar_t	*pt_light_scale_dlight;
cvar_t	*pt_light_scale_saber;
cvar_t	*pt_glow_scale;
cvar_t	*pt_light_styles;
cvar_t	*pt_weapon_fx_shaders;
cvar_t	*pt_sky_per_map;
cvar_t	*pt_sky_q3map_sun;
cvar_t	*pt_ambient_floor_scale;
cvar_t	*tm_per_channel;
cvar_t	*tm_contrast;

#define UBO_CVAR_DO( _handle, _value ) cvar_t *sun_##_handle;
	UBO_CVAR_LIST
#undef UBO_CVAR_DO

cvar_t *sun_color[3];
cvar_t *sun_elevation;
cvar_t *sun_azimuth;
cvar_t *sun_angle;
cvar_t *sun_brightness;
cvar_t *sun_bounce;
cvar_t *sun_animate;
cvar_t *sun_gamepad;

cvar_t *sun_preset;
cvar_t *sun_latitude;

cvar_t *physical_sky;
cvar_t *physical_sky_draw_clouds;
cvar_t *physical_sky_space;
cvar_t *physical_sky_brightness;

cvar_t *sky_scattering;
cvar_t *sky_transmittance;
cvar_t *sky_phase_g;
cvar_t *sky_amb_phase_g;

#endif
cvar_t	*r_ssaoRadius;
cvar_t	*r_ssaoIntensity;
cvar_t	*r_ssaoSlices;
cvar_t	*r_ssaoSteps;
cvar_t	*r_contactShadows;
cvar_t	*r_contactShadowLength;
cvar_t	*r_contactShadowThickness;
cvar_t	*r_contactShadowSteps;
cvar_t	*r_contactShadowIntensity;
cvar_t	*r_renderWidth;
cvar_t	*r_renderHeight;
cvar_t	*r_renderScale;
cvar_t	*r_ignorehwgamma;

#ifdef USE_PMLIGHT
cvar_t	*r_dlightMode;
cvar_t	*r_dlightScale;
cvar_t	*r_dlightIntensity;
#endif
cvar_t	*r_dlightSaturation;
cvar_t	*r_roundImagesDown;
cvar_t	*r_nomip;
#ifdef USE_VBO
cvar_t	*r_vbo;
cvar_t	*r_vbo_models;
#endif

// the limits apply to the sum of all scenes in a frame --
// the main view, all the 3D icons, etc
#define	DEFAULT_MAX_POLYS		600
#define	DEFAULT_MAX_POLYVERTS	3000
static cvar_t	*r_maxpolys;
static cvar_t	*r_maxpolyverts;
int		max_polys;
int		max_polyverts;

cvar_t	*r_modelpoolmegs; // unused

/*
Ghoul2 Insert Start
*/
#ifdef _DEBUG
cvar_t	*r_noPrecacheGLA;
#endif

cvar_t	*r_noServerGhoul2;
cvar_t	*r_noGhoul2;
cvar_t	*r_Ghoul2AnimSmooth=0;
cvar_t	*r_Ghoul2UnSqashAfterSmooth=0;
cvar_t	*r_Ghoul2UnSqash;
cvar_t	*r_Ghoul2TimeBase=0;
cvar_t	*r_Ghoul2NoLerp;
cvar_t	*r_Ghoul2NoBlend;
cvar_t	*r_Ghoul2BlendMultiplier=0;
// code/rd-common/tr_font.cpp (shared across all SP renderers) references
// this as extern -- must be defined by whichever renderer DLL links it in.
cvar_t	*com_buildScript;
// Used by R_LoadMDXM (tr_ghoul2.cpp) to precache the per-map cinematic .gla, matching rd-vanilla.
cvar_t	*sv_mapname;

cvar_t	*broadsword=0;
cvar_t	*broadsword_kickbones=0;
cvar_t	*broadsword_kickorigin=0;
cvar_t	*broadsword_playflop=0;
cvar_t	*broadsword_dontstopanim=0;
cvar_t	*broadsword_waitforshot=0;
cvar_t	*broadsword_smallbbox=0;
cvar_t	*broadsword_extra1=0;
cvar_t	*broadsword_extra2=0;

cvar_t	*broadsword_effcorr=0;
cvar_t	*broadsword_ragtobase=0;
cvar_t	*broadsword_dircap=0;

/*
Ghoul2 Insert End
*/

cvar_t *se_language;

cvar_t *r_aviMotionJpegQuality;
cvar_t *r_screenshotJpegQuality;

// Vulkan
#include "vk_local.h"
Vk_Instance vk;
Vk_World	vk_world;

#if 0
#if !defined(__APPLE__)
PFNGLSTENCILOPSEPARATEPROC qglStencilOpSeparate;
#endif

PFNGLACTIVETEXTUREARBPROC qglActiveTextureARB;
PFNGLCLIENTACTIVETEXTUREARBPROC qglClientActiveTextureARB;
PFNGLMULTITEXCOORD2FARBPROC qglMultiTexCoord2fARB;
#if !defined(__APPLE__)
PFNGLTEXIMAGE3DPROC qglTexImage3D;
PFNGLTEXSUBIMAGE3DPROC qglTexSubImage3D;
#endif

PFNGLCOMBINERPARAMETERFVNVPROC qglCombinerParameterfvNV;
PFNGLCOMBINERPARAMETERIVNVPROC qglCombinerParameterivNV;
PFNGLCOMBINERPARAMETERFNVPROC qglCombinerParameterfNV;
PFNGLCOMBINERPARAMETERINVPROC qglCombinerParameteriNV;
PFNGLCOMBINERINPUTNVPROC qglCombinerInputNV;
PFNGLCOMBINEROUTPUTNVPROC qglCombinerOutputNV;

PFNGLFINALCOMBINERINPUTNVPROC qglFinalCombinerInputNV;
PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC qglGetCombinerInputParameterfvNV;
PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC qglGetCombinerInputParameterivNV;
PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC qglGetCombinerOutputParameterfvNV;
PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC qglGetCombinerOutputParameterivNV;
PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC qglGetFinalCombinerInputParameterfvNV;
PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC qglGetFinalCombinerInputParameterivNV;

PFNGLPROGRAMSTRINGARBPROC qglProgramStringARB;
PFNGLBINDPROGRAMARBPROC qglBindProgramARB;
PFNGLDELETEPROGRAMSARBPROC qglDeleteProgramsARB;
PFNGLGENPROGRAMSARBPROC qglGenProgramsARB;
PFNGLPROGRAMENVPARAMETER4DARBPROC qglProgramEnvParameter4dARB;
PFNGLPROGRAMENVPARAMETER4DVARBPROC qglProgramEnvParameter4dvARB;
PFNGLPROGRAMENVPARAMETER4FARBPROC qglProgramEnvParameter4fARB;
PFNGLPROGRAMENVPARAMETER4FVARBPROC qglProgramEnvParameter4fvARB;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC qglProgramLocalParameter4dARB;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC qglProgramLocalParameter4dvARB;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC qglProgramLocalParameter4fARB;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC qglProgramLocalParameter4fvARB;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC qglGetProgramEnvParameterdvARB;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC qglGetProgramEnvParameterfvARB;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC qglGetProgramLocalParameterdvARB;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC qglGetProgramLocalParameterfvARB;
PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB;
PFNGLGETPROGRAMSTRINGARBPROC qglGetProgramStringARB;
PFNGLISPROGRAMARBPROC qglIsProgramARB;

PFNGLLOCKARRAYSEXTPROC qglLockArraysEXT;
PFNGLUNLOCKARRAYSEXTPROC qglUnlockArraysEXT;
#endif

bool g_bTextureRectangleHack = false;

void RE_SetLightStyle( int style, int color );
void RE_GetBModelVerts( int bmodelIndex, vec3_t *verts, vec3_t normal );

void R_Set2DRatio( void ) {
	if (cl_ratioFix->integer)
		tr.widthRatioCoef = ((float)(SCREEN_WIDTH * gls.windowHeight) / (float)(SCREEN_HEIGHT * gls.windowWidth));
	else
		tr.widthRatioCoef = 1.0f;

	if (tr.widthRatioCoef > 1)
		tr.widthRatioCoef = 1.0f;
}

/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
==================
RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with Hunk_FreeTempMemory()
==================
*/

byte *RB_ReadPixels( int x, int y, int width, int height, size_t *offset, int *padlen, int lineAlign )
{
	byte *buffer, *bufstart;
	int bufAlign, linelen;
	int packAlign = 1;

	linelen = width * 3;

	bufAlign = MAX(packAlign, 16); // for SIMD

	// Allocate a few more bytes so that we can choose an alignment we like
	//buffer = Hunk_AllocateTempMemory(padwidth * height + *offset + bufAlign - 1);
	buffer = (byte*)Hunk_AllocateTempMemory(width * height * 4 + *offset + bufAlign - 1);
	bufstart = (byte*)PADP((intptr_t)buffer + *offset, bufAlign);

	vk_read_pixels(bufstart, width, height);

	*offset = bufstart - buffer;
	*padlen = PAD(linelen, packAlign) - linelen;

	return buffer;
}

/*
==================
R_TakeScreenshot
==================
*/
void R_TakeScreenshot( int x, int y, int width, int height, char *fileName ) {
	byte *allbuf, *buffer;
	byte *srcptr, *destptr;
	byte *endline, *endmem;
	byte temp;

	int linelen, padlen;
	size_t offset = 18, memcount;

	allbuf = RB_ReadPixels(x, y, width, height, &offset, &padlen, 0);
	buffer = allbuf + offset - 18;

	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr and remove padding from line endings
	linelen = width * 3;

	srcptr = destptr = allbuf + offset;
	endmem = srcptr + (linelen + padlen) * height;

	while(srcptr < endmem)
	{
		endline = srcptr + linelen;

		while(srcptr < endline)
		{
			temp = srcptr[0];
			*destptr++ = srcptr[2];
			*destptr++ = srcptr[1];
			*destptr++ = temp;

			srcptr += 3;
		}

		// Skip the pad
		srcptr += padlen;
	}

	memcount = linelen * height;

	// gamma correct
	if(glConfig.deviceSupportsGamma && !glConfigExt.doGammaCorrectionWithShaders)
		R_GammaCorrect(allbuf + offset, memcount);

	ri.FS_WriteFile(fileName, buffer, memcount + 18);

	Hunk_FreeTempMemory(allbuf);
}

/*
==================
R_TakeScreenshotPNG
==================
*/
void R_TakeScreenshotPNG( int x, int y, int width, int height, char *fileName ) {
	byte *buffer=NULL;
	size_t offset=0;
	int padlen=0;

	buffer = RB_ReadPixels( x, y, width, height, &offset, &padlen, 0);
	RE_SavePNG( fileName, buffer, width, height, 3 );
	Hunk_FreeTempMemory( buffer );
}

/*
==================
R_TakeScreenshotJPEG
==================
*/
void R_TakeScreenshotJPEG( int x, int y, int width, int height, char *fileName ) {
	byte *buffer;
	size_t offset = 0, memcount;
	int padlen;

	buffer = RB_ReadPixels(x, y, width, height, &offset, &padlen, 0);
	memcount = (width * 3 + padlen) * height;

	// gamma correct
	if(glConfig.deviceSupportsGamma && !glConfigExt.doGammaCorrectionWithShaders)
		R_GammaCorrect(buffer + offset, memcount);

	RE_SaveJPG(fileName, r_screenshotJpegQuality->integer, width, height, buffer + offset, padlen);
	Hunk_FreeTempMemory(buffer);
}

/*
==================
R_ScreenshotFilename
==================
*/
void R_ScreenshotFilename( char *buf, int bufSize, const char *ext ) {
	time_t rawtime;
	char timeStr[32] = {0}; // should really only reach ~19 chars

	time( &rawtime );
	strftime( timeStr, sizeof( timeStr ), "%Y-%m-%d_%H-%M-%S", localtime( &rawtime ) ); // or gmtime

	Com_sprintf( buf, bufSize, "screenshots/shot%s.%s", timeStr, ext );
}

/*
====================
R_LevelShot

levelshots are specialized 256*256 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
#define LEVELSHOTSIZE 256
static void R_LevelShot( void ) {
	char		checkname[MAX_OSPATH];
	byte		*buffer;
	byte		*source, *allsource;
	byte		*src, *dst;
	size_t		offset = 0;
	int			padlen;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	Com_sprintf( checkname, sizeof(checkname), "levelshots/%s.tga", tr.world->baseName );

	allsource = RB_ReadPixels(0, 0, gls.captureWidth, gls.captureHeight, &offset, &padlen, 0);
	source = allsource + offset;

	buffer = (byte *)Hunk_AllocateTempMemory(LEVELSHOTSIZE * LEVELSHOTSIZE*3 + 18);
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = LEVELSHOTSIZE & 255;
	buffer[13] = LEVELSHOTSIZE >> 8;
	buffer[14] = LEVELSHOTSIZE & 255;
	buffer[15] = LEVELSHOTSIZE >> 8;
	buffer[16] = 24;	// pixel size

	// resample from source
	xScale = glConfig.vidWidth / (4.0*LEVELSHOTSIZE);
	yScale = glConfig.vidHeight / (3.0*LEVELSHOTSIZE);
	for ( y = 0 ; y < LEVELSHOTSIZE ; y++ ) {
		for ( x = 0 ; x < LEVELSHOTSIZE ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + 3 * ( glConfig.vidWidth * (int)( (y*3+yy)*yScale ) + (int)( (x*4+xx)*xScale ) );
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * LEVELSHOTSIZE + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correct
	if ( ( tr.overbrightBits > 0 ) && glConfig.deviceSupportsGamma && !glConfigExt.doGammaCorrectionWithShaders ) {
		R_GammaCorrect( buffer + 18, LEVELSHOTSIZE * LEVELSHOTSIZE * 3 );
	}

	ri.FS_WriteFile( checkname, buffer, LEVELSHOTSIZE * LEVELSHOTSIZE*3 + 18 );

	Hunk_FreeTempMemory( buffer );
	Hunk_FreeTempMemory( allsource );

	vk_debug("[skipnotify]Wrote %s\n", checkname );
}

void R_ScreenShot_f ( void ) {
	char checkname[MAX_OSPATH] = {0};
	qboolean silent = qfalse;
	int			typeMask;
	const char *ext;

	if (ri.VK_IsMinimized() && !R_CanMinimize()) {
		ri.Printf(PRINT_WARNING, "WARNING: unable to take screenshot when minimized because FBO is not available/enabled.\n");
		return;
	}

	if ( !strcmp( ri.Cmd_Argv(1), "levelshot" ) ) {
		R_LevelShot();
		return;
	}

	if (Q_stricmp(ri.Cmd_Argv(0), "screenshot_tga") == 0) {
		typeMask = SCREENSHOT_TGA;
		ext = "tga";
	}
	else if (Q_stricmp(ri.Cmd_Argv(0), "screenshot_png") == 0) {
		typeMask = SCREENSHOT_PNG;
		ext = "png";
	}
	else {
		typeMask = SCREENSHOT_JPG;
		ext = "jpg";
	}

	// check if already scheduled
	if (backEnd.screenshotMask & typeMask)
		return;

	if ( !strcmp( ri.Cmd_Argv(1), "silent" ) )
		silent = qtrue;

	if ( ri.Cmd_Argc() == 2 && !silent ) {
		// explicit filename
		Com_sprintf( checkname, sizeof( checkname ), "screenshots/%s.%s", ri.Cmd_Argv( 1 ), ext );
	}
	else {
		// timestamp the filename
		R_ScreenshotFilename( checkname, sizeof( checkname ), ext );

		if ( ri.FS_FileExists( checkname ) ) {
			vk_debug("ScreenShot: Couldn't create a file\n" );
			return;
 		}
	}

	// we will make the screenshot right at the end of RE_EndFrame()
	backEnd.screenshotMask |= typeMask;
	if (typeMask == SCREENSHOT_JPG) {
		backEnd.screenShotJPGsilent = silent;
		Q_strncpyz(backEnd.screenshotJPG, checkname, sizeof(backEnd.screenshotJPG));
	}
	else if (typeMask == SCREENSHOT_PNG) {
		backEnd.screenShotPNGsilent = silent;
		Q_strncpyz(backEnd.screenshotPNG, checkname, sizeof(backEnd.screenshotPNG));
	}
	else {
		backEnd.screenShotTGAsilent = silent;
		Q_strncpyz(backEnd.screenshotTGA, checkname, sizeof(backEnd.screenshotTGA));
	}
}

/*
==================
RB_TakeVideoFrameCmd
==================
*/
const void *RB_TakeVideoFrameCmd( const void *data )
{
	// SP has no video (AVI) capture pipeline -- refimport_t exposes neither
	// CL_WriteAVIVideoFrame nor the 7-arg RE_SaveJPGToBuffer this needs, and
	// rd-vanilla/rd-rend2 have no equivalent of this command at all.
	const videoFrameCommand_t *cmd = (const videoFrameCommand_t*)data;
	return (const void*)(cmd + 1);
}

void R_RemapSkyShader_f ( void ) {
	int num;

	if (ri.Cmd_Argc() != 2 || !strlen(ri.Cmd_Argv(1))) {
		vk_debug("Usage: /remapSky <new>\n");
		return;
	}

	for (num = 0; num < tr.numShaders; num++) {
		if (tr.shaders[num]->sky)
		{
			R_RemapShader(tr.shaders[num]->name, ri.Cmd_Argv(1), NULL);
		}
	}
}

void R_ClearRemaps_f( void ) {
	int num;

	for (num = 0; num < tr.numShaders; num++) {
		tr.shaders[num]->remappedShader = NULL;
	}
}

/*
==================
R_FogDistance_f

Prints or sets the opacity distance of the global fog.
==================
*/
static void R_FogDistance_f( void ) {
	fog_t	*fog;
	float	distance;

	if ( !tr.world ) {
		ri.Printf( PRINT_ALL, "R_FogDistance_f: World is not initialized\n" );
		return;
	}

	if ( tr.world->globalFog == -1 ) {
		ri.Printf( PRINT_ALL, "R_FogDistance_f: World does not have a global fog\n" );
		return;
	}

	fog = &tr.world->fogs[tr.world->globalFog];

	if ( ri.Cmd_Argc() <= 1 ) {
		distance = 1.0f / ( 8.0f * fog->tcScale );
		ri.Printf( PRINT_ALL, "R_FogDistance_f: Current Distance: %.0f\n", distance );
		return;
	}

	if ( ri.Cmd_Argc() != 2 ) {
		ri.Printf( PRINT_ALL, "R_FogDistance_f: Invalid number of arguments to set distance\n" );
		return;
	}

	distance = atof( ri.Cmd_Argv( 1 ) );
	if ( distance < 1.0f ) {
		distance = 1.0f;
	}

	// vk_update_fog_constants reads depthForOpaque every frame; tcScale feeds the CPU fog paths.
	fog->parms.depthForOpaque = distance;
	fog->tcScale = 1.0f / ( distance * 8 );
}

/*
==================
R_FogColor_f

Prints or sets the color of the global fog.
==================
*/
static void R_FogColor_f( void ) {
	fog_t	*fog;
	int		n;

	if ( !tr.world ) {
		ri.Printf( PRINT_ALL, "R_FogColor_f: World is not initialized\n" );
		return;
	}

	if ( tr.world->globalFog == -1 ) {
		ri.Printf( PRINT_ALL, "R_FogColor_f: World does not have a global fog\n" );
		return;
	}

	fog = &tr.world->fogs[tr.world->globalFog];

	if ( ri.Cmd_Argc() <= 1 ) {
		unsigned	i = fog->colorInt;

		ri.Printf( PRINT_ALL, "R_FogColor_f: Current Color: %0f %0f %0f\n",
			( (byte *)&i )[0] / 255.0,
			( (byte *)&i )[1] / 255.0,
			( (byte *)&i )[2] / 255.0 );
		return;
	}

	if ( ri.Cmd_Argc() != 4 ) {
		ri.Printf( PRINT_ALL, "R_FogColor_f: Invalid number of arguments to set color\n" );
		return;
	}

	fog->parms.color[0] = atof( ri.Cmd_Argv( 1 ) );
	fog->parms.color[1] = atof( ri.Cmd_Argv( 2 ) );
	fog->parms.color[2] = atof( ri.Cmd_Argv( 3 ) );
	fog->colorInt = ColorBytes4( fog->parms.color[0] * tr.identityLight,
								 fog->parms.color[1] * tr.identityLight,
								 fog->parms.color[2] * tr.identityLight, 1.0f );

	// The fog uniform and the clear color read the float copy.
	for ( n = 0; n < 4; n++ ) {
		fog->color[n] = ( ( fog->colorInt >> ( n * 8 ) ) & 255 ) / 255.0f;
	}
}

extern void R_ReloadFonts_f( void );

typedef struct consoleCommand_s {
	const char	*cmd;
	xcommand_t	func;
} consoleCommand_t;

static consoleCommand_t	commands[] = {
	{ "imagelist",			R_ImageList_f },
	{ "shaderlist",			R_ShaderList_f },
	{ "skinlist",			R_SkinList_f },
	{ "fontlist",			R_FontList_f },
	{ "screenshot",			R_ScreenShot_f },
	{ "screenshot_png",		R_ScreenShot_f },
	{ "screenshot_tga",		R_ScreenShot_f },
	{ "r_capturePortrait",	R_CapturePortrait_f },
	{ "gfxinfo",			GfxInfo_f },
	{ "r_we",				R_WorldEffect_f },
	{ "imagecacheinfo",		RE_RegisterImages_Info_f },
	{ "modellist",			R_Modellist_f },
	{ "modelcacheinfo",		RE_RegisterModels_Info_f },
	{ "r_fogDistance",		R_FogDistance_f },
	{ "r_fogColor",			R_FogColor_f },
	{ "r_reloadfonts",		R_ReloadFonts_f },
	{ "ph_reload",			G2Phys_Reload_f },
	{ "r_cleardecals",		RE_ClearDecals },
	{ "remapSky",			R_RemapSkyShader_f },
	{ "clearRemaps",		R_ClearRemaps_f },
#ifdef USE_RTX
	{ "show_pvs",			vk_rtx_show_pvs_f },
	{ "pt_images",			vk_rtx_list_debug_images_f },
	{ "pt_lightgen",		R_LightGen_f },
	{ "pt_ledit_list",		RTX_LightEdit_List_f },
	{ "pt_ledit_add",		RTX_LightEdit_Add_f },
	{ "pt_ledit_set",		RTX_LightEdit_Set_f },
	{ "pt_ledit_del",		RTX_LightEdit_Del_f },
	{ "pt_ledit_restore",	RTX_LightEdit_Restore_f },
	{ "pt_ledit_stats",		RTX_LightEdit_Stats_f },
	{ "pt_ledit_mute",		RTX_LightEdit_Mute_f },
	{ "pt_ledit_solo",		RTX_LightEdit_Solo_f },
	{ "pt_ledit_compact",	RTX_LightEdit_Compact_f },
	{ "pt_ledit_emissive",	RTX_LightEdit_Emissive_f },
	{ "pt_ledit_dynamic",	RTX_LightEdit_Dynamic_f },
	{ "pt_ledit_emissive_shaders",	RTX_LightEdit_EmissiveShaders_f },
	{ "pt_ledit_emissive_scale",	RTX_LightEdit_EmissiveScale_f },
	{ "pt_sky_print",		RTX_LightSky_Print_f },
	{ "pt_sky_set",			RTX_LightSky_Set_f },
	{ "pt_sky_reset",		RTX_LightSky_Reset_f },
#endif
	{ "vkinfo",				vk_info_f }
};

static const size_t numCommands = ARRAY_LEN( commands );

#ifdef _DEBUG
#define MIN_PRIMITIVES -1
#else
#define MIN_PRIMITIVES 0
#endif
#define MAX_PRIMITIVES 3

/*
===============
R_Register
===============
*/
void RTX_ParallelFor( int count, int grain, void (*fn)( int begin, int end, void *ctx ), void *ctx )
{
	if ( pt_jobs && pt_jobs->integer && ri.Jobs_ParallelFor )
		ri.Jobs_ParallelFor( count, grain, fn, ctx );
	else if ( count > 0 )
		fn( 0, count, ctx );
}

void R_Register( void )
{
	//FIXME: lol badness
	se_language = Cvar_Get("se_language", "english", CVAR_ARCHIVE | CVAR_NORESTART, "");
	//
	// latched and archived variables
	//
	r_allowExtensions					= Cvar_Get( "r_allowExtensions",					"1",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_ext_compressed_textures			= Cvar_Get( "r_ext_compress_textures",			"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_ext_compressed_lightmaps			= Cvar_Get( "r_ext_compress_lightmaps",			"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_ext_preferred_tc_method			= Cvar_Get( "r_ext_preferred_tc_method",			"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_ext_gamma_control					= Cvar_Get( "r_ext_gamma_control",				"1",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_ext_multitexture					= Cvar_Get( "r_ext_multitexture",				"1",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_ext_compiled_vertex_array			= Cvar_Get( "r_ext_compiled_vertex_array",		"1",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_ext_texture_env_add				= Cvar_Get( "r_ext_texture_env_add",				"1",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_ext_texture_filter_anisotropic	= Cvar_Get( "r_ext_texture_filter_anisotropic",	"16",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_gammaShaders						= Cvar_Get( "r_gammaShaders",					"1",						CVAR_ARCHIVE_ND|CVAR_LATCH, "Set gamma using pixel shaders inside the game window only." );
	r_environmentMapping				= Cvar_Get( "r_environmentMapping",				"1",						CVAR_ARCHIVE_ND, "" );
	r_DynamicGlow						= Cvar_Get( "r_DynamicGlow",						"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "Enable dynamic glow effect\nRequires " S_COLOR_CYAN "\\r_fbo 1" );
	r_DynamicGlowAllStages				= Cvar_Get( "r_DynamicGlowAllStages",			"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "In Vanilla certain glow stages are skipped, render those anyway" );
	r_DynamicGlowPasses					= Cvar_Get( "r_DynamicGlowPasses",				"5",						CVAR_ARCHIVE_ND, "" );
	r_DynamicGlowDelta					= Cvar_Get( "r_DynamicGlowDelta",				"0.8f",						CVAR_ARCHIVE_ND, "" );
	r_DynamicGlowIntensity				= Cvar_Get( "r_DynamicGlowIntensity",			"1.13f",					CVAR_ARCHIVE_ND, "" );
	r_DynamicGlowSoft					= Cvar_Get( "r_DynamicGlowSoft",					"1",						CVAR_ARCHIVE_ND, "" );
	r_DynamicGlowWidth					= Cvar_Get( "r_DynamicGlowWidth",				"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_DynamicGlowHeight					= Cvar_Get( "r_DynamicGlowHeight",				"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_DynamicGlowScale					= Cvar_Get( "r_DynamicGlowScale",				"0.25",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_picmip							= Cvar_Get( "r_picmip",							"0",						CVAR_ARCHIVE|CVAR_LATCH, "" );
	ri.Cvar_CheckRange( r_picmip, 0, 16, qtrue );
	r_smartpicmip						= Cvar_Get( "r_smartpicmip",						"1",						CVAR_ARCHIVE_ND|CVAR_LATCH, "Applies r_picmip setting to map textures only." );
#ifdef USE_VK_PBR
	r_baseNormalX						= Cvar_Get( "r_baseNormalX",						"1.0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "Scale of the x of the normal maps." );
	r_baseNormalY						= Cvar_Get( "r_baseNormalY",						"1.0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "Scale of the y of the normal maps." );
	r_baseParallax						= Cvar_Get( "r_baseParallax",						"0.05",						CVAR_ARCHIVE_ND|CVAR_LATCH, "Depth of the parallax of the normal height maps." );
	r_baseSpecular						= Cvar_Get( "r_baseSpecular",						"0.04",						CVAR_ARCHIVE_ND|CVAR_LATCH, "Specular of the stages without physical map." );
#endif
#ifdef VK_COMPUTE_NORMALMAP
	r_genNormalMaps						= Cvar_Get( "r_genNormalMaps",						"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "Approximate normal maps from baked diffuse (albedo) textures" );
#endif
#ifdef USE_VK_PBR
	r_normalMapping = Cvar_Get( "r_normalMapping", "0", CVAR_ARCHIVE_ND | CVAR_LATCH, "PBR shading of the lit stages with their normal maps" );
	r_specularMapping = Cvar_Get( "r_specularMapping", "0", CVAR_ARCHIVE_ND | CVAR_LATCH, "PBR shading of the lit stages with their specular / physical maps" );
#ifdef VK_CUBEMAP
	r_deluxeMapping = Cvar_Get( "r_deluxeMapping", "1", CVAR_ARCHIVE_ND | CVAR_LATCH, "Light direction of the PBR lightmap stages from the deluxe maps of maps compiled with q3map2 -deluxe" );
	r_deluxeSpecular = Cvar_Get( "r_deluxeSpecular", "1", CVAR_ARCHIVE_ND | CVAR_LATCH, "Scale of the specular light of the stages with a deluxe map" );
	r_cubeMapping = Cvar_Get( "r_cubeMapping", "0", CVAR_ARCHIVE_ND | CVAR_LATCH, "Reflections of the PBR stages from the cubemaps of the map probes" );
#endif
#endif
	r_colorMipLevels					= Cvar_Get( "r_colorMipLevels",					"0",						CVAR_LATCH, "" );
	r_detailTextures					= Cvar_Get( "r_detailtextures",					"1",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_texturebits						= Cvar_Get( "r_texturebits",						"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_texturebitslm						= Cvar_Get( "r_texturebitslm",					"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_overBrightBits					= Cvar_Get( "r_overBrightBits",					"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_mapOverBrightBits					= Cvar_Get( "r_mapOverBrightBits",				"0",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_simpleMipMaps						= Cvar_Get( "r_simpleMipMaps",					"1",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	r_vertexLight						= Cvar_Get( "r_vertexLight",						"0",						CVAR_ARCHIVE|CVAR_LATCH, "" );
	r_uiFullScreen						= Cvar_Get( "r_uifullscreen",					"0",						CVAR_NONE, "" );
	r_subdivisions						= Cvar_Get( "r_subdivisions",					"4",						CVAR_ARCHIVE_ND|CVAR_LATCH, "" );
	ri.Cvar_CheckRange( r_subdivisions, 0, 80, qfalse );

	r_fullbright						= Cvar_Get( "r_fullbright",						"0",						CVAR_ARCHIVE_ND, "" );
	r_intensity							= Cvar_Get( "r_intensity",						"1",						CVAR_LATCH, "" );
	r_singleShader						= Cvar_Get( "r_singleShader",					"0",						CVAR_CHEAT|CVAR_LATCH, "" );
	r_lodCurveError						= Cvar_Get( "r_lodCurveError",					"250",						CVAR_ARCHIVE_ND, "" );
	r_lodbias							= Cvar_Get( "r_lodbias",							"0",						CVAR_ARCHIVE_ND, "" );
	r_autolodscalevalue					= Cvar_Get( "r_autolodscalevalue",				"0",						CVAR_ROM, "" );

	r_shaderWarnings					= Cvar_Get( "r_shaderWarnings",					"1",						CVAR_ARCHIVE_ND, "print a warning for a shader that does not load: bit 1 a script that fails to parse, bit 2 no script and no image" );
	r_flares							= Cvar_Get( "r_flares",							"1",						CVAR_ARCHIVE_ND, "" );
	//r_flareSize							= Cvar_Get( "r_flareSize",						"40",						CVAR_ARCHIVE_ND, "" );
	//r_flareFade							= Cvar_Get( "r_flareFade",						"10",						CVAR_ARCHIVE_ND, "" );
	//r_flareCoeff						= Cvar_Get( "r_flareCoeff",						"150",						CVAR_ARCHIVE_ND, "" );
	//ri.Cvar_CheckRange(r_flareCoeff, 0.1f, 250, qfalse);

	r_znear								= Cvar_Get( "r_znear",							"4",						CVAR_ARCHIVE_ND, "" );
	ri.Cvar_CheckRange( r_znear, 0.001f, 10, qfalse );
	r_zproj								= Cvar_Get("r_zproj",							"64",						CVAR_ARCHIVE_ND, "" );
	r_ignoreGLErrors					= Cvar_Get( "r_ignoreGLErrors",					"1",						CVAR_ARCHIVE_ND, "" );
	r_fastsky							= Cvar_Get( "r_fastsky",							"0",						CVAR_ARCHIVE_ND, "" );
	r_inGameVideo						= Cvar_Get( "r_inGameVideo",						"1",						CVAR_ARCHIVE_ND, "" );
	r_drawSun							= Cvar_Get( "r_drawSun",							"0",						CVAR_ARCHIVE_ND, "" );
	r_dynamiclight						= Cvar_Get( "r_dynamiclight",					"1",						CVAR_ARCHIVE, "" );
	// rjr - removed for hacking
	r_dlightBacks						= Cvar_Get( "r_dlightBacks",						"1",						CVAR_ARCHIVE_ND, "dlight non-facing surfaces for continuity" );
	r_finish							= Cvar_Get( "r_finish",							"0",						CVAR_ARCHIVE_ND, "" );
	r_textureMode						= Cvar_Get( "r_textureMode",						"GL_LINEAR_MIPMAP_LINEAR",	CVAR_ARCHIVE, "" );
	r_markcount							= Cvar_Get( "r_markcount",						"100",						CVAR_ARCHIVE_ND, "" );
	r_gamma								= Cvar_Get( "r_gamma",							"1",						CVAR_ARCHIVE_ND, "" );
	r_facePlaneCull						= Cvar_Get( "r_facePlaneCull",					"1",						CVAR_ARCHIVE_ND, "" );
	r_cullRoofFaces						= Cvar_Get( "r_cullRoofFaces",					"0",						CVAR_CHEAT, "" ); //attempted smart method of culling out upwards facing surfaces on roofs for automap shots -rww
	r_roofCullCeilDist					= Cvar_Get( "r_roofCullCeilDist",				"256",						CVAR_CHEAT, "" ); //attempted smart method of culling out upwards facing surfaces on roofs for automap shots -rww
	r_roofCullFloorDist					= Cvar_Get( "r_roofCeilFloorDist",				"128",						CVAR_CHEAT, "" ); //attempted smart method of culling out upwards facing surfaces on roofs for automap shots -rww
	r_primitives						= Cvar_Get( "r_primitives",						"0",						CVAR_ARCHIVE_ND, "" );
	ri.Cvar_CheckRange( r_primitives, MIN_PRIMITIVES, MAX_PRIMITIVES, qtrue );
	r_ambientScale						= Cvar_Get( "r_ambientScale",					"0.6",						CVAR_NONE, "" );
	r_directedScale						= Cvar_Get( "r_directedScale",					"1",						CVAR_NONE, "" );
	r_autoMap							= Cvar_Get( "r_autoMap",							"0",						CVAR_ARCHIVE_ND, "" ); //automap renderside toggle for debugging -rww
	r_autoMapBackAlpha					= Cvar_Get( "r_autoMapBackAlpha",				"0",						CVAR_NONE, "" ); //alpha of automap bg -rww
	r_autoMapDisable					= Cvar_Get( "r_autoMapDisable",					"1",						CVAR_NONE, "" );
	r_showImages						= Cvar_Get( "r_showImages",						"0",						CVAR_CHEAT, "" );
	r_debugLight						= Cvar_Get( "r_debuglight",						"0",						CVAR_TEMP, "" );
	r_debugSort							= Cvar_Get( "r_debugSort",						"0",						CVAR_CHEAT, "" );
	r_dlightStyle						= Cvar_Get( "r_dlightStyle",						"1",						CVAR_TEMP, "" );
	r_surfaceSprites					= Cvar_Get( "r_surfaceSprites",					"1",						CVAR_ARCHIVE_ND | CVAR_LATCH, "" );
	r_surfaceWeather					= Cvar_Get( "r_surfaceWeather",					"0",						CVAR_TEMP, "" );
	r_windSpeed							= Cvar_Get( "r_windSpeed",						"0",						CVAR_NONE, "" );
	r_windAngle							= Cvar_Get( "r_windAngle",						"0",						CVAR_NONE, "" );
	r_windGust							= Cvar_Get( "r_windGust",						"0",						CVAR_NONE, "" );
	r_windDampFactor					= Cvar_Get( "r_windDampFactor",					"0.1",						CVAR_NONE, "" );
	r_windPointForce					= Cvar_Get( "r_windPointForce",					"0",						CVAR_NONE, "" );
	r_windPointX						= Cvar_Get( "r_windPointX",						"0",						CVAR_NONE, "" );
	r_windPointY						= Cvar_Get( "r_windPointY",						"0",						CVAR_NONE, "" );
	r_nocurves							= Cvar_Get( "r_nocurves",						"0",						CVAR_CHEAT, "" );
	r_drawworld							= Cvar_Get( "r_drawworld",						"1",						CVAR_CHEAT, "" );
	r_drawfog							= Cvar_Get("r_drawfog",							"2",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Fog mode\n"
		" 0 - disabled\n"
		" 1 - legacy fog\n"
		" 2 - \"hardware\" fog + collapse\n"
		" 3 - legacy fog + collapse\n");
	r_lightmap							= Cvar_Get( "r_lightmap",						"0",						CVAR_ARCHIVE_ND, "" );
	r_distanceCull						= Cvar_Get( "r_distanceCull",					"0",						CVAR_ARCHIVE_ND, "" );
	r_portalOnly						= Cvar_Get( "r_portalOnly",						"0",						CVAR_CHEAT, "" );
	r_skipBackEnd						= Cvar_Get( "r_skipBackEnd",						"0",						CVAR_CHEAT, "" );
	r_measureOverdraw					= Cvar_Get( "r_measureOverdraw",					"0",						CVAR_NONE, "" );
	r_lodscale							= Cvar_Get( "r_lodscale",						"10",						CVAR_ARCHIVE_ND, "" );
	r_norefresh							= Cvar_Get( "r_norefresh",						"0",						CVAR_CHEAT, "" );
	r_drawentities						= Cvar_Get( "r_drawentities",					"1",						CVAR_CHEAT, "" );
	r_ignore							= Cvar_Get( "r_ignore",							"1",						CVAR_CHEAT, "" );
	r_nocull							= Cvar_Get( "r_nocull",							"0",						CVAR_CHEAT, "" );
	r_novis								= Cvar_Get( "r_novis",							"0",						CVAR_CHEAT, "" );
	r_visGenerate						= Cvar_Get( "r_visGenerate",					"1",						CVAR_ARCHIVE_ND, "Generate a PVS for a map without a vis lump, cached in maps/<name>.vis" );
	r_showcluster						= Cvar_Get( "r_showcluster",						"0",						CVAR_CHEAT, "" );
	r_speeds							= Cvar_Get( "r_speeds",							"0",						CVAR_CHEAT, "" );
	r_verbose							= Cvar_Get( "r_verbose",							"0",						CVAR_CHEAT, "" );
	r_logFile							= Cvar_Get( "r_logFile",							"0",						CVAR_CHEAT, "" );
	r_debugSurface						= Cvar_Get( "r_debugSurface",					"0",						CVAR_CHEAT, "" );
	r_nobind							= Cvar_Get( "r_nobind",							"0",						CVAR_CHEAT, "" );
	r_showtris							= Cvar_Get( "r_showtris",						"0",						CVAR_NONE, "" );
	r_showsky							= Cvar_Get( "r_showsky",							"0",						CVAR_CHEAT, "" );
	r_shownormals						= Cvar_Get( "r_shownormals",						"0",						CVAR_CHEAT, "" );
	r_clear								= Cvar_Get( "r_clear",							"0",						CVAR_CHEAT, "" );
	r_offsetFactor						= Cvar_Get( "r_offsetfactor",					"-1",						CVAR_CHEAT, "" );
	r_offsetUnits						= Cvar_Get( "r_offsetunits",						"-2",						CVAR_CHEAT, "" );
	r_lockpvs							= Cvar_Get( "r_lockpvs",							"0",						CVAR_CHEAT, "" );
	r_noportals							= Cvar_Get( "r_noportals",						"0",						CVAR_NONE, "" );
	r_shadows							= Cvar_Get( "cg_shadows",						"1",						CVAR_NONE, "" );
#ifdef _DEBUG
	r_g2_shadowdebug					= Cvar_Get( "r_g2_shadowdebug",				"0",						CVAR_TEMP, "" );
	r_g2_shadowedges					= Cvar_Get( "r_g2_shadowedges",				"3",						CVAR_TEMP, "" );
	r_g2_shadowsurf						= Cvar_Get( "r_g2_shadowsurf",				"-1",						CVAR_TEMP, "" );
#endif
	r_shadowRange						= Cvar_Get( "r_shadowRange",						"1000",						CVAR_NONE, "" );
	r_marksOnTriangleMeshes				= Cvar_Get( "r_marksOnTriangleMeshes",			"0",						CVAR_ARCHIVE_ND, "" );
	r_aspectCorrectFonts				= Cvar_Get( "r_aspectCorrectFonts",				"0",						CVAR_ARCHIVE, "" );
	cl_ratioFix							= Cvar_Get( "cl_ratioFix",						"1",						CVAR_ARCHIVE, "" );
	r_patchStitching					= Cvar_Get( "r_patchStitching",					"1",						CVAR_ARCHIVE, "Enable stitching of neighbouring patch surfaces" );
	r_maxpolys							= Cvar_Get( "r_maxpolys",						XSTRING( DEFAULT_MAX_POLYS ),		CVAR_NONE, "" );
	r_maxpolyverts						= Cvar_Get( "r_maxpolyverts",					XSTRING( DEFAULT_MAX_POLYVERTS ),	CVAR_NONE, "" );

	// Read by the backend: registered here so the backend never looks a cvar up by name.
	r_swapInterval						= ri.Cvar_Get("r_swapInterval",					"0",						CVAR_NONE);
	r_timescale							= ri.Cvar_Get("timescale",						"1",						CVAR_NONE);

	r_renderThread						= Cvar_Get( "r_renderThread",					"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Run the back end on its own thread.\nApplies after vid_restart." );
	r_renderThreadSyncLog				= Cvar_Get( "r_renderThreadSyncLog",			"0",						CVAR_NONE, "Print the places that wait for the render thread,\nevery com_perflogFrames frames." );

	// Vulkan
	r_defaultImage						= Cvar_Get("r_defaultImage",						"",							CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	r_device							= Cvar_Get("r_device",							"-1",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Select physical device to render:\n" \
		" 0+ - use explicit device index\n" \
		" -1 - first discrete GPU\n" \
		" -2 - first integrated GPU");
	ri.Cvar_CheckRange(r_device, -2, 8, qtrue);
	r_device->modified					= qfalse;

	r_stencilbits						= Cvar_Get("r_stencilbits",						"8",						CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	r_ext_multisample					= Cvar_Get("r_ext_multisample",					"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	ri.Cvar_CheckRange(r_ext_multisample, 0, 64, qtrue);
	r_ext_supersample					= Cvar_Get("r_ext_supersample",					"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	ri.Cvar_CheckRange(r_ext_supersample, 0, 1, qtrue);
	r_ext_alpha_to_coverage				= Cvar_Get("r_ext_alpha_to_coverage",			"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	ri.Cvar_CheckRange(r_ext_alpha_to_coverage, 0, 1, qtrue);
	r_fbo								= Cvar_Get("r_fbo",								"1",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Render to an offscreen buffer. Dynamic glow, bloom, ambient occlusion, supersampling and cubemaps need it");
	r_hdr								= Cvar_Get("r_hdr",								"1",						CVAR_ARCHIVE | CVAR_LATCH, "Colour buffer format: 0 = 8-bit, 1 = 16-bit precision. Both clamp at white; a float target would break the destination-reading blend modes this renderer uses - see get_hdr_format()");
	ri.Cvar_CheckRange(r_hdr, -1, 1, qtrue);
	r_mapGreyScale						= Cvar_Get("r_mapGreyScale",						"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	ri.Cvar_CheckRange(r_mapGreyScale, -1, 1, qfalse);
	r_ext_max_anisotropy				= Cvar_Get("r_ext_max_anisotropy",				"2",						CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	ri.Cvar_CheckRange(r_ext_max_anisotropy, 1, 16, qtrue);
	r_greyscale							= Cvar_Get("r_greyscale",						"0",						CVAR_ARCHIVE_ND, "");
	ri.Cvar_CheckRange(r_greyscale, -1, 1, qfalse);
	r_dither							= Cvar_Get("r_dither",							"0",						CVAR_ARCHIVE_ND, "Set dithering mode:\n 0 - disabled\n 1 - ordered\nRequires " S_COLOR_CYAN "\\r_fbo 1");
	ri.Cvar_CheckRange(r_dither, 0, 1, qtrue);
	r_presentBits						= Cvar_Get("r_presentBits",						"24",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Select color bits used for presentation surfaces\nRequires " S_COLOR_CYAN "\\r_fbo 1");
	ri.Cvar_CheckRange(r_presentBits, 16, 30, qtrue);
	r_bloom								= Cvar_Get("r_bloom",							"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Enable bloom effect\nRequires " S_COLOR_CYAN "\\r_fbo 1");
	ri.Cvar_CheckRange(r_bloom, 0, 1, qtrue);
	r_bloom_threshold					= Cvar_Get("r_bloom_threshold",					"0.05",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Color level to extract to bloom texture, default is 0.05");
	ri.Cvar_CheckRange(r_bloom_threshold, 0.01f, 1, qfalse);
	r_bloom_threshold_mode				= Cvar_Get("r_bloom_threshold_mode",				"0",						CVAR_ARCHIVE_ND, "Color extraction mode:\n 0: (r|g|b) >= threshold\n 1: (r + g + b ) / 3 >= threshold\n 2: luma(r, g, b) >= threshold");
	r_bloom_intensity					= Cvar_Get("r_bloom_intensity",					"0.15",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Final bloom blend factor, default is 0.15");
	ri.Cvar_CheckRange(r_bloom_intensity, 0.01f, 2, qfalse);
	r_bloom_modulate					= Cvar_Get("r_bloom_modulate",					"0",						CVAR_ARCHIVE_ND, "Modulate extracted color:\n 0: off (color = color, i.e. no changes)\n 1: by itself (color = color * color)\n 2: by intensity (color = color * luma(color))");
	r_depthPrepass						= Cvar_Get("r_depthPrepass",						"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Render a depth+normal G-buffer extraction pass ahead of the main pass, for use by later screen-space techniques\nRequires " S_COLOR_CYAN "\\r_fbo 1");
	ri.Cvar_CheckRange(r_depthPrepass, 0, 1, qtrue);
	r_velocityBuffer					= Cvar_Get("r_velocityBuffer",					"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Add a per-pixel screen-space motion vector attachment to the G-buffer extraction pass\nRequires " S_COLOR_CYAN "\\r_depthPrepass 1");
	ri.Cvar_CheckRange(r_velocityBuffer, 0, 1, qtrue);
	r_showGBuffer						= Cvar_Get("r_showGBuffer",						"0",						CVAR_CHEAT, "Show the G-buffer extraction pass full-screen instead of the scene: 1 = depth, 2 = normal, 3 = motion vectors, 4 = ambient occlusion, 5 = contact shadows. Requires r_depthPrepass 1");
	ri.Cvar_CheckRange(r_showGBuffer, 0, 5, qtrue);
	r_distortionStyle					= Cvar_Get("r_distortionStyle",					"1",						CVAR_ARCHIVE_ND, "How RF_DISTORTION effects are drawn (force push/pull): 0 = per-pixel refraction, 1 = SP's screen-region capture");
	ri.Cvar_CheckRange(r_distortionStyle, 0, 1, qtrue);
	r_ssao								= Cvar_Get("r_ssao",							"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Screen-space ambient occlusion over the G-buffer: 0 = off, 1 = hemisphere SSAO (cheaper, blunter), 2 = GTAO (horizon search, cosine-weighted). Requires r_depthPrepass 1");
	ri.Cvar_CheckRange(r_ssao, 0, 2, qtrue);
#ifdef USE_RTX
	r_rtx								= ri.Cvar_Get("r_rtx",								"0",	CVAR_ARCHIVE | CVAR_LATCH);
	pt_restir							= ri.Cvar_Get("pt_restir",							"1",	CVAR_NONE);
	pt_caustics							= ri.Cvar_Get("pt_caustics",						"1",	CVAR_NONE);
	pt_dof								= ri.Cvar_Get("pt_dof",								"0",	CVAR_NONE);
	pt_projection						= ri.Cvar_Get("pt_projection",						"0",	CVAR_NONE);
	tm_blend_enable						= ri.Cvar_Get("tm_blend_enable",					"1",	CVAR_NONE);
	pt_debug_poly_lights				= ri.Cvar_Get("pt_debug_poly_lights",				"0",	CVAR_NONE);
	/* Note: Higher values results in pixel having higher correlation between frames;
	 * however, this can work against the denoiser, as it's temporal filtering would
	 * really likes pixels that vary over time... */
	pt_restir_m_clamp					= ri.Cvar_Get("pt_restir_m_clamp",					"8",	CVAR_NONE);
	pt_debug_image						= ri.Cvar_Get("pt_debug_image",						"0",	CVAR_NONE);
	pt_verbose							= ri.Cvar_Get("pt_verbose",							"0",	CVAR_NONE);
	pt_accumulation_rendering			= ri.Cvar_Get("pt_accumulation_rendering",			"0",	CVAR_NONE);
	pt_accumulation_rendering_framenum	= ri.Cvar_Get("pt_accumulation_rendering_framenum",	"500",	CVAR_NONE);
	// 0 none, 1 A-SVGF, 2 NRD ReLAX, 3 NRD ReBLUR
	// The pt_nrd_* cvars tune NRD (pt_denoiser 2 and 3). They are read every frame.
	// Frames of history, frames of the fast history, radius of the pre-pass blur of the diffuse in pixels (the specular takes 5/3 of it),
	// anti-firefly, hit distance reconstruction (0 off, 1 3x3, 2 5x5), 0: the direct diffuse bypasses NRD
	pt_nrd_max_accum					= ri.Cvar_Get("pt_nrd_max_accum",					"30",	CVAR_NONE);
	pt_nrd_max_fast_accum				= ri.Cvar_Get("pt_nrd_max_fast_accum",				"6",	CVAR_NONE);
	pt_nrd_prepass_blur					= ri.Cvar_Get("pt_nrd_prepass_blur",				"30",	CVAR_NONE);
	pt_nrd_antifirefly					= ri.Cvar_Get("pt_nrd_antifirefly",				"-1",	CVAR_NONE);	// -1: the default of each denoiser
	pt_nrd_hitdist_recon				= ri.Cvar_Get("pt_nrd_hitdist_recon",				"1",	CVAR_NONE);
	pt_nrd_direct						= ri.Cvar_Get("pt_nrd_direct",						"1",	CVAR_NONE);
	// 1: the validation layer of NRD covers the screen (see "VALIDATION" in the NRD README): tiles of normals, roughness, view Z,
	// motion vector check, world grid with jitter, history length, hit distances. Blended with the image by its alpha channel.
	// Colors are exact with sun_tm_enable 0.
	pt_nrd_validation					= ri.Cvar_Get("pt_nrd_validation",					"0",	CVAR_NONE);
	pt_denoiser							= ri.Cvar_Get("pt_denoiser",						"1",	CVAR_NONE);
	/* Size of the sphere a dlight emits from, in world units - not its reach, which is
	 * dlight_t::radius. An emitter with a body gets half buried in whatever surface the
	 * light was spawned on, and loses a disc of its own illumination that size. It buys
	 * only softer shadows, so keep it small. 0 makes the emitter as big as the light's
	 * reach, which is what the port used to do. */
	pt_dlight_radius					= ri.Cvar_Get("pt_dlight_radius",					"2",	CVAR_NONE);
	pt_dlight_min_dist					= ri.Cvar_Get("pt_dlight_min_dist",					"0.433",	CVAR_NONE);	// fraction of the raster radius, 0 disables it
	pt_jobs								= ri.Cvar_Get("pt_jobs",							"1",	CVAR_NONE);	// 1 = the RTX CPU preparation runs on the job pool, 0 = serial
	pt_dlight_lift						= ri.Cvar_Get("pt_dlight_lift",						"16",	CVAR_NONE);	// units a dlight on a surface moves off it, 0 disables it
	/* Overall level of the lights read from the map's entity lump. q3map2's `light` key
	 * is an inverse-square strength with no absolute unit, so the conversion to the
	 * tracer's radiance needs calibrating by eye. Read at map load only. */
	/* Converts the .lgt intensity to the tracer radiance. The file stores a lightmap
	 * luminance times a distance squared, which has no absolute unit. */
	pt_lightgen_scale					= ri.Cvar_Get("pt_lightgen_scale",				"0.005",	CVAR_ARCHIVE);

	pt_light_scale_edit					= ri.Cvar_Get("pt_light_scale_edit",			"0.1",		CVAR_ARCHIVE_ND);
	pt_light_scale_dlight				= ri.Cvar_Get("pt_light_scale_dlight",			"0.02",		CVAR_ARCHIVE_ND);	// intensity of blaster bolts, explosions and muzzle flashes, 1 for no change
	pt_light_scale_saber				= ri.Cvar_Get("pt_light_scale_saber",			"10",		CVAR_ARCHIVE_ND);
	pt_glow_scale						= ri.Cvar_Get("pt_glow_scale",					"1.0",		CVAR_ARCHIVE_ND);
	pt_light_styles						= ri.Cvar_Get("pt_light_styles",				"1",		CVAR_ARCHIVE_ND);
	pt_weapon_fx_shaders				= ri.Cvar_Get("pt_weapon_fx_shaders",			"gfx/effects/sabers/ gfx/effects/blaster_blob gfx/effects/whiteGlow gfx/effects/blasterFrontFlash gfx/effects/blasterSideFlash", CVAR_ARCHIVE_ND);
	pt_sky_per_map						= ri.Cvar_Get("pt_sky_per_map",					"1",		CVAR_ARCHIVE_ND);
	pt_sky_q3map_sun					= ri.Cvar_Get("pt_sky_q3map_sun",				"0",		CVAR_ARCHIVE_ND);
	pt_ambient_floor_scale				= ri.Cvar_Get("pt_ambient_floor_scale",			"1",		CVAR_ARCHIVE_ND);
	tm_per_channel						= ri.Cvar_Get("tm_per_channel",					"1",		CVAR_ARCHIVE_ND);	// 0 keeps the hue of bright colors, 1 maps each channel
	tm_contrast							= ri.Cvar_Get("tm_contrast",					"1",		CVAR_ARCHIVE_ND);	// around the middle gray, 1 for no change

#define UBO_CVAR_DO( _handle, _value ) sun_##_handle = ri.Cvar_Get( #_handle,	#_value, CVAR_NONE);
	UBO_CVAR_LIST
#undef UBO_CVAR_DO
	// The setup menu sets flt_taa, so it must persist. Cvar_Get adds the flag to the existing cvar.
	ri.Cvar_Get( "flt_taa", "1", CVAR_ARCHIVE_ND );
	// The setup menu shows the bloom of the tracer before r_rtx takes effect. Same values as vk_rtx_bloom.cpp.
	ri.Cvar_Get( "pt_bloom", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "pt_bloom_intensity", "1.0", CVAR_ARCHIVE_ND );

    static char _rgb[3] = {'r', 'g', 'b'};

    // sun
    for (int i = 0; i < 3; ++i)
    {
        char buff[32]; 
        snprintf(buff, 32, "sun_color_%c", _rgb[i]);
        sun_color[i] = ri.Cvar_Get(buff, "1.0", 0);
    }

    sun_elevation				= ri.Cvar_Get( "sun_elevation",				"34",	0);
    sun_azimuth					= ri.Cvar_Get( "sun_azimuth",				"258",	0); 
    sun_angle					= ri.Cvar_Get( "sun_angle",					"1.0",	0); 
    sun_brightness				= ri.Cvar_Get( "sun_brightness",			"10.0",	0); 
    sun_bounce					= ri.Cvar_Get( "sun_bounce",				"1.0",	0); 
    sun_animate					= ri.Cvar_Get( "sun_animate",				"0",	0); 
	sun_preset					= ri.Cvar_Get( "sun_preset",				va("%d", SUN_PRESET_NONE), CVAR_ARCHIVE);
	sun_latitude				= ri.Cvar_Get( "sun_latitude",				"32.9",	CVAR_ARCHIVE); // latitude of former HQ of id Software in Richardson, TX
	sun_gamepad					= ri.Cvar_Get( "sun_gamepad",				"0",	0);

    // sky
    physical_sky				= ri.Cvar_Get( "physical_sky",				"0",	CVAR_ARCHIVE);
    physical_sky_draw_clouds	= ri.Cvar_Get( "physical_sky_draw_clouds",	"1",	0);
    physical_sky_space			= ri.Cvar_Get( "physical_sky_space",		"0",	0);
	physical_sky_brightness		= ri.Cvar_Get( "physical_sky_brightness",	"0",	0);
	
	sky_scattering				= ri.Cvar_Get( "sky_scattering",			"5.0",	0);
	sky_transmittance			= ri.Cvar_Get( "sky_transmittance",			"10.0", 0);
	sky_phase_g					= ri.Cvar_Get( "sky_phase_g",				"0.9",	0);
	sky_amb_phase_g				= ri.Cvar_Get( "sky_amb_phase_g",			"0.3",	0);
#endif
	r_ssaoRadius						= Cvar_Get("r_ssaoRadius",					"48",						CVAR_ARCHIVE_ND, "Ambient occlusion sampling radius, in world units");
	ri.Cvar_CheckRange(r_ssaoRadius, 1, 512, qfalse);
	r_ssaoIntensity						= Cvar_Get("r_ssaoIntensity",				"1.0",						CVAR_ARCHIVE_ND, "Ambient occlusion power curve; higher darkens");
	ri.Cvar_CheckRange(r_ssaoIntensity, 0.1, 8, qfalse);
	r_ssaoSlices						= Cvar_Get("r_ssaoSlices",					"3",						CVAR_ARCHIVE_ND, "r_ssao 2: direction slices per pixel. With r_ssao 1, slices x steps is the sample count");
	ri.Cvar_CheckRange(r_ssaoSlices, 1, 8, qtrue);
	r_ssaoSteps							= Cvar_Get("r_ssaoSteps",					"6",						CVAR_ARCHIVE_ND, "r_ssao 2: horizon search steps per slice. With r_ssao 1, slices x steps is the sample count");
	ri.Cvar_CheckRange(r_ssaoSteps, 1, 16, qtrue);
	r_contactShadows					= Cvar_Get("r_contactShadows",				"0",						CVAR_ARCHIVE_ND, "Screen-space contact shadows from the map sun. Shares the GTAO pass, so it requires r_ssao 2");
	ri.Cvar_CheckRange(r_contactShadows, 0, 1, qtrue);
	r_contactShadowLength				= Cvar_Get("r_contactShadowLength",			"8",						CVAR_ARCHIVE_ND, "Contact shadow ray length, in world units");
	ri.Cvar_CheckRange(r_contactShadowLength, 1, 512, qfalse);
	r_contactShadowThickness			= Cvar_Get("r_contactShadowThickness",		"16",						CVAR_ARCHIVE_ND, "How deep behind a surface a contact shadow hit still counts, in world units");
	ri.Cvar_CheckRange(r_contactShadowThickness, 1, 256, qfalse);
	r_contactShadowSteps				= Cvar_Get("r_contactShadowSteps",			"16",						CVAR_ARCHIVE_ND, "Contact shadow ray march steps");
	ri.Cvar_CheckRange(r_contactShadowSteps, 2, 32, qtrue);
	r_contactShadowIntensity			= Cvar_Get("r_contactShadowIntensity",		"0.6",						CVAR_ARCHIVE_ND, "How much of a pixel a full contact shadow may take away; 1 would black it out entirely");
	ri.Cvar_CheckRange(r_contactShadowIntensity, 0, 1, qfalse);
#ifdef USE_PMLIGHT
	r_dlightMode						= Cvar_Get("r_dlightMode",						"2",						CVAR_ARCHIVE, "");
	ri.Cvar_CheckRange(r_dlightMode, 0, 2, qtrue);
	r_dlightScale						= Cvar_Get("r_dlightScale",						"1.0",					CVAR_ARCHIVE_ND, "");
	ri.Cvar_CheckRange(r_dlightScale, 0.1f, 1, qfalse);
	r_dlightIntensity					= Cvar_Get("r_dlightIntensity",					"1.0",						CVAR_ARCHIVE_ND, "");
	ri.Cvar_CheckRange(r_dlightIntensity, 0.1f, 1, qfalse);
#endif

	r_dlightSaturation					= Cvar_Get("r_dlightSaturation",					"1",						CVAR_ARCHIVE_ND, "");
	ri.Cvar_CheckRange(r_dlightSaturation, 0, 1, qfalse);

	r_roundImagesDown					= Cvar_Get("r_roundImagesDown",					"1",						CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	r_nomip								= Cvar_Get("r_nomip",							"0",						CVAR_ARCHIVE | CVAR_LATCH, "Apply picmip only on worldspawn textures");
	ri.Cvar_CheckRange(r_nomip, 0, 1, qtrue);
#ifdef USE_VBO
	r_vbo								= Cvar_Get("r_vbo",								"1",						CVAR_ARCHIVE | CVAR_LATCH, "Cache static world surfaces");
	r_vbo_models						= Cvar_Get("r_vbo_models",						"1",						CVAR_ARCHIVE | CVAR_LATCH, "Cache ghoul2 and md3 model surfaces");
#endif
	r_renderWidth						= Cvar_Get("r_renderWidth",						"800",						CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	r_renderHeight						= Cvar_Get("r_renderHeight",						"600",						CVAR_ARCHIVE_ND | CVAR_LATCH, "");
	r_renderScale						= Cvar_Get("r_renderScale",						"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Scaling mode to be used with custom render resolution:\n"
		" 0 - disabled\n"
		" 1 - nearest filtering, stretch to full size\n"
		" 2 - nearest filtering, preserve aspect ratio (black bars on sides)\n"
		" 3 - linear filtering, stretch to full size\n"
		" 4 - linear filtering, preserve aspect ratio (black bars on sides)\n");
	ri.Cvar_CheckRange(r_renderScale, 0, 4, qtrue);
	r_ignorehwgamma						= Cvar_Get("r_ignorehwgamma",					"0",						CVAR_ARCHIVE_ND | CVAR_LATCH, "Overrides hardware gamma capabilities");
	ri.Cvar_CheckRange(r_ignorehwgamma, 0, 1, qtrue);


/*
Ghoul2 Insert Start
*/
#ifdef _DEBUG
	r_noPrecacheGLA						= Cvar_Get( "r_noPrecacheGLA",					"0",						CVAR_CHEAT, "" );
#endif
	r_noServerGhoul2					= Cvar_Get( "r_noserverghoul2",					"0",						CVAR_CHEAT, "" );
	r_noGhoul2							= Cvar_Get( "r_noghoul2",						"0",						CVAR_CHEAT, "" );
	r_Ghoul2AnimSmooth					= Cvar_Get( "r_ghoul2animsmooth",				"0.25",						CVAR_NONE, "" );
	r_Ghoul2UnSqashAfterSmooth			= Cvar_Get( "r_ghoul2unsqashaftersmooth",		"1",						CVAR_NONE, "" );
	r_Ghoul2UnSqash						= Cvar_Get( "r_ghoul2unsquash",					"1",						CVAR_NONE, "" );
	r_Ghoul2TimeBase					= Cvar_Get( "r_ghoul2timebase",					"2",						CVAR_NONE, "" );
	r_Ghoul2NoLerp						= Cvar_Get( "r_ghoul2nolerp",					"0",						CVAR_NONE, "" );
	r_Ghoul2NoBlend						= Cvar_Get( "r_ghoul2noblend",					"0",						CVAR_NONE, "" );
	r_Ghoul2BlendMultiplier				= Cvar_Get( "r_ghoul2blendmultiplier",			"1",						CVAR_NONE, "" );
	com_buildScript						= ri.Cvar_Get( "com_buildScript",				"0",						0 );
	sv_mapname							= ri.Cvar_Get( "mapname",						"nomap",					CVAR_SERVERINFO | CVAR_ROM );
	broadsword							= Cvar_Get( "broadsword",						"0",						CVAR_ARCHIVE_ND, "" );
	broadsword_kickbones				= Cvar_Get( "broadsword_kickbones",				"1",						CVAR_NONE, "" );
	broadsword_kickorigin				= Cvar_Get( "broadsword_kickorigin",				"1",						CVAR_NONE, "" );
	broadsword_dontstopanim				= Cvar_Get( "broadsword_dontstopanim",			"0",						CVAR_NONE, "" );
	broadsword_waitforshot				= Cvar_Get( "broadsword_waitforshot",			"0",						CVAR_NONE, "" );
	broadsword_playflop					= Cvar_Get( "broadsword_playflop",				"1",						CVAR_NONE, "" );
	broadsword_smallbbox				= Cvar_Get( "broadsword_smallbbox",				"0",						CVAR_NONE, "" );
	broadsword_extra1					= Cvar_Get( "broadsword_extra1",					"0",						CVAR_NONE, "" );
	broadsword_extra2					= Cvar_Get( "broadsword_extra2",					"0",						CVAR_NONE, "" );
	broadsword_effcorr					= Cvar_Get( "broadsword_effcorr",				"1",						CVAR_NONE, "" );
	broadsword_ragtobase				= Cvar_Get( "broadsword_ragtobase",				"2",						CVAR_NONE, "" );
	broadsword_dircap					= Cvar_Get( "broadsword_dircap",					"64",						CVAR_NONE, "" );
/*
Ghoul2 Insert End
*/
	r_modelpoolmegs = Cvar_Get("r_modelpoolmegs", "20", CVAR_ARCHIVE, "" );
	if (ri.LowPhysicalMemory())
	{
		ri.Cvar_Set("r_modelpoolmegs", "0");
	}

	r_aviMotionJpegQuality				= Cvar_Get( "r_aviMotionJpegQuality",			"100",						CVAR_ARCHIVE_ND, "" );
	r_screenshotJpegQuality				= Cvar_Get( "r_screenshotJpegQuality",			"100",						CVAR_ARCHIVE_ND, "" );

	ri.Cvar_CheckRange( r_aviMotionJpegQuality, 10, 100, qtrue );
	ri.Cvar_CheckRange( r_screenshotJpegQuality, 10, 100, qtrue );

	G2Phys_Init();

	for (size_t i = 0; i < numCommands; i++)
	{
		ri.Cmd_AddCommand(commands[i].cmd, commands[i].func);
	}
}

/*
===============
R_Init
===============
*/
extern void R_InitWorldEffects( void ); //tr_WorldEffects.cpp
void R_Init( void ) {
	int i;
	byte *ptr;

	vk_debug("----- R_Init -----\n" );
	ri.Printf(PRINT_ALL, "----- R_Init -----\n");

	// The thread must be idle before the state it reads is cleared.
	R_RenderThreadShutdown();
	r_numVideoMaps = 0;

	// clear all our internal state
	Com_Memset( &tr, 0, sizeof( tr ) );
	Com_Memset( &backEnd, 0, sizeof( backEnd ) );
	Com_Memset( &tess, 0, sizeof( tess ) );
	//Com_Memset( &glState, 0, sizeof( glState ) );

#ifndef FINAL_BUILD
	if ( (intptr_t)tess.xyz & 15 ) {
		ri.Printf(PRINT_WARNING, "tess.xyz not 16 byte aligned\n");
	}
#endif
	//
	// init function tables
	//
	for (i = 0; i < FUNCTABLE_SIZE; i++) {
#if 0
		if (i == 0) {
			tr.sinTable[i] = EPSILON;
		}
		else if (i == (FUNCTABLE_SIZE - 1)) {
			tr.sinTable[i] = -EPSILON;
		}
		else {
			tr.sinTable[i] = sin(DEG2RAD(i * 360.0f / ((float)(FUNCTABLE_SIZE - 1))));
		}
#else
		tr.sinTable[i] = sin( DEG2RAD( i * 360.0f / FUNCTABLE_SIZE ) + 0.0001f );
#endif
		tr.squareTable[i] = (i < FUNCTABLE_SIZE / 2) ? 1.0f : -1.0f;
		if (i == 0) {
			tr.sawToothTable[i] = EPSILON;
		}
		else {
			tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		}
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];
		if (i < FUNCTABLE_SIZE / 2) {
			if (i < FUNCTABLE_SIZE / 4) {
				if (i == 0) {
					tr.triangleTable[i] = EPSILON;
				}
				else {
					tr.triangleTable[i] = (float)i / (FUNCTABLE_SIZE / 4);
				}
			}
			else {
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i - FUNCTABLE_SIZE / 4];
			}
		}
		else {
			tr.triangleTable[i] = -tr.triangleTable[i - FUNCTABLE_SIZE / 2];
		}
	}

	R_InitFogTable();
	R_ImageLoader_Init();
	R_NoiseInit();
	R_Register();

	max_polys = Q_min( r_maxpolys->integer, DEFAULT_MAX_POLYS );
	max_polyverts = Q_min( r_maxpolyverts->integer, DEFAULT_MAX_POLYVERTS );

	// One buffer for the frontend and one for the backend when the render thread is on.
	{
		const size_t g2PoolBytes = sizeof(mat3x4_t) * G2_SNAP_MAX_BONES;
		const size_t physPoolBytes = sizeof(float) * G2_SNAP_MAX_PHYS_FLOATS;
		const size_t bufferBytes = sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts + g2PoolBytes * 3 + physPoolBytes;
		const int numBuffers = R_RenderThreadWanted() ? 2 : 1;

		for ( int b = 0; b < numBuffers; b++ )
		{
			ptr = (byte *)Hunk_Alloc( (int)bufferBytes, h_low);
			backEndDataBuf[b] = (backEndData_t *) ptr;
			backEndDataBuf[b]->polys = (srfPoly_t *) ((char *) ptr + sizeof( *backEndData ));
			backEndDataBuf[b]->polyVerts = (polyVert_t *) ((char *) ptr + sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys);

			// Bone pools of the Ghoul2 snapshot.
			byte *pool = (byte *)( backEndDataBuf[b]->polyVerts + max_polyverts );
			backEndDataBuf[b]->g2.bones = (mat3x4_t *)pool;
			backEndDataBuf[b]->g2.prevBones = (mat3x4_t *)( pool + g2PoolBytes );
			backEndDataBuf[b]->g2.rtxBones = (mat3x4_t *)( pool + g2PoolBytes * 2 );
			backEndDataBuf[b]->g2.physVerts = (float *)( pool + g2PoolBytes * 3 );
		}

		if ( numBuffers == 1 )
			backEndDataBuf[1] = NULL;

		backEndData = backEndDataBuf[0];
		backEndDataBE = backEndDataBuf[0];
	}

	R_InitNextFrame();

	for(i = 0; i < MAX_LIGHT_STYLES; i++)
	{
		RE_SetLightStyle(i, -1);
	}

	R_InitImagesPool();

	vk_create_window();		// Vulkan

#ifdef USE_VBO
	vk_release_world_vbo();
	vk_release_model_vbo();
#endif

	R_Set2DRatio();
	R_InitImages();	

#ifdef _G2_GORE
	R_CreateGoreVBO();
#endif
	vk_create_pipelines();	// Vulkan
	vk_set_clearcolor();

	{
		// Single top-level session covering the whole startup registration
		// pass (shaders, skins, fonts, models, decals, world effects) -- MP
		// gets away with only ever needing one active parse session at a
		// time because it has no session-nesting requirement at all; SP's
		// COM_ParseExt() unconditionally requires one to be active. The
		// per-function fixes below still hold (nesting is fine up to
		// MAX_PARSE_DATA), this is a safety net for any call path that
		// isn't itself wrapped.
		COM_ParseSession initParseSession;
		R_InitShaders(qfalse);
		R_InitSkins();
		R_InitFonts();
		R_ModelInit();
		R_InitDecals();
		R_InitWorldEffects();
	}
	RestoreGhoul2InfoArray();
#ifdef USE_RTX
	vk_rtx_begin_registration();
#endif

	R_RenderThreadInit();

	vk_debug("----- finished R_Init -----\n" );

}


// This need some tlc
/*
===============
RE_Shutdown
===============
*/
void RE_Shutdown( qboolean destroyWindow, qboolean restarting ) {
	vk_debug("RE_Shutdown( %i, %i )\n", destroyWindow, restarting);

	// The thread stops first. The frame in flight uses the resources freed below.
	R_RenderThreadShutdown();

	for (size_t i = 0; i < numCommands; i++)
		ri.Cmd_RemoveCommand(commands[i].cmd);

	R_ShutdownWorldEffects();
	R_ShutdownFonts();
	G2Phys_Shutdown();

#ifdef USE_RTX
	RTX_LightEdit_Invalidate();
#endif

	// contains vulkan resources/state, reinitialized on a map change.
	//if (tr.registered) {

		if (destroyWindow){
			//vk_delete_textures();

			if (restarting)
				SaveGhoul2InfoArray();
		}

		vk_delete_textures();
		vk_release_resources();

#ifdef USE_RTX
		if ( vk.rtxActive && tr.world )
		{
			vk_rtx_destroy_primary_rays_resources();
			vk_rtx_clear_material_list();
		}
#endif
	//}

	//vk_release_resources(); not merged yet (https://github.com/ec-/Quake3e/commit/d31b84ebf2ab702686e98dff40b7673473026b30)

	if (destroyWindow) {
		vk_shutdown();

		Com_Memset(&glState, 0, sizeof(glState));

		// Unconditional (matches rd-vanilla) to avoid a stale window on mid-session renderer switches.
		ri.VK_destroyWindow();
		Com_Memset(&glConfig, 0, sizeof(glConfig));
	}

	tr.registered = qfalse;
	tr.inited = qfalse;
}

/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
void RE_EndRegistration( void ) {
	R_SyncRenderThread();

	// The load is over (C_LevelLoadBegin made every frame run on the main thread). The first
	// frames of the level can still load data in the backend: they run on the main thread too.
	R_RenderThreadSetSync( 4 );

	vk_wait_idle();

	// command buffer is not in recording state at this stage
	// so we can't issue RB_ShowImages() here.
	// moved to RB_SwapBuffers
}

void RE_GetLightStyle( int style, color4ub_t color )
{
	if (style >= MAX_LIGHT_STYLES)
	{
	    Com_Error( ERR_FATAL, "RE_GetLightStyle: %d is out of range", (int)style );
		return;
	}

	byteAlias_t *baDest = (byteAlias_t *)&color, 
				*baSource = (byteAlias_t *)&styleColors[style];
	baDest->i = baSource->i;
}

void RE_SetLightStyle( int style, int color )
{
	if (style >= MAX_LIGHT_STYLES)
	{
	    Com_Error( ERR_FATAL, "RE_SetLightStyle: %d is out of range", (int)style );
		return;
	}

	byteAlias_t *ba = (byteAlias_t *)&styleColors[style];
	if ( ba->i != color) {
		ba->i = color;
	}
}

// A non-positive rangedFog (linFogStart override or none) is the value that SetRangedFog(0) restores.
float g_oldRangedFog = 0.0f;
static void SetRangedFog( float range )
{
	if ( tr.rangedFog <= 0.0f )
		g_oldRangedFog = tr.rangedFog;

	tr.rangedFog = range;

	if ( tr.rangedFog == 0.0f && g_oldRangedFog )
		tr.rangedFog = g_oldRangedFog;
}

extern qboolean gG2_GBMNoReconstruct;
extern qboolean gG2_GBMUseSPMethod;
static void G2API_BoltMatrixReconstruction( qboolean reconstruct ) { gG2_GBMNoReconstruct = (qboolean)!reconstruct; }
static void G2API_BoltMatrixSPMethod( qboolean spMethod ) { gG2_GBMUseSPMethod = spMethod; }

static void SetRefractionProperties( float distortionAlpha, float distortionStretch, qboolean distortionPrePost, qboolean distortionNegate ) {
	tr_distortionAlpha = distortionAlpha;
	tr_distortionStretch = distortionStretch;
	tr_distortionPrePost = distortionPrePost;
	tr_distortionNegate = distortionNegate;
}

static float GetDistanceCull( void ) { return tr.distanceCull; }

static void GetRealRes( int *w, int *h ) {
	*w = glConfig.vidWidth;
	*h = glConfig.vidHeight;
}

extern void R_SVModelInit( void ); //tr_model.cpp
extern void R_AutomapElevationAdjustment( float newHeight ); //tr_world.cpp
extern qboolean R_InitializeWireframeAutomap( void ); //tr_world.cpp

extern qhandle_t RE_RegisterServerSkin( const char *name );

// Cached animation.cfg text. It lives in the zone, so Hunk_Clear does not free it.
static std::map<std::string, char *> s_animationCFGs;

static void RE_AnimationCFGs_DeleteAll( void )
{
	for ( std::map<std::string, char *>::iterator it = s_animationCFGs.begin(); it != s_animationCFGs.end(); ++it )
	{
		ri.Z_Free( it->second );
	}
	s_animationCFGs.clear();
}

void C_LevelLoadBegin(const char *psMapName, ForceReload_e eForceReload)
{
	static char sPrevMapName[MAX_QPATH]={0};
	bool bDeleteModels = eForceReload == eForceReload_MODELS || eForceReload == eForceReload_ALL;

	// Hunk_Clear frees the data of the backend before the renderer hears about it: until
	// RE_EndRegistration, every frame runs on the main thread.
	R_SyncRenderThread();
	R_RenderThreadSetSync( 0x3fffffff );

	if( bDeleteModels )
	{
		CModelCache->DeleteAll();
		RE_AnimationCFGs_DeleteAll();
	}
	else if( ri.Cvar_VariableIntegerValue( "sv_pure" ) )
		CModelCache->DumpNonPure();

	tr.numBSPModels = 0;

	/* If we're switching to the same level, don't increment current level */
	if (Q_stricmp( psMapName,sPrevMapName ))
	{
		Q_strncpyz( sPrevMapName, psMapName, sizeof(sPrevMapName) );
		tr.currentLevel++;
	}
}

int C_GetLevel( void )
{
	return tr.currentLevel;
}

void C_LevelLoadEnd( void )
{
	R_SyncRenderThread();
	CModelCache->LevelLoadEnd( qfalse );
	ri.SND_RegisterAudio_LevelLoadEnd( qfalse );
	ri.S_RestartMusic();
	*( ri.gbAlreadyDoingLoad() ) = qfalse;
}

// ---------------------------------------------------------------------
// SP-only GetRefAPI support: adapters for real vulkan functions whose
// signature differs from what SP's refexport_t expects, and minimal
// no-op stubs (matching the pattern already used by code/rd-rend2's own
// GetRefAPI, see code/rd-rend2/tr_init.cpp) for SP-only concepts this
// renderer does not implement. In a JKA build, only the dissolve wipes and
// the refraction properties have callers; the screenshot and raw image
// functions are used by JK2_MODE code only.
// ---------------------------------------------------------------------

// Defined in G2_API.cpp (ported from code/rd-vanilla) -- no header declares
// it for RENDERER since SP's ghoul2/G2.h only has the MP-named ...Rag variant.
extern void G2API_AnimateG2Models(CGhoul2Info_v &ghoul2, int AcurrentTime, CRagDollUpdateParams *params);

// SP's refexport_t wants a 3rd (bAllowScreenDissolve) param that MP's
// RegisterMedia_LevelLoadBegin doesn't have -- C_LevelLoadBegin (shared,
// above) doesn't use it either (matching code/rd-rend2's own SP adapter).
static void RE_LevelLoadBegin( const char *psMapName, ForceReload_e eForceReload, qboolean bAllowScreenDissolve )
{
	C_LevelLoadBegin( psMapName, eForceReload );
}

// tr_font.cpp's own 2-arg overload of AnyLanguage_ReadCharFromString is
// #ifdef JK2_MODE-only and not compiled in this build -- reimplement the
// same trivial wrapper around the always-available 3-arg overload.
static unsigned int RE_AnyLanguage_ReadCharFromString2( char **psText, qboolean *pbIsTrailingPunctuation )
{
	int advance = 0;
	unsigned int advance2 = AnyLanguage_ReadCharFromString( *psText, &advance, pbIsTrailingPunctuation );
	*psText += advance;
	return advance2;
}

// Cloak distortion properties, set by cgame through cgi_R_SetRefractProp.
// Read by ComputeDistortionPass (vk_shade_geometry.cpp).
float tr_distortionAlpha = 1.0f;
float tr_distortionStretch = 0.0f;
qboolean tr_distortionPrePost = qfalse;
qboolean tr_distortionNegate = qfalse;
static float *get_tr_distortionAlpha( void ) { return &tr_distortionAlpha; }
static float *get_tr_distortionStretch( void ) { return &tr_distortionStretch; }
static qboolean *get_tr_distortionPrePost( void ) { return &tr_distortionPrePost; }
static qboolean *get_tr_distortionNegate( void ) { return &tr_distortionNegate; }

// Called by Hunk_Clear. Safe on a renderer that is stopped or never started.
static void R_ClearStuffToStopGhoul2CrashingThings( void )
{
	RE_HunkClearCrap();
}

// Ported from code/rd-vanilla/tr_skin.cpp's RE_GetAnimationCFG -- reads and
// caches models/players/<x>/animation.cfg. code/game/NPC_stats.cpp's
// G_ParseAnimationFile() depends on this to populate animation frame data;
// previously stubbed to always return 0, which silently made every
// animation register with numFrames==0 (T-pose, no bone anim ever applied).
static int RE_GetAnimationCFG( const char *psCFGFilename, char *psDest, int iDestSize )
{
	char *psText = NULL;

	std::map<std::string, char *>::iterator it = s_animationCFGs.find( psCFGFilename );
	if ( it != s_animationCFGs.end() )
	{
		psText = it->second;
	}
	else
	{
		fileHandle_t f;
		long iLen = ri.FS_FOpenFileRead( psCFGFilename, &f, qfalse );
		if ( iLen <= 0 )
		{
			return 0;
		}

		psText = (char *)ri.Z_Malloc( iLen + 1, TAG_GHOUL2, qfalse, 4 );
		ri.FS_Read( psText, iLen, f );
		psText[iLen] = '\0';
		ri.FS_FCloseFile( f );

		s_animationCFGs[psCFGFilename] = psText;
	}

	if ( psText )
	{
		if ( psDest )
		{
			Q_strncpyz( psDest, psText, iDestSize );
		}
		return (int)strlen( psText );
	}

	return 0;
}
static void stub_Scissor( float x, float y, float w, float h ) {}
static qboolean stub_ProcessDissolve( void ) { return qfalse; }
static qboolean stub_InitDissolve( qboolean bForceCircularExtroWipe ) { return qfalse; }
static void stub_GetScreenShot( byte *data, int w, int h ) {}
static void stub_GetModelBounds( refEntity_t *refEnt, vec3_t bounds1, vec3_t bounds2 ) { VectorClear(bounds1); VectorClear(bounds2); }

#ifdef G2_PERFORMANCE_ANALYSIS
// Com_Frame() (qcommon/common.cpp) calls re.G2Time_ResetTimers() every frame
// unconditionally -- must be wired or it's a null call. Matches
// code/rd-rend2/tr_init.cpp's own extern+assignment for these.
extern void G2Time_ResetTimers(void);
extern void G2Time_ReportTimers(void);
#endif
void RE_CaptureNextFrame( byte *rgba, int width, int height );

static void *RE_GetExtension( const char *name )
{
	if ( !Q_stricmp( name, RD_PERF_API_NAME ) ) {
		return vk_perf_get_api();
	}

#ifdef USE_RTX
	return RTX_LightEdit_GetExtension( name );
#else
	return NULL;
#endif
}

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
extern "C" {
Q_EXPORT refexport_t* QDECL GetRefAPI( int apiVersion, refimport_t *rimp ) {
	static refexport_t re;

	assert( rimp );
	ri = *rimp;
	RB_PrintInstall();

	memset( &re, 0, sizeof( re ) );

	if ( apiVersion != REF_API_VERSION ) {
		vk_debug("Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, apiVersion );
		return NULL;
	}

	// the RE_ functions are Renderer Entry points

	// SP's refexport_t is a different (smaller, differently-shaped) struct
	// than MP's -- built field-by-field against code/rd-common/tr_public.h,
	// using code/rd-rend2/tr_init.cpp's own GetRefAPI as the reference
	// template for which fields SP expects and how an SP renderer normally
	// wires them up.
	re.Shutdown								= RE_Shutdown;
	re.GetExtension							= RE_GetExtension;

	re.BeginRegistration					= RE_BeginRegistration;
	re.RegisterModel						= RE_RegisterModel;
	re.RegisterSkin							= RE_RegisterSkin;
	re.GetAnimationCFG						= RE_GetAnimationCFG;
	re.RegisterShader						= RE_RegisterShader;
	re.RegisterShaderNoMip					= RE_RegisterShaderNoMip;
	re.LoadWorld							= RE_LoadWorldMap;
	re.R_LoadImage							= R_LoadImage;
	re.SetWorldVisData						= RE_SetWorldVisData;
	re.EndRegistration						= RE_EndRegistration;

	re.ClearScene							= RE_ClearScene;
	re.AddRefEntityToScene					= RE_AddRefEntityToScene;
	re.GetLighting							= RE_GetLighting;
	re.LAGoggles							= RE_LAGoggles;
	re.AddPolyToScene						= RE_AddPolyToScene;
	re.AddLightToScene						= RE_AddLightToScene;
	re.RenderScene							= RE_RenderScene;
	re.SetColor								= RE_SetColor;
	re.DrawStretchPic						= RE_StretchPic;
	re.DrawRotatePic						= RE_RotatePic;
	re.DrawRotatePic2						= RE_RotatePic2;
	re.Scissor								= stub_Scissor;
	re.DrawStretchRaw						= RE_StretchRaw;
	re.UploadCinematic						= RE_UploadCinematic;

	re.BeginFrame							= RE_BeginFrame;
	re.EndFrame								= RE_EndFrame;

	re.ProcessDissolve						= stub_ProcessDissolve;
	re.InitDissolve							= stub_InitDissolve;
	re.GetScreenShot						= stub_GetScreenShot;

	re.TempRawImage_ReadFromFile			= RE_TempRawImage_ReadFromFile;
	re.TempRawImage_CleanUp					= RE_TempRawImage_CleanUp;

	re.MarkFragments						= R_MarkFragments;
	re.GetModelBounds						= stub_GetModelBounds;
	re.LerpTag								= R_LerpTag;

	re.RegisterFont							= RE_RegisterFont;
	re.Font_StrLenPixels					= RE_Font_StrLenPixels;
	re.Font_HeightPixels					= RE_Font_HeightPixels;
	re.Font_StrLenChars						= RE_Font_StrLenChars;
	re.Font_DrawString						= RE_Font_DrawString;
	re.Language_IsAsian						= Language_IsAsian;
	re.Language_UsesSpaces					= Language_UsesSpaces;
	re.AnyLanguage_ReadCharFromString		= AnyLanguage_ReadCharFromString;

	re.R_InitWorldEffects					= R_InitWorldEffects;
	re.R_ClearStuffToStopGhoul2CrashingThings	= R_ClearStuffToStopGhoul2CrashingThings;
	re.inPVS								= R_inPVS;
	re.GetLightStyle						= RE_GetLightStyle;
	re.SetLightStyle						= RE_SetLightStyle;
	re.GetBModelVerts						= RE_GetBModelVerts;

	re.tr_distortionAlpha					= get_tr_distortionAlpha;
	re.tr_distortionStretch					= get_tr_distortionStretch;
	re.tr_distortionPrePost					= get_tr_distortionPrePost;
	re.tr_distortionNegate					= get_tr_distortionNegate;
	re.SetTempGlobalFogColor				= R_SetTempGlobalFogColor;

	re.SetRangedFog							= SetRangedFog;

	re.TheGhoul2InfoArray					= TheGhoul2InfoArray;

	re.G2API_AddBolt						= G2API_AddBolt;
	re.G2API_AddSurface						= G2API_AddSurface;
	re.G2API_AnimateG2Models				= G2API_AnimateG2Models;
	re.G2API_AttachEnt						= G2API_AttachEnt;
	re.G2API_AttachG2Model					= G2API_AttachG2Model;
	re.G2API_CleanGhoul2Models				= G2API_CleanGhoul2Models;
	re.G2API_CollisionDetect				= G2API_CollisionDetect;
	re.G2API_CopyGhoul2Instance				= G2API_CopyGhoul2Instance;
	re.G2API_DetachEnt						= G2API_DetachEnt;
	re.G2API_DetachG2Model					= G2API_DetachG2Model;
	re.G2API_GetAnimFileName				= G2API_GetAnimFileName;
	re.G2API_GetAnimFileNameIndex			= G2API_GetAnimFileNameIndex;
	re.G2API_GetAnimFileInternalNameIndex	= G2API_GetAnimFileInternalNameIndex;
	re.G2API_GetAnimRange					= G2API_GetAnimRange;
	re.G2API_GetBoneAnim					= G2API_GetBoneAnim;
	re.G2API_GetBoneAnimIndex				= G2API_GetBoneAnimIndex;
	re.G2API_GetBoneIndex					= G2API_GetBoneIndex;
	re.G2API_GetBoltMatrix					= G2API_GetBoltMatrix;
	re.G2API_GetGhoul2ModelFlags			= G2API_GetGhoul2ModelFlags;
	re.G2API_GetGLAName						= G2API_GetGLAName;
	re.G2API_GetParentSurface				= G2API_GetParentSurface;
	re.G2API_GetRagBonePos					= G2API_GetRagBonePos;
	re.G2API_GetSurfaceIndex				= G2API_GetSurfaceIndex;
	re.G2API_GetSurfaceName					= G2API_GetSurfaceName;
	re.G2API_GetSurfaceRenderStatus			= G2API_GetSurfaceRenderStatus;
	re.G2API_GetTime						= G2API_GetTime;
	re.G2API_GiveMeVectorFromMatrix			= G2API_GiveMeVectorFromMatrix;
	re.G2API_HaveWeGhoul2Models				= G2API_HaveWeGhoul2Models;
	re.G2API_IKMove							= G2API_IKMove;
	re.G2API_InitGhoul2Model				= G2API_InitGhoul2Model;
	re.G2API_IsPaused						= G2API_IsPaused;
	re.G2API_ListBones						= G2API_ListBones;
	re.G2API_ListSurfaces					= G2API_ListSurfaces;
	re.G2API_LoadGhoul2Models				= G2API_LoadGhoul2Models;
	re.G2API_LoadSaveCodeDestructGhoul2Info	= G2API_LoadSaveCodeDestructGhoul2Info;
	re.G2API_PauseBoneAnim					= G2API_PauseBoneAnim;
	re.G2API_PrecacheGhoul2Model			= G2API_PrecacheGhoul2Model;
	re.G2API_RagEffectorGoal				= G2API_RagEffectorGoal;
	re.G2API_RagEffectorKick				= G2API_RagEffectorKick;
	re.G2API_RagForceSolve					= G2API_RagForceSolve;
	re.G2API_RagPCJConstraint				= G2API_RagPCJConstraint;
	re.G2API_RagPCJGradientSpeed			= G2API_RagPCJGradientSpeed;
	re.G2API_RemoveBolt						= G2API_RemoveBolt;
	re.G2API_RemoveBone						= G2API_RemoveBone;
	re.G2API_RemoveGhoul2Model				= G2API_RemoveGhoul2Model;
	re.G2API_RemoveSurface					= G2API_RemoveSurface;
	re.G2API_SaveGhoul2Models				= G2API_SaveGhoul2Models;
	re.G2API_SetBoneAngles					= G2API_SetBoneAngles;
	re.G2API_SetBoneAnim					= G2API_SetBoneAnim;
	re.G2API_SetGhoul2ModelFlags			= G2API_SetGhoul2ModelFlags;
	re.G2API_SetLodBias						= G2API_SetLodBias;
	re.G2API_SetNewOrigin					= G2API_SetNewOrigin;
	re.G2API_SetRagDoll						= G2API_SetRagDoll;
	re.G2API_SetRootSurface					= G2API_SetRootSurface;
	re.G2API_SetShader						= G2API_SetShader;
	re.G2API_SetSkin						= G2API_SetSkin;
	re.G2API_SetSurfaceOnOff				= G2API_SetSurfaceOnOff;
	re.G2API_SetTime						= G2API_SetTime;
	re.G2API_StopBoneAngles					= G2API_StopBoneAngles;
	re.G2API_StopBoneAnglesIndex			= G2API_StopBoneAnglesIndex;
	re.G2API_StopBoneAnim					= G2API_StopBoneAnim;
	re.G2API_StopBoneAnimIndex				= G2API_StopBoneAnimIndex;

#ifdef _G2_GORE
	re.G2API_AddSkinGore					= G2API_AddSkinGore;
	re.G2API_ClearSkinGore					= G2API_ClearSkinGore;
#endif

#ifdef G2_PERFORMANCE_ANALYSIS
	re.G2Time_ResetTimers					= G2Time_ResetTimers;
	re.G2Time_ReportTimers					= G2Time_ReportTimers;
#endif

	re.CaptureNextFrame						= RE_CaptureNextFrame;
	re.PhysCommand							= G2Phys_UICommand;

	// Fields left unassigned here are NULL in a zero-initialized refexport_t
	// -- any caller reaching them crashes at a null function pointer, so
	// this list must stay in sync with code/rd-common/tr_public.h.
	re.AddWeatherZone						= RE_AddWeatherZone;
	re.WorldEffectCommand					= RE_WorldEffectCommand;
	re.GetWindVector						= R_GetWindVector;
	re.GetWindGusting						= R_GetWindGusting;
	re.IsShaking							= R_IsShaking;
	re.IsOutside							= R_IsOutside;
	re.IsOutsideCausingPain					= R_IsOutsideCausingPain;
	re.GetChanceOfSaberFizz					= R_GetChanceOfSaberFizz;
	re.ModelBounds							= R_ModelBounds;
	re.SVModelInit							= R_SVModelInit;
	// SaveJPGToBuffer/LoadJPGFromBuffer are #ifdef JK2_MODE-only fields in
	// refexport_t -- not compiled in for this (non-JK2) build.
	re.AnyLanguage_ReadCharFromString2		= RE_AnyLanguage_ReadCharFromString2;
	re.RegisterMedia_LevelLoadBegin			= RE_LevelLoadBegin;
	re.RegisterMedia_LevelLoadEnd			= C_LevelLoadEnd;
	re.RegisterMedia_GetLevel				= C_GetLevel;
	re.RegisterImages_LevelLoadEnd			= C_Images_LevelLoadEnd;
	re.RegisterModels_LevelLoadEnd			= C_Models_LevelLoadEnd;
	re.G2API_AddBoltSurfNum					= G2API_AddBoltSurfNum;
	re.G2API_GetAnimIndex					= G2API_GetAnimIndex;
	re.G2API_GetAnimRangeIndex				= G2API_GetAnimRangeIndex;
	re.G2API_PauseBoneAnimIndex				= G2API_PauseBoneAnimIndex;
	re.G2API_SetAnimIndex					= G2API_SetAnimIndex;
	re.G2API_SetAnimOverride				= G2API_SetAnimOverride;
	re.G2API_SetBoneAnglesIndex				= G2API_SetBoneAnglesIndex;
	re.G2API_SetBoneAnglesMatrix				= G2API_SetBoneAnglesMatrix;
	re.G2API_SetBoneAnglesMatrixIndex		= G2API_SetBoneAnglesMatrixIndex;
	re.G2API_SetBoneAnimIndex				= G2API_SetBoneAnimIndex;
	re.G2API_SetBoneIKState					= G2API_SetBoneIKState;
	re.G2API_SetGhoul2ModelIndexes			= G2API_SetGhoul2ModelIndexes;

	return &re;

}

} //extern "C"
