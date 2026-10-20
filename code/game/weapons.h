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

// Filename:-	weapons.h
//
// Note that this is now included from both server and game modules, so don't include any other header files
//	within this one that might break stuff...

#ifndef __WEAPONS_H__
#define __WEAPONS_H__

#include "../qcommon/q_shared.h"

// means of death
typedef enum meansOfDeath_s {

	MOD_UNKNOWN,

	// weapons
	MOD_SABER,
	MOD_BRYAR,
	MOD_BRYAR_ALT,
	MOD_BLASTER,
	MOD_BLASTER_ALT,
	MOD_DISRUPTOR,
	MOD_SNIPER,
	MOD_BOWCASTER,
	MOD_BOWCASTER_ALT,
	MOD_REPEATER,
	MOD_REPEATER_ALT,
	MOD_DEMP2,
	MOD_DEMP2_ALT,
	MOD_FLECHETTE,
	MOD_FLECHETTE_ALT,
	MOD_ROCKET,
	MOD_ROCKET_ALT,
	//NEW for JKA weapons:
	MOD_CONC,
	MOD_CONC_ALT,
	//END JKA weapons.
	MOD_THERMAL,
	MOD_THERMAL_ALT,
	MOD_DETPACK,
	MOD_LASERTRIP,
	MOD_LASERTRIP_ALT,
	MOD_MELEE,
	MOD_SEEKER,
	MOD_FORCE_GRIP,
	MOD_FORCE_LIGHTNING,
	MOD_FORCE_DRAIN,
	MOD_EMPLACED,

	MOD_CLONERIFLE,
	MOD_CLONERIFLE_ALT,
	MOD_REBELBLASTER,
	MOD_REBELBLASTER_ALT,
	MOD_CLONECOMMANDO,
	MOD_CLONECOMMANDO_ALT,
	MOD_REBELRIFLE,
	MOD_REBELRIFLE_ALT,
	MOD_REY,
	MOD_REY_ALT,
	MOD_JANGO,
	MOD_JANGO_ALT,
	MOD_BOBA,
	MOD_BOBA_ALT,
	MOD_CLONEPISTOL,
	MOD_CLONEPISTOL_ALT,
	MOD_CIS_SNIPER,
	MOD_CIS_SNIPER_ALT,
	MOD_SBD,
	MOD_DROIDEKA,

	// world / generic
	MOD_ELECTROCUTE,
	MOD_EXPLOSIVE,
	MOD_EXPLOSIVE_SPLASH,
	MOD_KNOCKOUT,
	MOD_ENERGY,
	MOD_ENERGY_SPLASH,
	MOD_WATER,
	MOD_SLIME,
	MOD_LAVA,
	MOD_CRUSH,
	MOD_IMPACT,
	MOD_FALLING,
	MOD_SUICIDE,
	MOD_TRIGGER_HURT,
	MOD_GAS,
	MOD_HIGH_POWERED_SHOT,

	MOD_DESTRUCTION,
	MOD_BLAST,
	MOD_STRIKE,

	NUM_MODS,
	MOD_UNSET = -1,

} meansOfDeath_t;

typedef enum //# weapon_e
{
	WP_NONE,

	// Player weapons
	WP_SABER,			// player and NPC weapon
	WP_BLASTER_PISTOL,	// player and NPC weapon
	WP_BLASTER,			// player and NPC weapon
	WP_DISRUPTOR,		// player and NPC weapon
	WP_BOWCASTER,		// NPC weapon - player can pick this up, but never starts with them
	WP_REPEATER,		// NPC weapon - player can pick this up, but never starts with them
	WP_DEMP2,			// NPC weapon - player can pick this up, but never starts with them
	WP_FLECHETTE,		// NPC weapon - player can pick this up, but never starts with them
	WP_ROCKET_LAUNCHER,	// NPC weapon - player can pick this up, but never starts with them
	WP_THERMAL,			// player and NPC weapon
	WP_TRIP_MINE,		// NPC weapon - player can pick this up, but never starts with them
	WP_DET_PACK,		// NPC weapon - player can pick this up, but never starts with them
	WP_CONCUSSION,		// NPC weapon - player can pick this up, but never starts with them

	//extras
	WP_MELEE,			// player and NPC weapon - Any ol' melee attack

	//when in atst
	WP_ATST_MAIN,
	WP_ATST_SIDE,

	// These can never be gotten directly by the player
	WP_STUN_BATON,		// stupid weapon, should remove

	//NPC weapons
	WP_BRYAR_PISTOL,	// NPC weapon - player can pick this up, but never starts with them

	WP_EMPLACED_GUN,

	WP_BOT_LASER,		// Probe droid	- Laser blast

	WP_TURRET,			// turret guns

	WP_TIE_FIGHTER,

	WP_RAPID_FIRE_CONC,

	WP_JAWA,
	WP_TUSKEN_RIFLE,
	WP_TUSKEN_STAFF,
	WP_SCEPTER,
	WP_NOGHRI_STICK,

	WP_BATTLEDROID,
	WP_THEFIRSTORDER,
	WP_CLONECARBINE,
	WP_REBELBLASTER,
	WP_CLONERIFLE,
	WP_CLONECOMMANDO,
	WP_REBELRIFLE,
	WP_REY,
	WP_JANGO,
	WP_BOBA,
	WP_CLONEPISTOL,
	WP_CIS_SNIPER,
	WP_SBD,
	WP_DROIDEKA,

	//# #eol
	WP_HC_NUM_WEAPONS
} weapon_t;

#define FIRST_WEAPON		WP_SABER		// this is the first weapon for next and prev weapon switching

/* Blockability Enum /!\ : B_UNSET Should not exist in play*/
typedef enum 
{
	B_UNSET = 0,
	B_DEFLECTABLE = 1,
	B_BLOCKABLE = 2,
	B_PASSTHROUGH = 3
} blockability_t;

typedef enum
{
	FL_NONE = 0,
	FL_MELEE,
	FL_BLASTER,
	FL_BLASTER_CHARGED,
	FL_BOWCASTER,
	FL_BEAM,
	FL_FULL_BEAM,
	FL_BEAM_CHARGED,
	FL_GRENADE_LAUNCHER,
	FL_DEMP2,
	FL_DEMP2_ALT,
	FL_FLECHETTE,
	FL_FLECHETTE_ALT,
	FL_NOGHRI,
	FL_MISSILE,
	FL_MISSILE_AIMED,
	FL_LASER_TRAP,
	FL_PROXIMITY_TRAP,
	FL_EXPLOSIVES,
	FL_GRENADE,
	FL_IMPACT_GRENADE,
	FL_STUNBATON,
	FL_SBD,
	FL_FLAMETHROWER,
	FL_OTHER, // For not yet done weapon
}firingLogic_t;

/*Weapon Bucket for Cycling*/
typedef enum {
	WB_MELEE = -1, //1 Saber, Melee, Stun_Baton
	WB_PISTOLS = -2, //2 All One handed weapons
	WB_BLASTERS = -3, //3 All Light Blasters
	WB_SPECIALISTS = -4, //4 ALL Specialist Weapons (Flechette,Bowcaster,Disruptors)
	WB_HEAVY_WEAPONS = -5, //5 Rocket Launcher, Concussion Rifle,...
	WB_THROWABLES = -6, //6 Explosives & Grenades
	WB_OTHERS = -7, //8 Weapon For Vehicle and npc which should not appears in others buckets
	WB_UNSET = 0 // No weapon should have this bucket...
} weaponBucket_t;

// AMMO_NONE must be first and AMMO_MAX must be last, cause weapon load validates based off of these vals
typedef enum //# ammo_e
{
	AMMO_NONE,
	AMMO_FORCE,		// AMMO_PHASER
	AMMO_BLASTER,	// AMMO_STARFLEET,
	AMMO_POWERCELL,	// AMMO_ALIEN,
	AMMO_METAL_BOLTS,
	AMMO_ROCKETS,
	AMMO_EMPLACED,
	AMMO_THERMAL,
	AMMO_TRIPMINE,
	AMMO_DETPACK,
	AMMO_FLAMER,	// FOR THE FLAMER,
	AMMO_HC_MAX
} ammo_t;

typedef enum firingType_s
{
	FT_AUTOMATIC = 1,
	FT_SEMI,
	FT_BURST,
	FT_HIGH_POWERED
} firingType_t;


typedef enum scopeType_s
{
	ST_NONE = 0,
	ST_DISRUPTOR = 2,
	ST_A280 = 4,
	ST_DC17M,
	ST_EE3,
	ST_F11D,
	ST_E5,
	ST_CUSTOM,
	ST_PROGRESSIVE
} scopeType_t;


enum firingOptions
{
	FIRING_TYPE,
	SHOTS_PER_BURST,
	BURST_FIRE_DELAY
};

typedef struct weaponIndexes_s
{
	char weaponClass[32];
	int index;
} weaponIndexes_t;

typedef enum{
	WC_UNSET = -1, //Not defined in the weapon data, inherited from the base weapon then WC_NONE
	WC_NONE = 0, //Explicitly no category (weapon none)
	WC_MELEE, //Like melee
	WC_STUN_BATON, //For this specific weapons. Is handled by "other case"
	WC_MELEE_1H, //Like Tusken Staff
	WC_PISTOL, // Like bryar
	WC_LIGHT, // Like blaster
	WC_SNIPER, // Like Disruptor
	WC_HEAVY, //Like Bowcaster or rocket launcher
	WC_GRENADE, //Like thermal
	WC_EXPLOSIVE, //Like detpack
	WC_MINIGUN, //Like a Z6
	WC_SHOULDER //Like a PLX-1
} weaponCategory_t;


typedef struct weaponAttackData_s
{
	firingLogic_t firingLogic; //The method of fire for this attack
	qboolean parsed;					// The attack block exists in the weapon data

	/* Base Data */
	int		energyPerShot;				// Amount of energy used per shot
	int		fireTime;					// Amount of time between firings
	int		effectDuration;				// Amount of time the effect lasts (For flamer)
	int		range;						// Range of weapon
	float 	spread;						// Accuracy of shots
	float 	npcSpread[3];				// Accuracy of shots for most npc
	int		damage;						// Damage per shot
	int		npcDamage[3];				// Damage per shot for most npc 
	int		defaultDamage;				// Default damage per shot
	float	velocity;					// Speed of missile
	float	npcVelocity[3];				// Speed of missile for most npc
	int 	fireOption[3];				// Option for the fire (Type, Projectile count,...)
	meansOfDeath_t methodOfDeath;		// Means of death for this attack
	meansOfDeath_t splashMethodOfDeath; // Means of death for this attack

	/* Splash Damage */
	int		splashDamage;		// Splash damage when shot explodes
	float	splashRadius;		// Splash radius when shot explodes

	/* Effects */
	void* missileFunc;		// "FX for the missile"
	char	projectileEffect[64]; // Effect to Override the base Projectile
	char	hitWallEffect[64];  
	char	hitWallEffect2[64]; 
	char	hitWallEffect3[64];
	char	hitDroidEffect[64];
	char	hitFleshEffect[64];
	char	muzzleEffect[64];  // Effect to Override the base muzzle
	char	explosionEffect[64]; // For explosives
	char    shockwaveEffect[64]; // For explosives

	/* Indicate if the weapon can be blocked by a lightsaber (Deflaut BLOCK_DEFLECTABLE)*/
	blockability_t blockability[3];

	/* Beam Effects */
	char	beamShader[64];
	vec3_t  beamColor;
	char	fullBeamShader[64];
	vec3_t  fullBeamColor;

	/* Beam Behaviour (0 / empty = none) */
	float	beamRadius;			// Half size of the trace box, makes far targets easier to hit
	int		selfKnockback;		// Push the shooter backwards by this much
	int		pushForce;			// Push the victim away by this much
	int		knockdownForce;		// Knock the living victim down with this strength
	char	beamTrailEffect[64];	// Effect played every 64 units along the beam

	/* Blaster Bounce Data */
	qboolean bounceWall;		//BounceOnWalls;
	int		bounceCount;		//BounceCounts

	/* Charged Shot Data */
	float	chargeUnitTime;		// Time to Charge one unit of power
	int		maxChargeUnits;		// Max amount of charge you can have.
	char	chargeSnd[64];		// sound to start when the weapon initiates the charging sequence
	char	chargeMuzzleShader[64];
	float	chargeMuzzleScale;


	/* Physical Missile Data */
	char	missileMdl[64];		// Missile Model
	char	missileSound[64];	// Missile flight sound
	float  	missileDlight;		// what is says
	int		missileSize;		// Physical "size" of the missile (max & min)
	int		missileMass;		// Physical "mass" of the missile
	int		missileDFlags;		// Overwrite the default flags for some weapons.
	vec3_t 	missileDlightColor;	// ditto
	char	firingSnd[64];		// Sound made when fired
	char	startSnd[64];		// Sound made when first started
	char	missileHitSound[64];	// Missile impact sound

	/* Demp 2 Detonate Case*/
	char	dempDetonateShader[64]; //Shader for explosion
	char	dempDetonateModel[64];  //Model for explosion


} weaponAttackData_t;

#define MAX_WEAPON_ATTACKS 4

typedef struct weaponData_s
{
	weaponAttackData_t attackData[MAX_WEAPON_ATTACKS];

	char	classname[32];		// Spawning name
	char	baseclass[32];		// Base Weapon
	int		baseWeaponNum;		// Base WeaponNum;
	char	weaponMdl[64];		// Weapon Model
	char	stopSnd[64];		// Sound made when weapon stops firing
	char	selectSnd[64];		// the sound to play when this weapon gets selected
	char	readySnd[64];		// the sound to play when this weapon is Idle

	int		ammoIndex;			// Index to proper ammo slot
	int		ammoLow;			// Count when ammo is low

	char	weaponIcon[64];		// Name of weapon icon file
	int		numBarrels;			// how many barrels should we expect for this weapon?

	int 	 scopeType;
	char 	 scopeMask[64];
	char 	 scopeInsert[64];
	qboolean scopefullMask;
	float 	 scopeFov;

	char	weaponMdl2[64];
	qboolean secondaryMdl;
	qboolean playerUsable;
	weaponCategory_t weaponCategory;
	weaponBucket_t weaponBucket;

} weaponData_t;


typedef struct ammoData_s
{
	char	icon[64];	// Name of ammo icon file
	int		giveWeaponIndex;
	int		max;		// Max amount player can hold of ammo
} ammoData_t;


// High Powered
//--------------
#define HIGH_POWERED_DAMAGE			200


// Npc constants
//-----------------
#define BLASTER_NPC_SPREAD			0.5f
#define BLASTER_NPC_VEL_CUT			0.5f
#define BLASTER_NPC_HARD_VEL_CUT	0.7f
#define DISRUPTOR_NPC_MAIN_DAMAGE_EASY	5
#define DISRUPTOR_NPC_MAIN_DAMAGE_MEDIUM	10
#define DISRUPTOR_NPC_MAIN_DAMAGE_HARD	15
#define DISRUPTOR_NPC_ALT_DAMAGE_EASY	15
#define DISRUPTOR_NPC_ALT_DAMAGE_MEDIUM	25
#define DISRUPTOR_NPC_ALT_DAMAGE_HARD	30
#define CLONEPISTOL_NPC_SPREAD			0.5f
#define E5_NPC_SPREAD			1.0f
#define REBELBLASTER_NPC_SPREAD 			0.4f
#define CLONERIFLE_NPC_SPREAD			1.0f
#define F_11D_NPC_SPREAD			0.4f
#define DISRUPTOR_ALT_TRACES			3		// can go through a max of 3 entities

// DEMP2
//----------
#define	DEMP2_DAMAGE				15
#define	DEMP2_VELOCITY				1800
#define	DEMP2_NPC_DAMAGE_EASY		6
#define	DEMP2_NPC_DAMAGE_NORMAL		12
#define	DEMP2_NPC_DAMAGE_HARD		18
#define	DEMP2_SIZE					2		// half of bbox size

#define DEMP2_ALT_DAMAGE			15
#define DEMP2_CHARGE_UNIT			500.0f	// demp2 charging gives us one more unit every 500ms--if you change this, you'll have to do the same in bg_pmove
#define DEMP2_ALT_RANGE				4096
#define DEMP2_ALT_SPLASHRADIUS		256

// Golan Arms Flechette
//---------
#define FLECHETTE_SHOTS				6
#define FLECHETTE_SPREAD			4.0f
#define FLECHETTE_DAMAGE			15
#define FLECHETTE_VEL				3500
#define FLECHETTE_SIZE				1

#define FLECHETTE_ALT_DAMAGE		20
#define FLECHETTE_ALT_VEL			950
#define FLECHETTE_ALT_MAX_VEL		1650
#define FLECHETTE_ALT_SPLASH_DAM	20
#define FLECHETTE_ALT_SPLASH_RAD	128
#define FLECHETTE_ALT_MAX_SIZE		3
#define FLECHETTE_ALT_MIN_SIZE		-3

// NOT CURRENTLY USED
#define FLECHETTE_MINE_RADIUS_CHECK		200
#define FLECHETTE_MINE_VEL				1000
#define FLECHETTE_MINE_DAMAGE			100
#define FLECHETTE_MINE_SPLASH_DAMAGE	200
#define FLECHETTE_MINE_SPLASH_RADIUS	200


// Emplaced Gun
//--------------
#define EMPLACED_VEL				6000	// very fast
#define EMPLACED_DAMAGE				150		// and very damaging
#define EMPLACED_SIZE				5		// make it easier to hit things

// ATST Main Gun
//--------------
#define ATST_MAIN_VEL				4000	//
#define ATST_MAIN_DAMAGE			25		//
#define ATST_MAIN_SIZE				3		// make it easier to hit things

// ATST Side Gun
//---------------
#define ATST_SIDE_MAIN_DAMAGE				75
#define ATST_SIDE_MAIN_VELOCITY				1300
#define ATST_SIDE_MAIN_NPC_DAMAGE_EASY		30
#define ATST_SIDE_MAIN_NPC_DAMAGE_NORMAL	40
#define ATST_SIDE_MAIN_NPC_DAMAGE_HARD		50
#define ATST_SIDE_MAIN_SIZE					4
#define ATST_SIDE_MAIN_SPLASH_DAMAGE		10	// yeah, pretty small, either zero out or make it worth having?
#define ATST_SIDE_MAIN_SPLASH_RADIUS		16	// yeah, pretty small, either zero out or make it worth having?

#define ATST_SIDE_ALT_VELOCITY				1100
#define ATST_SIDE_ALT_NPC_VELOCITY			600
#define ATST_SIDE_ALT_DAMAGE				130

#define ATST_SIDE_ROCKET_NPC_DAMAGE_EASY	30
#define ATST_SIDE_ROCKET_NPC_DAMAGE_NORMAL	50
#define ATST_SIDE_ROCKET_NPC_DAMAGE_HARD	90

#define	ATST_SIDE_ALT_SPLASH_DAMAGE			130
#define	ATST_SIDE_ALT_SPLASH_RADIUS			200
#define ATST_SIDE_ALT_ROCKET_SIZE			5
#define ATST_SIDE_ALT_ROCKET_SPLASH_SCALE	0.5f	// scales splash for NPC's

// Stun Baton
//--------------
#define STUN_BATON_DAMAGE			22
#define STUN_BATON_ALT_DAMAGE		22
#define STUN_BATON_RANGE			25

// Laser Trip Mine
//--------------
#define LT_DAMAGE			150
#define LT_SPLASH_RAD		256.0f
#define LT_SPLASH_DAM		90

#define LT_VELOCITY			250.0f
#define LT_ALT_VELOCITY		1000.0f

#define PROX_MINE_RADIUS_CHECK		190

#define LT_SIZE				3.0f
#define LT_ALT_TIME			2000
#define	LT_ACTIVATION_DELAY	1000
#define	LT_DELAY_TIME		50

// Thermal Detonator
//--------------
#define TD_DAMAGE			100
#define TD_NPC_DAMAGE_CUT	0.6f	// NPC thrown dets deliver only 60% of the damage that a player thrown one does
#define TD_SPLASH_RAD		128
#define TD_SPLASH_DAM		90
#define TD_VELOCITY			900
#define TD_MIN_CHARGE		0.15f
#define TD_TIME				4000
#define TD_THINK_TIME		300		// don't think too often?
#define TD_TEST_RAD			(TD_SPLASH_RAD * 0.8f) // no sense in auto-blowing up if exactly on the radius edge--it would hardly do any damage
#define TD_ALT_TIME			3000

#define TD_ALT_DAMAGE		100
#define TD_ALT_SPLASH_RAD	128
#define TD_ALT_SPLASH_DAM	90
#define TD_ALT_VELOCITY		600
#define TD_ALT_MIN_CHARGE	0.15f
#define TD_ALT_TIME			3000

// SBD Offsets
//--------------
#define SBD_LEFT_SHOT			-3.0f
#define SBD_RIGHT_SHOT			5.0f






#endif//#ifndef __WEAPONS_H__
