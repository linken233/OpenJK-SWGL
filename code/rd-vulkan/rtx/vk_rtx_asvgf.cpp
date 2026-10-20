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

static qboolean asvgf_history_valid = qfalse;

static void vk_rtx_asvgf_create_pipelines( void )
{
	uint32_t i;
	VkSpecializationMapEntry spec_entries;
	VkSpecializationInfo spec_info[4];
	uint32_t spec_data[4] = { 0, 1, 2, 3 };

    spec_entries.constantID = 0;
    spec_entries.offset = 0;
    spec_entries.size = sizeof(uint32_t);

	for ( i = 0; i < 4; i++ )
	{
		spec_info[i].mapEntryCount = 1;
		spec_info[i].pMapEntries = &spec_entries;
		spec_info[i].dataSize = sizeof(uint32_t);
		spec_info[i].pData = &spec_data[i];
	}

#define ASVGF_PIPELINE( _pipeline, _shader, _spec, _push_size ) \
	vk_rtx_create_standard_compute_pipeline( &vk.asvgf_pipeline[_pipeline], vk.compute_shader[_shader], _spec, _push_size )

	ASVGF_PIPELINE( GRADIENT_IMAGE,		SHADER_ASVGF_GRADIENT_IMG_COMP,			NULL,			0 );
	ASVGF_PIPELINE( GRADIENT_ATROUS,	SHADER_ASVGF_GRADIENT_ATROUS_COMP,		NULL,			sizeof(uint32_t) );
	ASVGF_PIPELINE( GRADIENT_REPROJECT,	SHADER_ASVGF_GRADIENT_REPROJECT_COMP,	NULL,			0 );
	ASVGF_PIPELINE( TEMPORAL,			SHADER_ASVGF_TEMPORAL_COMP,				NULL,			0 );
	ASVGF_PIPELINE( ATROUS_LF,			SHADER_ASVGF_LF_COMP,					NULL,			sizeof(uint32_t) );
	ASVGF_PIPELINE( ATROUS_ITER_0,		SHADER_ASVGF_ATROUS_COMP,				&spec_info[0],	0 );
	ASVGF_PIPELINE( ATROUS_ITER_1,		SHADER_ASVGF_ATROUS_COMP,				&spec_info[1],	0 );
	ASVGF_PIPELINE( ATROUS_ITER_2,		SHADER_ASVGF_ATROUS_COMP,				&spec_info[2],	0 );
	ASVGF_PIPELINE( ATROUS_ITER_3,		SHADER_ASVGF_ATROUS_COMP,				&spec_info[3],	0 );

#undef ASVGF_PIPELINE
}

static void vk_rtx_asvgf_destroy_pipelines( void )
{
	 uint32_t i;

	 for ( i = 0; i < ASVGF_NUM_PIPELINES; i++ )
		 vk_rtx_destroy_pipeline( &vk.asvgf_pipeline[i] );
}

static void vk_rtx_asvgf_invalidate_history( void )
{
	asvgf_history_valid = qfalse;
}

// The temporal filters skip the history on the first frame after the denoiser was off.
static void vk_rtx_asvgf_prepare_ubo( vkUniformRTX_t *ubo )
{
	if ( asvgf_history_valid )
		return;

	ubo->flt_temporal_lf = 0;
	ubo->flt_temporal_hf = 0;
	ubo->flt_temporal_spec = 0;
}

static void vk_rtx_asvgf_end_frame( qboolean active )
{
	asvgf_history_valid = active;
}

static void vk_rtx_asvgf_gradient_reproject( VkCommandBuffer cmd_buf )
{
	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &vk.asvgf_pipeline[GRADIENT_REPROJECT] );

	uint32_t group_size_pixels = 24; // matches GROUP_SIZE_PIXELS in asvgf_gradient_reproject.comp
	qvkCmdDispatch( cmd_buf,
		(vk.gpu_slice_width + group_size_pixels - 1) / group_size_pixels,
		(glConfig.vidHeight + group_size_pixels - 1) / group_size_pixels,
		1 );


	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_RNG_SEED_A + (vk.frame_counter & 1)] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_GRAD_SMPL_POS_A  + (vk.frame_counter & 1)] );
}

static void vk_rtx_asvgf_filter( VkCommandBuffer cmd_buf )
{
	const qboolean enable_lf = sun_pt_num_bounce_rays->value >= 0.5f ? qtrue : qfalse;

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_SH] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_COCG] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_HF] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_SPEC] );

	BEGIN_PERF_MARKER( cmd_buf, PROFILER_ASVGF_RECONSTRUCT_GRADIENT );

	/* create gradient image */
	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &vk.asvgf_pipeline[GRADIENT_IMAGE] );

	qvkCmdDispatch( cmd_buf,
			(vk.gpu_slice_width / GRAD_DWN + 15) / 16,
			(vk.extent_render.height / GRAD_DWN + 15) / 16,
			1 );

	// XXX BARRIERS!!!
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_GRAD_LF_PING] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_GRAD_HF_SPEC_PING] );

	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &vk.asvgf_pipeline[GRADIENT_ATROUS] );

	/* reconstruct gradient image */
	const int num_atrous_iterations_gradient = 7;
	for ( int i = 0; i < num_atrous_iterations_gradient; i++ )
	{
		uint32_t push_constants[1] = {
			i
		};

		qvkCmdPushConstants( cmd_buf, vk.asvgf_pipeline[GRADIENT_ATROUS].layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants );

		qvkCmdDispatch( cmd_buf,
				(vk.gpu_slice_width / GRAD_DWN + 15) / 16,
				(vk.extent_render.height / GRAD_DWN + 15) / 16,
				1 );

		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_GRAD_LF_PING + !(i & 1)] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_GRAD_HF_SPEC_PING + !(i & 1)] );
	}

	END_PERF_MARKER( cmd_buf, PROFILER_ASVGF_RECONSTRUCT_GRADIENT );
	BEGIN_PERF_MARKER( cmd_buf, PROFILER_ASVGF_TEMPORAL );

	/* temporal accumulation / filtering */
	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &vk.asvgf_pipeline[TEMPORAL] );

	qvkCmdDispatch( cmd_buf,
			(vk.gpu_slice_width + 14) / 15,
			(vk.extent_render.height + 14) / 15,
			1 );

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_LF_SH] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_LF_COCG] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_HF] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_MOMENTS] );

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_FILTERED_SPEC_A + (vk.frame_counter & 1)] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_HIST_MOMENTS_HF_A] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_HIST_MOMENTS_HF_B] ); // B aka prev

	END_PERF_MARKER( cmd_buf, PROFILER_ASVGF_TEMPORAL );
	BEGIN_PERF_MARKER( cmd_buf, PROFILER_ASVGF_ATROUS );

	/* spatial reconstruction filtering */
	const int num_atrous_iterations = 4;
	for ( int i = 0; i < num_atrous_iterations; i++ )
	{
		if ( enable_lf )
		{
			uint32_t push_constants[1] = {
				i
			};

			vk_rtx_bind_standard_compute_pipeline( cmd_buf, &vk.asvgf_pipeline[ATROUS_LF] );

			qvkCmdPushConstants( cmd_buf, vk.asvgf_pipeline[ATROUS_LF].layout,
				VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants );

			qvkCmdDispatch( cmd_buf,
				(vk.gpu_slice_width / GRAD_DWN + 15) / 16,
				(vk.extent_render.height / GRAD_DWN + 15) / 16,
				1 );

			if ( i == num_atrous_iterations - 1 )
			{
				BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_LF_SH] );
				BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_LF_COCG] );
			}
		}

		int specialization = ATROUS_ITER_0 + i;

		vk_rtx_bind_standard_compute_pipeline( cmd_buf, &vk.asvgf_pipeline[specialization] );

		qvkCmdDispatch( cmd_buf,
				(vk.gpu_slice_width + 15) / 16,
				(vk.extent_render.height + 15) / 16,
				1 );

		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_LF_SH] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_LF_COCG] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_HF] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_MOMENTS] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PONG_LF_SH] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PONG_LF_COCG] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PONG_HF] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PONG_MOMENTS] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_HIST_COLOR_HF] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_LF_SH + !(i & 1)] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_ASVGF_ATROUS_PING_LF_COCG + !(i & 1)] );
		BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_DENOISED_COLOR] );
	}

	END_PERF_MARKER( cmd_buf, PROFILER_ASVGF_ATROUS );
}

const denoiser_t vk_rtx_denoiser_asvgf = {
	"asvgf",
	DENOISER_FLAG_ACTIVE | DENOISER_FLAG_LF_SH | DENOISER_FLAG_SPEC_DEMODULATE | DENOISER_FLAG_GRADIENTS | DENOISER_FLAG_FAKE_SPECULAR,
	vk_rtx_asvgf_create_pipelines,
	vk_rtx_asvgf_destroy_pipelines,
	vk_rtx_asvgf_invalidate_history,
	vk_rtx_asvgf_prepare_ubo,
	vk_rtx_asvgf_gradient_reproject,
	vk_rtx_asvgf_filter,
	vk_rtx_asvgf_end_frame
};
