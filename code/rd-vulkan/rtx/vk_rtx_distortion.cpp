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

static vkpipeline_t	distortion_pipeline;

void vk_rtx_create_distortion_pipeline( void )
{
	vk_rtx_create_standard_compute_pipeline( &distortion_pipeline, vk.compute_shader[SHADER_DISTORTION_COMP], NULL, 0 );
}

void vk_rtx_destroy_distortion_pipeline( void )
{
	vk_rtx_destroy_pipeline( &distortion_pipeline );
}

/*
================
vk_rtx_distortion

The force push and the cloak, as the rasterizer draws them (see distortion.comp). Runs after the tone
mapper, on the finished image: a copy of it is what the distortion samples, the pass writes in place.
================
*/
void vk_rtx_distortion( VkCommandBuffer cmd_buf )
{
	const vkimage_t *output = &vk.img_rtx[RTX_IMG_TAA_OUTPUT];
	const vkimage_t *source = &vk.img_rtx[RTX_IMG_TAA_DISTORT_SRC];

	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;

	IMAGE_BARRIER( cmd_buf, output->handle, range, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );

	VkImageCopy region;
	Com_Memset( &region, 0, sizeof(VkImageCopy) );
	region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.extent = { vk.extent_taa_output.width, vk.extent_taa_output.height, 1 };

	qvkCmdCopyImage( cmd_buf, output->handle, VK_IMAGE_LAYOUT_GENERAL, source->handle, VK_IMAGE_LAYOUT_GENERAL, 1, &region );

	IMAGE_BARRIER( cmd_buf, source->handle, range, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );
	IMAGE_BARRIER( cmd_buf, output->handle, range, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );

	BEGIN_PERF_MARKER( cmd_buf, PROFILER_DISTORTION );

	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &distortion_pipeline );

	qvkCmdDispatch( cmd_buf,
		(vk.extent_taa_output.width + 15) / 16,
		(vk.extent_taa_output.height + 15) / 16,
		1 );

	END_PERF_MARKER( cmd_buf, PROFILER_DISTORTION );

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_TAA_OUTPUT] );
}
