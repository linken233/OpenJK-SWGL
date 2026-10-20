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

// DEMP2 Weapon

#include "cg_headers.h"

#include "cg_media.h"
#include "FxScheduler.h"
#include "FxUtil.h"

/*
---------------------------
FX_DEMP2_ProjectileThink
---------------------------
*/

void FX_DEMP2_ProjectileThink( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}


	if (weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect)
	{
		theFxScheduler.PlayEffect(weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect, cent->lerpOrigin, forward);
	}
	else
	{
		theFxScheduler.PlayEffect("demp2/projectile", cent->lerpOrigin, forward);
	}
}

/*
---------------------------
FX_DEMP2_AltProjectileThink
---------------------------
*/

void FX_DEMP2_AltProjectileThink( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}

	if (weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect)
	{
		theFxScheduler.PlayEffect(weapon->weaponAttacksInfo[cent->gent->attack_index].projectileEffect, cent->lerpOrigin, forward);
	}
	else
	{
		theFxScheduler.PlayEffect("demp2/projectile", cent->lerpOrigin, forward);
	}
}

//---------------------------------------------
void FX_DEMP2_AltDetonate( vec3_t org, float size, gentity_t* gent)
{
	weaponData_t* wpnData = &weaponData[gent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[gent->attack_index];

	qhandle_t detonate_shader;
	qhandle_t detonate_model;

	if(attackData->dempDetonateShader != 0 && attackData->dempDetonateShader[0] != 0)
	{
		detonate_shader = cgi_R_RegisterShader(attackData->dempDetonateShader);
	}
	else {
		detonate_shader = cgi_R_RegisterShader("gfx/effects/demp2shell");
	}

	if(attackData->dempDetonateModel != 0 && attackData->dempDetonateModel[0] != 0)
	{
		detonate_model = cgi_R_RegisterModel(attackData->dempDetonateModel);
	}
	else {
		detonate_model = cgi_R_RegisterModel("models/items/sphere.md3");
	}

	localEntity_t	*ex;

	ex = CG_AllocLocalEntity();
	ex->leType = LE_FADE_SCALE_MODEL;
	memset( &ex->refEntity, 0, sizeof( refEntity_t ));

	ex->refEntity.renderfx |= RF_VOLUMETRIC;

	ex->startTime = cg.time;
	ex->endTime = ex->startTime + 1300;

	ex->radius = size;
	ex->refEntity.customShader = detonate_shader;
	ex->refEntity.hModel = detonate_model;

	VectorCopy( org, ex->refEntity.origin );

	ex->color[0] = ex->color[1] = ex->color[2] = 255.0f;
}
