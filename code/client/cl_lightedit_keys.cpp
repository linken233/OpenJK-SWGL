// Light edit mode: key filter. The cgame sees every key event first and consumes only some of them.

#include "../server/exe_headers.h"

#include "client.h"
#include "vmachine.h"

/*
===================
CL_CgameKeyEvent

Offers a key event to the cgame. Returns qtrue when the cgame consumes it.
Nothing is offered while the console or a menu is open, or outside the running game.
===================
*/
qboolean CL_CgameKeyEvent( int key, qboolean down )
{
	int	mods = 0;

	if ( key <= 0 || key >= MAX_KEYS || key >= A_COMBO_BASE || Key_GetCatcher() != 0 )
	{		return qfalse;
	}
	if ( cls.state != CA_ACTIVE || !cls.cgameStarted || CL_IsRunningInGameCinematic() )
	{
		return qfalse;
	}

	if ( kg.keys[A_CTRL].down || kg.keys[A_CTRL2].down )
	{
		mods |= CG_KEYMOD_CTRL;
	}
	if ( kg.keys[A_SHIFT].down || kg.keys[A_SHIFT2].down )
	{
		mods |= CG_KEYMOD_SHIFT;
	}
	if ( kg.keys[A_ALT].down || kg.keys[A_ALT2].down )
	{
		mods |= CG_KEYMOD_ALT;
	}
	if ( down && kg.keys[keynames[key].upper].repeats > 1 )
	{
		mods |= CG_KEYMOD_REPEAT;
	}

	return (qboolean)( VM_Call( CG_KEY_EVENT, (intptr_t)key, (intptr_t)down, (intptr_t)mods ) == 1 );
}
