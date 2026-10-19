// jobs.cpp -- worker thread pool for data-parallel loops. See jobs.h.

#include "q_shared.h"
#include "qcommon.h"
#include "jobs.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#endif

#define JOBS_MAX_THREADS	16

static cvar_t	*com_jobThreads;

// Up to JOBS_MAX_SLOTS jobs run at the same time, one for each caller thread.
// Workers wait on s_wake, pick an open slot and claim chunks from its next counter.
// All slot fields except next are guarded by s_mutex.
#define JOBS_MAX_SLOTS	2

struct jobSlot_t {
	bool				inUse;			// a caller owns the slot
	bool				open;			// the job accepts new workers
	int					activeWorkers;
	jobRangeFn_t		fn;
	void				*ctx;
	int					count;
	int					chunk;
	std::atomic<int>	next;
	std::atomic<bool>	abort;			// a worker raised an error: skip the chunks that are left
	bool				failed;			// guarded by s_mutex
	int					failLevel;
	char				failText[1024];
};

struct jobAbort_s {};					// thrown on a worker by Jobs_WorkerError, caught in the worker

#define JOBS_PRINT_LINES	64
#define JOBS_PRINT_LENGTH	1024

// Never freed: a joinable thread at CRT exit would call std::terminate on a fatal-error exit.
static std::vector<std::thread>	&s_threads = *new std::vector<std::thread>;
static std::mutex				s_mutex;
static std::condition_variable	s_wake;
static std::condition_variable	s_done;
static bool						s_quit;
static jobSlot_t				s_slots[JOBS_MAX_SLOTS];

static thread_local bool		t_isWorker;
static thread_local jobSlot_t	*t_workerSlot;		// the job that a worker runs now
static thread_local jobErrorHook_t	t_errorHook;

// Prints of workers wait here. The main thread prints them in Jobs_DrainPrints.
static char						s_printQueue[JOBS_PRINT_LINES][JOBS_PRINT_LENGTH];
static int						s_printHead, s_printCount, s_printDropped;
static std::mutex				s_printMutex;

void Jobs_SetThreadErrorHook( jobErrorHook_t hook ) {
	t_errorHook = hook;
}

void Jobs_QueuePrint( const char *text ) {
	std::lock_guard<std::mutex> g( s_printMutex );

	if ( s_printCount >= JOBS_PRINT_LINES ) {
		s_printDropped++;
		return;
	}
	Q_strncpyz( s_printQueue[( s_printHead + s_printCount ) % JOBS_PRINT_LINES], text, JOBS_PRINT_LENGTH );
	s_printCount++;
}

void Jobs_DrainPrints( void ) {
	for ( ;; ) {
		char	line[JOBS_PRINT_LENGTH];
		int		dropped = 0;
		bool	haveLine = false;

		{
			std::lock_guard<std::mutex> g( s_printMutex );

			if ( s_printCount ) {
				Q_strncpyz( line, s_printQueue[s_printHead], sizeof( line ) );
				s_printHead = ( s_printHead + 1 ) % JOBS_PRINT_LINES;
				s_printCount--;
				haveLine = true;
			} else {
				dropped = s_printDropped;
				s_printDropped = 0;
			}
		}
		if ( haveLine ) {
			Com_Printf( "%s", line );
			continue;
		}
		if ( dropped ) {
			Com_Printf( "%d job messages dropped\n", dropped );
		}
		return;
	}
}

// Records the first error of the job and stops the job. The caller of Jobs_ParallelFor raises it after the join.
static void Jobs_RecordError( jobSlot_t *slot, int level, const char *text ) {
	{
		std::lock_guard<std::mutex> g( s_mutex );

		if ( !slot->failed ) {
			slot->failed = true;
			slot->failLevel = level;
			Q_strncpyz( slot->failText, text, sizeof( slot->failText ) );
		}
	}
	slot->abort.store( true );
}

void Jobs_WorkerError( int level, const char *text ) {
	Jobs_RecordError( t_workerSlot, level, text );
	throw jobAbort_s();
}

static void Jobs_RunChunks( jobSlot_t *slot ) {
	for ( ;; ) {
		if ( slot->abort.load( std::memory_order_relaxed ) ) {
			break;
		}
		const int begin = slot->next.fetch_add( slot->chunk );
		if ( begin >= slot->count ) {
			break;
		}
		const int end = ( begin + slot->chunk < slot->count ) ? begin + slot->chunk : slot->count;
		slot->fn( begin, end, slot->ctx );
	}
}

// Call with s_mutex held. A slot with no chunk left is not worth joining.
static jobSlot_t *Jobs_FindOpenSlot( void ) {
	for ( int i = 0; i < JOBS_MAX_SLOTS; i++ ) {
		if ( s_slots[i].open && s_slots[i].next.load() < s_slots[i].count ) {
			return &s_slots[i];
		}
	}
	return NULL;
}

static void Jobs_WorkerMain( void ) {
	t_isWorker = true;

	std::unique_lock<std::mutex> lock( s_mutex );
	for ( ;; ) {
		jobSlot_t *slot = NULL;

		s_wake.wait( lock, [&] { return s_quit || ( slot = Jobs_FindOpenSlot() ) != NULL; } );
		if ( s_quit ) {
			return;
		}
		slot->activeWorkers++;
		lock.unlock();

		t_workerSlot = slot;
		try {
			Jobs_RunChunks( slot );
		} catch ( const jobAbort_s & ) {
		} catch ( ... ) {
			Jobs_RecordError( slot, ERR_DROP, "Exception in a job function" );
		}
		t_workerSlot = NULL;

		lock.lock();
		if ( --slot->activeWorkers == 0 ) {
			s_done.notify_all();
		}
	}
}

static void Jobs_Test_f( void ) {
	enum { N = 100000 };
	static int values[N];
	struct sumCtx_t {
		long long total;
		std::mutex lock;
	} ctx;
	struct fn_t {
		static void Sum( int begin, int end, void *p ) {
			sumCtx_t *c = (sumCtx_t *)p;
			long long part = 0;
			for ( int i = begin; i < end; i++ ) {
				part += values[i];
			}
			std::lock_guard<std::mutex> g( c->lock );
			c->total += part;
		}
	};
	long long serial = 0;

	for ( int i = 0; i < N; i++ ) {
		values[i] = ( i * 7 ) % 1000;
		serial += values[i];
	}
	ctx.total = 0;
	Jobs_ParallelFor( N, 256, fn_t::Sum, &ctx );
	Com_Printf( "jobs_test: %d worker threads, parallel sum %lld, serial sum %lld, %s\n",
		Jobs_NumThreads(), ctx.total, serial, ( ctx.total == serial ) ? "OK" : "MISMATCH" );
}

/*
Counts the CPUs that the process may use. Reads the affinity mask at the time of the call:
an affinity set after this call does not change the pool.
*/
static int Jobs_UsableCpus( const char **source ) {
#ifdef _WIN32
	DWORD_PTR processMask = 0, systemMask = 0;

	if ( GetProcessAffinityMask( GetCurrentProcess(), &processMask, &systemMask ) && processMask ) {
		int count = 0;
		for ( ; processMask; processMask &= processMask - 1 ) {
			count++;
		}
		*source = "process affinity mask";
		return count;
	}
#elif defined(__linux__)
	cpu_set_t set;

	CPU_ZERO( &set );
	if ( sched_getaffinity( 0, sizeof( set ), &set ) == 0 && CPU_COUNT( &set ) > 0 ) {
		*source = "sched_getaffinity";
		return CPU_COUNT( &set );
	}
#endif
	*source = "hardware_concurrency";
	return (int)std::thread::hardware_concurrency();
}

void Jobs_Init( void ) {
	int want;

	Jobs_Shutdown();

	com_jobThreads = Cvar_Get( "com_jobThreads", "-1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cmd_AddCommand( "jobs_test", Jobs_Test_f );

	const char *source = "com_jobThreads";
	want = com_jobThreads->integer;
	if ( want < 0 ) {
		// The main thread and the render thread use two CPUs.
		want = Jobs_UsableCpus( &source ) - 2;
	}
	if ( want < 0 ) {
		want = 0;
	}
	if ( want > JOBS_MAX_THREADS ) {
		want = JOBS_MAX_THREADS;
	}

	s_quit = false;
	for ( int i = 0; i < JOBS_MAX_SLOTS; i++ ) {
		s_slots[i].inUse = false;
		s_slots[i].open = false;
		s_slots[i].activeWorkers = 0;
		s_slots[i].count = 0;
		s_slots[i].next.store( 0 );
		s_slots[i].abort.store( false );
		s_slots[i].failed = false;
	}
	for ( int i = 0; i < want; i++ ) {
		try {
			s_threads.push_back( std::thread( Jobs_WorkerMain ) );
		} catch ( ... ) {
			break;
		}
	}
	Com_Printf( "Job pool: %d worker threads (%s)\n", (int)s_threads.size(), source );
}

void Jobs_Shutdown( void ) {
	if ( s_threads.empty() ) {
		return;
	}
	{
		std::lock_guard<std::mutex> g( s_mutex );
		s_quit = true;
	}
	s_wake.notify_all();
	for ( size_t i = 0; i < s_threads.size(); i++ ) {
		s_threads[i].join();
	}
	s_threads.clear();
}

int Jobs_NumThreads( void ) {
	return (int)s_threads.size();
}

bool Jobs_IsWorkerThread( void ) {
	return t_isWorker;
}

void Jobs_ParallelFor( int count, int grain, jobRangeFn_t fn, void *ctx ) {
	if ( count <= 0 ) {
		return;
	}
	if ( grain < 1 ) {
		grain = 1;
	}

	const int numThreads = (int)s_threads.size();
	if ( numThreads == 0 || t_isWorker || count <= grain ) {
		fn( 0, count, ctx );
		return;
	}

	// About four chunks per thread balances uneven items without much atomic traffic.
	int chunk = count / ( ( numThreads + 1 ) * 4 );
	if ( chunk < grain ) {
		chunk = grain;
	}

	// A caller finds a free slot, or runs inline when every slot is taken.
	jobSlot_t *slot = NULL;
	{
		std::lock_guard<std::mutex> g( s_mutex );
		for ( int i = 0; i < JOBS_MAX_SLOTS; i++ ) {
			if ( !s_slots[i].inUse ) {
				slot = &s_slots[i];
				break;
			}
		}
		if ( slot ) {
			slot->inUse = true;
			slot->fn = fn;
			slot->ctx = ctx;
			slot->count = count;
			slot->chunk = chunk;
			slot->next.store( 0 );
			slot->abort.store( false );
			slot->failed = false;
			slot->open = true;
		}
	}
	if ( !slot ) {
		fn( 0, count, ctx );
		return;
	}
	s_wake.notify_all();

	// Closes the job so late workers skip it, waits for workers still inside fn, then frees the slot.
	// The destructor does the same when a chunk of this thread throws: no worker runs with a dead ctx.
	struct slotGuard_t {
		jobSlot_t	*slot;
		bool		released;
		bool		failed;
		int			failLevel;
		char		failText[1024];

		explicit slotGuard_t( jobSlot_t *s ) : slot( s ), released( false ), failed( false ), failLevel( 0 ) {}
		~slotGuard_t() {
			if ( !released ) {
				slot->abort.store( true );
				Release();
			}
		}
		void Release() {
			std::unique_lock<std::mutex> lock( s_mutex );
			slot->open = false;
			s_done.wait( lock, [this] { return slot->activeWorkers == 0; } );
			failed = slot->failed;
			failLevel = slot->failLevel;
			if ( failed ) {
				Q_strncpyz( failText, slot->failText, sizeof( failText ) );
			}
			slot->inUse = false;
			released = true;
		}
	} guard( slot );

	Jobs_RunChunks( slot );
	guard.Release();

	// A worker raised an error: raise it here, on the thread that owns the job.
	if ( guard.failed ) {
		if ( t_errorHook ) {
			t_errorHook( guard.failLevel, guard.failText );
		}
		Com_Error( guard.failLevel, "%s", guard.failText );
	}
}
