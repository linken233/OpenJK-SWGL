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

// tr_subs.cpp - common function replacements for modular renderer
#include "tr_local.h"

#include <mutex>

/*
==============
Backend print queue

The engine print function is not safe to call from the thread that runs the backend.
RB_PrintInstall replaces ri.Printf with a function that queues the message when the
calling thread is inside RB_PrintBackendBegin / RB_PrintBackendEnd, and prints at once
otherwise. The main thread prints the queue in RB_PrintDrain. Messages beyond the size
of the queue are dropped and counted.
==============
*/
#define RB_PRINT_QUEUE_LINES	256
#define RB_PRINT_LINE_LENGTH	1024

typedef struct rbPrintLine_s {
	int		level;
	char	text[RB_PRINT_LINE_LENGTH];
} rbPrintLine_t;

static rbPrintLine_t	rbPrintQueue[RB_PRINT_QUEUE_LINES];
static int				rbPrintHead;		// next line to print
static int				rbPrintCount;
static int				rbPrintDropped;
static std::mutex		rbPrintMutex;
static thread_local bool	rbOnBackendThread = false;
static void				( QDECL *rbRealPrintf )( int printLevel, const char *fmt, ... );

static void QDECL RB_PrintfInterposer( int level, const char *fmt, ... )
{
	char		text[8192];		// as long as the engine print buffer
	va_list		argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	if ( !rbOnBackendThread )
	{
		rbRealPrintf( level, "%s", text );
		return;
	}

	std::lock_guard<std::mutex> lock( rbPrintMutex );

	if ( rbPrintCount >= RB_PRINT_QUEUE_LINES )
	{
		rbPrintDropped++;
		return;
	}

	rbPrintLine_t &line = rbPrintQueue[( rbPrintHead + rbPrintCount ) % RB_PRINT_QUEUE_LINES];
	line.level = level;
	Q_strncpyz( line.text, text, sizeof( line.text ) );
	rbPrintCount++;
}

/*
==============
Backend error

The engine error function jumps out of the frame. That is not safe on the render thread.
RB_ErrorInterposer hands an error of the render thread to R_RenderThreadAbort. The main
thread raises it with RB_RealError at its next wait.
==============
*/
static void ( QDECL *rbRealError )( int errorLevel, const char *fmt, ... );

static void QDECL RB_ErrorInterposer( int level, const char *fmt, ... )
{
	char		text[8192];
	va_list		argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	if ( R_OnRenderThread() )
		R_RenderThreadAbort( level, text );

	rbRealError( level, "%s", text );
}

void RB_RealError( int level, const char *text )
{
	rbRealError( level, "%s", text );
}

/*
==============
Console commands

A console command runs on the main thread between two frames, and many of them change
state that the backend reads. RB_CommandInterposer registers each command behind a stub
that waits for the render thread first.
==============
*/
#define RB_MAX_COMMANDS		128

static xcommand_t	rbCommandFn[RB_MAX_COMMANDS];
static char			rbCommandName[RB_MAX_COMMANDS][MAX_QPATH];
static xcommand_t	rbCommandStub[RB_MAX_COMMANDS];
static int			rbNumCommands;
static void			( *rbRealAddCommand )( const char *cmd_name, xcommand_t function );

template<int N> static void RB_CommandThunk( void )
{
	R_SyncRenderThread();
	rbCommandFn[N]();
}

template<int N> struct RB_ThunkFill
{
	static void Run( xcommand_t *stubs )
	{
		RB_ThunkFill<N - 1>::Run( stubs );
		stubs[N - 1] = RB_CommandThunk<N - 1>;
	}
};

template<> struct RB_ThunkFill<0>
{
	static void Run( xcommand_t * ) {}
};

static void RB_AddCommandInterposer( const char *cmd_name, xcommand_t function )
{
	int i;

	for ( i = 0; i < rbNumCommands; i++ )
	{
		if ( !Q_stricmp( rbCommandName[i], cmd_name ) )
			break;
	}

	if ( i == rbNumCommands )
	{
		if ( rbNumCommands >= RB_MAX_COMMANDS )
		{
			rbRealAddCommand( cmd_name, function );
			return;
		}

		Q_strncpyz( rbCommandName[i], cmd_name, sizeof( rbCommandName[i] ) );
		rbNumCommands++;
	}

	rbCommandFn[i] = function;
	rbRealAddCommand( cmd_name, rbCommandStub[i] );
}

void RB_PrintInstall( void )
{
	if ( ri.Printf == RB_PrintfInterposer )
		return;

	rbRealPrintf = ri.Printf;
	ri.Printf = RB_PrintfInterposer;

	rbRealError = ri.Error;
	ri.Error = RB_ErrorInterposer;

	RB_ThunkFill<RB_MAX_COMMANDS>::Run( rbCommandStub );
	rbRealAddCommand = ri.Cmd_AddCommand;
	ri.Cmd_AddCommand = RB_AddCommandInterposer;
}

void RB_PrintBackendBegin( void )
{
	rbOnBackendThread = true;
}

void RB_PrintBackendEnd( void )
{
	rbOnBackendThread = false;
}

// Main thread only.
void RB_PrintDrain( void )
{
	if ( !rbRealPrintf )
		return;

	for ( ;; )
	{
		rbPrintLine_t	line;
		bool			haveLine = false;
		int				dropped = 0;

		{
			std::lock_guard<std::mutex> lock( rbPrintMutex );

			if ( rbPrintCount )
			{
				line = rbPrintQueue[rbPrintHead];
				rbPrintHead = ( rbPrintHead + 1 ) % RB_PRINT_QUEUE_LINES;
				rbPrintCount--;
				haveLine = true;
			}
			else
			{
				dropped = rbPrintDropped;
				rbPrintDropped = 0;
			}
		}

		if ( haveLine )
		{
			rbRealPrintf( line.level, "%s", line.text );
			continue;
		}

		if ( dropped )
			rbRealPrintf( PRINT_WARNING, "%d backend messages dropped\n", dropped );

		return;
	}
}

void QDECL Com_Printf( const char *msg, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	ri.Printf(PRINT_ALL, "%s", text);
}

void QDECL Com_OPrintf( const char *msg, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);
	ri.Printf(PRINT_ALL, "%s", text);
}

void QDECL Com_DPrintf( const char *format, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, format);
	Q_vsnprintf(text, sizeof(text), format, argptr);
	va_end(argptr);

	ri.Printf(PRINT_DEVELOPER, "%s", text);
}

void QDECL Com_Error( int level, const char *error, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	ri.Error(level, "%s", text);
}

// HUNK

// SP's refimport_t has no hunk allocator exposed to the renderer -- route
// through the zone allocator instead, matching code/rd-rend2's own path.
void *Hunk_Alloc( int size, ha_pref preference ) {
	// Hunk_Clear() frees TAG_HUNKALLOC on each map load. R_ClearStuffToStopGhoul2CrashingThings resets the pointers.
	return ri.Z_Malloc( size, TAG_HUNKALLOC, qtrue, 4 );
}

void *Hunk_AllocateTempMemory( int size ) {
	return ri.Z_Malloc( size, TAG_TEMP_HUNKALLOC, qfalse, 4 );
}

void Hunk_FreeTempMemory( void *buf ) {
	ri.Z_Free( buf );
}

// ZONE
void *Z_Malloc( int iSize, memtag_t eTag, qboolean bZeroit, int iAlign ) {
	return ri.Z_Malloc( iSize, eTag, bZeroit, iAlign );
}

// SP's qcommon.h declares a bare "int Z_Free(void*)" -- match its signature.
int Z_Free( void *ptr ) {
	return ri.Z_Free( ptr );
}

// SP's tr_image_jpg.cpp/tr_image_tga.cpp/tr_image_png.cpp (rd-common) call
// these directly -- rend2/vanilla both define them, matching tr_common.h.
void *R_Malloc( int iSize, memtag_t eTag, qboolean bZeroit ) {
	return ri.Z_Malloc( iSize, eTag, bZeroit, 4 );
}

void R_Free( void *ptr ) {
	ri.Z_Free( ptr );
}

int Z_MemSize( memtag_t eTag ) {
	return ri.Z_MemSize( eTag );
}

void Z_MorphMallocTag( void *pvBuffer, memtag_t eDesiredTag ) {
	ri.Z_MorphMallocTag( pvBuffer, eDesiredTag );
}

// CACHED BSP DISK IMAGE
void *CM_GetCachedMapDiskImage( void ) {
	//TODO Check if this is correct
	return NULL; // SP never populates the disk-image cache for the renderer
}

void CM_SetCachedMapDiskImage( void *ptr ) {

}

void CM_SetUsingCache( qboolean usingCache ) {
	*(ri.gbUsingCachedMapDataRightNow()) = usingCache;
}

qboolean Com_TheHunkMarkHasBeenMade( void ) {
	//TODO Check if this is correct
	return qtrue; // unreachable on SP -- RE_RegisterServerSkin has no export entry

}

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags, const char *var_desc ) {
	return ri.Cvar_Get( var_name, value, flags );
}

int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits ) {
	return ri.CIN_PlayCinematic( arg0, xpos, ypos, width, height, bits, NULL );
}
