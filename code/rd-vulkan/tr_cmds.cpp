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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

/*
=====================
R_PerformanceCounters
=====================
*/
void R_PerformanceCounters( void ) {
	if ( !r_speeds->integer ) {
		// clear the counters even if we aren't printing
		memset( &tr.pc, 0, sizeof( tr.pc ) );
		memset( &backEnd.pc, 0, sizeof( backEnd.pc ) );
		return;
	}

	if (r_speeds->integer == 1) {
		const float texSize = R_SumOfUsedImages( qfalse )/(8*1048576.0f)*(r_texturebits->integer?r_texturebits->integer:glConfig.colorBits);
		ri.Printf( PRINT_ALL,  "%i/%i shdrs/srfs %i leafs %i vrts %i/%i tris %.2fMB tex\n",
			backEnd.pc.c_shaders, backEnd.pc.c_surfaces, tr.pc.c_leafs, backEnd.pc.c_vertexes,
			backEnd.pc.c_indexes/3, backEnd.pc.c_totalIndexes/3,
			texSize );
	} else if (r_speeds->integer == 2) {
		ri.Printf( PRINT_ALL,  "(patch) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
			tr.pc.c_sphere_cull_patch_in, tr.pc.c_sphere_cull_patch_clip, tr.pc.c_sphere_cull_patch_out,
			tr.pc.c_box_cull_patch_in, tr.pc.c_box_cull_patch_clip, tr.pc.c_box_cull_patch_out );
		ri.Printf( PRINT_ALL,  "(md3) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
			tr.pc.c_sphere_cull_md3_in, tr.pc.c_sphere_cull_md3_clip, tr.pc.c_sphere_cull_md3_out,
			tr.pc.c_box_cull_md3_in, tr.pc.c_box_cull_md3_clip, tr.pc.c_box_cull_md3_out );
	} else if (r_speeds->integer == 3) {
		ri.Printf( PRINT_ALL,  "viewcluster: %i\n", tr.viewCluster );
	} else if (r_speeds->integer == 4) {
		if ( backEnd.pc.c_dlightVertexes ) {
			ri.Printf( PRINT_ALL,  "dlight srf:%i  culled:%i  verts:%i  tris:%i\n",
				tr.pc.c_dlightSurfaces, tr.pc.c_dlightSurfacesCulled,
				backEnd.pc.c_dlightVertexes, backEnd.pc.c_dlightIndexes / 3 );
		}
	}
	else if (r_speeds->integer == 5 )
	{
		vk_debug("zFar: %.0f\n", tr.viewParms.zFar );
	}
	else if (r_speeds->integer == 6 )
	{
		vk_debug("flare adds:%i tests:%i renders:%i\n",
			backEnd.pc.c_flareAdds, backEnd.pc.c_flareTests, backEnd.pc.c_flareRenders );
	}
	else if (r_speeds->integer == 7) {
		const float texSize = R_SumOfUsedImages(qtrue) / (1048576.0f);
		const float backBuff= glConfig.vidWidth * glConfig.vidHeight * glConfig.colorBits / (8.0f * 1024*1024);
		const float depthBuff= glConfig.vidWidth * glConfig.vidHeight * glConfig.depthBits / (8.0f * 1024*1024);
		const float stencilBuff= glConfig.vidWidth * glConfig.vidHeight * glConfig.stencilBits / (8.0f * 1024*1024);
		ri.Printf( PRINT_ALL,  "Tex MB %.2f + buffers %.2f MB = Total %.2fMB\n",
			texSize, backBuff*2+depthBuff+stencilBuff, texSize+backBuff*2+depthBuff+stencilBuff);
	}

	memset( &tr.pc, 0, sizeof( tr.pc ) );
	memset( &backEnd.pc, 0, sizeof( backEnd.pc ) );
}

/*
==============
Render thread

With r_renderThread on, R_IssueRenderCommands hands the finished frame to a thread and returns.
The frontend of the next frame then runs while the thread runs the backend. The frontend
writes backEndData, the backend reads backEndDataBE, and R_IssueRenderCommands swaps them.

The main thread calls R_SyncRenderThread before it changes anything that the backend reads
(shaders, images, models, the world, uploads to the GPU, console commands). It does nothing when
the thread is idle, and nothing on the render thread itself.

A frame that needs the engine on the main thread runs inline: screenshots, video maps,
the first frames of a level, the load of a weather cache or a sky preset.
==============
*/
#define RT_ERROR_TEXT	1024

struct rtAbort_s {};		// thrown on the render thread by R_RenderThreadAbort

struct renderThread_s {
	std::thread					thread;
	std::mutex					mutex;
	std::condition_variable		wake;			// main thread -> thread: a frame is ready, or quit
	std::condition_variable		done;			// thread -> main thread: the frame is finished
	bool						running;		// the thread exists
	bool						quit;
	bool						pending;		// a frame is handed off and not finished
	const void					*cmds;
	int							failLevel;
	char						failText[RT_ERROR_TEXT];
	int							syncFrames;		// next frames that run on the main thread
	qboolean					countersPending;	// the counters of the last handed off frame are not printed
};

// Not destroyed: after a fatal error the process exits with the thread alive, and the
// destructor of a running std::thread ends the process.
static renderThread_s &rt = *new renderThread_s();

static std::atomic<bool>	rtBusy;				// pending, readable without the lock
static std::atomic<bool>	rtFailed;			// the thread raised an error that the main thread did not raise yet
static std::atomic<int>		rtBackendMsec;		// msec of the last frame of the thread
static thread_local bool	rtIsThread = false;

qboolean R_OnRenderThread( void )
{
	return rtIsThread ? qtrue : qfalse;
}

// Raises the error of the render thread on the calling (main) thread.
static void R_RenderThreadRaise( void )
{
	int		level;
	char	text[RT_ERROR_TEXT];

	if ( !rtFailed.load() )
		return;

	{
		std::lock_guard<std::mutex> lock( rt.mutex );
		level = rt.failLevel;
		Q_strncpyz( text, rt.failText, sizeof( text ) );
		rtFailed.store( false );
	}

	RB_PrintDrain();
	RB_RealError( level, text );
}

void R_RenderThreadAbort( int level, const char *text )
{
	{
		std::lock_guard<std::mutex> lock( rt.mutex );

		if ( !rtFailed.load() )
		{
			rt.failLevel = level;
			Q_strncpyz( rt.failText, text, sizeof( rt.failText ) );
			rtFailed.store( true );
		}
	}

	throw rtAbort_s();
}

static void R_RenderThreadMain( void )
{
	rtIsThread = true;

	for ( ;; )
	{
		const void *cmds;

		{
			std::unique_lock<std::mutex> lock( rt.mutex );

			rt.wake.wait( lock, [] { return rt.pending || rt.quit; } );

			if ( !rt.pending )
				return;

			cmds = rt.cmds;
		}

		try
		{
			RB_PrintBackendBegin();
			RB_ExecuteRenderCommands( cmds );
		}
		catch ( const rtAbort_s & )
		{
		}
		RB_PrintBackendEnd();

		rtBackendMsec.store( backEnd.pc.msec );

		{
			std::lock_guard<std::mutex> lock( rt.mutex );

			rt.pending = false;
			rtBusy.store( false, std::memory_order_release );
		}
		rt.done.notify_all();
	}
}

/*
==============
Cvar changes of the backend

The cvar system is not safe to use from the render thread. The backend gives its changes to
R_BackendCvarSet, which keeps them until the main thread applies them (R_BackendCvarDrain).
==============
*/
#define RT_CVAR_QUEUE	16

static struct {
	char	name[64];
	char	value[64];
} rtCvarQueue[RT_CVAR_QUEUE];
static int			rtCvarCount;
static std::mutex	rtCvarMutex;

void R_BackendCvarSet( const char *name, const char *value )
{
	if ( !rtIsThread )
	{
		ri.Cvar_Set( name, value );
		return;
	}

	std::lock_guard<std::mutex> lock( rtCvarMutex );

	if ( rtCvarCount < RT_CVAR_QUEUE )
	{
		Q_strncpyz( rtCvarQueue[rtCvarCount].name, name, sizeof( rtCvarQueue[0].name ) );
		Q_strncpyz( rtCvarQueue[rtCvarCount].value, value, sizeof( rtCvarQueue[0].value ) );
		rtCvarCount++;
	}
}

void R_BackendCvarSetValue( const char *name, float value )
{
	char	text[64];

	if ( value == (int)value )
		Com_sprintf( text, sizeof( text ), "%i", (int)value );
	else
		Com_sprintf( text, sizeof( text ), "%f", value );

	R_BackendCvarSet( name, text );
}

// Main thread.
void R_BackendCvarDrain( void )
{
	for ( ;; )
	{
		char	name[64], value[64];

		{
			std::lock_guard<std::mutex> lock( rtCvarMutex );

			if ( !rtCvarCount )
				return;

			Q_strncpyz( name, rtCvarQueue[0].name, sizeof( name ) );
			Q_strncpyz( value, rtCvarQueue[0].value, sizeof( value ) );
			rtCvarCount--;
			memmove( &rtCvarQueue[0], &rtCvarQueue[1], sizeof( rtCvarQueue[0] ) * rtCvarCount );
		}

		ri.Cvar_Set( name, value );
	}
}

/*
==============
Wait log (r_renderThreadSyncLog)

Counts, for each place that waits for the render thread, the calls, the calls that waited
and the time waited. Every com_perflogFrames frames R_RenderThreadSyncLogFrame prints the
sites with the most waited time, then clears the counters.
==============
*/
#define RT_LOG_SITES	128
#define RT_LOG_TOP		12

enum {
	RTR_SYNC_FRAMES,		// R_RenderThreadForceSync / SetSync (load, init, vid_restart)
	RTR_SCREENSHOT,
	RTR_CAPTURE,
	RTR_VIDEOMAP,
	RTR_WEATHER,
	RTR_SKY_LOAD,
	RTR_COUNT
};

static const char *rtReasonNames[RTR_COUNT] = {
	"syncFrames", "screenshot", "capture", "videomap", "weather", "skyLoad"
};

struct rtLogSite_s {
	const char	*file;
	int			line;
	const char	*tag;
	int			calls;
	int			waited;
	double		ms;
};

static rtLogSite_s	rtLogSites[RT_LOG_SITES];
static int			rtLogNumSites;
static int			rtLogReasons[RTR_COUNT];
static int			rtLogFrames;
static int			rtLogInlineFrames;
static std::mutex	rtLogMutex;

static qboolean R_RenderThreadLogOn( void )
{
	return ( r_renderThreadSyncLog && r_renderThreadSyncLog->integer ) ? qtrue : qfalse;
}

static void R_RenderThreadLogAdd( const char *file, int line, const char *tag, qboolean waited, double ms )
{
	std::lock_guard<std::mutex> lock( rtLogMutex );
	rtLogSite_s	*site = NULL;

	for ( int i = 0; i < rtLogNumSites; i++ )
	{
		rtLogSite_s *s = &rtLogSites[i];

		if ( s->line == line && ( s->file == file || ( s->file && file && !strcmp( s->file, file ) ) )
			&& ( s->tag == tag || ( s->tag && tag && !strcmp( s->tag, tag ) ) ) )
		{
			site = s;
			break;
		}
	}

	if ( !site )
	{
		if ( rtLogNumSites >= RT_LOG_SITES )
			return;

		site = &rtLogSites[rtLogNumSites++];
		site->file = file;
		site->line = line;
		site->tag = tag;
		site->calls = 0;
		site->waited = 0;
		site->ms = 0.0;
	}

	site->calls++;
	if ( waited )
	{
		site->waited++;
		site->ms += ms;
	}
}

static const char *R_RenderThreadLogBaseName( const char *path )
{
	const char *name = path;

	for ( const char *p = path; *p; p++ )
	{
		if ( *p == '/' || *p == '\\' )
			name = p + 1;
	}
	return name;
}

// Prints and clears the counters. Main thread, once per frame.
static void R_RenderThreadSyncLogFrame( void )
{
	int		window = ri.Cvar_VariableIntegerValue( "com_perflogFrames" );
	int		order[RT_LOG_SITES];
	int		count, shown;

	if ( window < 1 )
		window = 30;

	if ( ++rtLogFrames < window )
		return;

	std::lock_guard<std::mutex> lock( rtLogMutex );

	count = rtLogNumSites;
	for ( int i = 0; i < count; i++ )
		order[i] = i;

	// Insertion sort, the most waited time first.
	for ( int i = 1; i < count; i++ )
	{
		const int key = order[i];
		int j = i - 1;

		while ( j >= 0 && ( rtLogSites[order[j]].ms < rtLogSites[key].ms
			|| ( rtLogSites[order[j]].ms == rtLogSites[key].ms && rtLogSites[order[j]].calls < rtLogSites[key].calls ) ) )
		{
			order[j + 1] = order[j];
			j--;
		}
		order[j + 1] = key;
	}

	ri.Printf( PRINT_ALL, "rtsync: %d frames, %d ran inline\n", rtLogFrames, rtLogInlineFrames );

	shown = count < RT_LOG_TOP ? count : RT_LOG_TOP;
	for ( int i = 0; i < shown; i++ )
	{
		const rtLogSite_s *s = &rtLogSites[order[i]];

		if ( s->file )
			ri.Printf( PRINT_ALL, "rtsync:   %s:%d %s  calls %d  waited %d  %.2f ms\n",
				R_RenderThreadLogBaseName( s->file ), s->line, s->tag, s->calls, s->waited, s->ms );
		else
			ri.Printf( PRINT_ALL, "rtsync:   %s  calls %d  waited %d  %.2f ms\n",
				s->tag, s->calls, s->waited, s->ms );
	}

	ri.Printf( PRINT_ALL, "rtsync: inline reasons" );
	for ( int i = 0; i < RTR_COUNT; i++ )
		ri.Printf( PRINT_ALL, " %s %d", rtReasonNames[i], rtLogReasons[i] );
	ri.Printf( PRINT_ALL, "\n" );

	rtLogNumSites = 0;
	rtLogFrames = 0;
	rtLogInlineFrames = 0;
	memset( rtLogReasons, 0, sizeof( rtLogReasons ) );
}

// Waits until the render thread has finished its frame, then raises its error if it had one.
// Main thread. The file, line and tag name the caller for the wait log.
static void R_RenderThreadWait( int perfSlot, qboolean raise, const char *file, int line, const char *tag )
{
	if ( rtIsThread )
		return;

	const qboolean log = R_RenderThreadLogOn();
	const bool busy = rtBusy.load( std::memory_order_acquire );

	if ( busy )
	{
		std::chrono::steady_clock::time_point begin;

		if ( log )
			begin = std::chrono::steady_clock::now();

		{
			vk_perf_scope perf( perfSlot );
			std::unique_lock<std::mutex> lock( rt.mutex );

			rt.done.wait( lock, [] { return !rt.pending; } );
		}

		if ( log )
			R_RenderThreadLogAdd( file, line, tag, qtrue,
				std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - begin ).count() );

		RB_PrintDrain();
		R_BackendCvarDrain();
	}
	else if ( log )
	{
		R_RenderThreadLogAdd( file, line, tag, qfalse, 0.0 );
	}

	if ( raise )
		R_RenderThreadRaise();
}

static void R_FlushDeferredOld( void );

void R_SyncRenderThreadAt( const char *file, int line, const char *func )
{
	R_RenderThreadWait( VKP_RT_SYNC, qtrue, file, line, func );
	R_WorldEffectsFlushQueue();

	// The thread is idle: the frame that used the old pointers has finished.
	if ( !rtIsThread )
		R_FlushDeferredOld();
}

// Waits for the render thread but does not raise its error: safe in a destructor. Main thread.
void R_WaitRenderThreadNoRaise( void )
{
	R_RenderThreadWait( VKP_RT_SYNC, qfalse, NULL, 0, "R_WaitRenderThreadNoRaise" );
}

/*
==============
Deferred free

The backend reads gore data that the game can destroy while the thread runs. With the thread on,
R_DeferredFree keeps the pointer until the frames that can reference it have finished.
A pointer is released at the second R_IssueRenderCommands after the call: the first one
finishes the frame in flight and issues the frame that may use the pointer.
Main thread only.
==============
*/
static std::vector<void *> rtDeferredNew;		// queued since the last R_IssueRenderCommands
static std::vector<void *> rtDeferredOld;		// queued before the last R_IssueRenderCommands

void R_DeferredFree( void *ptr )
{
	if ( !ptr )
		return;

	if ( !rt.running )
	{
		ri.Z_Free( ptr );
		return;
	}

	rtDeferredNew.push_back( ptr );
}

static void R_FlushDeferredOld( void )
{
	for ( size_t i = 0; i < rtDeferredOld.size(); i++ )
		ri.Z_Free( rtDeferredOld[i] );

	rtDeferredOld.clear();
}

// Call with the render thread idle.
static void R_RotateDeferredFree( void )
{
	R_FlushDeferredOld();
	rtDeferredOld.swap( rtDeferredNew );
}

// True while the render thread runs a frame. Main thread.
qboolean R_RenderThreadBusy( void )
{
	return rtBusy.load( std::memory_order_acquire ) ? qtrue : qfalse;
}

// At least the next frames run on the main thread.
void R_RenderThreadForceSync( int frames )
{
	if ( frames > rt.syncFrames )
		rt.syncFrames = frames;
}

// Exactly the next frames run on the main thread.
void R_RenderThreadSetSync( int frames )
{
	rt.syncFrames = frames;
}

qboolean R_RenderThreadWanted( void )
{
	if ( !r_renderThread || !r_renderThread->integer )
		return qfalse;

	// Two renderers behind one refexport_t share the window and the ghoul2 instances.
	if ( ri.Cvar_VariableIntegerValue( "g_FastRendererSwitch" ) )
		return qfalse;

	return ri.Z_SetThreadSafe ? qtrue : qfalse;
}

void R_RenderThreadInit( void )
{
	R_RenderThreadShutdown();

	if ( !R_RenderThreadWanted() || backEndDataBuf[1] == NULL )
	{
		if ( r_renderThread && r_renderThread->integer )
			ri.Printf( PRINT_ALL, "r_renderThread: not used (g_FastRendererSwitch or no zone lock)\n" );
		return;
	}

	ri.Z_SetThreadSafe( qtrue );

	rt.quit = false;
	rt.pending = false;
	rt.syncFrames = 4;
	rt.countersPending = qfalse;
	rtBusy.store( false );
	rtFailed.store( false );
	rtBackendMsec.store( 0 );
	rt.thread = std::thread( R_RenderThreadMain );
	rt.running = true;

	ri.Printf( PRINT_ALL, "r_renderThread: back end on its own thread\n" );
}

void R_RenderThreadShutdown( void )
{
	if ( !rt.running )
		return;

	R_RenderThreadWait( VKP_RT_SYNC, qfalse, NULL, 0, "R_RenderThreadShutdown" );

	{
		std::lock_guard<std::mutex> lock( rt.mutex );
		rt.quit = true;
	}
	rt.wake.notify_all();
	rt.thread.join();
	rt.running = false;

	R_FlushDeferredOld();
	for ( size_t i = 0; i < rtDeferredNew.size(); i++ )
		ri.Z_Free( rtDeferredNew[i] );
	rtDeferredNew.clear();

	ri.Z_SetThreadSafe( qfalse );
	RB_PrintDrain();

	// An error of the last frame has no frame to stop any more: print it.
	if ( rtFailed.exchange( false ) )
		ri.Printf( PRINT_WARNING, "r_renderThread: error in the last frame: %s\n", rt.failText );
}

// True for a frame that must run on the main thread.
static qboolean R_FrameNeedsMainThread( void )
{
	const qboolean log = R_RenderThreadLogOn();
	qboolean need = qfalse;

	if ( rt.syncFrames > 0 )
	{
		rt.syncFrames--;
		need = qtrue;
		if ( log )
			rtLogReasons[RTR_SYNC_FRAMES]++;
	}

	// Screenshots write files. Video maps run the engine cinematic code. The weather cache
	// and a sky preset read files.
	if ( backEnd.screenshotMask || rb_captureRGBA || r_numVideoMaps > 0 || RB_WorldEffectsNeedSync() )
	{
		need = qtrue;
		if ( log )
		{
			if ( backEnd.screenshotMask )
				rtLogReasons[RTR_SCREENSHOT]++;
			if ( rb_captureRGBA )
				rtLogReasons[RTR_CAPTURE]++;
			if ( r_numVideoMaps > 0 )
				rtLogReasons[RTR_VIDEOMAP]++;
			if ( RB_WorldEffectsNeedSync() )
				rtLogReasons[RTR_WEATHER]++;
		}
	}

#ifdef USE_RTX
	if ( vk.rtxActive && vkpt_physical_sky_load_pending() )
	{
		need = qtrue;
		if ( log )
			rtLogReasons[RTR_SKY_LOAD]++;
	}
#endif

	if ( need && log )
		rtLogInlineFrames++;

	return need;
}

/*
==============
R_SnapshotFogs

Copies the fog volumes, the global fog and the LA goggles slot into the frontend buffer.
The backend reads the copy, so the game can change the fogs while the thread runs.
==============
*/
static void R_SnapshotFogs( void )
{
	int		count;

	if ( !tr.world || !tr.world->fogs )
	{
		backEndData->numFogs = 0;
		backEndData->globalFog = -1;
		return;
	}

	count = tr.world->numfogs + 1;
	if ( count > BE_MAX_FOGS )
		count = BE_MAX_FOGS;

	backEndData->numFogs = count - 1;
	backEndData->globalFog = ( tr.world->globalFog < count - 1 ) ? tr.world->globalFog : -1;
	Com_Memcpy( backEndData->fogs, tr.world->fogs, count * sizeof( fog_t ) );
}

/*
==============
R_RenderThreadSwapBuffers

Copies the state that the backend reads and that the game changes during a frame, then
gives the frontend buffer to the backend. Main thread, with the render thread idle.
==============
*/
static void R_RenderThreadSwapBuffers( void )
{
	backEndData->frameCount = tr.frameCount;
	R_SnapshotFogs();
	Com_Memcpy( backEndData->styleColors, styleColors, sizeof( styleColors ) );
	backEndData->distortionAlpha = tr_distortionAlpha;
	backEndData->distortionStretch = tr_distortionStretch;
	backEndData->distortionPrePost = tr_distortionPrePost;
	backEndData->distortionNegate = tr_distortionNegate;
	backEndData->skyboxPortal = skyboxportal;
	backEndData->drawSkyboxPortal = drawskyboxportal;

	if ( backEndDataBuf[1] )
	{
		backEndDataBE = backEndData;
		backEndData = ( backEndData == backEndDataBuf[0] ) ? backEndDataBuf[1] : backEndDataBuf[0];
		R_G2_SwapRenderSurfHeap();
	}
}

/*
====================
R_IssueRenderCommands
====================
*/
void R_IssueRenderCommands( qboolean runPerformanceCounters ) {
	renderCommandList_t	*cmdList;

	// The render thread must finish the previous frame before this one starts.
	R_RenderThreadWait( VKP_RT_WAIT, qtrue, NULL, 0, "R_IssueRenderCommands (previous frame)" );
	R_RotateDeferredFree();

	if ( runPerformanceCounters && R_RenderThreadLogOn() )
		R_RenderThreadSyncLogFrame();

	// The thread is idle: apply the weather commands that the game gave during its frame.
	R_WorldEffectsFlushQueue();

	cmdList = &backEndData->commands;
	assert(cmdList);
	// add an end-of-list command
	byteAlias_t *ba = (byteAlias_t *)&cmdList->cmds[cmdList->used];
	ba->ui = RC_END_OF_LIST;


	// clear it out, in case this is a sync and not a buffer flip
	cmdList->used = 0;

	// The thread is idle: it is ok to look at the counters of its last frame.
	if ( rt.countersPending ) {
		rt.countersPending = qfalse;
		R_PerformanceCounters();
	}

	if (backEnd.screenshotMask == 0) {
		if (ri.VK_IsMinimized())
			return; // skip backend when minimized
		//if (backEnd.throttle)
		//	return; // or throttled on demand
	}
	else {
		if (ri.VK_IsMinimized() && !R_CanMinimize()) {
			backEnd.screenshotMask = 0;
			return;
		}
	}

	// actually start the commands going
	if ( !r_skipBackEnd->integer ) {
		R_RenderThreadSwapBuffers();

		if ( rt.running && !R_FrameNeedsMainThread() ) {
			{
				std::lock_guard<std::mutex> lock( rt.mutex );

				rt.cmds = backEndDataBE->commands.cmds;
				rt.pending = true;
				rtBusy.store( true, std::memory_order_release );
			}
			rt.wake.notify_one();

			rt.countersPending = runPerformanceCounters;
			return;
		}

		// let it start on the new batch
		RB_PrintBackendBegin();
		RB_ExecuteRenderCommands( backEndDataBE->commands.cmds );
		RB_PrintBackendEnd();
		RB_PrintDrain();
		rtBackendMsec.store( backEnd.pc.msec );
	}

	// at this point, the back end thread is idle, so it is ok
	// to look at it's performance counters
	if ( runPerformanceCounters ) {
		R_PerformanceCounters();
	}
}

/*
============
R_GetCommandBufferReserved

make sure there is enough command space
============
*/
static void* R_GetCommandBufferReserved( int bytes, int reservedBytes ) {
	renderCommandList_t* cmdList;

	cmdList = &backEndData->commands;
	bytes = PAD(bytes, sizeof(void*));

	// always leave room for the end of list command
	if (cmdList->used + bytes + sizeof(int) + reservedBytes > MAX_RENDER_COMMANDS) {
		if (bytes > MAX_RENDER_COMMANDS - sizeof(int)) {
			ri.Error(ERR_FATAL, "R_GetCommandBuffer: bad size %i", bytes);
		}
		// if we run out of room, just start dropping commands
		return NULL;
	}

	cmdList->used += bytes;

	return cmdList->cmds + cmdList->used - bytes;

}
/*
============
R_GetCommandBuffer

make sure there is enough command space
============
*/
void *R_GetCommandBuffer( int bytes ) {
	tr.lastRenderCommand = RC_END_OF_LIST;

	return R_GetCommandBufferReserved(bytes, PAD(sizeof(swapBuffersCommand_t), sizeof(void*)));
}

/*
=============
R_AddDrawSurfCmd

=============
*/
void	R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	drawSurfsCommand_t	*cmd;

	cmd = (drawSurfsCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_SURFS;

	cmd->drawSurfs = drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;

	R_G2_NoteDrawSurfsCmd( cmd );

	tr.numDrawSurfCmds++;
	if (tr.drawSurfCmd == NULL) {
		tr.drawSurfCmd = cmd;
	}
}

#ifdef VK_CUBEMAP
/*
=============
R_AddConvolveCubemapCmd

The prefilter of a probe, after the six views of its capture.
=============
*/
void R_AddConvolveCubemapCmd( int cubemapIndex ) {
	convolveCubemapCommand_t *cmd;

	cmd = (convolveCubemapCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_CONVOLVECUBEMAP;
	cmd->cubemapIndex = cubemapIndex;
}
#endif

/*
=============
RE_SetColor

Passing NULL will set the color to white
=============
*/
void RE_SetColor( const float *rgba ) {
	setColorCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}
	cmd = (setColorCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SET_COLOR;
	if ( !rgba ) {
		static float colorWhite[4] = { 1, 1, 1, 1 };

		rgba = colorWhite;
	}

	cmd->color[0] = rgba[0];
	cmd->color[1] = rgba[1];
	cmd->color[2] = rgba[2];
	cmd->color[3] = rgba[3];
}

/*
=============
RE_StretchPic
=============
*/

/*static Matrix4f orthographic(float left, float right, float bottom, float top, float near, float far) {
	Matrix4f ortho = new Matrix4f();

	float tx = -(right + left) / (right - left);
	float ty = -(top + bottom) / (top - bottom);
	float tz = -(far + near) / (far - near);

	ortho.m00 = 2f / (right - left);
	ortho.m11 = 2f / (top - bottom);
	ortho.m22 = -2f / (far - near);
	ortho.m03 = tx;
	ortho.m13 = ty;
	ortho.m23 = tz;

	return ortho;
}*/

void RE_StretchPic ( float x, float y, float w, float h,
					  float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	stretchPicCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}

	cmd = (stretchPicCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}

	cmd->commandId = RC_STRETCH_PIC;
	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
}

/*
=============
RE_RotatePic
=============
*/
void RE_RotatePic ( float x, float y, float w, float h,
					  float s1, float t1, float s2, float t2,float a, qhandle_t hShader, float aspectCorrection ) {
	rotatePicCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}

	cmd = (rotatePicCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}

	cmd->commandId = RC_ROTATE_PIC;
	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
	cmd->a = a;
	cmd->ratio = aspectCorrection;
}

/*
=============
RE_RotatePic2
=============
*/
void RE_RotatePic2 ( float x, float y, float w, float h,
					  float s1, float t1, float s2, float t2,float a, qhandle_t hShader, float aspectCorrection ) {
	rotatePicCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}

	cmd = (rotatePicCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}

	cmd->commandId = RC_ROTATE_PIC2;
	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
	cmd->a = a;
	cmd->ratio = aspectCorrection;
}

/*
=============
RE_LAGoggles

SP light amp goggles. The next scene draws every surface in the fog slot
past numfogs, with white lightmaps. The fog depth pulses with time.
=============
*/
void RE_LAGoggles( void )
{
	if ( !tr.world ) {
		return;
	}

	tr.refdef.doLAGoggles = qtrue;

	fog_t *fog = &tr.world->fogs[tr.world->numfogs];

	fog->parms.color[0] = 0.75f;
	fog->parms.color[1] = 0.42f + Q_flrand(0.0f, 1.0f) * 0.025f;
	fog->parms.color[2] = 0.07f;
	fog->colorInt = ColorBytes4( fog->parms.color[0], fog->parms.color[1], fog->parms.color[2], 1.0f );
	for ( int n = 0; n < 4; n++ ) {
		fog->color[n] = ( ( fog->colorInt >> ( n * 8 ) ) & 255 ) / 255.0f;
	}

	// rd-vanilla: depthForOpaque 10000 with tcScale 2 / (depth * pulse). Express
	// the same result as a plain depth, so the CPU and GPU fog paths agree.
	fog->parms.depthForOpaque = 10000.0f * ( 1.0f + cos( tr.refdef.floatTime ) * 0.1f ) / 16.0f;
	fog->tcScale = 1.0f / ( fog->parms.depthForOpaque * 8.0f );
	fog->hasSurface = qfalse;
}

void RE_RenderWorldEffects( void )
{
	drawBufferCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}

	cmd = (drawBufferCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_WORLD_EFFECTS;
}

void RE_RenderAutoMap( void )
{
	drawBufferCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}

	cmd = (drawBufferCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd )
	{
		return;
	}
	cmd->commandId = RC_AUTO_MAP;
}

/*
====================
RE_BeginFrame

If running in stereo, RE_BeginFrame will be called twice
for each RE_EndFrame
====================
*/
void RE_BeginFrame( stereoFrame_t stereoFrame ) {
	drawBufferCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}
	
	// An error that left the backend leaves the flag set.
	RB_PrintBackendEnd();
	RB_PrintDrain();
	R_BackendCvarDrain();

	// The frontend heap. The backend resets its own state at the start of the frame (RB_DrawBuffer).
	ResetGhoul2RenderableSurfaceHeap();

	tr.frameCount++;
	tr.frameSceneNum = 0;

	// The rest changes state that the backend reads. It waits for the render thread first.

	//
	// texturemode stuff
	//
	if ( r_textureMode->modified ) {
		R_SyncRenderThread();
		vk_texture_mode( r_textureMode->string, qfalse );
		r_textureMode->modified = qfalse;
		vk_update_post_process_pipelines();
	}

	//
	// gamma stuff
	//
	if ( r_gamma->modified || r_greyscale->modified || r_dither->modified ) {
		R_SyncRenderThread();
		r_gamma->modified = qfalse;
		r_greyscale->modified = qfalse;
		r_dither->modified = qfalse;

		R_SetColorMappings();
		vk_update_post_process_pipelines();
	}

	if ( cl_ratioFix->modified ) {
		R_SyncRenderThread();
		R_Set2DRatio();
		cl_ratioFix->modified = qfalse;
	}

#ifndef USE_BUFFER_CLEAR
	if ( r_fastsky->modified && vk.clearAttachment ) {
#else
	if ( r_fastsky->modified ) {
#endif
		R_SyncRenderThread();
		vk_set_clearcolor();
		r_fastsky->modified = qfalse;
	}

	if ( r_surfaceSprites->modified )
	{
		R_SyncRenderThread();
		backEndData->ss.groups_count = 0;
		backEndDataBE->ss.groups_count = 0;
		r_surfaceSprites->modified = qfalse;
	}

#ifdef USE_RTX
	vk_rtx_cvar_handler();
#endif

	//
	// draw buffer stuff
	//
	cmd = (drawBufferCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_BUFFER;
	tr.lastRenderCommand = RC_DRAW_BUFFER;

	cmd->buffer = 0;

#ifndef USE_BUFFER_CLEAR
	if ( vk.clearAttachment && ( r_fastsky->integer || ( tr.world && tr.world->globalFog != -1 ) ) ) {
		clearColorCommand_t *clrcmd;
		if ( ( clrcmd = (clearColorCommand_t*)R_GetCommandBuffer( sizeof( *clrcmd ) ) ) == nullptr )
			return;
		clrcmd->commandId = RC_CLEARCOLOR;
	}
#endif // USE_BUFFER_CLEAR
}

/*
=============
RE_EndFrame

Returns the number of msec spent in the back end
=============
*/
void RE_EndFrame( int *frontEndMsec, int *backEndMsec ) {
	swapBuffersCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}
	// Use the space that R_GetCommandBuffer keeps for this command. Without it, a full
	// buffer stops every later frame: R_InitNextFrame never runs again.
	cmd = (swapBuffersCommand_t *) R_GetCommandBufferReserved( sizeof( *cmd ), 0 );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SWAP_BUFFERS;

	R_IssueRenderCommands( qtrue );

	tr.needScreenMap = 0;

	// use the other buffers next frame, because another CPU
	// may still be rendering into the current ones
	R_InitNextFrame();

	vk_perf_count( VKP_RT_THREAD, rt.running ? 1 : 0 );
	vk_perf_end_frame();

	if ( frontEndMsec ) {
		*frontEndMsec = tr.frontEndMsec;
	}
	tr.frontEndMsec = 0;

	// With the render thread, backEnd.pc belongs to the thread: give the msec of its last frame.
	if ( rt.running ) {
		if ( backEndMsec ) {
			*backEndMsec = rtBackendMsec.load();
		}
		return;
	}
	if ( backEndMsec ) {
		*backEndMsec = backEnd.pc.msec;
	}
	backEnd.pc.msec = 0;
}

/*
=============
RE_TakeVideoFrame
=============
*/
void RE_TakeVideoFrame( int width, int height, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg )
{
	videoFrameCommand_t *cmd;

	if ( !tr.registered )
		return;
#if 0
	cmd = (videoFrameCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd )
		return;

	cmd->commandId = RC_VIDEOFRAME;
#endif

	R_SyncRenderThread();

	backEnd.screenshotMask |= SCREENSHOT_AVI;
	cmd = &backEnd.vcmd;

	cmd->width = width;
	cmd->height = height;
	cmd->captureBuffer = captureBuffer;
	cmd->encodeBuffer = encodeBuffer;
	cmd->motionJpeg = motionJpeg;
}

qboolean R_CanMinimize( void )
{
	if (vk.fboActive || vk.offscreenRender)
		return qtrue;

	return qfalse;
}
