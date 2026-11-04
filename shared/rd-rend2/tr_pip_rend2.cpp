/*
===========================================================================
SWGL PiP support for rend2 (SP target only - compiled into rdsp-rend2).

The PiP window (half the main resolution) is created at renderer startup and
is always there, showing black.  Cgame submits a second RenderScene with
refdef.pipRender set while the static camera is active; that scene goes
through the normal pipeline, only its main view lands in the PiP framebuffer
(see the hook in tr_scene.cpp), and the per-frame present command reads the
framebuffer back and blits it to the secondary SDL window (pure SDL, no GL).

Idle cost: one black blit to the window per frame, no scene rendered.
===========================================================================
*/

#include "tr_local.h"

#include <stdlib.h>	// malloc/free (rend2 has no per-allocation free through its arena allocators)

// glext.h in this project lacks a plain GL_RGBA8 define
#ifndef GL_RGBA8
#define GL_RGBA8					0x8058
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16		0x81A5
#endif

static cvar_t	*r_pipWindow;
static cvar_t	*r_pipWindowX;
static cvar_t	*r_pipWindowY;
static cvar_t	*r_pipTopmost;
static cvar_t	*r_pipCrosshair;
static cvar_t	*r_pipBorderless;	// read by the SDL layer at window creation
static cvar_t	*r_pipRate;			// render the pip scene once every N frames
static cvar_t	*r_pipDirect;		// 1 = render straight into the window framebuffer
static cvar_t	*r_pipReadbackWait;	// µs bounded fence wait to present the CURRENT frame
static cvar_t	*r_pipLatencyLog;	// log which pipeline stage the presented image took
static cvar_t	*r_pipSdlHw;		// SDL layer: hardware renderer for the pip window

static qboolean	pipSkipPresent;				// set when a frame's pip scene was rate-skipped
static int		pipRateCounter;					// r_pip_rate countdown: 0 = this scene renders

static qboolean	pipReady;					// window + framebuffer alive
static void	*pipWindowHandle;
static int	pipWidth, pipHeight;

static FBO_t	pipFbo;						// shim handed to the pipeline as targetFbo
static GLuint	pipColorRB, pipDepthRB;

static FBO_t   *pipPostFbo;					// main-resolution RGBA8 colour target for the
											// pip postprocess chain (created on first use)

static FBO_t   *pipDepthSave[2];				// main-camera depth save slots: 0 = renderFbo,
											// 1 = msaa resolve copy.  The pip scene runs last
											// in the frame and its depth-keyed Reshade effects
											// would otherwise drive the main window.

static byte	*pipBlack;					// all-black frame used for the idle blit
static qboolean	pipContentDirty;			// set when a pip scene rendered this frame

// double-buffered async readback, the window shows the previous frame
static GLuint	pipPbo[2];
static GLsync	pipFence[2];
static int		pipParity;				// buffer that will receive the next readback

/*
=============
Pip_RegisterCvars

Registered at renderer startup so the cgame gate (r_pip_window) exists from
frame one.
=============
*/
static void Pip_RegisterCvars( void ) {
	if ( !r_pipWindow ) {
		r_pipWindow = ri.Cvar_Get( "r_pip_window", "0", CVAR_ARCHIVE );
		r_pipWindowX = ri.Cvar_Get( "r_pip_window_x", "-1", CVAR_ARCHIVE );	// -3 = bottom-right corner of the other display
		r_pipWindowY = ri.Cvar_Get( "r_pip_window_y", "-1", CVAR_ARCHIVE );	// -3 = bottom-right corner of the other display
		r_pipTopmost = ri.Cvar_Get( "r_pip_topmost", "1", CVAR_ARCHIVE );
		r_pipCrosshair = ri.Cvar_Get( "r_pip_crosshair", "1", CVAR_ARCHIVE );
		r_pipBorderless = ri.Cvar_Get( "r_pip_borderless", "1", CVAR_ARCHIVE );
		r_pipRate = ri.Cvar_Get( "r_pip_rate", "1", CVAR_ARCHIVE );		// SWGL PiP perf: render the scene once every N frames (1 = every frame, the window simply keeps the last image on the skipped ones)
		r_pipDirect = ri.Cvar_Get( "r_pip_direct", "1", CVAR_ARCHIVE );	// SWGL PiP perf: render straight into the window framebuffer (native size) and skip the postprocess chain
		r_pipReadbackWait = ri.Cvar_Get( "r_pip_readback_wait", "25000", CVAR_ARCHIVE );	// SWGL PiP latency: µs to wait for this frame's readback so the window can show the CURRENT frame (default tuned for the ~60 fps reshade regime; 0 = the pure async design, one frame of pipeline latency)
		r_pipLatencyLog = ri.Cvar_Get( "r_pip_latencylog", "0", CVAR_ARCHIVE );	// SWGL PiP latency: log which pipeline stage each presented frame took
		r_pipSdlHw = ri.Cvar_Get( "r_pip_sdlhw", "1", CVAR_ARCHIVE );		// SWGL PiP latency: hardware renderer for the SDL window (flip-model present, no CPU copy; applied when the window is created, so a game restart takes it).  Beware: Reshade can hook a hardware renderer and post-process the pip window too.
	}
}

/*
=============
Pip_GLReady

rend2 loads the framebuffer entry points during its extension init, which
ran before us; just make sure the ones we need are there.
=============
*/
static qboolean Pip_GLReady( void ) {
	if ( !qglGenFramebuffers || !qglBindFramebuffer || !qglDeleteFramebuffers ||
		 !qglGenRenderbuffers || !qglDeleteRenderbuffers || !qglRenderbufferStorage ||
		 !qglFramebufferRenderbuffer || !qglCheckFramebufferStatus ||
		 !qglGenBuffers || !qglBindBuffer || !qglBufferData || !qglDeleteBuffers ||
		 !qglMapBufferRange || !qglUnmapBuffer ||
		 !qglFenceSync || !qglDeleteSync || !qglClientWaitSync ) {
		ri.Printf( PRINT_ALL, "PiP: framebuffer entry points not loaded, PiP disabled\n" );
		return qfalse;
	}
	return qtrue;
}

/*
=============
Pip_FenceSignaled

Non-blocking check for whether the GPU already finished the readback that
was recorded into the fence.
=============
*/
static qboolean Pip_FenceSignaled( GLsync fence ) {
	GLenum	status = qglClientWaitSync( fence, 0, 0 );

	return ( status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED ) ? qtrue : qfalse;
}

/*
=============
Pip_FenceLanded

Bounded wait for a fence (microseconds; 0 = the non-blocking test).  Used by
the r_pip_readback_wait lever: the GPU is usually done with a pip-sized copy
within a fraction of a millisecond, so a small budget catches the current
frame's pixels in time to show them without adding a frame of pipeline lag.
=============
*/
static qboolean Pip_FenceLanded( GLsync fence, GLuint microseconds ) {
	GLenum	status = qglClientWaitSync( fence, 0, (GLuint64)microseconds * 1000 );

	return ( status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED ) ? qtrue : qfalse;
}

/*
=============
Pip_LatencyLog

r_pip_latencylog: which pipeline stage did this presented frame take?  Only
fires while pip content is actually flowing.
=============
*/
static void Pip_LatencyLog( const char *path ) {
	if ( r_pipLatencyLog && r_pipLatencyLog->integer ) {
		ri.Printf( PRINT_ALL, "pip latency: %s\n", path );
	}
}

/*
=============
Pip_DestroyFBO
=============
*/
static void Pip_DestroyFBO( void ) {
	int		i;

	for ( i = 0; i < 2; i++ ) {
		if ( pipFence[i] ) {
			qglDeleteSync( pipFence[i] );
			pipFence[i] = 0;
		}
		if ( pipPbo[i] ) {
			qglDeleteBuffers( 1, &pipPbo[i] );
			pipPbo[i] = 0;
		}
	}
	if ( pipFbo.frameBuffer ) {
		qglDeleteFramebuffers( 1, &pipFbo.frameBuffer );
	}
	if ( pipColorRB ) {
		qglDeleteRenderbuffers( 1, &pipColorRB );
	}
	if ( pipDepthRB ) {
		qglDeleteRenderbuffers( 1, &pipDepthRB );
	}
	Com_Memset( &pipFbo, 0, sizeof( pipFbo ) );
	pipColorRB = 0;
	pipDepthRB = 0;
}

/*
=============
Pip_CreateFBO

Build the half-resolution framebuffer and fill the FBO_t shim that the
pipeline will use as the PiP scene's targetFbo.
=============
*/
static qboolean Pip_CreateFBO( int w, int h ) {
	GLuint colorRB = 0, depthRB = 0, fbo = 0;
	GLenum depthFormat = 0;

	qglGenRenderbuffers( 1, &colorRB );
	qglBindRenderbuffer( GL_RENDERBUFFER, colorRB );
	qglRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA8, w, h );

	if ( glConfig.depthBits >= 24 ) {
		depthFormat = GL_DEPTH_COMPONENT24;
	}
	else if ( glConfig.depthBits >= 16 ) {
		depthFormat = GL_DEPTH_COMPONENT16;
	}

	if ( depthFormat ) {
		qglGenRenderbuffers( 1, &depthRB );
		qglBindRenderbuffer( GL_RENDERBUFFER, depthRB );
		qglRenderbufferStorage( GL_RENDERBUFFER, depthFormat, w, h );
	}

	qglGenFramebuffers( 1, &fbo );
	qglBindFramebuffer( GL_FRAMEBUFFER, fbo );
	qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRB );
	if ( depthRB ) {
		qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRB );
	}

	if ( qglCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		ri.Printf( PRINT_ALL, "PiP: framebuffer incomplete, PiP disabled\n" );
		qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
		qglDeleteFramebuffers( 1, &fbo );
		qglDeleteRenderbuffers( 1, &colorRB );
		if ( depthRB ) {
			qglDeleteRenderbuffers( 1, &depthRB );
		}
		return qfalse;
	}

	// fill the FBO_t shim (the pipeline only needs frameBuffer/width/height to bind it)
	Com_Memset( &pipFbo, 0, sizeof( pipFbo ) );
	Q_strncpyz( pipFbo.name, "_pip", sizeof( pipFbo.name ) );
	pipFbo.index = -1;
	pipFbo.frameBuffer = fbo;
	pipFbo.colorBuffers[0] = colorRB;
	pipFbo.colorFormat = GL_RGBA8;
	pipFbo.depthBuffer = depthRB;
	pipFbo.depthFormat = depthFormat;
	pipFbo.width = w;
	pipFbo.height = h;

	pipColorRB = colorRB;
	pipDepthRB = depthRB;
	pipWidth = w;
	pipHeight = h;

	// double-buffered pack buffers for the async readback
	qglGenBuffers( 2, pipPbo );
	qglBindBuffer( GL_PIXEL_PACK_BUFFER, pipPbo[0] );
	qglBufferData( GL_PIXEL_PACK_BUFFER, w * h * 4, NULL, GL_STREAM_READ );
	qglBindBuffer( GL_PIXEL_PACK_BUFFER, pipPbo[1] );
	qglBufferData( GL_PIXEL_PACK_BUFFER, w * h * 4, NULL, GL_STREAM_READ );
	qglBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
	pipParity = 0;

	return qtrue;
}

/*
=============
Pip_AllocBuffers

(Re)allocate the readback buffer and the black idle frame.
=============
*/
static void Pip_FreeBuffers( void ) {
	if ( pipBlack ) {
		free( pipBlack );
		pipBlack = NULL;
	}
}

static qboolean Pip_AllocBuffers( int w, int h ) {
	Pip_FreeBuffers();

	pipBlack = (byte *)malloc( w * h * 4 );
	if ( !pipBlack ) {
		ri.Printf( PRINT_ALL, "PiP: out of memory for the idle frame, PiP disabled\n" );
		return qfalse;
	}
	Com_Memset( pipBlack, 0, w * h * 4 );

	return qtrue;
}

/*
=============
R_PipRendererInit

Called from R_Init once the GL context, the extensions and the shaders are
live.  Registers the control cvars and brings up the framebuffer and the OS
window so the PiP window is always there, showing black until a pipRender
scene arrives.
=============
*/
void R_PipRendererInit( void ) {
	int w, h;
	windowDesc_t desc;

	Pip_RegisterCvars();

	if ( !Pip_GLReady() ) {
		return;
	}

	w = glConfig.vidWidth / 2;
	h = glConfig.vidHeight / 2;
	if ( w < 2 || h < 2 ) {
		return;
	}

	if ( !Pip_CreateFBO( w, h ) ) {
		return;
	}

	if ( !Pip_AllocBuffers( w, h ) ) {
		return;
	}

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.api = GRAPHICS_API_OPENGL;

	pipWindowHandle = ri.WIN_Secondary_Create( &desc, (int)r_pipWindowX->value, (int)r_pipWindowY->value,
											w, h, r_pipTopmost->integer ? qtrue : qfalse );
	if ( !pipWindowHandle ) {
		// the SDL layer prints the reason
		return;
	}

	pipReady = qtrue;
	ri.Printf( PRINT_ALL, "PiP: rend2 pipeline ready, window at %dx%d\n", w, h );
}

/*
=============
R_PipRendererShutdown

Tear everything down (called from RE_Shutdown, which also runs on vid
restarts; the next R_Init rebuilds it).
=============
*/
void R_PipRendererShutdown( void ) {
	if ( pipWindowHandle ) {
		ri.WIN_Secondary_Destroy( pipWindowHandle );
		pipWindowHandle = NULL;
	}
	if ( pipFbo.frameBuffer || pipColorRB || pipDepthRB ) {
		Pip_DestroyFBO();
	}
	if ( pipPostFbo ) {
		// GL objects only: the FBO_t shell itself is hunk memory and goes away
		// with the full renderer shutdown this runs from
		if ( pipPostFbo->frameBuffer ) {
			qglDeleteFramebuffers( 1, &pipPostFbo->frameBuffer );
		}
		if ( pipPostFbo->colorBuffers[0] ) {
			qglDeleteRenderbuffers( 1, &pipPostFbo->colorBuffers[0] );
		}
		pipPostFbo = NULL;
	}
	for ( int i = 0; i < 2; i++ ) {
		if ( pipDepthSave[i] ) {
			if ( pipDepthSave[i]->frameBuffer ) {
				qglDeleteFramebuffers( 1, &pipDepthSave[i]->frameBuffer );
			}
			if ( pipDepthSave[i]->packedDepthStencilBuffer ) {
				qglDeleteRenderbuffers( 1, &pipDepthSave[i]->packedDepthStencilBuffer );
			}
			pipDepthSave[i] = NULL;
		}
	}
	Pip_FreeBuffers();
	pipWidth = pipHeight = 0;
	pipContentDirty = qfalse;
	pipParity = 0;
	pipReady = qfalse;
}

/*
=============
Pip_ShouldRender

True when a pipRender scene may actually go through the pipeline
(master cvar on, window and framebuffer alive).
=============
*/
qboolean Pip_ShouldRender( void ) {
	return ( pipReady && r_pipWindow && r_pipWindow->integer ) ? qtrue : qfalse;
}

/*
=============
Pip_RateSkip

SWGL PiP perf lever: true when this pip scene should not go through the
pipeline at all (r_pip_rate N renders once every N frames).  Only consulted
when the scene would really render, so a skipped frame leaves the SDL window
showing the last image it got.
=============
*/
qboolean Pip_RateSkip( void ) {
	if ( !r_pipRate || r_pipRate->integer <= 1 ) {
		return qfalse;
	}

	if ( pipRateCounter <= 0 ) {
		pipRateCounter = r_pipRate->integer - 1;	// this one renders, the next ones are skipped
		return qfalse;
	}

	pipRateCounter--;
	return qtrue;
}

/*
=============
Pip_MarkSkipPresent

Called from the tr_scene.cpp hook when this frame's pip scene was rate-skipped;
RB_PipPresent then does absolutely nothing so the window keeps showing the last
image (no readback, no idle black, no fences consumed).
=============
*/
void Pip_MarkSkipPresent( void ) {
	pipSkipPresent = qtrue;
}

/*
=============
Pip_Direct

SWGL PiP perf lever: when on, the pip main view is rendered straight into the
window framebuffer at its native size (quarter the pixels) and the whole
postprocess chain is skipped (its buffers are pinned to the main resolution,
and with the chain gone so are the depth save/restore).  With r_hdr on this
means no tonemap for the pip only - slightly different colors, accepted for a
guide window.
=============
*/
qboolean Pip_Direct( void ) {
	return ( r_pipDirect && r_pipDirect->integer ) ? qtrue : qfalse;
}

/*
=============
Pip_Fbo / Pip_Width / Pip_Height

Handed to the tr_scene.cpp hook so the pip scene's main view lands in our
framebuffer.
=============
*/
FBO_t *Pip_Fbo( void ) {
	return pipReady ? &pipFbo : NULL;
}

int Pip_Width( void ) {
	return pipWidth;
}

int Pip_Height( void ) {
	return pipHeight;
}

/*
=============
Pip_PostProcessFbo

Main-resolution RGBA8 colour target for the pip postprocess chain: the chain's
final passes write here instead of the backbuffer so the main window is left
alone.  Created on first use, recreated if the display mode changed.  No depth
buffer is needed (the chain's depth-copy step is skipped for pip targets).
=============
*/
FBO_t *Pip_PostProcessFbo( void ) {
	if ( !pipReady ) {
		return NULL;
	}

	if ( pipPostFbo && pipPostFbo->width == glConfig.vidWidth && pipPostFbo->height == glConfig.vidHeight ) {
		return pipPostFbo;
	}

	// display mode changed (rare): only release the GL objects here, the FBO_t
	// shell itself lives in the renderer's hunk memory until the vidrestart
	if ( pipPostFbo ) {
		if ( pipPostFbo->frameBuffer ) {
			qglDeleteFramebuffers( 1, &pipPostFbo->frameBuffer );
		}
		if ( pipPostFbo->colorBuffers[0] ) {
			qglDeleteRenderbuffers( 1, &pipPostFbo->colorBuffers[0] );
		}
		pipPostFbo = NULL;
	}

	pipPostFbo = FBO_Create( "_pipPost", glConfig.vidWidth, glConfig.vidHeight );
	FBO_Bind( pipPostFbo );
	FBO_CreateBuffer( pipPostFbo, GL_RGBA8, 0, 0 );

	if ( qglCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		ri.Printf( PRINT_ALL, "PiP: postprocess framebuffer incomplete, PiP disabled\n" );
		if ( pipPostFbo->frameBuffer ) {
			qglDeleteFramebuffers( 1, &pipPostFbo->frameBuffer );
		}
		if ( pipPostFbo->colorBuffers[0] ) {
			qglDeleteRenderbuffers( 1, &pipPostFbo->colorBuffers[0] );
		}
		pipPostFbo = NULL;
		return NULL;
	}

	return pipPostFbo;
}

/*
=============
Pip_DepthSaveFbo

Lazily created main-resolution packed depth-stencil save slots (0 = for the
renderFbo, 1 = for its msaa resolve copy).  The pip hook saves the main window
camera's depth here right before the pip scene runs and restores it once the
pip scene is done, so the depth that is visible at frame end - which is what
depth-based Reshade effects read - stays matching the main window instead of
the PiP view.
=============
*/
FBO_t *Pip_DepthSaveFbo( int which ) {
	if ( !pipReady || which < 0 || which >= 2 ) {
		return NULL;
	}

	if ( pipDepthSave[which] && pipDepthSave[which]->width == glConfig.vidWidth && pipDepthSave[which]->height == glConfig.vidHeight ) {
		return pipDepthSave[which];
	}

	// display mode changed (rare): only release the GL objects, the FBO_t
	// shell itself lives in the renderer's hunk memory until the vidrestart
	if ( pipDepthSave[which] ) {
		if ( pipDepthSave[which]->frameBuffer ) {
			qglDeleteFramebuffers( 1, &pipDepthSave[which]->frameBuffer );
		}
		if ( pipDepthSave[which]->packedDepthStencilBuffer ) {
			qglDeleteRenderbuffers( 1, &pipDepthSave[which]->packedDepthStencilBuffer );
		}
		pipDepthSave[which] = NULL;
	}

	pipDepthSave[which] = FBO_Create( va( "_pipDepthSave%d", which ), glConfig.vidWidth, glConfig.vidHeight );
	FBO_Bind( pipDepthSave[which] );
	FBO_CreateBuffer( pipDepthSave[which], GL_DEPTH24_STENCIL8, 0, 0 );

	if ( qglCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		ri.Printf( PRINT_ALL, "PiP: depth save framebuffer %d incomplete, depth restore disabled\n", which );
		if ( pipDepthSave[which]->frameBuffer ) {
			qglDeleteFramebuffers( 1, &pipDepthSave[which]->frameBuffer );
		}
		if ( pipDepthSave[which]->packedDepthStencilBuffer ) {
			qglDeleteRenderbuffers( 1, &pipDepthSave[which]->packedDepthStencilBuffer );
		}
		pipDepthSave[which] = NULL;
		return NULL;
	}

	return pipDepthSave[which];
}

/*
=============
R_AddPipPresentCmd
=============
*/
static void Pip_AddPresentCmd( void ) {
	pipPresentCommand_t	*cmd;

	cmd = (pipPresentCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_PIP_PRESENT;
}

/*
=============
R_PipAddFramePresentCmd

Queued by RE_EndFrame every frame so the window always gets updated (black
when idle).  No-op when the PiP layer is not alive.
=============
*/
void R_PipAddFramePresentCmd( void ) {
	if ( pipReady ) {
		Pip_AddPresentCmd();
	}
}

/*
=============
Pip_MarkContentDirty

Called from the tr_scene.cpp hook when a pipRender scene went through the
pipeline into our framebuffer; the next present shows it instead of black.
=============
*/
void Pip_MarkContentDirty( void ) {
	pipContentDirty = qtrue;
}

/*
=============
Pip_DrawCrosshair

Fake crosshair drawn CPU-side into the frame that goes to the window, so it
stays visible regardless of cg_draw2d (and on the black idle screen too).
The buffer is bottom-up (GL readback) and the present flips it, so a vertical
offset that should sit LOWER on screen is drawn ABOVE centre here.  The offset
puts the crosshair just above the third-person character's head (~h/10 lower
than centre at the window size this mod targets, which keeps the aim point
where the bullets actually go).
=============
*/
static void Pip_DrawCrosshair( byte *buf, int w, int h ) {
	int		cx = w / 2;
	int		cy = h / 2 - h / 14;	// lower on screen (present flips this bottom-up buffer):
									// sits just above the character's head top at the
									// window sizes this mod targets, matching where shots land
	int		gap = 6;		// empty space around the centre
	int		len = 12;		// arm length
	int		i;
	byte	*p;

	if ( !r_pipCrosshair || !r_pipCrosshair->integer ) {
		return;
	}

	// centre dot
	p = buf + (((size_t)cy * w) + cx) * 4;
	p[0] = p[1] = p[2] = p[3] = 255;

	for ( i = gap; i < gap + len; i++ ) {
		if ( cx + i < w ) {
			p = buf + (((size_t)cy * w) + (cx + i)) * 4;
			p[0] = p[1] = p[2] = p[3] = 255;
		}
		if ( cx - i >= 0 ) {
			p = buf + (((size_t)cy * w) + (cx - i)) * 4;
			p[0] = p[1] = p[2] = p[3] = 255;
		}
		if ( cy + i < h ) {
			p = buf + (((size_t)(cy + i) * w) + cx) * 4;
			p[0] = p[1] = p[2] = p[3] = 255;
		}
		if ( cy - i >= 0 ) {
			p = buf + (((size_t)(cy - i) * w) + cx) * 4;
			p[0] = p[1] = p[2] = p[3] = 255;
		}
	}
}

/*
=============
RB_PipPresent

Backend command, queued every frame right after the swap.  The readback runs
into double-buffered pack buffers so the render thread never stalls on it:
whatever landed from the previous frame is blitted to the SDL window now, and
this frame's copy is queued for the next one (one frame of latency).  While a
pip scene is rendering the window shows the live view; as soon as one does not
(pipRender off / camerastatic back to 0) the window goes back to black.  Both
cases carry the fake crosshair.  On r_pip_rate-skipped frames the present does
nothing so the window keeps the last image.  r_pip_readback_wait (µs) then
opts into showing the current frame when its copy lands in time, removing that
one frame of pipeline latency; r_pip_latencylog reports which path each frame
took.
=============
*/
const void *RB_PipPresent( const void *data ) {
	const pipPresentCommand_t	*cmd = (const pipPresentCommand_t *)data;
	int							previous = pipParity ^ 1;

	// rate-skipped frame (r_pip_rate): the window simply keeps showing the last
	// image it got, so do nothing at all - and above all do not touch the
	// dirty/idle logic or consume any fences, they stay as they are
	if ( pipSkipPresent ) {
		pipSkipPresent = qfalse;
		Pip_LatencyLog( "present skipped (r_pip_rate)" );
		return (const void *)(cmd + 1);
	}

	if ( !pipReady || !pipWindowHandle ) {
		return (const void *)(cmd + 1);
	}

	if ( pipContentDirty ) {
		qboolean	queuedThisFrame = qfalse;
		int			newSlot;

		// show the frame the GPU delivered from the other buffer, if any
		if ( pipFence[previous] && Pip_FenceSignaled( pipFence[previous] ) ) {
			void	*mapped;

			qglBindBuffer( GL_PIXEL_PACK_BUFFER, pipPbo[previous] );
			mapped = qglMapBufferRange( GL_PIXEL_PACK_BUFFER, 0, pipWidth * pipHeight * 4,
									   GL_MAP_READ_BIT | GL_MAP_WRITE_BIT );
			if ( mapped ) {
				// the SDL layer copies the pixels during the call, so we can unmap right after;
				// pack rows are bottom-up, hence the flip
				Pip_DrawCrosshair( (byte *)mapped, pipWidth, pipHeight );
				ri.WIN_Secondary_Present( pipWindowHandle, (const byte *)mapped, pipWidth, pipHeight, qtrue );
				qglUnmapBuffer( GL_PIXEL_PACK_BUFFER );
			}
			qglDeleteSync( pipFence[previous] );
			pipFence[previous] = 0;
			Pip_LatencyLog( "showed the previous frame's copy" );
		}

		// retire the write-side fence too: without this, one frame where the
		// show-side fence was not ready yet would have left the write slot
		// permanently occupied and the window frozen after a few seconds
		if ( pipFence[pipParity] && Pip_FenceSignaled( pipFence[pipParity] ) ) {
			qglDeleteSync( pipFence[pipParity] );
			pipFence[pipParity] = 0;
		}

		// queue this frame's copy into the buffer the GPU just freed
		if ( !pipFence[pipParity] ) {
			FBO_Bind( &pipFbo );
			qglBindBuffer( GL_PIXEL_PACK_BUFFER, pipPbo[pipParity] );
			qglReadPixels( 0, 0, pipWidth, pipHeight, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
			pipFence[pipParity] = qglFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
			pipParity = pipParity ^ 1;
			FBO_Bind( NULL );	// back to the default framebuffer
			qglBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
			queuedThisFrame = qtrue;
		}

		// SWGL PiP latency lever (r_pip_readback_wait > 0): wait up to the budget
		// for THIS frame's copy and, when it lands in time, show it straight away -
		// that removes the double-buffer design's one frame of pipeline lag.  A
		// GPU with headroom is usually done with a pip-sized copy right away, so
		// the log fills with CURRENT lines; when the budget expires the copy just
		// becomes next frame's show-side content as usual and nothing more happens
		// here.  Keep the budget well under a frame time: it bounds the stall.
		newSlot = pipParity ^ 1;	// the slot the copy sits in (parity toggled at queue time)
		if ( r_pipReadbackWait && r_pipReadbackWait->integer > 0 && queuedThisFrame && pipFence[newSlot] ) {
			if ( Pip_FenceLanded( pipFence[newSlot], (GLuint)r_pipReadbackWait->integer ) ) {
				void	*mapped;

				qglBindBuffer( GL_PIXEL_PACK_BUFFER, pipPbo[newSlot] );
				mapped = qglMapBufferRange( GL_PIXEL_PACK_BUFFER, 0, pipWidth * pipHeight * 4,
										   GL_MAP_READ_BIT | GL_MAP_WRITE_BIT );
				if ( mapped ) {
					Pip_DrawCrosshair( (byte *)mapped, pipWidth, pipHeight );
					ri.WIN_Secondary_Present( pipWindowHandle, (const byte *)mapped, pipWidth, pipHeight, qtrue );
					qglUnmapBuffer( GL_PIXEL_PACK_BUFFER );
				}
				// consumed - the buffer becomes the next write slot
				qglDeleteSync( pipFence[newSlot] );
				pipFence[newSlot] = 0;
				Pip_LatencyLog( "showed the CURRENT frame");
			} else {
				Pip_LatencyLog( "budget expired - window keeps the last image");
			}
		}

		pipContentDirty = qfalse;
	}
	else {
		// idle: the window must keep following the camerastatic state, so present
		// black with the crosshair every frame; a live frame still in flight from
		// the last active round is dropped (not shown) and its buffers freed
		if ( pipFence[0] ) {
			qglDeleteSync( pipFence[0] );
			pipFence[0] = 0;
		}
		if ( pipFence[1] ) {
			qglDeleteSync( pipFence[1] );
			pipFence[1] = 0;
		}
		pipParity = 0;

		Com_Memset( pipBlack, 0, (size_t)pipWidth * pipHeight * 4 );
		Pip_DrawCrosshair( pipBlack, pipWidth, pipHeight );
		ri.WIN_Secondary_Present( pipWindowHandle, pipBlack, pipWidth, pipHeight, qtrue );
	}

	return (const void *)(cmd + 1);
}
