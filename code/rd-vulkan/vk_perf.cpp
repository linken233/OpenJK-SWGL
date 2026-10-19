// Frame statistics of the Vulkan renderer, read by com_perflog (rd-common/perf_stats_api.h).

#include "tr_local.h"
#include "vk_perf.h"

#include <mutex>

#ifdef _WIN32
#	include <windows.h>
#else
#	include <time.h>
#endif

typedef struct {
	double	sum;		// over the frames since the last Collect
	double	frame;		// the frame in progress
	double	max;		// largest frame
} perfSlot_t;

static perfSlot_t	perf_time[VKP_TIME_COUNT];
static perfSlot_t	perf_count[VKP_COUNT_COUNT];
static int			perf_frames;
static std::mutex	perf_mutex;		// the render thread and the main thread both write the slots

static const char *const perf_time_name[VKP_TIME_COUNT] = {
	"scene", "backend", "fence", "acquire", "submit", "idle", "present", "rtwait", "rtsync",
	"gpu", "gpuspan", "gpuacq", "gpuaccel", "gpurt", "gpurest", "backendcpu"
};

static const char *const perf_count_name[VKP_COUNT_COUNT] = {
	"refents", "dlights", "drawsurfs", "polys", "geom%", "rtthread", "rtinline", "rtsyncleft", "pmode"
};

void vk_perf_time( int slot, double ms )
{
	std::lock_guard<std::mutex> lock( perf_mutex );

	perf_time[slot].frame += ms;
}

void vk_perf_count( int slot, int value )
{
	std::lock_guard<std::mutex> lock( perf_mutex );

	perf_count[slot].frame += value;
}

// Present mode code of the swapchain, added to the counts at the end of each frame.
static int			perf_present_mode;

void vk_perf_set_present_mode( int mode )
{
	perf_present_mode = mode;
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
	std::lock_guard<std::mutex> lock( perf_mutex );

	perf_count[VKP_PRESENT_MODE].frame += perf_present_mode;

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
	std::lock_guard<std::mutex> lock( perf_mutex );

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

// ---- Thread CPU time ----
// Windows reads the cycle counter of the thread and the thread times. The thread times are coarse, so
// they only calibrate cycles per millisecond over a long span. Until then the thread times serve.

static unsigned long long	cpu_total_cycles, cpu_total_time;	// in 100 ns units, under perf_mutex

#ifdef _WIN32
static void cpu_sample( unsigned long long *cycles, unsigned long long *time )
{
	FILETIME	created, exited, kernel, user;
	ULONG64		c = 0;

	QueryThreadCycleTime( GetCurrentThread(), &c );
	GetThreadTimes( GetCurrentThread(), &created, &exited, &kernel, &user );
	*cycles = c;
	*time = ( ( (unsigned long long)kernel.dwHighDateTime << 32 ) | kernel.dwLowDateTime )
		+ ( ( (unsigned long long)user.dwHighDateTime << 32 ) | user.dwLowDateTime );
}
#else
static void cpu_sample( unsigned long long *cycles, unsigned long long *time )
{
	struct timespec	ts;

	clock_gettime( CLOCK_THREAD_CPUTIME_ID, &ts );
	*cycles = 0;
	*time = (unsigned long long)ts.tv_sec * 10000000ULL + (unsigned long long)ts.tv_nsec / 100;
}
#endif

vk_perf_cpu_scope::vk_perf_cpu_scope( int s ) : slot( s )
{
	cpu_sample( &cycles, &time );
}

vk_perf_cpu_scope::~vk_perf_cpu_scope()
{
	unsigned long long	c, t;

	cpu_sample( &c, &t );
	c -= cycles;
	t -= time;

	std::lock_guard<std::mutex> lock( perf_mutex );

	cpu_total_cycles += c;
	cpu_total_time += t;

	if ( cpu_total_cycles > 0 && cpu_total_time >= 5000000ULL ) {
		// 0.5 s of thread time or more: convert the cycles
		perf_time[slot].frame += (double)c * (double)cpu_total_time / (double)cpu_total_cycles * 1e-4;
	} else {
		perf_time[slot].frame += (double)t * 1e-4;
	}
}

// ---- GPU frame time ----
// Each command buffer slot owns VKP_TS_COUNT queries. A frame is read when the fence of the next use
// of its slot is done, so the numbers come two frames late and cost no stall.

static VkQueryPool	gpu_pool;
static double		gpu_period_ms;		// ms per tick
static uint64_t		gpu_valid_mask;
static int			gpu_cur;
static unsigned		gpu_written[NUM_COMMAND_BUFFERS];

void vk_gpu_timer_create( void )
{
	VkPhysicalDeviceProperties	props;
	VkQueueFamilyProperties		families[16];
	uint32_t					count = 16;
	VkQueryPoolCreateInfo		info;

	gpu_pool = VK_NULL_HANDLE;
	Com_Memset( gpu_written, 0, sizeof( gpu_written ) );

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );
	qvkGetPhysicalDeviceQueueFamilyProperties( vk.physical_device, &count, families );

	if ( vk.queue_family_index >= count || !props.limits.timestampComputeAndGraphics
		|| props.limits.timestampPeriod <= 0.0f || families[vk.queue_family_index].timestampValidBits == 0 ) {
		ri.Printf( PRINT_ALL, "GPU timestamps: not supported\n" );
		return;
	}

	const uint32_t bits = families[vk.queue_family_index].timestampValidBits;

	gpu_valid_mask = ( bits >= 64 ) ? ~0ULL : ( ( 1ULL << bits ) - 1 );
	gpu_period_ms = (double)props.limits.timestampPeriod * 1e-6;

	Com_Memset( &info, 0, sizeof( info ) );
	info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	info.queryCount = NUM_COMMAND_BUFFERS * VKP_TS_COUNT;

	if ( qvkCreateQueryPool( vk.device, &info, NULL, &gpu_pool ) != VK_SUCCESS ) {
		gpu_pool = VK_NULL_HANDLE;
		return;
	}
	ri.Printf( PRINT_ALL, "GPU timestamps: %u bits, %.2f ns per tick\n", bits, props.limits.timestampPeriod );
}

void vk_gpu_timer_destroy( void )
{
	if ( gpu_pool != VK_NULL_HANDLE ) {
		qvkDestroyQueryPool( vk.device, gpu_pool, NULL );
		gpu_pool = VK_NULL_HANDLE;
	}
}

// Writes one timestamp in a command buffer. The query is reset just before, so the order of
// execution of the command buffers of a frame does not matter.
void vk_gpu_timer_write( VkCommandBuffer cmd, int stamp, VkPipelineStageFlagBits stage )
{
	if ( gpu_pool == VK_NULL_HANDLE ) {
		return;
	}

	const uint32_t query = gpu_cur * VKP_TS_COUNT + stamp;

	qvkCmdResetQueryPool( cmd, gpu_pool, query, 1 );
	qvkCmdWriteTimestamp( cmd, stage, gpu_pool, query );
	gpu_written[gpu_cur] |= 1u << stamp;
}

static qboolean gpu_read( int index, int stamp, uint64_t *out )
{
	uint64_t	data[2];	// value, availability

	if ( !( gpu_written[index] & ( 1u << stamp ) ) ) {
		return qfalse;
	}
	if ( qvkGetQueryPoolResults( vk.device, gpu_pool, index * VKP_TS_COUNT + stamp, 1, sizeof( data ), data, sizeof( data ),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT ) != VK_SUCCESS || !data[1] ) {
		return qfalse;
	}
	*out = data[0];
	return qtrue;
}

static double gpu_ms( uint64_t from, uint64_t to )
{
	return (double)( ( to - from ) & gpu_valid_mask ) * gpu_period_ms;
}

// Called at the start of a frame, after the wait for the fence of the command buffer slot.
// Reads the timestamps of the previous use of the slot, then selects the slot for the new frame.
void vk_gpu_timer_begin_frame( int index, qboolean fenceWaited )
{
	uint64_t	t[VKP_TS_COUNT];
	qboolean	has[VKP_TS_COUNT];

	if ( gpu_pool == VK_NULL_HANDLE ) {
		return;
	}

	if ( fenceWaited ) {
		for ( int i = 0; i < VKP_TS_COUNT; i++ ) {
			has[i] = gpu_read( index, i, &t[i] );
		}

		if ( has[VKP_TS_MAIN_TOP] && has[VKP_TS_MAIN_COLOR] && has[VKP_TS_MAIN_END] ) {
			const qboolean	rtx = ( has[VKP_TS_START] && has[VKP_TS_ACCEL_END] && has[VKP_TS_RT_END] ) ? qtrue : qfalse;
			const uint64_t	start = has[VKP_TS_START] ? t[VKP_TS_START] : t[VKP_TS_MAIN_TOP];
			const double	span = gpu_ms( start, t[VKP_TS_MAIN_END] );
			const double	acq = gpu_ms( t[VKP_TS_MAIN_TOP], t[VKP_TS_MAIN_COLOR] );
			const double	busy = span > acq ? span - acq : 0.0;

			vk_perf_time( VKP_GPU, busy );
			vk_perf_time( VKP_GPU_SPAN, span );
			vk_perf_time( VKP_GPU_ACQ, acq );

			if ( rtx ) {
				const double accel = gpu_ms( start, t[VKP_TS_ACCEL_END] );
				const double trace = gpu_ms( t[VKP_TS_ACCEL_END], t[VKP_TS_RT_END] );

				vk_perf_time( VKP_GPU_ACCEL, accel );
				vk_perf_time( VKP_GPU_TRACE, trace );
				vk_perf_time( VKP_GPU_REST, busy > accel + trace ? busy - accel - trace : 0.0 );
			}
		}
	}

	gpu_written[index] = 0;
	gpu_cur = index;
}
