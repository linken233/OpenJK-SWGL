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
#include "tr_WorldEffects.h"

backEndData_t	*backEndData;
backEndState_t	backEnd;

//bool tr_stencilled = false;
//extern qboolean tr_distortionPrePost;
//extern qboolean tr_distortionNegate;
//extern void RB_CaptureScreenImage(void);
//extern void RB_DistortionFill(void);

#define	MAC_EVENT_PUMP_MSEC		5

#if 0
//used by RF_DISTORTION
static inline bool R_WorldCoordToScreenCoordFloat( vec3_t worldCoord, float *x, float *y )
{
	int	xcenter, ycenter;
	vec3_t	local, transformed;
	vec3_t	vfwd;
	vec3_t	vright;
	vec3_t	vup;
	float xzi;
	float yzi;

	xcenter = glConfig.vidWidth / 2;
	ycenter = glConfig.vidHeight / 2;

	//AngleVectors (tr.refdef.viewangles, vfwd, vright, vup);
	VectorCopy(tr.refdef.viewaxis[0], vfwd);
	VectorCopy(tr.refdef.viewaxis[1], vright);
	VectorCopy(tr.refdef.viewaxis[2], vup);

	VectorSubtract (worldCoord, tr.refdef.vieworg, local);

	transformed[0] = DotProduct(local,vright);
	transformed[1] = DotProduct(local,vup);
	transformed[2] = DotProduct(local,vfwd);

	// Make sure Z is not negative.
	if(transformed[2] < 0.01)
	{
		return false;
	}

	xzi = xcenter / transformed[2] * (90.0/tr.refdef.fov_x);
	yzi = ycenter / transformed[2] * (90.0/tr.refdef.fov_y);

	*x = xcenter + xzi * transformed[0];
	*y = ycenter - yzi * transformed[1];

	return true;
}

//used by RF_DISTORTION
static inline bool R_WorldCoordToScreenCoord( vec3_t worldCoord, int *x, int *y )
{
	float	xF, yF;
	bool retVal = R_WorldCoordToScreenCoordFloat( worldCoord, &xF, &yF );
	*x = (int)xF;
	*y = (int)yF;
	return retVal;
}

//number of possible surfs we can postrender.
//note that postrenders lack much of the optimization that the standard sort-render crap does,
//so it's slower.
#define MAX_POST_RENDERS	128

typedef struct postRender_s {
	int			fogNum;
	int			entNum;
	int			dlighted;
	int			depthRange;
	drawSurf_t	*drawSurf;
	shader_t	*shader;
	qboolean	eValid;
} postRender_t;

static postRender_t g_postRenders[MAX_POST_RENDERS];
static int g_numPostRenders = 0;
#endif

/*
================
R_DistortionScreenCrop

SP draws RF_DISTORTION by copying a radius-sized square of the screen around the entity
and texturing the effect with that crop - see rd-vanilla's RB_RenderDrawSurfList. This
renderer already keeps one full-screen extract, so the same crop is expressed as a
texcoord scale + offset over it. Fills crop as { scaleS, scaleT, offsetS, offsetT }.

Unlike vanilla the window is centred on the entity rather than mirrored across both axes;
vanilla's cX/cY arithmetic subtracts the projected position from the screen size on X too,
which lands the copy on the opposite side of the screen.
================
*/
qboolean R_DistortionScreenCrop( const trRefEntity_t *ent, vec4_t crop )
{
	vec3_t	local, transformed;
	float	xcenter, ycenter, xzi, yzi, x, y, rad, maxX, maxY;

	rad = ent->e.radius;

	if ( rad < 1.0f || glConfig.vidWidth <= 0 || glConfig.vidHeight <= 0 )
		return qfalse;

	if ( rad > glConfig.vidWidth )
		rad = (float)glConfig.vidWidth;
	if ( rad > glConfig.vidHeight )
		rad = (float)glConfig.vidHeight;

	xcenter = glConfig.vidWidth * 0.5f;
	ycenter = glConfig.vidHeight * 0.5f;

	VectorSubtract( ent->e.origin, backEnd.refdef.vieworg, local );

	transformed[0] = DotProduct( local, backEnd.refdef.viewaxis[1] );
	transformed[1] = DotProduct( local, backEnd.refdef.viewaxis[2] );
	transformed[2] = DotProduct( local, backEnd.refdef.viewaxis[0] );

	// behind the eye, nothing sensible to crop around
	if ( transformed[2] < 0.01f )
		return qfalse;

	xzi = xcenter / transformed[2] * ( 90.0f / backEnd.refdef.fov_x );
	yzi = ycenter / transformed[2] * ( 90.0f / backEnd.refdef.fov_y );

	x = xcenter + xzi * transformed[0] - rad * 0.5f;
	y = ycenter - yzi * transformed[1] - rad * 0.5f;

	maxX = (float)glConfig.vidWidth - rad;
	maxY = (float)glConfig.vidHeight - rad;

	if ( x > maxX ) x = maxX;
	if ( x < 0.0f ) x = 0.0f;
	if ( y > maxY ) y = maxY;
	if ( y < 0.0f ) y = 0.0f;

	crop[0] = rad / (float)glConfig.vidWidth;
	crop[1] = rad / (float)glConfig.vidHeight;
	crop[2] = x / (float)glConfig.vidWidth;
	crop[3] = y / (float)glConfig.vidHeight;

	return qtrue;
}

/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace(void) {
	color4ub_t c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	if ( tess.shader != tr.whiteShader ) {
		RB_EndSurface();
		vk_set_2d();
		RB_BeginSurface( tr.whiteShader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	vk_set_2d();

	c[0] = c[1] = c[2] = ( backEnd.refdef.time & 255 );
	c[3] = 255;

	RB_AddQuadStamp2( backEnd.refdef.x, backEnd.refdef.y, backEnd.refdef.width, backEnd.refdef.height,
		0.0, 0.0, 0.0, 0.0, c );

	RB_EndSurface();

	tess.numIndexes = 0;
	tess.numVertexes = 0;

	backEnd.isHyperspace = qtrue;
}

#ifdef USE_PMLIGHT
static void RB_LightingPass(void);
#endif

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
static void RB_BeginDrawingView( void ) {

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		vk_queue_wait_idle();

		glState.finishCalled = qtrue;
	} else if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	// force depth range and viewport/scissor updates
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;

	// ensures that depth writes are enabled for the depth clear
	//vk_clear_depthstencil_attachments(r_shadows->integer == 2 ? qtrue : qfalse);
	vk_clear_depthstencil_attachments(qtrue);

	if ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) {
		RB_Hyperspace();

		backEnd.projection2D = qfalse;

		// force depth range and viewport/scissor updates
		vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	} else {
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;
}

/*
==================
RB_RenderGBufferSurfList

Depth+normal(+velocity) G-buffer extraction pass (r_depthPrepass / r_velocityBuffer).
Deliberately minimal and independent from RB_RenderDrawSurfList's big multi-stage/
dlight/glow/refraction machinery: opaque surfaces only, position+normal only, one
draw per surface (no batching).

Surfaces are classified into gbufferSurfClass_t (see below), not driven straight off
surfaceType_t. Rigid surfaces - the world, brush models, MDV, and CPU-skinned Ghoul2,
i.e. anything whose vertices reach tess already in their final object space - use a
push-constant-only pipeline (pipeline_layout_gbuffer) and never touch descriptor sets.
GPU-skinned Ghoul2 (SF_MDX with a VBO mesh, r_vbo_models 1) needs the shared
Entity+Bones UBOs (for u_ModelMatrix and bone skinning - see gbuffer_skinned.vert), so
it uses the standard vk.pipeline_layout and drives vk_update_descriptor_offset()/
vk_bind_descriptor_sets() exactly like every other Ghoul2 draw in the renderer does -
vk_draw_geometry() already calls vk_bind_descriptor_sets() for us, so setting the
offsets (and forcing a rebind via vk_reset_descriptor()) on entity/bone-cache change is
all that's needed here.
(An earlier version raw-bound a dedicated, vk.pipeline_layout-incompatible layout
instead, mirroring RB_DrawShadowVolumeGPU() in tr_ghoul2.cpp - that produced visible
corruption on bolted/attached Ghoul2 models (weapons); root cause not fully pinned
down, but reusing the already-proven standard binding path sidesteps it entirely.)

Every pipeline exists in three cull variants, indexed by cullType_t and picked per
surface off tess.shader->cullType, matching create_pipeline()'s choice in the main pass
so a surface covers the same pixels in both. No mirror variant is needed: RB_DrawSurfs()
only runs this pass for the primary view.

When r_velocityBuffer is on, every rigid surface goes through the velocity pipelines
instead, which emit a real screen-space motion vector from
proj_prev * view_prev * model_now vs. this frame's mvp. The previous view/projection
pair is vk_world.prevWorldModelView/prevProjection, committed once per displayed frame
back in RB_DrawSurfs. That accounts for camera motion exactly and treats the object
itself as having stayed put - no per-entity transform history is tracked yet, so a
moving prop (and a CPU-skinned character's own animation) reports only the camera's
share of its screen motion (still strictly better than the zero it used to report).

GPU-skinned Ghoul2 gets the same treatment through a different route: it has to stay on
vk.pipeline_layout (a layout with a wider push constant range would not be
push-constant compatible with it, which disturbs the descriptor sets
vk_bind_descriptor_sets() binds with it - the trap that corrupted bolted models), so
proj_prev * view_prev reaches gbuffer_skinned_vel.vert through the Camera UBO and gets
composed with u_ModelMatrix in the shader rather than CPU-side.

GPU-skinned Ghoul2 is the one surface type with a complete motion vector: CBoneCache
keeps last frame's pose and model->world matrix, RB_TransformBones() rolls them forward
and ships them in the Bones UBO, and gbuffer_skinned_vel.vert skins the vertex twice -
so camera, entity movement and animation all land in the vector. Everything else (world,
brush models, MDV, CPU-skinned Ghoul2 under r_vbo_models 0) still composes model_now and
reports only the camera's share of its own motion; that would need per-entity transform
history, which nothing stores.
==================
*/

// How RB_RenderGBufferSurfList() has to feed a surface to the GPU. Not the same thing
// as surfaceType_t: SF_MDX lands in either GB_GHOUL2_VBO or GB_RIGID depending on
// whether r_vbo_models gave it a VBO mesh to skin on the GPU.
typedef enum {
	GB_RIGID,		// vertices already in final object space, plain vec4 stride:
					// tess.xyz/tess.normal, the world VBO, or CPU-skinned Ghoul2
	GB_MDV_VBO,		// SF_VBO_MDVMESH: rigid too, but its own packed vertex stride
	GB_GHOUL2_VBO	// SF_MDX with a VBO mesh: GPU skinning, needs the Entity+Bones UBOs
} gbufferSurfClass_t;

static void RB_RenderGBufferSurfList( const drawSurf_t *drawSurfs, int numDrawSurfs ) {
	int						i;
	int						entityNum, oldEntityNum;
	int						fogNum;
	int						dlighted;
	shader_t				*shader;
	const drawSurf_t		*drawSurf;
	VkPipeline				pipeline;
	VkPipelineLayout		pipelineLayout;
	gbufferSurfClass_t		surfClass;
	qboolean				isSky, oldIsSky;
	const shaderStage_t		*atStage;
	cullType_t				cull;
	qboolean				useVelocity;
	Vk_Depth_Range			depthRange;
	vkGBufferPushConstants_t			pushData;
	vkGBufferVelocityPushConstants_t	velocityPushData;
#ifdef USE_VBO_GHOUL2
	CBoneCache				*oldBoneCache = nullptr;
#endif

	oldEntityNum = -1;
	oldIsSky = qfalse;
	depthRange = DEPTH_RANGE_NORMAL;
	backEnd.currentEntity = &tr.worldEntity;

	for ( i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++ ) {
		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );

		// Sky is let through only to lay down a motion vector, and only when there is a
		// velocity attachment to write it to. Its pipeline writes no depth, so it stays out
		// of the depth+normal buffer and cannot occlude anything.
		isSky = (qboolean)( vk.velocityActive && shader->sort == SS_ENVIRONMENT );

		if ( shader->sort != SS_OPAQUE && !isSky )
			continue;

		// sort == SS_OPAQUE is not enough on its own. A shader can carry that sort and still
		// be blended - a glare disc, a screen overlay - and writing its depth here makes it a
		// solid occluder for GTAO and the contact shadows while the eye sees straight through
		// it. That is where the large dark polygons come from. Require what actually makes a
		// surface occlude: it writes depth, and it is not blended. Alpha-tested surfaces do
		// both, and their cut is handled by the alpha-test pipelines below.
		if ( !isSky && shader->stages && shader->stages[0] ) {
			const uint32_t bits = shader->stages[0]->stateBits;

			if ( !( bits & GLS_DEPTHMASK_TRUE ) || ( bits & GLS_BLEND_BITS ) )
				continue;
		}

		switch ( *drawSurf->surface ) {
			case SF_FACE:
			case SF_GRID:
			case SF_TRIANGLES:
			case SF_MDV:
				surfClass = GB_RIGID;
				break;
			case SF_VBO_MDVMESH:
				surfClass = GB_MDV_VBO;
				break;
			case SF_MDX:
#ifdef USE_VBO_GHOUL2
				if ( ((const CRenderableSurface *)drawSurf->surface)->vboMesh != NULL ) {
					if ( vk.gbuffer_ghoul2_pipeline[0] == VK_NULL_HANDLE )
						continue;
					surfClass = GB_GHOUL2_VBO;
					break;
				}
#endif
#ifdef _G2_GORE
				// r_vbo_models 0 - the default - leaves vboMesh NULL, and RB_SurfaceGhoul()
				// then CPU-skins into tess.xyz/tess.normal in model space: exactly the
				// layout the rigid pipelines already consume. Drawing it here is what keeps
				// every character in the G-buffer; skipping it left the player and every NPC
				// missing from the depth+normal buffer in the stock configuration.
				//
				// Gated on _G2_GORE because without it RB_SurfaceGhoul() deletes the
				// CRenderableSurface on its way through (see tr_ghoul2.cpp) - fine when the
				// main pass is the only caller, a use-after-free once this pass tessellates
				// the same surface first.
				surfClass = GB_RIGID;
				break;
#else
				continue;
#endif
			default:
				// SF_SPRITES, SF_FLARE, SF_ENTITY, SF_POLY: not covered by this pass.
				continue;
		}

		// isSky is part of the condition because sky and world share the world entity: the
		// block below is what builds prevMvp, and sky needs a different one. Without this the
		// first surface of the frame would decide the matrix for both kinds.
		if ( entityNum != oldEntityNum || isSky != oldIsSky ) {
			depthRange = DEPTH_RANGE_NORMAL;

			if ( entityNum == REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.ori = backEnd.viewParms.world;
			} else {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];

				if ( backEnd.currentEntity->e.renderfx & RF_NODEPTH ) {
					// e.g. the view-through-walls hack: deliberately not depth
					// tested/written by the main pass either, so keep it out of
					// the depth+normal buffer too.
					oldEntityNum = entityNum;
					continue;
				}

				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori );

				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// Same depth range squeeze RB_RenderDrawSurfList applies to the
					// view model, so its prepass depth matches what the main pass
					// actually writes instead of poking through the world.
					depthRange = DEPTH_RANGE_WEAPON;
				}
			}

			// Reuses vk_get_mvp_transform()'s existing (and already-correct)
			// reversed-depth/projection-flip handling, rather than duplicating it
			// here. vk_world.modelview_transform is restored below, after the loop.
			Com_Memcpy( vk_world.modelview_transform, backEnd.ori.modelViewMatrix, 64 );
			vk_get_mvp_transform( pushData.mvp );
			Com_Memcpy( pushData.modelView, backEnd.ori.modelViewMatrix, 64 );

			// Does this entity already get a stencil shadow volume? If so its occlusion is
			// already in the frame and a contact shadow from it would double the darkening,
			// so the flag rides in the normal attachment's alpha and ssao.frag skips it as an
			// occluder. Conditions mirror the two places that actually queue tr.shadowShader:
			// tr_mesh.cpp for MD3 and tr_ghoul2.cpp for Ghoul2, which additionally demands
			// RF_SHADOW_PLANE.
			pushData.surfaceFlags[0] = 0.0f;
			pushData.surfaceFlags[1] = pushData.surfaceFlags[2] = pushData.surfaceFlags[3] = 0.0f;

			if ( entityNum != REFENTITYNUM_WORLD && R_STENCIL_SHADOWS() && shader->sort == SS_OPAQUE ) {
				const int rfx = backEnd.currentEntity->e.renderfx;

				if ( !( rfx & ( RF_NOSHADOW | RF_DEPTHHACK ) )
					&& ( *drawSurf->surface != SF_MDX || ( rfx & RF_SHADOW_PLANE ) ) ) {
					// entityNum + 1, so the shadow volume shader can tell whose pixel this is.
					pushData.surfaceFlags[0] = (float)( entityNum + 1 );
				}
			}

			if ( vk.velocityActive ) {
				Com_Memcpy( velocityPushData.mvp, pushData.mvp, 64 );
				Com_Memcpy( velocityPushData.modelView, pushData.modelView, 64 );
				Com_Memcpy( velocityPushData.surfaceFlags, pushData.surfaceFlags, sizeof( vec4_t ) );

				if ( vk_world.prevViewValid ) {
					// proj_prev * view_prev * model_now, applied (in the vertex
					// shader) to this frame's object-space position: where this
					// vertex would have landed last frame had only the camera
					// moved. Mirrors get_mvp_transform()'s own reversed-depth
					// Y-flip + multiply, just against last frame's stored matrices
					// instead of the live backEnd.viewParms ones. For world
					// surfaces ori.modelMatrix is identity (see R_RotateForViewer),
					// so this reduces to the plain camera-only case.
					float prevProj[16];
					float prevModelView[16];

					Com_Memcpy( prevProj, vk_world.prevProjection, 64 );
					prevProj[5] = -prevProj[5];

					if ( isSky ) {
						// A sky is at infinity: its previous screen position is where the SAME
						// direction pointed from the previous camera, which is this frame's world
						// position translated by (prevOrigin - origin). Folding that shift in here
						// leaves pure rotation in the motion vector and cancels the parallax the
						// finite sky brushes would otherwise report on a camera move.
						float skyShift[16];

						Matrix16Identity( skyShift );
						skyShift[12] = vk_world.prevViewOrigin[0] - backEnd.viewParms.ori.origin[0];
						skyShift[13] = vk_world.prevViewOrigin[1] - backEnd.viewParms.ori.origin[1];
						skyShift[14] = vk_world.prevViewOrigin[2] - backEnd.viewParms.ori.origin[2];

						myGlMultMatrix( backEnd.ori.modelMatrix, skyShift, prevModelView );
						myGlMultMatrix( prevModelView, vk_world.prevWorldModelView, prevModelView );
					}
					else {
						myGlMultMatrix( backEnd.ori.modelMatrix, vk_world.prevWorldModelView, prevModelView );
					}

					myGlMultMatrix( prevModelView, prevProj, velocityPushData.prevMvp );
				} else {
					// First frame after init/map load: no history yet, so report
					// zero motion for this one frame rather than a bogus delta.
					Com_Memcpy( velocityPushData.prevMvp, velocityPushData.mvp, 64 );
				}
			}

#ifdef USE_VBO_GHOUL2
			if ( surfClass == GB_GHOUL2_VBO ) {
				// u_ModelMatrix for this (possibly just-changed) entity - same
				// precomputed offset RB_RenderDrawSurfList uses, see
				// vk_update_entity_constants() in tr_backend.cpp.
				vk_reset_descriptor( VK_DESC_UNIFORM );
				vk_update_descriptor( VK_DESC_UNIFORM, vk.cmd->uniform_descriptor );
				vk_update_descriptor_offset( VK_DESC_UNIFORM_ENTITY_BINDING, vk.cmd->entity_ubo_offset[entityNum] );
				// gbuffer_skinned_vel.vert reads u_PrevViewProjection out of the Camera UBO.
				// Nothing else in this pass sets that offset, so without this the shader would
				// address whatever slot the previous frame or view happened to leave behind.
				vk_update_descriptor_offset( VK_DESC_UNIFORM_CAMERA_BINDING, vk.cmd->camera_ubo_offset );
			}
#endif

			oldEntityNum = entityNum;
			oldIsSky = isSky;
		}

#ifdef USE_VBO_GHOUL2
		if ( surfClass == GB_GHOUL2_VBO ) {
			CBoneCache *boneCache = ((CRenderableSurface *)drawSurf->surface)->boneCache;

			if ( boneCache != oldBoneCache ) {
				oldBoneCache = boneCache;

				// Force a rebind (vk_draw_geometry() -> vk_bind_descriptor_sets() picks
				// this up below) even though vk.cmd->uniform_descriptor's handle itself
				// doesn't change frame to frame - only the dynamic offset does, and
				// vk_update_descriptor() only marks things dirty on a handle change, so
				// vk_reset_descriptor() first is what forces it. Same pattern
				// vk_push_uniform()/vk_push_uniform_global() use in vk_shade_geometry.cpp.
				vk_reset_descriptor( VK_DESC_UNIFORM );
				vk_update_descriptor( VK_DESC_UNIFORM, vk.cmd->uniform_descriptor );
				vk_update_descriptor_offset( VK_DESC_UNIFORM_BONES_BINDING,
					RB_GetBoneUboOffset( (CRenderableSurface *)drawSurf->surface ) );
			}
		}
#endif

#ifdef USE_VBO
		// RB_BeginSurface() clears tess.numIndexes but NOT tess.vbo_world_index/vbo_model,
		// because in the main pass RB_EndSurface() is what closes a surface out. This loop
		// never calls RB_EndSurface(), so without this reset the world-VBO branch in
		// RB_SurfaceFace()/Grid()/Triangles() sees a leftover non-zero vbo_world_index,
		// skips its transition block - the one that sets the dummy numIndexes = 1 and calls
		// VBO_ClearQueue() - and every world surface after the first is then dropped by the
		// numIndexes == 0 test below.
		VBO_UnBind();
		tess.vbo_model = nullptr;
		tess.ibo_model = nullptr;
#endif

		RB_BeginSurface( shader, fogNum );
		rb_surfaceTable[*drawSurf->surface]( drawSurf->surface );

		if ( tess.numIndexes == 0 )
			continue;

#ifdef USE_VBO
		// Deforms only ever apply to immediate geometry: a shader with numDeforms is not
		// static, so it never reaches a VBO path in the first place.
		if ( tess.shader->numDeforms && !tess.vbo_world_index && !tess.vbo_model )
#else
		if ( tess.shader->numDeforms )
#endif
		{
			// RB_StageIteratorGeneric() does this for the main pass, and this loop bypasses
			// it - so without this a waving or moving surface sat at its undeformed position
			// in the G-buffer while the main pass drew it somewhere else.
			RB_DeformTessGeometry();
		}

		// An alpha-tested surface has to cut its silhouette or it lays down a solid
		// rectangle of depth - a grate reads as a wall to anything consuming this buffer.
		// Only the rigid class is handled; see vk_create_gbuffer_pipelines().
		atStage = NULL;

		if ( surfClass == GB_RIGID && !isSky && tess.xstages ) {
			int stageIdx;

			for ( stageIdx = 0; stageIdx < tess.shader->numUnfoggedPasses; stageIdx++ ) {
				const shaderStage_t *st = tess.xstages[stageIdx];

				if ( st && st->active && ( st->stateBits & GLS_ATEST_BITS ) && st->bundle[0].image[0] ) {
					atStage = st;
#ifdef USE_VBO
					// Which stage's texcoords vk_bind_geometry() should pull out of the VBO.
					tess.vboStage = stageIdx;
#endif
					break;
				}
			}
		}

		// Pipeline choice waits until here on purpose: cullType has to come off
		// tess.shader, the remapped shader RB_BeginSurface() just resolved, so this
		// surface is culled exactly the way the main pass will cull it. Reading
		// shader->cullType before the remap would disagree on remapped shaders.
		cull = tess.shader->cullType;

		if ( surfClass == GB_GHOUL2_VBO ) {
			// This one handle covers both cases: vk_create_gbuffer_pipeline() builds it from
			// gbuffer_skinned_vel.vert + gbuffer_worldvel.frag when r_velocityBuffer is on,
			// and from gbuffer_skinned.vert otherwise. Either way it stays on
			// vk.pipeline_layout and its 64-byte mvp push constant: the previous-frame camera
			// rides in the Camera UBO and the pose/placement history in the Bones UBO, rather
			// than a wider push range. See vkUniformCamera_t and vkUniformBones_t.
			pipeline = vk.gbuffer_ghoul2_pipeline[cull];
			pipelineLayout = vk.pipeline_layout;
			useVelocity = qfalse;
		}
		else if ( vk.velocityActive ) {
			pipeline = atStage						? vk.gbuffer_at_velocity_pipeline[cull]
					 : isSky						? vk.gbuffer_sky_velocity_pipeline[cull]
					 : ( surfClass == GB_MDV_VBO )	? vk.gbuffer_mdv_velocity_pipeline[cull]
													: vk.gbuffer_rigid_velocity_pipeline[cull];
			pipelineLayout = atStage ? vk.pipeline_layout_gbuffer_at_velocity : vk.pipeline_layout_gbuffer_velocity;
			useVelocity = qtrue;
		}
		else {
			pipeline = atStage					   ? vk.gbuffer_at_pipeline[cull]
					 : ( surfClass == GB_MDV_VBO ) ? vk.gbuffer_mdv_pipeline[cull]
												   : vk.gbuffer_world_pipeline[cull];
			pipelineLayout = atStage ? vk.pipeline_layout_gbuffer_at : vk.pipeline_layout_gbuffer;
			useVelocity = qfalse;
		}

		if ( pipeline != vk.cmd->last_pipeline ) {
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
			vk.cmd->last_pipeline = pipeline;
		}

		// Flush any pending descriptor-set bind BEFORE pushing constants. Per the
		// Vulkan pipeline layout compatibility rules, a vkCmdBindDescriptorSets with
		// a layout that isn't push-constant compatible with the one the values were
		// pushed through leaves those values undefined - and vk_bind_descriptor_sets()
		// always binds with vk.pipeline_layout, whose push constant range differs from
		// pipeline_layout_gbuffer[_velocity]'s. Pushing first and letting
		// vk_draw_geometry() bind afterwards would hand the vertex shader a garbage
		// mvp; doing it here makes vk_draw_geometry()'s own call below a no-op.
		vk_bind_descriptor_sets();

		if ( useVelocity ) {
			qvkCmdPushConstants( vk.cmd->command_buffer, pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( velocityPushData ), &velocityPushData );
		}
#ifdef USE_VBO_GHOUL2
		else if ( surfClass == GB_GHOUL2_VBO ) {
			// vk.pipeline_layout's push constant range is 64 bytes (mat4 mvp) - see
			// gbuffer_skinned.vert, which gets modelView-equivalent data from the
			// Entity UBO instead (u_ModelMatrix, bound above).
			qvkCmdPushConstants( vk.cmd->command_buffer, pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( pushData.mvp ), &pushData.mvp );
		}
#endif
		else {
			qvkCmdPushConstants( vk.cmd->command_buffer, pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( pushData ), &pushData );
		}

#ifdef USE_VBO
		// vk_draw_geometry() -> VBO_RenderIBOItems() replays whatever VBO_PrepareQueues()
		// last built, and in the main pass that call sits in RB_StageIteratorGeneric() -
		// which this pass bypasses entirely. Without it we would redraw the previous
		// frame's index ranges, and its host-visible range points into a dynamic vertex
		// buffer this frame has already overwritten: undefined index data, not just a
		// wrong picture. One surface is queued at a time here (no batching), so this
		// resolves to a single item.
		if ( tess.vbo_world_index )
			VBO_PrepareQueues();
#endif

		// Depth bias is dynamic state on these pipelines, so it has to be set for every
		// draw, not only the offset ones - an unset dynamic state carries over from whatever
		// was bound last. Zero for ordinary surfaces is a no-op; decals get the same nudge
		// the main pass gives them, so they land at the same depth in both.
		qvkCmdSetDepthBias( vk.cmd->command_buffer,
			tess.shader->polygonOffset ? r_offsetUnits->value : 0.0f, 0.0f,
			tess.shader->polygonOffset ? r_offsetFactor->value : 0.0f );

		if ( atStage ) {
			// The immediate path needs texcoords generated - RB_StageIteratorGeneric() does
			// this for the main pass and this loop bypasses it. The VBO path reads them
			// straight out of the buffer at tess.vboStage's offset instead.
#ifdef USE_VBO
			if ( !tess.vbo_world_index )
#endif
				ComputeTexCoords( 0, &atStage->bundle[0] );

			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 0, 1, &atStage->bundle[0].image[0]->descriptor_set, 0, NULL );
		}

		vk_bind_geometry( atStage ? ( TESS_XYZ | TESS_NNN | TESS_ST0 ) : ( TESS_XYZ | TESS_NNN ) );
		vk_bind_index();
		vk_draw_geometry( depthRange, qtrue );

		if ( atStage ) {
			// That bind used an alpha-test layout, which is not compatible with
			// vk.pipeline_layout - so a Ghoul2 surface later in this list, or the main pass
			// after it, must not trust the tracker's cached bindings.
			uint32_t d;

			for ( d = 0; d < VK_DESC_COUNT; d++ )
				vk_reset_descriptor( d );

			vk.cmd->descriptor_set.end = 0;
			vk.cmd->descriptor_set.start = ~0U;
		}
	}

	// tess is shared with the main pass that runs right after this one, and this loop
	// drives rb_surfaceTable[] / issues its own draws without ever calling
	// RB_EndSurface() to close the last surface out. RB_RenderDrawSurfList()'s first
	// iteration calls RB_EndSurface() unconditionally, so anything still sitting in
	// tess here would be re-emitted into the MAIN render pass with this pass's leftover
	// transform and pipeline state - visible garbage on screen. With r_vbo on that is
	// not even real geometry: RB_SurfaceFace()/Grid()/Triangles() park a dummy
	// numIndexes == 1 in tess and hand the actual drawing to the queued IBO items
	// (see tr_surface.cpp), which RB_EndSurface() would then replay. Hand the main pass
	// a clean tess instead.
#ifdef USE_VBO
	VBO_UnBind();
	tess.vbo_model = nullptr;
	tess.ibo_model = nullptr;
#endif
	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.multiDrawPrimitives = 0;

	// Same story for the orientation: put back what RB_BeginDrawingView() left in
	// place, so the main pass starts from the view's own transform rather than from
	// whichever entity happened to be last in this loop.
	backEnd.currentEntity = &tr.worldEntity;
	backEnd.ori = backEnd.viewParms.world;
	Com_Memcpy( vk_world.modelview_transform, backEnd.ori.modelViewMatrix, 64 );

	// No descriptor-tracker reset needed here: Ghoul2 surfaces above go through the
	// standard vk_update_descriptor()/vk_bind_descriptor_sets() path with
	// vk.pipeline_layout, same as RB_RenderDrawSurfList's own draws that follow -
	// nothing to reconcile between the two.
}

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				i, fogNum, oldFogNum, entityNum, oldEntityNum, dlighted, oldDlighted, reType, oldReType;
	Vk_Depth_Range	depthRange;
	drawSurf_t		*drawSurf;
	unsigned int	oldSort;
	float			oldShaderSort, originalTime;
	CBoneCache		*oldBoneCache = nullptr;

	// Only for the primary view's own opaque pass: portal/mirror sub-views and the glow
	// pass have no G-buffer of their own, and RDF_NOWORLDMODEL scenes (HUD model icons)
	// were never extracted either - see the guard on RB_RenderGBufferSurfList's caller.
	// The refraction pass re-runs this whole function over the same list, so it has to be
	// excluded too or the AO gets multiplied in a second time.
	qboolean		didGtaoPass = (qboolean)!( vk.ssaoActive
							&& !backEnd.isGlowPass
							&& !backEnd.refractionFill
							&& backEnd.viewParms.portalView == PV_NONE
							&& !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) );

#ifdef USE_VANILLA_SHADOWFINISH
	qboolean		didShadowPass = qfalse;

	if ( backEnd.isGlowPass )
	{ //only shadow on initial passes
		didShadowPass = qtrue;
	}
#endif

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	oldEntityNum			= -1;
	backEnd.currentEntity	= &tr.worldEntity;
	oldShader				= NULL;
	oldSort					= MAX_UINT;
	oldShaderSort			= -1;
	depthRange				= DEPTH_RANGE_NORMAL;
	oldFogNum				= -1;
	oldDlighted				= qfalse;
	oldReType				= -1;
	qboolean				push_constant;

	backEnd.pc.c_surfaces	+= numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++)
	{
		R_DecomposeSort(drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted);

		if (vk.renderPassIndex == RENDER_PASS_SCREENMAP && entityNum != REFENTITYNUM_WORLD && backEnd.refdef.entities[entityNum].e.renderfx & RF_DEPTHHACK) {
			continue;
		}

		// check if we have amy dynamic glow surfaces before dglow pass
		if( !backEnd.hasGlowSurfaces && vk.dglowActive && !backEnd.isGlowPass && shader->hasGlow )
			backEnd.hasGlowSurfaces = qtrue;

		// if we're rendering glowing objects, but this shader has no stages with glow, skip it!
		if ( backEnd.isGlowPass && !shader->hasGlow )
		{
			shader = oldShader;
			entityNum = oldEntityNum;
			fogNum = oldFogNum;
			dlighted = oldDlighted;
			continue;
		}

		if ( vk.vboGhoul2Active && *drawSurf->surface == SF_MDX )
		{
			if ( ((CRenderableSurface*)drawSurf->surface)->boneCache != oldBoneCache )
			{
				RB_EndSurface();
				RB_BeginSurface( shader, fogNum );
				oldBoneCache = ((CRenderableSurface*)drawSurf->surface)->boneCache;
				vk.cmd->bones_ubo_offset = RB_GetBoneUboOffset((CRenderableSurface*)drawSurf->surface);
			}
		}

		if (drawSurf->sort == oldSort && backEnd.refractionFill == shader->useDistortion ) {
			// fast path, same as previous sort
			rb_surfaceTable[*drawSurf->surface](drawSurf->surface);
			continue;
		}

		//oldSort = drawSurf->sort;

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites

		push_constant = qfalse;

		reType = (entityNum == REFENTITYNUM_WORLD) ? -1 : backEnd.refdef.entities[entityNum].e.reType;

		//if (((oldSort ^ drawSurf->sort) & ~QSORT_REFENTITYNUM_MASK) || !shader->entityMergable) {
		if ( shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted
			|| ( entityNum != oldEntityNum && ( !tess.entityMergable || reType != oldReType ) ) )
		{
			//if (oldShader != NULL) {
				RB_EndSurface();
			//}
#ifdef USE_PMLIGHT
#define INSERT_POINT SS_FOG
			if (backEnd.refdef.numLitSurfs && oldShaderSort < INSERT_POINT && shader->sort >= INSERT_POINT) {
				RB_LightingPass();

				oldEntityNum = -1; // force matrix setup
			}
			oldShaderSort = shader->sort;
#endif

#ifdef USE_VANILLA_SHADOWFINISH
			if (!didShadowPass && shader && shader->sort > SS_BANNER)
			{
				RB_ShadowFinish();
				didShadowPass = qtrue;
			}
#endif
			// The opaque geometry is complete and RB_EndSurface() above has flushed it, so
			// this is where the AO gets multiplied in - before anything translucent, which
			// must not be darkened by occlusion computed from opaque depth alone. Sky is
			// already safe: it sorts ahead of SS_OPAQUE and the AO buffer holds 1.0 wherever
			// the extraction pass wrote nothing.
			if ( !didGtaoPass && shader && shader->sort > SS_OPAQUE )
			{
				vk_apply_ssao();
				didGtaoPass = qtrue;
			}

			RB_BeginSurface(shader, fogNum);
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
			oldReType = reType;

			push_constant = qtrue;
		}

		oldSort = drawSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if (entityNum != oldEntityNum)
		{
			depthRange = DEPTH_RANGE_NORMAL;

			if (entityNum != REFENTITYNUM_WORLD)
			{
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;

				// set up the transformation matrix
				R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori );

				if ( backEnd.currentEntity->e.renderfx & RF_NODEPTH ) {
					// No depth at all, very rare but some things for seeing through walls
					depthRange = DEPTH_RANGE_ZERO;
				}

				if (backEnd.currentEntity->e.renderfx & RF_DEPTHHACK) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = DEPTH_RANGE_WEAPON;
				}
			}
			else
			{
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.ori  = backEnd.viewParms.world;
			}

			// we have to reset the shaderTime as well otherwise image animations on
			// the world (like water) continue with the wrong frame
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			vk_set_depthrange( depthRange );

			if ( push_constant ) {
				Com_Memcpy(vk_world.modelview_transform, backEnd.ori.modelViewMatrix, 64);
				vk_update_mvp(NULL);
			}

			oldEntityNum = entityNum;
		}

		qboolean isDistortionShader = (qboolean)
			((shader->useDistortion == qtrue) || (backEnd.currentEntity && backEnd.currentEntity->e.renderfx & RF_DISTORTION));

		if ( backEnd.refractionFill != isDistortionShader ) {
			if ( vk.refractionActive && vk.renderPassIndex != RENDER_PASS_REFRACTION && !backEnd.hasRefractionSurfaces )
				backEnd.hasRefractionSurfaces = qtrue;

			// skip refracted surfaces in main pass, 
			// and non-refracted surfaces in refraction pass 
			continue;	
		}

		// add the triangles for this surface
		rb_surfaceTable[*drawSurf->surface](drawSurf->surface);
	}

	// draw the contents of the last shader batch
	if (oldShader != NULL) {
		RB_EndSurface();
	}

	// The loop only applies the AO when the sort order crosses out of SS_OPAQUE. A view
	// holding no translucent surface at all never crosses it, so apply it here instead -
	// everything drawn was opaque, which is exactly what the AO is allowed to darken.
	if ( !didGtaoPass ) {
		vk_apply_ssao();
	}

	backEnd.refdef.floatTime = originalTime;

	// go back to the world modelview matrix
	Com_Memcpy(vk_world.modelview_transform, backEnd.viewParms.world.modelViewMatrix, 64);
	//vk_update_mvp();
	vk_set_depthrange(DEPTH_RANGE_NORMAL);

#ifdef USE_VANILLA_SHADOWFINISH
	if (!didShadowPass)
	{
		RB_ShadowFinish();
		didShadowPass = qtrue;
	}
#endif
}

#ifdef USE_PMLIGHT
/*
=================
RB_BeginDrawingLitView
=================
*/
static void RB_BeginDrawingLitSurfs( void )
{
	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// force depth range and viewport/scissor updates
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;

	glState.faceCulling = -1;		// force face culling to set next time
}

/*
==================
RB_RenderLitSurfList
==================
*/
static void RB_RenderLitSurfList( dlight_t *dl ) {
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
	Vk_Depth_Range	depthRange;
	const litSurf_t *litSurf;
	unsigned int	oldSort;
	double			originalTime; // -EC-

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// draw everything
	oldEntityNum			= -1;
	backEnd.currentEntity	= &tr.worldEntity;
	oldShader				= NULL;
	oldSort					= MAX_UINT;
	depthRange				= DEPTH_RANGE_NORMAL;

	tess.dlightUpdateParams = qtrue;

	for (litSurf = dl->head; litSurf; litSurf = litSurf->next) {
		//if ( litSurf->sort == sort ) {
		if (litSurf->sort == oldSort) {
			// fast path, same as previous sort
			rb_surfaceTable[*litSurf->surface](litSurf->surface);
			continue;
		}

		R_DecomposeLitSort(litSurf->sort, &entityNum, &shader, &fogNum);

		if (vk.renderPassIndex == RENDER_PASS_SCREENMAP && entityNum != REFENTITYNUM_WORLD && backEnd.refdef.entities[entityNum].e.renderfx & RF_DEPTHHACK) {
			continue;
		}

		// anything BEFORE opaque is sky/portal, anything AFTER it should never have been added
		//assert( shader->sort == SS_OPAQUE );
		// !!! but MIRRORS can trip that assert, so just do this for now
		//if ( shader->sort < SS_OPAQUE )
		//	continue;

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (((oldSort ^ litSurf->sort) & ~QSORT_REFENTITYNUM_MASK) || !tess.entityMergable) {
			if (oldShader != NULL) {
				RB_EndSurface();
			}
			RB_BeginSurface(shader, fogNum);
			oldShader = shader;
		}

		oldSort = litSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if (entityNum != oldEntityNum) {
			depthRange = DEPTH_RANGE_NORMAL;

			if (entityNum != REFENTITYNUM_WORLD) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];

				/*if (backEnd.currentEntity->intShaderTime)
					backEnd.refdef.floatTime = originalTime - (double)(backEnd.currentEntity->e.shaderTime.i) * 0.001;
				else*/
				backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime;

				// set up the transformation matrix
				R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori );

				if ( backEnd.currentEntity->e.renderfx & RF_NODEPTH ) {
					// No depth at all, very rare but some things for seeing through walls
					depthRange = DEPTH_RANGE_ZERO;
				}

				if (backEnd.currentEntity->e.renderfx & RF_DEPTHHACK) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = DEPTH_RANGE_WEAPON;
				}
			}
			else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.ori = backEnd.viewParms.world;
			}

			// we have to reset the shaderTime as well otherwise image animations on
			// the world (like water) continue with the wrong frame
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			// set up the dynamic lighting
			R_TransformDlights(1, dl, &backEnd.ori );
			tess.dlightUpdateParams = qtrue;

			vk_set_depthrange( depthRange );

			Com_Memcpy(vk_world.modelview_transform, backEnd.ori.modelViewMatrix, 64);
			vk_update_mvp(NULL);

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[*litSurf->surface](litSurf->surface);
	}

	// draw the contents of the last shader batch
	if (oldShader != NULL) {
		RB_EndSurface();
	}

	backEnd.refdef.floatTime = originalTime;

	// go back to the world modelview matrix
	Com_Memcpy(vk_world.modelview_transform, backEnd.viewParms.world.modelViewMatrix, 64);
	//vk_update_mvp();

	vk_set_depthrange(DEPTH_RANGE_NORMAL);
}
#endif // USE_PMLIGHT

/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw ( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty )
{
	int			i, j;
	int			start, end;

	if (!tr.registered) {
		return;
	}

	start = 0;
	if (r_speeds->integer) {
		start = ri.Milliseconds() * ri.Cvar_VariableValue("timescale");
	}

	// make sure rows and cols are powers of 2
	for (i = 0; (1 << i) < cols; i++)
	{
		;
	}
	for (j = 0; (1 << j) < rows; j++)
	{
		;
	}

	if ((1 << i) != cols || (1 << j) != rows) {
		Com_Error(ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	RE_UploadCinematic( cols, rows, (byte*)data, client, dirty );

	if (r_speeds->integer) {
		end = ri.Milliseconds() * ri.Cvar_VariableValue("timescale");
		ri.Printf(PRINT_ALL, "RE_UploadCinematic( %i, %i ): %i msec\n", cols, rows, end - start);
	}

	tr.cinematicShader->stages[0]->bundle[0].image[0] = tr.scratchImage[client];
	RE_StretchPic(x, y, w, h, 0.5f / cols, 0.5f / rows, 1.0f - 0.5f / cols, 1.0f - 0.5 / rows, tr.cinematicShader->index);
}

/*
=============
RB_SetColor

=============
*/
const void	*RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void *RB_StretchPic ( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;

	cmd = (const stretchPicCommand_t *)data;

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		RB_EndSurface();
		backEnd.currentEntity = &backEnd.entity2D;
		vk_set_2d(); // set correct shader time before RB_BeginSurface() on 3D->2D transition
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	vk_set_2d();

	if ( vk.bloomActive ) {
		vk_bloom();
	}

	RB_AddQuadStamp2( cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1,
		cmd->s2, cmd->t2, backEnd.color2D );

	return (const void *)(cmd + 1);
}

/*
=============
RB_DrawRotatePic
=============
*/
const void *RB_RotatePic ( const void *data )
{
	const rotatePicCommand_t	*cmd;
	image_t *image;
	shader_t *shader;

	cmd = (const rotatePicCommand_t *)data;

	shader = cmd->shader;
	image = shader->stages[0]->bundle[0].image[0];

	if ( image ) {
		shader = cmd->shader;
		if ( shader != tess.shader ) {
			RB_EndSurface();
			backEnd.currentEntity = &backEnd.entity2D;
			vk_set_2d(); // set correct shader time before RB_BeginSurface() on 3D->2D transition
			RB_BeginSurface( shader, 0 );
		}

		vk_set_2d();

		RB_CHECKOVERFLOW( 4, 6 );
		int numVerts = tess.numVertexes;
		int numIndexes = tess.numIndexes;

		float angle = DEG2RAD( cmd-> a );
		float s = sinf( angle );
		float c = cosf( angle );

		matrix3_t m = {
			{ c, s, 0.0f },
			{ -s, c, 0.0f },
			{ cmd->x + cmd->w, cmd->y, 1.0f }
		};

		tess.numVertexes += 4;
		tess.numIndexes += 6;

		tess.indexes[ numIndexes + 0 ] = numVerts + 3;
		tess.indexes[ numIndexes + 1 ] = numVerts + 0;
		tess.indexes[ numIndexes + 2 ] = numVerts + 2;
		tess.indexes[ numIndexes + 3 ] = numVerts + 2;
		tess.indexes[ numIndexes + 4 ] = numVerts + 0;
		tess.indexes[ numIndexes + 5 ] = numVerts + 1;

		byteAlias_t *baDest = NULL, *baSource = (byteAlias_t *)&backEnd.color2D;
		baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 0]; baDest->ui = baSource->ui;
		baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 1]; baDest->ui = baSource->ui;
		baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 2]; baDest->ui = baSource->ui;
		baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 3]; baDest->ui = baSource->ui;

		tess.xyz[ numVerts + 0 ][0] = m[0][0] * (-cmd->w) + m[2][0];
		tess.xyz[ numVerts + 0 ][1] = m[0][1] * (-cmd->w) + m[2][1];
		tess.xyz[ numVerts + 0 ][2] = 0;

		tess.xyz[ numVerts + 1 ][0] = m[2][0];
		tess.xyz[ numVerts + 1 ][1] = m[2][1];
		tess.xyz[ numVerts + 1 ][2] = 0;

		tess.xyz[ numVerts + 2 ][0] = m[1][0] * (cmd->h) + m[2][0];
		tess.xyz[ numVerts + 2 ][1] = m[1][1] * (cmd->h) + m[2][1];
		tess.xyz[ numVerts + 2 ][2] = 0;

		tess.xyz[ numVerts + 3 ][0] = m[0][0] * (-cmd->w) + m[1][0] * (cmd->h) + m[2][0];
		tess.xyz[ numVerts + 3 ][1] = m[0][1] * (-cmd->w) + m[1][1] * (cmd->h) + m[2][1];
		tess.xyz[ numVerts + 3 ][2] = 0;

		tess.texCoords[0][ numVerts + 0 ][0] = cmd->s1;
		tess.texCoords[0][ numVerts + 0 ][1] = cmd->t1;
		tess.texCoords[0][ numVerts + 1 ][0] = cmd->s2;
		tess.texCoords[0][ numVerts + 1 ][1] = cmd->t1;
		tess.texCoords[0][ numVerts + 2 ][0] = cmd->s2;
		tess.texCoords[0][ numVerts + 2 ][1] = cmd->t2;
		tess.texCoords[0][ numVerts + 3 ][0] = cmd->s1;
		tess.texCoords[0][ numVerts + 3 ][1] = cmd->t2;

		return (const void *)(cmd + 1);

	}

	return (const void *)(cmd + 1);
}

/*
=============
RB_DrawRotatePic2
=============
*/
const void *RB_RotatePic2 ( const void *data )
{
	const rotatePicCommand_t	*cmd;
	image_t *image;
	shader_t *shader;

	cmd = (const rotatePicCommand_t *)data;

	shader = cmd->shader;

	if ( shader->numUnfoggedPasses )
	{
		image = shader->stages[0]->bundle[0].image[0];

		if ( image )
		{
			shader = cmd->shader;
			if ( shader != tess.shader ) {
				RB_EndSurface();
				backEnd.currentEntity = &backEnd.entity2D;
				vk_set_2d(); // set correct shader time before RB_BeginSurface() on 3D->2D transition
				RB_BeginSurface( shader, 0 );
			}

			vk_set_2d();

			RB_CHECKOVERFLOW( 4, 6 );
			int numVerts = tess.numVertexes;
			int numIndexes = tess.numIndexes;

			float angle = DEG2RAD( cmd-> a );
			float s = sinf( angle );
			float c = cosf( angle );

			matrix3_t m = {
				{ c, s, 0.0f },
				{ -s, c, 0.0f },
				{ cmd->x, cmd->y, 1.0f }
			};

			tess.numVertexes += 4;
			tess.numIndexes += 6;

			tess.indexes[ numIndexes + 0 ] = numVerts + 3;
			tess.indexes[ numIndexes + 1 ] = numVerts + 0;
			tess.indexes[ numIndexes + 2 ] = numVerts + 2;
			tess.indexes[ numIndexes + 3 ] = numVerts + 2;
			tess.indexes[ numIndexes + 4 ] = numVerts + 0;
			tess.indexes[ numIndexes + 5 ] = numVerts + 1;

			byteAlias_t *baDest = NULL, *baSource = (byteAlias_t *)&backEnd.color2D;
			baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 0]; baDest->ui = baSource->ui;
			baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 1]; baDest->ui = baSource->ui;
			baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 2]; baDest->ui = baSource->ui;
			baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 3]; baDest->ui = baSource->ui;

			tess.xyz[ numVerts + 0 ][0] = m[0][0] * (-cmd->w * 0.5f) + m[1][0] * (-cmd->h * 0.5f) + m[2][0];
			tess.xyz[ numVerts + 0 ][1] = m[0][1] * (-cmd->w * 0.5f) + m[1][1] * (-cmd->h * 0.5f) + m[2][1];
			tess.xyz[ numVerts + 0 ][2] = 0;

			tess.xyz[ numVerts + 1 ][0] = m[0][0] * (cmd->w * 0.5f) + m[1][0] * (-cmd->h * 0.5f) + m[2][0];
			tess.xyz[ numVerts + 1 ][1] = m[0][1] * (cmd->w * 0.5f) + m[1][1] * (-cmd->h * 0.5f) + m[2][1];
			tess.xyz[ numVerts + 1 ][2] = 0;

			tess.xyz[ numVerts + 2 ][0] = m[0][0] * (cmd->w * 0.5f) + m[1][0] * (cmd->h * 0.5f) + m[2][0];
			tess.xyz[ numVerts + 2 ][1] = m[0][1] * (cmd->w * 0.5f) + m[1][1] * (cmd->h * 0.5f) + m[2][1];
			tess.xyz[ numVerts + 2 ][2] = 0;

			tess.xyz[ numVerts + 3 ][0] = m[0][0] * (-cmd->w * 0.5f) + m[1][0] * (cmd->h * 0.5f) + m[2][0];
			tess.xyz[ numVerts + 3 ][1] = m[0][1] * (-cmd->w * 0.5f) + m[1][1] * (cmd->h * 0.5f) + m[2][1];
			tess.xyz[ numVerts + 3 ][2] = 0;

			tess.texCoords[0][ numVerts + 0 ][0] = cmd->s1;
			tess.texCoords[0][ numVerts + 0 ][1] = cmd->t1;
			tess.texCoords[0][ numVerts + 1 ][0] = cmd->s2;
			tess.texCoords[0][ numVerts + 1 ][1] = cmd->t1;
			tess.texCoords[0][ numVerts + 2 ][0] = cmd->s2;
			tess.texCoords[0][ numVerts + 2 ][1] = cmd->t2;
			tess.texCoords[0][ numVerts + 3 ][0] = cmd->s1;
			tess.texCoords[0][ numVerts + 3 ][1] = cmd->t2;

			return (const void *)(cmd + 1);
		}
	}

	return (const void *)(cmd + 1);
}

#ifdef USE_PMLIGHT
static void RB_LightingPass( void )
{
	dlight_t* dl;
	int	i;

#ifdef USE_VBO
	//VBO_Flush();
	//tess.allowVBO = qfalse; // for now
#endif

	tess.dlightPass = qtrue;

	for (i = 0; i < backEnd.viewParms.num_dlights; i++)
	{
		dl = &backEnd.viewParms.dlights[i];
		if (dl->head)
		{
			tess.light = dl;
			RB_RenderLitSurfList(dl);
		}
	}

	tess.dlightPass = qfalse;

	backEnd.viewParms.num_dlights = 0;
}
#endif

static void vk_update_camera_constants( const trRefdef_t *refdef, const viewParms_t *viewParms ) 
{
	// set
	vkUniformCamera_t uniform = {};

	Com_Memcpy( uniform.viewOrigin, refdef->vieworg, sizeof( vec3_t) );
	uniform.viewOrigin[3] = refdef->floatTime;

	if ( vk.gbufferActive ) {
		// Lets the skinned gbuffer shaders bring their normal into view space, so the
		// normal attachment holds one space throughout.
		Matrix16Copy( viewParms->world.modelViewMatrix, uniform.viewMatrix );
	}

	/*
	const float* p = viewParms->projectionMatrix;
	float proj[16];
	Com_Memcpy(proj, p, 64);

	proj[5] = -p[5];
	//myGlMultMatrix(vk_world.modelview_transform, proj, uniform.mvp);
	myGlMultMatrix(viewParms->world.modelViewMatrix, proj, uniform.mvp);
	*/

	if ( vk.velocityActive ) {
		// Read here, before RB_DrawSurfs() overwrites vk_world.prev* with THIS frame's
		// transform after the gbuffer pass - so this is genuinely last frame's camera.
		// gbuffer_skinned_vel.vert composes it with u_ModelMatrix to place a skinned
		// vertex where it would have been last frame had only the camera moved; the rigid
		// pipelines get the equivalent composed CPU-side as prevMvp.
		float proj[16];

		if ( vk_world.prevViewValid ) {
			Com_Memcpy( proj, vk_world.prevProjection, 64 );
			proj[5] = -proj[5];
			myGlMultMatrix( vk_world.prevWorldModelView, proj, uniform.prevViewProjection );
		} else {
			// First frame after init/map load: no history, so hand the shader this frame's
			// own transform and let the motion vector come out as zero.
			Com_Memcpy( proj, viewParms->projectionMatrix, 64 );
			proj[5] = -proj[5];
			myGlMultMatrix( viewParms->world.modelViewMatrix, proj, uniform.prevViewProjection );
		}
	}

	vk.cmd->camera_ubo_offset = vk_append_uniform( &uniform, sizeof(uniform), vk.uniform_camera_item_size );
}

static void vk_update_entity_light_constants( vkUniformEntity_t &uniform, const trRefEntity_t *refEntity ) 
{
	static const float normalizeFactor = 1.0f / 255.0f;

	VectorScale(refEntity->ambientLight, normalizeFactor, uniform.ambientLight);
	VectorScale(refEntity->directedLight, normalizeFactor, uniform.directedLight);
	VectorCopy(refEntity->lightDir, uniform.lightOrigin);

	uniform.lightOrigin[3] = 0.0f;

	// model space, so the vertex shader can dot it against the model space normal
	VectorCopy(refEntity->modelLightDir, uniform.modelLightDir);
	uniform.modelLightDir[3] = 0.0f;
}

static void vk_update_entity_matrix_constants( vkUniformEntity_t &uniform, const trRefEntity_t *refEntity, int entityNum ) 
{
	orientationr_t ori;

	// backend ref cant be right
	/*if ( refEntity == &tr.worldEntity ) {
		ori = backEnd.viewParms.world;
		Matrix16Identity( uniform.modelMatrix );
	}else{
		R_RotateForEntity( refEntity, &backEnd.viewParms, &ori );
		Matrix16Copy( ori.modelMatrix, uniform.modelMatrix );
	}*/

	R_RotateForEntity(refEntity, &backEnd.viewParms, &ori);
	Matrix16Copy(ori.modelMatrix, uniform.modelMatrix);

	// Does this entity already get a stencil shadow volume? The skinned gbuffer shaders
	// forward this into the normal attachment's alpha so ssao.frag can decline to count it
	// as a contact-shadow occluder and darken the same ground twice. Conditions mirror the
	// Ghoul2 caster test in tr_ghoul2.cpp, which is the only thing that reads this.
	// Stores entityNum + 1, not a yes/no: the shadow volume fragment shader needs to know
	// WHICH entity a pixel belongs to so it can decline to shadow that entity with its own
	// volume while still letting other entities' volumes through. Zero means "not a caster".
	uniform.surfaceFlags[0] = ( R_STENCIL_SHADOWS()
							&& ( refEntity->e.renderfx & RF_SHADOW_PLANE )
							&& !( refEntity->e.renderfx & ( RF_NOSHADOW | RF_DEPTHHACK ) ) ) ? (float)( entityNum + 1 ) : 0.0f;
	uniform.surfaceFlags[1] = uniform.surfaceFlags[2] = uniform.surfaceFlags[3] = 0.0f;
	VectorCopy(ori.viewOrigin, uniform.localViewOrigin);

	Com_Memcpy( &uniform.localViewOrigin, ori.viewOrigin, sizeof( vec3_t) );
	uniform.localViewOrigin[3] = 0.0f;
}

static void vk_update_entity_constants( const trRefdef_t *refdef ) {
	uint32_t i;
	Com_Memset( vk.cmd->entity_ubo_offset, 0, sizeof(vk.cmd->entity_ubo_offset) );

	for ( i = 0; i < refdef->num_entities; i++ ) {
		trRefEntity_t *ent = &refdef->entities[i];

		R_SetupEntityLighting( refdef, ent );

		vkUniformEntity_t uniform = {};
		vk_update_entity_light_constants( uniform, ent );
		vk_update_entity_matrix_constants( uniform, ent, (int)i );

		vk.cmd->entity_ubo_offset[i] = vk_append_uniform( &uniform, sizeof(uniform), vk.uniform_entity_item_size );
	}

	const trRefEntity_t *ent = &tr.worldEntity;
	vkUniformEntity_t uniform = {};
	vk_update_entity_light_constants( uniform, ent );
	vk_update_entity_matrix_constants( uniform, ent, REFENTITYNUM_WORLD );

	vk.cmd->entity_ubo_offset[REFENTITYNUM_WORLD] = vk_append_uniform( &uniform, sizeof(uniform), vk.uniform_entity_item_size );
}

static void vk_update_ghoul2_constants( const trRefdef_t *refdef ) {
	uint32_t i;

	if ( !vk.vboGhoul2Active )
		return;

	for ( i = 0; i < refdef->num_entities; i++ )
	{
		const trRefEntity_t *ent = &refdef->entities[i];
		if (ent->e.reType != RT_MODEL)
			continue;

		model_t *model = R_GetModelByHandle(ent->e.hModel);
		if (!model)
			continue;

		switch (model->type)
		{
		case MOD_MDXM:
		case MOD_BAD:
		{
			// Transform Bones and upload them
			RB_TransformBones( ent, refdef );
		}
		break;

		default:
			break;
		}
	}

}

static void vk_update_fog_constants(const trRefdef_t* refdef)
{
	uint32_t i;
	size_t size;
	vkUniformFog_t uniform = {};

	uniform.num_fogs = tr.world ? ( tr.world->numfogs - 1 ) : 0;

	size = sizeof(vec4_t);

	for ( i = 0; i < MIN(uniform.num_fogs, 16); ++i )
	{
		const fog_t *fog = tr.world->fogs + i + 1;
		vkUniformFogEntry_t *fogData = uniform.fogs + i;

		VectorCopy4( fog->surface, fogData->plane );
		VectorCopy4( fog->color, fogData->color );
		fogData->depthToOpaque = sqrtf(-logf(1.0f / 255.0f)) / fog->parms.depthForOpaque;
		fogData->hasPlane = fog->hasSurface;
	}

	size += (i * sizeof(vkUniformFogEntry_t));

	vk.cmd->fogs_ubo_offset = vk_append_uniform( &uniform, size, vk.uniform_fogs_item_size );
}

static void RB_UpdateUniformConstants( const trRefdef_t *refdef, const viewParms_t *viewParms ) 
{
	vk_update_camera_constants( refdef, viewParms );

	if ( vk.vboGhoul2Active ) 
	{
		vk_update_entity_constants( refdef );
		vk_update_ghoul2_constants( refdef );
	}

	vk_update_fog_constants( refdef );
}

/*
=============
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	RB_EndSurface(); // finish any 2D drawing if needed

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	backEnd.hasGlowSurfaces = qfalse;
	backEnd.isGlowPass = qfalse;

	backEnd.hasRefractionSurfaces = qfalse;

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_UpdateUniformConstants( &backEnd.refdef, &backEnd.viewParms );

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView();

	// depth+normal(+velocity) G-buffer extraction pass (r_depthPrepass / r_velocityBuffer).
	// Once per displayed frame, ahead of the main pass, primary view only - portal/
	// mirror sub-views and HUD/menu 3D icon sub-scenes (RDF_NOWORLDMODEL) are excluded
	// since they don't own the frame's depth+normal buffer. vk_begin_frame() already
	// opened the main pass before this runs, and any portal/mirror sub-view queued
	// ahead of the primary one has already drawn into it (see RB_BeginDrawingView's
	// comment), so resuming main afterward must NOT clear it.
	if ( vk.gbufferActive && backEnd.viewParms.portalView == PV_NONE && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk_end_render_pass();
		vk_begin_gbuffer_extract_render_pass();
		RB_RenderGBufferSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		vk_end_render_pass();

		// The gbuffer's first consumer. Has to sit here, while the depth and normal
		// attachments rest in their readable layouts and before the main pass reopens.
		if ( vk.ssaoActive )
			vk_render_ssao( &backEnd.viewParms );

		vk_begin_main_render_pass( qfalse );

		if ( vk.velocityActive ) {
			// Commit this frame's camera transform as "previous" for next frame's
			// motion vectors, now that RB_RenderGBufferSurfList above is done
			// reading the OLD vk_world.prevWorldModelView/prevProjection. Exactly
			// once per real displayed frame, matching this whole block's guard.
			Com_Memcpy( vk_world.prevWorldModelView, backEnd.viewParms.world.modelViewMatrix, 64 );
			Com_Memcpy( vk_world.prevProjection, backEnd.viewParms.projectionMatrix, 64 );
			VectorCopy( backEnd.viewParms.ori.origin, vk_world.prevViewOrigin );
			vk_world.prevViewValid = qtrue;
		}
	}

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

#ifdef USE_VBO
	VBO_UnBind();
#endif

	if ( r_drawSun->integer ) {
		RB_DrawSun( 0.1f, tr.sunShader );
	}

#ifndef USE_VANILLA_SHADOWFINISH
	RB_ShadowFinish();
#endif

	RB_RenderFlares();

#ifdef USE_PMLIGHT
	if ( backEnd.refdef.numLitSurfs ) {
		RB_BeginDrawingLitSurfs();
		RB_LightingPass();
	}
#endif

	// draw main system development information (surface outlines, etc)
	R_DebugGraphics();

	if ( cmd->refdef.switchRenderPass ) {
		vk_end_render_pass();
		vk_begin_main_render_pass();
		backEnd.screenMapDone = qtrue;
	}

	// refraction / distortion pass
	if ( backEnd.hasRefractionSurfaces ) {
		vk_end_render_pass();
	
		// extract/copy offscreen color attachment to make it usable as input
		vk_refraction_extract();

		backEnd.refractionFill = qtrue;	
		vk_begin_post_refraction_extract_render_pass();

		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		backEnd.refractionFill = qfalse;
	}

	// checked in previous RB_RenderDrawSurfList() if there is at least one glowing surface
	if ( vk.dglowActive && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && backEnd.hasGlowSurfaces )
	{
		vk_end_render_pass();

		backEnd.isGlowPass = qtrue;
		vk_begin_dglow_extract_render_pass();

		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

		vk_begin_dglow_blur();
		backEnd.isGlowPass = qfalse;
	}

	//TODO Maybe check for rdf_noworld stuff but q3mme has full 3d ui
	backEnd.doneSurfaces = qtrue; // for bloom

	return (const void*)(cmd + 1);
}

/*
=============
RB_DrawBuffer

=============
*/
const void	*RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	vk_begin_frame();

	vk_set_depthrange(DEPTH_RANGE_NORMAL);

	// force depth range and viewport/scissor updates
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;

	if ( r_clear->integer && vk.clearAttachment ) {
		const vec4_t color = { 1, 0, 0.5, 1 };

		backEnd.projection2D = qtrue; // to ensure we have viewport that occupies entire window
		vk_clear_color_attachments( color );
		backEnd.projection2D = qfalse;
	}
	return (const void *)(cmd + 1);
}

/*
=============
RB_SwapBuffers

=============
*/
const void	*RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	RB_EndSurface();

	ResetGhoul2RenderableSurfaceHeap();

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages(tr.images.items, tr.images.count);
	}

	cmd = (const swapBuffersCommand_t *)data;

	tr.needScreenMap = 0;

	vk_end_frame();

	if ( backEnd.doneSurfaces && !glState.finishCalled ) {
		vk_queue_wait_idle();
	}

	if (backEnd.screenshotMask && vk.cmd->waitForFence) {
		if (backEnd.screenshotMask & SCREENSHOT_TGA && backEnd.screenshotTGA[0]) {
			R_TakeScreenshot(0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotTGA);
			if (!backEnd.screenShotTGAsilent) {
				ri.Printf(PRINT_ALL, "Wrote %s\n", backEnd.screenshotTGA);
			}
		}
		if (backEnd.screenshotMask & SCREENSHOT_JPG && backEnd.screenshotJPG[0]) {
			R_TakeScreenshotJPEG(0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotJPG);
			if (!backEnd.screenShotJPGsilent) {
				ri.Printf(PRINT_ALL, "Wrote %s\n", backEnd.screenshotJPG);
			}
		}
		if (backEnd.screenshotMask & SCREENSHOT_PNG && backEnd.screenshotPNG[0]) {
			R_TakeScreenshotPNG(0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotPNG);
			if (!backEnd.screenShotPNGsilent) {
				ri.Printf(PRINT_ALL, "Wrote %s\n", backEnd.screenshotPNG);
			}
		}
		if (backEnd.screenshotMask & SCREENSHOT_AVI) {
			RB_TakeVideoFrameCmd(&backEnd.vcmd);
		}

		backEnd.screenshotJPG[0] = '\0';
		backEnd.screenshotTGA[0] = '\0';
		backEnd.screenshotPNG[0] = '\0';
		backEnd.screenshotMask = 0;
	}

	vk_present_frame();

	backEnd.projection2D = qfalse;
	backEnd.doneSurfaces = qfalse;
	backEnd.doneBloom = qfalse;
	//backEnd.drawConsole = qfalse;

	return (const void *)(cmd + 1);
}

const void	*RB_WorldEffects( const void *data )
{
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	// Always flush the tess buffer
	if ( tess.shader && tess.numIndexes )
		RB_EndSurface();

	RB_RenderWorldEffects();

	if ( tess.shader )
		RB_BeginSurface( tess.shader, tess.fogNum );

	return (const void *)(cmd + 1);
}

/*
=============
RB_ClearColor
=============
*/
static const void *RB_ClearColor( const void *data )
{
	const clearColorCommand_t* cmd = (const clearColorCommand_t*)data;

	backEnd.projection2D = qtrue;

	if ( r_fastsky->integer )
		vk_clear_color_attachments( (float*)tr.clearColor );
	else
		vk_clear_color_attachments( (float*)tr.world->fogs[tr.world->globalFog].color );

	backEnd.projection2D = qfalse;

	return (const void*)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands
====================
*/
extern const void *R_DrawWireframeAutomap( const void *data ); //tr_world.cpp
void RB_ExecuteRenderCommands( const void *data ) {
	int		t1, t2;

	t1 = ri.Milliseconds()*ri.Cvar_VariableValue( "timescale" );

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_ROTATE_PIC:
			data = RB_RotatePic( data );
			break;
		case RC_ROTATE_PIC2:
			data = RB_RotatePic2( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_VIDEOFRAME:
			data = RB_TakeVideoFrameCmd( data );
			break;
		case RC_WORLD_EFFECTS:
			data = RB_WorldEffects( data );
			break;
		case RC_AUTO_MAP:
			data = R_DrawWireframeAutomap(data);
			break;
		case RC_CLEARCOLOR:
			data = RB_ClearColor(data);
			break;
		case RC_END_OF_LIST:
		default:
			// stop rendering
			vk_end_frame();
			t2 = ri.Milliseconds()*ri.Cvar_VariableValue( "timescale" );
			backEnd.pc.msec = t2 - t1;
			return;
		}
	}

}
