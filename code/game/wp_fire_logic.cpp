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
File for default fire behavior

===========================================================================
*/

#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "wp_saber.h"
#include "w_local.h"
#include "../cgame/cg_local.h"
#include "g_effects.h"


qboolean IsScopedZoom(void)
{
	return (qboolean)(cg.zoomMode >= ST_A280 || cg.zoomMode == ST_DISRUPTOR);
}

qboolean is_player_scoped(gentity_t* ent)
{
	return (qboolean)(IsScopedZoom() && ent->client->ps.clientNum == 0);
}

/*
	Resolve the attack index (0 main, 1 alt, 2 scoped_main, 3 scoped_alt) from the second button.
	This is the only place that maps the two buttons to an attack: the scoped player gets the scoped
	pair, a missing scoped_alt falls back to scoped_main. An attack locked by a burst wins.
*/
int WP_ResolveAttackIndex(gentity_t *gent, qboolean secondaryButton)
{
	if (!gent || !gent->client)
	{
		return secondaryButton ? 1 : 0;
	}
	int attackIndex = secondaryButton ? 1 : 0;
	if (gent->client->ps.clientNum > 0)
	{
		return attackIndex;
	}
	if (gent->client->ps.firing_attack >= 0)
	{
		return gent->client->ps.firing_attack;
	}
	if (IsScopedZoom())
	{
		const weaponData_t &wpn = weaponData[gent->s.weapon];
		if (secondaryButton && wpn.attackData[3].firingLogic != FL_NONE)
		{
			return 3;
		}
		if (wpn.attackData[2].firingLogic != FL_NONE)
		{
			return 2;
		}
	}
	return attackIndex;
}

/*
	Set the method of death based on the weapon
*/
void WP_SetMethodOfDeath(gentity_t *missile,int weaponNum, int attackIndex)
{
	// the MOD_*_ALT variants belong to the odd attacks (alt and scoped_alt)
	qboolean altMod = (qboolean)(attackIndex & 1);
	weaponData_t weapon = weaponData[weaponNum];
	weaponAttackData_t attack = weapon.attackData[attackIndex];
	if (attack.methodOfDeath != MOD_UNKNOWN)
	{
		missile->methodOfDeath = attack.methodOfDeath;
		missile->splashMethodOfDeath = (attack.splashMethodOfDeath != MOD_UNKNOWN) ? attack.splashMethodOfDeath : attack.methodOfDeath;
		return;
	}

	int baseWeapon = weaponData[weaponNum].baseWeaponNum ? weaponData[weaponNum].baseWeaponNum : weaponNum;
	switch (baseWeapon) {
		case WP_BRYAR_PISTOL:
		case WP_BLASTER_PISTOL:
		case WP_TUSKEN_RIFLE:
			missile->methodOfDeath = altMod ? MOD_BRYAR_ALT : MOD_BRYAR;
			missile->splashMethodOfDeath = altMod ? MOD_BRYAR_ALT : MOD_BRYAR;
			return;
		case WP_BOWCASTER:
			missile->methodOfDeath = altMod ? MOD_BOWCASTER_ALT : MOD_BOWCASTER;
			missile->splashMethodOfDeath = altMod ? MOD_BOWCASTER_ALT : MOD_BOWCASTER;
			return;
		case WP_DEMP2:
			missile->methodOfDeath = altMod ? MOD_DEMP2_ALT : MOD_DEMP2;
			missile->splashMethodOfDeath = altMod ? MOD_DEMP2_ALT : MOD_DEMP2;
			return;			
		case WP_REPEATER:
			missile->methodOfDeath = altMod ? MOD_REPEATER_ALT: MOD_REPEATER;
			missile->splashMethodOfDeath = altMod ? MOD_REPEATER_ALT: MOD_REPEATER;
			return;
		case WP_FLECHETTE:
			missile->methodOfDeath = altMod ? MOD_FLECHETTE_ALT : MOD_FLECHETTE;
			missile->splashMethodOfDeath = altMod ? MOD_FLECHETTE_ALT : MOD_FLECHETTE;
			return;
		case WP_NOGHRI_STICK:
			missile->methodOfDeath = MOD_BLASTER ;
			missile->splashMethodOfDeath = MOD_GAS;
			return;
		case WP_DISRUPTOR:
			missile->methodOfDeath = altMod ? MOD_SNIPER : MOD_DISRUPTOR;
			missile->splashMethodOfDeath = altMod ? MOD_SNIPER : MOD_DISRUPTOR;
			return;
		case WP_CONCUSSION:
			missile->methodOfDeath = altMod ? MOD_CONC_ALT : MOD_CONC;
			missile->splashMethodOfDeath = altMod ? MOD_CONC_ALT : MOD_CONC;
			return;
		case WP_ROCKET_LAUNCHER:
			missile->methodOfDeath = altMod ? MOD_ROCKET_ALT : MOD_ROCKET;
			missile->splashMethodOfDeath = altMod ? MOD_ROCKET_ALT : MOD_ROCKET;
			return;
		case WP_THERMAL:
			missile->methodOfDeath = altMod ? MOD_THERMAL_ALT : MOD_THERMAL;
			missile->splashMethodOfDeath = altMod ? MOD_THERMAL_ALT : MOD_THERMAL;
			return;
		case WP_DET_PACK:
			missile->methodOfDeath = MOD_DETPACK;
			missile->splashMethodOfDeath = MOD_DETPACK;
			return;
		case WP_TRIP_MINE:
			missile->methodOfDeath = altMod ? MOD_LASERTRIP_ALT: MOD_LASERTRIP;
			missile->splashMethodOfDeath = altMod ? MOD_LASERTRIP_ALT : MOD_LASERTRIP;
			return;
		case WP_CLONERIFLE:
			missile->methodOfDeath = altMod ? MOD_CLONERIFLE_ALT : MOD_CLONERIFLE;
			missile->splashMethodOfDeath = altMod ? MOD_CLONERIFLE_ALT : MOD_CLONERIFLE;
			return;
		case WP_REBELBLASTER:
			missile->methodOfDeath = altMod ? MOD_REBELBLASTER_ALT : MOD_REBELBLASTER;
			missile->splashMethodOfDeath = altMod ? MOD_REBELBLASTER_ALT : MOD_REBELBLASTER;
			return;
		case WP_CLONECOMMANDO:
			missile->methodOfDeath = altMod ? MOD_CLONECOMMANDO_ALT : MOD_CLONECOMMANDO;
			missile->splashMethodOfDeath = altMod ? MOD_CLONECOMMANDO_ALT : MOD_CLONECOMMANDO;
			return;
		case WP_REBELRIFLE:
			missile->methodOfDeath = altMod ? MOD_REBELRIFLE_ALT : MOD_REBELRIFLE;
			missile->splashMethodOfDeath = altMod ? MOD_REBELRIFLE_ALT : MOD_REBELRIFLE;
			return;
		case WP_REY:
			missile->methodOfDeath = altMod ? MOD_REY_ALT : MOD_REY;
			missile->splashMethodOfDeath = altMod ? MOD_REY_ALT : MOD_REY;
			return;
		case WP_JANGO:
			missile->methodOfDeath = altMod ? MOD_JANGO_ALT : MOD_JANGO;
			missile->splashMethodOfDeath = altMod ? MOD_JANGO_ALT : MOD_JANGO;
			return;
		case WP_BOBA:
			missile->methodOfDeath = altMod ? MOD_BOBA_ALT : MOD_BOBA;
			missile->splashMethodOfDeath = altMod ? MOD_BOBA_ALT : MOD_BOBA;
			return;
		case WP_CLONEPISTOL:
			missile->methodOfDeath = altMod ? MOD_CLONEPISTOL_ALT : MOD_CLONEPISTOL;
			missile->splashMethodOfDeath = altMod ? MOD_CLONEPISTOL_ALT : MOD_CLONEPISTOL;
			return;
		case WP_CIS_SNIPER:
			missile->methodOfDeath = altMod ? MOD_CIS_SNIPER_ALT : MOD_CIS_SNIPER;
			missile->splashMethodOfDeath = altMod ? MOD_CIS_SNIPER_ALT : MOD_CIS_SNIPER;
			return;
		case WP_SBD:
			missile->methodOfDeath = MOD_SBD;
			missile->splashMethodOfDeath = MOD_SBD;
			return;
		case WP_DROIDEKA:
			missile->methodOfDeath = MOD_DROIDEKA;
			missile->splashMethodOfDeath = MOD_DROIDEKA;
			return;
		case WP_BLASTER : 
		default:
			missile->methodOfDeath = altMod ? MOD_BLASTER_ALT : MOD_BLASTER;
			missile->splashMethodOfDeath = altMod ? MOD_BLASTER_ALT : MOD_BLASTER;
	}
}

/*
	Return the weapon missile velocity for NPC
*/
int WP_GetVelocity(gentity_t* ent,weaponAttackData_t * attackData) {
	//Player and those NPC fire like... really fast
	if (ent->s.number == 0
		|| ent->client->NPC_class == CLASS_BOBAFETT
		|| ent->client->NPC_class == CLASS_MANDALORIAN
		|| ent->client->NPC_class == CLASS_JANGO) {
		return attackData->velocity;;
	}
	int diff = g_spskill->integer;
	if (diff < 0)
	{
		//Very slow
		return 100;
	}
	if (diff > 2)
	{
		//Very Fast
		return 1000000;
	}
	if (attackData->npcVelocity[diff] > 0) {
		return attackData->npcVelocity[diff];
	}
	return attackData->velocity;
}

/*
	Return the damage, taking into account Weapon Type, Entity Type & Class + Difficulty
*/
int WP_GetWeaponDamage(gentity_t* ent, weaponAttackData_t* attackData)
{
	int baseWeaponNum = weaponData[ent->s.weapon].baseWeaponNum ? weaponData[ent->s.weapon].baseWeaponNum : ent->s.weapon;
	// Player and those NPC do Normal Damage
	if (ent->s.number == 0
		|| ent->client->NPC_class == CLASS_BOBAFETT
		|| ent->client->NPC_class == CLASS_MANDALORIAN
		|| ent->client->NPC_class == CLASS_JANGO) 
	{
		return attackData->damage;
	}
	int diff = g_spskill->integer;
	if (diff < 0)
	{
		//Almost no damage
		return 1; 
	}
	if (diff > 2)
	{
		//Huge damage
		return 100000;
	}
	if (attackData->npcDamage[diff] >= 0) 
	{
		return attackData->npcDamage[diff];
	}
	//Fallback Case Should not happen
	return (int) (attackData->damage * 0.3f * diff);
}

/*
	Return the spread for NPC if it is found, pc spread otherwise
*/
float WP_GetSpread(gentity_t* ent, weaponAttackData_t *attackData)
{
	//Player and those NPC use normal spread
	if (ent->s.number == 0
		|| ent->client->NPC_class == CLASS_BOBAFETT
		|| ent->client->NPC_class == CLASS_MANDALORIAN
		|| ent->client->NPC_class == CLASS_JANGO) 
	{
		return attackData->spread;
	}
	int diff = g_spskill->integer;
	float spread; 
	if (diff < 0) {
		//Very easy to avoid since spread is huge
		return (attackData->npcSpread[0] < 0 ? attackData->spread : attackData->npcSpread[0]) * (10*(-diff));
	}
	else if (diff > 2)
	{
		//Ultra Precision
		return 0;
	}
	spread = attackData->npcSpread[diff];
	if (spread < 0)
	{// Unset for this difficulty
		return attackData->spread;
	}
	return spread;
}

void WP_ApplyLockDownOnMissile(gentity_t* ent, gentity_t* missile, qboolean alwaysLock = qfalse) 
{
	int	lockEntNum, lockTime;
	if (ent->NPC && ent->enemy)
	{
		lockEntNum = ent->enemy->s.number;
		lockTime = Q_irand(600, 1200);
	}
	else
	{
		lockEntNum = g_rocketLockEntNum;
		lockTime = g_rocketLockTime;
	}
	// we'll consider attempting to lock this little poochie onto some baddie.
	if ((lockEntNum > 0 || (ent->NPC && lockEntNum >= 0)) && lockEntNum < ENTITYNUM_WORLD && lockTime > 0)
	{
		// take our current lock time and divide that by 8 wedge slices to get the current lock amount
		int dif = (level.time - lockTime) / (1200.0f / 8.0f);

		if (dif < 0)
		{
			dif = 0;
		}
		else if (dif > 8)
		{
			dif = 8;
		}

		// if we are fully locked, always take on the enemy.
		//	Also give a slight advantage to higher, but not quite full charges.
		//	Finally, just give any amount of charge a very slight random chance of locking.
		if (dif == 8 || Q_flrand(0.0f, 1.0f) * dif > 2 || Q_flrand(0.0f, 1.0f) > 0.97f || alwaysLock)
		{
			missile->enemy = &g_entities[lockEntNum];

			if (missile->enemy
				&& missile->enemy->inuse)//&& DistanceSquared( missile->currentOrigin, missile->enemy->currentOrigin ) < 262144 && InFOV( missile->currentOrigin, missile->enemy->currentOrigin, missile->enemy->client->ps.viewangles, 45, 45 ) )
			{
				if (missile->enemy->client
					&& (missile->enemy->client->ps.forcePowersKnown & (1 << FP_PUSH))
					&& missile->enemy->client->ps.forcePowerLevel[FP_PUSH] > FORCE_LEVEL_0)
				{//have force push, don't flee from homing rockets
				}
				else
				{
					vec3_t dir, dir2;

					AngleVectors(missile->enemy->currentAngles, dir, NULL, NULL);
					AngleVectors(ent->client->renderInfo.eyeAngles, dir2, NULL, NULL);

					if (DotProduct(dir, dir2) < 0.0f)
					{
						G_StartFlee(missile->enemy, ent, missile->enemy->currentOrigin, AEL_DANGER_GREAT, 3000, 5000);
						if (!TIMER_Done(missile->enemy, "flee"))
						{
							TIMER_Set(missile->enemy, "rocketChasing", 500);
						}
					}
				}
			}
		}
	}

	VectorCopy(forwardVec, missile->movedir);

	missile->e_ThinkFunc = thinkF_WP_RocketThink;
	missile->random = 1.0f;
	missile->nextthink = level.time + 100;
}
//---------------------------------------------------------
void WP_FireGenericBlasterMissile(gentity_t* ent, vec3_t start, vec3_t dir,int attackIndex,int forcedVelocity = -1)
// This method fire a missile typical of a blaster, default weapons includes : 
//  - Blaster Pistol + Charged Blaster Pistol (and all variant)
//  - Blaster (Incl. Rapid fire)
//  - Reapeater Main fire
//  - BowCaster Alt Fire
// 
// Use Attack Data Firing Logic (Grenade Launcher) to determine if it sould be affected by gravity
// Use attack Data bounceWall to determine if it should bounce on wall
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	int velocity = WP_GetVelocity(ent, attackData);
	int	damage = WP_GetWeaponDamage(ent, attackData);

	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		damage *= 3;
		velocity = ATST_MAIN_VEL + ent->client->ps.speed;
	}

	WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);//make sure our start point isn't on the other side of a wall

	WP_MissileTargetHint(ent, start, dir);

	gentity_t* missile = CreateMissile(start, dir, velocity, 10000, ent, attackIndex);

	//ClassName Seems to be unimportant, so left as "blaster_proj" for performance reason
	missile->classname = "blaster_proj";
	missile->s.weapon = ent->s.weapon;


	//If charged attack
	if (attackData->firingLogic == FL_BLASTER_CHARGED || attackData->firingLogic == FL_DEMP2_ALT)
	{
		int count = (level.time - ent->client->ps.weaponChargeTime) / attackData->chargeUnitTime;

		if (count < 1)
		{
			count = 1;
		}
		else if (count > attackData->maxChargeUnits)
		{
			count = attackData->maxChargeUnits;
		}

		if (attackData->firingLogic == FL_DEMP2_ALT)
		{
			damage *= (1 + (count * (count - 1)));
			missile->e_ThinkFunc = thinkF_DEMP2_AltDetonate;
		}
		else
		{
			damage *= count;
		}
		missile->count = count; // this will get used in the projectile rendering code to make a beefier effect
	}

	//If Grenade Launcher
	else if (attackData->firingLogic == FL_GRENADE_LAUNCHER) {
		missile->s.pos.trType = TR_GRAVITY;
		missile->s.pos.trDelta[2] += 40.0f;
	}
	else if (attackData->firingLogic == FL_MISSILE) {
		/* Reduce damage further for "small missiles" */
		if (ent->client && (ent->client->NPC_class == CLASS_BOBAFETT || ent->client->NPC_class == CLASS_MANDALORIAN || ent->client->NPC_class == CLASS_JANGO))
		{
			damage = damage / 2;
		}
	}
	else if (attackData->firingLogic == FL_MISSILE_AIMED) {
		WP_ApplyLockDownOnMissile(ent, missile);
	}

	if (attackData->missileSize > 0) {
		// Make it easier to hit things
		VectorSet(missile->maxs, attackData->missileSize, attackData->missileSize, attackData->missileSize);
		VectorScale(missile->maxs, -1, missile->mins);
	}
	if (attackData->missileMass > 0) {
		missile->mass = attackData->missileMass;
	}
	if (attackData->missileDFlags > 0) {
		missile->dflags = attackData->missileDFlags;
	}
	else if (attackData->firingLogic == FL_NOGHRI) {
		missile->dflags |= DAMAGE_NO_KNOCKBACK;
	}
	else {
		missile->dflags |= DAMAGE_DEATH_KNOCKBACK;
	}

	WP_SetMethodOfDeath(missile, ent->s.weapon, attackIndex);
	missile->damage = damage;
	missile->splashDamage = attackData->splashDamage;
	missile->splashRadius = attackData->splashRadius;

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	if (attackData->bounceWall) {
		missile->s.eFlags |= EF_BOUNCE;
	}
	missile->bounceCount = attackData->bounceCount;
}

//---------------------------------------------------------
void WP_FireGenericBlaster(gentity_t* ent, int attackIndex)
// This method handle the firing of generic blasters, see Previous WP_FireGenericBlasterMissile for more information
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	float spread = WP_GetSpread(ent, attackData);
	vec3_t	dir, angs;

	
	vectoangles(forwardVec, angs);
	/* Calculate Spread, If we are a vehicle or we have Sense 2, no spread*/
	if (!ent->client || (
		ent->client->NPC_class != CLASS_VEHICLE && (
			!(ent->client->ps.forcePowersActive & (1 << FP_SEE)) || ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2)
		))
	{
		// Some NPCs really can't aim
		if (ent->client && ent->NPC && ( ent->client->NPC_class == CLASS_STORMTROOPER 
			|| ent->client->NPC_class == CLASS_SWAMPTROOPER  || ent->client->NPC_class == CLASS_IMPWORKER)
		)
		{
			angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (spread + (6 - ent->NPC->currentAim) * 0.25f));
			angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (spread + (6 - ent->NPC->currentAim) * 0.25f));
		}
		else
		{
			angs[PITCH] += Q_flrand(-1.0f, 1.0f) * spread;
			angs[YAW] += Q_flrand(-1.0f, 1.0f) * spread;
		}
	}

	if (is_player_scoped(ent)) {
		AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		WP_FireGenericBlasterMissile(ent, ent->client->renderInfo.eyePoint, forwardVec, attackIndex);
	}
	else {
		AngleVectors(angs, dir, NULL, NULL);
		WP_FireGenericBlasterMissile(ent, muzzle, dir, attackIndex);
		//If it's a charged attack with dual pistol, fire a second projectile at muzzle 2
		if (attackData->firingLogic == FL_BLASTER_CHARGED && wpnData->weaponCategory == WC_PISTOL && ent->weaponModel[1] > 0)
		{
			WP_FireGenericBlasterMissile(ent, muzzle2, dir, attackIndex);
		}
	}

	WP_SwitchPistolMuzzle(ent);
}


//---------------------------------------------------------
void WP_FireFlameThrower(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	WP_FlameThrowerBurn(ent, &weaponData[ent->s.weapon].attackData[attackIndex], muzzle, forwardVec);
}

//---------------------------------------------------------
void WP_FlameThrowerBurn(gentity_t* ent, const weaponAttackData_t* attackData, const vec3_t muzzlePoint, const vec3_t aimDir)
// Sets on fire what is in the flame in front of the muzzle. Used by the flamethrower weapons and by the wrist flamethrower item.
//---------------------------------------------------------
{
	trace_t		tr;
	vec3_t	dir, start;
	float range = attackData->range;
	int duration = attackData->effectDuration > 0 ? attackData->effectDuration: 1000; // Default to 1 second if not specified
	int	damage = attackData->damage;

	//Get normalized direction in dir
	VectorCopy(aimDir, dir);
	VectorNormalizeFast(dir);

	//Init start point
	VectorCopy(muzzlePoint, start);


	vec3_t mins, maxs;
	for (int i = 0; i < 3; i++)
	{
		mins[i] = start[i] - range;
		maxs[i] = start[i] + range;
	}
	gentity_t* entityList[MAX_GENTITIES];
	int numListedEntities = gi.EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);
	
	for(int i = 0 ; i < numListedEntities; i++){
		gentity_t *target = entityList[i];

		if (target->s.clientNum == ent->s.clientNum) {
			//Don't target oneself
			continue;
		}

		//Aim for somewhere in the torso
		vec3_t hitloc;
		VectorCopy(target->currentOrigin, hitloc);
		hitloc[2] += 25.0f;
		

        // Is the target in front of us?
		vec3_t target_dir;
		VectorSubtract(hitloc, start, target_dir);

		float dist = VectorLength(target_dir);

		VectorNormalizeFast(target_dir);
		if (DotProduct(target_dir, dir) < 0.80f && dist > 20.0f) {
			continue; // We are not "in front" of the muzzle, We also are not directly next to the muzzle.
		}

		gi.trace(&tr, start, vec3_origin, vec3_origin, hitloc, ent->s.number, MASK_FORCE_PUSH, (EG2_Collision)0, 0);//was MASK_SHOT, but changed to match above trace and crosshair trace
		if (tr.fraction < 1.0f && tr.entityNum != target->s.number)
		{//must have clear LOS
			continue;
		}

		if (target->takedamage)
		{
			//G_Damage(target, ent, ent, target_dir, hitloc, damage, DAMAGE_NO_ARMOR | DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC | DAMAGE_IGNORE_TEAM, MOD_LAVA, HL_NONE);
			activeEffect_t effect;
			effect.effectType= ET_FIRE;
			effect.endTime = level.time + duration;
			effect.lastApplied = level.time;
			effect.parameter = (float)damage;
			effect.inflictorIndex = ent->s.number;
			if (attackData->hitFleshEffect[0])
			{
				effect.effectIndex = G_EffectIndex(attackData->hitFleshEffect);
			}
			else
			{
				effect.effectIndex = -1;
			}

			G_startEffect(target, &effect);
		}
	}
}
//---------------------------------------------------------
void WP_FireGenericBowcaster(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	vec3_t	dir, angs, start;
	gentity_t* missile;
	float spread = WP_GetSpread(ent, attackData);
	int vel,wpnCount = 1,count;

	count = (level.time - ent->client->ps.weaponChargeTime) / attackData->chargeUnitTime;

	if (count < 1)
	{
		count = 1;
	}
	else if (count > attackData->maxChargeUnits)
	{
		count = attackData->maxChargeUnits;
	}

	if (!(count & 1))
	{
		// if we aren't odd, knock us down a level
		count--;
	}

	//Check if we have a twinned shot
	if (attackData->firingLogic == FL_BOWCASTER && wpnData->weaponCategory == WC_PISTOL && ent->weaponModel[1] > 0) {
		wpnCount = 2;
	}

	//Prepare the first shot
	VectorCopy(muzzle, start);
	for (int j = 0; j < wpnCount; j++)
	{
		//make sure our start point isn't on the other side of a wall
		WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);
		WP_MissileTargetHint(ent, start, forwardVec);
		for (int i = 0; i < count; i++)
		{
			// create a range of different velocities
			//DWS-TODO : It might be possible to set a proprety to replace the 0.3f and configure it by weapons.
			vel = attackData->velocity * (Q_flrand(-1.0f, 1.0f) * 0.3f + 1.0f);

			vectoangles(forwardVec, angs);

			if (!(ent->client->ps.forcePowersActive & (1 << FP_SEE))
				|| ent->client->ps.forcePowerLevel[FP_SEE] < FORCE_LEVEL_2
				)
			{
				angs[PITCH] += Q_flrand(-1.0f, 1.0f) * attackData->spread * 0.2f;
				angs[YAW] += ((i + 0.5f) * attackData->spread - count * 0.5f * attackData->spread);
				if (ent->NPC)
				{
					angs[PITCH] += (Q_flrand(-1.0f, 1.0f) * (spread + (6 - ent->NPC->currentAim) * 0.25f));
					angs[YAW] += (Q_flrand(-1.0f, 1.0f) * (spread + (6 - ent->NPC->currentAim) * 0.25f));
				}
			}

			AngleVectors(angs, dir, NULL, NULL);


			missile = CreateMissile(start, dir, vel, 10000, ent);

			missile->classname = "bowcaster_proj";
			missile->s.weapon = ent->s.weapon;

			if (attackData->missileSize) {
				// Make it easier to hit things
				VectorSet(missile->maxs, attackData->missileSize, attackData->missileSize, attackData->missileSize);
				VectorScale(missile->maxs, -1, missile->mins);
			}


			WP_SetMethodOfDeath(missile, ent->s.weapon, attackIndex);
			missile->damage = WP_GetWeaponDamage(ent, attackData);
			missile->dflags = DAMAGE_DEATH_KNOCKBACK;
			missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
			missile->splashDamage = attackData->splashDamage;
			missile->splashRadius = attackData->splashRadius;


			if (attackData->bounceWall) {
				missile->s.eFlags |= EF_BOUNCE;
			}
			missile->bounceCount = attackData->bounceCount;

			ent->client->sess.missionStats.shotsFired++;
		}
		//Prepare the twinned projectile
		VectorCopy(muzzle2, start);
	}
	WP_SwitchPistolMuzzle(ent);
}


//DWS-TODO : MOD_XXXX still hardcoded
//DWS-TODO : Concussion normally have "push back" effect that is not found here. Commit with file deletion : b4b8b395348774ea41461dbcf310fc254a0bf293
//---------------------------------------------------------
void WP_FireGenericBeam(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	int			damage = attackData->damage, skip,traces = 10;	// traces : max loop turns, dodged shots included
	int			maxHits = 1, hits = 0;							// maxHits : entities the shot goes through
	// Only the plain beam (main shot) stops at the first target and pushes the victim
	const qboolean	piercing = (qboolean)(attackData->firingLogic != FL_BEAM);
	const int	dflags = piercing ? (DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC) : DAMAGE_DEATH_KNOCKBACK;

	if (attackData->firingLogic == FL_FULL_BEAM)
	{
		traces = maxHits = DISRUPTOR_ALT_TRACES;
	}
	qboolean	render_impact = qtrue;
	vec3_t		start, end;
	vec3_t		spot, dir;
	trace_t		tr;
	gentity_t* traceEnt, * tent;
	float		dist, shotDist, shotRange = 8192;
	qboolean	hitDodged = qfalse, fullCharge = qfalse;

	// The trace start will originate at the eye so we can ensure that it hits the crosshair.
	if (ent->s.number != 0)
	{
		VectorCopy(muzzle, start);
		if (attackData->firingLogic == FL_BEAM_CHARGED)
		{
			fullCharge = qtrue;
			traces = maxHits = DISRUPTOR_ALT_TRACES;
		}
		//DWS-TODO : Need to do something else here to remove the Disruptor NPC damage constant..
		damage = WP_GetWeaponDamage(ent, attackData);
	}
	else if(attackData->firingLogic == FL_BEAM_CHARGED)
	{

		int count = (level.time - ent->client->ps.weaponChargeTime - 50) / attackData->chargeUnitTime;

		if (count < 1)
		{
			count = 1;
		}
		else if (count >= attackData->maxChargeUnits)
		{
			count = attackData->maxChargeUnits;
			fullCharge = qtrue;
		}

		// more powerful charges go through more things
		if (count < 3)
		{
			traces = maxHits = 1;
		}
		else if (count < 6)
		{
			traces = maxHits = 2;
		}
		else {
			traces = maxHits = DISRUPTOR_ALT_TRACES;
		}

		damage = damage * count + attackData->damage * 0.5f; // give a boost to low charge shots
	}

	if (!ent->NPC) {
		if (is_player_scoped(ent)) {
			VectorCopy(ent->client->renderInfo.eyePoint, start);
			AngleVectors(ent->client->renderInfo.eyeAngles, forwardVec, NULL, NULL);
		}
		else {
			VectorCopy(muzzle, start);
		}
		WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);
		WP_MissileTargetHint(ent, start, forwardVec);
	}

	skip = ent->s.number;

	// Push the shooter backwards
	if (attackData->selfKnockback && ent->client)
	{
		VectorMA(ent->client->ps.velocity, -attackData->selfKnockback, forwardVec, ent->client->ps.velocity);
		ent->client->ps.groundEntityNum = ENTITYNUM_NONE;
		ent->client->ps.pm_time = (ent->client->ps.pm_flags & PMF_DUCKED) ? 100 : 250;
		ent->client->ps.pm_flags |= PMF_TIME_KNOCKBACK | PMF_TIME_NOFRICTION;
	}

	// Means of death from the weapon data, else the disruptor ones
	const meansOfDeath_t beamMod = (attackData->methodOfDeath != MOD_UNKNOWN) ? attackData->methodOfDeath
		: (fullCharge ? MOD_SNIPER : MOD_DISRUPTOR);

	vec3_t shot_mins, shot_maxs;
	VectorSet(shot_maxs, attackData->beamRadius, attackData->beamRadius, attackData->beamRadius);
	VectorScale(shot_maxs, -1, shot_mins);

	for (int i = 0; i < traces; i++)
	{
		VectorMA(start, shotRange, forwardVec, end);

		if (attackData->beamRadius > 0)
		{
			gi.trace(&tr, start, shot_mins, shot_maxs, end, skip, MASK_SHOT, G2_COLLIDE, 10);
		}
		else
		{
			gi.trace(&tr, start, NULL, NULL, end, skip, MASK_SHOT, G2_COLLIDE, 10);
		}

		if (tr.surfaceFlags & SURF_NOIMPACT)
		{
			render_impact = qfalse;
		}

		if (tr.entityNum == ent->s.number)
		{
			// should never happen, but basically we don't want to consider a hit to ourselves?
			// Get ready for an attempt to trace through another person
			VectorCopy(tr.endpos, muzzle2);
			VectorCopy(tr.endpos, start);
			skip = tr.entityNum;
			continue;
		}

		if (tr.fraction >= 1.0f)
		{
			// draw the beam but don't do anything els
			break;
		}

		traceEnt = &g_entities[tr.entityNum];

		if (traceEnt && (traceEnt->s.weapon == WP_SABER || (traceEnt->client 
			&& (
				traceEnt->client->NPC_class == CLASS_BOBAFETT 
					|| traceEnt->client->NPC_class == CLASS_MANDALORIAN 
					|| traceEnt->client->NPC_class == CLASS_JANGO
					|| traceEnt->client->NPC_class == CLASS_REBORN)
			))
		)
		{
			hitDodged = Jedi_DodgeEvasion(traceEnt, ent, &tr, HL_NONE);
			//acts like we didn't even hit him
		}
		if (!hitDodged)
		{
			if (render_impact)
			{
				if ((tr.entityNum < ENTITYNUM_WORLD && traceEnt->takedamage)
					|| !Q_stricmp(traceEnt->classname, "misc_model_breakable")
					|| traceEnt->s.eType == ET_MOVER)
				{
					// Create a simple impact type mark that doesn't last long in the world
					int effect;
					if (attackData->hitFleshEffect[0]) {
						effect = G_EffectIndex(attackData->hitFleshEffect);
					}
					else {
						effect = G_EffectIndex("disruptor/alt_hit");
					}

					G_PlayEffect(effect, tr.endpos, tr.plane.normal);

					if (traceEnt->client && LogAccuracyHit(traceEnt, ent))
					{//NOTE: hitting multiple ents can still get you over 100% accuracy
						ent->client->ps.persistant[PERS_ACCURACY_HITS]++;
					}

					int hitLoc = G_GetHitLocFromTrace(&tr, beamMod);
					// Must be read before the damage: the flag is set when the victim dies
					const qboolean noKnockBack = (qboolean)((traceEnt->flags & FL_NO_KNOCKBACK) != 0);
					G_Damage(traceEnt, ent, ent, forwardVec, tr.endpos, damage, dflags, beamMod, hitLoc);

					// Push and knockdown are done by hand, only on clients
					if (traceEnt->client && (attackData->pushForce || attackData->knockdownForce))
					{
						vec3_t pushDir;
						VectorCopy(forwardVec, pushDir);
						if (pushDir[2] < 0.2f)
						{
							pushDir[2] = 0.2f;
						}
						if (attackData->pushForce && !noKnockBack)
						{
							G_Throw(traceEnt, pushDir, attackData->pushForce);
							if (traceEnt->client->NPC_class == CLASS_ROCKETTROOPER)
							{
								traceEnt->client->ps.pm_time = Q_irand(1500, 3000);
							}
						}
						if (attackData->knockdownForce && traceEnt->health > 0 && G_HasKnockdownAnims(traceEnt))
						{
							G_Knockdown(traceEnt, ent, pushDir, attackData->knockdownForce, qtrue);
						}
					}
					if (traceEnt->s.eType == ET_MOVER || ++hits >= maxHits)
					{
						break;
					}
				}
				else
				{
					// we only make this mark on things that can't break or move
					tent = G_TempEntity(tr.endpos, EV_DISRUPTOR_SNIPER_MISS);
					tent->svFlags |= SVF_BROADCAST;
					VectorCopy(tr.plane.normal, tent->pos1);
					break; // hit solid, but doesn't take damage, so stop the shot...we _could_ allow it to shoot through walls, might be cool?
				}
			}
			else // not rendering impact, must be a skybox or other similar thing?
			{
				break; // don't try anymore traces
			}
		}
		VectorCopy(tr.endpos, start);
		skip = tr.entityNum;
		hitDodged = qfalse;
	}
	//just draw one solid beam all the way to the end...
	tent = G_TempEntity(tr.endpos,EV_GENERIC_BEAM);
	tent->svFlags |= SVF_BROADCAST;
	tent->count = fullCharge; // mark us so we can alter the effect
	tent->attack_index = attackIndex;
	tent->s.weapon = ent->s.weapon;

	VectorCopy(muzzle, tent->s.origin2);

	// now go along the trail and make sight events
	VectorSubtract(tr.endpos, muzzle, dir);

	shotDist = VectorNormalize(dir);

	//FIXME: if shoot *really* close to someone, the alert could be way out of their FOV
	for (dist = 0; dist < shotDist; dist += 64)
	{
		VectorMA(muzzle, dist, dir, spot);
		AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);
		if (attackData->beamTrailEffect[0])
		{
			G_PlayEffect(G_EffectIndex(attackData->beamTrailEffect), spot, forwardVec);
		}
	}
	//FIXME: spawn a temp ent that continuously spawns sight alerts here?  And 1 sound alert to draw their attention?
	VectorMA(start, shotDist - 4, forwardVec, spot);
	AddSightEvent(ent, spot, 256, AEL_DISCOVERED, 50);


	WP_SwitchPistolMuzzle(ent);
}

//---------------
//	Super Battle Droid & Droideka
//---------------
void WP_FireDroidsTwinBlasters(gentity_t* ent, int attackIndex)
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	vec3_t	angs;
	int velocity = WP_GetVelocity(ent, attackData);
	int damage = WP_GetWeaponDamage(ent, attackData);
	float scalers[2] = { SBD_LEFT_SHOT, SBD_RIGHT_SHOT };

	WP_TraceSetStart(ent, muzzle, vec3_origin, vec3_origin);

	if (ent->client && ent->client->NPC_class == CLASS_DROIDEKA)
	{
		if (NPC && NPC->enemy)
		{
			float enemyDist = DistanceSquared(NPC->currentOrigin, NPC->enemy->currentOrigin);
			vectoangles(forwardVec, angs);
			float baseAngle = 3.0f;
			float baseDist = 66596.9531f;

			float angle = baseAngle * (baseDist / enemyDist);
			if (angle > 14)
				angle = 14.0f;
			if (angle < 1)
				angle = 1.0f;

			if (ent->count)
				angs[YAW] += (angle * -1);
			else
				angs[YAW] += angle;

			AngleVectors(angs, forwardVec, NULL, NULL);
		}
		else
		{
			vectoangles(forwardVec, angs);
			if (ent->count)
				angs[YAW] += -2.0f;
			else
				angs[YAW] += 2.0f;

			AngleVectors(angs, forwardVec, NULL, NULL);
		}

	}

	for (int i = 0; i < 2; i++)
	{
		VectorMA(muzzle, scalers[i], vrightVec, muzzle);

		gentity_t* missile = CreateMissile(muzzle, forwardVec, velocity, 10000, ent, 0);

		missile->classname = "blaster_proj";
		missile->s.weapon = ent->s.weapon;

		WP_SetMethodOfDeath(missile, ent->s.weapon, attackIndex);
		missile->damage = damage;
		missile->dflags = DAMAGE_DEATH_KNOCKBACK;
		missile->methodOfDeath = MOD_SBD;
		missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
		missile->bounceCount = 8;
	}

	WP_SwitchPistolMuzzle(ent);
}


//------------------------------------------------------------------------------------
//	Grenades Functions
//------------------------------------------------------------------------------------
void WP_GrenadePlayExplosionEffect(gentity_t* ent, weaponAttackData_t *attackData) {
	if (attackData->explosionEffect && attackData->explosionEffect[0]) {
		G_PlayEffect(attackData->explosionEffect, ent->currentOrigin);
	}
	else
	{
		G_PlayEffect("thermal/explosion", ent->currentOrigin);
	}
	if (attackData->shockwaveEffect && attackData->shockwaveEffect[0]) {
		G_PlayEffect(attackData->shockwaveEffect, ent->currentOrigin);
	}
	else
	{
		G_PlayEffect("thermal/shockwave", ent->currentOrigin);
	}
}
//---------------------------------------------------------
void WP_GrenadeExplode(gentity_t* ent)
//---------------------------------------------------------
{
	if ((ent->s.eFlags & EF_HELD_BY_SAND_CREATURE))
	{
		ent->takedamage = qfalse; // don't allow double deaths!

		G_Damage(ent->activator, ent, ent->owner, vec3_origin, ent->currentOrigin, weaponData[ent->s.weapon].attackData[0].damage, 0, MOD_EXPLOSIVE);

		WP_GrenadePlayExplosionEffect(ent, &weaponData[ent->s.weapon].attackData[0]);

		G_FreeEntity(ent);
	}
	else if (!ent->count)
	{
		G_Sound(ent, G_SoundIndex("sound/weapons/thermal/warning.wav"));
		ent->count = 1;
		ent->nextthink = level.time + 800;
		ent->svFlags |= SVF_BROADCAST;//so everyone hears/sees the explosion?
	}
	else
	{
		vec3_t	pos;

		VectorSet(pos, ent->currentOrigin[0], ent->currentOrigin[1], ent->currentOrigin[2] + 8);

		ent->takedamage = qfalse; // don't allow double deaths!

		G_RadiusDamage(ent->currentOrigin, ent->owner, weaponData[ent->s.weapon].attackData[0].splashDamage, weaponData[ent->s.weapon].attackData[0].splashRadius, NULL, MOD_EXPLOSIVE_SPLASH);

		WP_GrenadePlayExplosionEffect(ent, &weaponData[ent->s.weapon].attackData[0]);

		G_FreeEntity(ent);
	}
}

//---------------------------------------------------------
qboolean WP_LobFire(gentity_t* self, vec3_t start, vec3_t target, vec3_t mins, vec3_t maxs, int clipmask,
	vec3_t velocity, qboolean tracePath, int ignoreEntNum, int enemyNum,
	float minSpeed, float maxSpeed, float idealSpeed, qboolean mustHit)
	//---------------------------------------------------------
{
	float	targetDist, shotSpeed, speedInc = 100, travelTime, impactDist, bestImpactDist = Q3_INFINITE;//fireSpeed,
	vec3_t	targetDir, shotVel, failCase = { 0.0f };
	trace_t	trace;
	trajectory_t	tr;
	qboolean	blocked;
	int		elapsedTime, skipNum, timeStep = 500, hitCount = 0, maxHits = 7;
	vec3_t	lastPos, testPos;
	gentity_t* traceEnt;

	if (!idealSpeed)
	{
		idealSpeed = 300;
	}
	else if (idealSpeed < speedInc)
	{
		idealSpeed = speedInc;
	}
	shotSpeed = idealSpeed;
	skipNum = (idealSpeed - speedInc) / speedInc;
	if (!minSpeed)
	{
		minSpeed = 100;
	}
	if (!maxSpeed)
	{
		maxSpeed = 900;
	}
	while (hitCount < maxHits)
	{
		VectorSubtract(target, start, targetDir);
		targetDist = VectorNormalize(targetDir);

		VectorScale(targetDir, shotSpeed, shotVel);
		travelTime = targetDist / shotSpeed;
		shotVel[2] += travelTime * 0.5 * g_gravity->value;

		if (!hitCount)
		{//save the first (ideal) one as the failCase (fallback value)
			if (!mustHit)
			{//default is fine as a return value
				VectorCopy(shotVel, failCase);
			}
		}

		if (tracePath)
		{//do a rough trace of the path
			blocked = qfalse;

			VectorCopy(start, tr.trBase);
			VectorCopy(shotVel, tr.trDelta);
			tr.trType = TR_GRAVITY;
			tr.trTime = level.time;
			travelTime *= 1000.0f;
			VectorCopy(start, lastPos);

			//This may be kind of wasteful, especially on long throws... use larger steps?  Divide the travelTime into a certain hard number of slices?  Trace just to apex and down?
			for (elapsedTime = timeStep; elapsedTime < floor(travelTime) + timeStep; elapsedTime += timeStep)
			{
				if ((float)elapsedTime > travelTime)
				{//cap it
					elapsedTime = floor(travelTime);
				}
				EvaluateTrajectory(&tr, level.time + elapsedTime, testPos);
				gi.trace(&trace, lastPos, mins, maxs, testPos, ignoreEntNum, clipmask, (EG2_Collision)0, 0);

				if (trace.allsolid || trace.startsolid)
				{
					blocked = qtrue;
					break;
				}
				if (trace.fraction < 1.0f)
				{//hit something
					if (trace.entityNum == enemyNum)
					{//hit the enemy, that's perfect!
						break;
					}
					else if (trace.plane.normal[2] > 0.7 && DistanceSquared(trace.endpos, target) < 4096)//hit within 64 of desired location, should be okay
					{//close enough!
						break;
					}
					else
					{//FIXME: maybe find the extents of this brush and go above or below it on next try somehow?
						impactDist = DistanceSquared(trace.endpos, target);
						if (impactDist < bestImpactDist)
						{
							bestImpactDist = impactDist;
							VectorCopy(shotVel, failCase);
						}
						blocked = qtrue;
						//see if we should store this as the failCase
						if (trace.entityNum < ENTITYNUM_WORLD)
						{//hit an ent
							traceEnt = &g_entities[trace.entityNum];
							if (traceEnt && traceEnt->takedamage && !OnSameTeam(self, traceEnt))
							{//hit something breakable, so that's okay
								//we haven't found a clear shot yet so use this as the failcase
								VectorCopy(shotVel, failCase);
							}
						}
						break;
					}
				}
				if (elapsedTime == floor(travelTime))
				{//reached end, all clear
					break;
				}
				else
				{
					//all clear, try next slice
					VectorCopy(testPos, lastPos);
				}
			}
			if (blocked)
			{//hit something, adjust speed (which will change arc)
				hitCount++;
				shotSpeed = idealSpeed + ((hitCount - skipNum) * speedInc);//from min to max (skipping ideal)
				if (hitCount >= skipNum)
				{//skip ideal since that was the first value we tested
					shotSpeed += speedInc;
				}
			}
			else
			{//made it!
				break;
			}
		}
		else
		{//no need to check the path, go with first calc
			break;
		}
	}

	if (hitCount >= maxHits)
	{//NOTE: worst case scenario, use the one that impacted closest to the target (or just use the first try...?)
		assert((failCase[0] + failCase[1] + failCase[2]) > 0.0f);
		VectorCopy(failCase, velocity);
		return qfalse;
	}
	VectorCopy(shotVel, velocity);
	return qtrue;
}

//---------------------------------------------------------
void WP_GrenadeThink(gentity_t* ent)
//---------------------------------------------------------
{
	int			count;
	qboolean	blow = qfalse;

	// Thermal detonators for the player do occasional radius checks and blow up if there are entities in the blast radius
	//	This is done so that the main fire is actually useful as an attack.  We explode anyway after delay expires.

	if ((ent->s.eFlags & EF_HELD_BY_SAND_CREATURE))
	{//blow once creature is underground (done with anim)
		//FIXME: chance of being spit out?  Especially if lots of delay left...
		ent->e_TouchFunc = touchF_NULL;//don't impact on anything
		if (!ent->activator
			|| !ent->activator->client
			|| !ent->activator->client->ps.legsAnimTimer)
		{//either something happened to the sand creature or it's done with it's attack anim
			//blow!
			ent->e_ThinkFunc = thinkF_WP_GrenadeExplode;
			ent->nextthink = level.time + Q_irand(50, 2000);
		}
		else
		{//keep checking
			ent->nextthink = level.time + TD_THINK_TIME;
		}
		return;
	}
	else if (ent->delay > level.time)
	{
		//	Finally, we force it to bounce at least once before doing the special checks, otherwise it's just too easy for the player?
		if (ent->has_bounced)
		{
			count = G_RadiusList(ent->currentOrigin, TD_TEST_RAD, ent, qtrue, ent_list);

			for (int i = 0; i < count; i++)
			{
				if (ent_list[i]->s.number == 0)
				{
					// avoid deliberately blowing up next to the player, no matter how close any enemy is..
					//	...if the delay time expires though, there is no saving the player...muwhaaa haa ha
					blow = qfalse;
					break;
				}
				else if (ent_list[i]->client
					&& ent_list[i]->client->NPC_class != CLASS_SAND_CREATURE//ignore sand creatures
					&& ent_list[i]->health > 0)
				{
					//FIXME! sometimes the ent_list order changes, so we should make sure that the player isn't anywhere in this list
					blow = qtrue;
				}
			}
		}
	}
	else
	{
		// our death time has arrived, even if nothing is near us
		blow = qtrue;
	}

	if (blow)
	{
		ent->e_ThinkFunc = thinkF_WP_GrenadeExplode;
		ent->nextthink = level.time + 50;
	}
	else
	{
		// we probably don't need to do this thinking logic very often...maybe this is fast enough?
		ent->nextthink = level.time + TD_THINK_TIME;
	}
}

//---------------------------------------------------------
//Make user quickly throw a specified grenade
gentity_t* WP_QuickGrenadeThrowSpecific(gentity_t* ent,int grenadeWeaponIndex) {
	if (grenadeWeaponIndex == 0) {
		grenadeWeaponIndex = WP_THERMAL;
	}
	int currentEquippedWeapon = ent->s.weapon;
	ent->s.weapon = grenadeWeaponIndex;
	gentity_t* toReturn = WP_FireGrenade(ent, 1); //1 is explode on impact
	ent->s.weapon = currentEquippedWeapon;
	return toReturn;
}
//Make user quickly throw a thermal detonator
gentity_t* WP_QuickGrenadeThrow(gentity_t* ent) {
	return WP_QuickGrenadeThrowSpecific(ent, 0);
}

//---------------------------------------------------------
gentity_t* WP_FireGrenade(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[attackIndex];
	gentity_t* bolt;
	vec3_t		dir, start;
	float		damageScale = 1.0f;

	VectorCopy(forwardVec, dir);
	VectorCopy(muzzle, start);

	bolt = G_Spawn();

	bolt->classname = "thermal_detonator";

	if (ent->s.number != 0)
	{
		// If not the player, cut the damage a bit so we don't get pounded on so much
		damageScale = TD_NPC_DAMAGE_CUT;
	}

	if (!attackIndex && ent->s.number == 0)
	{
		// Main fires for the players do a little bit of extra thinking
		bolt->e_ThinkFunc = thinkF_WP_GrenadeThink;
		bolt->nextthink = level.time + TD_THINK_TIME;
		bolt->delay = level.time + TD_TIME; // How long 'til she blows
	}
	else
	{
		bolt->e_ThinkFunc = thinkF_WP_GrenadeExplode;
		bolt->nextthink = level.time + TD_TIME; // How long 'til she blows
	}

	bolt->mass = 10;

	// How 'bout we give this thing a size...
	VectorSet(bolt->mins, -4.0f, -4.0f, -4.0f);
	VectorSet(bolt->maxs, 4.0f, 4.0f, 4.0f);
	bolt->clipmask = MASK_SHOT;
	bolt->clipmask &= ~CONTENTS_CORPSE;
	bolt->contents = CONTENTS_SHOTCLIP;
	bolt->takedamage = qtrue;
	bolt->health = 15;
	bolt->e_DieFunc = dieF_WP_GrenadeDie;

	WP_TraceSetStart(ent, start, bolt->mins, bolt->maxs);//make sure our start point isn't on the other side of a wall

	float chargeAmount = 1.0f; // default of full charge

	if (ent->client)
	{
		chargeAmount = level.time - ent->client->ps.weaponChargeTime;
	}

	// get charge amount
	chargeAmount = chargeAmount / (float)attackData->velocity;

	if (chargeAmount > 1.0f)
	{
		chargeAmount = 1.0f;
	}
	else if (chargeAmount < TD_MIN_CHARGE)
	{
		chargeAmount = TD_MIN_CHARGE;
	}

	float	thrownSpeed = attackData->velocity;
	const qboolean thisIsAShooter = (qboolean)!Q_stricmp("misc_weapon_shooter", ent->classname);

	if (thisIsAShooter)
	{
		if (ent->delay != 0)
		{
			thrownSpeed = ent->delay;
		}
	}

	// normal ones bounce, alt ones explode on impact
	bolt->s.pos.trType = TR_GRAVITY;
	bolt->owner = ent;
	VectorScale(dir, thrownSpeed * chargeAmount, bolt->s.pos.trDelta);

	if (ent->health > 0)
	{
		bolt->s.pos.trDelta[2] += 120;

		if ((ent->NPC || (ent->s.number && thisIsAShooter))
			&& ent->enemy)
		{//NPC or misc_weapon_shooter
			//FIXME: we're assuming he's actually facing this direction...
			vec3_t	target;

			VectorCopy(ent->enemy->currentOrigin, target);
			if (target[2] <= start[2])
			{
				vec3_t	vec;
				VectorSubtract(target, start, vec);
				VectorNormalize(vec);
				VectorMA(target, Q_flrand(0, -32), vec, target);//throw a little short
			}

			target[0] += Q_flrand(-5, 5) + (Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2);
			target[1] += Q_flrand(-5, 5) + (Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2);
			target[2] += Q_flrand(-5, 5) + (Q_flrand(-1.0f, 1.0f) * (6 - ent->NPC->currentAim) * 2);

			WP_LobFire(ent, start, target, bolt->mins, bolt->maxs, bolt->clipmask, bolt->s.pos.trDelta, qtrue, ent->s.number, ent->enemy->s.number);
		}
		else if (thisIsAShooter && ent->target && !VectorCompare(ent->pos1, vec3_origin))
		{//misc_weapon_shooter firing at a position
			WP_LobFire(ent, start, ent->pos1, bolt->mins, bolt->maxs, bolt->clipmask, bolt->s.pos.trDelta, qtrue, ent->s.number, ent->enemy->s.number);
		}
	}

	bolt->attack_index = attackIndex;
	if (!attackIndex)
	{
		bolt->s.eFlags |= EF_BOUNCE_HALF;
	}

	bolt->s.loopSound = G_SoundIndex("sound/weapons/thermal/thermloop.wav");

	bolt->damage = weaponData[ent->s.weapon].attackData[0].damage * damageScale;
	bolt->dflags = 0;
	bolt->splashDamage = weaponData[ent->s.weapon].attackData[0].splashDamage * damageScale;
	bolt->splashRadius = weaponData[ent->s.weapon].attackData[0].splashRadius;

	bolt->s.eType = ET_MISSILE;
	bolt->svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = ent->s.weapon;

	if (attackIndex)
	{
		bolt->methodOfDeath = MOD_THERMAL_ALT;
		bolt->splashMethodOfDeath = MOD_THERMAL_ALT;//? SPLASH;
	}
	else
	{
		bolt->methodOfDeath = MOD_THERMAL;
		bolt->splashMethodOfDeath = MOD_THERMAL;//? SPLASH;
	}

	bolt->s.pos.trTime = level.time;		// move a bit on the very first frame
	VectorCopy(start, bolt->s.pos.trBase);

	SnapVector(bolt->s.pos.trDelta);			// save net bandwidth
	VectorCopy(start, bolt->currentOrigin);

	VectorCopy(start, bolt->pos2);

	return bolt;
}

//---------------------------------------------------------
gentity_t* WP_DropGrenade(gentity_t* ent, int attackIndex)
//---------------------------------------------------------
{
	AngleVectors(ent->client->ps.viewangles, forwardVec, vrightVec, up);
	CalcEntitySpot(ent, SPOT_WEAPON, muzzle);
	return (WP_FireGrenade(ent, attackIndex));
}
extern void WP_GrenadeDie(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod, int dFlags, int hitLoc) {
	WP_GrenadeExplode(self);
}


//---------------------------------------------------------
void WP_RocketThink(gentity_t* ent)
//---------------------------------------------------------
{
	weaponData_t* wpnData = &weaponData[ent->s.weapon];
	weaponAttackData_t* atkData = &wpnData->attackData[ent->attack_index];
	vec3_t newdir, targetdir,
		up = { 0,0,1 }, right;
	vec3_t	org;
	float dot, dot2;

	if (ent->disconnectDebounceTime && ent->disconnectDebounceTime < level.time)
	{//time's up, we're done, remove us
		if (ent->lockCount)
		{//explode when die
			WP_ExplosiveDie(ent, ent->owner, ent->owner, 0, MOD_UNKNOWN, 0, HL_NONE);
		}
		else
		{//just remove when die
			G_FreeEntity(ent);
		}
		return;
	}
	if (ent->enemy && ent->enemy->inuse)
	{
		float vel = (ent->spawnflags & 1) ? ent->speed : atkData->velocity;
		float newDirMult = ent->angle ? ent->angle * 2.0f : 1.0f;
		float oldDirMult = ent->angle ? (1.0f - ent->angle) * 2.0f : 1.0f;

		if ((ent->spawnflags & 1))
		{//vehicle rocket
			if (ent->enemy->client && ent->enemy->client->NPC_class == CLASS_VEHICLE)
			{//tracking another vehicle
				if (ent->enemy->client->ps.speed + ent->speed > vel)
				{
					vel = ent->enemy->client->ps.speed + ent->speed;
				}
			}
		}

		VectorCopy(ent->enemy->currentOrigin, org);
		org[2] += (ent->enemy->mins[2] + ent->enemy->maxs[2]) * 0.5f;

		if (ent->enemy->client)
		{
			switch (ent->enemy->client->NPC_class)
			{
			case CLASS_ATST:
				org[2] += 80;
				break;
			case CLASS_MARK1:
				org[2] += 40;
				break;
			case CLASS_PROBE:
				org[2] += 60;
				break;
			default:
				break;
			}
			if (!TIMER_Done(ent->enemy, "flee"))
			{
				TIMER_Set(ent->enemy, "rocketChasing", 500);
			}
		}

		VectorSubtract(org, ent->currentOrigin, targetdir);
		VectorNormalize(targetdir);

		// Now the rocket can't do a 180 in space, so we'll limit the turn to about 45 degrees.
		dot = DotProduct(targetdir, ent->movedir);

		// a dot of 1.0 means right-on-target.
		if (dot < 0.0f)
		{
			// Go in the direction opposite, start a 180.
			CrossProduct(ent->movedir, up, right);
			dot2 = DotProduct(targetdir, right);

			if (dot2 > 0)
			{
				// Turn 45 degrees right.
				VectorMA(ent->movedir, 0.3f * newDirMult, right, newdir);
			}
			else
			{
				// Turn 45 degrees left.
				VectorMA(ent->movedir, -0.3f * newDirMult, right, newdir);
			}

			// Yeah we've adjusted horizontally, but let's split the difference vertically, so we kinda try to move towards it.
			newdir[2] = ((targetdir[2] * newDirMult) + (ent->movedir[2] * oldDirMult)) * 0.5;

			// slowing down coupled with fairly tight turns can lead us to orbit an enemy..looks bad so don't do it!
//			vel *= 0.5f;
		}
		else if (dot < 0.70f)
		{
			// Still a bit off, so we turn a bit softer
			VectorMA(ent->movedir, 0.5f * newDirMult, targetdir, newdir);
		}
		else
		{
			// getting close, so turn a bit harder
			VectorMA(ent->movedir, 0.9f * newDirMult, targetdir, newdir);
		}

		// add crazy drunkenness
		for (int i = 0; i < 3; i++)
		{
			newdir[i] += Q_flrand(-1.0f, 1.0f) * ent->random * 0.25f;
		}

		// decay the randomness
		ent->random *= 0.9f;

		if (ent->enemy->client
			&& ent->enemy->client->ps.groundEntityNum != ENTITYNUM_NONE)
		{//tracking a client who's on the ground, aim at the floor...?
			// Try to crash into the ground if we get close enough to do splash damage
			float dis = Distance(ent->currentOrigin, org);

			if (dis < 128)
			{
				// the closer we get, the more we push the rocket down, heh heh.
				newdir[2] -= (1.0f - (dis / 128.0f)) * 0.6f;
			}
		}

		VectorNormalize(newdir);

		VectorScale(newdir, vel * 0.5f, ent->s.pos.trDelta);
		VectorCopy(newdir, ent->movedir);
		SnapVector(ent->s.pos.trDelta);			// save net bandwidth
		VectorCopy(ent->currentOrigin, ent->s.pos.trBase);
		ent->s.pos.trTime = level.time;
	}

	ent->nextthink = level.time + 100;
	return;
}

// NOTE: this is 100% for the demp2 alt-fire effect, so changes to the visual effect will affect game side demp2 code
//--------------------------------------------------
void DEMP2_AltRadiusDamage(gentity_t* ent)
{
	float		frac = (level.time - ent->fx_time) / 1300.0f; // synchronize with demp2 effect
	float		dist, radius;
	gentity_t* gent;
	gentity_t* entityList[MAX_GENTITIES];
	int			numListedEntities, i, e;
	vec3_t		mins, maxs;
	vec3_t		v, dir;

	frac *= frac * frac; // yes, this is completely ridiculous...but it causes the shell to grow slowly then "explode" at the end

	radius = frac * 200.0f; // 200 is max radius...the model is aprox. 100 units tall...the fx draw code mults. this by 2.

	for (i = 0; i < 3; i++)
	{
		mins[i] = ent->currentOrigin[i] - radius;
		maxs[i] = ent->currentOrigin[i] + radius;
	}

	numListedEntities = gi.EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	for (e = 0; e < numListedEntities; e++)
	{
		gent = entityList[e];

		if (!gent->takedamage || !gent->contents)
		{
			continue;
		}

		// find the distance from the edge of the bounding box
		for (i = 0; i < 3; i++)
		{
			if (ent->currentOrigin[i] < gent->absmin[i])
			{
				v[i] = gent->absmin[i] - ent->currentOrigin[i];
			}
			else if (ent->currentOrigin[i] > gent->absmax[i])
			{
				v[i] = ent->currentOrigin[i] - gent->absmax[i];
			}
			else
			{
				v[i] = 0;
			}
		}

		// shape is an ellipsoid, so cut vertical distance in half`
		v[2] *= 0.5f;

		dist = VectorLength(v);

		if (dist >= radius)
		{
			// shockwave hasn't hit them yet
			continue;
		}

		if (dist < ent->radius)
		{
			// shockwave has already hit this thing...
			continue;
		}

		VectorCopy(gent->currentOrigin, v);
		VectorSubtract(v, ent->currentOrigin, dir);

		// push the center of mass higher than the origin so players get knocked into the air more
		dir[2] += 12;

		G_Damage(gent, ent, ent->owner, dir, ent->currentOrigin, weaponData[ent->s.weapon].attackData[1].damage, DAMAGE_DEATH_KNOCKBACK, ent->splashMethodOfDeath);
		if (gent->takedamage && gent->client)
		{
			gent->s.powerups |= (1 << PW_SHOCKED);
			gent->client->ps.powerups[PW_SHOCKED] = level.time + 2000;
			Saboteur_Decloak(gent, Q_irand(3000, 10000));
		}
	}

	// store the last fraction so that next time around we can test against those things that fall between that last point and where the current shockwave edge is
	ent->radius = radius;

	if (frac < 1.0f)
	{
		// shock is still happening so continue letting it expand
		ent->nextthink = level.time + 50;
	}
}

//---------------------------------------------------------
void DEMP2_AltDetonate(gentity_t* ent)
//---------------------------------------------------------
{
	G_SetOrigin(ent, ent->currentOrigin);

	// start the effects, unfortunately, I wanted to do some custom things that I couldn't easily do with the fx system, so part of it uses an event and localEntities
	G_PlayEffect("demp2/altDetonate", ent->currentOrigin, ent->pos1);
	G_AddEvent(ent, EV_DEMP2_ALT_IMPACT, ent->count * 2);

	ent->fx_time = level.time;
	ent->radius = 0;
	ent->nextthink = level.time + 50;
	ent->e_ThinkFunc = thinkF_DEMP2_AltRadiusDamage;
	ent->s.eType = ET_GENERAL; // make us a missile no longer
}