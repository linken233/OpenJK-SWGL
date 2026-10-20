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

static vkpipeline_t	taa_pipeline;
static qboolean		taa_history_valid = qfalse;

void vk_rtx_create_taa_pipeline( void )
{
	vk_rtx_create_standard_compute_pipeline( &taa_pipeline, vk.compute_shader[SHADER_ASVGF_TAAU_COMP], NULL, 0 );
}

void vk_rtx_destroy_taa_pipeline( void )
{
	vk_rtx_destroy_pipeline( &taa_pipeline );
}

void vk_rtx_taa_invalidate_history( void )
{
	taa_history_valid = qfalse;
}

// TAA stays off for the first frame after the denoiser was off.
void vk_rtx_taa_prepare_ubo( vkUniformRTX_t *ubo )
{
	if ( !taa_history_valid )
		ubo->flt_taa = 0;
}

void vk_rtx_taa_end_frame( qboolean denoiser_active )
{
	taa_history_valid = denoiser_active;
}

void vk_rtx_taa_evaluate_settings( qboolean denoiser_active )
{
	vk.effective_aa_mode = AA_MODE_OFF;
	vk.extent_taa_output = vk.extent_render;

	if ( !denoiser_active )
		return;

	if ( sun_flt_taa->integer == AA_MODE_TAA ) // sun_flt_taa
	{
		vk.effective_aa_mode = AA_MODE_TAA;
	}
	else if ( sun_flt_taa->integer == AA_MODE_UPSCALE ) // sun_flt_taa
	{
		if (vk.extent_render.width > vk.extent_unscaled.width || vk.extent_render.height > vk.extent_unscaled.height)
		{
			vk.effective_aa_mode = AA_MODE_TAA;
		}
		else
		{
			vk.effective_aa_mode = AA_MODE_UPSCALE;
			vk.extent_taa_output = vk.extent_unscaled;
		}
	}
}

void vk_rtx_taa( VkCommandBuffer cmd_buf )
{
	BEGIN_PERF_MARKER( cmd_buf, PROFILER_ASVGF_TAA );

	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &taa_pipeline );

	VkExtent2D dispatch_size = vk.extent_taa_output;

	if ( dispatch_size.width < vk.extent_taa_images.width )
		dispatch_size.width += 8;

	if ( dispatch_size.height < vk.extent_taa_images.height )
		dispatch_size.height += 8;

	qvkCmdDispatch( cmd_buf,
			(dispatch_size.width + 15) / 16,
			(dispatch_size.height + 15) / 16,
			1 );

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_TAA_FX] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_TAA_A] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_TAA_B] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_TAA_B] );

	END_PERF_MARKER( cmd_buf, PROFILER_ASVGF_TAA );
}
