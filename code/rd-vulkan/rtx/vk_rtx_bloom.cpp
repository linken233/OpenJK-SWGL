/*
===========================================================================
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.
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

// CPU side of the path tracer's HDR bloom, ported from Q2RTX (src/refresh/vkpt/bloom.c).
// The light above a threshold is scaled down to a quarter, blurred horizontally and then
// vertically with a Gaussian, and added to the TAA output before the tone mapper runs.
// Q2RTX blends the whole image instead: that needs its much brighter light textures, and
// here it only puts a haze on the scene without a halo around the glow stages.

#include "tr_local.h"

cvar_t			*cvar_bloom_enable;
static cvar_t	*cvar_bloom_sigma;
static cvar_t	*cvar_bloom_intensity;
static cvar_t	*cvar_bloom_threshold;

// Must match the push constant block of bloom_blur.comp.
typedef struct {
	float	pixstep_x;
	float	pixstep_y;
	float	argument_scale;
	float	normalization_scale;
	int		num_samples;
	int		pass;
} bloomBlurPushConstants_t;

static bloomBlurPushConstants_t	push_constants_hblur;
static bloomBlurPushConstants_t	push_constants_vblur;

static void vk_rtx_bloom_compute_push_constants( void )
{
	// pt_bloom_sigma is a fraction of the screen height; the blur runs at a quarter of it.
	const float sigma_pixels = cvar_bloom_sigma->value * vk.extent_taa_output.height;
	float effective_sigma = sigma_pixels * 0.25f;

	effective_sigma = MIN( effective_sigma, 100.f );
	effective_sigma = MAX( effective_sigma, 1.f );

	push_constants_hblur.pixstep_x = 1.f;
	push_constants_hblur.pixstep_y = 0.f;
	push_constants_hblur.argument_scale = -1.f / ( 2.f * effective_sigma * effective_sigma );
	push_constants_hblur.normalization_scale = 1.f / ( sqrtf( 2.f * (float)M_PI ) * effective_sigma );
	push_constants_hblur.num_samples = (int)roundf( effective_sigma * 4.f );
	push_constants_hblur.pass = 0;

	push_constants_vblur = push_constants_hblur;
	push_constants_vblur.pixstep_x = 0.f;
	push_constants_vblur.pixstep_y = 1.f;
	push_constants_vblur.pass = 1;
}

static void vk_rtx_bloom_register_cvars( void )
{
	cvar_bloom_enable		= ri.Cvar_Get( "pt_bloom",				"1",		CVAR_ARCHIVE_ND );
	cvar_bloom_sigma		= ri.Cvar_Get( "pt_bloom_sigma",		"0.02",		CVAR_ARCHIVE_ND );	// fraction of the screen height
	cvar_bloom_intensity	= ri.Cvar_Get( "pt_bloom_intensity",	"1.0",		CVAR_ARCHIVE_ND );	// scale of the halo added to the image
	cvar_bloom_threshold	= ri.Cvar_Get( "pt_bloom_threshold",	"1.0",		CVAR_ARCHIVE_ND );	// in tone mapper units: 1 is the level of a lightmap value of 1
}

// The histogram takes the bloom out of the image when bloom_intensity is not 0.
void vk_rtx_bloom_update( vkUniformRTX_t *ubo )
{
	ubo->bloom_intensity = cvar_bloom_enable->integer ? cvar_bloom_intensity->value : 0.f;
}

static void vk_create_bloom_pipeline( uint32_t pipeline_index, uint32_t shader_index, uint32_t push_size )
{
	vkpipeline_t *pipeline = &vk.bloom_pipeline[pipeline_index];

	VkDescriptorSetLayout set_layouts[] = {
		vk.desc_set_vertex_buffer[0].layout,
		vk.desc_set_layout_textures,
		vk.imageDescriptor.layout,
		vk.desc_set_layout_ubo
	};

	vk_rtx_bind_pipeline_shader( pipeline, vk.bloom_shader[shader_index] );
	vk_rtx_bind_pipeline_desc_set_layouts( pipeline, set_layouts, ARRAY_LEN(set_layouts) );
	vk_rtx_create_compute_pipeline( pipeline, NULL, push_size );
}

void vk_rtx_create_bloom_pipelines( void )
{
	vk_rtx_bloom_register_cvars();
	vk_load_bloom_shaders();

	vk_create_bloom_pipeline( BLOOM_DOWNSCALE,	SHADER_BLOOM_DOWNSCALE_COMP,	sizeof(float) );
	vk_create_bloom_pipeline( BLOOM_BLUR,		SHADER_BLOOM_BLUR_COMP,			sizeof(bloomBlurPushConstants_t) );
	vk_create_bloom_pipeline( BLOOM_COMPOSITE,	SHADER_BLOOM_COMPOSITE_COMP,	0 );
}

void vk_rtx_destroy_bloom_pipelines( void )
{
	uint32_t i;

	for ( i = 0; i < BLOOM_NUM_PIPELINES; i++ )
		vk_rtx_destroy_pipeline( &vk.bloom_pipeline[i] );
}

static void vk_rtx_bloom_bind( VkCommandBuffer cmd_buf, const vkpipeline_t *pipeline )
{
	VkDescriptorSet desc_sets[] = {
		vk.desc_set_vertex_buffer[vk.current_frame_index].set,
		vk_rtx_get_current_desc_set_textures(),
		vk.imageDescriptor.set,
		vk.desc_set_ubo
	};

	qvkCmdBindPipeline( cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->handle );
	qvkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
		pipeline->layout, 0, ARRAY_LEN(desc_sets), desc_sets, 0, 0 );
}

// Records the bloom into VKPT_IMG_TAA_OUTPUT, in place. Runs before the tone mapper.
void vk_rtx_bloom_record_cmd_buffer( VkCommandBuffer cmd_buf )
{
	const VkExtent2D extent = vk.extent_taa_output;
	VkExtent2D extent_4;
	extent_4.width = extent.width / 4;
	extent_4.height = extent.height / 4;

	vk_rtx_bloom_compute_push_constants();

	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;

	const VkImage taa_output = vk.img_rtx[RTX_IMG_TAA_OUTPUT].handle;
	const VkImage hblur = vk.img_rtx[RTX_IMG_BLOOM_HBLUR].handle;
	const VkImage vblur = vk.img_rtx[RTX_IMG_BLOOM_VBLUR].handle;

	const vkpipeline_t *downscale = &vk.bloom_pipeline[BLOOM_DOWNSCALE];
	const vkpipeline_t *blur = &vk.bloom_pipeline[BLOOM_BLUR];
	const vkpipeline_t *composite = &vk.bloom_pipeline[BLOOM_COMPOSITE];

	IMAGE_BARRIER( cmd_buf, taa_output, range,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );

	// The light above the threshold, at a quarter of the size, into BLOOM_VBLUR.
	const float threshold = cvar_bloom_threshold->value;
	vk_rtx_bloom_bind( cmd_buf, downscale );
	qvkCmdPushConstants( cmd_buf, downscale->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(threshold), &threshold );
	qvkCmdDispatch( cmd_buf, ( extent_4.width + 15 ) / 16, ( extent_4.height + 15 ) / 16, 1 );

	IMAGE_BARRIER( cmd_buf, vblur, range,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );

	// Horizontal pass: BLOOM_VBLUR -> BLOOM_HBLUR.
	vk_rtx_bloom_bind( cmd_buf, blur );
	qvkCmdPushConstants( cmd_buf, blur->layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof(push_constants_hblur), &push_constants_hblur );
	qvkCmdDispatch( cmd_buf, ( extent_4.width + 15 ) / 16, ( extent_4.height + 15 ) / 16, 1 );

	IMAGE_BARRIER( cmd_buf, hblur, range,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );

	// Vertical pass: BLOOM_HBLUR -> BLOOM_VBLUR.
	qvkCmdPushConstants( cmd_buf, blur->layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof(push_constants_vblur), &push_constants_vblur );
	qvkCmdDispatch( cmd_buf, ( extent_4.width + 15 ) / 16, ( extent_4.height + 15 ) / 16, 1 );

	IMAGE_BARRIER( cmd_buf, vblur, range,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );

	// Add BLOOM_VBLUR to the TAA output.
	vk_rtx_bloom_bind( cmd_buf, composite );
	qvkCmdDispatch( cmd_buf, ( extent.width + 15 ) / 16, ( extent.height + 15 ) / 16, 1 );

	IMAGE_BARRIER( cmd_buf, taa_output, range,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );
}
