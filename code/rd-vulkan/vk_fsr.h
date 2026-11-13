// AMD FSR SDK 2.4 wrapper: run-time loading, upscaler context, jitter and render size.
// The SDK DLLs are optional. When they are missing, every function reports "inactive".

#pragma once

#include "tr_local.h"

// An image handed to the upscaler. usage_state is a FFX_API_RESOURCE_STATE_* value
// that describes the state the image has when the dispatch starts.
typedef struct {
	VkImage		image;
	VkImageView	view;
	VkFormat	format;
	uint32_t	width, height;
	uint32_t	usage_state;
} vkFsrImage_t;

qboolean	vk_fsr_load_library( void );

// Effective state: the cvar is on, the library is loaded, and the context exists.
qboolean	vk_fsr_upscale_active( void );
qboolean	vk_fsr_framegen_active( void );
// r_fsrFrameGen 1 and no g_FastRendererSwitch. The cvars are latched. Does not load the library.
qboolean	vk_fsr_framegen_wanted( void );

// The size follows the cvars, not the context. After vk_fsr_upscale_create, check
// vk_fsr_upscale_active before the render images use it.
void		vk_fsr_get_render_size( uint32_t displayW, uint32_t displayH, uint32_t *renderW, uint32_t *renderH );
// From the sizes of the active context. 0 when no context is active.
float		vk_fsr_mip_bias( void );

// flags: FFX_UPSCALE_ENABLE_* bits (ffx_upscale.h).
qboolean	vk_fsr_upscale_create( uint32_t maxRenderW, uint32_t maxRenderH, uint32_t displayW, uint32_t displayH, uint32_t flags );
void		vk_fsr_upscale_destroy( void );

// Jitter in pixels at render size. Returns 0, 0 when no context exists.
void		vk_fsr_get_jitter( int frameIndex, float *x, float *y );

// reactive may be NULL. frameTimeMs is in milliseconds, fovY in radians.
// Returns qfalse when nothing is upscaled. A failed dispatch turns FSR off.
qboolean	vk_fsr_upscale_dispatch( VkCommandBuffer cmd, const vkFsrImage_t *color, const vkFsrImage_t *depth,
				const vkFsrImage_t *motion, const vkFsrImage_t *reactive, const vkFsrImage_t *output,
				float jitterX, float jitterY, float mvScaleX, float mvScaleY, float frameTimeMs,
				float cameraNear, float cameraFar, float fovY, qboolean reset );

// qtrue when the SDK can read and write images of this format.
qboolean	vk_fsr_format_supported( VkFormat format );

// Raster path (vk_fsr_raster.cpp): the camera data of the last split. Valid in every frame with a split.
typedef struct {
	float		jitterX, jitterY;		// render pixels, as given to FSR (0, 0 without the upscaler)
	float		mvScaleX, mvScaleY;		// motion vector scale, as given to FSR
	float		cameraNear, cameraFar;	// depth is [0..1], not inverted, finite far plane
	float		fovY;					// radians
	float		frameTimeMs;
	qboolean	reset;
	vec3_t		origin, forward, right, up;	// camera in world space
} vkFsrRasterFrame_t;

// Render-size depth and motion images of the raster world view. qfalse when the raster split is off.
// usage_state is COMPUTE_READ: the caller moves the depth from DEPTH_STENCIL_READ_ONLY_OPTIMAL to
// SHADER_READ_ONLY_OPTIMAL before the dispatch and back after. The motion rests in SHADER_READ_ONLY_OPTIMAL.
qboolean	vk_fsr_raster_depth_image( vkFsrImage_t *out );
qboolean	vk_fsr_raster_motion_image( vkFsrImage_t *out );

// Frame generation (vk_fsr.cpp). The inputs of one real frame.
typedef struct {
	vkFsrImage_t	depth;					// render size, in usage_state at the dispatch
	vkFsrImage_t	motion;
	uint32_t		renderW, renderH;
	float			jitterX, jitterY;		// render pixels, FSR convention
	float			mvScaleX, mvScaleY;
	float			frameTimeMs;
	float			cameraNear, cameraFar;
	float			fovY;					// radians
	vec3_t			origin, forward, right, up;
	qboolean		reset;					// camera cut: no frame is generated from this frame
} vkFsrFgFrame_t;

// Replaces vkCreateSwapchainKHR when frame generation can run. Creates the swapchain context,
// installs the replacement functions and creates the frame generation context.
// Returns qfalse (normal swapchain, one warning) when anything fails.
qboolean	vk_fsr_framegen_create_swapchain( const VkSwapchainCreateInfoKHR *ci, VkSwapchainKHR *swapchain );
// Waits for the presents in flight. Call before the swapchain images are released.
void		vk_fsr_framegen_wait_presents( void );
// Replaces vkDestroySwapchainKHR: AMD's teardown order, then the normal functions come back.
void		vk_fsr_framegen_destroy_swapchain( void );
// Configure and prepare of a real frame with a world view, in AMD's order. One call per frame;
// returns qtrue when the frame is generated from (the caller then records the HUD-less copy).
qboolean	vk_fsr_framegen_frame( VkCommandBuffer cmd, const vkFsrFgFrame_t *frame );
// qtrue after vk_fsr_framegen_frame enabled this frame and before the HUD-less copy is recorded.
qboolean	vk_fsr_framegen_hudless_pending( void );
// Draws vk.color_image through the gamma pass into the HUD-less image. No render pass may be open.
void		vk_fsr_framegen_hudless_copy( VkCommandBuffer cmd );
// Before each present: a frame with no world view gets frameGenerationEnabled = false.
void		vk_fsr_framegen_before_present( void );

// Destroys the upscaler context. The library stays loaded.
void		vk_fsr_shutdown( void );
// Before the device is destroyed: unloads the SDK when frame generation ran on the device.
void		vk_fsr_release_device( void );

void		vk_fsr_info_f( void );
