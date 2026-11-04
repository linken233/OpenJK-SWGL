/*
===========================================================================
Copyright (C) 2005 - 2015, ioquake3 contributors
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

#include <SDL.h>
#include <SDL_syswm.h>
#include "qcommon/qcommon.h"
#include "rd-common/tr_types.h"
#include <SDL_vulkan.h>		
#include "sys/sys_local.h"
#include "sdl_icon.h"

enum rserr_t
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
};

// g_FastRendererSwitch loads two renderers. Each renderer has its own window in a slot.
// win_slot is the slot of the renderer that runs now. The client sets it before each call.
static SDL_Window *win_screens[2];
static SDL_GLContext win_contexts[2];
static int win_slot;
static int win_shownSlot;
static qboolean win_dual;
#define screen			win_screens[win_slot]
#define opengl_context	win_contexts[win_slot]
static float displayAspect;

// SWGL PiP: secondary window state (no second GL context - pure SDL/software blit)
static SDL_Window *pipWindow = NULL;
static SDL_Renderer *pipRenderer = NULL;
static SDL_Texture *pipTexture = NULL;
static int pipTextureW = 0, pipTextureH = 0;

cvar_t *r_sdlDriver;
cvar_t *r_allowSoftwareGL;

// Window cvars
cvar_t	*r_fullscreen = 0;
cvar_t	*r_noborder;
cvar_t	*r_centerWindow;
cvar_t	*r_customwidth;
cvar_t	*r_customheight;
cvar_t	*r_swapInterval;
cvar_t	*r_stereo;
cvar_t	*r_mode;
cvar_t	*r_displayRefresh;

// Window surface cvars
cvar_t	*r_stencilbits;
cvar_t	*r_depthbits;
cvar_t	*r_colorbits;
cvar_t	*r_ignorehwgamma;
cvar_t  *r_ext_multisample;
cvar_t  *r_ext_multisample_default_fb;

/*
** R_GetModeInfo
*/
typedef struct vidmode_s
{
    const char *description;
    int         width, height;
} vidmode_t;

const vidmode_t r_vidModes[] = {
    { "Mode  0: 320x240",		320,	240 },
    { "Mode  1: 400x300",		400,	300 },
    { "Mode  2: 512x384",		512,	384 },
    { "Mode  3: 640x480",		640,	480 },
    { "Mode  4: 800x600",		800,	600 },
    { "Mode  5: 960x720",		960,	720 },
    { "Mode  6: 1024x768",		1024,	768 },
    { "Mode  7: 1152x864",		1152,	864 },
    { "Mode  8: 1280x1024",		1280,	1024 },
    { "Mode  9: 1600x1200",		1600,	1200 },
    { "Mode 10: 2048x1536",		2048,	1536 },
    { "Mode 11: 856x480 (wide)", 856,	 480 },
    { "Mode 12: 2400x600(surround)",2400,600 }
};
static const int	s_numVidModes = ARRAY_LEN( r_vidModes );

#define R_MODE_FALLBACK (4) // 640x480

qboolean R_GetModeInfo( int *width, int *height, int mode ) {
	const vidmode_t	*vm;

    if ( mode < -1 ) {
        return qfalse;
	}
	if ( mode >= s_numVidModes ) {
		return qfalse;
	}

	if ( mode == -1 ) {
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		return qtrue;
	}

	vm = &r_vidModes[mode];

    *width  = vm->width;
    *height = vm->height;

    return qtrue;
}

/*
** R_ModeList_f
*/
static void R_ModeList_f( void )
{
	int i;

	Com_Printf( "\n" );
	Com_Printf( "Mode -2: Use desktop resolution\n" );
	Com_Printf( "Mode -1: Use r_customWidth and r_customHeight variables\n" );
	for ( i = 0; i < s_numVidModes; i++ )
	{
		Com_Printf( "%s\n", r_vidModes[i].description );
	}
	Com_Printf( "\n" );
}

/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize(void)
{
	SDL_MinimizeWindow( screen );
}

void WIN_Present( window_t *window )
{
	// g_FastRendererSwitch: the frame of the hidden window is only read back, never presented.
	if ( win_dual && win_slot != win_shownSlot )
	{
		return;
	}

	if ( window->api == GRAPHICS_API_OPENGL )
	{
		SDL_GL_SwapWindow(screen);

		if ( r_swapInterval->modified )
		{
			r_swapInterval->modified = qfalse;
			if ( SDL_GL_SetSwapInterval( r_swapInterval->integer ) == -1 )
			{
				Com_DPrintf( "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError() );
			}
		}
	}

	// g_FastRendererSwitch: no real fullscreen (see GLimp_SetMode). A toggle here makes the window
	// exclusive fullscreen, and at the next switch it minimizes and its GL frames read back empty.
	if ( r_fullscreen->modified && win_dual )
	{
		r_fullscreen->modified = qfalse;
	}

	if ( r_fullscreen->modified )
	{
		bool	fullscreen;
		bool	needToToggle;
		bool	sdlToggled = qfalse;

		// Find out the current state
		fullscreen = (SDL_GetWindowFlags( screen ) & SDL_WINDOW_FULLSCREEN) != 0;

		if ( r_fullscreen->integer && Cvar_VariableIntegerValue( "in_nograb" ) )
		{
			Com_Printf( "Fullscreen not allowed with in_nograb 1\n" );
			Cvar_Set( "r_fullscreen", "0" );
			r_fullscreen->modified = qfalse;
		}

		// Is the state we want different from the current state?
		needToToggle = !!r_fullscreen->integer != fullscreen;

		if ( needToToggle )
		{
			sdlToggled = SDL_SetWindowFullscreen( screen, r_fullscreen->integer ) >= 0;

			// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
			if ( !sdlToggled )
				Cbuf_AddText( "vid_restart\n" );

			IN_Restart();
		}

		r_fullscreen->modified = qfalse;
	}
}

/*
===============
GLimp_CompareModes
===============
*/
static int GLimp_CompareModes( const void *a, const void *b )
{
	const float ASPECT_EPSILON = 0.001f;
	SDL_Rect *modeA = (SDL_Rect *)a;
	SDL_Rect *modeB = (SDL_Rect *)b;
	float aspectA = (float)modeA->w / (float)modeA->h;
	float aspectB = (float)modeB->w / (float)modeB->h;
	int areaA = modeA->w * modeA->h;
	int areaB = modeB->w * modeB->h;
	float aspectDiffA = fabs( aspectA - displayAspect );
	float aspectDiffB = fabs( aspectB - displayAspect );
	float aspectDiffsDiff = aspectDiffA - aspectDiffB;

	if( aspectDiffsDiff > ASPECT_EPSILON )
		return 1;
	else if( aspectDiffsDiff < -ASPECT_EPSILON )
		return -1;
	else
		return areaA - areaB;
}

/*
===============
GLimp_DetectAvailableModes
===============
*/
static bool GLimp_DetectAvailableModes(void)
{
	int i, j;
	char buf[ MAX_STRING_CHARS ] = { 0 };
	SDL_Rect *modes;
	int numModes = 0;

	int display = SDL_GetWindowDisplayIndex( screen );
	if ( display < 0 )
	{
		Com_Printf( S_COLOR_YELLOW "WARNING: Couldn't get window display index, no resolutions detected: %s\n", SDL_GetError() );
		return false;
	}

	SDL_DisplayMode windowMode;

	if( SDL_GetWindowDisplayMode( screen, &windowMode ) < 0 )
	{
		Com_Printf( S_COLOR_YELLOW "WARNING: Couldn't get window display mode, no resolutions detected (%s).\n", SDL_GetError() );
		return false;
	}

	int numDisplayModes = SDL_GetNumDisplayModes( display );
	if ( numDisplayModes < 0 )
		Com_Error( ERR_FATAL, "SDL_GetNumDisplayModes() FAILED (%s)", SDL_GetError() );

	modes = (SDL_Rect *)SDL_calloc( (size_t)numDisplayModes, sizeof( SDL_Rect ) );
	if ( !modes )
		Com_Error( ERR_FATAL, "Out of memory" );

	for( i = 0; i < numDisplayModes; i++ )
	{
		SDL_DisplayMode mode;

		if( SDL_GetDisplayMode( display, i, &mode ) < 0 )
			continue;

		if( !mode.w || !mode.h )
		{
			Com_Printf( "Display supports any resolution\n" );
			SDL_free( modes );
			return true;
		}

		if( windowMode.format != mode.format )
			continue;

		// SDL can give the same resolution with different refresh rates.
		// Only list resolution once.
		for( j = 0; j < numModes; j++ )
		{
			if( mode.w == modes[ j ].w && mode.h == modes[ j ].h )
				break;
		}

		if( j != numModes )
			continue;

		modes[ numModes ].w = mode.w;
		modes[ numModes ].h = mode.h;
		numModes++;
	}

	if( numModes > 1 )
		qsort( modes, numModes, sizeof( SDL_Rect ), GLimp_CompareModes );

	for( i = 0; i < numModes; i++ )
	{
		const char *newModeString = va( "%ux%u ", modes[ i ].w, modes[ i ].h );

		if( strlen( newModeString ) < (int)sizeof( buf ) - strlen( buf ) )
			Q_strcat( buf, sizeof( buf ), newModeString );
		else
			Com_Printf( "Skipping mode %ux%u, buffer too small\n", modes[ i ].w, modes[ i ].h );
	}

	if( *buf )
	{
		buf[ strlen( buf ) - 1 ] = 0;
		Com_Printf( "Available modes: '%s'\n", buf );
		Cvar_Set( "r_availableModes", buf );
	}

	SDL_free( modes );
	return true;
}

/*
===============
GLimp_SetMode
===============
*/
static rserr_t GLimp_SetMode(glconfig_t *glConfig, const windowDesc_t *windowDesc, const char *windowTitle, int mode, qboolean fullscreen, qboolean noborder)
{
	int perChannelColorBits;
	int colorBits, depthBits, stencilBits;
	int samples;
	int i = 0;
	SDL_Surface *icon = NULL;
	// The window of the renderer that is not on screen is created hidden.
	const qboolean hidden = (qboolean)( win_slot != win_shownSlot );
	Uint32 flags = hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;

	// g_FastRendererSwitch: no real fullscreen. A switch between two fullscreen windows
	// minimizes them and resizes the hidden one. Both windows are borderless and cover the display.
	const qboolean fakeFullscreen = (qboolean)( win_dual && fullscreen );
	if ( fakeFullscreen ) {
		noborder = qtrue;
		fullscreen = qfalse;
	}
	SDL_DisplayMode desktopMode;
	int display = 0;
	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;

	if ( windowDesc->api == GRAPHICS_API_OPENGL )
	{
		flags |= SDL_WINDOW_OPENGL;
	}
	else if ( windowDesc->api == GRAPHICS_API_VULKAN )
	{
		flags |= SDL_WINDOW_VULKAN;
	}

	Com_Printf( "Initializing display\n");

	icon = SDL_CreateRGBSurfaceFrom(
		(void *)CLIENT_WINDOW_ICON.pixel_data,
		CLIENT_WINDOW_ICON.width,
		CLIENT_WINDOW_ICON.height,
		CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
		CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width,
#ifdef Q3_LITTLE_ENDIAN
		0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
		0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
		);

	// If a window exists, note its display index
	SDL_Window *displayWindow = screen ? screen : win_screens[win_shownSlot];
	if ( displayWindow != NULL )
	{
		display = SDL_GetWindowDisplayIndex( displayWindow );
		if ( display < 0 )
		{
			Com_DPrintf( "SDL_GetWindowDisplayIndex() failed: %s\n", SDL_GetError() );
		}
	}

	if( display >= 0 && SDL_GetDesktopDisplayMode( display, &desktopMode ) == 0 )
	{
		displayAspect = (float)desktopMode.w / (float)desktopMode.h;

		Com_Printf( "Display aspect: %.3f\n", displayAspect );
	}
	else
	{
		Com_Memset( &desktopMode, 0, sizeof( SDL_DisplayMode ) );

		Com_Printf( "Cannot determine display aspect, assuming 1.333\n" );
	}

	Com_Printf( "...setting mode %d:", mode );

	if (mode == -2)
	{
		// use desktop video resolution
		if( desktopMode.h > 0 )
		{
			glConfig->vidWidth = desktopMode.w;
			glConfig->vidHeight = desktopMode.h;
		}
		else
		{
			glConfig->vidWidth = 640;
			glConfig->vidHeight = 480;
			Com_Printf( "Cannot determine display resolution, assuming 640x480\n" );
		}

		//glConfig.windowAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
	}
	else if ( !R_GetModeInfo( &glConfig->vidWidth, &glConfig->vidHeight, /*&glConfig.windowAspect,*/ mode ) )
	{
		Com_Printf( " invalid mode\n" );
		SDL_FreeSurface( icon );
		return RSERR_INVALID_MODE;
	}
	Com_Printf( " %d %d\n", glConfig->vidWidth, glConfig->vidHeight);

	// Center window
	if( r_centerWindow->integer && !fullscreen )
	{
		x = ( desktopMode.w / 2 ) - ( glConfig->vidWidth / 2 );
		y = ( desktopMode.h / 2 ) - ( glConfig->vidHeight / 2 );
	}

	// Destroy existing state if it exists
	if( opengl_context != NULL )
	{
		SDL_GL_DeleteContext( opengl_context );
		opengl_context = NULL;
	}

	if( screen != NULL )
	{
		SDL_GetWindowPosition( screen, &x, &y );
		Com_DPrintf( "Existing window at %dx%d before being destroyed\n", x, y );
		SDL_DestroyWindow( screen );
		screen = NULL;
	}

	// A borderless window has no title bar to move it, so center it on its display.
	// At the desktop resolution it then covers the display.
	if ( noborder && !fullscreen && !fakeFullscreen )
	{
		x = y = SDL_WINDOWPOS_CENTERED_DISPLAY( display >= 0 ? display : 0 );
	}

	if ( fakeFullscreen )
	{
		SDL_Rect bounds;
		if ( SDL_GetDisplayBounds( display >= 0 ? display : 0, &bounds ) == 0 )
		{
			x = bounds.x;
			y = bounds.y;
		}
	}


	if( fullscreen )
	{
#ifdef MACOS_X
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
        flags |= SDL_WINDOW_FULLSCREEN;
#endif
		glConfig->isFullscreen = qtrue;
	}
	else
	{
		if( noborder )
			flags |= SDL_WINDOW_BORDERLESS;

		glConfig->isFullscreen = fakeFullscreen;
	}

	colorBits = r_colorbits->integer;
	if ((!colorBits) || (colorBits >= 32))
		colorBits = 24;

	if (!r_depthbits->integer)
		depthBits = 24;
	else
		depthBits = r_depthbits->integer;

	stencilBits = r_stencilbits->integer;
	samples = r_ext_multisample->integer;
	if (!r_ext_multisample_default_fb->integer)
		samples = 0;

	if ( windowDesc->api == GRAPHICS_API_OPENGL )
	{
		for (i = 0; i < 16; i++)
		{
			int testColorBits, testDepthBits, testStencilBits;

			// 0 - default
			// 1 - minus colorBits
			// 2 - minus depthBits
			// 3 - minus stencil
			if ((i % 4) == 0 && i)
			{
				// one pass, reduce
				switch (i / 4)
				{
					case 2 :
						if (colorBits == 24)
							colorBits = 16;
						break;
					case 1 :
						if (depthBits == 24)
							depthBits = 16;
						else if (depthBits == 16)
							depthBits = 8;
					case 3 :
						if (stencilBits == 24)
							stencilBits = 16;
						else if (stencilBits == 16)
							stencilBits = 8;
				}
			}

			testColorBits = colorBits;
			testDepthBits = depthBits;
			testStencilBits = stencilBits;

			if ((i % 4) == 3)
			{ // reduce colorBits
				if (testColorBits == 24)
					testColorBits = 16;
			}

			if ((i % 4) == 2)
			{ // reduce depthBits
				if (testDepthBits == 24)
					testDepthBits = 16;
				else if (testDepthBits == 16)
					testDepthBits = 8;
			}

			if ((i % 4) == 1)
			{ // reduce stencilBits
				if (testStencilBits == 24)
					testStencilBits = 16;
				else if (testStencilBits == 16)
					testStencilBits = 8;
				else
					testStencilBits = 0;
			}

			if (testColorBits == 24)
				perChannelColorBits = 8;
			else
				perChannelColorBits = 4;

			SDL_GL_SetAttribute( SDL_GL_RED_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, testDepthBits );
			SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, testStencilBits );

			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0 );
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, samples );

			if ( windowDesc->gl.majorVersion )
			{
				int compactVersion = windowDesc->gl.majorVersion * 100 + windowDesc->gl.minorVersion * 10;

				SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, windowDesc->gl.majorVersion );
				SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, windowDesc->gl.minorVersion );

				if ( windowDesc->gl.profile == GLPROFILE_ES || compactVersion >= 320 )
				{
					int profile;
					switch ( windowDesc->gl.profile )
					{
					default:
					case GLPROFILE_COMPATIBILITY:
						profile = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
						break;

					case GLPROFILE_CORE:
						profile = SDL_GL_CONTEXT_PROFILE_CORE;
						break;

					case GLPROFILE_ES:
						profile = SDL_GL_CONTEXT_PROFILE_ES;
						break;
					}

					SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, profile );
				}
			}

			if ( windowDesc->gl.contextFlags & GLCONTEXT_DEBUG )
			{
				SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG );
			}

			if(r_stereo->integer)
			{
				glConfig->stereoEnabled = qtrue;
				SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
			}
			else
			{
				glConfig->stereoEnabled = qfalse;
				SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
			}

			SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
			SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, !r_allowSoftwareGL->integer );

			if( ( screen = SDL_CreateWindow( windowTitle, x, y,
					glConfig->vidWidth, glConfig->vidHeight, flags ) ) == NULL )
			{
				Com_DPrintf( "SDL_CreateWindow failed: %s\n", SDL_GetError( ) );
				continue;
			}

#ifndef MACOS_X
			SDL_SetWindowIcon( screen, icon );
#endif

			if( fullscreen )
			{
				SDL_DisplayMode mode;

				switch( testColorBits )
				{
					case 16: mode.format = SDL_PIXELFORMAT_RGB565; break;
					case 24: mode.format = SDL_PIXELFORMAT_RGB24;  break;
					default: Com_DPrintf( "testColorBits is %d, can't fullscreen\n", testColorBits ); continue;
				}

				mode.w = glConfig->vidWidth;
				mode.h = glConfig->vidHeight;
				mode.refresh_rate = glConfig->displayFrequency = r_displayRefresh->integer;
				mode.driverdata = NULL;

				if( SDL_SetWindowDisplayMode( screen, &mode ) < 0 )
				{
					Com_DPrintf( "SDL_SetWindowDisplayMode failed: %s\n", SDL_GetError( ) );
					continue;
				}
			}

			if( ( opengl_context = SDL_GL_CreateContext( screen ) ) == NULL )
			{
				Com_Printf( "SDL_GL_CreateContext failed: %s\n", SDL_GetError( ) );
				continue;
			}

			if ( SDL_GL_SetSwapInterval( r_swapInterval->integer ) == -1 )
			{
				Com_DPrintf( "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError() );
			}

			glConfig->colorBits = testColorBits;
			glConfig->depthBits = testDepthBits;
			glConfig->stencilBits = testStencilBits;

			Com_Printf( "Using %d color bits, %d depth, %d stencil display.\n",
					glConfig->colorBits, glConfig->depthBits, glConfig->stencilBits );
			break;
		}

		if (opengl_context == NULL) {
			SDL_FreeSurface(icon);
			return RSERR_UNKNOWN;
		}
	}
	else
	{
		// Just create a regular window
		if( ( screen = SDL_CreateWindow( windowTitle, x, y,
				glConfig->vidWidth, glConfig->vidHeight, flags ) ) == NULL )
		{
			Com_DPrintf( "SDL_CreateWindow failed: %s\n", SDL_GetError( ) );
		}
		else
		{
#ifndef MACOS_X
			SDL_SetWindowIcon( screen, icon );
#endif
			if( fullscreen )
			{
				if( SDL_SetWindowDisplayMode( screen, NULL ) < 0 )
				{
					Com_DPrintf( "SDL_SetWindowDisplayMode failed: %s\n", SDL_GetError( ) );
				}
			}
		}
	}

	SDL_FreeSurface( icon );

	if (!GLimp_DetectAvailableModes())
	{
		return RSERR_UNKNOWN;
	}

	return RSERR_OK;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static qboolean GLimp_StartDriverAndSetMode(glconfig_t *glConfig, const windowDesc_t *windowDesc, int mode, qboolean fullscreen, qboolean noborder)
{
	rserr_t err;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		const char *driverName;

		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			Com_Printf( "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError());
			return qfalse;
		}

		driverName = SDL_GetCurrentVideoDriver();

		if (!driverName)
		{
			Com_Error( ERR_FATAL, "No video driver initialized" );
			return qfalse;
		}

		Com_Printf( "SDL using driver \"%s\"\n", driverName );
		Cvar_Set( "r_sdlDriver", driverName );
	}

	if (SDL_GetNumVideoDisplays() <= 0)
	{
		Com_Error( ERR_FATAL, "SDL_GetNumVideoDisplays() FAILED (%s)", SDL_GetError() );
	}

	if (fullscreen && Cvar_VariableIntegerValue( "in_nograb" ) )
	{
		Com_Printf( "Fullscreen not allowed with in_nograb 1\n");
		Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}

	err = GLimp_SetMode(glConfig, windowDesc, CLIENT_WINDOW_TITLE, mode, fullscreen, noborder);

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			Com_Printf( "...WARNING: fullscreen unavailable in this mode\n" );
			return qfalse;
		case RSERR_INVALID_MODE:
			Com_Printf( "...WARNING: could not set the given mode (%d)\n", mode );
			return qfalse;
		case RSERR_UNKNOWN:
			Com_Printf( "...ERROR: no display modes could be found.\n" );
			return qfalse;
		default:
			break;
	}

	return qtrue;
}

window_t WIN_Init( const windowDesc_t *windowDesc, glconfig_t *glConfig )
{
	if ( !win_screens[win_slot ^ 1] )
	{
		Cmd_AddCommand("modelist", R_ModeList_f);
		Cmd_AddCommand("minimize", GLimp_Minimize);
	}

	r_sdlDriver			= Cvar_Get( "r_sdlDriver",			"",			CVAR_ROM );
	r_allowSoftwareGL	= Cvar_Get( "r_allowSoftwareGL",	"0",		CVAR_ARCHIVE_ND|CVAR_LATCH );

	// Window cvars
	r_fullscreen		= Cvar_Get( "r_fullscreen",			"0",		CVAR_ARCHIVE|CVAR_LATCH );
	r_noborder			= Cvar_Get( "r_noborder",			"0",		CVAR_ARCHIVE|CVAR_LATCH );
	r_centerWindow		= Cvar_Get( "r_centerWindow",		"0",		CVAR_ARCHIVE|CVAR_LATCH );
	r_customwidth		= Cvar_Get( "r_customwidth",		"1600",		CVAR_ARCHIVE|CVAR_LATCH );
	r_customheight		= Cvar_Get( "r_customheight",		"1024",		CVAR_ARCHIVE|CVAR_LATCH );
	r_swapInterval		= Cvar_Get( "r_swapInterval",		"0",		CVAR_ARCHIVE_ND );
	r_stereo			= Cvar_Get( "r_stereo",				"0",		CVAR_ARCHIVE_ND|CVAR_LATCH );
	r_mode				= Cvar_Get( "r_mode",				"-2",		CVAR_ARCHIVE|CVAR_LATCH );
	r_displayRefresh	= Cvar_Get( "r_displayRefresh",		"0",		CVAR_LATCH );
	Cvar_CheckRange( r_displayRefresh, 0, 240, qtrue );

	// Window render surface cvars
	r_stencilbits		= Cvar_Get( "r_stencilbits",		"8",		CVAR_ARCHIVE_ND|CVAR_LATCH );
	r_depthbits			= Cvar_Get( "r_depthbits",			"0",		CVAR_ARCHIVE_ND|CVAR_LATCH );
	r_colorbits			= Cvar_Get( "r_colorbits",			"0",		CVAR_ARCHIVE_ND|CVAR_LATCH );
	r_ignorehwgamma		= Cvar_Get( "r_ignorehwgamma",		"0",		CVAR_ARCHIVE_ND|CVAR_LATCH );
	r_ext_multisample	= Cvar_Get( "r_ext_multisample",	"0",		CVAR_ARCHIVE_ND|CVAR_LATCH );
	r_ext_multisample_default_fb = Cvar_Get("r_ext_multisample_default_fb", "1", CVAR_ROM);
	Cvar_Get( "r_availableModes", "", CVAR_ROM );

	// Create the window and set up the context
	if(!GLimp_StartDriverAndSetMode( glConfig, windowDesc, r_mode->integer,
										(qboolean)r_fullscreen->integer, (qboolean)r_noborder->integer ))
	{
		if( r_mode->integer != R_MODE_FALLBACK )
		{
			Com_Printf( "Setting r_mode %d failed, falling back on r_mode %d\n", r_mode->integer, R_MODE_FALLBACK );

			if (!GLimp_StartDriverAndSetMode( glConfig, windowDesc, R_MODE_FALLBACK, qfalse, qfalse ))
			{
				// Nothing worked, give up
				Com_Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem" );
			}
		}
	}

	glConfig->deviceSupportsGamma =
		(qboolean)(!r_ignorehwgamma->integer && SDL_SetWindowBrightness( screen, 1.0f ) >= 0);

	// This depends on SDL_INIT_VIDEO, hence having it here
	if ( win_slot == win_shownSlot )
	{
		IN_Init( screen );
	}

	// window_t is only really useful for Windows if the renderer wants to create a D3D context.
	window_t window = {};

	window.api = windowDesc->api;

#if defined(_WIN32)
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	if ( SDL_GetWindowWMInfo(screen, &info) )
	{
		switch(info.subsystem) {
			case SDL_SYSWM_WINDOWS:
				window.handle = info.info.win.window;
				break;

			default:
				break;
		}
	}
#endif

	return window;
}

/*
===============
GLimp_Shutdown
===============
*/
void WIN_Shutdown( void )
{
	const qboolean lastWindow = (qboolean)( win_screens[win_slot ^ 1] == NULL );

	if ( lastWindow )
	{
		Cmd_RemoveCommand("modelist");
		Cmd_RemoveCommand("minimize");
	}

	if ( win_slot == win_shownSlot )
	{
		IN_Shutdown();
	}

	if (opengl_context) {
		SDL_GL_DeleteContext(opengl_context);
		opengl_context = NULL;
	}

	// SWGL PiP: tear down the secondary window if it is still up
	if (pipWindow) {
		if (pipTexture) {
			SDL_DestroyTexture(pipTexture);
			pipTexture = NULL;
		}
		if (pipRenderer) {
			SDL_DestroyRenderer(pipRenderer);
			pipRenderer = NULL;
		}
		SDL_DestroyWindow(pipWindow);
		pipWindow = NULL;
	}

	if (screen) {
		SDL_DestroyWindow(screen);
		screen = NULL;
	}

	if ( lastWindow )
	{
		win_shownSlot = 0;
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
	}
	else if ( win_slot == win_shownSlot )
	{
		// The window of the other renderer is left. The next window of this slot is hidden.
		win_shownSlot ^= 1;
	}
}

qboolean WIN_GammaInSoftware( void )
{
	return win_dual;
}

void WIN_SetDual( qboolean dual )
{
	win_dual = dual;

	// A fullscreen window that loses the focus to the other window must not minimize.
	SDL_SetHint( SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, dual ? "0" : NULL );	// NULL: SDL default
}

void WIN_SelectSlot( int slot )
{
	win_slot = slot;

	if ( win_contexts[slot] && SDL_GL_GetCurrentContext() != win_contexts[slot] )
	{
		SDL_GL_MakeCurrent( win_screens[slot], win_contexts[slot] );
	}
}

// Puts the window of the given slot on screen in place of the other one, and moves input to it.
void WIN_ShowSlot( int slot )
{
	SDL_Window *oldWindow = win_screens[win_shownSlot];
	SDL_Window *newWindow = win_screens[slot];

	if ( slot == win_shownSlot || !newWindow )
	{
		return;
	}

	if ( !oldWindow )
	{
		SDL_ShowWindow( newWindow );
		win_shownSlot = slot;
		IN_Init( newWindow );
		return;
	}

	int x, y;
	const Uint32 fullscreenFlags = SDL_GetWindowFlags( oldWindow ) & ( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP );

	// Show the new window before the old one goes, so the game keeps the foreground.
	SDL_GetWindowPosition( oldWindow, &x, &y );
	SDL_SetWindowPosition( newWindow, x, y );
	SDL_ShowWindow( newWindow );
	if ( SDL_GetWindowFlags( newWindow ) & SDL_WINDOW_MINIMIZED )
	{
		SDL_RestoreWindow( newWindow );
	}
	if ( fullscreenFlags )
	{
		SDL_SetWindowFullscreen( newWindow, fullscreenFlags );
	}
	SDL_RaiseWindow( newWindow );

	if ( fullscreenFlags )
	{
		SDL_SetWindowFullscreen( oldWindow, 0 );
	}
	SDL_HideWindow( oldWindow );

	win_shownSlot = slot;
	IN_SetWindow( newWindow );
}

void GLimp_EnableLogging( qboolean enable )
{
}

void GLimp_LogComment( char *comment )
{
}

void WIN_SetGamma( glconfig_t *glConfig, byte red[256], byte green[256], byte blue[256] )
{
	Uint16 table[3][256];
	int i, j;

	if( !glConfig->deviceSupportsGamma || r_ignorehwgamma->integer > 0 )
		return;

	// g_FastRendererSwitch: the ramp is display-wide and a screenshot never shows it. The
	// renderer applies it to its own frame instead (WIN_GammaInSoftware).
	if ( win_dual )
		return;

	for (i = 0; i < 256; i++)
	{
		table[0][i] = ( ( ( Uint16 ) red[i] ) << 8 ) | red[i];
		table[1][i] = ( ( ( Uint16 ) green[i] ) << 8 ) | green[i];
		table[2][i] = ( ( ( Uint16 ) blue[i] ) << 8 ) | blue[i];
	}

#if defined(_WIN32)
	// Win2K and newer put this odd restriction on gamma ramps...
	{
		OSVERSIONINFO	vinfo;

		vinfo.dwOSVersionInfoSize = sizeof( vinfo );
		GetVersionEx( &vinfo );
		if( vinfo.dwMajorVersion >= 5 && vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
		{
			Com_DPrintf( "performing gamma clamp.\n" );
			for( j = 0 ; j < 3 ; j++ )
			{
				for( i = 0 ; i < 128 ; i++ )
				{
					table[j][i] = Q_min(table[j][i], (128 + i) << 8);
				}

				table[j][127] = Q_min(table[j][127], 254 << 8);
			}
		}
	}
#endif

	// enforce constantly increasing
	for (j = 0; j < 3; j++)
	{
		for (i = 1; i < 256; i++)
		{
			if (table[j][i] < table[j][i-1])
				table[j][i] = table[j][i-1];
		}
	}

	if ( SDL_SetWindowGammaRamp( screen, table[0], table[1], table[2] ) < 0 )
	{
		Com_DPrintf( "SDL_SetWindowGammaRamp() failed: %s\n", SDL_GetError() );
	}
}

void *WIN_GL_GetProcAddress( const char *proc )
{
	return SDL_GL_GetProcAddress( proc );
}

qboolean WIN_GL_ExtensionSupported( const char *extension )
{
	return SDL_GL_ExtensionSupported( extension ) == SDL_TRUE ? qtrue : qfalse;
}

void *WIN_VK_GetInstanceProcAddress( void )
{
	if ( SDL_Vulkan_LoadLibrary( NULL ) != 0 )
	{
		Com_Printf( "SDL_Vulkan_LoadLibrary() failed: %s\n", SDL_GetError() );
		return NULL;
	}

	return SDL_Vulkan_GetVkGetInstanceProcAddr();
}

qboolean WIN_VK_createSurfaceImpl( VkInstance instance, VkSurfaceKHR *surface )
{
	if ( !screen )
	{
		return qfalse;
	}

	if ( SDL_Vulkan_CreateSurface( screen, (SDL_vulkanInstance)instance, (SDL_vulkanSurface *)surface ) == SDL_FALSE )
	{
		Com_Printf( "SDL_Vulkan_CreateSurface() failed: %s\n", SDL_GetError() );
		return qfalse;
	}

	return qtrue;
}

void WIN_VK_destroyWindow( void )
{
	WIN_Shutdown();
	SDL_Vulkan_UnloadLibrary();
}

qboolean WIN_VK_IsMinimized( void )
{
	Uint32 flags;

	if ( !screen )
	{
		return qfalse;
	}

	// The hidden window of the second renderer must still render, for g_ShowSplit.
	if ( win_dual )
	{
		SDL_Window *shown = win_screens[win_shownSlot];
		return ( shown && ( SDL_GetWindowFlags( shown ) & SDL_WINDOW_MINIMIZED ) ) ? qtrue : qfalse;
	}

	flags = SDL_GetWindowFlags( screen );

	return ( ( flags & SDL_WINDOW_MINIMIZED ) || ( flags & SDL_WINDOW_HIDDEN ) ) ? qtrue : qfalse;
}

/*
===============
WIN_Secondary_Create

SWGL PiP: create (or resize) the secondary PiP window. Returns an opaque handle
(window id) suitable for WIN_Secondary_Present, or NULL on failure. The window is
pure SDL (no GL context) and blitted with a software renderer + streaming texture.
===============
*/
void *WIN_Secondary_Create( const windowDesc_t *desc, int x, int y, int w, int h, qboolean topmost )
{
	Uint32 flags;
	SDL_Window *prevFocus = NULL;

	if (w < 2 || h < 2) {
		return NULL;
	}

	if (!pipWindow)
	{
		int resolvedX = 0, resolvedY = 0;
		int mainX = 0, mainY = 0, mainW = 0, mainH = 0;

		// SWGL PiP: remember who had the keyboard focus so the PiP window does
		// not steal it from the game (a lost focus would kill console input)
		prevFocus = SDL_GetKeyboardFocus();

		// SWGL PiP: sentinels on an axis:
		//   -1 / -3 = corner of the OTHER display (x: right edge, y: bottom
		//             edge), i.e. the monitor the game window is not on
		//   -2 = relative to the main game window (x: fully to its left with a
		//        10px gap, y: aligned with its top)
		// clamped inside the union of the display bounds so the window always
		// lands on screen
		if (x == -2 || y == -2 || x <= -1 || y <= -1)
		{
			if (screen)
			{
				int		i, count = SDL_GetNumVideoDisplays();
				int		curDisp = -1, otherDisp = -1;
				SDL_Rect bounds;

				SDL_GetWindowPosition(screen, &mainX, &mainY);
				SDL_GetWindowSize(screen, &mainW, &mainH);

				// find which display the game window is on (by its centre point)
				// so we know which one is "the other"
				for (i = 0; i < count; i++)
				{
					if (SDL_GetDisplayBounds(i, &bounds) == 0)
					{
						if (mainX + mainW / 2 >= bounds.x && mainX + mainW / 2 < bounds.x + bounds.w &&
							mainY + mainH / 2 >= bounds.y && mainY + mainH / 2 < bounds.y + bounds.h)
						{
							curDisp = i;
							break;
						}
					}
				}
				for (i = 0; i < count; i++)
				{
					if (i != curDisp)
					{
						otherDisp = i;
						break;
					}
				}

				if (x == -2)
				{
					x = mainX - w - 10;	// 10 pixels left of the main window edge
					resolvedX = 1;
				}
				if (y == -2)
				{
					y = mainY;
					resolvedY = 1;
				}
				if (x <= -1 || y <= -1)
				{
					// corner of the other monitor, falling back to the game's own
					// monitor when there is no second screen
					if (SDL_GetDisplayBounds((otherDisp != -1 ? otherDisp : curDisp), &bounds) == 0)
					{
						if (x <= -1)
						{
							x = bounds.x + bounds.w - w;
							resolvedX = 1;
						}
						if (y <= -1)
						{
							y = bounds.y + bounds.h - h;
							resolvedY = 1;
						}
					}
					if (x <= -1 && !resolvedX) {
						x = SDL_WINDOWPOS_UNDEFINED;
					}
					if (y <= -1 && !resolvedY) {
						y = SDL_WINDOWPOS_UNDEFINED;
					}
					resolvedX = resolvedY = 1;
				}

				{
					int i, count = SDL_GetNumVideoDisplays();
					int ux = 0x7FFFFFFF, uy = 0x7FFFFFFF;
					int ux2 = -0x7FFFFFFF, uy2 = -0x7FFFFFFF;

					for (i = 0; i < count; i++)
					{
						SDL_Rect bounds;

						if (SDL_GetDisplayBounds(i, &bounds) == 0)
						{
							if (bounds.x < ux) {
								ux = bounds.x;
							}
							if (bounds.y < uy) {
								uy = bounds.y;
							}
							if (bounds.x + bounds.w > ux2) {
								ux2 = bounds.x + bounds.w;
							}
							if (bounds.y + bounds.h > uy2) {
								uy2 = bounds.y + bounds.h;
							}
						}
					}

					if (ux2 != -0x7FFFFFFF)
					{
						if (x + w > ux2) {
							x = ux2 - w;
						}
						if (x < ux) {
							x = ux;
						}
						if (y + h > uy2) {
							y = uy2 - h;
						}
						if (y < uy) {
							y = uy;
						}
					}
				}
			}
			else
			{
				x = SDL_WINDOWPOS_UNDEFINED;
				y = SDL_WINDOWPOS_UNDEFINED;
				resolvedX = resolvedY = 1;
			}
		}

		if (x < 0 || y < 0)
		{
			SDL_Rect bounds;

			// auto place: bottom-right corner of the primary display
			if (SDL_GetDisplayBounds(0, &bounds) == 0)
			{
				if (x < 0 && !resolvedX) {
					x = bounds.x + bounds.w - w - 16;
				}
				if (y < 0 && !resolvedY) {
					y = bounds.y + bounds.h - h - 48;	// leave room for the taskbar
				}
			}
			else
			{
				if (x < 0 && !resolvedX) {
					x = SDL_WINDOWPOS_UNDEFINED;
				}
				if (y < 0 && !resolvedY) {
					y = SDL_WINDOWPOS_UNDEFINED;
				}
			}
		}

		flags = SDL_WINDOW_SHOWN;
		if (topmost) {
			flags |= SDL_WINDOW_ALWAYS_ON_TOP;
		}
		if (Cvar_VariableIntegerValue( "r_pip_borderless" )) {
			// SWGL PiP: borderless also means fixed - no title bar to drag
			// (registered by the renderers; when absent, keep the title bar)
			flags |= SDL_WINDOW_BORDERLESS;
		}

		pipWindow = SDL_CreateWindow("Jedi Academy - PiP", x, y, w, h, flags);
		if (!pipWindow)
		{
			Com_Printf("PiP window: SDL_CreateWindow failed (%s)\n", SDL_GetError());
			return NULL;
		}

		// SWGL PiP latency: r_pip_sdlhw opts into the hardware renderer (D3D/OpenGL,
		// flip-model present, no CPU blit path); software stays the default because it
		// keeps the pip window out of Reshade's reach - a hardware renderer is another
		// device Reshade can hook and post-process (which adds its own latency).  Read
		// through the cvar system like r_pip_borderless; applied at window creation,
		// so a game restart is what takes it.
		pipRenderer = SDL_CreateRenderer(pipWindow, -1, Cvar_VariableIntegerValue("r_pip_sdlhw") ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);
		if (!pipRenderer)
		{
			Com_Printf("PiP window: SDL_CreateRenderer failed (%s)\n", SDL_GetError());
			SDL_DestroyWindow(pipWindow);
			pipWindow = NULL;
			return NULL;
		}

		Com_Printf("PiP window: created (%dx%d)\n", w, h);

		// SWGL PiP: give the focus back to the game window.  Keyboard focus is
		// what drives both console typing and mouse look, so restore that
		// explicitly (this is the bit mouse look was missing) and hand the mouse
		// grab/relative mode back to the game if it already had them
		if (prevFocus && prevFocus != pipWindow)
		{
			SDL_bool grabbed = SDL_GetWindowGrab(prevFocus);
			SDL_bool relative = SDL_GetRelativeMouseMode();

			SDL_RaiseWindow(prevFocus);
			SDL_SetWindowInputFocus(prevFocus);
			if (grabbed == SDL_TRUE) {
				SDL_SetWindowGrab(prevFocus, SDL_TRUE);
			}
			if (relative == SDL_TRUE) {
				SDL_SetRelativeMouseMode(SDL_TRUE);
			}
		}
	}
	else
	{
		// already exists - just keep it sized to the requested resolution
		SDL_SetWindowSize(pipWindow, w, h);
	}

	if (pipTextureW != w || pipTextureH != h)
	{
		if (pipTexture) {
			SDL_DestroyTexture(pipTexture);
			pipTexture = NULL;
		}
		pipTexture = SDL_CreateTexture(pipRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
		if (!pipTexture)
		{
			Com_Printf("PiP window: SDL_CreateTexture failed (%s)\n", SDL_GetError());
			return NULL;
		}
		pipTextureW = w;
		pipTextureH = h;
	}

	return (void *)(intptr_t)SDL_GetWindowID(pipWindow);
}

/*
===============
WIN_Secondary_Present

SWGL PiP: blit a bottom-up RGBA8888 buffer (GL readback) onto the PiP window.
===============
*/
void WIN_Secondary_Present( void *handle, const byte *rgba, int w, int h, qboolean bottomUp )
{
	SDL_Window *window;
	void *pixels;
	int pitch, rowBytes;
	const byte *src;
	byte *dst;
	int y;

	if (!handle || !rgba || w < 2 || h < 2) {
		return;
	}

	window = SDL_GetWindowFromID((Uint32)(intptr_t)handle);
	if (!window) {
		return;	// window is gone (closed) - nothing to do, next Create call re-creates it
	}

	if (!pipRenderer || !pipTexture || w != pipTextureW || h != pipTextureH) {
		return;
	}

	rowBytes = w * 4;
	if (SDL_LockTexture(pipTexture, NULL, &pixels, &pitch) != 0) {
		return;
	}

	if (bottomUp) {
		// GL readback rows come bottom-up - flip them into the texture
		src = rgba + (h - 1) * rowBytes;
		dst = (byte *)pixels;
		for (y = 0; y < h; y++) {
			Com_Memcpy(dst, src, rowBytes);
			src -= rowBytes;
			dst += pitch;
		}
	}
	else {
		Com_Memcpy(pixels, rgba, (size_t)h * (size_t)rowBytes);
	}

	SDL_UnlockTexture(pipTexture);

	SDL_RenderClear(pipRenderer);
	SDL_RenderCopy(pipRenderer, pipTexture, NULL, NULL);
	SDL_RenderPresent(pipRenderer);

	// SWGL PiP: pin the OS cursor at the centre of the game window while the
	// game is in look mode (cursor hidden = in game, not menus/UI).  Otherwise
	// the invisible cursor slowly drifts to the desktop edge, the game thinks
	// the mouse left the window and drops mouse look (which freezes the PiP)
	if (screen && SDL_ShowCursor(SDL_QUERY) == SDL_FALSE)
	{
		int sw, sh;

		SDL_GetWindowSize(screen, &sw, &sh);
		if (sw > 1 && sh > 1) {
			SDL_WarpMouseInWindow(screen, sw / 2, sh / 2);
		}
	}
}

/*
===============
WIN_Secondary_Destroy

SWGL PiP: destroy the secondary window (idempotent).
===============
*/
void WIN_Secondary_Destroy( void *handle )
{
	if (!handle) {
		return;
	}

	if (handle == (void *)(intptr_t)(pipWindow ? SDL_GetWindowID(pipWindow) : 0))
	{
		if (pipTexture) {
			SDL_DestroyTexture(pipTexture);
			pipTexture = NULL;
		}
		if (pipRenderer) {
			SDL_DestroyRenderer(pipRenderer);
			pipRenderer = NULL;
		}
		if (pipWindow) {
			SDL_DestroyWindow(pipWindow);
			pipWindow = NULL;
		}
		pipTextureW = pipTextureH = 0;
	}
}
