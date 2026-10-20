#include "ai_time.h"
#include "b_local.h"

/*TIMER FOR AI FUNCTIONS */
#if	AI_TIMERS
int GetTime(int lastTime)
{
	int			curtime;
	static int	timeBase = 0;
	static qboolean	initialized = qfalse;

	if (!initialized) {
		timeBase = gi.Milliseconds();
		initialized = qtrue;
	}
	curtime = gi.Milliseconds() - timeBase - lastTime;

	return curtime;
}
#endif