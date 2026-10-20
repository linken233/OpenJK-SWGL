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

static vkpipeline_t	rc_resolve_pipeline;
static vkpipeline_t	rc_debug_pipeline;
static qboolean		rc_clear_pending = qtrue;

static vkbuffer_t *rc_buffers[] = {
	&vk.buf_rc_tags,
	&vk.buf_rc_accum,
	&vk.buf_rc_resolved,
	&vk.buf_rc_last_frame
};

static void vk_rtx_radiance_cache_create_buffer( vkbuffer_t *buffer, VkDeviceSize element_size )
{
	VK_CHECK( vk_rtx_buffer_create( buffer, element_size * RC_CAPACITY,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) );
}

// The buffers are created once, with the other RTX buffers. The contents are cleared before the first use.
void vk_rtx_radiance_cache_create_buffers( void )
{
	vk_rtx_radiance_cache_create_buffer( &vk.buf_rc_tags,		sizeof(uint32_t) );
	vk_rtx_radiance_cache_create_buffer( &vk.buf_rc_accum,		sizeof(uint32_t) * 4 );
	vk_rtx_radiance_cache_create_buffer( &vk.buf_rc_resolved,	sizeof(float) * 4 );
	vk_rtx_radiance_cache_create_buffer( &vk.buf_rc_last_frame,	sizeof(uint32_t) );

	rc_clear_pending = qtrue;
}

void vk_rtx_radiance_cache_destroy_buffers( void )
{
	for ( uint32_t i = 0; i < ARRAY_LEN( rc_buffers ); i++ )
		vk_rtx_buffer_destroy( rc_buffers[i] );
}

void vk_rtx_radiance_cache_create_pipelines( void )
{
	vk_rtx_create_standard_compute_pipeline( &rc_resolve_pipeline, vk.compute_shader[SHADER_RADIANCE_CACHE_RESOLVE_COMP], NULL, 0 );
	vk_rtx_create_standard_compute_pipeline( &rc_debug_pipeline, vk.compute_shader[SHADER_RADIANCE_CACHE_DEBUG_COMP], NULL, 0 );
}

void vk_rtx_radiance_cache_destroy_pipelines( void )
{
	vk_rtx_destroy_pipeline( &rc_resolve_pipeline );
	vk_rtx_destroy_pipeline( &rc_debug_pipeline );
}

// A new map has other surfaces: the cells of the old one are wrong. The next use clears them.
void vk_rtx_radiance_cache_invalidate( void )
{
	rc_clear_pending = qtrue;
}

static void vk_rtx_radiance_cache_barrier( VkCommandBuffer cmd_buf, VkAccessFlags src_access, VkAccessFlags dst_access )
{
	for ( uint32_t i = 0; i < ARRAY_LEN( rc_buffers ); i++ )
		BUFFER_BARRIER( cmd_buf, src_access, dst_access, rc_buffers[i]->buffer, 0, VK_WHOLE_SIZE );
}

static void vk_rtx_radiance_cache_clear( VkCommandBuffer cmd_buf )
{
	if ( !rc_clear_pending )
		return;

	for ( uint32_t i = 0; i < ARRAY_LEN( rc_buffers ); i++ )
		qvkCmdFillBuffer( cmd_buf, rc_buffers[i]->buffer, 0, VK_WHOLE_SIZE, 0 );

	vk_rtx_radiance_cache_barrier( cmd_buf, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );

	rc_clear_pending = qfalse;
}

// One path per tile of pt_rc_update_stride x pt_rc_update_stride pixels. It runs after the lighting passes.
void vk_rtx_radiance_cache_update( VkCommandBuffer cmd_buf )
{
	if ( !sun_pt_rc_enable->integer )
		return;

	BEGIN_PERF_MARKER( cmd_buf, PROFILER_RADIANCE_CACHE_UPDATE );

	vk_rtx_radiance_cache_clear( cmd_buf );

	const uint32_t stride = (uint32_t)MAX( 1, MIN( RC_MAX_UPDATE_STRIDE, (int)sun_pt_rc_update_stride->value ) );

	pt_push_constants_t push;
	push.gpu_index = -1;
	push.bounce = 0;

	vk_rtx_dispatch_rays( cmd_buf, PIPELINE_RADIANCE_CACHE_UPDATE, push,
		( vk.extent_render.width + stride - 1 ) / stride,
		( vk.extent_render.height + stride - 1 ) / stride,
		1 );

	vk_rtx_radiance_cache_barrier( cmd_buf, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );

	END_PERF_MARKER( cmd_buf, PROFILER_RADIANCE_CACHE_UPDATE );
}

// One thread per cell. The barrier after it makes the cells readable for the next frame.
void vk_rtx_radiance_cache_resolve( VkCommandBuffer cmd_buf )
{
	if ( !sun_pt_rc_enable->integer )
		return;

	BEGIN_PERF_MARKER( cmd_buf, PROFILER_RADIANCE_CACHE_RESOLVE );

	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &rc_resolve_pipeline );
	qvkCmdDispatch( cmd_buf, RC_CAPACITY / RC_RESOLVE_GROUP_SIZE, 1, 1 );

	vk_rtx_radiance_cache_barrier( cmd_buf, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );

	END_PERF_MARKER( cmd_buf, PROFILER_RADIANCE_CACHE_RESOLVE );
}

// Replaces the color of the surfaces with a value of their cells. It runs after the denoiser.
void vk_rtx_radiance_cache_debug( VkCommandBuffer cmd_buf )
{
	if ( !sun_pt_rc_debug->integer )
		return;

	vk_rtx_radiance_cache_clear( cmd_buf );

	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &rc_debug_pipeline );
	qvkCmdDispatch( cmd_buf,
		( vk.extent_render.width + 15 ) / 16,
		( vk.extent_render.height + 15 ) / 16,
		1 );

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_DENOISED_COLOR] );
}
