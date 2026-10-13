/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Performance statistics of a renderer, for com_perflog.
//
// The engine gets this table from refexport_t::GetExtension( RD_PERF_API_NAME ). A renderer
// without statistics returns NULL. All calls run on the main thread, between two frames.
//
// The list of entries is open: a renderer adds entries by name and the version stays. A renderer
// with its own render thread gives "rtthread" (1 per frame while the thread runs), "rtwait" (time
// of the main thread in the wait for the previous frame) and "rtsync" (time of the main thread in
// waits for registration and uploads). "backend" is then the time of the render thread.

#pragma once

#include <chrono>

#define RD_PERF_API_NAME	"rdPerf_v1"
#define RD_PERF_API_VERSION	1
#define RD_PERF_MAX			24

typedef enum {
	RD_PERF_MS,			// milliseconds per frame
	RD_PERF_COUNT		// items per frame
} rdPerfUnit_t;

typedef struct {
	const char	*name;
	float		avg;	// mean per frame since the last Collect
	float		max;	// largest value of one frame since the last Collect
	int			unit;	// rdPerfUnit_t
} rdPerfEntry_t;

typedef struct rdPerfAPI_s {
	int		version;	// RD_PERF_API_VERSION

	// Fills at most maxEntries entries with the statistics since the last call, returns how many,
	// then starts a new interval.
	int		(*Collect)( rdPerfEntry_t *out, int maxEntries );
} rdPerfAPI_t;

// Milliseconds on a monotonic clock, with sub-microsecond resolution.
static inline double RD_PerfNowMs( void )
{
	using clock = std::chrono::steady_clock;
	return std::chrono::duration<double, std::milli>( clock::now().time_since_epoch() ).count();
}
