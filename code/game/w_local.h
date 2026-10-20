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

extern vec3_t	forwardVec, vrightVec, up;
extern vec3_t	muzzle,muzzle2;

void WP_SwitchPistolMuzzle(gentity_t* ent);
int WP_GetWeaponID(const char* weaponName);
void WP_TraceSetStart( const gentity_t *ent, vec3_t start, const vec3_t mins, const vec3_t maxs );
gentity_t *CreateMissile( vec3_t org, vec3_t dir, float vel, int life, gentity_t *owner, int attackIndex = 0);
void WP_Stick( gentity_t *missile, trace_t *trace, float fudge_distance = 0.0f );
void WP_Explode( gentity_t *self );
void WP_ExplosiveDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath,int dFlags,int hitLoc );
bool WP_MissileTargetHint(gentity_t* shooter, vec3_t start, vec3_t out);

void drop_charge(gentity_t *ent, vec3_t start, vec3_t dir);
void ViewHeightFix( const gentity_t * const ent );
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker );
extern qboolean G_BoxInBounds( const vec3_t point, const vec3_t mins, const vec3_t maxs, const vec3_t boundsMins, const vec3_t boundsMaxs );
extern qboolean Jedi_DodgeEvasion( gentity_t *self, gentity_t *shooter, trace_t *tr, int hitLoc );
extern qboolean PM_DroidMelee( int npc_class );
extern void G_Knockdown( gentity_t *self, gentity_t *attacker, const vec3_t pushDir, float strength, qboolean breakSaberLock );
extern qboolean G_HasKnockdownAnims( gentity_t *ent );

extern gentity_t *ent_list[MAX_GENTITIES];

extern int g_rocketLockEntNum;
extern int g_rocketLockTime;
extern int	g_rocketSlackTime;

int G_GetHitLocFromTrace( trace_t *trace, int mod );

// Specific weapon functions
void WP_FireGenericBeam(gentity_t* ent, int attackIndex);
void WP_FireDroidsTwinBlasters(gentity_t* ent, int attackIndex);
void WP_FireGenericBowcaster(gentity_t* ent, int attackIndex);
void WP_FireFlameThrower(gentity_t* ent, int attackIndex);
void WP_FireGenericBlaster(gentity_t* ent, int attackIndex);
void WP_FireDetPack(gentity_t* ent,int attackIndex);
void WP_RocketThink(gentity_t* ent);

// Grenades
qboolean WP_LobFire(gentity_t* self, vec3_t start, vec3_t target, vec3_t mins, vec3_t maxs, int clipmask,
	vec3_t velocity, qboolean tracePath, int ignoreEntNum, int enemyNum,
	float minSpeed = 0, float maxSpeed = 0, float idealSpeed = 0, qboolean mustHit = qfalse);
void WP_GrenadeExplode(gentity_t* ent);
void WP_GrenadeThink(gentity_t* ent);
void WP_GrenadeDie(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod, int dFlags, int hitLoc);
gentity_t* WP_FireGrenade(gentity_t* ent, int attackIndex);
gentity_t* WP_DropGrenade(gentity_t* ent, int attackIndex);

// Laser Traps
void touchLaserTrap(gentity_t* ent, gentity_t* other, trace_t* trace);
void CreateLaserTrap(gentity_t* laserTrap, vec3_t start, gentity_t* owner);
void WP_PlaceLaserTrap(gentity_t* ent, qboolean proximity);
void prox_mine_think(gentity_t* ent);
void prox_mine_stick(gentity_t* self, gentity_t* other, trace_t* trace);

// Det_packs
void charge_stick(gentity_t* self, gentity_t* other, trace_t* trace);

// Fire Stun Baton
void WP_FireStunBaton(gentity_t* ent);

// Npc Weapon
void WP_FireTurboLaserMissile( gentity_t *ent, vec3_t start, vec3_t dir );
void WP_EmplacedFire( gentity_t *ent );
void WP_Melee( gentity_t *ent );

//Weapon still to do
void WP_FireFlechette(gentity_t* ent, qboolean explosive);

