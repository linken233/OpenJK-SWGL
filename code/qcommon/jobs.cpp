// jobs.cpp -- worker thread pool for data-parallel loops. See jobs.h.

#include "q_shared.h"
#include "qcommon.h"
#include "jobs.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

#define JOBS_MAX_THREADS	16

static cvar_t	*com_jobThreads;

// One job is active at a time. Workers wait on s_wake and claim chunks from s_next.
// All fields except s_next are guarded by s_mutex.
static std::vector<std::thread>	s_threads;
static std::mutex				s_mutex;
static std::condition_variable	s_wake;
static std::condition_variable	s_done;
static std::mutex				s_submitMutex;
static bool						s_quit;
static bool						s_open;			// a job accepts new workers
static unsigned					s_generation;
static int						s_activeWorkers;

static jobRangeFn_t				s_fn;
static void						*s_ctx;
static int						s_count;
static int						s_chunk;
static std::atomic<int>			s_next;

static thread_local bool		t_isWorker;

static void Jobs_RunChunks( void ) {
	for ( ;; ) {
		const int begin = s_next.fetch_add( s_chunk );
		if ( begin >= s_count ) {
			break;
		}
		const int end = ( begin + s_chunk < s_count ) ? begin + s_chunk : s_count;
		s_fn( begin, end, s_ctx );
	}
}

static void Jobs_WorkerMain( void ) {
	unsigned seen = 0;

	t_isWorker = true;

	std::unique_lock<std::mutex> lock( s_mutex );
	for ( ;; ) {
		s_wake.wait( lock, [&] { return s_quit || s_generation != seen; } );
		if ( s_quit ) {
			return;
		}
		seen = s_generation;
		if ( !s_open ) {
			continue;	// the job ended before this worker woke
		}
		s_activeWorkers++;
		lock.unlock();

		Jobs_RunChunks();

		lock.lock();
		if ( --s_activeWorkers == 0 ) {
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

void Jobs_Init( void ) {
	int want;

	Jobs_Shutdown();

	com_jobThreads = Cvar_Get( "com_jobThreads", "-1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cmd_AddCommand( "jobs_test", Jobs_Test_f );

	want = com_jobThreads->integer;
	if ( want < 0 ) {
		want = (int)std::thread::hardware_concurrency() - 2;
	}
	if ( want < 0 ) {
		want = 0;
	}
	if ( want > JOBS_MAX_THREADS ) {
		want = JOBS_MAX_THREADS;
	}

	s_quit = false;
	s_open = false;
	for ( int i = 0; i < want; i++ ) {
		try {
			s_threads.push_back( std::thread( Jobs_WorkerMain ) );
		} catch ( ... ) {
			break;
		}
	}
	Com_Printf( "Job pool: %d worker threads\n", (int)s_threads.size() );
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

	// A second concurrent caller runs inline instead of waiting.
	std::unique_lock<std::mutex> submit( s_submitMutex, std::try_to_lock );
	if ( !submit.owns_lock() ) {
		fn( 0, count, ctx );
		return;
	}

	// About four chunks per thread balances uneven items without much atomic traffic.
	int chunk = count / ( ( numThreads + 1 ) * 4 );
	if ( chunk < grain ) {
		chunk = grain;
	}

	{
		std::lock_guard<std::mutex> g( s_mutex );
		s_fn = fn;
		s_ctx = ctx;
		s_count = count;
		s_chunk = chunk;
		s_next.store( 0 );
		s_open = true;
		s_generation++;
	}
	s_wake.notify_all();

	Jobs_RunChunks();

	// Close the job so late workers skip it, then wait for workers still inside fn.
	std::unique_lock<std::mutex> lock( s_mutex );
	s_open = false;
	s_done.wait( lock, [] { return s_activeWorkers == 0; } );
}
