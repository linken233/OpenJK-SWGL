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

#include "tr_local.h"

#include "ghoul2/G2.h"
#include "qcommon/matcomp.h"

static	int			r_firstSceneDrawSurf;
#ifdef USE_PMLIGHT
static int			r_firstSceneLitSurf;
#endif

int					r_numdlights;
static	int			r_firstSceneDlight;

static	int			r_numentities;
static	int			r_firstSceneEntity;

static	int			r_numpolys;
static	int			r_firstScenePoly;

static	int			r_numpolyverts;

int					skyboxportal;
int					drawskyboxportal;
//static int	r_frameCount;	// incremented every frame
/*
====================
R_InitNextFrame

====================
*/
void R_InitNextFrame( void ) {
	backEndData->commands.used = 0;

	ResetGhoul2RenderableSurfaceHeap();
#ifdef USE_VBO_SS
	backEndData->ss.groups_count = 0;
#endif

	r_firstSceneDrawSurf = 0;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = 0;
#endif

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;

	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;

	R_G2_SnapReset();

	//r_frameCount++;
}

/*
====================
RE_ClearScene

====================
*/
void RE_ClearScene( void ) {
	r_firstSceneDlight = r_numdlights;
	r_firstSceneEntity = r_numentities;
	r_firstScenePoly = r_numpolys;
	tr.refdef.doLAGoggles = qfalse;
}

/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces( void ) {
	int				i;
	shader_t		*sh;
	const srfPoly_t	*poly;

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = (sortKey_t)tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	for ( i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys ; i++, poly++ ) {
		sh = R_GetShaderByHandle( poly->hShader );
		R_AddDrawSurf( (surfaceType_t *)poly, sh, poly->fogIndex, qfalse );
	}
}

/*
=====================
RE_AddPolyToScene

=====================
*/
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys ) {
	srfPoly_t		*poly;
	int				i, j;
	int				fogIndex;
	const fog_t		*fog;
	vec3_t			bounds[2];

	if ( !tr.registered ) {
		return;
	}

	if ( !hShader ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW  "WARNING: RE_AddPolyToScene: NULL poly shader\n");
		return;
	}

	for ( j = 0; j < numPolys; j++ ) {
		if ( r_numpolyverts + numVerts >= max_polyverts || r_numpolys >= max_polys ) {
      /*
      NOTE TTimo this was initially a PRINT_WARNING
      but it happens a lot with high fighting scenes and particles
      since we don't plan on changing the const and making for room for those effects
      simply cut this message to developer only
      */
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW  "WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n");
			return;
		}

		poly = &backEndData->polys[r_numpolys];
		poly->surfaceType = SF_POLY;
		poly->hShader = hShader;
		poly->numVerts = numVerts;
		poly->verts = &backEndData->polyVerts[r_numpolyverts];

		memcpy( poly->verts, &verts[numVerts*j], numVerts * sizeof( *verts ) );

		// done.
		r_numpolys++;
		r_numpolyverts += numVerts;

		fogIndex = 0;

		// see if it is in a fog volume
		if ( tr.world != NULL && tr.world->numfogs > 1 ) {
			// find which fog volume the poly is in
			VectorCopy( poly->verts[0].xyz, bounds[0] );
			VectorCopy( poly->verts[0].xyz, bounds[1] );
			for ( i = 1 ; i < poly->numVerts ; i++ ) {
				AddPointToBounds( poly->verts[i].xyz, bounds[0], bounds[1] );
			}
			for ( int fI = 1 ; fI < tr.world->numfogs ; fI++ ) {
				fog = &tr.world->fogs[fI];
				if ( bounds[0][0] >= fog->bounds[0][0]
					&& bounds[0][1] >= fog->bounds[0][1]
					&& bounds[0][2] >= fog->bounds[0][2]
					&& bounds[1][0] <= fog->bounds[1][0]
					&& bounds[1][1] <= fog->bounds[1][1]
					&& bounds[1][2] <= fog->bounds[1][2] ) {
					// completely in this one
					fogIndex = fI;
					break;
				}
				else if ( ( bounds[0][0] >= fog->bounds[0][0] && bounds[0][1] >= fog->bounds[0][1] && bounds[0][2] >= fog->bounds[0][2] &&
					bounds[0][0] <= fog->bounds[1][0] && bounds[0][1] <= fog->bounds[1][1] && bounds[0][2] <= fog->bounds[1][2] ) ||
					( bounds[1][0] >= fog->bounds[0][0] && bounds[1][1] >= fog->bounds[0][1] && bounds[1][2] >= fog->bounds[0][2] &&
					bounds[1][0] <= fog->bounds[1][0] && bounds[1][1] <= fog->bounds[1][1] && bounds[1][2] <= fog->bounds[1][2] ) ) {
					// partially in this one: take it first if the viewpoint is in the same fog
					if ( tr.refdef.fogIndex == fI || R_FogParmsMatch( tr.refdef.fogIndex, fI ) ) {
						fogIndex = fI;
						break;
					}
					else if ( !fogIndex ) {
						fogIndex = fI;
					}
				}
			}
		}
		poly->fogIndex = fogIndex;
	}
}

//=================================================================================

/*
=====================
RE_AddRefEntityToScene

=====================
*/
void RE_AddRefEntityToScene( const refEntity_t *ent ) {
	if ( !tr.registered ) {
		return;
	}

	if ( r_numentities >= MAX_REFENTITIES ) {
		ri.Printf(PRINT_DEVELOPER, "RE_AddRefEntityToScene: Dropping refEntity, reached MAX_REFENTITIES\n");
		return;
	}

	/*if ( Q_isnan(ent->origin[0]) || Q_isnan(ent->origin[1]) || Q_isnan(ent->origin[2]) ) {
		static qboolean firstTime = qtrue;
		if (firstTime) {
			firstTime = qfalse;
			ri.Printf( PRINT_WARNING, "RE_AddRefEntityToScene passed a refEntity which has an origin with a NaN component\n");
		}
		return;
	}*/

	assert(!ent || ent->renderfx >= 0);

	if (ent->reType == RT_ENT_CHAIN)
	{ //minirefents must die.
		return;
	}

#ifdef _DEBUG
	if (ent->reType == RT_MODEL)
	{
		//assert(ent->hModel || ent->ghoul2 || ent->customShader);
	}
#endif

	// RT_LATHE/RT_CLOUDS are SP-only refEntityType_t values with enum
	// numbers past RT_MAX_MP_REF_ENTITY_TYPE (see rd-common/tr_types.h) --
	// the MP-only bound below rejects them as "bad reType". Matches
	// code/rd-vanilla/tr_scene.cpp's own bounds check exactly.
	if ( (int)ent->reType < 0 || ent->reType >= RT_MAX_SP_REF_ENTITY_TYPE || ent->reType == RT_MAX_MP_REF_ENTITY_TYPE ) {
		Com_Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	backEndData->entities[r_numentities].e = *ent;
	backEndData->entities[r_numentities].lightingCalculated = qfalse;
	backEndData->entities[r_numentities].g2RtxFirst = 0;
	backEndData->entities[r_numentities].g2RtxCount = 0;

	r_numentities++;
}

/************************************************************************************************
 * RE_AddMiniRefEntityToScene                                                                   *
 *    Adds a mini ref ent to the scene.  If the input parameter is null, it signifies the end   *
 *    of the chain.  Otherwise, if there is a valid chain parent, it will be added to that.     *
 *    If there is no parent, it will be added as a regular ref ent.                             *
 *                                                                                              *
 * Input                                                                                        *
 *    ent: the mini ref ent to be added                                                         *
 *                                                                                              *
 * Output / Return                                                                              *
 *    none                                                                                      *
 *                                                                                              *
 ************************************************************************************************/
void RE_AddMiniRefEntityToScene( const miniRefEntity_t *ent )
{
#if 0
	refEntity_t		*parent;
#endif

	if ( !tr.registered )
	{
		return;
	}
	if (!ent)
	{
		return;
	}

	refEntity_t		tempEnt;

	memcpy(&tempEnt, ent, sizeof(*ent));
	memset(((char *)&tempEnt)+sizeof(*ent), 0, sizeof(tempEnt) - sizeof(*ent));
	RE_AddRefEntityToScene(&tempEnt);
}

/*
=====================
RE_AddDynamicLightToScene

=====================
*/
static void RE_AddDynamicLightToScene( const vec3_t org, float intensity, float r, float g, float b, int additive ) {
	dlight_t	*dl;

	if ( !tr.registered ) {
		return;
	}
	//if ( r_numdlights >= MAX_DLIGHTS ) {
	if (r_numdlights >= ARRAY_LEN(backEndData->dlights)) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
#ifdef USE_PMLIGHT
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif

	if (r_dlightSaturation->value != 1.0)
	{
		float luminance = LUMA(r, g, b);
		r = LERP(luminance, r, r_dlightSaturation->value);
		g = LERP(luminance, g, r_dlightSaturation->value);
		b = LERP(luminance, b, r_dlightSaturation->value);
	}

	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = additive;
	dl->linear = qfalse;
#ifdef USE_RTX
	// The tracer switches on this. Upstream's gamecode sets it; SP's never does, and
	// backEndData->dlights[] is never cleared, so it would hold whatever the slot held
	// last frame - a stale spot light with stale cone data.
	dl->light_type = DLIGHT_SPHERE;
#endif
}

/*
=====================
RE_AddLinearLightToScene
=====================
*/
void RE_AddLinearLightToScene( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b ) {
	dlight_t	*dl;
	if (VectorCompare(start, end)) {
		RE_AddDynamicLightToScene(start, intensity, r, g, b, 0);
		return;
	}
	if (!tr.registered) {
		return;
	}
	if (r_numdlights >= ARRAY_LEN(backEndData->dlights)) {
		return;
	}
	if (intensity <= 0) {
		return;
	}
#ifdef USE_PMLIGHT
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif

	if (r_dlightSaturation->value != 1.0)
	{
		float luminance = LUMA(r, g, b);
		r = LERP(luminance, r, r_dlightSaturation->value);
		g = LERP(luminance, g, r_dlightSaturation->value);
		b = LERP(luminance, b, r_dlightSaturation->value);
	}

	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy(start, dl->origin);
	VectorCopy(end, dl->origin2);
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = 0;
	dl->linear = qtrue;
#ifdef USE_RTX
	// The tracer has no linear/capsule light; a sphere at the start point is the closest
	// it can do, and it is still better than the stale type this would otherwise carry.
	dl->light_type = DLIGHT_SPHERE;
#endif
}

/*
=====================
RE_AddLightToScene

=====================
*/
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qfalse );
}

/*
=====================
RE_AddAdditiveLightToScene

=====================
*/
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qtrue );
}

/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderWorldEffects( void );
void RE_RenderAutoMap( void );
#ifdef VK_CUBEMAP
/*
=====================
R_RenderCubemaps

Up to CUBEMAPS_PER_FRAME * 6 faces, before the first world scene of the frame. The faces
of a probe can go to different frames. The views hold the world only: no entity, dlight or
poly of the scene.
The axes make the capture agree with the direction ( x, -y, z ) that pbr.glsl samples.
=====================
*/
void R_RenderCubemaps( void )
{
	static const float faceAxis[6][3][3] = {
		{ {  1,  0,  0 }, {  0,  0,  1 }, {  0, -1,  0 } },
		{ { -1,  0,  0 }, {  0,  0, -1 }, {  0, -1,  0 } },
		{ {  0, -1,  0 }, { -1,  0,  0 }, {  0,  0, -1 } },
		{ {  0,  1,  0 }, { -1,  0,  0 }, {  0,  0,  1 } },
		{ {  0,  0,  1 }, { -1,  0,  0 }, {  0, -1,  0 } },
		{ {  0,  0, -1 }, {  1,  0,  0 }, {  0, -1,  0 } },
	};
	// Draw surfs of the largest face so far, of the scene after the captures, and where the last captures ended.
	static int s_cubeMaxFace, s_cubeScenePeak, s_cubeLastEnd;
	viewParms_t parms;
	int i, face, rendered;
	const int entryDrawSurfs = tr.refdef.numDrawSurfs;

	if ( !vk.cubemapActive || !tr.world || tr.numCubemapsCaptured >= tr.numCubemaps )
		return;

	tr.refdef.x = tr.refdef.y = 0;
	tr.refdef.width = tr.refdef.height = REF_CUBEMAP_SIZE;
	tr.refdef.fov_x = tr.refdef.fov_y = 90.0f;
	tr.refdef.rdflags = 0;
	tr.refdef.needScreenMap = qfalse;
	tr.refdef.switchRenderPass = qfalse;

	// all areas: the probe is not in the area of the player
	Com_Memset( tr.refdef.areamask, 0, sizeof( tr.refdef.areamask ) );
	tr.refdef.areamaskModified = qtrue;

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;
#ifdef USE_PMLIGHT
	tr.refdef.numLitSurfs = r_firstSceneLitSurf;
	tr.refdef.litSurfs = backEndData->litSurfs;
#endif
	tr.refdef.num_entities = 0;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];
	tr.refdef.num_dlights = 0;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];
	tr.refdef.numPolys = 0;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];
	tr.refdef.g2SnapFirst = 0;
	tr.refdef.g2SnapCount = 0;

	tr.frameSceneNum++;
	tr.sceneCount++;

	i = tr.numCubemapsCaptured;
	face = tr.numCubemapFacesCaptured;

	// The scene size of the last frame: draw surfs after the captures, with a slow decay.
	if ( s_cubeLastEnd > 0 && entryDrawSurfs >= s_cubeLastEnd )
		s_cubeScenePeak = Q_max( entryDrawSurfs - s_cubeLastEnd, s_cubeScenePeak - s_cubeScenePeak / 16 );
	else
		s_cubeScenePeak -= s_cubeScenePeak / 16;

	for ( rendered = 0; i < tr.numCubemaps && rendered < CUBEMAPS_PER_FRAME * 6; rendered++ )
	{
		int faceStart;

		// The commands of the views read the draw surfs in the backend. Stop before the next face, one
		// more face and the scene of the frame could fill the list. The list also refuses to wrap.
		if ( tr.refdef.numDrawSurfs > CUBEMAP_DRAWSURF_LIMIT
			|| tr.refdef.numDrawSurfs + 2 * s_cubeMaxFace + s_cubeScenePeak > MAX_DRAWSURFS )
			break;

		faceStart = tr.refdef.numDrawSurfs;

		{
			VectorCopy( tr.cubemaps[i].origin, tr.refdef.vieworg );
			VectorCopy( faceAxis[face][0], tr.refdef.viewaxis[0] );
			VectorCopy( faceAxis[face][1], tr.refdef.viewaxis[1] );
			VectorCopy( faceAxis[face][2], tr.refdef.viewaxis[2] );

			Com_Memset( &parms, 0, sizeof( parms ) );
			parms.viewportWidth = parms.viewportHeight = REF_CUBEMAP_SIZE;
			parms.scissorWidth = parms.scissorHeight = REF_CUBEMAP_SIZE;
			parms.portalView = PV_NONE;
#ifdef USE_PMLIGHT
			parms.dlights = tr.refdef.dlights;
			parms.num_dlights = 0;
#endif
			parms.zNear = r_znear->value;
			parms.fovX = parms.fovY = 90.0f;

			VectorCopy( tr.refdef.vieworg, parms.ori.origin );
			VectorCopy( tr.refdef.viewaxis[0], parms.ori.axis[0] );
			VectorCopy( tr.refdef.viewaxis[1], parms.ori.axis[1] );
			VectorCopy( tr.refdef.viewaxis[2], parms.ori.axis[2] );
			VectorCopy( tr.refdef.vieworg, parms.pvsOrigin );

			parms.targetCube = i + 1;
			parms.targetCubeFace = face;

			R_RenderView( &parms );
		}

		s_cubeMaxFace = Q_max( s_cubeMaxFace, tr.refdef.numDrawSurfs - faceStart );

		if ( ++face == 6 )
		{
			R_AddConvolveCubemapCmd( i );
			i++;
			face = 0;
		}
	}

	R_G2_SnapshotScene();	// no entities; forgets the commands of the views

	tr.numCubemapsCaptured = i;
	tr.numCubemapFacesCaptured = face;

	// the scene of the frame goes after the captures
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
	s_cubeLastEnd = r_firstSceneDrawSurf;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = tr.refdef.numLitSurfs;
#endif
}
#endif

#ifdef USE_RTX
// An impact effect places its dlight exactly on the surface it hit. The raster lights that
// surface from any distance, but the tracer needs dot(n, L) > 0: a light in the plane of a
// wall lights nothing of it, and the few samples that pass come back noisy. Find the nearest
// surface within probe units and move the light off it along the surface normal.
static void R_DlightLiftOffSurface( vec3_t origin, float lift, float probe )
{
	static const vec3_t axes[6] = {
		{  1,  0,  0 }, { -1,  0,  0 }, {  0,  1,  0 }, {  0, -1,  0 }, {  0,  0,  1 }, {  0,  0, -1 }
	};

	trace_t best;
	float best_fraction = 2.0f;

	for ( int i = 0; i < 6; i++ )
	{
		vec3_t end;
		trace_t tr;

		VectorMA( origin, probe, axes[i], end );
		ri.SV_Trace( &tr, origin, vec3_origin, vec3_origin, end, ENTITYNUM_NONE, CONTENTS_SOLID, G2_NOCOLLIDE, 0 );

		if ( tr.startsolid || tr.allsolid || tr.fraction >= 1.0f )
			continue;

		if ( tr.fraction < best_fraction )
		{
			best_fraction = tr.fraction;
			best = tr;
		}
	}

	if ( best_fraction > 1.0f )
		return;

	// Do not push the light through the surface on the far side of a thin room.
	vec3_t target, lifted;
	trace_t tr;

	VectorMA( best.endpos, lift, best.plane.normal, target );
	ri.SV_Trace( &tr, best.endpos, vec3_origin, vec3_origin, target, ENTITYNUM_NONE, CONTENTS_SOLID, G2_NOCOLLIDE, 0 );
	VectorMA( best.endpos, tr.fraction * lift * 0.9f, best.plane.normal, lifted );

	VectorCopy( lifted, origin );
}

/*
=====================
R_LiftDlightsForRtx

Sets rtxOrigin of the dlights of a scene that the tracer draws. The tracer tracks the lights
by origin, so it keeps origin as it is. The lift calls the collision code of the game,
which the backend must not do.
=====================
*/
static void R_LiftDlightsForRtx( const trRefdef_t *refdef )
{
	if ( !vk.rtxActive || ( refdef->rdflags & ( RDF_NOWORLDMODEL | RDF_SKYBOXPORTAL ) ) )
		return;

	// the tracer reads the first 128 lights of the scene (DLIGHT_TRACK_MAX)
	const int count = MIN( refdef->num_dlights, 128 );

	for ( int i = 0; i < count; i++ )
	{
		dlight_t *dlight = refdef->dlights + i;

		VectorCopy( dlight->origin, dlight->rtxOrigin );

		if ( dlight->light_type != DLIGHT_SPOT && pt_dlight_lift->value > 0.0f )
			R_DlightLiftOffSurface( dlight->rtxOrigin, pt_dlight_lift->value, 4.0f );
	}
}
#endif

void RE_RenderScene( const refdef_t *fd ) {
	renderCommand_t	lastRenderCommand;
	viewParms_t		parms;
	int				startTime;
	static	int		lastTime = 0;
	vk_perf_scope	perfScene( VKP_SCENE );

	if (!tr.registered) {
		return;
	}

	if (r_norefresh->integer) {
		return;
	}

	startTime = ri.Milliseconds() * ri.Cvar_VariableValue("timescale");

	if (!tr.world && !(fd->rdflags & RDF_NOWORLDMODEL)) {
		Com_Error(ERR_DROP, "R_RenderScene: NULL worldmodel");
	}

#ifdef VK_CUBEMAP
	if ( !( fd->rdflags & ( RDF_NOWORLDMODEL | RDF_SKYBOXPORTAL ) ) )
		R_RenderCubemaps();
#endif

	memcpy(tr.refdef.text, fd->text, sizeof(tr.refdef.text));

	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy(fd->vieworg, tr.refdef.vieworg);
	VectorCopy(fd->viewaxis[0], tr.refdef.viewaxis[0]);
	VectorCopy(fd->viewaxis[1], tr.refdef.viewaxis[1]);
	VectorCopy(fd->viewaxis[2], tr.refdef.viewaxis[2]);

	tr.refdef.time = fd->time;
	tr.refdef.frametime = fd->time - lastTime;

	if (fd->rdflags & RDF_SKYBOXPORTAL)
	{
		skyboxportal = 1;
	}
	else
	{
		// pasted this from SP
		// cdr - only change last time for the real render, not the portal
		lastTime = fd->time;
	}

	if (fd->rdflags & RDF_DRAWSKYBOX)
	{
		drawskyboxportal = 1;
	}
	else
	{
		drawskyboxportal = 0;
	}

	if (tr.refdef.frametime > 500)
	{
		tr.refdef.frametime = 500;
	}
	else if (tr.refdef.frametime < 0)
	{
		tr.refdef.frametime = 0;
	}
	tr.refdef.rdflags = fd->rdflags;

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if (!(tr.refdef.rdflags & RDF_NOWORLDMODEL)) {
		int		areaDiff;
		int		i;

		// compare the area bits
		areaDiff = 0;
		for (i = 0; i < MAX_MAP_AREA_BYTES / sizeof(int); i++) {
			areaDiff |= ((int*)tr.refdef.areamask)[i] ^ ((int*)fd->areamask)[i];
			((int*)tr.refdef.areamask)[i] = ((int*)fd->areamask)[i];
		}

		if (areaDiff) {
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}


	// derived info

	tr.refdef.floatTime = tr.refdef.time * 0.001f;

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;

#ifdef USE_PMLIGHT
	tr.refdef.numLitSurfs = r_firstSceneLitSurf;
	tr.refdef.litSurfs = backEndData->litSurfs;
#endif

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];

	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];

	// Add the decals here because decals add polys and we need to ensure
	// that the polys are added before the the renderer is prepared
	if (!(tr.refdef.rdflags & RDF_NOWORLDMODEL))
	{
		R_AddDecals();
	}

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];

	// turn off dynamic lighting globally by clearing all the
	// dlights if it needs to be disabled or if vertex lighting is enabled
	if (r_dynamiclight->integer == 0 || r_vertexLight->integer == 1) {
		tr.refdef.num_dlights = 0;
	}

#ifdef USE_RTX
	R_LiftDlightsForRtx(&tr.refdef);
#endif

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates, so
	// convert to GL's 0-at-the-bottom space
	//
	memset(&parms, 0, sizeof(parms));
	parms.viewportX = tr.refdef.x;
	parms.viewportY = glConfig.vidHeight - (tr.refdef.y + tr.refdef.height);
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;

	parms.scissorX = parms.viewportX;
	parms.scissorY = parms.viewportY;
	parms.scissorWidth = parms.viewportWidth;
	parms.scissorHeight = parms.viewportHeight;

	parms.portalView = PV_NONE;

#ifdef USE_PMLIGHT
	parms.dlights = tr.refdef.dlights;
	parms.num_dlights = tr.refdef.num_dlights;
#endif

	parms.zNear = r_znear->value;

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;

	VectorCopy(fd->vieworg, parms.ori.origin);
	VectorCopy(fd->viewaxis[0], parms.ori.axis[0]);
	VectorCopy(fd->viewaxis[1], parms.ori.axis[1]);
	VectorCopy(fd->viewaxis[2], parms.ori.axis[2]);

	VectorCopy(fd->vieworg, parms.pvsOrigin);

	lastRenderCommand = (renderCommand_t)tr.lastRenderCommand;
	tr.drawSurfCmd = NULL;
	tr.numDrawSurfCmds = 0;

	tr.refdef.g2SnapFirst = 0;
	tr.refdef.g2SnapCount = 0;

	R_RenderView(&parms);

	// All views of the scene are built: copy the poses of its Ghoul2 models for the backend.
	R_G2_SnapshotScene();

	// The entity UBO of the backend holds the lighting of every model.
	if (vk.vboGhoul2Active)
	{
		for (int i = 0; i < tr.refdef.num_entities; i++)
		{
			trRefEntity_t *ent = &tr.refdef.entities[i];

			if (ent->e.reType == RT_MODEL)
				R_SetupEntityLighting(&tr.refdef, ent);
		}
	}

	vk_perf_count( VKP_ENTITIES, tr.refdef.num_entities );
	vk_perf_count( VKP_DLIGHTS, tr.refdef.num_dlights );
	vk_perf_count( VKP_DRAWSURFS, tr.refdef.numDrawSurfs );
	vk_perf_count( VKP_POLYS, tr.refdef.numPolys );

	if (tr.needScreenMap)
	{
		if (lastRenderCommand == RC_DRAW_BUFFER) {
			// duplicate all views, including portals
			drawSurfsCommand_t *cmd, *src = NULL;
			int i;

			for (i = 0; i < tr.numDrawSurfCmds; i++)
			{
				cmd = (drawSurfsCommand_t*)R_GetCommandBuffer(sizeof(*cmd));
				src = tr.drawSurfCmd + i;
				*cmd = *src;
			}

			if (src)
			{
				// first drawsurface
				tr.drawSurfCmd[0].refdef.needScreenMap = qtrue;
				// last drawsurface
				src->refdef.switchRenderPass = qtrue;
			}
		}

		tr.needScreenMap = 0;
	}

	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = tr.refdef.numLitSurfs;
#endif
	r_firstSceneEntity = r_numentities;
	r_firstSceneDlight = r_numdlights;
	r_firstScenePoly = r_numpolys;

	tr.frontEndMsec += ri.Milliseconds()*ri.Cvar_VariableValue( "timescale" ) - startTime;

	RE_RenderWorldEffects();

#ifdef RDF_AUTOMAP
	if ( tr.refdef.rdflags & RDF_AUTOMAP )
	{
		RE_RenderAutoMap();
	}
#endif
}

#if 0 //rwwFIXMEFIXME: Disable this before release!!!!!! I am just trying to find a crash bug.
int R_GetRNumEntities(void)
{
	return r_numentities;
}

void R_SetRNumEntities(int num)
{
	r_numentities = num;
}
#endif
