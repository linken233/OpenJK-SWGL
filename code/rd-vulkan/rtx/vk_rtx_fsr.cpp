/*
===========================================================================
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

// AMD FSR temporal upscaling on the path tracer. With r_fsr 1 it replaces asvgf_taau:
// fsr_prepare.comp makes the depth and the reactive mask, FSR writes TAA_OUTPUT,
// fsr_fx.comp makes the other outputs of asvgf_taau. Bloom and tone mapping do not change.

#include "tr_local.h"
#include "vk_fsr.h"

#include <float.h>

#ifdef _WIN32
#	include "ffx_upscale.h"
#	define RTX_FSR_FLAGS	( FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_DEPTH_INVERTED | \
							  FFX_UPSCALE_ENABLE_DEPTH_INFINITE | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE )
#	define RTX_FSR_STATE_READ	FFX_API_RESOURCE_STATE_COMPUTE_READ
#	define RTX_FSR_STATE_UAV	FFX_API_RESOURCE_STATE_UNORDERED_ACCESS
#else
#	define RTX_FSR_FLAGS		0
#	define RTX_FSR_STATE_READ	0
#	define RTX_FSR_STATE_UAV	0
#endif

// A jump of the view origin larger than this in one frame is a camera cut.
#define RTX_FSR_CUT_DISTANCE	256.0f

static struct {
	vkimage_t				depth;				// R32_SFLOAT, inverted infinite device depth
	vkimage_t				reactive;			// R32_SFLOAT, reactive mask
	qboolean				images_ready;		// the images are in the GENERAL layout

	VkDescriptorSetLayout	set_layout;
	VkDescriptorPool		pool;
	VkDescriptorSet			set;
	VkPipelineLayout		pipeline_layout;
	VkPipeline				prepare_pipeline;
	VkPipeline				fx_pipeline;

	uint32_t				display_w, display_h;	// the context sizes
	VkExtent2D				render;

	qboolean				frame;				// FSR replaces asvgf_taau in this frame
	qboolean				reset;
	float					jitter_x, jitter_y;	// FSR jitter of this frame, in render pixels
	float					fov_y;				// radians
	vec3_t					prev_origin;
	qboolean				prev_origin_valid;
	int						prev_time;

	qboolean				depth_written;		// vk_rtx_fsr_record made the depth in this frame
	vec3_t					fg_prev_origin;		// frame generation: camera cut detection
	qboolean				fg_prev_origin_valid;
	qboolean				fg_history_valid;	// the last frame had a world view and a prepare dispatch
	int						fg_prev_time;
} rtx_fsr;

static void rtx_fsr_write_descriptors( void )
{
	VkDescriptorImageInfo	info[2];
	VkWriteDescriptorSet	writes[2];
	uint32_t				i;

	if ( rtx_fsr.set == VK_NULL_HANDLE || rtx_fsr.depth.view == VK_NULL_HANDLE || rtx_fsr.reactive.view == VK_NULL_HANDLE )
		return;

	info[0].sampler = VK_NULL_HANDLE;
	info[0].imageView = rtx_fsr.depth.view;
	info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	info[1].sampler = VK_NULL_HANDLE;
	info[1].imageView = rtx_fsr.reactive.view;
	info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( i = 0; i < ARRAY_LEN( writes ); i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = rtx_fsr.set;
		writes[i].dstBinding = i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[i].pImageInfo = &info[i];
	}

	qvkUpdateDescriptorSets( vk.device, ARRAY_LEN( writes ), writes, 0, NULL );
}

static void rtx_fsr_create_image( vkimage_t *image, const char *name, uint32_t width, uint32_t height )
{
	VkImageCreateInfo		desc;
	VkImageViewCreateInfo	view;

	Com_Memset( image, 0, sizeof( vkimage_t ) );
	image->extent.width = width;
	image->extent.height = height;
	image->extent.depth = 1;
	image->mipLevels = 1;
	image->arrayLayers = 1;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = VK_FORMAT_R32_SFLOAT;
	desc.extent = image->extent;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	desc.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );
	VK_CreateImageMemory( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image->handle, &image->memory );

	Com_Memset( &view, 0, sizeof( view ) );
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.image = image->handle;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = desc.format;
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.levelCount = 1;
	view.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &image->view ) );

	VK_SET_OBJECT_NAME( image->handle, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	VK_SET_OBJECT_NAME( image->view, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
}

static void rtx_fsr_destroy_images( void )
{
	if ( rtx_fsr.depth.handle != VK_NULL_HANDLE )
		vk_rtx_destroy_image( &rtx_fsr.depth );
	if ( rtx_fsr.reactive.handle != VK_NULL_HANDLE )
		vk_rtx_destroy_image( &rtx_fsr.reactive );
	rtx_fsr.images_ready = qfalse;
}

// The images keep their size while it is large enough. The frames in flight can use the old ones.
static void rtx_fsr_ensure_images( uint32_t width, uint32_t height )
{
	if ( rtx_fsr.depth.handle != VK_NULL_HANDLE && rtx_fsr.depth.extent.width == width && rtx_fsr.depth.extent.height == height )
		return;

	if ( rtx_fsr.depth.handle != VK_NULL_HANDLE || rtx_fsr.reactive.handle != VK_NULL_HANDLE ) {
		vk_device_wait_idle();
		rtx_fsr_destroy_images();
	}

	rtx_fsr_create_image( &rtx_fsr.depth, "FSR depth", width, height );
	rtx_fsr_create_image( &rtx_fsr.reactive, "FSR reactive", width, height );
	rtx_fsr.images_ready = qfalse;

	rtx_fsr_write_descriptors();
}

/*
===============
vk_rtx_fsr_update

Makes sure that the upscaler context exists when r_fsr is on. Creates it after a
vid_restart or a map change (RE_Shutdown destroys it). After a failure it retries every frame.
Writes the render size and returns qtrue when FSR is active. Does nothing with r_fsr 0.
===============
*/
qboolean vk_rtx_fsr_update( uint32_t displayW, uint32_t displayH, VkExtent2D *render )
{
	uint32_t w, h;

	if ( r_fsr == NULL || !r_fsr->integer )
		return qfalse;

	if ( vk_fsr_upscale_active() && rtx_fsr.display_w == displayW && rtx_fsr.display_h == displayH ) {
		*render = rtx_fsr.render;
		return qtrue;
	}

	rtx_fsr.display_w = rtx_fsr.display_h = 0;

	// A failed library load or create returns early here, with no new warning.
	vk_fsr_get_render_size( displayW, displayH, &w, &h );
	w = ( w + 1 ) & ~1;	// the two checkerboard fields need an even width

	if ( !vk_fsr_upscale_create( w, h, displayW, displayH, RTX_FSR_FLAGS ) || !vk_fsr_upscale_active() )
		return qfalse;

	// The images keep the display size (frame generation can reuse the depth).
	rtx_fsr_ensure_images( MAX( w, ( displayW + 1 ) & ~1 ), MAX( h, displayH ) );

	rtx_fsr.display_w = displayW;
	rtx_fsr.display_h = displayH;
	rtx_fsr.render.width = w;
	rtx_fsr.render.height = h;
	rtx_fsr.reset = qtrue;
	rtx_fsr.prev_origin_valid = qfalse;
	rtx_fsr.prev_time = 0;

	ri.Printf( PRINT_ALL, "FSR: the path tracer renders at %ux%u and upscales to %ux%u\n", w, h, displayW, displayH );

	*render = rtx_fsr.render;
	return qtrue;
}

void vk_rtx_fsr_set_frame( qboolean on )
{
	rtx_fsr.frame = on;
}

qboolean vk_rtx_fsr_frame_active( void )
{
	return rtx_fsr.frame;
}

// The next frame generation frame starts a new history.
void vk_rtx_fsr_invalidate_history( void )
{
	rtx_fsr.fg_history_valid = qfalse;
}

/*
===============
vk_rtx_fsr_prepare_ubo

Sets the jitter and the texture LOD bias of an FSR frame. FSR puts the sample of a
pixel at center - jitter; the tracer puts it at center + sub_pixel_jitter.
===============
*/
void vk_rtx_fsr_prepare_ubo( vkUniformRTX_t *ubo, const trRefdef_t *refdef )
{
	float jx, jy;

	vk_fsr_get_jitter( (int)( vk.frame_counter & 0x7fffffff ), &jx, &jy );

	rtx_fsr.jitter_x = jx;
	rtx_fsr.jitter_y = jy;
	ubo->sub_pixel_jitter[0] = -jx;
	ubo->sub_pixel_jitter[1] = -jy;

	ubo->pt_texture_lod_bias += vk_fsr_mip_bias();

	rtx_fsr.fov_y = DEG2RAD( refdef->fov_y );

	if ( rtx_fsr.prev_origin_valid && Distance( refdef->vieworg, rtx_fsr.prev_origin ) > RTX_FSR_CUT_DISTANCE )
		rtx_fsr.reset = qtrue;
	VectorCopy( refdef->vieworg, rtx_fsr.prev_origin );
	rtx_fsr.prev_origin_valid = qtrue;
}

static void rtx_fsr_barrier( VkCommandBuffer cmd, VkImage image, VkAccessFlags src, VkAccessFlags dst, VkImageLayout from, VkImageLayout to )
{
	VkImageMemoryBarrier barrier;

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = src;
	barrier.dstAccessMask = dst;
	barrier.oldLayout = from;
	barrier.newLayout = to;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );
}

static void rtx_fsr_bind( VkCommandBuffer cmd, VkPipeline pipeline )
{
	VkDescriptorSet sets[] = {
		vk.desc_set_vertex_buffer[vk.current_frame_index].set,
		vk_rtx_get_current_desc_set_textures(),
		vk.imageDescriptor.set,
		vk.desc_set_ubo,
		rtx_fsr.set
	};

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rtx_fsr.pipeline_layout, 0, ARRAY_LEN( sets ), sets, 0, NULL );
}

static vkFsrImage_t rtx_fsr_image( const vkimage_t *image, VkFormat format, uint32_t width, uint32_t height, uint32_t state )
{
	vkFsrImage_t out;

	out.image = image->handle;
	out.view = image->view;
	out.format = format;
	out.width = width;
	out.height = height;
	out.usage_state = state;
	return out;
}

// Records the FSR dispatch. The inputs go to SHADER_READ_ONLY_OPTIMAL for FSR and back to GENERAL.
static qboolean rtx_fsr_dispatch( VkCommandBuffer cmd )
{
	const uint32_t	rw = vk.extent_render.width;
	const uint32_t	rh = vk.extent_render.height;
	VkImage			inputs[4];
	vkFsrImage_t	color, depth, motion, reactive, output;
	float			frame_ms;
	int				now, i;
	qboolean		ok;

	inputs[0] = vk.img_rtx[RTX_IMG_FLAT_COLOR].handle;
	inputs[1] = vk.img_rtx[RTX_IMG_FLAT_MOTION].handle;
	inputs[2] = rtx_fsr.depth.handle;
	inputs[3] = rtx_fsr.reactive.handle;

	color = rtx_fsr_image( &vk.img_rtx[RTX_IMG_FLAT_COLOR], VK_FORMAT_R16G16B16A16_SFLOAT, rw, rh, RTX_FSR_STATE_READ );
	motion = rtx_fsr_image( &vk.img_rtx[RTX_IMG_FLAT_MOTION], VK_FORMAT_R16G16B16A16_SFLOAT, rw, rh, RTX_FSR_STATE_READ );
	depth = rtx_fsr_image( &rtx_fsr.depth, VK_FORMAT_R32_SFLOAT, rw, rh, RTX_FSR_STATE_READ );
	reactive = rtx_fsr_image( &rtx_fsr.reactive, VK_FORMAT_R32_SFLOAT, rw, rh, RTX_FSR_STATE_READ );
	output = rtx_fsr_image( &vk.img_rtx[RTX_IMG_TAA_OUTPUT], VK_FORMAT_R16G16B16A16_SFLOAT,
		vk.extent_taa_output.width, vk.extent_taa_output.height, RTX_FSR_STATE_UAV );

	now = ri.Milliseconds();
	frame_ms = ( rtx_fsr.prev_time != 0 && now > rtx_fsr.prev_time ) ? (float)( now - rtx_fsr.prev_time ) : 16.6f;
	frame_ms = Com_Clamp( 1.0f, 200.0f, frame_ms );
	rtx_fsr.prev_time = now;

	for ( i = 0; i < 4; i++ )
		rtx_fsr_barrier( cmd, inputs[i], VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	rtx_fsr_barrier( cmd, output.image, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );

	// FLAT_MOTION is in UV units, with previous = current + motion. FSR wants the same direction
	// in render pixels: the scale is the render size, with a positive sign.
	ok = vk_fsr_upscale_dispatch( cmd, &color, &depth, &motion, &reactive, &output,
		rtx_fsr.jitter_x, rtx_fsr.jitter_y, (float)rw, (float)rh, frame_ms,
		(float)FSR_DEPTH_NEAR, FLT_MAX, rtx_fsr.fov_y,
		( rtx_fsr.reset || !vk_rtx_taa_history_valid() ) ? qtrue : qfalse );

	for ( i = 0; i < 4; i++ )
		rtx_fsr_barrier( cmd, inputs[i], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
	rtx_fsr_barrier( cmd, output.image, VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL );

	if ( ok )
		rtx_fsr.reset = qfalse;

	return ok;
}

/*
===============
vk_rtx_fsr_record

Runs where vk_rtx_taa runs, in a frame that uses FSR. When FSR cannot run,
asvgf_taau runs in its place, so the frame is never black.
===============
*/
void vk_rtx_fsr_record( VkCommandBuffer cmd )
{
	qboolean ok;

	if ( !rtx_fsr.frame || rtx_fsr.prepare_pipeline == VK_NULL_HANDLE || rtx_fsr.fx_pipeline == VK_NULL_HANDLE ||
		rtx_fsr.depth.handle == VK_NULL_HANDLE || !vk_fsr_upscale_active() ) {
		vk_rtx_taa( cmd );
		return;
	}

	BEGIN_PERF_MARKER( cmd, PROFILER_ASVGF_TAA );

	if ( !rtx_fsr.images_ready ) {
		rtx_fsr_barrier( cmd, rtx_fsr.depth.handle, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );
		rtx_fsr_barrier( cmd, rtx_fsr.reactive.handle, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );
		rtx_fsr.images_ready = qtrue;
	}

	// Depth and reactive mask, at render size.
	rtx_fsr_bind( cmd, rtx_fsr.prepare_pipeline );
	qvkCmdDispatch( cmd, ( vk.extent_render.width + 15 ) / 16, ( vk.extent_render.height + 15 ) / 16, 1 );
	rtx_fsr.depth_written = qtrue;

	ok = rtx_fsr_dispatch( cmd );

	if ( ok ) {
		// TAA_FX, TAA_A and the readback, on the whole TAA image as asvgf_taau does.
		rtx_fsr_bind( cmd, rtx_fsr.fx_pipeline );
		qvkCmdDispatch( cmd, ( vk.extent_taa_images.width + 15 ) / 16, ( vk.extent_taa_images.height + 15 ) / 16, 1 );

		BARRIER_COMPUTE( cmd, vk.img_rtx[RTX_IMG_TAA_FX] );
		BARRIER_COMPUTE( cmd, vk.img_rtx[RTX_IMG_TAA_A] );
		BARRIER_COMPUTE( cmd, vk.img_rtx[RTX_IMG_TAA_OUTPUT] );
	}

	END_PERF_MARKER( cmd, PROFILER_ASVGF_TAA );

	if ( !ok )
		vk_rtx_taa( cmd );
}

/*
===============
vk_rtx_fsr_framegen

Runs after the TAA or FSR pass of a traced world view. Frame generation reads the device depth
of fsr_prepare.comp and FLAT_MOTION, both at render size. Without the upscaler in this frame,
fsr_prepare.comp runs here to make the depth.
===============
*/
void vk_rtx_fsr_framegen( VkCommandBuffer cmd, const vkUniformRTX_t *ubo, const trRefdef_t *refdef )
{
	const uint32_t	rw = vk.extent_render.width;
	const uint32_t	rh = vk.extent_render.height;
	const qboolean	depth_written = rtx_fsr.depth_written;
	VkImage			inputs[2];
	vkFsrFgFrame_t	f;
	int				now, i;

	rtx_fsr.depth_written = qfalse;

	if ( !vk_fsr_framegen_active() || ( refdef->rdflags & ( RDF_NOWORLDMODEL | RDF_SKYBOXPORTAL ) ) )
		return;
	if ( rtx_fsr.prepare_pipeline == VK_NULL_HANDLE || rtx_fsr.set == VK_NULL_HANDLE ) {
		rtx_fsr.fg_history_valid = qfalse;
		return;
	}

	if ( !depth_written ) {
		// The same size as vk_rtx_fsr_update gives, so an FSR context keeps its images.
		rtx_fsr_ensure_images( MAX( rw, ( vk.extent_unscaled.width + 1 ) & ~1 ), MAX( rh, vk.extent_unscaled.height ) );
		if ( !rtx_fsr.images_ready ) {
			rtx_fsr_barrier( cmd, rtx_fsr.depth.handle, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );
			rtx_fsr_barrier( cmd, rtx_fsr.reactive.handle, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );
			rtx_fsr.images_ready = qtrue;
		}
		rtx_fsr_bind( cmd, rtx_fsr.prepare_pipeline );
		qvkCmdDispatch( cmd, ( rw + 15 ) / 16, ( rh + 15 ) / 16, 1 );
	}

	Com_Memset( &f, 0, sizeof( f ) );
	f.depth = rtx_fsr_image( &rtx_fsr.depth, VK_FORMAT_R32_SFLOAT, rw, rh, RTX_FSR_STATE_READ );
	f.motion = rtx_fsr_image( &vk.img_rtx[RTX_IMG_FLAT_MOTION], VK_FORMAT_R16G16B16A16_SFLOAT, rw, rh, RTX_FSR_STATE_READ );
	f.renderW = rw;
	f.renderH = rh;

	// FSR puts the sample of a pixel at centre - jitter; the tracer at centre + sub_pixel_jitter.
	f.jitterX = depth_written ? rtx_fsr.jitter_x : -ubo->sub_pixel_jitter[0];
	f.jitterY = depth_written ? rtx_fsr.jitter_y : -ubo->sub_pixel_jitter[1];
	// FLAT_MOTION is in UV units with previous = current + motion (see rtx_fsr_dispatch).
	f.mvScaleX = (float)rw;
	f.mvScaleY = (float)rh;

	now = ri.Milliseconds();
	f.frameTimeMs = ( rtx_fsr.fg_prev_time != 0 && now > rtx_fsr.fg_prev_time ) ? (float)( now - rtx_fsr.fg_prev_time ) : 16.6f;
	f.frameTimeMs = Com_Clamp( 1.0f, 200.0f, f.frameTimeMs );
	rtx_fsr.fg_prev_time = now;

	f.cameraNear = (float)FSR_DEPTH_NEAR;
	f.cameraFar = FLT_MAX;
	f.fovY = DEG2RAD( refdef->fov_y );
	VectorCopy( refdef->vieworg, f.origin );
	VectorCopy( refdef->viewaxis[0], f.forward );
	VectorScale( refdef->viewaxis[1], -1.0f, f.right );	// viewaxis[1] points left
	VectorCopy( refdef->viewaxis[2], f.up );

	// Frame generation has its own history: TAA has none without a denoiser and FSR upscaling.
	f.reset = rtx_fsr.fg_history_valid ? qfalse : qtrue;
	if ( rtx_fsr.fg_prev_origin_valid && Distance( refdef->vieworg, rtx_fsr.fg_prev_origin ) > RTX_FSR_CUT_DISTANCE )
		f.reset = qtrue;
	VectorCopy( refdef->vieworg, rtx_fsr.fg_prev_origin );
	rtx_fsr.fg_prev_origin_valid = qtrue;

	inputs[0] = rtx_fsr.depth.handle;
	inputs[1] = vk.img_rtx[RTX_IMG_FLAT_MOTION].handle;

	for ( i = 0; i < 2; i++ )
		rtx_fsr_barrier( cmd, inputs[i], VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	vk_fsr_framegen_frame( cmd, &f );
	rtx_fsr.fg_history_valid = qtrue;

	for ( i = 0; i < 2; i++ )
		rtx_fsr_barrier( cmd, inputs[i], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
}

/*
===============
vk_rtx_fsr_create_pipelines

Runs with the other compute pipelines. Set 4 holds the depth and the reactive mask.
===============
*/
void vk_rtx_fsr_create_pipelines( void )
{
	VkDescriptorSetLayoutBinding	bindings[2];
	VkDescriptorSetLayoutCreateInfo	set_info;
	VkDescriptorPoolSize			pool_size;
	VkDescriptorPoolCreateInfo		pool_info;
	VkDescriptorSetAllocateInfo		alloc;
	VkPipelineLayoutCreateInfo		layout_info;
	VkComputePipelineCreateInfo		pipe_info[2];
	VkPipeline						pipes[2];
	uint32_t						i;

	vk_rtx_fsr_destroy_pipelines();

	// r_fsr and r_fsrFrameGen are latched: with both off nothing here is needed.
	if ( ( r_fsr == NULL || !r_fsr->integer ) && !vk_fsr_framegen_wanted() )
		return;

	Com_Memset( bindings, 0, sizeof( bindings ) );
	for ( i = 0; i < ARRAY_LEN( bindings ); i++ ) {
		bindings[i].binding = i;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}

	Com_Memset( &set_info, 0, sizeof( set_info ) );
	set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set_info.bindingCount = ARRAY_LEN( bindings );
	set_info.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &set_info, NULL, &rtx_fsr.set_layout ) );

	pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	pool_size.descriptorCount = ARRAY_LEN( bindings );

	Com_Memset( &pool_info, 0, sizeof( pool_info ) );
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = 1;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_info, NULL, &rtx_fsr.pool ) );

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = rtx_fsr.pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &rtx_fsr.set_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &rtx_fsr.set ) );

	{
		VkDescriptorSetLayout set_layouts[] = {
			vk.desc_set_vertex_buffer[0].layout,
			vk.desc_set_layout_textures,
			vk.imageDescriptor.layout,
			vk.desc_set_layout_ubo,
			rtx_fsr.set_layout
		};

		Com_Memset( &layout_info, 0, sizeof( layout_info ) );
		layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_info.setLayoutCount = ARRAY_LEN( set_layouts );
		layout_info.pSetLayouts = set_layouts;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &layout_info, NULL, &rtx_fsr.pipeline_layout ) );
	}

	Com_Memset( pipe_info, 0, sizeof( pipe_info ) );
	for ( i = 0; i < ARRAY_LEN( pipe_info ); i++ ) {
		pipe_info[i].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipe_info[i].stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipe_info[i].stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipe_info[i].stage.pName = "main";
		pipe_info[i].layout = rtx_fsr.pipeline_layout;
	}
	pipe_info[0].stage.module = vk.compute_shader[SHADER_FSR_PREPARE_COMP]->modules[0];
	pipe_info[1].stage.module = vk.compute_shader[SHADER_FSR_FX_COMP]->modules[0];

	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, ARRAY_LEN( pipe_info ), pipe_info, NULL, pipes ) );
	rtx_fsr.prepare_pipeline = pipes[0];
	rtx_fsr.fx_pipeline = pipes[1];

	rtx_fsr_write_descriptors();
}

// RE_Shutdown and vk_rtx_shutdown both come here. The second call finds nothing.
void vk_rtx_fsr_destroy_pipelines( void )
{
	if ( rtx_fsr.prepare_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, rtx_fsr.prepare_pipeline, NULL );
		rtx_fsr.prepare_pipeline = VK_NULL_HANDLE;
	}
	if ( rtx_fsr.fx_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, rtx_fsr.fx_pipeline, NULL );
		rtx_fsr.fx_pipeline = VK_NULL_HANDLE;
	}
	if ( rtx_fsr.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, rtx_fsr.pipeline_layout, NULL );
		rtx_fsr.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( rtx_fsr.pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, rtx_fsr.pool, NULL );	// frees the set
		rtx_fsr.pool = VK_NULL_HANDLE;
		rtx_fsr.set = VK_NULL_HANDLE;
	}
	if ( rtx_fsr.set_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, rtx_fsr.set_layout, NULL );
		rtx_fsr.set_layout = VK_NULL_HANDLE;
	}
}

/*
===============
vk_rtx_fsr_shutdown

Runs in vk_rtx_shutdown, before the device goes. Destroys the context and the images.
===============
*/
void vk_rtx_fsr_shutdown( void )
{
	vk_fsr_upscale_destroy();
	vk_rtx_fsr_destroy_pipelines();
	rtx_fsr_destroy_images();
	Com_Memset( &rtx_fsr, 0, sizeof( rtx_fsr ) );
}
