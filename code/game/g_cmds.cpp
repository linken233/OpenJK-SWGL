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

#include "g_local.h"
#include "g_lightedit.h"
#include "objectives.h"
#include "wp_saber.h"
#include "g_vehicles.h"
#include "g_functions.h"
#include "../cgame/cg_local.h"
#include "b_local.h"
#include "g_characters.h"

extern	bool		in_camera;
extern stringID_table_t SaberStyleTable[];

extern cvar_t *static_cam;

extern cvar_t* g_weaponOne;
extern cvar_t* g_weaponTwo;
extern cvar_t* g_weaponThree;
extern cvar_t* g_weaponFour;
extern cvar_t* g_weaponFive;
extern cvar_t* g_weaponSix;

extern void ForceHeal( gentity_t *self );
extern void ForceGrip( gentity_t *self );
extern void ForceTelepathy( gentity_t *self );
extern void ForceRage( gentity_t *self );
extern void ForceProtect( gentity_t *self );
extern void ForceAbsorb( gentity_t *self );
extern void ForceSeeing( gentity_t *self );
extern void ForceStasis( gentity_t *self );
extern void ForceDestruction(gentity_t *self);
extern void ForceGrasp(gentity_t *self);
extern void ForceFear(gentity_t *self);
extern void ForceLightningStrike(gentity_t *self);
extern void ForceBlast(gentity_t *self);
extern void G_CreateG2AttachedWeaponModel( gentity_t *ent, const char *psWeaponModel, int boltNum, int weaponNum );
extern void G_StartMatrixEffect( gentity_t *ent, int meFlags = 0, int length = 1000, float timeScale = 0.0f, int spinTime = 0 );
extern void ItemUse_Bacta(gentity_t *ent);
extern void WP_DropWeapon_Configurable(gentity_t* dropper, vec3_t velocity, bool fromConsoleCommand, bool deleteWeapon,bool forceNoWeapon);
extern gentity_t *G_GetSelfForPlayerCmd( void );

/*
==================
CheatsOk
==================
*/
qboolean	CheatsOk( gentity_t *ent ) {
	if ( !g_cheats->integer ) {
		gi.SendServerCommand( ent-g_entities, "print \"Cheats are not enabled on this server.\n\"");
		return qfalse;
	}
	if ( ent->health <= 0 ) {
		gi.SendServerCommand( ent-g_entities, "print \"You must be alive to use this command.\n\"");
		return qfalse;
	}
	return qtrue;
}


/*
==================
ConcatArgs
==================
*/
char	*ConcatArgs( int start ) {
	int		i, c, tlen;
	static char	line[MAX_STRING_CHARS];
	int		len;
	const char	*arg;

	len = 0;
	c = gi.argc();
	for ( i = start ; i < c ; i++ ) {
		arg = gi.argv( i );
		tlen = strlen( arg );
		if ( len + tlen >= MAX_STRING_CHARS - 1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

/*
==================
SanitizeString

Remove case and control characters
==================
*/
void SanitizeString( char *in, char *out ) {
	while ( *in ) {
		if ( *in == 94 ) {
			in += 2;		// skip color code
			continue;
		}
		if ( *in < 32 ) {
			in++;
			continue;
		}
		*out++ = tolower( *in++ );
	}

	*out = 0;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, char *s ) {
	gclient_t	*cl;
	int			idnum;
	char		s2[MAX_STRING_CHARS];
	char		n2[MAX_STRING_CHARS];

	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9') {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			gi.SendServerCommand( to-g_entities, "print \"Bad client slot: %i\n\"", idnum);
			return -1;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected != CON_CONNECTED ) {
			gi.SendServerCommand( to-g_entities, "print \"Client %i is not active\n\"", idnum);
			return -1;
		}
		return idnum;
	}

	// check for a name match
	SanitizeString( s, s2 );
	for ( idnum=0,cl=level.clients ; idnum < level.maxclients ; idnum++,cl++ ) {
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		SanitizeString( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) {
			return idnum;
		}
	}

	gi.SendServerCommand( to-g_entities, "print \"User %s is not on the server\n\"", s);
	return -1;
}

extern stringID_table_t attrTable[];
void G_Give( gentity_t *ent, const char *name, const char *args, int argc )
{
	gitem_t		*it;
	int			i;
	qboolean	give_all = qfalse;

	if ( !Q_stricmp( name, "all" ) )
		give_all = qtrue;

	if ( give_all || !Q_stricmp( name, "health") )
	{
		if (argc == 3)
		{
			ent->client->ps.stats[STAT_MAX_HEALTH] = atoi(args);

			ent->health = Com_Clampi(1, ent->client->ps.stats[STAT_MAX_HEALTH], atoi(args));
		}
		else
			ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
		if ( !give_all )
			return;
	}

	if ( give_all || !Q_stricmp( name, "armor" ) || !Q_stricmp( name, "shield" ) )
	{
		if (argc == 3)
		{
			ent->client->ps.stats[STAT_ARMOR] = Com_Clampi(0, ent->client->ps.stats[STAT_MAX_HEALTH], atoi(args));
		}
		else
			ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];

		if ( !give_all )
			return;
	}

	if ( give_all || !Q_stricmp( name, "force" ) )
	{
		if (argc == 3)
		{
			ent->client->ps.forcePowerMax = atoi(args);

			// Such a big number it turns negative
			if (ent->client->ps.forcePowerMax < 0 || ent->client->ps.forcePowerMax > 10000000)
			{
				ent->client->ps.forcePowerMax = 10000000;
			}

			ent->client->ps.forcePower = Com_Clampi(0, ent->client->ps.forcePowerMax, atoi(args));
		}
		else
			ent->client->ps.forcePower = ent->client->ps.forcePowerMax;

		if ( !give_all )
			return;
	}

	if ( give_all || !Q_stricmp( name, "weapons" ) )
	{
		for ( int i = 0; i < WP_MELEE; i++ )
		{
			ent->client->ps.weapons[i] = 1;
		}
		// Skip the unusable weapons, add in extra weapons.
		for (int i = WP_BATTLEDROID; i < weaponCount; i++)
		{
			if (i == WP_SBD || i == WP_DROIDEKA)
			{
				continue;
			}

			ent->client->ps.weapons[i] = 1;
		}
		if ( !give_all )
			return;
	}

	if ( !give_all && !Q_stricmp( name, "weaponnum" ) )
	{
		ent->client->ps.weapons[ atoi(args) ] = 1;
		return;
	}

	if ( !give_all && !Q_stricmp( name, "eweaps" ) )	//for developing, gives you all the weapons, including enemy
	{
		for ( int i = 0; i < weaponCount; i++ )
		{
			ent->client->ps.weapons[i] = 1;
		}
		return;
	}

	if ( give_all || !Q_stricmp( name, "ammo" ) )
	{
		int num = 999;
		if ( argc == 3 )
			num = Com_Clampi( -1, 999, atoi( args ) );
		for ( i=AMMO_BLASTER; i<ammoCount; i++ )
			ent->client->ps.ammo[i] = num != -1 ? num : ammoData[i].max;
		if ( !give_all )
			return;
	}

	if ( give_all || !Q_stricmp( name, "batteries" ) )
	{
		if ( argc == 3 )
			ent->client->ps.batteryCharge = Com_Clampi( 0, MAX_BATTERIES, atoi( args ) );
		else
			ent->client->ps.batteryCharge = MAX_BATTERIES;

		if (!give_all)
			return;
	}

	if (!Q_stricmp(name, "attribute"))
	{
		if (args)
		{
			int attr = GetIDForString(attrTable, args);
			if (attr >= 0)
			{
				if (!(ent->attrFlags & attr))
				{
					gi.SendServerCommand(ent - g_entities, va("print \"^2Applying Attribute: %s\n\"", args));
					ent->attrFlags |= attr;
				}
				else
				{
					gi.SendServerCommand(ent - g_entities, va("print \"^2Removing Attribute: %s\n\"", args));
					ent->attrFlags &= ~attr;
				}
			}
			else if (!Q_stricmp("clear", args) || !Q_stricmp("remove", args))
			{
				gi.SendServerCommand(ent - g_entities, "print \"^2Removing all attributes\n\"");
				ent->attrFlags = 0;
			}
			else
			{
				gi.SendServerCommand(ent - g_entities, va("print \"^1Unknown attribute: %s\n\"", args));
			}
			return;
		}
	}

	// spawn a specific item right on the player
	if ( !give_all ) {
		gentity_t	*it_ent;
		trace_t		trace;
		it = FindItem (args);
		if (!it) {
			it = FindItem (name);
			if (!it) {
				gi.SendServerCommand( ent-g_entities, "print \"unknown item\n\"");
				return;
			}
		}

		it_ent = G_Spawn();
		VectorCopy( ent->currentOrigin, it_ent->s.origin );
		it_ent->classname = G_NewString(it->classname);
		G_SpawnItem (it_ent, it);
		FinishSpawningItem(it_ent );
		memset( &trace, 0, sizeof( trace ) );
		Touch_Item (it_ent, ent, &trace);
		if (it_ent->inuse) {
			G_FreeEntity( it_ent );
		}
	}
}

void Cmd_Give_f( gentity_t *ent )
{
	if ( !CheatsOk( ent ) ) {
		return;
	}

	G_Give( ent, gi.argv(1), ConcatArgs( 2 ), gi.argc() );
}
/*
================================================================================
Cmd_CharacterStat_f

Description : 
	Provides detailed information about a specific character, specific variant or basic information for all characters.

Usage:
	characterStat [characterCode] [variantCode]


Behavior:
	- characterStat
		Print each Character Name in the log, no data about variants or stats nor structure;

	- characterStat [characterCode]
		Print the Character data and the list of variants for that character, no data about the variants;

	- characterStat [characterCode] [variantCode]
		Print the Character data and the Variant data for that specific variant of that character.
*/

void PrintCharacter(int characterIndex, int variantIndex)
{
	gi.Printf("^7CharacterData:\n{\n");
	gi.Printf("\t^3code:^5\"%s\"^7;\n", charactersData[characterIndex].code);
	gi.Printf("\t^3nameKey:^5\"%s\"^7;\n", charactersData[characterIndex].nameKey);
	gi.Printf("\t^3name:^5\"%s\"^7;\n", charactersData[characterIndex].name);
	gi.Printf("\t^3icon:^5\"%s\"^7;\n", charactersData[characterIndex].icon);
	gi.Printf("\t^3faction:^5\"%s\"^7;\n", charactersData[characterIndex].factions);
	gi.Printf("\t^3tags:^5\"%s\"^7;\n", charactersData[characterIndex].tags);
	if(variantIndex >= 0)
	{
		gi.Printf("^7\tSelectedVariant:\n\t{\n");
		gi.Printf("\t\t^3code:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].code);
		gi.Printf("\t\t^3name:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].name);
		gi.Printf("\t\t^3nameKey:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].nameKey);
		gi.Printf("\t\t^3icon:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].icon);
		gi.Printf("\t\t^3npcName:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].npcName);
		gi.Printf("\t\t^3model:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].model);
		gi.Printf("\t\t^3descriptionKey:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].descriptionKey);
		gi.Printf("\t\t^3weapons:^5\"%s\"^7;\n", "TODO SORRY");
		gi.Printf("\t\t^3forcePower:^5\"%s\"^7;\n", "TODO SORRY");

		for (int i = 0; (i < charactersData[characterIndex].variantList[variantIndex].presetCount) && !Q_IsStringEmpty(charactersData[characterIndex].variantList[variantIndex].presetList[i].skin); i++)
		{
			gi.Printf("\t\t^7Preset:\n{\n");
			gi.Printf("\t\t\t^3skin:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].presetList[i].skin);
			gi.Printf("\t\t\t^3icon:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].presetList[i].icon);
			gi.Printf("\t\t^7}\n");
		}
		for (int i = 0; (i < charactersData[characterIndex].variantList[variantIndex].headSkinCount) && !Q_IsStringEmpty(charactersData[characterIndex].variantList[variantIndex].headSkinList[i].skin); i++)
		{
			gi.Printf("\t\t^7Head:\n{\n");
			gi.Printf("\t\t\t^3skin:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].headSkinList[i].skin);
			gi.Printf("\t\t\t^3icon:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].headSkinList[i].icon);
			gi.Printf("\t\t^7}\n");
		}
		for (int i = 0; (i < charactersData[characterIndex].variantList[variantIndex].torsoSkinCount) && !Q_IsStringEmpty(charactersData[characterIndex].variantList[variantIndex].torsoSkinList[i].skin); i++)
		{
			gi.Printf("\t\t^7Torso:\n{\n");
			gi.Printf("\t\t\t^3skin:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].torsoSkinList[i].skin);
			gi.Printf("\t\t\t^3icon:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].torsoSkinList[i].icon);
			gi.Printf("\t\t^7}\n");
		}
		for (int i = 0; (i < charactersData[characterIndex].variantList[variantIndex].lowerSkinCount) && !Q_IsStringEmpty(charactersData[characterIndex].variantList[variantIndex].lowerSkinList[i].skin); i++)
		{
			gi.Printf("\t\t^7Lower:\n{\n");
			gi.Printf("\t\t\t^3skin:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].lowerSkinList[i].skin);
			gi.Printf("\t\t\t^3icon:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[variantIndex].lowerSkinList[i].icon);
			gi.Printf("\t\t^7}\n");
		}
		gi.Printf("\t^7}\n");
	}
	else {
		for (int i = 0; (i < charactersData[characterIndex].variantCount) && !Q_IsStringEmpty(charactersData[characterIndex].variantList[i].code); i++)
		{
			gi.Printf("\t^3Variant:^5\"%s\"^7;\n", charactersData[characterIndex].variantList[i].nameKey);

		}
	}
	gi.Printf("^7}\n");

}

void Cmd_CharacterStat_f(gentity_t* ent)
{
	if (!CheatsOk(ent))
		return;

	if (gi.argc() > 3)
	{
		gi.SendServerCommand(0, "print \"Usage: \n\tcharacterStat \n\tcharacterStat [characterName]\n\tcharacterStat [characterName] [variantName]\n\"");
		return;
	}
	//Case specific character
	if (gi.argc() == 3 || gi.argc() == 2)
	{
		for (int i = 0; (i < MAX_CHARACTERS) || charactersData[i].code == NULL; i++)
		{
			if (!Q_stricmp(charactersData[i].code, gi.argv(1)))
			{
				//No specific variant, print all variants
				if (gi.argc() == 2)
				{
					PrintCharacter(i, -1);
					return;
				}
				//Specific variant, search and print print that variant
				for (int j = 0; (j < charactersData[i].variantCount) || charactersData[i].variantList[j].code == NULL ; j++)
				{
					if (!Q_stricmp(charactersData[i].variantList[j].code, gi.argv(2)))
					{
						PrintCharacter(i, j);
						return;
					}
				}
				gi.SendServerCommand(0, va("print \"Variant %s not found for character %s\n\"", gi.argv(2), gi.argv(1)));
				return;
			}
		}
		gi.SendServerCommand(0, va("print \"Character %s not found\n\"", gi.argv(1)));
		return;
	}
	for (int i = 0; (i < MAX_CHARACTERS) && !Q_IsStringEmpty(charactersData[i].code); i++)
	{
		gi.Printf("%s,", charactersData[i].code);
	}
}
/*
================================================================================
Cmd_WeaponStat_f

Description:
	Provides detailed information about a specific weapon and its attacks.

Usage:
	weaponStat [weapon_class] [attackIndex]

Behavior:
	- weaponStat 
		For currently equiped weapon : Displays the properties of weapon and all associated attackData
		entries where firingLogic != FL_NONE (light overview of each attack).

	- weaponStat <attackIndex>
		For current weapon : Displays only the properties of the specified attack (1-based index)
		along with a light overview of the general weapon properties.

	- weaponStat <weapon_class>
		Displays the properties of the weapon and all associated attackData
		entries where firingLogic != FL_NONE (light overview of each attack).

	- weaponStat <weapon_class> <attackIndex>
		Displays only the properties of the specified attack (1-based index)
		along with a light overview of the general weapon properties.

Parameters:
	[weapon_class] : Optional, The classname of the weapon to inspect (e.g., weapon_blaster)
	[attackIndex]  : Optional, 1-based index of the attackData to display

Notes:
	- Requires cheats to be enabled (CheatsOk(ent) must return true).
	- Output is sent to the server console via gi.SendServerCommand / gi.Printf.
================================================================================
*/
extern int WP_GetWeaponID(const char* weaponName);
const char* getStringValueForvec3(const vec3_t* v);
static const char* IntArray3ToString(const int v[3]);
static const char* FloatArray3ToString(const float v[3]);
const char* getStringValueForqboolean(qboolean value);
const char* getStringValueForfiringLogic(const firingLogic_t* logic);
const char* getStringValueForblockability(const blockability_t* b);
const char* getStringValueForMethodOfDeath(const meansOfDeath_t* methodOfDeath);
const char* getStringValueForweaponCategory(const weaponCategory_t* wc);
const char* getStringValueForweaponBucket(const weaponBucket_t* wb);

void PrintWeaponAttackData(const weaponAttackData_t* attack)
{
	if (!attack)
	{
		gi.Printf("\t\t(null)\n");
		return;
	}

	gi.Printf("\t{\n");

	gi.Printf("\t\t^6'firingLogic'^7 : ^5%s^7 (firingLogic_t)\n",
		getStringValueForfiringLogic(&attack->firingLogic));

	gi.Printf("\t\t^6'energyPerShot'^7 : ^5%d^7 (int)\n", attack->energyPerShot);
	gi.Printf("\t\t^6'fireTime'^7 : ^5%d^7 (int)\n", attack->fireTime);
	gi.Printf("\t\t^6'effectDuration'^7 : ^5%d^7 (int)\n", attack->effectDuration);
	gi.Printf("\t\t^6'range'^7 : ^5%d^7 (int)\n", attack->range);
	gi.Printf("\t\t^6'spread'^7 : ^5%f^7 (float)\n", attack->spread);

	gi.Printf("\t\t^6'npcSpread'^7 : ^5%f, %f, %f^7 (float[3])\n",
		attack->npcSpread[0], attack->npcSpread[1], attack->npcSpread[2]);

	gi.Printf("\t\t^6'damage'^7 : ^5%d^7 (int)\n", attack->damage);
	gi.Printf("\t\t^6'npcDamage'^7 : ^5%d, %d, %d^7 (int[3])\n",
		attack->npcDamage[0], attack->npcDamage[1], attack->npcDamage[2]);

	gi.Printf("\t\t^6'defaultDamage'^7 : ^5%d^7 (int)\n", attack->defaultDamage);
	gi.Printf("\t\t^6'velocity'^7 : ^5%f^7 (float)\n", attack->velocity);

	gi.Printf("\t\t^6'npcVelocity'^7 : ^5%f, %f, %f^7 (float[3])\n",
		attack->npcVelocity[0], attack->npcVelocity[1], attack->npcVelocity[2]);

	gi.Printf("\t\t^6'fireOption'^7 : ^5%d, %d, %d^7 (int[3])\n",
		attack->fireOption[0], attack->fireOption[1], attack->fireOption[2]);

	gi.Printf("\t\t^6'methodOfDeath'^7 : ^5\"%s\"^7 (methodOfDeath_t)\n",
		getStringValueForMethodOfDeath(&attack->methodOfDeath));

	gi.Printf("\t\t^6'splashMethodOfDeath'^7 : ^5\"%s\"^7 (methodOfDeath_t)\n",
		getStringValueForMethodOfDeath(&attack->splashMethodOfDeath));

	gi.Printf("\t\t^6'splashDamage'^7 : ^5%d^7 (int)\n", attack->splashDamage);
	gi.Printf("\t\t^6'splashRadius'^7 : ^5%f^7 (float)\n", attack->splashRadius);

	gi.Printf("\t\t^6'projectileEffect'^7 : ^5\"%s\"^7 (char[64])\n", attack->projectileEffect);
	gi.Printf("\t\t^6'hitWallEffect'^7 : ^5\"%s\"^7 (char[64])\n", attack->hitWallEffect);
	gi.Printf("\t\t^6'hitWallEffect2'^7 : ^5\"%s\"^7 (char[64])\n", attack->hitWallEffect2);
	gi.Printf("\t\t^6'hitWallEffect3'^7 : ^5\"%s\"^7 (char[64])\n", attack->hitWallEffect3);
	gi.Printf("\t\t^6'hitDroidEffect'^7 : ^5\"%s\"^7 (char[64])\n", attack->hitDroidEffect);
	gi.Printf("\t\t^6'hitFleshEffect'^7 : ^5\"%s\"^7 (char[64])\n", attack->hitFleshEffect);
	gi.Printf("\t\t^6'muzzleEffect'^7 : ^5\"%s\"^7 (char[64])\n", attack->muzzleEffect);
	gi.Printf("\t\t^6'explosionEffect'^7 : ^5\"%s\"^7 (char[64])\n", attack->explosionEffect);
	gi.Printf("\t\t^6'shockwaveEffect'^7 : ^5\"%s\"^7 (char[64])\n", attack->shockwaveEffect);

	gi.Printf("\t\t^6'blockability'^7 : ^5%s, %s, %s^7 (blockability_t[3])\n",
		getStringValueForblockability(&attack->blockability[0]),
		getStringValueForblockability(&attack->blockability[1]),
		getStringValueForblockability(&attack->blockability[2]));

	gi.Printf("\t\t^6'beamShader'^7 : ^5\"%s\"^7 (char[64])\n", attack->beamShader);
	gi.Printf("\t\t^6'beamColor'^7 : ^5%s^7 (vec3_t)\n", getStringValueForvec3(&attack->beamColor));
	gi.Printf("\t\t^6'fullBeamShader'^7 : ^5\"%s\"^7 (char[64])\n", attack->fullBeamShader);
	gi.Printf("\t\t^6'fullBeamColor'^7 : ^5%s^7 (vec3_t)\n", getStringValueForvec3(&attack->fullBeamColor));

	gi.Printf("\t\t^6'bounceWall'^7 : ^5%s^7 (qboolean)\n", getStringValueForqboolean(attack->bounceWall));
	gi.Printf("\t\t^6'bounceCount'^7 : ^5%d^7 (int)\n", attack->bounceCount);

	gi.Printf("\t\t^6'chargeUnitTime'^7 : ^5%f^7 (float)\n", attack->chargeUnitTime);
	gi.Printf("\t\t^6'maxChargeUnits'^7 : ^5%d^7 (int)\n", attack->maxChargeUnits);
	gi.Printf("\t\t^6'chargeSnd'^7 : ^5\"%s\"^7 (char[64])\n", attack->chargeSnd);
	gi.Printf("\t\t^6'chargeMuzzleShader'^7 : ^5\"%s\"^7 (char[64])\n", attack->chargeMuzzleShader);
	gi.Printf("\t\t^6'chargeMuzzleScale'^7 : ^5%f^7 (float)\n", attack->chargeMuzzleScale);

	gi.Printf("\t\t^6'missileMdl'^7 : ^5\"%s\"^7 (char[64])\n", attack->missileMdl);
	gi.Printf("\t\t^6'missileSound'^7 : ^5\"%s\"^7 (char[64])\n", attack->missileSound);
	gi.Printf("\t\t^6'missileDlight'^7 : ^5%f^7 (float)\n", attack->missileDlight);
	gi.Printf("\t\t^6'missileSize'^7 : ^5%d^7 (int)\n", attack->missileSize);
	gi.Printf("\t\t^6'missileMass'^7 : ^5%d^7 (int)\n", attack->missileMass);
	gi.Printf("\t\t^6'missileDFlags'^7 : ^5%d^7 (int)\n", attack->missileDFlags);
	gi.Printf("\t\t^6'missileDlightColor'^7 : ^5%s^7 (vec3_t)\n", getStringValueForvec3(&attack->missileDlightColor));

	gi.Printf("\t\t^6'firingSnd'^7 : ^5\"%s\"^7 (char[64])\n", attack->firingSnd);
	gi.Printf("\t\t^6'startSound'^7 : ^5\"%s\"^7 (char[64])\n", attack->startSnd);
	gi.Printf("\t\t^6'missileHitSound'^7 : ^5\"%s\"^7 (char[64])\n", attack->missileHitSound);

	gi.Printf("\t\t^6'dempDetonateShader'^7 : ^5\"%s\"^7 (char[64])\n", attack->dempDetonateShader);
	gi.Printf("\t\t^6'dempDetonateModel'^7 : ^5\"%s\"^7 (char[64])\n", attack->dempDetonateModel);

	gi.Printf("\t}\n");
}


void PrintWeaponAttackData_Light(const weaponAttackData_t* attack)
{
	if (!attack)
	{
		gi.Printf("\t(null)\n");
		return;
	}

	gi.Printf("\t{\n");


	gi.Printf("\t\t^6'firingLogic'^7 : ^5%s^7 (firingLogic_t)\n",
		getStringValueForfiringLogic(&attack->firingLogic));

	gi.Printf("\t\t^6'energyPerShot'^7 : ^5%d^7 (int)\n", attack->energyPerShot);
	gi.Printf("\t\t^6'fireTime'^7 : ^5%d^7 (int)\n", attack->fireTime);
	gi.Printf("\t\t^6'effectDuration'^7 : ^5%d^7 (int)\n", attack->effectDuration);
	gi.Printf("\t\t^6'range'^7 : ^5%d^7 (int)\n", attack->range);
	gi.Printf("\t\t^6'spread'^7 : ^5%f^7 (float)\n", attack->spread);
	gi.Printf("\t\t^6'damage'^7 : ^5%d^7 (int)\n", attack->damage);
	gi.Printf("\t\t^6'velocity'^7 : ^5%f^7 (float)\n", attack->velocity);
	gi.Printf("\t\t^6'fireOption'^7 : ^5%d, %d, %d^7 (int[3])\n",
		attack->fireOption[0], attack->fireOption[1], attack->fireOption[2]);
	gi.Printf("\t\t^6'splashDamage'^7 : ^5%d^7 (int)\n", attack->splashDamage);
	gi.Printf("\t\t^6'splashRadius'^7 : ^5%f^7 (float)\n", attack->splashRadius);

	gi.Printf("\t}\n");
}
void PrintWeaponData(const weaponData_t* weapon)
{
	int i;

	if (!weapon)
	{
		gi.Printf("(null)\n");
		return;
	}

	for (i = 0; i < MAX_WEAPON_ATTACKS; i++)
	{
		if (weapon->attackData[i].firingLogic == FL_NONE)
		{
			continue;
		}
		gi.Printf("\t^6'attackData[%d]'^7 : ", i);
		PrintWeaponAttackData_Light(&weapon->attackData[i]);
	}

	gi.Printf("\t^6'classname'^7 : ^5\"%s\"^7 (char[32])\n", weapon->classname);
	gi.Printf("\t^6'baseclass'^7 : ^5\"%s\"^7 (char[32])\n", weapon->baseclass);
	gi.Printf("\t^6'baseWeaponNum'^7 : ^5%d^7 (int)\n", weapon->baseWeaponNum);

	gi.Printf("\t^6'weaponMdl'^7 : ^5\"%s\"^7 (char[64])\n", weapon->weaponMdl);
	gi.Printf("\t^6'stopSnd'^7 : ^5\"%s\"^7 (char[64])\n", weapon->stopSnd);
	gi.Printf("\t^6'selectSnd'^7 : ^5\"%s\"^7 (char[64])\n", weapon->selectSnd);
	gi.Printf("\t^6'readySnd'^7 : ^5\"%s\"^7 (char[64])\n", weapon->readySnd);

	gi.Printf("\t^6'ammoIndex'^7 : ^5%d^7 (int)\n", weapon->ammoIndex);
	gi.Printf("\t^6'ammoLow'^7 : ^5%d^7 (int)\n", weapon->ammoLow);

	gi.Printf("\t^6'weaponIcon'^7 : ^5\"%s\"^7 (char[64])\n", weapon->weaponIcon);
	gi.Printf("\t^6'numBarrels'^7 : ^5%d^7 (int)\n", weapon->numBarrels);

	gi.Printf("\t^6'scopeType'^7 : ^5%d^7 (int)\n", weapon->scopeType);
	gi.Printf("\t^6'scopeFov'^7 : ^5%f^7 (float)\n", weapon->scopeFov);

	gi.Printf("\t^6'weaponMdl2'^7 : ^5\"%s\"^7 (char[64])\n", weapon->weaponMdl2);
	gi.Printf("\t^6'secondaryMdl'^7 : ^5%s^7 (qboolean)\n", getStringValueForqboolean(weapon->secondaryMdl));
	gi.Printf("\t^6'playerUsable'^7 : ^5%s^7 (qboolean)\n", getStringValueForqboolean(weapon->playerUsable));

	gi.Printf("\t^6'weaponCategory'^7 : ^5%s^7 (weaponCategory_t)\n", getStringValueForweaponCategory(&weapon->weaponCategory));
	gi.Printf("\t^6'weaponBucket'^7 : ^5%s^7 (weaponBucket_t)\n", getStringValueForweaponBucket(&weapon->weaponBucket));
}

void Cmd_WeaponStat_f(gentity_t* ent)
{
	if (!CheatsOk(ent))
		return;

	if (gi.argc() > 3)
	{
		gi.SendServerCommand(0, "print \"Usage: \n\tweaponStat \n\tweaponStat [weapon_class]\n\tweaponStat [attackIndex]\nUsage: weaponStat [weapon_class] [attackIndex]\n\"");
		return;
	}

	int weaponNum = player->client->ps.weapon;
	int attackIndex = -1;
	if (gi.argc() == 2)
	{
		attackIndex = atoi(gi.argv(1)) - 1;
		if (attackIndex < 0 && attackIndex >= MAX_WEAPON_ATTACKS)
		{
			attackIndex = -1;
			weaponNum = WP_GetWeaponID(gi.argv(1));
			if (weaponNum < 0 || weaponNum >= weaponCount)
			{
				gi.SendServerCommand(0, "print \"Invalid weapon class : %s\nUsage: \n\tweaponStat \n\tweaponStat [weapon_class]\n\tweaponStat [attackIndex]\nUsage: weaponStat [weapon_class] [attackIndex]\n\"", gi.argv(1));
				return;
			}
		}
	}
	weaponData_t* weapon = &weaponData[weaponNum];

	if (gi.argc() == 3)
	{
		attackIndex = atoi(gi.argv(2)) - 1;
		if (attackIndex < 0 || attackIndex >= MAX_WEAPON_ATTACKS)
		{
			gi.SendServerCommand(0, "print \"Invalid attack index : %s ; Should be 1,2,3 or 4.\nUsage: \n\tweaponStat \n\tweaponStat [weapon_class]\n\tweaponStat [attackIndex]\nUsage: weaponStat [weapon_class] [attackIndex]\n\"", gi.argv(2));
			return;
		}
	}
	gi.Printf("Data for ^5\"%s\"^7:\n{\n", weapon->classname);

	if (attackIndex == -1)
	{
		PrintWeaponData(weapon);
	}
	else
	{
		gi.Printf("\t^6'attackData[%d]'^7 : ", attackIndex);
		PrintWeaponAttackData(&weapon->attackData[attackIndex]);

		gi.Printf("\t^6'classname'^7 : ^5\"%s\"^7 (char[32])\n", weapon->classname);
		gi.Printf("\t^6'baseclass'^7 : ^5\"%s\"^7 (char[32])\n", weapon->baseclass);
	}

	gi.Printf("}\n");
}


void Cmd_ForceRegen_f( gentity_t* ent )
{
	if (!CheatsOk(ent))
	{
		return;
	}
	int amt = atoi(gi.argv(1));

	if(!amt)
	{
		gi.SendServerCommand(0, "print \"Usage: setForceRegen <number> \nDefault value is 100\nThe lower the value, the faster your force will regenerate\n\"");
		return;
	}

	if (amt > 0)
	{
		player->client->ps.forcePowerRegenRate = amt;
	}
	else
	{
		gi.SendServerCommand(0, "print \"Invalid number, please input a number larger than 0\n\"");
	}
	return;
}
//------------------
void Cmd_Fx( gentity_t *ent )
{
	vec3_t		dir;
	gentity_t	*fx_ent = NULL;

	if ( Q_stricmp( gi.argv(1), "play" ) == 0 )
	{
		if ( gi.argc() == 3 )
		{
			// I guess, only allow one active at a time
			while (( fx_ent = G_Find( fx_ent, FOFS(classname), "cmd_fx")) != NULL )
			{
				G_FreeEntity( fx_ent );
			}

			fx_ent = G_Spawn();

			fx_ent->fxFile = gi.argv( 2 );

			// Move out in front of the person spawning the effect
			AngleVectors( ent->currentAngles, dir, NULL, NULL );
			VectorMA( ent->currentOrigin, 32, dir, fx_ent->s.origin );

extern void SP_fx_runner( gentity_t *ent );

			SP_fx_runner( fx_ent );
			fx_ent->delay = 2000;			// adjusting delay
			fx_ent->classname = "cmd_fx";	//	and classname

			return;
		}
	}
	else if ( Q_stricmp( gi.argv(1), "stop" ) == 0 )
	{
		while (( fx_ent = G_Find( fx_ent, FOFS(classname), "cmd_fx")) != NULL )
		{
			G_FreeEntity( fx_ent );
		}

		return;
	}
	else if ( Q_stricmp( gi.argv(1), "delay" ) == 0 )
	{
		while (( fx_ent = G_Find( fx_ent, FOFS(classname), "cmd_fx")) != NULL )
		{
			if ( gi.argc() == 3 )
			{
				fx_ent->delay = atoi( gi.argv( 2 ));
			}
			else
			{
				gi.Printf( S_COLOR_GREEN"FX: current delay is: %i\n", fx_ent->delay );
			}

			return;
		}
	}
	else if ( Q_stricmp( gi.argv(1), "random" ) == 0 )
	{
		while (( fx_ent = G_Find( fx_ent, FOFS(classname), "cmd_fx")) != NULL )
		{
			if ( gi.argc() == 3 )
			{
				fx_ent->random = atoi( gi.argv( 2 ));
			}
			else
			{
				gi.Printf( S_COLOR_GREEN"FX: current random is: %6.2f\n", fx_ent->random );
			}

			return;
		}
	}
	else if ( Q_stricmp( gi.argv(1), "origin" ) == 0 )
	{
		while (( fx_ent = G_Find( fx_ent, FOFS(classname), "cmd_fx")) != NULL )
		{
			if ( gi.argc() == 5 )
			{
				fx_ent->s.origin[0] = atof( gi.argv( 2 ));
				fx_ent->s.origin[1] = atof( gi.argv( 3 ));
				fx_ent->s.origin[2] = atof( gi.argv( 4 ));

				G_SetOrigin( fx_ent, fx_ent->s.origin );
			}
			else
			{
				gi.Printf( S_COLOR_GREEN"FX: current origin is: <%6.2f %6.2f %6.2f>\n",
									fx_ent->currentOrigin[0], fx_ent->currentOrigin[1], fx_ent->currentOrigin[2] );
			}

			return;
		}
	}
	else if ( Q_stricmp( gi.argv(1), "dir" ) == 0 )
	{
		while (( fx_ent = G_Find( fx_ent, FOFS(classname), "cmd_fx")) != NULL )
		{
			if ( gi.argc() == 5 )
			{
				fx_ent->s.angles[0] = atof( gi.argv( 2 ));
				fx_ent->s.angles[1] = atof( gi.argv( 3 ));
				fx_ent->s.angles[2] = atof( gi.argv( 4 ));

				if ( !VectorNormalize( fx_ent->s.angles ))
				{
					// must have been zero length
					fx_ent->s.angles[2] = 1;
				}
			}
			else
			{
				gi.Printf( S_COLOR_GREEN"FX: current dir is: <%6.2f %6.2f %6.2f>\n",
									fx_ent->s.angles[0], fx_ent->s.angles[1], fx_ent->s.angles[2] );
			}

			return;
		}
	}

	gi.Printf( S_COLOR_CYAN"Fx--------------------------------------------------------\n" );
	gi.Printf( S_COLOR_CYAN"commands:              sample usage:\n" );
	gi.Printf( S_COLOR_CYAN"----------------------------------------------------------\n" );
	gi.Printf( S_COLOR_CYAN"fx play <filename>     fx play sparks, fx play env/fire\n" );
	gi.Printf( S_COLOR_CYAN"fx stop                fx stop\n" );
	gi.Printf( S_COLOR_CYAN"fx delay <#>           fx delay 1000\n" );
	gi.Printf( S_COLOR_CYAN"fx random <#>          fx random 200\n" );
	gi.Printf( S_COLOR_CYAN"fx origin <#><#><#>    fx origin 10 20 30\n" );
	gi.Printf( S_COLOR_CYAN"fx dir <#><#><#>       fx dir 0 0 -1\n\n" );
}
/*
==================
	Cmd_Update_CheatStatsHealth
	Cmd_Update_CheatStatsShield
	Cmd_Update_CheatStatsForce
	Update Client statistics from cheat cvar
==================
*/
void Cmd_Update_CheatStatsHealth(gentity_t* ent)
{
	int health = Com_Clampi(1,999,gi.Cvar_VariableIntegerValue("ui_cheats_health"));
	ent->client->ps.stats[STAT_MAX_HEALTH] = health;
	ent->health = health;
}

void Cmd_Update_CheatStatsShield(gentity_t* ent)
{
	int shield = gi.Cvar_VariableIntegerValue("ui_cheats_shield");
	ent->client->ps.stats[STAT_ARMOR] = Com_Clampi(0, ent->client->ps.stats[STAT_MAX_HEALTH], shield);
}

// Sets the ammo of the type ui_cheats_ammoType (an ammoData index) to ui_cheats_ammo.
void Cmd_Update_CheatStatsAmmo(gentity_t* ent)
{
	const int ammoType = gi.Cvar_VariableIntegerValue("ui_cheats_ammoType");
	if (ammoType <= AMMO_NONE || ammoType >= ammoCount)
	{
		return;
	}
	ent->client->ps.ammo[ammoType] = Com_Clampi(0, 999, gi.Cvar_VariableIntegerValue("ui_cheats_ammo"));
}

void Cmd_Update_CheatStatsForce(gentity_t* ent)
{
	int force = Com_Clampi(0, 999, gi.Cvar_VariableIntegerValue("ui_cheats_force"));
	ent->client->ps.forcePowerMax = force;
	ent->client->ps.forcePower = force;
}

void Cmd_Update_CheatStatsForceRegenRate(gentity_t* ent)
{
	int regenRate = Com_Clampi(1, 5000, gi.Cvar_VariableIntegerValue("ui_cheats_force_regen_rate"));
	ent->client->ps.forcePowerRegenRate= regenRate;
}

void Cmd_Update_CheatStatsForceRegenAmount(gentity_t* ent)
{
	int regenAmount = Com_Clampi(1, 999, gi.Cvar_VariableIntegerValue("ui_cheats_force_regen_amount"));
	ent->client->ps.forcePowerRegenAmount = regenAmount;
}

/*
==================
Cmd_Update_CF

Updates client Flags
==================
*/
void Cmd_Update_CF(gentity_t* ent)
{
	if (gi.Cvar_VariableIntegerValue("ui_cheats_godMode"))
	{
		ent->flags |= FL_GODMODE;
	}
	else {
		ent->flags &= ~FL_GODMODE;
	}
	if (gi.Cvar_VariableIntegerValue("ui_cheats_noForce"))
	{
		ent->flags |= FL_NOFORCE;
	}
	else {
		ent->flags &= ~FL_NOFORCE;
	}
	if (gi.Cvar_VariableIntegerValue("ui_cheats_undying"))
	{
		ent->flags |= FL_UNDYING;
	}
	else {
		ent->flags &= ~FL_UNDYING;
	}
	if (gi.Cvar_VariableIntegerValue("ui_cheats_noTarget"))
	{
		ent->flags |= FL_NOTARGET;
	}
	else {
		ent->flags &= ~FL_NOTARGET;
	}
	if (gi.Cvar_VariableIntegerValue("ui_cheats_noClip"))
	{
		ent->client->noclip = 1;
	}
	else {
		ent->client->noclip = 0;;
	}
}
/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (gentity_t *ent)
{
	const char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE))
	{
		msg = "godmode OFF\n";
		gi.cvar_set("ui_cheats_godMode", "0");
	}
	else
	{
		msg = "godmode ON\n";
		gi.cvar_set("ui_cheats_godMode", "1");
	}

	gi.SendServerCommand( ent-g_entities, "print \"%s\"", msg);
}

/*
==================
Cmd_Noforce_f

Sets client to be immune to the force

argv(0) noforce
==================
*/
void Cmd_Noforce_f(gentity_t *ent)
{
	const char	*msg;

	if (!CheatsOk(ent)) {
		return;
	}

	ent->flags ^= FL_NOFORCE;
	if (!(ent->flags & FL_NOFORCE))
	{
		msg = "No Force OFF\n";
		gi.cvar_set("ui_cheats_noForce", "0");
	}
	else
	{
		msg = "No Force ON\n";
		gi.cvar_set("ui_cheats_noForce", "1");
	}

	gi.SendServerCommand(ent - g_entities, "print \"%s\"", msg);
}


/*
==================
Cmd_Undying_f

Sets client to undead mode

argv(0) undying
==================
*/
void Cmd_Undying_f (gentity_t *ent)
{
	const char	*msg;

	if ( !CheatsOk( ent ) )
	{
		return;
	}

	ent->flags ^= FL_UNDYING;
	if (!(ent->flags & FL_UNDYING) )
	{
		msg = "undead mode OFF\n";
		gi.cvar_set("ui_cheats_undying", "0");
	}
	else
	{
		int		max;
		const char	*cmd;

		cmd = gi.argv(1);
		if ( cmd && atoi( cmd ) )
		{
			max = atoi( cmd );
		}
		else
		{
			max = 999;
		}

		ent->health = ent->max_health = max;

		msg = "undead mode ON\n";
		gi.cvar_set("ui_cheats_undying", "1");

		if ( ent->client )
		{
			ent->client->ps.stats[STAT_HEALTH] = ent->client->ps.stats[STAT_MAX_HEALTH] = 999;
		}
	}

	gi.SendServerCommand( ent-g_entities, "print \"%s\"", msg);
}

/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent ) {
	const char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET))
	{
		msg = "notarget OFF\n";
		gi.cvar_set("ui_cheats_noTarget", "0");
	}
	else
	{
		msg = "notarget ON\n";
		gi.cvar_set("ui_cheats_noTarget", "1");
	}

	gi.SendServerCommand( ent-g_entities, "print \"%s\"", msg);
}



/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent ) {
	const char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	if ( ent->client->noclip ) 
	{
		msg = "noclip OFF\n";
		gi.cvar_set("ui_cheats_noClip", "0");
	} else 
	{
		msg = "noclip ON\n";
		gi.cvar_set("ui_cheats_noClip", "1");
	}
	ent->client->noclip = !ent->client->noclip;
	ent->flags ^= FL_NOFORCE;

	gi.SendServerCommand( ent-g_entities, "print \"%s\"", msg);
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent ) {
	if ( !CheatsOk( ent ) ) {
		return;
	}

	gi.SendServerCommand( ent-g_entities, "clientLevelShot" );
}


/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent ) {
	if( ( level.time - ent->client->respawnTime ) < 5000 ) {
		gi.SendServerCommand( ent-g_entities, "cp @SP_INGAME_ONE_KILL_PER_5_SECONDS");
		return;
	}
	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
	player_die (ent, ent, ent, 100000, MOD_SUICIDE);
}


/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent ) {
	const char *s = gi.argv(1);
	const int len = strlen(s);
	gentity_t	*check;

	if ( gi.argc () < 2 ) {
		gi.Printf("usage: where classname\n");
		return;
	}
	for (int i = 0; i < globals.num_entities; i++)
	{
		if(!PInUse(i))
			continue;
//		if(!check || !check->inuse) {
//			continue;
//		}
		check = &g_entities[i];
		if (!Q_stricmpn(s, check->classname, len) ) {
			gi.SendServerCommand( ent-g_entities, "print \"%s %s\n\"", check->classname, vtos( check->s.pos.trBase ) );
		}
	}
}


/*
-------------------------
UserSpawn
-------------------------
*/

extern qboolean G_CallSpawn( gentity_t *ent );

void UserSpawn( gentity_t *ent, const char *name )
{
	vec3_t		origin;
	vec3_t		vf;
	vec3_t		angles;
	gentity_t	*ent2;

	//Spawn the ent
	ent2 = G_Spawn();
	ent2->classname = G_NewString( name );

	//TODO: This should ultimately make sure this is a safe spawn!

	//Spawn the entity and place it there
	VectorSet( angles, 0, ent->s.apos.trBase[YAW], 0 );
	AngleVectors( angles, vf, NULL, NULL );
	VectorMA( ent->s.pos.trBase, 96, vf, origin );	//FIXME: Find the radius size of the object, and push out 32 + radius

	origin[2] += 8;
	VectorCopy( origin, ent2->s.pos.trBase );
	VectorCopy( origin, ent2->s.origin );
	VectorCopy( ent->s.apos.trBase, ent2->s.angles );

	gi.linkentity( ent2 );

	//Find a valid spawning spot
	if ( G_CallSpawn( ent2 ) == qfalse )
	{
		gi.SendServerCommand( ent-g_entities, "print \"Failed to spawn '%s'\n\"", name );
		G_FreeEntity( ent2 );
		return;
	}
}

/*
-------------------------
Cmd_Spawn
-------------------------
*/

void Cmd_Spawn( gentity_t *ent )
{
	char	*name;

	name = ConcatArgs( 1 );

	gi.SendServerCommand( ent-g_entities, "print \"Spawning '%s'\n\"", name );

	UserSpawn( ent, name );
}

/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent ) {
	vec3_t		origin, angles;
	int			i;

	if ( !g_cheats->integer ) {
		gi.SendServerCommand( ent-g_entities, va("print \"Cheats are not enabled on this server.\n\""));
		return;
	}
	if ( gi.argc() != 5 ) {
		gi.SendServerCommand( ent-g_entities, va("print \"usage: setviewpos x y z yaw\n\""));
		return;
	}

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		origin[i] = atof( gi.argv( i+1 ) );
	}
	origin[2] -= 25;	//acount for eye height from viewpos cmd

	angles[YAW] = atof( gi.argv( 4 ) );

	TeleportPlayer( ent, origin, angles );
}



/*
=================
Cmd_SetObjective_f
=================
*/
qboolean G_CheckPlayerDarkSide( void );

void Cmd_SetObjective_f( gentity_t *ent )
{
	int objectiveI,status,displayStatus;

	if ( gi.argc() == 2 ) {
		objectiveI = atoi(gi.argv(1));
		gi.Printf("objective #%d  display status=%d, status=%d\n",objectiveI,
			ent->client->sess.mission_objectives[objectiveI].display,
			ent->client->sess.mission_objectives[objectiveI].status
			);
		return;
	}
	if ( gi.argc() != 4 ) {
		gi.SendServerCommand( ent-g_entities, va("print \"usage: setobjective <objective #>  <display status> <status>\n\""));
		return;
	}

	if ( !CheatsOk( ent ) )
	{
		return;
	}

	objectiveI = atoi(gi.argv(1));
	displayStatus = atoi(gi.argv(2));
	status = atoi(gi.argv(3));

	ent->client->sess.mission_objectives[objectiveI].display = (qboolean)(displayStatus != 0);
	ent->client->sess.mission_objectives[objectiveI].status = status;
	G_CheckPlayerDarkSide();
}

/*
=================
Cmd_ViewObjective_f
=================
*/
void Cmd_ViewObjective_f( gentity_t *ent )
{
	int objectiveI;

	if ( gi.argc() != 2 ) {
		gi.SendServerCommand( ent-g_entities, va("print \"usage: viewobjective <objective #>\n\""));
		return;
	}

	objectiveI = atoi(gi.argv(1));

	gi.SendServerCommand( ent-g_entities, va("print \"Objective %d   Display Status(1=show): %d  Status:%d\n\"",objectiveI,ent->client->sess.mission_objectives[objectiveI].display,ent->client->sess.mission_objectives[objectiveI].status));
}


/*
================
Cmd_UseElectrobinoculars_f
================
*/
void Cmd_UseElectrobinoculars_f(gentity_t *ent)
{
	if ( ent->health < 1 || in_camera )
	{
		return;
	}

	if ( ent->client->ps.inventory[INV_ELECTROBINOCULARS] <= 0 )
	{
		// have none to place...play sound?
		return;
	}

	G_AddEvent( ent, EV_USE_INV_BINOCULARS, 0 );
}

/*
================
Cmd_UseBacta_f
================
*/
void Cmd_UseBacta_f(gentity_t *ent)
{
	if ( ent->health < 1 || in_camera )
	{
		return;
	}

	ItemUse_Bacta(ent);
}

//----------------------------------------------------------------------------------
qboolean PickSeekerSpawnPoint( vec3_t org, vec3_t fwd, vec3_t right, int skip, vec3_t spot )
{
	vec3_t	mins, maxs, forward, end;
	trace_t tr;

	VectorSet( maxs, -8, -8, -24); // ?? size
	VectorSet( maxs, 8, 8, 8 );

	VectorCopy( fwd, forward );

	// to the front and side a bit
	forward[2] = 0.3f; // start up a bit

	VectorMA( org, 48, forward, end );
	VectorMA( end, -8, right, end );

	gi.trace( &tr, org, mins, maxs, end, skip, MASK_PLAYERSOLID, (EG2_Collision)0, 0 );

	if ( !tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f )
	{
		VectorCopy( tr.endpos, spot );
		return qtrue;
	}

	// side
	VectorMA( org, 48, right, end );

	gi.trace( &tr, org, mins, maxs, end, skip, MASK_PLAYERSOLID, (EG2_Collision)0, 0 );

	if ( !tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f )
	{
		VectorCopy( tr.endpos, spot );
		return qtrue;
	}

	// other side
	VectorMA( org, -48, right, end );

	gi.trace( &tr, org, mins, maxs, end, skip, MASK_PLAYERSOLID, (EG2_Collision)0, 0 );

	if ( !tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f )
	{
		VectorCopy( tr.endpos, spot );
		return qtrue;
	}

	// behind
	VectorMA( org, -48, fwd, end );

	gi.trace( &tr, org, mins, maxs, end, skip, MASK_PLAYERSOLID, (EG2_Collision)0, 0 );

	if ( !tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f )
	{
		VectorCopy( tr.endpos, spot );
		return qtrue;
	}

	return qfalse;
}

/*
================
Cmd_UseSeeker_f
================
*/
void Cmd_UseSeeker_f( gentity_t *ent )
{
	if ( ent->health < 1 || in_camera )
	{
		return;
	}

	// don't use them if we don't have any...also don't use them if one is already going
	if ( ent->client && ent->client->ps.inventory[INV_SEEKER] > 0 && level.time > ent->client->ps.powerups[PW_SEEKER] )
	{
		gentity_t	*tent = G_Spawn();

		if ( tent )
		{
			vec3_t	fwd, right, spot;

			AngleVectors( ent->client->ps.viewangles, fwd, right, NULL );

			VectorCopy( ent->currentOrigin, spot ); // does nothing really, just initialize the goods...

			if ( PickSeekerSpawnPoint( ent->currentOrigin, fwd, right, ent->s.number, spot ))
			{
				VectorCopy( spot, tent->s.origin );
				G_SetOrigin( tent, spot );
				G_SetAngles( tent, ent->currentAngles );

extern void SP_NPC_Droid_Seeker( gentity_t *ent );

				SP_NPC_Droid_Seeker( tent );
				G_Sound( tent, G_SoundIndex( "sound/chars/seeker/misc/hiss" ));

				// make sure that we even have some
				ent->client->ps.inventory[INV_SEEKER]--;
				ent->client->ps.powerups[PW_SEEKER] = level.time + 1000;// can only drop one every second..maybe this is annoying?

			}
		}
	}
}

/*
================
Cmd_UseGoggles_f
================
*/
void Cmd_UseGoggles_f(gentity_t *ent)
{
	if ( ent->health < 1 || in_camera )
	{
		return;
	}

	if ( ent->client && ent->client->ps.inventory[INV_LIGHTAMP_GOGGLES] > 0 )
	{
		G_AddEvent( ent, EV_USE_INV_LIGHTAMP_GOGGLES, 0 );
	}
}

/*
================
Cmd_UseSentry_f
================
*/
qboolean place_portable_assault_sentry( gentity_t *self, vec3_t origin, vec3_t dir );
void Cmd_UseSentry_f(gentity_t *ent)
{
	if ( ent->health < 1 || in_camera )
	{
		return;
	}

	if ( ent->client->ps.inventory[INV_SENTRY] <= 0 )
	{
		// have none to place...play sound?
		return;
	}

	if ( place_portable_assault_sentry( ent, ent->currentOrigin, ent->client->ps.viewangles ))
	{
		ent->client->ps.inventory[INV_SENTRY]--;
		G_AddEvent( ent, EV_USE_INV_SENTRY, 0 );
	}
	else
	{
		// couldn't be placed....play a notification sound!!
	}
}

/*
================
Cmd_UseJetpack_f
================
*/
void Cmd_UseJetpack_f(gentity_t *ent)
{
	G_JetpackToggle( ent );
}

/*
================
Cmd_UseWristFlamer_f
================
*/
void Cmd_UseWristFlamer_f(gentity_t *ent)
{
	G_WristFlamerToggle( ent );
}

/*
================
Cmd_UseInventory_f
================
*/
void Cmd_UseInventory_f(gentity_t *ent)
{
	switch (cg.inventorySelect)
	{
		case INV_ELECTROBINOCULARS :
			Cmd_UseElectrobinoculars_f(ent);
			return;
			// WTF WHY WAS THIS COMMENTED OUT --eez
		case INV_BACTA_CANISTER :
			Cmd_UseBacta_f(ent);
			return;
		case INV_SEEKER :
			Cmd_UseSeeker_f(ent);
			return;
		case INV_LIGHTAMP_GOGGLES :
			Cmd_UseGoggles_f(ent);
			return;
		case INV_SENTRY :
			Cmd_UseSentry_f(ent);
			return;
		case INV_JETPACK :
			Cmd_UseJetpack_f(ent);
			return;
		case INV_WRIST_FLAMER :
			Cmd_UseWristFlamer_f(ent);
			return;
		default :
			return;

	}
}

void Cmd_FlushCamFile_f(gentity_t *ent)
{
	gi.FlushCamFile();
}

void Cmd_FlushSpawnerFile_f(gentity_t* ent)
{
	gi.FlushSpawnerFile();
}

/*
=================
Cmd_AnimOverride_f

animoverride <name>: forces the animation override <name> on the player.
animoverride default: forces _humanoid.gla, also on a model with animoverride.cfg.
animoverride none: removes it. The player uses the override of its model again, if it has one.
animoverride: shows the animation file set of the player.
=================
*/
extern qboolean G_SetAnimOverride(gentity_t *ent, const char *overrideName);
void Cmd_AnimOverride_f(gentity_t *ent)
{
	if (gi.argc() < 2)
	{
		const int fileIndex = ent->client->clientInfo.animFileIndex;
		const char *fileSet = (fileIndex >= 0 && fileIndex < level.numKnownAnimFileSets) ? level.knownAnimFileSets[fileIndex].filename : "none";
		gi.SendServerCommand(ent - g_entities, va("print \"Animation file set: %s\nUsage: animoverride <name> | default | none\n\"", fileSet));
		return;
	}

	char overrideName[MAX_QPATH];
	Q_strncpyz(overrideName, gi.argv(1), sizeof(overrideName));
	if (!G_SetAnimOverride(ent, Q_stricmp(overrideName, "none") ? overrideName : ""))
	{
		gi.SendServerCommand(ent - g_entities, va("print \"animoverride: %s is not used, see the console\n\"", overrideName));
	}
}

void G_Taunt( gentity_t *ent )
{
	if ( ent->client )
	{
		if ( ent->client->ps.weapon == WP_SABER
			&& (ent->client->ps.saberAnimLevel == SS_STAFF //ent->client->ps.saber[0].type == SABER_STAFF
				|| ent->client->ps.dualSabers) )
		{
			ent->client->ps.taunting = level.time + 100;
			//make sure all sabers are on
			ent->client->ps.SaberActivate();
		}
		else
		{
			ent->client->ps.taunting = level.time + 100;
		}
	}
}

void G_Victory( gentity_t *ent )
{
	if ( ent->health > 0 )
	{//say something and put away saber
		G_SoundOnEnt( ent, CHAN_VOICE, "sound/chars/kyle/misc/taunt1.wav" );
		if ( ent->client )
		{
			ent->client->ps.SaberDeactivate();
		}
	}
}

enum
{
	TAUNT_TAUNT = 0,
	TAUNT_BOW,
	TAUNT_MEDITATE,
	TAUNT_FLOURISH,
	TAUNT_GLOAT
};

extern void G_SpeechEvent( gentity_t *self, int event );
void G_TauntSound( gentity_t *ent, int taunt )
{
	switch ( taunt )
	{
	case TAUNT_TAUNT:
	default:
		if ( Q_irand( 0, 1 ) )
		{
			G_SpeechEvent( ent, Q_irand( EV_ANGER1, EV_ANGER3 ) );
		}
		else
		{
			G_SpeechEvent( ent, Q_irand( EV_TAUNT1, EV_TAUNT3 ) );
		}
		break;
	case TAUNT_BOW:
		break;
	case TAUNT_MEDITATE:
		break;
	case TAUNT_FLOURISH:
		if ( Q_irand( 0, 1 ) )
		{
			G_SpeechEvent( ent, Q_irand( EV_DEFLECT1, EV_DEFLECT3 ) );
		}
		else
		{
			G_SpeechEvent( ent, Q_irand( EV_GLOAT1, EV_GLOAT3 ) );
		}
		break;
	case TAUNT_GLOAT:
		G_SpeechEvent( ent, Q_irand( EV_VICTORY1, EV_VICTORY3 ) );
		break;
	}
}

void G_SetTauntAnim( gentity_t *ent, int taunt )
{
	if ( !ent || !ent->client )
	{
		return;
	}
	if ( !ent->client->ps.torsoAnimTimer
		&& !ent->client->ps.legsAnimTimer
		&& !ent->client->ps.weaponTime
		&& ent->client->ps.saberLockTime < level.time )
	{
		int anim = -1;
		switch ( taunt )
		{
		case TAUNT_TAUNT:
			if ( ent->client->ps.weapon != WP_SABER )
			{
				anim = BOTH_ENGAGETAUNT;
			}
			else if ( ent->client->ps.saber[0].tauntAnim != -1 )
			{
				anim = ent->client->ps.saber[0].tauntAnim;
			}
			else if ( ent->client->ps.dualSabers
				&& ent->client->ps.saber[1].tauntAnim != -1 )
			{
				anim = ent->client->ps.saber[1].tauntAnim;
			}
			else
			{
				switch ( ent->client->ps.saberAnimLevel )
				{
				case SS_FAST:
				case SS_TAVION:
					if ( ent->client->ps.saber[1].Active() )
					{//turn off second saber
						G_Sound( ent, ent->client->ps.saber[1].soundOff );
					}
					else if ( ent->client->ps.saber[0].Active() )
					{//turn off first
						G_Sound( ent, ent->client->ps.saber[0].soundOff );
					}
					ent->client->ps.SaberDeactivate();
					anim = BOTH_GESTURE1;
					break;
				case SS_MEDIUM:
				case SS_STRONG:
				case SS_DESANN:
					anim = BOTH_ENGAGETAUNT;
					break;
				case SS_DUAL:
					ent->client->ps.SaberActivate();
					anim = BOTH_DUAL_TAUNT;
					break;
				case SS_STAFF:
					ent->client->ps.SaberActivate();
					anim = BOTH_STAFF_TAUNT;
					break;
				}
			}
			break;
		case TAUNT_BOW:
			if ( ent->client->ps.weapon != WP_SABER )
			{
				anim = BOTH_BOW;
			}
			else if ( ent->client->ps.saber[0].bowAnim != -1 )
			{
				anim = ent->client->ps.saber[0].bowAnim;
			}
			else if ( ent->client->ps.dualSabers
				&& ent->client->ps.saber[1].bowAnim != -1 )
			{
				anim = ent->client->ps.saber[1].bowAnim;
			}
			else
			{
				anim = BOTH_BOW;
			}
			if ( ent->client->ps.weapon == WP_SABER )
			{
				if ( ent->client->ps.saber[1].Active() )
				{//turn off second saber
					G_Sound( ent, ent->client->ps.saber[1].soundOff );
				}
				else if ( ent->client->ps.saber[0].Active() )
				{//turn off first
					G_Sound( ent, ent->client->ps.saber[0].soundOff );
				}
				ent->client->ps.SaberDeactivate();
			}
			break;
		case TAUNT_MEDITATE:
			if ( ent->client->ps.weapon != WP_SABER )
			{
				anim = BOTH_MEDITATE;
			}
			else if ( ent->client->ps.saber[0].meditateAnim != -1 )
			{
				anim = ent->client->ps.saber[0].meditateAnim;
			}
			else if ( ent->client->ps.dualSabers
				&& ent->client->ps.saber[1].meditateAnim != -1 )
			{
				anim = ent->client->ps.saber[1].meditateAnim;
			}
			else
			{
				anim = BOTH_MEDITATE;
			}
			if ( ent->client->ps.weapon == WP_SABER )
			{
				if ( ent->client->ps.saber[1].Active() )
				{//turn off second saber
					G_Sound( ent, ent->client->ps.saber[1].soundOff );
				}
				else if ( ent->client->ps.saber[0].Active() )
				{//turn off first
					G_Sound( ent, ent->client->ps.saber[0].soundOff );
				}
				ent->client->ps.SaberDeactivate();
			}
			break;
		case TAUNT_FLOURISH:
			if ( ent->client->ps.weapon == WP_SABER )
			{
				ent->client->ps.SaberActivate();
				if ( ent->client->ps.saber[0].flourishAnim != -1 )
				{
					anim = ent->client->ps.saber[0].flourishAnim;
				}
				else if ( ent->client->ps.dualSabers
					&& ent->client->ps.saber[1].flourishAnim != -1 )
				{
					anim = ent->client->ps.saber[1].flourishAnim;
				}
				else
				{
					switch ( ent->client->ps.saberAnimLevel )
					{
					case SS_FAST:
					case SS_TAVION:
						anim = BOTH_SHOWOFF_FAST;
						break;
					case SS_MEDIUM:
						anim = BOTH_SHOWOFF_MEDIUM;
						break;
					case SS_STRONG:
					case SS_DESANN:
						anim = BOTH_SHOWOFF_STRONG;
						break;
					case SS_DUAL:
						anim = BOTH_SHOWOFF_DUAL;
						break;
					case SS_STAFF:
						anim = BOTH_SHOWOFF_STAFF;
						break;
					}
				}
			}
			break;
		case TAUNT_GLOAT:
			if ( ent->client->ps.weapon == WP_SABER )
			{
				if ( ent->client->ps.saber[0].gloatAnim != -1 )
				{
					anim = ent->client->ps.saber[0].gloatAnim;
				}
				else if ( ent->client->ps.dualSabers
					&& ent->client->ps.saber[1].gloatAnim != -1 )
				{
					anim = ent->client->ps.saber[1].gloatAnim;
				}
				else
				{
					switch ( ent->client->ps.saberAnimLevel )
					{
					case SS_FAST:
					case SS_TAVION:
						anim = BOTH_VICTORY_FAST;
						break;
					case SS_MEDIUM:
						anim = BOTH_VICTORY_MEDIUM;
						break;
					case SS_STRONG:
					case SS_DESANN:
						ent->client->ps.SaberActivate();
						anim = BOTH_VICTORY_STRONG;
						break;
					case SS_DUAL:
						ent->client->ps.SaberActivate();
						anim = BOTH_VICTORY_DUAL;
						break;
					case SS_STAFF:
						ent->client->ps.SaberActivate();
						anim = BOTH_VICTORY_STAFF;
						break;
					}
				}
			}
			break;
		}
		if ( anim != -1 )
		{
			if ( ent->client->ps.groundEntityNum != ENTITYNUM_NONE )
			{
				int parts = SETANIM_TORSO;
				if ( anim != BOTH_ENGAGETAUNT )
				{
					parts = SETANIM_BOTH;
					VectorClear( ent->client->ps.velocity );
				}
				NPC_SetAnim( ent, parts, anim, (SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD) );
			}
			if ( taunt != TAUNT_MEDITATE
				&& taunt != TAUNT_BOW )
			{//no sound for meditate or bow
				G_TauntSound( ent, taunt );
			}
		}
	}
}

extern cvar_t	*g_saberPickuppableDroppedSabers;
extern void WP_RemoveSaber( gentity_t *ent, int saberNum );
extern void CG_ChangeWeapon( int num );
extern void ChangeWeapon( gentity_t *ent, int newWeapon );
void Cmd_SaberDrop_f( gentity_t *ent, int saberNum )
{
	if ( saberNum < 0 )
	{
		return;
	}
	if ( saberNum > 1 )
	{
		return;
	}
	if ( !ent || !ent->client )
	{
		return;
	}
	if ( ent->weaponModel[saberNum] <= 0 )
	{
		return;
	}

	if ( ent->client->ps.weapon != WP_SABER )
	{
		return;
	}

	if ( ent->client->ps.weaponTime > 0 )
	{
		return;
	}

	if ( ent->client->ps.saberMove != LS_READY
		&& ent->client->ps.saberMove != LS_PUTAWAY
		&& ent->client->ps.saberMove != LS_DRAW
		&& ent->client->ps.saberMove != LS_NONE )
	{
		return;
	}

	if ( !g_saberPickuppableDroppedSabers->integer )
	{
		return;
	}

	if ( !ent->client->ps.saber[saberNum].name
		|| !ent->client->ps.saber[saberNum].name[0] )
	{
		return;
	}

	//have a valid string to use for saberType

	//turn it into a pick-uppable item!
	if ( G_DropSaberItem( ent->client->ps.saber[saberNum].name,
			ent->client->ps.saber[saberNum].blade[0].color,
			(saberNum==0?ent->client->renderInfo.handRPoint:ent->client->renderInfo.handLPoint),
			ent->client->ps.velocity,
			ent->currentAngles )
		!= NULL )
	{//dropped it
		WP_RemoveSaber( ent, saberNum );
	}

	if ( ent->weaponModel[0] <= 0
		&& ent->weaponModel[1] <= 0 )
	{//no sabers left!
		//remove saber from inventory
		ent->client->ps.weapons[WP_SABER] = 0;
		//change weapons
		if ( ent->s.number < MAX_CLIENTS )
		{//player
			CG_ChangeWeapon( WP_NONE );
		}
		else
		{
			ChangeWeapon( ent, WP_NONE );
		}
		ent->client->ps.weapon = WP_NONE;
	}
}

/*
=================
ClientCommand
=================
*/
extern void G_ChangePlayerModel(gentity_t* ent, const char* newModel);
void ClientCommand( int clientNum ) {
	gentity_t *ent;
	const char	*cmd;

	ent = g_entities + clientNum;
	if ( !ent->client ) {
		return;		// not fully in game yet
	}

	cmd = gi.argv(0);

	if (Q_stricmp (cmd, "spawn") == 0)
	{
		Cmd_Spawn( ent );
		return;
	}
	if ( !Q_stricmp(cmd, "updateCheatStatsHealth") )
	{
		Cmd_Update_CheatStatsHealth( ent );
		return;
	}
	if ( !Q_stricmp(cmd, "updateCheatStatsShield") )
	{
		Cmd_Update_CheatStatsShield( ent );
		return;
	}
	if ( !Q_stricmp(cmd, "updateCheatStatsAmmo") )
	{
		Cmd_Update_CheatStatsAmmo( ent );
		return;
	}
	if ( !Q_stricmp(cmd, "updateCheatStatsForce") )
	{
		Cmd_Update_CheatStatsForce( ent );
		return;
	}
	if ( !Q_stricmp(cmd, "updateCheatStatsForceRegenRate") )
	{
		Cmd_Update_CheatStatsForceRegenRate( ent );
		return;
	}
	if ( !Q_stricmp(cmd, "updateCheatStatsForceRegenAmount") )
	{
		Cmd_Update_CheatStatsForceRegenAmount( ent );
		return;
	}

	if (!Q_stricmp(cmd, "characterStat"))
	{
		Cmd_CharacterStat_f(ent);
		return;
	}

	if (Q_stricmp(cmd, "give") == 0)
		Cmd_Give_f(ent);
	else if (Q_stricmp(cmd, "weaponStat") == 0)
		Cmd_WeaponStat_f(ent);
	else if (Q_stricmp(cmd, "god") == 0)
		Cmd_God_f(ent);
	else if (Q_stricmp(cmd, "noforce") == 0)
		Cmd_Noforce_f(ent);
	else if (Q_stricmp(cmd, "updateCheatsFlags") == 0)
		Cmd_Update_CF(ent);
	else if (Q_stricmp (cmd, "undying") == 0)
		Cmd_Undying_f (ent);
	else if (Q_stricmp (cmd, "notarget") == 0)
		Cmd_Notarget_f (ent);
	else if (Q_stricmp(cmd, "setForceRegen") == 0)
			Cmd_ForceRegen_f(ent);
	else if (Q_stricmp (cmd, "ledit_mode") == 0)
		G_LightEdit_Cmd_f( ent );
	else if (Q_stricmp (cmd, "noclip") == 0)
	{
		Cmd_Noclip_f (ent);
	}
	else if (Q_stricmp (cmd, "kill") == 0)
	{
		if ( !CheatsOk( ent ) )
		{
			return;
		}
		Cmd_Kill_f (ent);
	}
	else if (Q_stricmp (cmd, "levelshot") == 0)
		Cmd_LevelShot_f (ent);
	else if (Q_stricmp (cmd, "where") == 0)
		Cmd_Where_f (ent);
	else if (Q_stricmp (cmd, "setviewpos") == 0)
		Cmd_SetViewpos_f( ent );
	else if (Q_stricmp (cmd, "setobjective") == 0)
		Cmd_SetObjective_f( ent );
	else if (Q_stricmp (cmd, "viewobjective") == 0)
		Cmd_ViewObjective_f( ent );
	else if (Q_stricmp (cmd, "force_throw") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceThrow( ent, qfalse );
	}
	else if (Q_stricmp (cmd, "force_pull") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceThrow( ent, qtrue );
	}
	else if (Q_stricmp (cmd, "force_speed") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceSpeed( ent );
	}
	else if (Q_stricmp (cmd, "force_heal") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceHeal( ent );
	}
	else if (Q_stricmp(cmd, "force_stasis") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceStasis(ent);
	}
	else if (Q_stricmp(cmd, "force_blast") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceBlast(ent);
	}
	else if (Q_stricmp(cmd, "force_grasp") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceGrasp(ent);
	}
	else if (Q_stricmp(cmd, "force_destruction") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceDestruction(ent);
	}
	else if (Q_stricmp(cmd, "force_strike") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceLightningStrike(ent);
	}
	else if (Q_stricmp(cmd, "force_fear") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceFear(ent);
	}
	else if (Q_stricmp (cmd, "force_grip") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceGrip( ent );
	}
	else if (Q_stricmp (cmd, "force_distract") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceTelepathy( ent );
	}
	else if (Q_stricmp (cmd, "force_rage") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceRage(ent);
	}
	else if (Q_stricmp (cmd, "force_protect") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceProtect(ent);
	}
	else if (Q_stricmp (cmd, "force_absorb") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceAbsorb(ent);
	}
	else if (Q_stricmp (cmd, "force_sight") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		ForceSeeing(ent);
	}
	else if (Q_stricmp (cmd, "addsaberstyle") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		if ( !ent || !ent->client )
		{//wtf?
			return;
		}
		if ( gi.argc() < 2 )
		{
			gi.SendServerCommand( ent-g_entities, va("print \"usage: addsaberstyle <saber style>\n\""));
			gi.SendServerCommand( ent-g_entities, va("print \"Valid styles: SS_FAST, SS_MEDIUM, SS_STRONG, SS_DESANN, SS_TAVION, SS_DUAL and SS_STAFF\n\""));
			return;
		}

		int addStyle = GetIDForString( SaberStyleTable, gi.argv(1) );
		if ( addStyle > SS_NONE && addStyle < SS_STAFF )
		{
			ent->client->ps.saberStylesKnown |= (1<<addStyle);
		}
	}
	/* For UI -> CG Synchronisation */
	/*else if (Q_stricmp(cmd, "syncsaberstyle") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		if (ent->client->ps.saberAnimLevel != cg.saberAnimLevelPending) {
			cg.saberAnimLevelPending = ent->client->ps.saberAnimLevel;
		}
	}*/
	else if (Q_stricmp (cmd, "removesaberstyle") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		if ( !ent || !ent->client )
		{//wtf?
			return;
		}
		if ( gi.argc() < 2 )
		{
			gi.SendServerCommand( ent-g_entities, va("print \"usage: remove <saber style>\n\""));
			gi.SendServerCommand( ent-g_entities, va("print \"Valid styles: SS_FAST, SS_MEDIUM, SS_STRONG, SS_DESANN, SS_TAVION, SS_DUAL and SS_STAFF\n\""));
			gi.SendServerCommand( ent-g_entities, va("print \"If no style are remaining, SS_MEDIUM will be set by default\n\""));
			return;
		}

		int removeStyle = GetIDForString( SaberStyleTable, gi.argv(1) );
		if (removeStyle > SS_NONE && removeStyle < SS_STAFF )
		{
			ent->client->ps.saberStylesKnown &= ~(1<<removeStyle);
		}
		if (ent->client->ps.saberStylesKnown == 0) {
			ent->client->ps.saberStylesKnown = (1 << SS_MEDIUM);
			cg.saberAnimLevelPending = ent->client->ps.saberAnimLevel = SS_MEDIUM;
		}
	}
	else if (Q_stricmp (cmd, "setsaberstyle") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		if ( !ent || !ent->client )
		{//wtf?
			return;
		}
		if ( gi.argc() < 2 )
		{
			gi.SendServerCommand( ent-g_entities, va("print \"usage: setsaberstyle <saber style>\n\""));
			gi.SendServerCommand( ent-g_entities, va("print \"Valid styles: SS_FAST, SS_MEDIUM, SS_STRONG, SS_DESANN, SS_TAVION, SS_DUAL and SS_STAFF\n\""));
			return;
		}

		int setStyle = GetIDForString( SaberStyleTable, gi.argv(1) );
		if ( setStyle > SS_NONE && setStyle < SS_STAFF )
		{
			ent->client->ps.saberStylesKnown = (1<<setStyle);
			cg.saberAnimLevelPending = ent->client->ps.saberAnimLevel = setStyle;
		}
	}
	else if (Q_stricmp (cmd, "taunt") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
//		G_Taunt( ent );
		G_SetTauntAnim( ent, TAUNT_TAUNT );
	}
	else if (Q_stricmp (cmd, "bow") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		G_SetTauntAnim( ent, TAUNT_BOW );
	}
	else if (Q_stricmp (cmd, "meditate") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		G_SetTauntAnim( ent, TAUNT_MEDITATE );
	}
	else if (Q_stricmp (cmd, "flourish") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		G_SetTauntAnim( ent, TAUNT_FLOURISH );
	}
	else if (Q_stricmp (cmd, "gloat") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		G_SetTauntAnim( ent, TAUNT_GLOAT );
	}
	/*
	else if (Q_stricmp (cmd, "drive") == 0)
	{
		if ( !CheatsOk( ent ) )
		{
			return;
		}
		if ( gi.argc() < 2 )
		{
			gi.SendServerCommand( ent-g_entities, va("print \"usage: drive <vehicle name>\n\""));
			gi.SendServerCommand( ent-g_entities, va("print \"Vehicles will be in vehicles.cfg, try using 'speeder' for now\n\""));
			return;
		}
		G_DriveVehicle( ent, NULL, gi.argv(1) );
	}
	*/
	else if (Q_stricmp (cmd, "NPCdrive") == 0)
	{
		if ( !CheatsOk( ent ) )
		{
			return;
		}
		if ( gi.argc() < 3 )
		{
			gi.SendServerCommand( ent-g_entities, va("print \"usage: drive <NPC_targetname> <vehicle name>\n\""));
			gi.SendServerCommand( ent-g_entities, va("print \"Vehicles will be in vehicles.cfg, try using 'speeder' for now\n\""));
			return;
		}
		gentity_t *found = G_Find( NULL, FOFS(targetname), gi.argv(1) );
		if ( found && found->health > 0 && found->client )
		{
			// TEMPORARY! BRING BACK LATER!!!
			//G_DriveVehicle( found, NULL, gi.argv(2) );
		}
	}
	else if (Q_stricmp (cmd, "thereisnospoon") == 0)
		G_StartMatrixEffect( ent );
	else if (Q_stricmp (cmd, "use_electrobinoculars") == 0)
		Cmd_UseElectrobinoculars_f( ent );
	else if (Q_stricmp (cmd, "use_bacta") == 0)
		Cmd_UseBacta_f( ent );
	else if (Q_stricmp (cmd, "use_seeker") == 0)
		Cmd_UseSeeker_f( ent );
	else if (Q_stricmp (cmd, "use_lightamp_goggles") == 0)
		Cmd_UseGoggles_f( ent );
	else if (Q_stricmp (cmd, "use_sentry") == 0)
		Cmd_UseSentry_f( ent );
	else if (Q_stricmp (cmd, "use_jetpack") == 0)
		Cmd_UseJetpack_f( ent );
	else if (Q_stricmp (cmd, "use_wristflamer") == 0)
		Cmd_UseWristFlamer_f( ent );
	else if (Q_stricmp (cmd, "fx") == 0)
		Cmd_Fx( ent );
	else if (Q_stricmp (cmd, "invuse") == 0)
	{
		Cmd_UseInventory_f( ent );
	}
	else if (Q_stricmp (cmd, "playmusic") == 0)
	{
		const char *cmd2 = gi.argv(1);
		if ( cmd2 )
		{
			gi.SetConfigstring( CS_MUSIC, cmd2 );
		}
	}
	else if (Q_stricmp (cmd, "flushcam") == 0)
	{
		Cmd_FlushCamFile_f( ent );
	}
	else if (Q_stricmp(cmd, "flushspawner") == 0)
	{
		Cmd_FlushSpawnerFile_f(ent);
	}
	else if (Q_stricmp(cmd, "dropcurrentweapon") == 0) {
		WP_DropWeapon_Configurable(ent, NULL,true,false,false);
	}
	else if (Q_stricmp(cmd, "countNPC") == 0) {
		int npcCount = 0;
		for(int i = 0 ; i < MAX_GENTITIES; i++)
		{
			if (g_entities[i].inuse && g_entities[i].client && g_entities[i].client->ps.clientNum >= MAX_CLIENTS)
			{
				npcCount++;
			}
		}
		gi.SendServerCommand( ent-g_entities, va("print \"Number of NPCs: %d\n\"", npcCount) );
	}
	else if (Q_stricmp(cmd, "removecurrentweapon") == 0) {
		WP_DropWeapon_Configurable(ent, NULL, true, true, false);
	}
	else if ( Q_stricmp( cmd, "dropsaber" ) == 0 )
	{
		const char *cmd2 = gi.argv(1);
		int	saberNum = 2;//by default, drop both
		if ( cmd2 && cmd2[0] )
		{
			saberNum = atoi(cmd2);
		}
		if ( saberNum > 1 )
		{//drop both
			Cmd_SaberDrop_f( ent, 1 );
			Cmd_SaberDrop_f( ent, 0 );
		}
		else
		{//drop either left or right
			Cmd_SaberDrop_f( ent, saberNum );
		}
	}
	else if (Q_stricmp(cmd, "imhansolo") == 0)
	{
		G_StartMatrixEffect(player);
		gi.SetConfigstring(CS_MUSIC, "music/imhansolo");
		G_ChangePlayerModel(&g_entities[0], "han_solo");
	}
	else if (Q_stricmp(cmd, "camerastatic") == 0)
	{
		if (!static_cam->value)
		{
			gi.SendServerCommand( ent-g_entities, "print \"Static Camera ON\n\"");
			static_cam->value = 1;
		}
		else
		{
			gi.SendServerCommand(ent - g_entities, "print \"Static Camera OFF\n\"");
			static_cam->value = 0;
		}
	}
	else if (Q_stricmp(cmd, "animoverride") == 0)
	{
		Cmd_AnimOverride_f(ent);
	}
	else if (Q_stricmp(cmd, "applycharweapons") == 0)
	{
		ent = G_GetSelfForPlayerCmd();
		
		memset(ent->client->ps.weapons, 0, sizeof(ent->client->ps.weapons)); // Clear all weapons

		// Each weapon, and 150 more ammo of each ammo type (not more than its max).
		const cvar_t *slots[] = { g_weaponOne, g_weaponTwo, g_weaponThree, g_weaponFour, g_weaponFive, g_weaponSix };
		qboolean ammoGiven[MAX_AMMO] = { qfalse };
		for (const cvar_t *slot : slots)
		{
			const int weapon = WP_GetWeaponID(slot->string);
			ent->client->ps.weapons[weapon] = 1;
			const int ammoIndex = weaponData[weapon].ammoIndex;
			if (weapon > WP_NONE && ammoIndex > AMMO_NONE && ammoIndex < ammoCount && !ammoGiven[ammoIndex])
			{
				ammoGiven[ammoIndex] = qtrue;
				ent->client->ps.ammo[ammoIndex] = Q_min(ent->client->ps.ammo[ammoIndex] + 150, ammoData[ammoIndex].max);
			}
		}

		CG_NextWeapon_f();
	}
	else
	{
		gi.SendServerCommand( clientNum, va("print \"Unknown command %s\n\"", cmd ) );
	}
}


/* WEAPON DEBUG UTILITY METHODS */

const char* getStringValueForvec3(const vec3_t* v)
{
	if (!v)
		return "(null)";

	return va("(%f,%f,%f)", (*v)[0], (*v)[1], (*v)[2]);
}
static const char* IntArray3ToString(const int v[3])
{
	return va("%d, %d, %d", v[0], v[1], v[2]);
}
static const char* FloatArray3ToString(const float v[3])
{
	return va("%f, %f, %f", v[0], v[1], v[2]);
}
const char* getStringValueForqboolean(qboolean value)
{
	switch (value)
	{
	case qfalse:
		return "qfalse";
	case qtrue:
		return "qtrue";
	case qunset:
		return "qunset";
	default:
		return "unknown";
	}
}
const char* getStringValueForMethodOfDeath(const meansOfDeath_t* means)
{
	if (!means)
		return "(null)";

	switch (*means)
	{
	case MOD_SABER: return "MOD_SABER";
	case MOD_BRYAR: return "MOD_BRYAR";
	case MOD_BRYAR_ALT: return "MOD_BRYAR_ALT";
	case MOD_BLASTER: return "MOD_BLASTER";
	case MOD_BLASTER_ALT: return "MOD_BLASTER_ALT";
	case MOD_DISRUPTOR: return "MOD_DISRUPTOR";
	case MOD_SNIPER: return "MOD_SNIPER";
	case MOD_BOWCASTER: return "MOD_BOWCASTER";
	case MOD_BOWCASTER_ALT: return "MOD_BOWCASTER_ALT";
	case MOD_REPEATER: return "MOD_REPEATER";
	case MOD_REPEATER_ALT: return "MOD_REPEATER_ALT";
	case MOD_DEMP2: return "MOD_DEMP2";
	case MOD_DEMP2_ALT: return "MOD_DEMP2_ALT";
	case MOD_FLECHETTE: return "MOD_FLECHETTE";
	case MOD_FLECHETTE_ALT: return "MOD_FLECHETTE_ALT";
	case MOD_ROCKET: return "MOD_ROCKET";
	case MOD_ROCKET_ALT: return "MOD_ROCKET_ALT";
	case MOD_CONC: return "MOD_CONC";
	case MOD_CONC_ALT: return "MOD_CONC_ALT";
	case MOD_THERMAL: return "MOD_THERMAL";
	case MOD_THERMAL_ALT: return "MOD_THERMAL_ALT";
	case MOD_DETPACK: return "MOD_DETPACK";
	case MOD_LASERTRIP: return "MOD_LASERTRIP";
	case MOD_LASERTRIP_ALT: return "MOD_LASERTRIP_ALT";
	case MOD_MELEE: return "MOD_MELEE";
	case MOD_SEEKER: return "MOD_SEEKER";
	case MOD_FORCE_GRIP: return "MOD_FORCE_GRIP";
	case MOD_FORCE_LIGHTNING: return "MOD_FORCE_LIGHTNING";
	case MOD_FORCE_DRAIN: return "MOD_FORCE_DRAIN";
	case MOD_EMPLACED: return "MOD_EMPLACED";
	case MOD_CLONERIFLE: return "MOD_CLONERIFLE";
	case MOD_CLONERIFLE_ALT: return "MOD_CLONERIFLE_ALT";
	case MOD_REBELBLASTER: return "MOD_REBELBLASTER";
	case MOD_REBELBLASTER_ALT: return "MOD_REBELBLASTER_ALT";
	case MOD_CLONECOMMANDO: return "MOD_CLONECOMMANDO";
	case MOD_CLONECOMMANDO_ALT: return "MOD_CLONECOMMANDO_ALT";
	case MOD_REBELRIFLE: return "MOD_REBELRIFLE";
	case MOD_REBELRIFLE_ALT: return "MOD_REBELRIFLE_ALT";
	case MOD_REY: return "MOD_REY";
	case MOD_REY_ALT: return "MOD_REY_ALT";
	case MOD_JANGO: return "MOD_JANGO";
	case MOD_JANGO_ALT: return "MOD_JANGO_ALT";
	case MOD_BOBA: return "MOD_BOBA";
	case MOD_BOBA_ALT: return "MOD_BOBA_ALT";
	case MOD_CLONEPISTOL: return "MOD_CLONEPISTOL";
	case MOD_CLONEPISTOL_ALT: return "MOD_CLONEPISTOL_ALT";
	case MOD_CIS_SNIPER: return "MOD_CIS_SNIPER";
	case MOD_CIS_SNIPER_ALT: return "MOD_CIS_SNIPER_ALT";
	case MOD_SBD: return "MOD_SBD";
	case MOD_DROIDEKA: return "MOD_DROIDEKA";
	case MOD_ELECTROCUTE: return "MOD_ELECTROCUTE";
	case MOD_EXPLOSIVE: return "MOD_EXPLOSIVE";
	case MOD_EXPLOSIVE_SPLASH: return "MOD_EXPLOSIVE_SPLASH";
	case MOD_KNOCKOUT: return "MOD_KNOCKOUT";
	case MOD_ENERGY: return "MOD_ENERGY";
	case MOD_ENERGY_SPLASH: return "MOD_ENERGY_SPLASH";
	case MOD_WATER: return "MOD_WATER";
	case MOD_SLIME: return "MOD_SLIME";
	case MOD_LAVA: return "MOD_LAVA";
	case MOD_CRUSH: return "MOD_CRUSH";
	case MOD_IMPACT: return "MOD_IMPACT";
	case MOD_FALLING: return "MOD_FALLING";
	case MOD_SUICIDE: return "MOD_SUICIDE";
	case MOD_TRIGGER_HURT: return "MOD_TRIGGER_HURT";
	case MOD_GAS: return "MOD_GAS";
	case MOD_HIGH_POWERED_SHOT: return "MOD_HIGH_POWERED_SHOT";
	case MOD_DESTRUCTION: return "MOD_DESTRUCTION";
	case MOD_BLAST: return "MOD_BLAST";
	case MOD_STRIKE: return "MOD_STRIKE";
	default: return "MOD_UNKNOWN";
	}
}
const char* getStringValueForfiringLogic(const firingLogic_t* logic)
{
	if (!logic)
		return "(null)";

	switch (*logic)
	{
	case FL_NONE:            return "FL_NONE";
	case FL_MELEE:           return "FL_MELEE";
	case FL_BLASTER:         return "FL_BLASTER";
	case FL_BLASTER_CHARGED: return "FL_BLASTER_CHARGED";
	case FL_BOWCASTER:       return "FL_BOWCASTER";
	case FL_BEAM:            return "FL_BEAM";
	case FL_FULL_BEAM:       return "FL_FULL_BEAM";
	case FL_BEAM_CHARGED:    return "FL_BEAM_CHARGED";
	case FL_GRENADE_LAUNCHER:return "FL_GRENADE_LAUNCHER";
	case FL_DEMP2:           return "FL_DEMP2";
	case FL_DEMP2_ALT:       return "FL_DEMP2_ALT";
	case FL_FLECHETTE:       return "FL_FLECHETTE";
	case FL_FLECHETTE_ALT:   return "FL_FLECHETTE_ALT";
	case FL_NOGHRI:          return "FL_NOGHRI";
	case FL_MISSILE:         return "FL_MISSILE";
	case FL_MISSILE_AIMED:   return "FL_MISSILE_AIMED";
	case FL_LASER_TRAP:      return "FL_LASER_TRAP";
	case FL_PROXIMITY_TRAP:  return "FL_PROXIMITY_TRAP";
	case FL_EXPLOSIVES:      return "FL_EXPLOSIVES";
	case FL_GRENADE:         return "FL_GRENADE";
	case FL_IMPACT_GRENADE:  return "FL_IMPACT_GRENADE";
	case FL_STUNBATON:       return "FL_STUNBATON";
	case FL_SBD:             return "FL_SBD";
	case FL_FLAMETHROWER:    return "FL_FLAMETHROWER";
	case FL_OTHER:           return "FL_OTHER";
	default:                 return "FL_UNKNOWN";
	}
}
const char* getStringValueForblockability(const blockability_t* b)
{
	if (!b)
		return "(null)";

	switch (*b)
	{
	case B_UNSET:        return "B_UNSET";
	case B_DEFLECTABLE:  return "B_DEFLECTABLE";
	case B_BLOCKABLE:    return "B_BLOCKABLE";
	case B_PASSTHROUGH:  return "B_PASSTHROUGH";
	default:             return "B_UNKNOWN";
	}
}
const char* getStringValueForweaponCategory(const weaponCategory_t* wc)
{
	if (!wc)
		return "(null)";

	switch (*wc)
	{
	case WC_UNSET:      return "WC_UNSET";
	case WC_NONE:       return "WC_NONE";
	case WC_MELEE:      return "WC_MELEE";
	case WC_STUN_BATON: return "WC_STUN_BATON";
	case WC_MELEE_1H:   return "WC_MELEE_1H";
	case WC_PISTOL:     return "WC_PISTOL";
	case WC_LIGHT:      return "WC_LIGHT";
	case WC_SNIPER:     return "WC_SNIPER";
	case WC_HEAVY:      return "WC_HEAVY";
	case WC_GRENADE:    return "WC_GRENADE";
	case WC_EXPLOSIVE:  return "WC_EXPLOSIVE";
	case WC_MINIGUN:    return "WC_MINIGUN";
	case WC_SHOULDER:   return "WC_SHOULDER";
	default:            return "WC_UNKNOWN";
	}
}
const char* getStringValueForweaponBucket(const weaponBucket_t* wb)
{
	if (!wb)
		return "(null)";

	switch (*wb)
	{
	case WB_MELEE:          return "WB_MELEE";
	case WB_PISTOLS:        return "WB_PISTOLS";
	case WB_BLASTERS:       return "WB_BLASTERS";
	case WB_SPECIALISTS:    return "WB_SPECIALISTS";
	case WB_HEAVY_WEAPONS:  return "WB_HEAVY_WEAPONS";
	case WB_THROWABLES:     return "WB_THROWABLES";
	case WB_OTHERS:         return "WB_OTHERS";
	case WB_UNSET:          return "WB_UNSET";
	default:                return "WB_UNKNOWN";
	}
}