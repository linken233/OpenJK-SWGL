/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// vmachine.h -- virtual machine header for client
#ifndef __VMACHINE_H__
#define __VMACHINE_H__

#include "../qcommon/q_shared.h"

/*
==================================================================

functions exported to the main executable

==================================================================
*/

typedef enum {
	CG_INIT,
	CG_SHUTDOWN,
	CG_CONSOLE_COMMAND,
	CG_DRAW_ACTIVE_FRAME,
	CG_CROSSHAIR_PLAYER,
	CG_CAMERA_POS,
	CG_CAMERA_ANG,
/*
Ghoul2 Insert Start
*/

	CG_RESIZE_G2_BOLT,
	CG_RESIZE_G2,
	CG_RESIZE_G2_BONE,
	CG_RESIZE_G2_SURFACE,
	CG_RESIZE_G2_TEMPBONE,
/*
Ghoul2 Insert End
*/
	CG_DRAW_DATAPAD_HUD,
	CG_DRAW_DATAPAD_OBJECTIVES,
	CG_DRAW_DATAPAD_WEAPONS,
	CG_DRAW_DATAPAD_INVENTORY,
	CG_DRAW_DATAPAD_FORCEPOWERS,
	CG_DRAW_DATAPAD_LOADOUT,
	CG_DRAW_DATAPAD_LOADOUT_FRAME,
	CG_DRAW_WEAPON_SLOT,		// ( int slot, int x, int y, int w, int h ): icon of the weapon of a slot, 1-6 a player weapon, 7 the NPC weapon
	CG_DRAW_CHARACTERS,
	CG_GET_AMMO_NAME,	// ( int ammoIndex, char *name, int nameSize ): qtrue if the player uses this ammo type
	CG_KEY_EVENT		// ( int key, qboolean down, int mods ): 1 when the cgame consumes the key
} cgameExport_t;

// Modifier mask of CG_KEY_EVENT.
#define CG_KEYMOD_CTRL		1
#define CG_KEYMOD_SHIFT		2
#define CG_KEYMOD_ALT		4
#define CG_KEYMOD_REPEAT	8	// a down event that repeats

/*
==============================================================

VIRTUAL MACHINE

==============================================================
*/
typedef struct vm_s {
	// NOTE: arm64 mac has a different calling convention for fixed parameters vs. variadic parameters.
	//       As the cgame entryPoints (vmMain) in jk2 and jka use fixed arg0 to arg7 we can't use "..." around here or we end up with undefined behavior.
	//       See: https://developer.apple.com/documentation/apple-silicon/addressing-architectural-differences-in-your-macos-code
	intptr_t	(*entryPoint)( int callNum, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7 );
} vm_t;

extern vm_t cgvm;

intptr_t	VM_Call( int callnum, ... );
intptr_t	VM_DllSyscall( intptr_t arg, ... );
void		CL_ShutdownCGame( void );

#endif //__VMACHINE_H__
