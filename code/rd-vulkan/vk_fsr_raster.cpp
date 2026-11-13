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

// AMD FSR temporal upscaling on the raster path (r_fsr 1, r_rtx 0).
// The world view and its portals render at the render size into the scene images.
// At the split, FSR writes vk.color_image at the display size. The 2D, the later 3D views,
// the glow blend, the bloom and the gamma pass then run at the display size.

#include "tr_local.h"
#include "vk_fsr.h"

#ifdef _WIN32
#	include "ffx_upscale.h"
#	define FSRR_STATE_READ	FFX_API_RESOURCE_STATE_COMPUTE_READ
#	define FSRR_STATE_UAV	FFX_API_RESOURCE_STATE_UNORDERED_ACCESS
#else
#	define FSRR_STATE_READ	0
#	define FSRR_STATE_UAV	0
#endif

// LDR colour, standard depth [0..1] with a finite far plane: no context flags.
#define FSRR_FLAGS			0

// A jump of the view origin larger than this in one frame is a camera cut.
#define FSRR_CUT_DISTANCE	256.0f

static struct {
	qboolean	primary;			// the current view is the world view that FSR upscales
	qboolean	viewJittered;		// backEnd.viewParms holds the jittered projection
	float		unjittered[16];		// the projection of the current view without the jitter
	float		ndcX, ndcY;			// jitter of the current view, in NDC
	float		jitterX, jitterY;	// jitter of this frame, in render pixels

	qboolean	armed;				// the world view is done: the split comes at the next 2D
	qboolean	opaqueCopied;		// fsr_opaque_image holds the colour before the blended surfaces
	qboolean	glowPending;		// the glow is blurred and waits for the blend after FSR

	qboolean	reset;
	int			jitterIndex;
	int			prevTime;
	vec3_t		prevOrigin;
	qboolean	prevOriginValid;

	vkFsrRasterFrame_t	frame;		// the camera data of the last split, for frame generation
} fsrr;

/*
===============
vk_fsr_raster_forced

r_fsr 1 or frame generation on the raster path. Then r_fbo, r_depthPrepass and r_velocityBuffer
are on, and MSAA, r_ext_supersample and r_renderScale are off. The cvars are latched, so the
result is constant.
===============
*/
qboolean vk_fsr_raster_forced( void )
{
#ifdef _WIN32
	if ( ( r_fsr == NULL || !r_fsr->integer ) && !vk_fsr_framegen_wanted() )
		return qfalse;
	if ( r_rtx != NULL && r_rtx->integer )
		return qfalse;
	// A missing DLL prints one warning and keeps the user settings.
	return vk_fsr_load_library();
#else
	return qfalse;
#endif
}

uint32_t vk_scene_width( void )
{
	return vk.fsrRaster ? vk.fsrRenderWidth : (uint32_t)glConfig.vidWidth;
}

uint32_t vk_scene_height( void )
{
	return vk.fsrRaster ? vk.fsrRenderHeight : (uint32_t)glConfig.vidHeight;
}

uint32_t vk_dglow_width( void )
{
	return vk.fsrRaster ? vk.fsrRenderWidth : (uint32_t)gls.captureWidth;
}

uint32_t vk_dglow_height( void )
{
	return vk.fsrRaster ? vk.fsrRenderHeight : (uint32_t)gls.captureHeight;
}

static qboolean fsrr_storage_format( VkFormat format )
{
	VkFormatProperties props;

	qvkGetPhysicalDeviceFormatProperties( vk.physical_device, format, &props );
	return ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT ) ? qtrue : qfalse;
}

/*
===============
vk_fsr_raster_init

Runs in vk_initialize, before the attachments. Creates the upscaler context and sets the render
size. When FSR cannot run, vk.fsrRaster stays qfalse and every image keeps the display size.
===============
*/
void vk_fsr_raster_init( void )
{
	const qboolean upscale = ( r_fsr != NULL && r_fsr->integer ) ? qtrue : qfalse;
	uint32_t w, h;

	Com_Memset( &fsrr, 0, sizeof( fsrr ) );
	vk.fsrRaster = qfalse;
	vk.fsrUpscaleRaster = qfalse;
	vk.fsrScene = qfalse;
	vk.fsrMipBias = 0.0f;

	if ( !vk_fsr_raster_forced() || vk.rtxActive )
		return;

	ri.Printf( PRINT_ALL, "FSR: %s forces r_fbo, r_depthPrepass and r_velocityBuffer on, and MSAA, r_ext_supersample and r_renderScale off\n",
		upscale ? "r_fsr" : "r_fsrFrameGen" );

	if ( !vk.fboActive || !vk.gbufferActive || !vk.velocityActive || !vk.gbufferDepthSampled ) {
		ri.Printf( PRINT_WARNING, "FSR: the raster path has no G-buffer depth or motion vectors. FSR is off.\n" );
		return;
	}

	if ( !vk_fsr_format_supported( vk.gbuffer_depth_format ) ) {
		ri.Printf( PRINT_WARNING, "FSR: the SDK has no support for the G-buffer depth format. FSR is off.\n" );
		return;
	}

	w = glConfig.vidWidth;
	h = glConfig.vidHeight;

	if ( upscale ) {
		// The SDK has no R16G16B16A16_UNORM (r_hdr 1): FSR then uses copies in R16G16B16A16_SFLOAT.
		vk.fsrColorDirect = ( vk_fsr_format_supported( vk.color_format ) && fsrr_storage_format( vk.color_format ) ) ? qtrue : qfalse;
		vk.fsrColorFormat = vk.fsrColorDirect ? vk.color_format : VK_FORMAT_R16G16B16A16_SFLOAT;

		if ( !fsrr_storage_format( vk.fsrColorFormat ) ) {
			ri.Printf( PRINT_WARNING, "FSR: the device has no storage image format for FSR. FSR upscaling is off.\n" );
		} else {
			vk_fsr_get_render_size( glConfig.vidWidth, glConfig.vidHeight, &w, &h );

			// vk_fsr prints the warning when the library, the device or the context fails.
			if ( vk_fsr_upscale_create( w, h, glConfig.vidWidth, glConfig.vidHeight, FSRR_FLAGS ) && vk_fsr_upscale_active() )
				vk.fsrUpscaleRaster = qtrue;
		}
	}

	// Frame generation alone: the split runs at the native size, with no upscaler.
	if ( !vk.fsrUpscaleRaster ) {
		if ( !vk_fsr_framegen_wanted() )
			return;
		w = glConfig.vidWidth;
		h = glConfig.vidHeight;
		vk.fsrColorDirect = qfalse;
	}

	vk.fsrRaster = qtrue;
	vk.fsrRenderWidth = w;
	vk.fsrRenderHeight = h;
	vk.fsrMipBias = vk.fsrUpscaleRaster ? vk_fsr_mip_bias() : 0.0f;
	fsrr.reset = qtrue;

	if ( vk.fsrUpscaleRaster )
		ri.Printf( PRINT_ALL, "FSR: the raster world view renders at %ux%u and upscales to %ux%u\n",
			w, h, glConfig.vidWidth, glConfig.vidHeight );
	else
		ri.Printf( PRINT_ALL, "FSR: the raster world view renders at %ux%u for frame generation, no upscaling\n", w, h );
}

// The command list of this frame has a world view that FSR upscales.
static qboolean fsrr_find_world_view( void )
{
	const void *curCmd = &backEndDataBE->commands.cmds;

	for ( ;; ) {
		curCmd = PADP( curCmd, sizeof( void * ) );

		switch ( *(const int *)curCmd ) {
		case RC_SET_COLOR:
			curCmd = (const void *)( (const setColorCommand_t *)curCmd + 1 );
			break;
		case RC_STRETCH_PIC:
			curCmd = (const void *)( (const stretchPicCommand_t *)curCmd + 1 );
			break;
		case RC_ROTATE_PIC:
		case RC_ROTATE_PIC2:
			curCmd = (const void *)( (const rotatePicCommand_t *)curCmd + 1 );
			break;
		case RC_DRAW_SURFS: {
			const drawSurfsCommand_t *ds = (const drawSurfsCommand_t *)curCmd;

			if ( ds->viewParms.portalView == PV_NONE && !( ds->refdef.rdflags & ( RDF_NOWORLDMODEL | RDF_SKYBOXPORTAL ) )
#ifdef VK_CUBEMAP
				&& !ds->viewParms.targetCube
#endif
				)
				return qtrue;
			curCmd = (const void *)( ds + 1 );
			break;
		}
		case RC_DRAW_BUFFER:
		case RC_WORLD_EFFECTS:
		case RC_AUTO_MAP:
			curCmd = (const void *)( (const drawBufferCommand_t *)curCmd + 1 );
			break;
		case RC_VIDEOFRAME:
			curCmd = (const void *)( (const videoFrameCommand_t *)curCmd + 1 );
			break;
		case RC_CLEARCOLOR:
			curCmd = (const void *)( (const clearColorCommand_t *)curCmd + 1 );
			break;
#ifdef VK_CUBEMAP
		case RC_CONVOLVECUBEMAP:
			curCmd = (const void *)( (const convolveCubemapCommand_t *)curCmd + 1 );
			break;
#endif
		default:	// RC_SWAP_BUFFERS ends the frame, RC_END_OF_LIST the list
			return qfalse;
		}
	}
}

/*
===============
vk_fsr_raster_begin_frame

Runs in vk_begin_frame, before the first render pass. A frame with a world view starts in the
scene images. A frame with no world view (menu, console, cinematic) stays at the display size.
===============
*/
void vk_fsr_raster_begin_frame( void )
{
	vk.fsrScene = qfalse;
	fsrr.primary = qfalse;
	fsrr.viewJittered = qfalse;
	fsrr.armed = qfalse;
	fsrr.opaqueCopied = qfalse;
	fsrr.glowPending = qfalse;

	if ( !vk.fsrRaster )
		return;

	// RE_Shutdown destroys the context at each map change. The sizes do not change, so the images stay.
	if ( vk.fsrUpscaleRaster && !vk_fsr_upscale_active() ) {
		if ( vk_fsr_upscale_create( vk.fsrRenderWidth, vk.fsrRenderHeight, glConfig.vidWidth, glConfig.vidHeight, FSRR_FLAGS )
			&& vk_fsr_upscale_active() ) {
			fsrr.reset = qtrue;
			fsrr.prevTime = 0;
		}
	}

	if ( !fsrr_find_world_view() )
		return;

	vk.fsrScene = qtrue;

	// Without a context the scene is not jittered; the split then stretches it.
	vk_fsr_get_jitter( fsrr.jitterIndex, &fsrr.jitterX, &fsrr.jitterY );
}

/*
===============
vk_fsr_raster_begin_view

Runs in RB_DrawSurfs after backEnd.viewParms is set. Puts the jitter into the projection of
every world view in the scene images (portals, mirrors and sky portal too). FSR puts the sample
of pixel c at c - jitter: the image moves by +jitter pixels.
===============
*/
void vk_fsr_raster_begin_view( void )
{
	float *p = backEnd.viewParms.projectionMatrix;
	float vw, vh;

	fsrr.primary = qfalse;
	fsrr.viewJittered = qfalse;

	if ( !vk.fsrScene || ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) )
		return;
#ifdef VK_CUBEMAP
	if ( backEnd.viewParms.targetCube )
		return;
#endif

	Com_Memcpy( fsrr.unjittered, p, sizeof( fsrr.unjittered ) );

	vw = (float)backEnd.viewParms.viewportWidth * (float)vk.fsrRenderWidth / (float)glConfig.vidWidth;
	vh = (float)backEnd.viewParms.viewportHeight * (float)vk.fsrRenderHeight / (float)glConfig.vidHeight;

	if ( ( fsrr.jitterX != 0.0f || fsrr.jitterY != 0.0f ) && vw > 0.0f && vh > 0.0f ) {
		// Clip w is -z_eye, so P[8] and P[9] move NDC by exactly -P[8] and -P[9]. Vulkan NDC y points down.
		fsrr.ndcX = 2.0f * fsrr.jitterX / vw;
		fsrr.ndcY = 2.0f * fsrr.jitterY / vh;
		p[8] -= fsrr.ndcX;
		p[9] -= fsrr.ndcY;
		fsrr.viewJittered = qtrue;
	}

	// The screenmap pass draws a copy of the views first. FSR takes the copy in the main pass.
	if ( backEnd.viewParms.portalView != PV_NONE || ( backEnd.refdef.rdflags & RDF_SKYBOXPORTAL )
		|| vk.renderPassIndex == RENDER_PASS_SCREENMAP || fsrr.armed )
		return;

	fsrr.primary = qtrue;
	fsrr.frame.fovY = DEG2RAD( backEnd.refdef.fov_y );
	fsrr.frame.cameraNear = backEnd.viewParms.zNear;
	fsrr.frame.cameraFar = backEnd.viewParms.zFar;
	VectorCopy( backEnd.viewParms.ori.origin, fsrr.frame.origin );
	VectorCopy( backEnd.viewParms.ori.axis[0], fsrr.frame.forward );
	VectorScale( backEnd.viewParms.ori.axis[1], -1.0f, fsrr.frame.right );	// axis[1] points left
	VectorCopy( backEnd.viewParms.ori.axis[2], fsrr.frame.up );

	if ( !vk_world.prevViewValid )
		fsrr.reset = qtrue;
	if ( fsrr.prevOriginValid && Distance( backEnd.viewParms.ori.origin, fsrr.prevOrigin ) > FSRR_CUT_DISTANCE )
		fsrr.reset = qtrue;
	VectorCopy( backEnd.viewParms.ori.origin, fsrr.prevOrigin );
	fsrr.prevOriginValid = qtrue;
}

qboolean vk_fsr_raster_primary_view( void )
{
	return fsrr.primary;
}

void vk_fsr_raster_end_primary_view( void )
{
	if ( fsrr.primary )
		fsrr.armed = qtrue;
}

// Adds the jitter of the current view to a copy of the previous projection. The motion vectors
// (current minus previous position) then have no jitter.
void vk_fsr_raster_jitter_projection( float *proj )
{
	if ( !fsrr.viewJittered )
		return;
	proj[8] -= fsrr.ndcX;
	proj[9] -= fsrr.ndcY;
}

// The projection of the current view without the jitter, for the motion vectors of the next frame.
void vk_fsr_raster_unjittered_projection( float *proj )
{
	Com_Memcpy( proj, fsrr.viewJittered ? fsrr.unjittered : backEnd.viewParms.projectionMatrix, 64 );
}

static void fsrr_barrier( VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkAccessFlags src, VkAccessFlags dst,
	VkImageLayout from, VkImageLayout to )
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
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );
}

static void fsrr_clear_display_depth( VkCommandBuffer cmd );

#define FSRR_ANY_WRITE		( VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT )
#define FSRR_ANY_READ		( VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT )

// Copies src into dst with a blit: it scales and converts the format. Both images rest in
// SHADER_READ_ONLY_OPTIMAL before and after.
static void fsrr_blit_image( VkCommandBuffer cmd, VkImage src, uint32_t sw, uint32_t sh, VkImage dst, uint32_t dw, uint32_t dh, VkFilter filter )
{
	VkImageBlit region;

	fsrr_barrier( cmd, src, VK_IMAGE_ASPECT_COLOR_BIT, FSRR_ANY_WRITE, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	fsrr_barrier( cmd, dst, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

	Com_Memset( &region, 0, sizeof( region ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.srcOffsets[1].x = sw;
	region.srcOffsets[1].y = sh;
	region.srcOffsets[1].z = 1;
	region.dstSubresource = region.srcSubresource;
	region.dstOffsets[1].x = dw;
	region.dstOffsets[1].y = dh;
	region.dstOffsets[1].z = 1;
	qvkCmdBlitImage( cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, filter );

	fsrr_barrier( cmd, src, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_READ_BIT, FSRR_ANY_READ,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	fsrr_barrier( cmd, dst, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, FSRR_ANY_READ,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
}

/*
===============
vk_fsr_raster_copy_opaque

Runs in RB_RenderDrawSurfList of the FSR view, where the sort order leaves SS_BANNER.
Copies the scene colour before the blended surfaces, for the reactive mask.
===============
*/
void vk_fsr_raster_copy_opaque( void )
{
	if ( !fsrr.primary || fsrr.opaqueCopied || backEnd.isGlowPass || backEnd.refractionFill )
		return;
	if ( vk.renderPassIndex != RENDER_PASS_MAIN || !vk_fsr_upscale_active() )
		return;

	vk_end_render_pass();

	fsrr_blit_image( vk.cmd->command_buffer, vk.fsr_scene_image, vk.fsrRenderWidth, vk.fsrRenderHeight,
		vk.fsr_opaque_image, vk.fsrRenderWidth, vk.fsrRenderHeight, VK_FILTER_NEAREST );

	vk_begin_main_render_pass( qfalse );

	fsrr.opaqueCopied = qtrue;
}

void vk_fsr_raster_set_glow_pending( void )
{
	fsrr.glowPending = qtrue;
}

qboolean vk_fsr_raster_split_pending( void )
{
	return ( vk.fsrScene && fsrr.armed ) ? qtrue : qfalse;
}

static vkFsrImage_t fsrr_image( VkImage image, VkImageView view, VkFormat format, uint32_t w, uint32_t h, uint32_t state )
{
	vkFsrImage_t out;

	out.image = image;
	out.view = view;
	out.format = format;
	out.width = w;
	out.height = h;
	out.usage_state = state;
	return out;
}

// Draws the reactive mask (fsr_reactive.frag) from the opaque copy and the scene colour.
// Blended surfaces get 0.9 where they change the colour by more than the threshold.
static qboolean fsrr_reactive_mask( VkCommandBuffer cmd )
{
	VkRenderPassBeginInfo	begin;
	VkDescriptorSet			sets[2];

	if ( vk.fsr_reactive_pipeline == VK_NULL_HANDLE )
		return qfalse;

	// The scene colour comes from the scene pass; the reactive image was last read by FSR.
	fsrr_barrier( cmd, vk.fsr_scene_image, VK_IMAGE_ASPECT_COLOR_BIT, FSRR_ANY_WRITE, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	fsrr_barrier( cmd, vk.fsr_reactive_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	Com_Memset( &begin, 0, sizeof( begin ) );
	begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	begin.renderPass = vk.render_pass.fsr_reactive;
	begin.framebuffer = vk.framebuffers.fsr_reactive;
	begin.renderArea.extent.width = vk.fsrRenderWidth;
	begin.renderArea.extent.height = vk.fsrRenderHeight;
	qvkCmdBeginRenderPass( cmd, &begin, VK_SUBPASS_CONTENTS_INLINE );

	sets[0] = vk.fsr_opaque_descriptor;
	sets[1] = vk.fsr_scene_descriptor;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.fsr_reactive_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_blend, 0, ARRAY_LEN( sets ), sets, 0, NULL );
	qvkCmdDraw( cmd, 4, 1, 0, 0 );

	qvkCmdEndRenderPass( cmd );

	fsrr_barrier( cmd, vk.fsr_reactive_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	return qtrue;
}

// Records the reactive mask and the upscale into vk.color_image. FSR reads its inputs in
// SHADER_READ_ONLY_OPTIMAL and writes its output in GENERAL; all images go back to their rest
// layouts after. Returns qfalse when nothing is upscaled.
static qboolean fsrr_dispatch( VkCommandBuffer cmd )
{
	const uint32_t	rw = vk.fsrRenderWidth;
	const uint32_t	rh = vk.fsrRenderHeight;
	const uint32_t	dw = glConfig.vidWidth;
	const uint32_t	dh = glConfig.vidHeight;
	const VkImage	input = vk.fsrColorDirect ? vk.fsr_scene_image : vk.fsr_input_image;
	const VkImage	target = vk.fsrColorDirect ? vk.color_image : vk.fsr_output_image;
	vkFsrImage_t	color, depth, motion, reactive, output;
	qboolean		haveReactive = qfalse;
	qboolean		ok;

	// The SDK has no image view for the colour format: convert the input to fsrColorFormat.
	if ( !vk.fsrColorDirect )
		fsrr_blit_image( cmd, vk.fsr_scene_image, rw, rh, vk.fsr_input_image, rw, rh, VK_FILTER_NEAREST );

	color = fsrr_image( input, vk.fsrColorDirect ? vk.fsr_scene_image_view : vk.fsr_input_image_view, vk.fsrColorFormat, rw, rh, FSRR_STATE_READ );
	depth = fsrr_image( vk.gbuffer_depth_image, vk.gbuffer_depth_image_view, vk.gbuffer_depth_format, rw, rh, FSRR_STATE_READ );
	motion = fsrr_image( vk.gbuffer_velocity_image, vk.gbuffer_velocity_image_view, vk.velocity_format, rw, rh, FSRR_STATE_READ );
	output = fsrr_image( target, vk.fsrColorDirect ? vk.color_image_view : vk.fsr_output_image_view, vk.fsrColorFormat, dw, dh, FSRR_STATE_UAV );

	// The colour and the motion vectors come from render passes or copies: make the writes visible to compute.
	fsrr_barrier( cmd, input, VK_IMAGE_ASPECT_COLOR_BIT, FSRR_ANY_WRITE, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	fsrr_barrier( cmd, vk.gbuffer_velocity_image, VK_IMAGE_ASPECT_COLOR_BIT, FSRR_ANY_WRITE, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	if ( fsrr.opaqueCopied )
		haveReactive = fsrr_reactive_mask( cmd );
	reactive = fsrr_image( vk.fsr_reactive_image, vk.fsr_reactive_image_view, VK_FORMAT_R8_UNORM, rw, rh, FSRR_STATE_READ );

	fsrr_barrier( cmd, vk.gbuffer_depth_image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	fsrr_barrier( cmd, target, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );

	// fsrr.frame is set for this split by fsrr_update_frame.
	ok = vk_fsr_upscale_dispatch( cmd, &color, &depth, &motion, haveReactive ? &reactive : NULL, &output,
		fsrr.frame.jitterX, fsrr.frame.jitterY, fsrr.frame.mvScaleX, fsrr.frame.mvScaleY, fsrr.frame.frameTimeMs,
		fsrr.frame.cameraNear, fsrr.frame.cameraFar, fsrr.frame.fovY, fsrr.frame.reset );

	fsrr_barrier( cmd, vk.gbuffer_depth_image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
	fsrr_barrier( cmd, target, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_SHADER_WRITE_BIT, FSRR_ANY_READ | VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	if ( !ok )
		return qfalse;

	// Back to the colour format. The conversion clamps to [0..1], as the colour image does.
	if ( !vk.fsrColorDirect )
		fsrr_blit_image( cmd, vk.fsr_output_image, dw, dh, vk.color_image, dw, dh, VK_FILTER_NEAREST );

	fsrr.jitterIndex++;
	return qtrue;
}

// The camera data of this split, for the upscaler and frame generation. Runs in every split
// with a world view, also when the upscale does not run.
static void fsrr_update_frame( void )
{
	const int now = ri.Milliseconds();

	fsrr.frame.frameTimeMs = ( fsrr.prevTime != 0 && now > fsrr.prevTime ) ? (float)( now - fsrr.prevTime ) : 16.6f;
	fsrr.frame.frameTimeMs = Com_Clamp( 1.0f, 200.0f, fsrr.frame.frameTimeMs );
	fsrr.prevTime = now;

	// The jitter that is in the projection of this frame (0, 0 without the upscaler).
	fsrr.frame.jitterX = fsrr.viewJittered ? fsrr.jitterX : 0.0f;
	fsrr.frame.jitterY = fsrr.viewJittered ? fsrr.jitterY : 0.0f;
	// The G-buffer velocity is current minus previous position in Vulkan NDC (y down).
	// FSR wants previous minus current in render pixels.
	fsrr.frame.mvScaleX = -0.5f * (float)vk.fsrRenderWidth;
	fsrr.frame.mvScaleY = -0.5f * (float)vk.fsrRenderHeight;
	fsrr.frame.reset = fsrr.reset;
}

// Frame generation: configure and prepare from the G-buffer depth and velocity of this split,
// then the HUD-less copy. A display-size main pass is open and the glow is blended.
static void fsrr_framegen( VkCommandBuffer cmd )
{
	vkFsrFgFrame_t	f;
	qboolean		enabled;

	if ( !vk_fsr_framegen_active() )
		return;

	Com_Memset( &f, 0, sizeof( f ) );
	vk_fsr_raster_depth_image( &f.depth );
	vk_fsr_raster_motion_image( &f.motion );
	f.renderW = vk.fsrRenderWidth;
	f.renderH = vk.fsrRenderHeight;
	f.jitterX = fsrr.frame.jitterX;
	f.jitterY = fsrr.frame.jitterY;
	f.mvScaleX = fsrr.frame.mvScaleX;
	f.mvScaleY = fsrr.frame.mvScaleY;
	f.frameTimeMs = fsrr.frame.frameTimeMs;
	f.cameraNear = fsrr.frame.cameraNear;
	f.cameraFar = fsrr.frame.cameraFar;
	f.fovY = fsrr.frame.fovY;
	VectorCopy( fsrr.frame.origin, f.origin );
	VectorCopy( fsrr.frame.forward, f.forward );
	VectorCopy( fsrr.frame.right, f.right );
	VectorCopy( fsrr.frame.up, f.up );
	f.reset = fsrr.frame.reset;

	vk_end_render_pass();

	// FSR reads the depth as a sampled image. The velocity rests in SHADER_READ_ONLY_OPTIMAL.
	fsrr_barrier( cmd, vk.gbuffer_depth_image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	fsrr_barrier( cmd, vk.gbuffer_velocity_image, VK_IMAGE_ASPECT_COLOR_BIT, FSRR_ANY_WRITE, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	enabled = vk_fsr_framegen_frame( cmd, &f );

	fsrr_barrier( cmd, vk.gbuffer_depth_image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
	fsrr_barrier( cmd, vk.gbuffer_velocity_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_SHADER_READ_BIT, FSRR_ANY_WRITE,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	if ( enabled ) {
		// The bloom is part of the 3D scene. It runs here, before the 2D, and not again in this frame.
		if ( vk.bloomActive && !rb_portraitReady ) {
			vk_begin_main_render_pass( qfalse );
			vk.cmd->last_pipeline = VK_NULL_HANDLE;
			vk_bloom();
			vk_end_render_pass();
		}

		vk_fsr_framegen_hudless_copy( cmd );
	}

	vk_begin_main_render_pass( qfalse );
	fsrr_clear_display_depth( cmd );
}

// Clears the depth of the display-size main pass. The scene depth stays in the scene image.
static void fsrr_clear_display_depth( VkCommandBuffer cmd )
{
	VkClearAttachment attachment;
	VkClearRect rect;

	Com_Memset( &attachment, 0, sizeof( attachment ) );
	attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		attachment.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	attachment.clearValue.depthStencil.depth = 1.0f;

	Com_Memset( &rect, 0, sizeof( rect ) );
	rect.rect.extent.width = glConfig.vidWidth;
	rect.rect.extent.height = glConfig.vidHeight;
	rect.layerCount = 1;

	qvkCmdClearAttachments( cmd, 1, &attachment, 1, &rect );
}

/*
===============
vk_fsr_raster_after_world_view

The split point: runs once, at the first command after the world view, or in vk_end_frame.
Records the FSR dispatch (or a stretch copy) into vk.color_image and opens the display-size
main pass. Then records the frame generation prepare and the HUD-less copy.
===============
*/
void vk_fsr_raster_after_world_view( void )
{
	const VkCommandBuffer cmd = vk.cmd->command_buffer;
	const qboolean armed = fsrr.armed;
	uint32_t i;

	if ( !vk.fsrScene )
		return;

	if ( tess.numIndexes )
		RB_EndSurface();

	vk_end_render_pass();

	if ( armed )
		fsrr_update_frame();

	// No FSR in this frame: stretch the scene, so the frame is never black.
	if ( !armed || !vk.fsrUpscaleRaster || !vk_fsr_upscale_active() || !fsrr_dispatch( cmd ) )
		fsrr_blit_image( cmd, vk.fsr_scene_image, vk.fsrRenderWidth, vk.fsrRenderHeight,
			vk.color_image, glConfig.vidWidth, glConfig.vidHeight, VK_FILTER_LINEAR );

	// The reset of a camera cut goes to the upscaler and to frame generation once.
	if ( armed )
		fsrr.reset = qfalse;

	vk.fsrScene = qfalse;
	fsrr.armed = qfalse;
	fsrr.primary = qfalse;
	fsrr.viewJittered = qfalse;

	if ( fsrr.glowPending ) {
		// The blend pass clears the depth of the display-size pass.
		fsrr.glowPending = qfalse;
		vk_dglow_blend();
	} else {
		vk_begin_main_render_pass( qfalse );
		fsrr_clear_display_depth( cmd );
	}

	if ( armed )
		fsrr_framegen( cmd );

	// The dispatch bound compute state and the passes changed: draw again from a clean state.
	for ( i = 0; i < VK_DESC_COUNT; i++ )
		vk_reset_descriptor( i );
	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}

/*
===============
Render-size images of the last FSR world view. Valid in and after vk_fsr_raster_after_world_view.
Depth: [0..1], not inverted, rests in DEPTH_STENCIL_READ_ONLY_OPTIMAL; move it to SHADER_READ_ONLY_OPTIMAL
around the dispatch. Motion: NDC delta without jitter, rests in SHADER_READ_ONLY_OPTIMAL.
===============
*/
qboolean vk_fsr_raster_depth_image( vkFsrImage_t *out )
{
	if ( !vk.fsrRaster || vk.gbuffer_depth_image == VK_NULL_HANDLE )
		return qfalse;
	*out = fsrr_image( vk.gbuffer_depth_image, vk.gbuffer_depth_image_view, vk.gbuffer_depth_format,
		vk.fsrRenderWidth, vk.fsrRenderHeight, FSRR_STATE_READ );
	return qtrue;
}

qboolean vk_fsr_raster_motion_image( vkFsrImage_t *out )
{
	if ( !vk.fsrRaster || vk.gbuffer_velocity_image == VK_NULL_HANDLE )
		return qfalse;
	*out = fsrr_image( vk.gbuffer_velocity_image, vk.gbuffer_velocity_image_view, vk.velocity_format,
		vk.fsrRenderWidth, vk.fsrRenderHeight, FSRR_STATE_READ );
	return qtrue;
}
