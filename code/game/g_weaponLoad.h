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

// g_weaponLoad.h
// Contains Default Data for hardcoded weapons

/*===========================================================================
	Default (Harcoded) Indexes
===========================================================================*/
weaponIndexes_t _weaponIndexes[] = {
	{"weapon_none",0},
	{"weapon_saber",1},
	{"weapon_blaster_pistol",2},
	{"weapon_blaster",3 },
	{"weapon_disruptor",4},
	{"weapon_bowcaster",5},
	{"weapon_repeater",6},
	{"weapon_demp2",7},
	{"weapon_flechette",8},
	{"weapon_rocket_launcher",9},
	{"weapon_thermal",10},
	{"weapon_trip_mine",11},
	{"weapon_det_pack",12},
	{"weapon_concussion_rifle",13},
	{"weapon_melee",14},
	{"weapon_atst_main",15},
	{"weapon_atst_side",16},
	{"weapon_stun_baton",17},
	{"weapon_bryar_pistol",18},
	{"weapon_emplaced_gun",19},
	{"weapon_bot_laser",20},
	{"weapon_turret",21},
	{"weapon_tie_fighter",22},
	{"weapon_rapid_concussion",23 },
	{"weapon_jawa",24},
	{"weapon_tusken_rifle",25 },
	{"weapon_tusken_staff",26},
	{"weapon_scepter",27},
	{"weapon_noghri_stick",28},
	{"weapon_battledroid",29},
	{"weapon_thefirstorder", 30},
	{"weapon_clonecarbine",31},
	{"weapon_rebelblaster",32},
	{"weapon_clonerifle",33},
	{"weapon_clonecommando",34},
	{"weapon_rebelrifle",35},
	{"weapon_rey",36},
	{"weapon_jango",37},
	{"weapon_boba",38},
	{"weapon_clonepistol",39},
	{"weapon_cis_sniper",40},
	{"weapon_sbd",41},
	{"weapon_droideka",42},
};
static const size_t numHcWeaponIndexes = ARRAY_LEN(_weaponIndexes);

/*===========================================================================
	Logic Function definitions
===========================================================================*/
typedef struct {
	const char* name;
	void	(*func)(centity_t* cent, const struct weaponInfo_s* weapon);
} func_t;
// Bryar
void FX_BryarProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_BryarAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Blaster
void FX_BlasterProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_BlasterAltFireThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Clone
void FX_CloneProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_CloneAltFireThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_CloneAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_CloneCommandoProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);


// Bowcaster
void FX_BowcasterProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Heavy Repeater
void FX_RepeaterProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_RepeaterAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// DEMP2
void FX_DEMP2_ProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_DEMP2_AltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Golan Arms Flechette
void FX_FlechetteProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_FlechetteAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Personal Rocket Launcher
void FX_RocketProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_RocketAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Concussion Rifle
void FX_ConcProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Emplaced weapon
void FX_EmplacedProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Turret weapon
void FX_TurretProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// ATST Main weapon
void FX_ATSTMainProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// ATST Side weapons
void FX_ATSTSideMainProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);
void FX_ATSTSideAltProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

//Tusken projectile
void FX_TuskenShotProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

//Noghri projectile
void FX_NoghriShotProjectileThink(centity_t* cent, const struct weaponInfo_s* weapon);

// Table used to attach an extern missile function string to the actual cgame function
func_t	funcs[] = {
	{"bryar_func",			FX_BryarProjectileThink},
	{"bryar_alt_func",		FX_BryarAltProjectileThink},
	{"blaster_func",		FX_BlasterProjectileThink},
	{"blaster_alt_func",	FX_BlasterAltFireThink},
	{"bowcaster_func",		FX_BowcasterProjectileThink},
	{"repeater_func",		FX_RepeaterProjectileThink},
	{"repeater_alt_func",	FX_RepeaterAltProjectileThink},
	{"demp2_func",			FX_DEMP2_ProjectileThink},
	{"demp2_alt_func",		FX_DEMP2_AltProjectileThink},
	{"flechette_func",		FX_FlechetteProjectileThink},
	{"flechette_alt_func",	FX_FlechetteAltProjectileThink},
	{"rocket_func",			FX_RocketProjectileThink},
	{"rocket_alt_func",		FX_RocketAltProjectileThink},
	{"conc_func",			FX_ConcProjectileThink},
	{"emplaced_func",		FX_EmplacedProjectileThink},
	{"turret_func",			FX_TurretProjectileThink},
	{"atstmain_func",		FX_ATSTMainProjectileThink},
	{"atst_side_alt_func",	FX_ATSTSideAltProjectileThink},
	{"atst_side_main_func",	FX_ATSTSideMainProjectileThink},
	{"tusk_shot_func",		FX_TuskenShotProjectileThink},
	{"noghri_shot_func",	FX_NoghriShotProjectileThink},
	{ "clone_func",			FX_CloneProjectileThink},
	{ "clone_alt_func",     FX_CloneAltFireThink },
	{ "clone_pistol_alt_func", FX_CloneAltProjectileThink },
	{ "clone_commando_alt_func", FX_CloneCommandoProjectileThink },
	{NULL,					NULL}
};

/*===========================================================================
	Parameters & Parameters Function Declarations
===========================================================================*/
struct wpnParms_s
{
	int	weaponNum;	// Current weapon number
	int	ammoNum;
	int atkNum;
} wpnParms;

// Weapon Function Definition
void WPN_BaseWeapon(const char** holdBuf);
void WPN_Ammo(const char** holdBuf);
void WPN_AmmoIcon(const char** holdBuf);
void WPN_AmmoMax(const char** holdBuf);
void WPN_AmmoLowCnt(const char** holdBuf);
void WPN_AmmoType(const char** holdBuf);
void WPN_StopSnd(const char** holdBuf);
void WPN_ReadySound(const char** holdBuf);
void WPN_SelectSnd(const char** holdBuf);
void WPN_WeaponClass(const char** holdBuf);
void WPN_WeaponIcon(const char** holdBuf);
void WPN_WeaponModel(const char** holdBuf);
void WPN_BarrelCount(const char** holdBuf);
void WPN_ScopeType(const char** holdBuf);
void WPN_ScopeMask(const char** holdBuf);
void WPN_ScopeFullMask(const char** holdBuf);
void WPN_ScopeInsert(const char** holdBuf);
void WPN_ScopeFov(const char** holdBuf);
void WPN_WeaponModel2(const char** holdBuf);
void WPN_PlayerUsable(const char** holdBuf);
void WPN_WeaponCategory(const char** holdBuf);
void WPN_WeaponBucket(const char** holdBuf);
void WPN_ParseAttack(const char** holdBuf);

// Attack Definition
void ATK_EnergyPerShot(const char** holdBuf);
void ATK_FireTime(const char** holdBuf);
void ATK_FiringSound(const char** holdBuf);
void ATK_StartSound(const char** holdBuf);
void ATK_MuzzleEffect(const char** holdBuf);
void ATK_ChargeMuzzleEffect(const char** holdBuf);
void ATK_ChargeMuzzleScale(const char** holdBuf);
void ATK_Range(const char** holdBuf);
void ATK_MethodOfDeath(const char** holdBuf);
void ATK_SplashMethodOfDeath(const char** holdBuf);
void ATK_MissileName(const char** holdBuf);
void ATK_MissileSound(const char** holdBuf);
void ATK_MissileSize(const char** holdBuf);
void ATK_MissileLight(const char** holdBuf);
void ATK_MissileLightColor(const char** holdBuf);
void ATK_FuncName(const char** holdBuf);
void ATK_Damage(const char** holdBuf);
void ATK_SplashDamage(const char** holdBuf);
void ATK_SplashRadius(const char** holdBuf);
void ATK_MissileHitSound(const char** holdBuf);
void ATK_dempDetonateShader(const char** holdBuf);
void ATK_dempDetonateModel(const char** holdBuf);
void ATK_Velocity(const char** holdBuf);
void ATK_Spread(const char** holdBuf);
void ATK_ProjectileEffect(const char** holdBuf);
void ATK_ExplosionEffect(const char** holdBuf);
void ATK_HitWallEffect(const char** holdBuf);
void ATK_HitWallEffect2(const char** holdBuf);
void ATK_HitWallEffect3(const char** holdBuf);
void ATK_HitFleshEffect(const char** holdBuf);
void ATK_HitDroidEffect(const char** holdBuf);
void ATK_BeamShader(const char** holdBuf);
void ATK_BeamColor(const char** holdBuf);
void ATK_FullBeamShader(const char** holdBuf);
void ATK_FullBeamColor(const char** holdBuf);
void ATK_ChargeSnd(const char** holdBuf);
void ATK_ShockwaveEffect(const char** holdBuf);
void ATK_FireOptions(const char** holdBuf);
void ATK_ChargeUnitTime(const char** holdBuf);
void ATK_MaxChargeUnits(const char** holdBuf);
void ATK_BounceWall(const char** holdBuf);
void ATK_BounceCount(const char** holdBuf);
void ATK_MissileMass(const char** holdBuf);
void ATK_MissileDFlags(const char** holdBuf);
void ATK_NpcDamage(const char** holdBuf);
void ATK_NpcSpread(const char** holdBuf);
void ATK_NpcVelocity(const char** holdBuf);
void ATK_FiringLogic(const char** holdBuf);
void ATK_Blockability(const char** holdBuf);
void ATK_EffectDuration(const char** holdBuf);
void ATK_BeamRadius(const char** holdBuf);
void ATK_SelfKnockback(const char** holdBuf);
void ATK_PushForce(const char** holdBuf);
void ATK_KnockdownForce(const char** holdBuf);
void ATK_BeamTrailEffect(const char** holdBuf);

// Legacy weapons.dat force fields
void WPN_FuncSkip(const char** holdBuf);


typedef struct
{
	const char* parmName;
	void	(*func)(const char** holdBuf);
} wpnParms_t;

wpnParms_t AttackDataParms[] =
{
	{ "attackType",			ATK_FiringLogic },
	{ "blockability",		ATK_Blockability },
	{ "chargeUnitTime",		ATK_ChargeUnitTime},
	{ "maxChargeUnits",		ATK_MaxChargeUnits},
	{ "bounceWall",			ATK_BounceWall},
	{ "bounceCount",		ATK_BounceCount},
	{ "energypershot",		ATK_EnergyPerShot},
	{ "fireTime",			ATK_FireTime },
	{ "methodOfDeath",		ATK_MethodOfDeath },
	{ "splashMethodOfDeath",ATK_SplashMethodOfDeath },
	{ "firingsound",		ATK_FiringSound },
	{ "startsound",			ATK_StartSound },
	{ "chargesound",		ATK_ChargeSnd },
	{ "range",				ATK_Range },
	{ "missileModel",		ATK_MissileName },
	{ "missileSize",		ATK_MissileSize },
	{ "missileSound",		ATK_MissileSound },
	{ "missileLight",		ATK_MissileLight },
	{ "missileLightColor",	ATK_MissileLightColor },
	{ "missileFuncName",	ATK_FuncName },
	{ "missileHitSound",	ATK_MissileHitSound },
	{ "missileMass",		ATK_MissileMass },
	{ "missileDFlags",		ATK_MissileDFlags},
	{ "beamShader",			ATK_BeamShader},
	{ "beamColor",			ATK_BeamColor },
	{ "fullBeamShader",		ATK_FullBeamShader },
	{ "fullBeamColor",		ATK_FullBeamColor },
	{ "muzzleEffect",		ATK_MuzzleEffect },
	{ "chargeMuzzleShader",	ATK_ChargeMuzzleEffect },
	{ "chargeMuzzleScale",	ATK_ChargeMuzzleScale },
	{ "projectileEffect",	ATK_ProjectileEffect },
	{ "explosionEffect",	ATK_ExplosionEffect },
	{ "shockwaveEffect",	ATK_ShockwaveEffect},
	{ "hitWallEffect" ,		ATK_HitWallEffect},
	{ "hitWallEffect2" ,	ATK_HitWallEffect2},
	{ "hitWallEffect3" ,	ATK_HitWallEffect3},
	{ "hitFleshEffect" ,	ATK_HitFleshEffect},
	{ "hitDroidEffect" ,	ATK_HitDroidEffect},
	{ "damage",				ATK_Damage },
	{ "npcDamage",			ATK_NpcDamage },
	{ "npcVelocity",		ATK_NpcVelocity },
	{ "npcSpread",			ATK_NpcSpread },
	{ "splashDamage",		ATK_SplashDamage },
	{ "splashRadius",		ATK_SplashRadius },
	{ "dempDetonateShader",	ATK_dempDetonateShader },
	{ "dempDetonateModel",	ATK_dempDetonateModel },
	{ "velocity",			ATK_Velocity },
	{ "spread",				ATK_Spread },
	{ "fireOptions",		ATK_FireOptions},
	{ "effectDuration",		ATK_EffectDuration},
	{ "beamRadius",			ATK_BeamRadius},
	{ "selfKnockback",		ATK_SelfKnockback},
	{ "pushForce",			ATK_PushForce},
	{ "knockdownForce",		ATK_KnockdownForce},
	{ "beamTrailEffect",	ATK_BeamTrailEffect},
};

wpnParms_t WpnParms[] =
{
	//Base Information about the weapon
	{ "weaponclass",	WPN_WeaponClass },
	{ "baseweapon",		WPN_BaseWeapon },
	{ "weaponicon",		WPN_WeaponIcon },
	{ "weaponmodel",	WPN_WeaponModel },
	{ "weaponmodel2",	WPN_WeaponModel2 },
	{ "barrelcount",	WPN_BarrelCount }, //Is this really usefull?
	{ "playerUsable",	WPN_PlayerUsable },
	{ "weaponCategory",	WPN_WeaponCategory },
	{ "weaponBucket",	WPN_WeaponBucket },
	{ "scopeType",		WPN_ScopeType },
	{ "scopeInsert",	WPN_ScopeInsert },
	{ "scopeMask",		WPN_ScopeMask },
	{ "scopeFullMask",	WPN_ScopeFullMask},
	{ "scopeFov",		WPN_ScopeFov },

	//Ammo Information
	{ "ammo",			WPN_Ammo },	//ammo
	{ "ammoicon",		WPN_AmmoIcon },
	{ "ammomax",		WPN_AmmoMax },
	{ "ammolowcount",	WPN_AmmoLowCnt }, //weapons
	{ "ammotype",		WPN_AmmoType },

	// Sound Info
	{ "stopsound",		WPN_StopSnd },
	{ "selectsound",	WPN_SelectSnd },
	{ "readySound",		WPN_ReadySound },
	
	//Attacks Definition
	{ "attackDefinition",	WPN_ParseAttack},


	// Legacy Data that can still be found
	{ "weapontype",		WPN_FuncSkip  },
	{ "firingforce",		WPN_FuncSkip },
	{ "chargeforce",		WPN_FuncSkip },
	{ "altchargeforce",	WPN_FuncSkip },
	{ "selectforce",		WPN_FuncSkip },
};

static const size_t numWpnParms = ARRAY_LEN(WpnParms);
static const size_t numAtkParms = ARRAY_LEN(AttackDataParms);
