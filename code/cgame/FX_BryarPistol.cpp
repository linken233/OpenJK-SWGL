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

// Bryar Pistol Weapon Effects

#include "cg_headers.h"

#include "cg_media.h"
#include "FxScheduler.h"

/*
-------------------------

	MAIN FIRE

-------------------------
FX_BryarProjectileThink
-------------------------
*/
void FX_BryarProjectileThink(  centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->gent->s.pos.trDelta, forward ) == 0.0f )
	{
		if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
		{
			forward[2] = 1.0f;
		}
	}

	// hack the scale of the forward vector if we were just fired or bounced...this will shorten up the tail for a split second so tails don't clip so harshly
	int dif = cg.time - cent->gent->s.pos.trTime;

	if ( dif < 75 )
	{
		if ( dif < 0 )
		{
			dif = 0;
		}

		float scale = ( dif / 75.0f ) * 0.95f + 0.05f;

		VectorScale( forward, scale, forward );
	}
	if (weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect) {
		theFxScheduler.PlayEffect(weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect, cent->lerpOrigin, forward);
	}
	else if ( cent->gent && cent->gent->owner && cent->gent->owner->s.number > 0 )
	{
		theFxScheduler.PlayEffect( "bryar/NPCshot", cent->lerpOrigin, forward );
	}
	else
	{
		theFxScheduler.PlayEffect( cgs.effects.bryarShotEffect, cent->lerpOrigin, forward );
	}
}
/*
-------------------------

	ALT FIRE

-------------------------
FX_BryarAltProjectileThink
-------------------------
*/
void FX_BryarAltProjectileThink(  centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->gent->s.pos.trDelta, forward ) == 0.0f )
	{
		if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
		{
			forward[2] = 1.0f;
		}
	}

	// hack the scale of the forward vector if we were just fired or bounced...this will shorten up the tail for a split second so tails don't clip so harshly
	int dif = cg.time - cent->gent->s.pos.trTime;

	if ( dif < 75 )
	{
		if ( dif < 0 )
		{
			dif = 0;
		}

		float scale = ( dif / 75.0f ) * 0.95f + 0.05f;

		VectorScale( forward, scale, forward );
	}

	// see if we have some sort of extra charge going on
	for ( int t = 1; t < cent->gent->count; t++ )
	{
		// just add ourselves over, and over, and over when we are charged
		if (weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect) {
			theFxScheduler.PlayEffect(weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect, cent->lerpOrigin, forward);
		}
		else {
			theFxScheduler.PlayEffect(cgs.effects.bryarPowerupShotEffect, cent->lerpOrigin, forward);
		}
	}

	if (weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect) {
		theFxScheduler.PlayEffect(weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect, cent->lerpOrigin, forward);
	}
	else {
		theFxScheduler.PlayEffect(cgs.effects.bryarShotEffect, cent->lerpOrigin, forward);
	}
}
