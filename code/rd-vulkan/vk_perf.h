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
	VKP_TIME_COUNT
} vkPerfTime_t;

typedef enum {
	VKP_ENTITIES,		// refEntities of the scenes
	VKP_DLIGHTS,
	VKP_DRAWSURFS,
	VKP_POLYS,
	VKP_GEOMETRY_PCT,	// share of the geometry buffer that the frame used
	VKP_COUNT_COUNT
} vkPerfCount_t;

void	vk_perf_time( int slot, double ms );
void	vk_perf_count( int slot, int value );
void	vk_perf_end_frame( void );
void	*vk_perf_get_api( void );

// Adds the time from construction to destruction to a slot.
struct vk_perf_scope {
	int		slot;
	double	start;

	explicit vk_perf_scope( int s ) : slot( s ), start( RD_PerfNowMs() ) {}
	~vk_perf_scope() { vk_perf_time( slot, RD_PerfNowMs() - start ); }
};
