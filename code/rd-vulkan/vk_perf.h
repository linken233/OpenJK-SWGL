// Frame statistics of the Vulkan renderer, read by com_perflog (rd-common/perf_stats_api.h).

#pragma once

#include "rd-common/perf_stats_api.h"

typedef enum {
	VKP_SCENE,			// RE_RenderScene: the front end (view setup, culling, draw surface lists)
	VKP_BACKEND,		// RB_ExecuteRenderCommands: the whole back end
	VKP_FENCE_WAIT,		// back end: wait for the fence of the previous use of the command buffer
	VKP_ACQUIRE,		// back end: acquire the swapchain image
	VKP_SUBMIT,			// back end: queue submit
	VKP_IDLE,			// back end: queue wait idle after the frame
	VKP_PRESENT,		// back end: queue present (waits for vsync)
	VKP_RT_WAIT,		// main thread: R_IssueRenderCommands waits for the render thread to finish the previous frame
	VKP_RT_SYNC,		// main thread: R_SyncRenderThread waits for the render thread (registration, uploads, ...)
	VKP_GPU,			// GPU: first to last command of a frame, minus the wait for the swapchain image
	VKP_GPU_SPAN,		// GPU: first to last command of a frame
	VKP_GPU_ACQ,		// GPU: the main command buffer waits for the swapchain image
	VKP_GPU_ACCEL,		// GPU, RTX: first trace command buffer up to the end of the TLAS build
	VKP_GPU_TRACE,		// GPU, RTX: end of the TLAS build up to the end of the denoise and post command buffer
	VKP_GPU_REST,		// GPU, RTX: the rest of VKP_GPU (raster passes, gaps)
	VKP_BACKEND_CPU,	// back end: CPU time of the thread, to compare with VKP_BACKEND
	VKP_TIME_COUNT
} vkPerfTime_t;

typedef enum {
	VKP_ENTITIES,		// refEntities of the scenes
	VKP_DLIGHTS,
	VKP_DRAWSURFS,
	VKP_POLYS,
	VKP_GEOMETRY_PCT,	// share of the geometry buffer that the frame used
	VKP_RT_THREAD,		// 1 for a frame with the render thread on
	VKP_RT_INLINE,		// 1 for a frame whose back end ran on the main thread while the thread runs
	VKP_RT_SYNC_LEFT,	// frames that the render thread still must run on the main thread
	VKP_PRESENT_MODE,	// 0 IMMEDIATE, 1 MAILBOX, 2 FIFO, 3 FIFO_RELAXED, 4 other
	VKP_COUNT_COUNT
} vkPerfCount_t;

void	vk_perf_time( int slot, double ms );
void	vk_perf_count( int slot, int value );
void	vk_perf_end_frame( void );
void	*vk_perf_get_api( void );
void	vk_perf_set_present_mode( int mode );

// Adds the CPU time of the current thread (user + kernel) from construction to destruction to a slot.
struct vk_perf_cpu_scope {
	int			slot;
	unsigned long long	cycles, time;

	explicit vk_perf_cpu_scope( int s );
	~vk_perf_cpu_scope();
};

// The slots are written by the main thread and by the render thread. A lock protects them.
// Adds the time from construction to destruction to a slot.
struct vk_perf_scope {
	int		slot;
	double	start;

	explicit vk_perf_scope( int s ) : slot( s ), start( RD_PerfNowMs() ) {}
	~vk_perf_scope() { vk_perf_time( slot, RD_PerfNowMs() - start ); }
};
