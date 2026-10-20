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

// g_weaponLoad.cpp
// fills in memory struct with ext_dat\weapons.dat

// this is excluded from PCH usage 'cos it looks kinda scary to me, being game and ui.... -Ste
#include "g_local.h"
#include "g_parseUtils.h"
#include "g_weaponLoad.h"

// FIXME :What are the right values?
#define MAX_FIRETIME 10000
#define MAX_RANGE Q3_INFINITE
#define MAX_AMMO_STORAGE 1000
#define MAX_BARREL_COUNT 4

weaponAttackData_t* currentAttackData = NULL;

// Unset values (-1) become 0 once the inheritance is done
static void WPN_ClearUnset(weaponData_t* wpn)
{
	if (wpn->ammoIndex == -1) { wpn->ammoIndex = 0; }
	if (wpn->ammoLow == -1) { wpn->ammoLow = 0; }
	if (wpn->numBarrels == -1) { wpn->numBarrels = 0; }
	if (wpn->scopeType == -1) { wpn->scopeType = 0; }
	if (wpn->weaponCategory == WC_UNSET) { wpn->weaponCategory = WC_NONE; }
	for (int k = 0; k < MAX_WEAPON_ATTACKS; k++)
	{
		weaponAttackData_t* atk = &wpn->attackData[k];
		if (atk->energyPerShot == -1) { atk->energyPerShot = 0; }
		if (atk->damage == -1) { atk->damage = 0; }
		if (atk->defaultDamage == -1) { atk->defaultDamage = 0; }
		if (atk->splashDamage == -1) { atk->splashDamage = 0; }
		if (atk->splashRadius == -1) { atk->splashRadius = 0; }
		if (atk->spread == -1) { atk->spread = 0; }
	}
}

//--------------------------------------------
// Warning prefixed with the weapon being parsed
//--------------------------------------------
static void WPN_Warn(const char* fmt, ...)
{
	va_list	argptr;
	char	msg[1024];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	gi.Printf(S_COLOR_YELLOW"WARNING: weapon '%s': %s", weaponData[wpnParms.weaponNum].classname, msg);
}


//--------------------------------------------

void WPN_FuncSkip(const char** holdBuf)
{
	SkipRestOfLine(holdBuf);
}

/*
----------------------------------------------------------
	Weapon Functions
----------------------------------------------------------
*/
void WPN_WeaponClass(const char** holdBuf)
{
	int i;
	char weaponClass[32];
	int weaponNum;

	ParseStr(holdBuf, weaponClass, 32, "weaponclass");

	for (i = 0; i < numHcWeaponIndexes; i++)
	{
		if (!Q_stricmp(weaponClass, _weaponIndexes[i].weaponClass))
		{
			weaponNum = _weaponIndexes[i].index;
			break;
		}
	}
	if (i == numHcWeaponIndexes)	// !Find parameter
	{
		if (weaponCount >= MAX_WEAPONS) {
			weaponNum = 0;
			gi.Printf(S_COLOR_YELLOW"WARNING: too many weapons in external weapon data(%d); Parsing '%s'\n", MAX_WEAPONS, weaponClass);
			return;
		}
		weaponNum = weaponCount;
		weaponCount++;
	}

	wpnParms.weaponNum = weaponNum;

	Q_strncpyz(weaponData[weaponNum].classname, weaponClass, 32);

	weaponIndexes[weaponNum].index = weaponNum;
}

//--------------------------------------------
void WPN_WeaponModel(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].weaponMdl, 64, "weaponMdl");
}

//--------------------------------------------
void WPN_WeaponIcon(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].weaponIcon, 64, "weaponIcon");
}

//--------------------------------------------
void WPN_AmmoType(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].ammoIndex, AMMO_NONE, MAX_AMMO - 1, "Ammotype");
}

//--------------------------------------------
void WPN_AmmoLowCnt(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].ammoLow, 0, 200, "Ammolowcount");
	// FIXME :What are the right values?
}

//--------------------------------------------
void WPN_StopSnd(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].stopSnd, 64, "stopSnd");
}

//--------------------------------------------
void WPN_SelectSnd(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].selectSnd, 64, "selectSnd");
}

//--------------------------------------------
void WPN_Ammo(const char** holdBuf)
{
	const char* tokenStr;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}

	if (!Q_stricmp(tokenStr, "AMMO_NONE"))
		wpnParms.ammoNum = AMMO_NONE;
	else if (!Q_stricmp(tokenStr, "AMMO_FORCE"))
		wpnParms.ammoNum = AMMO_FORCE;
	else if (!Q_stricmp(tokenStr, "AMMO_BLASTER"))
		wpnParms.ammoNum = AMMO_BLASTER;
	else if (!Q_stricmp(tokenStr, "AMMO_POWERCELL"))
		wpnParms.ammoNum = AMMO_POWERCELL;
	else if (!Q_stricmp(tokenStr, "AMMO_FLAMER"))
		wpnParms.ammoNum = AMMO_FLAMER;
	else if (!Q_stricmp(tokenStr, "AMMO_METAL_BOLTS"))
		wpnParms.ammoNum = AMMO_METAL_BOLTS;
	else if (!Q_stricmp(tokenStr, "AMMO_ROCKETS"))
		wpnParms.ammoNum = AMMO_ROCKETS;
	else if (!Q_stricmp(tokenStr, "AMMO_EMPLACED"))
		wpnParms.ammoNum = AMMO_EMPLACED;
	else if (!Q_stricmp(tokenStr, "AMMO_THERMAL"))
		wpnParms.ammoNum = AMMO_THERMAL;
	else if (!Q_stricmp(tokenStr, "AMMO_TRIPMINE"))
		wpnParms.ammoNum = AMMO_TRIPMINE;
	else if (!Q_stricmp(tokenStr, "AMMO_DETPACK"))
		wpnParms.ammoNum = AMMO_DETPACK;
	else
	{
		WPN_Warn("bad ammotype in external weapon data '%s'\n", tokenStr);
		wpnParms.ammoNum = 0;
	}
}

//--------------------------------------------
void WPN_AmmoIcon(const char** holdBuf)
{
	ParseStr(holdBuf, ammoData[wpnParms.ammoNum].icon, 64, "ammoicon");
}

//--------------------------------------------
void WPN_AmmoMax(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &ammoData[wpnParms.ammoNum].max, 0, MAX_AMMO_STORAGE, "Ammo Max");
}

//--------------------------------------------
void WPN_BarrelCount(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].numBarrels, 0, MAX_BARREL_COUNT, "Barrel Count");
}

//--------------------------------------------
void WPN_WeaponModel2(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].weaponMdl2, 64, "weaponMdl2");
}

//--------------------------------------------
void WPN_BaseWeapon(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].baseclass, 32, "Base weapon");
}

//--------------------------------------------
void WPN_PlayerUsable(const char** holdBuf)
{
	int tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].playerUsable = (qboolean)tokenInt;
}

//--------------------------------------------
void WPN_WeaponCategory(const char** holdBuf)
{
	const char* tokenStr;
	weaponCategory_t weaponCategory = WC_NONE;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}

	if (!Q_stricmp(tokenStr, "WC_NONE")) {
		weaponCategory = WC_NONE;
	}
	else if (!Q_stricmp(tokenStr, "WC_MELEE")) {
		weaponCategory = WC_MELEE;
	}
	else if (!Q_stricmp(tokenStr, "WC_MELEE_1H")) {
		weaponCategory = WC_MELEE_1H;
	}
	else if (!Q_stricmp(tokenStr, "WC_PISTOL")) {
		weaponCategory = WC_PISTOL;
	}
	else if (!Q_stricmp(tokenStr, "WC_LIGHT")) {
		weaponCategory = WC_LIGHT;
	}
	else if (!Q_stricmp(tokenStr, "WC_HEAVY")) {
		weaponCategory = WC_HEAVY;
	}
	else if (!Q_stricmp(tokenStr, "WC_GRENADE")) {
		weaponCategory = WC_GRENADE;
	}
	else if (!Q_stricmp(tokenStr, "WC_EXPLOSIVE")) {
		weaponCategory = WC_EXPLOSIVE;
	}
	else if (!Q_stricmp(tokenStr, "WC_MINIGUN")) {
		weaponCategory = WC_MINIGUN;
	}
	else if (!Q_stricmp(tokenStr, "WC_SHOULDER")) {
		weaponCategory = WC_SHOULDER;
	}
	else if (!Q_stricmp(tokenStr, "WC_SNIPER")) {
		weaponCategory = WC_SNIPER;
	}
	else if (!Q_stricmp(tokenStr, "WC_STUN_BATON")) {
		weaponCategory = WC_STUN_BATON;
	}
	else {
		weaponCategory = WC_UNSET;
		WPN_Warn("Invalid value %s for WeaponCategory in external WEAPONS.DAT\n", tokenStr);
	}
	weaponData[wpnParms.weaponNum].weaponCategory = weaponCategory;
}

void WPN_WeaponBucket(const char** holdBuf)
{
	const char* tokenStr;
	weaponBucket_t weaponBucket = WB_OTHERS;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}

	if (!Q_stricmp(tokenStr, "WB_PISTOLS")) {
		weaponBucket = WB_PISTOLS;
	}
	else if (!Q_stricmp(tokenStr, "WB_BLASTERS")) {
		weaponBucket = WB_BLASTERS;
	}
	else if (!Q_stricmp(tokenStr, "WB_SPECIALISTS")) {
		weaponBucket = WB_SPECIALISTS;
	}
	else if (!Q_stricmp(tokenStr, "WB_HEAVY_WEAPONS")) {
		weaponBucket = WB_HEAVY_WEAPONS;
	}
	else if (!Q_stricmp(tokenStr, "WB_MELEE")) {
		weaponBucket = WB_MELEE;
	}
	else if (!Q_stricmp(tokenStr, "WB_THROWABLES")) {
		weaponBucket = WB_THROWABLES;
	}
	else if (!Q_stricmp(tokenStr, "WB_OTHERS")) {
		weaponBucket = WB_OTHERS;
	}
	else {
		WPN_Warn("Invalid value %s for WeaponCategory in external WEAPONS.DAT\n", tokenStr);
	}
	weaponData[wpnParms.weaponNum].weaponBucket = weaponBucket;
}

//--------------------------------------------
void WPN_ScopeType(const char** holdBuf)
{
	const char* tokenStr;
	scopeType_t scopeType = ST_NONE;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}

	if (!Q_stricmp(tokenStr, "ST_DISRUPTOR"))
	{
		scopeType = ST_DISRUPTOR;
	}
	else if (!Q_stricmp(tokenStr, "ST_PROGRESSIVE"))
	{
		scopeType = ST_PROGRESSIVE;
	}
	else if (!Q_stricmp(tokenStr, "ST_A280"))
	{
		scopeType = ST_A280;
	}
	else if (!Q_stricmp(tokenStr, "ST_DC17M"))
	{
		scopeType = ST_DC17M;
	}
	else if (!Q_stricmp(tokenStr, "ST_E5"))
	{
		scopeType = ST_E5;
	}
	else if (!Q_stricmp(tokenStr, "ST_EE3"))
	{
		scopeType = ST_EE3;
	}
	else if (!Q_stricmp(tokenStr, "ST_F11D"))
	{
		scopeType = ST_F11D;
	}
	else if (!Q_stricmp(tokenStr, "ST_CUSTOM"))
	{
		scopeType = ST_CUSTOM;
	}
	else
	{
		WPN_Warn("bad scopeType in external weapon data '%s'\n", tokenStr);
		return;
	}
	weaponData[wpnParms.weaponNum].scopeType = scopeType;
}

//--------------------------------------------
void WPN_ScopeMask(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].scopeMask, 64, "scopeMask");
}

//--------------------------------------------
void WPN_ScopeFullMask(const char** holdBuf)
{
	int tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].scopefullMask = (qboolean)tokenInt;
}

//--------------------------------------------
void WPN_ScopeInsert(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].scopeInsert, 64, "scopeInsert");
}

//--------------------------------------------
void WPN_ScopeFov(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].scopeFov);
}

//--------------------------------------------
void WPN_ReadySound(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].readySnd, 64, "readySound");
}

/*
----------------------------------------------------------
	Attack Functions
----------------------------------------------------------
*/

void ATK_FiringLogic(const char** holdBuf)
{
	const char* tokenStr;
	firingLogic_t firingLogic = FL_NONE;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}

	if (!Q_stricmp(tokenStr, "FL_MELEE"))
	{
		firingLogic = FL_MELEE;
	}
	else if (!Q_stricmp(tokenStr, "FL_BLASTER"))
	{
		firingLogic = FL_BLASTER;
	}
	else if (!Q_stricmp(tokenStr, "FL_FLAMETHROWER"))
	{
		firingLogic = FL_FLAMETHROWER;
	}
	else if (!Q_stricmp(tokenStr, "FL_BLASTER_CHARGED"))
	{
		firingLogic = FL_BLASTER_CHARGED;
	}
	else if (!Q_stricmp(tokenStr, "FL_BOWCASTER"))
	{
		firingLogic = FL_BOWCASTER;
	}
	else if (!Q_stricmp(tokenStr, "FL_BEAM"))
	{
		firingLogic = FL_BEAM;
	}
	else if (!Q_stricmp(tokenStr, "FL_FULL_BEAM"))
	{
		firingLogic = FL_FULL_BEAM;
	}
	else if (!Q_stricmp(tokenStr, "FL_BEAM_CHARGED"))
	{
		firingLogic = FL_BEAM_CHARGED;
	}
	else if (!Q_stricmp(tokenStr, "FL_GRENADE_LAUNCHER"))
	{
		firingLogic = FL_GRENADE_LAUNCHER;
	}
	else if (!Q_stricmp(tokenStr, "FL_DEMP2"))
	{
		firingLogic = FL_DEMP2;
	}
	else if (!Q_stricmp(tokenStr, "FL_DEMP2_ALT"))
	{
		firingLogic = FL_DEMP2_ALT;
	}
	else if (!Q_stricmp(tokenStr, "FL_FLECHETTE"))
	{
		firingLogic = FL_FLECHETTE;
	}
	else if (!Q_stricmp(tokenStr, "FL_FLECHETTE_ALT"))
	{
		firingLogic = FL_FLECHETTE_ALT;
	}
	else if (!Q_stricmp(tokenStr, "FL_NOGHRI"))
	{
		firingLogic = FL_NOGHRI;
	}
	else if (!Q_stricmp(tokenStr, "FL_MISSILE"))
	{
		firingLogic = FL_MISSILE;
	}
	else if (!Q_stricmp(tokenStr, "FL_MISSILE_AIMED"))
	{
		firingLogic = FL_MISSILE_AIMED;
	}
	else if (!Q_stricmp(tokenStr, "FL_LASER_TRAP"))
	{
		firingLogic = FL_LASER_TRAP;
	}
	else if (!Q_stricmp(tokenStr, "FL_PROXIMITY_TRAP"))
	{
		firingLogic = FL_PROXIMITY_TRAP;
	}
	else if (!Q_stricmp(tokenStr, "FL_EXPLOSIVES"))
	{
		firingLogic = FL_EXPLOSIVES;
	}
	else if (!Q_stricmp(tokenStr, "FL_GRENADE"))
	{
		firingLogic = FL_GRENADE;
	}
	else if (!Q_stricmp(tokenStr, "FL_IMPACT_GRENADE"))
	{
		firingLogic = FL_IMPACT_GRENADE;
	}
	else if (!Q_stricmp(tokenStr, "FL_STUNBATON"))
	{
		firingLogic = FL_STUNBATON;
	}
	else if (!Q_stricmp(tokenStr, "FL_SBD"))
	{
		firingLogic = FL_SBD;
	}
	else if (!Q_stricmp(tokenStr, "FL_OTHER"))
	{
		firingLogic = FL_OTHER;
	}
	else {
		WPN_Warn("Invalid value %s for FiringLogic in external WEAPONS.DAT\n", tokenStr);
	}
	weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].firingLogic = firingLogic;
}

//--------------------------------------------
void ATK_Blockability(const char** holdBuf) {
	int i;
	const char* tokenStr;

	for (i = 0; i < 3; ++i)
	{
		blockability_t blockability = B_UNSET;

		if (COM_ParseString(holdBuf, &tokenStr))
		{
			SkipRestOfLine(holdBuf);
			return;
		}

		if (!Q_stricmp(tokenStr, "B_BLOCKABLE"))
		{
			blockability = B_BLOCKABLE;
		}
		else if (!Q_stricmp(tokenStr, "B_PASSTHROUGH"))
		{
			blockability = B_PASSTHROUGH;
		}
		else if (!Q_stricmp(tokenStr, "B_DEFLECTABLE"))
		{
			blockability = B_DEFLECTABLE;
		}
		else
		{
			WPN_Warn("Invalid value %s for blockability[%d] in external WEAPONS.DAT\n", tokenStr, i);
		}
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].blockability[i] = blockability;
	}
}

//--------------------------------------------
void ATK_StartSound(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].startSnd, 64, "startSnd");
}

//--------------------------------------------
void ATK_FiringSound(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].firingSnd, 64, "firingSnd");
}

//--------------------------------------------
void ATK_ChargeSnd(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].chargeSnd, 64, "chargeSnd");
}


//--------------------------------------------
void ATK_FireTime(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].fireTime, 0, MAX_FIRETIME, "Firetime");
}

//--------------------------------------------
void ATK_EffectDuration(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].effectDuration, 0, INT_MAX, "EffectDuration");
}

//--------------------------------------------
void ATK_BeamRadius(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].beamRadius);
}

//--------------------------------------------
void ATK_SelfKnockback(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].selfKnockback, 0, 1000, "SelfKnockback");
}

//--------------------------------------------
void ATK_PushForce(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].pushForce, 0, 1000, "PushForce");
}

//--------------------------------------------
void ATK_KnockdownForce(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].knockdownForce, 0, 1000, "KnockdownForce");
}

//--------------------------------------------
void ATK_BeamTrailEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].beamTrailEffect, 64, "beamTrailEffect");
}

//--------------------------------------------
void ATK_Range(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].range, 0, MAX_RANGE, "Range");
}

//--------------------------------------------
void ATK_MethodOfDeathInternal(const char** holdBuf, qboolean splashDamage ) {
	const char* tokenStr;
	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}
	meansOfDeath_t methodOfDeath = MOD_UNKNOWN;
	if (!Q_stricmp(tokenStr, "MOD_SABER"))
	{
		methodOfDeath = MOD_SABER;
	}
	else if (!Q_stricmp(tokenStr, "MOD_BRYAR"))
	{
		methodOfDeath = MOD_BRYAR;
	}
	else if (!Q_stricmp(tokenStr, "MOD_BRYAR_ALT"))
	{
		methodOfDeath = MOD_BRYAR_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_BLASTER"))
	{
		methodOfDeath = MOD_BLASTER;
	}
	else if (!Q_stricmp(tokenStr, "MOD_BLASTER_ALT"))
	{
		methodOfDeath = MOD_BLASTER_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_DISRUPTOR"))
	{
		methodOfDeath = MOD_DISRUPTOR;
	}
	else if (!Q_stricmp(tokenStr, "MOD_SNIPER"))
	{
		methodOfDeath = MOD_SNIPER;
	}
	else if (!Q_stricmp(tokenStr, "MOD_BOWCASTER"))
	{
		methodOfDeath = MOD_BOWCASTER;
	}
	else if (!Q_stricmp(tokenStr, "MOD_BOWCASTER_ALT"))
	{
		methodOfDeath = MOD_BOWCASTER_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_REPEATER"))
	{
		methodOfDeath = MOD_REPEATER;
	}
	else if (!Q_stricmp(tokenStr, "MOD_REPEATER_ALT"))
	{
		methodOfDeath = MOD_REPEATER_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_DEMP2"))
	{
		methodOfDeath = MOD_DEMP2;
	}
	else if (!Q_stricmp(tokenStr, "MOD_DEMP2_ALT"))
	{
		methodOfDeath = MOD_DEMP2_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_FLECHETTE"))
	{
		methodOfDeath = MOD_FLECHETTE;
	}
	else if (!Q_stricmp(tokenStr, "MOD_FLECHETTE_ALT"))
	{
		methodOfDeath = MOD_FLECHETTE_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_ROCKET"))
	{
		methodOfDeath = MOD_ROCKET;
	}
	else if (!Q_stricmp(tokenStr, "MOD_ROCKET_ALT"))
	{
		methodOfDeath = MOD_ROCKET_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CONC"))
	{
		methodOfDeath = MOD_CONC;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CONC_ALT"))
	{
		methodOfDeath = MOD_CONC_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_THERMAL"))
	{
		methodOfDeath = MOD_THERMAL;
	}
	else if (!Q_stricmp(tokenStr, "MOD_THERMAL_ALT"))
	{
		methodOfDeath = MOD_THERMAL_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_DETPACK"))
	{
		methodOfDeath = MOD_DETPACK;
	}
	else if (!Q_stricmp(tokenStr, "MOD_LASERTRIP"))
	{
		methodOfDeath = MOD_LASERTRIP;
	}
	else if (!Q_stricmp(tokenStr, "MOD_LASERTRIP_ALT"))
	{
		methodOfDeath = MOD_LASERTRIP_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_MELEE"))
	{
		methodOfDeath = MOD_MELEE;
	}
	else if (!Q_stricmp(tokenStr, "MOD_SEEKER"))
	{
		methodOfDeath = MOD_SEEKER;
	}
	else if (!Q_stricmp(tokenStr, "MOD_FORCE_GRIP"))
	{
		methodOfDeath = MOD_FORCE_GRIP;
	}
	else if (!Q_stricmp(tokenStr, "MOD_FORCE_LIGHTNING"))
	{
		methodOfDeath = MOD_FORCE_LIGHTNING;
	}
	else if (!Q_stricmp(tokenStr, "MOD_FORCE_DRAIN"))
	{
		methodOfDeath = MOD_FORCE_DRAIN;
	}
	else if (!Q_stricmp(tokenStr, "MOD_EMPLACED"))
	{
		methodOfDeath = MOD_EMPLACED;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CLONERIFLE"))
	{
		methodOfDeath = MOD_CLONERIFLE;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CLONERIFLE_ALT"))
	{
		methodOfDeath = MOD_CLONERIFLE_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_REBELBLASTER"))
	{
		methodOfDeath = MOD_REBELBLASTER;
	}
	else if (!Q_stricmp(tokenStr, "MOD_REBELBLASTER_ALT"))
	{
		methodOfDeath = MOD_REBELBLASTER_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CLONECOMMANDO"))
	{
		methodOfDeath = MOD_CLONECOMMANDO;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CLONECOMMANDO_ALT"))
	{
		methodOfDeath = MOD_CLONECOMMANDO_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_REBELRIFLE"))
	{
		methodOfDeath = MOD_REBELRIFLE;
	}
	else if (!Q_stricmp(tokenStr, "MOD_REBELRIFLE_ALT"))
	{
		methodOfDeath = MOD_REBELRIFLE_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_REY"))
	{
		methodOfDeath = MOD_REY;
	}
	else if (!Q_stricmp(tokenStr, "MOD_REY_ALT"))
	{
		methodOfDeath = MOD_REY_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_JANGO"))
	{
		methodOfDeath = MOD_JANGO;
	}
	else if (!Q_stricmp(tokenStr, "MOD_JANGO_ALT"))
	{
		methodOfDeath = MOD_JANGO_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_BOBA"))
	{
		methodOfDeath = MOD_BOBA;
	}
	else if (!Q_stricmp(tokenStr, "MOD_BOBA_ALT"))
	{
		methodOfDeath = MOD_BOBA_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CLONEPISTOL"))
	{
		methodOfDeath = MOD_CLONEPISTOL;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CLONEPISTOL_ALT"))
	{
		methodOfDeath = MOD_CLONEPISTOL_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CIS_SNIPER"))
	{
		methodOfDeath = MOD_CIS_SNIPER;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CIS_SNIPER_ALT"))
	{
		methodOfDeath = MOD_CIS_SNIPER_ALT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_SBD"))
	{
		methodOfDeath = MOD_SBD;
	}
	else if (!Q_stricmp(tokenStr, "MOD_DROIDEKA"))
	{
		methodOfDeath = MOD_DROIDEKA;
	}
	else if (!Q_stricmp(tokenStr, "MOD_ELECTROCUTE"))
	{
		methodOfDeath = MOD_ELECTROCUTE;
	}
	else if (!Q_stricmp(tokenStr, "MOD_EXPLOSIVE"))
	{
		methodOfDeath = MOD_EXPLOSIVE;
	}
	else if (!Q_stricmp(tokenStr, "MOD_EXPLOSIVE_SPLASH"))
	{
		methodOfDeath = MOD_EXPLOSIVE_SPLASH;
	}
	else if (!Q_stricmp(tokenStr, "MOD_KNOCKOUT"))
	{
		methodOfDeath = MOD_KNOCKOUT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_ENERGY"))
	{
		methodOfDeath = MOD_ENERGY;
	}
	else if (!Q_stricmp(tokenStr, "MOD_ENERGY_SPLASH"))
	{
		methodOfDeath = MOD_ENERGY_SPLASH;
	}
	else if (!Q_stricmp(tokenStr, "MOD_WATER"))
	{
		methodOfDeath = MOD_WATER;
	}
	else if (!Q_stricmp(tokenStr, "MOD_SLIME"))
	{
		methodOfDeath = MOD_SLIME;
	}
	else if (!Q_stricmp(tokenStr, "MOD_LAVA"))
	{
		methodOfDeath = MOD_LAVA;
	}
	else if (!Q_stricmp(tokenStr, "MOD_CRUSH"))
	{
		methodOfDeath = MOD_CRUSH;
	}
	else if (!Q_stricmp(tokenStr, "MOD_IMPACT"))
	{
		methodOfDeath = MOD_IMPACT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_FALLING"))
	{
		methodOfDeath = MOD_FALLING;
	}
	else if (!Q_stricmp(tokenStr, "MOD_SUICIDE"))
	{
		methodOfDeath = MOD_SUICIDE;
	}
	else if (!Q_stricmp(tokenStr, "MOD_TRIGGER_HURT"))
	{
		methodOfDeath = MOD_TRIGGER_HURT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_GAS"))
	{
		methodOfDeath = MOD_GAS;
	}
	else if (!Q_stricmp(tokenStr, "MOD_HIGH_POWERED_SHOT"))
	{
		methodOfDeath = MOD_HIGH_POWERED_SHOT;
	}
	else if (!Q_stricmp(tokenStr, "MOD_DESTRUCTION"))
	{
		methodOfDeath = MOD_DESTRUCTION;
	}
	else if (!Q_stricmp(tokenStr, "MOD_BLAST"))
	{
		methodOfDeath = MOD_BLAST;
	}
	else if (!Q_stricmp(tokenStr, "MOD_STRIKE"))
	{
		methodOfDeath = MOD_STRIKE;
	}
	else
	{
		WPN_Warn("bad methodofdeath in external weapon data '%s'\n", tokenStr);
	}

	if (splashDamage)
	{
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].splashMethodOfDeath = methodOfDeath;
	}
	else
	{
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].methodOfDeath = methodOfDeath;
	}
}

void ATK_MethodOfDeath(const char** holdBuf) {
	ATK_MethodOfDeathInternal(holdBuf, qfalse);
}
void ATK_SplashMethodOfDeath(const char** holdBuf) {
	ATK_MethodOfDeathInternal(holdBuf, qtrue);
}
//--------------------------------------------
void ATK_EnergyPerShot(const char** holdBuf)
{
	ParseIntWithLims(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].energyPerShot, 0, MAX_AMMO_STORAGE, "EnergyPerShot");
}

//--------------------------------------------
void ATK_MissileName(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].missileMdl, 64, "MissileName");
}

//--------------------------------------------
void ATK_MissileHitSound(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].missileHitSound, 64, "MissileHitSound");
}

//--------------------------------------------
void ATK_MissileSound(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].missileSound, 64, "MissileSound");
}

//--------------------------------------------
void ATK_ChargeUnitTime(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].chargeUnitTime);
}

//--------------------------------------------
void ATK_MaxChargeUnits(const char** holdBuf)
{
	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].maxChargeUnits);
}

//--------------------------------------------
void ATK_BounceWall(const char** holdBuf)
{
	int tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].bounceWall = (qboolean)tokenInt;
}

//--------------------------------------------
void ATK_BounceCount(const char** holdBuf)
{
	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].bounceCount);
}

//--------------------------------------------
void ATK_MissileLightColor(const char** holdBuf)
{
	int i;
	float	tokenFlt;

	for (i = 0; i < 3; ++i)
	{
		if (COM_ParseFloat(holdBuf, &tokenFlt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenFlt < 0) || (tokenFlt > 1))
		{
			WPN_Warn("bad missilelightcolor[%d] in external weapon data '%f'\n", i, tokenFlt);
			continue;
		}
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].missileDlightColor[i] = tokenFlt;
	}

}

//--------------------------------------------
void ATK_NpcDamage(const char** holdBuf)
{
	int i;
	int	tokenInt;

	for (i = 0; i < 3; ++i)
	{
		if (COM_ParseInt(holdBuf, &tokenInt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenInt < 0))
		{
			WPN_Warn("bad npcDamage[%d] in external weapon data '%d'\n", i, tokenInt);
			continue;
		}
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].npcDamage[i] = tokenInt;
	}

}

//--------------------------------------------
void ATK_NpcSpread(const char** holdBuf)
{
	int i;
	float	tokenFlt;

	for (i = 0; i < 3; ++i)
	{
		if (COM_ParseFloat(holdBuf, &tokenFlt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if (tokenFlt < 0)
		{
			WPN_Warn("bad npcSpread[%d] in external weapon data '%f'\n", i, tokenFlt);
			continue;	// Stays unset
		}
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].npcSpread[i] = tokenFlt;
	}

}

//--------------------------------------------
void ATK_NpcVelocity(const char** holdBuf)
{
	int i;
	int	tokenInt;

	for (i = 0; i < 3; ++i)
	{
		if (COM_ParseInt(holdBuf, &tokenInt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenInt < 0))
		{
			WPN_Warn("bad npcVelocity[%d] in external weapon data '%d'\n", i, tokenInt);
			continue;
		}
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].npcVelocity[i] = tokenInt;
	}

}

//--------------------------------------------
void ATK_MissileSize(const char** holdBuf)
{

	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].missileSize);
}

//--------------------------------------------
void ATK_MissileDFlags(const char** holdBuf)
{
	const char* tokenStr;
	int dFlags = 0;
	const char* tokenStart;

	do {
		tokenStart = *holdBuf;
		COM_ParseString(holdBuf, &tokenStr);
		if (!Q_stricmp(tokenStr, "DAMAGE_RADIUS")) {
			dFlags |= DAMAGE_RADIUS;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_NO_ARMOR")) {
			dFlags |= DAMAGE_NO_ARMOR;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_NO_KNOCKBACK")) {
			dFlags |= DAMAGE_NO_KNOCKBACK;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_NO_HIT_LOC")) {
			dFlags |= DAMAGE_NO_HIT_LOC;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_NO_PROTECTION")) {
			dFlags |= DAMAGE_NO_PROTECTION;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_EXTRA_KNOCKBACK")) {
			dFlags |= DAMAGE_EXTRA_KNOCKBACK;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_DEATH_KNOCKBACK")) {
			dFlags |= DAMAGE_DEATH_KNOCKBACK;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_IGNORE_TEAM")) {
			dFlags |= DAMAGE_IGNORE_TEAM;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_NO_DAMAGE")) {
			dFlags |= DAMAGE_NO_DAMAGE;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_DISMEMBER")) {
			dFlags |= DAMAGE_DISMEMBER;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_NO_KILL")) {
			dFlags |= DAMAGE_NO_KILL;
		}
		else if (!Q_stricmp(tokenStr, "DAMAGE_HEAVY_WEAP_CLASS")) {
			dFlags |= DAMAGE_HEAVY_WEAP_CLASS;
		}
		else if (Q_stricmp(tokenStr, ";")) {
			WPN_Warn("bad Damage Flag in external weapon data '%s' (missing ';' ?)\n", tokenStr);
			*holdBuf = tokenStart;	// Give the bad token back so the caller parses it as a key
			break;
		}
	} while (Q_stricmp(tokenStr, ";"));

	weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].missileDFlags = dFlags;
}
//--------------------------------------------
void ATK_MissileMass(const char** holdBuf)
{

	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].missileMass);
}

//--------------------------------------------
void ATK_MissileLight(const char** holdBuf)
{
	float	tokenFlt;

	if (COM_ParseFloat(holdBuf, &tokenFlt))
	{
		WPN_Warn("missilelight missing in external weapon data \n");
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenFlt < 0) || (tokenFlt > 500)) // FIXME :What are the right values?
	{
		WPN_Warn("bad missilelight in external weapon data '%f'\n", tokenFlt);
		tokenFlt = (tokenFlt < 0) ? 0 : 500;
	}
	weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].missileDlight = tokenFlt;
}


//--------------------------------------------
void ATK_FuncName(const char** holdBuf)
{
	const char* tokenStr;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}
	size_t len = strlen(tokenStr);

	len++;
	if (len > 64)
	{
		len = 64;
		WPN_Warn("FuncName '%s' too long in external WEAPONS.DAT\n", tokenStr);
	}

	for (func_t* s = funcs; s->name; s++) {
		if (!Q_stricmp(s->name, tokenStr)) {
			// found it
			weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].missileFunc = (void*)s->func;
			return;
		}
	}
	WPN_Warn("FuncName '%s' in external WEAPONS.DAT does not exist\n", tokenStr);
}

//--------------------------------------------
void ATK_MuzzleEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].muzzleEffect, 64, "MuzzleEffect");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].muzzleEffect);
}

//--------------------------------------------
void ATK_ProjectileEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].projectileEffect, 64, "projectileEffect");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].projectileEffect);
}

//--------------------------------------------
void ATK_ExplosionEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].explosionEffect, 64, "explosionEffect");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].explosionEffect);
}

//--------------------------------------------
void ATK_HitWallEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitWallEffect, 64, "hitWallEffect");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitWallEffect);
}

//--------------------------------------------
void ATK_HitWallEffect2(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitWallEffect2, 64, "hitWallEffect2");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitWallEffect2);
}

//--------------------------------------------
void ATK_HitWallEffect3(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitWallEffect3, 64, "hitWallEffect3");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitWallEffect3);
}

//--------------------------------------------
void ATK_HitFleshEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitFleshEffect, 64, "hitFleshEffect");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitFleshEffect);
}

void ATK_BeamShader(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].beamShader, 64, "beamShader");
}

//--------------------------------------------
void ATK_BeamColor(const char** holdBuf)
{
	int i;
	float	tokenFlt;

	for (i = 0; i < 3; ++i)
	{
		if (COM_ParseFloat(holdBuf, &tokenFlt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenFlt < 0) || (tokenFlt > 1))
		{
			WPN_Warn("bad beamColor[%d] in external weapon data '%f'\n", i, tokenFlt);
			continue;
		}
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].beamColor[i] = tokenFlt;
	}
}

//--------------------------------------------
void ATK_FullBeamShader(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].fullBeamShader, 64, "fullBeamShader");
}

//--------------------------------------------
void ATK_FullBeamColor(const char** holdBuf)
{
	int i;
	float	tokenFlt;

	for (i = 0; i < 3; ++i)
	{
		if (COM_ParseFloat(holdBuf, &tokenFlt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenFlt < 0) || (tokenFlt > 1))
		{
			WPN_Warn("bad fullBeamColor[%d] in external weapon data '%f'\n", i, tokenFlt);
			continue;
		}
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].fullBeamColor[i] = tokenFlt;
	}
}

//--------------------------------------------
//--------------------------------------------
void ATK_HitDroidEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitDroidEffect, 64, "hitDroidEffect");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].hitDroidEffect);
}

//--------------------------------------------
void ATK_ShockwaveEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].shockwaveEffect, 64, "shockWaveEffect");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].shockwaveEffect);
}

//--------------------------------------------
void ATK_ChargeMuzzleEffect(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].chargeMuzzleShader, 64, "chargeMuzzleEffect");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].chargeMuzzleShader);
}

void ATK_ChargeMuzzleScale(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].chargeMuzzleScale);
}

//--------------------------------------------
void ATK_Damage(const char** holdBuf)
{
	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].damage);
	weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].defaultDamage = weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].damage;
}

//--------------------------------------------
void ATK_SplashDamage(const char** holdBuf)
{
	ParseInt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].splashDamage);
}

//--------------------------------------------
void ATK_SplashRadius(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].splashRadius);
}

//--------------------------------------------
void ATK_FireOptions(const char** holdBuf)
{
	int i;
	int tokenInt;
	const char* tokenStr;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}

	if (!Q_stricmp(tokenStr, "FT_AUTOMATIC"))
	{
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].fireOption[0] = FT_AUTOMATIC;
	}
	else if (!Q_stricmp(tokenStr, "FT_SEMI"))
	{
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].fireOption[0] = FT_SEMI;
	}
	else if (!Q_stricmp(tokenStr, "FT_BURST"))
	{
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].fireOption[0] = FT_BURST;
	}
	else if (!Q_stricmp(tokenStr, "FT_HIGH_POWERED"))
	{
		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].fireOption[0] = FT_HIGH_POWERED;
	}
	else {
		WPN_Warn("bad Fireoptions in external weapon data '%s'\n", tokenStr);
		return;
	}
	for (i = 1; i < 3; i++)
	{
		if (COM_ParseInt(holdBuf, &tokenInt))
		{
			SkipRestOfLine(holdBuf);
			continue;
		}

		if ((tokenInt < 0) || (tokenInt > 10000))
		{
			WPN_Warn("bad Fireoptions[%d] in external weapon data '%d'\n", i, tokenInt);
			continue;
		}

		weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].fireOption[i] = tokenInt;
	}
}

//--------------------------------------------
void ATK_dempDetonateShader(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].dempDetonateShader, 64, "dempDetonateShader");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].dempDetonateShader);
}

//--------------------------------------------
void ATK_dempDetonateModel(const char** holdBuf)
{
	ParseStr(holdBuf, weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].dempDetonateModel, 64, "dempDetonateModel");
	G_EffectIndex(weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].dempDetonateModel);
}

//--------------------------------------------
void ATK_Velocity(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].velocity);
}

//--------------------------------------------
void ATK_Spread(const char** holdBuf)
{
	ParseFlt(holdBuf, &weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].spread);
}

/*
----------------------------------------------------------
	Main Parsing Functions
----------------------------------------------------------
*/

//--------------------------------------------
// Parse the parameter of an attack Section
//--------------------------------------------
static void WP_ParseAtkParms(const char** holdBuf)
{
	const char* token;
	size_t	i;

	while (holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);

		if (!token[0])
		{
			Com_Error(ERR_DROP, "Fatal Error while parsing weapons.dat, end of file reached in an attack definition of weapon '%s' (missing '}' ?)", weaponData[wpnParms.weaponNum].classname);
		}
		if (!Q_stricmp(token, "}"))	// End of data for this attack
			break;
		// Loop through possible parameters
		for (i = 0; i < numAtkParms; ++i)
		{
			if (!Q_stricmp(token, AttackDataParms[i].parmName))
			{
				AttackDataParms[i].func(holdBuf);
				break;
			}
		}

		if (i < numAtkParms)	// Find parameter???
		{
			continue;
		}
		WPN_Warn("bad attack parameter in external weapon data '%s'\n", token);
	}
}

//--------------------------------------------
// Parse An attack section in a weapon
//--------------------------------------------
void WPN_ParseAttack(const char** holdBuf)
{
	char* token;
	token = COM_ParseExt(holdBuf, qtrue);
	if (!Q_stricmp(token, "main")) {
		wpnParms.atkNum = 0;
	}
	else if (!Q_stricmp(token, "scoped_main")) {
		wpnParms.atkNum = 2;
	}
	else if (!Q_stricmp(token, "alt")) {
		wpnParms.atkNum = 1;
	}
	else if (!Q_stricmp(token, "scoped_alt")) {
		wpnParms.atkNum = 3;
	}
	else {
		Com_Error(ERR_DROP, "Fatal Error while parsing weapons.dat, an attack definition is badly formated!");
	}

	token = COM_ParseExt(holdBuf, qtrue);
	if (Q_stricmp(token, "{"))
	{
		Com_Error(ERR_DROP, "Fatal Error while parsing weapons.dat, an attack definition is badly formated!");
	}
	weaponData[wpnParms.weaponNum].attackData[wpnParms.atkNum].parsed = qtrue;
	WP_ParseAtkParms(holdBuf);
}

//--------------------------------------------
// Parse the parameters of a given weapon
//--------------------------------------------
void WP_ParseWeaponParms(const char** holdBuf)
{
	const char* token;
	size_t	i;

	while (holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);

		if (!token[0])
		{
			Com_Error(ERR_DROP, "Fatal Error while parsing weapons.dat, end of file reached in weapon '%s' (missing '}' ?)", weaponData[wpnParms.weaponNum].classname);
		}
		if (!Q_stricmp(token, "}"))	// End of data for this weapon
			break;
		// Loop through possible parameters
		for (i = 0; i < numWpnParms; ++i)
		{
			if (!Q_stricmp(token, WpnParms[i].parmName))
			{
				WpnParms[i].func(holdBuf);
				break;
			}
		}

		if (i < numWpnParms)	// Find parameter???
		{
			continue;
		}
		WPN_Warn("bad parameter in external weapon data '%s'\n", token);
	}
}

//--------------------------------------------
// Parse each weapons
//--------------------------------------------
void WP_ParseParms(const char* buffer)
{
	const char* holdBuf;
	const char* token;

	holdBuf = buffer;
	COM_BeginParseSession();

	while (holdBuf)
	{
		token = COM_ParseExt(&holdBuf, qtrue);

		if (!Q_stricmp(token, "{"))
		{
			WP_ParseWeaponParms(&holdBuf);
		}

	}

	COM_EndParseSession();

}

//--------------------------------------------
// Number of weapons whose index is fixed by the WP_* enum
//--------------------------------------------
int WP_HardcodedWeaponCount(void)
{
	return (int)numHcWeaponIndexes;
}

//--------------------------------------------
// Main Load Function
//--------------------------------------------
void WP_LoadWeaponParms(void)
{
	char* buffer;
	int len;

	len = gi.FS_ReadFile("ext_data/weapons.dat", (void**)&buffer);

	if (len == -1)
	{
		Com_Error(ERR_FATAL, "Cannot find ext_data/weapons.dat!\n");
	}

	// initialise the data area
	memset(weaponData, 0, sizeof(weaponData));
	memset(weaponIndexes, 0, sizeof(weaponIndexes));

	// initialise the weapon Count;
	weaponCount = numHcWeaponIndexes;


	//put in the qunset flag for playerUsable since 0 = false;
	for (int i = 0; i < MAX_WEAPONS; i++) {
		weaponData[i].playerUsable = qunset;
		weaponData[i].scopefullMask = qunset;
		weaponData[i].weaponCategory = WC_UNSET;
		weaponData[i].ammoIndex = -1;
		weaponData[i].ammoLow = -1;
		weaponData[i].numBarrels = -1;
		weaponData[i].scopeType = -1;
		for (int k = 0; k < MAX_WEAPON_ATTACKS; k++)
		{
			weaponData[i].attackData[k].bounceCount = -1;
			weaponData[i].attackData[k].energyPerShot = -1;
			weaponData[i].attackData[k].damage = -1;
			weaponData[i].attackData[k].defaultDamage = -1;
			weaponData[i].attackData[k].splashDamage = -1;
			weaponData[i].attackData[k].splashRadius = -1;
			weaponData[i].attackData[k].spread = -1;
			weaponData[i].attackData[k].chargeMuzzleScale = -1;
			weaponData[i].attackData[k].beamRadius = -1;
			weaponData[i].attackData[k].selfKnockback = -1;
			weaponData[i].attackData[k].pushForce = -1;
			weaponData[i].attackData[k].knockdownForce = -1;
			weaponData[i].attackData[k].bounceWall = qunset;
			weaponData[i].attackData[k].missileDFlags = -1;
			weaponData[i].attackData[k].missileSize = -1;
			weaponData[i].attackData[k].npcDamage[0] = -1;
			weaponData[i].attackData[k].npcDamage[1] = -1;
			weaponData[i].attackData[k].npcDamage[2] = -1;
			weaponData[i].attackData[k].npcVelocity[0] = -1;
			weaponData[i].attackData[k].npcVelocity[1] = -1;
			weaponData[i].attackData[k].npcVelocity[2] = -1;
			weaponData[i].attackData[k].npcSpread[0] = -1;
			weaponData[i].attackData[k].npcSpread[1] = -1;
			weaponData[i].attackData[k].npcSpread[2] = -1;
			weaponData[i].attackData[k].blockability[0] = B_UNSET;
			weaponData[i].attackData[k].blockability[1] = B_UNSET;
			weaponData[i].attackData[k].blockability[2] = B_UNSET;
			weaponData[i].attackData[k].methodOfDeath = MOD_UNSET;
			weaponData[i].attackData[k].splashMethodOfDeath = MOD_UNSET;
		}
	}

	WP_ParseParms(buffer);

	gi.FS_FreeFile(buffer);	//let go of the buffer

	//Read files containing Dynamics Weapons
	char* holdChar;
	char	weaponFileList[5096];			//	The list of file names read in
	int		fileCount;
	int		fileNameSize;

	//Read all the externals file
	fileCount = gi.FS_GetFileList("ext_data/", ".wpn", weaponFileList, sizeof(weaponFileList));

	holdChar = weaponFileList;

	Com_Printf("Found %d External files\n", fileCount);
	for (int i = 0; i < fileCount; i++, holdChar += fileNameSize + 1)
	{
		fileNameSize = strlen(holdChar);

		Com_Printf("Parsing %s\n", holdChar);
		char* fileBuffer;
		int fileLen;

		fileLen = gi.FS_ReadFile(va("ext_data/%s", holdChar), (void**)&fileBuffer);

		if (fileLen == -1)
		{
			Com_Printf("Error reading file\n");
		}
		else
		{
			WP_ParseParms(fileBuffer);

			gi.FS_FreeFile(fileBuffer);	//let go of the buffer
		}
	}

	//Get unset data from each weapon which is a copy of another;
	for (int i = 0; i < weaponCount; i++) {
		if (weaponData[i].baseclass[0])
		{
			int baseWeapon = -1;
			for (int j = 0; j < weaponCount; j++) {
				if (i == j || Q_stricmp(weaponData[i].baseclass, weaponData[j].classname)) {
					continue;
				}
				baseWeapon = j;
				//Copying the weapon Strings from one to another
				if (weaponData[i].weaponMdl[0] == 0) {
					strcpy(weaponData[i].weaponMdl, weaponData[j].weaponMdl);
				}
				if (weaponData[i].stopSnd[0] == 0) {
					strcpy(weaponData[i].stopSnd, weaponData[j].stopSnd);
				}
				if (weaponData[i].selectSnd[0] == 0) {
					strcpy(weaponData[i].selectSnd, weaponData[j].selectSnd);
				}
				if (weaponData[i].weaponIcon[0] == 0) {
					strcpy(weaponData[i].weaponIcon, weaponData[j].weaponIcon);
				}
				if (weaponData[i].weaponMdl2[0] == 0) {
					strcpy(weaponData[i].weaponMdl2, weaponData[j].weaponMdl2);
				}


				//Copiyng raw values.
				weaponData[i].ammoIndex = weaponData[i].ammoIndex == -1 ? weaponData[j].ammoIndex : weaponData[i].ammoIndex;
				weaponData[i].ammoLow = weaponData[i].ammoLow == -1 ? weaponData[j].ammoLow : weaponData[i].ammoLow;

				weaponData[i].numBarrels = weaponData[i].numBarrels == -1 ? weaponData[j].numBarrels : weaponData[i].numBarrels;

				for (int k = 0; k < MAX_WEAPON_ATTACKS; k++) {

					weaponData[i].attackData[k].firingLogic = weaponData[i].attackData[k].firingLogic == 0 ? weaponData[j].attackData[k].firingLogic : weaponData[i].attackData[k].firingLogic;
					weaponData[i].attackData[k].defaultDamage = weaponData[i].attackData[k].defaultDamage == -1 ? weaponData[j].attackData[k].defaultDamage : weaponData[i].attackData[k].defaultDamage;
					weaponData[i].attackData[k].energyPerShot = weaponData[i].attackData[k].energyPerShot == -1 ? weaponData[j].attackData[k].energyPerShot : weaponData[i].attackData[k].energyPerShot;
					weaponData[i].attackData[k].fireTime = weaponData[i].attackData[k].fireTime == 0 ? weaponData[j].attackData[k].fireTime : weaponData[i].attackData[k].fireTime;
					weaponData[i].attackData[k].range = weaponData[i].attackData[k].range == 0 ? weaponData[j].attackData[k].range : weaponData[i].attackData[k].range;
					weaponData[i].attackData[k].damage = weaponData[i].attackData[k].damage == -1 ? weaponData[j].attackData[k].damage : weaponData[i].attackData[k].damage;
					weaponData[i].attackData[k].splashDamage = weaponData[i].attackData[k].splashDamage == -1 ? weaponData[j].attackData[k].splashDamage : weaponData[i].attackData[k].splashDamage;
					weaponData[i].attackData[k].splashRadius = weaponData[i].attackData[k].splashRadius == -1 ? weaponData[j].attackData[k].splashRadius : weaponData[i].attackData[k].splashRadius;
					weaponData[i].attackData[k].velocity = weaponData[i].attackData[k].velocity == 0 ? weaponData[j].attackData[k].velocity : weaponData[i].attackData[k].velocity;
					weaponData[i].attackData[k].spread = weaponData[i].attackData[k].spread == -1 ? weaponData[j].attackData[k].spread : weaponData[i].attackData[k].spread;

					weaponData[i].attackData[k].bounceCount = weaponData[i].attackData[k].bounceCount == -1 ? weaponData[j].attackData[k].bounceCount : weaponData[i].attackData[k].bounceCount;
					weaponData[i].attackData[k].chargeMuzzleScale = weaponData[i].attackData[k].chargeMuzzleScale == -1 ? weaponData[j].attackData[k].chargeMuzzleScale : weaponData[i].attackData[k].chargeMuzzleScale;
					weaponData[i].attackData[k].bounceWall = weaponData[i].attackData[k].bounceWall == qunset ? weaponData[j].attackData[k].bounceWall : weaponData[i].attackData[k].bounceWall;
					weaponData[i].attackData[k].missileDFlags = weaponData[i].attackData[k].missileDFlags == -1 ? weaponData[j].attackData[k].missileDFlags : weaponData[i].attackData[k].missileDFlags;
					weaponData[i].attackData[k].missileSize = weaponData[i].attackData[k].missileSize == -1 ? weaponData[j].attackData[k].missileSize : weaponData[i].attackData[k].missileSize;
					weaponData[i].attackData[k].chargeUnitTime = weaponData[i].attackData[k].chargeUnitTime == 0 ? weaponData[j].attackData[k].chargeUnitTime : weaponData[i].attackData[k].chargeUnitTime;
					weaponData[i].attackData[k].maxChargeUnits = weaponData[i].attackData[k].maxChargeUnits == 0 ? weaponData[j].attackData[k].maxChargeUnits : weaponData[i].attackData[k].maxChargeUnits;

					weaponData[i].attackData[k].methodOfDeath = weaponData[i].attackData[k].methodOfDeath == MOD_UNSET ? weaponData[j].attackData[k].methodOfDeath : weaponData[i].attackData[k].methodOfDeath;
					weaponData[i].attackData[k].splashMethodOfDeath = weaponData[i].attackData[k].splashMethodOfDeath == MOD_UNSET ? weaponData[j].attackData[k].splashMethodOfDeath : weaponData[i].attackData[k].splashMethodOfDeath;


					//Copy
					if (weaponData[i].attackData[k].chargeMuzzleShader[0] == 0) {
						strcpy(weaponData[i].attackData[k].chargeMuzzleShader, weaponData[j].attackData[k].chargeMuzzleShader);
					}
					if (weaponData[i].attackData[k].firingSnd[0] == 0) {
						strcpy(weaponData[i].attackData[k].firingSnd, weaponData[j].attackData[k].firingSnd);
					}
					if (weaponData[i].attackData[k].chargeSnd[0] == 0) {
						strcpy(weaponData[i].attackData[k].chargeSnd, weaponData[j].attackData[k].chargeSnd);
					}

					if (weaponData[i].attackData[k].missileMdl[0] == 0) {
						strcpy(weaponData[i].attackData[k].missileMdl, weaponData[j].attackData[k].missileMdl);
					}
					if (weaponData[i].attackData[k].missileSound[0] == 0) {
						strcpy(weaponData[i].attackData[k].missileSound, weaponData[j].attackData[k].missileSound);
					}
					if (weaponData[i].attackData[k].missileHitSound[0] == 0) {
						strcpy(weaponData[i].attackData[k].missileHitSound, weaponData[j].attackData[k].missileHitSound);
					}
					if (weaponData[i].attackData[k].hitDroidEffect[0] == 0) {
						strcpy(weaponData[i].attackData[k].hitDroidEffect, weaponData[j].attackData[k].hitDroidEffect);
					}
					if (weaponData[i].attackData[k].hitFleshEffect[0] == 0) {
						strcpy(weaponData[i].attackData[k].hitFleshEffect, weaponData[j].attackData[k].hitFleshEffect);
					}
					if (weaponData[i].attackData[k].hitWallEffect[0] == 0) {
						strcpy(weaponData[i].attackData[k].hitWallEffect, weaponData[j].attackData[k].hitWallEffect);
					}
					if (weaponData[i].attackData[k].hitWallEffect2[0] == 0) {
						strcpy(weaponData[i].attackData[k].hitWallEffect2, weaponData[j].attackData[k].hitWallEffect2);
					}
					if (weaponData[i].attackData[k].hitWallEffect3[0] == 0) {
						strcpy(weaponData[i].attackData[k].hitWallEffect3, weaponData[j].attackData[k].hitWallEffect3);
					}
					if (weaponData[i].attackData[k].projectileEffect[0] == 0) {
						strcpy(weaponData[i].attackData[k].projectileEffect, weaponData[j].attackData[k].projectileEffect);
					}
					if (weaponData[i].attackData[k].muzzleEffect[0] == 0) {
						strcpy(weaponData[i].attackData[k].muzzleEffect, weaponData[j].attackData[k].muzzleEffect);
					}
					if (weaponData[i].attackData[k].beamShader[0] == 0) {
						strcpy(weaponData[i].attackData[k].beamShader, weaponData[j].attackData[k].beamShader);
					}
					if (weaponData[i].attackData[k].fullBeamShader[0] == 0) {
						strcpy(weaponData[i].attackData[k].fullBeamShader, weaponData[j].attackData[k].fullBeamShader);
					}
					if (weaponData[i].attackData[k].beamTrailEffect[0] == 0) {
						strcpy(weaponData[i].attackData[k].beamTrailEffect, weaponData[j].attackData[k].beamTrailEffect);
					}
					if (weaponData[i].attackData[k].beamRadius == -1) {
						weaponData[i].attackData[k].beamRadius = weaponData[j].attackData[k].beamRadius;
					}
					if (weaponData[i].attackData[k].selfKnockback == -1) {
						weaponData[i].attackData[k].selfKnockback = weaponData[j].attackData[k].selfKnockback;
					}
					if (weaponData[i].attackData[k].pushForce == -1) {
						weaponData[i].attackData[k].pushForce = weaponData[j].attackData[k].pushForce;
					}
					if (weaponData[i].attackData[k].knockdownForce == -1) {
						weaponData[i].attackData[k].knockdownForce = weaponData[j].attackData[k].knockdownForce;
					}

					if (weaponData[i].attackData[k].dempDetonateModel[0] == 0) {
						strcpy(weaponData[i].attackData[k].dempDetonateModel, weaponData[j].attackData[k].dempDetonateModel);
					}
					if (weaponData[i].attackData[k].dempDetonateShader[0] == 0) {
						strcpy(weaponData[i].attackData[k].dempDetonateShader, weaponData[j].attackData[k].dempDetonateShader);
					}

					//copying weapon missile trail Function pointers
					weaponData[i].attackData[k].missileFunc = weaponData[i].attackData[k].missileFunc == 0 ? weaponData[j].attackData[k].missileFunc : weaponData[i].attackData[k].missileFunc;

					weaponData[i].attackData[k].missileDlight = weaponData[i].attackData[k].missileDlight == 0 ? weaponData[j].attackData[k].missileDlight : weaponData[i].attackData[k].missileDlight;
					//Copying vectors
					if (weaponData[i].attackData[k].missileDlightColor[0] == 0 && weaponData[i].attackData[k].missileDlightColor[1] == 0 && weaponData[i].attackData[k].missileDlightColor[2] == 0) {
						VectorCopy(weaponData[j].attackData[k].missileDlightColor, weaponData[i].attackData[k].missileDlightColor);
					}
					if (weaponData[i].attackData[k].beamColor[0] == 0 && weaponData[i].attackData[k].beamColor[1] == 0 && weaponData[i].attackData[k].beamColor[2] == 0) {
						VectorCopy(weaponData[j].attackData[k].beamColor, weaponData[i].attackData[k].beamColor);
					}
					if (weaponData[i].attackData[k].fullBeamColor[0] == 0 && weaponData[i].attackData[k].fullBeamColor[1] == 0 && weaponData[i].attackData[k].fullBeamColor[2] == 0) {
						VectorCopy(weaponData[j].attackData[k].fullBeamColor, weaponData[i].attackData[k].fullBeamColor);
					}

					//Copying the int arrays
					if (weaponData[i].attackData[k].fireOption[0] == 0 && weaponData[i].attackData[k].fireOption[1] == 0 && weaponData[i].attackData[k].fireOption[2] == 0) {
						weaponData[i].attackData[k].fireOption[0] = weaponData[j].attackData[k].fireOption[0];
						weaponData[i].attackData[k].fireOption[1] = weaponData[j].attackData[k].fireOption[1];
						weaponData[i].attackData[k].fireOption[2] = weaponData[j].attackData[k].fireOption[2];
					}
					if (weaponData[i].attackData[k].npcDamage[0] == -1 && weaponData[i].attackData[k].npcDamage[1] == -1 && weaponData[i].attackData[k].npcDamage[2] == -1) {
						weaponData[i].attackData[k].npcDamage[0] = weaponData[j].attackData[k].npcDamage[0];
						weaponData[i].attackData[k].npcDamage[1] = weaponData[j].attackData[k].npcDamage[1];
						weaponData[i].attackData[k].npcDamage[2] = weaponData[j].attackData[k].npcDamage[2];
					}
					if (weaponData[i].attackData[k].npcVelocity[0] == -1 && weaponData[i].attackData[k].npcVelocity[1] == -1 && weaponData[i].attackData[k].npcVelocity[2] == -1) {
						weaponData[i].attackData[k].npcVelocity[0] = weaponData[j].attackData[k].npcVelocity[0];
						weaponData[i].attackData[k].npcVelocity[1] = weaponData[j].attackData[k].npcVelocity[1];
						weaponData[i].attackData[k].npcVelocity[2] = weaponData[j].attackData[k].npcVelocity[2];
					}
					if (weaponData[i].attackData[k].npcSpread[0] == -1 && weaponData[i].attackData[k].npcSpread[1] == -1 && weaponData[i].attackData[k].npcSpread[2] == -1) {
						weaponData[i].attackData[k].npcSpread[0] = weaponData[j].attackData[k].npcSpread[0];
						weaponData[i].attackData[k].npcSpread[1] = weaponData[j].attackData[k].npcSpread[1];
						weaponData[i].attackData[k].npcSpread[2] = weaponData[j].attackData[k].npcSpread[2];
					}
					if (weaponData[i].attackData[k].blockability[0] == B_UNSET && weaponData[i].attackData[k].blockability[1] == B_UNSET && weaponData[i].attackData[k].blockability[2] == B_UNSET) {
						weaponData[i].attackData[k].blockability[0] = weaponData[j].attackData[k].blockability[0];
						weaponData[i].attackData[k].blockability[1] = weaponData[j].attackData[k].blockability[1];
						weaponData[i].attackData[k].blockability[2] = weaponData[j].attackData[k].blockability[2];
					}
				}

				weaponData[i].scopeType = weaponData[i].scopeType == -1 ? weaponData[j].scopeType : weaponData[i].scopeType;

				weaponData[i].secondaryMdl = weaponData[i].secondaryMdl == 0 ? weaponData[j].secondaryMdl : weaponData[i].secondaryMdl;
				weaponData[i].playerUsable = weaponData[i].playerUsable == qunset ? weaponData[j].playerUsable : weaponData[i].playerUsable;
				weaponData[i].scopefullMask = weaponData[i].scopefullMask == qunset ? weaponData[j].scopefullMask : weaponData[i].scopefullMask;
				weaponData[i].weaponCategory = weaponData[i].weaponCategory == WC_UNSET ? weaponData[j].weaponCategory : weaponData[i].weaponCategory;
				weaponData[i].weaponBucket = weaponData[i].weaponBucket == 0 ? weaponData[j].weaponBucket : weaponData[i].weaponBucket;
				weaponData[i].baseWeaponNum = j;

				//The first one is the good one...
				break;
			}
			//Show an error if unconsistent values are founds. It might still work tough.
			if (weaponData[i].baseclass[0] && baseWeapon == -1) {
				gi.Printf(S_COLOR_YELLOW"WARNING: Weapon '%s' is marked as alternate of base weapon '%s' but base weapon was not found\n", weaponData[i].classname, weaponData[i].baseclass);
			}
			/* Generate Ammo for explosive and Grenade*/
			if (baseWeapon == WP_THERMAL || baseWeapon == WP_DET_PACK || baseWeapon == WP_TRIP_MINE) {
				if (ammoCount == MAX_AMMO) {
					Com_Error(ERR_DROP, "Error, Too many ammo in AmmoData\n");
				}
				ammoData_t* baseAmmo = &ammoData[weaponData[baseWeapon].ammoIndex > 0 ? weaponData[baseWeapon].ammoIndex : 0];
				ammoData[ammoCount].max = baseAmmo->max;
				Q_strncpyz(ammoData[ammoCount].icon, weaponData[i].weaponIcon, 64);
				ammoData[ammoCount].giveWeaponIndex = i;
				weaponData[i].ammoIndex = ammoCount;
				ammoCount++;
			}
		}
		// A scoped attack without attackType takes the one of its main/alt attack when the weapon has a scope, else the game ignores it
		for (int k = 2; k < MAX_WEAPON_ATTACKS; k++)
		{
			weaponAttackData_t* scopedAttack = &weaponData[i].attackData[k];
			if (!scopedAttack->parsed || scopedAttack->firingLogic != FL_NONE)
			{
				continue;
			}
			if (weaponData[i].scopeType > ST_NONE)
			{
				scopedAttack->firingLogic = weaponData[i].attackData[k - 2].firingLogic;
			}
			if (scopedAttack->firingLogic == FL_NONE)
			{
				gi.Printf(S_COLOR_YELLOW"WARNING: weapon '%s': the %s attack has no attackType, it is ignored\n", weaponData[i].classname, (k == 2) ? "scoped_main" : "scoped_alt");
			}
		}
		/* Replace unset Value with false or 0 */
		WPN_ClearUnset(&weaponData[i]);
		weaponData[i].playerUsable = weaponData[i].playerUsable != qunset ? weaponData[i].playerUsable : qfalse;
		weaponData[i].scopefullMask = weaponData[i].scopefullMask != qunset ? weaponData[i].scopefullMask : qfalse;
		for (int k = 0; k < MAX_WEAPON_ATTACKS; k++)
		{
			weaponData[i].attackData[k].bounceCount = weaponData[i].attackData[k].bounceCount != -1 ? weaponData[i].attackData[k].bounceCount : 0;
			if (weaponData[i].attackData[k].beamRadius == -1) { weaponData[i].attackData[k].beamRadius = 0; }
			if (weaponData[i].attackData[k].selfKnockback == -1) { weaponData[i].attackData[k].selfKnockback = 0; }
			if (weaponData[i].attackData[k].pushForce == -1) { weaponData[i].attackData[k].pushForce = 0; }
			if (weaponData[i].attackData[k].knockdownForce == -1) { weaponData[i].attackData[k].knockdownForce = 0; }
			weaponData[i].attackData[k].bounceWall = weaponData[i].attackData[k].bounceWall != qunset ? weaponData[i].attackData[k].bounceWall : qfalse;
			weaponData[i].attackData[k].maxChargeUnits = weaponData[i].attackData[k].maxChargeUnits == 0 ? 1 : weaponData[i].attackData[k].maxChargeUnits;
			weaponData[i].attackData[k].chargeUnitTime = weaponData[i].attackData[k].chargeUnitTime == 0 ? 0.25 : weaponData[i].attackData[k].chargeUnitTime;

			weaponData[i].attackData[k].methodOfDeath = weaponData[i].attackData[k].methodOfDeath == MOD_UNSET ? MOD_UNKNOWN: weaponData[i].attackData[k].methodOfDeath;
			weaponData[i].attackData[k].splashMethodOfDeath = weaponData[i].attackData[k].splashMethodOfDeath == MOD_UNSET ? MOD_UNKNOWN : weaponData[i].attackData[k].splashMethodOfDeath;

			//For blockability, if easy is unset then set it to DEFLECTABLE; For higher difficulty, copy the easier difficulty if unset.
			weaponData[i].attackData[k].blockability[0] = weaponData[i].attackData[k].blockability[0] == B_UNSET ? B_DEFLECTABLE : weaponData[i].attackData[k].blockability[0];
			weaponData[i].attackData[k].blockability[1] = weaponData[i].attackData[k].blockability[1] == B_UNSET ? weaponData[i].attackData[k].blockability[0] : weaponData[i].attackData[k].blockability[1];
			weaponData[i].attackData[k].blockability[2] = weaponData[i].attackData[k].blockability[2] == B_UNSET ? weaponData[i].attackData[k].blockability[1] : weaponData[i].attackData[k].blockability[2];

			if (weaponData[i].attackData[k].npcDamage[0] == -1 && weaponData[i].attackData[k].npcDamage[1] == -1 && weaponData[i].attackData[k].npcDamage[2] == -1) {
				weaponData[i].attackData[k].npcDamage[0] = weaponData[i].attackData[k].damage * 0.3f;
				weaponData[i].attackData[k].npcDamage[1] = weaponData[i].attackData[k].damage * 0.6f;
				weaponData[i].attackData[k].npcDamage[2] = weaponData[i].attackData[k].damage * 0.9f;
			}
		}
	}
	// Unused slots must not keep the unset values either
	for (int i = weaponCount; i < MAX_WEAPONS; i++)
	{
		WPN_ClearUnset(&weaponData[i]);
	}
	//Sort weapons in buckets
	int buckets[-WB_OTHERS][MAX_WEAPONS];
	//Init the data to -1
	memset(&buckets, -1, sizeof(buckets));
	memset(&weaponBuckets, -1, sizeof(weaponBuckets));
	//Naive initialisation of the buckets
	for (int i = 1; i < weaponCount; i++)
	{
		int bucketIndex = (-weaponData[i].weaponBucket) - 1;
		int weaponIndex = 0;

		if (bucketIndex < 0 || bucketIndex >= (-WB_OTHERS))
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: Weapon '%s' has an invalid bucket index '%d'\n", weaponData[i].classname, weaponData[i].weaponBucket);
			continue;
		}

		while (buckets[bucketIndex][weaponIndex] != -1)
		{
			weaponIndex++;
		}
		buckets[bucketIndex][weaponIndex] = i;
	}
	//Now merge the buckets in one array
	int gi = 0;
	for (int i = 0; i < (-WB_OTHERS); i++)
	{
		weaponBuckets[gi] = (-i - 1);
		gi++;
		int j;
		for (j = 0; buckets[i][j] != -1; j++) {
			weaponBuckets[gi] = buckets[i][j];
			gi++;
		}
	}

}
