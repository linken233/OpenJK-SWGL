/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Light edit extension of the RTX renderer.
//
// The client gets this table from refexport_t::GetExtension( RTX_LIGHTEDIT_API_NAME ).
// A renderer without the RTX path returns NULL. All calls run on the main thread, between
// two frames. An id is stable until the next map load: a removed light keeps its id.
// A client uses the table only when version is RTX_LIGHTEDIT_API_VERSION: the structures
// change between versions. Build the renderer, the game DLL and the exe together.

#pragma once

#define RTX_LIGHTEDIT_API_NAME		"rtxLightEdit_v1"
#define RTX_LIGHTEDIT_API_VERSION	7

#define RTX_LIGHTEDIT_NAME_LEN		32

// Where a light comes from.
typedef enum {
	RTX_LSRC_ADDED = 0,		// made in the editor or read from an "added" block of the .lgt
	RTX_LSRC_ENTITY,		// a `light` entity of the BSP
	RTX_LSRC_LGT,			// a light that pt_lightgen reconstructed from the lightmaps
	RTX_LSRC_EMISSIVE		// version 6: the emissive polygons of one BSP surface (face)
} rtxLightSource_t;

typedef enum {
	RTX_LTYPE_SPHERE = 0,
	RTX_LTYPE_SPOT,
	RTX_LTYPE_RECT,			// version 4: a rectangle of two polygon lights (four when two-sided)
	RTX_LTYPE_SURFACE		// version 6: an emissive surface; Set copies intensity (factor), color (tint) and style only
} rtxLightType_t;

// Limits of a rectangle light, in world units.
#define RTX_LRECT_MIN_SIZE		1.0f
#define RTX_LRECT_MAX_SIZE		2048.0f
#define RTX_LRECT_DEFAULT_SIZE	32.0f

// Light styles. Style 0 is steady. Styles 1..63 follow styleColors[], which the cgame
// sets each frame from the CS_LIGHT_STYLES config strings.
#define RTX_LSTYLE_MAX			64

// Flags of rtxLightDesc_t. Get sets them; Add and Set ignore them.
#define RTX_LFLAG_MODIFIED		0x0001	// a source light that differs from its original values
#define RTX_LFLAG_DISABLED		0x0002	// a source light that Remove switched off
#define RTX_LFLAG_DELETED		0x0004	// an added light that Remove deleted
#define RTX_LFLAG_IN_SOLID		0x0008	// the origin is outside the world; the light emits nothing
#define RTX_LFLAG_DRAGGING		0x0010	// between BeginDrag and EndDrag
#define RTX_LFLAG_MUTED			0x0020	// switched off by Mute or Solo; not saved, not an edit
#define RTX_LFLAG_COMPACTED		0x0040	// removed (DISABLED or DELETED too) and its slots released by Compact; Restore takes new slots

typedef struct {
	int		id;				// Get only
	int		source;			// rtxLightSource_t, Get only
	int		sourceKey;		// rank among the `light` entities, .lgt block index, or rank of the face among those of its shader; -1 for added
	int		type;			// rtxLightType_t
	int		flags;			// RTX_LFLAG_*, Get only

	vec3_t	origin;
	vec3_t	color;			// 0..1
	float	intensity;		// units of the source: q3map2 `light` for added and entity lights, lightmap units for lgt
	float	radius;			// emitter radius, sets the shadow softness only

	vec3_t	dir;			// spot: unit vector of the axis; rect: unit normal, the lit side
	float	coneOuter;		// spot only, half angle in degrees, 1..89
	float	coneInner;		// spot only, half angle in degrees, 0..coneOuter

	char	name[RTX_LIGHTEDIT_NAME_LEN];

	// Version 4.
	float	width;			// rect only: size along the U axis of RTX_LightRectAxes
	float	height;			// rect only: size along the V axis
	float	roll;			// rect only: rotation around dir, in degrees
	int		twoSided;		// rect only: 1 when both sides emit
	int		style;			// all types: light style, 0 = steady, 1 .. RTX_LSTYLE_MAX - 1
} rtxLightDesc_t;

// Version 6: an emissive surface is the group of the emissive polygons that one BSP face gave. Its
// record has the source RTX_LSRC_EMISSIVE and the type RTX_LTYPE_SURFACE. origin is a point of the
// surface and radius the half size of its box. intensity is the factor of the light it casts, color
// the tint of that light, style its light style. Only the cast light changes, never the look of the
// surface. The factor of the shader (SetEmissiveScale) applies on top.
#define RTX_LEMI_MAX_EDGES		32
#define RTX_LEMI_SHADER_LEN		64

typedef struct {
	int		shader;				// index for GetEmissiveShader
	char	shaderName[RTX_LEMI_SHADER_LEN];
	float	shaderScale;		// factor of the shader
	int		face;				// index of the BSP surface
	int		rank;				// rank of the face among the faces of the shader, in load order
	int		numTris;			// polygons of the group
	int		numSlots;			// slots it holds now: numTris, or 0 once compacted
	vec3_t	mins, maxs;			// box of the polygons
	int		numEdges;			// outline: the boundary edges of the polygons, or the 12 edges of the box
	float	edges[RTX_LEMI_MAX_EDGES][6];	// x y z of both ends
} rtxEmissiveSurface_t;

// Axes of a rectangle light. The renderer and the cgame use this function, so the
// overlay and the tracer agree. U and V are unit vectors, perpendicular to normal.
static inline void RTX_LightRectAxes( const float *normal, float rollDegrees, float *u, float *v )
{
	float	u0[3], v0[3], len, c, s;

	if ( normal[2] > 0.99f || normal[2] < -0.99f )
	{
		u0[0] = 1.0f; u0[1] = 0.0f; u0[2] = 0.0f;
	}
	else
	{
		// u0 = normalize( Z x normal ), horizontal
		u0[0] = -normal[1]; u0[1] = normal[0]; u0[2] = 0.0f;
		len = sqrtf( u0[0] * u0[0] + u0[1] * u0[1] );
		u0[0] /= len; u0[1] /= len;
	}

	// v0 = normal x u0
	v0[0] = normal[1] * u0[2] - normal[2] * u0[1];
	v0[1] = normal[2] * u0[0] - normal[0] * u0[2];
	v0[2] = normal[0] * u0[1] - normal[1] * u0[0];

	c = cosf( rollDegrees * 0.017453292f );
	s = sinf( rollDegrees * 0.017453292f );

	for ( int k = 0; k < 3; k++ )
	{
		u[k] = u0[k] * c + v0[k] * s;
		v[k] = v0[k] * c - u0[k] * s;
	}
}

// Version 4: sky and sun of the map. The renderer keeps one setting per map and saves it
// in the `global` block of the .lgt.
typedef enum {
	RTX_SKY_GLOBAL = 0,		// no setting for the map: the physical_sky and sun_* cvars apply
	RTX_SKY_SKYBOX,			// the skybox of the map, no sun
	RTX_SKY_PHYSICAL,		// the physical sky (earth) and its sun
	RTX_SKY_HYBRID			// the skybox of the map, with an analytic sun
} rtxSkyMode_t;

#define RTX_SKY_FROM_FILE		0x0001	// Get only: the .lgt holds a global block
#define RTX_SKY_FROM_Q3MAP_SUN	0x0002	// Get only: the sun values come from q3map_sun
#define RTX_SKY_MAP_HAS_SUN		0x0004	// Get only: the sky shader of the map has q3map_sun

typedef struct {
	int		mode;			// rtxSkyMode_t
	int		flags;			// RTX_SKY_*, Get only
	float	sunAzimuth;		// degrees, as the sun_azimuth cvar
	float	sunElevation;	// degrees, as the sun_elevation cvar
	vec3_t	sunColor;		// as sun_color_r/g/b
	float	sunBrightness;	// as sun_brightness
	float	sunAngle;		// angular diameter in degrees, 1..10, as sun_angle
	// Version 7.
	float	skyDiffuse;		// 0..1: cloud layer. The direct sun scales by (1 - skyDiffuse), the rest is sky light. Modes PHYSICAL and HYBRID only
	vec3_t	ambientColor;	// 0..1 per component: colour of the ambient floor. It applies in every mode, GLOBAL included
	float	ambientIntensity;	// 0..100: 0 = no floor
} rtxSkyDesc_t;

typedef struct {
	int		numRecords;			// ids are 0 .. numRecords - 1
	int		numEntity;			// source lights of each kind, deleted added lights excluded
	int		numLgt;
	int		numAdded;
	int		numModified;
	int		numDisabled;		// removed source lights that still hold their slots
	int		numCompacted;		// removed lights whose slots Compact released (added lights included)
	int		numInSolid;
	int		numEmissive;		// emissive surfaces (records of the source RTX_LSRC_EMISSIVE)
	int		numEmissivePolys;	// their polygons
	int		numLightPolys;		// static entries the tracer sees, spare slots included
	int		slotsUsed;			// entries that records hold (emissive surfaces included); the others are free
	int		capacity;			// static entries before the next growth
	int		maxLightPolys;		// hard limit on static entries
	int		overfullClusters;	// clusters that hit MAX_LIGHTS_PER_CLUSTER at the last rebuild
	int		rebuilds;			// cluster list rebuilds since the map load
	int		unsavedChanges;		// edits since the last load, save or reload
	char	mapName[64];
} rtxLightStats_t;

typedef struct rtxLightEditAPI_s {
	int			version;	// RTX_LIGHTEDIT_API_VERSION

	// True when the RTX path is active and a world is loaded.
	qboolean	(*IsAvailable)( void );

	// Begin reserves spare light slots. End releases nothing; it ends the drag, if any.
	qboolean	(*Begin)( void );
	void		(*End)( void );

	int			(*Count)( void );
	qboolean	(*Get)( int id, rtxLightDesc_t *out );

	// Original values of a source light, as read at map load. An added light gives the
	// values of its Add call (or of its .lgt block).
	qboolean	(*GetOriginal)( int id, rtxLightDesc_t *out );

	// Add returns the new id, or -1. LastError then gives the reason.
	int			(*Add)( const rtxLightDesc_t *desc );

	// Set copies origin, color, intensity, radius, name, type, dir and cones. A source light then gets
	// RTX_LFLAG_MODIFIED when it differs from its original values.
	qboolean	(*Set)( int id, const rtxLightDesc_t *desc );

	// Remove deletes an added light and disables a source light. Restore undoes Remove.
	qboolean	(*Remove)( int id );
	qboolean	(*Restore)( int id );

	// Revert gives a source light its original values back. It does not touch DISABLED.
	qboolean	(*Revert)( int id );

	// Between BeginDrag and EndDrag, Set moves the light without a full rebuild at each call.
	void		(*BeginDrag)( int id );
	void		(*EndDrag)( int id );

	// Save writes maps/<map>.lgt (format v2) to the homepath, after a copy to .lgt.bak.
	// Reload drops the edits in memory and applies the file again.
	qboolean	(*Save)( void );
	qboolean	(*Reload)( void );

	void		(*GetStats)( rtxLightStats_t *out );
	const char	*(*LastError)( void );

	// True when the point is outside the world (no cluster).
	qboolean	(*PointInSolid)( const vec3_t point );

	// Version 2.

	// Between BeginBatch and EndBatch, the cluster lists are rebuilt once, at EndBatch.
	void		(*BeginBatch)( void );
	void		(*EndBatch)( void );

	// Mute switches a light off for the session only: no edit, no save, no undo.
	// Solo mutes every editable light except id; Solo( -1 ) ends it. Mute and Solo are
	// independent: a light emits when no Mute and no Solo hides it.
	qboolean	(*Mute)( int id, qboolean muted );
	void		(*Solo)( int id );
	int			(*GetSolo)( void );

	// Counts each change of the light list. The tracer restarts its accumulation when it
	// changes.
	int			(*Generation)( void );

	// Factor from the intensity of a light to what the tracer emits, for its source and
	// class (pt_light_scale_edit, pt_lightgen_scale). -1 gives the factor of a new added light.
	// A copy between two lights keeps its brightness with I2 = I1 * scale1 / scale2.
	float		(*IntensityScale)( int id );

	// Version 3.

	// Version 6 removed CountEmissive, GetEmissive and EmissiveShaderOf: an emissive polygon has no
	// index any more, its surface is a record (see GetEmissiveSurface).
	// Dynamic lights of the last traced frame (dlights, sabers, beams), read only. Fills at
	// most maxCount entries and returns how many it filled.
	int			(*GetDynamic)( int maxCount, vec3_t *origins, vec3_t *colors );

	// Version 4.

	// Light emission factor of each emissive shader. The factor scales the light that the
	// polygons of the shader cast and the emission of the surface (within 4% steps).
	// Shader indices are stable until the next map load. SetEmissiveScale counts as an edit.
	int			(*CountEmissiveShaders)( void );
	qboolean	(*GetEmissiveShader)( int shader, char *name, int nameSize, float *scale, int *numPolys );
	qboolean	(*SetEmissiveScale)( int shader, float scale );	// 0 .. 100, 1 = unchanged

	// Sky and sun of the map. GetSky gives the values in effect. SetSky applies them now and
	// counts as an edit. SetSky with mode RTX_SKY_GLOBAL removes the sky setting and keeps the ambient
	// floor of the desc. ResetSky removes the sky setting and keeps the ambient floor.
	qboolean	(*GetSky)( rtxSkyDesc_t *out );
	qboolean	(*SetSky)( const rtxSkyDesc_t *desc );
	void		(*ResetSky)( void );

	// Version 5.

	// Compact releases the slots of every removed light and packs the used slots. The ids,
	// the records and the file do not change. The released lights get RTX_LFLAG_COMPACTED;
	// Restore gives them new slots. Gives the number of lights it compacted, -1 on error.
	// slotsReleased may be NULL.
	int			(*Compact)( int *slotsReleased );

	// Version 6.

	// Description of the emissive surface of a record (source RTX_LSRC_EMISSIVE). The other values of
	// the record come from Get. Compact releases the slots of a removed surface like those of a light.
	qboolean	(*GetEmissiveSurface)( int id, rtxEmissiveSurface_t *out );
} rtxLightEditAPI_t;
