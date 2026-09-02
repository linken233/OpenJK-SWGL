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

#include "cg_headers.h"

#include "cg_media.h"
#include "FxScheduler.h"
#include "../game/wp_saber.h"
#include "../game/g_vehicles.h"

#include "../game/anims.h"

extern void CG_LightningBolt( centity_t *cent, vec3_t origin );

extern cvar_t *g_char_model;

//Values at bottom of file
extern char baseHitFleshEffects[][64];
extern char baseHitWallEffects[][64];

#define	PHASER_HOLDFRAME	2
#define LOADOUT_PAGESIZE	18
extern void G_SoundOnEnt( gentity_t *ent, soundChannel_t channel, const char *soundPath );
const char *CG_DisplayBoxedText(int iBoxX, int iBoxY, int iBoxWidth, int iBoxHeight,
								const char *psText, int iFontHandle, float fScale,
								const vec4_t v4Color);

extern void Inquisitor_Stop(gentity_t* ent, qboolean running = qfalse);
extern void Inquisitor_Spin(gentity_t* ent, qboolean increment = qtrue);

/*

=================
CG_GetWorldModelName

Get WorldModelName. Returned string must be used right away
=================
*/
void CG_GetWorldModelName(char* initalModelName,char* weaponModel,int size) {

	Q_strncpyz(weaponModel, initalModelName, size);
	if (char* spot = strstr(weaponModel, ".md3"))
	{
		*spot = 0;
		spot = strstr(weaponModel, "_w");//i'm using the in view weapon array instead of scanning the item list, so put the _w back on
		if (!spot)
		{
			Q_strcat(weaponModel, size, "_w");
		}
		Q_strcat(weaponModel, size, ".glm");	//and change to ghoul2
	}
}
/*

=================
CG_GetAmmoName

Get Ammo className from weaponClass Name
=================
*/
void CG_GetAmmoName(const char* weaponClassName, char* ammoClassName, int size) {
	if (Q_stricmpn(weaponClassName, "weapon_", 7)) {
		Com_Printf(S_COLOR_YELLOW"Warning : %s not starting with 'weapon_' ammo name will be inconsistent");
	}
	const char* ammoName = weaponClassName + 7;
	Com_sprintf(ammoClassName, size, "ammo_%s", ammoName);
}
/*
=================
CG_InitItemForAmmo

We need a new item initialized.
=================
*/
void CG_InitItemForAmmo(gitem_t* item, int weaponNum) {
	//Init the item
	item->mins[0] = -16;
	item->mins[1] = -16;
	item->mins[2] = -8;

	item->maxs[0] = 16;
	item->maxs[1] = 16;
	item->maxs[2] = 16;


	item->pickup_sound = "sound/weapons/w_pkup.wav";	//give it a default sound
	item->precaches = NULL;
	item->sounds = NULL;

	//Set specific data from weapon
	char ammoClassName[64];
	CG_GetAmmoName(weaponData[weaponNum].classname, ammoClassName, 64);
	item->classname = G_NewString(ammoClassName);
	item->giTag = weaponData[weaponNum].ammoIndex;
	item->giType = (itemType_t) IT_AMMO;
	item->giTagName = G_NewString(ammoClassName);
	item->quantity = 5; //TODO : What value to put here
	item->icon = G_NewString(weaponData[weaponNum].weaponIcon);

	char world_model[64];
	CG_GetWorldModelName(&weaponData[weaponNum].weaponMdl[0], &world_model[0], 64);
	item->world_model = G_NewString(world_model);
	gi.G2API_PrecacheGhoul2Model(world_model);

	RegisterItem(item);
}
/*
=================
CG_InitItemForWeapon

We need a new item initialized.
=================
*/
void CG_InitItemForWeapon(gitem_t* item, int weaponNum) {
	//Init the item
	item->mins[0] = -16;
	item->mins[1] = -16;
	item->mins[2] = -8;

	item->maxs[0] = 16;
	item->maxs[1] = 16;
	item->maxs[2] = 16;


	item->pickup_sound = "sound/weapons/w_pkup.wav";	//give it a default sound
	item->precaches = NULL;
	item->sounds = NULL;

	//Set specific data from weapon
	item->classname = G_NewString(weaponData[weaponNum].classname);
	item->giTag = weaponNum;
	item->giType = (itemType_t) IT_WEAPON;
	item->giTagName = G_NewString(weaponData[weaponNum].classname);
	item->quantity = 50; //TODO : Shall we get base weapon ammo count?
	item->icon = G_NewString(weaponData[weaponNum].weaponIcon);

	char world_model[64];
	CG_GetWorldModelName(&weaponData[weaponNum].weaponMdl[0], &world_model[0], 64);
	item->world_model = G_NewString(world_model);
	gi.G2API_PrecacheGhoul2Model(world_model);

	RegisterItem(item);
}

/*
=================
CG_GetAttackIndex

Return The attack Index of the attack
=================
*/
int CG_GetAttackIndex(gentity_t *gent,qboolean alt_fire) 
{
	int weaponNum = gent->s.weapon;
	if (gent->client && gent->client->ps.clientNum > 0) {
		return alt_fire ? 1 : 0;
	}
	if (gent->client && gent->client->ps.firing_attack >= 0) {
		return gent->client->ps.firing_attack;
	}
	int attackIndex = alt_fire ? 1 : 0;
	if (cg.zoomMode == ST_DISRUPTOR || cg.zoomMode >= ST_A280) {
		if (alt_fire && weaponData[weaponNum].attackData[3].firingLogic != FL_NONE) {
			return 3;
		}
		else if (weaponData[weaponNum].attackData[2].firingLogic != FL_NONE) {
			return 2;
		}
	}
	return attackIndex;
}
/*
=================
CG_RegisterWeapon

The server says this item is used on this level
=================
*/
void CG_RegisterWeapon( int weaponNum ) {
	weaponInfo_t	*weaponInfo;
	qboolean found = qfalse;
	gitem_t			*item, *ammo;
	char			path[MAX_QPATH];
	vec3_t			mins, maxs;
	int				i;
	char			*currWeaponMdl;

	weaponInfo = &cg_weapons[weaponNum];
	int baseWeaponNum = weaponData[weaponNum].baseWeaponNum ? weaponData[weaponNum].baseWeaponNum : weaponNum;

	// error checking
	if ( weaponNum <= 0 ) {
		return;
	}

	if ( weaponInfo->registered ) {
		return;
	}

	// clear out the memory we use
	memset( weaponInfo, 0, sizeof( *weaponInfo ) );
	weaponInfo->registered = qtrue;

	// find the weapon in the item list
	for ( i = 1 ;  i < bg_numItems; i++ ) {
		item = &(bg_itemlist[i]);
		if( ( item->giType == IT_WEAPON && item->giTag == weaponNum )
			|| (item->giType == IT_WEAPON && item->giTag == -1 && !Q_stricmp(item->giTagName, weaponData[weaponNum].classname))
		){
			weaponInfo->item = item;
			//Overwrite correctly the item
			if (item->giTag == -1) {
				item->giTag = weaponNum;
			}
			found = qtrue;
			Com_Printf(S_COLOR_CYAN, "Found item %s for weapon %s", item->classname, weaponData[weaponNum].classname);
			break;
		}
	}
	// if we couldn't find which weapon this is, Create one!
	if ( !found) {
		if (i == MAX_ITEMS) {
			CG_Error("Too many items in external items data(%d); Cannot create nor found item for weapon : '%s'\n", MAX_ITEMS, weaponData[weaponNum].classname);
		}
		item = &(bg_itemlist[bg_numItems]);
		CG_InitItemForWeapon(item, weaponNum);
		weaponInfo->item = item;
		bg_numItems++;

		if (weaponData[weaponNum].baseWeaponNum == WP_THERMAL
			|| weaponData[weaponNum].baseWeaponNum == WP_DET_PACK
			|| weaponData[weaponNum].baseWeaponNum == WP_TRIP_MINE) {
			if (i == MAX_ITEMS) {
				CG_Error("Too many items in external items data(%d); Cannot create nor found ammo item for weapon : '%s'\n", MAX_ITEMS, weaponData[weaponNum].classname);
			}
			item = &(bg_itemlist[bg_numItems]);
			CG_InitItemForAmmo(item, weaponNum);
			bg_numItems++;
		}
	}

	CG_RegisterItemVisuals( item - bg_itemlist );

	currWeaponMdl = (weaponData[weaponNum].secondaryMdl) ? weaponData[weaponNum].weaponMdl2 : weaponData[weaponNum].weaponMdl;

	// set up in view weapon model
	weaponInfo->weaponModel = cgi_R_RegisterModel( currWeaponMdl );
	{//in case the weaponmodel isn't _w, precache the _w.glm
		char weaponModel[64];
		CG_GetWorldModelName(&currWeaponMdl[0], &weaponModel[0],64);
		gi.G2API_PrecacheGhoul2Model(weaponModel);
	}

	if ( weaponInfo->weaponModel == 0 )
	{
		CG_Error( "Couldn't find weapon model '%s' for weapon %s(%d)\n", currWeaponMdl, weaponData[weaponNum].classname,weaponNum);
		return;
	}

	// calc midpoint for rotation
	cgi_R_ModelBounds( weaponInfo->weaponModel, mins, maxs );
	for ( i = 0 ; i < 3 ; i++ ) {
		weaponInfo->weaponMidpoint[i] = mins[i] + 0.5 * ( maxs[i] - mins[i] );
	}

	// setup the shader we will use for the icon
	if (weaponData[weaponNum].weaponIcon[0])
	{
		weaponInfo->weaponIcon = cgi_R_RegisterShaderNoMip(weaponData[weaponNum].weaponIcon);
		weaponInfo->weaponIconNoAmmo = cgi_R_RegisterShaderNoMip(va("%s_na", weaponData[weaponNum].weaponIcon));
	}

	for ( ammo = bg_itemlist + 1 ; ammo->classname ; ammo++ ) {
		if ( ammo->giType == IT_AMMO && ammo->giTag == weaponData[weaponNum].ammoIndex) {
			break;
		}
	}

	if ( ammo->classname && ammo->world_model ) {
		weaponInfo->ammoModel = cgi_R_RegisterModel( ammo->world_model );
	}

	for (i=0; i< weaponData[weaponNum].numBarrels; i++) {
		Q_strncpyz( path, currWeaponMdl, sizeof(path) );
		COM_StripExtension( path, path, sizeof(path) );
		if (i)
		{
			//char	crap[50];
			//Com_sprintf(crap, sizeof(crap), "_barrel%d.md3", i+1 );
			//strcat ( path, crap );
			Q_strcat( path, sizeof(path), va("_barrel%d.md3", i+1) );
		}
		else
			Q_strcat( path, sizeof(path), "_barrel.md3" );
		weaponInfo->barrelModel[i] = cgi_R_RegisterModel( path );
	}


	// set up the world model for the weapon
	weaponInfo->weaponWorldModel = cgi_R_RegisterModel( item->world_model );
	if ( !weaponInfo->weaponWorldModel) {
		weaponInfo->weaponWorldModel = weaponInfo->weaponModel;
	}

	// set up the hand that holds the in view weapon - assuming we have one
	Q_strncpyz( path, currWeaponMdl, sizeof(path) );
	COM_StripExtension( path, path, sizeof(path) );
	Q_strcat( path, sizeof(path), "_hand.md3" );
	weaponInfo->handsModel = cgi_R_RegisterModel( path );

	if ( !weaponInfo->handsModel ) {
		weaponInfo->handsModel = cgi_R_RegisterModel( "models/weapons2/briar_pistol/briar_pistol_hand.md3" );
	}


	// register weaponAttackInfo
	for (int i = 0; i < MAX_WEAPON_ATTACKS; i++) {
		if (weaponData[weaponNum].attackData[i].firingLogic == FL_FLAMETHROWER) {
			theFxScheduler.RegisterEffect("env/fire.efx");
			theFxScheduler.RegisterEffect("env/small_fire.efx");
		}
		// register the sounds for the weapon
		if (weaponData[weaponNum].attackData[i].startSnd[0]) {
			weaponInfo->weaponAttacksInfo[i].startSound = cgi_S_RegisterSound(weaponData[weaponNum].attackData[i].startSnd);
		}
		if (weaponData[weaponNum].attackData[i].firingSnd[0]) {
			weaponInfo->weaponAttacksInfo[i].firingSound = cgi_S_RegisterSound(weaponData[weaponNum].attackData[i].firingSnd);
		}
		if (weaponData[weaponNum].attackData[i].chargeSnd[0]) {
			weaponInfo->weaponAttacksInfo[i].chargeSound = cgi_S_RegisterSound(weaponData[weaponNum].attackData[i].chargeSnd);
		}
		if (weaponData[weaponNum].attackData[i].missileMdl[0]) {
			weaponInfo->weaponAttacksInfo[i].missileModel = cgi_R_RegisterModel(weaponData[weaponNum].attackData[i].missileMdl);
		}
		if (weaponData[weaponNum].attackData[i].missileSound[0]) {
			weaponInfo->weaponAttacksInfo[i].missileSound = cgi_S_RegisterSound(weaponData[weaponNum].attackData[i].missileSound);
		}
		if (weaponData[weaponNum].attackData[i].missileHitSound[0]) {
			weaponInfo->weaponAttacksInfo[i].missileHitSound = cgi_S_RegisterSound(weaponData[weaponNum].attackData[i].missileHitSound);
		}
		if (weaponData[weaponNum].attackData[i].muzzleEffect[0])
		{
			weaponInfo->weaponAttacksInfo[i].muzzleEffect = theFxScheduler.RegisterEffect(weaponData[weaponNum].attackData[i].muzzleEffect);
		}
		if (weaponData[weaponNum].attackData[i].chargeMuzzleShader[0])
		{
			weaponInfo->weaponAttacksInfo[i].chargeMuzzleShader = cgi_R_RegisterShader(weaponData[weaponNum].attackData[i].chargeMuzzleShader);
		}
		if (weaponData[weaponNum].attackData[i].projectileEffect[0])
		{
			weaponInfo->weaponAttacksInfo[i].projectileEffect = theFxScheduler.RegisterEffect(weaponData[weaponNum].attackData[i].projectileEffect);
		}
		if (weaponData[weaponNum].attackData[i].explosionEffect[0])
		{
			weaponInfo->weaponAttacksInfo[i].explosionEffect = theFxScheduler.RegisterEffect(weaponData[weaponNum].attackData[i].explosionEffect);
		}
		if (weaponData[weaponNum].attackData[i].shockwaveEffect[0])
		{
			weaponInfo->weaponAttacksInfo[i].shockwaveEffect = theFxScheduler.RegisterEffect(weaponData[weaponNum].attackData[i].shockwaveEffect);
		}
		if (weaponData[weaponNum].attackData[i].hitWallEffect[0])
		{
			weaponInfo->weaponAttacksInfo[i].hitWallEffect = theFxScheduler.RegisterEffect(weaponData[weaponNum].attackData[i].hitWallEffect);
		}
		else if (baseHitWallEffects[baseWeaponNum][0]) {
			weaponInfo->weaponAttacksInfo[i].hitWallEffect = theFxScheduler.RegisterEffect(baseHitWallEffects[baseWeaponNum]);
		}
		if (weaponData[weaponNum].attackData[i].hitWallEffect2[0])
		{
			weaponInfo->weaponAttacksInfo[i].hitWallEffect2 = theFxScheduler.RegisterEffect(weaponData[weaponNum].attackData[i].hitWallEffect2);
		}
		if (weaponData[weaponNum].attackData[i].hitWallEffect3[0])
		{
			weaponInfo->weaponAttacksInfo[i].hitWallEffect3 = theFxScheduler.RegisterEffect(weaponData[weaponNum].attackData[i].hitWallEffect3);
		}
		if (weaponData[weaponNum].attackData[i].hitDroidEffect[0])
		{
			weaponInfo->weaponAttacksInfo[i].hitDroidEffect = theFxScheduler.RegisterEffect(weaponData[weaponNum].attackData[i].hitDroidEffect);
		}
		if (weaponData[weaponNum].attackData[i].hitFleshEffect[0])
		{
			weaponInfo->weaponAttacksInfo[i].hitFleshEffect = theFxScheduler.RegisterEffect(weaponData[weaponNum].attackData[i].hitFleshEffect);
		}
		else if (baseHitFleshEffects[baseWeaponNum][0]) {
			weaponInfo->weaponAttacksInfo[i].hitFleshEffect = theFxScheduler.RegisterEffect(baseHitFleshEffects[baseWeaponNum]);
		}
		if (weaponData[weaponNum].attackData[i].missileFunc)
		{
			weaponInfo->weaponAttacksInfo[i].missileTrailFunc = (void (*)(struct centity_s*, const struct weaponInfo_s*))weaponData[weaponNum].attackData[i].missileFunc;
		}
		if (weaponData[weaponNum].attackData[i].beamShader[0])
		{
			cgi_R_RegisterShader(weaponData[weaponNum].attackData[i].beamShader);
		}
		if (weaponData[weaponNum].attackData[i].fullBeamShader[0])
		{
			cgi_R_RegisterShader(weaponData[weaponNum].attackData[i].fullBeamShader);
		}

	}
	if (weaponData[weaponNum].stopSnd[0]) {
		weaponInfo->stopSound = cgi_S_RegisterSound(weaponData[weaponNum].stopSnd);
	}
	if (weaponData[weaponNum].selectSnd[0]) {
		weaponInfo->selectSound = cgi_S_RegisterSound( weaponData[weaponNum].selectSnd );
	}
	if (weaponData[weaponNum].readySnd[0]) {
		weaponInfo->readySound = cgi_S_RegisterSound( weaponData[weaponNum].readySnd);
	}

	//Register a blank effect to overwrite the charging sound of dual pistols... Hate this hack...
	cgs.effects.blankEffect = theFxScheduler.RegisterEffect("misc/blank");

	switch (baseWeaponNum)	//extra client only stuff
	{
	case WP_SABER:
	{
		//saber/force FX
		theFxScheduler.RegisterEffect("sparks/spark_nosnd");//was "sparks/spark"
		theFxScheduler.RegisterEffect("sparks/blood_sparks2");
		theFxScheduler.RegisterEffect("force/force_touch");
		theFxScheduler.RegisterEffect("saber/saber_block");
		theFxScheduler.RegisterEffect("saber/saber_cut");
		//theFxScheduler.RegisterEffect( "saber/limb_bolton" );
		theFxScheduler.RegisterEffect("saber/fizz");
		theFxScheduler.RegisterEffect("saber/boil");
		//theFxScheduler.RegisterEffect( "saber/fire" );//was "sparks/spark"

		cgs.effects.forceHeal = theFxScheduler.RegisterEffect("force/heal");
		//cgs.effects.forceInvincibility	= theFxScheduler.RegisterEffect( "force/invin" );
		cgs.effects.forceConfusion = theFxScheduler.RegisterEffect("force/confusion");
		cgs.effects.forceLightning = theFxScheduler.RegisterEffect("force/lightning");
		cgs.effects.forceLightningWide = theFxScheduler.RegisterEffect("force/lightningwide");
		cgs.effects.redForceLightning = theFxScheduler.RegisterEffect("force/redlightning");
		cgs.effects.redForceLightningWide = theFxScheduler.RegisterEffect("force/redlightningwide");
		cgs.effects.orangeForceLightning = theFxScheduler.RegisterEffect("force/orangelightning");
		cgs.effects.orangeForceLightningWide = theFxScheduler.RegisterEffect("force/orangelightningwide");
		cgs.effects.yellowForceLightning = theFxScheduler.RegisterEffect("force/yellowlightning");
		cgs.effects.yellowForceLightningWide = theFxScheduler.RegisterEffect("force/yellowlightningwide");
		cgs.effects.greenForceLightning = theFxScheduler.RegisterEffect("force/greenlightning");
		cgs.effects.greenForceLightningWide = theFxScheduler.RegisterEffect("force/greenlightningwide");
		cgs.effects.purpleForceLightning = theFxScheduler.RegisterEffect("force/purplelightning");
		cgs.effects.purpleForceLightningWide = theFxScheduler.RegisterEffect("force/purplelightningwide");
		cgs.effects.whiteForceLightning = theFxScheduler.RegisterEffect("force/whitelightning");
		cgs.effects.whiteForceLightningWide = theFxScheduler.RegisterEffect("force/whitelightningwide");
		cgs.effects.blackForceLightning = theFxScheduler.RegisterEffect("force/blacklightning");
		cgs.effects.blackForceLightningWide = theFxScheduler.RegisterEffect("force/blacklightningwide");

		//new Jedi Academy force power effects
		cgs.effects.forceDrain = theFxScheduler.RegisterEffect("mp/drain");
		cgs.effects.forceDrainWide = theFxScheduler.RegisterEffect("mp/drainwide");
		//cgs.effects.forceDrained	= theFxScheduler.RegisterEffect( "mp/drainhit");

		cgs.effects.destructionProjectile = theFxScheduler.RegisterEffect("force/destruction");
		cgs.effects.destructionHit = theFxScheduler.RegisterEffect("force/dest_explosion");
		cgs.media.destructionSound = cgi_S_RegisterSound("sound/weapons/concussion/missleloop.wav");

		cgs.effects.blastProjectile = theFxScheduler.RegisterEffect("repeater/alt_projectile");
		cgs.effects.blastHit = theFxScheduler.RegisterEffect("force/blast");
		cgs.media.blastSound = cgi_S_RegisterSound("sound/weapons/force/absorbloop.wav");

		cgs.effects.strikeProjectile = theFxScheduler.RegisterEffect("env/huge_lightning");
		cgs.effects.strikeHit = theFxScheduler.RegisterEffect("env/small_fire_blue");
		cgs.media.strikeSound = cgi_S_RegisterSound("sound/weapons/explosions/explode5.wav");

		//saber sounds
		cgi_S_RegisterSound("sound/weapons/saber/saberonquick.wav");
		cgi_S_RegisterSound("sound/weapons/saber/saberspinoff.wav");
		cgi_S_RegisterSound("sound/weapons/saber/saberoffquick.wav");
		for (i = 1; i < 4; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/saber/saberbounce%d.wav", i));
		}
		for (i = 1; i < 4; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/saber/saberhit%d.wav", i));
		}
		for (i = 1; i < 4; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/saber/saberhitwall%d.wav", i));
		}
		for (i = 1; i < 10; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/saber/saberblock%d.wav", i));
		}
		for (i = 1; i < 10; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/saber/saberhup%d.wav", i));
		}
		for (i = 1; i < 4; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/saber/saberspin%d.wav", i));
		}
		cgi_S_RegisterSound("sound/weapons/saber/saber_catch.wav");
		for (i = 1; i < 4; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/saber/bounce%d.wav", i));
		}
		cgi_S_RegisterSound("sound/weapons/saber/hitwater.wav");
		cgi_S_RegisterSound("sound/weapons/saber/boiling.wav");
		for (i = 1; i < 4; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/saber/rainfizz%d.wav", i));
		}
		cgi_S_RegisterSound("sound/movers/objects/saber_slam");

		//force sounds
		cgi_S_RegisterSound("sound/weapons/force/heal.mp3");
		cgi_S_RegisterSound("sound/weapons/force/speed.mp3");
		cgi_S_RegisterSound("sound/weapons/force/speedloop.mp3");
		for (i = 1; i < 5; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/force/heal%d.mp3", i));
			cgi_S_RegisterSound(va("sound/weapons/force/heal%d_m.mp3", i));
			cgi_S_RegisterSound(va("sound/weapons/force/heal%d_f.mp3", i));
		}
		cgi_S_RegisterSound("sound/weapons/force/lightning.wav");
		cgi_S_RegisterSound("sound/weapons/force/lightning2.wav");
		for (i = 1; i < 4; i++)
		{
			cgi_S_RegisterSound(va("sound/weapons/force/lightninghit%d.wav", i));
		}
		cgi_S_RegisterSound("sound/weapons/force/push.wav");
		cgi_S_RegisterSound("sound/weapons/force/pull.wav");
		cgi_S_RegisterSound("sound/weapons/force/jump.wav");
		cgi_S_RegisterSound("sound/weapons/force/jumpbuild.wav");
		cgi_S_RegisterSound("sound/weapons/force/grip.mp3");
		//new Jedi Academy force sounds
		cgi_S_RegisterSound("sound/weapons/force/absorb.mp3");
		cgi_S_RegisterSound("sound/weapons/force/absorbhit.mp3");
		cgi_S_RegisterSound("sound/weapons/force/absorbloop.mp3");
		cgi_S_RegisterSound("sound/weapons/force/protect.mp3");
		cgi_S_RegisterSound("sound/weapons/force/protecthit.mp3");
		cgi_S_RegisterSound("sound/weapons/force/protectloop.mp3");
		cgi_S_RegisterSound("sound/weapons/force/rage.mp3");
		cgi_S_RegisterSound("sound/weapons/force/ragehit.mp3");
		cgi_S_RegisterSound("sound/weapons/force/rageloop.mp3");
		cgi_S_RegisterSound("sound/weapons/force/see.mp3");
		cgi_S_RegisterSound("sound/weapons/force/seeloop.mp3");
		cgi_S_RegisterSound("sound/weapons/force/drain.mp3");
		cgi_S_RegisterSound("sound/weapons/force/drained.mp3");
		//force graphics
		cgs.media.playerShieldDamage = cgi_R_RegisterShader("gfx/misc/personalshield");
		//cgs.media.forceSightBubble = cgi_R_RegisterShader("gfx/misc/sightbubble");
		//cgs.media.forceShell = cgi_R_RegisterShader("powerups/forceshell");
		cgs.media.forceShell = cgi_R_RegisterShader("gfx/misc/forceprotect");
		cgs.media.sightShell = cgi_R_RegisterShader("powerups/sightshell");
		cgi_R_RegisterShader("gfx/2d/jsense");
		//force effects - FIXME: only if someone has these powers?
		theFxScheduler.RegisterEffect("force/rage2");
		//theFxScheduler.RegisterEffect( "force/heal_joint" );
		theFxScheduler.RegisterEffect("force/heal2");
		theFxScheduler.RegisterEffect("force/drain_hand");

		//saber graphics
		cgs.media.saberBlurShader = cgi_R_RegisterShader("gfx/effects/sabers/saberBlur");
		cgs.media.swordTrailShader = cgi_R_RegisterShader("gfx/effects/sabers/swordTrail");
		cgs.media.yellowDroppedSaberShader = cgi_R_RegisterShader("gfx/effects/yellow_glow");
		cgi_R_RegisterShader("gfx/effects/saberDamageGlow");
		cgi_R_RegisterShader("gfx/effects/solidWhite_cull");
		cgi_R_RegisterShader("gfx/effects/forcePush");
		cgi_R_RegisterShader("gfx/effects/saberFlare");
		cgs.media.redSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/red_glow");
		cgs.media.redSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/red_line");
		cgs.media.orangeSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/orange_glow");
		cgs.media.orangeSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/orange_line");
		cgs.media.yellowSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/yellow_glow");
		cgs.media.yellowSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/yellow_line");
		cgs.media.greenSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/green_glow");
		cgs.media.greenSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/green_line");
		cgs.media.blueSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/blue_glow");
		cgs.media.blueSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/blue_line");
		cgs.media.purpleSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/purple_glow");
		cgs.media.purpleSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/purple_line");
		cgs.media.unstableRedSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/unstable_red_glow");
		cgs.media.unstableRedSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/unstable_red_line");
		cgs.media.blackSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/black_glow");
		cgs.media.blackSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/black_line");
		cgs.media.blackSaberBlurShader = cgi_R_RegisterShader("gfx/effects/sabers/blackSaberBlur");
		cgs.media.rgbSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/rgb_glow");
		cgs.media.rgbSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/rgb_line");
		cgs.media.darkSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/darksaber_glow");
		cgs.media.darkSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/darksaber_line");
		cgs.media.darkSaberCoreShader = cgi_R_RegisterShader("gfx/effects/sabers/darksabercore");
		cgs.media.darkSaberGlowShader = cgi_R_RegisterShader("gfx/effects/sabers/darksaberglow");

		cgs.media.forceCoronaShader = cgi_R_RegisterShaderNoMip("gfx/hud/force_swirl");

		//new Jedi Academy force graphics
		cgs.media.drainShader = cgi_R_RegisterShader("gfx/misc/redLine");

		//for grip slamming into walls
		theFxScheduler.RegisterEffect("env/impact_dustonly");
		cgi_S_RegisterSound("sound/weapons/melee/punch1.mp3");
		cgi_S_RegisterSound("sound/weapons/melee/punch2.mp3");
		cgi_S_RegisterSound("sound/weapons/melee/punch3.mp3");
		cgi_S_RegisterSound("sound/weapons/melee/punch4.mp3");

		//For kicks with saber staff...
		theFxScheduler.RegisterEffect("melee/kick_impact");

		//Kothos beam
		cgi_R_RegisterShader("gfx/misc/dr1");
		break;
	}
	case WP_BRYAR_PISTOL:
	case WP_BLASTER_PISTOL:
	case WP_JAWA:
	case WP_REY:
		cgs.effects.bryarShotEffect			= theFxScheduler.RegisterEffect( "bryar/shot" );
											theFxScheduler.RegisterEffect( "bryar/NPCshot" );
		cgs.effects.bryarPowerupShotEffect	= theFxScheduler.RegisterEffect( "bryar/crackleShot" );
		cgs.effects.bryarWallImpactEffect2	= theFxScheduler.RegisterEffect( "bryar/wall_impact2" );
		cgs.effects.bryarWallImpactEffect3	= theFxScheduler.RegisterEffect( "bryar/wall_impact3" );

		// Note....these are temp shared effects
		theFxScheduler.RegisterEffect( "blaster/deflect" );
		theFxScheduler.RegisterEffect( "blaster/smoke_bolton" );
		break;
	case WP_BLASTER:
	case WP_BATTLEDROID:
	case WP_THEFIRSTORDER:
	case WP_REBELBLASTER:
	case WP_REBELRIFLE:
	case WP_JANGO:
	case WP_SBD:
	case WP_DROIDEKA:
	case WP_CIS_SNIPER:
		cgs.effects.blasterShotEffect = theFxScheduler.RegisterEffect( "blaster/shot" );
		theFxScheduler.RegisterEffect( "blaster/NPCshot" );
		theFxScheduler.RegisterEffect( "blaster/deflect" );
		theFxScheduler.RegisterEffect( "blaster/smoke_bolton" );
		break;
	case WP_DISRUPTOR:
		theFxScheduler.RegisterEffect( "disruptor/alt_miss" );
		theFxScheduler.RegisterEffect( "disruptor/alt_hit" );
		theFxScheduler.RegisterEffect( "disruptor/line_cap" );
		theFxScheduler.RegisterEffect( "disruptor/death_smoke" );

		cgi_R_RegisterShader( "gfx/effects/redLine" );
		cgi_R_RegisterShader( "gfx/misc/whiteline2" );
		cgi_R_RegisterShader( "gfx/effects/smokeTrail" );
		cgi_R_RegisterShader( "gfx/effects/burn" );

		cgi_R_RegisterShaderNoMip( "gfx/2d/crop_charge" );

		// zoom sounds
		cgi_S_RegisterSound( "sound/weapons/disruptor/zoomstart.wav" );
		cgi_S_RegisterSound( "sound/weapons/disruptor/zoomend.wav" );
		cgs.media.disruptorZoomLoop = cgi_S_RegisterSound( "sound/weapons/disruptor/zoomloop.wav" );

		// Disruptor gun zoom interface
		cgs.media.disruptorMask			= cgi_R_RegisterShader( "gfx/2d/cropCircle2");
		cgs.media.disruptorInsert		= cgi_R_RegisterShader( "gfx/2d/cropCircle");
		cgs.media.disruptorLight		= cgi_R_RegisterShader( "gfx/2d/cropCircleGlow" );
		cgs.media.disruptorInsertTick	= cgi_R_RegisterShader( "gfx/2d/insertTick" );
		break;

	case WP_BOWCASTER:
		cgs.effects.bowcasterShotEffect		= theFxScheduler.RegisterEffect( "bowcaster/shot" );
		cgs.effects.bowcasterBounceEffect	= theFxScheduler.RegisterEffect( "bowcaster/bounce_wall" );
		theFxScheduler.RegisterEffect( "bowcaster/deflect" );
		break;

	case WP_REPEATER:
		theFxScheduler.RegisterEffect( "repeater/muzzle_smoke" );
		theFxScheduler.RegisterEffect( "repeater/projectile" );
		theFxScheduler.RegisterEffect( "repeater/alt_projectile" );
		break;

	case WP_DEMP2:
		theFxScheduler.RegisterEffect( "demp2/projectile" );
		theFxScheduler.RegisterEffect( "demp2/altDetonate" );
		cgi_R_RegisterModel( "models/items/sphere.md3" );
		cgi_R_RegisterShader( "gfx/effects/demp2shell" );
		break;

	case WP_ATST_MAIN:
		theFxScheduler.RegisterEffect( "atst/shot" );
		theFxScheduler.RegisterEffect( "atst/wall_impact" );
		theFxScheduler.RegisterEffect( "atst/flesh_impact" );
		theFxScheduler.RegisterEffect( "atst/droid_impact" );
		cgs.media.emplacedHealthBarShader = cgi_R_RegisterShaderNoMip("gfx/hud/health_frame");
		cgs.media.turretComputerOverlayShader = cgi_R_RegisterShaderNoMip("gfx/hud/generic_target");
		cgs.media.turretCrossHairShader = cgi_R_RegisterShaderNoMip("gfx/2d/panel_crosshair");
		break;

	case WP_ATST_SIDE:
		// For the ALT fire
		theFxScheduler.RegisterEffect( "atst/side_alt_shot" );
		theFxScheduler.RegisterEffect( "atst/side_alt_explosion" );

		// For the regular fire
		theFxScheduler.RegisterEffect( "atst/side_main_shot" );
		theFxScheduler.RegisterEffect( "atst/side_main_impact" );
		break;

	case WP_FLECHETTE:
		cgs.effects.flechetteShotEffect				= theFxScheduler.RegisterEffect( "flechette/shot" );
		cgs.effects.flechetteAltShotEffect			= theFxScheduler.RegisterEffect( "flechette/alt_shot" );
		cgs.effects.flechetteRicochetEffect			= theFxScheduler.RegisterEffect( "flechette/ricochet" );
		theFxScheduler.RegisterEffect( "flechette/alt_blow" );
		break;

	case WP_ROCKET_LAUNCHER:
		theFxScheduler.RegisterEffect( "rocket/shot" );
		theFxScheduler.RegisterEffect( "rocket/explosion" );

		cgi_R_RegisterShaderNoMip( "gfx/2d/wedge" );
		cgi_R_RegisterShaderNoMip( "gfx/2d/lock" );

		cgi_S_RegisterSound( "sound/weapons/rocket/lock.wav" );
		cgi_S_RegisterSound( "sound/weapons/rocket/tick.wav" );
		break;

	case WP_CONCUSSION:
		//Primary
		theFxScheduler.RegisterEffect( "concussion/shot" );
		theFxScheduler.RegisterEffect( "concussion/explosion" );
		//Alt
		theFxScheduler.RegisterEffect( "concussion/alt_miss" );
		theFxScheduler.RegisterEffect( "concussion/alt_hit" );
		theFxScheduler.RegisterEffect( "concussion/alt_ring" );
		//not used (eventually)?
		cgi_R_RegisterShader( "gfx/effects/blueLine" );
		cgi_R_RegisterShader( "gfx/misc/whiteline2" );
		break;

	case WP_THERMAL:
		cgs.media.grenadeBounce1		= cgi_S_RegisterSound( "sound/weapons/thermal/bounce1.wav" );
		cgs.media.grenadeBounce2		= cgi_S_RegisterSound( "sound/weapons/thermal/bounce2.wav" );

		cgi_S_RegisterSound( "sound/weapons/thermal/thermloop.wav" );
		cgi_S_RegisterSound( "sound/weapons/thermal/warning.wav" );
		theFxScheduler.RegisterEffect( "thermal/explosion" );
		theFxScheduler.RegisterEffect( "thermal/shockwave" );
		break;

	case WP_TRIP_MINE:
		theFxScheduler.RegisterEffect( "tripMine/explosion" );
		theFxScheduler.RegisterEffect( "tripMine/laser" );
		theFxScheduler.RegisterEffect( "tripMine/laserImpactGlow" );
		theFxScheduler.RegisterEffect( "tripMine/glowBit" );

		cgs.media.tripMineStickSound = cgi_S_RegisterSound( "sound/weapons/laser_trap/stick.wav" );
		cgi_S_RegisterSound( "sound/weapons/laser_trap/warning.wav" );
		cgi_S_RegisterSound( "sound/weapons/laser_trap/hum_loop.wav" );
		break;

	case WP_DET_PACK:
		theFxScheduler.RegisterEffect( "detpack/explosion.efx" );

		cgs.media.detPackStickSound = cgi_S_RegisterSound( "sound/weapons/detpack/stick.wav" );
		cgi_R_RegisterModel( "models/weapons2/detpack/detpack.md3" );
		cgi_S_RegisterSound( "sound/weapons/detpack/warning.wav" );
		cgi_S_RegisterSound( "sound/weapons/explosions/explode5.wav" );
		break;

	case WP_EMPLACED_GUN:
		theFxScheduler.RegisterEffect( "emplaced/shot" );
		theFxScheduler.RegisterEffect( "emplaced/shotNPC" );
		theFxScheduler.RegisterEffect( "emplaced/wall_impact" );
		//E-Web, too, can't tell here which one you wanted, so...
		theFxScheduler.RegisterEffect( "eweb/shot" );
		theFxScheduler.RegisterEffect( "eweb/shotNPC" );

		cgi_R_RegisterShader( "models/map_objects/imp_mine/turret_chair_dmg" );
		cgi_R_RegisterShader( "models/map_objects/imp_mine/turret_chair_on" );

		cgs.media.emplacedHealthBarShader		= cgi_R_RegisterShaderNoMip( "gfx/hud/health_frame" );
		cgs.media.turretComputerOverlayShader	= cgi_R_RegisterShaderNoMip( "gfx/hud/generic_target" );
		cgs.media.turretCrossHairShader			= cgi_R_RegisterShaderNoMip( "gfx/2d/panel_crosshair" );
		break;

	case WP_MELEE:
	case WP_TUSKEN_STAFF:
		//TEMP
		theFxScheduler.RegisterEffect( "melee/punch_impact" );
		theFxScheduler.RegisterEffect( "melee/kick_impact" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch1.mp3" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch2.mp3" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch3.mp3" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch4.mp3" );
		break;

	case WP_STUN_BATON:
		cgi_R_RegisterShader( "gfx/effects/stunPass" );
		theFxScheduler.RegisterEffect( "stunBaton/flesh_impact" );
		//TEMP
		cgi_S_RegisterSound( "sound/weapons/melee/punch1.mp3" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch2.mp3" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch3.mp3" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch4.mp3" );
		cgi_S_RegisterSound( "sound/weapons/baton/fire" );
		break;

	case WP_TURRET:
		theFxScheduler.RegisterEffect( "turret/shot" );
		theFxScheduler.RegisterEffect( "turret/wall_impact" );
		theFxScheduler.RegisterEffect( "turret/flesh_impact" );
		cgs.media.emplacedHealthBarShader = cgi_R_RegisterShaderNoMip("gfx/hud/health_frame");
		cgs.media.turretComputerOverlayShader = cgi_R_RegisterShaderNoMip("gfx/hud/generic_target");
		cgs.media.turretCrossHairShader = cgi_R_RegisterShaderNoMip("gfx/2d/panel_crosshair");
		break;

	case WP_TUSKEN_RIFLE:
		//melee
		theFxScheduler.RegisterEffect( "melee/punch_impact" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch1.mp3" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch2.mp3" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch3.mp3" );
		cgi_S_RegisterSound( "sound/weapons/melee/punch4.mp3" );
		//fire
		theFxScheduler.RegisterEffect( "tusken/shot" );

		break;

	case WP_SCEPTER:
		//???
		break;

	case WP_NOGHRI_STICK:
		//fire
		theFxScheduler.RegisterEffect( "noghri_stick/shot" );
		//explosion
		theFxScheduler.RegisterEffect( "noghri_stick/gas_cloud" );
		//cgi_S_RegisterSound("sound/weapons/noghri/smoke.wav");
		break;

	case WP_TIE_FIGHTER:
		theFxScheduler.RegisterEffect( "ships/imp_blastershot" );
		cgs.media.emplacedHealthBarShader = cgi_R_RegisterShaderNoMip("gfx/hud/health_frame");
		cgs.media.turretComputerOverlayShader = cgi_R_RegisterShaderNoMip("gfx/hud/generic_target");
		cgs.media.turretCrossHairShader = cgi_R_RegisterShaderNoMip("gfx/2d/panel_crosshair");
		break;

	case WP_CLONECARBINE:
	case WP_CLONERIFLE:
	case WP_CLONEPISTOL:
	case WP_CLONECOMMANDO:
		if (weaponNum == WP_CLONECOMMANDO)
		{
			theFxScheduler.RegisterEffect("dc17/shot");
			theFxScheduler.RegisterEffect("dc17/explosion");
			cgi_R_RegisterShader("gfx/effects/blueline");
		}
		cgs.effects.cloneShotEffect = theFxScheduler.RegisterEffect("clone/projectile");
		cgs.effects.cloneWallImpactEffect = theFxScheduler.RegisterEffect("clone/wall_impact");
		cgs.effects.cloneFleshImpactEffect = theFxScheduler.RegisterEffect("clone/flesh_impact");
		break;
	}
}

/*
=================
CG_RegisterItemVisuals

The server says this item is used on this level
=================
*/
void CG_RegisterItemVisuals( int itemNum ) {
	itemInfo_t		*itemInfo;
	gitem_t			*item;

	itemInfo = &cg_items[ itemNum ];
	if ( itemInfo->registered ) {
		return;
	}

	item = &bg_itemlist[ itemNum ];

	memset( itemInfo, 0, sizeof( *itemInfo ) );
	itemInfo->registered = qtrue;

	itemInfo->models = cgi_R_RegisterModel( item->world_model );

	if ( item->icon && item->icon[0] )
	{
		itemInfo->icon = cgi_R_RegisterShaderNoMip( item->icon );
	}
	else
	{
		itemInfo->icon = -1;
	}

	if ( item->giType == IT_WEAPON )
	{
		if (item->giTag >= 0) {
			CG_RegisterWeapon(item->giTag);
		}
		//Case dynamic weapons
		else if (item->giTag < 0) {
			int _wpnum;
			for (_wpnum = 0; _wpnum < weaponCount; _wpnum++) {
				if(!Q_stricmp(item->giTagName,weaponData[_wpnum].classname)){
					CG_RegisterWeapon(_wpnum);
					break;
				}
			}
		}
	}

	// some ammo types are actually the weapon, like in the case of explosives
	if ( item->giType == IT_AMMO )
	{
		switch( item->giTag )
		{
		case AMMO_THERMAL:
			CG_RegisterWeapon( WP_THERMAL );
			break;
		case AMMO_TRIPMINE:
			CG_RegisterWeapon( WP_TRIP_MINE );
			break;
		case AMMO_DETPACK:
			CG_RegisterWeapon( WP_DET_PACK );
			break;
		}

		if (item->giTag && ammoData[item->giTag].giveWeaponIndex) {
			CG_RegisterWeapon(ammoData[item->giTag].giveWeaponIndex);
		}
	}


	if ( item->giType == IT_HOLDABLE )
	{
		// This should be set up to actually work.
		switch( item->giTag )
		{
		case INV_SEEKER:
			cgi_S_RegisterSound("sound/chars/seeker/misc/fire.wav");
			cgi_S_RegisterSound( "sound/chars/seeker/misc/hiss.wav");
			theFxScheduler.RegisterEffect( "env/small_explode");

			CG_RegisterWeapon( WP_BLASTER );
			break;

		case INV_SENTRY:
			CG_RegisterWeapon( WP_TURRET );
			cgi_S_RegisterSound( "sound/player/use_sentry" );
			break;

		case INV_ELECTROBINOCULARS:
			// Binocular interface
			cgs.media.binocularCircle		= cgi_R_RegisterShader( "gfx/2d/binCircle" );
			cgs.media.binocularMask			= cgi_R_RegisterShader( "gfx/2d/binMask" );
			cgs.media.binocularArrow		= cgi_R_RegisterShader( "gfx/2d/binSideArrow" );
			cgs.media.binocularTri			= cgi_R_RegisterShader( "gfx/2d/binTopTri" );
			cgs.media.binocularStatic		= cgi_R_RegisterShader( "gfx/2d/binocularWindow" );
			cgs.media.binocularOverlay		= cgi_R_RegisterShader( "gfx/2d/binocularNumOverlay" );
			break;

		case INV_LIGHTAMP_GOGGLES:
			// LA Goggles Shaders
			cgs.media.laGogglesStatic		= cgi_R_RegisterShader( "gfx/2d/lagogglesWindow" );
			cgs.media.laGogglesMask			= cgi_R_RegisterShader( "gfx/2d/amp_mask" );
			cgs.media.laGogglesSideBit		= cgi_R_RegisterShader( "gfx/2d/side_bit" );
			cgs.media.laGogglesBracket		= cgi_R_RegisterShader( "gfx/2d/bracket" );
			cgs.media.laGogglesArrow		= cgi_R_RegisterShader( "gfx/2d/bracket2" );
			break;

		case INV_BACTA_CANISTER:
			for ( int i = 1; i < 5; i++ )
			{
				cgi_S_RegisterSound(va("sound/weapons/force/heal%d.mp3", i));
				cgi_S_RegisterSound( va( "sound/weapons/force/heal%d_m.mp3", i ) );
				cgi_S_RegisterSound( va( "sound/weapons/force/heal%d_f.mp3", i ) );
			}
			break;
		}
	}
}


/*
========================================================================================

VIEW WEAPON

========================================================================================
*/

/*
=================
CG_MapTorsoToWeaponFrame

animations MUST match the defined pattern!
the weapon hand animation has 3 anims,
	6 frames of attack
	4 frames of drop
	5 frames of raise

  if the torso anim does not match these lengths, it will not animate correctly!
=================
*/
extern qboolean ValidAnimFileIndex ( int index );
int CG_MapTorsoToWeaponFrame( const clientInfo_t *ci, int frame, int animNum, int weaponNum, int firing )
{
	// we should use the animNum to map a weapon frame instead of relying on the torso frame
	if ( !ValidAnimFileIndex( ci->animFileIndex ) )
	{
		return 0;
	}
	animation_t *animations = level.knownAnimFileSets[ci->animFileIndex].animations;
	int ret=0;

	switch( animNum )
	{
	case TORSO_WEAPONREADY1:
	case TORSO_WEAPONREADY2:
	case TORSO_WEAPONREADY3:
	case TORSO_WEAPONREADY4:
	case TORSO_WEAPONREADY10:
		ret = 0;
		break;

	case TORSO_DROPWEAP1:
		if ( frame >= animations[animNum].firstFrame && frame < animations[animNum].firstFrame + 5 )
		{
			ret = frame - animations[animNum].firstFrame + 6;
		}
		else
		{
//			assert(0);
		}
		break;

	case TORSO_RAISEWEAP1:
		if ( frame >= animations[animNum].firstFrame && frame < animations[animNum].firstFrame + 4 )
		{
			ret = frame - animations[animNum].firstFrame + 6 + 5;
		}
		else
		{
//			assert(0);
		}
		break;

	case BOTH_ATTACK1:
	case BOTH_ATTACK2:
	case BOTH_ATTACK3:
	case BOTH_ATTACK4:
		if ( frame >= animations[animNum].firstFrame && frame < animations[animNum].firstFrame + 6 )
		{
			ret = 1 + ( frame - animations[animNum].firstFrame );
		}
		else
		{
//			assert(0);
		}
		break;
	default:
		break;
	}

	return ret;
}

/*
==============
CG_CalculateWeaponPosition
==============
*/
void CG_CalculateWeaponPosition( vec3_t origin, vec3_t angles )
{
	float	scale;
	int		delta;
	float	fracsin;

	VectorCopy( cg.refdef.vieworg, origin );
	VectorCopy( cg.refdefViewAngles, angles );

	// on odd legs, invert some angles
	if ( cg.bobcycle & 1 ) {
		scale = -cg.xyspeed;
	} else {
		scale = cg.xyspeed;
	}

	// gun angles from bobbing
	angles[ROLL] += scale * cg.bobfracsin * 0.0075;
	angles[YAW] += scale * cg.bobfracsin * 0.01;
	angles[PITCH] += cg.xyspeed * cg.bobfracsin * 0.0075;

	// drop the weapon when landing
	delta = cg.time - cg.landTime;
	if ( delta < LAND_DEFLECT_TIME ) {
		origin[2] += cg.landChange*0.25 * delta / LAND_DEFLECT_TIME;
	} else if ( delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME ) {
		origin[2] += cg.landChange*0.25 *
			(LAND_DEFLECT_TIME + LAND_RETURN_TIME - delta) / LAND_RETURN_TIME;
	}

#if 0
	// drop the weapon when stair climbing
	delta = cg.time - cg.stepTime;
	if ( delta < STEP_TIME/2 ) {
		origin[2] -= cg.stepChange*0.25 * delta / (STEP_TIME/2);
	} else if ( delta < STEP_TIME ) {
		origin[2] -= cg.stepChange*0.25 * (STEP_TIME - delta) / (STEP_TIME/2);
	}
#endif

	// idle drift
	scale = /*cg.xyspeed + */40;
	fracsin = sin( cg.time * 0.001 );
	angles[ROLL] += scale * fracsin * 0.01;
	angles[YAW] += scale * fracsin * 0.01;
	angles[PITCH] += (scale * 0.5f ) * fracsin * 0.01;
}

/*
======================
CG_MachinegunSpinAngle
======================
*/
/*
#define		SPIN_SPEED	0.9
#define		COAST_TIME	1000
static float	CG_MachinegunSpinAngle( centity_t *cent ) {
	int		delta;
	float	angle;
	float	speed;

	delta = cg.time - cent->pe.barrelTime;
	if ( cent->pe.barrelSpinning ) {
		angle = cent->pe.barrelAngle + delta * SPIN_SPEED;
	} else {
		if ( delta > COAST_TIME ) {
			delta = COAST_TIME;
		}

		speed = 0.5 * ( SPIN_SPEED + (float)( COAST_TIME - delta ) / COAST_TIME );
		angle = cent->pe.barrelAngle + delta * speed;
	}

	if ( cent->pe.barrelSpinning == !(cent->currentState.eFlags & EF_FIRING) ) {
		cent->pe.barrelTime = cg.time;
		cent->pe.barrelAngle = AngleNormalize360( angle );
		cent->pe.barrelSpinning = !!(cent->currentState.eFlags & EF_FIRING);
	}

	return angle;
}
*/
/*
Ghoul2 Insert Start
*/
// set up the appropriate ghoul2 info to a refent
void CG_SetGhoul2InfoRef( refEntity_t *ent, refEntity_t	*s1)
{
	ent->ghoul2 = s1->ghoul2;
	VectorCopy( s1->modelScale, ent->modelScale);
	ent->radius = s1->radius;
	VectorCopy( s1->angles, ent->angles);
}

qboolean CG_IsChargedAttack(centity_t* cent) 
{
	int weaponNum = cent->gent->s.weapon;
	int attackIndex = CG_GetAttackIndex(cent->gent, cent->altFire);
	weaponAttackData_t *attackData = &weaponData[weaponNum].attackData[attackIndex];
	if (attackData->firingLogic == FL_BEAM_CHARGED
		|| attackData->firingLogic == FL_BOWCASTER
		|| attackData->firingLogic == FL_DEMP2_ALT
		|| attackData->firingLogic == FL_BLASTER_CHARGED
		) {
		return qtrue;
	}
	return qfalse;
}

const char* CG_GetMuzzleEffect(const centity_t* cent, const weaponData_t* wData) {
	const char* effect = NULL;
		
	int attackIndex = CG_GetAttackIndex(cent->gent, cent->altFire);
	// I declared this variable just for readability.

	//If I can't fire cause I'm underwater, don't play the effect.
	if (!WP_checkWaterFire(cent->gent, &wData->attackData[attackIndex]))
	{
		return effect;
	}
	// Try and get a default muzzle so we have one to fall back on
	if ( wData->attackData[attackIndex].muzzleEffect[0])
	{
		effect = &wData->attackData[attackIndex].muzzleEffect[0];
	}
	else if (wData->attackData[0].muzzleEffect[0])
	{
		// We need to make sure that the base guns also get their sound.
		effect = &wData->attackData[0].muzzleEffect[0];
	}

	return effect;
}

//--------------------------------------------------------------------------
static void CG_DoMuzzleFlash( centity_t *cent, vec3_t org, vec3_t dir, weaponData_t *wData )
{
	// Handle muzzle flashes, really this could just be a qboolean instead of a time.......
	if ( cent->muzzleFlashTime > 0 )
	{
		cent->muzzleFlashTime  = 0;

		const char* effect = CG_GetMuzzleEffect(cent, wData);
		if (effect)
		{
			if (( cent->gent && cent->gent->NPC ) || cg.renderingThirdPerson )
			{
				theFxScheduler.PlayEffect( effect, org, dir );
			}
			else
			{
				// We got an effect and we're firing, so let 'er rip.
				theFxScheduler.PlayEffect( effect, cent->currentState.clientNum );
			}
		}
	}
}

/*
Ghoul2 Insert End
*/

/*
==============
CG_AddViewWeapon

Add the weapon, and flash for the player's view
==============
*/
extern int PM_TorsoAnimForFrame( gentity_t *ent, int torsoFrame );
extern float CG_ForceSpeedFOV( void );
extern fxHandle_t CG_GetWideForceLightning(centity_t* const cent);
extern fxHandle_t CG_GetForceLightning(centity_t* const cent);
void CG_AddViewWeapon( playerState_t *ps )
{
	refEntity_t	hand;
	refEntity_t	flash;
	vec3_t		angles;
	const weaponInfo_t	*weapon;
	weaponData_t  *wData;
	centity_t	*cent;
	float		fovOffset, leanOffset;

	// no gun if in third person view
	if ( cg.renderingThirdPerson )
		return;
	
	if ( (cg_trueguns.integer || CG_ChangeFirstPersonView()) && !cg.zoomMode )
		return;

	if ( ps->pm_type == PM_INTERMISSION )
		return;

	cent = &cg_entities[ps->clientNum];

	if ( ps->eFlags & EF_LOCKED_TO_WEAPON )
	{
		return;
	}

	if ( cent->gent && cent->gent->client && cent->gent->client->ps.forcePowersActive&(1<<FP_LIGHTNING) )
	{//doing the electrocuting
		vec3_t temp;//tAng, fxDir,
		//VectorSet( tAng, cent->pe.torso.pitchAngle, cent->pe.torso.yawAngle, 0 );

		VectorCopy( cent->gent->client->renderInfo.handLPoint, temp );
		VectorMA( temp, -5, cg.refdef.viewaxis[0], temp );
		if ( cent->gent->client->ps.forcePowerLevel[FP_LIGHTNING] > FORCE_LEVEL_2 )
		{//arc
			//vec3_t	fxAxis[3];
			//AnglesToAxis( tAng, fxAxis );
			theFxScheduler.PlayEffect( CG_GetWideForceLightning(cent), temp, cg.refdef.viewaxis );
		}
		else
		{//line
			//AngleVectors( tAng, fxDir, NULL, NULL );
			theFxScheduler.PlayEffect(CG_GetForceLightning(cent), temp, cg.refdef.viewaxis[0] );
		}
	}

	if ( cent->gent && cent->gent->client && cent->gent->client->ps.forcePowersActive&(1<<FP_DRAIN) )
	{//doing the draining
		vec3_t temp;//tAng, fxDir,
		//VectorSet( tAng, cent->pe.torso.pitchAngle, cent->pe.torso.yawAngle, 0 );

		VectorCopy( cent->gent->client->renderInfo.handLPoint, temp );
		VectorMA( temp, -5, cg.refdef.viewaxis[0], temp );
		if ( cent->gent->client->ps.forcePowerLevel[FP_DRAIN] > FORCE_LEVEL_2 )
		{//arc
			//vec3_t	fxAxis[3];
			//AnglesToAxis( tAng, fxAxis );
			theFxScheduler.PlayEffect( cgs.effects.forceDrainWide, temp, cg.refdef.viewaxis );
		}
		else
		{//line
			//AngleVectors( tAng, fxDir, NULL, NULL );
			theFxScheduler.PlayEffect( cgs.effects.forceDrain, temp, cg.refdef.viewaxis[0] );
		}
	}

	// allow the gun to be completely removed
	if ( !cg_drawGun.integer || cg.zoomMode )
	{
		vec3_t		origin;

		// special hack for lightning guns...
		VectorCopy( cg.refdef.vieworg, origin );
		VectorMA( origin, -10, cg.refdef.viewaxis[2], origin );
		VectorMA( origin, 16, cg.refdef.viewaxis[0], origin );
// Doesn't look like we'll have lightning style guns.  Clean this crap up when we are sure about this.
//		CG_LightningBolt( cent, origin );

		// We should still do muzzle flashes though...
		CG_RegisterWeapon( ps->weapon );
		weapon = &cg_weapons[ps->weapon];
		wData =  &weaponData[ps->weapon];

		CG_DoMuzzleFlash( cent, origin, cg.refdef.viewaxis[0], wData );

		// If we don't update this, the muzzle flash point won't even be updated properly
		VectorCopy( origin, cent->gent->client->renderInfo.muzzlePoint );
		VectorCopy( cg.refdef.viewaxis[0], cent->gent->client->renderInfo.muzzleDir );

		cent->gent->client->renderInfo.mPCalcTime = cg.time;
		return;
	}

	// drop gun lower at higher fov
	float actualFOV;
	if ( (cg.snap->ps.forcePowersActive&(1<<FP_SPEED)) && player->client->ps.forcePowerDuration[FP_SPEED] )//cg.renderingThirdPerson &&
	{
		actualFOV = CG_ForceSpeedFOV();
	}
	else
	{
		if ( cg.overrides.active & CG_OVERRIDE_FOV )
			actualFOV = cg.overrides.fov;
		else {
			actualFOV = cg_fovViewmodel.integer ? cg_fovViewmodel.value : cg_fov.value;
		}
	}

	if ( cg_fovViewmodelAdjust.integer && actualFOV > 90 )
		fovOffset = -0.1 * ( actualFOV - 80 );
	else
		fovOffset = 0;

	if ( ps->leanofs != 0 )
	{	//add leaning offset
		leanOffset = ps->leanofs * 0.25f;
		fovOffset += abs(ps->leanofs) * -0.1f;
	}
	else
	{
		leanOffset = 0;
	}

	CG_RegisterWeapon( ps->weapon );
	weapon = &cg_weapons[ps->weapon];
	wData =  &weaponData[ps->weapon];

	memset (&hand, 0, sizeof(hand));

	if ( ps->weapon == WP_STUN_BATON || ps->weapon == WP_CONCUSSION )
	{
		cgi_S_AddLoopingSound( cent->currentState.number,
			cent->lerpOrigin,
			vec3_origin,
			weapon->weaponAttacksInfo[0].firingSound);
	}

	// set up gun position
	CG_CalculateWeaponPosition( hand.origin, angles );

	vec3_t extraOffset;
	extraOffset[0] = extraOffset[1] = extraOffset[2] = 0.0f;

	if( ps->weapon == WP_TUSKEN_RIFLE || ps->weapon == WP_NOGHRI_STICK || ps->weapon == WP_TUSKEN_STAFF )
	{
		extraOffset[0] = 2;
		extraOffset[1] = -3;
		extraOffset[2] = -6;
	}

	VectorMA( hand.origin, cg_gun_x.value+extraOffset[0], cg.refdef.viewaxis[0], hand.origin );
	VectorMA( hand.origin, (cg_gun_y.value+leanOffset+extraOffset[1]), cg.refdef.viewaxis[1], hand.origin );
	VectorMA( hand.origin, (cg_gun_z.value+fovOffset+extraOffset[2]), cg.refdef.viewaxis[2], hand.origin );
	//VectorMA( hand.origin, 0, cg.refdef.viewaxis[0], hand.origin );
	//VectorMA( hand.origin, (0+leanOffset), cg.refdef.viewaxis[1], hand.origin );
	//VectorMA( hand.origin, (0+fovOffset), cg.refdef.viewaxis[2], hand.origin );

	AnglesToAxis( angles, hand.axis );


	if ( cg_fovViewmodel.integer ) {
		float fracDistFOV = tanf( cg.refdef.fov_x * ( M_PI/180 ) * 0.5f );
		float fracWeapFOV = (1.0f / fracDistFOV) * tanf( actualFOV * (M_PI / 180) * 0.5f );
		VectorScale( hand.axis[0], fracWeapFOV, hand.axis[0] );
	}

	// map torso animations to weapon animations
#ifndef FINAL_BUILD
	if ( cg_gun_frame.integer )
	{
		// development tool
		hand.frame = hand.oldframe = cg_gun_frame.integer;
		hand.backlerp = 0;
	}
	else
#endif
	{
		// get clientinfo for animation map
		const clientInfo_t	*ci = &cent->gent->client->clientInfo;
		int torsoAnim = cent->gent->client->ps.torsoAnim;//pe.torso.animationNumber;
		float currentFrame;
		int startFrame,endFrame,flags;
		float animSpeed;
		if (cent->gent->lowerLumbarBone>=0&& gi.G2API_GetBoneAnimIndex(&cent->gent->ghoul2[cent->gent->playerModel], cent->gent->lowerLumbarBone, cg.time, &currentFrame, &startFrame, &endFrame, &flags, &animSpeed,0) )
		{
			hand.oldframe = CG_MapTorsoToWeaponFrame( ci,floor(currentFrame), torsoAnim, cent->currentState.weapon, ( cent->currentState.eFlags & EF_FIRING ) );
			hand.frame = CG_MapTorsoToWeaponFrame( ci,ceil(currentFrame), torsoAnim, cent->currentState.weapon, ( cent->currentState.eFlags & EF_FIRING ) );
			hand.backlerp=1.0f-(currentFrame-floor(currentFrame));
			if ( cg_debugAnim.integer == 1 && cent->currentState.clientNum == 0 )
			{
				Com_Printf( "Torso frame %d to %d makes Weapon frame %d to %d\n", cent->pe.torso.oldFrame,  cent->pe.torso.frame, hand.oldframe, hand.frame );
			}
		}
		else
		{
//			assert(0); // no idea what to do here
			hand.oldframe=0;
			hand.frame=0;
			hand.backlerp=0.0f;
		}
	}

	// add the weapon(s) - FIXME: allow for 2 weapons generically, not just 2 sabers?
	int	numSabers = 1;
	if ( cent->gent->client->ps.dualSabers )
	{
		numSabers = 2;
	}
	for ( int saberNum = 0; saberNum < numSabers; saberNum++ )
	{
		refEntity_t	gun;
		memset (&gun, 0, sizeof(gun));

		gun.hModel = weapon->weaponModel;
		if (!gun.hModel)
		{
			return;
		}

		AnglesToAxis( angles, gun.axis );
		CG_PositionEntityOnTag( &gun, &hand, weapon->handsModel, "tag_weapon");

		gun.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON;

	//---------
		// OK, we are making an assumption here that if we have the phaser that it is always on....
		//FIXME: if saberInFlight, need to draw empty hand guiding it
		if ( cent->gent && cent->gent->client && cent->currentState.weapon == WP_SABER )
		{
			vec3_t org_, axis_[3];

			for ( int bladeNum = 0; bladeNum < cent->gent->client->ps.saber[saberNum].numBlades; bladeNum++ )
			{
				//FIXME: need to get from tag_flash2 for saberstaff's second blade?
				CG_GetTagWorldPosition( &gun, "tag_flash", org_, axis_ );
				//loop this and do for both blades
				if ( cent->gent->client->ps.saber[0].blade[0].active && cent->gent->client->ps.saber[0].blade[0].length < cent->gent->client->ps.saber[0].blade[0].lengthMax )
				{
					cent->gent->client->ps.saber[0].blade[0].length += cg.frametime*0.03;
					if ( cent->gent->client->ps.saber[0].blade[0].length > cent->gent->client->ps.saber[0].blade[0].lengthMax )
					{
						cent->gent->client->ps.saber[0].blade[0].length = cent->gent->client->ps.saber[0].blade[0].lengthMax;
					}
				}
		//		FX_Saber( org_, axis_[0], cent->gent->client->ps.saberLength, 2.0 + Q_flrand(-1.0f, 1.0f) * 0.2f, cent->gent->client->ps.saberColor );
				if ( saberNum == 0 && bladeNum == 0 )
				{
					VectorCopy( axis_[0], cent->gent->client->renderInfo.muzzleDir );
				}
				else
				{//need these points stored here when in 1st person saber
					VectorCopy(org_, cent->gent->client->ps.saber[saberNum].blade[bladeNum].muzzlePoint);
				}
				VectorCopy( axis_[0], cent->gent->client->ps.saber[saberNum].blade[bladeNum].muzzleDir );
			}
		}
		//---------

		//	CG_AddRefEntityWithPowerups( &gun, cent->currentState.powerups, cent->gent );
			cgi_R_AddRefEntityToScene( &gun );

	/*	if ( ps->weapon == WP_STUN_BATON )
		{
			gun.shaderRGBA[0] = gun.shaderRGBA[1] = gun.shaderRGBA[2] = 25;

			gun.customShader = cgi_R_RegisterShader( "gfx/effects/stunPass" );
			gun.renderfx = RF_RGB_TINT | RF_FIRST_PERSON | RF_DEPTHHACK;
			cgi_R_AddRefEntityToScene( &gun );
		}
	*/
		// add the spinning barrel[s]
		for (int i = 0; (i < wData->numBarrels); i++)
		{
			refEntity_t	barrel;
			memset( &barrel, 0, sizeof( barrel ) );
			barrel.hModel = weapon->barrelModel[i];

			//VectorCopy( parent->lightingOrigin, barrel.lightingOrigin );
			//barrel.shadowPlane = parent->shadowPlane;
			barrel.renderfx = gun.renderfx;
			angles[YAW] = 0;
			angles[PITCH] = 0;
	//		if ( ps->weapon == WP_TETRION_DISRUPTOR) {
	//			angles[ROLL] = CG_MachinegunSpinAngle( cent );
	//		} else {
				angles[ROLL] = 0;//CG_MachinegunSpinAngle( cent );
	//		}

			AnglesToAxis( angles, barrel.axis );
			if (!i)
			{
				CG_PositionRotatedEntityOnTag( &barrel, &hand, weapon->handsModel, "tag_barrel", NULL );
			} else
			{
				CG_PositionRotatedEntityOnTag( &barrel, &hand, weapon->handsModel, va("tag_barrel%d",i+1), NULL );
			}

			cgi_R_AddRefEntityToScene( &barrel );
		}

		memset (&flash, 0, sizeof(flash));

		// Seems like we should always do this in case we have an animating muzzle flash....that way we can always store the correct muzzle dir, etc.
		CG_PositionEntityOnTag( &flash, &gun, gun.hModel, "tag_flash");

		CG_DoMuzzleFlash( cent, flash.origin, flash.axis[0], wData );

		if ( cent->gent && cent->gent->client )
		{
			if ( saberNum == 0 )
			{
				VectorCopy(flash.origin, cent->gent->client->renderInfo.muzzlePoint);
				VectorCopy(flash.axis[0], cent->gent->client->renderInfo.muzzleDir);
			}
	//		VectorNormalize( cent->gent->client->renderInfo.muzzleDir );
			cent->gent->client->renderInfo.mPCalcTime = cg.time;

			//CG_LightningBolt( cent, flash.origin );
		}
	}

	// Do special charge bits
	//-----------------------
	//Should not be important...
	if ( ps->weaponstate == WEAPON_CHARGING_ALT || ps->weaponstate == WEAPON_CHARGING )
	{
		int		shader = 0;

		int weapon = ps->weapon;
		int baseWeapon = weaponData[weapon].baseWeaponNum ? weaponData[weapon].baseWeaponNum : weapon;

		float	val = 0.0f, scale = 1.0f;
		vec3_t	WHITE	= {1.0f,1.0f,1.0f};

		if (baseWeapon == WP_BRYAR_PISTOL
			|| baseWeapon == WP_BLASTER_PISTOL
			|| baseWeapon == WP_REY)
		{
			// Hardcoded max charge time of 1 second
			val = ( cg.time - ps->weaponChargeTime ) * 0.001f;
			shader = cgi_R_RegisterShader( "gfx/effects/bryarFrontFlash" );
		}
		else if ( baseWeapon == WP_BOWCASTER )
		{
			// Hardcoded max charge time of 1 second
			val = ( cg.time - ps->weaponChargeTime ) * 0.001f;
			shader = cgi_R_RegisterShader( "gfx/effects/greenFrontFlash" );
		}
		else if (baseWeapon == WP_DEMP2 )
		{
			// Hardcoded max charge time of 1 second
			val = ( cg.time - ps->weaponChargeTime ) * 0.001f;
			shader = cgi_R_RegisterShader( "gfx/misc/lightningFlash" );
			scale = 6.5f;
		}
		//Default values for new weapons;
		else {
			// Hardcoded max charge time of 1 second
			val = (cg.time - ps->weaponChargeTime) * 0.001f;
			shader = cgi_R_RegisterShader("gfx/effects/bryarFrontFlash");
		}

		//Overwrite the muzzle effect if needed
		qboolean altFire = (ps->weaponstate == WEAPON_CHARGING_ALT) ? qtrue : qfalse;
		int attackIndex = CG_GetAttackIndex(cent->gent, altFire);
		if (weaponData[weapon].attackData[attackIndex].chargeMuzzleShader[0]) 
		{
			shader = cg_weapons[weapon].weaponAttacksInfo[attackIndex].chargeMuzzleShader;
		}
		if (weaponData[weapon].attackData[attackIndex].chargeMuzzleScale != -1)
		{
			scale = weaponData[weapon].attackData[attackIndex].chargeMuzzleScale;
		}

		if ( val < 0.0f )
		{
			val = 0.0f;
		}
		else if ( val > 1.0f )
		{
			val = 1.0f;
			CGCam_Shake( 0.1f, 100 );
		}
		else
		{
			CGCam_Shake( val * val * 0.3f, 100 );
		}

		val += Q_flrand(0.0f, 1.0f) * 0.5f;

		FX_AddSprite( flash.origin, NULL, NULL, 3.0f * val * scale, 0.0f, 0.7f, 0.7f, WHITE, WHITE, Q_flrand(0.0f, 1.0f) * 360, 0.0f, 1.0f, shader, FX_USE_ALPHA | FX_DEPTH_HACK );
	}

	// Check if the heavy repeater is finishing up a sustained burst
	//-------------------------------
	if ( ps->weapon == WP_REPEATER && ps->weaponstate == WEAPON_FIRING )
	{
		if ( cent->gent && cent->gent->client && cent->gent->client->ps.weaponstate != WEAPON_FIRING )
		{
			int	ct = 0;

			// the more continuous shots we've got, the more smoke we spawn
			if ( cent->gent->client->ps.weaponShotCount > 60 ) {
				ct = 5;
			}
			else if ( cent->gent->client->ps.weaponShotCount > 35 ) {
				ct = 3;
			}
			else if ( cent->gent->client->ps.weaponShotCount > 15 ) {
				ct = 1;
			}

			for ( int i = 0; i < ct; i++ )
			{
				theFxScheduler.PlayEffect( "repeater/muzzle_smoke", cent->currentState.clientNum );
			}

			cent->gent->client->ps.weaponShotCount = 0;
		}
	}
}

/*
==============================================================================

WEAPON SELECTION

==============================================================================
*/

/*
===================
CG_WeaponCheck
===================
*/
int CG_WeaponCheck( int weaponIndex )
{
	int				value;

	if ( weaponIndex == WP_SABER)
	{
		return qtrue;
	}

	value = weaponData[weaponIndex].attackData[0].energyPerShot;
	for (int k = 1; k < MAX_WEAPON_ATTACKS; k++) {
		if (weaponData[weaponIndex].attackData[k].energyPerShot < value) {
			value = weaponData[weaponIndex].attackData[k].energyPerShot;
		}
	}

	if( !cg.snap )
	{
		return qfalse;
	}

	// check how much energy(ammo) it takes to fire this weapon against how much ammo we have
	if ( value > cg.snap->ps.ammo[weaponData[weaponIndex].ammoIndex] )
	{
		value = qfalse;
	}
	else
	{
		value = qtrue;
	}

	return value;
}

int cgi_UI_GetItemText(char *menuFile,char *itemName, char *text);

/* Default Value for descriptions of the weapons */
/* Can be overriden by a weapon_class.str file */
/* Use weapon_number as index, should only be used for hardcoded weapons (those with a WP_XXXX enum)*/
const char *weaponDesc[MAX_WEAPONS] =
{
"SABER_DESC",				//WP_SABER
"NEW_BLASTER_PISTOL_DESC",	//WP_BLASTER_PISTOL
"BLASTER_RIFLE_DESC",		//WP_BLASTER_RIFLE
"DISRUPTOR_RIFLE_DESC",		//WP DISRUPTOR
"BOWCASTER_DESC",			//WP_BOWCASTER
"HEAVYREPEATER_DESC",		//WP_REPEATER
"DEMP2_DESC",				//WP_DEMP2
"FLECHETTE_DESC",			//WP_FLECHETTE
"MERR_SONN_DESC",			//WP_ROCKET_LAUNCHER
"THERMAL_DETONATOR_DESC",	//WP_THERMAL
"TRIP_MINE_DESC",			//WP_TRIP_MINE
"DET_PACK_DESC",			//WP_DET_PACK
"CONCUSSION_DESC",			//WP_CONCUSSION
"MELEE_DESC",				//WP_MELEE
"ATST_MAIN_DESC",			//WP_ATST_MAIN
"ATST_SIDE_DESC",			//WP_ATST_SIDE
"STUN_BATON_DESC",			//WP_STUN_BATON
"BLASTER_PISTOL_DESC",		//WP_BRYAR_PISTOL
"EMPLACED_GUN_DESC",		//WP_EMPLACED_GUN
"BOT_LASER_DESC",			//WP_BOT_LASER (Probe Droid)
"TURRET_DESC",				//WP_TURRET
"TIE_FIGHTER_DESC",			//WP_TIE_FIGHTER
"RAPID_CONCUSSION_DESC",	//WP_RAPID_FIRE_CONC
"JAWA_DESC",				//WP_JAWA
"TUSKEN_RIFLE_DESC",		//WP_TUSKEN_RIFLE
"TUSKEN_STAFF_DESC",		//WP_TUSKEN_STAFF
"SCEPTER_DESC",				//WP_SCEPTER
"NOGHRI_STICK_DESC",		//WP_NOGHRI_STICK
"BATTLEDROID_DESC",			//WP_BATTLEDROID
"THEFIRSTORDER_DESC",		//WP_THEFIRSTORDER
"CLONECARBINE_DESC",		//WP_CLONECARBINE
"REBELBLASTER_DESC",		//WP_REBELBLASTER
"CLONERIFLE_DESC",			//WP_CLONERIFLE
"CLONECOMMANDO_DESC",		//WP_CLONECOMMANDO
"REBELRIFLE_DESC",			//WP_REBELRIFLE
"REY_DESC",					//WP_REY
"JANGO_DESC",				//WP_JANGO
"BOBA_DESC",				//WP_BOBA
"CLONEPISTOL_DESC",			//WP_DC17
"CIS_SNIPER_DESC",			//WP_CIS_SNIPER
"SBD_DESC",					//WP_SBD
"DROIDEKA_DESC"				//WP_DROIDEKA
};

/*
===================
CG_DrawDataPadWeaponSelect

Allows user to cycle through the various weapons currently owned and view the description
===================
*/
void CG_DrawDataPadWeaponSelect( void )
{
	int				i;
	int				ownedWeaponCount,weaponSelectI;
	float			holdX;
	int				sideLeftIconCnt,sideRightIconCnt;
	int				holdCount,iconCnt;
	char			text[1024]={0};

	// showing weapon select clears pickup item display, but not the blend blob
	cg.itemPickupTime = 0;

	// count the number of weapons owned
	ownedWeaponCount = 0;
	for ( i =  1; i < weaponCount ; i++ )
	{
		if ( cg.snap->ps.weapons[i] )
		{
			ownedWeaponCount++;
		}
	}

	if (ownedWeaponCount == 0)	// If no weapons, don't display
	{
		return;
	}

	const short sideMax = 3;	// Max number of icons on the side

	// Calculate how many icons will appear to either side of the center one
	holdCount = ownedWeaponCount - 1;	// -1 for the center icon
	if (holdCount == 0)			// No icons to either side
	{
		sideLeftIconCnt = 0;
		sideRightIconCnt = 0;
	}
	else if (ownedWeaponCount > (2*sideMax))	// Go to the max on each side
	{
		sideLeftIconCnt = sideMax;
		sideRightIconCnt = sideMax;
	}
	else							// Less than max, so do the calc
	{
		sideLeftIconCnt = holdCount/2;
		sideRightIconCnt = holdCount - sideLeftIconCnt;
	}

	// This seems to be a problem if datapad comes up too early
	if (cg.DataPadWeaponSelect<FIRST_WEAPON)
	{
		cg.DataPadWeaponSelect = FIRST_WEAPON;
	}
	else if (cg.DataPadWeaponSelect>= weaponCount)
	{
		cg.DataPadWeaponSelect = weaponCount - 1;
	}

	int currentIconIndex = 0;
	for (int i = 0; i < WEAPON_BUCKETS_SIZE; i++) {
		if (weaponBuckets[i] == cg.DataPadWeaponSelect) {
			currentIconIndex = i;
			break;
		}
	}
	
	if (currentIconIndex == 0) {
		weaponSelectI = WEAPON_BUCKETS_SIZE - 1;
	}
	else {
		weaponSelectI = currentIconIndex - 1;
	}

	const float smallIconSize_x = 40 * cgs.widthRatioCoef, smallIconSize_y = 40;
	const float bigIconSize_x = 80 * cgs.widthRatioCoef, bigIconSize_y = 80;
	const float bigPad = 64;
	const float pad = 32 * cgs.widthRatioCoef;

	const float centerXPos = 320;
	const float graphicYPos = 340;


	// Left side ICONS
	// Work backwards from current icon
	holdX = centerXPos - ((bigIconSize_x / 2) + bigPad + smallIconSize_x);

	cgi_R_SetColor( colorTable[CT_WHITE] );
	for (iconCnt=1 ; iconCnt <= sideLeftIconCnt ; weaponSelectI-- )
	{
		if (weaponSelectI<0)
		{
			weaponSelectI = WEAPON_BUCKETS_SIZE - 1;
		}

		if ( !(cg.snap->ps.weapons[weaponBuckets[weaponSelectI]]))	// Does he have this weapon?
		{
			continue;
		}

		++iconCnt;					// Good icon
		int currentWeapon = weaponBuckets[weaponSelectI];
		if (weaponData[currentWeapon].weaponIcon[0])
		{
			weaponInfo_t	*weaponInfo;
			CG_RegisterWeapon(currentWeapon);
			weaponInfo = &cg_weapons[currentWeapon];

			if (!CG_WeaponCheck(currentWeapon))
			{
				CG_DrawPic(holdX, graphicYPos, smallIconSize_x, smallIconSize_y, weaponInfo->weaponIconNoAmmo);
			}
			else
			{
				CG_DrawPic(holdX, graphicYPos, smallIconSize_x, smallIconSize_y, weaponInfo->weaponIcon);
			}

			holdX -= (smallIconSize_x + pad);
		}
	}

	// Current Center Icon
	cgi_R_SetColor(colorTable[CT_WHITE]);

	if (weaponData[cg.DataPadWeaponSelect].weaponIcon[0])
	{
		weaponInfo_t	*weaponInfo;
		CG_RegisterWeapon( cg.DataPadWeaponSelect );
		weaponInfo = &cg_weapons[cg.DataPadWeaponSelect];

			// Draw graphic to show weapon has ammo or no ammo
		if (!CG_WeaponCheck(cg.DataPadWeaponSelect))
		{
			CG_DrawPic(centerXPos - (bigIconSize_x / 2), (graphicYPos - ((bigIconSize_y - smallIconSize_y) / 2)) + 10, bigIconSize_x, bigIconSize_y, weaponInfo->weaponIconNoAmmo);
		}
		else
		{
			CG_DrawPic(centerXPos - (bigIconSize_x / 2), (graphicYPos - ((bigIconSize_y - smallIconSize_y) / 2)) + 10, bigIconSize_x, bigIconSize_y, weaponInfo->weaponIcon);
		}
	}

	if (weaponSelectI>= (WEAPON_BUCKETS_SIZE -1) )
	{
		weaponSelectI = 0;
	}
	else {
		weaponSelectI = currentIconIndex + 1;
	}

	// Right side ICONS
	// Work forwards from current icon
	cgi_R_SetColor(colorTable[CT_WHITE]);
	holdX = centerXPos + (bigIconSize_x / 2) + bigPad;
	for (iconCnt=1;iconCnt<(sideRightIconCnt+1);weaponSelectI++)
	{
		if (weaponSelectI >= WEAPON_BUCKETS_SIZE - 1)
		{
			weaponSelectI = 0;
		}
		int currentWeapon = weaponBuckets[weaponSelectI];
		if ( !(cg.snap->ps.weapons[currentWeapon]))	// Does he have this weapon?
		{
			continue;
		}

		++iconCnt;					// Good icon

		if (weaponData[currentWeapon].weaponIcon[0])
		{
			weaponInfo_t	*weaponInfo;
			CG_RegisterWeapon(currentWeapon);
			weaponInfo = &cg_weapons[currentWeapon];

			// Draw graphic to show weapon has ammo or no ammo
			if (!CG_WeaponCheck(currentWeapon))
			{
				CG_DrawPic(holdX, graphicYPos, smallIconSize_x, smallIconSize_y, weaponInfo->weaponIconNoAmmo);
			}
			else
			{
				CG_DrawPic(holdX, graphicYPos, smallIconSize_x, smallIconSize_y, weaponInfo->weaponIcon);
			}


			holdX += (smallIconSize_x + pad);
		}
	}

	// Print the weapon description
	//Dynamic Weapons
	if (cgi_SP_GetStringTextString(va("%s_DESC", weaponData[cg.DataPadWeaponSelect].classname), text, sizeof(text)))
	{
		; //Nothing to do
	}
	else if (cgi_SP_GetStringTextString(va("SP_INGAME_%s", weaponDesc[cg.DataPadWeaponSelect - 1]), text, sizeof(text)))
	{
		; //Nothing to do
	}
	else if (!cgi_SP_GetStringTextString( va("SPMOD_INGAME_%s",weaponDesc[cg.DataPadWeaponSelect-1]), text, sizeof(text) ))
	{
		Com_sprintf(text, sizeof("No weapon description Found") + 1, "No weapon description Found"); 
	}

	if (text[0])
	{
		const short textboxXPos = 40;
		const short textboxYPos = 60;
		const int	textboxWidth = 560;
		const int	textboxHeight = 300;
		const float	textScale = 1.0f;

		CG_DisplayBoxedText(
			textboxXPos, textboxYPos,
			textboxWidth, textboxHeight,
			text,
			CG_MagicFontToReal(4),
			textScale,
			colorTable[CT_WHITE]
				);
	}

	cgi_R_SetColor( NULL );
}

/*
===================
CG_DrawDataPadIconBackground

Draw the proper background graphic for the icons being displayed on the datapad
===================
*/
void CG_DrawDataPadIconBackground(const int backgroundType)
{
//	const int		graphicXPos = 40;
//	const int		graphicYPos = 340;
//	const short		graphicHeight = 60;
//	const short		graphicWidth = 560;
//	qhandle_t		background;

/*
	if (backgroundType == ICON_INVENTORY)	// Display inventory background?
	{
		background = cgs.media.inventoryIconBackground;
	}
	else if (backgroundType == ICON_WEAPONS)	// Display weapon background?
	{
		background = cgs.media.weaponIconBackground;
	}
	else 	// Display force background?
	{
		background = cgs.media.forceIconBackground;
	}

	cgi_R_SetColor( colorTable[CT_WHITE] );	// Let the graphic set the color

	CG_DrawPic( graphicXPos,
		graphicYPos+(graphicHeight/2),
		graphicWidth,
		-graphicHeight,
		background);	// Top half

	CG_DrawPic( graphicXPos,
		graphicYPos+(graphicHeight/2),
		graphicWidth,
		graphicHeight,
		background);	// Bottom half

*/
}

/*
===============
SetWeaponSelectTime
===============
*/
void SetWeaponSelectTime(void)
{

	if (((cg.inventorySelectTime + WEAPON_SELECT_TIME) > cg.time) ||	// The Inventory HUD was currently active to just swap it out with Force HUD
		((cg.forcepowerSelectTime + WEAPON_SELECT_TIME) > cg.time))		// The Force HUD was currently active to just swap it out with Force HUD
	{
		cg.inventorySelectTime = 0;
		cg.forcepowerSelectTime = 0;
		cg.weaponSelectTime = cg.time + 130.0f;
	}
	else
	{
		cg.weaponSelectTime = cg.time;
	}
}

/*
===================
CG_DrawWeaponSelect
===================
*/
extern Vehicle_t *G_IsRidingVehicle( gentity_t *ent );
extern bool G_IsRidingTurboVehicle( gentity_t *ent );

void CG_DrawWeaponSelect( void )
{
	int		i;
	int		count;
	float	smallIconSize_y, bigIconSize_y;
	float	smallIconSize_x, bigIconSize_x;
	float	holdX, x, y, pad;
	int		x2, y2, w2, h2;
	int		sideLeftIconCnt,sideRightIconCnt;
	int		sideMax,holdCount,iconCnt;
	//int		height;
	vec4_t	calcColor;
	vec4_t	textColor = { .875f, .718f, .121f, 1.0f };
	int		yOffset = 0;
	bool	isOnVeh = false;

	if ((cg.weaponSelectTime+WEAPON_SELECT_TIME)<cg.time)	// Time is up for the HUD to display
	{
		return;
	}

	// don't display if dead
	if ( cg.predicted_player_state.stats[STAT_HEALTH] <= 0 )
	{
		return;
	}

	cg.iconSelectTime = cg.weaponSelectTime;

	// showing weapon select clears pickup item display, but not the blend blob
	//cg.itemPickupTime = 0;

	// count the number of weapons owned
	count = 0;
	isOnVeh = (G_IsRidingVehicle(cg_entities[0].gent)!=0);
 	for ( i = 1 ; i < weaponCount ; i++ )
	{
		if ((cg.snap->ps.weapons[i])  && weaponData[i].playerUsable &&
			(!isOnVeh || i==WP_NONE || i==WP_SABER || i==WP_BLASTER))
		{
			count++;
		}
	}

	if (count == 0)	// If no weapons, don't display
	{
		return;
	}

	sideMax = 3;	// Max number of icons on the side

	// Calculate how many icons will appear to either side of the center one
	holdCount = count - 1;	// -1 for the center icon
	if (holdCount == 0)			// No icons to either side
	{
		sideLeftIconCnt = 0;
		sideRightIconCnt = 0;
	}
	else if (count > (2*sideMax))	// Go to the max on each side
	{
		sideLeftIconCnt = sideMax;
		sideRightIconCnt = sideMax;
	}
	else							// Less than max, so do the calc
	{
		sideLeftIconCnt = holdCount/2;
		sideRightIconCnt = holdCount - sideLeftIconCnt;
	}


	//Search where is the current weapon in the array
	int currentWeaponIndex = -1;
	for (i = 0; i < WEAPON_BUCKETS_SIZE; i++) {
		if (weaponBuckets[i] == cg.weaponSelect) {
			currentWeaponIndex = i;
			i--;
			break;
		}
	}
	if (i < 0) {
		i = WEAPON_BUCKETS_SIZE - 1;
	}

	smallIconSize_x = 40 * cgs.widthRatioCoef;
	smallIconSize_y = 40;
	bigIconSize_x = 80 * cgs.widthRatioCoef;
	bigIconSize_y = 80;
	pad = 12 * cgs.widthRatioCoef;

	if (!cgi_UI_GetMenuInfo("weaponselecthud",&x2,&y2,&w2,&h2))
	{
		return;
	}
	x = 320;
	y = 410;

	// Background
	memcpy(calcColor, colorTable[CT_WHITE], sizeof(vec4_t));
	calcColor[3] = .60f;
	cgi_R_SetColor( calcColor);

	// Left side ICONS
	cgi_R_SetColor( calcColor);
	// Work backwards from current icon
	holdX = x - ((bigIconSize_x / 2) + pad + smallIconSize_x);
	//height = smallIconSize * cg.iconHUDPercent;

	for (iconCnt=1;iconCnt<(sideLeftIconCnt+1);i--)
	{
		if (i<1)
		{
			i = WEAPON_BUCKETS_SIZE-1;
		}
		int tmpWeapon = weaponBuckets[i];
		if ( tmpWeapon <= 0 || !(cg.snap->ps.weapons[tmpWeapon] && weaponData[tmpWeapon].playerUsable) )	// Does he have this weapon?
		{
			continue;
		}
		if (isOnVeh)
		{
			if (tmpWeapon != WP_NONE && tmpWeapon != WP_SABER && tmpWeapon != WP_BLASTER )
			{
				continue;	// Don't draw anything else if on a vehicle
			}
		}

		++iconCnt;					// Good icon

		if (weaponData[tmpWeapon].weaponIcon[0])
		{
			weaponInfo_t	*weaponInfo;
			CG_RegisterWeapon(tmpWeapon);
			weaponInfo = &cg_weapons[tmpWeapon];

			if (!CG_WeaponCheck(tmpWeapon))
			{
				CG_DrawPic(holdX, y + 10 + yOffset, smallIconSize_x, smallIconSize_y, weaponInfo->weaponIconNoAmmo);
			}
			else
			{
				CG_DrawPic(holdX, y + 10 + yOffset, smallIconSize_x, smallIconSize_y, weaponInfo->weaponIcon);
			}

			holdX -= (smallIconSize_x + pad);
		}
	}

	// Current Center Icon
	//height = bigIconSize * cg.iconHUDPercent;
	cgi_R_SetColor(NULL);
	if (weaponData[cg.weaponSelect].weaponIcon[0])
	{
		weaponInfo_t	*weaponInfo;
		CG_RegisterWeapon( cg.weaponSelect );
		weaponInfo = &cg_weapons[cg.weaponSelect];

		if (!CG_WeaponCheck(cg.weaponSelect))
		{
			CG_DrawPic(x - (bigIconSize_x / 2), (y - ((bigIconSize_y - smallIconSize_y) / 2)) + 10 + yOffset, bigIconSize_x, bigIconSize_y, weaponInfo->weaponIconNoAmmo);
		}
		else
		{
			CG_DrawPic(x - (bigIconSize_x / 2), (y - ((bigIconSize_y - smallIconSize_y) / 2)) + 10 + yOffset, bigIconSize_x, bigIconSize_y, weaponInfo->weaponIcon);
		}
	}



	i = currentWeaponIndex + 1;
	if (i >= WEAPON_BUCKETS_SIZE ) {
		i = 1;
	}

	// Right side ICONS
	// Work forwards from current icon
	cgi_R_SetColor( calcColor);
	holdX = x + (bigIconSize_x / 2) + pad;
	//height = smallIconSize * cg.iconHUDPercent;
	for (iconCnt=1;iconCnt<(sideRightIconCnt+1);i++)
	{
		if (i>= WEAPON_BUCKETS_SIZE)
		{
			i = 1;
		}
		int tmpWeapon = weaponBuckets[i];
		if ( tmpWeapon <= 0 || !(cg.snap->ps.weapons[tmpWeapon] && weaponData[tmpWeapon].playerUsable))	// Does he have this weapon?
		{
			continue;
		}
		if (isOnVeh)
		{
			if (tmpWeapon != WP_NONE && tmpWeapon != WP_SABER && tmpWeapon != WP_BLASTER )
			{
				continue;	// Don't draw anything else if on a vehicle
			}
		}

		++iconCnt;					// Good icon

		if (weaponData[tmpWeapon].weaponIcon[0])
		{
			weaponInfo_t	*weaponInfo;
			CG_RegisterWeapon(tmpWeapon);
			weaponInfo = &cg_weapons[tmpWeapon];
			// No ammo for this weapon?
			if (!CG_WeaponCheck(tmpWeapon))
			{
				CG_DrawPic(holdX, y + 10 + yOffset, smallIconSize_x, smallIconSize_y, weaponInfo->weaponIconNoAmmo);
			}
			else
			{
				CG_DrawPic(holdX, y + 10 + yOffset, smallIconSize_x, smallIconSize_y, weaponInfo->weaponIcon);
			}


			holdX += (smallIconSize_x + pad);
		}
	}

	gitem_t *item = cg_weapons[ cg.weaponSelect ].item;

	// draw the selected name
	if ( item && item->classname && item->classname[0] )
	{
		char text[1024];

		if ( cgi_SP_GetStringTextString( va("SP_INGAME_%s",item->classname), text, sizeof( text )))
		{
			int w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 1.0f, cgs.widthRatioCoef);
			int x = ( SCREEN_WIDTH - w ) / 2;
			cgi_R_Font_DrawString(x, (SCREEN_HEIGHT - 24) + yOffset, text, textColor, cgs.media.qhFontSmall, -1, 1.0f, cgs.widthRatioCoef);
		}
		else if ( cgi_SP_GetStringTextString( va("SPMOD_INGAME_%s",item->classname), text, sizeof( text )))
		{
			int w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 1.0f, cgs.widthRatioCoef);
			int x = ( SCREEN_WIDTH - w ) / 2;
			cgi_R_Font_DrawString(x, (SCREEN_HEIGHT - 24) + yOffset, text, textColor, cgs.media.qhFontSmall, -1, 1.0f, cgs.widthRatioCoef);
		}
		//Dynamic Weapons
		else if ( cgi_SP_GetStringTextString( va("%s_NAME",item->classname), text, sizeof( text )))
		{
			int w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontSmall, 1.0f, cgs.widthRatioCoef);
			int x = ( SCREEN_WIDTH - w ) / 2;
			cgi_R_Font_DrawString(x, (SCREEN_HEIGHT - 24) + yOffset, text, textColor, cgs.media.qhFontSmall, -1, 1.0f, cgs.widthRatioCoef);
		}
	}

	cgi_R_SetColor( NULL );
}


/*
===============
CG_WeaponSelectable
===============
*/
qboolean CG_WeaponSelectable( int i, int original, qboolean dpMode )
{
	int	usage_for_weap;

	if (i >= weaponCount)
	{
#ifndef FINAL_BUILD
		Com_Printf("CG_WeaponSelectable() passed illegal index of %d!\n",i);
#endif
		return qfalse;
	}

	if (!weaponData[i].playerUsable || cg.weaponSelectTime + 100 > cg.time )
	{//TEMP standard weapon cycle debounce for E3 because G2 can't keep up with fast weapon changes
		return qfalse;
	}

	//FIXME: this doesn't work below, can still cycle too fast!
	if ( original == WP_SABER && cg.weaponSelectTime + 500 > cg.time )
	{//when switch to lightsaber, have to stay there for at least half a second!
		return qfalse;
	}

	if ( G_IsRidingVehicle(cg_entities[0].gent) )
	{
		if (G_IsRidingTurboVehicle(cg_entities[0].gent) || (i!=WP_NONE && i!=WP_SABER && i!=WP_BLASTER) )
		{
			return qfalse;
		}
	}

	if (( weaponData[i].ammoIndex != AMMO_NONE ) && !dpMode )
	{//weapon uses ammo, see if we have any
		usage_for_weap = weaponData[i].attackData[0].energyPerShot;
		for (int k = 1; k < MAX_WEAPON_ATTACKS; k++) {
			if (weaponData[i].attackData[k].energyPerShot < usage_for_weap) {
				usage_for_weap = weaponData[i].attackData[k].energyPerShot;
			}
		}

		if ( cg.snap->ps.ammo[weaponData[i].ammoIndex] - usage_for_weap < 0 )
		{
			if ( i != WP_DET_PACK ) // detpack can be switched to...should possibly check if there are any stuck to a wall somewhere?
			{
				// This weapon doesn't have enough ammo to shoot either the main or the alt-fire
				return qfalse;
			}
		}
	}

	if (!(cg.snap->ps.weapons[i]))
	{
		// Don't have this weapon to start with.
		return qfalse;
	}

	return qtrue;
}

void CG_ToggleATSTWeapon( void )
{
	if ( cg.weaponSelect == WP_ATST_MAIN )
	{
		cg.weaponSelect = WP_ATST_SIDE;
	}
	else
	{
		cg.weaponSelect = WP_ATST_MAIN;
	}
//	cg.weaponSelectTime = cg.time;
	SetWeaponSelectTime();
}

void CG_PlayerLockedWeaponSpeech( int jumping )
{
extern qboolean Q3_TaskIDPending( gentity_t *ent, taskID_t taskType );
	static int speechDebounceTime = 0;
	if ( !in_camera )
	{//not in a cinematic
		if ( speechDebounceTime < cg.time )
		{//spoke more than 3 seconds ago
			if ( !Q3_TaskIDPending( &g_entities[0], TID_CHAN_VOICE ) )
			{//not waiting on a scripted sound to finish
				if( !jumping )
				{
					if( Q_flrand(0.0f, 1.0f) > 0.5 )
					{
						if(!Q_stricmp(g_char_model->string, "kyle") || !Q_stricmp(g_char_model->string, "kyleJK2"))
							G_SoundOnEnt( player, CHAN_VOICE, va( "sound/chars/kyle/09kyk015.wav" ));
					}
					else
					{
						if (!Q_stricmp(g_char_model->string, "kyle") || !Q_stricmp(g_char_model->string, "kyleJK2"))
							G_SoundOnEnt( player, CHAN_VOICE, va( "sound/chars/kyle/09kyk016.wav" ));
					}
				}
				else
				{
					if (!Q_stricmp(g_char_model->string, "kyle") || !Q_stricmp(g_char_model->string, "kyleJK2"))
						G_SoundOnEnt( player, CHAN_VOICE, va( "sound/chars/kyle/16kyk007.wav" ));
				}
				speechDebounceTime = cg.time + 3000;
			}
		}
	}
}
/*
===============
CG_NextWeapon_f
===============
*/
void CG_NextWeapon_f( void ) {
	int		i;
	int		original;

	if ( !cg.snap ) {
		return;
	}

	if( g_entities[0].flags & FL_LOCK_PLAYER_WEAPONS )
	{
		CG_PlayerLockedWeaponSpeech( qfalse );
		return;
	}

	if( g_entities[0].client && g_entities[0].client->NPC_class == CLASS_ATST )
	{
		CG_ToggleATSTWeapon();
		return;
	}

	if ( cg.snap->ps.eFlags & EF_LOCKED_TO_WEAPON )
	{
		// can't do any sort of weapon switching when in the emplaced gun
		return;
	}

	if ( cg.snap->ps.viewEntity )
	{
		// yeah, probably need a better check here
		if ( g_entities[cg.snap->ps.viewEntity].client && ( g_entities[cg.snap->ps.viewEntity].client->NPC_class == CLASS_R5D2
				|| g_entities[cg.snap->ps.viewEntity].client->NPC_class == CLASS_R2D2
				|| g_entities[cg.snap->ps.viewEntity].client->NPC_class == CLASS_MOUSE ))
		{
			return;
		}
	}

	original = cg.weaponSelect;

	qboolean canSelectNone = qfalse;
	if (G_IsRidingVehicle(&g_entities[cg.snap->ps.viewEntity]))
	{
		canSelectNone = qtrue;
	}

	//Search where is the current weapon in the array
	int currentWeaponIndex = - 1;
	for (i = 0; i < WEAPON_BUCKETS_SIZE; i++) {
		if (weaponBuckets[i] == original) {
			currentWeaponIndex = i;
			break;
		}
	}
	//Cycle forward in the weaponBucketArray, ignoring negatives values
	for (i = 0; i < WEAPON_BUCKETS_SIZE; i++)
	{
		currentWeaponIndex++;
		//Go back at the beggining of the array
		if (currentWeaponIndex == WEAPON_BUCKETS_SIZE) {
			currentWeaponIndex = 0;
		}
		//Never try to switch to Bucket or WP_NONE
		if (weaponBuckets[currentWeaponIndex] < 0 || (!canSelectNone && weaponBuckets[currentWeaponIndex] == 0)) {
			continue;
		}

		if (CG_WeaponSelectable(weaponBuckets[currentWeaponIndex], original, qfalse))
		{
			//Found you!
			SetWeaponSelectTime();
			cg.weaponSelect = weaponBuckets[currentWeaponIndex];
			return;
		}
	}
}

/* 1 -> XXX is base weapons*/
/* -1 -> Ammo */
/* -2 -> Inventory*/
/* -3 -> All Weapons*/
extern vmCvar_t		ui_loadout_base_weapon;
void CG_LDO_SelectBaseWeapon_f(void)
{
	cgi_Cvar_Update(&ui_loadout_base_weapon);
	char *baseWeapon = ui_loadout_base_weapon.string;

	//Reset the selected weapon / Ammo / Item & the selected Page
	cg.LoadoutWeaponSelect = 0;
	cg.LoadoutPageSelect = 0;
	if (!Q_stricmp("LD_AMMUNITION", baseWeapon)) {
		cg.LoadoutBaseWeaponSelect = -1;
		return;
	}
	else if (!Q_stricmp("LD_INVENTORY", baseWeapon)) {
		cg.LoadoutBaseWeaponSelect = -2;
		return;
	}
	else if (!Q_stricmp("WEAPON_ALL", baseWeapon)) {
		cg.LoadoutBaseWeaponSelect = -3;
		return;
	}
	else if(!Q_stricmp("WB_MELEE", baseWeapon)){
		cg.LoadoutBaseWeaponSelect = -WB_MELEE;
	}
	else if(!Q_stricmp("WB_PISTOLS", baseWeapon)){
		cg.LoadoutBaseWeaponSelect = -WB_PISTOLS;
	}
	else if(!Q_stricmp("WB_BLASTERS", baseWeapon)){
		cg.LoadoutBaseWeaponSelect = -WB_BLASTERS;
	}
	else if(!Q_stricmp("WB_SPECIALISTS", baseWeapon)){
		cg.LoadoutBaseWeaponSelect = -WB_SPECIALISTS;
	}
	else if(!Q_stricmp("WB_HEAVY_WEAPONS", baseWeapon)){
		cg.LoadoutBaseWeaponSelect = -WB_HEAVY_WEAPONS;
	}
	else if(!Q_stricmp("WB_THROWABLES", baseWeapon)){
		cg.LoadoutBaseWeaponSelect = -WB_THROWABLES;
	}
	else if(!Q_stricmp("WB_OTHERS", baseWeapon)){
		cg.LoadoutBaseWeaponSelect = -WB_OTHERS;
	}
	else{
		cg.LoadoutBaseWeaponSelect = 0;
	}

}

extern vmCvar_t		ui_loadout_weapon;
void CG_LDO_SelectWeapon_f(void)
{
	if(cg.LoadoutBaseWeaponSelect == 0)
	{
		return;
	}
	//Get the weapon menu index value
	cgi_Cvar_Update(&ui_loadout_weapon);

	if(ui_loadout_weapon.integer == 0){
		cg.LoadoutWeaponSelect = 0;
		return;
	}

	int targetMenuIndex = (cg.LoadoutPageSelect * LOADOUT_PAGESIZE) + ui_loadout_weapon.integer;
	int currMenuIndex = 0;
	int i;
	//Search Ammo || Items
	if (cg.LoadoutBaseWeaponSelect == -1
		|| cg.LoadoutBaseWeaponSelect == -2
		)
	{
		for (i = 0; i < bg_numItems; i++)
		{
			gitem_t* item = &bg_itemlist[i];
			//Declared like this for readability
			if ( ( (cg.LoadoutBaseWeaponSelect == -1 && item->giType == IT_AMMO)
				|| (cg.LoadoutBaseWeaponSelect == -2 && item->giType == IT_HOLDABLE)
				|| (cg.LoadoutBaseWeaponSelect == -2 && item->giType == IT_HEALTH)
				|| (cg.LoadoutBaseWeaponSelect == -2 && item->giType == IT_ARMOR) )
				&& item->icon && item->icon[0]
				)
			{
				//This might be the weapon we are looking for
				currMenuIndex++;
				if (targetMenuIndex == currMenuIndex) {
					//This is !
					cg.LoadoutWeaponSelect = i;
					break;
				}
			}
		}
		return;
	}
	if (cg.LoadoutBaseWeaponSelect == -3)
	{
		//Normal search in the bucket array
		for (i = 1; i < WEAPON_BUCKETS_SIZE; i++)
		{
			int weaponIndex = weaponBuckets[i];
			if (weaponIndex > 0 && weaponData[weaponIndex].playerUsable)
			{
				currMenuIndex++;
				if (targetMenuIndex == currMenuIndex) {
					//This is !
					cg.LoadoutWeaponSelect = weaponIndex;
					break;
				}
			}
		}
		return;
	}
	int bucketIndex = -1;
	int bucketSize = -1;
	//Search for base index and size
	for (int i = 0; i < WEAPON_BUCKETS_SIZE;i++) {
		if (weaponBuckets[i] == -cg.LoadoutBaseWeaponSelect) {
			bucketIndex = i;
		}
		else if(bucketIndex >= 0 && weaponBuckets[i] < 0){
			bucketSize = i - bucketIndex;
			break;
		}
	}
	//Normal search in the bucket array
	for (i = bucketIndex; i < (bucketIndex + bucketSize); i++)
	{
		int weaponIndex = weaponBuckets[i];
		if (weaponIndex > 0 && weaponData[weaponIndex].playerUsable)
		{
			currMenuIndex++;
			if (targetMenuIndex == currMenuIndex) {
				cg.LoadoutWeaponSelect = weaponIndex;
				break;
			}
		}
	}
}

void CG_LDO_SwitchWeapon_f(void) {
	if (cg.LoadoutWeaponSelect == 0)
	{
		return;
	}
	gentity_t* ent = cg_entities[0].gent;
	//Add Ammo
	if (cg.LoadoutBaseWeaponSelect == -1) {
		gitem_t* item = &bg_itemlist[cg.LoadoutWeaponSelect];
		cgi_S_StartSound(NULL, ent->s.number, CHAN_AUTO, cgi_S_RegisterSound(item->pickup_sound));
		ent->client->ps.ammo[item->giTag] += 50;
		return;
	}
	//Add Holdable
	if (cg.LoadoutBaseWeaponSelect == -2 && bg_itemlist[cg.LoadoutWeaponSelect].giType == IT_HOLDABLE) {
		gitem_t* item = &bg_itemlist[cg.LoadoutWeaponSelect];
		cgi_S_StartSound(NULL, ent->s.number, CHAN_AUTO, cgi_S_RegisterSound(item->pickup_sound));
		if (item->giTag == INV_SECURITY_KEY)
		{
			INV_SecurityKeyGive(ent, ent->message);
		}
		else if (item->giTag == INV_GOODIE_KEY)
		{
			INV_GoodieKeyGive(ent);
		}
		else
		{// Picking up a normal item?
			ent->client->ps.inventory[item->giTag]++;
		}
		return;
	}
	//Add Armor or health
	if (cg.LoadoutBaseWeaponSelect == -2 &&
		(bg_itemlist[cg.LoadoutWeaponSelect].giType == IT_HEALTH || bg_itemlist[cg.LoadoutWeaponSelect].giType == IT_ARMOR)
		)
	{
		gitem_t* item = &bg_itemlist[cg.LoadoutWeaponSelect];

		cgi_S_StartSound(NULL, ent->s.number, CHAN_AUTO, cgi_S_RegisterSound(item->pickup_sound));

		int stat = (item->giType == IT_ARMOR) ? STAT_ARMOR : STAT_HEALTH;
		if (stat == STAT_ARMOR) {
			ent->client->ps.powerups[PW_BATTLESUIT] = Q3_INFINITE;
		}
		int quantity = item->quantity ? item->quantity : 30;
		ent->client->ps.stats[stat] += quantity;
		if (ent->client->ps.stats[stat] > ent->client->ps.stats[STAT_MAX_HEALTH]) {
			ent->client->ps.stats[stat] = ent->client->ps.stats[STAT_MAX_HEALTH];
		}
		return;
	}

	cg.snap->ps.weapons[cg.LoadoutWeaponSelect] = cg.snap->ps.weapons[cg.LoadoutWeaponSelect] ? 0 : 1;
	if (cg.snap->ps.weapons[cg.LoadoutWeaponSelect])
	{
		int ammoIndex = weaponData[cg.LoadoutWeaponSelect].ammoIndex;
		int givenAmmo = ammoData[ammoIndex].max / 3;
		ent->client->ps.weapons[cg.LoadoutWeaponSelect] = 1;
		if (ent->client->ps.ammo[ammoIndex] < givenAmmo) {
			ent->client->ps.ammo[ammoIndex] = givenAmmo;
		}
		//FIXME : If weapon == SABER and saber is not defined => CTD
	}
	else {
		ent->client->ps.weapons[cg.LoadoutWeaponSelect] = 0;
		if (cg.snap->ps.weapon == cg.LoadoutWeaponSelect) {
			ent->client->ps.weapon = WP_NONE;

			if (ent->ghoul2.IsValid())
			{
				if (ent->weaponModel[0] > 0)
				{
					gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->weaponModel[0]);
					ent->weaponModel[0] = -1;
				}
				if (ent->weaponModel[1] > 0)
				{
					gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->weaponModel[1]);
					ent->weaponModel[1] = -1;
				}
			}
		}
	}
}

int CG_LDO_GetMaxPages(void) {
	int i;
	int totalIcons = 0;

	if (cg.LoadoutBaseWeaponSelect == -1
		|| cg.LoadoutBaseWeaponSelect == -2)
	{
		for (i = 0; i < bg_numItems; i++) {
			gitem_t* item = &bg_itemlist[i];
			//Declared like this for readability
			if (((cg.LoadoutBaseWeaponSelect == -1 && item->giType == IT_AMMO)
				|| (cg.LoadoutBaseWeaponSelect == -2 && item->giType == IT_HOLDABLE)
				|| (cg.LoadoutBaseWeaponSelect == -2 && item->giType == IT_HEALTH)
				|| (cg.LoadoutBaseWeaponSelect == -2 && item->giType == IT_ARMOR)
				) && item->icon && item->icon[0])
			{
				totalIcons++;
			}
		}
	}
	else if (cg.LoadoutBaseWeaponSelect == -3)
	{
		for (i = 1; i < WEAPON_BUCKETS_SIZE; i++)
		{
			int iw = weaponBuckets[i];
			if (iw < 0) {
				continue;
			}
			if (weaponData[i].playerUsable)
			{
				totalIcons++;
			}
		}
	}
	else {
		int bucketIndexStart = -1;
		for (i = 0; i < WEAPON_BUCKETS_SIZE; i++)
		{
			//Searching for the first index bucket
			if (weaponBuckets[i] == -cg.LoadoutBaseWeaponSelect) {
				bucketIndexStart = i;
			}
			//Then counting Icon
			else if (bucketIndexStart >= 0) {
				int iw = weaponBuckets[i];
				if (iw < 0) {
					break;;
				}
				if (weaponData[i].playerUsable)
				{
					totalIcons++;
				}
			}
		}
	}
	return (totalIcons + LOADOUT_PAGESIZE - 1) / LOADOUT_PAGESIZE;
}

void CG_LDO_PreviousPage_f(void) {
	if (cg.LoadoutBaseWeaponSelect == 0) {
		return;
	}
	cg.LoadoutPageSelect--;
	int maxPages = CG_LDO_GetMaxPages();
	if (cg.LoadoutPageSelect < 0) {
		cg.LoadoutPageSelect = maxPages - 1 ;
	}
}

void CG_LDO_NextPage_f(void) {
	if (cg.LoadoutBaseWeaponSelect == 0) {
		return;
	}
	int maxPages = CG_LDO_GetMaxPages();
	cg.LoadoutPageSelect++;
	if (cg.LoadoutPageSelect >= maxPages) {
		cg.LoadoutPageSelect = 0;
	}
}

/* CG_NPC_NextWeapon_f
	Next weapon for charName
*/
extern int WP_GetWeaponID(const char* weaponName);

extern vmCvar_t		ui_npc_weapon;
extern vmCvar_t		ui_npc_weapon_label;

void CG_NPC_NextWeapon_f(void) {
	cgi_Cvar_Update(&ui_npc_weapon);
	int currentWeaponIndex = WP_GetWeaponID(ui_npc_weapon.string);
	if (currentWeaponIndex == -1) {
		currentWeaponIndex = 1; // 0 is WP_NONE
	}
	int nextWeaponIndex = currentWeaponIndex;
	do {
		nextWeaponIndex = (nextWeaponIndex == weaponCount - 1) ? 1 : nextWeaponIndex + 1;
	} while (!weaponData[nextWeaponIndex].classname 
		|| !weaponData[nextWeaponIndex].classname[0]
		|| (!weaponData[nextWeaponIndex].playerUsable && strcmp(weaponData[nextWeaponIndex].classname, "weapon_clonerandom") )
	);
	cgi_Cvar_Set("ui_npc_weapon", weaponData[nextWeaponIndex].classname);
	CG_NPC_UpdateLabel();
}

void CG_NPC_PrevWeapon_f(void) {
	cgi_Cvar_Update(&ui_npc_weapon);
	int currentWeaponIndex = WP_GetWeaponID(ui_npc_weapon.string);
	if (currentWeaponIndex == -1) {
		currentWeaponIndex = 1; // 0 is WP_NONE
	}
	int prevWeaponIndex = currentWeaponIndex;
	do {
		prevWeaponIndex = (prevWeaponIndex == 1) ? weaponCount - 1 : prevWeaponIndex - 1;
	} while (!weaponData[prevWeaponIndex].classname
		|| !weaponData[prevWeaponIndex].classname[0]
		|| (!weaponData[prevWeaponIndex].playerUsable && strcmp(weaponData[prevWeaponIndex].classname, "weapon_clonerandom"))
		);
	cgi_Cvar_Set("ui_npc_weapon",weaponData[prevWeaponIndex].classname);
	CG_NPC_UpdateLabel();
}

char label[256];
void CG_NPC_UpdateLabel(void) {
	cgi_Cvar_Update(&ui_npc_weapon);
	int currentWeaponIndex = WP_GetWeaponID(ui_npc_weapon.string);
	//Get Weapon Name */
	if (cgi_SP_GetStringTextString(va("SP_INGAME_%s", weaponData[currentWeaponIndex].classname), label, sizeof(label)))
	{
		; //Nothing to do
	}
	else if (cgi_SP_GetStringTextString(va("SPMOD_INGAME_%s", weaponData[currentWeaponIndex].classname), label, sizeof(label)))
	{
		; //Nothing to do
	}
	//Dynamic Weapons
	else if (!cgi_SP_GetStringTextString(va("%s_NAME", weaponData[currentWeaponIndex].classname), label, sizeof(label)))
	{
		Com_sprintf(label, sizeof(label), weaponData[currentWeaponIndex].classname);		
	}
	cgi_Cvar_Set("ui_npc_weapon_label", label);
}

void CG_DrawNpcWeaponLabel(void) {
	CG_NPC_UpdateLabel();
	const short textboxXPos = 508;
	const short textboxYPos = 118;
	const int	textboxWidth = 106;
	const int	textboxHeight = 16;
	const float	textScale = 0.75f;

	CG_DisplayBoxedText(
		textboxXPos, textboxYPos,
		textboxWidth, textboxHeight,
		label,
		CG_MagicFontToReal(4),
		textScale,
		colorTable[CT_WHITE]
	);
}

/*For the SIX player cyclers*/
//vmCvar for use and utility method
extern vmCvar_t		ui_weaponOne;
extern vmCvar_t		ui_weaponTwo;
extern vmCvar_t		ui_weaponThree;
extern vmCvar_t		ui_weaponFour;
extern vmCvar_t		ui_weaponFive;
extern vmCvar_t		ui_weaponSix;

vmCvar_t* CG_GetUiWeaponCvar(int index)
{
	switch (index)
	{
	case 1:
		return &ui_weaponOne;
	case 2:
		return &ui_weaponTwo;
	case 3:
		return &ui_weaponThree;
	case 4:
		return &ui_weaponFour;
	case 5:
		return &ui_weaponFive;
	case 6:
		return &ui_weaponSix;
	default:
		return 0;
	}
}

//Weapon cvar names & utility method
char* uiWeaponOneName = "ui_weaponOne";
char* uiWeaponTwoName = "ui_weaponTwo";
char* uiWeaponThreeName = "ui_weaponThree";
char* uiWeaponFourName = "ui_weaponFour";
char* uiWeaponFiveName = "ui_weaponFive";
char* uiWeaponSixName = "ui_weaponSix";
char* CG_GetUiWeaponName(int index)
{
	switch (index)
	{
	case 1:
		return uiWeaponOneName;
	case 2:
		return uiWeaponTwoName;
	case 3:
		return uiWeaponThreeName;
	case 4:
		return uiWeaponFourName;
	case 5:
		return uiWeaponFiveName;
	case 6:
		return uiWeaponSixName;
	default:
		return 0;
	}
}

//label cvar name & utility method
char* uiWeaponOne_labelName = "ui_weaponOne_label";
char* uiWeaponTwo_labelName = "ui_weaponTwo_label";
char* uiWeaponThree_labelName = "ui_weaponThree_label";
char* uiWeaponFour_labelName = "ui_weaponFour_label";
char* uiWeaponFive_labelName = "ui_weaponFive_label";
char* uiWeaponSix_labelName = "ui_weaponSix_label";
char* CG_GetUiWeapon_labelName(int index)
{
	switch (index)
	{
	case 1:
		return uiWeaponOne_labelName;
	case 2:
		return uiWeaponTwo_labelName;
	case 3:
		return uiWeaponThree_labelName;
	case 4:
		return uiWeaponFour_labelName;
	case 5:
		return uiWeaponFive_labelName;
	case 6:
		return uiWeaponSix_labelName;
	default:
		return 0;
	}
}

void CG_PC_UpdateLabel(int index) {
	vmCvar_t* ui_player_weapon = CG_GetUiWeaponCvar(index);
	cgi_Cvar_Update(ui_player_weapon);
	int currentWeaponIndex = WP_GetWeaponID(ui_player_weapon->string);
	//Get Weapon Name */
	if (cgi_SP_GetStringTextString(va("SP_INGAME_%s", weaponData[currentWeaponIndex].classname), label, sizeof(label)))
	{
		; //Nothing to do
	}
	else if (cgi_SP_GetStringTextString(va("SPMOD_INGAME_%s", weaponData[currentWeaponIndex].classname), label, sizeof(label)))
	{
		; //Nothing to do
	} 
	//Dynamic Weapons
	else if (!cgi_SP_GetStringTextString(va("%s_NAME", weaponData[currentWeaponIndex].classname), label, sizeof(label)))
	{
		Com_sprintf(label, sizeof(label), weaponData[currentWeaponIndex].classname);
	}
	cgi_Cvar_Set(CG_GetUiWeapon_labelName(index), label);
}

const short offsetFromPlayerLabel = 61;
const short offsetPerIndex = 15;
void CG_DrawPCWeaponLabel(int index) {
	if (index <= 0 || index >= 7)
	{
		return;
	}
	CG_PC_UpdateLabel(index);
	const short textboxXPos = 508;
	const short textboxYPos = 57+ offsetFromPlayerLabel + ((index-1)*offsetPerIndex);
	const int	textboxWidth = 106;
	const int	textboxHeight = 16;
	const float	textScale = 0.75f;

	CG_DisplayBoxedText(
		textboxXPos, textboxYPos,
		textboxWidth, textboxHeight,
		label,
		CG_MagicFontToReal(4),
		textScale,
		colorTable[CT_WHITE]
	);
}


void CG_PC_NextWeapon_f(int index) {
	if (index <= 0 || index >= 7)
	{
		return;
	}
	vmCvar_t* ui_player_weapon = CG_GetUiWeaponCvar(index);
	cgi_Cvar_Update(ui_player_weapon);
	int currentWeaponIndex = WP_GetWeaponID(ui_player_weapon->string);
	if (currentWeaponIndex == -1) {
		currentWeaponIndex = 0; // 0 is WP_NONE
	}
	int nextWeaponIndex = currentWeaponIndex;
	do {
		nextWeaponIndex = (nextWeaponIndex == weaponCount - 1) ? 0 : nextWeaponIndex + 1;
	} while (!weaponData[nextWeaponIndex].classname
		|| !weaponData[nextWeaponIndex].classname[0]
		|| !weaponData[nextWeaponIndex].playerUsable);
	cgi_Cvar_Set(CG_GetUiWeaponName(index), weaponData[nextWeaponIndex].classname);
	CG_PC_UpdateLabel(index);
}

void CG_PC_PrevWeapon_f(int index) {
	if (index <= 0 || index >= 7)
	{
		return;
	}
	vmCvar_t* ui_player_weapon = CG_GetUiWeaponCvar(index);
	cgi_Cvar_Update(ui_player_weapon);
	int currentWeaponIndex = WP_GetWeaponID(ui_player_weapon->string);
	if (currentWeaponIndex == -1) {
		currentWeaponIndex = 0; // 0 is WP_NONE
	}
	int prevWeaponIndex = currentWeaponIndex;
	do {
		prevWeaponIndex = (prevWeaponIndex <= 0) ? weaponCount - 1 : prevWeaponIndex - 1;
	} while (!weaponData[prevWeaponIndex].classname
		|| !weaponData[prevWeaponIndex].classname[0]
		|| !weaponData[prevWeaponIndex].playerUsable);
	cgi_Cvar_Set(CG_GetUiWeaponName(index), weaponData[prevWeaponIndex].classname);
	CG_PC_UpdateLabel(index);
}

/*
===============
CG_UI_DrawListWeaponCategory_f
Show the different variants of weapons for a given weapon
===============
*/
void CG_LDO_DrawWeapons(void) {
	int marX = 8, marY = 16;
	int sizeX = 70, sizeY = 55;
	int startX = 159, startY = 49;
	int posX = startX, posY = startY;
	//Icon X Pos, Icon Y Pos, Index of Weapon/Item, Ignored Icon Count (for page display)
	int ix = 0, iy = 0, iw = 0,iic = 0;
	int firstIcon = (cg.LoadoutPageSelect * LOADOUT_PAGESIZE);
	char text[1024] = { 0 };
	qhandle_t background = cgi_R_RegisterShaderNoMip("gfx/menus/w_icon_background");

	if (cg.LoadoutBaseWeaponSelect == 0) {
		return;
	}

	//Print Ammo or inventory
	if (cg.LoadoutBaseWeaponSelect == -1
		|| cg.LoadoutBaseWeaponSelect == -2)
	{
		for (iw = 0; iw < bg_numItems && iy < 3; iw++) {
			gitem_t *item = &bg_itemlist[iw];
			//Declared like this for readability
			if ( ((cg.LoadoutBaseWeaponSelect == -1 && item->giType == IT_AMMO)
				|| (cg.LoadoutBaseWeaponSelect == -2 && item->giType == IT_HOLDABLE)
				|| (cg.LoadoutBaseWeaponSelect == -2 && item->giType == IT_HEALTH)
				|| (cg.LoadoutBaseWeaponSelect == -2 && item->giType == IT_ARMOR)
				) && item->icon && item->icon[0] )
			{
				if (iic < firstIcon) {
					iic++;
					continue;
				}
				CG_DrawPic(posX, posY, sizeX, sizeY, background);
				CG_RegisterItemVisuals(iw);
				itemInfo_t *itemInfo = &cg_items[iw];
				CG_DrawPic(posX + 7, posY, sizeY, sizeY, itemInfo->icon);

				//Next icon
				if (ix == 5)
				{
					ix = 0;
					posX = startX;
					iy++;
					posY += marY + sizeY;
				}
				else
				{
					ix++;
					posX += marX + sizeX;
				}
			}
		}

		if (cg.LoadoutWeaponSelect > 0) {
			char count[128];
			if (cg.LoadoutBaseWeaponSelect == -1) Q_strncpyz(count, "50 Units", sizeof(count)); else Q_strncpyz(count,"1 unit",sizeof(count));
			// Print the item Description
			//Dynamic Weapons
			if (cgi_SP_GetStringTextString(va("%s_NAME", bg_itemlist[cg.LoadoutWeaponSelect].classname), text, sizeof(text)))
			{
				; //Nothing to do
			}
			if (cgi_SP_GetStringTextString(va("SP_INGAME_%s", bg_itemlist[cg.LoadoutWeaponSelect].classname), text, sizeof(text)))
			{
				; //Nothing to do
			}
			else if (!cgi_SP_GetStringTextString(va("SPMOD_INGAME_%s", bg_itemlist[cg.LoadoutWeaponSelect].classname), text, sizeof(text)))
			{
				Com_sprintf(text, sizeof(text), "Unknown Item"); 
			}
			Com_sprintf(text, sizeof(text), va("%s\nPress this item to get %s of it.", text, count));
		}
	}
	else if(cg.LoadoutBaseWeaponSelect == -3)
	{
		for (int i = 1; i < WEAPON_BUCKETS_SIZE && iy < 3; i++)
		{
			iw = weaponBuckets[i];
			if (iw <= 0) {
				continue;
			}
			if (weaponData[iw].playerUsable)
			{
				if (iic < firstIcon) {
					iic++;
					continue;
				}
				CG_DrawPic(posX, posY, sizeX, sizeY, background);
				weaponInfo_t* weaponInfo;
				CG_RegisterWeapon(iw);
				weaponInfo = &cg_weapons[iw];
				if (cg.snap->ps.weapons[iw])
				{
					CG_DrawPic(posX + 7, posY, sizeY, sizeY, weaponInfo->weaponIcon);
				}
				else
				{
					CG_DrawPic(posX + 7, posY, sizeY, sizeY, weaponInfo->weaponIconNoAmmo);
				}
				//Next icon
				if (ix == 5)
				{
					ix = 0;
					posX = startX;
					iy++;
					posY += marY + sizeY;
				}
				else
				{
					ix++;
					posX += marX + sizeX;
				}
			}
		}
		//Draw the current weapon Description to the screen?

		if (cg.LoadoutWeaponSelect > 0) {
			// Print the weapon description
			//Dynamic Weapons
			if (cgi_SP_GetStringTextString(va("%s_DESC", weaponData[cg.LoadoutWeaponSelect].classname), text, sizeof(text)))
			{
				; //Nothing to do
			}
			else if (cgi_SP_GetStringTextString(va("SP_INGAME_%s", weaponDesc[cg.LoadoutWeaponSelect - 1]), text, sizeof(text)))
			{
				; //Nothing to do
			}
			else if (!cgi_SP_GetStringTextString(va("SPMOD_INGAME_%s", weaponDesc[cg.LoadoutWeaponSelect - 1]), text, sizeof(text)))
			{
				Com_sprintf(text, sizeof("No weapon description Found") + 1, "No weapon description Found");
			}
		}
	}
	else {
		//Search in the current Bucket
		int bucketIndex = -1;
		for (int i = 0; i < WEAPON_BUCKETS_SIZE; i++) {
			if (weaponBuckets[i] == -cg.LoadoutBaseWeaponSelect) {
				bucketIndex = i;
				break;
			}
		}
		//Show all the icon of the bucket
		for (int i = bucketIndex+1; i < WEAPON_BUCKETS_SIZE && iy < 3; i++)
		{
			iw = weaponBuckets[i];
			if (iw < 0) {
				break; // Shown all the icons
			}
			if (iw > 0 && weaponData[iw].playerUsable)
			{
				if (iic < firstIcon) {
					iic++;
					continue;
				}
				CG_DrawPic(posX, posY, sizeX, sizeY, background);
				weaponInfo_t* weaponInfo;
				CG_RegisterWeapon(iw);
				weaponInfo = &cg_weapons[iw];
				if (cg.snap->ps.weapons[iw])
				{
					CG_DrawPic(posX + 7, posY, sizeY, sizeY, weaponInfo->weaponIcon);
				}
				else
				{
					CG_DrawPic(posX + 7, posY, sizeY, sizeY, weaponInfo->weaponIconNoAmmo);
				}
				//Next icon
				if (ix == 5)
				{
					ix = 0;
					posX = startX;
					iy++;
					posY += marY + sizeY;
				}
				else
				{
					ix++;
					posX += marX + sizeX;
				}
			}
		}
		//Draw the current weapon Description to the screen?

		if (cg.LoadoutWeaponSelect > 0) {
			// Print the weapon description
			if (cgi_SP_GetStringTextString(va("%s_DESC", weaponData[cg.LoadoutWeaponSelect].classname), text, sizeof(text)))
			{
				; //Nothing to do
			}
			else if (cgi_SP_GetStringTextString(va("SP_INGAME_%s", weaponDesc[cg.LoadoutWeaponSelect - 1]), text, sizeof(text)))
			{
				; //Nothing to do
			}
			else if (!cgi_SP_GetStringTextString(va("SPMOD_INGAME_%s", weaponDesc[cg.LoadoutWeaponSelect - 1]), text, sizeof(text)))
			{
				Com_sprintf(text, sizeof("No weapon description Found") + 1, "No weapon description Found");
			}
		}
	}
	const short textboxXPos = 156;
	const short textboxYPos = 273;
	const int	textboxWidth = 466;
	const int	textboxHeight = 136;
	const float	textScale = 0.75f;

	CG_DisplayBoxedText(
		textboxXPos, textboxYPos,
		textboxWidth, textboxHeight,
		text,
		CG_MagicFontToReal(4),
		textScale,
		colorTable[CT_WHITE]
	);
}

/*
===============
CG_DPNextWeapon_f
===============
*/
void CG_DPNextWeapon_f( void ) {
	int		i;
	int		original;

	if ( !cg.snap ) {
		return;
	}

	original = cg.DataPadWeaponSelect;
	if (original < 0 || original >= weaponCount) {
		original = 1;
	}

	//Search where is the current weapon in the array
	int currentWeaponIndex = - 1;
	for (i = 0; i < WEAPON_BUCKETS_SIZE; i++) {
		if (weaponBuckets[i] == original) {
			currentWeaponIndex = i;
			break;
		}
	}
	//Cycle forward in the weaponBucketArray, ignoring negatives values
	for (i = 0; i <= WEAPON_BUCKETS_SIZE; i++)
	{
		currentWeaponIndex++;
		//Go back at the beggining of the array
		if (currentWeaponIndex == WEAPON_BUCKETS_SIZE) {
			currentWeaponIndex = 0;
		}
		//Never try to switch to Bucket or WP_NONE
		if (weaponBuckets[currentWeaponIndex] <= 0) {
			continue;
		}


		if (CG_WeaponSelectable(weaponBuckets[currentWeaponIndex], original, qtrue))
		{
			cg.DataPadWeaponSelect = weaponBuckets[currentWeaponIndex];
			return;
		}
	}
}

/*
==============
CG_Dualwield_f
==============
*/
void CG_Dualwield_f(void)
{
	if (cg_dualWielding.integer)
	{
		cg_dualWielding.integer = 0;
	}
	else
	{
		cg_dualWielding.integer = 1;
	}
}


/*
===============
CG_DPPrevWeapon_f
===============
*/
void CG_DPPrevWeapon_f( void )
{
	int		i;
	int		original;

	if ( !cg.snap )
	{
		return;
	}

	original = cg.DataPadWeaponSelect;
	if (original < 0 || original >= weaponCount) {
		original = 1;
	}

	//Search where is the current weapon in the array
	int currentWeaponIndex = - 1;
	for (i = 0; i <= WEAPON_BUCKETS_SIZE; i++) {
		if (weaponBuckets[i] == original) {
			currentWeaponIndex = i;
			break;
		}
	}
	//Cycle backward in the weaponBucketArray, ignoring negatives values
	for (i = 0; i <= WEAPON_BUCKETS_SIZE; i++)
	{
		currentWeaponIndex--;
		//Go back at the end of the array
		if (currentWeaponIndex < 0) {
			currentWeaponIndex = WEAPON_BUCKETS_SIZE - 1;
		}
		//Never try to switch to Bucket or WP_NONE
		if (weaponBuckets[currentWeaponIndex] <= 0) {
			continue;
		}


		if (CG_WeaponSelectable(weaponBuckets[currentWeaponIndex], original, qtrue))
		{
			cg.DataPadWeaponSelect = weaponBuckets[currentWeaponIndex];
			return;
		}
	}
}

/*
===============
CG_PrevWeapon_f
===============
*/
void CG_PrevWeapon_f( void ) {
	int		i;
	int		original;

	if ( !cg.snap ) {
		return;
	}

	if( g_entities[0].flags & FL_LOCK_PLAYER_WEAPONS )
	{
		CG_PlayerLockedWeaponSpeech( qfalse );
		return;
	}

	if( g_entities[0].client && g_entities[0].client->NPC_class == CLASS_ATST )
	{
		CG_ToggleATSTWeapon();
		return;
	}

	if ( cg.snap->ps.eFlags & EF_LOCKED_TO_WEAPON )
	{
		// can't do any sort of weapon switching when in the emplaced gun
		return;
	}

	if ( cg.snap->ps.viewEntity )
	{
		// yeah, probably need a better check here
		if ( g_entities[cg.snap->ps.viewEntity].client && ( g_entities[cg.snap->ps.viewEntity].client->NPC_class == CLASS_R5D2
				|| g_entities[cg.snap->ps.viewEntity].client->NPC_class == CLASS_R2D2
				|| g_entities[cg.snap->ps.viewEntity].client->NPC_class == CLASS_MOUSE ))
		{
			return;
		}
	}

	original = cg.weaponSelect;

	qboolean canSelectNone = qfalse;
	if (G_IsRidingVehicle(&g_entities[cg.snap->ps.viewEntity]))
	{
		canSelectNone = qtrue;
	}

	//Search where is the current weapon in the array
	int currentWeaponIndex = -1;
	for (i = 0; i < WEAPON_BUCKETS_SIZE; i++) {
		if (weaponBuckets[i] == original) {
			currentWeaponIndex = i;
			break;
		}
	}
	//Cycle backward in the weaponBucketArray, ignoring negatives values
	for (i = 0 ; i < WEAPON_BUCKETS_SIZE; i++ )
	{
		currentWeaponIndex--;
		//Go back at the end of the array
		if (currentWeaponIndex < 0) {
			currentWeaponIndex = WEAPON_BUCKETS_SIZE - 1;
		}
		//Never try to switch to Bucket or WP_NONE
		if (weaponBuckets[currentWeaponIndex] < 0 || (!canSelectNone && weaponBuckets[currentWeaponIndex] == 0)) {
			continue;
		}


		if ( CG_WeaponSelectable(weaponBuckets[currentWeaponIndex], original, qfalse ) )
		{
			//Found you!
			SetWeaponSelectTime();
			cg.weaponSelect = weaponBuckets[currentWeaponIndex];
			return;
		}
	}
}

/*
void CG_ChangeWeapon( int num )

  Meant to be called from the normal game, so checks the game-side weapon inventory data
*/
void CG_ChangeWeapon( int num )
{
	gentity_t	*player = &g_entities[0];

	if ( num < WP_NONE || num >= weaponCount)
	{
		return;
	}

	if( player->flags & FL_LOCK_PLAYER_WEAPONS )
	{
		CG_PlayerLockedWeaponSpeech( qfalse );
		return;
	}

	if ( player->client != NULL && !(player->client->ps.weapons[num]) )
	{
		return;		// don't have the weapon
	}

	// because we don't have an empty hand model for the thermal, don't allow selecting that weapon if it has no ammo
	if ( (num == WP_THERMAL &&  cg.snap && cg.snap->ps.ammo[AMMO_THERMAL] <= 0 )
		|| (num == WP_TRIP_MINE && cg.snap && cg.snap->ps.ammo[AMMO_TRIPMINE] <= 0)
		|| (num == WP_DET_PACK && cg.snap && cg.snap->ps.ammo[AMMO_DETPACK] <= 0)
	)
	{
		return;
	}
	
	int baseWeaponNum = weaponData[num].baseWeaponNum;
	if (baseWeaponNum == WP_THERMAL
		|| baseWeaponNum == WP_DET_PACK
		|| baseWeaponNum == WP_TRIP_MINE) {
		if (cg.snap && cg.snap->ps.ammo[weaponData[num].ammoIndex] <= 0) {
			return;
		}
	}

	SetWeaponSelectTime();
//	cg.weaponSelectTime = cg.time;
	cg.weaponSelect = num;
}

/*
===============
CG_Weapon_f
===============
*/
void CG_Weapon_f( void )
{
	int	num;

	if ( cg.weaponSelectTime + 100 > cg.time )
	{
		return;
	}

	if ( !cg.snap ) {
		return;
	}
	/*
	if ( cg.snap->ps.pm_flags & PMF_FOLLOW ) {
		return;
	}
	*/

	if( g_entities[0].flags & FL_LOCK_PLAYER_WEAPONS )
	{
		CG_PlayerLockedWeaponSpeech( qfalse );
		return;
	}

	if( g_entities[0].client && g_entities[0].client->NPC_class == CLASS_ATST )
	{
		CG_ToggleATSTWeapon();
		return;
	}

	if ( cg.snap->ps.eFlags & EF_LOCKED_TO_WEAPON )
	{
		// can't do any sort of weapon switching when in the emplaced gun
		return;
	}

	if ( cg.snap->ps.viewEntity )
	{
		// yeah, probably need a better check here
		if ( g_entities[cg.snap->ps.viewEntity].client && ( g_entities[cg.snap->ps.viewEntity].client->NPC_class == CLASS_R5D2
				|| g_entities[cg.snap->ps.viewEntity].client->NPC_class == CLASS_R2D2
				|| g_entities[cg.snap->ps.viewEntity].client->NPC_class == CLASS_MOUSE ))
		{
			return;
		}
	}

	num = atoi( CG_Argv( 1 ) );

	if ( num < WB_OTHERS || num >= weaponCount) {
		return;
	}

	if ( num == WP_SABER )
	{//lightsaber
		if ( ! ( cg.snap->ps.weapons[num] ) )
		{//don't have saber, try stun baton
			if ( ( cg.snap->ps.weapons[WP_STUN_BATON] ) )
			{
				num = WP_STUN_BATON;
			}
			else
			{//don't have stun baton, use fists
				num = WP_MELEE;
			}
		}
		else if ( num == cg.snap->ps.weapon )
		{//already have it up, let's try to toggle it
			if ( !in_camera )
			{//player can't activate/deactivate saber when in a cinematic
				//can't toggle it if not holding it and not controlling it or dead
				if ( cg.predicted_player_state.stats[STAT_HEALTH] > 0 && (!cg_entities[0].gent->client->ps.saberInFlight || (&g_entities[cg_entities[0].gent->client->ps.saberEntityNum] != NULL && g_entities[cg_entities[0].gent->client->ps.saberEntityNum].s.pos.trType == TR_LINEAR) ) )
				{//it's either in-hand or it's under telekinetic control
					if ( cg_entities[0].gent->client->ps.SaberActive() )
					{//a saber is on
						if ( cg_entities[0].gent->client->ps.dualSabers
							&& cg_entities[0].gent->client->ps.saber[1].Active() )
						{//2nd saber is on, turn it off, too
							cg_entities[0].gent->client->ps.saber[1].Deactivate();
						}
						cg_entities[0].gent->client->ps.saber[0].Deactivate();
						Inquisitor_Stop(cg_entities[0].gent, qtrue);
						if ( cg_entities[0].gent->client->ps.saberInFlight )
						{//play it on the saber
							cgi_S_UpdateEntityPosition( cg_entities[0].gent->client->ps.saberEntityNum, g_entities[cg_entities[0].gent->client->ps.saberEntityNum].currentOrigin );
							cgi_S_StartSound (NULL, cg_entities[0].gent->client->ps.saberEntityNum, CHAN_AUTO, cgs.sound_precache[cg_entities[0].gent->client->ps.saber[0].soundOff] );
						}
						else
						{
							cgi_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.sound_precache[cg_entities[0].gent->client->ps.saber[0].soundOff] );
						}
					}
					else
					{//turn them both on
						cg_entities[0].gent->client->ps.SaberActivate();
						Inquisitor_Spin(cg_entities[0].gent, qfalse);
					}
				}
			}
		}
	}

	if (num < 0) {
		int currentWeaponSelected = cg.weaponSelect;
		int bucketIndex = -1;
		int baseWeaponIndex = -1;
		int currentWeaponIndex = -1;
		int bucketSize = 0;
		//Initialize the bucket indexes
		for (int i = 0; i < WEAPON_BUCKETS_SIZE; i++)
		{
			//Let's search for the selectedBucketIndex
			if (weaponBuckets[i] == num)
			{
				bucketIndex = i;
			}
			//Now search the current weapon if it's in the current Bucket
			else if (bucketIndex >= 0 && weaponBuckets[i] == currentWeaponSelected) {
				baseWeaponIndex = i;
			}
			//If we enter a new bucket, we stop searching
			else if (bucketIndex >= 0 && weaponBuckets[i] < 0) {
				bucketSize = i - bucketIndex;
				break;
			}
		}
		if (baseWeaponIndex == -1) {
			baseWeaponIndex = bucketIndex;
		}
		currentWeaponIndex = baseWeaponIndex;
		//Search in the current bucket the next weapon
		for (int i = 0; i < bucketSize; i++)
		{
			//Test next weapon
			currentWeaponIndex++;
			//If the next weapon is the last one, back to the first
			if (currentWeaponIndex >= (bucketIndex + bucketSize)) {
				currentWeaponIndex = bucketIndex + 1;
			}
			//Can we switch to this weapon?
			if (CG_WeaponSelectable(weaponBuckets[currentWeaponIndex], cg.snap->ps.weapon, qfalse)) {
				SetWeaponSelectTime();
				cg.weaponSelect = weaponBuckets[currentWeaponIndex];
				return;
			}
		}
	}
	if (!CG_WeaponSelectable(num, cg.snap->ps.weapon, qfalse))
	{
		return;
	}

	SetWeaponSelectTime();
	cg.weaponSelect = num;
}

/*
===================
CG_OutOfAmmoChange

The current weapon has just run out of ammo
===================
*/
void CG_OutOfAmmoChange( void ) {
	int		i;
	int		original;

	if ( cg.weaponSelectTime + 200 > cg.time )
		return;

	if( g_entities[0].client && g_entities[0].client->NPC_class == CLASS_ATST )
	{
		CG_ToggleATSTWeapon();
		return;
	}

	original = cg.weaponSelect;

	for ( i = WP_ROCKET_LAUNCHER; i > 0 ; i-- )
	{
		// We don't want the emplaced, melee, or explosive devices here
		if ( original != i && CG_WeaponSelectable( i, original, qfalse ) )
		{
			SetWeaponSelectTime();
			cg.weaponSelect = i;
			break;
		}
	}

	if ( cg_autoswitch.integer != 1 )
	{
		// didn't have that, so try these. Start with thermal...
		for ( i = WP_THERMAL; i <= WP_DET_PACK; i++ )
		{
			// We don't want the emplaced, or melee here
			if ( original != i && CG_WeaponSelectable( i, original, qfalse ) )
			{
				if ( i == WP_DET_PACK && cg.snap->ps.ammo[weaponData[i].ammoIndex] <= 0 )
				{
					// crap, no point in switching to this
				}
				else
				{
					SetWeaponSelectTime();
					cg.weaponSelect = i;
					break;
				}
			}
		}
	}

	// try stun baton as a last ditch effort
	if ( CG_WeaponSelectable( WP_STUN_BATON, original, qfalse ))
	{
		SetWeaponSelectTime();
		cg.weaponSelect = WP_STUN_BATON;
	}
}


/*
=================
CG_PlayerIsDualWielding
=================
*/
qboolean CG_PlayerIsDualWielding(int weapon)
{
	return (qboolean)(cg_dualWielding.integer && weaponData[weapon].weaponCategory == WC_PISTOL);
}

/*
========================
CG_ChangeFirstPersonView
========================
*/
qboolean CG_ChangeFirstPersonView(void)
{
	int weapon = weaponData[player->client->ps.weapon].baseWeaponNum ? weaponData[player->client->ps.weapon].baseWeaponNum : player->client->ps.weapon;
	return (qboolean)(CG_PlayerIsDualWielding(weapon) || weapon  == WP_SBD || weapon == WP_DROIDEKA);
}


/*
===================================================================================================

WEAPON EVENTS

===================================================================================================
*/

/*
================
CG_FireWeapon

Caused by an EV_FIRE_WEAPON event
================
*/
void CG_FireWeapon( centity_t *cent, int attackIndex)
{
	entityState_t *ent;
	//weaponInfo_t	*weap;

	ent = &cent->currentState;
	if ( ent->weapon == WP_NONE ) {
		return;
	}
	if ( ent->weapon >= weaponCount) {
		CG_Error( "CG_FireWeapon: ent->weapon >= WP_NUM_WEAPONS" );
		return;
	}
	if ( (ent->weapon == WP_TUSKEN_RIFLE || ent->weapon == WP_NOGHRI_STICK) && cent->gent->client)
	{
		if (cent->gent->client->ps.torsoAnim==BOTH_TUSKENATTACK1 ||
		cent->gent->client->ps.torsoAnim==BOTH_TUSKENATTACK2 ||
		cent->gent->client->ps.torsoAnim==BOTH_TUSKENATTACK3 ||
		cent->gent->client->ps.torsoAnim==BOTH_TUSKENLUNGE1)
		{
			return;
		}
	}

	//weap = &cg_weapons[ ent->weapon ];

	// mark the entity as muzzle flashing, so when it is added it will
	// append the flash to the weapon model
	cent->muzzleFlashTime = cg.time;
	cent->altFire = (attackIndex == 1 || attackIndex == 3) ? qtrue: qfalse;
	cent->attack_index = attackIndex;

	if ( ent->weapon == WP_SABER )
	{
		if ( cent->pe.lightningFiring )
		{
			return;
		}
	}

}

/*
=================
CG_BounceEffect

Caused by an EV_BOUNCE | EV_BOUNCE_HALF event
=================
*/
void CG_BounceEffect( centity_t *cent, int weapon, vec3_t origin, vec3_t normal )
{
	int baseWeapon = weaponData[weapon].baseWeaponNum ? weaponData[weapon].baseWeaponNum : weapon;
	switch(baseWeapon)
	{
	case WP_THERMAL:
		if ( rand() & 1 ) {
			cgi_S_StartSound( origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.grenadeBounce1 );
		} else {
			cgi_S_StartSound( origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.grenadeBounce2 );
		}
		break;

	case WP_BOWCASTER:
		theFxScheduler.PlayEffect( cgs.effects.bowcasterBounceEffect, origin, normal );
		break;

	case WP_FLECHETTE:
		theFxScheduler.PlayEffect( "flechette/ricochet", origin, normal );
		break;

	default:
		if ( rand() & 1 ) {
			cgi_S_StartSound( origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.grenadeBounce1 );
		} else {
			cgi_S_StartSound( origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.grenadeBounce2 );
		}
		break;
	}
}

//----------------------------------------------------------------------
void CG_MissileStick( centity_t *cent, int weapon, vec3_t position )
//----------------------------------------------------------------------
{
	sfxHandle_t snd = 0;

	int baseWeapon = weaponData[weapon].baseWeaponNum ? weaponData[weapon].baseWeaponNum : weapon;
	switch(baseWeapon)
	{
	case WP_FLECHETTE:
		snd = cgs.media.flechetteStickSound;
		break;

	case WP_DET_PACK:
		snd = cgs.media.detPackStickSound;
		break;

	case WP_TRIP_MINE:
		snd = cgs.media.tripMineStickSound;
		break;
	}

	if ( snd )
	{
		cgi_S_StartSound( NULL, cent->currentState.number, CHAN_AUTO, snd );
	}
}

qboolean CG_VehicleWeaponImpact( centity_t *cent )
{//see if this is a missile entity that's owned by a vehicle and should do a special, overridden impact effect
	if (cent->currentState.otherEntityNum2
		&& g_vehWeaponInfo[cent->currentState.otherEntityNum2].iImpactFX)
	{//missile is from a special vehWeapon
		CG_PlayEffectID(g_vehWeaponInfo[cent->currentState.otherEntityNum2].iImpactFX, cent->lerpOrigin, cent->gent->pos1);
		return qtrue;
	}
	return qfalse;
}

/*
=================
CG_MissileHitWall

Caused by an EV_MISSILE_MISS event, or directly by local bullet tracing
=================
*/
void CG_MissileHitWall( centity_t *cent, int weapon, vec3_t origin, vec3_t dir, qboolean altFire )
{
	weaponData_t* wpnData = &weaponData[weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[cent->gent->attack_index];
	int baseWeapon = weaponData[weapon].baseWeaponNum ? weaponData[weapon].baseWeaponNum : weapon;

    //Force powers
	if (cent->currentState.powerups & (1 << PW_FORCE_PROJECTILE)) {
		switch (baseWeapon)
		{
		case WP_ROCKET_LAUNCHER:
			FX_BlastHitWall(origin, dir);
			break;
		case WP_CONCUSSION:
			FX_DestructionHitWall(origin, dir);
			break;
		case WP_DISRUPTOR:
			FX_StrikeHitWall(origin, dir);
			break;
		default:
			break;
		}
		return;
	}

	switch (attackData->firingLogic)
	{
		case FL_STUNBATON:
		case FL_MELEE:
		    return;
		case FL_MISSILE:
		case FL_MISSILE_AIMED:
		case FL_DEMP2:
		case FL_BLASTER:
		case FL_BOWCASTER:
		case FL_GRENADE_LAUNCHER:
		case FL_FLECHETTE_ALT:
		case FL_NOGHRI:
		case FL_LASER_TRAP:
		case FL_PROXIMITY_TRAP:
		case FL_EXPLOSIVES:
		case FL_FLECHETTE:
			FX_GenericBlasterHitWall(cent->gent, weapon, origin, dir);
		    return;
		case FL_BLASTER_CHARGED:
			FX_GenericChargedBlasterHitWall(cent->gent, weapon, origin, dir);
		    return;
		case FL_DEMP2_ALT:
			return;
		case FL_BEAM:
		case FL_FULL_BEAM:
		case FL_BEAM_CHARGED:
		    break;
		case FL_GRENADE:
		case FL_IMPACT_GRENADE:
			if (weaponData[weapon].attackData[cent->gent->attack_index].explosionEffect[0])
			{
				theFxScheduler.PlayEffect(weaponData[weapon].attackData[cent->gent->attack_index].explosionEffect, origin, dir);
			}
			else
			{
				theFxScheduler.PlayEffect("thermal/explosion", origin, dir);
			}

			if (weaponData[weapon].attackData[cent->gent->attack_index].shockwaveEffect[0])
			{
				theFxScheduler.PlayEffect(weaponData[weapon].attackData[cent->gent->attack_index].shockwaveEffect, origin);
			}
			else
			{
				theFxScheduler.PlayEffect("thermal/shockwave", origin);
			}
		    break;
		case FL_FLAMETHROWER:
			theFxScheduler.PlayEffect("env/small_fire.efx", origin);
			break;
		case FL_OTHER:
		case FL_NONE:
		    break;
	}
}




/*
-------------------------
CG_MissileHitPlayer
-------------------------
*/
void CG_MissileHitPlayer( centity_t *cent, int weapon, vec3_t origin, vec3_t dir, int attackIndex)
{
	gentity_t *other = NULL;
	qboolean	humanoid = qtrue;

	if ( cent->gent )
	{
		other = &g_entities[cent->gent->s.otherEntityNum];
		if( other->client )
		{
			class_t	npc_class = other->client->NPC_class;
			// check for all droids, maybe check for certain monsters if they're considered non-humanoid..?
			if ( npc_class == CLASS_SEEKER || npc_class == CLASS_PROBE || npc_class == CLASS_MOUSE ||
				 npc_class == CLASS_GONK || npc_class == CLASS_R2D2 || npc_class == CLASS_R5D2 ||
				 npc_class == CLASS_PROTOCOL || npc_class == CLASS_MARK1 || npc_class == CLASS_MARK2 ||
				 npc_class == CLASS_INTERROGATOR || npc_class == CLASS_ATST || npc_class == CLASS_SENTRY )
			{
				humanoid = qfalse;
			}
		}
	}
	weaponData_t* wpnData = &weaponData[weapon];
	weaponAttackData_t* attackData = &wpnData->attackData[cent->gent->attack_index];
	int baseWeapon = weaponData[weapon].baseWeaponNum ? weaponData[weapon].baseWeaponNum : weapon;

    //Force powers
	if (cent->currentState.powerups & (1 << PW_FORCE_PROJECTILE)) {
		switch(baseWeapon)
		{
		case WP_ROCKET_LAUNCHER:
			FX_BlastHitWall(origin, dir);
			break;
		case WP_CONCUSSION:
			FX_DestructionHitPlayer(origin, dir, humanoid);
			break;
		case WP_DISRUPTOR:
			FX_StrikeHitWall(origin, dir);
			break;
		default:
			break;
		}
		return;
	}
	switch (attackData->firingLogic)
	{
	case FL_STUNBATON:
	case FL_MELEE:
		return;
	case FL_DEMP2:		
		// Do a full body effect here for some more feedback
		if (other && other->client)
		{
			other->s.powerups |= (1 << PW_SHOCKED);
			other->client->ps.powerups[PW_SHOCKED] = cg.time + 1000;
		}
	case FL_MISSILE:
	case FL_MISSILE_AIMED:
	case FL_BLASTER:
	case FL_BOWCASTER:
	case FL_GRENADE_LAUNCHER:
	case FL_FLECHETTE_ALT:
	case FL_NOGHRI:
	case FL_LASER_TRAP:
	case FL_PROXIMITY_TRAP:
	case FL_EXPLOSIVES:
	case FL_BLASTER_CHARGED:
	case FL_FLECHETTE:
		FX_GenericBlasterHitPlayer(cent->gent, weapon, origin, dir, other, humanoid);
		return;
	case FL_DEMP2_ALT:
		return;
	case FL_BEAM:
	case FL_FULL_BEAM:
	case FL_BEAM_CHARGED:
		break;
	case FL_GRENADE:
	case FL_IMPACT_GRENADE:
		if (weaponData[weapon].attackData[cent->gent->attack_index].explosionEffect[0])
		{
			theFxScheduler.PlayEffect(weaponData[weapon].attackData[cent->gent->attack_index].explosionEffect, origin, dir);
		}
		else
		{
			theFxScheduler.PlayEffect("thermal/explosion", origin, dir);
		}

		if (weaponData[weapon].attackData[cent->gent->attack_index].shockwaveEffect[0])
		{
			theFxScheduler.PlayEffect(weaponData[weapon].attackData[cent->gent->attack_index].shockwaveEffect, origin);
		}
		else
		{
			theFxScheduler.PlayEffect("thermal/shockwave", origin);
		}
		break;
		
	case FL_FLAMETHROWER:
	case FL_OTHER:
	case FL_NONE:
		break;
	}
}

char baseHitWallEffects[][64] = {
	"",//WP_NONE
	"",//WP_SABER
	"bryar/wall_impact",//WP_BLASTER_PISTOL
	"blaster/wall_impact",//WP_BLASTER
	"disruptor/wall_impact",//WP_DISRUPTOR
	"bowcaster/explosion",//WP_BOWCASTER
	"repeater/wall_impact",//WP_REPEATER
	"demp2/wall_impact",//WP_DEMP2
	"flechette/wall_impact",//WP_FLECHETTE
	"rocket/explosion",//WP_ROCKET_LAUNCHER
	"",//WP_THERMAL
	"tripmine/explosion",//WP_TRIP_MINE
	"detpack/explosion",//WP_DET_PACK
	"concussion/explosion",//WP_CONCUSSION
	"",//WP_MELEE
	"atst/wall_impact",//WP_ATST_MAIN
	"atst/side_main_impact",//WP_ATST_SIDE
	"",//WP_STUN_BATON
	"bryar/wall_impact",//WP_BRYAR_PISTOL
	"eweb/wall_impact",//WP_EMPLACED_GUN
	"",//WP_BOT_LASER
	"turret/flesh_impact",//WP_TURRET
	"",//WP_TIE_FIGHTER
	"",//WP_RAPID_FIRE_CONC
	"bryar/wall_impact",//WP_JAWA
	"tusken/hitwall" ,//WP_TUSKEN_RIFLE
	"",//WP_TUSKEN_STAFF
	"",//WP_SCEPTER
	"noghri_stick/flesh_impact",//WP_NOGHRI_STICK
	"blaster/wall_impact",//WP_BATTLEDROID
	"blaster/wall_impact",//WP_THEFIRSTORDER
	"clone/wall_impact",//WP_CLONECARBINE
	"blaster/wall_impact",//WP_REBELBLASTER
	"clone/wall_impact",//WP_CLONERIFLE
	"clone/wall_impact",//WP_CLONECOMMANDO
	"blaster/wall_impact",//WP_REBELRIFLE
	"bryar/wall_impact",//WP_REY
	"blaster/wall_impact",//WP_JANGO
	"blaster/wall_impact",//WP_BOBA
	"clone/wall_impact",//WP_CLONEPISTOL
	"blaster/wall_impact",//WP_CIS_SNIPER
	"blaster/wall_impact",//WP_SBD
	"blaster/wall_impact",//WP_DROIDEKA
};
char baseHitFleshEffects[][64] = {
	"",//WP_NONE
	"",//WP_SABER
	"bryar/flesh_impact",//WP_BLASTER_PISTOL
	"blaster/flesh_impact",//WP_BLASTER
	"disruptor/flesh_impact",//WP_DISRUPTOR
	"bowcaster/explosion",//WP_BOWCASTER
	"repeater/flesh_impact",//WP_REPEATER
	"demp2/flesh_impact",//WP_DEMP2
	"flechette/flesh_impact",//WP_FLECHETTE
	"rocket/explosion",//WP_ROCKET_LAUNCHER
	"",//WP_THERMAL
	"tripmine/explosion",//WP_TRIP_MINE
	"detpack/explosion",//WP_DET_PACK
	"concussion/explosion",//WP_CONCUSSION
	"",//WP_MELEE
	"atst/flesh_impact",//WP_ATST_MAIN
	"atst/side_main_impact",//WP_ATST_SIDE
	"",//WP_STUN_BATON
	"bryar/flesh_impact",//WP_BRYAR_PISTOL
	"eweb/flesh_impact",//WP_EMPLACED_GUN
	"",//WP_BOT_LASER
	"turret/wall_impact",//WP_TURRET
	"",//WP_TIE_FIGHTER
	"",//WP_RAPID_FIRE_CONC
	"bryar/flesh_impact",//WP_JAWA
	"tusken/hit" ,//WP_TUSKEN_RIFLE
	"",//WP_TUSKEN_STAFF
	"",//WP_SCEPTER
	"noghri_stick/flesh_impact",//WP_NOGHRI_STICK
	"blaster/flesh_impact",//WP_BATTLEDROID
	"blaster/flesh_impact",//WP_THEFIRSTORDER
	"clone/flesh_impact",//WP_CLONECARBINE
	"blaster/flesh_impact",//WP_REBELBLASTER
	"clone/flesh_impact",//WP_CLONERIFLE
	"clone/flesh_impact",//WP_CLONECOMMANDO
	"blaster/flesh_impact",//WP_REBELRIFLE
	"bryar/flesh_impact",//WP_REY
	"blaster/flesh_impact",//WP_JANGO
	"blaster/flesh_impact",//WP_BOBA
	"clone/flesh_impact",//WP_CLONEPISTOL
	"blaster/flesh_impact",//WP_CIS_SNIPER
	"blaster/flesh_impact",//WP_SBD
	"blaster/flesh_impact",//WP_DROIDEKA
};
