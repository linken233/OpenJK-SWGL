/*
===========================================================================
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

// Golan Arms Flechette Weapon

#include "cg_headers.h"

#include "cg_media.h"
#include "FxScheduler.h"

/*
-------------------------
FX_FlechetteProjectileThink
-------------------------
*/

void FX_FlechetteProjectileThink( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	EvaluateTrajectoryDelta( &cent->gent->s.pos, cg.time, forward );

	if ( VectorNormalize( forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}

	if (weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect)
	{
		theFxScheduler.PlayEffect(weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect, cent->lerpOrigin, forward);
	}
	else
	{
		theFxScheduler.PlayEffect(cgs.effects.flechetteShotEffect, cent->lerpOrigin, forward);
	}
}

/*
-------------------------
FX_FlechetteProjectileThink
-------------------------
*/

void FX_FlechetteAltProjectileThink( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}

	theFxScheduler.PlayEffect( cgs.effects.flechetteAltShotEffect, cent->lerpOrigin, forward );
}
