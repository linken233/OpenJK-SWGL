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
#include "conversion.h"
#include "rtx_light_edit.h"
#include "rtx_light_style.h"
#include "rtx_light_sky.h"
#include "ghoul2/G2.h"

static int entity_frame_num = 0;
static int model_entity_ids[2][SHADER_MAX_ENTITIES];
static int world_entity_ids[2][MAX_REFENTITIES];
static int light_entity_ids[2][MAX_MODEL_LIGHTS];
static int model_entity_id_count[2];
static int world_entity_id_count[2];
static int light_entity_id_count[2];
static int mdxm_matrix_count[2];
static ModelInstance model_instances_prev[SHADER_MAX_ENTITIES];

static uint32_t g_num_instances = 0;
static vk_geometry_instance_t g_instances[MAX_TLAS_INSTANCES];

static const mat4_t g_identity_transform = {
	1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f
};

static int			num_model_lights;
static light_poly_t model_lights[MAX_MODEL_LIGHTS];
static int			last_model_lights;	// count of the last built frame, read by the light edit

static int			num_accumulated_frames = 0;

static vec3_t avg_envmap_color = { 0.0, 0.0, 0.0 };

typedef struct reference_mode_s 
{
	qboolean enable_accumulation;
	denoiser_type_t denoiser;
	float num_bounce_rays;
	float temporal_blend_factor;
	int reflect_refract;
} reference_mode_t;

// The reservoirs of ReSTIR GI in the images of the previous frame are valid after a frame that wrote them.
static qboolean restir_gi_history_valid = qfalse;

void vk_rtx_restir_gi_invalidate( void )
{
	restir_gi_history_valid = qfalse;
}

void temporal_cvar_changed( void )
{
	vk_rtx_restir_gi_invalidate();
	vk_rtx_invalidate_denoiser_history();
	vk_rtx_taa_invalidate_history();
	num_accumulated_frames = 0;
}

void VK_BeginRenderClear()
{
	vkpt_reset_command_buffers( &vk.cmd_buffers_graphics );
	vkpt_reset_command_buffers( &vk.cmd_buffers_transfer );

	if ( vk.imageDescriptor.needsUpdate == qtrue ) 
	{
		vk_rtx_update_descriptor( &vk.imageDescriptor );
		vk.imageDescriptor.needsUpdate = qfalse;
	}
}

void vkpt_pt_create_all_dynamic( VkCommandBuffer cmd_buf, int idx, const EntityUploadInfo *upload_info )
{
	vk.scratch_buf_ptr = 0;

	accel_build_batch_t batch;
	Com_Memset( &batch, 0, sizeof(accel_build_batch_t) );

	uint64_t offset_vertex_base = 0;
	uint64_t offset_vertex = offset_vertex_base;
	uint64_t offset_index = 0;

	// opaque
	vk_rtx_create_blas( &batch, &vk.buf_positions_instanced, offset_vertex,  NULL, offset_index, 		
		upload_info->opaque_prim_count * 3, 0, &vk.model_instance.blas.dynamic[idx], qtrue, qtrue, qfalse, 0, "instanced opaque" );
	
	// transparent
	offset_vertex = offset_vertex_base + upload_info->transparent_prim_offset * sizeof(prim_positions_t);
	vk_rtx_create_blas( &batch, &vk.buf_positions_instanced, offset_vertex,  NULL, offset_index, 		
		upload_info->transparent_prim_count * 3, 0, &vk.model_instance.blas.transparent_models[idx], qtrue, qtrue, qfalse, 0, "instanced transparent" );

	// masked
	offset_vertex = offset_vertex_base + upload_info->masked_prim_offset * sizeof(prim_positions_t);
	vk_rtx_create_blas( &batch, &vk.buf_positions_instanced, offset_vertex,  NULL, offset_index, 		
		upload_info->masked_prim_count * 3, 0, &vk.model_instance.blas.masked_models[idx], qtrue, qtrue, qfalse, 0, "instanced masked" );

	// viewer models
	offset_vertex = offset_vertex_base + upload_info->viewer_model_prim_offset * sizeof(prim_positions_t);
	vk_rtx_create_blas( &batch, &vk.buf_positions_instanced, offset_vertex,  NULL, offset_index,
		upload_info->viewer_model_prim_count * 3, 0, &vk.model_instance.blas.viewer_models[idx], qtrue, qtrue, qfalse, 0, "instanced viewer models" );

	// distortion surfaces
	offset_vertex = offset_vertex_base + upload_info->distortion_prim_offset * sizeof(prim_positions_t);
	vk_rtx_create_blas( &batch, &vk.buf_positions_instanced, offset_vertex,  NULL, offset_index,
		upload_info->distortion_prim_count * 3, 0, &vk.model_instance.blas.distortion_models[idx], qtrue, qtrue, qfalse, 0, "instanced distortion surfaces" );

	// sprites / beams
	vkbuffer_t* buffer_vertex = NULL;
	vkbuffer_t* buffer_index = NULL;
	uint32_t num_vertices = 0;
	uint32_t num_indices = 0;

	vkbuffer_t *buffer_aabb = NULL;
	uint64_t offset_aabb = 0;
	uint32_t num_aabbs = 0;
	//vkpt_get_beam_aabb_buffer(&buffer_aabb, &offset_aabb, &num_aabbs);
	//vk_rtx_create_blas_aabb(&batch, buffer_aabb, offset_aabb, num_aabbs, &vk.model_instance.blas.beams + idx, true, true);
	
	vkpt_get_transparency_buffers(VKPT_TRANSPARENCY_SPRITES, &buffer_vertex, &offset_vertex, &buffer_index, &offset_index, &num_vertices, &num_indices);
	vk_rtx_create_blas(&batch, buffer_vertex, offset_vertex, buffer_index, offset_index, num_vertices, num_indices, &vk.model_instance.blas.sprites[idx], qtrue, qtrue, qfalse, 0, "sprites");

	if ( batch.numBuilds > 0)
		qvkCmdBuildAccelerationStructuresKHR( cmd_buf, batch.numBuilds, batch.buildInfos, batch.rangeInfoPtrs );

	MEM_BARRIER_BUILD_ACCEL(cmd_buf);
	vk.scratch_buf_ptr = 0;
}

static void append_blas( vk_geometry_instance_t *instances, uint32_t *num_instances, vk_blas_t* blas, int vbo_index, uint32_t prim_offset, int mask, int flags, int sbt_offset )
{
	if (!blas->present)
		return;

	vk_geometry_instance_t instance;
	Com_Memset( &instance, 0, sizeof(vk_geometry_instance_t) );

	mat3x4_t transform = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f };
	Com_Memcpy( &instance.transform, &transform, sizeof(mat3x4_t) );

	instance.instance_id				= vbo_index;
	instance.mask						= mask;
	instance.instance_offset			= sbt_offset;
	instance.flags						= flags;
	instance.acceleration_structure		= 0;
	
	VkAccelerationStructureDeviceAddressInfoKHR  info;
	info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	info.pNext = NULL;
	info.accelerationStructure = blas->accel;

	instance.acceleration_structure = qvkGetAccelerationStructureDeviceAddressKHR(vk.device, &info);

	//assert(*num_instances < INSTANCE_MAX_NUM);
	memcpy(instances + *num_instances, &instance, sizeof(instance));
	vk.uniform_instance_buffer.tlas_instance_prim_offsets[*num_instances] = prim_offset;
	vk.uniform_instance_buffer.tlas_instance_model_indices[*num_instances] = -1;
	++*num_instances;
}

void vkpt_pt_reset_instances()
{
	g_num_instances = 0;
}

#define MAT4_GET(mat, row, col) (mat)[(col) * 4 + (row)]  // For column-major 4x4

//vkpt_pt_instance_model_blas( 
//	&tr.world->geometry.world_static.geom_opaque, g_identity_transform, VERTEX_BUFFER_WORLD, -1, 0 );

void vkpt_pt_instance_model_blas( const model_geometry_t* geom, const mat4_t transform, uint32_t buffer_idx, int model_instance_index, uint32_t override_instance_mask )
{
	if (!geom->accel)
		return;

	vk_geometry_instance_t gpu_instance;
	Com_Memset( &gpu_instance, 0, sizeof(vk_geometry_instance_t) );

	gpu_instance.transform[0] = MAT4_GET(transform, 0, 0);
	gpu_instance.transform[1] = MAT4_GET(transform, 0, 1);
	gpu_instance.transform[2] = MAT4_GET(transform, 0, 2);
	gpu_instance.transform[3] = MAT4_GET(transform, 0, 3);

	gpu_instance.transform[4] = MAT4_GET(transform, 1, 0);
	gpu_instance.transform[5] = MAT4_GET(transform, 1, 1);
	gpu_instance.transform[6] = MAT4_GET(transform, 1, 2);
	gpu_instance.transform[7] = MAT4_GET(transform, 1, 3);

	gpu_instance.transform[8]  = MAT4_GET(transform, 2, 0);
	gpu_instance.transform[9]  = MAT4_GET(transform, 2, 1);
	gpu_instance.transform[10] = MAT4_GET(transform, 2, 2);
	gpu_instance.transform[11] = MAT4_GET(transform, 2, 3);

	gpu_instance.instance_id			= buffer_idx;
	gpu_instance.mask					= override_instance_mask ? override_instance_mask : geom->instance_mask;
	gpu_instance.instance_offset		= geom->sbt_offset;
	gpu_instance.flags					= geom->instance_flags,
	gpu_instance.acceleration_structure = geom->blas_device_address,

	assert(g_num_instances < MAX_TLAS_INSTANCES);
	memcpy(g_instances + g_num_instances, &gpu_instance, sizeof(gpu_instance));
	vk.uniform_instance_buffer.tlas_instance_prim_offsets[g_num_instances] = geom->prim_offsets[0];
	vk.uniform_instance_buffer.tlas_instance_model_indices[g_num_instances] = model_instance_index;
	++g_num_instances;
}

static void vkpt_pt_create_toplevel( VkCommandBuffer cmd_buf, uint32_t idx, const EntityUploadInfo *upload_info ) 
{
	//
	// model/entity instances
	//
	append_blas( g_instances, &g_num_instances,		
		&vk.model_instance.blas.dynamic[idx], VERTEX_BUFFER_INSTANCED, 0, 
		AS_FLAG_OPAQUE, VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR, SBTO_OPAQUE );

	append_blas( g_instances, &g_num_instances,		
		&vk.model_instance.blas.transparent_models[idx], VERTEX_BUFFER_INSTANCED, upload_info->transparent_prim_offset, 
		AS_FLAG_TRANSPARENT, VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR, SBTO_OPAQUE );

	append_blas( g_instances, &g_num_instances,		
		&vk.model_instance.blas.masked_models[idx], VERTEX_BUFFER_INSTANCED, upload_info->masked_prim_offset, 
		AS_FLAG_OPAQUE, VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR | VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, SBTO_MASKED );

	// Primary rays only: shadow, bounce and reflection rays leave out AS_FLAG_VIEWER_MODELS.
	append_blas( g_instances, &g_num_instances,
		&vk.model_instance.blas.viewer_models[idx], VERTEX_BUFFER_INSTANCED, upload_info->viewer_model_prim_offset,
		AS_FLAG_VIEWER_MODELS, VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR, SBTO_OPAQUE );

	// Only the distortion rays of the primary rays see these: the tracer does not light them and they cast no shadow.
	append_blas( g_instances, &g_num_instances,
		&vk.model_instance.blas.distortion_models[idx], VERTEX_BUFFER_INSTANCED, upload_info->distortion_prim_offset,
		AS_FLAG_DISTORTION, VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR, SBTO_OPAQUE );

	uint32_t num_instances_geometry = g_num_instances;

	// effects
	//if (pt_enable_sprites->integer != 0)
	{
		append_blas( g_instances, &g_num_instances, &vk.model_instance.blas.sprites[idx], 0, 0,
			AS_FLAG_EFFECTS, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, SBTO_SPRITE );
	}

	uint32_t num_instances_effects = g_num_instances - num_instances_geometry;

	void *instance_data = buffer_map(vk.buf_instances + idx);
	memcpy(instance_data, &g_instances, sizeof(vk_geometry_instance_t) * g_num_instances);

	buffer_unmap(vk.buf_instances + idx);
	instance_data = NULL;

	accel_build_batch_t batch;
	Com_Memset( &batch, 0, sizeof(accel_build_batch_t) );

	vk.scratch_buf_ptr = 0;
	vk_rtx_destroy_tlas( &vk.tlas_geometry[idx] );
	vk_rtx_create_tlas( &batch, &vk.tlas_geometry[idx], vk.buf_instances[idx].address, num_instances_geometry );

	vk_rtx_destroy_tlas( &vk.tlas_effects[idx] );
	vk_rtx_create_tlas( &batch, &vk.tlas_effects[idx], vk.buf_instances[idx].address + num_instances_geometry * sizeof(vk_geometry_instance_t), num_instances_effects);

	qvkCmdBuildAccelerationStructuresKHR( cmd_buf, batch.numBuilds, batch.buildInfos, batch.rangeInfoPtrs );

	MEM_BARRIER_BUILD_ACCEL(cmd_buf); /* probably not needed here but doesn't matter */
}

static uint32_t compute_mesh_material_flags(const trRefEntity_t* entity, const int model_index, shader_t *shader )
{
	rtx_material_t *material = vk_rtx_shader_to_material( shader );

	uint32_t material_id = material->flags;

	if ( ( entity && entity->e.renderfx & RF_FIRST_PERSON ) )
		material_id |= MATERIAL_FLAG_WEAPON;

	return material_id;
}

static inline void transform_point(const float* p, const float* matrix, float* result)
{
	vec4_t point = { p[0], p[1], p[2], 1.f };
	vec4_t transformed;
	mult_matrix_vector(transformed, matrix, point);
	VectorCopy(transformed, result); // vec4 -> vec3
}

// The time since the previous frame, for the previous position of the deformed vertices.
static float rtx_deform_dt = 0.0f;

// The wave table of a genFunc_t from GF_SIN to GF_INVERSE_SAWTOOTH.
static const float *deform_table( genFunc_t func )
{
	switch ( func )
	{
		case GF_SQUARE:				return tr.squareTable;
		case GF_TRIANGLE:			return tr.triangleTable;
		case GF_SAWTOOTH:			return tr.sawToothTable;
		case GF_INVERSE_SAWTOOTH:	return tr.inverseSawToothTable;
		default:					return tr.sinTable;
	}
}

// A model drawn as a screen distortion: the force push dome (RF_DISTORTION) or the cloak (the internal distortion shader).
static qboolean vk_rtx_is_distortion_entity( const trRefEntity_t *entity )
{
	if ( entity->e.renderfx & RF_DISTORTION )
		return qtrue;

	return ( entity->e.customShader && tr.distortionShader->useDistortion
		&& R_GetShaderByHandle( entity->e.customShader ) == tr.distortionShader ) ? qtrue : qfalse;
}

// A model drawn without the depth test (RF_NODEPTH, the force sight shell): the primary rays see it through the walls.
static qboolean vk_rtx_is_nodepth_entity( const trRefEntity_t *entity )
{
	return ( entity->e.renderfx & RF_NODEPTH ) ? qtrue : qfalse;
}

// An entity with RF_ALPHA_FADE blends its meshes over the screen with its alpha, as the rasterizer does.
static qboolean vk_rtx_is_alpha_fade_entity( const trRefEntity_t *entity )
{
	return ( ( entity->e.renderfx & RF_ALPHA_FADE ) && entity->e.shaderRGBA[3] < 255 ) ? qtrue : qfalse;
}

// The distortion words (RTX_DISTORT_FIRST) of a mesh of a distortion entity, and its kind in word 1.
// Force push: the stage texture coordinate goes through the tcMods of the entity time and the crop of the screen
// around the entity, as the rasterizer draws r_distortionStyle 1. Words 0-3 matrix, 4-5 offset, 6 turbulence
// amplitude, 7 its phase, 8 colour, 9 blend. The cloak has no words: the passes are in the frame data.
static void fill_model_instance_distortion( uint32_t *data, const trRefdef_t *refdef, const trRefEntity_t *entity, const shader_t *shader )
{
	if ( shader == tr.distortionShader )
	{
		data[1] |= INSTANCE_DISTORT_CLOAK;
		return;
	}

	const shaderStage_t *stage = shader->stages[0];
	vec4_t crop;

	if ( !stage || !stage->active || !R_DistortionScreenCrop( entity, crop ) )
		return;

	float matrix[4], offTurb[4];
	const double shaderTime = tess.shaderTime;
	trRefEntity_t *currentEntity = backEnd.currentEntity;

	tess.shaderTime = ( refdef->floatTime - entity->e.shaderTime ) - shader->timeOffset;
	backEnd.currentEntity = (trRefEntity_t *)entity;

	vk_compute_tex_mods( &stage->bundle[0], matrix, offTurb );
	uint32_t color = vk_rtx_bundle_color( &stage->bundle[0] );

	tess.shaderTime = shaderTime;
	backEnd.currentEntity = currentEntity;

	if ( stage->bundle[0].rgbGen == CGEN_ENTITY || ( entity->e.renderfx & RF_RGB_TINT ) )
		color = ( color & 0xff000000u ) | ( data[0] & 0x00ffffffu );

	uint32_t blend = stage->stateBits & GLS_BLEND_BITS;

	// An entity that fades blends over the screen with its alpha, whatever the shader says.
	const qboolean fade = ( ( entity->e.renderfx & RF_ALPHA_FADE ) && entity->e.shaderRGBA[3] < 255 ) ? qtrue : qfalse;

	if ( stage->bundle[0].alphaGen == AGEN_ENTITY || fade )
		color = ( color & 0x00ffffffu ) | ( data[0] & 0xff000000u );

	if ( fade )
		blend = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;

	// The crop is a scale and an offset of the texture coordinate; the turbulence is added after it.
	matrix[0] *= crop[0];
	matrix[2] *= crop[0];
	matrix[1] *= crop[1];
	matrix[3] *= crop[1];
	offTurb[0] = offTurb[0] * crop[0] + crop[2];
	offTurb[1] = offTurb[1] * crop[1] + crop[3];
	offTurb[3] -= floorf( offTurb[3] );

	const float words[8] = { matrix[0], matrix[1], matrix[2], matrix[3], offTurb[0], offTurb[1], offTurb[2], offTurb[3] };

	memcpy( &data[RTX_DISTORT_FIRST], words, sizeof( words ) );
	data[RTX_DISTORT_FIRST + 8] = color;
	data[RTX_DISTORT_FIRST + 9] = STAGE_BLEND_ACTIVE | blend;
	data[1] |= INSTANCE_DISTORT_CROP;
}

// distortion: the caller checked vk_rtx_is_distortion_entity, which a job must not call.
static void fill_model_instance_shader_data( InstanceBuffer *uniform_instance_buffer, int current_instance_index, const trRefdef_t *refdef, trRefEntity_t* entity, shader_t *shader, qboolean distortion )
{
	uint32_t forceRGBGen = 0;

	if ( entity->e.renderfx & ( RF_DISINTEGRATE1 | RF_DISINTEGRATE2 ) )
	{
		if ( entity->e.renderfx & RF_DISINTEGRATE1 )
			forceRGBGen = (uint32_t)CGEN_DISINTEGRATION_1;
		else
			forceRGBGen = (uint32_t)CGEN_DISINTEGRATION_2;
	}
	else if ( entity->e.renderfx & RF_RGB_TINT )
		forceRGBGen = CGEN_ENTITY;

	uint32_t *data = &uniform_instance_buffer->model_instance_shader_data[current_instance_index * INSTANCE_SHADER_UINTS];
	data[0] =
		  ((uint32_t)entity->e.shaderRGBA[0]      )
		| ((uint32_t)entity->e.shaderRGBA[1] <<  8)
		| ((uint32_t)entity->e.shaderRGBA[2] << 16)
		| ((uint32_t)entity->e.shaderRGBA[3] << 24);
	// Bits 0-7: the forced rgbGen. Bit 8: a first person model, whose tcGen environment reflects
	// the light of the entity (RB_CalcEnvironmentTexCoords). Bit 9: a model mesh, not a brush model.
	data[1] = forceRGBGen | ( ( entity->e.renderfx & RF_FIRST_PERSON ) ? 0x100u : 0u ) | ( shader ? 0x200u : 0u );
	if ( shader && shader->cullType == CT_TWO_SIDED )
		data[1] |= INSTANCE_CULL_NONE;
	else if ( shader && shader->cullType == CT_BACK_SIDED )
		data[1] |= INSTANCE_CULL_FRONT;
	const qboolean alpha_fade = ( shader && vk_rtx_is_alpha_fade_entity( entity ) ) ? qtrue : qfalse;
	if ( alpha_fade )
		data[1] |= INSTANCE_ALPHA_FADE;

	// alphaGen lightingSpecular reflects the light of the entity, as RB_CalcSpecularAlpha.
	R_SetupEntityLighting( refdef, entity );
	data[2] = encode_normal( entity->lightDir );

	// The deformVertexes of the shader that the instancing shader applies to the mesh (see
	// apply_deforms). data[3]: the number of deforms, half of the frame time in the high bits.
	// data[4]: the shader time. Then RTX_DEFORM_UINTS per deform: type in bits 0-3 of the first
	// word, wave function in bits 4-7, the first parameter as a half in the high bits; the other
	// parameters are halves. 1 wave: base, amp, phase, freq, spread. 2 bulge: height, width, speed
	// and the phase (a float). 3 uniform bulge: height. 4 move: the wave and the vector.
	// 5 normals: amp, freq.
	for ( int i = 3; i < INSTANCE_SHADER_UINTS; i++ )
		data[i] = 0;

	// A fading model is not lit by the tracer: the layer takes the light of its entity (words RTX_DISTORT_FIRST, +1).
	if ( alpha_fade )
	{
		for ( int k = 0; k < 2; k++ )
		{
			const float *c = k ? entity->directedLight : entity->ambientLight;
			uint32_t packed = 0;

			for ( int j = 0; j < 3; j++ )
				packed |= (uint32_t)Com_Clampi( 0, 255, (int)c[j] ) << ( 8 * j );

			data[RTX_DISTORT_FIRST + k] = packed;
		}

		// Word +2: the entity, so that a ray blends one layer of each fading entity.
		data[RTX_DISTORT_FIRST + 2] = (uint32_t)( entity - refdef->entities ) + 1u;
	}

	// RF_DISINTEGRATE1/2: the impact point and the squared threshold, as vk_compute_disintegration() sends them.
	if ( entity->e.renderfx & ( RF_DISINTEGRATE1 | RF_DISINTEGRATE2 ) )
	{
		const float threshold = ( refdef->time - entity->e.endTime ) * 0.045f;
		const float words[4] = { entity->e.oldorigin[0], entity->e.oldorigin[1], entity->e.oldorigin[2], threshold * threshold };

		memcpy( &data[RTX_DISTORT_FIRST], words, sizeof( words ) );
	}

	if ( shader && shader->numDeforms )
	{
		// The deforms of rd-vanilla use the time of the entity, not the time of the shader.
		const float time = refdef->floatTime - entity->e.shaderTime;

		uint32_t count = 0;

		for ( int i = 0; i < shader->numDeforms && count < RTX_MAX_DEFORMS; i++ )
		{
			const deformStage_t *ds = shader->deforms[i];
			const waveForm_t *wf = &ds->deformationWave;
			uint32_t *slot = &data[RTX_DEFORM_FIRST + count * RTX_DEFORM_UINTS];
			const qboolean wave_ok = ( wf->func >= GF_SIN && wf->func <= GF_INVERSE_SAWTOOTH ) ? qtrue : qfalse;

			if ( ds->deformation == DEFORM_WAVE && wf->frequency == 0.0f )
			{
				// Frequency 0: one scale for all the vertices, with no spread (RB_CalcDeformVertexes).
				float scale;

				if ( wf->func == GF_NOISE )
					scale = wf->base + R_NoiseGet4f( 0, 0, 0, 0 ) * wf->amplitude;
				else if ( wf->func == GF_RAND )
					scale = wf->base + ( GetNoiseTime( (int)( refdef->time + wf->phase ) ) <= 0.0f ? wf->amplitude : 0.0f );
				else if ( wave_ok )
					scale = wf->base + deform_table( wf->func )[Q_ftol( wf->phase * FUNCTABLE_SIZE ) & FUNCTABLE_MASK] * wf->amplitude;
				else
					continue;

				slot[0] = 3u | ( (uint32_t)floatToHalf( scale ) << 16 );
			}
			else if ( ds->deformation == DEFORM_WAVE && wave_ok )
			{
				slot[0] = 1u | ( (uint32_t)wf->func << 4 ) | ( (uint32_t)floatToHalf( wf->base ) << 16 );
				slot[1] = (uint32_t)floatToHalf( wf->amplitude ) | ( (uint32_t)floatToHalf( wf->phase ) << 16 );
				slot[2] = (uint32_t)floatToHalf( wf->frequency ) | ( (uint32_t)floatToHalf( ds->deformationSpread ) << 16 );
			}
			else if ( ds->deformation == DEFORM_MOVE && wave_ok )
			{
				slot[0] = 4u | ( (uint32_t)wf->func << 4 ) | ( (uint32_t)floatToHalf( wf->base ) << 16 );
				slot[1] = (uint32_t)floatToHalf( wf->amplitude ) | ( (uint32_t)floatToHalf( wf->phase ) << 16 );
				slot[2] = (uint32_t)floatToHalf( wf->frequency );
				slot[4] = (uint32_t)floatToHalf( ds->moveVector[0] ) | ( (uint32_t)floatToHalf( ds->moveVector[1] ) << 16 );
				slot[5] = (uint32_t)floatToHalf( ds->moveVector[2] );
			}
			else if ( ds->deformation == DEFORM_BULGE )
			{
				if ( ds->bulgeSpeed == 0.0f && ds->bulgeWidth == 0.0f )
				{
					slot[0] = 3u | ( (uint32_t)floatToHalf( ds->bulgeHeight ) << 16 );
				}
				else
				{
					// Only the phase of the sine matters: keep the float small.
					const float now = fmodf( refdef->floatTime * ds->bulgeSpeed, 2.0f * (float)M_PI );

					slot[0] = 2u | ( (uint32_t)floatToHalf( ds->bulgeHeight ) << 16 );
					slot[1] = (uint32_t)floatToHalf( ds->bulgeWidth );
					slot[2] = (uint32_t)floatToHalf( ds->bulgeSpeed );
					memcpy( &slot[3], &now, sizeof( float ) );
				}
			}
			else if ( ds->deformation == DEFORM_NORMALS )
			{
				slot[0] = 5u;
				slot[1] = (uint32_t)floatToHalf( wf->amplitude );
				slot[2] = (uint32_t)floatToHalf( wf->frequency );
			}
			else
				continue;

			count++;
		}

		if ( count )
		{
			data[3] = count | ( (uint32_t)floatToHalf( rtx_deform_dt ) << 16 );
			memcpy( &data[4], &time, sizeof( float ) );
		}
	}

	if ( shader && distortion )
		fill_model_instance_distortion( data, refdef, entity, shader );
	else if ( shader && vk_rtx_is_nodepth_entity( entity ) )
		data[1] |= INSTANCE_NODEPTH;
}

// The instance buffer is full: the remaining meshes are not traced this frame.
static void vk_rtx_instance_overflow( void )
{
	static int last_warning = -100000;

	if ( ri.Milliseconds() - last_warning > 5000 )
	{
		last_warning = ri.Milliseconds();
		ri.Printf( PRINT_WARNING, "RTX: more than %i model meshes, some are not traced\n", SHADER_MAX_ENTITIES );
	}
}

static void fill_model_instance( ModelInstance* instance, const trRefEntity_t* entity, const maliasmesh_t *mesh, shader_t *shader,
	const float* transform, qboolean is_viewer_weapon, qboolean is_double_sided, uint32_t material_id, int bone_offset )
{
	int cluster = -1;
	if ( tr.world )
		cluster = BSP_PointLeaf( tr.world->nodes, (float*)entity->e.origin )->cluster;

#if 0
	int frame = entity->e.frame;
	int oldframe = entity->e.oldframe;
#else
	int frame = 0;
	int oldframe = 0;
#endif
	memcpy(instance->transform, transform, sizeof(float) * 16);
	memcpy(instance->transform_prev, transform, sizeof(float) * 16);
	instance->material = material_id;
	instance->shell = 0U;
	instance->cluster = cluster;
	instance->source_buffer_idx = mesh->modelIndex; // + VERTEX_BUFFER_FIRST_MODEL ;
	instance->prim_count = mesh->numIndexes / 3;

	const int offset_cur = 0;
	instance->prim_offset_curr_pose_curr_frame = offset_cur;
	instance->prim_offset_prev_pose_curr_frame = offset_cur;
	instance->prim_offset_curr_pose_prev_frame = instance->prim_offset_curr_pose_curr_frame;
	instance->prim_offset_prev_pose_prev_frame = instance->prim_offset_prev_pose_curr_frame;
	instance->pose_lerp_curr_frame = entity->e.backlerp;
	instance->pose_lerp_prev_frame = instance->pose_lerp_curr_frame;
	instance->mdxm_matrix_offset_curr = bone_offset;
	instance->mdxm_matrix_offset_prev = instance->mdxm_matrix_offset_curr;
	instance->alpha_and_frame = floatToHalf(1.0f);
	instance->render_buffer_idx = 0; // to be filled later
	instance->render_prim_offset = 0;

	instance->idx_offset = mesh->indexOffset;
}

static void add_dlight_spot(const dlight_t* dlight, light_poly_t* light)
{
	// Copy spot data
	VectorCopy(dlight->spot.direction, light->positions + 6);
	uint32_t spot_type = 0;
	uint32_t spot_data = 0;
	switch(dlight->spot.emission_profile)
	{
	case DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF:
		spot_type = DYNLIGHT_SPOT_EMISSION_PROFILE_FALLOFF;
		spot_data = floatToHalf(dlight->spot.cos_total_width) | (floatToHalf(dlight->spot.cos_falloff_start) << 16);
		break;
	case DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE:
		spot_type = DYNLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE;
		spot_data = floatToHalf(dlight->spot.total_width) | (dlight->spot.texture << 16);
		break;
	}
	light->positions[4] = uintBitsToFloat(spot_type);
	light->positions[5] = uintBitsToFloat(spot_data);
}

// A dlight whose origin lands in solid is dropped, which is the common case for an
// impact: the effect origin sits on the surface it hit, and the leaf a point on a plane
// falls in is often the solid side. Probe a short way along each axis, and move the
// light out to wherever that finds open space - a light sitting behind the plane of the
// surface it lit is rejected by the dot(n, L) test anyway.
static int dlight_cluster( world_t *worldData, vec3_t origin, float probe )
{
	static const vec3_t offsets[6] = {
		{  1,  0,  0 }, { -1,  0,  0 }, {  0,  1,  0 },
		{  0, -1,  0 }, {  0,  0,  1 }, {  0,  0, -1 }
	};

	int cluster = BSP_PointLeaf( worldData->nodes, origin )->cluster;

	for ( int i = 0; cluster < 0 && i < 6; i++ )
	{
		vec3_t probed;
		VectorMA( origin, probe, offsets[i], probed );
		cluster = BSP_PointLeaf( worldData->nodes, probed )->cluster;

		if ( cluster >= 0 )
			VectorCopy( probed, origin );
	}

	return cluster;
}

// ReSTIR matches a light to its previous-frame self by this id (see
// update_mlight_prev_to_current and reproject_light_index), so it has to identify the
// light, not its slot or its position: refdef->dlights is rebuilt every frame in any
// order, and a blaster bolt moves every frame. Each dlight takes the id of the nearest
// dlight of the previous frame with the same hue, within DLIGHT_TRACK_DISTANCE. A light
// that matches nothing gets a new id.
#define DLIGHT_TRACK_DISTANCE	192.0f
#define DLIGHT_TRACK_MAX		128

typedef struct {
	vec3_t		origin;
	vec3_t		hue;		// colour / peak: an impact flash fades by scaling its colour
	uint32_t	id;
} dlight_track_t;

static dlight_track_t	dlight_track_prev[DLIGHT_TRACK_MAX];
static int				dlight_track_prev_count;
static uint32_t			dlight_track_next_id = 1;

static void dlight_hue( const dlight_t *dlight, vec3_t hue )
{
	const float peak = MAX( dlight->color[0], MAX( dlight->color[1], dlight->color[2] ) );

	if ( peak > 0.0f )
		VectorScale( dlight->color, 1.0f / peak, hue );
	else
		VectorClear( hue );
}

// hash.entity is 14 bits, and 0 means "no identity" to update_mlight_prev_to_current.
static void dlight_track_ids( const dlight_t *dlights, int num_dlights, uint32_t *ids )
{
	dlight_track_t	cur[DLIGHT_TRACK_MAX];
	qboolean		claimed[DLIGHT_TRACK_MAX];
	int				i, j;

	num_dlights = MIN( num_dlights, DLIGHT_TRACK_MAX );
	Com_Memset( claimed, 0, sizeof( claimed ) );

	for ( i = 0; i < num_dlights; i++ )
	{
		const dlight_t *dlight = dlights + i;
		float best = DLIGHT_TRACK_DISTANCE * DLIGHT_TRACK_DISTANCE;
		int match = -1;

		VectorCopy( dlight->origin, cur[i].origin );
		dlight_hue( dlight, cur[i].hue );

		for ( j = 0; j < dlight_track_prev_count; j++ )
		{
			const dlight_track_t *prev = &dlight_track_prev[j];

			if ( claimed[j] )
				continue;
			if ( fabsf( prev->hue[0] - cur[i].hue[0] ) > 0.1f || fabsf( prev->hue[1] - cur[i].hue[1] ) > 0.1f || fabsf( prev->hue[2] - cur[i].hue[2] ) > 0.1f )
				continue;

			const float d = DistanceSquared( prev->origin, cur[i].origin );
			if ( d < best )
			{
				best = d;
				match = j;
			}
		}

		if ( match >= 0 )
		{
			claimed[match] = qtrue;
			cur[i].id = dlight_track_prev[match].id;
		}
		else
		{
			cur[i].id = dlight_track_next_id;
			dlight_track_next_id = ( dlight_track_next_id % 16383u ) + 1u;
		}

		ids[i] = cur[i].id;
	}

	Com_Memcpy( dlight_track_prev, cur, sizeof( dlight_track_t ) * num_dlights );
	dlight_track_prev_count = num_dlights;
}

static void
add_dlights(const dlight_t* dlights, int num_dlights, light_poly_t* light_list, int* num_lights, int max_lights, world_t *worldData, int* light_entity_ids)
{
	uint32_t ids[DLIGHT_TRACK_MAX];

	num_dlights = MIN( num_dlights, DLIGHT_TRACK_MAX );
	dlight_track_ids( dlights, num_dlights, ids );

	// Intensity of the dynamic lights (blaster bolts, explosions, muzzle flashes), 1 for no change.
	const float dlight_scale = MAX( 0.f, pt_light_scale_dlight->value );

	for (int i = 0; i < num_dlights; i++)
	{
		if (*num_lights >= max_lights)
			return;

		const dlight_t* dlight = dlights + i;
		light_poly_t* light = light_list + *num_lights;

		// The frontend moved the light off the surface it hit, see R_LiftDlightsForRtx.
		vec3_t origin;
		VectorCopy( dlight->rtxOrigin, origin );

		const qboolean is_spot = (qboolean)(dlight->light_type == DLIGHT_SPOT);
		const float falloff_radius = dlight->radius;
		float emitter_radius = pt_dlight_radius->value;

		if ( emitter_radius <= 0.0f || emitter_radius > falloff_radius )
			emitter_radius = falloff_radius;

		light->cluster = dlight_cluster( worldData, origin, 8.0f );

		// Zeroed first: the whole 32 bits are compared as one word, and bsp is never
		// assigned here, so leaving it to the stack makes the match a coin toss.
		entity_hash_t hash;
		Com_Memset( &hash, 0, sizeof(hash) );

		hash.entity = ids[i];
		hash.mesh = 0xAA;

		if(light->cluster >= 0)
		{
			//Super wasteful but we want to have all lights in the same list.

			// dlight->radius is the distance the light reaches, but positions[3] is the
			// radius of the sphere the light is emitted *from*. Feeding the first into
			// the second gave every impact a 100-300 unit emitter centred on the point
			// it hit, half of it buried in that surface: sample_lights drops every
			// sample below the shaded point's horizon, so the light lost a dark disc of
			// its own illumination, and the samples that survived came back shadowed by
			// the surface itself. Emit from a small sphere instead.
			//
			// Shrinking the emitter costs (falloff/emitter)^2 of solid angle; putting
			// exactly that back leaves lighting at a distance independent of the emitter
			// size, since irradiance out there is colour * r^2 / d^2 and r cancels. Only
			// the plateau within the emitter itself scales, as 1/r^2.
			//
			// A spotlight emits from a disk facing its own direction and its irradiance
			// is 2 * falloff / d^2 whatever the radius, so it needs no compensation.
			float power_scale = 1.0f;

			if ( !is_spot && emitter_radius > 0.0f )
				power_scale = MAX( 1.0f, ( falloff_radius * falloff_radius ) / ( emitter_radius * emitter_radius ) );

			VectorCopy(origin, light->positions + 0);
			VectorScale(dlight->color, ( falloff_radius / 250.f ) * power_scale * dlight_scale, light->color);
			light->positions[3] = emitter_radius;
			light->material = NULL;
			light->style = 0;

			switch(dlight->light_type) {
				case DLIGHT_SPOT:
					light->type = LIGHT_SPOT;
					// Copy spot data
					add_dlight_spot(dlight, light);
					hash.model = 0xFD;
					break;
				case DLIGHT_SPHERE:
				default:
					// light_list entries are reused frame to frame, so falling out of
					// this switch would leave the previous occupant's type and cone.
					light->type = LIGHT_SPHERE;
					hash.model = 0xFE;

					// The raster peaks at the dlight colour. Below this distance the light stays at
					// its level there. A small emitter near a surface is then not a hot spot of noise.
					light->positions[4] = 0.0f;
					light->positions[5] = pt_dlight_min_dist->value
						* ( r_dlightScale->value > 0.0f ? falloff_radius / r_dlightScale->value : falloff_radius );
					break;
			}

			if ( pt_verbose->integer > 1 )
				ri.Printf( PRINT_ALL, "dlight %i: %s falloff %.0f emitter %.1f gain %.1f -> colour %.2f %.2f %.2f\n",
					i, is_spot ? "spot  " : "sphere", falloff_radius, emitter_radius, power_scale,
					light->color[0], light->color[1], light->color[2] );

			light_entity_ids[(*num_lights)] = *(uint32_t*)&hash;
			(*num_lights)++;

		}
		else if ( pt_verbose->integer > 1 )
		{
			ri.Printf( PRINT_ALL, "dlight %i dropped: origin %.0f %.0f %.0f is in solid, radius %.0f\n",
				i, dlight->origin[0], dlight->origin[1], dlight->origin[2], dlight->radius );
		}
	}
}

#define MESH_FILTER_TRANSPARENT 1
#define MESH_FILTER_OPAQUE 2
#define MESH_FILTER_MASKED 4
#define MESH_FILTER_DISTORTION 8
#define MESH_FILTER_ALL 3

typedef struct {
	maliasmesh_t	*mesh;
	shader_t		*shader;
	int				bone_offset;
} vk_rtx_entity_mesh_t;

static vk_rtx_entity_mesh_t entity_meshes[SHADER_MAX_ENTITIES];
static uint32_t entity_mesh_count = 0;

void vk_rtx_add_entity_mesh( maliasmesh_t *mesh, shader_t *shader, int bone_offset ) 
{
	vk_rtx_entity_mesh_t *entity_mesh = &entity_meshes[entity_mesh_count++];
	entity_mesh->mesh			= mesh;
	entity_mesh->shader			= shader;
	entity_mesh->bone_offset	= bone_offset;

	//Com_Printf("mesh_count: %u bone offset: %u\n", entity_mesh_count, bone_offset);
}

static qboolean vk_rtx_collect_entity_meshes( const model_t* model, const uint32_t entityNum, trRefEntity_t *entity, int *mdxm_matrix_offset, mat3x4_t *mdxm_matrix_data )
{
	// Only entries below entity_mesh_count are read, and vk_rtx_add_entity_mesh sets all their fields.
	entity_mesh_count = 0;

	switch (model->type)
	{
		case MOD_MDXM:
		case MOD_BAD:
			vk_rtx_AddGhoulSurfaces( entity, entityNum, mdxm_matrix_offset, mdxm_matrix_data );
			break;
		case MOD_MESH:
			vk_rtx_AddMD3Surfaces( entity, entityNum, model );
			break;
		default:
			break;
	}

	return entity_mesh_count > 0 ? qtrue : qfalse;
}

#define MAX_VIEWER_MD3_SIZE 64.0f

// A third person camera pushed against a wall goes into the player model. The raster clips the
// model with the near plane and back faces, but its triangles still block the light of the
// surfaces near the camera: they get a shadow with no visible caster, or go black.
static qboolean camera_inside_model( const trRefdef_t *refdef, trRefEntity_t *entity, const model_t *model, const uint32_t entityNum,
	int mdxm_matrix_offset, mat3x4_t *mdxm_matrix_data )
{
	const mdvFrame_t *md3_frame = NULL;

	if ( model->type == MOD_MESH )
	{
		// A misc_camera looks out of its own MD3, so the view starts inside that model.
		const mdvModel_t *mdv = model->data.mdv[0];
		const int frame = ( entity->e.frame >= 0 && entity->e.frame < mdv->numFrames ) ? entity->e.frame : 0;
		md3_frame = mdv->frames + frame;
	}
	else if ( model->type != MOD_MDXM )
		return qfalse;

	float scale = MAX( entity->e.modelScale[0], MAX( entity->e.modelScale[1], entity->e.modelScale[2] ) );
	if ( scale <= 0.0f )
		scale = 1.0f;

	const float radius = ( md3_frame ? md3_frame->radius : ( entity->e.radius > 0.0f ? entity->e.radius : 64.0f ) ) * scale;

	vec3_t delta;
	VectorSubtract( refdef->vieworg, entity->e.origin, delta );

	if ( DotProduct( delta, delta ) > radius * radius )
		return qfalse;

	vec3_t bounds[2];
	if ( md3_frame )
	{
		VectorCopy( md3_frame->bounds[0], bounds[0] );
		VectorCopy( md3_frame->bounds[1], bounds[1] );

		// A larger rigid model is a room or a prop the camera stands in, not a camera body.
		for ( int k = 0; k < 3; k++ )
		{
			if ( ( bounds[1][k] - bounds[0][k] ) * scale > MAX_VIEWER_MD3_SIZE )
				return qfalse;
		}
	}
	else
	{
		// The bone matrices go where the next process_regular_entity puts the same matrices.
		ClearBounds( bounds[0], bounds[1] );
		vk_rtx_GhoulBounds( bounds[0] );
		vk_rtx_collect_entity_meshes( model, entityNum, entity, &mdxm_matrix_offset, mdxm_matrix_data );
		vk_rtx_GhoulBounds( NULL );
	}

	for ( int k = 0; k < 3; k++ )
	{
		const float len2 = DotProduct( entity->e.axis[k], entity->e.axis[k] );
		const float local = len2 > 0.0f ? DotProduct( delta, entity->e.axis[k] ) / len2 : 0.0f;

		if ( local < bounds[0][k] || local > bounds[1][k] )
			return qfalse;
	}

	return qtrue;
}

// Model lights of the last built frame, for the light edit.
int vk_rtx_get_model_lights( const light_poly_t **out )
{
	*out = model_lights;
	return last_model_lights;
}

static void instance_model_lights(int num_light_polys, const light_poly_t* light_polys, const float* transform)
{
	for (int nlight = 0; nlight < num_light_polys; nlight++)
	{
		if (num_model_lights >= MAX_MODEL_LIGHTS)
		{
			assert(!"Model light count overflow");
			break;
		}

		const light_poly_t* src_light = light_polys + nlight;
		light_poly_t* dst_light = model_lights + num_model_lights;

		// Transform the light's positions and center
		transform_point(src_light->positions + 0, transform, dst_light->positions + 0);
		transform_point(src_light->positions + 3, transform, dst_light->positions + 3);
		transform_point(src_light->positions + 6, transform, dst_light->positions + 6);
		transform_point(src_light->off_center, transform, dst_light->off_center);

		// Find the cluster based on the center. Maybe it's OK to use the model's cluster, need to test.
		dst_light->cluster = BSP_PointLeaf(tr.world->nodes, dst_light->off_center)->cluster;

		// We really need to map these lights to a cluster
		if (dst_light->cluster < 0)
			continue;

		// Copy the other light properties
		VectorCopy(src_light->color, dst_light->color);
		dst_light->material = src_light->material;
		dst_light->style = src_light->style;

		num_model_lights++;
	}
}

static void process_bsp_entity(
	const uint32_t entityNum,
	const trRefdef_t *refdef,
	trRefEntity_t *entity, 
	const model_t *model,
	int* instance_count )
{
	InstanceBuffer* uniform_instance_buffer = &vk.uniform_instance_buffer;

	const int current_instance_idx = *instance_count;
	if (current_instance_idx >= SHADER_MAX_ENTITIES)
	{
		vk_rtx_instance_overflow();
		return;
	}

	mat4_t transform;
	create_entity_matrix( transform, entity, qfalse );

	bmodel_t* bmodel = model->data.bmodel;


	vec3_t origin;
	transform_point(bmodel->center, transform, origin);
	int cluster = BSP_PointLeaf( tr.world->nodes, origin)->cluster;

	if (cluster < 0)
	{
		// In some cases, a model slides into a wall, like a push button, so that its center 
		// is no longer in any BSP node. We still need to assign a cluster to the model,
		// so try the corners of the model instead, see if any of them has a valid cluster.

		for (int corner = 0; corner < 8; corner++)
		{
			vec3_t corner_pt = {
				(corner & 1) ? bmodel->aabb_max[0] : bmodel->aabb_min[0],
				(corner & 2) ? bmodel->aabb_max[1] : bmodel->aabb_min[1],
				(corner & 4) ? bmodel->aabb_max[2] : bmodel->aabb_min[2]
			};

			vec3_t corner_pt_world;
			transform_point(corner_pt, transform, corner_pt_world);

			cluster = BSP_PointLeaf( tr.world->nodes, corner_pt_world)->cluster;

			if(cluster >= 0)
				break;
		}
	}

	entity_hash_t hash;
	hash.entity = entity->e.id;
	hash.model = ~entity->e.hModel;
	hash.mesh = 0;
	hash.bsp = 1;

	memcpy(&model_entity_ids[entity_frame_num][current_instance_idx], &hash, sizeof(uint32_t));

#if 0
	float model_alpha = (entity->flags & RF_TRANSLUCENT) ? entity->alpha : 1.f;
#else
	float model_alpha = 1.f;
#endif

	ModelInstance* mi = uniform_instance_buffer->model_instances + current_instance_idx;
	// calc_color reads the entity color here; without it a brush model got the data another
	// instance left at this index.
	fill_model_instance_shader_data( uniform_instance_buffer, current_instance_idx, refdef, entity, NULL, qfalse );
	memcpy(&mi->transform, transform, sizeof(transform));
	memcpy(&mi->transform_prev, transform, sizeof(transform));
	mi->material = 0;
	mi->cluster = cluster;
	mi->source_buffer_idx = VERTEX_BUFFER_SUB_MODELS;
	mi->prim_count = bmodel->geometry.prim_counts[0];
	mi->prim_offset_curr_pose_curr_frame = 0; // bsp models are not processed by the instancing shader
	mi->prim_offset_prev_pose_curr_frame = 0;
	mi->prim_offset_curr_pose_prev_frame = 0;
	mi->prim_offset_prev_pose_prev_frame = 0;
	mi->pose_lerp_curr_frame = 0.f;
	mi->pose_lerp_prev_frame = 0.f;
	mi->mdxm_matrix_offset_curr = -1;
	mi->mdxm_matrix_offset_prev = -1;
	// A door shows its lock state through its animMaps: RF_SETANIMINDEX picks the frame, and
	// EF_DISABLE_SHADER_ANIM sets shaderTime to now every frame to hold them on frame 0.
	// The instance keeps that frame + 1 (0: the animMaps follow the time), see animate_material.
	int anim_frame = 0;
	if ( entity->e.renderfx & RF_SETANIMINDEX )
		anim_frame = entity->e.skinNum + 1;
	else if ( entity->e.shaderTime != 0.0f && refdef->floatTime - entity->e.shaderTime < 0.1 )
		anim_frame = 1;
	mi->alpha_and_frame = ( (uint32_t)anim_frame << 16 ) | floatToHalf(model_alpha);
	mi->render_buffer_idx = VERTEX_BUFFER_SUB_MODELS;
	mi->render_prim_offset = bmodel->geometry.prim_offsets[0];

	if (bmodel->geometry.accel)
	{
		vkpt_pt_instance_model_blas(&bmodel->geometry, mi->transform, VERTEX_BUFFER_SUB_MODELS, current_instance_idx, (model_alpha < 1.f) ? AS_FLAG_TRANSPARENT : 0);
	}

	if (!bmodel->transparent)
	{
		vkpt_shadow_map_add_instance(transform, tr.world->geometry.world_submodels.buffer->buffer,  tr.world->geometry.world_submodels.vertex_data_offset
			+ mi->render_prim_offset * sizeof(prim_positions_t), mi->prim_count);
	}

	(*instance_count)++;
}

// One mesh instance whose data a job writes. process_regular_entity fills the list in order.
typedef struct {
	trRefEntity_t	*entity;
	maliasmesh_t	*mesh;
	shader_t		*shader;
	uint32_t		material_id;
	int				bone_offset;
	int				instance_index;
	int				render_prim_offset;
	qboolean		distortion;		// the distortion data uses tess and backEnd, so it stays on the calling thread
	mat4_t			transform;
} vk_rtx_instance_job_t;

static vk_rtx_instance_job_t	instance_jobs[SHADER_MAX_ENTITIES];
static int						instance_job_count;

typedef struct {
	const trRefdef_t	*refdef;
	qboolean			serial_pass;
} instanceJobCtx_t;

// Each job writes only the slots of its own instance index.
static void instance_job_range( int begin, int end, void *p )
{
	const instanceJobCtx_t *ctx = (const instanceJobCtx_t *)p;
	InstanceBuffer *buffer = &vk.uniform_instance_buffer;

	for ( int n = begin; n < end; n++ )
	{
		const vk_rtx_instance_job_t *job = &instance_jobs[n];

		if ( job->distortion != ctx->serial_pass )
			continue;

		ModelInstance *mi = &buffer->model_instances[job->instance_index];

		fill_model_instance_shader_data( buffer, job->instance_index, ctx->refdef, job->entity, job->shader, job->distortion );
		fill_model_instance( mi, job->entity, job->mesh, job->shader, job->transform, qfalse, qfalse, job->material_id, job->bone_offset );

		mi->render_buffer_idx = VERTEX_BUFFER_INSTANCED;
		mi->render_prim_offset = job->render_prim_offset;
	}
}

static void vk_rtx_run_instance_jobs( const trRefdef_t *refdef )
{
	instanceJobCtx_t ctx;
	ctx.refdef = refdef;

	// Grain 8: one job is a few hundred nanoseconds to a few microseconds.
	ctx.serial_pass = qfalse;
	RTX_ParallelFor( instance_job_count, 8, instance_job_range, &ctx );

	ctx.serial_pass = qtrue;
	instance_job_range( 0, instance_job_count, &ctx );
}

static void process_regular_entity(
	const uint32_t entityNum,
	const trRefdef_t *refdef,
	trRefEntity_t *entity, 
	const model_t *model,
	qboolean is_viewer_weapon, 
	qboolean is_double_sided, 

	int* instance_count, 
	int* animated_count, 
	int* num_instanced_prim, 

	int mesh_filter, 
	qboolean* contains_transparent,
	qboolean* contains_masked,
	int* mdxm_matrix_offset,
	mat3x4_t *mdxm_matrix_data
)
{
	if ( !vk_rtx_collect_entity_meshes( model, entityNum, entity, mdxm_matrix_offset, mdxm_matrix_data ) )
		return;

	InstanceBuffer *uniform_instance_buffer = &vk.uniform_instance_buffer;
	uint32_t i;
	const int job_base = instance_job_count;

	mat4_t transform;
	create_entity_matrix( transform, (trRefEntity_t*)entity, qfalse );

	int current_instance_index = *instance_count;
	int current_animated_index = *animated_count;
	int current_num_instanced_prim = *num_instanced_prim;

	if ( contains_transparent )
		*contains_transparent = qfalse;

	bool use_static_blas = false;

	for ( i = 0; i < entity_mesh_count; i++ ) 
	{
		vk_rtx_entity_mesh_t *entity_mesh = &entity_meshes[i];

		if ( current_instance_index >= SHADER_MAX_ENTITIES || current_animated_index >= SHADER_MAX_ENTITIES )
		{
			vk_rtx_instance_overflow();
			break;
		}

		if (  entity_mesh->mesh->indexOffset < 0 ) // failed to upload the vertex data - don't instance this mesh
		{
			instance_job_count = job_base;	// drop the jobs of the meshes before this one
			return;
		}

		uint32_t material_id = compute_mesh_material_flags( entity, entity_mesh->mesh->modelIndex, entity_mesh->shader );

		if (!material_id)
			continue;

		if ( mesh_filter & MESH_FILTER_DISTORTION )
		{
			// All the meshes of a distortion entity are distortion surfaces.
		}
		else if ( RB_IsMasked( entity_mesh->shader ) || ( entity->e.renderfx & RF_DISINTEGRATE1 ) )
		{
			// RF_DISINTEGRATE1 burns holes in the body: the any-hit of the masked meshes lets the rays through them.
			if (contains_masked)
				*contains_masked = qtrue;

			if (!(mesh_filter & MESH_FILTER_MASKED))
				continue;
		}
		else if ( RB_IsTransparent( entity_mesh->shader ) || vk_rtx_is_alpha_fade_entity( entity ) )
		{
			if(contains_transparent)
				*contains_transparent = qtrue;

			if(!(mesh_filter & MESH_FILTER_TRANSPARENT))
				continue;
		}
		else
		{
			if (!(mesh_filter & MESH_FILTER_OPAQUE))
				continue;
		}

		entity_hash_t hash;
		hash.entity = entity->e.id;
		hash.model =  entity->e.hModel;
		hash.mesh = i;
		hash.bsp = 0;

		memcpy(&model_entity_ids[entity_frame_num][current_instance_index], &hash, sizeof(uint32_t));
		
		// The instance data is written later by vk_rtx_run_instance_jobs, in parallel.
		// Everything that depends on the running counters is decided here, in order.
		{
			vk_rtx_instance_job_t *job = &instance_jobs[instance_job_count++];

			// Calling it here keeps the jobs from writing the lighting of a shared entity.
			R_SetupEntityLighting( refdef, entity );

			job->entity = entity;
			job->mesh = entity_mesh->mesh;
			job->shader = entity_mesh->shader;
			job->material_id = material_id;
			job->bone_offset = entity_mesh->bone_offset;
			job->instance_index = current_instance_index;
			job->render_prim_offset = current_num_instanced_prim;
			job->distortion = ( entity_mesh->shader && vk_rtx_is_distortion_entity( entity ) ) ? qtrue : qfalse;
			Com_Memcpy( job->transform, transform, sizeof( job->transform ) );
		}


#if 0
		// ~sunny, not implemented yet
		if (use_static_blas)
		{
			mi->render_buffer_idx = mi->source_buffer_idx;
			mi->render_prim_offset = mi->prim_offset_curr_pose_curr_frame;

			if (!MAT_IsTransparent(mat_shell.material_id))
			{
				vkpt_shadow_map_add_instance(transform, vbo->buffer.buffer, vbo->vertex_data_offset
					+ mi->render_prim_offset * sizeof(prim_positions_t), mi->prim_count);
			}
		}
		else 
#endif
		{
			uniform_instance_buffer->animated_model_indices[current_animated_index] = current_instance_index;

			current_animated_index++;
			current_num_instanced_prim += entity_mesh->mesh->numIndexes / 3;
		}

		current_instance_index++;
	}

#if 0
	// add cylinder lights for wall lamps
	if (model->model_class == MCLASS_STATIC_LIGHT)
	{
		vec4_t begin, end, color;
		vec4_t offset1 = { 0.f, 0.5f, -10.f, 1.f };
		vec4_t offset2 = { 0.f, 0.5f,  10.f, 1.f };

		mult_matrix_vector(begin, transform, offset1);
		mult_matrix_vector(end, transform, offset2);
		VectorSet(color, 0.25f, 0.5f, 0.07f);

		vkpt_build_cylinder_light(model_lights, &num_model_lights, MAX_MODEL_LIGHTS, bsp_world_model, begin, end, color, 1.5f);
	}
#endif

	*instance_count = current_instance_index;
	*animated_count = current_animated_index;
	*num_instanced_prim = current_num_instanced_prim;
}

typedef struct {
	InstanceBuffer	*buffer;
	const int		*cur_ids;
	const int		*prev_ids;
	int				cur_count;
	int				prev_count;
} entityMatchCtx_t;

// Finds the previous frame instance of each current instance. A job writes only its own entries.
static void match_current_to_prev_range( int begin, int end, void *p )
{
	const entityMatchCtx_t *c = (const entityMatchCtx_t *)p;

	for ( int i = begin; i < end; i++ )
	{
		entity_hash_t hash;
		memcpy( &hash, &c->cur_ids[i], sizeof( entity_hash_t ) );

		if ( hash.entity == 0u )
			continue;

		for ( int j = c->prev_count - 1; j >= 0; j-- )
		{
			if ( c->cur_ids[i] != c->prev_ids[j] )
				continue;

			c->buffer->model_current_to_prev[i] = j;

			// Copy the "prev" instance paramters from the previous frame's instance buffer
			ModelInstance* mi_curr = c->buffer->model_instances + i;
			ModelInstance* mi_prev = model_instances_prev + j;

			memcpy(mi_curr->transform_prev, mi_prev->transform, sizeof(mi_curr->transform_prev));
			mi_curr->prim_offset_curr_pose_prev_frame = mi_prev->prim_offset_curr_pose_curr_frame;
			mi_curr->prim_offset_prev_pose_prev_frame = mi_prev->prim_offset_prev_pose_curr_frame;
			mi_curr->pose_lerp_prev_frame = mi_prev->pose_lerp_curr_frame;
			mi_curr->mdxm_matrix_offset_prev = mi_prev->mdxm_matrix_offset_curr;
			break;
		}
	}
}

// Finds the current instance of each previous frame instance.
static void match_prev_to_current_range( int begin, int end, void *p )
{
	const entityMatchCtx_t *c = (const entityMatchCtx_t *)p;

	for ( int j = begin; j < end; j++ )
	{
		entity_hash_t hash;
		memcpy( &hash, &c->prev_ids[j], sizeof( entity_hash_t ) );

		if ( hash.entity == 0u )
			continue;

		for ( int i = c->cur_count - 1; i >= 0; i-- )
		{
			if ( c->cur_ids[i] == c->prev_ids[j] )
			{
				c->buffer->model_prev_to_current[j] = i;
				break;
			}
		}
	}
}

static void prepare_entities( EntityUploadInfo *upload_info, const trRefdef_t *refdef )
{
	uint32_t i;

	entity_frame_num = !entity_frame_num;
	instance_job_count = 0;

	{
		static float last_float_time = 0.0f;

		rtx_deform_dt = refdef->floatTime - last_float_time;
		if ( rtx_deform_dt < 0.0f || rtx_deform_dt > 0.1f )
			rtx_deform_dt = 0.0f;
		last_float_time = refdef->floatTime;
	}

	InstanceBuffer *instance_buffer = &vk.uniform_instance_buffer;

	static int transparent_model_indices[MAX_REFENTITIES];
	static int masked_model_indices[MAX_REFENTITIES];
	static int viewer_model_indices[MAX_REFENTITIES];
	static int viewer_weapon_indices[MAX_REFENTITIES];
	static int distortion_model_indices[MAX_REFENTITIES];
	static int explosion_indices[MAX_REFENTITIES];
	int transparent_model_num = 0;
	int masked_model_num = 0;
	int viewer_model_num = 0;
	int viewer_weapon_num = 0;
	int distortion_model_num = 0;
	int explosion_num = 0;

	int model_instance_idx = 0;
	int num_instanced_prim = 0; /* need to track this here to find lights */
	int instance_idx = 0;
	int mdxm_matrix_offset = 0;

	// cgame gives an id only to the ghoul2 entities. A model without id (the md3 view weapon,
	// items, props) then never finds its previous frame and has no motion vector. It gets
	// 8192 + its rank among the entities of the same model; cgame ids stay below 8192.
	{
		static unsigned short model_rank[MAX_MOD_KNOWN];
		Com_Memset( model_rank, 0, sizeof( model_rank ) );

		for ( i = 0; i < refdef->num_entities; i++ )
		{
			trRefEntity_t *entity = refdef->entities + i;

			if ( entity->e.reType != RT_MODEL || entity->e.id != 0 )
				continue;
			if ( entity->e.hModel < 0 || entity->e.hModel >= MAX_MOD_KNOWN )
				continue;

			entity->e.id = 8192 + ( ++model_rank[entity->e.hModel] & 0x1fff );
		}
	}

	for ( i = 0; i < refdef->num_entities; i++ )
	{
		if ( !r_drawentities->integer )
			break;

		trRefEntity_t *entity = refdef->entities + i;

		//
		// the weapon model must be handled special --
		// we don't want the hacked weapon position showing in
		// mirrors, because the true body position will already be drawn
		//
		//if ( ( entity->e.renderfx & RF_FIRST_PERSON ) && ( tr.viewParms.portalView != PV_NONE ) )
		//	return;

		switch ( entity->e.reType ) 
		{
			case RT_PORTALSURFACE:
				break;		// don't draw anything
			case RT_SPRITE:
			case RT_BEAM:
			case RT_ORIENTED_QUAD:
			case RT_ELECTRICITY:
			case RT_LINE:
			case RT_ORIENTEDLINE:
			case RT_CYLINDER:
			case RT_SABER_GLOW:
			case RT_ENT_CHAIN:
			// SP-only types. The tracer does not draw them yet; they must not reach the error below.
			case RT_LATHE:
			case RT_CLOUDS:
				break;
			case RT_MODEL:
				{
	#if 0
					if ( entity->e.renderfx & RF_FIRST_PERSON ){}
					else if ( entity->e.renderfx & RF_THIRD_PERSON ){}
					else 
	#endif
					model_t *model = R_GetModelByHandle( entity->e.hModel );

					if ( !model )
						continue;
					
					qboolean contains_transparent = qfalse;
					qboolean contains_masked = qfalse;

					// Not lit and not in the shadow: the distortion pass takes them after the others.
					if ( model->type != MOD_BRUSH && ( vk_rtx_is_distortion_entity( entity ) || vk_rtx_is_nodepth_entity( entity ) ) )
					{
						distortion_model_indices[distortion_model_num++] = i;
						break;
					}

					switch ( model->type )
					{
						case MOD_BRUSH:
							{
								process_bsp_entity( i, refdef, entity, model, &model_instance_idx );
							}
							break;
						case MOD_MESH:
						case MOD_MDXM:
						case MOD_BAD:
							{
								// A model around the camera gives its opaque and masked meshes to the viewer pass.
								const qboolean is_viewer_model = camera_inside_model( refdef, entity, model, i, mdxm_matrix_offset, vk.mdxm_matrices_shadow );

								// The primary rays hit the viewer pass, so a rigid model around the camera (misc_camera) stays out of the scene.
								if ( is_viewer_model && model->type == MOD_MESH )
									break;

								process_regular_entity( i, refdef, entity, model, qfalse, qfalse, &model_instance_idx, &instance_idx, &num_instanced_prim,
									is_viewer_model ? 0 : MESH_FILTER_OPAQUE, &contains_transparent, &contains_masked, &mdxm_matrix_offset, vk.mdxm_matrices_shadow );

								if (contains_transparent)
									transparent_model_indices[transparent_model_num++] = i;
								if (is_viewer_model)
									viewer_model_indices[viewer_model_num++] = i;
								else if (contains_masked)
									masked_model_indices[masked_model_num++] = i;

								if (model->num_light_polys > 0)
								{
									mat4_t transform;
									//const bool is_viewer_weapon = (entity->e.renderfx & RF_FIRST_PERSON) != 0;
									create_entity_matrix( transform, entity, qfalse );

									instance_model_lights( model->num_light_polys, model->light_polys, transform );
								}
							}
							break;
						default:
							break;
					}

					break;
				}
			default:
				Com_Error(ERR_DROP, "R_AddEntitySurfaces: Bad reType");
				break;
		}
	}

	upload_info->opaque_prim_count = num_instanced_prim;
	upload_info->transparent_prim_count = num_instanced_prim;

	// transparent
	for (int i = 0; i < transparent_model_num; i++)
	{
		const int entityNum = transparent_model_indices[i];
		trRefEntity_t *entity = refdef->entities + transparent_model_indices[i];

		model_t *model = R_GetModelByHandle( entity->e.hModel );
		process_regular_entity( entityNum, refdef, entity, model, qfalse, qfalse, &model_instance_idx, &instance_idx, &num_instanced_prim,
			MESH_FILTER_TRANSPARENT, NULL, NULL, &mdxm_matrix_offset, vk.mdxm_matrices_shadow );
	}

	upload_info->transparent_prim_count = num_instanced_prim - upload_info->transparent_prim_offset;
	upload_info->masked_prim_offset = num_instanced_prim;

	// masked
	for (int i = 0; i < masked_model_num; i++)
	{
		const int entityNum = masked_model_indices[i];
		trRefEntity_t *entity = refdef->entities + masked_model_indices[i];

		model_t *model = R_GetModelByHandle( entity->e.hModel );
		process_regular_entity( entityNum, refdef, entity, model, qfalse, qtrue, &model_instance_idx, &instance_idx, &num_instanced_prim,
			MESH_FILTER_MASKED, NULL, NULL, &mdxm_matrix_offset, vk.mdxm_matrices_shadow );
	}

	upload_info->masked_prim_count = num_instanced_prim - upload_info->masked_prim_offset;


	// viewer models
	upload_info->viewer_model_prim_offset = num_instanced_prim;

	for (int i = 0; i < viewer_model_num; i++)
	{
		const int entityNum = viewer_model_indices[i];
		trRefEntity_t *entity = refdef->entities + entityNum;

		model_t *model = R_GetModelByHandle( entity->e.hModel );
		process_regular_entity( entityNum, refdef, entity, model, qfalse, qfalse, &model_instance_idx, &instance_idx, &num_instanced_prim,
			MESH_FILTER_OPAQUE | MESH_FILTER_MASKED, NULL, NULL, &mdxm_matrix_offset, vk.mdxm_matrices_shadow );
	}

	upload_info->viewer_model_prim_count = num_instanced_prim - upload_info->viewer_model_prim_offset;

	// distortion surfaces
	upload_info->distortion_prim_offset = num_instanced_prim;

	for (int i = 0; i < distortion_model_num; i++)
	{
		const int entityNum = distortion_model_indices[i];
		trRefEntity_t *entity = refdef->entities + entityNum;

		model_t *model = R_GetModelByHandle( entity->e.hModel );
		process_regular_entity( entityNum, refdef, entity, model, qfalse, qfalse, &model_instance_idx, &instance_idx, &num_instanced_prim,
			MESH_FILTER_DISTORTION, NULL, NULL, &mdxm_matrix_offset, vk.mdxm_matrices_shadow );
	}

	upload_info->distortion_prim_count = num_instanced_prim - upload_info->distortion_prim_offset;

	// viewer weapons
	{
	}

	// The meshes are counted. Write the instance data of all of them.
	vk_rtx_run_instance_jobs( refdef );

	// bind
	upload_info->num_instances = instance_idx;
	upload_info->num_prims  = num_instanced_prim;

	memset(instance_buffer->model_current_to_prev, -1, sizeof(instance_buffer->model_current_to_prev));
	memset(instance_buffer->model_prev_to_current, -1, sizeof(instance_buffer->model_prev_to_current));
	memset(instance_buffer->mlight_prev_to_current, ~0u, sizeof(instance_buffer->mlight_prev_to_current));

	model_entity_id_count[entity_frame_num] = model_instance_idx;
	{
		// A repeated id makes the last match win, as in the serial double loop.
		entityMatchCtx_t match;
		match.buffer = instance_buffer;
		match.cur_ids = model_entity_ids[entity_frame_num];
		match.prev_ids = model_entity_ids[!entity_frame_num];
		match.cur_count = model_entity_id_count[entity_frame_num];
		match.prev_count = model_entity_id_count[!entity_frame_num];

		RTX_ParallelFor( match.cur_count, 16, match_current_to_prev_range, &match );
		RTX_ParallelFor( match.prev_count, 16, match_prev_to_current_range, &match );
	}

	mdxm_matrix_count[entity_frame_num] = mdxm_matrix_offset;
	if (mdxm_matrix_count[entity_frame_num] > 0)
	{
		// If we had some matrices previously...
		if (mdxm_matrix_count[!entity_frame_num] > 0)
		{
			// Copy over the previous frame MDXM matrices into an offset location in the current frame buffer
			memcpy(vk.mdxm_matrices_shadow + (mdxm_matrix_count[entity_frame_num]),
				vk.mdxm_matrices_prev, mdxm_matrix_count[!entity_frame_num] * sizeof(mat3x4_t));
		
			// Patch the previous matrix offsets to point at the new locations
			for (int i = 0; i < model_entity_id_count[entity_frame_num]; i++)
			{
				ModelInstance* instance = &instance_buffer->model_instances[i];
				if (instance->mdxm_matrix_offset_prev >= 0) {
					// Offset = current matrix count
					instance->mdxm_matrix_offset_prev += mdxm_matrix_count[entity_frame_num];
				}
			}
		}

		// Store the current matrices for the next frame
		memcpy(vk.mdxm_matrices_prev, vk.mdxm_matrices_shadow, mdxm_matrix_count[entity_frame_num] * sizeof(mat3x4_t));

		// Upload the current matrices to the staging buffer
		MDXMMatrixBuffer* mdxm_matrix_staging = (MDXMMatrixBuffer*)buffer_map(&vk.buf_mdxm_matrices_staging[vk.current_frame_index]);

		int total_matrix_count = (mdxm_matrix_count[entity_frame_num] + mdxm_matrix_count[!entity_frame_num]);
		memcpy(mdxm_matrix_staging, vk.mdxm_matrices_shadow, total_matrix_count * sizeof(mat3x4_t));

		buffer_unmap(&vk.buf_mdxm_matrices_staging[vk.current_frame_index]);
	}

	// Save the current model instances for the next frame
	memcpy(model_instances_prev, instance_buffer->model_instances, sizeof(ModelInstance) * model_entity_id_count[entity_frame_num]);
}

static void vk_rtx_process_render_feedback( ref_feedback_t *feedback, mnode_t *viewleaf, 
	qboolean *sun_visible, float *adapted_luminance )
{
	if (viewleaf)
		feedback->viewcluster = viewleaf->cluster;
	else
		feedback->viewcluster = -1;

	{
		static char const * unknown = "<unknown>";
		char const * view_material = unknown;
		char const * view_material_override = unknown;
		ReadbackBuffer readback;
		vk_rtx_readback( &readback );

		if ( readback.material != ~0u )
		{
			// Upstream resolves this through its PBR material table, which SP has no
			// equivalent of. The material index is the shader index, so go straight to
			// the shader: its name is what identifies the surface under the crosshair.
			const int material_id = readback.material & MATERIAL_INDEX_MASK;

			if ( material_id > 0 && material_id < tr.numShaders )
			{
				const shader_t *sh = tr.shaders[material_id];

				if ( sh )
				{
					view_material = sh->name;

					if ( sh->stages[0] && sh->stages[0]->active && sh->stages[0]->bundle[0].image[0] )
						view_material_override = sh->stages[0]->bundle[0].image[0]->imgName;

					// Point the crosshair at a surface and this names it, along with what
					// the tracer decided it was - the sort and content flags are what
					// RB_IsTransparent goes on.
					if ( pt_verbose->integer )
					{
						static int last_reported = -1;

						if ( material_id != last_reported )
						{
							last_reported = material_id;

								if ( tr.world && tr.world->cluster_light_offsets && (int)readback.cluster >= 0
									&& (int)readback.cluster < tr.world->numClusters )
									ri.Printf( PRINT_ALL, "rtx view: cluster %i, %i static lights in its list\n", (int)readback.cluster,
										tr.world->cluster_light_offsets[readback.cluster + 1] - tr.world->cluster_light_offsets[readback.cluster] );

							ri.Printf( PRINT_ALL, "rtx view: %s [%s]  sort %i  content 0x%08x  %s\n",
								sh->name, view_material_override, (int)sh->sort, sh->contentFlags,
								RB_IsTransparent( (shader_t *)sh ) ? "transparent" :
								RB_IsMasked( (shader_t *)sh ) ? "masked" : "opaque" );

							// The stages as the tracer composes them.
							const rtx_material_t *mat = vk_rtx_get_material( (uint32_t)material_id );
							for ( uint32_t s = 0; mat && s < mat->num_stages && s < MAX_RTX_STAGES; s++ )
							{
								const MaterialStage *st = &mat->stage[s];
								const shaderStage_t *pStage = sh->stages[s];

								ri.Printf( PRINT_ALL, "  stage %u: blend 0x%02x%s  mode %u  bundles %u  glow %i  emissive %i\n",
									s, st->blend & GLS_BLEND_BITS, ( st->blend & STAGE_BLEND_LIGHTMAP ) ? " (lightmap, skipped)" : "",
									st->tex_mode, st->tex_count + 1, pStage ? (int)pStage->glow : -1, (int)mat->glow_emissive );

								for ( uint32_t j = 0; pStage && j <= st->tex_count && j < NUM_TEXTURE_BUNDLES; j++ )
								{
									const textureBundle_t *b = &pStage->bundle[j];
									ri.Printf( PRINT_ALL, "    bundle %u: %s%s  rgbGen %u alphaGen %u color 0x%08x  tcMods %i\n",
										j, b->image[0] ? b->image[0]->imgName : "(none)", b->isLightmap ? " (lightmap)" : "",
										st->bundle[j].rgbGen, st->bundle[j].alphaGen & ~( BUNDLE_SKIP | BUNDLE_GLOW ), st->bundle[j].color, (int)b->numTexMods );
								}
							}
						}
					}
				}
			}
		}
		Q_strncpyz( feedback->view_material, view_material, sizeof( feedback->view_material ) );
		Q_strncpyz( feedback->view_material_override, view_material_override, sizeof( feedback->view_material_override ) );

		feedback->lookatcluster = readback.cluster;
		feedback->num_light_polys = 0;

		// The offsets only exist once R_PreparePT has run over this world.
		if ( tr.world && tr.world->cluster_light_offsets
			&& feedback->lookatcluster >= 0 && feedback->lookatcluster < tr.world->numClusters )
		{
			int* light_offsets = tr.world->cluster_light_offsets + feedback->lookatcluster;
			feedback->num_light_polys = light_offsets[1] - light_offsets[0];
		}

		VectorCopy( readback.hdr_color, feedback->hdr_color );

		*sun_visible = readback.sun_luminance > 0.f ? qtrue : qfalse;
		*adapted_luminance = readback.adapted_luminance;

		// Auto-exposure drives the whole tone curve off this one readback value, so when
		// the image looks flat this is the first thing to rule out.
		if ( pt_verbose->integer )
		{
			static int report_frame = 0;

			// frame_counter drives the A/B ping-pong of every history image. Printed as a
			// delta because what matters is that it advances by exactly one per frame -
			// anything else collapses the alternation and the gradient compares a seed
			// against itself.
			static uint64_t last_counter = 0;

			if ( ( report_frame++ & 63 ) == 0 )
			{
				ri.Printf( PRINT_ALL, "rtx exposure: adapted luminance %f  sun %f  hdr %f %f %f  frame %u (+%u over 64)\n",
					readback.adapted_luminance, readback.sun_luminance,
					readback.hdr_color[0], readback.hdr_color[1], readback.hdr_color[2],
					(uint32_t)vk.frame_counter, (uint32_t)( vk.frame_counter - last_counter ) );

				last_counter = vk.frame_counter;
			}
		}
	}
}

static float get_num_bounce_rays( void )
{
	if ( sun_pt_num_bounce_rays->value == 0.5f )
		return 0.5f;

	return MAX( 0, MIN( 2, round( sun_pt_num_bounce_rays->value ) ) );
}

// Returns qtrue when the main view is the same as in the previous frame.
static qboolean accumulation_view_unchanged( const trRefdef_t *refdef )
{
	static vec3_t		prev_vieworg;
	static matrix3_t	prev_viewaxis;
	static float		prev_fov_x, prev_fov_y;
	const float			epsilon = 1e-5f;
	qboolean			same = qtrue;
	int					i;

	for ( i = 0; i < 3; i++ )
	{
		if ( fabsf( refdef->vieworg[i] - prev_vieworg[i] ) > epsilon )
			same = qfalse;
	}

	for ( i = 0; i < 9; i++ )
	{
		if ( fabsf( refdef->viewaxis[i / 3][i % 3] - prev_viewaxis[i / 3][i % 3] ) > epsilon )
			same = qfalse;
	}

	if ( fabsf( refdef->fov_x - prev_fov_x ) > epsilon || fabsf( refdef->fov_y - prev_fov_y ) > epsilon )
		same = qfalse;

	// A change of the light list restarts the accumulation too.
	static int prev_light_generation;

	if ( prev_light_generation != RTX_LightEdit_Generation() )
	{
		prev_light_generation = RTX_LightEdit_Generation();
		same = qfalse;
	}

	VectorCopy( refdef->vieworg, prev_vieworg );
	Com_Memcpy( prev_viewaxis, refdef->viewaxis, sizeof( prev_viewaxis ) );
	prev_fov_x = refdef->fov_x;
	prev_fov_y = refdef->fov_y;

	return same;
}

static void evaluate_reference_mode( reference_mode_t *ref_mode, const trRefdef_t *refdef )
{
	const qboolean view_unchanged = accumulation_view_unchanged( refdef );

	if ( pt_accumulation_rendering->integer > 0 )
	{
		const int num_warmup_frames = 5;
		const int num_frames_to_accumulate = MAX( 1, pt_accumulation_rendering_framenum->integer );

		if ( !view_unchanged )
			num_accumulated_frames = 0;

		num_accumulated_frames++;

		// Print once, on the frame that completes the run.
		if ( num_accumulated_frames - num_warmup_frames == num_frames_to_accumulate )
			ri.Printf( PRINT_ALL, "rtx accumulation: %d frames done\n", num_frames_to_accumulate );

		ref_mode->enable_accumulation = qtrue;
		ref_mode->denoiser = DENOISER_NONE;
		ref_mode->temporal_blend_factor = 1.f / MIN( MAX( 1, num_accumulated_frames - num_warmup_frames ), num_frames_to_accumulate );
	}
	else
	{
		num_accumulated_frames = 0;

		ref_mode->enable_accumulation = qfalse;
		ref_mode->denoiser = vk_rtx_denoiser_from_cvar();
		ref_mode->temporal_blend_factor = 0.f;
	}

	ref_mode->num_bounce_rays = get_num_bounce_rays();
	ref_mode->reflect_refract = MIN( 10, MAX( 0, sun_pt_reflect_refract->integer ) );
}

static void vk_rtx_prepare_ubo( trRefdef_t *refdef, world_t *world, mnode_t *viewleaf, reference_mode_t *ref_mode, const vec3_t sky_matrix[3], qboolean render_world ) 
{
	float			P[16];
	float			V[16];
	float			raw_proj[16];
	float			viewport_proj[16];

	viewParms_t		*vkpt_refdef = &backEnd.viewParms;
	
	vkUniformRTX_t *ubo = &vk.uniform_buffer;
	memcpy( ubo->V_prev, ubo->V, sizeof(float) * 16 );
	memcpy( ubo->P_prev, ubo->P, sizeof(float) * 16 );
	memcpy( ubo->invP_prev, ubo->invP, sizeof(float) * 16 );
	ubo->cylindrical_hfov_prev = ubo->cylindrical_hfov;
	ubo->prev_taa_output_width = ubo->taa_output_width;
	ubo->prev_taa_output_height = ubo->taa_output_height;

	// matrices
	{
		create_projection_matrix( raw_proj, vkpt_refdef->zNear, vkpt_refdef->zFar, refdef->fov_x, refdef->fov_y );

		// In some cases (ex.: player setup), 'fd' will describe a viewport that is not full screen.
		// Simulate that with a projection matrix adjustment to avoid modifying the rendering code.

		Com_Memset( viewport_proj, 0, sizeof(float) * 16);

		viewport_proj[0] = (float)refdef->width / (float)vk.extent_unscaled.width;
		viewport_proj[12] = (float)(refdef->x * 2 + refdef->width - (int)vk.extent_unscaled.width) / (float)vk.extent_unscaled.width;
		viewport_proj[5] = (float)refdef->height / (float)vk.extent_unscaled.height;
		viewport_proj[13] = -(float)(refdef->y * 2 + refdef->height - (int)vk.extent_unscaled.height) / (float)vk.extent_unscaled.height;
		viewport_proj[10] = 1.f;
		viewport_proj[15] = 1.f;

		mult_matrix_matrix( P, viewport_proj, raw_proj );
	}
	create_view_matrix( V, refdef );
	memcpy( ubo->V, V, sizeof(float) * 16 );
	memcpy( ubo->P, P, sizeof(float) * 16 );
	inverse( V, ubo->invV );
	inverse( P, ubo->invP );

	float vfov = refdef->fov_y * (float)M_PI / 180.f;
	float unscaled_aspect = (float)vk.extent_unscaled.width / (float)vk.extent_unscaled.height;
	float rad_per_pixel;
	float fov_scale[2] = { 0.f, 0.f };

	switch (pt_projection->integer)
	{
	case PROJECTION_PANINI:
		fov_scale[1] = tanf(vfov / 2.f);
		fov_scale[0] = fov_scale[1] * unscaled_aspect;
		break;
	case PROJECTION_STEREOGRAPHIC:
		fov_scale[1] = tanf(vfov / 2.f * STEREOGRAPHIC_ANGLE);
		fov_scale[0] = fov_scale[1] * unscaled_aspect;
		break;
	case PROJECTION_CYLINDRICAL:
		rad_per_pixel = atanf(tanf(refdef->fov_y * (float)M_PI / 360.f) / ((float)vk.extent_unscaled.height * 0.5f));
		ubo->cylindrical_hfov = rad_per_pixel * (float)vk.extent_unscaled.width;
		break;
	case PROJECTION_EQUIRECTANGULAR:
		fov_scale[1] = vfov / 2.f;
		fov_scale[0] = fov_scale[1] * unscaled_aspect;
		break;
	case PROJECTION_MERCATOR:
		fov_scale[1] = logf(tanf((float)M_PI * 0.25f + (vfov / 2.f) * 0.5f));
		fov_scale[0] = fov_scale[1] * unscaled_aspect;
		break;
	}

	ubo->projection_fov_scale_prev[0] = ubo->projection_fov_scale[0];
	ubo->projection_fov_scale_prev[1] = ubo->projection_fov_scale[1];
	ubo->projection_fov_scale[0] = fov_scale[0];
	ubo->projection_fov_scale[1] = fov_scale[1];
	ubo->pt_projection = render_world ? pt_projection->integer : 0; // always use rectilinear projection when rendering the player setup view
	ubo->current_frame_idx = vk.frame_counter;	// tr.frameCount, vk.swapchain_image_index;
#ifdef USE_VK_IMGUI
	ubo->render_mode = vk_imgui_get_render_mode();
#else
	ubo->render_mode = 0;
#endif
	ubo->width = vk.extent_render.width;
	ubo->height = vk.extent_render.height;
	ubo->prev_width = vk.extent_render_prev.width;
	ubo->prev_height = vk.extent_render_prev.height;
	ubo->inv_width = 1.0f / (float)vk.extent_render.width;
	ubo->inv_height = 1.0f / (float)vk.extent_render.height;
	ubo->unscaled_width = vk.extent_unscaled.width;
	ubo->unscaled_height = vk.extent_unscaled.height;
	ubo->taa_image_width = vk.extent_taa_images.width;
	ubo->taa_image_height = vk.extent_taa_images.height;
	ubo->taa_output_width = vk.extent_taa_output.width;
	ubo->taa_output_height = vk.extent_taa_output.height;
	ubo->current_gpu_slice_width = vk.gpu_slice_width;
	ubo->prev_gpu_slice_width = vk.gpu_slice_width_prev;
	ubo->screen_image_width = vk.extent_screen_images.width;
	ubo->screen_image_height = vk.extent_screen_images.height;
	//ubo->water_normal_texture = water_normal_texture - r_images;
	ubo->pt_swap_checkerboard = 0;

	// The tone mapping as tone_mapping_apply does it: the tracer inverts it for the colours in
	// screen units (screen_to_hdr_color).
	{
		ubo->tonemap_hdr = vk.rtx_surf_is_hdr ? 1 : 0;
		ubo->tonemap_per_channel = Com_Clamp( 0.f, 1.f, tm_per_channel->value );
		ubo->tonemap_contrast = Com_Clamp( 0.5f, 2.f, tm_contrast->value );
	}
	ubo->restir_m_clamp = pt_restir_m_clamp->integer;
	if (pt_restir->integer == 3)
	{
		// adjust clamp for "very cheap" mode
		ubo->restir_m_clamp /= 2;
	}
	ubo->restir_m_clamp = MAX(ubo->restir_m_clamp, 1); // don't break things

	vk.extent_render_prev = vk.extent_render;
	vk.gpu_slice_width_prev = vk.gpu_slice_width;

	int camera_cluster_contents = viewleaf ? viewleaf->contents : 0;

	if ( camera_cluster_contents & CONTENTS_WATER )
		ubo->medium = MEDIUM_WATER;
	else if ( camera_cluster_contents & CONTENTS_SLIME )
		ubo->medium = MEDIUM_SLIME;
	else if ( camera_cluster_contents & CONTENTS_LAVA )
		ubo->medium = MEDIUM_LAVA;
	else
		ubo->medium = MEDIUM_NONE;

	ubo->time = refdef->time;
	ubo->num_static_lights = render_world ? world->num_light_polys : 0;

#define UBO_CVAR_DO( handle, default_value ) ubo->handle = sun_##handle->value;
	UBO_CVAR_LIST
#undef UBO_CVAR_DO
	
	if ( ref_mode->denoiser == DENOISER_NONE ) 
	{
		// disable fake specular because it is not supported without denoiser, and the result
		// looks too dark with it missing
		ubo->pt_fake_roughness_threshold = 1.f;

		// swap the checkerboard fields every frame in reference or noisy mode to accumulate 
		// both reflection and refraction in every pixel
		ubo->pt_swap_checkerboard = (vk.frame_counter & 1);

		if (ref_mode->enable_accumulation)
		{
			//ubo->pt_texture_lod_bias = -log2(sqrt(get_accumulation_rendering_framenum()));

			// disable the other stabilization hacks
			ubo->pt_specular_anti_flicker = 0.f;
			ubo->pt_sun_bounce_range = 10000.f;
			ubo->pt_ndf_trim = 1.f;

			// the reservoirs of ReSTIR GI correlate the frames
			ubo->pt_restir_gi = 0.f;
		}
	} 
	else if ( vk.effective_aa_mode == AA_MODE_UPSCALE )
	{
		// adjust texture LOD bias to the resolution scale, i.e. use negative bias if scale is < 100
		//float resolution_scale = (drs_effective_scale != 0) ? (float)drs_effective_scale : (float)scr_viewsize->integer;
		//resolution_scale *= 0.01f;
		//resolution_scale = clamp(resolution_scale, 0.1f, 1.f);
		//ubo->pt_texture_lod_bias = cvar_pt_texture_lod_bias->value + log2f(resolution_scale);
	}

	{
		// figure out if DoF should be enabled in the current rendering mode
		qboolean enable_dof = qtrue;

		switch ( pt_dof->integer) 
		{
			case 0: enable_dof = qfalse; break;
			case 1: enable_dof = ref_mode->enable_accumulation; break;
			case 2: enable_dof = ref_mode->denoiser != DENOISER_NONE ? qfalse : qtrue; break;
			default: enable_dof = qtrue; break;
		}
		
		// DoF does not make physical sense with the cylindrical projection
		if ( pt_projection->integer != 0) 
			enable_dof = qfalse;
		
		// if DoF should not be enabled, make the aperture size zero
		if (!enable_dof)
			ubo->pt_aperture = 0.f;
	}

	// number of polygon vertices must be an integer
	ubo->pt_aperture_type = roundf(ubo->pt_aperture_type);

	ubo->temporal_blend_factor = ref_mode->temporal_blend_factor;	
	ubo->pt_denoiser_flags = vk_rtx_get_denoiser( ref_mode->denoiser )->flags;

	// The fake specular needs a denoiser that reconstructs it. Without it the result looks too dark.
	if ( !( ubo->pt_denoiser_flags & DENOISER_FLAG_FAKE_SPECULAR ) )
		ubo->pt_fake_roughness_threshold = 1.f;
	ubo->flt_taa = vk.effective_aa_mode;
	ubo->pt_num_bounce_rays = ref_mode->num_bounce_rays;
	ubo->pt_reflect_refract = ref_mode->reflect_refract;

	if ( ref_mode->num_bounce_rays < 1.f )
		ubo->pt_specular_mis = 0; // disable MIS if there are no specular rays

	ubo->pt_min_log_sky_luminance = exp2f(ubo->pt_min_log_sky_luminance);
	ubo->pt_max_log_sky_luminance = exp2f(ubo->pt_max_log_sky_luminance);

	//memcpy(ubo->cam_pos, backEnd.viewParms.ori.origin, sizeof(float) * 3);
	memcpy(ubo->cam_pos, refdef->vieworg, sizeof(float) * 3);
	ubo->cluster_debug_index = vk.cluster_debug_index;
	ubo->pad1[0] = refdef->doLAGoggles ? 1.0f : 0.0f;	// LA goggles flag, read by composite_color
	ubo->pad1[1] = r_znear->value;						// near plane, the t_min of the primary rays

	vk_rtx_denoisers_prepare_ubo( ubo );
	vk_rtx_taa_prepare_ubo( ubo );

	// Reservoirs of another resolution do not fit the pixels.
	ubo->restir_gi_history_valid = ( restir_gi_history_valid && ubo->prev_width == ubo->width && ubo->prev_height == ubo->height ) ? 1 : 0;

	if ( vk.effective_aa_mode == AA_MODE_UPSCALE )
	{
		int taa_index = (int)(vk.frame_counter % NUM_TAA_SAMPLES);
		ubo->sub_pixel_jitter[0] = vk.taa_samples[taa_index][0];
		ubo->sub_pixel_jitter[1] = vk.taa_samples[taa_index][1];
	}
	else
	{
		ubo->sub_pixel_jitter[0] = 0.f;
		ubo->sub_pixel_jitter[1] = 0.f;
	}

	memset( ubo->environment_rotation_matrix, 0, sizeof(ubo->environment_rotation_matrix) );
	VectorCopy( sky_matrix[0], ubo->environment_rotation_matrix + 0 );
	VectorCopy( sky_matrix[1], ubo->environment_rotation_matrix + 4 );
	VectorCopy( sky_matrix[2], ubo->environment_rotation_matrix + 8 );

	ubo->pt_cameras = 0;
	//ubo->num_cameras = 0;
}

// The cloak passes of the frame and whether the distortion pass runs.
static void vk_rtx_prepare_distortion_ubo( vkUniformRTX_t *ubo, const EntityUploadInfo *upload_info )
{
	float *passes[2] = { ubo->distortion_cloak_pass0, ubo->distortion_cloak_pass1 };

	ubo->distortion_surfaces = upload_info->distortion_prim_count ? 1 : 0;

	for ( int i = 0; i < 2; i++ )
	{
		vec4_t params;
		uint32_t state_bits;

		Com_Memset( passes[i], 0, 4 * sizeof( float ) );

		if ( ubo->distortion_surfaces && ComputeDistortionPass( i, params, &state_bits ) )
		{
			VectorCopy( params, passes[i] );
			passes[i][3] = (float)( state_bits & GLS_BLEND_BITS );
		}
	}
}

// The fog brushes and the global fog of the map, as boxes. The shader gets k2 = ln(255) / depthForOpaque^2:
// the rasterizer fog is T = exp(-(d * sqrt(ln 255) / depthForOpaque)^2).
static void vk_rtx_prepare_fog_ubo( vkUniformRTX_t *ubo, const trRefdef_t *refdef, qboolean render_world )
{
	int count = 0;

	Com_Memset( ubo->fog_volumes, 0, sizeof( ubo->fog_volumes ) );

	if ( !tr.world || !render_world )
		return;

	// The LA goggles fog replaces every other fog: one box around the world.
	if ( refdef->doLAGoggles )
	{
		const fog_t *fog = backEndDataBE->fogs + backEndDataBE->numFogs;
		const float depth = fog->parms.depthForOpaque < 1.0f ? 1.0f : fog->parms.depthForOpaque;

		VectorSet( ubo->fog_volumes[0], -65536.0f, -65536.0f, -65536.0f );
		ubo->fog_volumes[0][3] = logf( 255.0f ) / ( depth * depth );
		VectorSet( ubo->fog_volumes[1], 65536.0f, 65536.0f, 65536.0f );
		VectorCopy( fog->color, ubo->fog_volumes[2] );
		return;
	}

	for ( int i = 1; i < backEndDataBE->numFogs && count < MAX_FOG_VOLUMES; i++ )
	{
		const fog_t *fog = backEndDataBE->fogs + i;
		const float depth = fog->parms.depthForOpaque < 1.0f ? 1.0f : fog->parms.depthForOpaque;
		float (*v)[4] = ubo->fog_volumes + count * 3;

		VectorCopy( fog->bounds[0], v[0] );
		v[0][3] = logf( 255.0f ) / ( depth * depth );
		VectorCopy( fog->bounds[1], v[1] );
		VectorCopy( fog->color, v[2] );
		count++;
	}
}

static void
update_mlight_prev_to_current(void)
{
	light_entity_id_count[entity_frame_num] = num_model_lights;
	for(int i = 0; i < light_entity_id_count[entity_frame_num]; i++) {
		entity_hash_t hash = *(entity_hash_t*)&light_entity_ids[entity_frame_num][i];
		if(hash.entity == 0u) continue;
		for(int j = 0; j < light_entity_id_count[!entity_frame_num]; j++) {
			if(light_entity_ids[entity_frame_num][i] == light_entity_ids[!entity_frame_num][j]) {
				vk.uniform_instance_buffer.mlight_prev_to_current[j] = i;
				break;
			}
		}
	}
}

static void vk_rtx_setup_rt_pipeline( VkCommandBuffer cmd_buf, VkPipelineBindPoint bind_point, uint32_t index )
{
	// https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/8038
	qvkCmdBindPipeline( cmd_buf, bind_point, vk.rt_pipelines[index] );

	VkDescriptorSet desc_sets[] = {
		vk.rt_descriptor_set[vk.current_frame_index].set,
        vk_rtx_get_current_desc_set_textures(),
		vk.imageDescriptor.set,
        vk.desc_set_ubo,
		vk.desc_set_vertex_buffer[vk.current_frame_index].set,
	};

	qvkCmdBindDescriptorSets( cmd_buf, bind_point,
			vk.rt_pipeline_layout, 0, ARRAY_LEN(desc_sets), desc_sets, 0, 0);
}

void vk_rtx_dispatch_rays( VkCommandBuffer cmd_buf, uint32_t pipeline_index, pt_push_constants_t push, uint32_t width, uint32_t height, uint32_t depth )
{
	vk_rtx_setup_rt_pipeline( cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_index );

	assert( vk.buf_shader_binding_table.address );

	qvkCmdPushConstants( cmd_buf, vk.rt_pipeline_layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(push), &push );	

	uint32_t sbt_offset = SBT_ENTRIES_PER_PIPELINE * pipeline_index * vk.shaderGroupBaseAlignment;

	VkStridedDeviceAddressRegionKHR raygen;
	raygen.deviceAddress = vk.buf_shader_binding_table.address + sbt_offset;
	raygen.stride = vk.shaderGroupBaseAlignment;
	raygen.size = vk.shaderGroupBaseAlignment;

	VkStridedDeviceAddressRegionKHR miss_and_hit;
	miss_and_hit.deviceAddress = vk.buf_shader_binding_table.address + sbt_offset;
	miss_and_hit.stride = vk.shaderGroupBaseAlignment;
	miss_and_hit.size = (VkDeviceSize)vk.shaderGroupBaseAlignment * SBT_ENTRIES_PER_PIPELINE;

	VkStridedDeviceAddressRegionKHR callable;
	callable.deviceAddress = NULL;
	callable.stride = 0;
	callable.size = 0;

	qvkCmdTraceRaysKHR( cmd_buf,
		&raygen,
		&miss_and_hit,
		&miss_and_hit,
		&callable,
		width, height, depth );
}

// A launch over the two checkerboard fields of the G-buffer.
static void dispatch_rays( VkCommandBuffer cmd_buf, uint32_t pipeline_index, pt_push_constants_t push, uint32_t height ) 
{
	vk_rtx_dispatch_rays( cmd_buf, pipeline_index, push, vk.extent_render.width / 2, height, vk.device_count == 1 ? 2 : 1 );
}

static void vk_rtx_trace_primary_rays( VkCommandBuffer cmd_buf )
{
	int frame_idx = vk.frame_counter & 1;

	BUFFER_BARRIER( cmd_buf,
		VK_ACCESS_TRANSFER_READ_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		vk.buf_readback.buffer,
		0,
		VK_WHOLE_SIZE
	);

	BEGIN_PERF_MARKER( cmd_buf, PROFILER_PRIMARY_RAYS );

	pt_push_constants_t push;
	push.gpu_index = -1; // vk.device_count == 1 ? -1 : i;
	push.bounce = 0;	
		
	dispatch_rays( cmd_buf, PIPELINE_PRIMARY_RAYS, push, vk.extent_render.height );

	END_PERF_MARKER( cmd_buf, PROFILER_PRIMARY_RAYS );

	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_VISBUF_PRIM_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_VISBUF_BARY_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_TRANSPARENT] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_FX] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_DISTORT] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_MOTION] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_SHADING_POSITION] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_VIEW_DIRECTION] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_THROUGHPUT] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_BOUNCE_THROUGHPUT] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_GODRAYS_THROUGHPUT_DIST] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_BASE_COLOR_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_METALLIC_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_CLUSTER_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_VIEW_DEPTH_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_NORMAL_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_RNG_SEED_A + frame_idx] );
#ifdef USE_RTX_INSPECT_TANGENTS
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_TANGENT_A + frame_idx] );
#endif
}

static void vk_rtx_trace_reflections( VkCommandBuffer cmd_buf, int bounce ) 
{
	int frame_idx = vk.frame_counter & 1;

	uint32_t pipeline_index = (bounce == 0) ? PIPELINE_REFLECT_REFRACT_1 : PIPELINE_REFLECT_REFRACT_2;

	pt_push_constants_t push;
	push.gpu_index = -1; // vk.device_count == 1 ? -1 : i;
	push.bounce = bounce;

	dispatch_rays( cmd_buf, pipeline_index, push, vk.extent_render.height );

	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_TRANSPARENT] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_MOTION] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_SHADING_POSITION] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_VIEW_DIRECTION] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_THROUGHPUT] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_GODRAYS_THROUGHPUT_DIST] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_BASE_COLOR_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_METALLIC_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_CLUSTER_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_VIEW_DEPTH_A + frame_idx] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_NORMAL_A + frame_idx] );
#ifdef USE_RTX_INSPECT_TANGENTS
	//BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_TANGENT_A + frame_idx] );
#endif
}

static void vk_rxt_trace_lighting( VkCommandBuffer cmd_buf, float num_bounce_rays )
{
	// direct lighting
	BEGIN_PERF_MARKER(cmd_buf, PROFILER_DIRECT_LIGHTING);

	for ( int i = 0; i < vk.device_count; i++ )
	{
		uint32_t pipeline_index = (pt_caustics->value != 0) ? PIPELINE_DIRECT_LIGHTING_CAUSTICS : PIPELINE_DIRECT_LIGHTING;

		pt_push_constants_t push;
		push.gpu_index = vk.device_count == 1 ? -1 : i;
		push.bounce = 0;

		dispatch_rays( cmd_buf, pipeline_index, push, vk.extent_render.height );
	}

	END_PERF_MARKER(cmd_buf, PROFILER_DIRECT_LIGHTING);

	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_SH] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_COCG] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_HF] );
	BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_SPEC] );

	if ( pt_restir->value != 0 ) 
	{
		int frame_idx = vk.frame_counter & 1;
		BARRIER_COMPUTE_WRITE(cmd_buf, vk.img_rtx[RTX_IMG_PT_RESTIR_A + frame_idx]);
	}

	BUFFER_BARRIER( cmd_buf,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_TRANSFER_READ_BIT,
		vk.buf_readback.buffer,
		0,
		VK_WHOLE_SIZE
	);

	// indirect lighting
	BEGIN_PERF_MARKER( cmd_buf, PROFILER_INDIRECT_LIGHTING );

	if ( num_bounce_rays > 0 ) 
	{
		for (int i = 0; i < vk.device_count; i++)
		{
			int height;
			if (num_bounce_rays == 0.5f)
				height = vk.extent_render.height / 2;
			else
				height = vk.extent_render.height;	

			for (int bounce_ray = 0; bounce_ray < (int)ceilf(num_bounce_rays); bounce_ray++)
			{
				uint32_t pipeline_index = (bounce_ray == 0) ? PIPELINE_INDIRECT_LIGHTING_FIRST : PIPELINE_INDIRECT_LIGHTING_SECOND;

				pt_push_constants_t push;
				push.gpu_index = vk.device_count == 1 ? -1 : i;
				push.bounce = 0;
		
				dispatch_rays( cmd_buf, pipeline_index, push, height );

				BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_SH] );
				BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_COCG] );
				BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_HF] );
				BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_SPEC] );
				BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_BOUNCE_THROUGHPUT] );
			}
		}

		if ( vk.uniform_buffer.pt_restir_gi != 0.f )
		{
			int frame_idx = vk.frame_counter & 1;
			BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_RESTIR_GI_POS_A + frame_idx] );
			BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_RESTIR_GI_RAD_A + frame_idx] );
		}

		// The spatial step of ReSTIR GI shades the reservoirs of the bounce pass. It needs the G-buffer
		// that the bounce pass leaves in place with one bounce ray.
		if ( vk.uniform_buffer.pt_restir_gi >= 2.f && num_bounce_rays == 1.f )
		{
			int frame_idx = vk.frame_counter & 1;
			BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_RESTIR_GI_POS_A + frame_idx] );
			BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_RESTIR_GI_RAD_A + frame_idx] );

			for ( int i = 0; i < vk.device_count; i++ )
			{
				pt_push_constants_t push;
				push.gpu_index = vk.device_count == 1 ? -1 : i;
				push.bounce = 0;

				dispatch_rays( cmd_buf, PIPELINE_RESTIR_GI_SPATIAL, push, vk.extent_render.height );
			}

			BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_SH] );
			BARRIER_COMPUTE_WRITE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_COCG] );
		}
	}

	END_PERF_MARKER( cmd_buf, PROFILER_INDIRECT_LIGHTING );
}


// Names for pt_debug_image, generated from the same list the images themselves are.
static const char * const rtx_image_names[] = {
#define IMG_DO(_name, ...) #_name,
	LIST_IMAGES
	LIST_IMAGES_A_B
#undef IMG_DO
};

// Resolves pt_debug_image, which takes either an index or an image name. A name matches
// case-insensitively, whole or as a substring as long as it picks out exactly one image,
// so "motion" is enough for PT_MOTION.
static uint32_t vk_rtx_resolve_debug_image( const char *arg )
{
	uint32_t i, found = 0, matches = 0;

	if ( arg[0] >= '0' && arg[0] <= '9' )
	{
		const int index = atoi( arg );

		if ( index <= 0 || index >= (int)ARRAY_LEN( rtx_image_names ) ) {
			ri.Printf( PRINT_ALL, "pt_debug_image: %i is out of range, %u images available\n",
				index, (uint32_t)ARRAY_LEN( rtx_image_names ) );
			return RTX_IMG_TAA_OUTPUT;
		}

		return (uint32_t)index;
	}

	for ( i = 0; i < ARRAY_LEN( rtx_image_names ); i++ ) {
		if ( !Q_stricmp( rtx_image_names[i], arg ) )
			return i;
	}

	for ( i = 0; i < ARRAY_LEN( rtx_image_names ); i++ ) {
		if ( Q_stristr( rtx_image_names[i], arg ) ) {
			found = i;
			matches++;
		}
	}

	if ( matches == 1 )
		return found;

	if ( matches == 0 ) {
		ri.Printf( PRINT_ALL, "pt_debug_image: no image matching '%s' - try pt_images\n", arg );
		return RTX_IMG_TAA_OUTPUT;
	}

	ri.Printf( PRINT_ALL, "pt_debug_image: '%s' matches %u images:\n", arg, matches );

	for ( i = 0; i < ARRAY_LEN( rtx_image_names ); i++ ) {
		if ( Q_stristr( rtx_image_names[i], arg ) )
			ri.Printf( PRINT_ALL, "  %s\n", rtx_image_names[i] );
	}

	return RTX_IMG_TAA_OUTPUT;
}

// Which of the tracer's images the final blit puts on screen. Anything other than the
// tone-mapped output is a debug view: the blit only converts between float formats, so
// the integer images (the visibility buffer, the cluster ids) will make the validation
// layer complain rather than display.
uint32_t vk_rtx_debug_image_index( void )
{
	static char resolved_from[MAX_QPATH] = { 0 };
	static uint32_t resolved = RTX_IMG_TAA_OUTPUT;

	if ( !pt_debug_image )
		return RTX_IMG_TAA_OUTPUT;

	// Only re-resolve when the string changes; this runs every frame.
	if ( strcmp( resolved_from, pt_debug_image->string ) )
	{
		Q_strncpyz( resolved_from, pt_debug_image->string, sizeof( resolved_from ) );

		if ( !resolved_from[0] || !strcmp( resolved_from, "0" ) )
			resolved = RTX_IMG_TAA_OUTPUT;
		else
		{
			resolved = vk_rtx_resolve_debug_image( resolved_from );

			if ( resolved != RTX_IMG_TAA_OUTPUT )
				ri.Printf( PRINT_ALL, "pt_debug_image %u: %s (%ux%u)\n", resolved,
					rtx_image_names[resolved],
					vk.img_rtx[resolved].extent.width, vk.img_rtx[resolved].extent.height );
		}
	}

	// The images past RTX_IMG_NUM_STATIC come in A/B pairs at adjacent indices, and the
	// descriptor set swaps which physical one the shader calls A on odd frames. Follow
	// that swap, so "..._A" always shows what the shader wrote as A this frame rather
	// than a fixed physical image alternating between two roles - otherwise the view
	// flickers between two states on its own and hides whatever it was meant to show.
	if ( resolved >= RTX_IMG_NUM_STATIC && ( vk.frame_counter & 1 ) )
		resolved = RTX_IMG_NUM_STATIC + ( ( resolved - RTX_IMG_NUM_STATIC ) ^ 1 );

	return resolved;
}

void vk_rtx_list_debug_images_f( void )
{
	uint32_t i;

	ri.Printf( PRINT_ALL, "%u path tracer images:\n", (uint32_t)ARRAY_LEN( rtx_image_names ) );

	for ( i = 0; i < ARRAY_LEN( rtx_image_names ); i++ )
		ri.Printf( PRINT_ALL, "%3u  %-34s %ux%u\n", i, rtx_image_names[i],
			vk.img_rtx[i].extent.width, vk.img_rtx[i].extent.height );
}
static VkResult vkpt_final_blit_simple( VkCommandBuffer cmd_buf )
{
	VkImageSubresourceRange subresource_range;
	subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = 1;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;

	IMAGE_BARRIER(cmd_buf,
		vk.color_image,
		subresource_range,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

#ifdef USE_VK_IMGUI
	VkImage output_img = vk_imgui_get_rtx_render_mode();
	uint32_t output_idx = RTX_IMG_TAA_OUTPUT;
#else
	// pt_debug_image puts any of the tracer's intermediate images on screen in place of
	// the tone-mapped result. "pt_debug_image list" prints the names.
	uint32_t output_idx = vk_rtx_debug_image_index();
	VkImage output_img = vk.img_rtx[output_idx].handle;
#endif

	IMAGE_BARRIER( cmd_buf,
		output_img,
		subresource_range,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	);

	// Most of the intermediate images are smaller than the TAA output - the gradient ones
	// are a third of it - so the blit has to scale from each image's own size.
	VkOffset3D blit_size;
	blit_size.x = ( output_idx == RTX_IMG_TAA_OUTPUT ) ? vk.extent_taa_output.width  : vk.img_rtx[output_idx].extent.width;
	blit_size.y = ( output_idx == RTX_IMG_TAA_OUTPUT ) ? vk.extent_taa_output.height : vk.img_rtx[output_idx].extent.height;
	blit_size.z = 1;

	VkOffset3D blit_size_unscaled;
	blit_size_unscaled.x = vk.extent_unscaled.width;
	blit_size_unscaled.y = vk.extent_unscaled.height;
	blit_size_unscaled.z = 1;

	VkImageBlit img_blit;
	Com_Memset( &img_blit, 0, sizeof(VkImageBlit) );
	img_blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	img_blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	img_blit.srcOffsets[1] = blit_size;
	img_blit.dstOffsets[1] = blit_size_unscaled;

	qvkCmdBlitImage( cmd_buf,
		output_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &img_blit, VK_FILTER_NEAREST );

	IMAGE_BARRIER( cmd_buf,
		vk.color_image,
		subresource_range,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		0,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	IMAGE_BARRIER( cmd_buf,
		output_img,
		subresource_range,
		VK_ACCESS_TRANSFER_READ_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	);

	return VK_SUCCESS;
}

static VkResult
vkpt_light_buffer_upload_staging( VkCommandBuffer cmd_buf )
{
	vkbuffer_t *staging = vk.buf_light_staging + vk.current_frame_index;

	assert( !staging->is_mapped );

	VkBufferCopy copyRegion = { 0, 0, sizeof(LightBuffer) };

	qvkCmdCopyBuffer( cmd_buf, staging->buffer, vk.buf_light.buffer, 1, &copyRegion );

	int buffer_idx = vk.frame_counter % 3;
	if ( vk.buf_light_stats[buffer_idx].buffer )
	{
		qvkCmdFillBuffer(cmd_buf, vk.buf_light_stats[buffer_idx].buffer, 0, vk.buf_light_stats[buffer_idx].size, 0);
	}

	return VK_SUCCESS;
}

static VkResult
vkpt_mdxm_matrix_buffer_upload_staging( VkCommandBuffer cmd_buf )
{
	vkbuffer_t* staging = vk.buf_mdxm_matrices_staging + vk.current_frame_index;

	assert(!staging->is_mapped);

	VkBufferCopy copyRegion = { 0, 0, sizeof(MDXMMatrixBuffer) };

	qvkCmdCopyBuffer(cmd_buf, staging->buffer, vk.buf_mdxm_matrices.buffer, 1, &copyRegion);
	
	return VK_SUCCESS;
}

static void vk_rtx_end_command_buffer( 
	VkCommandBuffer cmd_buf, 
	int wait_semaphore_count, VkSemaphore *wait_semaphores, VkPipelineStageFlags *wait_stages,
	int signal_semaphore_count, VkSemaphore *signal_semaphores ) 
{
	VkSubmitInfo submit_info;

	VK_CHECK( qvkEndCommandBuffer( cmd_buf ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;

	if ( !ri.VK_IsMinimized() ) 
	{			
		submit_info.waitSemaphoreCount		= wait_semaphore_count;
		submit_info.pWaitSemaphores			= wait_semaphores;
		submit_info.pWaitDstStageMask		= wait_stages;
		submit_info.signalSemaphoreCount	= signal_semaphore_count;
		submit_info.pSignalSemaphores		= signal_semaphores;
	}
	else 
	{
		submit_info.waitSemaphoreCount		= 0;
		submit_info.pWaitSemaphores			= NULL;
		submit_info.pWaitDstStageMask		= NULL;
		submit_info.signalSemaphoreCount	= 0;
		submit_info.pSignalSemaphores		= NULL;
	}

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, VK_NULL_HANDLE ) );
};

static VkResult vkpt_pt_update_descripter_set_bindings( int idx )
{
	VkAccelerationStructureKHR tlas[TLAS_COUNT];
	Com_Memset(tlas, 0, sizeof(VkAccelerationStructureKHR) * TLAS_COUNT);
	tlas[TLAS_INDEX_GEOMETRY] = vk.tlas_geometry[idx].accel;
	tlas[TLAS_INDEX_EFFECTS] = vk.tlas_effects[idx].accel;

	vk_rtx_bind_descriptor_as( &vk.rt_descriptor_set[idx], RAY_GEN_ACCEL_STRUCTURE_BINDING_IDX, VK_SHADER_STAGE_RAYGEN_BIT_KHR, tlas, TLAS_COUNT );

	VkBufferView particle_color_buffer_view = get_transparency_particle_color_buffer_view();
	VkBufferView beam_color_buffer_view		= get_transparency_beam_color_buffer_view();
	VkBufferView sprite_info_buffer_view	= get_transparency_sprite_info_buffer_view();
	VkBufferView beam_intersect_buffer_view = get_transparency_beam_intersect_buffer_view();

	vk_rtx_bind_descriptor_buffer_view( &vk.rt_descriptor_set[idx], RAY_GEN_PARTICLE_COLOR_BUFFER_BINDING_IDX,	VK_SHADER_STAGE_ANY_HIT_BIT_KHR,			particle_color_buffer_view );
	vk_rtx_bind_descriptor_buffer_view( &vk.rt_descriptor_set[idx], RAY_GEN_BEAM_COLOR_BUFFER_BINDING_IDX,		VK_SHADER_STAGE_ANY_HIT_BIT_KHR,			beam_color_buffer_view );
	vk_rtx_bind_descriptor_buffer_view( &vk.rt_descriptor_set[idx], RAY_GEN_SPRITE_INFO_BUFFER_BINDING_IDX,		VK_SHADER_STAGE_ANY_HIT_BIT_KHR,			sprite_info_buffer_view );
	vk_rtx_bind_descriptor_buffer_view( &vk.rt_descriptor_set[idx], RAY_GEN_BEAM_INTERSECT_BUFFER_BINDING_IDX,	VK_SHADER_STAGE_INTERSECTION_BIT_KHR,		beam_intersect_buffer_view );

	vk_rtx_update_descriptor( &vk.rt_descriptor_set[idx] );


	return VK_SUCCESS;
}

static void vk_begin_trace_rays( world_t &worldData, trRefdef_t *refdef, reference_mode_t *ref_mode, 
	vkUniformRTX_t *ubo, drawSurf_t *drawSurfs, int numDrawSurfs, 
	float *shadowmap_view_proj, qboolean god_rays_enabled, qboolean render_world, 
	const EntityUploadInfo *upload_info ) 
{
	VkSemaphore					transfer_semaphores;
	VkSemaphore					trace_semaphores;
	VkSemaphore					prev_trace_semaphores;
	VkPipelineStageFlags		wait_stages;
	//VkCommandBuffer				transfer_cmd_buf;
	//VkCommandBufferBeginInfo	begin_info;
	uint32_t device_indices[2];
	uint32_t all_device_mask = (1 << vk.device_count) - 1;
	bool *prev_trace_signaled = &vk.tess[(vk.current_frame_index - 1) % NUM_COMMAND_BUFFERS].semaphores.trace_signaled;
	bool *curr_trace_signaled = &vk.tess[vk.current_frame_index].semaphores.trace_signaled;
	const denoiser_t *denoiser = vk_rtx_get_denoiser( ref_mode->denoiser );

	{
		// Transfer the light buffer from staging into device memory.
		// Previous frame's tracing still uses device memory, so only do the copy after that is finished.
			
		VkCommandBuffer transfer_cmd_buf = vkpt_begin_command_buffer(&vk.cmd_buffers_transfer);

		vkpt_light_buffer_upload_staging( transfer_cmd_buf );
		vkpt_mdxm_matrix_buffer_upload_staging( transfer_cmd_buf );

		for ( int gpu = 0; gpu < vk.device_count; gpu++ )	// multi-gpu not implemented
		{
			device_indices[gpu]		= gpu;
			transfer_semaphores		= vk.tess[vk.current_frame_index].semaphores.transfer_finished;
			trace_semaphores		= vk.tess[vk.current_frame_index].semaphores.trace_finished;
			prev_trace_semaphores	= vk.tess[(vk.current_frame_index - 1) % NUM_COMMAND_BUFFERS].semaphores.trace_finished;
			wait_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}

		vkpt_submit_command_buffer(
			transfer_cmd_buf, 
			vk.queue_transfer, 
			all_device_mask, 
			(*prev_trace_signaled) ? vk.device_count : 0, &prev_trace_semaphores, &wait_stages, device_indices, 
			vk.device_count, &transfer_semaphores, device_indices, 
			VK_NULL_HANDLE);

		*prev_trace_signaled = false;
	}

	{
		VkCommandBuffer trace_cmd_buf = vkpt_begin_command_buffer(&vk.cmd_buffers_graphics);

		vk_gpu_timer_write( trace_cmd_buf, VKP_TS_START, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT );

		update_transparency(trace_cmd_buf, refdef, ubo->V);

		// Copy the UBO contents from the staging buffer.
		// Actual contents are uploaded to the staging UBO below, right before executing the command buffer.
		vkpt_uniform_buffer_copy_from_staging( trace_cmd_buf );
		//VK_CHECK( vkpt_uniform_buffer_update( trace_cmd_buf ) );

		BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_UPDATE_ENVIRONMENT );
		if ( render_world )
		{
			vkpt_physical_sky_record_cmd_buffer( trace_cmd_buf );
		}
		END_PERF_MARKER( trace_cmd_buf, PROFILER_UPDATE_ENVIRONMENT );

		if ( render_world )
			vk_rtx_rebuild_world_deforms( trace_cmd_buf );

		BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_INSTANCE_GEOMETRY );
		vkpt_instance_geometry( trace_cmd_buf, upload_info->num_instances, qfalse );
		END_PERF_MARKER( trace_cmd_buf, PROFILER_INSTANCE_GEOMETRY );

		BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_BVH_UPDATE );
		vkpt_pt_create_all_dynamic( trace_cmd_buf, vk.current_frame_index, upload_info );
		vkpt_pt_create_toplevel( trace_cmd_buf, vk.current_frame_index, upload_info );
		vkpt_pt_update_descripter_set_bindings( vk.current_frame_index );
		END_PERF_MARKER( trace_cmd_buf, PROFILER_BVH_UPDATE );

		vk_gpu_timer_write( trace_cmd_buf, VKP_TS_ACCEL_END, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );

		BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_SHADOW_MAP );
		if ( god_rays_enabled )
		{
			vk_rtx_shadow_map_render( trace_cmd_buf, worldData, shadowmap_view_proj, 
				tr.world->geometry.world_static.geom_opaque.prim_offsets[0] * 3,
				tr.world->geometry.world_static.geom_opaque.prim_counts[0] * 3,
				0,
				upload_info->opaque_prim_count * 3,
				tr.world->geometry.world_static.geom_transparent.prim_offsets[0] * 3,
				tr.world->geometry.world_static.geom_transparent.prim_counts[0] * 3
			);
		}
		END_PERF_MARKER( trace_cmd_buf, PROFILER_SHADOW_MAP );

		vk_rtx_trace_primary_rays( trace_cmd_buf );

		// The host-side image of the uniform buffer is only ready after the `vkpt_pt_create_toplevel` call above
		VK_CHECK( vkpt_uniform_buffer_upload_to_staging() );

		vkpt_submit_command_buffer(
			trace_cmd_buf,
			vk.queue_graphics,
			all_device_mask,
			vk.device_count, &transfer_semaphores, &wait_stages, device_indices,
			0, 0, 0,
			VK_NULL_HANDLE);
	}


	{
		VkCommandBuffer trace_cmd_buf = vkpt_begin_command_buffer( &vk.cmd_buffers_graphics );

		if ( god_rays_enabled )
		{
			BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_GOD_RAYS );
			vk_rtx_record_god_rays_trace_command_buffer( trace_cmd_buf, 0 );
			END_PERF_MARKER( trace_cmd_buf, PROFILER_GOD_RAYS );
		}

		if ( ref_mode->reflect_refract > 0 )
		{
			BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_REFLECT_REFRACT_1 );
			vk_rtx_trace_reflections( trace_cmd_buf, 0 );
			END_PERF_MARKER( trace_cmd_buf, PROFILER_REFLECT_REFRACT_1 );
		}

		if ( god_rays_enabled )
		{
			if ( ref_mode->reflect_refract > 0 )
			{
				BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_GOD_RAYS_REFLECT_REFRACT );
				vk_rtx_record_god_rays_trace_command_buffer( trace_cmd_buf, 1 );
				END_PERF_MARKER(trace_cmd_buf, PROFILER_GOD_RAYS_REFLECT_REFRACT );
			}

			BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_GOD_RAYS_FILTER );
			vk_rtx_record_god_rays_filter_command_buffer( trace_cmd_buf );
			END_PERF_MARKER( trace_cmd_buf, PROFILER_GOD_RAYS_FILTER );
		}

		if ( ref_mode->reflect_refract > 1 )
		{
			BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_REFLECT_REFRACT_2 );
			for (int pass = 0; pass < ref_mode->reflect_refract - 1; pass++)
			{
				vk_rtx_trace_reflections( trace_cmd_buf, pass );
			}
			END_PERF_MARKER( trace_cmd_buf, PROFILER_REFLECT_REFRACT_2 );
		}

		if ( denoiser->pre_lighting )
		{
			BEGIN_PERF_MARKER( trace_cmd_buf, PROFILER_ASVGF_GRADIENT_REPROJECT );
			denoiser->pre_lighting( trace_cmd_buf );
			END_PERF_MARKER( trace_cmd_buf, PROFILER_ASVGF_GRADIENT_REPROJECT );
		}
		vk_rxt_trace_lighting( trace_cmd_buf, ref_mode->num_bounce_rays );

		vk_rtx_radiance_cache_update( trace_cmd_buf );
		vk_rtx_radiance_cache_resolve( trace_cmd_buf );

		vkpt_submit_command_buffer(
			trace_cmd_buf,
			vk.queue_graphics,
			all_device_mask,
			0, 0, 0, 0,
			vk.device_count, &trace_semaphores, device_indices,
			VK_NULL_HANDLE);

		*curr_trace_signaled = true;
	}

	{
		VkCommandBuffer post_cmd_buf = vkpt_begin_command_buffer(&vk.cmd_buffers_graphics);

		BEGIN_PERF_MARKER( post_cmd_buf, PROFILER_ASVGF_FULL );
		denoiser->filter( post_cmd_buf );
		END_PERF_MARKER( post_cmd_buf, PROFILER_ASVGF_FULL );

		vk_rtx_radiance_cache_debug( post_cmd_buf );

		vk_rtx_interleave( post_cmd_buf );

		vk_rtx_taa( post_cmd_buf );

		BEGIN_PERF_MARKER(post_cmd_buf, PROFILER_BLOOM);
		if ( cvar_bloom_enable->integer != 0 )
		{
			vk_rtx_bloom_record_cmd_buffer( post_cmd_buf );
		}
		END_PERF_MARKER(post_cmd_buf, PROFILER_BLOOM);

		BEGIN_PERF_MARKER( post_cmd_buf, PROFILER_TONE_MAPPING );
		if ( sun_tm_enable->integer != 0 )
		{
			float frame_time = MIN( 1.f, MAX(0.f, refdef->frametime) );

			static unsigned previous_wallclock_time = 0;
			unsigned current_wallclock_time = ri.Milliseconds();
			float frame_wallclock_time = (previous_wallclock_time != 0) ? (float)(current_wallclock_time - previous_wallclock_time) * 1e-3f : 0.f;
			previous_wallclock_time = current_wallclock_time;

			vkpt_tone_mapping_record_cmd_buffer( post_cmd_buf, frame_time <= 0.f ? frame_wallclock_time : frame_time );
		}
		END_PERF_MARKER( post_cmd_buf, PROFILER_TONE_MAPPING );

		if ( ubo->distortion_surfaces )
			vk_rtx_distortion( post_cmd_buf );

		{
			VkBufferCopy copyRegion = { 0, 0, sizeof(ReadbackBuffer) };
			qvkCmdCopyBuffer( post_cmd_buf, vk.buf_readback.buffer, vk.buf_readback_staging[vk.current_frame_index].buffer, 1, &copyRegion);
		}

		vk_gpu_timer_write( post_cmd_buf, VKP_TS_RT_END, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );

		vkpt_submit_command_buffer_simple( post_cmd_buf, vk.queue_graphics, true );
	}
}

static qboolean frame_ready;

void vk_rtx_begin_scene( trRefdef_t *refdef, drawSurf_t *drawSurfs, int numDrawSurfs ) 
{
#if 0
	// reset offsets
	vk.scratch_buf_ptr = 0;
#endif
	reference_mode_t	ref_mode;
	vkUniformRTX_t		*ubo;
	sun_light_t			sun_light;
	vec3_t				sky_matrix[3];
	qboolean			render_world = qfalse;
	mnode_t				*viewleaf;

	if ( tr.world )
		render_world = qtrue;

	//bool render_world = (fd->rdflags & RDF_NOWORLDMODEL) == 0;

	viewleaf = tr.world ? BSP_PointLeaf( tr.world->nodes, refdef->vieworg ) : NULL;

	qboolean sun_visible_prev = qfalse;
	static float prev_adapted_luminance = 0.f;
	float adapted_luminance = 0.f;
	vk_rtx_process_render_feedback( &refdef->feedback, viewleaf, &sun_visible_prev, &adapted_luminance );

	// Sometimes, the readback returns 1.0 luminance instead of the real value.
	// Ignore these mysterious spikes.
	if ( adapted_luminance != 1.0f ) 
		prev_adapted_luminance = adapted_luminance;
	
	if ( prev_adapted_luminance <= 0.f )
		prev_adapted_luminance = 0.005f;

	if ( !tr.world && !render_world )
		return;

	// environment	
	prepare_sky_matrix( refdef->time, sky_matrix );
	
	Com_Memset( &sun_light, 0, sizeof(sun_light_t) );
	if ( render_world )
	{
		vk_rtx_evaluate_sun_light( &sun_light, sky_matrix, refdef->time );

		if ( !vkpt_physical_sky_needs_update() && !RTX_LightSky_Hybrid() )
			sun_light.visible = (qboolean)(sun_light.visible && sun_visible_prev);
	}
	
	evaluate_reference_mode( &ref_mode, refdef );
	vk_rtx_taa_evaluate_settings( ref_mode.denoiser != DENOISER_NONE ? qtrue : qfalse );

	num_model_lights = 0;
	EntityUploadInfo upload_info;
	Com_Memset( &upload_info, 0, sizeof(EntityUploadInfo) );
	vkpt_pt_reset_instances();
	vkpt_shadow_map_reset_instances();
	prepare_entities( &upload_info, refdef );

	if ( tr.world && render_world )
	{
		vkpt_pt_instance_model_blas( &tr.world->geometry.world_static.geom_opaque,					g_identity_transform, VERTEX_BUFFER_WORLD, -1, 0 );
		vkpt_pt_instance_model_blas( &tr.world->geometry.world_static.geom_transparent,				g_identity_transform, VERTEX_BUFFER_WORLD, -1, 0 );
		vkpt_pt_instance_model_blas( &tr.world->geometry.world_static.geom_masked,					g_identity_transform, VERTEX_BUFFER_WORLD, -1, 0 );

		// The Force Sight surfaces are in the TLAS only while Force Sight is on, as the rasterizer skips them.
		if ( refdef->rdflags & RDF_ForceSightOn )
			vkpt_pt_instance_model_blas( &tr.world->geometry.world_static.geom_forcesight,			g_identity_transform, VERTEX_BUFFER_WORLD, -1, 0 );

		vkpt_pt_instance_model_blas( &tr.world->geometry.world_dynamic_material.geom_opaque,		g_identity_transform, VERTEX_BUFFER_WORLD_D_MATERIAL, -1, 0 );
		vkpt_pt_instance_model_blas( &tr.world->geometry.world_dynamic_material.geom_transparent,	g_identity_transform, VERTEX_BUFFER_WORLD_D_MATERIAL, -1, 0 );
		vkpt_pt_instance_model_blas( &tr.world->geometry.world_dynamic_material.geom_masked,		g_identity_transform, VERTEX_BUFFER_WORLD_D_MATERIAL, -1, 0 );
		vkpt_pt_instance_model_blas( &tr.world->geometry.world_dynamic_geometry.geom_opaque,		g_identity_transform, VERTEX_BUFFER_WORLD_D_GEOMETRY, -1, 0 );
		vkpt_pt_instance_model_blas( &tr.world->geometry.world_dynamic_geometry.geom_transparent,	g_identity_transform, VERTEX_BUFFER_WORLD_D_GEOMETRY, -1, 0 );
		vkpt_pt_instance_model_blas( &tr.world->geometry.world_dynamic_geometry.geom_masked,		g_identity_transform, VERTEX_BUFFER_WORLD_D_GEOMETRY, -1, 0 );
		vkpt_pt_instance_model_blas( &tr.world->geometry.sky_static.geom_opaque,					g_identity_transform, VERTEX_BUFFER_SKY, -1, 0 );

#ifdef DEBUG_POLY_LIGHTS
		if ( pt_debug_poly_lights->integer )
			vkpt_pt_instance_model_blas( &tr.world->geometry.debug_light_polys.geom_opaque,			g_identity_transform, VERTEX_BUFFER_DEBUG_LIGHT_POLYS, -1, 0 );
#endif

#if 0
		vkpt_build_beam_lights(model_lights, &num_model_lights, MAX_MODEL_LIGHTS, bsp_world_model, fd->entities, fd->num_entities, prev_adapted_luminance, light_entity_ids[entity_frame_num], &num_model_lights);
#endif
		vk_rtx_build_saber_lights( model_lights, &num_model_lights, MAX_MODEL_LIGHTS, tr.world, refdef, prev_adapted_luminance, light_entity_ids[entity_frame_num] );
		add_dlights(refdef->dlights, refdef->num_dlights, model_lights, &num_model_lights, MAX_MODEL_LIGHTS, tr.world, light_entity_ids[entity_frame_num]);
	}

	last_model_lights = num_model_lights;
	update_mlight_prev_to_current();

#if 1
	vkpt_vertex_buffer_ensure_primbuf_size(upload_info.num_prims);
#endif

	ubo = &vk.uniform_buffer;
	vk_rtx_prepare_ubo( refdef, tr.world, viewleaf, &ref_mode, sky_matrix, render_world );
	ubo->prev_adapted_luminance = prev_adapted_luminance;
	vk_rtx_prepare_distortion_ubo( ubo, &upload_info );
	vk_rtx_prepare_fog_ubo( ubo, refdef, (qboolean)render_world );

#if 0
	if ( tm_blend_enable->integer )
		Vector4Copy( fd->blend, ubo->fs_blend_color );
	else
#endif
		Vector4Set( ubo->fs_blend_color, 0.f, 0.f, 0.f, 0.f );

#if 0
	if (vkpt_refdef.fd->rdflags & RDF_IRGOGGLES)
		Vector4Set(ubo->fs_colorize, 1.f, 0.f, 0.f, 0.8f);
	else
#endif
		Vector4Set( ubo->fs_colorize, 0.f, 0.f, 0.f, 0.f );

	vk_rtx_physical_sky_update_ubo( ubo, &sun_light, render_world );
	vk_rtx_bloom_update( ubo );

	vec3_t sky_radiance;
	VectorScale( avg_envmap_color, ubo->pt_env_scale, sky_radiance );

	vk_rtx_animate_materials( refdef );
	vkpt_light_buffer_upload_to_staging( render_world, tr.world, num_model_lights, model_lights, sky_radiance );
	RTX_LightStyle_EndFrame();

	float shadowmap_view_proj[16];
	float shadowmap_depth_scale;
	const float unused = 0.0f;
	vk_rtx_shadow_map_setup(
		&sun_light,
		tr.world->world_aabb.mins,
		tr.world->world_aabb.maxs,
		shadowmap_view_proj,
		&shadowmap_depth_scale,
		qfalse);

	vk_rtx_god_rays_prepare_ubo(
		ubo,
		&tr.world->world_aabb,
		ubo->P,
		ubo->V,
		shadowmap_view_proj,
		shadowmap_depth_scale);

	qboolean god_rays_enabled = ( vk_rtx_god_rays_enabled(&sun_light) && render_world) ? qtrue : qfalse;

	if ( render_world )
		vk_rtx_deform_world_geometry( refdef );

	vk_begin_trace_rays( *tr.world, refdef, &ref_mode, ubo, 
		drawSurfs, numDrawSurfs, shadowmap_view_proj, 
		god_rays_enabled, render_world, &upload_info );

	// The tracer writes the reservoirs when the bounce pass runs at full resolution.
	restir_gi_history_valid = ( ubo->pt_restir_gi != 0.f && ubo->pt_num_bounce_rays >= 1.f ) ? qtrue : qfalse;

	vk_rtx_denoisers_end_frame( ref_mode.denoiser );
	vk_rtx_taa_end_frame( ref_mode.denoiser != DENOISER_NONE ? qtrue : qfalse );

	frame_ready = qtrue;


	VK_BeginRenderClear();
}

static inline qboolean extents_equal( VkExtent2D a, VkExtent2D b )
{
	return (qboolean)( a.width == b.width && a.height == b.height );
}

void vk_rtx_begin_blit( void ) 
{
	if ( !frame_ready )
		return;

	vk_end_render_pass(); // end main

	if ( vk.effective_aa_mode == AA_MODE_UPSCALE )
	{
		vkpt_final_blit_simple( vk.cmd->command_buffer );
	}
	else
	{
		VkExtent2D extent_unscaled_half;
		extent_unscaled_half.width = vk.extent_unscaled.width / 2;
		extent_unscaled_half.height = vk.extent_unscaled.height / 2;

		if ( extents_equal( vk.extent_render, vk.extent_unscaled ) ||
			extents_equal( vk.extent_render, extent_unscaled_half )/* && drs_effective_scale == 0*/ ) // don't do nearest filter 2x upscale with DRS enabled
			vkpt_final_blit_simple( vk.cmd->command_buffer );
		else {
			vkpt_final_blit_filtered( vk.cmd->command_buffer );
		}
	}

	frame_ready = qfalse;

	vk_begin_post_blend_render_pass( vk.render_pass.rtx_final_blit.blend, qfalse );
}