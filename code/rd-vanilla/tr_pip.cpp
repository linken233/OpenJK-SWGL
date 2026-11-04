/*
===========================================================================
SWGL PiP support for the vanilla renderer.

The PiP window (and its half-resolution framebuffer) is created at renderer
startup so it is always there, showing black.  Idle cost is kept near zero:
no scene is rendered unless cgame submits a RenderScene with refdef.pipRender
set (which cgame only does while the static camera is active).

Command order per frame:
main scene -> HUD 2D -> [pip bind -> pip scene] -> pip present -> swap.

The present command runs every frame: it shows the scene rendered by the pip
bind this frame, or black when nothing was rendered into the pip FBO.

Only rd-vanilla (rdsp_swgl_x86_64.dll) supports this; rd-rend2 ignores pipRender.
===========================================================================
*/

#include "tr_local.h"

// glext.h in this project lacks a plain GL_RGBA8 define
#ifndef GL_RGBA8
#define GL_RGBA8					0x8058
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16		0x81A5
#endif

// lazily loaded framebuffer entry points (core names with EXT fallback)
typedef PFNGLGENFRAMEBUFFERSPROC		p_pfnGenFramebuffers;
typedef PFNGLBINDFRAMEBUFFERPROC		p_pfnBindFramebuffer;
typedef PFNGLDELETEFRAMEBUFFERSPROC		p_pfnDeleteFramebuffers;
typedef PFNGLGENRENDERBUFFERSPROC		p_pfnGenRenderbuffers;
typedef PFNGLDELETERENDERBUFFERSPROC	p_pfnDeleteRenderbuffers;
typedef PFNGLRENDERBUFFERSTORAGEPROC	p_pfnRenderbufferStorage;
typedef PFNGLFRAMEBUFFERRENDERBUFFERPROC p_pfnFramebufferRenderbuffer;
typedef PFNGLCHECKFRAMEBUFFERSTATUSPROC	p_pfnCheckFramebufferStatus;

static p_pfnGenFramebuffers			pGenFramebuffers;
static p_pfnBindFramebuffer			pBindFramebuffer;
static p_pfnDeleteFramebuffers		pDeleteFramebuffers;
static p_pfnGenRenderbuffers		pGenRenderbuffers;
static p_pfnDeleteRenderbuffers		pDeleteRenderbuffers;
static p_pfnRenderbufferStorage		pRenderbufferStorage;
static p_pfnFramebufferRenderbuffer	pFramebufferRenderbuffer;
static p_pfnCheckFramebufferStatus	pCheckFramebufferStatus;

static cvar_t	*r_pipWindow;
static cvar_t	*r_pipWindowX;
static cvar_t	*r_pipWindowY;
static cvar_t	*r_pipTopmost;
static cvar_t	*r_pipCrosshair;
static cvar_t	*r_pipBorderless;	// read by the SDL layer at window creation

static qboolean	pipGLAttempted = qfalse;
static qboolean	pipGLReady = qfalse;

static void		*pipWindowHandle;
static int		pipWidth, pipHeight;
static GLuint	pipFBO, pipColorRB, pipDepthRB;
static byte		*pipReadback;
static int		pipReadbackSize;
static qboolean	pipContentDirty;	// set by RB_PipBind, cleared after the present

/*
=============
R_PipCvar

r_pip_window master switch, registered at renderer startup.
=============
*/
cvar_t *R_PipCvar( void ) {
	if ( !r_pipWindow ) {
		r_pipWindow = ri.Cvar_Get( "r_pip_window", "0", CVAR_ARCHIVE );
	}
	return r_pipWindow;
}

/*
=============
Pip_RegisterControlCvars
=============
*/
static void Pip_RegisterControlCvars( void ) {
	if ( !r_pipWindowX ) {
		r_pipWindowX = ri.Cvar_Get( "r_pip_window_x", "-1", CVAR_ARCHIVE );	// -3 = bottom-right corner of the other display
		r_pipWindowY = ri.Cvar_Get( "r_pip_window_y", "-1", CVAR_ARCHIVE );	// -3 = bottom-right corner of the other display
		r_pipTopmost = ri.Cvar_Get( "r_pip_topmost", "1", CVAR_ARCHIVE );
		r_pipCrosshair = ri.Cvar_Get( "r_pip_crosshair", "1", CVAR_ARCHIVE );
		r_pipBorderless = ri.Cvar_Get( "r_pip_borderless", "1", CVAR_ARCHIVE );
	}
}

/*
=============
Pip_ShouldRender

True when a pipRender scene may actually go through the pipeline
(master cvar on, GL entry points loaded, window and framebuffer alive).
=============
*/
qboolean Pip_ShouldRender( void ) {
	return (qboolean)( pipGLReady && pipWindowHandle && pipFBO && R_PipCvar()->integer );
}

/*
=============
R_PipAddFramePresentCmd

Queued by RE_EndFrame every frame so the window always gets updated (black
when idle).  No-op when the PiP window is not alive.
=============
*/
void R_PipAddFramePresentCmd( void ) {
	if ( pipGLReady && pipWindowHandle ) {
		R_AddPipPresentCmd();
	}
}

/*
=============
Pip_GLReady

Load the framebuffer entry points once. Core names first, EXT suffix fallback.
=============
*/
static qboolean Pip_GLReady( void ) {
	if ( pipGLReady ) {
		return qtrue;
	}
	if ( pipGLAttempted ) {
		return qfalse;
	}
	pipGLAttempted = qtrue;

	if ( !ri.GL_ExtensionSupported( "GL_EXT_framebuffer_object" ) &&
		 !ri.GL_ExtensionSupported( "GL_ARB_framebuffer_object" ) ) {
		Com_Printf( "PiP: no framebuffer object support, PiP disabled\n" );
		return qfalse;
	}

	pGenFramebuffers = (p_pfnGenFramebuffers)ri.GL_GetProcAddress( "glGenFramebuffers" );
	if ( !pGenFramebuffers ) {
		pGenFramebuffers = (p_pfnGenFramebuffers)ri.GL_GetProcAddress( "glGenFramebuffersEXT" );
	}
	pBindFramebuffer = (p_pfnBindFramebuffer)ri.GL_GetProcAddress( "glBindFramebuffer" );
	if ( !pBindFramebuffer ) {
		pBindFramebuffer = (p_pfnBindFramebuffer)ri.GL_GetProcAddress( "glBindFramebufferEXT" );
	}
	pDeleteFramebuffers = (p_pfnDeleteFramebuffers)ri.GL_GetProcAddress( "glDeleteFramebuffers" );
	if ( !pDeleteFramebuffers ) {
		pDeleteFramebuffers = (p_pfnDeleteFramebuffers)ri.GL_GetProcAddress( "glDeleteFramebuffersEXT" );
	}
	pGenRenderbuffers = (p_pfnGenRenderbuffers)ri.GL_GetProcAddress( "glGenRenderbuffers" );
	if ( !pGenRenderbuffers ) {
		pGenRenderbuffers = (p_pfnGenRenderbuffers)ri.GL_GetProcAddress( "glGenRenderbuffersEXT" );
	}
	pDeleteRenderbuffers = (p_pfnDeleteRenderbuffers)ri.GL_GetProcAddress( "glDeleteRenderbuffers" );
	if ( !pDeleteRenderbuffers ) {
		pDeleteRenderbuffers = (p_pfnDeleteRenderbuffers)ri.GL_GetProcAddress( "glDeleteRenderbuffersEXT" );
	}
	pRenderbufferStorage = (p_pfnRenderbufferStorage)ri.GL_GetProcAddress( "glRenderbufferStorage" );
	if ( !pRenderbufferStorage ) {
		pRenderbufferStorage = (p_pfnRenderbufferStorage)ri.GL_GetProcAddress( "glRenderbufferStorageEXT" );
	}
	pFramebufferRenderbuffer = (p_pfnFramebufferRenderbuffer)ri.GL_GetProcAddress( "glFramebufferRenderbuffer" );
	if ( !pFramebufferRenderbuffer ) {
		pFramebufferRenderbuffer = (p_pfnFramebufferRenderbuffer)ri.GL_GetProcAddress( "glFramebufferRenderbufferEXT" );
	}
	pCheckFramebufferStatus = (p_pfnCheckFramebufferStatus)ri.GL_GetProcAddress( "glCheckFramebufferStatus" );
	if ( !pCheckFramebufferStatus ) {
		pCheckFramebufferStatus = (p_pfnCheckFramebufferStatus)ri.GL_GetProcAddress( "glCheckFramebufferStatusEXT" );
	}

	if ( !pGenFramebuffers || !pBindFramebuffer || !pDeleteFramebuffers ||
		 !pGenRenderbuffers || !pDeleteRenderbuffers || !pRenderbufferStorage ||
		 !pFramebufferRenderbuffer || !pCheckFramebufferStatus ) {
		Com_Printf( "PiP: could not load framebuffer entry points, PiP disabled\n" );
		return qfalse;
	}

	pipGLReady = qtrue;
	return qtrue;
}

/*
=============
Pip_DestroyFBO
=============
*/
static void Pip_DestroyFBO( void ) {
	if ( !pipGLReady ) {
		return;
	}
	if ( pipFBO ) {
		pDeleteFramebuffers( 1, &pipFBO );
		pipFBO = 0;
	}
	if ( pipColorRB ) {
		pDeleteRenderbuffers( 1, &pipColorRB );
		pipColorRB = 0;
	}
	if ( pipDepthRB ) {
		pDeleteRenderbuffers( 1, &pipDepthRB );
		pipDepthRB = 0;
	}
}

/*
=============
Pip_AllocReadback

(Re)allocate the CPU readback buffer for a w x h RGBA image.
=============
*/
static void Pip_AllocReadback( int w, int h ) {
	int	need = w * h * 4;

	if ( pipReadback && pipReadbackSize == need ) {
		return;
	}
	if ( pipReadback ) {
		ri.Z_Free( pipReadback );
		pipReadback = NULL;
		pipReadbackSize = 0;
	}
	pipReadback = (byte *)ri.Z_Malloc( need, TAG_HUNKALLOC, qfalse, 16 );
	if ( pipReadback ) {
		pipReadbackSize = need;
	}
}

/*
=============
Pip_CreateFBO

(Re)create the PiP framebuffer at the requested size.
=============
*/
static void Pip_CreateFBO( int w, int h ) {
	GLenum depthFormat = 0;

	Pip_DestroyFBO();

	if ( w < 2 || h < 2 ) {
		return;
	}

	if ( glConfig.depthBits >= 24 ) {
		depthFormat = GL_DEPTH_COMPONENT24;
	}
	else if ( glConfig.depthBits >= 16 ) {
		depthFormat = GL_DEPTH_COMPONENT16;
	}

	pGenRenderbuffers( 1, &pipColorRB );
	pBindFramebuffer( GL_RENDERBUFFER, pipColorRB );
	pRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA8, w, h );
	pBindFramebuffer( GL_RENDERBUFFER, 0 );

	if ( depthFormat ) {
		pGenRenderbuffers( 1, &pipDepthRB );
		pBindFramebuffer( GL_RENDERBUFFER, pipDepthRB );
		pRenderbufferStorage( GL_RENDERBUFFER, depthFormat, w, h );
		pBindFramebuffer( GL_RENDERBUFFER, 0 );
	}

	pGenFramebuffers( 1, &pipFBO );
	pBindFramebuffer( GL_FRAMEBUFFER, pipFBO );
	pFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, pipColorRB );
	if ( pipDepthRB ) {
		pFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pipDepthRB );
	}

	if ( pCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		Com_Printf( "PiP: framebuffer incomplete, PiP disabled\n" );
		Pip_DestroyFBO();
		return;
	}

	Pip_AllocReadback( w, h );

	pipWidth = w;
	pipHeight = h;

	// the SDL layer keeps its streaming texture in sync through WIN_Secondary_Create
	if ( pipWindowHandle ) {
		pipWindowHandle = ri.WIN_Secondary_Create( NULL, -1, -1, w, h, qfalse );
	}
}

/*
=============
Pip_CreateWindow
=============
*/
static void Pip_CreateWindow( int w, int h ) {
	windowDesc_t	desc;

	if ( pipWindowHandle ) {
		return;
	}

	memset( &desc, 0, sizeof( desc ) );
	desc.api = GRAPHICS_API_OPENGL;

	pipWindowHandle = ri.WIN_Secondary_Create( &desc, (int)r_pipWindowX->value, (int)r_pipWindowY->value,
											   w, h, r_pipTopmost->integer ? qtrue : qfalse );
}

/*
=============
R_PipRendererInit

Called from R_Init once the GL context and glConfig are live.  Registers the
control cvars and brings up the framebuffer and the OS window so the PiP
window is always there, showing black until a pipRender scene arrives.
=============
*/
void R_PipRendererInit( void ) {
	int w, h;

	R_PipCvar();
	Pip_RegisterControlCvars();

	if ( !Pip_GLReady() ) {
		return;
	}

	w = glConfig.vidWidth / 2;
	h = glConfig.vidHeight / 2;
	if ( w < 2 || h < 2 ) {
		return;
	}

	Pip_CreateFBO( w, h );
	if ( !pipFBO || !pipReadback ) {
		return;
	}

	Pip_CreateWindow( w, h );
}

/*
=============
R_PipRendererShutdown

Tear everything down (called from RE_Shutdown, which also runs on vid restarts;
the next R_Init rebuilds it).
=============
*/
void R_PipRendererShutdown( void ) {
	if ( pipWindowHandle ) {
		ri.WIN_Secondary_Destroy( pipWindowHandle );
		pipWindowHandle = NULL;
	}
	if ( pipGLReady ) {
		Pip_DestroyFBO();
	}
	if ( pipReadback ) {
		ri.Z_Free( pipReadback );
		pipReadback = NULL;
	}
	pipReadbackSize = 0;
	pipWidth = pipHeight = 0;
	pipContentDirty = qfalse;
	pipGLReady = qfalse;
	pipGLAttempted = qfalse;
	memset( &pipColorRB, 0, sizeof( pipColorRB ) * 3 );	// pipFBO, pipColorRB, pipDepthRB
}

/*
=============
RB_PipBind

Backend command: make the PiP framebuffer the current render target.
=============
*/
const void *RB_PipBind( const void *data ) {
	const pipBindCommand_t	*cmd = (const pipBindCommand_t *)data;

	if ( !pipGLReady || !pipFBO ) {
		return (const void *)(cmd + 1);
	}

	pBindFramebuffer( GL_FRAMEBUFFER, pipFBO );
	qglDrawBuffer( GL_COLOR_ATTACHMENT0 );
	qglViewport( 0, 0, pipWidth, pipHeight );
	qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	pipContentDirty = qtrue;

	return (const void *)(cmd + 1);
}

/*
=============
Pip_DrawCrosshair

Fake crosshair drawn CPU-side into the frame that goes to the window, so it
stays visible regardless of cg_draw2d (and on the black idle screen too).
The design is symmetric, so it looks the same after the bottom-up flip.
=============
*/
static void Pip_DrawCrosshair( byte *buf, int w, int h ) {
	int		cx = w / 2;
	int		cy = h / 2;
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

Backend command, queued every frame right before the swap: read the PiP
framebuffer back and blit it to the PiP window.  Shows black (idle) unless a
pipRender scene landed in the framebuffer this frame.
=============
*/
const void *RB_PipPresent( const void *data ) {
	const pipPresentCommand_t	*cmd = (const pipPresentCommand_t *)data;
	int							w = glConfig.vidWidth / 2;
	int							h = glConfig.vidHeight / 2;

	if ( !pipGLReady || !pipWindowHandle ) {
		return (const void *)(cmd + 1);
	}

	// resolution may have changed under us (vid_restart)
	if ( w >= 2 && h >= 2 && ( w != pipWidth || h != pipHeight ) ) {
		Pip_CreateFBO( w, h );
	}

	if ( !pipFBO || !pipReadback ) {
		return (const void *)(cmd + 1);
	}

	pBindFramebuffer( GL_FRAMEBUFFER, pipFBO );

	if ( !pipContentDirty ) {
		// idle frame: clear the framebuffer to black so the window shows black
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	qglReadPixels( 0, 0, pipWidth, pipHeight, GL_RGBA, GL_UNSIGNED_BYTE, pipReadback );
	pBindFramebuffer( GL_FRAMEBUFFER, 0 );

	Pip_DrawCrosshair( pipReadback, pipWidth, pipHeight );

	// readback rows are bottom-up, the SDL side flips them
	ri.WIN_Secondary_Present( pipWindowHandle, pipReadback, pipWidth, pipHeight, qtrue );

	pipContentDirty = qfalse;

	// restore state for the main window
	qglDrawBuffer( GL_BACK );
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );

	return (const void *)(cmd + 1);
}
