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

// Clone Weapon

#include "cg_headers.h"

#include "cg_local.h"
#include "cg_media.h"
#include "FxScheduler.h"

static vec3_t WHITE = {1.0f, 1.0f, 1.0f};

/*
-------------------------
FX_CloneProjectileThink
-------------------------
*/

void FX_CloneProjectileThink(centity_t *cent, const struct weaponInfo_s *weapon)
{
	vec3_t forward;

	if (cent->currentState.eFlags & EF_USE_ANGLEDELTA)
	{
		AngleVectors(cent->currentState.angles, forward, 0, 0);
	}
	else
	{
		if (VectorNormalize2(cent->gent->s.pos.trDelta, forward) == 0.0f)
		{
			if (VectorNormalize2(cent->currentState.pos.trDelta, forward) == 0.0f)
			{
				forward[2] = 1.0f;
			}
		}
	}

	// hack the scale of the forward vector if we were just fired or bounced...this will shorten up the tail for a split second so tails don't clip so harshly
	int dif = cg.time - cent->gent->s.pos.trTime;

	if (dif < 75)
	{
		if (dif < 0)
		{
			dif = 0;
		}

		float scale = (dif / 75.0f) * 0.95f + 0.05f;

		VectorScale(forward, scale, forward);
	}


	if (weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect)
	{
		theFxScheduler.PlayEffect(weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect, cent->lerpOrigin, forward);
	}
	else
	{
		theFxScheduler.PlayEffect(cgs.effects.cloneShotEffect, cent->lerpOrigin, forward);
	}
}

/*
-------------------------
FX_CloneAltFireThink
-------------------------
*/
void FX_CloneAltFireThink(centity_t *cent, const struct weaponInfo_s *weapon)
{
	FX_CloneProjectileThink(cent, weapon);
}

/*
-------------------------
FX_CloneAltProjectileThink
-------------------------
*/

void FX_CloneAltProjectileThink(centity_t *cent, const struct weaponInfo_s *weapon)
{
	vec3_t forward;

	if (VectorNormalize2(cent->gent->s.pos.trDelta, forward) == 0.0f)
	{
		if (VectorNormalize2(cent->currentState.pos.trDelta, forward) == 0.0f)
		{
			forward[2] = 1.0f;
		}
	}

	// hack the scale of the forward vector if we were just fired or bounced...this will shorten up the tail for a split second so tails don't clip so harshly
	int dif = cg.time - cent->gent->s.pos.trTime;

	if (dif < 75)
	{
		if (dif < 0)
		{
			dif = 0;
		}

		float scale = (dif / 75.0f) * 0.95f + 0.05f;

		VectorScale(forward, scale, forward);
	}

	// see if we have some sort of extra charge going on
	for (int t = 1; t < cent->gent->count; t++)
	{
		// just add ourselves over, and over, and over when we are charged
		theFxScheduler.PlayEffect(cgs.effects.clonePowerupShotEffect, cent->lerpOrigin, forward);
	}

	theFxScheduler.PlayEffect(cgs.effects.cloneShotEffect, cent->lerpOrigin, forward);
}


/*
---------------------------
FX_CloneExplosionProjectileThink
---------------------------
*/
void FX_CloneCommandoProjectileThink(centity_t *cent, const struct weaponInfo_s *weapon)
{
	vec3_t forward;

	if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}

	theFxScheduler.PlayEffect("dc17/shot", cent->lerpOrigin, forward );
}

/*
---------------------------
FX_CloneCommandoSniperShot
---------------------------
*/
void FX_CloneCommandoSniperShot(vec3_t start, vec3_t end)
{
	FX_AddLine(-1, start, end, 0.1f, 10.0f, 0.0f,
							1.0f, 0.0f, 0.0f,
							WHITE, WHITE, 0.0f,
							175, cgi_R_RegisterShader("gfx/effects/blueline"),
							0, FX_SIZE_LINEAR | FX_ALPHA_LINEAR);
}

/*
---------------------------
FX_DisruptorAltMiss
---------------------------
*/

void FX_CloneCommandoSniperMiss(vec3_t origin, vec3_t normal)
{
	vec3_t pos, c1, c2;

	VectorMA(origin, 4.0f, normal, c1);
	VectorCopy(c1, c2);
	c1[2] += 4;
	c2[2] += 12;

	VectorAdd(origin, normal, pos);
	pos[2] += 28;

	FX_AddBezier(origin, pos, c1, vec3_origin, c2, vec3_origin, 6.0f, 6.0f, 0.0f, 0.0f, 0.2f, 0.5f, WHITE, WHITE, 0.0f, 4000, cgi_R_RegisterShader("gfx/effects/smokeTrail"), FX_ALPHA_WAVE);

	theFxScheduler.PlayEffect("clone/wall_impact", origin, normal);
}
