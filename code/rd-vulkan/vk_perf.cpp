// Frame statistics of the Vulkan renderer, read by com_perflog (rd-common/perf_stats_api.h).

#include "tr_local.h"
#include "vk_perf.h"

typedef struct {
	double	sum;		// over the frames since the last Collect
	double	frame;		// the frame in progress
	double	max;		// largest frame
} perfSlot_t;

static perfSlot_t	perf_time[VKP_TIME_COUNT];
static perfSlot_t	perf_count[VKP_COUNT_COUNT];
static int			perf_frames;

static const char *const perf_time_name[VKP_TIME_COUNT] = {
	"scene", "backend", "fence", "acquire", "submit", "idle", "present"
};

static const char *const perf_count_name[VKP_COUNT_COUNT] = {
	"refents", "dlights", "drawsurfs", "polys", "geom%"
};

void vk_perf_time( int slot, double ms )
{
	perf_time[slot].frame += ms;
}

void vk_perf_count( int slot, int value )
{
	perf_count[slot].frame += value;
}

static void perf_close_slot( perfSlot_t &s )
{
	s.sum += s.frame;
	if ( s.frame > s.max ) {
		s.max = s.frame;
	}
	s.frame = 0.0;
}

// Called at the end of each frame, from RE_EndFrame.
void vk_perf_end_frame( void )
{
	for ( int i = 0; i < VKP_TIME_COUNT; i++ ) {
		perf_close_slot( perf_time[i] );
	}
	for ( int i = 0; i < VKP_COUNT_COUNT; i++ ) {
		perf_close_slot( perf_count[i] );
	}
	perf_frames++;
}

static int vk_perf_collect( rdPerfEntry_t *out, int maxEntries )
{
	const double	n = perf_frames > 0 ? 1.0 / perf_frames : 0.0;
	int				num = 0;

	// The back end contains the waits below it. "record" is what is left: command recording on the CPU.
	const double	waits = perf_time[VKP_FENCE_WAIT].sum + perf_time[VKP_ACQUIRE].sum + perf_time[VKP_SUBMIT].sum
						+ perf_time[VKP_IDLE].sum + perf_time[VKP_PRESENT].sum;

	for ( int i = 0; i < VKP_TIME_COUNT && num < maxEntries; i++ ) {
		out[num].name = perf_time_name[i];
		out[num].avg = (float)( perf_time[i].sum * n );
		out[num].max = (float)perf_time[i].max;
		out[num].unit = RD_PERF_MS;
		num++;
	}

	if ( num < maxEntries ) {
		out[num].name = "record";
		out[num].avg = (float)( ( perf_time[VKP_BACKEND].sum - waits ) * n );
		out[num].max = 0.0f;
		out[num].unit = RD_PERF_MS;
		num++;
	}

	for ( int i = 0; i < VKP_COUNT_COUNT && num < maxEntries; i++ ) {
		out[num].name = perf_count_name[i];
		out[num].avg = (float)( perf_count[i].sum * n );
		out[num].max = (float)perf_count[i].max;
		out[num].unit = RD_PERF_COUNT;
		num++;
	}

	memset( perf_time, 0, sizeof( perf_time ) );
	memset( perf_count, 0, sizeof( perf_count ) );
	perf_frames = 0;

	return num;
}

static rdPerfAPI_t	perf_api = { RD_PERF_API_VERSION, vk_perf_collect };

void *vk_perf_get_api( void )
{
	return &perf_api;
}
