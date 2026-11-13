// AMD FSR SDK 2.4 wrapper. See vk_fsr.h.

#include "vk_fsr.h"

#ifdef _WIN32

#ifndef PLATFORM_WINDOWS
#	define PLATFORM_WINDOWS 1
#endif
#include "ffx_api_loader.h"
#include "ffx_upscale.h"
#include "ffx_framegeneration.h"
#include "vk/ffx_api_vk.h"
#include "vk/ffx_api_framegeneration_vk.h"

#include <math.h>

#define FSR_LOADER_DLL	"amd_fidelityfx_loader_vk.dll"
#define FSR_MAX_VERSIONS	16

typedef struct {
	HMODULE			module;
	ffxFunctions	fn;
	qboolean		load_tried;			// the load ran once, with success or not
	qboolean		load_ok;
	qboolean		warned;				// the "FSR is off" warning was printed
	qboolean		create_failed;		// the upscaler context failed to create
	ffxContext		upscale;
	qboolean		framegen_ok;
	qboolean		framegen_used;		// a frame generation swapchain was created on this device
	uint32_t		render_w, render_h;	// sizes of the upscaler context, for the mip bias
	uint32_t		display_w, display_h;
	int32_t			phase_count;
} fsrState_t;

static fsrState_t fsr;

// Frame generation. The contexts live as long as the swapchain, not as long as a map.
typedef struct {
	ffxContext						swapchain_ctx;
	ffxContext						fg_ctx;
	ffxQueryDescSwapchainReplacementFunctionsVK repl;
	PFN_vkAcquireNextImageKHR		orig_acquire;		// the functions before the replacement
	PFN_vkGetSwapchainImagesKHR		orig_get_images;
	PFN_vkQueuePresentKHR			orig_present;
	uint64_t						frame_id;
	qboolean						configured;			// configure ran for this frame
	qboolean						hudless_pending;	// this frame is generated from and needs its HUD-less copy
	qboolean						reset_next;			// the next frame with a world view is not generated from
	qboolean						warned;
} fsrFg_t;

static fsrFg_t fg;

// Prints the warning once per session.
static void fsr_warn( const char *text )
{
	if ( fsr.warned )
		return;
	fsr.warned = qtrue;
	ri.Printf( PRINT_WARNING, "FSR: %s. FSR is off.\n", text );
}

// Routes the messages of the SDK to the console.
static void fsr_message( uint32_t type, const wchar_t *message )
{
	ri.Printf( type == FFX_API_MESSAGE_TYPE_ERROR ? PRINT_WARNING : PRINT_DEVELOPER, "FSR: %ls\n", message );
}

static VkDeviceContext fsr_device_context( void )
{
	VkDeviceContext dc;

	dc.vkDevice = vk.device;
	dc.vkPhysicalDevice = vk.physical_device;
	dc.vkDeviceProcAddr = qvkGetDeviceProcAddr;
	return dc;
}

static void fsr_init_backend( ffxCreateBackendVKDesc *backend )
{
	Com_Memset( backend, 0, sizeof( *backend ) );
	backend->header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
	backend->deviceContext = fsr_device_context();
}

/*
===============
vk_fsr_load_library

Loads the loader DLL one time and keeps it loaded. Returns qfalse when it is missing.
===============
*/
qboolean vk_fsr_load_library( void )
{
	if ( fsr.load_tried )
		return fsr.load_ok;

	fsr.load_tried = qtrue;

	fsr.module = LoadLibraryA( FSR_LOADER_DLL );
	if ( fsr.module == NULL ) {
		fsr_warn( "cannot load " FSR_LOADER_DLL );
		return qfalse;
	}

	ffxLoadFunctions( &fsr.fn, fsr.module );
	if ( fsr.fn.CreateContext == NULL || fsr.fn.DestroyContext == NULL || fsr.fn.Query == NULL ||
		fsr.fn.Dispatch == NULL || fsr.fn.Configure == NULL ) {
		FreeLibrary( fsr.module );
		fsr.module = NULL;
		Com_Memset( &fsr.fn, 0, sizeof( fsr.fn ) );
		fsr_warn( FSR_LOADER_DLL " has no FSR API" );
		return qfalse;
	}

	fsr.load_ok = qtrue;
	return qtrue;
}

// The user wants the upscaler and nothing blocks it. The context may not exist yet.
static qboolean fsr_upscale_wanted( void )
{
	if ( r_fsr == NULL || !r_fsr->integer || fsr.create_failed )
		return qfalse;
	if ( vk.device == VK_NULL_HANDLE || qvkGetDeviceProcAddr == NULL )
		return qfalse;
	// vk_create_device printed the warning when the features are missing.
	if ( !vk.fsrDeviceFeatures )
		return qfalse;
	return vk_fsr_load_library();
}

qboolean vk_fsr_upscale_active( void )
{
	return ( fsr_upscale_wanted() && fsr.upscale != NULL ) ? qtrue : qfalse;
}

qboolean vk_fsr_framegen_active( void )
{
	if ( !vk_fsr_framegen_wanted() || !vk.fsrDeviceFeatures )
		return qfalse;
	return ( fsr.load_ok && fsr.framegen_ok && vk.fsrFgSwapchain ) ? qtrue : qfalse;
}

qboolean vk_fsr_framegen_wanted( void )
{
	if ( r_fsrFrameGen == NULL || !r_fsrFrameGen->integer )
		return qfalse;
	// The two renderers of g_FastRendererSwitch share the window and its present code.
	if ( ri.Cvar_VariableIntegerValue( "g_FastRendererSwitch" ) )
		return qfalse;
	return qtrue;
}

/*
===============
vk_fsr_get_render_size

Gives the render size of the quality mode, or the display size when FSR is off.
===============
*/
void vk_fsr_get_render_size( uint32_t displayW, uint32_t displayH, uint32_t *renderW, uint32_t *renderH )
{
	*renderW = displayW;
	*renderH = displayH;

	if ( fsr_upscale_wanted() ) {
		ffxCreateBackendVKDesc backend;
		ffxQueryDescUpscaleGetRenderResolutionFromQualityMode q;
		uint32_t w = 0, h = 0;

		fsr_init_backend( &backend );
		Com_Memset( &q, 0, sizeof( q ) );
		q.header.type = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETRENDERRESOLUTIONFROMQUALITYMODE;
		q.header.pNext = &backend.header;
		q.displayWidth = displayW;
		q.displayHeight = displayH;
		q.qualityMode = (uint32_t)r_fsrQuality->integer;
		q.pOutRenderWidth = &w;
		q.pOutRenderHeight = &h;

		if ( fsr.fn.Query( NULL, &q.header ) == FFX_API_RETURN_OK && w > 0 && h > 0 ) {
			*renderW = w;
			*renderH = h;
		} else {
			fsr_warn( "the render size query failed" );
			fsr.create_failed = qtrue;
		}
	}
}

/*
===============
vk_fsr_mip_bias

log2( render / display ) - 1, as the SDK recommends. Uses the sizes of the context.
0 when no context is active.
===============
*/
float vk_fsr_mip_bias( void )
{
	if ( !vk_fsr_upscale_active() || fsr.render_w == 0 || fsr.display_w == 0 )
		return 0.0f;
	return log2f( (float)fsr.render_w / (float)fsr.display_w ) - 1.0f;
}

void vk_fsr_upscale_destroy( void )
{
	if ( fsr.upscale != NULL ) {
		// The command buffers that use the context must be done.
		vk_device_wait_idle();
		fsr.fn.DestroyContext( &fsr.upscale, NULL );
		fsr.upscale = NULL;
	}
	fsr.phase_count = 0;
	fsr.render_w = fsr.render_h = 0;
	fsr.display_w = fsr.display_h = 0;
}

/*
===============
vk_fsr_upscale_create

Creates the upscaler context. An existing context is destroyed first.
On failure, prints one warning and turns FSR off until the next vid_restart.
===============
*/
qboolean vk_fsr_upscale_create( uint32_t maxRenderW, uint32_t maxRenderH, uint32_t displayW, uint32_t displayH, uint32_t flags )
{
	ffxCreateBackendVKDesc backend;
	ffxCreateContextDescUpscaleVersion version;
	ffxCreateContextDescUpscale desc;
	ffxQueryDescUpscaleGetJitterPhaseCount phase;
	ffxReturnCode_t rc;

	vk_fsr_upscale_destroy();

	if ( !fsr_upscale_wanted() )
		return qfalse;

	fsr_init_backend( &backend );

	Com_Memset( &version, 0, sizeof( version ) );
	version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION;
	version.header.pNext = &backend.header;
	version.version = FFX_UPSCALER_VERSION;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
	desc.header.pNext = &version.header;
	desc.flags = flags;
#ifdef _DEBUG
	desc.flags |= FFX_UPSCALE_ENABLE_DEBUG_CHECKING;
#endif
	desc.maxRenderSize.width = maxRenderW;
	desc.maxRenderSize.height = maxRenderH;
	desc.maxUpscaleSize.width = displayW;
	desc.maxUpscaleSize.height = displayH;
	desc.fpMessage = fsr_message;

	rc = fsr.fn.CreateContext( &fsr.upscale, &desc.header, NULL );
	if ( rc != FFX_API_RETURN_OK || fsr.upscale == NULL ) {
		fsr.upscale = NULL;
		fsr.create_failed = qtrue;
		fsr_warn( "cannot create the upscaler context" );
		return qfalse;
	}

	fsr.render_w = maxRenderW;
	fsr.render_h = maxRenderH;
	fsr.display_w = displayW;
	fsr.display_h = displayH;

	// The phase count depends on the scale only, so one query is enough.
	fsr_init_backend( &backend );
	Com_Memset( &phase, 0, sizeof( phase ) );
	phase.header.type = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTERPHASECOUNT;
	phase.header.pNext = &backend.header;
	phase.renderWidth = maxRenderW;
	phase.displayWidth = displayW;
	phase.pOutPhaseCount = &fsr.phase_count;
	if ( fsr.fn.Query( NULL, &phase.header ) != FFX_API_RETURN_OK || fsr.phase_count <= 0 )
		fsr.phase_count = 8;

	return qtrue;
}

/*
===============
vk_fsr_get_jitter

Gives the sub-pixel jitter of a frame, in pixels at render size.
===============
*/
void vk_fsr_get_jitter( int frameIndex, float *x, float *y )
{
	ffxCreateBackendVKDesc backend;
	ffxQueryDescUpscaleGetJitterOffset q;

	*x = 0.0f;
	*y = 0.0f;

	if ( fsr.upscale == NULL || fsr.phase_count <= 0 )
		return;

	fsr_init_backend( &backend );
	Com_Memset( &q, 0, sizeof( q ) );
	q.header.type = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTEROFFSET;
	q.header.pNext = &backend.header;
	q.index = frameIndex % fsr.phase_count;
	if ( q.index < 0 )
		q.index += fsr.phase_count;
	q.phaseCount = fsr.phase_count;
	q.pOutX = x;
	q.pOutY = y;

	if ( fsr.fn.Query( NULL, &q.header ) != FFX_API_RETURN_OK ) {
		*x = 0.0f;
		*y = 0.0f;
	}
}

// Wraps a Vulkan image. NULL gives an empty resource, which the SDK accepts for optional inputs.
static FfxApiResource fsr_resource( const vkFsrImage_t *img, uint32_t usage )
{
	FfxApiResourceDescription d;

	Com_Memset( &d, 0, sizeof( d ) );
	if ( img == NULL || img->image == VK_NULL_HANDLE )
		return ffxApiGetResourceVK( NULL, d, 0 );

	d.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
	d.format = ffxApiGetSurfaceFormatVK( img->format );
	d.width = img->width;
	d.height = img->height;
	d.depth = 1;
	d.mipCount = 1;
	d.flags = FFX_API_RESOURCE_FLAGS_NONE;
	d.usage = usage;
	if ( ffxApiIsDepthFormat( img->format ) )
		d.usage |= FFX_API_RESOURCE_USAGE_DEPTHTARGET;

	return ffxApiGetResourceVK( (void *)img->image, d, img->usage_state );
}

/*
===============
vk_fsr_upscale_dispatch

Records the upscale into cmd. The caller moves the images to the states in usage_state first.
reactive may be NULL. Returns qfalse when nothing is upscaled: the caller then uses its own path.
A failed dispatch turns FSR off. The context stays until the next create or shutdown,
because cmd can already refer to its resources.
===============
*/
qboolean vk_fsr_upscale_dispatch( VkCommandBuffer cmd, const vkFsrImage_t *color, const vkFsrImage_t *depth,
	const vkFsrImage_t *motion, const vkFsrImage_t *reactive, const vkFsrImage_t *output,
	float jitterX, float jitterY, float mvScaleX, float mvScaleY, float frameTimeMs,
	float cameraNear, float cameraFar, float fovY, qboolean reset )
{
	ffxDispatchDescUpscale d;

	if ( !vk_fsr_upscale_active() || cmd == VK_NULL_HANDLE || color == NULL || depth == NULL || motion == NULL || output == NULL )
		return qfalse;

	Com_Memset( &d, 0, sizeof( d ) );
	d.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
	d.commandList = cmd;
	d.color = fsr_resource( color, FFX_API_RESOURCE_USAGE_READ_ONLY );
	d.depth = fsr_resource( depth, FFX_API_RESOURCE_USAGE_READ_ONLY );
	d.motionVectors = fsr_resource( motion, FFX_API_RESOURCE_USAGE_READ_ONLY );
	d.reactive = fsr_resource( reactive, FFX_API_RESOURCE_USAGE_READ_ONLY );
	d.output = fsr_resource( output, FFX_API_RESOURCE_USAGE_UAV );
	d.jitterOffset.x = jitterX;
	d.jitterOffset.y = jitterY;
	d.motionVectorScale.x = mvScaleX;
	d.motionVectorScale.y = mvScaleY;
	d.renderSize.width = color->width;
	d.renderSize.height = color->height;
	d.upscaleSize.width = output->width;
	d.upscaleSize.height = output->height;
	d.sharpness = r_fsrSharpness->value;
	d.enableSharpening = d.sharpness > 0.0f;
	d.frameTimeDelta = frameTimeMs;
	d.preExposure = 1.0f;
	d.reset = reset ? true : false;
	d.cameraNear = cameraNear;
	d.cameraFar = cameraFar;
	d.cameraFovAngleVertical = fovY;
	d.viewSpaceToMetersFactor = 0.025f;	// about 2.5 cm per game unit

	if ( fsr.fn.Dispatch( &fsr.upscale, &d.header ) != FFX_API_RETURN_OK ) {
		fsr.create_failed = qtrue;
		fsr_warn( "the upscale dispatch failed" );
		return qfalse;
	}

	return qtrue;
}

// The SDK maps only some Vulkan formats (ffxApiGetSurfaceFormatVK). R16G16B16A16_UNORM is not one of them.
qboolean vk_fsr_format_supported( VkFormat format )
{
	return ( ffxApiGetSurfaceFormatVK( format ) != FFX_API_SURFACE_FORMAT_UNKNOWN ) ? qtrue : qfalse;
}

/*
===============================================================================

Frame generation (FSR frame generation swapchain)

===============================================================================
*/

// Prints the warning once per session. The normal swapchain stays.
static void fg_warn( const char *text )
{
	if ( fg.warned )
		return;
	fg.warned = qtrue;
	ri.Printf( PRINT_WARNING, "FSR: %s. Frame generation is off.\n", text );
}

// The swapchain calls this in its present to record the generated frame.
static ffxReturnCode_t fg_dispatch_callback( ffxDispatchDescFrameGeneration *params, void *user )
{
	return fsr.fn.Dispatch( (ffxContext *)user, &params->header );
}

static VkQueueInfoFFXAPI fg_queue( VkQueue queue, uint32_t family )
{
	VkQueueInfoFFXAPI q;

	q.queue = queue;
	q.familyIndex = family;
	q.submitFunc = NULL;
	return q;
}

static void fg_destroy_contexts( VkSwapchainKHR swapchain )
{
	ffxReturnCode_t rc;

	if ( fg.fg_ctx != NULL ) {
		rc = fsr.fn.DestroyContext( &fg.fg_ctx, NULL );
		ri.Printf( PRINT_DEVELOPER, "FSR: frame generation context destroyed (%u)\n", (unsigned)rc );
		fg.fg_ctx = NULL;
	}
	if ( fg.swapchain_ctx != NULL ) {
		rc = fsr.fn.DestroyContext( &fg.swapchain_ctx, NULL );
		ri.Printf( PRINT_DEVELOPER, "FSR: frame generation swapchain context destroyed (%u)\n", (unsigned)rc );
		fg.swapchain_ctx = NULL;
	}
	// The SDK destroys the swapchain after its context (AMD's Vulkan shutdown order).
	if ( swapchain != VK_NULL_HANDLE && fg.repl.pOutDestroySwapchainFFXAPI != NULL )
		fg.repl.pOutDestroySwapchainFFXAPI( vk.device, swapchain, NULL, NULL );
	Com_Memset( &fg.repl, 0, sizeof( fg.repl ) );
}

/*
===============
vk_fsr_framegen_create_swapchain

Creates the frame generation swapchain from the create info of vk_create_swapchain, installs the
replacement functions and creates the frame generation context. On failure, the normal swapchain
is used and one warning is printed.
===============
*/
qboolean vk_fsr_framegen_create_swapchain( const VkSwapchainCreateInfoKHR *ci, VkSwapchainKHR *swapchain )
{
	ffxCreateContextDescFrameGenerationSwapChainVersionVK	sc_version;
	ffxCreateContextDescFrameGenerationSwapChainVK			sc_desc;
	ffxCreateBackendVKDesc									backend;
	ffxCreateContextDescFrameGenerationVersion				fg_version;
	ffxCreateContextDescFrameGeneration						fg_desc;
	VkSwapchainKHR											sc = VK_NULL_HANDLE;
	uint32_t												format;
	ffxReturnCode_t											rc;

	vk.fsrFgSwapchain = qfalse;
	fsr.framegen_ok = qfalse;

	if ( !vk_fsr_framegen_wanted() )
		return qfalse;
	// vk_create_device printed the warning when the features are missing.
	if ( !vk.fsrDeviceFeatures || !vk_fsr_load_library() )
		return qfalse;
	if ( !vk.fsrFgQueues ) {
		fg_warn( "the device has no free queue for the present and the image acquire" );
		return qfalse;
	}
	if ( !vk.fsrFgTimeline ) {
		fg_warn( "the device has no timeline semaphores" );
		return qfalse;
	}
	if ( !vk.fboActive ) {
		fg_warn( "frame generation needs r_fbo 1" );
		return qfalse;
	}
	format = ffxApiGetSurfaceFormatVK( ci->imageFormat );
	if ( format == FFX_API_SURFACE_FORMAT_UNKNOWN ) {
		fg_warn( "the SDK has no support for the swapchain format" );
		return qfalse;
	}

	Com_Memset( &sc_version, 0, sizeof( sc_version ) );
	sc_version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_VERSION_VK;
	sc_version.version = FFX_FRAMEGENERATION_SWAPCHAIN_VK_VERSION;

	Com_Memset( &sc_desc, 0, sizeof( sc_desc ) );
	sc_desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FGSWAPCHAIN_VK;
	sc_desc.header.pNext = &sc_version.header;
	sc_desc.physicalDevice = vk.physical_device;
	sc_desc.device = vk.device;
	sc_desc.swapchain = &sc;
	sc_desc.allocator = NULL;
	sc_desc.createInfo = *ci;
	sc_desc.createInfo.oldSwapchain = VK_NULL_HANDLE;
	sc_desc.gameQueue = fg_queue( vk.queue, vk.queue_family_index );
	sc_desc.presentQueue = fg_queue( vk.fsrFgQueue[0], vk.fsrFgQueueFamily[0] );
	sc_desc.imageAcquireQueue = fg_queue( vk.fsrFgQueue[1], vk.fsrFgQueueFamily[1] );
	sc_desc.asyncComputeQueue = fg_queue( vk.fsrFgQueue[2], vk.fsrFgQueueFamily[2] );

	// The create touches the device also when it fails, so release the DLLs at shutdown.
	fsr.framegen_used = qtrue;
	fg.swapchain_ctx = NULL;
	rc = fsr.fn.CreateContext( &fg.swapchain_ctx, &sc_desc.header, NULL );
	if ( rc != FFX_API_RETURN_OK || fg.swapchain_ctx == NULL || sc == VK_NULL_HANDLE ) {
		// A context of a successful create without a swapchain is destroyed too.
		if ( rc == FFX_API_RETURN_OK )
			fg_destroy_contexts( sc );
		fg.swapchain_ctx = NULL;
		fg_warn( "cannot create the frame generation swapchain" );
		return qfalse;
	}

	Com_Memset( &fg.repl, 0, sizeof( fg.repl ) );
	fg.repl.header.type = FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_FUNCTIONS_VK;
	if ( fsr.fn.Query( &fg.swapchain_ctx, &fg.repl.header ) != FFX_API_RETURN_OK
		|| fg.repl.pOutAcquireNextImageKHR == NULL || fg.repl.pOutGetSwapchainImagesKHR == NULL
		|| fg.repl.pOutQueuePresentKHR == NULL || fg.repl.pOutDestroySwapchainFFXAPI == NULL ) {
		fg_destroy_contexts( sc );
		fg_warn( "the frame generation swapchain has no replacement functions" );
		return qfalse;
	}

	fsr_init_backend( &backend );

	Com_Memset( &fg_version, 0, sizeof( fg_version ) );
	fg_version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_VERSION;
	fg_version.header.pNext = &backend.header;
	fg_version.version = FFX_FRAMEGENERATION_VERSION;

	Com_Memset( &fg_desc, 0, sizeof( fg_desc ) );
	fg_desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
	fg_desc.header.pNext = &fg_version.header;
	// The path tracer gives an inverted depth with an infinite far plane (vk_rtx_fsr.cpp).
	fg_desc.flags = vk.rtxActive ? ( FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED | FFX_FRAMEGENERATION_ENABLE_DEPTH_INFINITE ) : 0;
#ifdef _DEBUG
	fg_desc.flags |= FFX_FRAMEGENERATION_ENABLE_DEBUG_CHECKING;
#endif
	fg_desc.displaySize.width = ci->imageExtent.width;
	fg_desc.displaySize.height = ci->imageExtent.height;
	fg_desc.maxRenderSize.width = MAX( ci->imageExtent.width, (uint32_t)glConfig.vidWidth );
	fg_desc.maxRenderSize.height = MAX( ci->imageExtent.height, (uint32_t)glConfig.vidHeight );
	fg_desc.backBufferFormat = format;

	if ( fsr.fn.CreateContext( &fg.fg_ctx, &fg_desc.header, NULL ) != FFX_API_RETURN_OK || fg.fg_ctx == NULL ) {
		fg.fg_ctx = NULL;
		fg_destroy_contexts( sc );
		fg_warn( "cannot create the frame generation context" );
		return qfalse;
	}

	// All swapchain calls of the renderer go through the qvk pointers.
	fg.orig_acquire = qvkAcquireNextImageKHR;
	fg.orig_get_images = qvkGetSwapchainImagesKHR;
	fg.orig_present = qvkQueuePresentKHR;
	qvkAcquireNextImageKHR = fg.repl.pOutAcquireNextImageKHR;
	qvkGetSwapchainImagesKHR = fg.repl.pOutGetSwapchainImagesKHR;
	qvkQueuePresentKHR = fg.repl.pOutQueuePresentKHR;

	*swapchain = sc;
	vk.fsrFgSwapchain = qtrue;
	vk.fsrFgExtent = ci->imageExtent;
	fsr.framegen_ok = qtrue;
	fg.frame_id = 1;
	fg.configured = qfalse;
	fg.hudless_pending = qfalse;
	fg.reset_next = qtrue;

	ri.Printf( PRINT_ALL, "FSR: frame generation on, %ux%u\n", ci->imageExtent.width, ci->imageExtent.height );
	return qtrue;
}

void vk_fsr_framegen_wait_presents( void )
{
	ffxDispatchDescFrameGenerationSwapChainWaitForPresentsVK w;

	if ( fg.swapchain_ctx == NULL )
		return;

	Com_Memset( &w, 0, sizeof( w ) );
	w.header.type = FFX_API_DISPATCH_DESC_TYPE_FGSWAPCHAIN_WAIT_FOR_PRESENTS_VK;
	fsr.fn.Dispatch( &fg.swapchain_ctx, &w.header );
}

static FfxApiResource fg_hudless_resource( void )
{
	vkFsrImage_t img;

	img.image = vk.fsr_hudless_image;
	img.view = vk.fsr_hudless_image_view;
	img.format = vk.present_format.format;
	img.width = vk.fsrFgExtent.width;
	img.height = vk.fsrFgExtent.height;
	img.usage_state = FFX_API_RESOURCE_STATE_COMPUTE_READ;
	return fsr_resource( vk.fsr_hudless_image != VK_NULL_HANDLE ? &img : NULL, FFX_API_RESOURCE_USAGE_READ_ONLY );
}

// Once per frame, before the prepare dispatch and the present.
static qboolean fg_configure( qboolean enabled )
{
	ffxConfigureDescFrameGeneration c;
	const int debug = r_fsrFrameGenDebug ? r_fsrFrameGenDebug->integer : 0;

	Com_Memset( &c, 0, sizeof( c ) );
	c.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
	c.swapChain = (void *)vk.swapchain;
	c.presentCallback = NULL;
	c.frameGenerationCallback = fg_dispatch_callback;
	c.frameGenerationCallbackUserContext = &fg.fg_ctx;
	c.frameGenerationEnabled = enabled ? true : false;
	c.allowAsyncWorkloads = false;
	if ( enabled )
		c.HUDLessColor = fg_hudless_resource();
	c.flags = 0;
	if ( debug & 1 )
		c.flags |= FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES;
	if ( debug & 2 )
		c.flags |= FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS;
	if ( debug & 4 )
		c.flags |= FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW;
	if ( debug & 16 )
		c.flags |= FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_PACING_LINES;
	c.onlyPresentGenerated = ( debug & 8 ) ? true : false;
	c.generationRect.left = 0;
	c.generationRect.top = 0;
	c.generationRect.width = (int32_t)vk.fsrFgExtent.width;
	c.generationRect.height = (int32_t)vk.fsrFgExtent.height;
	c.frameID = fg.frame_id;

	return ( fsr.fn.Configure( &fg.fg_ctx, &c.header ) == FFX_API_RETURN_OK ) ? qtrue : qfalse;
}

/*
===============
vk_fsr_framegen_destroy_swapchain

AMD's order: wait for the presents, disable frame generation, destroy the frame generation
context, destroy the swapchain context, destroy the swapchain, put the normal functions back.
===============
*/
void vk_fsr_framegen_destroy_swapchain( void )
{
	if ( fg.swapchain_ctx == NULL && fg.fg_ctx == NULL )
		return;

	vk_fsr_framegen_wait_presents();

	if ( fg.fg_ctx != NULL ) {
		ffxConfigureDescFrameGeneration c;

		// This configure also waits for the interpolation and composition work in flight.
		Com_Memset( &c, 0, sizeof( c ) );
		c.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
		c.swapChain = (void *)vk.swapchain;
		c.frameGenerationEnabled = false;
		c.frameID = fg.frame_id;
		fsr.fn.Configure( &fg.fg_ctx, &c.header );
	}

	fg_destroy_contexts( vk.swapchain );
	vk.swapchain = VK_NULL_HANDLE;

	if ( fg.orig_acquire != NULL ) {
		qvkAcquireNextImageKHR = fg.orig_acquire;
		qvkGetSwapchainImagesKHR = fg.orig_get_images;
		qvkQueuePresentKHR = fg.orig_present;
	}
	fg.orig_acquire = NULL;
	fg.orig_get_images = NULL;
	fg.orig_present = NULL;

	vk.fsrFgSwapchain = qfalse;
	fsr.framegen_ok = qfalse;
	fg.configured = qfalse;
	fg.hudless_pending = qfalse;
	fg.reset_next = qtrue;
}

/*
===============
vk_fsr_framegen_frame

Runs once in a frame with a world view, when its depth and motion vectors are complete:
ffxConfigure, then the PrepareV2 dispatch into cmd. The present comes later (AMD's order).
A camera cut, or the first world frame after a frame with no world view, is not generated
from: it only starts the history.
===============
*/
qboolean vk_fsr_framegen_frame( VkCommandBuffer cmd, const vkFsrFgFrame_t *f )
{
	ffxDispatchDescFrameGenerationPrepareV2 p;
	qboolean enabled;

	if ( !vk_fsr_framegen_active() || fg.fg_ctx == NULL || fg.configured || cmd == VK_NULL_HANDLE || f == NULL )
		return qfalse;

	enabled = ( !f->reset && !fg.reset_next && vk.fsr_hudless_image != VK_NULL_HANDLE ) ? qtrue : qfalse;

	fg.configured = qtrue;
	fg.reset_next = qfalse;
	fg.hudless_pending = qfalse;

	if ( !fg_configure( enabled ) ) {
		fg_warn( "the frame generation configure failed" );
		fsr.framegen_ok = qfalse;
		return qfalse;
	}

	Com_Memset( &p, 0, sizeof( p ) );
	p.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_V2;
	p.frameID = fg.frame_id;
	p.flags = 0;
	p.commandList = cmd;
	p.renderSize.width = f->renderW;
	p.renderSize.height = f->renderH;
	p.jitterOffset.x = f->jitterX;
	p.jitterOffset.y = f->jitterY;
	p.motionVectorScale.x = f->mvScaleX;
	p.motionVectorScale.y = f->mvScaleY;
	p.frameTimeDelta = f->frameTimeMs;
	p.reset = enabled ? false : true;
	p.cameraNear = f->cameraNear;
	p.cameraFar = f->cameraFar;
	p.cameraFovAngleVertical = f->fovY;
	p.viewSpaceToMetersFactor = 0.025f;	// about 2.5 cm per game unit
	p.depth = fsr_resource( &f->depth, FFX_API_RESOURCE_USAGE_READ_ONLY );
	p.motionVectors = fsr_resource( &f->motion, FFX_API_RESOURCE_USAGE_READ_ONLY );
	VectorCopy( f->origin, p.cameraPosition );
	VectorCopy( f->up, p.cameraUp );
	VectorCopy( f->right, p.cameraRight );
	VectorCopy( f->forward, p.cameraForward );

	if ( fsr.fn.Dispatch( &fg.fg_ctx, &p.header ) != FFX_API_RETURN_OK ) {
		// The configure of this frame already ran: turn the generation off for it.
		fg_configure( qfalse );
		fg.reset_next = qtrue;
		ri.Printf( PRINT_DEVELOPER, "FSR: the frame generation prepare failed\n" );
		return qfalse;
	}

	fg.hudless_pending = enabled;
	return enabled;
}

qboolean vk_fsr_framegen_hudless_pending( void )
{
	return fg.hudless_pending;
}

/*
===============
vk_fsr_framegen_hudless_copy

Draws vk.color_image (the 3D scene, before any 2D) with the gamma pipeline into the HUD-less
image: the same transform as the back buffer gets in vk_end_frame. No render pass may be open.
===============
*/
void vk_fsr_framegen_hudless_copy( VkCommandBuffer cmd )
{
	VkImageMemoryBarrier	barrier;
	VkRenderPassBeginInfo	begin;
	VkClearValue			clear;

	if ( !fg.hudless_pending )
		return;
	fg.hudless_pending = qfalse;

	if ( vk.render_pass.fsr_hudless == VK_NULL_HANDLE || vk.framebuffers.fsr_hudless == VK_NULL_HANDLE
		|| vk.gamma_pipeline == VK_NULL_HANDLE )
		return;

	// The scene comes from a render pass, a blit or a compute pass.
	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.color_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	Com_Memset( &clear, 0, sizeof( clear ) );
	Com_Memset( &begin, 0, sizeof( begin ) );
	begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	begin.renderPass = vk.render_pass.fsr_hudless;
	begin.framebuffer = vk.framebuffers.fsr_hudless;
	begin.renderArea.extent = vk.fsrFgExtent;
	begin.clearValueCount = 1;
	begin.pClearValues = &clear;
	qvkCmdBeginRenderPass( cmd, &begin, VK_SUBPASS_CONTENTS_INLINE );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.gamma_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );
	qvkCmdDraw( cmd, 4, 1, 0, 0 );

	qvkCmdEndRenderPass( cmd );
}

/*
===============
vk_fsr_framegen_before_present

Runs before each present through the frame generation swapchain. A frame with no world view
(menu, console, loading screen, cinematic) is presented as it is, with no generated frame.
===============
*/
void vk_fsr_framegen_before_present( void )
{
	if ( !vk.fsrFgSwapchain || fg.fg_ctx == NULL )
		return;

	if ( !fg.configured ) {
		fg_configure( qfalse );
		fg.reset_next = qtrue;
	}

	fg.configured = qfalse;
	fg.hudless_pending = qfalse;
	fg.frame_id++;
}

/*
===============
vk_fsr_shutdown

Destroys the contexts. The loader DLL stays loaded across vid_restart.
===============
*/
void vk_fsr_shutdown( void )
{
	if ( fsr.load_ok )
		vk_fsr_upscale_destroy();

	// The frame generation contexts belong to the swapchain (vk_destroy_swapchain).
	// After a map change the first frames are not generated from.
	fg.reset_next = qtrue;
	fsr.create_failed = qfalse;
}

/*
===============
vk_fsr_release_device

Runs in vk_shutdown before the device is destroyed. The frame generation DLL keeps objects of
the device after its swapchain is destroyed and frees them at the next present, with the old
device (the loader then stops the process). When frame generation ran on this device, the SDK
DLLs are unloaded here; the next vk_fsr_load_library loads them again.
===============
*/
void vk_fsr_release_device( void )
{
	static const char *providers[] = { "amd_fidelityfx_framegeneration_vk.dll", "amd_fidelityfx_upscaler_vk.dll" };
	uint32_t i, n;

	if ( !fsr.framegen_used || fsr.module == NULL )
		return;

	vk_fsr_framegen_destroy_swapchain();
	vk_fsr_upscale_destroy();

	FreeLibrary( fsr.module );
	fsr.module = NULL;

	// The loader loaded the providers. Nothing else in the process uses them.
	for ( i = 0; i < ARRAY_LEN( providers ); i++ ) {
		HMODULE h;

		for ( n = 0; n < 8 && ( h = GetModuleHandleA( providers[i] ) ) != NULL; n++ )
			FreeLibrary( h );
	}

	Com_Memset( &fsr.fn, 0, sizeof( fsr.fn ) );
	fsr.load_tried = qfalse;
	fsr.load_ok = qfalse;
	fsr.framegen_used = qfalse;
	fsr.framegen_ok = qfalse;
	fsr.render_w = fsr.render_h = 0;
	fsr.display_w = fsr.display_h = 0;
}

/*
===============
vk_fsr_info_f

Prints the library state, the provider versions and the effective state.
===============
*/
void vk_fsr_info_f( void )
{
	ri.Printf( PRINT_ALL, "FSR library:   %s\n", vk_fsr_load_library() ? FSR_LOADER_DLL " loaded" : "not loaded" );

	if ( fsr.load_ok && vk.device != VK_NULL_HANDLE ) {
		VkDeviceContext dc = fsr_device_context();
		uint64_t ids[FSR_MAX_VERSIONS];
		const char *names[FSR_MAX_VERSIONS];
		uint64_t count = FSR_MAX_VERSIONS;
		ffxQueryDescGetVersions q;
		uint64_t i;

		Com_Memset( &q, 0, sizeof( q ) );
		q.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
		q.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
		q.device = &dc;
		q.outputCount = &count;
		q.versionIds = ids;
		q.versionNames = names;

		if ( fsr.fn.Query( NULL, &q.header ) == FFX_API_RETURN_OK ) {
			if ( count > FSR_MAX_VERSIONS )
				count = FSR_MAX_VERSIONS;
			ri.Printf( PRINT_ALL, "Upscaler versions: %i\n", (int)count );
			for ( i = 0; i < count; i++ )
				ri.Printf( PRINT_ALL, "  %s\n", names[i] );
		} else {
			ri.Printf( PRINT_ALL, "Upscaler versions: query failed\n" );
		}

		if ( fsr.upscale != NULL ) {
			ffxQueryGetProviderVersion pv;

			Com_Memset( &pv, 0, sizeof( pv ) );
			pv.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
			if ( fsr.fn.Query( &fsr.upscale, &pv.header ) == FFX_API_RETURN_OK && pv.versionName != NULL )
				ri.Printf( PRINT_ALL, "Upscaler in use: %s\n", pv.versionName );
		}

		if ( fg.fg_ctx != NULL ) {
			ffxQueryGetProviderVersion pv;

			Com_Memset( &pv, 0, sizeof( pv ) );
			pv.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
			if ( fsr.fn.Query( &fg.fg_ctx, &pv.header ) == FFX_API_RETURN_OK && pv.versionName != NULL )
				ri.Printf( PRINT_ALL, "Frame generation in use: %s\n", pv.versionName );
			// Real frames against presents: about twice as many presents while frames are generated.
			if ( fg.repl.pOutGetLastPresentCountFFXAPI != NULL )
				ri.Printf( PRINT_ALL, "Frame generation: %llu real frames, %llu presents since the swapchain was created\n",
					(unsigned long long)( fg.frame_id - 1 ), (unsigned long long)fg.repl.pOutGetLastPresentCountFFXAPI( vk.swapchain ) );
			ri.Printf( PRINT_ALL, "Frame generation queues: present %u/%u, acquire %u/%u, async compute %s\n",
				vk.fsrFgQueueFamily[0], vk.fsrFgQueueIndex[0], vk.fsrFgQueueFamily[1], vk.fsrFgQueueIndex[1],
				vk.fsrFgQueue[2] != VK_NULL_HANDLE ? va( "%u/%u", vk.fsrFgQueueFamily[2], vk.fsrFgQueueIndex[2] ) : "none" );
		}
	}

	ri.Printf( PRINT_ALL, "r_fsr %i (quality %i, sharpness %.2f), r_fsrFrameGen %i\n",
		r_fsr ? r_fsr->integer : 0, r_fsrQuality ? r_fsrQuality->integer : 0,
		r_fsrSharpness ? r_fsrSharpness->value : 0.0f, r_fsrFrameGen ? r_fsrFrameGen->integer : 0 );
	ri.Printf( PRINT_ALL, "Effective:     upscale %s, frame generation %s\n",
		vk_fsr_upscale_active() ? "on" : "off", vk_fsr_framegen_active() ? "on" : "off" );
}

#else	// !_WIN32: the FSR SDK DLLs are Windows only

qboolean	vk_fsr_load_library( void ) { return qfalse; }
qboolean	vk_fsr_upscale_active( void ) { return qfalse; }
qboolean	vk_fsr_framegen_active( void ) { return qfalse; }
qboolean	vk_fsr_framegen_wanted( void ) { return qfalse; }
qboolean	vk_fsr_framegen_create_swapchain( const VkSwapchainCreateInfoKHR *, VkSwapchainKHR * ) { return qfalse; }
void		vk_fsr_framegen_wait_presents( void ) {}
void		vk_fsr_framegen_destroy_swapchain( void ) {}
qboolean	vk_fsr_framegen_frame( VkCommandBuffer, const vkFsrFgFrame_t * ) { return qfalse; }
qboolean	vk_fsr_framegen_hudless_pending( void ) { return qfalse; }
void		vk_fsr_framegen_hudless_copy( VkCommandBuffer ) {}
void		vk_fsr_framegen_before_present( void ) {}
void		vk_fsr_get_render_size( uint32_t displayW, uint32_t displayH, uint32_t *renderW, uint32_t *renderH ) { *renderW = displayW; *renderH = displayH; }
float		vk_fsr_mip_bias( void ) { return 0.0f; }
qboolean	vk_fsr_upscale_create( uint32_t, uint32_t, uint32_t, uint32_t, uint32_t ) { return qfalse; }
void		vk_fsr_upscale_destroy( void ) {}
void		vk_fsr_get_jitter( int, float *x, float *y ) { *x = 0.0f; *y = 0.0f; }
qboolean	vk_fsr_upscale_dispatch( VkCommandBuffer, const vkFsrImage_t *, const vkFsrImage_t *, const vkFsrImage_t *,
				const vkFsrImage_t *, const vkFsrImage_t *, float, float, float, float, float, float, float, float, qboolean ) { return qfalse; }
qboolean	vk_fsr_format_supported( VkFormat ) { return qfalse; }
void		vk_fsr_shutdown( void ) {}
void		vk_fsr_release_device( void ) {}
void		vk_fsr_info_f( void ) { ri.Printf( PRINT_ALL, "FSR is not available on this platform.\n" ); }

#endif

// The frame generation presenter thread uses the queues. Wait for it before the device wait.
void vk_device_wait_idle( void )
{
	if ( qvkDeviceWaitIdle == NULL || vk.device == VK_NULL_HANDLE )
		return;
	vk_fsr_framegen_wait_presents();
	qvkDeviceWaitIdle( vk.device );
}
