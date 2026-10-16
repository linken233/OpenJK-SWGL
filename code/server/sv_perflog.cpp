// com_perflog: a summary of the frame times every N frames, with the numbers a "can this PC run
// the game" test needs: frame rate, the slowest frames, where the CPU time goes, and how many NPCs
// were alive. The renderer adds its own times through rd-common/perf_stats_api.h.

#include "../server/exe_headers.h"

#include "server.h"
#include "../client/client.h"
#include "../rd-common/perf_stats_api.h"

#include <algorithm>

extern int		com_frameNumber;

cvar_t	*com_perflog;			// 0 = off, 1 = console, 2 = console and perflog.csv
static cvar_t	*com_perflogFrames;	// frames per summary
static cvar_t	*com_perflogSpike;	// ms: a frame slower than this prints one line (0 = off)

#define PERF_MAX_WINDOW		1024

static float		perf_interval[PERF_MAX_WINDOW];	// ms from the end of the previous frame to the end of this one
static int			perf_count;
static double		perf_lastEnd;
static int			perf_wasOn;

static double		perf_frameGame;		// game time of the frame in progress
static double		perf_sumWork, perf_sumSv, perf_sumCl, perf_sumGame;
static double		perf_maxGame;
static double		perf_frameScr, perf_frameAudio;	// client phases of the frame in progress
static int			perf_frameSlices;				// calls into the game in the frame in progress
static double		perf_frameMaxCall;				// longest call into the game in the frame in progress
static int			perf_spikeTraces;				// world traces at the end of the previous frame
static int			perf_gameFrames;	// frames that ran game code

static fileHandle_t	perf_csv;
static qboolean		perf_csvHeader;

// Trace repeat statistics: how many world traces repeat the arguments of an earlier trace, in the
// same frame or in the previous one. This is the hit rate that a cache of world traces would have.
#define PERF_TRACE_SLOTS	65536
#define PERF_TRACE_PROBES	8

typedef struct {
	unsigned long long	hash;	// 0 = empty
	int					frame;	// last frame in which the trace was made
} perfTraceSlot_t;

static perfTraceSlot_t	perf_traceSlots[PERF_TRACE_SLOTS];
static int				perf_traces, perf_sameFrame, perf_samePrev;
static int				perf_repeatsOn;				// 1 when the repeat statistics ran in this window
static cvar_t			*com_perflogRepeats;		// 1 = count the traces that repeat an earlier trace

void PerfLog_NoteTrace( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int contentmask )
{
	unsigned long long	h = 1469598103934665603ULL;	// FNV-1a over the bits of the arguments
	unsigned int		bits[13];

	perf_traces++;

	// The repeat statistics cost a hash and a table probe for every trace: only with com_perflogRepeats 1.
	if ( !com_perflogRepeats || !com_perflogRepeats->integer ) {
		return;
	}

	for ( int i = 0; i < 3; i++ ) {
		memcpy( &bits[i], &start[i], 4 );
		memcpy( &bits[3 + i], &end[i], 4 );
		memcpy( &bits[6 + i], &mins[i], 4 );
		memcpy( &bits[9 + i], &maxs[i], 4 );
	}
	bits[12] = (unsigned int)contentmask;

	for ( int i = 0; i < 13; i++ ) {
		h = ( h ^ bits[i] ) * 1099511628211ULL;
	}
	if ( h == 0 ) {
		h = 1;
	}

	perf_repeatsOn = 1;

	for ( int probe = 0; probe < PERF_TRACE_PROBES; probe++ ) {
		perfTraceSlot_t &slot = perf_traceSlots[( h + probe ) & ( PERF_TRACE_SLOTS - 1 )];

		if ( slot.hash == h ) {
			if ( slot.frame == com_frameNumber ) {
				perf_sameFrame++;
			} else if ( slot.frame == com_frameNumber - 1 ) {
				perf_samePrev++;
			}
			slot.frame = com_frameNumber;
			return;
		}
		if ( slot.hash == 0 || slot.frame < com_frameNumber - 1 ) {
			slot.hash = h;
			slot.frame = com_frameNumber;
			return;
		}
	}
}

void PerfLog_Init( void )
{
	com_perflog = Cvar_Get( "com_perflog", "0", 0 );
	com_perflogFrames = Cvar_Get( "com_perflogFrames", "30", 0 );
	com_perflogSpike = Cvar_Get( "com_perflogSpike", "0", 0 );
	com_perflogRepeats = Cvar_Get( "com_perflogRepeats", "0", 0 );
}

static const char *PerfLog_ClientStateName( int state )
{
	static const char *const names[] = {
		"uninitialized", "disconnected", "connecting", "challenging", "connected",
		"loading", "primed", "active", "cinematic"
	};

	return ( state >= 0 && state < (int)ARRAY_LEN( names ) ) ? names[state] : "?";
}

// Writes the state of the connection between the client and the local server: the client state,
// the server and its client slot, the pause flags, and whether a saved game load is in progress.
static void PerfLog_DescribeConnection( char *out, int size )
{
	static const char *const svNames[] = { "dead", "loading", "game" };
	static const char *const slotNames[] = { "free", "zombie", "connected", "primed", "active" };
	extern qboolean gbAlreadyDoingLoad;

	const int running = com_sv_running ? com_sv_running->integer : 0;
	const int slot = ( running && svs.clients ) ? (int)svs.clients[0].state : -1;

	Com_sprintf( out, size, "client %s, server %s (%s), client slot %s, cl_paused %i, sv_paused %i, loading %i, catcher %i, sv.time %i",
		PerfLog_ClientStateName( cls.state ),
		running ? "running" : "off",
		( (int)sv.state >= 0 && (int)sv.state < 3 ) ? svNames[sv.state] : "?",
		( slot >= 0 && slot < 5 ) ? slotNames[slot] : "none",
		cl_paused ? cl_paused->integer : 0, sv_paused ? sv_paused->integer : 0,
		(int)gbAlreadyDoingLoad, Key_GetCatcher(), sv.time );
}

// Called by CL_Frame. Prints one line when the connection state changes. A load or a map change
// does this a few times, so the line is rare.
void PerfLog_ConnectionTrace( void )
{
	static int	last[5] = { -1, -1, -1, -1, -1 };
	int			cur[5];
	char		text[256];

	cur[0] = cls.state;
	cur[1] = com_sv_running ? com_sv_running->integer : 0;
	cur[2] = ( cur[1] && svs.clients ) ? (int)svs.clients[0].state : -1;
	cur[3] = cl_paused ? cl_paused->integer : 0;
	cur[4] = cur[1] ? (int)sv.state : -1;

	if ( !memcmp( cur, last, sizeof( cur ) ) ) {
		return;
	}
	memcpy( last, cur, sizeof( cur ) );

	PerfLog_DescribeConnection( text, sizeof( text ) );
	Com_Printf( "connection: %s\n", text );
}

// Game code that runs inside the server frame: called by SV_Frame around every call into the game.
void PerfLog_AddGame( double ms )
{
	perf_frameGame += ms;
	perf_frameSlices++;
	perf_frameMaxCall = std::max( perf_frameMaxCall, ms );
}

// Client phases of CL_Frame: 0 = screen update, 1 = audio update.
void PerfLog_AddPhase( int phase, double ms )
{
	( phase ? perf_frameAudio : perf_frameScr ) += ms;
}

double PerfLog_Now( void )
{
	return RD_PerfNowMs();
}

static void PerfLog_Reset( void )
{
	perf_count = 0;
	perf_sumWork = perf_sumSv = perf_sumCl = perf_sumGame = 0.0;
	perf_maxGame = 0.0;
	perf_gameFrames = 0;
	perf_frameGame = 0.0;
	perf_traces = perf_sameFrame = perf_samePrev = 0;
	perf_repeatsOn = 0;
	perf_spikeTraces = 0;
}

static void PerfLog_CountEntities( int *npcs, int *used )
{
	*npcs = 0;
	*used = 0;

	if ( !ge || !com_sv_running->integer || !ge->gentities ) {
		return;
	}

	for ( int i = 0; i < ge->num_entities; i++ ) {
		const gentity_t *ent = SV_GentityNum( i );

		if ( !ent->inuse ) {
			continue;
		}
		( *used )++;
		if ( i > 0 && ent->client ) {
			( *npcs )++;
		}
	}
}

static void PerfLog_WriteCsv( int npcs, int used, double fps, double avg, double p95, double max,
	double work, double svMs, double gameMs, double clMs, const rdPerfEntry_t *rd, int numRd )
{
	char	line[2048];
	int		len;

	if ( !perf_csv ) {
		perf_csv = FS_FOpenFileWrite( "perflog.csv" );
		if ( !perf_csv ) {
			return;
		}
	}

	if ( !perf_csvHeader ) {
		len = Com_sprintf( line, sizeof( line ), "frame,fps,frame_avg,frame_p95,frame_max,work,sv,game,cl,npcs,ents" );
		for ( int i = 0; i < numRd; i++ ) {
			len += Com_sprintf( line + len, sizeof( line ) - len, ",%s", rd[i].name );
		}
		len += Com_sprintf( line + len, sizeof( line ) - len, "\n" );
		FS_Write( line, len, perf_csv );
		perf_csvHeader = qtrue;
	}

	len = Com_sprintf( line, sizeof( line ), "%i,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%i,%i",
		com_frameNumber, fps, avg, p95, max, work, svMs, gameMs, clMs, npcs, used );
	for ( int i = 0; i < numRd; i++ ) {
		len += Com_sprintf( line + len, sizeof( line ) - len, ",%.3f", rd[i].avg );
	}
	len += Com_sprintf( line + len, sizeof( line ) - len, "\n" );
	FS_Write( line, len, perf_csv );
	FS_ForceFlush( perf_csv );
}

static void PerfLog_Report( void )
{
	rdPerfEntry_t	rd[RD_PERF_MAX];
	int				numRd = 0;
	float			sorted[PERF_MAX_WINDOW];
	char			msLine[1024], cntLine[512];
	int				npcs, used;

	const int n = perf_count;
	const double inv = 1.0 / n;

	double wall = 0.0;
	for ( int i = 0; i < n; i++ ) {
		sorted[i] = perf_interval[i];
		wall += perf_interval[i];
	}
	std::sort( sorted, sorted + n );

	const double fps = wall > 0.0 ? n * 1000.0 / wall : 0.0;
	const double p95 = sorted[std::min( n - 1, (int)( n * 0.95 ) )];

	PerfLog_CountEntities( &npcs, &used );

	const rdPerfAPI_t *api = re.GetExtension ? (const rdPerfAPI_t *)re.GetExtension( RD_PERF_API_NAME ) : NULL;
	if ( api && api->version == RD_PERF_API_VERSION ) {
		numRd = api->Collect( rd, RD_PERF_MAX );
	}

	// the game sum covers every frame: a game frame costs much more than the mean
	Com_Printf( "perf %i: %.1f fps | frame avg %.1f p95 %.1f max %.1f ms | cpu work %.1f = server %.1f (game %.1f, max %.1f, %i/%i frames) + client %.1f | npcs %i, ents %i\n",
		com_frameNumber, fps, wall * inv, p95, (double)sorted[n - 1],
		perf_sumWork * inv, perf_sumSv * inv, perf_sumGame * inv, perf_maxGame, perf_gameFrames, n,
		perf_sumCl * inv, npcs, used );

	// A client that is not in the game for a whole interval is a stalled load: show why.
	if ( cls.state != CA_ACTIVE ) {
		char text[256];

		PerfLog_DescribeConnection( text, sizeof( text ) );
		Com_Printf( "     connection: %s\n", text );
	}

	if ( perf_traces > 0 ) {
		if ( perf_repeatsOn ) {
			Com_Printf( "     world traces/frame %i: same args as earlier this frame %i%%, as the previous frame %i%%\n",
				perf_traces / n, perf_sameFrame * 100 / perf_traces, perf_samePrev * 100 / perf_traces );
		} else {
			Com_Printf( "     world traces/frame %i\n", perf_traces / n );
		}
	}

	if ( numRd > 0 ) {
		int ms = 0, cnt = 0;

		for ( int i = 0; i < numRd; i++ ) {
			if ( rd[i].unit == RD_PERF_MS ) {
				if ( rd[i].max > 0.0f ) {
					ms += Com_sprintf( msLine + ms, sizeof( msLine ) - ms, " %s %.1f/%.1f", rd[i].name, rd[i].avg, rd[i].max );
				} else {
					ms += Com_sprintf( msLine + ms, sizeof( msLine ) - ms, " %s %.1f", rd[i].name, rd[i].avg );
				}
			} else {
				cnt += Com_sprintf( cntLine + cnt, sizeof( cntLine ) - cnt, " %s %.0f", rd[i].name, rd[i].avg );
			}
		}
		Com_Printf( "     renderer ms (avg/worst):%s\n     renderer load:%s\n", msLine, cntLine );

		// With a render thread: the back end runs beside the main thread. The main thread loses
		// the time it waits for it.
		float rtThread = 0.0f, rtWait = 0.0f, rtSync = 0.0f, rtBackend = 0.0f, rtWaitMax = 0.0f;

		for ( int i = 0; i < numRd; i++ ) {
			if ( !Q_stricmp( rd[i].name, "rtthread" ) ) {
				rtThread = rd[i].avg;
			} else if ( !Q_stricmp( rd[i].name, "rtwait" ) ) {
				rtWait = rd[i].avg;
				rtWaitMax = rd[i].max;
			} else if ( !Q_stricmp( rd[i].name, "rtsync" ) ) {
				rtSync = rd[i].avg;
			} else if ( !Q_stricmp( rd[i].name, "backend" ) ) {
				rtBackend = rd[i].avg;
			}
		}

		if ( rtThread > 0.0f ) {
			Com_Printf( "     render thread: back end %.1f ms on its own thread | main thread waits %.1f ms (frame end %.1f, worst %.1f; registration and uploads %.1f)\n",
				rtBackend, rtWait + rtSync, rtWait, rtWaitMax, rtSync );
		}
	}

	if ( com_perflog->integer >= 2 ) {
		PerfLog_WriteCsv( npcs, used, fps, wall * inv, p95, (double)sorted[n - 1],
			perf_sumWork * inv, perf_sumSv * inv, perf_sumGame * inv, perf_sumCl * inv, rd, numRd );
	}
}

// Called by Com_Frame at the end of every frame, with the times of its three points: the start
// of the server frame, the end of it, and the end of the client frame.
void PerfLog_EndFrame( double workStart, double svEnd, double clEnd )
{
	const int window = com_perflogFrames->integer < 1 ? 1 : ( com_perflogFrames->integer > PERF_MAX_WINDOW ? PERF_MAX_WINDOW : com_perflogFrames->integer );

	if ( !com_perflog->integer ) {
		perf_frameGame = 0.0;
		perf_frameScr = perf_frameAudio = perf_frameMaxCall = 0.0;
		perf_frameSlices = 0;
		if ( perf_wasOn ) {
			perf_wasOn = 0;
			PerfLog_Reset();
			if ( perf_csv ) {
				FS_FCloseFile( perf_csv );
				perf_csv = 0;
				perf_csvHeader = qfalse;
			}
		}
		return;
	}

	if ( !perf_wasOn ) {
		// the first frame of a run has no start time to measure its interval against
		perf_wasOn = 1;
		perf_lastEnd = clEnd;
		PerfLog_Reset();
		return;
	}

	const double work = clEnd - workStart;
	const double svMs = svEnd - workStart;
	const double cl = clEnd - svEnd;
	const double game = perf_frameGame;

	const double interval = clEnd - perf_lastEnd;

	if ( com_perflogSpike->value > 0.0f && interval > com_perflogSpike->value ) {
		Com_Printf( "spike %i: %.1f ms | server %.1f (game %.1f in %i calls, longest %.1f) | client %.1f (screen %.1f, audio %.1f) | traces %i\n",
			com_frameNumber, interval, svMs, game, perf_frameSlices, perf_frameMaxCall, cl, perf_frameScr, perf_frameAudio,
			perf_traces - perf_spikeTraces );
	}
	perf_spikeTraces = perf_traces;

	perf_interval[perf_count++] = (float)interval;
	perf_lastEnd = clEnd;

	perf_sumWork += work;
	perf_sumSv += svMs;
	perf_sumCl += cl;
	perf_sumGame += game;
	perf_maxGame = std::max( perf_maxGame, game );
	if ( game > 0.0 ) {
		perf_gameFrames++;
	}
	perf_frameGame = 0.0;
	perf_frameScr = perf_frameAudio = perf_frameMaxCall = 0.0;
	perf_frameSlices = 0;

	if ( perf_count >= window ) {
		PerfLog_Report();
		PerfLog_Reset();
	}
}
