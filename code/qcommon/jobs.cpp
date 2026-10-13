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
};

// Never freed: a joinable thread at CRT exit would call std::terminate on a fatal-error exit.
static std::vector<std::thread>	&s_threads = *new std::vector<std::thread>;
static std::mutex				s_mutex;
static std::condition_variable	s_wake;
static std::condition_variable	s_done;
static bool						s_quit;
static jobSlot_t				s_slots[JOBS_MAX_SLOTS];

static thread_local bool		t_isWorker;

static void Jobs_RunChunks( jobSlot_t *slot ) {
	for ( ;; ) {
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

		Jobs_RunChunks( slot );

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
	for ( int i = 0; i < JOBS_MAX_SLOTS; i++ ) {
		s_slots[i].inUse = false;
		s_slots[i].open = false;
		s_slots[i].activeWorkers = 0;
		s_slots[i].count = 0;
		s_slots[i].next.store( 0 );
	}
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
			slot->open = true;
		}
	}
	if ( !slot ) {
		fn( 0, count, ctx );
		return;
	}
	s_wake.notify_all();

	Jobs_RunChunks( slot );

	// Close the job so late workers skip it, wait for workers still inside fn, then free the slot.
	std::unique_lock<std::mutex> lock( s_mutex );
	slot->open = false;
	s_done.wait( lock, [slot] { return slot->activeWorkers == 0; } );
	slot->inUse = false;
}
