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

static vkpipeline_t interleave_pipeline;

void vk_rtx_create_interleave_pipeline( void )
{
	vk_rtx_create_standard_compute_pipeline( &interleave_pipeline, vk.compute_shader[SHADER_CHECKERBOARD_INTERLEAVE_COMP], NULL, 0 );
}

void vk_rtx_destroy_interleave_pipeline( void )
{
	vk_rtx_destroy_pipeline( &interleave_pipeline );
}

void vk_rtx_interleave( VkCommandBuffer cmd_buf )
{
#ifdef VKPT_DEVICE_GROUPS
	if (qvk.device_count > 1) {
		BEGIN_PERF_MARKER(cmd_buf, PROFILER_MGPU_TRANSFERS);

		// create full interleaved motion and color buffers on GPU 0
		VkOffset2D offset_left = { 0, 0 };
		VkOffset2D offset_right = { qvk.extent_render.width / 2, 0 };
		VkExtent2D extent = { qvk.extent_render.width / 2, qvk.extent_render.height };

		vkpt_mgpu_image_copy(cmd_buf,
							VKPT_IMG_PT_MOTION,
							VKPT_IMG_PT_MOTION,
							1,
							0,
							offset_left,
							offset_right,
							extent);

		vkpt_mgpu_image_copy(cmd_buf,
							VKPT_IMG_DENOISED_COLOR,
							VKPT_IMG_DENOISED_COLOR,
							1,
							0,
							offset_left,
							offset_right,
							extent);

		vkpt_mgpu_global_barrier(cmd_buf);

		END_PERF_MARKER(cmd_buf, PROFILER_MGPU_TRANSFERS);
	}
#endif

	BEGIN_PERF_MARKER( cmd_buf, PROFILER_INTERLEAVE );

	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &interleave_pipeline );

	//set_current_gpu(cmd_buf, 0);

	// dispatch using the image dimensions, not render dimensions - to clear the unused area with black color
	qvkCmdDispatch( cmd_buf,
		(vk.extent_screen_images.width + 15) / 16,
		(vk.extent_screen_images.height + 15) / 16,
		1 );

	END_PERF_MARKER( cmd_buf, PROFILER_INTERLEAVE );

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_FLAT_COLOR] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_FLAT_FX] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_FLAT_MOTION] );
}
