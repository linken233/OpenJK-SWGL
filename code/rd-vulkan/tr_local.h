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

#ifndef TR_LOCAL_H
#define TR_LOCAL_H

// Path tracer, ported from JKSunny/EternalJK. Comment out to build the renderer without it;
// everything it adds is guarded by this and the raster path never sees it.
#define USE_RTX

// PBR materials, ported from JKSunny/EternalJK (branch pbr): normal and physical maps from the
// shader keywords or found next to the diffuse texture, and normal maps computed from the
// diffuse texture (r_genNormalMaps).
#define USE_VK_PBR

#ifdef USE_VK_PBR
	#define VK_COMPUTE_NORMALMAP

	#ifdef VK_COMPUTE_NORMALMAP
		#define MAX_BATCH_COMPUTE_NORMALMAPS 1024
	#endif

	// environment cubemaps of the map probes, for the PBR reflections (r_cubeMapping)
	#define VK_CUBEMAP

	#ifdef VK_CUBEMAP
		#define REF_CUBEMAP_SIZE		256
		#define REF_CUBEMAP_MIPS		7		// pbr.glsl reads lod = roughness * 6
		#define MAX_CUBEMAPS			64
		#define CUBEMAPS_PER_FRAME		4		// probes captured in one frame
	#endif
#endif

#define USE_VBO					// store static world geometry in VBO

#ifdef USE_VBO
	#define MAX_VBOS      4096

	#define USE_VBO_GHOUL2
	#define USE_VBO_MDV	
	#define USE_VBO_SS
	#define USE_VBO_GRID		/* put SF_GRID to VBO */
#endif

#define USE_FOG_ONLY
#define USE_FOG_COLLAPSE		// not compatible with legacy dlights
#if defined ( USE_VBO ) && !defined( USE_FOG_ONLY )
#define USE_FOG_ONLY
#endif

#define USE_PMLIGHT				// promode dynamic lights via \r_dlightMode 1|2
#define MAX_REAL_DLIGHTS		( MAX_DLIGHTS*2 )
#define MAX_LITSURFS			( MAX_DRAWSURFS )
#define	MAX_FLARES				256
	
#define MAX_TEXTURE_SIZE		2048 // must be less or equal to 32768
#define MAX_TEXTURE_UNITS		8

#define USE_BUFFER_CLEAR		/* clear attachments on render pass begin */

#include "qcommon/q_shared.h"
#include "qcommon/qfiles.h"
#include "rd-common/tr_public.h"
#include "rd-common/tr_common.h"
#include "rd-common/mdx_format.h"
#include "game/ghoul2_shared.h"
#include "ghoul2/G2.h"
typedef enum { h_high, h_low, h_dontcare } ha_pref;
void *Hunk_Alloc( int size, ha_pref preference );
void *Hunk_AllocateTempMemory( int size );
void Hunk_FreeTempMemory( void *buf );
int Hunk_MemoryRemaining( void );
void *CM_GetCachedMapDiskImage( void );
void CM_SetCachedMapDiskImage( void *ptr );
void CM_SetUsingCache( qboolean usingCache );
qboolean Com_TheHunkMarkHasBeenMade( void );
cvar_t *Cvar_Get( const char *var_name, const char *value, int flags, const char *var_desc );
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits );
int Z_Free( void *ptr );
#define Square(x) ((x)*(x))
#define TAG_TEMP_IMAGE TAG_TEMP_WORKSPACE
void G2_RemoveRedundantBoneOverrides( boneInfo_v &blist, int *activeBones );
void G2_RemoveRedundantBolts( boltInfo_v &bltlist, surfaceInfo_v &slist, int *activeSurfaces, int *activeBones );
void Create_Matrix( const float *angle, mdxaBone_t *matrix );
#define BONE_NEED_TRANSFORM 0x8000
extern mdxaBone_t worldMatrix;
extern mdxaBone_t worldMatrixInv;
#define G2VFM_ARG(x) (x)
#define G2VFM_VEC(x) (*(vec3_t*)(x))
int G2_IsSurfaceOff( CGhoul2Info *ghlInfo, surfaceInfo_v &slist, const char *surfaceName );
#define IHeapAllocator CMiniHeap
void RemoveBoneCache( CBoneCache *boneCache );

// Skinned vertices of one ghoul2 model for the collision traces (G2API_CollisionDetect).
// Valid while the skeleton (CBoneCache::mCurrentTouch), the LOD, the scale and the
// surface list do not change, so one skinning serves all the traces of a frame.
struct g2CollisionVerts_t
{
	int						touch;			// -1: empty
	int						lod;
	vec3_t					scale;
	uint32_t				surfSig;
	std::vector<float>		verts;			// 5 floats per vertex: xyz, st
	std::vector<intptr_t>	surfVerts;		// per surface index: pointer into verts, or 0
	std::vector<float>		surfBounds;		// per surface index: mins xyz, maxs xyz

	// A surface is skinned when a trace first reaches its conservative box (G2_TraceSurfaces).
	int						epoch = -1;		// tr.modelEpoch when built
	class CBoneCache		*boneCache = nullptr;
	std::vector<uint8_t>	surfState;		// per surface index: 0 not reached, 1 conservative box ready, 2 skinned
	std::vector<const mdxmSurface_t *>	surfDef;	// per surface index: the surface
	std::vector<float *>	surfOut;		// per surface index: where its skinned vertices go
	std::vector<float>		surfCons;		// per surface index: conservative mins xyz, maxs xyz
};
g2CollisionVerts_t *G2_GetCollisionVerts( CBoneCache *boneCache );
int G2_GetBoneCacheTouch( const CBoneCache *boneCache );

#define GHOUL2_ZONETRANSALLOC 0
typedef intptr_t g2vert_int_t;
void G2API_AnimateG2ModelsRag(CGhoul2Info_v &ghoul2, int AcurrentTime, CRagDollUpdateParams *params);
void G2API_SetRagDoll(CGhoul2Info_v &ghoul2, CRagDollParams *parms);
qboolean G2API_RagEffectorGoal(CGhoul2Info_v &ghoul2, const char *boneName, vec3_t pos);
qboolean G2API_RagEffectorKick(CGhoul2Info_v &ghoul2, const char *boneName, vec3_t velocity);
qboolean G2API_RagForceSolve(CGhoul2Info_v &ghoul2, qboolean force);
qboolean G2API_RagPCJConstraint(CGhoul2Info_v &ghoul2, const char *boneName, vec3_t min, vec3_t max);
qboolean G2API_RagPCJGradientSpeed(CGhoul2Info_v &ghoul2, const char *boneName, const float speed);
qboolean G2API_GetRagBonePos(CGhoul2Info_v &ghoul2, const char *boneName, vec3_t pos, vec3_t entAngles, vec3_t entPos, vec3_t entScale);
qboolean G2API_SetBoneIKState(CGhoul2Info_v &ghoul2, int time, const char *boneName, int ikState, sharedSetBoneIKStateParams_t *params);
qboolean G2API_IKMove(CGhoul2Info_v &ghoul2, int time, sharedIKMoveParams_t *params);
#define CVAR_NONE 0

#if defined(_WIN32)
#	include <windows.h>
#endif

// I know...
#define	MAX_UINT			((unsigned)(~0))
typedef enum {
	CT_FRONT_SIDED,
	CT_BACK_SIDED,
	CT_TWO_SIDED
} cullType_t;

// Vulkan
#include "vk_local.h"
#include "vk_perf.h"

// GL constants substitutions
typedef enum {
	GL_NEAREST = 105,
	GL_LINEAR,
	GL_NEAREST_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR,
	GL_LINEAR_MIPMAP_LINEAR,
	GL_MODULATE,
	GL_ADD,
	GL_ADD_NONIDENTITY,

	GL_BLEND_MODULATE,
	GL_BLEND_ADD,
	GL_BLEND_ALPHA,
	GL_BLEND_ONE_MINUS_ALPHA,
	GL_BLEND_MIX_ALPHA,				// SRC_ALPHA + ONE_MINUS_SRC_ALPHA
	GL_BLEND_MIX_ONE_MINUS_ALPHA,	// ONE_MINUS_SRC_ALPHA + SRC_ALPHA

	GL_BLEND_DST_COLOR_SRC_ALPHA, // GLS_SRCBLEND_DST_COLOR + GLS_DSTBLEND_SRC_ALPHA

	GL_DECAL,
	GL_BACK_LEFT,
	GL_BACK_RIGHT
} glCompat;

//#define GL_INDEX_TYPE						GL_UNSIGNED_INT
#define GL_INDEX_TYPE						uint32_t
#define GLint								int
#define GLuint								unsigned int
#define GLboolean							VkBool32

#define GL_POINTS							0x0000
#define GL_LINES							0x0001
#define GL_TRIANGLES						0x0004
#define GL_QUADS							0x0007
#define GL_POLYGON							0x0009

#define GL_RGB5								0x8050
#define GL_RGB8								0x8051
#define GL_RGBA4							0x8056
#define GL_RGBA8							0x8058

#define GL_CLAMP_TO_EDGE					0x812F
#define GL_RGB4_S3TC						0x83A1
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT		0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT	0x83F3

typedef void GLvoid;
typedef int GLsizei;
typedef unsigned int glIndex_t;

#define LL(x) x=LittleLong(x)

// 14 bits
// can't be increased without changing bit packing for drawsurfs
// see QSORT_SHADERNUM_SHIFT
#define SHADERNUM_BITS	14

// drawsurf sort key, see QSORT_SHADERNUM_SHIFT
typedef uint64_t sortKey_t;
#define MAX_SHADERS		(1<<SHADERNUM_BITS)
#define SHADERNUM_MASK	(MAX_SHADERS-1)

//#define MAX_SHADER_STATES 2048
#define MAX_STATES_PER_SHADER 32
#define MAX_STATE_NAME 32

typedef enum
{
	DLIGHT_VERTICAL	= 0,
	DLIGHT_PROJECTED
} eDLightTypes;

#ifdef USE_RTX
typedef enum dlight_type_e
{
    DLIGHT_SPHERE = 0,
    DLIGHT_SPOT
} dlight_type;

typedef enum dlight_spot_emission_profile_e
{
    DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF = 0,
    DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE
} dlight_spot_emission_profile;
#endif

typedef struct dlight_s {
	eDLightTypes	mType;

	vec3_t			origin;
	vec3_t			origin2;
	vec3_t			mProjOrigin;		// projected light's origin

	vec3_t			color;				// range from 0.0 to 1.0, should be color normalized

	float			radius;
	float			mProjRadius;		// desired radius of light

	int				additive;			// texture detail is lost tho when the lightmap is dark
	qboolean		linear;
#ifdef USE_RTX
    // VKPT light types support
    dlight_type light_type;
	vec3_t		rtxOrigin;		// origin the tracer emits from: origin, moved off the surface it hit. Set by R_LiftDlightsForRtx.
	//float		intensity;		// use radius
    // Spotlight options
    struct {
        // Spotlight emission profile
        dlight_spot_emission_profile emission_profile;
        // Spotlight direction
        vec3_t  direction;
        union {
            // Options for DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF
            struct {
                // Cosine of angle of spotlight cone width (no emission beyond that)
                float   cos_total_width;
                // Cosine of angle of start of falloff (full emission below that)
                float   cos_falloff_start;
            };
            // Options for DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE
            struct {
                // Angle of spotlight cone width (no emission beyond that), in radians
                float   total_width;
                // Emission profile texture, indexed by 'angle / total_width'
                qhandle_t texture;
            };
        };
    } spot;
#endif
#ifdef USE_PMLIGHT
	struct litSurf_s* head;
	struct litSurf_s* tail;
#endif

	vec3_t			transformed;		// origin in local coordinate system
	vec3_t			transformed2;		// origin2 in local coordinate system
	vec3_t			mProjTransformed;	// projected light's origin in local coordinate system

	vec3_t			mDirection;
	vec3_t			mBasis2;
	vec3_t			mBasis3;

	vec3_t			mTransDirection;
	vec3_t			mTransBasis2;
	vec3_t			mTransBasis3;
} dlight_t;


// a trMiniRefEntity_t has all the information passed in by
// the client game, other info will come from it's parent main ref entity
typedef struct
{
	miniRefEntity_t	e;
} trMiniRefEntity_t;

// a trRefEntity_t has all the information passed in by
// the client game, as well as some locally derived info
typedef struct trRefEntity_s {
	refEntity_t	e;

	float		axisLength;		// compensate for non-normalized axis
	qboolean	lightingCalculated;
	vec3_t		lightDir;			// normalized direction towards light, original
	vec3_t		modelLightDir;  // normalized direction towards light, in model space
	vec3_t		ambientLight;	// color normalized to 0-255
	int			ambientLightInt;	// 32 bit rgba packed
	vec3_t		directedLight;

#ifdef USE_PMLIGHT
	vec3_t		shadowLightDir;	// normalized direction towards light
#endif
	qboolean	intShaderTime;
#ifdef VK_CUBEMAP
	int			cubemapIndex;	// 1 + index of the nearest cubemap, 0 for none
#endif
	// Ghoul2 model records of the path tracer in backEndData->g2, set by R_G2_SnapshotScene.
	int			g2RtxFirst;
	int			g2RtxCount;
} trRefEntity_t;

#ifdef VK_CUBEMAP
typedef struct cubemap_s {
	char		name[MAX_QPATH];
	vec3_t		origin;
	float		parallaxRadius;
} cubemap_t;
#endif


typedef struct orientationr_s {
	vec3_t		origin;			// in world coordinates
	matrix3_t	axis;		// orientation in world
	vec3_t		viewOrigin;		// viewParms->or.origin in local coordinates
	float		modelViewMatrix[16];
	float		modelMatrix[16];
} orientationr_t;

typedef struct textureMode_s {
	const char *name;
	int	minimize, maximize;
} textureMode_t;

extern	int	gl_filter_min, gl_filter_max;

typedef enum
{
	IMGFLAG_NONE			= 0x0000,
	IMGFLAG_MIPMAP			= 0x0001,
	IMGFLAG_PICMIP			= 0x0002,
	IMGFLAG_CLAMPTOEDGE		= 0x0004,
	IMGFLAG_CLAMPTOBORDER	= 0x0008,
	IMGFLAG_NO_COMPRESSION	= 0x0010,
	IMGFLAG_NOLIGHTSCALE	= 0x0020,
	IMGFLAG_LIGHTMAP		= 0x0040,
	IMGFLAG_NOSCALE			= 0x0080,
	IMGFLAG_RGB				= 0x0100,
	IMGFLAG_COLORSHIFT		= 0x0200,
	IMGFLAG_STORAGE			= 0x0800,	// written by a compute shader
} imgFlags_t;

#if defined( _WIN32 )
DEFINE_ENUM_FLAG_OPERATORS( imgFlags_t );
#elif defined( __linux__ ) || defined( __APPLE__ ) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
inline constexpr imgFlags_t operator | (imgFlags_t a, imgFlags_t b) throw() {
	return imgFlags_t(((int)a) | ((int)b));
}
inline imgFlags_t &operator |= (imgFlags_t &a, imgFlags_t b) throw() {
	return (imgFlags_t &)(((int&)a) |= ((int)b));
}
inline constexpr imgFlags_t operator & (imgFlags_t a, imgFlags_t b) throw() {
	return imgFlags_t(((int)a) & ((int)b));
}
inline imgFlags_t &operator &= (imgFlags_t &a, imgFlags_t b) throw() {
	return (imgFlags_t &)(((int &)a) &= ((int)b));
}
inline constexpr imgFlags_t operator ~ (imgFlags_t a) throw() {
	return imgFlags_t(~((int)a));
}
#endif

typedef struct image_s {
	char					*imgName;					// game path, including extension
	struct image_s			*next;
	word					width, height;				// after power of two and picmip but not including clamp to MAX_TEXTURE_SIZE
	uint32_t				uploadWidth, uploadHeight;	// after power of two and picmip but not including clamp to MAX_TEXTURE_SIZE
	uint32_t				index;
	imgFlags_t				flags;

	int						frameUsed;					// for texture usage in frame statistics
	int						internalFormat;

	bool					mipmap;						// deprecated
	bool					allowPicmip;				// deprecated

	VkImage					handle;
	VkImageView				view;
	VkDescriptorSet			descriptor_set;
#ifdef USE_RTX
	VkSampler				sampler;
	// The tracer samples an sRGB texture through this view, decoded to linear. The raster
	// uses view, UNORM, and reads the bytes as vanilla does. Equal to view otherwise.
	VkImageView				rtx_view;
    byte					*pix_data;
    vec3_t					light_color; // use this color if this is a light source
	vec2_t					min_light_texcoord;
	vec2_t					max_light_texcoord;
	bool					entire_texture_emissive;
	bool					processing_complete;
#endif
	qboolean				isLightmap;
	uint32_t				mipLevels;		// gl texture binding
	uint32_t				type;			// index in textureMapTypes: the swizzle of the view
	VkSamplerAddressMode	wrapClampMode;	
} image_t;

typedef struct VBO_s
{	
	int				index;

	VkBuffer		buffer;
	VkDeviceMemory	memory;

	uint32_t		offsets[12];

	int				size;
	void			*mapped;
	struct {
		VkBuffer		buffer;
		VkDeviceMemory	memory;
	} staging;
} VBO_t;

typedef struct IBO_s
{
	VkBuffer		buffer;
	VkDeviceMemory	memory;

	int				size;
	void			*mapped;

	struct {
		VkBuffer		buffer;
		VkDeviceMemory	memory;
	} staging;	
} IBO_t;

//===============================================================================

typedef enum {
	SS_BAD,
	SS_PORTAL,			// mirrors, portals, viewscreens
	SS_ENVIRONMENT,		// sky box
	SS_OPAQUE,			// opaque

	SS_DECAL,			// scorch marks, etc.
	SS_SEE_THROUGH,		// ladders, grates, grills that may have small blended edges
						// in addition to alpha test
	SS_BANNER,

	SS_INSIDE,			// inside body parts (i.e. heart)
	SS_MID_INSIDE,
	SS_MIDDLE,
	SS_MID_OUTSIDE,
	SS_OUTSIDE,			// outside body parts (i.e. ribs)

	SS_FOG,

	SS_UNDERWATER,		// for items that should be drawn in front of the water plane

	SS_BLEND0,			// regular transparency and filters
	SS_BLEND1,			// generally only used for additive type effects
	SS_BLEND2,
	SS_BLEND3,

	SS_BLEND6,
	SS_STENCIL_SHADOW,
	SS_ALMOST_NEAREST,	// gun smoke puffs

	SS_NEAREST			// blood blobs
} shaderSort_t;


#define MAX_SHADER_STAGES 8

typedef enum {
	GF_NONE,

	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH,
	GF_INVERSE_SAWTOOTH,

	GF_NOISE,
	GF_RAND

} genFunc_t;


typedef enum {
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_BULGE_UNIFORM,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT0,
	DEFORM_TEXT1,
	DEFORM_TEXT2,
	DEFORM_TEXT3,
	DEFORM_TEXT4,
	DEFORM_TEXT5,
	DEFORM_TEXT6,
	DEFORM_TEXT7,
	DEFORM_DISINTEGRATION
} deform_t;

typedef enum {
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_BLEND,
	AGEN_CONST,
	AGEN_DOT,
	AGEN_ONE_MINUS_DOT
} alphaGen_t;

typedef enum {
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING,	// tr.identityLight
	CGEN_IDENTITY,			// always (1,1,1,1)
	CGEN_ENTITY,			// grabbed from entity's modulate field
	CGEN_ONE_MINUS_ENTITY,	// grabbed from 1 - entity.modulate
	CGEN_EXACT_VERTEX,		// tess.vertexColors
	CGEN_VERTEX,			// tess.vertexColors * tr.identityLight
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,			// programmatically generated
	CGEN_LIGHTING_DIFFUSE,
	CGEN_LIGHTING_DIFFUSE_ENTITY, //diffuse lighting * entity
	CGEN_FOG,				// standard fog
	CGEN_CONST,				// fixed color
	CGEN_LIGHTMAPSTYLE,
	CGEN_DISINTEGRATION_1,
	CGEN_DISINTEGRATION_2
} colorGen_t;

typedef enum {
	TCGEN_BAD,
	TCGEN_IDENTITY,			// clear to 0,0
	TCGEN_LIGHTMAP,
	TCGEN_LIGHTMAP1,
	TCGEN_LIGHTMAP2,
	TCGEN_LIGHTMAP3,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_FOG,
	TCGEN_VECTOR			// S and T from world coordinates
} texCoordGen_t;

typedef enum {
	ACFF_NONE,
	ACFF_MODULATE_RGB,
	ACFF_MODULATE_RGBA,
	ACFF_MODULATE_ALPHA
} acff_t;

typedef enum {
	GLFOGOVERRIDE_NONE = 0,
	GLFOGOVERRIDE_BLACK,
	GLFOGOVERRIDE_WHITE,

	GLFOGOVERRIDE_MAX
} EGLFogOverride;

typedef struct waveForm_s {
	genFunc_t	func;

	float base;
	float amplitude;
	float phase;
	float frequency;
} waveForm_t;

#define TR_MAX_TEXMODS 4

typedef enum {
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE
} texMod_t;

#define	MAX_SHADER_DEFORMS	3
typedef struct deformStage_s {
	deform_t	deformation;			// vertex coordinate modification type

	vec3_t		moveVector;
	waveForm_t	deformationWave;
	float		deformationSpread;

	float		bulgeWidth;
	float		bulgeHeight;
	float		bulgeSpeed;
} deformStage_t;


typedef struct texModInfo_s {
	texMod_t		type;

	// used for TMOD_TURBULENT and TMOD_STRETCH
	waveForm_t		wave;

	// used for TMOD_TRANSFORM
	float			matrix[2][2];		// s' = s * m[0][0] + t * m[1][0] + trans[0]
	float			translate[2];		// t' = s * m[0][1] + t * m[0][1] + trans[1]
} texModInfo_t;

#define SSDEF_FACE_CAMERA     0x01
#define SSDEF_ALPHA_TEST      0x02
#define SSDEF_FACE_UP         0x04
#define SSDEF_FX_SPRITE       0x08
#define SSDEF_USE_FOG         0x10
#define SSDEF_FOG_MODULATE    0x20
#define SSDEF_ADDITIVE        0x40
#define SSDEF_FLATTENED       0x80

#define SSDEF_ALL             0xFF
#define SSDEF_COUNT           (SSDEF_ALL + 1)

typedef struct surfaceSprite_s
{
	int				type;
	float			width, height, density, wind, windIdle, fadeDist, fadeMax, fadeScale;
	float			fxAlphaStart, fxAlphaEnd, fxDuration, vertSkew;
	vec2_t			variance, fxGrow;
	int				facing;		// Hangdown on vertical sprites, faceup on others.
	uint32_t		ssbo_bits;
} surfaceSprite_t;

enum type_t
{
	SURFSPRITE_NONE,
	SURFSPRITE_VERTICAL,
	SURFSPRITE_ORIENTED,
	SURFSPRITE_EFFECT,
	SURFSPRITE_WEATHERFX,
	SURFSPRITE_FLATTENED,
};

enum surfaceSpriteOrientation_t
{
	SURFSPRITE_FACING_NORMAL,
	SURFSPRITE_FACING_UP,
	SURFSPRITE_FACING_DOWN,
	SURFSPRITE_FACING_ANY,
};

struct SurfaceSpriteBlock
{
	vec2_t fxGrow;
	float fxDuration;
	float fadeStartDistance;
	float fadeEndDistance;
	float fadeScale;
	float wind;
	float windIdle;
	float fxAlphaStart;
	float fxAlphaEnd;
	float pad0[2];
};

#define	MAX_IMAGE_ANIMATIONS	32

typedef struct textureBundle_s {
	image_t			*image[MAX_IMAGE_ANIMATIONS];
#ifdef USE_VK_PBR
	image_t			*deluxeMap;			// light directions of a lightmap bundle, or NULL
#endif

	texCoordGen_t	tcGen;
	vec3_t			*tcGenVectors;

	texModInfo_t	*texMods;
	short			numTexMods;
	short			numImageAnimations;
	float			imageAnimationSpeed;

	bool			oneShotAnimMap;				// check this
	bool			vertexLightmap;				// check this
	
	waveForm_t		rgbWave;
	colorGen_t		rgbGen;

	waveForm_t		alphaWave;
	alphaGen_t		alphaGen;

	byte			constantColor[4];			// for CGEN_CONST and AGEN_CONST
	acff_t			adjustColorsForFog;
	EGLFogOverride	mGLFogColorOverride;

	qboolean		isLightmap;
	qboolean		isVideoMap;
	unsigned int 	isScreenMap : 1;
	unsigned int 	dlight : 1;

	int				videoMapHandle;
	bool glow;
} textureBundle_t;


#define NUM_TEXTURE_BUNDLES 3

typedef struct shaderStage_s {
	bool			active;
	bool			isDetail;

	byte			index;						// index of stage
	byte			lightmapStyle[2];

	textureBundle_t	bundle[NUM_TEXTURE_BUNDLES];

	qboolean		depthFragment;

	uint32_t		stateBits;					// GLS_xxxx mask
	uint32_t		tessFlags;
	uint32_t		numTexBundles;

	int				mtEnv;						// 0, GL_MODULATE, GL_ADD, glCompat
	int				mtEnv3;						// 0, GL_MODULATE, GL_ADD, GL_DECAL

	surfaceSprite_t	*ss;

	// Whether this object emits a glow or not.
	bool			glow;


	uint32_t		vk_pipeline[2];
	uint32_t		vk_2d_pipeline;
	uint32_t		vk_portal_pipeline;
	uint32_t		vk_mirror_pipeline[2];
	uint32_t		vk_pipeline_df; // depthFragment
	uint32_t		vk_mirror_pipeline_df;
#ifdef USE_VBO
	uint32_t		rgb_offset[NUM_TEXTURE_BUNDLES]; // within current shader
	uint32_t		tex_offset[NUM_TEXTURE_BUNDLES]; // within current shader
#endif
#ifdef USE_VK_PBR
	// The PBR maps of the stage (PBR_HAS_*), which the path tracer reads.
	uint32_t		vk_pbr_flags;
	uint32_t		vk_light_flags;		// LIGHTDEF_*: the light type of a lit stage
	image_t			*normalMap;
	image_t			*physicalMap;
	uint32_t		normalMapType;
	uint32_t		physicalMapType;
	vec4_t			normalScale;
	vec4_t			specularScale;
	float			parallaxBias;
#endif
} shaderStage_t;

struct shaderCommands_s;

#define LIGHTMAP_2D			-4		// shader is for 2D rendering
#define LIGHTMAP_BY_VERTEX	-3		// pre-lit triangle models
#define LIGHTMAP_WHITEIMAGE	-2
#define	LIGHTMAP_NONE		-1

typedef enum {
	FP_NONE,		// surface is translucent and will just be adjusted properly
	FP_EQUAL,		// surface is opaque but possibly alpha tested
	FP_LE,			// surface is trnaslucent, but still needs a fog pass (fog surface)
	FP_GLFOG
} fogPass_t;

typedef struct skyParms_s {
	float		cloudHeight;
	image_t* outerbox[6], * innerbox[6];
} skyParms_t;

typedef struct fogParms_s {
	vec3_t	color;
	float	depthForOpaque;
} fogParms_t;

typedef struct shader_s {
	char		name[MAX_QPATH];					// game path, including extension
	int			lightmapSearchIndex[MAXLIGHTMAPS];	// for a shader to match, both name and lightmapIndex must match
	int			lightmapIndex[MAXLIGHTMAPS];		// for a shader to match, both name and lightmapIndex must match
	byte		styles[MAXLIGHTMAPS];

	int			index;								// this shader == tr.shaders[index]
	int			sortedIndex;						// this shader == tr.sortedShaders[sortedIndex]

	float		sort;								// lower numbered shaders draw before higher numbered

	int			surfaceFlags;						// if explicitlyDefined, this will have SURF_* flags
	int			contentFlags;

	qboolean	defaultShader;						// we want to return index 0 if the shader failed to
													// load for some reason, but R_FindShader should
													// still keep a name allocated for it, so if
													// something calls RE_RegisterShader again with
													// the same name, we don't try looking for it again
	qboolean	missingShader;						// no script and no image of this name
	qboolean	explicitlyDefined;					// found in a .shader file
	qboolean	entityMergable;						// merge across entites optimizable (smoke, blood)

	qboolean	isSky;
	skyParms_t	*sky;
	fogParms_t	*fogParms;

	float		portalRange;						// distance to fog out at
	float		portalRangeR;

	int			multitextureEnv;					// 0, GL_MODULATE, GL_ADD (FIXME: put in stage)

	cullType_t	cullType;							// CT_FRONT_SIDED, CT_BACK_SIDED, or CT_TWO_SIDED
	qboolean	polygonOffset;						// set for decals and other items that must be offset

	unsigned	noMipMaps:1;						// for console fonts, 2D elements, etc.
	unsigned	noPicMip:1;							// for images that must always be full resolution
	unsigned	noLightScale:1;
	unsigned	noTC:1;								// for images that don't want to be texture compressed (eg skies)

	fogPass_t	fogPass;							// draw a blended pass, possibly with depth test equals

	qboolean	fogCollapse;
	int			tessFlags;
	
	shaderStage_t	*stages[MAX_SHADER_STAGES];
	deformStage_t	*deforms[MAX_SHADER_DEFORMS];

	short		numDeforms;
	short		numUnfoggedPasses;

#ifdef USE_PMLIGHT
	int			lightingStage;
	int			lightingBundle;
#endif

	void		(*optimalStageIteratorFunc)(void);

	float		clampTime;							// time this shader is clamped to
	float		timeOffset;                         // current time offset for this shader

	
	qboolean	useDistortion;
	qboolean	hasGlow;							// True if this shader has a stage with glow in it (just an optimization).

	int			hasScreenMap;

	struct {
		int			num_stages;
		uint32_t	ssbo_index;
	} surface_sprites;


#ifdef USE_VBO
	// VBO structures
	qboolean	isStaticShader;
	int			svarsSize;
	int			iboOffset;
	int			vboOffset;
	int			normalOffset;
#ifdef USE_VK_PBR
	int			qtangentOffset;
	int			lightdirOffset;
#endif
	int			numIndexes;
	int			numVertexes;
	int			curVertexes;
	int			curIndexes;
#endif

	struct shader_s		*remappedShader;			// current shader this one is remapped too
	struct	shader_s	*next;
#ifdef USE_RTX
	qboolean			sun;			// authored by q3map_sun on the sky shader
	vec3_t				sunColor;
	int					surfacelight;	// emissive surfaces feed the path tracer's light list
	struct shader_s		*updatedShader;
#endif
} shader_t;

/*
Ghoul2 Insert Start
*/
 // bogus little registration system for hit and location based damage files in hunk memory
typedef struct hitMatReg_s {
	byte	*loc;
	int		width;
	int		height;
	char	name[MAX_QPATH];
} hitMatReg_t;

#define MAX_HITMAT_ENTRIES 1000

extern hitMatReg_t		hitMatReg[MAX_HITMAT_ENTRIES];

/*
Ghoul2 Insert End
*/


// trRefdef_t holds everything that comes in refdef_t,
// as well as the locally generated scene information
typedef struct trRefdef_s {
	int					x, y, width, height;
	float				fov_x, fov_y;
	vec3_t				vieworg;
	matrix3_t			viewaxis;					// transformation matrix

	int					time;						// time in milliseconds for shader effects and other time dependent rendering issues
	int					frametime;
	int					rdflags;					// RDF_NOWORLDMODEL, etc
	qboolean			doLAGoggles;				// SP light amp goggles, set by RE_LAGoggles, cleared by RE_ClearScene
	int					fogIndex;					// fog brush that contains vieworg, 0 for none

	// 1 bits will prevent the associated area from rendering at all
	byte				areamask[MAX_MAP_AREA_BYTES];
	qboolean			areamaskModified;			// qtrue if areamask changed since last scene

	float				floatTime;					// tr.refdef.time / 1000.0

	char				text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];	// text messages for deform text shaders

	int					num_entities;
	trRefEntity_t		*entities;

	int					num_dlights;
	struct dlight_s		*dlights;

	int					numPolys;
	struct srfPoly_s	*polys;

	int					numDrawSurfs;
	struct drawSurf_s	*drawSurfs;

#ifdef USE_PMLIGHT
	int					numLitSurfs;
	struct litSurf_s	*litSurfs;
#endif

	qboolean			switchRenderPass;
	qboolean			needScreenMap;
#ifdef USE_RTX
	ref_feedback_t		feedback;
#endif

	int					g2SnapFirst;				// Ghoul2 snapshot models of this scene: backEndData->g2.models[first, first + count)
	int					g2SnapCount;
} trRefdef_t;


//=================================================================================

// skins allow models to be retextured without modifying the model file
#ifndef USE_OPENJK
typedef struct {
	char		name[MAX_QPATH];
	shader_t	*shader;
} skinSurface_t;

typedef struct skin_s {
	char			name[MAX_QPATH];		// game path, including extension
	int				numSurfaces;
	skinSurface_t	*surfaces[128];
} skin_t;
#else
typedef _skinSurface_t skinSurface_t;
#endif

typedef struct fog_s {
	int				originalBrushNumber;
	vec3_t			bounds[2];

	vec4_t			color;
	unsigned		colorInt;				// in packed byte format
	unsigned char	colorRGBA[4];			// in packed byte format

	float			tcScale;				// texture coordinate vector scales
	fogParms_t		parms;

	// for clipping distance in fog when outside
	qboolean		hasSurface;
	float			surface[4];
} fog_t;

typedef struct {
	float			eyeT;
	qboolean		eyeOutside;
	vec4_t			fogDistanceVector;
	vec4_t			fogDepthVector;
	const float		*fogColor; // vec4_t
} fogProgramParms_t;

typedef enum {
	PV_NONE = 0,
	PV_PORTAL,								// this view is through a portal
	PV_MIRROR,								// portal + inverted face culling
	PV_COUNT
} portalView_t;

typedef struct viewParms_s {
	orientationr_t	ori;					// Can't use "or" as it is a reserved word with gcc DREWS 2/2/2002
	orientationr_t	world;
	vec3_t			pvsOrigin;				// may be different than or.origin for portals
	portalView_t	portalView;				// define view type for default, portal or mirror 
	int				frameSceneNum;			// copied from tr.frameSceneNum
	int				frameCount;				// copied from tr.frameCount
	cplane_t		portalPlane;			// clip anything behind this if mirroring
	int				viewportX, viewportY, viewportWidth, viewportHeight;
	int				scissorX, scissorY, scissorWidth, scissorHeight;
	float			fovX, fovY;
	float			projectionMatrix[16];
	cplane_t		frustum[5];
	vec3_t			visBounds[2];
	float			zFar;
	float			zNear;
#ifdef USE_PMLIGHT
	// each view will have its own dlight set
	unsigned int	num_dlights;
	struct dlight_s	*dlights;
#endif
#ifdef VK_CUBEMAP
	int				targetCube;				// 1 + index of the cubemap that the view captures, 0 for none
	int				targetCubeFace;
#endif
} viewParms_t;

/*
==============================================================================

SURFACES

==============================================================================
*/
// any changes in surfaceType must be mirrored in rb_surfaceTable[]
typedef enum surfaceType_e {
	SF_BAD,
	SF_SKIP,				// ignore
	SF_FACE,
	SF_GRID,
	SF_TRIANGLES,
	SF_POLY,
	SF_MDV,
	SF_MDX,
	SF_FLARE,
	SF_ENTITY,				// beams, rails, lightning, etc that can be determined by entity
	SF_VBO_MDVMESH,
	SF_SPRITES,

	SF_NUM_SURFACE_TYPES,

	SF_MAX = 0x7fffffff				// ensures that sizeof( surfaceType_t ) == sizeof( int )
} surfaceType_t;

typedef struct drawSurf_s {
	sortKey_t			sort;			// bit combination for fast compares
	surfaceType_t		*surface;		// any of surface*_t
#ifdef USE_RTX
	vk_blas_t			*blas;
#endif
#ifdef VK_CUBEMAP
	int					cubemapIndex;	// 1 + index of the cubemap, 0 for none
#endif
} drawSurf_t;

#ifdef USE_PMLIGHT
typedef struct litSurf_s {
	sortKey_t			sort;			// bit combination for fast compares
	surfaceType_t		*surface;		// any of surface*_t
	struct litSurf_s	*next;
} litSurf_t;
#endif

#define	MAX_FACE_POINTS		64

#define	MAX_PATCH_SIZE		32			// max dimensions of a patch mesh in map file
#define	MAX_GRID_SIZE		65			// max dimensions of a grid mesh in memory

// when cgame directly specifies a polygon, it becomes a srfPoly_t
// as soon as it is called
typedef struct srfPoly_s {
	surfaceType_t	surfaceType;
	qhandle_t		hShader;
	int				fogIndex;
	int				numVerts;
	polyVert_t		*verts;
} srfPoly_t;

typedef struct srfDisplayList_s {
	surfaceType_t	surfaceType;
	int				listNum;
} srfDisplayList_t;

typedef struct srfFlare_s {
	surfaceType_t	surfaceType;
	vec3_t			origin;
	vec3_t			normal;
	vec3_t			color;
	shader_t		*shader;
} srfFlare_t;


#ifdef USE_VBO_SS
#define SS_MAX_GROUP						1024
#define SS_MAX_GROUP_CMD					1024

#define SS_ENT_BITS							REFENTITYNUM_BITS
#define SS_VBO_BITS							10
#define SS_FOG_BITS							7
#define SS_ENT_MASK							((1U << SS_ENT_BITS) - 1)
#define SS_VBO_MASK							((1U << SS_VBO_BITS) - 1)
#define SS_FOG_MASK							((1U << SS_FOG_BITS) - 1)

#define SS_PACK_SURF_BITS(ent,vbo,fog)		(((ent) & SS_ENT_MASK) | (((vbo) & SS_VBO_MASK) << SS_ENT_BITS) | (((fog) & SS_FOG_MASK) << (SS_ENT_BITS + SS_VBO_BITS)))
#define SS_PACK_SSBO_BITS(index, offset)	((uint32_t)(((uint32_t)(index) << 16) | ((uint32_t)(offset) & 0xFFFF)))

#define SS_UNPACK_SSBO_INDEX(packed)		( ( packed ) >> 16 )
#define SS_UNPACK_SSBO_OFFSET(packed)		( ( packed ) & 0xFFFF )
#define SS_UNPACK_ENT(surf_bits)			((surf_bits) & SS_ENT_MASK)
#define SS_UNPACK_VBO(surf_bits)			(((surf_bits) >> SS_ENT_BITS) & SS_VBO_MASK)
#define SS_UNPACK_FOG(surf_bits)			(((surf_bits) >> (SS_ENT_BITS + SS_VBO_BITS)) & SS_FOG_MASK)

struct sprite_t
{
	vec4_t		position;
	vec3_t		normal;
	color4ub_t	color;
	vec2_t		widthHeight;
	vec2_t		skew;
};

struct spriteStage_t
{
	shader_t				*shader;
	VBO_t					*vbo;
	const surfaceSprite_t	*sprite;
	int						firstInstance;
	int						instanceCount;
	int						fogIndex;
};

typedef struct {
	shader_t		*shader;
	uint32_t		surf_bits;	// ent/vbo/fog
	uint32_t		ssbo_bits;	// index/offset
} vk_ss_group_def_t;

typedef struct { // indirect command
	int numInstances;
	int firstInstance;
} vk_ss_group_cmd_t;

typedef struct {
	vk_ss_group_def_t	def;
	vk_ss_group_cmd_t	cmd[SS_MAX_GROUP_CMD];
	int					num_commands;
} vk_ss_group_t;

typedef struct {
	surfaceType_t	surfaceType;
} srfSprites_t;
#endif

#define VERTEX_LM			5
#define	VERTEXSIZE			( 6 + ( MAXLIGHTMAPS * 3 ) )
#define	VERTEX_COLOR		( 5 + ( MAXLIGHTMAPS * 2 ) )
#define	VERTEX_FINAL_COLOR	( 5 + ( MAXLIGHTMAPS * 3 ) )

// drawVert_t of the renderer, with the tangent of the vertex (xyz, w the bitangent sign).
typedef struct srfVert_s {
	vec3_t		xyz;
	float		st[2];
	float		lightmap[MAXLIGHTMAPS][2];
	vec3_t		normal;
#ifdef USE_VK_PBR
	vec4_t		qtangent;
	vec4_t		lightdir;		// world space direction to the light, from the light grid
#endif
	byte		color[MAXLIGHTMAPS][4];
} srfVert_t;


#ifdef _G2_GORE
typedef struct
{
	vec4_t		verts;
	vec4_t		normals;
	vec2_t		texcoords;
	byte		bonerefs[4];
	byte		weights[4];
	vec4_t		tangents;
} g2GoreVert_t;

typedef struct srfG2GoreSurface_s
{
	surfaceType_t   surfaceType;

	// indexes
	int             numIndexes;
	glIndex_t      *indexes;

	// vertexes
	int             numVerts;
	g2GoreVert_t    *verts;

	// BSP VBO offsets
	int             firstVert;
	int             firstIndex;

} srfG2GoreSurface_t;

#endif
typedef struct srfGridMesh_s {
	surfaceType_t	surfaceType;

	// culling information
	vec3_t			meshBounds[2];
	vec3_t			localOrigin;
	float			meshRadius;

	// lod information, which may be different
	// than the culling information to allow for
	// groups of curves that LOD as a unit
	vec3_t			lodOrigin;
	float			lodRadius;
	int				lodFixed;
	int				lodStitched;

#ifdef USE_VBO
	int				vboItemIndex;
	int				vboExpectIndices;
	int				vboExpectVertices;
#endif

	// vertexes
	int				width, height;
	float			*widthLodError;
	float			*heightLodError;
	srfVert_t		verts[1];				// variable sized
} srfGridMesh_t;

typedef struct srfSurfaceFace_s {
	surfaceType_t	surfaceType;
	cplane_t		plane;

#ifdef USE_VBO
	int				vboItemIndex;
#endif
	float			*normals;
#ifdef USE_VK_PBR
	float			*qtangents;				// vec4_t for each point
	float			*lightdir;				// vec4_t for each point
#endif

	// triangle definitions (no normals at points)
	int				numPoints;
	int				numIndices;
	int				ofsIndices;

	float			points[1][VERTEXSIZE];	// variable sized
											// there is a variable length list of indices here also
} srfSurfaceFace_t;

// misc_models in maps are turned into direct geometry by q3map
typedef struct srfTriangles_s {
	surfaceType_t	surfaceType;

#ifdef USE_VBO
	int				vboItemIndex;
#endif
	// culling information (FIXME: use this!)
	vec3_t			bounds[2];
//	vec3_t			localOrigin;
//	float			radius;

	// triangle definitions
	int				numIndexes;
	int				*indexes;

	int				numVerts;
	srfVert_t		*verts;
} srfTriangles_t;

extern	void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/
//
// in memory representation
//

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

typedef struct msurface_s {
	int					viewCount;		// if == tr.viewCount, already added
	struct shader_s		*shader;
	int					fogIndex;
#ifdef USE_PMLIGHT
	int					vcVisible;		// if == tr.viewCount, is actually VISIBLE in this frame, i.e. passed facecull and has been added to the drawsurf list
	int					lightCount;		// if == tr.lightCount, already added to the litsurf list for the current light
#endif

	struct {
		uint32_t		num_stages;
		spriteStage_t	*stage;
	} surface_sprites;

#ifdef USE_RTX
	vk_blas_t			*blas;
	qboolean			added;
	qboolean			skip;
	qboolean			notBrush;
#endif
#ifdef VK_CUBEMAP
	int					cubemapIndex;	// 1 + index of the nearest cubemap, 0 for none
#endif
	surfaceType_t		*data;			// any of srf*_t
} msurface_t;

#define	CONTENTS_NODE		-1

typedef struct mnode_s {
	// common with leaf and node
	int				contents;		// -1 for nodes, to differentiate from leafs
	int				visframe;		// node needs to be traversed if current
	vec3_t			mins, maxs;		// for bounding box culling
	struct mnode_s	*parent;

	// node specific
	cplane_t		*plane;
	struct mnode_s	*children[2];

	// leaf specific
	int				cluster;
	int				area;

	msurface_t		**firstmarksurface;
	int				nummarksurfaces;
} mnode_t;

typedef struct bmodel_s {
	vec3_t		bounds[2];			// for culling
	msurface_t	*firstSurface;
	int			numSurfaces;
#ifdef USE_RTX
	model_geometry_t geometry;

	vec3_t			center;
	vec3_t			aabb_min;
	vec3_t			aabb_max;

	int				num_light_polys;
	int				allocated_light_polys;
	light_poly_t	*light_polys;

	bool transparent;
	bool masked;
#endif
} bmodel_t;

typedef struct
{
	byte		ambientLight[MAXLIGHTMAPS][3];
	byte		directLight[MAXLIGHTMAPS][3];
	byte		styles[MAXLIGHTMAPS];
	byte		latLong[2];
} mgrid_t;

typedef struct world_s {
	char		name[MAX_QPATH];		// ie: maps/tim_dm2.bsp
	char		baseName[MAX_QPATH];	// ie: tim_dm2

	int			dataSize;

	int			numShaders;
	dshader_t	*shaders;

	bmodel_t	*bmodels;

	int			numplanes;
	cplane_t	*planes;

	int			numnodes;				// includes leafs
	int			numDecisionNodes;
	mnode_t		*nodes;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	int			numfogs;
	fog_t		*fogs;
	int			globalFog;


	vec3_t		lightGridOrigin;
	vec3_t		lightGridSize;
	vec3_t		lightGridInverseSize;
	int			lightGridBounds[3];

	int			lightGridOffsets[8];

	vec3_t		lightGridStep;

	mgrid_t		*lightGridData;
	word		*lightGridArray;
	int			numGridArrayElements;


	int			numClusters;
	int			clusterBytes;
	const byte	*vis;					// may be passed in by CM_LoadMap to save space
#ifdef USE_RTX
	aabb_t			world_aabb;
	aabb_t			*cluster_aabbs;

	vkgeometry_t	geometry;

	const byte		*vis2;
	int				numvisibility;

	int				num_cluster_lights;
	int				*cluster_light_offsets;
	int				*cluster_lights;

	int				num_light_polys;
	int				allocated_light_polys;
	light_poly_t	*light_polys;

	int				num_bmodels;
	byte			sky_visibility[VIS_MAX_BYTES];
#endif

	byte		*novis;					// clusterBytes of 0xff

	char		*entityString;
	char		*entityParsePoint;
} world_t;

//======================================================================

/*
==============================================================================
MDV MODELS - meta format for vertex animation models like .md2, .md3, .mdc
==============================================================================
*/
typedef struct
{
	float           bounds[2][3];
	float           localOrigin[3];
	float           radius;
} mdvFrame_t;

typedef struct
{
	float           origin[3];
	float           axis[3][3];
} mdvTag_t;

typedef struct
{
	char            name[MAX_QPATH];	// tag name
} mdvTagName_t;

typedef struct
{
	vec3_t          xyz;
	vec3_t          normal;
	vec3_t          tangent;
	vec3_t          bitangent;
} mdvVertex_t;

typedef struct
{
	float           st[2];
} mdvSt_t;

typedef struct mdvSurface_s
{
	surfaceType_t   surfaceType;

	char            name[MAX_QPATH];	// polyset name

	int             numShaderIndexes;
	int				*shaderIndexes;

	int             numVerts;
	mdvVertex_t    *verts;
	mdvSt_t        *st;

	int             numIndexes;
	glIndex_t      *indexes;

	struct mdvModel_s *model;
} mdvSurface_t;

typedef struct srfVBOMDVMesh_s
{
	surfaceType_t   surfaceType;

	struct mdvModel_s *mdvModel;
	struct mdvSurface_s *mdvSurface;

	// backEnd stats
	int				indexOffset;
	int             numIndexes;
	int             numVerts;
	glIndex_t       minIndex;
	glIndex_t       maxIndex;

#ifdef USE_RTX
	maliasmesh_t	rtx_mesh;
#endif

	// static render data
	VBO_t          *vbo;
	IBO_t          *ibo;
} srfVBOMDVMesh_t;

typedef struct mdvModel_s
{
	int             numFrames;
	mdvFrame_t     *frames;

	int             numTags;
	mdvTag_t       *tags;
	mdvTagName_t   *tagNames;

	int             numSurfaces;
	mdvSurface_t   *surfaces;

	int             numVBOSurfaces;
	srfVBOMDVMesh_t  *vboSurfaces;

	int             numSkins;
} mdvModel_t;

#ifdef USE_VBO_GHOUL2
typedef struct mdxmVBOMesh_s
{
	surfaceType_t surfaceType;

	int indexOffset;
	int minIndex;
	int maxIndex;
	int numIndexes;
	int numVertexes;

#ifdef USE_RTX
	maliasmesh_t	rtx_mesh;
#ifndef USE_RTX_GLOBAL_MODEL_VBO
	int vertexOffset;
	model_vbo_t *vbo_rtx;
#endif
#endif

	VBO_t *vbo;
	IBO_t *ibo;
} mdxmVBOMesh_t;

typedef struct mdxmVBOModel_s
{
	int numVBOMeshes;
	mdxmVBOMesh_t *vboMeshes;
#ifdef USE_RTX
	uint32_t	model_index;
	model_vbo_t vbo_rtx;
#endif

	VBO_t *vbo;
	IBO_t *ibo;
} mdxmVBOModel_t;
#endif

typedef struct mdxmData_s
{
	mdxmHeader_t	*header;
#ifdef USE_VBO_GHOUL2
	mdxmVBOModel_t	*vboModels;
#endif
} mdxmData_t;


typedef enum {
	MOD_BAD,
	MOD_BRUSH,
	MOD_MESH,
	/*
	Ghoul2 Insert Start
	*/
	MOD_MDXM,
	MOD_MDXA
	/*
	Ghoul2 Insert End
	*/

} modtype_t;

typedef struct model_s {
	char		name[MAX_QPATH];
	modtype_t	type;
	int			index;				// model = tr.models[model->mod_index]

	int			dataSize;			// just for listing purposes

	struct // union presents issues with glm world models like weapons ..
	{
		bmodel_t		*bmodel;			// type == MOD_BRUSH
		mdvModel_t		*mdv[MD3_MAX_LODS];	// type == MOD_MESH
		mdxmData_t		*glm;				// type == MOD_MDXM which is a GHOUL II Mesh file NOT a GHOUL II animation file
		mdxaHeader_t	*gla;				// type == MOD_MDXA which is a GHOUL II Animation file
	} data;

	unsigned char	numLods;
#ifdef USE_RTX
	int				num_light_polys;
	light_poly_t	*light_polys;
#endif
	bool			bspInstance;			// model is a bsp instance
} model_t;


#define	MAX_MOD_KNOWN	1024

void		R_ModelInit ( void );

model_t		*R_GetModelByHandle( qhandle_t hModel );
int			R_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame,
					 float frac, const char *tagName );
void		R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs );

void		R_Modellist_f ( void );

//====================================================


#define	MAX_DRAWIMAGES			4096
#define	MAX_LIGHTMAPS			256
#define	MAX_SKINS				1024

#define	MAX_DRAWSURFS			0x10000
#define	DRAWSURF_MASK			( MAX_DRAWSURFS - 1 )
// A cubemap face is added only below this count, so that one face (up to about 20000) and the scene still fit in the ring.
#define	CUBEMAP_DRAWSURF_LIMIT	( MAX_DRAWSURFS - 40960 )

/*

the drawsurf sort data is packed into a single 64 bit value so it can be
compared quickly during the sorting process (the radix sort reads 5 bytes)

the bits are allocated as follows:

33    : RF_ALPHA_FADE (drawn last)
19-32 : sorted shader index
6-18  : entity index (REFENTITYNUM_BITS)
1-5   : fog index
0     : dlightmap index
*/

#define	DLIGHT_BITS 1 // qboolean in opengl1 renderer
#define	DLIGHT_MASK ( ( 1 << DLIGHT_BITS) - 1 )
#define	FOGNUM_BITS 5
#define	FOGNUM_MASK ( (1 << FOGNUM_BITS ) - 1 )

#define	QSORT_FOGNUM_SHIFT	DLIGHT_BITS
#define	QSORT_REFENTITYNUM_SHIFT ( QSORT_FOGNUM_SHIFT + FOGNUM_BITS )
#define	QSORT_SHADERNUM_SHIFT	( QSORT_REFENTITYNUM_SHIFT + REFENTITYNUM_BITS )
#define	QSORT_ALPHAFADE_SHIFT	( QSORT_SHADERNUM_SHIFT + SHADERNUM_BITS )
#define	QSORT_ALPHAFADE_BIT		( (sortKey_t)1 << QSORT_ALPHAFADE_SHIFT )
#define	QSORT_BYTES				5	// bytes of the key that R_RadixSort sorts
#if QSORT_ALPHAFADE_SHIFT >= QSORT_BYTES * 8
	#error "Need to update sorting, too many bits."
#endif
#define QSORT_REFENTITYNUM_MASK ( (sortKey_t)REFENTITYNUM_MASK << QSORT_REFENTITYNUM_SHIFT )

/*
** performanceCounters_t
*/
typedef struct frontEndCounters_s {
	int		c_sphere_cull_patch_in, c_sphere_cull_patch_clip, c_sphere_cull_patch_out;
	int		c_box_cull_patch_in, c_box_cull_patch_clip, c_box_cull_patch_out;
	int		c_sphere_cull_md3_in, c_sphere_cull_md3_clip, c_sphere_cull_md3_out;
	int		c_box_cull_md3_in, c_box_cull_md3_clip, c_box_cull_md3_out;

	int		c_leafs;
	int		c_dlightSurfaces;
	int		c_dlightSurfacesCulled;
#ifdef USE_PMLIGHT
	int		c_light_cull_out;
	int		c_light_cull_in;
	int		c_lit_leafs;
	int		c_lit_surfs;
	int		c_lit_culls;
	int		c_lit_masks;
#endif
} frontEndCounters_t;

#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_SIZE2		10
#define FUNCTABLE_MASK		( FUNCTABLE_SIZE - 1 )

// the renderer front end should never modify glstate_t
typedef struct glstate_s {
	int			currenttextures[2];
	qboolean	finishCalled;
	int			texEnv[2];
	int			faceCulling;
	uint32_t	glStateBits;
} glstate_t;

typedef struct glstatic_s {
	// unmodified width/height according to actual \r_mode*
	int windowWidth;
	int windowHeight;
	int captureWidth;
	int captureHeight;
	int initTime;
} glstatic_t;

typedef enum {
	MI_NONE,
	MI_NVX,
	MI_ATI
} memInfo_t;

typedef enum {
	TCR_NONE = 0x0000,
	TCR_RGTC = 0x0001,
	TCR_BPTC = 0x0002,
} textureCompressionRef_t;

// We can't change glConfig_t without breaking DLL/vms compatibility, so
// store extensions we have here.
typedef struct {
	qboolean    intelGraphics;
	qboolean	occlusionQuery;

	int glslMajorVersion;
	int glslMinorVersion;
	int glslMaxAnimatedBones;

	memInfo_t memInfo;

	qboolean	framebufferObject;
	int			maxRenderbufferSize;
	int			maxColorAttachments;

	textureCompressionRef_t textureCompression;
	qboolean				textureFloat;
	
	qboolean swizzleNormalmap;

	qboolean framebufferMultisample;
	qboolean framebufferBlit;

	qboolean depthClamp;
	qboolean seamlessCubeMap;

	qboolean vertexArrayObject;
	qboolean directStateAccess;
} glRefConfig_t;

typedef struct backEndCounters_s {
	int		c_surfaces, c_shaders, c_vertexes, c_indexes, c_totalIndexes;
	float	c_overDraw;

	int		c_dlightVertexes;
	int		c_dlightIndexes;

	int		c_flareAdds;
	int		c_flareTests;
	int		c_flareRenders;

	int		msec;			// total msec for backend run
#ifdef USE_PMLIGHT
	int		c_lit_batches;
	int		c_lit_vertices;
	int		c_lit_indices;
	int		c_lit_indices_latecull_in;
	int		c_lit_indices_latecull_out;
	int		c_lit_vertices_lateculltest;
#endif
} backEndCounters_t;

typedef struct videoFrameCommand_s {
	int           commandId;
	int           width;
	int           height;
	byte          *captureBuffer;
	byte          *encodeBuffer;
	qboolean      motionJpeg;
} videoFrameCommand_t;

enum {
	SCREENSHOT_TGA = 1 << 0,
	SCREENSHOT_JPG = 1 << 1,
	SCREENSHOT_PNG = 1 << 2,
	SCREENSHOT_AVI = 1 << 3 // take video frame
};

// all state modified by the back end is seperated
// from the front end state
typedef struct backEndState_s {
	trRefdef_t			refdef;
	viewParms_t			viewParms;
	orientationr_t		ori;					// Can't use or as it is a c++ reserved word DREWS 2/2/2002
	backEndCounters_t	pc;
	qboolean			isHyperspace;
	trRefEntity_t		*currentEntity;
	qboolean			skyRenderedThisView;	// flag for drawing sun
	qboolean			projection2D;			// if qtrue, drawstretchpic doesn't need to change modes
	color4ub_t			color2D;
	qboolean			vertexes2D;				// shader needs to be finished
	trRefEntity_t		entity2D;				// currentEntity will point at this when doing 2D rendering

	int			screenshotMask;					// tga | jpg | bmp
	char		screenshotTGA[MAX_OSPATH];
	char		screenshotJPG[MAX_OSPATH];
	char		screenshotPNG[MAX_OSPATH];
	qboolean	screenShotTGAsilent;
	qboolean	screenShotJPGsilent;
	qboolean	screenShotPNGsilent;
	videoFrameCommand_t	vcmd;					// avi capture

	qboolean doneShadows;
	qboolean doneSurfaces;						// done any 3d surfaces already
	qboolean screenMapDone;
	qboolean doneBloom;

	qboolean hasGlowSurfaces;					// renderdoc shows empty dglow pass, or passes with 2 or 3 surfaces. maybe use a min surf count instead?
	qboolean isGlowPass;

	qboolean hasRefractionSurfaces;
	qboolean refractionFill;
} backEndState_t;

typedef struct drawSurfsCommand_s drawSurfsCommand_t;

/*
** trGlobals_t
**
** Most renderer globals are defined here.
** backend functions should never modify any of these fields,
** but may read fields that aren't dynamically modified
** by the frontend.
*/

#define NUM_SCRATCH_IMAGES 32

typedef struct {
    image_t **items;
    uint32_t count;
    uint32_t capacity;
} image_pool_t;

typedef struct trGlobals_s {
	qboolean				registered;			// cleared at shutdown, set at beginRegistration
	qboolean				inited;				// cleared at shutdown, set at vk_create_window

	window_t				window;

	int						visCount;			// incremented every time a new vis cluster is entered
	int						frameCount;			// incremented every frame
	int						modelEpoch;			// incremented by R_ModelInit; invalidates resolved ghoul2 model pointers
	int						sceneCount;			// incremented every scene
	int						viewCount;			// incremented every view (twice a scene if portaled)
												// and every R_MarkFragments call
#ifdef USE_PMLIGHT
	int						lightCount;			// incremented for each dlight in the view
#endif

	int						frameSceneNum;		// zeroed at RE_BeginFrame

	qboolean				worldMapLoaded;
	qboolean				worldInternalLightmapping; // qtrue indicates lightmap atlasing
	world_t					*world;
	char					worldDir[MAX_QPATH];// ie: maps/tim_dm2 (copy of world_t::name sans extension but still includes the path)

	const byte				*externalVisData;	// from RE_SetWorldVisData, shared with CM_Load

	image_t					*defaultImage;
	image_t					*scratchImage[NUM_SCRATCH_IMAGES];
	image_t					*fogImage;
	image_t					*dlightImage;		// inverse-quare highlight for projective adding
	image_t					*flareImage;
	image_t					*whiteImage;		// full of 0xff
	image_t					*physDotImage;		// soft round dot of the phys editor
#ifdef USE_VK_PBR
	image_t					*brdfLutImage;		// PBR environment BRDF
#endif
#ifdef VK_CUBEMAP
	int						numCubemaps;
	cubemap_t				*cubemaps;
	int						numCubemapsCaptured;
	int						numCubemapFacesCaptured;	// faces done of the probe numCubemapsCaptured
#endif
	image_t					*blackImage;			
	image_t					*identityLightImage;// full of tr.identityLightByte

	shader_t				*defaultShader;
	shader_t				*whiteShader;
	shader_t				*cinematicShader;
	shader_t				*beamShader;	// white, additive, vertex colour: the RT_BEAM tube under RTX
	shader_t				*shadowShader;
	shader_t				*physLineShader;	// lines and dots of the phys editor: vertex colour, depth test
	shader_t				*physHiddenShader;	// the same without depth test, for the hidden parts
	shader_t				*distortionShader;
	shader_t				*projectionShadowShader;

	shader_t				*flareShader;
	shader_t				*sunShader;

	int						numLightmaps;
	image_t					**lightmaps;
#ifdef USE_VK_PBR
	image_t					**deluxemaps;			// NULL when the map has none or r_deluxeMapping is 0
	qboolean				worldDeluxeMapping;		// each lightmap is followed by its deluxe map
#endif

	int						lightmapAtlasSize[2];
	int						lightmapsPerAtlasSide[2];

	trRefEntity_t			*currentEntity;
	trRefEntity_t			worldEntity;		// point currentEntity at this when rendering world
	int						currentEntityNum;
	sortKey_t				shiftedEntityNum;	// currentEntityNum << QSORT_REFENTITYNUM_SHIFT
	model_t					*currentModel;

	viewParms_t				viewParms;

	float					identityLight;		// 1.0 / ( 1 << overbrightBits )
	int						identityLightByte;	// identityLight * 255
	int						overbrightBits;		// r_overbrightBits->integer, but set to 0 if no hw gamma

	orientationr_t			ori;				// for current entity

	trRefdef_t				refdef;

	int						viewCluster;
#ifdef USE_PMLIGHT
	dlight_t*				light;				// current light during R_RecursiveLightNode
#endif
	vec3_t					sunLight;			// from the sky shader for this level
	vec3_t					sunDirection;
	int						sunSurfaceLight;	// from the sky shader for this level
	vec3_t					sunAmbient;			// from the sky shader	(only used for John's terrain system)

	frontEndCounters_t		pc;
	int						frontEndMsec;		// not in pc due to clearing issue

	//
	// put large tables at the end, so most elements will be
	// within the +/32K indexed range on risc processors
	//
	model_t					*models[MAX_MOD_KNOWN];
	int						numModels;

	image_pool_t			images;

	world_t					bspModels[MAX_SUB_BSP];
	int						numBSPModels;
	int						currentLevel;

	int						numVBOs;
	VBO_t					*vbos[4069];

	int						numIBOs;
	IBO_t					*ibos[4069];

#ifdef _G2_GORE
	VBO_t					*goreVBO;
	int						*goreVBOIndex;
	int						goreVBOCurrentIndex;
	IBO_t					*goreIBO;
	int						goreIBOCurrentIndex;
#endif

#ifdef USE_VBO_SS
	struct {
		VBO_t					*vbo;
		IBO_t					*ibo;
	} ss;
#endif

	// shader indexes from other modules will be looked up in tr.shaders[]
	// shader indexes from drawsurfs will be looked up in sortedShaders[]
	// lower indexed sortedShaders must be rendered first (opaque surfaces before translucent)
	int						numShaders;
	shader_t				*shaders[MAX_SHADERS];
	shader_t				*sortedShaders[MAX_SHADERS];

	int						numSkins;
	skin_t					*skins[MAX_SKINS];

	float					sinTable[FUNCTABLE_SIZE];
	float					squareTable[FUNCTABLE_SIZE];
	float					triangleTable[FUNCTABLE_SIZE];
	float					sawToothTable[FUNCTABLE_SIZE];
	float					inverseSawToothTable[FUNCTABLE_SIZE];
	float					fogTable[FOG_TABLE_SIZE];

	qboolean				mapLoading;

	float					rangedFog;
	float					distanceCull;

	float					widthRatioCoef;

	int						dynamicGlowWidth;
	int						dynamicGlowHeight;

	int						needScreenMap;
	int						numDrawSurfCmds;
	drawSurfsCommand_t		*drawSurfCmd;
	int						lastRenderCommand;
	int						numFogs; // read before parsing shaders

	vec4_t					clearColor;
#ifdef VK_COMPUTE_NORMALMAP
	// Normal maps to compute from their diffuse texture at the next frame.
	struct {
		image_t			*normal;
		VkImageView		storage_view;		// level 0 of the normal map
		VkDescriptorSet	descriptor_set;
	}						compute_normalmaps[MAX_BATCH_COMPUTE_NORMALMAPS];
	uint32_t				compute_normalmaps_batch_num;
#endif
} trGlobals_t;

struct glconfigExt_t
{
	glconfig_t *glConfig;
	qboolean	doGammaCorrectionWithShaders;
	qboolean	doStencilShadowsInOneDrawcall;
	const char	*originalExtensionString;
};

extern backEndState_t	backEnd;
extern trGlobals_t		tr;
extern glconfig_t		glConfig;		// outside of TR since it shouldn't be cleared during ref re-init
extern glconfigExt_t	glConfigExt;
extern glstate_t		glState;		// outside of TR since it shouldn't be cleared during ref re-init
extern window_t			window;
extern glRefConfig_t	glRefConfig;

// Vulkan
extern glstatic_t	gls;
extern Vk_Instance	vk;					// shouldn't be cleared during ref re-init
extern Vk_World		vk_world;			// this data is cleared during ref re-init

//
// cvars
//
extern cvar_t	*r_ignore;				// used for debugging anything
extern cvar_t	*r_verbose;				// used for verbose debug spew

extern cvar_t	*r_znear;				// near Z clip plane
extern cvar_t	*r_zproj;				// z distance of projection plane

extern cvar_t	*r_stencilbits;			// number of desired stencil bits
extern cvar_t	*r_depthbits;			// number of desired depth bits
extern cvar_t	*r_colorbits;			// number of desired color bits, only relevant for fullscreen
extern cvar_t	*r_stereo;				// desired pixelformat stereo flag
extern cvar_t	*r_texturebits;			// number of desired texture bits
										// 0 = use framebuffer depth
										// 16 = use 16-bit textures
										// 32 = use 32-bit textures
										// all else = error
extern cvar_t	*r_texturebitslm;		// number of desired lightmap texture bits

extern cvar_t	*r_measureOverdraw;		// enables stencil buffer overdraw measurement

extern cvar_t	*r_lodbias;				// push/pull LOD transitions
extern cvar_t	*r_lodscale;
extern cvar_t	*r_autolodscalevalue;

extern cvar_t	*r_primitives;			// "0" = based on compiled vertex array existance
										// "1" = glDrawElemet tristrips
										// "2" = glDrawElements triangles
										// "-1" = no drawing

extern cvar_t	*r_inGameVideo;				// controls whether in game video should be draw
extern cvar_t	*r_fastsky;				// controls whether sky should be cleared or drawn
extern cvar_t	*r_drawSun;				// controls drawing of sun quad
extern cvar_t	*r_dynamiclight;		// dynamic lights enabled/disabled
// rjr - removed for hacking
extern cvar_t	*r_dlightBacks;			// dlight non-facing surfaces for continuity

extern	cvar_t	*r_norefresh;			// bypasses the ref rendering
extern	cvar_t	*r_drawentities;		// disable/enable entity rendering
extern	cvar_t	*r_drawworld;			// disable/enable world rendering
extern	cvar_t	*r_drawfog;				// disable/enable fog rendering
extern	cvar_t	*r_speeds;				// various levels of information display
extern  cvar_t	*r_detailTextures;		// enables/disables detail texturing stages
extern	cvar_t	*r_novis;				// disable/enable usage of PVS
extern	cvar_t	*r_visGenerate;			// generate a PVS for a map without a vis lump
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_facePlaneCull;		// enables culling of planar surfaces with back side test
extern	cvar_t	*r_cullRoofFaces;		//attempted smart method of culling out upwards facing surfaces on roofs for automap shots -rww
extern	cvar_t	*r_roofCullCeilDist;	//ceiling distance cull tolerance -rww
extern	cvar_t	*r_roofCullFloorDist;	//floor distance cull tolerance -rww
extern	cvar_t	*r_nocurves;
extern	cvar_t	*r_showcluster;

extern	cvar_t	*r_autoMap;				//automap renderside toggle for debugging -rww
extern	cvar_t	*r_autoMapBackAlpha;	//alpha of automap bg -rww
extern	cvar_t	*r_autoMapDisable;

extern cvar_t	*r_dlightStyle;
extern cvar_t	*r_surfaceSprites;
extern cvar_t	*r_surfaceWeather;

extern cvar_t	*r_windSpeed;
extern cvar_t	*r_windAngle;
extern cvar_t	*r_windGust;
extern cvar_t	*r_windDampFactor;
extern cvar_t	*r_windPointForce;
extern cvar_t	*r_windPointX;
extern cvar_t	*r_windPointY;

extern cvar_t	*r_mode;				// video mode
extern cvar_t	*r_fullscreen;
extern cvar_t	*r_noborder;			// disable border in windowed mode
extern cvar_t	*r_centerWindow;		// override vid_x/ypos and center the window
extern cvar_t	*r_gamma;
extern cvar_t	*r_displayRefresh;		// optional display refresh option
extern cvar_t	*r_ignorehwgamma;		// overrides hardware gamma capabilities

extern cvar_t	*r_allowExtensions;				// global enable/disable of OpenGL extensions
extern cvar_t	*r_ext_compressed_textures;		// these control use of specific extensions
extern cvar_t	*r_ext_compressed_lightmaps;	// turns on compression of lightmaps, off by default
extern cvar_t	*r_ext_preferred_tc_method;
extern cvar_t	*r_ext_gamma_control;
extern cvar_t	*r_ext_texenv_op;
extern cvar_t	*r_ext_multitexture;
extern cvar_t	*r_ext_compiled_vertex_array;
extern cvar_t	*r_ext_texture_env_add;
extern cvar_t	*r_ext_texture_filter_anisotropic;

extern cvar_t	*r_environmentMapping;

extern cvar_t	*r_DynamicGlow;
extern cvar_t	*r_DynamicGlowAllStages;
extern cvar_t	*r_DynamicGlowPasses;
extern cvar_t	*r_DynamicGlowDelta;
extern cvar_t	*r_DynamicGlowIntensity;
extern cvar_t	*r_DynamicGlowSoft;
extern cvar_t	*r_DynamicGlowWidth;
extern cvar_t	*r_DynamicGlowHeight;
extern cvar_t	*r_DynamicGlowScale;

extern cvar_t	*r_smartpicmip;
#ifdef USE_VK_PBR
extern cvar_t	*r_baseNormalX;
extern cvar_t	*r_baseNormalY;
extern cvar_t	*r_baseParallax;
extern cvar_t	*r_baseSpecular;
#endif
#ifdef VK_COMPUTE_NORMALMAP
extern cvar_t	*r_genNormalMaps;
#endif
#ifdef USE_VK_PBR
extern cvar_t	*r_normalMapping;
extern cvar_t	*r_specularMapping;
#endif
#ifdef VK_CUBEMAP
extern cvar_t	*r_cubeMapping;
extern cvar_t	*r_deluxeMapping;
extern cvar_t	*r_deluxeSpecular;
#endif

extern	cvar_t	*r_nobind;				// turns off binding to appropriate textures
extern	cvar_t	*r_singleShader;		// make most world faces use default shader
extern	cvar_t	*r_colorMipLevels;		// development aid to see texture mip usage
extern	cvar_t	*r_picmip;				// controls picmip values
extern	cvar_t	*r_finish;
extern	cvar_t	*r_swapInterval;
extern	cvar_t	*r_renderThread;
extern	cvar_t	*r_renderThreadSyncLog;
extern	cvar_t	*r_timescale;			// the timescale cvar of the engine
extern	cvar_t	*r_markcount;
extern	cvar_t	*r_textureMode;
extern	cvar_t	*r_offsetFactor;
extern	cvar_t	*r_offsetUnits;

extern	cvar_t	*r_fullbright;			// avoid lightmap pass
extern	cvar_t	*r_lightmap;			// render lightmaps only
extern	cvar_t	*r_distanceCull;		// render lightmaps only
extern	cvar_t	*r_vertexLight;			// vertex lighting mode for better performance
extern	cvar_t	*r_uiFullScreen;		// ui is running fullscreen

extern	cvar_t	*r_logFile;				// number of frames to emit GL logs
extern	cvar_t	*r_showtris;			// enables wireframe rendering of the world
extern	cvar_t	*r_showsky;				// forces sky in front of all surfaces
extern	cvar_t	*r_shownormals;			// draws wireframe normals
extern	cvar_t	*r_clear;				// force screen clear every frame


extern	cvar_t	*r_shadows;				// 0 = none, 1 = blur, 2 = stencil, 3 = black planar projection, 4 = stencil on the GPU
// Ghoul2 shadow volume debug aids, Debug builds only. In release these fold to
// constants and the call sites compile away.
#ifdef _DEBUG
extern	cvar_t	*r_g2_shadowdebug;		// 1 = traces, 2 = parity map (front pass tints red, back blue, so balanced reads purple)
extern	cvar_t	*r_g2_shadowedges;		// emission filter: bit 0 = the sides, bit 1 = the caps
extern	cvar_t	*r_g2_shadowsurf;		// -1 = every shadow surface, >= 0 = only that surface index

#define R_SHADOW_DEBUG			( r_g2_shadowdebug->integer )
#define R_SHADOW_DEBUG_EDGES	( r_g2_shadowedges->integer )
#define R_SHADOW_DEBUG_SURF		( r_g2_shadowsurf->integer )
#else
#define R_SHADOW_DEBUG			0
#define R_SHADOW_DEBUG_EDGES	3
#define R_SHADOW_DEBUG_SURF		-1
#endif

// cg_shadows 2 is the one stencil mode; r_vbo_models picks how the volume is
// built, on the GPU when the ghoul2 VBO is up, on the CPU otherwise.
#define R_STENCIL_SHADOWS()	( r_shadows->integer == 2 )

extern	cvar_t	*r_flares;				// light flares
extern	cvar_t	*r_shaderWarnings;		// 1: scripts that fail to parse, 2: shaders with no script and no image
//extern	cvar_t	*r_flareSize;			// light flare size
//extern cvar_t	*r_flareFade;
//extern cvar_t	*r_flareCoeff;			// coefficient for the flare intensity falloff function. 

extern	cvar_t	*r_intensity;

extern	cvar_t	*r_lockpvs;
extern	cvar_t	*r_noportals;
extern	cvar_t	*r_portalOnly;

extern	cvar_t	*r_subdivisions;
extern	cvar_t	*r_lodCurveError;
extern	cvar_t	*r_skipBackEnd;

extern	cvar_t	*r_ignoreGLErrors;

extern	cvar_t	*r_overBrightBits;
extern	cvar_t	*r_mapOverBrightBits;

extern	cvar_t	*r_debugSurface;
extern	cvar_t	*r_simpleMipMaps;

extern	cvar_t	*r_showImages;
extern	cvar_t	*r_debugSort;

extern	cvar_t	*r_marksOnTriangleMeshes;

extern	cvar_t	*r_aspectCorrectFonts;
extern	cvar_t	*cl_ratioFix;
extern cvar_t	*r_patchStitching;

// Vulkan
extern cvar_t	*r_defaultImage;
extern cvar_t	*r_device;
extern cvar_t	*r_ext_multisample;
extern cvar_t	*r_ext_supersample;
extern cvar_t	*r_ext_alpha_to_coverage;
extern cvar_t	*r_fbo;
extern cvar_t	*r_hdr;
extern cvar_t	*r_mapGreyScale;
extern cvar_t	*r_ext_max_anisotropy;
extern cvar_t	*r_greyscale;
extern cvar_t	*r_dither;
extern cvar_t	*r_presentBits;
extern cvar_t	*r_bloom;
extern cvar_t	*r_bloom_threshold;
extern cvar_t	*r_bloom_intensity;
extern cvar_t	*r_bloom_threshold_mode;
extern cvar_t	*r_bloom_modulate;
extern cvar_t	*r_depthPrepass;
extern cvar_t	*r_velocityBuffer;
extern cvar_t	*r_showGBuffer;
extern cvar_t	*r_distortionStyle;
extern cvar_t	*r_ssao;
extern cvar_t	*r_ssaoRadius;
extern cvar_t	*r_ssaoIntensity;
extern cvar_t	*r_ssaoSlices;
extern cvar_t	*r_ssaoSteps;
extern cvar_t	*r_contactShadows;
extern cvar_t	*r_contactShadowLength;
extern cvar_t	*r_contactShadowThickness;
extern cvar_t	*r_contactShadowSteps;
extern cvar_t	*r_contactShadowIntensity;
extern cvar_t	*r_renderWidth;
extern cvar_t	*r_renderHeight;
extern cvar_t	*r_renderScale;
#ifdef USE_PMLIGHT
extern cvar_t	*r_dlightMode;			// 0 - vq3, 1 - pmlight
extern cvar_t	*r_dlightScale;			// 0.1 - 1.0
extern cvar_t	*r_dlightIntensity;		// 0.1 - 1.0
#endif
extern cvar_t	*r_dlightSaturation;	// 0.0 - 1.0
extern cvar_t	*r_roundImagesDown;
extern cvar_t	*r_nomip;				// apply picmip only on worldspawn textures
#ifdef USE_VBO
extern cvar_t	*r_vbo;
extern cvar_t	*r_vbo_models;
#endif

/*
Ghoul2 Insert Start
*/
#ifdef _DEBUG
extern	cvar_t	*r_noPrecacheGLA;
#endif

extern	cvar_t	*r_noServerGhoul2;
extern	cvar_t	*r_noGhoul2;
/*
Ghoul2 Insert End
*/
//====================================================================

void		R_RenderView( const viewParms_t *parms );
void		R_AddMD3Surfaces( trRefEntity_t *e );
void		R_AddPolygonSurfaces( void );
void		R_DecomposeSort( sortKey_t sort, int *entityNum, shader_t **shader, int *fogNum, int *dlightMap );
// cubemapIndex -1: the cubemap of the current entity
void		R_AddDrawSurf( surfaceType_t *surface, shader_t *shader, int fogIndex, int dlightMap, int cubemapIndex = -1 );
#ifdef USE_PMLIGHT
void		R_DecomposeLitSort( sortKey_t sort, int* entityNum, shader_t** shader, int* fogNum );
void		R_AddLitSurf( surfaceType_t* surface, shader_t* shader, int fogIndex );
#endif
qboolean	R_FogParmsMatch( int fog1, int fog2 );
int			R_FogNumForSphere( const vec3_t origin, float radius );

shader_t		*GeneratePermanentShader( void );

#define	CULL_IN		0		// completely unclipped
#define	CULL_CLIP	1		// clipped by one or more planes
#define	CULL_OUT	2		// completely outside the clipping planes
void		R_LocalPointToWorld ( const vec3_t local, vec3_t world );
void		R_WorldNormalToEntity ( const vec3_t localVec, vec3_t world );
int			R_CullLocalBox ( const vec3_t bounds[2]);
int			R_CullPointAndRadius( const vec3_t origin, float radius );
int			R_CullLocalPointAndRadius( const vec3_t origin, float radius );
int			R_CullDlight( const dlight_t* dl );

void		R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms, orientationr_t *ori );

void		RE_StretchRaw ( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty );
void		RE_UploadCinematic( int cols, int rows, const byte *data, int client, qboolean dirty );

void		RE_BeginRegistration( glconfig_t *glconfig );
void		R_ColorShiftLightingBytes( const byte in[4], byte out[4], qboolean hasAlpha ); //rwwRMG - added
void		RE_LoadWorldMap( const char *mapname );

void		RE_SetWorldVisData( const byte *vis );

qhandle_t	RE_RegisterServerModel( const char *name );
qhandle_t	RE_RegisterModel( const char *name );
qhandle_t	RE_RegisterSkin( const char *name );

qboolean	R_GetEntityToken( char *buffer, int size );

model_t		*R_AllocModel( void );

void    	R_Init( void );

image_t		*R_FindImageFile( const char *name, imgFlags_t flags );
image_t		*R_CreateImage( const char *name, byte *pic, int width, int height, imgFlags_t flags );
// With the type of a map (PHYS_*), which gives the swizzle of its view (textureMapTypes).
image_t		*R_FindImageFileType( const char *name, imgFlags_t flags, uint32_t type );
image_t		*R_CreateImageType( const char *name, byte *pic, int width, int height, imgFlags_t flags, uint32_t type );
image_t		*R_GetLoadedImage( const char *name, imgFlags_t flags );
#ifdef USE_VK_PBR
qboolean	vk_create_normal_texture( shaderStage_t *stage, const char *name, imgFlags_t flags );
qboolean	vk_create_phyisical_texture( shaderStage_t *stage, const char *name, imgFlags_t flags );
#endif
#ifdef USE_VK_PBR
// MikkTSpace tangents, vk_mikktspace.cpp
void		vk_mikkt_bsp_tri_generate( srfTriangles_t *tri );
void		vk_mikkt_bsp_face_generate( srfSurfaceFace_t *cv );
void		vk_mikkt_mdxm_generate( const mdxmSurface_t *surf, vec4_t *tangents );
void		vk_mikkt_mdv_generate( const mdvSurface_t *surf, vec4_t *tangents );
void		R_LightDirForPoint( const vec3_t point, const vec3_t normal, const world_t *world, vec4_t lightDir );
// PBR shading of the raster renderer, vk_pbr.cpp
void		vk_create_pbr_resources( void );
void		vk_destroy_pbr_resources( void );
void		vk_init_pbr_descriptors( void );
void		vk_create_brdf_lut( void );
uint32_t	vk_stage_light_flags( const shaderStage_t *pStage, Vk_Shader_Type type );
#endif
#ifdef VK_CUBEMAP
// environment cubemaps, tr_bsp.cpp, tr_light.cpp, tr_scene.cpp, vk_cubemap.cpp
void		R_LoadCubemaps( world_t *worldData, int index );
int			R_CubemapForPoint( const vec3_t point );
void		R_RenderCubemaps( void );
void		R_AddConvolveCubemapCmd( int cubemapIndex );
void		vk_create_cubemap_resources( void );
void		vk_destroy_cubemap_resources( void );
void		vk_release_cubemaps( void );
void		vk_prefilter_cubemap( int cubemapIndex );
void		vk_init_cubemap_descriptors( void );
VkDescriptorSet vk_cubemap_descriptor( int cubemapIndex );
#endif
#ifdef VK_COMPUTE_NORMALMAP
// normal maps computed from the diffuse texture, vk_normalmap.cpp
void		vk_create_compute_normalmap_pipelines( void );
void		vk_destroy_compute_normalmap_pipelines( void );
void		vk_dispatch_compute_normalmaps( void );
void		vk_clear_compute_normalmaps( void );
void		vk_add_compute_normalmap( shaderStage_t *stage, image_t *albedo, imgFlags_t flags );
#endif

textureMode_t *GetTextureMode( const char *name );
qboolean	R_GetModeInfo( int *width, int *height, int mode );

void		R_SetColorMappings( void );
void		R_GammaCorrect( byte *buffer, int bufSize );
void		R_Set2DRatio( void );

void		R_ImageList_f( void );
void		RE_RegisterImages_Info_f( void );
void		RE_RegisterModels_Info_f( void );
void		R_SkinList_f( void );
void		R_FontList_f( void );

void		R_InitFogTable( void );
float		R_FogFactor( float s, float t );
void		R_InitImages( void );
float		R_SumOfUsedImages( qboolean bUseFormat );
void		R_InitSkins( void );
skin_t		*R_GetSkinByHandle( qhandle_t hSkin );
const void	*RB_TakeVideoFrameCmd( const void *data );
float		R_ClampDenorm( float v );
void		RE_HunkClearCrap( void );

//
// tr_shader.c
//
extern	const int	lightmapsNone[MAXLIGHTMAPS];
extern	const int	lightmaps2d[MAXLIGHTMAPS];
extern	const int	lightmapsVertex[MAXLIGHTMAPS];
extern	const int	lightmapsFullBright[MAXLIGHTMAPS];
extern	const byte	stylesDefault[MAXLIGHTMAPS];

qhandle_t	RE_RegisterShaderLightMap( const char *name, const int *lightmapIndex, const byte *styles ) ;
qhandle_t	RE_RegisterShader( const char *name );
qhandle_t	RE_RegisterShaderNoMip( const char *name );
const char	*RE_ShaderNameFromIndex(int index);

shader_t	*R_FindShader( const char *name, const int *lightmapIndex, const byte *styles, qboolean mipRawImage );
shader_t	*R_GetShaderByHandle( qhandle_t hShader );
shader_t	*R_FindShaderByName( const char *name );
shader_t	*FinishShader( void );

void		R_InitShaders( qboolean server );
void		R_ShaderList_f( void );
void		R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset );
void		R_CreateDefaultShadingCmds( image_t *image );

//
// tr_surface.c
//
#ifdef USE_VBO_GRID
void		RB_SurfaceGridEstimate(srfGridMesh_t *cv, int *numVertexes, int *numIndexes);
#endif

/*
====================================================================

TESSELATOR/SHADER DECLARATIONS

====================================================================
*/
typedef byte color4ub_t[4];

typedef struct stageVars
{
	color4ub_t	colors[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
	vec2_t		texcoords[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
    vec2_t      *texcoordPtr[NUM_TEXTURE_BUNDLES];
} stageVars_t;

#define	NUM_TEX_COORDS				( MAXLIGHTMAPS + 1 )
#define MAX_MULTIDRAW_PRIMITIVES	16384

struct shaderCommands_s
{
	glIndex_t		indexes[SHADER_MAX_INDEXES]						QALIGN(16);
	vec4_t			xyz[SHADER_MAX_VERTEXES*2]						QALIGN(16);
	vec4_t			normal[SHADER_MAX_VERTEXES]						QALIGN(16);
#ifdef USE_RTX
	vec4_t			qtangent[SHADER_MAX_VERTEXES]					QALIGN(16);
#ifdef USE_VK_PBR
	vec4_t			lightdir[SHADER_MAX_VERTEXES]					QALIGN(16);
#endif
#endif
	vec2_t			texCoords[NUM_TEX_COORDS][SHADER_MAX_VERTEXES]	QALIGN(16);
	vec2_t			texCoords00[SHADER_MAX_VERTEXES]				QALIGN(16);
	color4ub_t		vertexColors[SHADER_MAX_VERTEXES]				QALIGN(16);
	byte			vertexAlphas[SHADER_MAX_VERTEXES][4];			QALIGN(16) //rwwRMG - added support
	stageVars_t		svars QALIGN(16);

#ifdef USE_VBO
	surfaceType_t	surfType;
	int				vbo_world_index; // world item index
	VBO_t			*vbo_model; // ghoul2/mdv item index
	IBO_t			*ibo_model; // ghoul2/mdv item index
	int				vboStage;
	qboolean		allowVBO;
	
#endif

	shader_t		*shader;
	float			shaderTime;
	int				fogNum;
#ifdef VK_CUBEMAP
	int				cubemapIndex;
#endif
	bool			entityMergable;
	int				numIndexes;
	int				numVertexes;

	glIndex_t	minIndex;
	glIndex_t	maxIndex;

	int			multiDrawPrimitives;
	GLsizei		multiDrawNumIndexes[MAX_MULTIDRAW_PRIMITIVES];
	glIndex_t	*multiDrawFirstIndex[MAX_MULTIDRAW_PRIMITIVES];
	glIndex_t	*multiDrawLastIndex[MAX_MULTIDRAW_PRIMITIVES];
	glIndex_t	multiDrawMinIndex[MAX_MULTIDRAW_PRIMITIVES];
	glIndex_t	multiDrawMaxIndex[MAX_MULTIDRAW_PRIMITIVES];

#ifdef USE_PMLIGHT
	const dlight_t	*light;
	qboolean		dlightPass;
	qboolean		dlightUpdateParams;
#endif

	Vk_Depth_Range	depthRange;

	// info extracted from current shader
	int				numPasses;

	shaderStage_t	**xstages;

	int				registration;

	qboolean		SSInitializedWind;

	//rww - doing a fade, don't compute shader color/alpha overrides
	bool			fading;
};

#ifdef _MSC_VER
	typedef __declspec(align(16)) shaderCommands_s	shaderCommands_t;
#else
	typedef struct shaderCommands_s  shaderCommands_t;
#endif

extern	shaderCommands_t	tess;
extern	color4ub_t			styleColors[MAX_LIGHT_STYLES];

void RB_BeginSurface( shader_t *shader, int fogNum );
void RB_EndSurface( void );
void RB_CheckOverflow( int verts, int indexes );
#define RB_CHECKOVERFLOW( v , i ) if ( tess.numVertexes + ( v ) >= SHADER_MAX_VERTEXES || tess.numIndexes + ( i ) >= SHADER_MAX_INDEXES ) {RB_CheckOverflow( v , i );}

void RB_StageIteratorGeneric( void );
void RB_StageIteratorSky( void );

void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, color4ub_t color );
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, color4ub_t color, float s1, float t1, float s2, float t2 );
void RB_AddQuadStamp2( float x, float y, float w, float h, float s1, float t1, float s2, float t2, color4ub_t color );

/*
============================================================

WORLD MAP

============================================================
*/
void R_AddBrushModelSurfaces( trRefEntity_t *e );
void R_AddWorldSurfaces( void );
qboolean R_inPVS( const vec3_t p1, const vec3_t p2, byte *mask );


/*
============================================================

FLARES

============================================================
*/
void R_ClearFlares( void );

void RB_AddFlare( void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal );
void RB_AddDlightFlares( void );
void RB_RenderFlares ( void );

/*
============================================================

LIGHTS

============================================================
*/

void R_DlightBmodel( bmodel_t *bmodel, bool NoLight );
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent );
void R_TransformDlights( int count, dlight_t *dl, orientationr_t *ori );
int	R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
qboolean RE_GetLighting( const vec3_t origin, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
void RE_LAGoggles( void );
byte *RE_TempRawImage_ReadFromFile( const char *psLocalFilename, int *piWidth, int *piHeight, byte *pbReSampleBuffer, qboolean qbVertFlip );
void RE_TempRawImage_CleanUp( void );
extern float tr_distortionAlpha;
extern float tr_distortionStretch;
extern qboolean tr_distortionPrePost;
extern qboolean tr_distortionNegate;

/*
============================================================

SHADOWS

============================================================
*/
void RB_ShadowTessEnd( void );
void RB_ShadowFinish( void );
void RB_ProjectionShadowDeform( void );

/*
============================================================

SKIES

============================================================
*/
void R_InitSkyTexCoords( float heightCloud );
void RB_DrawSun( float scale, shader_t* shader );
/*
============================================================

CURVE TESSELATION

============================================================
*/

srfGridMesh_t	*R_SubdividePatchToGrid( int width, int height, srfVert_t points[MAX_PATCH_SIZE * MAX_PATCH_SIZE] );
srfGridMesh_t	*R_GridInsertColumn( srfGridMesh_t *grid, int column, int row, vec3_t point, float loderror );
srfGridMesh_t	*R_GridInsertRow( srfGridMesh_t *grid, int row, int column, vec3_t point, float loderror );
void			R_FreeSurfaceGridMesh( srfGridMesh_t *grid );

/*
Ghoul2 Insert Start
*/
float ProjectRadius( float r, vec3_t location );
/*
Ghoul2 Insert End
*/

/*
============================================================

MARKERS, POLYGON PROJECTION ON WORLD POLYGONS

============================================================
*/
int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

/*
============================================================

SCENE GENERATION

============================================================
*/
void R_InitNextFrame( void );

void RE_ClearScene( void );
void RE_AddRefEntityToScene( const refEntity_t *ent );
void RE_AddMiniRefEntityToScene( const miniRefEntity_t *ent );
void RE_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num );
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_RenderScene( const refdef_t *fd );

/*
=============================================================

ANIMATED MODELS

=============================================================
*/

/*
Ghoul2 Insert Start
*/
// Cloth and hair physics (.phys), see tr_ghoul2_phys.cpp.
struct physInstance_t;

struct physSurfaceOut_t
{
	int		numVerts;
	float	*xyz;		// 3 floats per vertex, model space
	float	*normal;
	// Subdivided mesh (subdivide): numVerts counts its vertices. st NULL: the vertices are those of the surface.
	const float	*st;		// 2 floats per vertex
	const int	*chunks;	// 4 per chunk: first vertex, vertex count, first index, index count
	const int	*indexes;	// of each chunk, from the first vertex of the chunk
	int			numChunks;
	int			numIndexes;
	mutable int	snapScene;	// copy of this frame in backEndData->g2.physVerts, see R_G2_SnapPhysVerts
	mutable int	snapOffset;

	physSurfaceOut_t() : numVerts( 0 ), xyz( NULL ), normal( NULL ), st( NULL ), chunks( NULL ), indexes( NULL ),
		numChunks( 0 ), numIndexes( 0 ), snapScene( -1 ), snapOffset( -1 ) {}
};

void					G2Phys_Init( void );
void					G2Phys_Shutdown( void );
void					G2Phys_Reload_f( void );
void					G2Phys_FreeInstance( void *inst );
physInstance_t			*G2Phys_Update( const trRefEntity_t *ent, CBoneCache *bc, const model_t *glm, const surfaceInfo_v &slist );
const physSurfaceOut_t	*G2Phys_GetSurface( const physInstance_t *inst, int surfaceNum );
int						G2Phys_UICommand( const char *cmd, const char *arg, char *out, int outSize );
void					G2Phys_SurfaceOutline( const trRefEntity_t *ent, const physInstance_t *inst, CBoneCache *bc, const model_t *glm,
											   int surfaceNum, const char *surfaceName );
void					G2Phys_AddBoneMarkers( const trRefEntity_t *ent, CBoneCache *bc, const model_t *glm );

class CRenderableSurface
{
public:
#ifdef _G2_GORE
	int				ident;
#else
	const int		ident;				// ident of this surface - required so the materials renderer knows what sort of surface this refers to
#endif
	int				g2Snap;				// index into backEndData->g2.models, -1 for none. The backend never reads the bone cache.
	int				physVerts;			// first float of the solved cloth vertices in backEndData->g2.physVerts, -1 for a skinned surface
	int				physChunk;			// subdivided cloth: the chunk this draw surface draws, -1 = the vertices of the surface
#ifdef USE_VBO_GHOUL2
	mdxmVBOMesh_t	*vboMesh;
#endif
	mdxmSurface_t	*surfaceData;		// pointer to surface data loaded into file - only used by client renderer DO NOT USE IN GAME SIDE - if there is a vid restart this will be out of wack on the game
#ifdef _G2_GORE
	float			*alternateTex;		// alternate texture coordinates.
	srfG2GoreSurface_t *goreVBO;		// alternate texture coordinates.
	void			*goreChain;

	float			scale;
	float			fade;
	float			impactTime;			// this is a number between 0 and 1 that dictates the progression of the bullet impact
#endif

#ifdef _G2_GORE
	CRenderableSurface& operator= ( const CRenderableSurface& src )
	{
		ident			= src.ident;
		g2Snap			= src.g2Snap;
		physVerts		= src.physVerts;
		physChunk		= src.physChunk;
		surfaceData		= src.surfaceData;
#ifdef _G2_GORE
		alternateTex	= src.alternateTex;
		goreVBO			= src.goreVBO;
		goreChain		= src.goreChain;
#endif
#ifdef USE_VBO_GHOUL2
		vboMesh			= src.vboMesh;
#endif
		return *this;
	}
#endif

CRenderableSurface():
	ident( SF_MDX ),
	g2Snap( -1 ),
	physVerts( -1 ),
	physChunk( -1 ),
#ifdef USE_VBO_GHOUL2
	vboMesh( nullptr ),
#endif
#ifdef _G2_GORE
	surfaceData( nullptr ),
	alternateTex( nullptr ),
	goreVBO( nullptr ),
	goreChain( nullptr )
#else
	surfaceData( 0 )
#endif
	{}

#ifdef _G2_GORE
	void Init()
	{
		ident			= SF_MDX;
		g2Snap			= -1;
		physVerts		= -1;
		physChunk		= -1;
		surfaceData		= nullptr;
#ifdef _G2_GORE
		alternateTex	= nullptr;
		goreVBO			= nullptr;
		goreChain		= nullptr;
#endif
#ifdef USE_VBO_GHOUL2
		vboMesh			= nullptr;
#endif
	}
#endif
};

void	R_AddGhoulSurfaces( trRefEntity_t *ent );
void	RB_SurfaceGhoul( CRenderableSurface *surface );
/*
Ghoul2 Insert End
*/
/*
=============================================================
=============================================================
*/
void	R_TransformModelToClip( const vec3_t src, const float *modelViewMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst );
void	R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window );
#ifdef USE_VBO_GHOUL2
qboolean ShaderRequiresCPUDeforms( const shader_t *shader );
#endif
void	RB_DeformTessGeometry( void );
void	RB_CalcEnvironmentTexCoords( float *dstTexCoords );
void	RB_CalcFogTexCoords( float *dstTexCoords );
void	RB_CalcScrollTexCoords( const float scroll[2], float *srcTexCoords, float *dstTexCoords );
void	RB_CalcRotateTexCoords( float rotSpeed, float *srcTexCoords, float *dstTexCoords );
void	RB_CalcScaleTexCoords( const float scale[2], float *srcTexCoords, float *dstTexCoords );
void	RB_CalcTurbulentTexCoords( const waveForm_t *wf, float *srcTexCoords, float *dstTexCoords );
void	RB_CalcTransformTexCoords( const texModInfo_t *tmi, float *srcTexCoords, float *dstTexCoords );
void	RB_CalcStretchTexCoords( const waveForm_t* wf, float* srcTexCoords, float* dstTexCoords );
void	RB_CalcModulateColorsByFog( unsigned char *dstColors );
void	RB_CalcModulateAlphasByFog( unsigned char *dstColors );
void	RB_CalcModulateRGBAsByFog( unsigned char *dstColors );
void	RB_CalcWaveAlpha( const waveForm_t *wf, unsigned char *dstColors );
void	RB_CalcWaveColor( const waveForm_t *wf, unsigned char *dstColors );
float	RB_CalcWaveColorSingle( const waveForm_t *wf );
float	RB_CalcWaveAlphaSingle( const waveForm_t *wf );
void	RB_CalcAlphaFromEntity( unsigned char *dstColors );
void	RB_CalcAlphaFromOneMinusEntity( unsigned char *dstColors );
void	RB_CalcColorFromEntity( unsigned char *dstColors );
void	RB_CalcColorFromOneMinusEntity( unsigned char *dstColors );
void	RB_CalcSpecularAlpha( unsigned char *alphas );
void	RB_CalcDisintegrateColors( unsigned char *colors, colorGen_t rgbGen );
void	RB_CalcDiffuseColor( unsigned char *colors );
void	RB_CalcDiffuseEntityColor( unsigned char *colors );
void	RB_CalcDisintegrateVertDeform( void );

void	RB_CalcScaleTexMatrix( const float scale[2], float *matrix );
void	RB_CalcScrollTexMatrix( const float scrollSpeed[2], float *matrix );
void	vk_compute_tex_mods( const textureBundle_t *bundle, float *outMatrix, float *outOffTurb );
void	RB_CalcRotateTexMatrix( float degsPerSecond, float *matrix );
void	RB_CalcTurbulentFactors( const waveForm_t *wf, float *amplitude, float *now );
void	RB_CalcTransformTexMatrix( const texModInfo_t *tmi, float *matrix  );
void	RB_CalcStretchTexMatrix( const waveForm_t *wf, float *matrix );
/*
=============================================================

RENDERER BACK END FUNCTIONS

=============================================================
*/
void RB_ExecuteRenderCommands( const void *data );
qboolean R_DistortionScreenCrop( const trRefEntity_t *ent, vec4_t crop );
qboolean ComputeDistortionPass( int stage, vec4_t params, uint32_t *stateBits );

/*
=============================================================

RENDERER BACK END COMMAND QUEUE

=============================================================
*/
#define	MAX_RENDER_COMMANDS	0x80000

typedef struct renderCommandList_s {
	byte		cmds[MAX_RENDER_COMMANDS];
	int			used;
} renderCommandList_t;

typedef struct setColorCommand_s {
	int			commandId;
	float		color[4];
} setColorCommand_t;

typedef struct drawBufferCommand_s {
	int			commandId;
	int			buffer;
} drawBufferCommand_t;

typedef struct subImageCommand_s {
	int			commandId;
	image_t		*image;
	int			width;
	int			height;
	void		*data;
} subImageCommand_t;

typedef struct swapBuffersCommand_s {
	int			commandId;
} swapBuffersCommand_t;

typedef struct endFrameCommand_s {
	int			commandId;
	int			buffer;
} endFrameCommand_t;

typedef struct stretchPicCommand_s {
	int			commandId;
	shader_t	*shader;
	float		x, y;
	float		w, h;
	float		s1, t1;
	float		s2, t2;
} stretchPicCommand_t;

typedef struct rotatePicCommand_s {
	int			commandId;
	shader_t	*shader;
	float		x, y;
	float		w, h;
	float		s1, t1;
	float		s2, t2;
	float		a, ratio;	// ratio: aspect correction of the rotation, as in rd-vanilla
} rotatePicCommand_t;

typedef struct drawSurfsCommand_s {
	int			commandId;
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	drawSurf_t	*drawSurfs;
	int			numDrawSurfs;
} drawSurfsCommand_t;

typedef struct
{
	int			commandId;
} clearColorCommand_t;

#ifdef VK_CUBEMAP
typedef struct {
	int			commandId;
	int			cubemapIndex;
} convolveCubemapCommand_t;
#endif


typedef enum {
	RC_END_OF_LIST = 0,
	RC_SET_COLOR,
	RC_STRETCH_PIC,
	RC_ROTATE_PIC,
	RC_ROTATE_PIC2,
	RC_DRAW_SURFS,
	RC_DRAW_BUFFER,
	RC_SWAP_BUFFERS,
	RC_WORLD_EFFECTS,
	RC_AUTO_MAP,
	RC_VIDEOFRAME,
	RC_CLEARCOLOR,
	RC_CONVOLVECUBEMAP
} renderCommand_t;

/*
Ghoul2 snapshot

The frontend copies the final bone matrices of every Ghoul2 model it draws into a per-frame
pool. The backend reads only this pool, never the bone cache of the game. Pool sizes:
G2_SNAP_MAX_BONES matrices of 48 bytes in each of three arrays (current, previous, tracer),
about 9 MB, enough for 900 characters of 72 bones. When a pool is full the frontend counts
the overflow, prints a warning and the model draws without skinning data.
The solved cloth vertices go in a float pool of G2_SNAP_MAX_PHYS_FLOATS (6 floats per vertex:
position then normal), about 6 MB. A subdivided surface puts the number of chunks and of vertices first (2 ints), then the
chunks (4 ints each), 8 floats per vertex (position, normal, texture coordinates), then its
indexes as ints. Each chunk has its own draw surface, so it fits in one batch.
When the pool is full the surface draws skinned, without cloth.
*/
#define G2_SNAP_MAX_MODELS		8192
#define G2_SNAP_MAX_BONES		65536
#define G2_SNAP_MAX_PHYS_FLOATS	( 6 * 262144 )
#define G2_SNAP_MAX_RTX_MODELS	8192
#define G2_SNAP_MAX_RTX_MESHES	32768

typedef struct g2SnapModel_s {
	int			firstBone;			// first matrix of this model in the bone pools
	int			numBones;
	int			uboOffset;			// bone UBO offset, written by the backend, -1 until uploaded
	mat4_t		prevModelMatrix;	// model to world matrix of the previous frame, for the velocity buffer
} g2SnapModel_t;

#ifdef USE_RTX
typedef struct g2SnapRtxMesh_s {
	maliasmesh_t		*mesh;		// NULL when the VBO of the model is off
	shader_t			*shader;
	const mdxmSurface_t	*surface;	// for the bounds of the skinned vertices
} g2SnapRtxMesh_t;

typedef struct g2SnapRtxModel_s {
	int			snapModel;			// index in g2Snapshot_t::models, -1 when the pool was full
	int			firstMesh;
	int			numMeshes;
} g2SnapRtxModel_t;
#endif

typedef struct g2Snapshot_s {
	int				numModels;
	int				numBones;
	int				overflowModels;		// models dropped this frame
	int				overflowBones;
	mat3x4_t		*bones;				// current pose
	mat3x4_t		*prevBones;			// previous pose, filled when the velocity buffer is on
	mat3x4_t		*rtxBones;			// pose for the path tracer
	int				numPhysFloats;
	int				overflowPhys;		// cloth surfaces drawn skinned this frame
	float			*physVerts;			// solved cloth vertices, see CRenderableSurface::physVerts
	g2SnapModel_t	models[G2_SNAP_MAX_MODELS];
#ifdef USE_RTX
	int				numRtxModels;
	int				numRtxMeshes;
	int				overflowRtx;
	g2SnapRtxModel_t rtxModels[G2_SNAP_MAX_RTX_MODELS];
	g2SnapRtxMesh_t	rtxMeshes[G2_SNAP_MAX_RTX_MESHES];
#endif
} g2Snapshot_t;

#ifdef _G2_GORE
// Gore uploads wait for the next frame command buffer (vk_flush_gore_uploads).
// The regions are byte ranges of the staging buffer, copied at the same offset.
#define MAX_GORE_UPLOADS	256

typedef struct goreUploads_s {
	VkBufferCopy	regions[MAX_GORE_UPLOADS];
	uint32_t		count;
} goreUploads_t;
#endif

// Fog 0, the fogs of the BSP lump, the misc_bsp global fog copy and the LA goggles slot.
#define BE_MAX_FOGS		(MAX_MAP_FOGS + 3)

// all of the information needed by the back end must be
// contained in a backEndData_t.
typedef struct backEndData_s {
	g2Snapshot_t		g2;
#ifdef USE_VBO_SS
	struct {
		vk_ss_group_t	groups[SS_MAX_GROUP];	// surface sprite draws of the scene, filled by the frontend
		uint32_t		groups_count;
	} ss;
#endif
	color4ub_t			styleColors[MAX_LIGHT_STYLES];	// light styles at the time of the swap
	int					frameCount;						// tr.frameCount at the time of the swap
	int					numFogs;						// tr.world->numfogs and globalFog at the time of the swap
	int					globalFog;
	fog_t				fogs[BE_MAX_FOGS];				// copy of tr.world->fogs[0, numFogs], the slot past numFogs is the LA goggles fog
	float				distortionAlpha;				// cloak distortion settings at the time of the swap
	float				distortionStretch;
	qboolean			distortionPrePost;
	qboolean			distortionNegate;
	int					skyboxPortal;					// skyboxportal and drawskyboxportal at the time of the swap
	int					drawSkyboxPortal;
	char				weaponFxShaders[256];			// pt_weapon_fx_shaders and pt_debug_image at the time of the swap
	char				debugImage[MAX_QPATH];
#ifdef _G2_GORE
	goreUploads_t		goreVBOUploads;		// filled by R_UpdateGoreVBO in the frontend, consumed by vk_flush_gore_uploads
	goreUploads_t		goreIBOUploads;
#endif
	drawSurf_t			drawSurfs[MAX_DRAWSURFS];
#ifdef USE_PMLIGHT
	litSurf_t			litSurfs[MAX_LITSURFS];
	dlight_t			dlights[MAX_REAL_DLIGHTS];
#else
	dlight_t			dlights[MAX_DLIGHTS];
#endif
	trRefEntity_t		entities[MAX_REFENTITIES];
	srfPoly_t			*polys;//[MAX_POLYS];
	polyVert_t			*polyVerts;//[MAX_POLYVERTS];
	renderCommandList_t	commands;
} backEndData_t;

extern int max_polys;
extern int max_polyverts;

// Two buffers when r_renderThread is on. The frontend writes backEndData, the backend reads
// backEndDataBE. With one buffer both point to the same data.
extern backEndData_t *backEndData;
extern backEndData_t *backEndDataBE;
extern backEndData_t *backEndDataBuf[2];

// Render thread (tr_cmds.cpp). All calls are main thread only, except R_SyncRenderThread
// which does nothing on the render thread.
void	R_RenderThreadInit( void );
void	R_RenderThreadShutdown( void );
void	R_SyncRenderThreadAt( const char *file, int line, const char *func );
// Each call site shows up in the r_renderThreadSyncLog output.
#define R_SyncRenderThread()	R_SyncRenderThreadAt( __FILE__, __LINE__, __FUNCTION__ )
void	R_WaitRenderThreadNoRaise( void );
void	R_DeferredFree( void *ptr );
void	R_RenderThreadForceSync( int frames );
void	R_RenderThreadSetSync( int frames );
qboolean R_RenderThreadWanted( void );
qboolean R_OnRenderThread( void );
#if defined(_MSC_VER)
__declspec(noreturn) void R_RenderThreadAbort( int level, const char *text );
#else
void	R_RenderThreadAbort( int level, const char *text ) __attribute__((noreturn));
#endif
void	RB_RealError( int level, const char *text );
void	R_BackendCvarSetValue( const char *name, float value );
void	R_BackendCvarSet( const char *name, const char *value );
void	R_BackendCvarDrain( void );
extern int	r_numVideoMaps;
qboolean RB_WorldEffectsNeedSync( void );
void	R_WorldEffectsFlushQueue( void );
void	R_WorldEffectsSetViewOrigin( const vec3_t origin );
qboolean R_RenderThreadBusy( void );
qboolean vkpt_physical_sky_load_pending( void );
void	R_G2_SwapRenderSurfHeap( void );

void *R_GetCommandBuffer( int bytes );

void R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs );

void RE_SetColor( const float *rgba );
void RE_StretchPic( float x, float y, float w, float h,
					  float s1, float t1, float s2, float t2, qhandle_t hShader );
void RE_RotatePic( float x, float y, float w, float h,
					  float s1, float t1, float s2, float t2,float a, qhandle_t hShader, float aspectCorrection );
void RE_RotatePic2( float x, float y, float w, float h,
					  float s1, float t1, float s2, float t2,float a, qhandle_t hShader, float aspectCorrection );
void RE_BeginFrame( stereoFrame_t stereoFrame );
void RE_EndFrame( int *frontEndMsec, int *backEndMsec );
void RE_TakeVideoFrame( int width, int height, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg );

/*
Ghoul2 Insert Start
*/
// tr_ghoul2.cpp
void			Multiply_3x4Matrix( mdxaBone_t *out, const mdxaBone_t *in2, const mdxaBone_t *in );
extern qboolean R_LoadMDXM ( model_t *mod, void *buffer, const char *name, qboolean &bAlreadyCached );
extern qboolean R_LoadMDXA ( model_t *mod, void *buffer, const char *name, qboolean &bAlreadyCached );
void			ResetGhoul2RenderableSurfaceHeap( void );
/*
Ghoul2 Insert End
*/

void R_InitDecals( void );
void RE_ClearDecals( void );
void RE_AddDecalToScene( qhandle_t shader, const vec3_t origin, const vec3_t dir, float orientation, 
	float r, float g, float b, float a, qboolean alphaFade, float radius, qboolean temporary );
void R_AddDecals( void );

// tr_surfacesprites
typedef struct {
	int			numIndexes;
	int			numVertexes;
	glIndex_t	indexes[SHADER_MAX_INDEXES]			QALIGN(16);
	vec4_t		xyz[SHADER_MAX_VERTEXES * 2]		QALIGN(16);
	vec4_t		normal[SHADER_MAX_VERTEXES]			QALIGN(16);
	color4ub_t	vertexColors[SHADER_MAX_VERTEXES]	QALIGN(16);
} ss_input;

void R_BindAnimatedImage(const textureBundle_t *bundle);
void RB_DrawSurfaceSprites(const shaderStage_t *stage, const ss_input *input);

qboolean ShaderHashTableExists(void);

// Vulkan

// shader
shader_t	*R_CreateShaderFromTextureBundle( const char *name, const textureBundle_t *bundle, uint32_t stateBits );

// debug
void		DrawTris( const shaderCommands_t *pInput );
void		DrawNormals( const shaderCommands_t *pInput );
void		RB_ShowImages( image_t** const pImg, uint32_t numImages );

// ...
void		R_IssueRenderCommands( qboolean runPerformanceCounters );
void		WIN_Shutdown( void );

// screenshot
void		R_TakeScreenshot( int x, int y, int width, int height, char *fileName );
void		R_TakeScreenshotJPEG( int x, int y, int width, int height, char *fileName );
void		R_TakeScreenshotPNG( int x, int y, int width, int height, char *fileName );
byte		*RB_ReadPixels( int x, int y, int width, int height, size_t *offset, int *padlen, int lineAlign );
extern byte	*rb_captureRGBA;	// set by RE_CaptureNextFrame
extern qboolean	rb_portraitReady;	// r_capturePortrait: the portrait scene was drawn this frame
void		R_CapturePortrait_f( void );

// lights
#ifdef USE_PMLIGHT
void		vk_lighting_pass( void );
qboolean	R_LightCullBounds( const dlight_t *dl, const vec3_t mins, const vec3_t maxs );
#endif

// image
void		R_InitImagesPool();
void		R_InitImageScratch( void );
void		R_DestroyImageScratch(void);

char		*GenerateImageMappingName( const char *name );

void		vk_bind( image_t *image );
void		vk_flush_staging_buffer( qboolean final );
void		vk_alloc_staging_buffer( VkDeviceSize size );
void		vk_upload_image( image_t *image, byte *pic );
void		vk_upload_image_data( image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, qboolean update ) ;
void		vk_generate_image_upload_data( image_t *image, byte *data, Image_Upload_Data *upload_data );
void		vk_create_image( image_t *image, int width, int height, int mip_levels );
void		vk_clean_staging_buffer( void );

// print queue (tr_subs.cpp): messages printed while the calling thread runs the backend wait in a queue
void		RB_PrintInstall( void );
void		RB_PrintBackendBegin( void );
void		RB_PrintBackendEnd( void );
void		RB_PrintDrain( void );

// ghoul2
void		R_G2_SnapReset( void );
void		R_G2_SnapshotScene( void );
void		R_G2_NoteDrawSurfsCmd( drawSurfsCommand_t *cmd );
void		RB_G2_UploadBones( const trRefdef_t *refdef );
int			RB_GetBoneUboOffset( const CRenderableSurface *surf );

// surface sprites
#ifdef USE_VBO_SS
	void	vk_clean_surface_sprites( void );
	void	vk_push_surface_sprites_cmd( const vk_ss_group_def_t *def, int firstInstance, int instanceCount );
	void	RB_SurfaceSpritesVBO( srfSprites_t *surf );
	const sprite_t	*vk_surface_sprites_cpu_instances( const VBO_t *vbo, int *count );
#endif

static QINLINE unsigned int log2pad(unsigned int v, int roundup)
{
	unsigned int x = 1;

	while (x < v) x <<= 1;

	if (roundup == 0) {
		if (x > v) {
			x >>= 1;
		}
	}

	return x;
}

void		ComputeColors( const int b, color4ub_t *dest, const shaderStage_t *pStage, int forceRGBGen );
void		ComputeTexCoords( const int b, const textureBundle_t *bundle );

#ifdef USE_VBO
// VBO functions
extern void R_BuildWorldVBO( msurface_t *surf, int surfCount );
extern void R_BuildSurfaceSpritesVBO( const world_t &worldData, int index ) ;
extern void R_BuildMDXM( model_t *mod, mdxmHeader_t *mdxm );
extern void R_BuildMD3( model_t *mod, mdvModel_t *mdvModel );
#ifdef _G2_GORE
extern void R_CreateGoreVBO( void );
extern void R_UpdateGoreVBO( srfG2GoreSurface_t *goreSurface );
extern void vk_flush_gore_uploads( VkCommandBuffer cmd );
#endif

extern void VBO_PushData( int itemIndex, shaderCommands_t *input );
extern void VBO_UnBind( void );

extern void VBO_Cleanup( void );
extern void VBO_QueueItem( int itemIndex );
extern void VBO_ClearQueue( void );
extern void VBO_Flush( void );

IBO_t *R_CreateIBO( const char *name, const byte *vbo_data, int vbo_size );
VBO_t *R_CreateVBO( const char *name, const byte *vbo_data, int vbo_size );
#endif
#ifdef USE_RTX
extern  cvar_t  *r_rtx;
extern  cvar_t  *pt_restir;
extern  cvar_t  *pt_caustics;
extern  cvar_t  *pt_dof;
extern  cvar_t  *pt_projection;
extern  cvar_t  *tm_blend_enable;
extern  cvar_t  *pt_debug_poly_lights;
extern  cvar_t  *pt_restir_m_clamp;
extern  cvar_t  *pt_debug_image;
extern  cvar_t  *pt_verbose;
extern  cvar_t  *pt_accumulation_rendering;
extern  cvar_t  *pt_accumulation_rendering_framenum;
extern  cvar_t  *pt_denoiser;
extern  cvar_t  *pt_nrd_max_accum;
extern  cvar_t  *pt_nrd_max_fast_accum;
extern  cvar_t  *pt_nrd_prepass_blur;
extern  cvar_t  *pt_nrd_antifirefly;
extern  cvar_t  *pt_nrd_hitdist_recon;
extern  cvar_t  *pt_nrd_direct;
extern  cvar_t  *pt_nrd_validation;
extern  cvar_t  *pt_dlight_radius;
extern  cvar_t  *pt_dlight_min_dist;
extern  cvar_t  *pt_jobs;
// Job pool loop, or a plain serial call when pt_jobs is 0.
void	RTX_ParallelFor( int count, int grain, void (*fn)( int begin, int end, void *ctx ), void *ctx );
extern  cvar_t  *pt_dlight_lift;
extern  cvar_t  *pt_lightgen_scale;
extern  cvar_t  *pt_light_scale_edit;
extern  cvar_t  *pt_light_scale_dlight;
extern  cvar_t  *pt_light_scale_saber;
extern  cvar_t  *pt_glow_scale;
extern  cvar_t  *pt_light_styles;
extern  cvar_t  *pt_weapon_fx_shaders;
extern  cvar_t  *pt_sky_per_map;
extern  cvar_t  *pt_sky_q3map_sun;
extern  cvar_t  *pt_ambient_floor_scale;
extern  cvar_t  *tm_per_channel;
extern  cvar_t  *tm_contrast;

#define UBO_CVAR_DO( _handle, _value ) \
	extern cvar_t *sun_##_handle;
	UBO_CVAR_LIST
#undef UBO_CVAR_DO

extern  cvar_t *sun_color[3];
extern  cvar_t *sun_elevation;
extern  cvar_t *sun_azimuth;
extern  cvar_t *sun_angle;
extern  cvar_t *sun_brightness;
extern  cvar_t *sun_bounce;
extern  cvar_t *sun_animate;
extern  cvar_t *sun_gamepad;

extern  cvar_t *sun_preset;
extern  cvar_t *sun_latitude;

extern  cvar_t *physical_sky;
extern  cvar_t *physical_sky_draw_clouds;
extern  cvar_t *physical_sky_space;
extern  cvar_t *physical_sky_brightness;

extern  cvar_t *sky_scattering;
extern  cvar_t *sky_transmittance;
extern  cvar_t *sky_phase_g;
extern  cvar_t *sky_amb_phase_g;
#endif
#ifdef USE_RTX
void RB_AddTriangle( vec3_t a, vec3_t b, vec3_t c, color4ub_t color );
#endif

#ifdef USE_RTX
	// Re-included with the linker switch on, now that every type it names is known.
	// vk_rtx.h closes its own include guard before this section precisely so a second
	// include reaches it - see the tail of that header.
	#define VK_RTX_LINKER
	#include "rtx/vk_rtx.h"
	#undef VK_RTX_LINKER
#endif

#endif // TR_LOCAL_H
