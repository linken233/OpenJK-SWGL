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

#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "wp_saber.h"
#include "w_local.h"
#include "../cgame/cg_local.h"

static qboolean is_player_scoped(gentity_t *ent)
{
	return (qboolean)(cg.zoomMode >= ST_A280 && ent->client->ps.clientNum == 0);
}

// The reason why we set alt_fire here is because
// of how "damage" and/or "velocity" will be evaluated
// in WP_FireWeaponMissile.
static void is_player_alt_firing(gentity_t *ent, weapon_t weapon_num, qboolean *alt_fire)
{
	if (ent->client->ps.clientNum == 0)
	{
		// The damage for the high powered shot gets
		// set in bg_pmove so use main damage i.e. set
		// alt_fire to false.
		if (ent->client->ps.firing_attack & TERTIARY_ATTACK
			&& weaponData[weapon_num].tertiaryFireOpt[FIRING_TYPE] == FT_HIGH_POWERED)
		{
			*alt_fire = qfalse;
		}
		// Set alt_fire to true if ALT_ATTACK is enabled (weapons
		// like the clone carbine use ALT_ATTACK) or if scoped.
		else if (ent->client->ps.firing_attack & ALT_ATTACK || cg.zoomMode >= ST_A280)
		{
			*alt_fire = qtrue;
		}
	}
}

// If you have a high powered shot switch
// the MOD to play the disintegration effect and 
// let there be no knockback so the effect can
// fully play out.
static void check_means_of_death(gentity_t *ent, gentity_t *missile, weapon_t weapon_num)
{
	if (weaponData[weapon_num].tertiaryFireOpt[FIRING_TYPE] == FT_HIGH_POWERED
		&& ent->client->ps.tertiaryMode)
	{
		missile->dflags = DAMAGE_NO_KNOCKBACK;
		missile->methodOfDeath = MOD_HIGH_POWERED_SHOT;
	}
}

//---------------
//	F-11D
//---------------

//---------------------------------------------------------
void WP_FireFirstOrderMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = F_11D_VELOCITY;
	int	damage = altFire ? weaponData[WP_THEFIRSTORDER].altDamage : weaponData[WP_THEFIRSTORDER].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= F_11D_NPC_VEL_CUT;
			}
			else
			{
				velocity *= F_11D_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_THEFIRSTORDER;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = F_11D_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = F_11D_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = F_11D_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_BLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BLASTER;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_THEFIRSTORDER);
}

//---------------------------------------------------------
void WP_FireFirstOrder(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;
	qboolean scoped = is_player_scoped(ent);

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_THEFIRSTORDER, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (scoped)
	{
		// angs[PITCH] += Q_flrand(-0.5f, 0.5f) * 0.01;
		// angs[YAW] += Q_flrand(-0.25f, 0.25f) * 0.01;

		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		vectoangles(forwardVec, angs);

		angs[PITCH] += Q_flrand(-1.0f, 1.0f) * F_11D_ALT_SPREAD;
		angs[YAW] += Q_flrand(-1.0f, 1.0f) * F_11D_ALT_SPREAD;
	} 
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * F_11D_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * F_11D_ALT_SPREAD;
		}
		// Troopers use their aim values as well as the gun's inherent inaccuracy
		// so check for all classes of stormtroopers and anyone else that has aim error
		if (ent->client && ent->NPC &&
			(ent->client->NPC_class == CLASS_STORMTROOPER ||
			ent->client->NPC_class == CLASS_SWAMPTROOPER))
		{
			angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (F_11D_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (F_11D_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
		}
		else
		{
			// add some slop to the main-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * F_11D_MAIN_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * F_11D_MAIN_SPREAD;
		}
	}

	if (scoped)
	{
		AngleVectors(angs, forwardVec, NULL, NULL);
		WP_FireFirstOrderMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, alt_fire);
	}
	else
	{
		AngleVectors(angs, dir, NULL, NULL);

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireFirstOrderMissile(ent, muzzle, dir, alt_fire);
	}
}

//---------------
//	DC-15 Carbine
//---------------

//---------------------------------------------------------
void WP_FireCloneCarbineMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = CLONECARBINE_VELOCITY;
	int	damage = altFire ? weaponData[WP_CLONECARBINE].altDamage : weaponData[WP_CLONECARBINE].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= CLONECARBINE_NPC_VEL_CUT;
			}
			else
			{
				velocity *= CLONECARBINE_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "clone_proj";
	missile->s.weapon = WP_CLONECARBINE;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = CLONECARBINE_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = CLONECARBINE_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = CLONECARBINE_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_BLASTER_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BLASTER;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_CLONECARBINE);

	if (CG_IsWeaponPistol(ent))
	{
		if (ent->weaponModel[1] > 0)
		{
			ent->count = (ent->count) ? 0 : 1;
		}
	}
}

//---------------------------------------------------------
void WP_FireCloneCarbine(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_CLONECARBINE, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONECARBINE_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONECARBINE_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if (ent->client && ent->NPC &&
				(ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER))
			{
				angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (CLONECARBINE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));
				angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (CLONECARBINE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONECARBINE_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONECARBINE_MAIN_SPREAD;
			}
		}
	}

	AngleVectors(angs, dir, NULL, NULL);

	// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
	WP_FireCloneCarbineMissile(ent, muzzle, dir, alt_fire);
}

//---------------
//	DC-17
//---------------

static void WP_FireCloneCommandoBeam(gentity_t *ent)
{
	trace_t	tr;
	gentity_t *trace_ent = NULL;
	gentity_t *tent = NULL;

	vec3_t start, end, forward_acc;
	vec3_t dir, spot;

	qboolean render_impact = qtrue;
	qboolean hit_dodged = qfalse;

	int shot_dist = 0;
	int shot_range = 8192;

	// Takes the angle where your eyes are looking at and copy it.
	VectorCopy(ent->client->renderInfo.eyeAngles, forward_acc);

	// Changes the accuracy.
	forward_acc[PITCH] += Q_flrand(-0.15f, 0.15f);
	forward_acc[YAW] += Q_flrand(-0.15f, 0.15f);

	// Copy where your eyes are in space into a varible called start.
	VectorCopy(ent->client->renderInfo.eyePoint, start);
	// Takes the angle where your eyes are looking and converts it into a forward vector.
	AngleVectors(forward_acc, forwardVec, NULL, NULL );

	// Takes the starting point plus the shot_range times the forwardVec.
	// Gets the end vector.
	VectorMA(start, shot_range, forwardVec, end);

	// Shooting a ray and checking where it hits.
	gi.trace(&tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT, G2_COLLIDE, 10);

	// We didn't hit anything.
	if ( tr.surfaceFlags & SURF_NOIMPACT )
	{
		render_impact = qfalse;
	}

	// The ent that you fired on.
	trace_ent = &g_entities[tr.entityNum];

	if (trace_ent && (trace_ent->s.weapon == WP_SABER
		|| (trace_ent->client && (trace_ent->client->NPC_class == CLASS_BOBAFETT
		|| trace_ent->client->NPC_class == CLASS_MANDALORIAN
		|| trace_ent->client->NPC_class == CLASS_JANGO
		|| trace_ent->client->NPC_class == CLASS_REBORN))))
		{
			// Acts like we didn't even hit him.
			hit_dodged = Jedi_DodgeEvasion(trace_ent, ent, &tr, HL_NONE);
		}

	// They didn't dodge and we hit something.
	if (!hit_dodged && render_impact)
	{
		// If the beam hits a NPC.
		if ((tr.entityNum < ENTITYNUM_WORLD && trace_ent->takedamage)
			|| !Q_stricmp(trace_ent->classname, "misc_model_breakable")
			|| trace_ent->s.eType == ET_MOVER)
		{
			G_PlayEffect(G_EffectIndex("clone/flesh_impact"), tr.endpos, tr.plane.normal);

			if (trace_ent->client && LogAccuracyHit(trace_ent, ent))
			{
				ent->client->ps.persistant[PERS_ACCURACY_HITS]++;
			}

			G_Damage(trace_ent, ent, ent, forwardVec, tr.endpos, CLONECOMMANDO_SCOPE_DAMAGE,
					DAMAGE_DEATH_KNOCKBACK, MOD_CLONECOMMANDO, G_GetHitLocFromTrace(&tr, MOD_CLONECOMMANDO));
		}
		// If the beam hits a wall.
		else
		{
			tent = G_TempEntity(tr.endpos, EV_CLONECOMMANDO_SNIPER_MISS);
			tent->svFlags |= SVF_BROADCAST;
			VectorCopy(tr.plane.normal, tent->pos1);
		}
	}

	// Play the beam effect.
	tent = G_TempEntity(tr.endpos, EV_CLONECOMMANDO_SNIPER_SHOT);
	tent->svFlags |= SVF_BROADCAST;
	VectorCopy(muzzle, tent->s.origin2);

	// Getting the difference between you and the ent you hit.
	// Getting the magnitude between you and the ent.
	shot_dist = Distance(tr.endpos, muzzle);

	// As the beam travles, alert the emenies to your location.
	for (float dist = 0; dist < shot_dist; dist += 64)
	{
		VectorMA(muzzle, dist, dir, spot);
		AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
	}

	// If they got spawned right in front you.
	VectorMA(start, shot_dist-4, forwardVec, spot);
	AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
}

//---------------------------------------------------------
void WP_FireCloneCommandoMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int	damage = 0;
	int velocity = altFire ? CLONECOMMANDO_ALT_VELOCITY : CLONECOMMANDO_VELOCITY;
	float kick = (ent->client->usercmd.upmove < 0.0f) ? -100.0f : -300.0f;

	if (cg.zoomMode >= ST_A280)
	{
		damage = CLONECOMMANDO_SCOPE_DAMAGE;
	}
	else if (altFire)
	{
		damage = weaponData[WP_CLONECOMMANDO].altDamage;
	}
	else
	{
		damage = weaponData[WP_CLONECOMMANDO].damage;
	}

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= CLONECOMMANDO_NPC_VEL_CUT;
			}
			else
			{
				velocity *= CLONECOMMANDO_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "clone_proj";
	missile->s.weapon = WP_CLONECOMMANDO;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = CLONECOMMANDO_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = CLONECOMMANDO_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = CLONECOMMANDO_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		VectorSet(missile->maxs, CLONECOMMANDO_ALT_SIZE, CLONECOMMANDO_ALT_SIZE, CLONECOMMANDO_ALT_SIZE);
		VectorScale(missile->maxs, -1, missile->mins);

		missile->s.pos.trType = TR_GRAVITY;
		missile->s.pos.trDelta[2] += 200.0f;
		missile->methodOfDeath = MOD_CLONECOMMANDO_ALT;
		missile->splashDamage = weaponData[WP_CLONECOMMANDO].altSplashDamage;
		missile->splashRadius = weaponData[WP_CLONECOMMANDO].altSplashRadius;

		// Shove us backwards.
		VectorMA(ent->client->ps.velocity, kick, forwardVec, ent->client->ps.velocity);
	}
	else
	{
		missile->methodOfDeath = MOD_CLONECOMMANDO;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
void WP_FireCloneCommando(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	if (cg.zoomMode >= ST_A280 || ent->client->ps.clientNum != 0)
	{
		alt_fire = qfalse;
	}
	else if (ent->client->ps.firing_attack & TERTIARY_ATTACK)
	{
		alt_fire = qtrue;
	}

	vec3_t	dir, angs;

	vectoangles(forwardVec, angs);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (is_player_scoped(ent))
	{
		WP_FireCloneCommandoBeam(ent);
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{
		// Troopers use their aim values as well as the gun's inherent inaccuracy
		// so check for all classes of stormtroopers and anyone else that has aim error
		if (ent->client && ent->NPC &&
			(ent->client->NPC_class == CLASS_STORMTROOPER ||
			ent->client->NPC_class == CLASS_SWAMPTROOPER))
		{
			angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (CLONECOMMANDO_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (CLONECOMMANDO_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
		}
		else
		{
			// add some slop to the main-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CLONECOMMANDO_MAIN_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * CLONECOMMANDO_MAIN_SPREAD;
		}
	}

	if (!(cg.zoomMode >= ST_A280) || ent->client->ps.clientNum != 0)
	{
		AngleVectors(angs, dir, NULL, NULL);

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireCloneCommandoMissile(ent, muzzle, dir, alt_fire);
	}
}

//---------------
//	A280
//---------------

//---------------------------------------------------------
void WP_FireRebelRifleMissile(gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire)
//---------------------------------------------------------
{
	int velocity = REBELRIFLE_VELOCITY;
	int	damage = altFire ? weaponData[WP_REBELRIFLE].altDamage : weaponData[WP_REBELRIFLE].damage;

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
		{
			if (g_spskill->integer < 2)
			{
				velocity *= REBELRIFLE_NPC_VEL_CUT;
			}
			else
			{
				velocity *= REBELRIFLE_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile(start, dir, velocity, 10000, ent, altFire);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_REBELRIFLE;

	// Do the damages
	if (ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer == 0)
		{
			damage = REBELRIFLE_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = REBELRIFLE_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = REBELRIFLE_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;

	if (altFire)
	{
		missile->methodOfDeath = MOD_REBELRIFLE_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_REBELRIFLE;
	}

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_REBELRIFLE);
}

//---------------------------------------------------------
void WP_FireRebelRifle(gentity_t *ent, qboolean alt_fire)
//---------------------------------------------------------
{
	vec3_t	dir, angs;
	qboolean scoped = is_player_scoped(ent);

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_REBELRIFLE, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (scoped)
	{
		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		vectoangles(forwardVec, angs);

		if (ent->client->ps.firing_attack & TERTIARY_ATTACK)
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_TERTIARY_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_TERTIARY_SPREAD;
		}
		else
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_ALT_SPREAD;
		}
	}
	else if (!(ent->client->ps.forcePowersActive&(1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if (alt_fire)
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if (ent->client && ent->NPC &&
				(ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER))
			{
				angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (REBELRIFLE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
				angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (REBELRIFLE_NPC_SPREAD + (6 - ent->NPC->currentAim)*0.25f));//was 0.5f
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * REBELRIFLE_MAIN_SPREAD;
			}
		}
	}

	if (scoped)
	{
		AngleVectors(angs, forwardVec, NULL, NULL);
		WP_FireRebelRifleMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, alt_fire);
	}
	else
	{
		AngleVectors(angs, dir, NULL, NULL);

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireRebelRifleMissile(ent, muzzle, dir, alt_fire);
	}
}

//---------------
//	EE-3 Carbine Rifle
//---------------

//---------------------------------------------------------
void WP_FireBobaRifleMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire )
//---------------------------------------------------------
{
	int velocity = BOBA_VELOCITY;
	int	damage = altFire ? weaponData[WP_BOBA].altDamage : weaponData[WP_BOBA].damage;

	if ( ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE )
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}
	else
	{
		// If an enemy is shooting at us, lower the velocity so you have a chance to evade
		if ( ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO) )
		{
			if ( g_spskill->integer < 2 )
			{
				velocity *= BOBA_NPC_VEL_CUT;
			}
			else
			{
				velocity *= BOBA_NPC_HARD_VEL_CUT;
			}
		}
	}

	WP_TraceSetStart( ent, start, vec3_origin, vec3_origin );//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t *missile = CreateMissile( start, dir, velocity, 10000, ent, altFire );

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_BOBA;

	// Do the damages
	if ( ent->s.number != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO) )
	{
		if ( g_spskill->integer == 0 )
		{
			damage = BOBA_NPC_DAMAGE_EASY;
		}
		else if ( g_spskill->integer == 1 )
		{
			damage = BOBA_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = BOBA_NPC_DAMAGE_HARD;
		}
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_BOBA;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	check_means_of_death(ent, missile, WP_BOBA);
}

//---------------------------------------------------------
void WP_FireBobaRifle( gentity_t *ent, qboolean alt_fire )
//---------------------------------------------------------
{
	vec3_t	dir, angs;
	qboolean scoped = is_player_scoped(ent);

	vectoangles( forwardVec, angs );
	is_player_alt_firing(ent, WP_BOBA, &alt_fire);

	if ( ent->client && ent->client->NPC_class == CLASS_VEHICLE )
	{//no inherent aim screw up
	}
	else if (scoped)
	{
		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		vectoangles(forwardVec, angs);

		if (ent->client->ps.firing_attack & TERTIARY_ATTACK)
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BOBA_TERTIARY_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * BOBA_TERTIARY_SPREAD;
		}
		else
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BOBA_ALT_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * BOBA_ALT_SPREAD;
		}
	}
	else if ( !(ent->client->ps.forcePowersActive&(1<<FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2 )
	{//force sight 2+ gives perfect aim
		//FIXME: maybe force sight level 3 autoaims some?
		if ( alt_fire )
		{
			// add some slop to the alt-fire direction
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BOBA_ALT_SPREAD;
			angs[YAW]	+= Q_flrand(-1.0f, 1.0f) * BOBA_ALT_SPREAD;
		}
		else
		{
			// Troopers use their aim values as well as the gun's inherent inaccuracy
			// so check for all classes of stormtroopers and anyone else that has aim error
			if ( ent->client && ent->NPC &&
				( ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER ) )
			{
				angs[PITCH] += ( Q_flrand(-1.0f, 1.0f) * (BOBA_NPC_SPREAD+(6-ent->NPC->currentAim)*0.25f));//was 0.5f
				angs[YAW]	+= ( Q_flrand(-1.0f, 1.0f) * (BOBA_NPC_SPREAD+(6-ent->NPC->currentAim)*0.25f));//was 0.5f
			}
			else
			{
				// add some slop to the main-fire direction
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * BOBA_MAIN_SPREAD;
				angs[YAW]	+= Q_flrand(-1.0f, 1.0f) * BOBA_MAIN_SPREAD;
			}
		}
	}

	if (scoped)
	{
		AngleVectors(angs, forwardVec, NULL, NULL);
		WP_FireBobaRifleMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, alt_fire);
	}
	else
	{
		AngleVectors( angs, dir, NULL, NULL );

		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireBobaRifleMissile( ent, muzzle, dir, alt_fire );
	}
}

//---------------
//	Super Battle Droid
//---------------
void WP_FireSBD(gentity_t *ent)
{
	vec3_t	angs;
	int velocity = SBD_VELOCITY;
	int damage = SBD_DAMAGE;
	float scalers[2] = {SBD_LEFT_SHOT, SBD_RIGHT_SHOT};

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent->client && ent->client->ps.clientNum != 0)
	{
		if ( g_spskill->integer < 2 )
		{
			velocity *= SBD_NPC_VEL_CUT;
		}
		else
		{
			velocity *= SBD_NPC_HARD_VEL_CUT;
		}
	}

	WP_TraceSetStart(ent, muzzle, vec3_origin, vec3_origin);
	if (ent->client->NPC_class == CLASS_DROIDEKA)
	{
		vectoangles(forwardVec, angs);
		if (ent->count)
			angs[YAW] += -3;
		else
			angs[YAW] += 3;

		AngleVectors(angs, forwardVec, NULL, NULL);

	}

	for (int i = 0; i < 2; i++)
	{
		VectorMA(muzzle, scalers[i], vrightVec, muzzle);

		gentity_t *missile = CreateMissile(muzzle, forwardVec, velocity, 10000, ent, qfalse);

		missile->classname = "blaster_proj";
		missile->s.weapon = WP_SBD;

		// Do the damages
		if (ent->s.number != 0)
		{
			if ( g_spskill->integer == 0 )
			{
				damage = SBD_NPC_DAMAGE_EASY;
			}
			else if ( g_spskill->integer == 1 )
			{
				damage = SBD_NPC_DAMAGE_NORMAL;
			}
			else
			{
				damage = SBD_NPC_DAMAGE_HARD;
			}
		}

		missile->damage = damage;
		missile->dflags = DAMAGE_DEATH_KNOCKBACK;
		missile->methodOfDeath = MOD_SBD;
		missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
		missile->bounceCount = 8;
	}
	if (ent->client->NPC_class == CLASS_DROIDEKA)
	{//dual pistols, toggle the muzzle point back and forth between the two cannons each time
		ent->count = (ent->count) ? 0 : 1;
	}
	
}


//---------------------------------------------------------
// E-5s Sniper Rifle
//---------------------------------------------------------
void WP_FireCISSniper(gentity_t* ent, qboolean alt_fire)
{
	vec3_t	dir, angs;
	qboolean scoped = is_player_scoped(ent);

	vectoangles(forwardVec, angs);
	is_player_alt_firing(ent, WP_CIS_SNIPER, &alt_fire);

	if (ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{//no inherent aim screw up
	}
	else if (scoped)
	{
		// angs[PITCH] += Q_flrand(-0.5f, 0.5f) * 0.01;
		// angs[YAW] += Q_flrand(-0.25f, 0.25f) * 0.01;

		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		vectoangles(forwardVec, angs);
		angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CIS_SNIPER_ALT_SPREAD;
		angs[YAW] += Q_flrand(-1.0f, 1.0f) * CIS_SNIPER_ALT_SPREAD;
	}
	else if (!(ent->client->ps.forcePowersActive & (1 << FP_SEE))
		|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
	{//force sight 2+ gives perfect aim
		// Troopers use their aim values as well as the gun's inherent inaccuracy
		// so check for all classes of stormtroopers and anyone else that has aim error
		if (ent->client && ent->NPC &&
			(ent->client->NPC_class == CLASS_STORMTROOPER ||
				ent->client->NPC_class == CLASS_SWAMPTROOPER))
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CIS_SNIPER_NPC_SPREAD;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * CIS_SNIPER_NPC_SPREAD;
		}
		else
		{
			// add some slop to the main-fire direction if we're not in scope
			if (!scoped)
			{
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * CIS_SNIPER_MAIN_SPREAD;
				angs[YAW] += Q_flrand(-1.0f, 1.0f) * CIS_SNIPER_MAIN_SPREAD;
			}
		}
	}

	if (scoped)
	{
		AngleVectors(angs, forwardVec, NULL, NULL);
		WP_FireCISSniperMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, scoped);
	}
	else
	{
		AngleVectors(angs, dir, NULL, NULL);
		// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
		WP_FireCISSniperMissile(ent, muzzle, dir, scoped);
	}
}

//---------------
//	E-5s Sniper Rifle
//---------------

//---------------------------------------------------------
void WP_FireCISSniperMissile(gentity_t* ent, vec3_t start, vec3_t dir, qboolean scoped)
//---------------------------------------------------------
{
	int velocity = CIS_SNIPER_VELOCITY;;
	int	damage = weaponData[WP_CIS_SNIPER].damage;

	// If an enemy is shooting at us, lower the velocity so you have a chance to evade
	if (ent->client && ent->client->ps.clientNum != 0 && (ent->client->NPC_class != CLASS_BOBAFETT && ent->client->NPC_class != CLASS_MANDALORIAN && ent->client->NPC_class != CLASS_JANGO))
	{
		if (g_spskill->integer < 2)
		{
			//velocity *= CIS_SNIPER_NPC_VEL_CUT;
		}
		else
		{
			//velocity *= CIS_SNIPER_NPC_HARD_VEL_CUT;
		}
	}
	

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = CreateMissile(start, dir, velocity, 10000, ent);

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_CIS_SNIPER;

	if (ent != player)
	{
		// Do the damages
		if (g_spskill->integer == 0)
		{
			damage = CIS_SNIPER_NPC_DAMAGE_EASY;
		}
		else if (g_spskill->integer == 1)
		{
			damage = CIS_SNIPER_NPC_DAMAGE_NORMAL;
		}
		else
		{
			damage = CIS_SNIPER_NPC_DAMAGE_HARD;
		}
	}
	else
	{
		// Being scoped should improve damage
		if (scoped)
		{
			damage = CIS_SNIPER_SCOPE_DAMAGE;
		}
	}	

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_CIS_SNIPER;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}
