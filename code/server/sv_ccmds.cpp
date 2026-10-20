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

#include "../server/exe_headers.h"

#include "server.h"
#include "../game/weapons.h"
#include "../game/g_items.h"
#include "../game/statindex.h"
#include "../qcommon/game_version.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

qboolean qbLoadTransition = qfalse;
extern qboolean openedAngles;


//=========================================================
// don't call this directly, it should only be called from SV_Map_f() or SV_MapTransition_f()
//
static bool SV_Map_( ForceReload_e eForceReload )
{
	char		*map = NULL;
	char		expanded[MAX_QPATH] = {0};
	char		*kor_align = NULL;

	char* jk2maps[] = { "kejim_post", "kejim_base", "artus_mine", "artus_detention", "artus_topside",
		"valley", "yavin_temple", "yavin_trial", "ns_streets", "ns_hideout", "ns_starpad",
		"bespin_undercity", "bespin_streets", "bespin_platform", "cairn_bay", "cairn_assembly",
		"cairn_reactor", "cairn_dock1", "doom_comm", "doom_detention", "doom_shields", "yavin_swamp",
		"yavin_canyon", "yavin_courtyard", "yavin_final", "jodemo" };

	char* jkamaps[] = { "academy1", "academy2", "academy3", "academy4", "academy5", "academy6", 
						"hoth2", "hoth3", "kor1", "kor2", "t1_danger", "t1_fatal", "t1_inter", 
						"t1_rail", "t1_sour", "t1_surprise", "t2_dpred", "t2_rancor", "t2_rogue", "t2_trip", 
						"t2_wedge", "t3_bounty", "t3_byss", "t3_hevil", "t3_rift", "t3_stamp", "taspir1", "taspir2", 
						"vjun1", "vjun2", "vjun3", "yavin1", "yavin1b", "yavin2"};

	char* knightfallMaps[] = { "ep3_ok_anakin_r1", "ep3_ok_anakin_r2", "ep3_ok_anakin_r3", "ep3_ok_anakin_r4", "ep3_ok_anakin_r5", "ep3_ok_anakin_r6",
						"ep3_ok_anakin_r7", "ep3_ok_anakin_r8" };

	map = Cmd_Argv(1);
	kor_align = Cmd_Argv(2);

	if ( !*map ) {
		Com_Printf ("no map specified\n");
		return false;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	if (strchr (map, '\\') ) {
		Com_Printf ("Can't have mapnames with a \\\n");
		return false;
	}

	Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);

	if ( FS_ReadFile (expanded, NULL) == -1 ) {
		Com_Printf ("Can't find map %s\n", expanded);
		extern	cvar_t	*com_buildScript;
		if (com_buildScript && com_buildScript->integer)
		{//yes, it's happened, someone deleted a map during my build...
			Com_Error( ERR_FATAL, "Can't find map %s\n", expanded );
		}
		return false;
	}

	if (!g_validJKO->integer)
	{
		for (int i = 0; i < (sizeof(jk2maps)/sizeof(jk2maps[0])); i++)
		{
			if (strcmp(map, jk2maps[i]) == 0)
			{
				Com_Printf("Can't find map %s\n", expanded);
				return false;
			}
		}
			
	}

	if (g_validJKO->integer)
	{
		for (int i = 0; i < (sizeof(jk2maps) / sizeof(jk2maps[0])); i++)
		{
			if (strcmp(map, jk2maps[i]) == 0)
			{
				Cvar_Set("g_char_model", "kyle");
				Cvar_Set("g_char_skin_head", "model_default");
				Cvar_Set("g_char_skin_torso", "model_default");
				Cvar_Set("g_char_skin_legs", "model_default");
				Cvar_Set("g_saber", "kyle");
				Cvar_Set("g_saber_color", "blue");
				Cvar_Set("g_saber2", "empty");
				Cvar_Set("snd", "kyle");
				Cvar_Set("sex", "m");

			}
		}

	}

	bool notJKMap = true;
	bool notKnightfallMap = true;

	for (int i = 0; i < (sizeof(jk2maps) / sizeof(jk2maps[0])); i++)
	{
		if (strcmp(map, jk2maps[i]) == 0)
		{
			notJKMap = false;
		}
	}

	for (int i = 0; i < (sizeof(knightfallMaps) / sizeof(knightfallMaps[0])); i++)
	{
		if (strcmp(map, knightfallMaps[i]) == 0)
		{
			notKnightfallMap = false;
		}
	}


	for (int i = 0; i < (sizeof(jkamaps) / sizeof(jkamaps[0])); i++)
	{
		if (strcmp(map, jkamaps[i]) == 0)
		{
			notJKMap = false;
		}
	}

	if (notJKMap)
	{
		Cvar_Set("g_newgameplus", "0");
	}

	if (!notKnightfallMap)
	{
		Cvar_Set("g_knightfall", "1");
	}
	else
	{
		Cvar_Set("g_knightfall", "0");
	}
	
	if (!strcmp(kor_align, "dark") && (!strcmp(map, "kor1") || !strcmp(map, "kor2")))
	{
		Cvar_Set("g_darkkorriban", "1");
	}
	else if (!strcmp(kor_align, "light") && (!strcmp(map, "kor1") || !strcmp(map, "kor2")))
	{
		Cvar_Set("g_darkkorriban", "0");
	}
	else if (!strcmp(map, "kor1") || !strcmp(map, "kor2"))
	{
	}
	else
	{
		Cvar_Set("g_darkkorriban", "0");
	}


	if (map[0]!='_')
	{
		SG_WipeSavegame("auto");
	}

	Cvar_Set("d_npcfreeze", "0"); // Turn off NPC freeze since you don't need it between maps
	openedAngles = qfalse;
	SV_SpawnServer( map, eForceReload, qtrue );	// start up the map
	return true;
}



// Save out some player data for later restore if this is a spawn point with KEEP_PREV (spawnflags&1) set...
//
// (now also called by auto-save code to setup the cvars correctly
void SV_Player_EndOfLevelSave(void)
{
	int	i;

	// I could just call GetClientState() but that's in sv_bot.cpp, and I'm not sure if that's going to be deleted for
	//	the single player build, so here's the guts again...
	//
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (cl
		&&
		cl->gentity && cl->gentity->client	// crash fix for voy4->brig transition when you kill Foster.
											//	Shouldn't happen, but does sometimes...
		)
	{
		Cvar_Set( sCVARNAME_PLAYERSAVE, "");	// default to blank

//		clientSnapshot_t*	pFrame = &cl->frames[cl->netchan.outgoingSequence & PACKET_MASK];
		playerState_t*		pState = cl->gentity->client;
		const char	*s2;
		const char *s;
#ifdef JK2_MODE
		s = va("%i %i %i %i %i %i %i %f %f %f %i %i %i %i %i %i",
						pState->stats[STAT_HEALTH],
						pState->stats[STAT_ARMOR],
						pState->stats[STAT_WEAPONS],
						pState->stats[STAT_ITEMS],
						pState->weapon,
						pState->weaponstate,
						pState->batteryCharge,
						pState->viewangles[0],
						pState->viewangles[1],
						pState->viewangles[2],
						pState->forcePowersKnown,
						pState->forcePower,
						pState->saberActive,
						pState->saberAnimLevel,
						pState->saberLockEnemy,
						pState->saberLockTime
						);
#else
				//				|general info				  |-force powers |-saber 1		|-saber 2										  |-general saber
				s = va("%i %i %i %i %i %i %f %f %f %i %i %i %i %i %s %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %s %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
						pState->stats[STAT_HEALTH],
						pState->stats[STAT_ARMOR],
						pState->stats[STAT_ITEMS],
						pState->weapon,
						pState->weaponstate,
						pState->batteryCharge,
						pState->viewangles[0],
						pState->viewangles[1],
						pState->viewangles[2],
						//force power data
						pState->forcePowersKnown,
						pState->forcePower,
						pState->forcePowerMax,
						pState->forcePowerRegenRate,
						pState->forcePowerRegenAmount,
						//saber 1 data
						pState->saber[0].name,
						pState->saber[0].blade[0].active,
						pState->saber[0].blade[1].active,
						pState->saber[0].blade[2].active,
						pState->saber[0].blade[3].active,
						pState->saber[0].blade[4].active,
						pState->saber[0].blade[5].active,
						pState->saber[0].blade[6].active,
						pState->saber[0].blade[7].active,
						pState->saber[0].blade[0].color,
						pState->saber[0].blade[1].color,
						pState->saber[0].blade[2].color,
						pState->saber[0].blade[3].color,
						pState->saber[0].blade[4].color,
						pState->saber[0].blade[5].color,
						pState->saber[0].blade[6].color,
						pState->saber[0].blade[7].color,
						//saber 2 data
						pState->saber[1].name,
						pState->saber[1].blade[0].active,
						pState->saber[1].blade[1].active,
						pState->saber[1].blade[2].active,
						pState->saber[1].blade[3].active,
						pState->saber[1].blade[4].active,
						pState->saber[1].blade[5].active,
						pState->saber[1].blade[6].active,
						pState->saber[1].blade[7].active,
						pState->saber[1].blade[0].color,
						pState->saber[1].blade[1].color,
						pState->saber[1].blade[2].color,
						pState->saber[1].blade[3].color,
						pState->saber[1].blade[4].color,
						pState->saber[1].blade[5].color,
						pState->saber[1].blade[6].color,
						pState->saber[1].blade[7].color,
						//general saber data
						pState->saberStylesKnown,
						pState->saberAnimLevel,
						pState->saberLockEnemy,
						pState->saberLockTime
						);
#endif
		Cvar_Set( sCVARNAME_PLAYERSAVE, s );
		Cvar_Set( "playermaxhealth", va("%i", pState->stats[STAT_MAX_HEALTH]) );

#ifndef JK2_MODE
		s2 = "";
		for (i=0;i< MAX_WEAPONS; i++)
		{
			s2 = va("%s %i",s2, pState->weapons[i]);
		}
		Cvar_Set( "playerweaps", s2 );
#endif
		//ammo
		s2 = "";
		for (i=0;i< MAX_AMMO; i++)
		{
			s2 = va("%s %i",s2, pState->ammo[i]);
		}
		Cvar_Set( "playerammo", s2 );

		//inventory
		s2 = "";
		for (i=0;i< INV_MAX; i++)
		{
			s2 = va("%s %i",s2, pState->inventory[i]);
		}
		Cvar_Set( "playerinv", s2 );

		// the new JK2 stuff - force powers, etc...
		//
		s2 = "";
		for (i=0;i< NUM_FORCE_POWERS; i++)
		{
			s2 = va("%s %i",s2, pState->forcePowerLevel[i]);
		}
		Cvar_Set( "playerfplvl", s2 );
	}
}


// Restart the server on a different map
//
static void SV_MapTransition_f(void)
{
	const char	*spawntarget;

#ifdef JK2_MODE
	SCR_PrecacheScreenshot();
#endif
	SV_Player_EndOfLevelSave();

	spawntarget = Cmd_Argv(2);
	if ( *spawntarget != '\0' )
	{
		Cvar_Set( "spawntarget", spawntarget );
	}
	else
	{
		Cvar_Set( "spawntarget", "" );
	}

	SV_Map_( eForceReload_NOTHING );
}

/*
==================
SV_Map_f

Restart the server on a different map, but clears a cvar so that typing "map blah" doesn't try and preserve
player weapons/ammo/etc from the previous level that you haven't really exited (ie ignores KEEP_PREV on spawn points)
==================
*/
#ifdef JK2_MODE
extern void SCR_UnprecacheScreenshot();
#endif
static void SV_Map_f( void )
{
	Cvar_Set( sCVARNAME_PLAYERSAVE, "");
	Cvar_Set( "playerweaps", "");
	Cvar_Set( "playermaxhealth", "");
	Cvar_Set( "playerlightning", "");
	Cvar_Set( "playerscale", "");
	Cvar_Set( "spawntarget", "" );
	Cvar_Set("tier_storyinfo", "0");
	Cvar_Set("tiers_complete", "");
#ifdef JK2_MODE
	SCR_UnprecacheScreenshot();
#endif

	ForceReload_e eForceReload = eForceReload_NOTHING;	// default for normal load

	char *cmd = Cmd_Argv( 0 );
	if ( !Q_stricmp( cmd, "devmapbsp") )
		eForceReload = eForceReload_BSP;
	else if ( !Q_stricmp( cmd, "devmapmdl") )
		eForceReload = eForceReload_MODELS;
	else if ( !Q_stricmp( cmd, "devmapall") )
		eForceReload = eForceReload_ALL;

	qboolean cheat = (qboolean)(!Q_stricmpn( cmd, "devmap", 6 ) );

	// retain old cheat state
	if ( !cheat && Cvar_VariableIntegerValue( "helpUsObi" ) )
		cheat = qtrue;

	if (SV_Map_( eForceReload ))
	{
		// set the cheat value
		// if the level was started with "map <levelname>", then
		// cheats will not be allowed.  If started with "devmap <levelname>"
		// then cheats will be allowed
		Cvar_Set( "helpUsObi", cheat ? "1" : "0" );
	}
#ifdef JK2_MODE
	Cvar_Set( "cg_missionstatusscreen", "0" );
#endif
}

/*
==================
SV_LoadTransition_f
==================
*/
void SV_LoadTransition_f(void)
{
	const char	*map;
	const char	*spawntarget;

	map = Cmd_Argv(1);
	if ( !*map ) {
		return;
	}

	qbLoadTransition = qtrue;

#ifdef JK2_MODE
	SCR_PrecacheScreenshot();
#endif
	SV_Player_EndOfLevelSave();

	//Save the full current state of the current map so we can return to it later
	SG_WriteSavegame( va("hub/%s", sv_mapname->string), qfalse );

	//set the spawntarget if there is one
	spawntarget = Cmd_Argv(2);
	if ( *spawntarget != '\0' )
	{
		Cvar_Set( "spawntarget", spawntarget );
	}
	else
	{
		Cvar_Set( "spawntarget", "" );
	}

	if ( !SV_TryLoadTransition( map ) )
	{//couldn't load a savegame
		SV_Map_( eForceReload_NOTHING );
	}
	qbLoadTransition = qfalse;
}
//===============================================================

char	*ivtos( const vec3_t v ) {
	static	int		index;
	static	char	str[8][32];
	char	*s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1)&7;

	Com_sprintf (s, 32, "( %i %i %i )", (int)v[0], (int)v[1], (int)v[2]);

	return s;
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	cl = &svs.clients[0];

	if ( !cl ) {
		Com_Printf("Server is not running.\n");
		return;
	}

#if defined(_WIN32)
#define STATUS_OS "Windows"
#elif defined(__linux__)
#define STATUS_OS "Linux"
#elif defined(MACOS_X)
#define STATUS_OS "OSX"
#else
#define STATUS_OS "Unknown"
#endif

	Com_Printf( "name    : %s^7\n", cl->name );
	Com_Printf( "score   : %i\n", cl->gentity->client->persistant[PERS_SCORE] );
	Com_Printf( "version : %s %s %i\n", STATUS_OS, JK_VERSION, PROTOCOL_VERSION );
#ifdef JK2_MODE
	Com_Printf( "game    : Jedi Outcast %s\n", FS_GetCurrentGameDir() );
#else
	Com_Printf( "game    : Jedi Academy %s\n", FS_GetCurrentGameDir() );
#endif
	Com_Printf( "map     : %s at %s\n", sv_mapname->string, ivtos( cl->gentity->client->origin ) );
}

/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	Com_Printf ("Server info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SERVERINFO ) );
}


/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	Com_Printf ("System info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SYSTEMINFO ) );
}


/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 1 ) {
		Com_Printf ("Usage: info\n");
		return;
	}

	cl = &svs.clients[0];
	if ( !cl->state ) {
		Com_Printf("Client is not active\n");
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( cl->userinfo );
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName( char *args, int argNum ) {
	if ( argNum == 2 )
		Field_CompleteFilename( "maps", "bsp", qtrue, qfalse );
}

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteSaveName( char *args, int argNum ) {
	if ( argNum == 2 )
		Field_CompleteFilename( "saves", "sav", qtrue, qtrue );
}

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("systeminfo", SV_Systeminfo_f);
	Cmd_AddCommand ("dumpuser", SV_DumpUser_f);
	Cmd_AddCommand ("sectorlist", SV_SectorList_f);
	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
	Cmd_AddCommand ("devmap", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
	Cmd_AddCommand ("devmapbsp", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmapbsp", SV_CompleteMapName );
	Cmd_AddCommand ("devmapmdl", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmapmdl", SV_CompleteMapName );
	Cmd_AddCommand ("devmapsnd", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmapsnd", SV_CompleteMapName );
	Cmd_AddCommand ("devmapall", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmapall", SV_CompleteMapName );
	Cmd_AddCommand ("maptransition", SV_MapTransition_f);
	Cmd_AddCommand ("load", SV_LoadGame_f);
	Cmd_SetCommandCompletionFunc( "load", SV_CompleteSaveName );
	Cmd_AddCommand ("loadtransition", SV_LoadTransition_f);
	Cmd_AddCommand ("save", SV_SaveGame_f);
	Cmd_AddCommand ("wipe", SV_WipeGame_f);

//#ifdef _DEBUG
//	extern void UI_Dump_f(void);
//	Cmd_AddCommand ("ui_dump", UI_Dump_f);
//#endif
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("serverinfo");
	Cmd_RemoveCommand ("systeminfo");
	Cmd_RemoveCommand ("dumpuser");
	Cmd_RemoveCommand ("serverrecord");
	Cmd_RemoveCommand ("serverstop");
	Cmd_RemoveCommand ("sectorlist");
#endif
}

