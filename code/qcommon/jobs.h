// jobs.h -- worker thread pool for data-parallel loops.
//
// Job functions run on worker threads and on the caller thread. They must be thread-safe.
// They must not call Com_Printf, Z_Malloc, Z_Free, Cvar_* or any other engine service.
// com_jobThreads is read once at Jobs_Init. A change applies after a restart.

#ifndef JOBS_H
#define JOBS_H

typedef void ( *jobRangeFn_t )( int begin, int end, void *ctx );

// Start the pool. Reads com_jobThreads: -1 = auto (cores - 2, max 16), 0 = no workers.
void	Jobs_Init( void );

// Stop the pool and join all workers. Safe to call when not initialised.
void	Jobs_Shutdown( void );

// Number of worker threads (the caller thread is not counted). 0 = all work runs inline.
int		Jobs_NumThreads( void );

// True when the calling thread is a pool worker.
bool	Jobs_IsWorkerThread( void );

// Run fn over [0,count) in chunks of at least grain items. Blocks until all chunks end.
// The caller thread executes chunks too. Two callers on different threads can run at once,
// sharing the workers. Runs inline when there are no workers, when the range is small, when
// called from a worker, or when both job slots are taken.
void	Jobs_ParallelFor( int count, int grain, jobRangeFn_t fn, void *ctx );

// Errors and prints of a worker. Com_Error and Com_Printf call these on a worker thread.
// Jobs_WorkerError records the error (first one wins), stops the job and does not return. Jobs_ParallelFor raises it
// on the thread that called it, after all workers left the job: through the hook of that thread, else Com_Error.
// Jobs_QueuePrint keeps a message; the main thread prints the queue in Jobs_DrainPrints.
typedef void ( *jobErrorHook_t )( int level, const char *text );
void	Jobs_SetThreadErrorHook( jobErrorHook_t hook );
#if defined( _MSC_VER )
__declspec( noreturn ) void Jobs_WorkerError( int level, const char *text );
#else
void	Jobs_WorkerError( int level, const char *text ) __attribute__( ( noreturn ) );
#endif
void	Jobs_QueuePrint( const char *text );
void	Jobs_DrainPrints( void );

#endif
