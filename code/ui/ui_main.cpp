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

/*
=======================================================================

USER INTERFACE MAIN

=======================================================================
*/

#include <algorithm>
#include <vector>

#include "../server/exe_headers.h"

#include "ui_local.h"

#include "menudef.h"

#include "ui_shared.h"

#include "../ghoul2/G2.h"

#include "../game/bg_public.h"
#include "../game/anims.h"
extern stringID_table_t animTable [MAX_ANIMATIONS+1];

#include "../qcommon/stringed_ingame.h"
#include "../qcommon/q_shared.h"
#include <string>
#include "../qcommon/game_version.h"
#include "../rd-common/mdx_merge.h"
#include "../client/vmachine.h"
#include "ui_lightedit.h"

extern qboolean ItemParse_model_g2anim_go( itemDef_t *item, const char *animName );
extern qboolean ItemParse_asset_model_go( itemDef_t *item, const char *name );
extern qboolean ItemParse_model_g2skin_go( itemDef_t *item, const char *skinName );
extern qboolean UI_SaberModelForSaber( const char *saberName, char *saberModel );
extern qboolean UI_SaberSkinForSaber( const char *saberName, char *saberSkin );
extern void UI_SaberAttachToChar( itemDef_t *item );
extern void Menu_SetItemText(const menuDef_t* menu, const char* itemName, const char* text);
extern void Menu_ShowItemByName(menuDef_t* menu, const char* p, qboolean bShow);

extern qboolean PC_Script_Parse(const char **out);

static int	uiCharConfigCount;
static int	uiCharConfigSelected;
static char	uiCharConfigs[64][MAX_QPATH];
static int	uiCharConfigDeleteIndex = -1;	// configuration of the first Delete click
static int	uiCharConfigDeleteTime;			// realTime limit of the second Delete click

#define LISTBUFSIZE 10240

static struct
{
	char	listBuf[LISTBUFSIZE];			//	The list of file names read in

	// For scrolling through file names
	int				currentLine;		//	Index to currentSaveFileComments[] currently highlighted
	int				saveFileCnt;		//	Number of save files read in

	int				awaitingSave;		//	Flag to see if user wants to overwrite a game.

	char			*savegameMap;
	int				savegameFromFlag;
} s_savegame;

#define MAX_SAVELOADFILES	100
#define MAX_SAVELOADNAME	32

#ifdef JK2_MODE
byte screenShotBuf[SG_SCR_WIDTH * SG_SCR_HEIGHT * 4];
#endif

typedef struct
{
	char *currentSaveFileName;						// file name of savegame
	char currentSaveFileComments[iSG_COMMENT_SIZE];	// file comment
	char currentSaveFileDateTimeString[iSG_COMMENT_SIZE];	// file time and date
	time_t currentSaveFileDateTime;
	char currentSaveFileMap[MAX_TOKEN_CHARS];			// map save game is from
} savedata_t;

static savedata_t s_savedata[MAX_SAVELOADFILES];
void UI_SetActiveMenu( const char* menuname,const char *menuID );
void ReadSaveDirectory (void);
void Item_RunScript(itemDef_t *item, const char *s);
qboolean Item_SetFocus(itemDef_t *item, float x, float y);

qboolean		Asset_Parse(char **buffer);
menuDef_t		*Menus_FindByName(const char *p);
void			Menus_HideItems(const char *menuName);
int				Text_Height(const char *text, float scale, int iFontIndex );
int				Text_Width(const char *text, float scale, int iFontIndex );
void			_UI_DrawTopBottom(float x, float y, float w, float h, float size);
void			_UI_DrawSides(float x, float y, float w, float h, float size);
void			UI_CheckVid1Data(const char *menuTo,const char *warningMenuName);
void			UI_GetVideoSetup ( void );
void			UI_UpdateVideoSetup ( void );
static void		UI_UpdateCharacterCvars ( void );
static void		UI_GetCharacterCvars ( void );
static void		UI_GetCharacterCustomization( void );
static void		UI_RandomSkin(void);
static void		UI_RandomRGB(void);
static void		UI_RGBSaberCvars(void);
static void		UI_UpdateSaberCvars ( void );
static void		UI_GetSaberCvars ( void );
static void		UI_ResetSaberCvars ( void );
static void		UI_InitAllocForcePowers ( const char *forceName );
static void		UI_InitAllocSaberStyle( const char *saberStyle );
static void		UI_SwitchSaberStyle( const char * saberStyle);
static void		UI_GetSaberStyle(void);
static void		UI_Cheats(const char* cheatName);
static void		UI_AffectForcePowerLevel ( const char *forceName );
static void		UI_ShowForceLevelDesc ( const char *forceName );
static void		UI_ResetForceLevels ( void );
static void		UI_ClearWeapons ( void );
static void		UI_GiveWeapon ( const int weaponIndex );
static void		UI_EquipWeapon ( const int weaponIndex );
static void		UI_LoadMissionSelectMenu( const char *cvarName );
static void		UI_AddWeaponSelection ( const int weaponIndex, const int ammoIndex, const int ammoAmount, const char *iconItemName,const char *litIconItemName, const char *hexBackground, const char *soundfile );
static void		UI_AddThrowWeaponSelection ( const int weaponIndex, const int ammoIndex, const int ammoAmount, const char *iconItemName,const char *litIconItemName, const char *hexBackground, const char *soundfile );
static void		UI_RemoveWeaponSelection ( const int weaponIndex );
static void		UI_RemoveThrowWeaponSelection ( void );
static void		UI_HighLightWeaponSelection ( const int selectionslot );
static void		UI_NormalWeaponSelection ( const int selectionslot );
static void		UI_NormalThrowSelection ( void );
static void		UI_HighLightThrowSelection( void );
static void		UI_ClearInventory ( void );
static void		UI_GiveInventory ( const int itemIndex, const int amount );
static void		UI_ForcePowerWeaponsButton(qboolean activeFlag);
static void		UI_UpdateCharacterSkin( void );
static void		UI_UpdateCharacter( qboolean changedModel );
static void		UI_UpdateSaberType( void );
static void		UI_UpdateSaberHilt( qboolean secondSaber );
//static void		UI_UpdateSaberColor( qboolean secondSaber );
static void		UI_InitWeaponSelect( void );
static void		UI_WeaponHelpActive( void );
static void		UI_AnglesEsc();
static void		UI_OpenedAngles();

#ifndef JK2_MODE
static void		UI_UpdateFightingStyle ( void );
static void		UI_UpdateFightingStyleChoices ( void );
static void		UI_CalcForceStatus(void);
#endif // !JK2_MODE

static void		UI_DecrementForcePowerLevel( void );
static void		UI_DecrementCurrentForcePower ( void );
static void		UI_ShutdownForceHelp( void );
static void		UI_ForceHelpActive( void );

#ifndef JK2_MODE
static void		UI_DemoSetForceLevels( void );
#endif // !JK2_MODE

static void		UI_RecordForceLevels( void );
static void		UI_RecordWeapons( void );
static void		UI_ResetCharacterListBoxes( void );


void		UI_LoadMenus(const char *menuFile, qboolean reset);
static void		UI_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle, int iFontIndex);
static qboolean UI_OwnerDrawVisible(int flags);
int				UI_OwnerDrawWidth(int ownerDraw, float scale);
static void		UI_Update(const char *name);
void			UI_UpdateCvars( void );
void			UI_ResetDefaults( void );
void			UI_AdjustSaveGameListBox( int currentLine );

void			Menus_CloseByName(const char *p);

//static void UI_ApplyCharWeapons(void);

static void UI_UpdateNPCCvars(void);

static void UI_UpdateTeamSelect(void);

static void UI_UpdateSaberVisibility(qboolean weaponsTab);
static void UI_PreviewNextStance(void);
extern qboolean UI_SaberTypeForSaber(const char *saberName, char *saberType);
extern saberType_t TranslateSaberType(const char *name);

// State of the preview of the character menu (see UI_UpdateCharPreview): what it shows now, and whether it hides the sabers.
static char uiPreviewSig[256];
static qboolean uiPreviewHideSaber;

static void UI_HiltChange(int i);

static void UI_RecordForcePowers(const char** args);

static void UI_ApplyForcePowers(void);

static void UI_CharacterSelect(const char** args);

static void UI_ApplySaberStyles(void);

static void UI_ShowMissionInfo(void);

static void UI_CharacterDefaultSkin(const char* otherSkin);

static void UI_SaveCharacterPowers(void);
static void UI_LoadCharacterCfg(void);
static void Com_FlushCharacterFile(void);
static void UI_LoadCharacterDefaultCfg(void);
static void UI_RefreshCharConfigs(const char *select);
static void UI_RefreshAnimOverrides(qboolean rebuildList);

// Arguments of "animoverride" for FEEDER_ANIM_OVERRIDES: "none", "default", then the override folders.
static std::vector<std::string> uiAnimOverrides;

// FEEDER_AMMO: the ammo types of the player (ammoData index) and their translated names, from the cgame.
static void UI_RefreshAmmoTypes(void);
static void UI_SelectAmmoType(int index);
static std::vector<int> uiAmmoTypes;
static std::vector<std::string> uiAmmoNames;
static void UI_SaveCharConfig(void);
static void UI_LoadCharConfig(void);
static void UI_DeleteCharConfig(void);

// Movedata Sounds
enum
{
	MDS_NONE = 0,
	MDS_FORCE_JUMP,
	MDS_ROLL,
	MDS_SABER,
	MDS_MOVE_SOUNDS_MAX
};

enum
{
	MD_ACROBATICS = 0,
	MD_SINGLE_FAST,
	MD_SINGLE_MEDIUM,
	MD_SINGLE_STRONG,
	MD_DUAL_SABERS,
	MD_SABER_STAFF,
	MD_MOVE_TITLE_MAX
};

// Some hard coded badness
// At some point maybe this should be externalized to a .dat file
const char *datapadMoveTitleData[MD_MOVE_TITLE_MAX] =
{
"@MENUS_ACROBATICS",
"@MENUS_SINGLE_FAST",
"@MENUS_SINGLE_MEDIUM",
"@MENUS_SINGLE_STRONG",
"@MENUS_DUAL_SABERS",
"@MENUS_SABER_STAFF",
};

const char *datapadMoveTitleBaseAnims[MD_MOVE_TITLE_MAX] =
{
"BOTH_RUN1",
"BOTH_SABERFAST_STANCE",
"BOTH_STAND2",
"BOTH_SABERSLOW_STANCE",
"BOTH_SABERDUAL_STANCE",
"BOTH_SABERSTAFF_STANCE",
};

#define MAX_MOVES 16

typedef struct
{
	const char	*title;
	const char	*desc;
	const char	*anim;
	short	sound;
} datpadmovedata_t;


#define MAX_MISSION_TOPIC 16
#define MAX_MISSION 32

typedef struct
{
	const char *title;
	const char *value;
} missionTopicData_t;

typedef struct missionData
{
	const char *title;
	const char *missionNum;
	const char *picCode;
	const char *mapCode;
	const char *descCode;
	qboolean characterSelect;

	missionData() = default;
	missionData(const char *title, const char *missionNum, const char *picCode,
				const char *mapCode, const char *descCode, qboolean characterSelect = qfalse):
				title{title}, missionNum{missionNum}, picCode{picCode},
				mapCode{mapCode}, descCode{descCode}, characterSelect{characterSelect} {}
} missionData_t;

static datpadmovedata_t datapadMoveData[MD_MOVE_TITLE_MAX][MAX_MOVES] =
{
{
// Acrobatics
{ "@MENUS_FORCE_JUMP1",				"@MENUS_FORCE_JUMP1_DESC",			"BOTH_FORCEJUMP1",				MDS_FORCE_JUMP },
{ "@MENUS_FORCE_FLIP",				"@MENUS_FORCE_FLIP_DESC",			"BOTH_FLIP_F",					MDS_FORCE_JUMP },
{ "@MENUS_ROLL",						"@MENUS_ROLL_DESC",					"BOTH_ROLL_F",					MDS_ROLL },
{ "@MENUS_BACKFLIP_OFF_WALL",			"@MENUS_BACKFLIP_OFF_WALL_DESC",	"BOTH_WALL_FLIP_BACK1",			MDS_FORCE_JUMP },
{ "@MENUS_SIDEFLIP_OFF_WALL",			"@MENUS_SIDEFLIP_OFF_WALL_DESC",	"BOTH_WALL_FLIP_RIGHT",			MDS_FORCE_JUMP },
{ "@MENUS_WALL_RUN",					"@MENUS_WALL_RUN_DESC",				"BOTH_WALL_RUN_RIGHT",			MDS_FORCE_JUMP },
{ "@MENUS_LONG_JUMP",					"@MENUS_LONG_JUMP_DESC",			"BOTH_FORCELONGLEAP_START",		MDS_FORCE_JUMP },
{ "@MENUS_WALL_GRAB_JUMP",			"@MENUS_WALL_GRAB_JUMP_DESC",		"BOTH_FORCEWALLREBOUND_FORWARD",MDS_FORCE_JUMP },
{ "@MENUS_RUN_UP_WALL_BACKFLIP",		"@MENUS_RUN_UP_WALL_BACKFLIP_DESC",	"BOTH_FORCEWALLRUNFLIP_START",	MDS_FORCE_JUMP },
{ "@MENUS_JUMPUP_FROM_KNOCKDOWN",		"@MENUS_JUMPUP_FROM_KNOCKDOWN_DESC","BOTH_KNOCKDOWN3",				MDS_NONE },
{ "@MENUS_JUMPKICK_FROM_KNOCKDOWN",	"@MENUS_JUMPKICK_FROM_KNOCKDOWN_DESC","BOTH_KNOCKDOWN2",			MDS_NONE },
{ "@MENUS_ROLL_FROM_KNOCKDOWN",		"@MENUS_ROLL_FROM_KNOCKDOWN_DESC",	"BOTH_KNOCKDOWN1",				MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
},

{
//Single Saber, Fast Style
{ "@MENUS_STAB_BACK",					"@MENUS_STAB_BACK_DESC",			"BOTH_A2_STABBACK1",			MDS_SABER },
{ "@MENUS_LUNGE_ATTACK",				"@MENUS_LUNGE_ATTACK_DESC",			"BOTH_LUNGE2_B__T_",			MDS_SABER },
{ "@MENUS_FORCE_PULL_IMPALE",			"@MENUS_FORCE_PULL_IMPALE_DESC",	"BOTH_PULL_IMPALE_STAB",		MDS_SABER },
{ "@MENUS_FAST_ATTACK_KATA",			"@MENUS_FAST_ATTACK_KATA_DESC",		"BOTH_A1_SPECIAL",				MDS_SABER },
{ "@MENUS_ATTACK_ENEMYONGROUND",		"@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN",				MDS_FORCE_JUMP },
{ "@MENUS_CARTWHEEL",					"@MENUS_CARTWHEEL_DESC",			"BOTH_ARIAL_RIGHT",				MDS_FORCE_JUMP },
{ "@MENUS_BOTH_ROLL_STAB",			"@MENUS_BOTH_ROLL_STAB2_DESC",		"BOTH_ROLL_STAB",				MDS_SABER },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
},

{
//Single Saber, Medium Style
{ "@MENUS_SLASH_BACK",				"@MENUS_SLASH_BACK_DESC",			"BOTH_ATTACK_BACK",				MDS_SABER },
{ "@MENUS_FLIP_ATTACK",				"@MENUS_FLIP_ATTACK_DESC",			"BOTH_JUMPFLIPSLASHDOWN1",		MDS_FORCE_JUMP },
{ "@MENUS_FORCE_PULL_SLASH",			"@MENUS_FORCE_PULL_SLASH_DESC",		"BOTH_PULL_IMPALE_SWING",		MDS_SABER },
{ "@MENUS_MEDIUM_ATTACK_KATA",		"@MENUS_MEDIUM_ATTACK_KATA_DESC",	"BOTH_A2_SPECIAL",				MDS_SABER },
{ "@MENUS_ATTACK_ENEMYONGROUND",		"@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN",				MDS_FORCE_JUMP },
{ "@MENUS_CARTWHEEL",					"@MENUS_CARTWHEEL_DESC",			"BOTH_ARIAL_RIGHT",				MDS_FORCE_JUMP },
{ "@MENUS_BOTH_ROLL_STAB",			"@MENUS_BOTH_ROLL_STAB2_DESC",		"BOTH_ROLL_STAB",				MDS_SABER },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
},

{
//Single Saber, Strong Style
{ "@MENUS_SLASH_BACK",				"@MENUS_SLASH_BACK_DESC",			"BOTH_ATTACK_BACK",				MDS_SABER },
{ "@MENUS_JUMP_ATTACK",				"@MENUS_JUMP_ATTACK_DESC",			"BOTH_FORCELEAP2_T__B_",		MDS_FORCE_JUMP },
{ "@MENUS_FORCE_PULL_SLASH",			"@MENUS_FORCE_PULL_SLASH_DESC",		"BOTH_PULL_IMPALE_SWING",		MDS_SABER },
{ "@MENUS_STRONG_ATTACK_KATA",		"@MENUS_STRONG_ATTACK_KATA_DESC",	"BOTH_A3_SPECIAL",				MDS_SABER },
{ "@MENUS_ATTACK_ENEMYONGROUND",		"@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN",				MDS_FORCE_JUMP },
{ "@MENUS_CARTWHEEL",					"@MENUS_CARTWHEEL_DESC",			"BOTH_ARIAL_RIGHT",				MDS_FORCE_JUMP },
{ "@MENUS_BOTH_ROLL_STAB",			"@MENUS_BOTH_ROLL_STAB2_DESC",		"BOTH_ROLL_STAB",				MDS_SABER },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
},

{
//Dual Sabers
{ "@MENUS_SLASH_BACK",				"@MENUS_SLASH_BACK_DESC",			"BOTH_ATTACK_BACK",				MDS_SABER },
{ "@MENUS_FLIP_FORWARD_ATTACK",		"@MENUS_FLIP_FORWARD_ATTACK_DESC",	"BOTH_JUMPATTACK6",				MDS_FORCE_JUMP },
{ "@MENUS_DUAL_SABERS_TWIRL",			"@MENUS_DUAL_SABERS_TWIRL_DESC",	"BOTH_SPINATTACK6",				MDS_SABER },
{ "@MENUS_ATTACK_ENEMYONGROUND",		"@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN_DUAL",			MDS_FORCE_JUMP },
{ "@MENUS_DUAL_SABER_BARRIER",		"@MENUS_DUAL_SABER_BARRIER_DESC",	"BOTH_A6_SABERPROTECT",			MDS_SABER },
{ "@MENUS_DUAL_STAB_FRONT_BACK",		"@MENUS_DUAL_STAB_FRONT_BACK_DESC", "BOTH_A6_FB",					MDS_SABER },
{ "@MENUS_DUAL_STAB_LEFT_RIGHT",		"@MENUS_DUAL_STAB_LEFT_RIGHT_DESC", "BOTH_A6_LR",					MDS_SABER },
{ "@MENUS_CARTWHEEL",					"@MENUS_CARTWHEEL_DESC",			"BOTH_ARIAL_RIGHT",				MDS_FORCE_JUMP },
{ "@MENUS_BOTH_ROLL_STAB",			"@MENUS_BOTH_ROLL_STAB_DESC",		"BOTH_ROLL_STAB",				MDS_SABER },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
},

{
// Saber Staff
{ "@MENUS_STAB_BACK",					"@MENUS_STAB_BACK_DESC",			"BOTH_A2_STABBACK1",			MDS_SABER },
{ "@MENUS_BACK_FLIP_ATTACK",			"@MENUS_BACK_FLIP_ATTACK_DESC",		"BOTH_JUMPATTACK7",				MDS_FORCE_JUMP },
{ "@MENUS_SABER_STAFF_TWIRL",			"@MENUS_SABER_STAFF_TWIRL_DESC",	"BOTH_SPINATTACK7",				MDS_SABER },
{ "@MENUS_ATTACK_ENEMYONGROUND",		"@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN_STAFF",			MDS_FORCE_JUMP },
{ "@MENUS_SPINNING_KATA",				"@MENUS_SPINNING_KATA_DESC",		"BOTH_A7_SOULCAL",				MDS_SABER },
{ "@MENUS_KICK1",						"@MENUS_KICK1_DESC",				"BOTH_A7_KICK_F",				MDS_FORCE_JUMP },
{ "@MENUS_JUMP_KICK",					"@MENUS_JUMP_KICK_DESC",			"BOTH_A7_KICK_F_AIR",			MDS_FORCE_JUMP },
{ "@MENUS_SPLIT_KICK",				"@MENUS_SPLIT_KICK_DESC",			"BOTH_A7_KICK_RL",				MDS_FORCE_JUMP },
{ "@MENUS_SPIN_KICK",					"@MENUS_SPIN_KICK_DESC",			"BOTH_A7_KICK_S",				MDS_FORCE_JUMP },
{ "@MENUS_FLIP_KICK",					"@MENUS_FLIP_KICK_DESC",			"BOTH_A7_KICK_BF",				MDS_FORCE_JUMP },
{ "@MENUS_BUTTERFLY_ATTACK",			"@MENUS_BUTTERFLY_ATTACK_DESC",		"BOTH_BUTTERFLY_FR1",			MDS_SABER },
{ "@MENUS_BOTH_ROLL_STAB",			"@MENUS_BOTH_ROLL_STAB2_DESC",		"BOTH_ROLL_STAB",				MDS_SABER },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
{ NULL, NULL, 0,	MDS_NONE },
}
};

static missionTopicData_t missionTopicData[1][MAX_MISSION_TOPIC] =
{
{
	{ "@SWGLMISSIONS_MISSIONS_EPI",					"0"},
	//{ "@SWGLMISSIONS_MISSIONS_EPII",					"1"},
	{ "@SWGLMISSIONS_MISSIONS_EPIII",					"1"},
	//{ "@SWGLMISSIONS_MISSIONS_EPIV",					"3"},
	//{ "@SWGLMISSIONS_MISSIONS_EPV",						"4"},
	{ "@SWGLMISSIONS_MISSIONS_EPVI",					"2"},
	//{ "@SWGLMISSIONS_MISSIONS_EPVII",					"6"},
	{ "@SWGLMISSIONS_MISSIONS_EPVIII",					"3"},
	//{ "@SWGLMISSIONS_MISSIONS_EPIX",					"8"},
	{ "@SWGLMISSIONS_MISSIONS_THECLONEWARS",					"4"},
	{ "@SWGLMISSIONS_MISSIONS_KOTOR",					"5"},
	{ "@SWGLMISSIONS_MISSIONS_JKJO",					"6"},
	{ "@SWGLMISSIONS_MISSIONS_JKJA",					"7"},
	{ "@SWGLMISSIONS_MISSIONS_CHALLENGES",				"8"},
	{ "@SWGLMISSIONS_MISSIONS_EXTRAS",					"9"},
	{ NULL,	NULL},
}
};

static missionData_t missionData[MAX_MISSION_TOPIC][MAX_MISSION] =
{

	// Phantom Menace

{
	{ "@SWGLMISSIONS_EPI_TRAINING",			"0",		"Ep1_Training", "ep1_training", "@SWGLMISSIONS_EP1_TRAINING_DESC", qfalse},
	{ "@SWGLMISSIONS_EPI_DOTF",			"1",		"Ep1_DotF", NULL, "@SWGLMISSIONS_EP1_DOTF_DESC", qtrue},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},
// Revenge of the Sith
{
	{ "@SWGLMISSIONS_EPIII_TODP",		"0",			"Ep3_ToDP", "ep3_todp_ani", "@SWGLMISSIONS_EP3_TODP_DESC",},
	{ "@SWGLMISSIONS_EPIII_FOTR",		"1",			"Ep3_FotR", NULL, "@SWGLMISSIONS_EP3_FOTR_DESC", qtrue},
	{ "@SWGLMISSIONS_EPIII_OK",			"2",		"Ep3_OK", NULL, "@SWGLMISSIONS_EP3_OK_DESC", qtrue},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},
// A New Hope
// The Empire Strikes Back
// Return of the Jedi
{
	{ "@SWGLMISSIONS_EPVI_FVS",			"0",		"Ep6_FVS", NULL, "@SWGLMISSIONS_EP6_FVS_DESC", qtrue},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},
// The Force Awakens
// The Last Jedi
{
	{ "@SWGLMISSIONS_EPVIII_TLJ",		"0",			"Ep8_TLJ", "ep8_tlj_kylo", "@SWGLMISSIONS_EP8_TLJ_DESC" },
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},
// The Clone Wars
{
	{ "@SWGLMISSIONS_TCW_YHBAR",		"0",			"TCW_YHBAR", "tcw_yhbar_sidious_1", "@SWGLMISSIONS_TCW_YHBAR_DESC"},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},
// The Old Republic
{
	{ "@SWGLMISSIONS_TOR_RVM",			"0",		"KotOR_SF", "kotor_sf_revan", "@SWGLMISSIONS_KOTOR_SF_DESC"},
	{ "@SWGLMISSIONS_TOR_TTC",			"1",		"KotOR_TTC", "kotor_ttc_meetra", "@SWGLMISSIONS_KOTOR_TTC_DESC"},
	{ "@SWGLMISSIONS_TOR_TFE",			"2",		"SWTOR_FE", NULL, "@SWGLMISSIONS_SWTOR_TFE_DESC", qtrue},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},
// Jedi Outcast
{
	{ "@SWGLMISSIONS_KEJIM_POST",		"0",			"levelshots_sav/kejim_post", "kejim_post", "@SWGLMISSIONS_KEJIM_POST_DESC"},
	{ "@SWGLMISSIONS_KEJIM_BASE",		"1",			"levelshots_sav/kejim_base", "kejim_base", "@SWGLMISSIONS_KEJIM_BASE_DESC"},
	{ "@SWGLMISSIONS_ARTUS_MINE",		"2",			"levelshots_sav/artus_mine", "artus_mine", "@SWGLMISSIONS_ARTUS_MINE_DESC"},
	{ "@SWGLMISSIONS_ARTUS_DETENTION",	"3",				"levelshots_sav/artus_detention", "artus_detention", "@SWGLMISSIONS_ARTUS_DETENTION_DESC"},
	{ "@SWGLMISSIONS_ARTUS_TOPSIDE",	"4",				"levelshots_sav/artus_topside", "artus_topside", "@SWGLMISSIONS_ARTUS_TOPSIDE_DESC"},
	{ "@SWGLMISSIONS_VALLEY",		"5",			"levelshots_sav/valley", "valley", "@SWGLMISSIONS_VALLEY_DESC"},
	{ "@SWGLMISSIONS_YAVIN_TEMPLE",	"6",				"levelshots_sav/yavin_temple", "yavin_temple", "@SWGLMISSIONS_YAVIN_TEMPLE_DESC"},
	{ "@SWGLMISSIONS_YAVIN_TRIAL",		"7",			"levelshots_sav/yavin_trial", "yavin_trial", "@SWGLMISSIONS_YAVIN_TRIAL_DESC"},
	{ "@SWGLMISSIONS_NS_STREETS",	"8",				"levelshots_sav/ns_streets", "ns_streets", "@SWGLMISSIONS_NS_STREETS_DESC"},
	{ "@SWGLMISSIONS_NS_HIDEOUT",	"9",				"levelshots_sav/ns_hideout", "ns_hideout", "@SWGLMISSIONS_NS_HIDEOUT_DESC"},
	{ "@SWGLMISSIONS_NS_STARPAD",		"10",			"levelshots_sav/ns_starpad", "ns_starpad", "@SWGLMISSIONS_NS_STARPAD_DESC"},
	{ "@SWGLMISSIONS_BESPIN_UNDERCITY",	"11",				"levelshots_sav/bespin_undercity", "bespin_undercity", "@SWGLMISSIONS_BESPIN_UNDERCITY_DESC"},
	{ "@SWGLMISSIONS_BESPIN_STREETS",		"12",			"levelshots_sav/bespin_streets", "bespin_streets", "@SWGLMISSIONS_BESPIN_STREETS_DESC"},
	{ "@SWGLMISSIONS_BESPIN_PLATFORM",		"13",			"levelshots_sav/bespin_platform", "bespin_platform", "@SWGLMISSIONS_BESPIN_PLATFORM_DESC"},
	{ "@SWGLMISSIONS_CAIRN_BAY",		"14",			"levelshots_sav/cairn_bay", "cairn_bay", "@SWGLMISSIONS_CAIRN_BAY_DESC"},
	{ "@SWGLMISSIONS_CAIRN_ASSEMBLY",		"15",			"levelshots_sav/cairn_assembly", "cairn_assembly", "@SWGLMISSIONS_CAIRN_ASSEMBLY_DESC"},
	{ "@SWGLMISSIONS_CAIRN_REACTOR",	"16",				"levelshots_sav/cairn_reactor", "cairn_reactor", "@SWGLMISSIONS_CAIRN_REACTOR_DESC"},
	{ "@SWGLMISSIONS_CAIRN_DOCK1",		"17",			"levelshots_sav/cairn_dock1", "cairn_dock1", "@SWGLMISSIONS_CAIRN_DOCK1_DESC"},
	{ "@SWGLMISSIONS_DOOM_COMM",		"18",			"levelshots_sav/doom_comm", "doom_comm", "@SWGLMISSIONS_DOOM_COMM_DESC"},
	{ "@SWGLMISSIONS_DOOM_DETENTION",	"19",				"levelshots_sav/doom_detention", "doom_detention", "@SWGLMISSIONS_DOOM_DETENTION_DESC"},
	{ "@SWGLMISSIONS_DOOM_SHIELDS",		"20",			"levelshots_sav/doom_shields", "doom_shields", "@SWGLMISSIONS_DOOM_SHIELDS_DESC"},
	{ "@SWGLMISSIONS_YAVIN_SWAMP",		"21",			"levelshots_sav/yavin_swamp", "yavin_swamp", "@SWGLMISSIONS_YAVIN_SWAMP_DESC"},
	{ "@SWGLMISSIONS_YAVIN_CANYON",			"22",		"levelshots_sav/yavin_canyon", "yavin_canyon", "@SWGLMISSIONS_YAVIN_CANYON_DESC"},
	{ "@SWGLMISSIONS_YAVIN_COURTYARD",	"23",				"levelshots_sav/yavin_courtyard", "yavin_courtyard", "@SWGLMISSIONS_YAVIN_COURTYARD_DESC"},
	{ "@SWGLMISSIONS_YAVIN_FINAL",		"24",			"levelshots_sav/yavin_final", "yavin_final", "@SWGLMISSIONS_YAVIN_FINAL_DESC"},
	{ "@SWGLMISSIONS_JODEMO",			"25",		"levelshots_sav/jodemo", "jodemo", "@SWGLMISSIONS_JODEMO_DESC"},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},
// Jedi Academy
{
	{ "@SWGLMISSIONS_T1_YAVIN1_TITLE",		"0",			"levelshots/yavin1b", "yavin1b", "@SWGLMISSIONS_T1_YAVIN1_DESC"},
	{ "@SWGLMISSIONS_T1_YAVIN2_TITLE",		"1",			"levelshots/yavin2", "yavin2", "@SWGLMISSIONS_T1_YAVIN2_DESC"},
	{ "@SWGLMISSIONS_T1_SOUR_TITLE",		"2",			"levelshots/t1_sour", "t1_sour", "@SWGLMISSIONS_T1_SOUR_DESC"},
	{ "@SWGLMISSIONS_T1_SURPRISE_TITLE",	"3",				"levelshots/t1_surprise", "t1_surprise", "@SWGLMISSIONS_T1_SURPRISE_DESC"},
	{ "@SWGLMISSIONS_T1_RAIL_TITLE",		"4",			"levelshots/t1_rail", "t1_rail", "@SWGLMISSIONS_T1_RAIL_DESC"},
	{ "@SWGLMISSIONS_T1_DANGER_TITLE",		"5",			"levelshots/t1_danger", "t1_danger", "@SWGLMISSIONS_T1_DANGER_DESC"},
	{ "@SWGLMISSIONS_T1_FATAL_TITLE",		"6",			"levelshots/t1_fatal", "t1_fatal", "@SWGLMISSIONS_T1_FATAL_DESC"},
	{ "@SWGLMISSIONS_T1_HOTH2_TITLE",		"7",			"levelshots/hoth2", "hoth2", "@SWGLMISSIONS_T1_HOTH2_DESC"},
	{ "@SWGLMISSIONS_T1_HOTH3_TITLE",		"8",			"levelshots/hoth3", "hoth3", "@SWGLMISSIONS_T1_HOTH3_DESC"},
	{ "@SWGLMISSIONS_T2_RANCOR_TITLE",		"9",			"levelshots/t2_rancor", "t2_rancor", "@SWGLMISSIONS_T2_RANCOR_DESC"},
	{ "@SWGLMISSIONS_T2_TRIP_TITLE",		"10",			"levelshots/t2_trip", "t2_trip", "@SWGLMISSIONS_T2_TRIP_DESC"},
	{ "@SWGLMISSIONS_T2_WEDGE_TITLE",		"11",			"levelshots/t2_wedge", "t2_wedge", "@SWGLMISSIONS_T2_WEDGE_DESC"},
	{ "@SWGLMISSIONS_T2_ROGUE_TITLE",		"12",			"levelshots/t2_rogue", "t2_rogue", "@SWGLMISSIONS_T2_ROGUE_DESC"},
	{ "@SWGLMISSIONS_T2_DPRED_TITLE",		"13",			"levelshots/t2_dpred", "t2_dpred", "@SWGLMISSIONS_T2_DPRED_DESC"},
	{ "@SWGLMISSIONS_T2_VJUN1_TITLE",		"14",			"levelshots/vjun1", "vjun1", "@SWGLMISSIONS_T2_VJUN1_DESC"},
	{ "@SWGLMISSIONS_T2_VJUN2_TITLE",		"15",			"levelshots/vjun2", "vjun2", "@SWGLMISSIONS_T2_VJUN2_DESC"},
	{ "@SWGLMISSIONS_T2_VJUN3_TITLE",		"16",			"levelshots/vjun3", "vjun3", "@SWGLMISSIONS_T2_VJUN3_DESC"},
	{ "@SWGLMISSIONS_T3_RIFT_TITLE",		"17",			"levelshots/t3_rift", "t3_rift", "@SWGLMISSIONS_T3_RIFT_DESC"},
	{ "@SWGLMISSIONS_T3_STAMPEDE_TITLE",	"18",				"levelshots/t3_stamp", "t3_stamp", "@SWGLMISSIONS_T3_STAMPEDE_DESC"},
	{ "@SWGLMISSIONS_T3_HEVIL_TITLE",		"19",			"levelshots/t3_hevil", "t3_hevil", "@SWGLMISSIONS_T3_HEVIL_DESC"},
	{ "@SWGLMISSIONS_T3_BYSS_TITLE",		"20",			"levelshots/t3_byss", "t3_byss", "@SWGLMISSIONS_T3_BYSS_DESC"},
	{ "@SWGLMISSIONS_T3_BOUNTY_TITLE",		"21",			"levelshots/t3_bounty", "t3_bounty", "@SWGLMISSIONS_T3_BOUNTY_DESC"},
	{ "@SWGLMISSIONS_T3_TASPIR1_TITLE",		"22",		"levelshots/taspir1", "taspir1", "@SWGLMISSIONS_T3_TASPIR1_DESC"},
	{ "@SWGLMISSIONS_T3_TASPIR2_TITLE",		"23",		"levelshots/taspir2", "taspir2", "@SWGLMISSIONS_T3_TASPIR2_DESC"},
	{ "@SWGLMISSIONS_T3_KOR1_TITLE",		"24",			"levelshots/kor1", "kor1", "@SWGLMISSIONS_T3_KOR1_DESC"},
	{ "@SWGLMISSIONS_T3_KOR2_TITLE",		"25",			"levelshots/kor2", "kor2", "@SWGLMISSIONS_T3_KOR2_DESC"},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},
// Challenges
{
	{ "@SWGLMISSIONS_CHALLENGES_EP1_HANGAR",		"0",			"SV_EP1_HANGAR", "sv_ep1_hangar", "@SWGLMISSIONS_CHALLENGES_EP1_HANGAR_DESC"},
	{ "@SWGLMISSIONS_CHALLENGES_EP2_TUSKENCAMP",		"1",			"SV_EP2_TUSKENCAMP", "sv_ep2_tuskencamp", "@SWGLMISSIONS_CHALLENGES_EP2_TUSKENCAMP_DESC"},
	{ "@SWGLMISSIONS_CHALLENGES_EP3_DUEL_ROOM",		"2",			"SV_EP3_DUEL_ROOM", "sv_ep3_duel_room", "@SWGLMISSIONS_CHALLENGES_EP3_DUEL_ROOM_DESC"},
	{ "@SWGLMISSIONS_CHALLENGES_EP3_MYGEETO",		"3",			"SV_EP3_MYGEETO", "sv_ep3_mygeeto", "@SWGLMISSIONS_CHALLENGES_EP3_MYGEETO_DESC"},
	{ "@SWGLMISSIONS_CHALLENGES_EP4_DS_HANGAR",		"4",			"SV_EP4_DS_HANGAR", "sv_ep4_ds_hangar", "@SWGLMISSIONS_CHALLENGES_EP4_DS_HANGAR_DESC"},
	{ "@SWGLMISSIONS_CHALLENGES_EP5_ECHO_BASE",		"5",			"SV_EP5_ECHO_BASE", "sv_ep5_echo_base", "@SWGLMISSIONS_CHALLENGES_EP5_ECHO_BASE_DESC"},
	{ "@SWGLMISSIONS_CHALLENGES_EP6_DS2_HANGAR",		"6",			"SV_EP6_DS2_HANGAR", "sv_ep6_ds2_hangar", "@SWGLMISSIONS_CHALLENGES_EP6_DS2_HANGAR_DESC"},
	{ "@SWGLMISSIONS_CHALLENGES_JKA_VJUN",		"7",			"SV_JKA_VJUN", "sv_jka_vjun", "@SWGLMISSIONS_CHALLENGES_JKA_VJUN_DESC"},
	{ "@SWGLMISSIONS_CHALLENGES_JKO_VOTJ",		"8",			"SV_JKO_VOTJ", "sv_jko_votj", "@SWGLMISSIONS_CHALLENGES_JKO_VOTJ_DESC"},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},

// Extras
{
	{ "@SWGLMISSIONS_EXTRAS_EP3_OK",		"0",			"EP3_ORK", "ep3_ok_anakin_1", "@SWGLMISSIONS_EXTRAS_EP3_OK_DESC"},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
	{ NULL,			NULL,		NULL, NULL, NULL, qfalse},
},
};


stringID_table_t SaberStyleTable[] =
{
	{ "NULL",SS_NONE },
	ENUM2STRING(SS_FAST),
	{ "fast",SS_FAST },
	ENUM2STRING(SS_MEDIUM),
	{ "medium",SS_MEDIUM },
	ENUM2STRING(SS_STRONG),
	{ "strong",SS_STRONG },
	ENUM2STRING(SS_DESANN),
	{ "desann",SS_DESANN },
	ENUM2STRING(SS_TAVION),
	{ "tavion",SS_TAVION },
	ENUM2STRING(SS_DUAL),
	{ "dual",SS_DUAL },
	ENUM2STRING(SS_STAFF),
	{ "staff",SS_STAFF },
	{ "", 0 },
};

static int gamecodetoui[] = {4,2,3,0,5,1,6};

uiInfo_t uiInfo;

static fileHandle_t	characterfile;

qboolean openedAngles;

const char* npcCode;

const char* missionCode;

int weaponOne;
int weaponTwo;
int weaponThree;
int weaponFour;
int weaponFive;

int saberStyles;

static int forcePowerRecord[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

static void UI_RegisterCvars( void );
void UI_Load(void);

static int UI_GetScreenshotFormatForString( const char *str ) {
	if ( !Q_stricmp(str, "jpg") || !Q_stricmp(str, "jpeg") )
		return SSF_JPEG;
	else if ( !Q_stricmp(str, "tga") )
		return SSF_TGA;
	else if ( !Q_stricmp(str, "png") )
		return SSF_PNG;
	else
		return -1;
}

static const char *UI_GetScreenshotFormatString( int format )
{
	switch ( format )
	{
	default:
	case SSF_JPEG:
		return "jpg";
	case SSF_TGA:
		return "tga";
	case SSF_PNG:
		return "png";
	}
}

typedef struct cvarTable_s {
	vmCvar_t	*vmCvar;
	const char		*cvarName;
	const char		*defaultString;
	void		(*update)( void );
	uint32_t	cvarFlags;
} cvarTable_t;

vmCvar_t	ui_menuFiles;
vmCvar_t	ui_hudFiles;

vmCvar_t	ui_char_anim;
vmCvar_t	ui_char_model;
vmCvar_t	ui_char_skin_head;
vmCvar_t	ui_char_skin_torso;
vmCvar_t	ui_char_skin_legs;
vmCvar_t	ui_saber_type;
vmCvar_t	ui_saber;
vmCvar_t	ui_saber2;
vmCvar_t	ui_saber_color;
vmCvar_t	ui_saber2_color;
vmCvar_t	ui_lightning_color;
vmCvar_t	ui_edit_saber;
vmCvar_t	ui_char_color_red;
vmCvar_t	ui_char_color_green;
vmCvar_t	ui_char_color_blue;
vmCvar_t	ui_PrecacheModels;
vmCvar_t	ui_screenshotType;
vmCvar_t	ui_npc_faction;
vmCvar_t	ui_npc_custom;

vmCvar_t	ui_npc_type;
vmCvar_t	ui_npc_skin;
vmCvar_t	ui_npc_saberone;
vmCvar_t	ui_npc_saberonecolor;
vmCvar_t	ui_npc_sabertwo;
vmCvar_t	ui_npc_sabertwocolor;
vmCvar_t	ui_npc_weapon;
vmCvar_t	ui_npc_weapon_label;
vmCvar_t	ui_npc_random_weapon;
vmCvar_t	ui_npc_spawnscript;
vmCvar_t	ui_npc_fleescript;
vmCvar_t	ui_npc_deathscript;
vmCvar_t	ui_npc_targetname;
vmCvar_t	ui_char_moveset;
vmCvar_t	ui_char_preview_stance;
vmCvar_t	ui_char_preview_stance_label;
vmCvar_t	ui_char_aim[4];	// "pitch roll" in degrees of the torso of the preview, by weapon class (see uiPreviewAims)
vmCvar_t	ui_npc_menu;
vmCvar_t	ui_mission_topic;
vmCvar_t	ui_mission;
vmCvar_t	ui_mission_code;
vmCvar_t	ui_mission_mapcode;

vmCvar_t	ui_variant_code;
vmCvar_t	ui_team;
vmCvar_t	ui_health;
vmCvar_t	ui_shield;
vmCvar_t	ui_force;

// New cvars for the cheats affichage
vmCvar_t ui_cheats_godMode;
vmCvar_t ui_cheats_noForce;
vmCvar_t ui_cheats_undying;
vmCvar_t ui_cheats_noTarget;
vmCvar_t ui_cheats_noClip;
vmCvar_t ui_cheats_health;
vmCvar_t ui_cheats_shield;
vmCvar_t ui_cheats_force;
vmCvar_t ui_cheats_force_regen_rate;
vmCvar_t ui_cheats_force_regen_amount;



// Force Power cvars (I literally don't know any other way to do this.....)
vmCvar_t	ui_jump_level;
vmCvar_t	ui_push_level;
vmCvar_t	ui_pull_level;
vmCvar_t	ui_speed_level;
vmCvar_t	ui_seeing_level;

vmCvar_t	ui_absorb_level;
vmCvar_t	ui_heal_level;
vmCvar_t	ui_protect_level;
vmCvar_t	ui_mindtrick_level;
vmCvar_t	ui_stasis_level;
vmCvar_t	ui_grasp_level;
vmCvar_t	ui_blast_level;

vmCvar_t	ui_grip_level;
vmCvar_t	ui_lightning_level;
vmCvar_t	ui_drain_level;
vmCvar_t	ui_rage_level;
vmCvar_t	ui_destruction_level;
vmCvar_t	ui_fear_level;
vmCvar_t	ui_strike_level;

vmCvar_t	ui_saboff_level;
vmCvar_t	ui_sabdef_level;
vmCvar_t	ui_sabthrow_level;

vmCvar_t	ui_saber_styles;

vmCvar_t	ui_saber_edit;
vmCvar_t    g_disabledAttributes;
vmCvar_t    ui_selected_attribute;

// New per-slot weapon cvars
vmCvar_t	ui_weaponone;
vmCvar_t	ui_weapontwo;
vmCvar_t	ui_weaponthree;
vmCvar_t	ui_weaponfour;
vmCvar_t	ui_weaponfive;
vmCvar_t	ui_weaponsix;
vmCvar_t	ui_weaponone_label;
vmCvar_t	ui_weapontwo_label;
vmCvar_t	ui_weaponthree_label;
vmCvar_t	ui_weaponfour_label;
vmCvar_t	ui_weaponfive_label;
vmCvar_t	ui_weaponsix_label;

// Model angle slider
vmCvar_t	ui_char_model_angle;

static void UI_UpdateScreenshot( void )
{
	qboolean changed = qfalse;
	// check some things
	if ( ui_screenshotType.string[0] && isalpha( ui_screenshotType.string[0] ) )
	{
		int ssf = UI_GetScreenshotFormatForString( ui_screenshotType.string );
		if ( ssf == -1 )
		{
			ui.Printf( "UI Screenshot Format Type '%s' unrecognised, defaulting to JPEG\n", ui_screenshotType.string );
			uiInfo.uiDC.screenshotFormat = SSF_JPEG;
			changed = qtrue;
		}
		else
			uiInfo.uiDC.screenshotFormat = ssf;
	}
	else if ( ui_screenshotType.integer < SSF_JPEG || ui_screenshotType.integer > SSF_PNG )
	{
		ui.Printf( "ui_screenshotType %i is out of range, defaulting to 0 (JPEG)\n", ui_screenshotType.integer );
		uiInfo.uiDC.screenshotFormat = SSF_JPEG;
		changed = qtrue;
	}
	else {
		uiInfo.uiDC.screenshotFormat = atoi( ui_screenshotType.string );
		changed = qtrue;
	}

	if ( changed ) {
		Cvar_Set( "ui_screenshotType", UI_GetScreenshotFormatString( uiInfo.uiDC.screenshotFormat ) );
		Cvar_Update( &ui_screenshotType );
	}
}

vmCvar_t	ui_rgb_saber_red;
vmCvar_t	ui_rgb_saber_green;
vmCvar_t	ui_rgb_saber_blue;

vmCvar_t	ui_rgb_saber2_red;
vmCvar_t	ui_rgb_saber2_green;
vmCvar_t	ui_rgb_saber2_blue;

vmCvar_t	ui_SFXSabers;
vmCvar_t	ui_SFXSabersGlowSize;
vmCvar_t	ui_SFXSabersCoreSize;

vmCvar_t r_ratioFix;
static void UI_Set2DRatio(void) {
	if (r_ratioFix.integer)
		uiInfo.uiDC.widthRatioCoef = (float)(SCREEN_WIDTH * uiInfo.uiDC.glconfig.vidHeight) / (float)(SCREEN_HEIGHT * uiInfo.uiDC.glconfig.vidWidth);
	else
		uiInfo.uiDC.widthRatioCoef = 1.0f;
}

static cvarTable_t cvarTable[] =
{
	{ &ui_menuFiles,			"ui_menuFiles",			"ui/menus.txt", NULL, CVAR_ARCHIVE },
#ifdef JK2_MODE
	{ &ui_hudFiles,				"cg_hudFiles",			"ui/jk2hud.txt", NULL, CVAR_ARCHIVE},
#else
	{ &ui_hudFiles,				"cg_hudFiles",			"ui/jahud.txt", NULL, CVAR_ARCHIVE},
#endif

	{ &ui_char_anim,			"ui_char_anim",			"BOTH_WALK1", NULL, 0},

	{ &ui_char_model,			"ui_char_model",		"", NULL, 0},	//these are filled in by the "g_*" versions on load
	{ &ui_char_skin_head,		"ui_char_skin_head",	"", NULL, 0},	//the "g_*" versions are initialized in UI_Init, ui_atoms.cpp
	{ &ui_char_skin_torso,		"ui_char_skin_torso",	"", NULL, 0},
	{ &ui_char_skin_legs,		"ui_char_skin_legs",	"", NULL, 0},

	{ &ui_saber_type,			"ui_saber_type",		"", NULL, 0},
	{ &ui_saber,				"ui_saber",				"", NULL, 0},
	{ &ui_saber2,				"ui_saber2",			"", NULL, 0},
	{ &ui_saber_color,			"ui_saber_color",		"", NULL, 0},
	{ &ui_saber2_color,			"ui_saber2_color",		"", NULL, 0},

	{ &ui_char_color_red,		"ui_char_color_red",	"255", NULL, 0},
	{ &ui_char_color_green,		"ui_char_color_green",	"255", NULL, 0},
	{ &ui_char_color_blue,		"ui_char_color_blue",	"255", NULL, 0},

	{ &ui_PrecacheModels,		"ui_PrecacheModels",	"1", NULL, CVAR_ARCHIVE},

	{ &ui_screenshotType,		"ui_screenshotType",	"jpg", UI_UpdateScreenshot, CVAR_ARCHIVE },

	{ &ui_rgb_saber_red,		"ui_rgb_saber_red",	"", 0},
	{ &ui_rgb_saber_blue,		"ui_rgb_saber_blue",	"", 0},
	{ &ui_rgb_saber_green,		"ui_rgb_saber_green",	"", 0},
	{ &ui_rgb_saber2_red,		"ui_rgb_saber2_red",	"", 0},
	{ &ui_rgb_saber2_blue,		"ui_rgb_saber2_blue",	"", 0},
	{ &ui_rgb_saber2_green,		"ui_rgb_saber2_green",	"", 0},
	{ &ui_npc_faction,			"ui_npc_faction",		"tor", NULL, 0},
	{ &ui_npc_custom,			"ui_npc_custom",		"variants", NULL, 0},

	{ &ui_SFXSabers,	"cg_SFXSabers",	"1", NULL, CVAR_ARCHIVE },
	{ &ui_SFXSabersGlowSize,	"cg_SFXSabersGlowSize",	"1.0", NULL, CVAR_ARCHIVE },
	{ &ui_SFXSabersCoreSize,	"cg_SFXSabersCoreSize",	"1.0", NULL, CVAR_ARCHIVE },
	{ &ui_npc_type,				"ui_npc_type",	"stormtrooper", NULL, CVAR_ARCHIVE},
	{ &ui_npc_saberone,			"ui_npc_saberone",	"single_1", NULL, CVAR_ARCHIVE},
	{ &ui_npc_saberonecolor,	"ui_npc_saberonecolor",	"red", NULL, CVAR_ARCHIVE},
	{ &ui_npc_sabertwo,			"ui_npc_sabertwo",	"single_1", NULL, CVAR_ARCHIVE},
	{ &ui_npc_sabertwocolor,	"ui_npc_sabertwocolor",	"red", NULL, CVAR_ARCHIVE},
	{ &ui_npc_weapon,			"ui_npc_weapon",	"WP_BLASTER", NULL, CVAR_ARCHIVE},
	{ &ui_npc_weapon_label,			"ui_npc_weapon_label",	"Blaster", NULL, CVAR_ARCHIVE},
	{ &ui_npc_random_weapon,	"ui_npc_random_weapon",	"weapon_clonerandom", NULL, CVAR_ARCHIVE},	// random weapon of the NPC selector, set by npcSetRandomWeapon
	{ &ui_lightning_color,		"ui_lightning_color",	"blue", NULL, CVAR_ARCHIVE},
	{ &ui_npc_spawnscript,		"ui_npc_spawnscript",	"none", NULL, 0},
	{ &ui_npc_fleescript,		"ui_npc_fleescript",	"none", NULL, 0},
	{ &ui_npc_deathscript,		"ui_npc_deathscript",	"none", NULL, 0},
	{ &ui_npc_targetname,		"ui_npc_targetname",	"", NULL, 0},
	{ &ui_char_preview_stance,	"ui_char_preview_stance",	"2", NULL, 0},	// saber stance of the character preview: index of uiPreviewStances
	{ &ui_char_preview_stance_label,	"ui_char_preview_stance_label",	"", NULL, 0},
	{ &ui_char_aim[0],		"ui_char_aim_minigun",	"20 0", NULL, 0},
	{ &ui_char_aim[1],		"ui_char_aim_shoulder",	"15 0", NULL, 0},
	{ &ui_char_aim[2],		"ui_char_aim_pistol",	"15 0", NULL, 0},
	{ &ui_char_aim[3],		"ui_char_aim_rifle",	"0 -10", NULL, 0},	// rifles and sniper rifles
	{ &ui_char_moveset,			"ui_char_moveset",		"none", NULL, 0},	// moveset chosen in the character menu: "none", "default" or an override folder
	{ &ui_saber_edit,			"ui_saber_edit",		"0", NULL},
	{ &ui_char_model_angle, "ui_char_model_angle", "180", NULL, 0},
	{ &ui_npc_menu, "ui_npc_menu", "0", NULL, 0},
	{ &ui_mission_topic, "ui_mission_topic", "0", NULL, CVAR_ARCHIVE},
	{ &ui_mission, "ui_mission", "0", NULL, CVAR_ARCHIVE},
	{ &ui_mission_code, "ui_mission_code", "", NULL, CVAR_ARCHIVE},
	{ &ui_mission_mapcode, "ui_mission_mapcode", "", NULL, CVAR_ARCHIVE},
	{ &r_ratioFix, "r_ratioFix", "", UI_Set2DRatio, 0},


	{ &ui_jump_level, "ui_jump_level", "0", NULL, 0 },
	{ &ui_push_level, "ui_push_level", "0", NULL, 0 },
	{ &ui_pull_level, "ui_pull_level", "0", NULL, 0 },
	{ &ui_speed_level, "ui_speed_level", "0", NULL, 0 },
	{ &ui_seeing_level, "ui_sense_level", "0", NULL, 0 },

	{ &ui_absorb_level, "ui_absorb_level", "0", NULL, 0 },
	{ &ui_heal_level, "ui_heal_level", "0", NULL, 0 },
	{ &ui_protect_level, "ui_protect_level", "0", NULL, 0 },
	{ &ui_mindtrick_level, "ui_mindtrick_level", "0", NULL, 0 },
	{ &ui_stasis_level, "ui_stasis_level", "0", NULL, 0 },
	{ &ui_grasp_level, "ui_grasp_level", "0", NULL, 0 },
	{ &ui_blast_level, "ui_blast_level", "0", NULL, 0 },

	{ &ui_grip_level, "ui_grip_level", "0", NULL, 0 },
	{ &ui_lightning_level, "ui_lightning_level", "0", NULL, 0 },
	{ &ui_drain_level, "ui_drain_level", "0", NULL, 0 },
	{ &ui_rage_level, "ui_rage_level", "0", NULL, 0 },
	{ &ui_destruction_level, "ui_destruction_level", "0", NULL, 0 },
	{ &ui_fear_level, "ui_fear_level", "0", NULL, 0 },
	{ &ui_strike_level, "ui_strike_level", "0", NULL, 0 },

	{ &ui_saboff_level, "ui_saboff_level", "0", NULL, 0 },
	{ &ui_sabdef_level, "ui_sabdef_level", "0", NULL, 0 },
	{ &ui_sabthrow_level, "ui_sabthrow_level", "0", NULL, 0 },
	{ &ui_variant_code,			"ui_variant_code",		"default", NULL, 0},
	{ &ui_team,					"ui_team",	"enemy", NULL, CVAR_ARCHIVE},
	{ &ui_health,				"ui_health",	"100", NULL, CVAR_ARCHIVE},
	{ &ui_shield,				"ui_shield",	"100", NULL, CVAR_ARCHIVE},
	{ &ui_force,				"ui_force",	"100", NULL, CVAR_ARCHIVE},
	{ &ui_saber_styles,		"ui_saber_styles", "0", NULL, 0 },

	{ &ui_cheats_godMode,		"ui_cheats_godMode", "0", NULL, CVAR_CHEAT },
	{ &ui_cheats_noForce,		"ui_cheats_noForce", "0", NULL, CVAR_CHEAT },
	{ &ui_cheats_undying,		"ui_cheats_undying", "0", NULL, CVAR_CHEAT },
	{ &ui_cheats_noTarget,		"ui_cheats_noTarget", "0", NULL, CVAR_CHEAT },
	{ &ui_cheats_noClip,		"ui_cheats_noClip", "0", NULL, CVAR_CHEAT },
	{ &ui_cheats_health,		"ui_cheats_health", "100", NULL, CVAR_CHEAT },
	{ &ui_cheats_shield,		"ui_cheats_shield", "100", NULL, CVAR_CHEAT },
	{ &ui_cheats_force,			"ui_cheats_force", "100", NULL, CVAR_CHEAT },
	{ &ui_cheats_force_regen_rate,"ui_cheats_force_regen_rate", "0", NULL, CVAR_CHEAT },
	{ &ui_cheats_force_regen_amount,"ui_cheats_force_regen_amount", "0", NULL, CVAR_CHEAT },

	{ &ui_weaponone, "ui_weaponone",   "WP_NONE", NULL, 0 },
	{ &ui_weapontwo, "ui_weapontwo",   "WP_NONE", NULL, 0 },
	{ &ui_weaponthree, "ui_weaponthree",   "WP_NONE", NULL, 0 },
	{ &ui_weaponfour, "ui_weaponfour",   "WP_NONE", NULL, 0 },
	{ &ui_weaponfive, "ui_weaponfive",   "WP_NONE", NULL, 0 },
	{ &ui_weaponsix, "ui_weaponsix",   "WP_NONE", NULL, 0 },
	{ &ui_weaponone_label, "ui_weaponone_label",   "None", NULL, 0 },
	{ &ui_weapontwo_label, "ui_weapontwo_label",   "None", NULL, 0 },
	{ &ui_weaponthree_label, "ui_weaponthree_label",   "None", NULL, 0 },
	{ &ui_weaponfour_label, "ui_weaponfour_label",   "None", NULL, 0 },
	{ &ui_weaponfive_label, "ui_weaponfive_label",   "None", NULL, 0 },
	{ &ui_weaponsix_label, "ui_weaponsix_label",   "None", NULL, 0 },

	{ &ui_selected_attribute, "ui_selected_attribute","",NULL,0},
	{ &g_disabledAttributes, "g_disabledAttributes","0",NULL,CVAR_ARCHIVE},
};

#define FP_UPDATED_NONE -1
#define NOWEAPON -1

static const size_t cvarTableSize = ARRAY_LEN( cvarTable );

void Text_Paint(float x, float y, float scale, vec4_t color, const char *text, int iMaxPixelWidth, int style, int iFontIndex);
int Key_GetCatcher( void );

#define	UI_FPS_FRAMES	4
void _UI_Refresh( int realtime )
{
	static int index;
	static int	previousTimes[UI_FPS_FRAMES];

	if ( !( Key_GetCatcher() & KEYCATCH_UI ) )
	{
		return;
	}

	extern void SE_CheckForLanguageUpdates(void);
	SE_CheckForLanguageUpdates();

	if ( Menus_AnyFullScreenVisible() )
	{//if not in full screen, don't mess with ghoul2
		//rww - ghoul2 needs to know what time it is even if the client/server are not running
		//FIXME: this screws up the game when you go back to the game...
		re.G2API_SetTime(realtime, 0);
		re.G2API_SetTime(realtime, 1);
	}

	uiInfo.uiDC.frameTime = realtime - uiInfo.uiDC.realTime;
	uiInfo.uiDC.realTime = realtime;

	previousTimes[index % UI_FPS_FRAMES] = uiInfo.uiDC.frameTime;
	index++;
	if ( index > UI_FPS_FRAMES )
	{
		int i, total;
		// average multiple frames together to smooth changes out a bit
		total = 0;
		for ( i = 0 ; i < UI_FPS_FRAMES ; i++ )
		{
			total += previousTimes[i];
		}
		if ( !total )
		{
			total = 1;
		}
		uiInfo.uiDC.FPS = 1000 * UI_FPS_FRAMES / total;
	}

	UI_UpdateCvars();

	if (Menu_Count() > 0)
	{
		// paint all the menus
		Menu_PaintAll();
		// refresh server browser list
//		UI_DoServerRefresh();
		// refresh server status
//		UI_BuildServerStatus(qfalse);
		// refresh find player list
//		UI_BuildFindPlayerList(qfalse);
	}

	// draw cursor
	UI_SetColor( NULL );
	if (Menu_Count() > 0)
	{
		if (uiInfo.uiDC.cursorShow == qtrue)
		{
			UI_DrawHandlePic( uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor);
		}
	}
}

#define MODSBUFSIZE (MAX_MODS * MAX_QPATH)

/*
===============
UI_LoadMods
===============
*/
static void UI_LoadMods() {
	int		numdirs;
	char	dirlist[MODSBUFSIZE];
	char	*dirptr;
	char	*descptr;
	int		i;
	int		dirlen;

	uiInfo.modCount = 0;

	numdirs = FS_GetFileList( "$modlist", "", dirlist, sizeof(dirlist) );
	dirptr  = dirlist;
	for( i = 0; i < numdirs; i++ ) {
		dirlen = strlen( dirptr ) + 1;
		descptr = dirptr + dirlen;
		uiInfo.modList[uiInfo.modCount].modName = String_Alloc(dirptr);
		uiInfo.modList[uiInfo.modCount].modDescr = String_Alloc(descptr);
		dirptr += dirlen + strlen(descptr) + 1;
		uiInfo.modCount++;
		if (uiInfo.modCount >= MAX_MODS) {
			break;
		}
	}
}

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .qvm file
================
*/
extern "C" Q_EXPORT intptr_t QDECL vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  )
{
	return 0;
}



/*
================
Text_PaintChar
================
*/
/*
static void Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader)
{
	float w, h;

	w = width * scale;
	h = height * scale;
	ui.R_DrawStretchPic((int)x, (int)y, w, h, s, t, s2, t2, hShader );	//make the coords (int) or else the chars bleed
}
*/

/*
================
Text_Paint
================
*/
// iMaxPixelWidth is 0 here for no limit (but gets converted to -1), else max printable pixel width relative to start pos
//
void Text_Paint(float x, float y, float scale, vec4_t color, const char *text, int iMaxPixelWidth, int style, int iFontIndex)
{
	if (iFontIndex == 0)
	{
		iFontIndex = uiInfo.uiDC.Assets.qhMediumFont;
	}
	// kludge.. convert JK2 menu styles to SOF2 printstring ctrl codes...
	//
	int iStyleOR = 0;
	switch (style)
	{
//	case  ITEM_TEXTSTYLE_NORMAL:			iStyleOR = 0;break;					// JK2 normal text
//	case  ITEM_TEXTSTYLE_BLINK:				iStyleOR = STYLE_BLINK;break;		// JK2 fast blinking
	case  ITEM_TEXTSTYLE_PULSE:				iStyleOR = STYLE_BLINK;break;		// JK2 slow pulsing
	case  ITEM_TEXTSTYLE_SHADOWED:			iStyleOR = STYLE_DROPSHADOW;break;	// JK2 drop shadow ( need a color for this )
	case  ITEM_TEXTSTYLE_OUTLINED:			iStyleOR = STYLE_DROPSHADOW;break;	// JK2 drop shadow ( need a color for this )
	case  ITEM_TEXTSTYLE_OUTLINESHADOWED:	iStyleOR = STYLE_DROPSHADOW;break;	// JK2 drop shadow ( need a color for this )
	case  ITEM_TEXTSTYLE_SHADOWEDMORE:		iStyleOR = STYLE_DROPSHADOW;break;	// JK2 drop shadow ( need a color for this )
	}

	ui.R_Font_DrawString(	x,		// int ox
							y,		// int oy
							text,	// const char *text
							color,	// paletteRGBA_c c
							iStyleOR | iFontIndex,	// const int iFontHandle
							!iMaxPixelWidth?-1:iMaxPixelWidth,	// iMaxPixelWidth (-1 = none)
							scale,	// const float scale = 1.0f
							uiInfo.uiDC.widthRatioCoef);
}


/*
================
Text_PaintWithCursor
================
*/
// iMaxPixelWidth is 0 here for no-limit
void Text_PaintWithCursor(float x, float y, float scale, vec4_t color, const char *text, int cursorPos, char cursor, int iMaxPixelWidth, int style, int iFontIndex)
{
	Text_Paint(x, y, scale, color, text, iMaxPixelWidth, style, iFontIndex);

	// now print the cursor as well...
	//
	char sTemp[1024];
	int iCopyCount = iMaxPixelWidth > 0 ? Q_min( (int)strlen( text ), iMaxPixelWidth ) : (int)strlen( text );
		iCopyCount = Q_min(iCopyCount, cursorPos);
		iCopyCount = Q_min(iCopyCount,(int)sizeof(sTemp));

	// copy text into temp buffer for pixel measure...
	//
	strncpy(sTemp,text,iCopyCount);
			sTemp[iCopyCount] = '\0';

	int iNextXpos = ui.R_Font_StrLenPixels(sTemp, iFontIndex, scale, uiInfo.uiDC.widthRatioCoef);

	Text_Paint(x+iNextXpos, y, scale, color, va("%c",cursor), iMaxPixelWidth, style|ITEM_TEXTSTYLE_BLINK, iFontIndex);
}


const char *UI_FeederItemText(float feederID, int index, int column, qhandle_t *handle)
{
	*handle = -1;
	if (feederID == 0x30 || feederID == 0x31 || feederID == 0x32 || feederID == 0x33 || feederID == 0x34)
	{
		return UI_Phys_FeederItemText(feederID, index, column);
	}
	if (feederID == FEEDER_SAVEGAMES)
	{
		if (column==0)
		{
			return s_savedata[index].currentSaveFileComments;
		}
		else
		{
			return s_savedata[index].currentSaveFileDateTimeString;
		}
	}
	else if (feederID == FEEDER_MOVES)
	{
		return datapadMoveData[uiInfo.movesTitleIndex][index].title;
	}
	else if (feederID == FEEDER_MISSION_TOPIC)
	{
		return missionTopicData[0][index].title;
	}
	else if (feederID == FEEDER_MISSION)
	{
		return missionData[Cvar_VariableIntegerValue("ui_mission_topic")][index].title;
	}
	else if (feederID == FEEDER_MOVES_TITLES)
	{
		return datapadMoveTitleData[index];
	}
	else if (feederID == FEEDER_PLAYER_SPECIES)
	{
		if (index >= 0 && index < uiInfo.playerSpeciesCount)
		{
			return uiInfo.playerSpecies[index].Name;
		}
	}
	else if (feederID == FEEDER_CHAR_CONFIGS)
	{
		return (index >= 0 && index < uiCharConfigCount) ? uiCharConfigs[index] : "";
	}
	else if (feederID == FEEDER_AMMO)
	{
		return (index >= 0 && index < (int)uiAmmoNames.size()) ? uiAmmoNames[index].c_str() : "";
	}
	else if (feederID == FEEDER_LIGHTEDIT)
	{
		return UI_LightEdit_FeederItemText(index, column);
	}
	else if (feederID == FEEDER_LIGHTEDIT_EMI)
	{
		return UI_LightEdit_EmiFeederItemText(index, column);
	}
	else if (feederID == FEEDER_ANIM_OVERRIDES || feederID == FEEDER_CHAR_MOVESET)
	{
		if (index < 0 || index >= (int)uiAnimOverrides.size())
		{
			return "";
		}
		if (index == 0)
		{
			// In the character menu, "none" keeps the moveset of the .npc, else of the model.
			return feederID == FEEDER_CHAR_MOVESET ? "NPC / model default" : "Model default";
		}
		if (index == 1)
		{
			return "Humanoid";
		}
		// "_plx1" and "plx1" are the same override. SWGL_ANIMATIONS.str gives its display name (key SWGL_ANIMATIONS_<NAME>).
		const char *name = uiAnimOverrides[index].c_str();
		while (*name == '_')
		{
			name++;
		}
		char key[MAX_QPATH];
		Com_sprintf(key, sizeof(key), "SWGL_ANIMATIONS_%s", name);
		Q_strupr(key);
#ifdef JK2_MODE
		const char *text = ui.SP_GetStringTextString(key);
#else
		const char *text = SE_GetString(key);
#endif
		return text[0] ? text : name;
	}
	else if (feederID == FEEDER_LANGUAGES)
	{
#ifdef JK2_MODE
		// FIXME
		return NULL;
#else
		return SE_GetLanguageName( index );
#endif
	}

	else if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount)
		{

			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name;
		}
	}

	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount)
		{
			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name;
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		{
			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name;
		}
	}
	else if (feederID == FEEDER_COLORCHOICES)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount)
		{
			*handle = ui.R_RegisterShaderNoMip( uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader);
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader;
		}
	}
	else if (feederID == FEEDER_MODEL_SKINS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinCount)
		{
			std::string skinString(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Skin[index].name + 6, (strlen(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Skin[index].name) - 6));

			*handle = ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name, skinString.c_str()));
			return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Skin[index].name;
		}
	}
	else if (feederID == FEEDER_MODS)
	{
		if (index >= 0 && index < uiInfo.modCount)
		{
			if (uiInfo.modList[index].modDescr && *uiInfo.modList[index].modDescr)
			{
				return uiInfo.modList[index].modDescr;
			}
			else
			{
				return uiInfo.modList[index].modName;
			}
		}
	}
	else if (feederID == FEEDER_ATTR_DISABLED)
	{
		if (index >= 0 && index < uiInfo.disabledAttributeCount)
		{
			return uiInfo.disabledAttributeList[index];
		}
	}
	else if (feederID == FEEDER_ATTR_ENABLED)
	{
		if (index >= 0 && index < uiInfo.enabledAttributeCount)
		{
			return uiInfo.enabledAttributeList[index];
		}
	}

	return "";
}

qhandle_t UI_FeederItemImage(float feederID, int index)
{
	if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount)
		{
			//return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadIcons[index];
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name));
		}
	}
	else if (feederID == FEEDER_MODEL_SKINS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinCount)
		{
			std::string skinString(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Skin[index].name + 6, (strlen(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Skin[index].name) - 6));
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name, skinString.c_str()));
		}
	}

	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount)
		{
			//return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoIcons[index];
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name));
		}
	}
	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		{
			//return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegIcons[index];
			return ui.R_RegisterShaderNoMip(va("models/players/%s/icon_%s.jpg", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name));
		}
	}
	else if (feederID == FEEDER_COLORCHOICES)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount)
		{
			return ui.R_RegisterShaderNoMip( uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader);
		}
	}
/*	else if (feederID == FEEDER_ALLMAPS || feederID == FEEDER_MAPS)
	{
		int actual;
		UI_SelectedMap(index, &actual);
		index = actual;
		if (index >= 0 && index < uiInfo.mapCount)
		{
			if (uiInfo.mapList[index].levelShot == -1)
			{
				uiInfo.mapList[index].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[index].imageName);
			}
			return uiInfo.mapList[index].levelShot;
		}
	}
*/
	return 0;
}


/*
=================
CreateNextSaveName
=================
*/
static int CreateNextSaveName(char *fileName)
{
	int i;

	// Loop through all the save games and look for the first open name
	for (i=0;i<MAX_SAVELOADFILES;i++)
	{
#ifdef JK2_MODE
		Com_sprintf( fileName, MAX_SAVELOADNAME, "jkii%02d", i );
#else
		Com_sprintf( fileName, MAX_SAVELOADNAME, "jedi_%02d", i );
#endif

		if (!ui.SG_GetSaveGameComment(fileName, NULL, NULL))
		{
			return qtrue;
		}
	}

	return qfalse;
}

/*
===============
UI_DeferMenuScript

Return true if the menu script should be deferred for later
===============
*/
static qboolean UI_DeferMenuScript ( const char **args )
{
	const char* name;

	// Whats the reason for being deferred?
	if (!String_Parse(args, &name))
	{
		return qfalse;
	}

	// Handle the custom cases
	if ( !Q_stricmp ( name, "VideoSetup" ) )
	{
		const char* warningMenuName;
		qboolean	deferred;

		// No warning menu specified
		if ( !String_Parse(args, &warningMenuName) )
		{
			return qfalse;
		}

		// Defer if the video options were modified
		deferred = Cvar_VariableIntegerValue( "ui_r_modified" ) ? qtrue : qfalse;

		if ( deferred )
		{
			// Open the warning menu
			Menus_OpenByName(warningMenuName);
		}

		return deferred;
	}

	return qfalse;
}

/*
===============
UI_RunMenuScript
===============
*/
static qboolean UI_RunMenuScript ( const char **args )
{
	const char *name, *name2,*mapName,*menuName,*warningMenuName;

	if (String_Parse(args, &name))
	{
		if (UI_LightEdit_RunScript(name))
		{
			return qtrue;
		}
		if (Q_stricmpn(name, "phys", 4) == 0 && UI_Phys_Script(name, args))
		{
			return qtrue;
		}
		if (Q_stricmp(name, "resetdefaults") == 0)
		{
			UI_ResetDefaults();
			return qtrue;
		}
		if (Q_stricmp(name, "saveControls") == 0)
		{
			Controls_SetConfig();
			return qtrue;
		}
		if (Q_stricmp(name, "loadAttributes") == 0)
		{
			Attributes_GetConfig();
			return qtrue;
		}
		if (Q_stricmp(name, "saveAttributes") == 0)
		{
			Attributes_SaveConfig();
			return qtrue;
		}
		if (Q_stricmp(name, "disableAllAttributes") == 0)
		{
			Attributes_DisableAll();
			return qtrue;
		}
		if (Q_stricmp(name, "enableAllAttributes") == 0)
		{
			Attributes_EnableAll();
			return qtrue;
		}
		if (Q_stricmp(name, "disableSelectedAttribute") == 0)
		{
			Attributes_DisableSelected();
			return qtrue;
		}
		if (Q_stricmp(name, "enableSelectedAttribute") == 0)
		{
			Attributes_EnableSelected();
			return qtrue;
		}
		if (Q_stricmp(name, "loadControls") == 0)
		{
			Controls_GetConfig();
			return qtrue;
		}
		if (Q_stricmp(name, "clearError") == 0)
		{
			Cvar_Set("com_errorMessage", "");
			return qtrue;
		}
		if (Q_stricmp(name, "ReadSaveDirectory") == 0)
		{
			s_savegame.saveFileCnt = -1;	//force a refresh at drawtime
//			ReadSaveDirectory();
		}
		if (Q_stricmp(name, "loadAuto") == 0)
		{
			Menus_CloseAll();
			ui.Cmd_ExecuteText( EXEC_APPEND, "load auto\n");	//load game menu
			return qtrue;
		}
		if (Q_stricmp(name, "loadgame") == 0)
		{
			if (s_savedata[s_savegame.currentLine].currentSaveFileName)// && (*s_file_desc_field.field.buffer))
			{
				Menus_CloseAll();
				ui.Cmd_ExecuteText( EXEC_APPEND, va("load %s\n", s_savedata[s_savegame.currentLine].currentSaveFileName));
			}
			// after loading a game, the list box (and it's highlight) get's reset back to 0, but currentLine sticks around, so set it to 0 here
			s_savegame.currentLine = 0;
			return qtrue;
		}
		if (Q_stricmp(name, "deletegame") == 0)
		{
			if (s_savedata[s_savegame.currentLine].currentSaveFileName)	// A line was chosen
			{
#ifndef FINAL_BUILD
				ui.Printf( va("%s\n","Attempting to delete game"));
#endif

				ui.Cmd_ExecuteText( EXEC_NOW, va("wipe %s\n", s_savedata[s_savegame.currentLine].currentSaveFileName));

				if( (s_savegame.currentLine>0) && ((s_savegame.currentLine+1) == s_savegame.saveFileCnt) )
				{
					s_savegame.currentLine--;
					// yeah this is a pretty bad hack
					// adjust cursor position of listbox so correct item is highlighted
					UI_AdjustSaveGameListBox( s_savegame.currentLine );
				}

//				ReadSaveDirectory();	//refresh
				s_savegame.saveFileCnt = -1;	//force a refresh at drawtime

			}
			return qtrue;
		}
		if (Q_stricmp(name, "savegame") == 0)
		{
			char fileName[MAX_SAVELOADNAME];
			char description[64];
			// Create a new save game
//			if ( !s_savedata[s_savegame.currentLine].currentSaveFileName)	// No line was chosen
			{
				CreateNextSaveName(fileName);	// Get a name to save to
			}
//			else	// Overwrite a current save game? Ask first.
			{
//				s_savegame.yes.generic.flags	= QMF_HIGHLIGHT_IF_FOCUS;
//				s_savegame.no.generic.flags		= QMF_HIGHLIGHT_IF_FOCUS;

//				strcpy(fileName,s_savedata[s_savegame.currentLine].currentSaveFileName);
//				s_savegame.awaitingSave = qtrue;
//				s_savegame.deletegame.generic.flags	= QMF_GRAYED;	// Turn off delete button
//				break;
			}

			// Save description line
			ui.Cvar_VariableStringBuffer("ui_gameDesc",description,sizeof(description));
			ui.SG_StoreSaveGameComment(description);

			ui.Cmd_ExecuteText( EXEC_APPEND, va("save %s\n", fileName));
			s_savegame.saveFileCnt = -1;	//force a refresh the next time around
			return qtrue;
		}
		if (Q_stricmp(name, "LoadMods") == 0)
		{
			UI_LoadMods();
			return qtrue;
		}
		if (Q_stricmp(name, "RunMod") == 0)
		{
			if (uiInfo.modList[uiInfo.modIndex].modName)
			{
				Cvar_Set( "fs_game", uiInfo.modList[uiInfo.modIndex].modName);
				extern	void FS_Restart( qboolean inPlace = qfalse );
				FS_Restart();
				Cbuf_ExecuteText( EXEC_APPEND, "vid_restart;" );
			}
			return qtrue;
		}
		if (Q_stricmp(name, "Quit") == 0)
		{
			Cbuf_ExecuteText( EXEC_NOW, "quit");
			return qtrue;
		}
		if (Q_stricmp(name, "Controls") == 0)
		{
			Cvar_Set( "cl_paused", "1" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			Menus_CloseAll();
			Menus_ActivateByName("setup_menu2");
			return qtrue;
		}
		if (Q_stricmp(name, "Leave") == 0)
		{
			Cbuf_ExecuteText( EXEC_APPEND, "disconnect\n" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			Menus_CloseAll();
			//Menus_ActivateByName("mainMenu");
			return qtrue;
		}
		if (Q_stricmp(name, "getvideosetup") == 0)
		{
			UI_GetVideoSetup ( );
			return qtrue;
		}
		if (Q_stricmp(name, "updatevideosetup") == 0)
		{
			UI_UpdateVideoSetup ( );
			return qtrue;
		}
		if (Q_stricmp(name, "nextDataPadForcePower") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "dpforcenext\n");
			return qtrue;
		}
		if (Q_stricmp(name, "prevDataPadForcePower") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "dpforceprev\n");
			return qtrue;
		}
		if (Q_stricmp(name, "nextDataPadWeapon") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "dpweapnext\n");
			return qtrue;
		}
		if (Q_stricmp(name, "loadoutSelectBaseWeapon") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "loadoutSelectBaseWeapon\n");
			return qtrue;
		}
		if (Q_stricmp(name, "loadoutSelectWeapon") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "loadoutSelectWeapon\n");
			return qtrue;
		}
		if (Q_stricmp(name, "loadoutSwitchSelectWeapon") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "loadoutSwitchSelectWeapon\n");
			return qtrue;
		}
		if (Q_stricmp(name, "loadoutClearSlot") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "loadoutClearSlot\n");
			return qtrue;
		}
		if (Q_stricmp(name, "loadoutEscape") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "loadoutEscape\n");
			return qtrue;
		}
		if (Q_stricmp(name, "characterButtonClick") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterButtonClick\n");
			return qtrue;
		}
		if (Q_stricmp(name, "characterFactionClick") == 0)
		{
			const char *slot;
			if (String_Parse(args, &slot))
			{
				ui.Cmd_ExecuteText( EXEC_APPEND, va("characterFactionClick %s\n", slot));
			}
			return qtrue;
		}
		if (Q_stricmp(name, "characterFactionPreviousPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterFactionPreviousPage\n");
			return qtrue;
		}
		if (Q_stricmp(name, "characterFactionNextPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterFactionNextPage\n");
			return qtrue;
		}
		if (Q_stricmp(name, "characterVariantClick") == 0)
		{
			const char *slot;
			if (String_Parse(args, &slot))
			{
				ui.Cmd_ExecuteText( EXEC_APPEND, va("characterVariantClick %s\n", slot));
			}
			return qtrue;
		}
		if (Q_stricmp(name, "characterNextPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterNextPage\n");
			return qtrue;
		}
		if (Q_stricmp(name, "characterVariantPreviousPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterVariantPreviousPage\n");
			return qtrue;
		}
		if (Q_stricmp(name, "characterVariantNextPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterVariantNextPage\n");
			return qtrue;
		}
		if (Q_stricmp(name, "characterPreviousPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterPreviousPage\n");
			return qtrue;
		}
		if (Q_stricmp(name, "characterUpdateSearch") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterUpdateSearch\n");
		}
		else if (Q_stricmp(name, "characterTagPreviousPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterTagPreviousPage\n");
		}
		else if (Q_stricmp(name, "characterTagNextPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterTagNextPage\n");
		}
		else if (Q_stricmp(name, "characterToggleTags") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterToggleTags\n");
		}
		else if (Q_stricmp(name, "characterResetSearch") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterResetSearch\n");
		}
		else if (Q_stricmp(name, "characterConfigsRefresh") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterConfigsRefresh\n");
		}
		else if (Q_stricmp(name, "characterBack") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "characterBack\n");
		}
		else if (Q_stricmp(name, "characterEscape") == 0)
		{
			// Escape does the action of the Back button of the shown screen: character screen -> grid, grid -> profile menu.
			menuDef_t *charMenu = Menus_FindByName("IngameSWGLChars");
			const char *backName = !Q_stricmp(Cvar_VariableString("ui_character_screen"), "character") ? "CharacterBack" : "SelectionBack";
			itemDef_t *back = charMenu ? (itemDef_t *)Menu_FindItemByName(charMenu, backName) : NULL;
			if (back && back->action)
			{
				Item_RunScript(back, back->action);
			}
		}
		else if (Q_stricmpn(name, "uiPcWeaponNext", 14) == 0)
		{
			int weaponNumber = atoi(name + 15);
			if (weaponNumber <= 0 || weaponNumber >= 7)
			{
				Com_Printf("Invalid parameter for %s\n", name);
			}
			else
			{
				ui.Cmd_ExecuteText(EXEC_APPEND, name);
			}
		}
		else if (Q_stricmpn(name, "uiPcWeaponPrev", 14) == 0)
		{
			int weaponNumber = atoi(name + 15);
			if (weaponNumber <= 0 || weaponNumber >= 7)
			{
				Com_Printf("Invalid parameter for %s\n", name);
			}
			else
			{
				ui.Cmd_ExecuteText(EXEC_APPEND, name);
			}
		}
		else if (Q_stricmp(name, "uiNpcWeaponNext") == 0)
		{
			//if(key)
			ui.Cmd_ExecuteText( EXEC_APPEND, "uiNpcWeaponNext\n");
		}
		else if (Q_stricmp(name, "uiNpcWeaponPrev") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "uiNpcWeaponPrev\n");
		}
		else if (Q_stricmp(name, "uiNpcWeaponLabelUpd") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "uiNpcWeaponLabelUpd\n");
		}
		else if (Q_stricmp(name, "npcSetRandomWeapon") == 0)
		{
			// The weapon of the NPC is the random weapon of the selector (the loadout menu lists the weapons of the player only).
			Cvar_Set("ui_npc_weapon", Cvar_VariableString("ui_npc_random_weapon"));
		}
		else if (Q_stricmp(name, "loadoutNextPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "loadoutNextPage\n");
		}
		else if (Q_stricmp(name, "loadoutPreviousPage") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "loadoutPreviousPage\n");
		}
		else if (Q_stricmp(name, "prevDataPadWeapon") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "dpweapprev\n");
		}
		else if (Q_stricmp(name, "nextDataPadInventory") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "dpinvnext\n");
		}
		else if (Q_stricmp(name, "prevDataPadInventory") == 0)
		{
			ui.Cmd_ExecuteText( EXEC_APPEND, "dpinvprev\n");
		}
		else if (Q_stricmp(name, "checkvid1data") == 0)		// Warn user data has changed before leaving screen?
		{
			String_Parse(args, &menuName);

			String_Parse(args, &warningMenuName);

			UI_CheckVid1Data(menuName,warningMenuName);
		}
		else if (Q_stricmp(name, "startgame") == 0)
		{
			Menus_CloseAll();
#ifdef JK2_MODE
			ui.Cmd_ExecuteText( EXEC_APPEND, "map kejim_post\n" );
#else
			ui.Cmd_ExecuteText( EXEC_APPEND, "map yavin1\n");
#endif
		}
		else if (Q_stricmp(name, "startmap") == 0)
		{
			Menus_CloseAll();

			String_Parse(args, &mapName);

			ui.Cmd_ExecuteText( EXEC_APPEND, va("maptransition %s\n",mapName));
		}
		else if (Q_stricmp(name, "closeingame") == 0)
		{
			trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
			trap_Key_ClearStates();
			Cvar_Set( "cl_paused", "0" );
			Menus_CloseAll();

			if (1 == Cvar_VariableIntegerValue("ui_missionfailed"))
			{
				Menus_ActivateByName("missionfailed_menu");
				ui.Key_SetCatcher( KEYCATCH_UI );
			}
			else
			{
				Menus_ActivateByName("mainhud");
			}
		}
		else if (Q_stricmp(name, "closedatapad") == 0)
		{
			trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
			trap_Key_ClearStates();
			Cvar_Set( "cl_paused", "0" );
			Menus_CloseAll();
			Menus_ActivateByName("mainhud");

			Cvar_Set( "cg_updatedDataPadForcePower1", "0" );
			Cvar_Set( "cg_updatedDataPadForcePower2", "0" );
			Cvar_Set( "cg_updatedDataPadForcePower3", "0" );
			Cvar_Set( "cg_updatedDataPadObjective", "0" );
		}
		else if (Q_stricmp(name, "closesabermenu") == 0)
		{
			// if we're in the saber menu when creating a character, close this down
			if( !Cvar_VariableIntegerValue( "saber_menu" ) )
			{
				Menus_CloseByName( "saberMenu" );
				Menus_OpenByName( "characterMenu" );
			}
		}
		else if (Q_stricmp(name, "clearmouseover") == 0)
		{
			itemDef_t *item;
			menuDef_t *menu = Menu_GetFocused();

			if (menu)
			{
				const char *itemName;
				String_Parse(args, &itemName);
				item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, itemName);
				if (item)
				{
					item->window.flags &= ~WINDOW_MOUSEOVER;
				}
			}
		}
		else if (Q_stricmp(name, "setMovesListDefault") == 0)
		{
			uiInfo.movesTitleIndex = 2;
		}
		else if (Q_stricmp(name, "resetMovesDesc") == 0)
		{
			menuDef_t *menu = Menu_GetFocused();
			itemDef_t *item;

			if (menu)
			{
				item = (itemDef_s *) Menu_FindItemByName(menu, "item_desc");
				if (item)
				{
					listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;
					if( listPtr )
					{
						listPtr->cursorPos = 0;
						listPtr->startPos = 0;
					}
					item->cursorPos = 0;
				}
			}

		}
		else if (Q_stricmp(name, "resetMovesList") == 0)
		{
			menuDef_t *menu;
			menu = Menus_FindByName("datapadMovesMenu");
			//update saber models
			if (menu)
			{
				itemDef_t *item;
				item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, "character");
				if (item)
				{
					UI_SaberAttachToChar( item );
				}
			}

			Cvar_Set( "ui_move_desc", " " );
		}
//		else if (Q_stricmp(name, "setanisotropicmax") == 0)
//		{
//			r_ext_texture_filter_anisotropic->value;
//		}
		else if (Q_stricmp(name, "setMoveCharacter") == 0)
		{
			itemDef_t *item;
			menuDef_t *menu;
			modelDef_t *modelPtr;
			char skin[128];

			UI_GetCharacterCvars();
			UI_GetSaberCvars();

			uiInfo.movesTitleIndex = 0;

			menu = Menus_FindByName("datapadMovesMenu");

			if (menu)
			{
				item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, "character");
				if (item)
				{
					modelPtr = (modelDef_t*)item->typeData;
					if (modelPtr)
					{
						uiInfo.movesBaseAnim = datapadMoveTitleBaseAnims[uiInfo.movesTitleIndex];
						ItemParse_model_g2anim_go( item, uiInfo.movesBaseAnim );

						uiInfo.moveAnimTime = 0 ;
						DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", modelPtr->g2anim, qtrue);
						Com_sprintf( skin, sizeof( skin ), "models/players/%s/|%s|%s|%s",
															Cvar_VariableString ( "g_char_model"),
															Cvar_VariableString ( "g_char_skin_head"),
															Cvar_VariableString ( "g_char_skin_torso"),
															Cvar_VariableString ( "g_char_skin_legs")
									);

						ItemParse_model_g2skin_go( item, skin );
						UI_SaberAttachToChar( item );
					}
				}
			}
		}
		else if (Q_stricmp(name, "char_weapon") == 0)
		{
		itemDef_t* item;
		menuDef_t* menu;
		modelDef_t* modelPtr;
		char skin[128];

		uiInfo.movesTitleIndex = 0;

		uiInfo.movesBaseAnim = "BOTH_STAND1IDLE1";

		menu = Menus_FindByName("IngameSWGLChars");

		if (menu)
		{
			item = (itemDef_s*)Menu_FindItemByName((menuDef_t*)menu, "character");
			if (item)
			{

				modelPtr = (modelDef_t*)item->typeData;
				if (modelPtr)
				{
					ItemParse_model_g2anim_go(item, uiInfo.movesBaseAnim);
					uiInfo.moveAnimTime = 0;

					Com_sprintf(skin, sizeof(skin), "models/players/%s/|%s|%s|%s",
						Cvar_VariableString("ui_char_model"),
						Cvar_VariableString("ui_char_skin_head"),
						Cvar_VariableString("ui_char_skin_torso"),
						Cvar_VariableString("ui_char_skin_legs")
					);

					// The preview update attaches the weapon and plays its pose.
					uiPreviewSig[0] = 0;
				}
			}
		}
		}
		else if (Q_stricmp(name, "glCustom") == 0)
		{
			Cvar_Set("ui_r_glCustom", "4");
		}
		else if (Q_stricmp(name, "character") == 0)
		{
			UI_UpdateCharacter( qfalse );
		}
		else if (Q_stricmp(name, "characterchanged") == 0)
		{
			UI_UpdateCharacter( qtrue );
		}
		else if (Q_stricmp(name, "char_skin") == 0)
		{
			UI_UpdateCharacterSkin();
		}
		else if (Q_stricmp(name, "char_default_skin") == 0)
		{
			const char* otherSkin;
			String_Parse(args, &otherSkin);
			UI_CharacterDefaultSkin(otherSkin);
		}
		else if (Q_stricmp(name, "saveCharacter") == 0)
		{
			UI_SaveCharacterPowers();
		}
		else if (Q_stricmp(name, "loadCharacter") == 0)
		{
			UI_LoadCharacterCfg();
		}
		else if (Q_stricmp(name, "charConfigRefresh") == 0)
		{
			UI_RefreshCharConfigs(NULL);
		}
		else if (Q_stricmp(name, "animOverrideRefresh") == 0 || Q_stricmp(name, "movesetRefresh") == 0 || Q_stricmp(name, "previewStance") == 0 || Q_stricmp(name, "saberEscape") == 0)
		{
			// The chain of else if is at the depth limit of the compiler: one branch for the four scripts.
			if (Q_stricmp(name, "saberEscape") == 0)
			{
				// Escape in the saber menu: back to the character menu when it was opened from there, else to the game selection.
				if (Cvar_VariableIntegerValue("ui_saber_edit"))
				{
					Cvar_Set("ui_saber_edit", "0");
					Menus_CloseByName("ingameswglsaber");
					UI_RunMenuCommand("uiScript char_weapon");
				}
				else
				{
					Menus_CloseAll();
					Menus_OpenByName("GameSelectionMenu");
				}
			}
			else if (Q_stricmp(name, "previewStance") == 0)
			{
				UI_PreviewNextStance();
			}
			else
			{
				UI_RefreshAnimOverrides((qboolean)(Q_stricmp(name, "animOverrideRefresh") == 0));
			}
		}
		else if (Q_stricmp(name, "ammoRefresh") == 0)
		{
			UI_RefreshAmmoTypes();
		}
		else if (Q_stricmp(name, "charConfigSave") == 0)
		{
			UI_SaveCharConfig();
		}
		else if (Q_stricmp(name, "charConfigLoad") == 0)
		{
			UI_LoadCharConfig();
		}
		else if (Q_stricmp(name, "charConfigLoadNamed") == 0)
		{
			// "My Characters" screen: select the configuration <name> of the current variant and load it.
			const char *configName;
			if (String_Parse(args, &configName))
			{
				UI_RefreshCharConfigs(configName);
				UI_LoadCharConfig();
			}
		}
		else if (Q_stricmp(name, "charConfigDelete") == 0)
		{
			UI_DeleteCharConfig();
		}
		else if (Q_stricmp(name, "resetCharacter") == 0)
		{
			UI_LoadCharacterDefaultCfg();
		}
		else if (Q_stricmp(name, "saber_type") == 0)
		{
			UI_UpdateSaberType();
		}
		else if (Q_stricmp(name, "saber_hilt") == 0)
		{
			UI_UpdateSaberHilt( qfalse );
		}
		else if (Q_stricmp(name, "hilt_change") == 0)
		{
			const char* num;
			String_Parse(args, &num);
			UI_HiltChange(atoi(num));
		}
		/*else if (Q_stricmp(name, "saber_color") == 0)
		{
//			UI_UpdateSaberColor( qfalse );
		}*/
		else if (Q_stricmp(name, "saber2_hilt") == 0)
		{
			UI_UpdateSaberHilt( qtrue );
		}
		/*else if (Q_stricmp(name, "saber2_color") == 0)
		{
//			UI_UpdateSaberColor( qtrue );
		}*/
		else if (Q_stricmp(name, "updatecharcvars") == 0)
		{
			UI_UpdateCharacterCvars();
		}
		else if (Q_stricmp(name, "updatenpccvars") == 0)
		{
			UI_UpdateNPCCvars();
		}
		else if (Q_stricmp(name, "toggleTeamAvailability") == 0)
		{
			UI_UpdateTeamSelect();
		}
		else if (Q_stricmp(name, "getcharcvars") == 0)
		{
			UI_GetCharacterCvars();
		}
		else if (Q_stricmp(name, "random_skin") == 0)
		{
			UI_RandomSkin();
		}
		else if (Q_stricmp(name, "random_rgb") == 0)
		{
			UI_RandomRGB();
		}
		else if (Q_stricmp(name, "applyplayerpowers") == 0)
		{

			const char* str = va("%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s", Cvar_VariableString("ui_heal_level"),
				Cvar_VariableString("ui_jump_level"),
				Cvar_VariableString("ui_speed_level"),
				Cvar_VariableString("ui_push_level"),
				Cvar_VariableString("ui_pull_level"),
				Cvar_VariableString("ui_mindtrick_level"),
				Cvar_VariableString("ui_grip_level"),
				Cvar_VariableString("ui_lightning_level"),
				Cvar_VariableString("ui_sabthrow_level"),
				Cvar_VariableString("ui_sabdef_level"),
				Cvar_VariableString("ui_saboff_level"),
				Cvar_VariableString("ui_rage_level"),
				Cvar_VariableString("ui_protect_level"),
				Cvar_VariableString("ui_absorb_level"),
				Cvar_VariableString("ui_drain_level"),
				Cvar_VariableString("ui_sense_level"),
				Cvar_VariableString("ui_stasis_level"),
				Cvar_VariableString("ui_blast_level"),
				Cvar_VariableString("ui_grasp_level"),
				Cvar_VariableString("ui_destruction_level"),
				Cvar_VariableString("ui_strike_level"),
				Cvar_VariableString("ui_fear_level"));

			const char** powerArgs = &str;

			UI_RecordForcePowers(powerArgs);
			UI_ApplyForcePowers();
		}
		else if (Q_stricmp(name, "savePage") == 0)
		{
			const char* page;
			String_Parse(args, &page);
			menuDef_t* savedMenu = Menu_GetFocused();
			savedMenu->savedPage = page;
		}
		else if (Q_stricmp(name, "goToSavedPage") == 0)
		{
			menuDef_t* menu = Menu_GetFocused();

			if (menu->savedPage)
			{
				Menu_ShowItemByName(menu, menu->savedPage, qtrue);
			}
			else
			{
				Menu_ShowItemByName(menu, "Page1", qtrue);
			}
		}
		else if (Q_stricmp(name, "getcharcustom") == 0)
		{
			UI_GetCharacterCustomization();
		}
		else if (Q_stricmp(name, "updatesabercvars") == 0)
		{
			UI_UpdateSaberCvars();
		}
		else if (Q_stricmp(name, "rgbsabercvars") == 0)
		{
			UI_RGBSaberCvars();
		}
		else if (Q_stricmp(name, "getsabercvars") == 0)
		{
			UI_GetSaberCvars();
		}
		else if (Q_stricmp(name, "resetsabercvardefaults") == 0)
		{
			// NOTE : ONLY do this if saber menu is set properly (ie. first time we enter this menu)
			if( !Cvar_VariableIntegerValue( "saber_menu" ) )
			{
				UI_ResetSaberCvars();
			}
    	}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "updatefightingstylechoices") == 0)
		{
			UI_UpdateFightingStyleChoices();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "initallocforcepower") == 0)
		{
			const char *forceName;
			String_Parse(args, &forceName);

			UI_InitAllocForcePowers(forceName);
		}
		else if (Q_stricmp(name, "initallocsaberstyle") == 0)
		{
			const char *saberStyle;
			String_Parse(args, &saberStyle);

			UI_InitAllocSaberStyle(saberStyle);
		}
		else if (Q_stricmp(name, "switchsaberstyle") == 0)
		{
			const char *saberStyle;
			String_Parse(args, &saberStyle);

			UI_SwitchSaberStyle(saberStyle);
		}
		else if (Q_stricmp(name, "cmd_god") == 0)
		{
			UI_Cheats("god");
		}
		else if (Q_stricmp(name, "cmd_noclip") == 0)
		{
			UI_Cheats("noclip");
		}
		else if (Q_stricmp(name, "cmd_notarget") == 0)
		{
			UI_Cheats("notarget");
		}
		else if (Q_stricmp(name, "cmd_undying") == 0)
		{
			UI_Cheats("undying");
		}
		else if (Q_stricmp(name, "cmd_noforce") == 0)
		{
			UI_Cheats("noforce");
		}
		else if (Q_stricmp(name, "getsaberstyle") == 0)
		{
			UI_GetSaberStyle();
		}
		else if (Q_stricmp(name, "getNPCcode") == 0)
		{
			const char* code;
			String_Parse(args, &code);
			npcCode = code;
		}
		else if (Q_stricmp(name, "applycharacter") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_NOW, va("playermodel %s\n", Cvar_VariableString("ui_npc_type")));
			ui.Cmd_ExecuteText(EXEC_NOW, va("playermodel %s %s %s %s\n", Cvar_VariableString("ui_char_model"), Cvar_VariableString("ui_char_skin_head"), Cvar_VariableString("ui_char_skin_torso"), Cvar_VariableString("ui_char_skin_legs")));
			// "Change team" off: the character keeps the current team.
			if (Cvar_VariableIntegerValue("g_allowAlignmentChange"))
			{
				ui.Cmd_ExecuteText(EXEC_NOW, va("playerteam %s\n", Cvar_VariableString("ui_team")));
			}
			if (Cvar_VariableIntegerValue("g_adoptcharstats") >= 1)
			{
				ui.Cmd_ExecuteText(EXEC_NOW, va("give health %s\n", Cvar_VariableString("ui_health")));
				ui.Cmd_ExecuteText(EXEC_NOW, va("give shield %s\n", Cvar_VariableString("ui_health")));
				ui.Cmd_ExecuteText(EXEC_NOW, va("give force %s\n", Cvar_VariableString("ui_force")));
			}			
			ui.Cmd_ExecuteText(EXEC_NOW, va("lightningColor %s\n", Cvar_VariableString("ui_lightning_color")));
			// "none" keeps the moveset of the .npc or of the model.
			if (Q_stricmp(Cvar_VariableString("ui_char_moveset"), "none"))
			{
				ui.Cmd_ExecuteText(EXEC_NOW, va("animoverride %s\n", Cvar_VariableString("ui_char_moveset")));
			}
		}
		else if (Q_stricmp(name, "anglesesc") == 0)
		{
			UI_AnglesEsc();
		}
		else if (Q_stricmp(name, "openedangles") == 0)
		{
			UI_OpenedAngles();
		}
		else if (Q_stricmp(name, "affectforcepowerlevel") == 0)
		{
			const char *forceName;
			String_Parse(args, &forceName);

			UI_AffectForcePowerLevel(forceName);
		}
		else if (Q_stricmp(name, "decrementcurrentforcepower") == 0)
		{
			UI_DecrementCurrentForcePower();
		}
		else if (Q_stricmp(name, "shutdownforcehelp") == 0)
		{
			UI_ShutdownForceHelp();
		}
		else if (Q_stricmp(name, "forcehelpactive") == 0)
		{
			UI_ForceHelpActive();
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "demosetforcelevels") == 0)
		{
			UI_DemoSetForceLevels();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "recordforcelevels") == 0)
		{
			UI_RecordForceLevels();
		}
		else if (Q_stricmp(name, "recordweapons") == 0)
		{
			UI_RecordWeapons();
		}
		else if (Q_stricmp(name, "showforceleveldesc") == 0)
		{
			const char *forceName;
			String_Parse(args, &forceName);

			UI_ShowForceLevelDesc(forceName);
		}
		else if (Q_stricmp(name, "resetforcelevels") == 0)
		{
			UI_ResetForceLevels();
		}
		else if (Q_stricmp(name, "weaponhelpactive") == 0)
		{
			UI_WeaponHelpActive();
		}
		// initialize weapon selection screen
		else if (Q_stricmp(name, "initweaponselect") == 0)
		{
			UI_InitWeaponSelect();
		}
		else if (Q_stricmp(name, "clearweapons") == 0)
		{
			UI_ClearWeapons();
		}
		else if (Q_stricmp(name, "applysaberstyles") == 0)
		{
			UI_ApplySaberStyles();
		}
		else if (Q_stricmp(name, "applycharweapons") == 0)
		{
			//UI_ApplyCharWeapons();
			ui.Cmd_ExecuteText(EXEC_NOW, va("applycharweapons\n"));
		}
		else if (Q_stricmp(name, "applyforcepowers") == 0)
		{
			UI_ApplyForcePowers();
		}
		else if (Q_stricmp(name, "characterSelect") == 0)
		{
			UI_CharacterSelect(args);
		}
		else if (Q_stricmp(name, "showMissions") == 0)
		{
			menuDef_t* menu = Menu_GetFocused();
			if (!missionData[Cvar_VariableIntegerValue("ui_mission_topic")][0].title)
			{
				Menu_ShowItemByName(menu, "emptyBox", qtrue);
				Menu_ShowItemByName(menu, "missionList", qfalse);
				Menu_ShowItemByName(menu, "noJKO", qfalse);
				Menu_ShowItemByName(menu, "noJKOHelp", qfalse);
				Menu_ShowItemByName(menu, "HowtoJKO", qfalse);
			}
			else if (!Q_stricmp(missionTopicData[0][Cvar_VariableIntegerValue("ui_mission_topic")].title, "@SWGLMISSIONS_MISSIONS_JKJO")
				&& Cvar_VariableIntegerValue("g_validJKO") < 1)
			{
				Menu_ShowItemByName(menu, "noJKO", qtrue);
				Menu_ShowItemByName(menu, "noJKOHelp", qtrue);
				Menu_ShowItemByName(menu, "missionList", qfalse);
				Menu_ShowItemByName(menu, "emptyBox", qfalse);
				Menu_ShowItemByName(menu, "HowtoJKO", qfalse);
			}
			else
			{
				Menu_ShowItemByName(menu, "emptyBox", qfalse);
				Menu_ShowItemByName(menu, "missionList", qtrue);
				Menu_ShowItemByName(menu, "noJKO", qfalse);
				Menu_ShowItemByName(menu, "noJKOHelp", qfalse);
				Menu_ShowItemByName(menu, "HowtoJKO", qfalse);
			}

			menu = NULL;
		}
		else if (Q_stricmp(name, "showMissionInfo") == 0)
		{
			UI_ShowMissionInfo();
		}
		else if (Q_stricmp(name, "StartMission") == 0)
		{
			Menus_CloseAll();

			ui.Cmd_ExecuteText(EXEC_APPEND, va("devmap %s\n", Cvar_VariableString("ui_mission_mapcode")));

		}
		else if (Q_stricmp(name, "stopgamesounds") == 0)
		{
			trap_S_StopSounds();
		}
		else if (Q_stricmp(name, "loadmissionselectmenu") == 0)
		{
			const char *cvarName;
			String_Parse(args, &cvarName);

			if (cvarName)
			{
				UI_LoadMissionSelectMenu(cvarName);
			}
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "calcforcestatus") == 0)
		{
			UI_CalcForceStatus();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "giveweapon") == 0)
		{
			const char *weaponIndex;
			String_Parse(args, &weaponIndex);
			UI_GiveWeapon(atoi(weaponIndex));
		}
		else if (Q_stricmp(name, "equipweapon") == 0)
		{
			const char *weaponIndex;
			String_Parse(args, &weaponIndex);
			UI_EquipWeapon(atoi(weaponIndex));
		}
		else if (Q_stricmp(name, "addweaponselection") == 0)
		{
			const char *weaponIndex;
			String_Parse(args, &weaponIndex);
			if (!weaponIndex)
			{
				return qfalse;
			}

			const char *ammoIndex;
			String_Parse(args, &ammoIndex);
			if (!ammoIndex)
			{
				return qfalse;
			}

			const char *ammoAmount;
			String_Parse(args, &ammoAmount);
			if (!ammoAmount)
			{
				return qfalse;
			}

			const char *itemName;
			String_Parse(args, &itemName);
			if (!itemName)
			{
				return qfalse;
			}

			const char *litItemName;
			String_Parse(args, &litItemName);
			if (!litItemName)
			{
				return qfalse;
			}

			const char *backgroundName;
			String_Parse(args, &backgroundName);
			if (!backgroundName)
			{
				return qfalse;
			}

			const char *soundfile = NULL;
			String_Parse(args, &soundfile);

			UI_AddWeaponSelection(atoi(weaponIndex),atoi(ammoIndex),atoi(ammoAmount),itemName,litItemName, backgroundName, soundfile);
		}
		else if (Q_stricmp(name, "addthrowweaponselection") == 0)
		{
			const char *weaponIndex;
			String_Parse(args, &weaponIndex);
			if (!weaponIndex)
			{
				return qfalse;
			}

			const char *ammoIndex;
			String_Parse(args, &ammoIndex);
			if (!ammoIndex)
			{
				return qfalse;
			}

			const char *ammoAmount;
			String_Parse(args, &ammoAmount);
			if (!ammoAmount)
			{
				return qfalse;
			}

			const char *itemName;
			String_Parse(args, &itemName);
			if (!itemName)
			{
				return qfalse;
			}

			const char *litItemName;
			String_Parse(args, &litItemName);
			if (!litItemName)
			{
				return qfalse;
			}

			const char *backgroundName;
			String_Parse(args, &backgroundName);
			if (!backgroundName)
			{
				return qfalse;
			}

			const char *soundfile;
			String_Parse(args, &soundfile);

			UI_AddThrowWeaponSelection(atoi(weaponIndex),atoi(ammoIndex),atoi(ammoAmount),itemName,litItemName,backgroundName, soundfile);
		}
		else if (Q_stricmp(name, "removeweaponselection") == 0)
		{
			const char *weaponIndex;
			String_Parse(args, &weaponIndex);
			if (weaponIndex)
			{
				UI_RemoveWeaponSelection(atoi(weaponIndex));
			}
		}
		else if (Q_stricmp(name, "removethrowweaponselection") == 0)
		{
			UI_RemoveThrowWeaponSelection();
		}
		else if (Q_stricmp(name, "normalthrowselection") == 0)
		{
			UI_NormalThrowSelection();
		}
		else if (Q_stricmp(name, "highlightthrowselection") == 0)
		{
			UI_HighLightThrowSelection();
		}
		else if (Q_stricmp(name, "normalweaponselection") == 0)
		{
			const char *slotIndex;
			String_Parse(args, &slotIndex);
			if (!slotIndex)
			{
				return qfalse;
			}

			UI_NormalWeaponSelection(atoi(slotIndex));
		}
		else if (Q_stricmp(name, "highlightweaponselection") == 0)
		{
			const char *slotIndex;
			String_Parse(args, &slotIndex);
			if (!slotIndex)
			{
				return qfalse;
			}

			UI_HighLightWeaponSelection(atoi(slotIndex));
		}
		else if (Q_stricmp(name, "clearinventory") == 0)
		{
			UI_ClearInventory();
		}
		else if (Q_stricmp(name, "giveinventory") == 0)
		{
			const char *inventoryIndex,*amount;
			String_Parse(args, &inventoryIndex);
			String_Parse(args, &amount);
			UI_GiveInventory(atoi(inventoryIndex),atoi(amount));
		}
#ifndef JK2_MODE
		else if (Q_stricmp(name, "updatefightingstyle") == 0)
		{
			UI_UpdateFightingStyle();
		}
#endif // !JK2_MODE
		else if (Q_stricmp(name, "update") == 0)
		{
			if (String_Parse(args, &name2))
			{
				UI_Update(name2);
			}
			else
			{
				Com_Printf("update missing cmd\n");
			}
		}
		else if (Q_stricmp(name, "load_quick") == 0)
		{
#ifdef JK2_MODE
			ui.Cmd_ExecuteText(EXEC_APPEND,"load quik\n");
#else
			ui.Cmd_ExecuteText(EXEC_APPEND,"load quick\n");
#endif
		}
		else if (Q_stricmp(name, "load_auto") == 0)
		{
			ui.Cmd_ExecuteText(EXEC_APPEND,"load *respawn\n");	//death menu, might load a saved game instead if they just loaded on this map
		}
		else if (Q_stricmp(name, "decrementforcepowerlevel") == 0)
		{
			UI_DecrementForcePowerLevel();
		}
		else if (Q_stricmp(name, "getmousepitch") == 0)
		{
			Cvar_Set("ui_mousePitch", (trap_Cvar_VariableValue("m_pitch") >= 0) ? "0" : "1");
		}
		else if (Q_stricmp(name, "resetcharacterlistboxes") == 0)
		{
			UI_ResetCharacterListBoxes();
		}
		else if ( Q_stricmp( name, "LaunchMP" ) == 0 )
		{
			// TODO for MAC_PORT, will only be valid for non-JK2 mode
		}
		else
		{
			Com_Printf("unknown UI script %s\n", name);
		}
	}

	return qtrue;
}

/*
=================
UI_GetValue
=================
*/
static float UI_GetValue(int ownerDraw)
{
  return 0;
}

//Force Warnings
enum
{
	FW_VERY_LIGHT = 0,
	FW_SEMI_LIGHT,
	FW_NEUTRAL,
	FW_SEMI_DARK,
	FW_VERY_DARK
};

const char *lukeForceStatusSounds[] =
{
"sound/chars/luke/misc/MLUK_03.mp3",	// Very Light
"sound/chars/luke/misc/MLUK_04.mp3",	// Semi Light
"sound/chars/luke/misc/MLUK_05.mp3",	// Neutral
"sound/chars/luke/misc/MLUK_01.mp3",	// Semi dark
"sound/chars/luke/misc/MLUK_02.mp3",	// Very dark
};

const char *kyleForceStatusSounds[] =
{
"sound/chars/kyle/misc/MKYK_05.mp3",	// Very Light
"sound/chars/kyle/misc/MKYK_04.mp3",	// Semi Light
"sound/chars/kyle/misc/MKYK_03.mp3",	// Neutral
"sound/chars/kyle/misc/MKYK_01.mp3",	// Semi dark
"sound/chars/kyle/misc/MKYK_02.mp3",	// Very dark
};


#ifndef JK2_MODE
static void UI_CalcForceStatus(void)
{
	float		lightSide,darkSide,total;
	short		who, index=FW_VERY_LIGHT;
	qboolean	lukeFlag=qtrue;
	float		percent;
	client_t*	cl = &svs.clients[0];	// 0 because only ever us as a player
	char		value[256];

	if (!cl)
	{
		return;
	}
	playerState_t*		pState = cl->gentity->client;

	if (!cl->gentity || !cl->gentity->client)
	{
		return;
	}

	memset(value, 0, sizeof(value));

	lightSide = pState->forcePowerLevel[FP_HEAL] +
		pState->forcePowerLevel[FP_TELEPATHY] +
		pState->forcePowerLevel[FP_PROTECT] +
		pState->forcePowerLevel[FP_STASIS] +
		pState->forcePowerLevel[FP_GRASP] +
		pState->forcePowerLevel[FP_ABSORB];

	darkSide = pState->forcePowerLevel[FP_GRIP] +
		pState->forcePowerLevel[FP_LIGHTNING] +
		pState->forcePowerLevel[FP_RAGE] +
		pState->forcePowerLevel[FP_DESTRUCTION] +
		pState->forcePowerLevel[FP_FEAR] +
		pState->forcePowerLevel[FP_DRAIN];

	total = lightSide + darkSide;

	percent = lightSide / total;

	who = Q_irand( 0, 100 );
	if (percent >= 0.90f)	//  90 - 100%
	{
		index = FW_VERY_LIGHT;
		if (who <50)
		{
			strcpy(value,"vlk");	// Very light Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value,"vll");	// Very light Luke
		}

	}
	else if (percent > 0.60f )
	{
		index = FW_SEMI_LIGHT;
		if ( who<50 )
		{
			strcpy(value,"slk");	// Semi-light Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value,"sll");	// Semi-light light Luke
		}
	}
	else if (percent > 0.40f )
	{
		index = FW_NEUTRAL;
		if ( who<50 )
		{
			strcpy(value,"ntk");	// Neutral Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value,"ntl");	// Netural Luke
		}
	}
	else if (percent > 0.10f )
	{
		index = FW_SEMI_DARK;
		if ( who<50 )
		{
			strcpy(value,"sdk");	// Semi-dark Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value,"sdl");	// Semi-Dark Luke
		}
	}
	else
	{
		index = FW_VERY_DARK;
		if ( who<50 )
		{
			strcpy(value,"vdk");	// Very dark Kyle
			lukeFlag = qfalse;
		}
		else
		{
			strcpy(value,"vdl");	// Very Dark Luke
		}
	}

	Cvar_Set("ui_forcestatus", value );

	if (lukeFlag)
	{
		DC->startLocalSound(DC->registerSound(lukeForceStatusSounds[index], qfalse), CHAN_VOICE );
	}
	else
	{
		DC->startLocalSound(DC->registerSound(kyleForceStatusSounds[index], qfalse), CHAN_VOICE );
	}
}
#endif // !JK2_MODE

/*
=================
UI_StopCinematic
=================
*/
static void UI_StopCinematic(int handle)
{
	if (handle >= 0)
	{
		trap_CIN_StopCinematic(handle);
	}
	else
	{
		handle = abs(handle);
		if (handle == UI_MAPCINEMATIC)
		{
			// FIXME - BOB do we need this?
//			if (uiInfo.mapList[ui_currentMap.integer].cinematic >= 0)
//			{
//				trap_CIN_StopCinematic(uiInfo.mapList[ui_currentMap.integer].cinematic);
//				uiInfo.mapList[ui_currentMap.integer].cinematic = -1;
//			}
		}
		else if (handle == UI_NETMAPCINEMATIC)
		{
			// FIXME - BOB do we need this?
//			if (uiInfo.serverStatus.currentServerCinematic >= 0)
//			{
//				trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
//				uiInfo.serverStatus.currentServerCinematic = -1;
//			}
		}
		else if (handle == UI_CLANCINEMATIC)
		{
			// FIXME - BOB do we need this?
//			int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
//			if (i >= 0 && i < uiInfo.teamCount)
//			{
//				if (uiInfo.teamList[i].cinematic >= 0)
//				{
//					trap_CIN_StopCinematic(uiInfo.teamList[i].cinematic);
//					uiInfo.teamList[i].cinematic = -1;
//				}
//			}
		}
	}
}
static void UI_HandleLoadSelection()
{
	Cvar_Set("ui_SelectionOK", va("%d",(s_savegame.currentLine < s_savegame.saveFileCnt)) );
	if (s_savegame.currentLine >= s_savegame.saveFileCnt)
		return;
#ifdef JK2_MODE
	Cvar_Set("ui_gameDesc", s_savedata[s_savegame.currentLine].currentSaveFileComments );	// set comment

	if (!ui.SG_GetSaveImage(s_savedata[s_savegame.currentLine].currentSaveFileName, &screenShotBuf))
	{
		memset( screenShotBuf,0,(SG_SCR_WIDTH * SG_SCR_HEIGHT * 4));
	}
#endif
}

/*
* We've opened the angles menu, so we need the escape button to behave a little different
*/
static void UI_OpenedAngles()
{
	// If NPC freeze is already online, we'll assume the player froze the NPCs themselves, so let's not do anything to ruin it.
	if(!Cvar_VariableIntegerValue("d_npcfreeze"))
		openedAngles = qtrue;
}

/*
* If we opened the angles menu, need to turn off NPC freeze, otherwise leave it on/don't change it.
*/
static void UI_AnglesEsc()
{
	if (openedAngles)
	{
		Cvar_Set("d_npcfreeze", "0");
		openedAngles = qfalse;
	}
}

/*
=================
UI_FeederCount
=================
*/
static int UI_FeederCount(float feederID)
{
	if (feederID == 0x30 || feederID == 0x31 || feederID == 0x32 || feederID == 0x33 || feederID == 0x34)
	{
		return UI_Phys_FeederCount(feederID);
	}

	if (feederID == FEEDER_SAVEGAMES )
	{
		if (s_savegame.saveFileCnt == -1)
		{
			ReadSaveDirectory();	//refresh
			UI_HandleLoadSelection();
#ifndef JK2_MODE
			UI_AdjustSaveGameListBox(s_savegame.currentLine);
#endif
		}
		return s_savegame.saveFileCnt;
	}
	// count number of moves for the current title
	else if (feederID == FEEDER_MOVES)
	{
		int count=0,i;

		for (i=0;i<MAX_MOVES;i++)
		{
			if (datapadMoveData[uiInfo.movesTitleIndex][i].title)
			{
				count++;
			}
		}

		return count;
	}
	else if (feederID == FEEDER_MISSION_TOPIC)
	{
		int count = 0, i;

		for (i = 0; i < MAX_MISSION_TOPIC; i++)
		{
			if (missionTopicData[0][i].title)
			{
				count++;
			}
		}

		return count;
	}
	else if (feederID == FEEDER_MISSION)
	{
		int count = 0, i;

		for (i = 0; i < MAX_MISSION; i++)
		{
			if (missionData[Cvar_VariableIntegerValue("ui_mission_topic")][i].title)
			{
				count++;
			}
		}

		return count;
	}
	else if (feederID == FEEDER_MOVES_TITLES)
	{
		return (MD_MOVE_TITLE_MAX);
	}
	else if (feederID == FEEDER_MODS)
	{
		return uiInfo.modCount;
	}
	else if (feederID == FEEDER_ATTR_DISABLED)
	{
		return uiInfo.disabledAttributeCount;
	}
	else if (feederID == FEEDER_ATTR_ENABLED)
	{
		return uiInfo.enabledAttributeCount;
	}
	else if (feederID == FEEDER_LANGUAGES)
	{
		return uiInfo.languageCount;
	}
	else if (feederID == FEEDER_CHAR_CONFIGS)
	{
		return uiCharConfigCount;
	}
	else if (feederID == FEEDER_ANIM_OVERRIDES || feederID == FEEDER_CHAR_MOVESET)
	{
		return (int)uiAnimOverrides.size();
	}
	else if (feederID == FEEDER_AMMO)
	{
		return (int)uiAmmoTypes.size();
	}
	else if (feederID == FEEDER_LIGHTEDIT)
	{
		return UI_LightEdit_FeederCount();
	}
	else if (feederID == FEEDER_LIGHTEDIT_EMI)
	{
		return UI_LightEdit_EmiFeederCount();
	}
	else if (feederID == FEEDER_PLAYER_SPECIES)
	{
		return uiInfo.playerSpeciesCount;
	}
	else if (feederID == FEEDER_MODEL_SKINS)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinCount;
	}
	else if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount;
	}
	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount;
	}
	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount;
	}

	else if (feederID == FEEDER_COLORCHOICES)
	{
		return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount;
	}

	return 0;
}

/*
=================
UI_FeederSelection
=================
*/
// A head, torso or lower skin needs the two other parts: after a full skin (model_*), their cvars
// hold the name of that skin. A part that is not in the list of the species takes its first entry.
static void UI_ValidSkinPart(const char *cvarName, const skinName_t *list, int count)
{
	if (count <= 0)
	{
		return;
	}
	const char *value = Cvar_VariableString(cvarName);
	for (int i = 0; i < count; i++)
	{
		if (!Q_stricmp(list[i].name, value))
		{
			return;
		}
	}
	Cvar_Set(cvarName, list[0].name);
}

static void UI_ValidSkinParts(void)
{
	const playerSpeciesInfo_t *species = &uiInfo.playerSpecies[uiInfo.playerSpeciesIndex];
	UI_ValidSkinPart("ui_char_skin_head", species->SkinHead, species->SkinHeadCount);
	UI_ValidSkinPart("ui_char_skin_torso", species->SkinTorso, species->SkinTorsoCount);
	UI_ValidSkinPart("ui_char_skin_legs", species->SkinLeg, species->SkinLegCount);
}

// No color (empty cvars or 0 0 0): white, the color that does not change the skin.
static void UI_DefaultCharacterColor(void)
{
	if (Cvar_VariableIntegerValue("ui_char_color_red") == 0
		&& Cvar_VariableIntegerValue("ui_char_color_green") == 0
		&& Cvar_VariableIntegerValue("ui_char_color_blue") == 0)
	{
		Cvar_Set("ui_char_color_red", "255");
		Cvar_Set("ui_char_color_green", "255");
		Cvar_Set("ui_char_color_blue", "255");
	}
}

static void UI_FeederSelection(float feederID, int index, itemDef_t *item)
{
	if (UI_Phys_FeederSelection(feederID, index))
	{
		return;
	}

	if (feederID == FEEDER_SAVEGAMES)
	{
		s_savegame.currentLine = index;
		UI_HandleLoadSelection();
	}
	else if (feederID == FEEDER_MOVES)
	{
		itemDef_t *item;
		menuDef_t *menu;
		modelDef_t *modelPtr;
		char skin[MAX_QPATH * 2];

		menu = Menus_FindByName("datapadMovesMenu");

		if (menu)
		{
			item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, "character");
			if (item)
			{
				modelPtr = (modelDef_t*)item->typeData;
				if (modelPtr)
				{
					if (datapadMoveData[uiInfo.movesTitleIndex][index].anim)
					{
						ItemParse_model_g2anim_go( item, datapadMoveData[uiInfo.movesTitleIndex][index].anim );
						uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", modelPtr->g2anim, qtrue);

						uiInfo.moveAnimTime += uiInfo.uiDC.realTime;

						// Play sound for anim
						if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == MDS_FORCE_JUMP)
						{
							DC->startLocalSound(uiInfo.uiDC.Assets.datapadmoveJumpSound, CHAN_LOCAL );
						}
						else if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == MDS_ROLL)
						{
							DC->startLocalSound(uiInfo.uiDC.Assets.datapadmoveRollSound, CHAN_LOCAL );
						}
						else if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == MDS_SABER)
						{
							// Randomly choose one sound
							int soundI = Q_irand( 1, 6 );
							sfxHandle_t *soundPtr;
							soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound1;
							if (soundI == 2)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound2;
							}
							else if (soundI == 3)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound3;
							}
							else if (soundI == 4)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound4;
							}
							else if (soundI == 5)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound5;
							}
							else if (soundI == 6)
							{
								soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound6;
							}

							DC->startLocalSound(*soundPtr, CHAN_LOCAL );
						}

						if (datapadMoveData[uiInfo.movesTitleIndex][index].desc)
						{
							Cvar_Set( "ui_move_desc", datapadMoveData[uiInfo.movesTitleIndex][index].desc);
						}

						Com_sprintf( skin, sizeof( skin ), "models/players/%s/|%s|%s|%s",
															Cvar_VariableString ( "g_char_model"),
															Cvar_VariableString ( "g_char_skin_head"),
															Cvar_VariableString ( "g_char_skin_torso"),
															Cvar_VariableString ( "g_char_skin_legs")
									);

						ItemParse_model_g2skin_go( item, skin );

					}
				}
			}
		}
	}
	else if (feederID == FEEDER_MOVES_TITLES)
	{
		itemDef_t *item;
		menuDef_t *menu;
		modelDef_t *modelPtr;

		uiInfo.movesTitleIndex = index;
		uiInfo.movesBaseAnim = datapadMoveTitleBaseAnims[uiInfo.movesTitleIndex];
		menu = Menus_FindByName("datapadMovesMenu");

		if (menu)
		{
			item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, "character");
			if (item)
			{
				modelPtr = (modelDef_t*)item->typeData;
				if (modelPtr)
				{
					ItemParse_model_g2anim_go( item, uiInfo.movesBaseAnim );
					uiInfo.moveAnimTime = DC->g2hilev_SetAnim(&item->ghoul2[0], "model_root", modelPtr->g2anim, qtrue);
				}
			}
		}
	}
	else if (feederID == FEEDER_MISSION_TOPIC)
	{
		if (index >= 0)
		{
			ui.Cvar_Set("ui_mission_topic", missionTopicData[0][index].value);
		}
	}
	else if (feederID == FEEDER_MISSION)
	{
		if (index >= 0)
		{
			ui.Cvar_Set("ui_mission", missionData[Cvar_VariableIntegerValue("ui_mission_topic")][index].missionNum);
			ui.Cvar_Set("ui_mission_code", missionData[Cvar_VariableIntegerValue("ui_mission_topic")][index].picCode);
			ui.Cvar_Set("ui_mission_mapcode", missionData[Cvar_VariableIntegerValue("ui_mission_topic")][index].mapCode);

			if (Q_stricmp(Cvar_VariableString("ui_mission_mapcode"), "") && Q_stricmp(Cvar_VariableString("ui_mission_mapcode"), NULL))
			{
				menuDef_t* menu;
				menu = Menu_GetFocused();
				Menu_ShowItemByName(menu, "StartButton", qtrue);
				Menu_ShowItemByName(menu, "MissionInfo", qtrue);
				Menu_ShowItemByName(menu, "StartButtonOFF", qfalse);
				menu = NULL;
			}
			else
			{
				menuDef_t* menu;
				menu = Menu_GetFocused();
				Menu_ShowItemByName(menu, "StartButton", qfalse);
				Menu_ShowItemByName(menu, "MissionInfo", qfalse);
				Menu_ShowItemByName(menu, "StartButtonOFF", qtrue);
				menu = NULL;
			}
		}
	}
	else if (feederID == FEEDER_MODS)
	{
		uiInfo.modIndex = index;
	}
	else if (feederID == FEEDER_ATTR_DISABLED)
	{
		char buffer[ATTRIBUTES_SIZE];
		Q_strncpyz(buffer, uiInfo.disabledAttributeList[index], ATTRIBUTES_SIZE);
		ui.Cvar_Set("ui_selected_attribute", buffer);
		uiInfo.disabledAttributeIndex = index;
	}
	else if (feederID == FEEDER_ATTR_ENABLED)
	{
		char buffer[ATTRIBUTES_SIZE];
		Q_strncpyz(buffer, uiInfo.enabledAttributeList[index], ATTRIBUTES_SIZE);
		ui.Cvar_Set("ui_selected_attribute", buffer);
		uiInfo.enabledAttributeIndex = index;
	}
	else if (feederID == FEEDER_PLAYER_SPECIES)
	{
		if (index >= 0 && index < uiInfo.playerSpeciesCount)
		{
			uiInfo.playerSpeciesIndex = index;
		}
	}
	else if (feederID == FEEDER_LANGUAGES)
	{
		uiInfo.languageCountIndex = index;
	}
	else if (feederID == FEEDER_CHAR_CONFIGS)
	{
		// The name goes to the text field: Save with it overwrites this configuration.
		uiCharConfigSelected = index;
		Cvar_Set("ui_char_config_name", index > 0 && index < uiCharConfigCount ? uiCharConfigs[index] : "");
	}
	else if (feederID == FEEDER_ANIM_OVERRIDES)
	{
		if (index >= 0 && index < (int)uiAnimOverrides.size())
		{
			Cvar_Set("ui_animoverride", uiAnimOverrides[index].c_str());
			ui.Cmd_ExecuteText(EXEC_APPEND, va("animoverride %s\n", uiAnimOverrides[index].c_str()));
		}
	}
	else if (feederID == FEEDER_CHAR_MOVESET)
	{
		if (index >= 0 && index < (int)uiAnimOverrides.size())
		{
			Cvar_Set("ui_char_moveset", uiAnimOverrides[index].c_str());
		}
	}
	else if (feederID == FEEDER_AMMO)
	{
		UI_SelectAmmoType(index);
	}
	else if (feederID == FEEDER_LIGHTEDIT)
	{
		UI_LightEdit_FeederSelection(index);
	}
	else if (feederID == FEEDER_LIGHTEDIT_EMI)
	{
		UI_LightEdit_EmiFeederSelection(index);
	}
	else if (feederID == FEEDER_PLAYER_SKIN_HEAD)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount)
		{
			Cvar_Set("ui_char_skin_head", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name);
			UI_ValidSkinParts();
		}
	}
	else if (feederID == FEEDER_MODEL_SKINS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinCount)
		{
			Cvar_Set("ui_char_skin_head", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Skin[index].name);
			Cvar_Set("ui_char_skin_torso", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Skin[index].name);
			Cvar_Set("ui_char_skin_legs", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Skin[index].name);
		}
	}

	else if (feederID == FEEDER_PLAYER_SKIN_TORSO)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount)
		{
			Cvar_Set("ui_char_skin_torso", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name);
			UI_ValidSkinParts();
		}
	}

	else if (feederID == FEEDER_PLAYER_SKIN_LEGS)
	{
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount)
		{
			Cvar_Set("ui_char_skin_legs", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name);
			UI_ValidSkinParts();
		}
	}

	else if (feederID == FEEDER_COLORCHOICES)
	{
extern void	Item_RunScript(itemDef_t *item, const char *s);		//from ui_shared;
		if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount)
		{
			Item_RunScript(item, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].actionText);
		}
	}
/*	else if (feederID == FEEDER_CINEMATICS)
	{
		uiInfo.movieIndex = index;
		if (uiInfo.previewMovie >= 0)
		{
			trap_CIN_StopCinematic(uiInfo.previewMovie);
		}
		uiInfo.previewMovie = -1;
	}
	else if (feederID == FEEDER_DEMOS)
	{
		uiInfo.demoIndex = index;
	}
*/
}

void Key_KeynumToStringBuf( int keynum, char *buf, int buflen );
void Key_GetBindingBuf( int keynum, char *buf, int buflen );
int Key_ComboKeynum( int modifier, int key );

static qboolean UI_Crosshair_HandleKey(int flags, float *special, int key)
{
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER)
  {
		if (key == A_MOUSE2)
		{
			uiInfo.currentCrosshair--;
		} else {
			uiInfo.currentCrosshair++;
		}

		if (uiInfo.currentCrosshair >= NUM_CROSSHAIRS) {
			uiInfo.currentCrosshair = 0;
		} else if (uiInfo.currentCrosshair < 0) {
			uiInfo.currentCrosshair = NUM_CROSSHAIRS - 1;
		}
		Cvar_Set("cg_drawCrosshair", va("%d", uiInfo.currentCrosshair));
		return qtrue;
	}
	return qfalse;
}


static qboolean UI_OwnerDrawHandleKey(int ownerDraw, int flags, float *special, int key)
{
	if (ownerDraw >= UI_PHYS_OWNERDRAW_FIRST && ownerDraw <= UI_PHYS_OWNERDRAW_LAST)
	{
		return UI_Phys_HandleKey(ownerDraw, key);
	}

	switch (ownerDraw)
	{
		case UI_CROSSHAIR:
			UI_Crosshair_HandleKey(flags, special, key);
			break;
		default:
			break;
	}

  return qfalse;
}

//unfortunately we cannot rely on any game/cgame module code to do our animation stuff,
//because the ui can be loaded while the game/cgame are not loaded. So we're going to recreate what we need here.
#undef MAX_ANIM_FILES
#define MAX_ANIM_FILES 512
class ui_animFileSet_t
{
public:
	char			filename[MAX_QPATH];
	animation_t		animations[MAX_ANIMATIONS];
}; // ui_animFileSet_t
static ui_animFileSet_t	ui_knownAnimFileSets[MAX_ANIM_FILES];

int				ui_numKnownAnimFileSets;

// Parses af_filename into the new file set. frameOffset is added to each first frame (a merged GLA part).
// initialize clears the file set first.
static qboolean UI_ParseAnimationFileAt( const char *af_filename, int frameOffset, qboolean initialize )
{
	const char		*text_p;
	int			len;
	int			i;
	const char		*token;
	float		fps;
	char		text[80000];
	int			animNum;
	animation_t	*animations = ui_knownAnimFileSets[ui_numKnownAnimFileSets].animations;

	len = re.GetAnimationCFG(af_filename, text, sizeof(text));
	if ( len <= 0 )
	{
		return qfalse;
	}
	if ( len >= (int)(sizeof( text ) - 1) )
	{
		Com_Error( ERR_FATAL, "UI_ParseAnimationFile: File %s too long\n (%d > %d)", af_filename, len, sizeof( text ) - 1);
		return qfalse;
	}

	// parse the text
	text_p = text;

	//FIXME: have some way of playing anims backwards... negative numFrames?

	//initialize anim array so that from 0 to MAX_ANIMATIONS, set default values of 0 1 0 100
	for(i = 0; initialize && i < MAX_ANIMATIONS; i++)
	{
		animations[i].firstFrame = 0;
		animations[i].numFrames = 0;
		animations[i].loopFrames = -1;
		animations[i].frameLerp = 100;
//		animations[i].initialLerp = 100;
	}

	// read information for each frame
	COM_BeginParseSession();
	while(1)
	{
		token = COM_Parse( &text_p );

		if ( !token || !token[0])
		{
			break;
		}

		animNum = GetIDForString(animTable, token);
		if(animNum == -1)
		{
//#ifndef FINAL_BUILD
#ifdef _DEBUG
			if (strcmp(token,"ROOT"))
			{
				Com_Printf(S_COLOR_RED"WARNING: Unknown token %s in %s\n", token, af_filename);
			}
#endif
			while (token[0])
			{
				token = COM_ParseExt( &text_p, qfalse );	//returns empty string when next token is EOL
			}
			continue;
		}

		token = COM_Parse( &text_p );
		if ( !token )
		{
			break;
		}
		animations[animNum].firstFrame = atoi( token ) + frameOffset;

		token = COM_Parse( &text_p );
		if ( !token )
		{
			break;
		}
		animations[animNum].numFrames = atoi( token );

		token = COM_Parse( &text_p );
		if ( !token )
		{
			break;
		}
		animations[animNum].loopFrames = atoi( token );

		token = COM_Parse( &text_p );
		if ( !token )
		{
			break;
		}
		fps = atof( token );
		if ( fps == 0 )
		{
			fps = 1;//Don't allow divide by zero error
		}
		if ( fps < 0 )
		{//backwards
			animations[animNum].frameLerp = floor(1000.0f / fps);
		}
		else
		{
			animations[animNum].frameLerp = ceil(1000.0f / fps);
		}

//		animations[animNum].initialLerp = ceil(1000.0f / fabs(fps));
	}
	COM_EndParseSession();

	return qtrue;
}

qboolean UI_ParseAnimationFile( const char *af_filename )
{
	return UI_ParseAnimationFileAt( af_filename, 0, qtrue );
}

// Reads the header and skeleton of a GLA file, with the checks of the renderer (see mdx_merge.h).
static const mdxaHeader_t *UI_ReadGLAHeader( const char *path, std::vector<byte> &buf )
{
	fileHandle_t	f;
	mdxaHeader_t	header;

	const int len = ui.FS_FOpenFile( path, &f, FS_READ );
	if ( len <= 0 || !f )
	{
		return NULL;
	}

	// The skeleton is between the header and the frames.
	const int skelEnd = ( len >= (int)sizeof( header ) && ui.FS_Read( &header, sizeof( header ), f ) == sizeof( header ) )
		? LittleLong( header.ofsFrames ) : 0;
	qboolean readOk = qfalse;
	if ( skelEnd > (int)sizeof( header ) && skelEnd <= len && LittleLong( header.ofsEnd ) <= len )
	{
		buf.resize( skelEnd );
		memcpy( buf.data(), &header, sizeof( header ) );
		const int skelBytes = skelEnd - (int)sizeof( header );
		readOk = (qboolean)( ui.FS_Read( buf.data() + sizeof( header ), skelBytes, f ) == skelBytes );
	}
	ui.FS_FCloseFile( f );

	return readOk ? GLA_CheckHeader( buf.data(), skelEnd ) : NULL;
}

// Parses <dir>/<name>.cfg, else <dir>/animation.cfg, at frameOffset.
static qboolean UI_ParseAnimationPart( const char *dir, const char *name, int frameOffset )
{
	char path[MAX_QPATH];

	Com_sprintf( path, sizeof( path ), "%s/%s.cfg", dir, name );
	if ( UI_ParseAnimationFileAt( path, frameOffset, qfalse ) )
	{
		return qtrue;
	}
	Com_sprintf( path, sizeof( path ), "%s/animation.cfg", dir );
	return UI_ParseAnimationFileAt( path, frameOffset, qfalse );
}

/*
=================
UI_ParseMergedAnimations

Parses the animations of the GLA parts that the renderer appends to the GLA of a skeleton (see mdx_merge.h):
_weapons.gla if glaPath is a _humanoid*.gla file, then the GLA of the animation override if overrideKey is not NULL.
The same frame offsets as G_ParseMergedAnimations in the game.
=================
*/
static void UI_ParseMergedAnimations( const char *glaPath, const char *overrideKey )
{
	std::vector<byte> baseBuf, weaponsBuf, extraBuf;

	if ( !overrideKey && !GLA_TakesWeapons( glaPath ) )
	{
		return;
	}

	const mdxaHeader_t *base = UI_ReadGLAHeader( glaPath, baseBuf );
	if ( !base )
	{
		return;
	}
	int numFrames = LittleLong( base->numFrames );

	const mdxaHeader_t *weapons = GLA_TakesWeapons( glaPath ) ? UI_ReadGLAHeader( GLA_WEAPONS_PATH, weaponsBuf ) : NULL;
	if ( weapons && GLA_CanMerge( base, numFrames, weapons ) )
	{
		UI_ParseAnimationPart( GLA_WEAPONS_DIR, "_weapons", numFrames );
		numFrames += LittleLong( weapons->numFrames );
	}

	if ( overrideKey )
	{
		// The override GLA is models/players/_<key>/_<key>.gla, else models/players/<key>/<key>.gla.
		char overrideName[MAX_QPATH];
		char overridePath[MAX_QPATH];
		Com_sprintf( overrideName, sizeof( overrideName ), "_%s", overrideKey );
		Com_sprintf( overridePath, sizeof( overridePath ), "models/players/%s/%s.gla", overrideName, overrideName );
		const mdxaHeader_t *extra = UI_ReadGLAHeader( overridePath, extraBuf );
		if ( !extra )
		{
			Q_strncpyz( overrideName, overrideKey, sizeof( overrideName ) );
			Com_sprintf( overridePath, sizeof( overridePath ), "models/players/%s/%s.gla", overrideName, overrideName );
			extra = UI_ReadGLAHeader( overridePath, extraBuf );
		}
		if ( extra && GLA_CanMerge( base, numFrames, extra ) )
		{
			char overrideDir[MAX_QPATH];
			Com_sprintf( overrideDir, sizeof( overrideDir ), "models/players/%s", overrideName );
			UI_ParseAnimationPart( overrideDir, overrideName, numFrames );
		}
	}
}

qboolean UI_ParseAnimFileSet( const char *animCFG, int *animFileIndex )
{ //Not going to bother parsing the sound config here.
	char		afilename[MAX_QPATH];
	char		strippedName[MAX_QPATH];
	int			i;
	char		*slash;

	Q_strncpyz( strippedName, animCFG, sizeof(strippedName));
	slash = strrchr( strippedName, '/' );
	if ( slash )
	{
		// truncate modelName to find just the dir not the extension
		*slash = 0;
	}

	//if this anims file was loaded before, don't parse it again, just point to the correct table of info
	for ( i = 0; i < ui_numKnownAnimFileSets; i++ )
	{
		if ( Q_stricmp(ui_knownAnimFileSets[i].filename, strippedName ) == 0 )
		{
			*animFileIndex = i;
			return qtrue;
		}
	}

	if ( ui_numKnownAnimFileSets == MAX_ANIM_FILES )
	{//TOO MANY!
		for (i = 0; i < MAX_ANIM_FILES; i++)
		{
			Com_Printf("animfile[%d]: %s\n", i, ui_knownAnimFileSets[i].filename);
		}
		Com_Error( ERR_FATAL, "UI_ParseAnimFileSet: %d == MAX_ANIM_FILES == %d", ui_numKnownAnimFileSets, MAX_ANIM_FILES);
	}

	//Okay, time to parse in a new one
	Q_strncpyz( ui_knownAnimFileSets[ui_numKnownAnimFileSets].filename, strippedName, sizeof( ui_knownAnimFileSets[ui_numKnownAnimFileSets].filename ) );

	// An animation override "_humanoid_o_<key>" is a virtual GLA: _humanoid.gla, _weapons.gla, then the override GLA.
	char overrideKey[MAX_QPATH];
	char glaPath[MAX_QPATH];
	Com_sprintf( glaPath, sizeof( glaPath ), "%s/%s.gla", strippedName, COM_SkipPath( strippedName ) );
	const qboolean isOverride = GLA_GetOverrideName( glaPath, overrideKey, sizeof( overrideKey ) );
	if ( isOverride )
	{
		Q_strncpyz( glaPath, GLA_HUMANOID_PATH, sizeof( glaPath ) );
	}
	else
	{
		Com_sprintf( glaPath, sizeof( glaPath ), "%s.gla", animCFG );
	}

	// Load and parse animations.cfg file
	Com_sprintf( afilename, sizeof( afilename ), "%s/animation.cfg", isOverride ? GLA_HUMANOID_DIR : strippedName );
	if ( !UI_ParseAnimationFile( afilename ) )
	{
		*animFileIndex = -1;
		return qfalse;
	}
	UI_ParseMergedAnimations( glaPath, isOverride ? overrideKey : NULL );

	//set index and increment
	*animFileIndex = ui_numKnownAnimFileSets++;

	return qtrue;
}

int UI_G2SetAnim(CGhoul2Info *ghlInfo, const char *boneName, int animNum, const qboolean freeze)
{
	int animIndex,blendTime;
	char *GLAName;

	GLAName = re.G2API_GetGLAName(ghlInfo);

	if (!GLAName || !GLAName[0])
	{
		return 0;
	}

	UI_ParseAnimFileSet(GLAName, &animIndex);

	if (animIndex != -1)
	{
		animation_t *anim = &ui_knownAnimFileSets[animIndex].animations[animNum];
		if (anim->numFrames <= 0)
		{
			return 0;
		}
		int sFrame = anim->firstFrame;
		int eFrame = anim->firstFrame + anim->numFrames;
		int flags = BONE_ANIM_OVERRIDE;
		int time = uiInfo.uiDC.realTime;
		float animSpeed = (50.0f / anim->frameLerp);

		blendTime = 150;

		// Freeze anim if it's not looping, special hack for datapad moves menu
		if (freeze)
		{
			if (anim->loopFrames == -1)
			{
				flags = BONE_ANIM_OVERRIDE_FREEZE;
			}
			else
			{
				flags = BONE_ANIM_OVERRIDE_LOOP;
			}
		}
		else if (anim->loopFrames != -1)
		{
			flags = BONE_ANIM_OVERRIDE_LOOP;
		}
		flags |= BONE_ANIM_BLEND;
		blendTime = 150;


		re.G2API_SetBoneAnim(ghlInfo, boneName, sFrame, eFrame, flags, animSpeed, time, -1, blendTime);

		return ((anim->frameLerp * (anim->numFrames-2)));
	}

	return 0;
}

static qboolean UI_ParseColorData(char* buf, playerSpeciesInfo_t &species)
{
	const char	*token;
	const char	*p;

	p = buf;
	COM_BeginParseSession();
	species.ColorCount = 0;
	species.ColorMax = 16;
	species.Color = (playerColor_t *)malloc(species.ColorMax * sizeof(playerColor_t));

	while ( p )
	{
		token = COM_ParseExt( &p, qtrue );	//looking for the shader
		if ( token[0] == 0 )
		{
			COM_EndParseSession(  );
			return (qboolean)(species.ColorCount != 0);
		}

		if (species.ColorCount >= species.ColorMax)
		{
			species.ColorMax *= 2;
			species.Color = (playerColor_t *)realloc(species.Color, species.ColorMax * sizeof(playerColor_t));
		}

		memset(&species.Color[species.ColorCount], 0, sizeof(playerColor_t));

		Q_strncpyz( species.Color[species.ColorCount].shader, token, MAX_QPATH );

		token = COM_ParseExt( &p, qtrue );	//looking for action block {
		if ( token[0] != '{' )
		{
			COM_EndParseSession(  );
			return qfalse;
		}

		token = COM_ParseExt( &p, qtrue );	//looking for action commands
		while (token[0] != '}')
		{
			if ( token[0] == 0)
			{	//EOF
				COM_EndParseSession(  );
				return qfalse;
			}
			Q_strcat(species.Color[species.ColorCount].actionText, ACTION_BUFFER_SIZE, token);
			Q_strcat(species.Color[species.ColorCount].actionText, ACTION_BUFFER_SIZE, " ");
			token = COM_ParseExt( &p, qtrue );	//looking for action commands or final }
		}
		species.ColorCount++;	//next color please
	}
	COM_EndParseSession(  );
	return qtrue;//never get here
}

/*
=================
bIsImageFile
builds path and scans for valid image extentions
=================
*/
static qboolean IsImageFile(const char* dirptr, const char* skinname, qboolean building)
{
	char fpath[MAX_QPATH];
	int f;

	if (Q_stricmpn(skinname, "model_", 6) == 0)
	{
		std::string skinString(skinname + 6, (strlen(skinname) - 6));

		skinname = skinString.c_str();

		Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.jpg", dirptr, skinname);
		ui.FS_FOpenFile(fpath, &f, FS_READ);
		if (!f)
		{ //not there, try png
			Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.png", dirptr, skinname);
			ui.FS_FOpenFile(fpath, &f, FS_READ);
		}
		if (!f)
		{ //not there, try tga
			Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.tga", dirptr, skinname);
			ui.FS_FOpenFile(fpath, &f, FS_READ);
		}
		if (f)
		{
			ui.FS_FCloseFile(f);
			if (building) ui.R_RegisterShaderNoMip(fpath);
			return qtrue;
		}
	}

	else
	{
		Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.jpg", dirptr, skinname);
		ui.FS_FOpenFile(fpath, &f, FS_READ);
		if (!f)
		{ //not there, try png
			Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.png", dirptr, skinname);
			ui.FS_FOpenFile(fpath, &f, FS_READ);
		}
		if (!f)
		{ //not there, try tga
			Com_sprintf(fpath, MAX_QPATH, "models/players/%s/icon_%s.tga", dirptr, skinname);
			ui.FS_FOpenFile(fpath, &f, FS_READ);
		}
		if (f)
		{
			ui.FS_FCloseFile(f);
			if (building) ui.R_RegisterShaderNoMip(fpath);
			return qtrue;
		}
	}

	return qfalse;
}

static void UI_FreeSpecies( playerSpeciesInfo_t *species )
{
	free(species->SkinHead);
	free(species->SkinTorso);
	free(species->SkinLeg);
	free(species->Color);
	free(species->Skin);
	memset(species, 0, sizeof(playerSpeciesInfo_t));
}

void UI_FreeAllSpecies( void )
{
	int i;

	for (i = 0; i < uiInfo.playerSpeciesCount; i++)
	{
		UI_FreeSpecies(&uiInfo.playerSpecies[i]);
	}
	free(uiInfo.playerSpecies);

	uiInfo.playerSpeciesCount = 0;
	uiInfo.playerSpecies = NULL;
}

/*
=================
PlayerModel_BuildList
=================
*/
static void UI_BuildPlayerModel_List(qboolean inGameLoad)
{
	static const size_t DIR_LIST_SIZE = 32768;

	int		numdirs;
	size_t	dirListSize = DIR_LIST_SIZE;
	char	stackDirList[8192];
	char* dirlist;
	char* dirptr;
	int		dirlen;
	int		i;
	const int building = Cvar_VariableIntegerValue("com_buildscript");

	dirlist = (char*)malloc(DIR_LIST_SIZE);
	if (!dirlist)
	{
		Com_Printf(S_COLOR_YELLOW "WARNING: Failed to allocate %u bytes of memory for player model "
			"directory list. Using stack allocated buffer of %u bytes instead.",
			DIR_LIST_SIZE, sizeof(stackDirList));

		dirlist = stackDirList;
		dirListSize = sizeof(stackDirList);
	}

	uiInfo.playerSpeciesCount = 0;
	uiInfo.playerSpeciesIndex = 0;
	uiInfo.playerSpeciesMax = 8;
	uiInfo.playerSpecies = (playerSpeciesInfo_t*)malloc(uiInfo.playerSpeciesMax * sizeof(playerSpeciesInfo_t));

	// iterate directory of all player models
	numdirs = ui.FS_GetFileList("models/players", "/", dirlist, dirListSize);
	dirptr = dirlist;
	for (i = 0; i < numdirs; i++, dirptr += dirlen + 1)
	{
		char* fileptr;
		int		filelen;
		int f = 0;
		char fpath[MAX_QPATH];

		dirlen = strlen(dirptr);

		if (dirlen)
		{
			if (dirptr[dirlen - 1] == '/')
				dirptr[dirlen - 1] = '\0';
		}
		else
		{
			continue;
		}

		if (!strcmp(dirptr, ".") || !strcmp(dirptr, ".."))
			continue;

		Com_sprintf(fpath, sizeof(fpath), "models/players/%s/PlayerChoice.txt", dirptr);
		filelen = ui.FS_FOpenFile(fpath, &f, FS_READ);

		if (f)
		{
			char filelist[2048];
			playerSpeciesInfo_t* species = NULL;

			std::vector<char> buffer(filelen + 1);
			ui.FS_Read(&buffer[0], filelen, f);
			ui.FS_FCloseFile(f);

			buffer[filelen] = 0;

			//record this species
			if (uiInfo.playerSpeciesCount >= uiInfo.playerSpeciesMax)
			{
				uiInfo.playerSpeciesMax *= 2;
				uiInfo.playerSpecies = (playerSpeciesInfo_t*)realloc(uiInfo.playerSpecies, uiInfo.playerSpeciesMax * sizeof(playerSpeciesInfo_t));
			}
			species = &uiInfo.playerSpecies[uiInfo.playerSpeciesCount];
			memset(species, 0, sizeof(playerSpeciesInfo_t));
			Q_strncpyz(species->Name, dirptr, MAX_QPATH);

			if (!UI_ParseColorData(buffer.data(), *species))
			{
				ui.Printf("UI_BuildPlayerModel_List: Errors parsing '%s'\n", fpath);
			}

			species->SkinHeadMax = 8;
			species->SkinTorsoMax = 8;
			species->SkinLegMax = 8;
			species->SkinMax = 8;


			species->SkinHead = (skinName_t*)malloc(species->SkinHeadMax * sizeof(skinName_t));
			species->SkinTorso = (skinName_t*)malloc(species->SkinTorsoMax * sizeof(skinName_t));
			species->SkinLeg = (skinName_t*)malloc(species->SkinLegMax * sizeof(skinName_t));
			species->Skin = (skinName_t*)malloc(species->SkinMax * sizeof(skinName_t));


			int		j;
			char	skinname[64];
			int		numfiles;
			int		iSkinParts = 0;

			numfiles = ui.FS_GetFileList(va("models/players/%s", dirptr), ".skin", filelist, sizeof(filelist));
			fileptr = filelist;
			for (j = 0; j < numfiles; j++, fileptr += filelen + 1)
			{
				if (building)
				{
					ui.FS_FOpenFile(va("models/players/%s/%s", dirptr, fileptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
					ui.FS_FOpenFile(va("models/players/%s/sounds.cfg", dirptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
					ui.FS_FOpenFile(va("models/players/%s/animevents.cfg", dirptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
				}

				filelen = strlen(fileptr);
				COM_StripExtension(fileptr, skinname, sizeof(skinname));

				// The icon is optional: the skin list writes the name when it is missing.
				if (building)
				{
					IsImageFile(dirptr, skinname, qtrue);
				}
				{
					if (Q_stricmpn(skinname, "head_", 5) == 0)
					{
						if (species->SkinHeadCount >= species->SkinHeadMax)
						{
							species->SkinHeadMax *= 2;
							species->SkinHead = (skinName_t*)realloc(species->SkinHead, species->SkinHeadMax * sizeof(skinName_t));
						}
						Q_strncpyz(species->SkinHead[species->SkinHeadCount++].name, skinname, SKIN_LENGTH);
						iSkinParts |= 1 << 0;
					}
					else
						if (Q_stricmpn(skinname, "torso_", 6) == 0)
						{
							if (species->SkinTorsoCount >= species->SkinTorsoMax)
							{
								species->SkinTorsoMax *= 2;
								species->SkinTorso = (skinName_t*)realloc(species->SkinTorso, species->SkinTorsoMax * sizeof(skinName_t));
							}
							Q_strncpyz(species->SkinTorso[species->SkinTorsoCount++].name, skinname, SKIN_LENGTH);
							iSkinParts |= 1 << 1;
						}
						else
							if (Q_stricmpn(skinname, "lower_", 6) == 0)
							{
								if (species->SkinLegCount >= species->SkinLegMax)
								{
									species->SkinLegMax *= 2;
									species->SkinLeg = (skinName_t*)realloc(species->SkinLeg, species->SkinLegMax * sizeof(skinName_t));
								}
								Q_strncpyz(species->SkinLeg[species->SkinLegCount++].name, skinname, SKIN_LENGTH);
								iSkinParts |= 1 << 2;
							}
					if (Q_stricmpn(skinname, "model_", 6) == 0)
					{
						if (species->SkinCount >= species->SkinMax)
						{
							species->SkinMax *= 2;
							species->Skin = (skinName_t*)realloc(species->Skin, species->SkinMax * sizeof(skinName_t));
						}
						Q_strncpyz(species->Skin[species->SkinCount++].name, skinname, SKIN_LENGTH);
						iSkinParts |= 1 << 3;

						//if (Q_stricmpn(skinname, "model_default", 13) == 0)
					}
				}
			}
			if (iSkinParts < 7)
			{	//didn't get a skin for each, then skip this model.
				UI_FreeSpecies(species);
				continue;
			}
			uiInfo.playerSpeciesCount++;
			if (!inGameLoad && ui_PrecacheModels.integer)
			{
				CGhoul2Info_v ghoul2;
				Com_sprintf(fpath, sizeof(fpath), "models/players/%s/model.glm", dirptr);
				int g2Model = DC->g2_InitGhoul2Model(ghoul2, fpath, 0, 0, 0, 0, 0);
				if (g2Model >= 0)
				{
					DC->g2_RemoveGhoul2Model(ghoul2, 0);
				}
			}

			ui.FS_FCloseFile(f);
		}
		else
		{
			char filelist[2048];
			playerSpeciesInfo_t* species = NULL;

			//record this species
			if (uiInfo.playerSpeciesCount >= uiInfo.playerSpeciesMax)
			{
				uiInfo.playerSpeciesMax *= 2;
				uiInfo.playerSpecies = (playerSpeciesInfo_t*)realloc(uiInfo.playerSpecies, uiInfo.playerSpeciesMax * sizeof(playerSpeciesInfo_t));
			}
			species = &uiInfo.playerSpecies[uiInfo.playerSpeciesCount];
			memset(species, 0, sizeof(playerSpeciesInfo_t));
			Q_strncpyz(species->Name, dirptr, MAX_QPATH);

			species->SkinMax = 8;

			species->Skin = (skinName_t*)malloc(species->SkinMax * sizeof(skinName_t));

			int		j;
			char	skinname[64];
			int		numfiles;
			int		iSkinParts = 0;

			numfiles = ui.FS_GetFileList(va("models/players/%s", dirptr), ".skin", filelist, sizeof(filelist));
			fileptr = filelist;
			for (j = 0; j < numfiles; j++, fileptr += filelen + 1)
			{
				if (building)
				{
					ui.FS_FOpenFile(va("models/players/%s/%s", dirptr, fileptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
					ui.FS_FOpenFile(va("models/players/%s/sounds.cfg", dirptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
					ui.FS_FOpenFile(va("models/players/%s/animevents.cfg", dirptr), &f, FS_READ);
					if (f) ui.FS_FCloseFile(f);
				}

				filelen = strlen(fileptr);
				COM_StripExtension(fileptr, skinname, sizeof(skinname));

				if (building)
				{
					IsImageFile(dirptr, skinname, qtrue);
				}
				{
					if (Q_stricmpn(skinname, "model_", 6) == 0)
					{
						if (species->SkinCount >= species->SkinMax)
						{
							species->SkinMax *= 2;
							species->Skin = (skinName_t*)realloc(species->Skin, species->SkinMax * sizeof(skinName_t));
						}
						Q_strncpyz(species->Skin[species->SkinCount++].name, skinname, SKIN_LENGTH);
						iSkinParts |= 1 << 3;
						//if (Q_stricmpn(skinname, "model_default", 13) == 0)
					}
				}
			}
			if (iSkinParts < 7)
			{	//didn't get a skin for each, then skip this model.
				UI_FreeSpecies(species);
				continue;
			}
			uiInfo.playerSpeciesCount++;
		}
	}



	if (dirlist != stackDirList)
	{
		free(dirlist);
	}
}


/*
================
UI_Shutdown
=================
*/
void UI_Shutdown( void )
{
	UI_FreeAllSpecies();
}

// Menu items set a latched cvar for the next restart (Cvar_SetLatched), so they show the latched value.
static float UI_Cvar_PendingValue( const char *var_name )
{
	char buf[MAX_CVAR_VALUE_STRING];

	Cvar_PendingStringBuffer( var_name, buf, sizeof( buf ) );
	return atof( buf );
}

/*
=================
UI_Init
=================
*/
void _UI_Init( qboolean inGameLoad )
{
	// Get the list of possible languages
#ifndef JK2_MODE
	uiInfo.languageCount = SE_GetNumLanguages();	// this does a dir scan, so use carefully
#else
	// sod it, parse every menu strip file until we find a gap in the sequence...
	//
	for (int i=0; i<10; i++)
	{
		if (!ui.SP_Register(va("menus%d",i), /*SP_REGISTER_REQUIRED|*/SP_REGISTER_MENU))
			break;
	}
#endif

	uiInfo.inGameLoad = inGameLoad;


	UI_InitMemory();

	// cache redundant calulations
	trap_GetGlconfig( &uiInfo.uiDC.glconfig );


	UI_RegisterCvars();

	// for 640x480 virtualized screen
	uiInfo.uiDC.yscale = uiInfo.uiDC.glconfig.vidHeight * (1.0/480.0);
	uiInfo.uiDC.xscale = uiInfo.uiDC.glconfig.vidWidth * (1.0/640.0);
	if ( uiInfo.uiDC.glconfig.vidWidth * 480 > uiInfo.uiDC.glconfig.vidHeight * 640 )
	{
		// wide screen
		uiInfo.uiDC.bias = 0.5 * ( uiInfo.uiDC.glconfig.vidWidth - ( uiInfo.uiDC.glconfig.vidHeight * (640.0/480.0) ) );
	}
	else
	{
		// no wide screen
		uiInfo.uiDC.bias = 0;
	}

	Init_Display(&uiInfo.uiDC);

	uiInfo.uiDC.drawText			= &Text_Paint;
	uiInfo.uiDC.drawHandlePic		= &UI_DrawHandlePic;
	uiInfo.uiDC.drawRect			= &_UI_DrawRect;
	uiInfo.uiDC.drawSides			= &_UI_DrawSides;
	uiInfo.uiDC.drawTextWithCursor	= &Text_PaintWithCursor;
	uiInfo.uiDC.executeText			= &Cbuf_ExecuteText;
	uiInfo.uiDC.drawTopBottom		= &_UI_DrawTopBottom;
	uiInfo.uiDC.feederCount			= &UI_FeederCount;
	uiInfo.uiDC.feederSelection		= &UI_FeederSelection;
	uiInfo.uiDC.fillRect			= &UI_FillRect;
	uiInfo.uiDC.getBindingBuf		= &Key_GetBindingBuf;
	uiInfo.uiDC.getCVarString		= Cvar_PendingStringBuffer;
	uiInfo.uiDC.getCVarValue		= UI_Cvar_PendingValue;
	uiInfo.uiDC.getOverstrikeMode	= &trap_Key_GetOverstrikeMode;
	uiInfo.uiDC.getValue			= &UI_GetValue;
	uiInfo.uiDC.keynumToStringBuf	= &Key_KeynumToStringBuf;
	uiInfo.uiDC.keyCombo			= &Key_ComboKeynum;
	uiInfo.uiDC.modelBounds			= &trap_R_ModelBounds;
	uiInfo.uiDC.ownerDrawVisible	= &UI_OwnerDrawVisible;
	uiInfo.uiDC.ownerDrawWidth		= &UI_OwnerDrawWidth;
	uiInfo.uiDC.ownerDrawItem		= &UI_OwnerDraw;
	uiInfo.uiDC.Print				= &Com_Printf;
	uiInfo.uiDC.registerSound		= &trap_S_RegisterSound;
	uiInfo.uiDC.registerModel		= ui.R_RegisterModel;
	uiInfo.uiDC.clearScene			= &trap_R_ClearScene;
	uiInfo.uiDC.addRefEntityToScene = &trap_R_AddRefEntityToScene;
	uiInfo.uiDC.renderScene			= &trap_R_RenderScene;
	uiInfo.uiDC.runScript			= &UI_RunMenuScript;
	uiInfo.uiDC.deferScript			= &UI_DeferMenuScript;
	uiInfo.uiDC.setBinding			= &trap_Key_SetBinding;
	uiInfo.uiDC.setColor			= &UI_SetColor;
	uiInfo.uiDC.setCVar				= Cvar_SetLatched;
	uiInfo.uiDC.setOverstrikeMode	= &trap_Key_SetOverstrikeMode;
	uiInfo.uiDC.startLocalSound		= &trap_S_StartLocalSound;
	uiInfo.uiDC.stopCinematic		= &UI_StopCinematic;
	uiInfo.uiDC.textHeight			= &Text_Height;
	uiInfo.uiDC.textWidth			= &Text_Width;
	uiInfo.uiDC.feederItemImage		= &UI_FeederItemImage;
	uiInfo.uiDC.feederItemText		= &UI_FeederItemText;
	uiInfo.uiDC.ownerDrawHandleKey	= &UI_OwnerDrawHandleKey;

	uiInfo.uiDC.registerSkin		= re.RegisterSkin;

	uiInfo.uiDC.g2_SetSkin = re.G2API_SetSkin;
	uiInfo.uiDC.g2_SetBoneAnim = re.G2API_SetBoneAnim;
	uiInfo.uiDC.g2_RemoveGhoul2Model = re.G2API_RemoveGhoul2Model;
	uiInfo.uiDC.g2_InitGhoul2Model = re.G2API_InitGhoul2Model;
	uiInfo.uiDC.g2_CleanGhoul2Models = re.G2API_CleanGhoul2Models;
	uiInfo.uiDC.g2_AddBolt = re.G2API_AddBolt;
	uiInfo.uiDC.g2_GetBoltMatrix = re.G2API_GetBoltMatrix;
	uiInfo.uiDC.g2_GiveMeVectorFromMatrix = re.G2API_GiveMeVectorFromMatrix;

	uiInfo.uiDC.g2hilev_SetAnim = UI_G2SetAnim;

	UI_BuildPlayerModel_List(inGameLoad);

	String_Init();

	const char *menuSet = UI_Cvar_VariableString("ui_menuFiles");

	if (menuSet == NULL || menuSet[0] == '\0')
	{
		menuSet = "ui/menus.txt";
	}

#ifndef JK2_MODE
	if (inGameLoad)
	{
		UI_LoadMenus("ui/ingame.txt", qtrue);
	}
	else
#endif
	{
		UI_LoadMenus(menuSet, qtrue);
	}

	Menus_CloseAll();

	uiInfo.uiDC.whiteShader = ui.R_RegisterShaderNoMip( "white" );

	AssetCache();

	uis.debugMode = qfalse;

	// sets defaults for ui temp cvars
	uiInfo.effectsColor = (int)trap_Cvar_VariableValue("color")-1;
	if (uiInfo.effectsColor < 0)
	{
		uiInfo.effectsColor = 0;
	}
	uiInfo.effectsColor = gamecodetoui[uiInfo.effectsColor];
	uiInfo.currentCrosshair = (int)trap_Cvar_VariableValue("cg_drawCrosshair");
	Cvar_Set("ui_mousePitch", (trap_Cvar_VariableValue("m_pitch") >= 0) ? "0" : "1");

	Cvar_Set("cg_endcredits", "0");	// Reset value
	Cvar_Set("ui_missionfailed","0"); // reset

	uiInfo.forcePowerUpdated = FP_UPDATED_NONE;
	uiInfo.selectedWeapon1 = NOWEAPON;
	uiInfo.selectedWeapon2 = NOWEAPON;
	uiInfo.selectedThrowWeapon = NOWEAPON;

	uiInfo.uiDC.Assets.nullSound = trap_S_RegisterSound("sound/null", qfalse);

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	trap_S_RegisterSound("sound/interface/weapon_deselect", qfalse);
#endif

}

/*
=================
UI_RegisterCvars
=================
*/
static void UI_RegisterCvars( void )
{
	size_t i = 0;
	const cvarTable_t *cv = NULL;

	for ( i=0, cv=cvarTable; i<cvarTableSize; i++, cv++ ) {
		Cvar_Register( cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags );
		if ( cv->update )
			cv->update();
	}
}

/*
=================
UI_ParseMenu
=================
*/
void UI_ParseMenu(const char *menuFile)
{
	char	*buffer,*holdBuffer,*token2;
	int len;
//	pc_token_t token;

	//Com_DPrintf("Parsing menu file: %s\n", menuFile);
	len = PC_StartParseSession(menuFile,&buffer);

	holdBuffer = buffer;

	if (len<=0)
	{
		Com_Printf("UI_ParseMenu: Unable to load menu %s\n", menuFile);
		return;
	}

	while ( 1 )
	{

		token2 = PC_ParseExt();

		if (!*token2)
		{
			break;
		}
/*
		if ( menuCount == MAX_MENUS )
		{
			PC_ParseWarning("Too many menus!");
			break;
		}
*/
		if ( *token2 == '{')
		{
			continue;
		}
		else if ( *token2 == '}' )
		{
			break;
		}
		else if (Q_stricmp(token2, "assetGlobalDef") == 0)
		{
			if (Asset_Parse(&holdBuffer))
			{
				continue;
			}
			else
			{
				break;
			}
		}
		else if (Q_stricmp(token2, "menudef") == 0)
		{
			// start a new menu
			Menu_New(holdBuffer);
			continue;
		}

		PC_ParseWarning(va("Invalid keyword '%s'",token2));
	}

	PC_EndParseSession(buffer);

}

/*
=================
Load_Menu
	Load current menu file
=================
*/
qboolean Load_Menu(const char **holdBuffer)
{
	const char	*token2;

	token2 = COM_ParseExt( holdBuffer, qtrue );

	if (!token2[0])
	{
		return qfalse;
	}

	if (*token2 != '{')
	{
		return qfalse;
	}

	while ( 1 )
	{
		token2 = COM_ParseExt( holdBuffer, qtrue );

		if ((!token2) || (token2 == 0))
		{
			return qfalse;
		}

		if ( *token2 == '}' )
		{
			return qtrue;
		}

//#ifdef _DEBUG
//		extern void UI_Debug_AddMenuFilePath(const char *);
//		UI_Debug_AddMenuFilePath(token2);
//#endif
		UI_ParseMenu(token2);

	}
	return qfalse;
}

/*
=================
UI_LoadMenus
	Load all menus based on the files listed in the data file in menuFile (default "ui/menus.txt")
=================
*/
void UI_LoadMenus(const char *menuFile, qboolean reset)
{
//	pc_token_t token;
//	int handle;
	int start;

	char *buffer;
	const char *holdBuffer;
	int len;

	start = Sys_Milliseconds();

	len = ui.FS_ReadFile(menuFile,(void **) &buffer);

	if (len<1)
	{
		Com_Printf( va( S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile ) );
		len = ui.FS_ReadFile("ui/menus.txt",(void **) &buffer);

		if (len<1)
		{
			Com_Error( ERR_FATAL, "%s", va("default menu file not found: ui/menus.txt, unable to continue!\n", menuFile ));
			return;
		}
	}

	if (reset)
	{
		Menu_Reset();
	}

	const char	*token2;
	holdBuffer = buffer;
	COM_BeginParseSession();
	while ( 1 )
	{
		token2 = COM_ParseExt( &holdBuffer, qtrue );
		if (!*token2)
		{
			break;
		}

		if( *token2 == 0 || *token2 == '}')			// End of the menus file
		{
			break;
		}

		if (*token2 == '{')
		{
				continue;
		}
		else if (Q_stricmp(token2, "loadmenu") == 0)
		{
			if (Load_Menu(&holdBuffer))
			{
				continue;
			}
			else
			{
				break;
			}
		}
		else
		{
			Com_Printf("Unknown keyword '%s' in menus file %s\n", token2, menuFile);
		}
	}
	COM_EndParseSession();

	Com_Printf("UI menu load time = %d milli seconds\n", Sys_Milliseconds() - start);

	ui.FS_FreeFile( buffer );	//let go of the buffer
}

/*
=================
UI_Load
=================
*/
void UI_Load(void)
{
	const char *menuSet;
	char lastName[1024];
	menuDef_t *menu = Menu_GetFocused();

	if (menu && menu->window.name)
	{
		strcpy(lastName, menu->window.name);
	}
	else
	{
		lastName[0] = 0;
	}

#ifndef JK2_MODE
	if (uiInfo.inGameLoad)
	{
		menuSet= "ui/ingame.txt";
	}
	else
#endif
	{
		menuSet= UI_Cvar_VariableString("ui_menuFiles");
	}
	if (menuSet == NULL || menuSet[0] == '\0')
	{
		menuSet = "ui/menus.txt";
	}

	String_Init();

	UI_LoadMenus(menuSet, qtrue);
	Menus_CloseAll();
	Menus_ActivateByName(lastName);
}

/*
=================
Asset_Parse
=================
*/
qboolean Asset_Parse(char **buffer)
{
	char		*token;
	const char	*tempStr;
	int			pointSize;

	token = PC_ParseExt();

	if (!token)
	{
		return qfalse;
	}

	if (*token != '{')
	{
		return qfalse;
	}

	while ( 1 )
	{
		token = PC_ParseExt();

		if (!token)
		{
			return qfalse;
		}

		if (*token == '}')
		{
			return qtrue;
		}

		// fonts
		if (Q_stricmp(token, "smallFont") == 0)		//legacy, really it only matters which order they are registered
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'smallFont'");
				return qfalse;
			}

			UI_RegisterFont(tempStr);

			//not used anymore
			if (PC_ParseInt(&pointSize))
			{
//				PC_ParseWarning("Bad 2nd parameter for keyword 'smallFont'");
			}

			continue;
		}

		if (Q_stricmp(token, "mediumFont") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'font'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.qhMediumFont = UI_RegisterFont(tempStr);
			uiInfo.uiDC.Assets.fontRegistered = qtrue;

			//not used
			if (PC_ParseInt(&pointSize))
			{
//				PC_ParseWarning("Bad 2nd parameter for keyword 'font'");
			}
			continue;
		}

		if (Q_stricmp(token, "bigFont") == 0) //legacy
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'bigFont'");
				return qfalse;
			}

			UI_RegisterFont(tempStr);

			if (PC_ParseInt(&pointSize))
			{
//				PC_ParseWarning("Bad 2nd parameter for keyword 'bigFont'");
			}

			continue;
		}

#ifdef JK2_MODE
		if (Q_stricmp(token, "stripedFile") == 0)
		{
			if (!PC_ParseStringMem((const char **) &tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'stripedFile'");
				return qfalse;
			}

			char sTemp[1024];
			Q_strncpyz( sTemp, tempStr,  sizeof(sTemp) );
			if (!ui.SP_Register(sTemp, /*SP_REGISTER_REQUIRED|*/SP_REGISTER_MENU))
			{
				PC_ParseWarning(va("(.SP file \"%s\" not found)",sTemp));
				//return qfalse;	// hmmm... dunno about this, don't want to break scripts for just missing subtitles
			}
			else
			{
//				extern void AddMenuPackageRetryKey(const char *);
//				AddMenuPackageRetryKey(sTemp);
			}

			continue;
		}
#endif

		// gradientbar
		if (Q_stricmp(token, "gradientbar") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'gradientbar'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.gradientBar = ui.R_RegisterShaderNoMip(tempStr);
			continue;
		}

		// enterMenuSound
		if (Q_stricmp(token, "menuEnterSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'menuEnterSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.menuEnterSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// exitMenuSound
		if (Q_stricmp(token, "menuExitSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'menuExitSound'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuExitSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// itemFocusSound
		if (Q_stricmp(token, "itemFocusSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'itemFocusSound'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.itemFocusSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// menuBuzzSound
		if (Q_stricmp(token, "menuBuzzSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'menuBuzzSound'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuBuzzSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// Chose a force power from the ingame force allocation screen (the one where you get to allocate a force power point)
		if (Q_stricmp(token, "forceChosenSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'forceChosenSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.forceChosenSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}


		// Unchose a force power from the ingame force allocation screen (the one where you get to allocate a force power point)
		if (Q_stricmp(token, "forceUnchosenSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'forceUnchosenSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.forceUnchosenSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token, "datapadmoveRollSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveRollSound'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveRollSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token, "datapadmoveJumpSound") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveRoll'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveJumpSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound1") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound1'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound1 = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound2") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound2'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound2 = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound3") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound3'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound3 = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound4") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound4'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound4 = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound5") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound5'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound5 = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token, "datapadmoveSaberSound6") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'datapadmoveSaberSound6'");
				return qfalse;
			}

			uiInfo.uiDC.Assets.datapadmoveSaberSound6 = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token, "cursor") == 0)
		{
			if (PC_ParseString(&tempStr))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'cursor'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor = ui.R_RegisterShaderNoMip( tempStr);
			continue;
		}

		if (Q_stricmp(token, "fadeClamp") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.fadeClamp))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'fadeClamp'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "fadeCycle") == 0)
		{
			if (PC_ParseInt(&uiInfo.uiDC.Assets.fadeCycle))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'fadeCycle'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "fadeAmount") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.fadeAmount))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'fadeAmount'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "shadowX") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.shadowX))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'shadowX'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "shadowY") == 0)
		{
			if (PC_ParseFloat(&uiInfo.uiDC.Assets.shadowY))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'shadowY'");
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token, "shadowColor") == 0)
		{
			if (PC_ParseColor(&uiInfo.uiDC.Assets.shadowColor))
			{
				PC_ParseWarning("Bad 1st parameter for keyword 'shadowColor'");
				return qfalse;
			}
			uiInfo.uiDC.Assets.shadowFadeClamp = uiInfo.uiDC.Assets.shadowColor[3];
			continue;
		}

		// precaching various sound files used in the menus
		if (Q_stricmp(token, "precacheSound") == 0)
		{
			if (PC_Script_Parse(&tempStr))
			{
				char *soundFile;
				do
				{
					soundFile = COM_ParseExt(&tempStr, qfalse);
					if (soundFile[0] != 0 && soundFile[0] != ';') {
						if (!trap_S_RegisterSound( soundFile, qfalse ))
						{
							PC_ParseWarning("Can't locate precache sound");
						}
					}
				} while (soundFile[0]);
			}
			continue;
		}
	}

	PC_ParseWarning(va("Invalid keyword '%s'",token));
	return qfalse;
}

/*
=================
UI_Update
=================
*/
static void UI_Update(const char *name)
{
	int	val = trap_Cvar_VariableValue(name);


	if (Q_stricmp(name, "s_khz") == 0)
	{
		ui.Cmd_ExecuteText( EXEC_APPEND, "snd_restart\n" );
		return;
	}

	if (Q_stricmp(name, "ui_SetName") == 0)
	{
		Cvar_Set( "name", UI_Cvar_VariableString("ui_Name"));
 	}
	else if (Q_stricmp(name, "ui_GetName") == 0)
	{
		Cvar_Set( "ui_Name", UI_Cvar_VariableString("name"));
 	}
	else if (Q_stricmp(name, "ui_r_colorbits") == 0)
	{
		switch (val)
		{
			case 0:
				Cvar_SetValue( "ui_r_depthbits", 0 );
				break;

			case 16:
				Cvar_SetValue( "ui_r_depthbits", 16 );
				break;

			case 32:
				Cvar_SetValue( "ui_r_depthbits", 24 );
				break;
		}
	}
	else if (Q_stricmp(name, "ui_r_lodbias") == 0)
	{
		switch (val)
		{
			case 0:
				Cvar_SetValue( "ui_r_subdivisions", 4 );
				break;
			case 1:
				Cvar_SetValue( "ui_r_subdivisions", 12 );
				break;

			case 2:
				Cvar_SetValue( "ui_r_subdivisions", 20 );
				break;
		}
	}
	else if (Q_stricmp(name, "ui_r_glCustom") == 0)
	{
		switch (val)
		{
			case 0:	// high quality

				Cvar_SetValue( "ui_r_fullScreen", 1 );
				Cvar_SetValue( "ui_r_subdivisions", 4 );
				Cvar_SetValue( "ui_r_lodbias", 0 );
				Cvar_SetValue( "ui_r_colorbits", 32 );
				Cvar_SetValue( "ui_r_depthbits", 24 );
				Cvar_SetValue( "ui_r_picmip", 0 );
				Cvar_SetValue( "ui_r_mode", 4 );
				Cvar_SetValue( "ui_r_texturebits", 32 );
				Cvar_SetValue( "ui_r_fastSky", 0 );
				Cvar_SetValue( "ui_r_inGameVideo", 1 );
				//Cvar_SetValue( "ui_cg_shadows", 2 );//stencil
				Cvar_Set( "ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
				break;

			case 1: // normal
				Cvar_SetValue( "ui_r_fullScreen", 1 );
				Cvar_SetValue( "ui_r_subdivisions", 4 );
				Cvar_SetValue( "ui_r_lodbias", 0 );
				Cvar_SetValue( "ui_r_colorbits", 0 );
				Cvar_SetValue( "ui_r_depthbits", 24 );
				Cvar_SetValue( "ui_r_picmip", 1 );
				Cvar_SetValue( "ui_r_mode", 3 );
				Cvar_SetValue( "ui_r_texturebits", 0 );
				Cvar_SetValue( "ui_r_fastSky", 0 );
				Cvar_SetValue( "ui_r_inGameVideo", 1 );
				//Cvar_SetValue( "ui_cg_shadows", 2 );
				Cvar_Set( "ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
				break;

			case 2: // fast

				Cvar_SetValue( "ui_r_fullScreen", 1 );
				Cvar_SetValue( "ui_r_subdivisions", 12 );
				Cvar_SetValue( "ui_r_lodbias", 1 );
				Cvar_SetValue( "ui_r_colorbits", 0 );
				Cvar_SetValue( "ui_r_depthbits", 0 );
				Cvar_SetValue( "ui_r_picmip", 2 );
				Cvar_SetValue( "ui_r_mode", 3 );
				Cvar_SetValue( "ui_r_texturebits", 0 );
				Cvar_SetValue( "ui_r_fastSky", 1 );
				Cvar_SetValue( "ui_r_inGameVideo", 0 );
				//Cvar_SetValue( "ui_cg_shadows", 1 );
				Cvar_Set( "ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST" );
				break;

			case 3: // fastest

				Cvar_SetValue( "ui_r_fullScreen", 1 );
				Cvar_SetValue( "ui_r_subdivisions", 20 );
				Cvar_SetValue( "ui_r_lodbias", 2 );
				Cvar_SetValue( "ui_r_colorbits", 16 );
				Cvar_SetValue( "ui_r_depthbits", 16 );
				Cvar_SetValue( "ui_r_mode", 3 );
				Cvar_SetValue( "ui_r_picmip", 3 );
				Cvar_SetValue( "ui_r_texturebits", 16 );
				Cvar_SetValue( "ui_r_fastSky", 1 );
				Cvar_SetValue( "ui_r_inGameVideo", 0 );
				//Cvar_SetValue( "ui_cg_shadows", 0 );
				Cvar_Set( "ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST" );
			break;
		}
	}
	else if (Q_stricmp(name, "ui_mousePitch") == 0)
	{
		if (val == 0)
		{
			Cvar_SetValue( "m_pitch", 0.022f );
		}
		else
		{
			Cvar_SetValue( "m_pitch", -0.022f );
		}
	}
	else
	{//failure!!
		Com_Printf("unknown UI script UPDATE %s\n", name);
	}
}

#define ASSET_SCROLLBAR             "gfx/menus/scrollbar.tga"
#define ASSET_SCROLLBAR_ARROWDOWN   "gfx/menus/scrollbar_arrow_dwn_a.tga"
#define ASSET_SCROLLBAR_ARROWUP     "gfx/menus/scrollbar_arrow_up_a.tga"
#define ASSET_SCROLLBAR_ARROWLEFT   "gfx/menus/scrollbar_arrow_left.tga"
#define ASSET_SCROLLBAR_ARROWRIGHT  "gfx/menus/scrollbar_arrow_right.tga"
#define ASSET_SCROLL_THUMB          "gfx/menus/scrollbar_thumb.tga"


/*
=================
AssetCache
=================
*/
void AssetCache(void)
{
//	int n;
	uiInfo.uiDC.Assets.scrollBar = ui.R_RegisterShaderNoMip( ASSET_SCROLLBAR );
	uiInfo.uiDC.Assets.scrollBarArrowDown = ui.R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWDOWN );
	uiInfo.uiDC.Assets.scrollBarArrowUp = ui.R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWUP );
	uiInfo.uiDC.Assets.scrollBarArrowLeft = ui.R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWLEFT );
	uiInfo.uiDC.Assets.scrollBarArrowRight = ui.R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWRIGHT );
	uiInfo.uiDC.Assets.scrollBarThumb = ui.R_RegisterShaderNoMip( ASSET_SCROLL_THUMB );

	uiInfo.uiDC.Assets.sliderBar = ui.R_RegisterShaderNoMip( "menu/new/slider" );
	uiInfo.uiDC.Assets.sliderThumb = ui.R_RegisterShaderNoMip( "menu/new/sliderthumb");


	/*
	for( n = 0; n < NUM_CROSSHAIRS; n++ )
	{
		uiInfo.uiDC.Assets.crosshairShader[n] = ui.R_RegisterShaderNoMip( va("gfx/2d/crosshair%c", 'a' + n ) );
	}
	*/
}

/*
================
_UI_DrawSides
=================
*/
void _UI_DrawSides(float x, float y, float w, float h, float size)
{
	// size is in virtual units vertically; the 640x480 space is stretched, so scale it for the same thickness in pixels
	size *= uiInfo.uiDC.yscale / uiInfo.uiDC.xscale;
	trap_R_DrawStretchPic( x, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x + w - size, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}

/*
================
_UI_DrawTopBottom
=================
*/
void _UI_DrawTopBottom(float x, float y, float w, float h, float size)
{
	trap_R_DrawStretchPic( x, y, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x, y + h - size, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}
/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void _UI_DrawRect( float x, float y, float width, float height, float size, const float *color )
{
	trap_R_SetColor( color );

	_UI_DrawTopBottom(x, y, width, height, size);
	_UI_DrawSides(x, y, width, height, size);

	trap_R_SetColor( NULL );
}

/*
=================
UI_UpdateCvars
=================
*/
void UI_UpdateCvars( void )
{
	size_t i = 0;
	const cvarTable_t *cv = NULL;

	for ( i=0, cv=cvarTable; i<cvarTableSize; i++, cv++ ) {
		if ( cv->vmCvar ) {
			int modCount = cv->vmCvar->modificationCount;
			Cvar_Update( cv->vmCvar );
			if ( cv->vmCvar->modificationCount != modCount ) {
				if ( cv->update )
					cv->update();
			}
		}
	}
}

/*
=================
UI_DrawEffects
=================
*/
static void UI_DrawEffects(rectDef_t *rect, float scale, vec4_t color)
{
	UI_DrawHandlePic( rect->x, rect->y - 14, 128, 8, 0/*uiInfo.uiDC.Assets.fxBasePic*/ );
	UI_DrawHandlePic( rect->x + uiInfo.effectsColor * 16 + 8, rect->y - 16, 16, 12, 0/*uiInfo.uiDC.Assets.fxPic[uiInfo.effectsColor]*/ );
}

/*
=================
UI_Version
=================
*/
static void UI_Version(rectDef_t *rect, float scale, vec4_t color, int iFontIndex)
{
	int width;

	width = DC->textWidth(JK_VERSION, scale, 0);

	DC->drawText(rect->x - width, rect->y, scale, color, JK_VERSION, 0, ITEM_TEXTSTYLE_SHADOWED, iFontIndex);
}

/*
=================
UI_DrawKeyBindStatus
=================
*/
static void UI_DrawKeyBindStatus(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iFontIndex)
{
	if (Display_KeyBindPending())
	{
#ifdef JK2_MODE
		Text_Paint(rect->x, rect->y, scale, color, ui.SP_GetStringTextString("MENUS_WAITINGFORKEY"), 0, textStyle, iFontIndex);
#else
		Text_Paint(rect->x, rect->y, scale, color, SE_GetString("MENUS_WAITINGFORKEY"), 0, textStyle, iFontIndex);
#endif
	}
	else
	{
//		Text_Paint(rect->x, rect->y, scale, color, ui.SP_GetStringTextString("MENUS_ENTERTOCHANGE"), 0, textStyle, iFontIndex);
	}
}

/*
=================
UI_DrawKeyBindStatus
=================
*/
static void UI_DrawGLInfo(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iFontIndex)
{
#define MAX_LINES 64
	char buff[4096];
	char * eptr = buff;
	const char *lines[MAX_LINES];
	int y, numLines=0, i=0;

	y = rect->y;
	Text_Paint(rect->x, y, scale, color, va("GL_VENDOR: %s",uiInfo.uiDC.glconfig.vendor_string), rect->w, textStyle, iFontIndex);
	y += 15;
	Text_Paint(rect->x, y, scale, color, va("GL_VERSION: %s: %s", uiInfo.uiDC.glconfig.version_string,uiInfo.uiDC.glconfig.renderer_string), rect->w, textStyle, iFontIndex);
	y += 15;
	Text_Paint(rect->x, y, scale, color, "GL_PIXELFORMAT:", rect->w, textStyle, iFontIndex);
	y += 15;
	Text_Paint(rect->x, y, scale, color, va ("Color(%d-bits) Z(%d-bits) stencil(%d-bits)",uiInfo.uiDC.glconfig.colorBits, uiInfo.uiDC.glconfig.depthBits, uiInfo.uiDC.glconfig.stencilBits), rect->w, textStyle, iFontIndex);
	y += 15;
	// build null terminated extension strings
	Q_strncpyz(buff, uiInfo.uiDC.glconfig.extensions_string, sizeof(buff));
	int testy=y-16;
	while ( testy <= rect->y + rect->h && *eptr && (numLines < MAX_LINES) )
	{
		while ( *eptr && *eptr == ' ' )
			*eptr++ = '\0';

		// track start of valid string
		if (*eptr && *eptr != ' ')
		{
			lines[numLines++] = eptr;
			testy+=16;
		}

		while ( *eptr && *eptr != ' ' )
			eptr++;
	}

	numLines--;
	while (i < numLines)
	{
		Text_Paint(rect->x, y, scale, color, lines[i++], rect->w, textStyle, iFontIndex);
		y += 16;
	}
}

/*
=================
UI_DataPad_Inventory
=================
*/
/*
static void UI_DataPad_Inventory(rectDef_t *rect, float scale, vec4_t color, int iFontIndex)
{
	Text_Paint(rect->x, rect->y, scale, color, "INVENTORY", 0, 1, iFontIndex);
}
*/
/*
=================
UI_DataPad_ForcePowers
=================
*/
/*
static void UI_DataPad_ForcePowers(rectDef_t *rect, float scale, vec4_t color, int iFontIndex)
{
	Text_Paint(rect->x, rect->y, scale, color, "FORCE POWERS", 0, 1, iFontIndex);
}
*/

static void UI_DrawCrosshair(rectDef_t *rect, float scale, vec4_t color) {
 	trap_R_SetColor( color );
	if (uiInfo.currentCrosshair < 0 || uiInfo.currentCrosshair >= NUM_CROSSHAIRS) {
		uiInfo.currentCrosshair = 0;
	}
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.uiDC.Assets.crosshairShader[uiInfo.currentCrosshair]);
 	trap_R_SetColor( NULL );
}


/*
=================
UI_OwnerDraw
=================
*/
static void UI_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle, int iFontIndex)
{
	rectDef_t rect;

	rect.x = x + text_x;
	rect.y = y + text_y;
	rect.w = w;
	rect.h = h;

	if (UI_Phys_OwnerDraw(ownerDraw, x + text_x, y + text_y, w, h, scale, color, textStyle, iFontIndex))
	{
		return;
	}

	switch (ownerDraw)
	{
		case UI_EFFECTS:
			UI_DrawEffects(&rect, scale, color);
			break;
		case UI_VERSION:
			UI_Version(&rect, scale, color, iFontIndex);
			break;

		case UI_DATAPAD_MISSION:
			ui.Draw_DataPad(DP_HUD);
			ui.Draw_DataPad(DP_OBJECTIVES);
			break;

		case UI_DATAPAD_WEAPONS:
			ui.Draw_DataPad(DP_HUD);
			ui.Draw_DataPad(DP_WEAPONS);
			break;

		case UI_DATAPAD_INVENTORY:
			ui.Draw_DataPad(DP_HUD);
			ui.Draw_DataPad(DP_INVENTORY);
			break;

		case UI_DATAPAD_FORCEPOWERS:
			ui.Draw_DataPad(DP_HUD);
			ui.Draw_DataPad(DP_FORCEPOWERS);
			break;

		case UI_DATAPAD_LOADOUT:
			ui.Draw_DataPad(DP_LOADOUT_FRAME);
			ui.Draw_DataPad(DP_LOADOUT);
			break;

		// The icon of the weapon of a slot of the character menu, drawn by the cgame (it has the weapon data).
		case UI_NPC_WEAPON_SLOT:
			VM_Call(CG_DRAW_WEAPON_SLOT, (intptr_t)7, (intptr_t)x, (intptr_t)y, (intptr_t)w, (intptr_t)h);
			UI_UpdateSaberVisibility(qtrue);
			break;

		case UI_PLAYER_WEAPON_SLOT_1:
		case UI_PLAYER_WEAPON_SLOT_2:
		case UI_PLAYER_WEAPON_SLOT_3:
		case UI_PLAYER_WEAPON_SLOT_4:
		case UI_PLAYER_WEAPON_SLOT_5:
		case UI_PLAYER_WEAPON_SLOT_6:
			VM_Call(CG_DRAW_WEAPON_SLOT, (intptr_t)(ownerDraw - UI_PLAYER_WEAPON_SLOT_1 + 1), (intptr_t)x, (intptr_t)y, (intptr_t)w, (intptr_t)h);
			if (ownerDraw == UI_PLAYER_WEAPON_SLOT_1)
			{
				UI_UpdateSaberVisibility(qtrue);
			}
			break;

		case UI_DATAPAD_CHARACTERS:
			ui.Draw_DataPad(DP_CHARACTERS);
			UI_UpdateSaberVisibility(qfalse);
			break;

		case UI_LIGHTEDIT_SWATCH:
			UI_LightEdit_DrawSwatch(x, y, w, h);
			break;

		case UI_LIGHTEDIT_STATS:
			UI_LightEdit_DrawStats(x, y, scale, color, iFontIndex);
			break;

		case UI_LIGHTEDIT_SKY:
			UI_LightEdit_DrawSky(x, y, scale, color, iFontIndex);
			break;

		case UI_ALLMAPS_SELECTION://saved game thumbnail

			int levelshot;
			levelshot = ui.R_RegisterShaderNoMip( va( "levelshots/%s", s_savedata[s_savegame.currentLine].currentSaveFileMap ) );
#ifdef JK2_MODE
			if (screenShotBuf[0])
			{
				ui.DrawStretchRaw( x, y, w, h, SG_SCR_WIDTH, SG_SCR_HEIGHT, screenShotBuf, 0, qtrue );
			}
			else
#endif
			if (levelshot)
			{
				ui.R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, levelshot );
			}
			else
			{
				UI_DrawHandlePic(x, y, w, h, uis.menuBackShader);
			}

			ui.R_Font_DrawString(	x,		// int ox
									y+h,	// int oy
									s_savedata[s_savegame.currentLine].currentSaveFileMap,	// const char *text
									color,	// paletteRGBA_c c
									iFontIndex,	// const int iFontHandle
									w,//-1,		// iMaxPixelWidth (-1 = none)
									scale,	// const float scale = 1.0f
									uiInfo.uiDC.widthRatioCoef);
			break;
		case UI_PREVIEWCINEMATIC:
			// FIXME BOB - make this work?
//			UI_DrawPreviewCinematic(&rect, scale, color);
			break;
		case UI_CROSSHAIR:
			UI_DrawCrosshair(&rect, scale, color);
			break;
		case UI_GLINFO:
			UI_DrawGLInfo(&rect,scale, color, textStyle, iFontIndex);
			break;
		case UI_KEYBINDSTATUS:
			UI_DrawKeyBindStatus(&rect,scale, color, textStyle, iFontIndex);
			break;
		default:
		  break;
	}

}

/*
=================
UI_OwnerDrawVisible
=================
*/
static qboolean UI_OwnerDrawVisible(int flags)
{
	qboolean vis = qtrue;

	while (flags)
	{
/*		if (flags & UI_SHOW_DEMOAVAILABLE)
		{
			if (!uiInfo.demoAvailable)
			{
				vis = qfalse;
			}
			flags &= ~UI_SHOW_DEMOAVAILABLE;
		}
		else
*/		{
			flags = 0;
		}
	}
	return vis;
}

/*
=================
Text_Width
=================
*/
int Text_Width(const char *text, float scale, int iFontIndex)
{
	// temp code until Bob retro-fits all menus to have font specifiers...
	//
	if ( iFontIndex == 0 )
	{
		iFontIndex = uiInfo.uiDC.Assets.qhMediumFont;
	}
	return ui.R_Font_StrLenPixels(text, iFontIndex, scale, uiInfo.uiDC.widthRatioCoef);
}

/*
=================
UI_OwnerDrawWidth
=================
*/
int UI_OwnerDrawWidth(int ownerDraw, float scale)
{
//	int i, h, value;
//	const char *text;
	const char *s = NULL;


	switch (ownerDraw)
	{
	case UI_KEYBINDSTATUS:
		if (Display_KeyBindPending())
		{
#ifdef JK2_MODE
			s = ui.SP_GetStringTextString("MENUS_WAITINGFORKEY");
#else
			s = SE_GetString("MENUS_WAITINGFORKEY");
#endif
		}
		else
		{
//			s = ui.SP_GetStringTextString("MENUS_ENTERTOCHANGE");
		}
		break;

	// FIXME BOB
//	case UI_SERVERREFRESHDATE:
//		s = UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource.integer));
//		break;
    default:
      break;
	}

	if (s)
	{
		return Text_Width(s, scale, 0);
	}
	return 0;
}

/*
=================
Text_Height
=================
*/
int Text_Height(const char *text, float scale, int iFontIndex)
{
	// temp until Bob retro-fits all menu files with font specifiers...
	//
	if ( iFontIndex == 0 )
	{
		iFontIndex = uiInfo.uiDC.Assets.qhMediumFont;
	}
	return ui.R_Font_HeightPixels(iFontIndex, scale);
}


/*
=================
UI_MouseEvent
=================
*/
//JLFMOUSE  CALLED EACH FRAME IN UI
void _UI_MouseEvent( int dx, int dy )
{
	// update mouse screen position
	uiInfo.uiDC.cursorx += dx;
	if (uiInfo.uiDC.cursorx < 0)
	{
		uiInfo.uiDC.cursorx = 0;
	}
	else if (uiInfo.uiDC.cursorx > SCREEN_WIDTH)
	{
		uiInfo.uiDC.cursorx = SCREEN_WIDTH;
	}

	uiInfo.uiDC.cursory += dy;
	if (uiInfo.uiDC.cursory < 0)
	{
		uiInfo.uiDC.cursory = 0;
	}
	else if (uiInfo.uiDC.cursory > SCREEN_HEIGHT)
	{
		uiInfo.uiDC.cursory = SCREEN_HEIGHT;
	}

	if (Menu_Count() > 0)
	{
    //menuDef_t *menu = Menu_GetFocused();
    //Menu_HandleMouseMove(menu, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
		Display_MouseMove(NULL, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
	}

}

/*
=================
UI_KeyEvent
=================
*/
void _UI_KeyEvent( int key, qboolean down )
{
/*	extern qboolean SwallowBadNumLockedKPKey( int iKey );
	if (SwallowBadNumLockedKPKey(key)){
		return;
	}
*/

	if (Menu_Count() > 0)
	{
		menuDef_t *menu = Menu_GetFocused();
		if (menu)
		{
			//DemoEnd();
			if (key == A_ESCAPE && down && !Menus_AnyFullScreenVisible() && !(menu->window.flags & WINDOW_IGNORE_ESCAPE))
			{
				Menus_CloseAll();
			}
			else
			{
				Menu_HandleKey(menu, key, down );
			}
		}
		else
		{
			trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
			trap_Key_ClearStates();
			Cvar_Set( "cl_paused", "0" );
		}
	}
}

/*
=================
UI_Report
=================
*/
void UI_Report(void)
{
  String_Report();
}



/*
=================
UI_DataPadMenu
=================
*/
void UI_DataPadMenu(void)
{
	int	newForcePower,newObjective;

	Menus_CloseByName("mainhud");

	newForcePower = (int)trap_Cvar_VariableValue("cg_updatedDataPadForcePower1");
	newObjective = (int)trap_Cvar_VariableValue("cg_updatedDataPadObjective");

	if (newForcePower)
	{
		Menus_ActivateByName("datapadForcePowersMenu");
	}
	else if (newObjective)
	{
		Menus_ActivateByName("datapadMissionMenu");
	}
	else
	{
		Menus_ActivateByName("datapadMissionMenu");
	}
	ui.Key_SetCatcher( KEYCATCH_UI );

}

void UI_SystemMenu(void)
{

	Menus_CloseByName("mainhud");

	Menus_ActivateByName("GameSelectionMenu");

	ui.Key_SetCatcher(KEYCATCH_UI);

}

void UI_PhysMenu(void)
{
	UI_Phys_LoadMenu();

	Menus_CloseByName("mainhud");

	Menus_ActivateByName("physEdit");

	ui.Key_SetCatcher(KEYCATCH_UI);
}

void UI_SpawnerMenu(void)
{

	Menus_CloseByName("mainhud");

	Menus_ActivateByName("ingameswglchars");

	ui.Key_SetCatcher(KEYCATCH_UI);

}

/*
=================
UI_InGameMenu
=================
*/
void UI_InGameMenu(const char*menuID)
{
#ifdef JK2_MODE
	ui.PrecacheScreenshot();
#endif
	Menus_CloseByName("mainhud");

	if (menuID)
	{
		Com_Printf(S_COLOR_CYAN"here\n");
		Menus_ActivateByName(menuID);
	}
	else
	{
		Menus_ActivateByName("ingameMainMenu");
	}
	ui.Key_SetCatcher( KEYCATCH_UI );
}

qboolean _UI_IsFullscreen( void )
{
	return Menus_AnyFullScreenVisible();
}

/*
=======================================================================

MAIN MENU

=======================================================================
*/


/*
===============
UI_MainMenu

The main menu only comes up when not in a game,
so make sure that the attract loop server is down
and that local cinematics are killed
===============
*/
void UI_MainMenu(void)
{
	char buf[256];
	ui.Cvar_Set("sv_killserver", "1");	// let the demo server know it should shut down

	ui.Key_SetCatcher( KEYCATCH_UI );

	menuDef_t *m = Menus_ActivateByName("mainMenu");
	if (!m)
	{	//wha? try again
		UI_LoadMenus("ui/menus.txt",qfalse);
	}
	ui.Cvar_VariableStringBuffer("com_errorMessage", buf, sizeof(buf));
	if (strlen(buf)) {
		Menus_ActivateByName("error_popmenu");
	}
}


/*
=================
Menu_Cache
=================
*/
void Menu_Cache( void )
{
	uis.cursor		= ui.R_RegisterShaderNoMip( "menu/new/crosshairb");
	// Common menu graphics
	uis.whiteShader = ui.R_RegisterShader( "white" );
	uis.menuBackShader = ui.R_RegisterShaderNoMip( "menu/art/unknownmap" );
}

/*
=================
UI_UpdateVideoSetup

Copies the temporary user interface version of the video cvars into
their real counterparts.  This is to create a interface which allows
you to discard your changes if you did something you didnt want
=================
*/
void UI_UpdateVideoSetup ( void )
{
	// Window mode: 0 windowed, 1 borderless window, 2 full screen.
	const int windowMode = Cvar_VariableIntegerValue( "ui_r_windowmode" );
	Cvar_Set ( "ui_r_fullscreen", windowMode == 2 ? "1" : "0" );
	Cvar_Set ( "r_noborder", windowMode == 1 ? "1" : "0" );

	// rd-vulkan takes the anisotropy level from r_ext_max_anisotropy. The other renderers
	// take it from r_ext_texture_filter_anisotropic, which the menu sets.
	char aniso[MAX_CVAR_VALUE_STRING];
	Cvar_PendingStringBuffer( "r_ext_texture_filter_anisotropic", aniso, sizeof( aniso ) );
	if ( atof( aniso ) >= 1.0f )
	{
		Cvar_Set ( "r_ext_max_anisotropy", va( "%d", Com_Clampi( 1, 16, (int)( atof( aniso ) + 0.5f ) ) ) );
	}

	Cvar_Set ( "r_mode", Cvar_VariableString ( "ui_r_mode" ) );
	Cvar_Set ( "r_fullscreen", Cvar_VariableString ( "ui_r_fullscreen" ) );
	Cvar_Set ( "r_colorbits", Cvar_VariableString ( "ui_r_colorbits" ) );
	Cvar_Set ( "r_lodbias", Cvar_VariableString ( "ui_r_lodbias" ) );
	Cvar_Set ( "r_picmip", Cvar_VariableString ( "ui_r_picmip" ) );
	Cvar_Set ( "r_texturebits", Cvar_VariableString ( "ui_r_texturebits" ) );
	Cvar_Set ( "r_texturemode", Cvar_VariableString ( "ui_r_texturemode" ) );
	Cvar_Set ( "r_detailtextures", Cvar_VariableString ( "ui_r_detailtextures" ) );
	Cvar_Set ( "r_ext_compress_textures", Cvar_VariableString ( "ui_r_ext_compress_textures" ) );
	Cvar_Set ( "r_depthbits", Cvar_VariableString ( "ui_r_depthbits" ) );
	Cvar_Set ( "r_subdivisions", Cvar_VariableString ( "ui_r_subdivisions" ) );
	Cvar_Set ( "r_fastSky", Cvar_VariableString ( "ui_r_fastSky" ) );
	Cvar_Set ( "r_inGameVideo", Cvar_VariableString ( "ui_r_inGameVideo" ) );
	Cvar_Set ( "r_allowExtensions", Cvar_VariableString ( "ui_r_allowExtensions" ) );
//	Cvar_Set ( "cg_shadows", Cvar_VariableString ( "ui_cg_shadows" ) );
	Cvar_Set ( "ui_r_modified", "0" );

	Cbuf_ExecuteText( EXEC_APPEND, "vid_restart;" );
}

/*
=================
UI_GetVideoSetup

Retrieves the current actual video settings into the temporary user
interface versions of the cvars.
=================
*/
void UI_GetVideoSetup ( void )
{
	Cvar_Register ( NULL, "ui_r_glCustom",				"4", CVAR_ARCHIVE );

	// Make sure the cvars are registered as read only.
	Cvar_Register ( NULL, "ui_r_mode",					"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_fullscreen",			"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_windowmode",			"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_colorbits",				"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_lodbias",				"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_picmip",				"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_texturebits",			"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_texturemode",			"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_detailtextures",		"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_ext_compress_textures",	"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_depthbits",				"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_subdivisions",			"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_fastSky",				"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_inGameVideo",			"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_allowExtensions",		"0", CVAR_ROM );
//	Cvar_Register ( NULL, "ui_cg_shadows",				"0", CVAR_ROM );
	Cvar_Register ( NULL, "ui_r_modified",				"0", CVAR_ROM );

	// Copy over the real video cvars into their temporary counterparts
	Cvar_Set ( "ui_r_mode", Cvar_VariableString ( "r_mode" ) );
	Cvar_Set ( "ui_r_colorbits", Cvar_VariableString ( "r_colorbits" ) );
	Cvar_Set ( "ui_r_fullscreen", Cvar_VariableString ( "r_fullscreen" ) );
	Cvar_Set ( "ui_r_windowmode", Cvar_VariableIntegerValue( "r_fullscreen" ) ? "2" :
								  Cvar_VariableIntegerValue( "r_noborder" ) ? "1" : "0" );
	Cvar_Set ( "ui_r_lodbias", Cvar_VariableString ( "r_lodbias" ) );
	Cvar_Set ( "ui_r_picmip", Cvar_VariableString ( "r_picmip" ) );
	Cvar_Set ( "ui_r_texturebits", Cvar_VariableString ( "r_texturebits" ) );
	Cvar_Set ( "ui_r_texturemode", Cvar_VariableString ( "r_texturemode" ) );
	Cvar_Set ( "ui_r_detailtextures", Cvar_VariableString ( "r_detailtextures" ) );
	Cvar_Set ( "ui_r_ext_compress_textures", Cvar_VariableString ( "r_ext_compress_textures" ) );
	Cvar_Set ( "ui_r_depthbits", Cvar_VariableString ( "r_depthbits" ) );
	Cvar_Set ( "ui_r_subdivisions", Cvar_VariableString ( "r_subdivisions" ) );
	Cvar_Set ( "ui_r_fastSky", Cvar_VariableString ( "r_fastSky" ) );
	Cvar_Set ( "ui_r_inGameVideo", Cvar_VariableString ( "r_inGameVideo" ) );
	Cvar_Set ( "ui_r_allowExtensions", Cvar_VariableString ( "r_allowExtensions" ) );
//	Cvar_Set ( "ui_cg_shadows", Cvar_VariableString ( "cg_shadows" ) );
	Cvar_Set ( "ui_r_modified", "0" );
}

static void UI_SetSexandSoundForModel(const char* char_model)
{
	int			f,i;
	char		soundpath[MAX_QPATH];
	qboolean	isFemale = qfalse;

	i = ui.FS_FOpenFile(va("models/players/%s/sounds.cfg", char_model), &f, FS_READ);
	if ( !f )
	{//no?  oh bother.
		Cvar_Reset("snd");
		Cvar_Reset("sex");
		return;
	}

	soundpath[0] = 0;

	ui.FS_Read(&soundpath, i, f);

	while (i >= 0 && soundpath[i] != '\n')
	{
		if (soundpath[i] == 'f')
		{
			isFemale = qtrue;
			soundpath[i] = 0;
		}

		i--;
	}

	i = 0;

	while (soundpath[i] && soundpath[i] != '\r' && soundpath[i] != '\n')
	{
		i++;
	}
	soundpath[i] = 0;

	ui.FS_FCloseFile(f);

	Cvar_Set ( "snd", soundpath);
	if (isFemale)
	{
		Cvar_Set ( "sex", "f");
	}
	else
	{
		Cvar_Set ( "sex", "m");
	}
}
static void UI_UpdateCharacterCvars(void)
{
	const char* char_model = Cvar_VariableString("ui_char_model");
	UI_SetSexandSoundForModel(char_model);
	Cvar_Set("g_char_model", char_model);
	Cvar_Set("g_char_skin_head", Cvar_VariableString("ui_char_skin_head"));
	Cvar_Set("g_char_skin_torso", Cvar_VariableString("ui_char_skin_torso"));
	Cvar_Set("g_char_skin_legs", Cvar_VariableString("ui_char_skin_legs"));
	Cvar_Set("g_char_color_red", Cvar_VariableString("ui_char_color_red"));
	Cvar_Set("g_char_color_green", Cvar_VariableString("ui_char_color_green"));
	Cvar_Set("g_char_color_blue", Cvar_VariableString("ui_char_color_blue"));
	Cvar_Set("g_forceLightningColor", Cvar_VariableString("ui_lightning_color"));
	Cvar_Set("g_weaponOne", Cvar_VariableString("ui_weaponOne"));
	Cvar_Set("g_weaponTwo", Cvar_VariableString("ui_weaponTwo"));
	Cvar_Set("g_weaponThree", Cvar_VariableString("ui_weaponThree"));
	Cvar_Set("g_weaponFour", Cvar_VariableString("ui_weaponFour"));
	Cvar_Set("g_weaponFive", Cvar_VariableString("ui_weaponFive"));
	Cvar_Set("g_weaponSix", Cvar_VariableString("ui_weaponSix"));
}

static void UI_UpdateNPCCvars()
{
	Cvar_Set("g_NPCtype", Cvar_VariableString("ui_npc_type"));
	// Character variant of the NPC: its random parts (.cha) are picked at the spawn.
	Cvar_Set("g_NPCcharKey", Cvar_VariableString("g_charKey"));
	Cvar_Set("g_NPCvariant", Cvar_VariableString("ui_variant_code"));
	Cvar_Set("g_NPCmodel", Cvar_VariableString("ui_char_model"));
	Cvar_Set("g_NPChead", Cvar_VariableString("ui_char_skin_head"));
	Cvar_Set("g_NPCtorso", Cvar_VariableString("ui_char_skin_torso"));
	Cvar_Set("g_NPClegs", Cvar_VariableString("ui_char_skin_legs"));
	Cvar_Set("g_NPCteam", Cvar_VariableString("ui_team"));
	Cvar_Set("g_NPCweapon", Cvar_VariableString("ui_npc_weapon"));
	Cvar_Set("g_NPCsaber", Cvar_VariableString("ui_saber"));
	Cvar_Set("g_NPCtargetname", Cvar_VariableString("ui_npc_targetname"));
	Cvar_Set("g_NPCsabercolor", Cvar_VariableString("ui_saber_color"));
	Cvar_Set("g_NPCsabertwo", Cvar_VariableString("ui_saber2"));
	Cvar_Set("g_NPCsabertwocolor", Cvar_VariableString("ui_saber2_color"));
	Cvar_Set("g_NPCLightningColor", Cvar_VariableString("ui_lightning_color"));
	Cvar_Set("g_NPCmoveset", Cvar_VariableString("ui_char_moveset"));
	Cvar_Set("g_NPCspawnscript", Cvar_VariableString("ui_npc_spawnscript"));
	Cvar_Set("g_NPCfleescript", Cvar_VariableString("ui_npc_fleescript"));
	Cvar_Set("g_NPCdeathscript", Cvar_VariableString("ui_npc_deathscript"));
	Cvar_Set("g_NPChealth", Cvar_VariableString("ui_health"));
	Cvar_Set("g_npc_color_red", Cvar_VariableString("ui_char_color_red"));
	Cvar_Set("g_npc_color_green", Cvar_VariableString("ui_char_color_green"));
	Cvar_Set("g_npc_color_blue", Cvar_VariableString("ui_char_color_blue"));

	// Force Powers
	Cvar_Set("g_npc_jump_level", Cvar_VariableString("ui_jump_level"));
	Cvar_Set("g_npc_push_level", Cvar_VariableString("ui_push_level"));
	Cvar_Set("g_npc_pull_level", Cvar_VariableString("ui_pull_level"));
	Cvar_Set("g_npc_speed_level", Cvar_VariableString("ui_speed_level"));
	Cvar_Set("g_npc_seeing_level", Cvar_VariableString("ui_seeing_level"));

	Cvar_Set("g_npc_absorb_level", Cvar_VariableString("ui_absorb_level"));
	Cvar_Set("g_npc_heal_level", Cvar_VariableString("ui_heal_level"));
	Cvar_Set("g_npc_protect_level", Cvar_VariableString("ui_protect_level"));
	Cvar_Set("g_npc_telepathy_level", Cvar_VariableString("ui_telepathy_level"));
	Cvar_Set("g_npc_stasis_level", Cvar_VariableString("ui_stasis_level"));
	Cvar_Set("g_npc_grasp_level", Cvar_VariableString("ui_grasp_level"));
	Cvar_Set("g_npc_blast_level", Cvar_VariableString("ui_blast_level"));

	Cvar_Set("g_npc_grip_level", Cvar_VariableString("ui_grip_level"));
	Cvar_Set("g_npc_lightning_level", Cvar_VariableString("ui_lightning_level"));
	Cvar_Set("g_npc_drain_level", Cvar_VariableString("ui_drain_level"));
	Cvar_Set("g_npc_rage_level", Cvar_VariableString("ui_rage_level"));
	Cvar_Set("g_npc_destruction_level", Cvar_VariableString("ui_destruction_level"));
	Cvar_Set("g_npc_fear_level", Cvar_VariableString("ui_fear_level"));
	Cvar_Set("g_npc_strike_level", Cvar_VariableString("ui_strike_level"));

	Cvar_Set("g_npc_saber_attack_level", Cvar_VariableString("ui_saboff_level"));
	Cvar_Set("g_npc_saber_defend_level", Cvar_VariableString("ui_sabdef_level"));
	Cvar_Set("g_npc_saber_throw_level", Cvar_VariableString("ui_sabthrow_level"));

	// Saber Styles
	Cvar_Set("g_npc_saber_styles", Cvar_VariableString("ui_saber_styles"));
}

static void UI_GetCharacterCvars ( void )
{
	Cvar_Set ( "ui_char_skin_head", Cvar_VariableString ( "g_char_skin_head" ) );
	Cvar_Set ( "ui_char_skin_torso", Cvar_VariableString ( "g_char_skin_torso" ) );
	Cvar_Set ( "ui_char_skin_legs", Cvar_VariableString ( "g_char_skin_legs" ) );
	Cvar_Set ( "ui_char_color_red", Cvar_VariableString ( "g_char_color_red" ) );
	Cvar_Set ( "ui_char_color_green", Cvar_VariableString ( "g_char_color_green" ) );
	Cvar_Set ( "ui_char_color_blue", Cvar_VariableString ( "g_char_color_blue" ) );

	const char* model = Cvar_VariableString ( "g_char_model" );
	Cvar_Set ( "ui_char_model", model );

	for (int i = 0; i < uiInfo.playerSpeciesCount; i++)
	{
		if ( !Q_stricmp(model, uiInfo.playerSpecies[i].Name) )
		{
			uiInfo.playerSpeciesIndex = i;
		}
	}
}

static void UI_RandomSkin(void)
{
	for (int i = 0; i < uiInfo.playerSpeciesCount; i++)
	{
		if (!Q_stricmp(Cvar_VariableString("ui_char_model"), uiInfo.playerSpecies[i].Name))
		{
			// Usual menu and item variables
			menuDef_t* menu;
			itemDef_t* item;
			menu = Menu_GetFocused();
			item = (itemDef_s*)Menu_FindItemByName(menu, "character");

			// Update the feeders so they'll reflect whatever is picked. Passing a random number since we don't need to store or save anything.
			UI_FeederSelection(FEEDER_PLAYER_SKIN_HEAD, Q_irand(0, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount - 1), item);
			UI_FeederSelection(FEEDER_PLAYER_SKIN_TORSO, Q_irand(0, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount - 1), item);
			UI_FeederSelection(FEEDER_PLAYER_SKIN_LEGS, Q_irand(0, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount - 1), item);

			return;

		}
	}
}

static void UI_UpdateTeamSelect(void)
{
	// Usual menu and item variables
	menuDef_t* menu;
	itemDef_t* enabled;
	itemDef_t* disabled;
	menu = Menu_GetFocused();
	enabled = (itemDef_s*)Menu_FindItemByName(menu, "teamsToggle");
	disabled = (itemDef_s*)Menu_FindItemByName(menu, "teamsDisabled");

	if (!enabled || !disabled)
	{
		return;
	}

	if (Cvar_VariableIntegerValue("ui_npc_menu") >= 1)
	{
		Menu_ShowItemByName(menu, "teamsToggle", qtrue);
		Menu_ShowItemByName(menu, "teamsDisabled", qfalse);
	}
	else
	{
		if (Cvar_VariableIntegerValue("g_allowAlignmentChange"))
		{
			if (enabled->window.flags & WINDOW_VISIBLE || disabled->window.flags & WINDOW_VISIBLE)
			{
				Menu_ShowItemByName(menu, "teamsToggle", qtrue);
				Menu_ShowItemByName(menu, "teamsDisabled", qfalse);
			}
		}
		else
		{
			if (enabled->window.flags & WINDOW_VISIBLE || disabled->window.flags & WINDOW_VISIBLE)
			{
				Menu_ShowItemByName(menu, "teamsToggle", qfalse);
				Menu_ShowItemByName(menu, "teamsDisabled", qtrue);
			}
		}
	}
}

/*
=================
Preview of the character of the character menu

The weapon of the slot is in the hand of the character, with the pose the game gives to it.
The pose of a saber is the saber stance chosen with ui_char_preview_stance.
=================
*/
static const int uiPreviewStances[] = { SS_TAVION, SS_FAST, SS_MEDIUM, SS_STRONG, SS_DESANN };
static const char *uiPreviewStanceNames[] = { "Tavion", "Light", "Medium", "Heavy", "Desann" };
#define UI_PREVIEW_STANCES	((int)(sizeof(uiPreviewStances) / sizeof(uiPreviewStances[0])))

static int UI_PreviewStanceIndex(void)
{
	const int index = Cvar_VariableIntegerValue("ui_char_preview_stance");
	return (index >= 0 && index < UI_PREVIEW_STANCES) ? index : 2;
}

// SS_DUAL or SS_STAFF when the sabers of the menu force the stance, else 0. The player has no choice there either.
static int UI_PreviewForcedStance(void)
{
	char saberType[MAX_QPATH] = { 0 };
	const char *saber2 = Cvar_VariableString("ui_saber2");

	if (saber2[0] && Q_stricmp(saber2, "empty") && Q_stricmp(saber2, "none"))
	{
		return SS_DUAL;
	}
	UI_SaberTypeForSaber(Cvar_VariableString("ui_saber"), saberType);
	return (saberType[0] && TranslateSaberType(saberType) == SABER_STAFF) ? SS_STAFF : 0;
}

static itemDef_t *UI_PreviewItem(void)
{
	menuDef_t *menu = Menus_FindByName("IngameSWGLChars");
	return menu ? (itemDef_t *)Menu_FindItemByName(menu, "character") : NULL;
}

// Kind of the weapon of the preview (CHARPREVIEW_), its .glm and the idle torso animation of the game with it (-1: none).
static int UI_PreviewWeapon(char *model, int modelSize, int *torsoAnim)
{
	char buffer[MAX_QPATH];
	int torso = -1;
	const int kind = (int)VM_Call(CG_GET_PREVIEW_WEAPON, (intptr_t)(Cvar_VariableIntegerValue("ui_npc_menu") >= 1), (intptr_t)buffer, (intptr_t)sizeof(buffer), (intptr_t)&torso);
	Q_strncpyz(model, buffer, modelSize);
	if (torsoAnim)
	{
		*torsoAnim = torso;
	}
	return kind;
}

// The moveset of the spawn: the choice of the menu, else the animOverride of the .npc (the player too: "playermodel <npc>" reads it),
// else the one of the model. "default" is _humanoid.gla.
static void UI_PreviewApplyMoveset(CGhoul2Info *ghoul2)
{
	const char *moveset = Cvar_VariableString("ui_char_moveset");
	char npcMoveset[MAX_QPATH];
	char glaPath[MAX_QPATH] = { 0 };

	if (!moveset[0] || !Q_stricmp(moveset, "none"))
	{
		if (VM_Call(CG_GET_NPC_ANIM_OVERRIDE, (intptr_t)Cvar_VariableString("ui_npc_type"), (intptr_t)npcMoveset, (intptr_t)sizeof(npcMoveset)))
		{
			moveset = npcMoveset;
		}
	}

	if (!Q_stricmp(moveset, "default"))
	{
		Q_strncpyz(glaPath, GLA_HUMANOID_PATH, sizeof(glaPath));
	}
	else if (moveset[0] && Q_stricmp(moveset, "none"))
	{
		GLA_OverridePath(moveset, glaPath, sizeof(glaPath));
	}
	re.G2API_SetAnimOverride(ghoul2, glaPath);
}

// "pitch roll" of the torso for the idle torso animation of a weapon class: the cvar ui_char_aim_<class>. Other animations: no lean.
static const char *UI_PreviewAimText(int torsoAnim)
{
	switch (torsoAnim)
	{
	case TORSO_Z6_AIM:			return ui_char_aim[0].string;
	case TORSO_PLX_AIM:			return ui_char_aim[1].string;
	case TORSO_WEAPONIDLE2:		return ui_char_aim[2].string;
	case TORSO_WEAPONREADY3:
	case TORSO_WEAPONIDLE3:		return ui_char_aim[3].string;
	}
	return "0 0";
}

// qtrue if the model has this animation.
static qboolean UI_PreviewHasAnim(CGhoul2Info *ghoul2, int anim)
{
	const char *glaName = re.G2API_GetGLAName(ghoul2);
	int animIndex;

	if (!glaName || !glaName[0])
	{
		return qfalse;
	}
	UI_ParseAnimFileSet(glaName, &animIndex);
	return (qboolean)(animIndex != -1 && ui_knownAnimFileSets[animIndex].animations[anim].numFrames > 0);
}

// Plays the animation on a bone with the flags of PM_SetAnimFinal.
static void UI_PreviewSetAnim(CGhoul2Info *ghoul2, const char *boneName, int animNum)
{
	const char *glaName = re.G2API_GetGLAName(ghoul2);
	int animIndex;

	if (!glaName || !glaName[0])
	{
		return;
	}
	UI_ParseAnimFileSet(glaName, &animIndex);
	if (animIndex == -1)
	{
		return;
	}

	const animation_t *anim = &ui_knownAnimFileSets[animIndex].animations[animNum];
	if (anim->numFrames > 0)
	{
		re.G2API_SetBoneAnim(ghoul2, boneName, anim->firstFrame, anim->firstFrame + anim->numFrames, ((anim->loopFrames != -1) ? BONE_ANIM_OVERRIDE_LOOP : BONE_ANIM_OVERRIDE_FREEZE) | BONE_ANIM_BLEND, 50.0f / anim->frameLerp, uiInfo.uiDC.realTime, -1, 150);
	}
}

// The character model was loaded again: the next update of the preview plays its pose again.
void UI_CharPreviewReset(void)
{
	uiPreviewSig[0] = 0;
}

// qtrue if the preview shows a saber: the blades are drawn only then.
qboolean UI_CharPreviewHidesSaber(itemDef_t *item)
{
	return (qboolean)(uiPreviewHideSaber && item && item == UI_PreviewItem());
}

// Puts the weapon of the slot in the hand of the preview, when it is not a saber. qfalse: the caller attaches the sabers.
qboolean UI_CharPreviewAttachWeapon(itemDef_t *item)
{
	if (!item || item != UI_PreviewItem())
	{
		return qfalse;
	}

	char model[MAX_QPATH];
	const int kind = UI_PreviewWeapon(model, sizeof(model), NULL);
	uiPreviewHideSaber = (qboolean)(kind != CHARPREVIEW_SABER);
	if (kind == CHARPREVIEW_SABER)
	{
		return qfalse;
	}
	if (model[0])
	{
		const int g2Weapon = DC->g2_InitGhoul2Model(item->ghoul2, model, 0, 0, 0, 0, 0);
		if (g2Weapon > 0)
		{
			const int boltNum = DC->g2_AddBolt(&item->ghoul2[0], "*r_hand");
			re.G2API_AttachG2Model(&item->ghoul2[g2Weapon], &item->ghoul2[0], boltNum, 0);
		}
	}
	return qtrue;
}

// "previewStance": next saber stance of the preview.
static void UI_PreviewNextStance(void)
{
	Cvar_Set("ui_char_preview_stance", va("%d", (UI_PreviewStanceIndex() + 1) % UI_PREVIEW_STANCES));
}

// Attaches the weapon and plays the pose of the preview when one of them changed.
static void UI_UpdateCharPreview(menuDef_t *menu)
{
	itemDef_t *item = (itemDef_t *)Menu_FindItemByName(menu, "character");
	if (!item || !item->typeData || !item->ghoul2.size() || item->ghoul2[0].mModelindex < 0)
	{
		return;
	}

	char model[MAX_QPATH];
	int torsoAnim;
	const int kind = UI_PreviewWeapon(model, sizeof(model), &torsoAnim);
	const int forcedStance = (kind == CHARPREVIEW_SABER) ? UI_PreviewForcedStance() : 0;
	const int stance = forcedStance ? 100 + forcedStance : UI_PreviewStanceIndex();

	// The count of ghoul2 models is in the signature: a model that the menu rebuilds loses the weapon.
	auto makeSig = [&](char *sig, int sigSize) {
		Com_sprintf(sig, sigSize, "%d|%s|%d|%d|%d|%s|%s|%s|%s|%s|%s", kind, model, stance, (int)item->ghoul2[0].mModel, (int)item->ghoul2.size(),
			UI_PreviewAimText(torsoAnim), Cvar_VariableString("ui_char_model"), Cvar_VariableString("ui_saber"), Cvar_VariableString("ui_saber2"), Cvar_VariableString("ui_char_moveset"), Cvar_VariableString("ui_npc_type"));
		};
	char sig[sizeof(uiPreviewSig)];
	makeSig(sig, sizeof(sig));
	if (!strcmp(sig, uiPreviewSig))
	{
		return;
	}

	// The animations are the ones of the GLA the model has now: the moveset first. A model that is not ready has no animation set yet: try again at the next frame.
	CGhoul2Info *ghoul2 = &item->ghoul2[0];
	UI_PreviewApplyMoveset(ghoul2);
	if (!UI_PreviewHasAnim(ghoul2, BOTH_STAND1))
	{
		return;
	}

	if (forcedStance)
	{
		Cvar_Set("ui_char_preview_stance_label", va("Stance: %s  (set by the weapon)", (forcedStance == SS_DUAL) ? "Dual" : "Staff"));
	}
	else
	{
		Cvar_Set("ui_char_preview_stance_label", va("Stance: %s  (click to change)", uiPreviewStanceNames[stance]));
	}
	UI_SaberAttachToChar(item);
	makeSig(uiPreviewSig, sizeof(uiPreviewSig));

	int baseAnim = BOTH_STAND1;
	switch (kind)
	{
	case CHARPREVIEW_SABER:
		// Same stances and fallbacks as PM_ReadyPoseForSaberAnimLevel.
		baseAnim = BOTH_STAND2;
		if (forcedStance)
		{
			baseAnim = (forcedStance == SS_DUAL) ? BOTH_SABERDUAL_STANCE : BOTH_SABERSTAFF_STANCE;
			if (!UI_PreviewHasAnim(ghoul2, baseAnim))
			{
				baseAnim = BOTH_STAND2;
			}
		}
		else if (uiPreviewStances[stance] == SS_FAST)
		{
			baseAnim = BOTH_SABERFAST_STANCE;
		}
		else if (uiPreviewStances[stance] == SS_STRONG)
		{
			baseAnim = BOTH_SABERSLOW_STANCE;
		}
		else if (uiPreviewStances[stance] == SS_TAVION)
		{
			baseAnim = UI_PreviewHasAnim(ghoul2, BOTH_SABERTAVION_STANCE) ? BOTH_SABERTAVION_STANCE : BOTH_SABERFAST_STANCE;
		}
		else if (uiPreviewStances[stance] == SS_DESANN && UI_PreviewHasAnim(ghoul2, BOTH_SABERDESSAN_STANCE))
		{
			baseAnim = BOTH_SABERDESSAN_STANCE;
		}
		break;
	}
	if (kind != CHARPREVIEW_OTHER || !UI_PreviewHasAnim(ghoul2, torsoAnim))
	{
		torsoAnim = -1;
	}

	((modelDef_t *)item->typeData)->g2anim = baseAnim;
	uiInfo.moveAnimTime = 0;
	UI_PreviewSetAnim(ghoul2, "model_root", baseAnim);
	if (torsoAnim >= 0)
	{
		UI_PreviewSetAnim(ghoul2, "lower_lumbar", torsoAnim);
	}
	else
	{
		re.G2API_StopBoneAnim(ghoul2, "lower_lumbar");
	}

	// The game pitches the spine with the view angle (CG_G2ClientSpineAngles): the preview aims a little down with a weapon.
	static const struct { const char *bone; float share; } spine[] = { { "lower_lumbar", 0.4f }, { "upper_lumbar", 0.4f }, { "thoracic", 0.2f } };
	float aimPitch = 0.0f, aimRoll = 0.0f;
	sscanf(UI_PreviewAimText(torsoAnim), "%f %f", &aimPitch, &aimRoll);
	for (int i = 0; i < 3; i++)
	{
		if (aimPitch != 0.0f || aimRoll != 0.0f)
		{
			const vec3_t angles = { aimPitch * spine[i].share, 0.0f, aimRoll * spine[i].share };
			re.G2API_SetBoneAngles(ghoul2, spine[i].bone, angles, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, NULL, 0, uiInfo.uiDC.realTime);
		}
		else
		{
			re.G2API_StopBoneAngles(ghoul2, spine[i].bone);
		}
	}
}

// weaponsTab: called from the weapon slots of the weapons tab, else from the ownerdraw that every tab of the character menu draws.
static void UI_UpdateSaberVisibility(qboolean weaponsTab)
{
	menuDef_t* menu = Menu_GetFocused();

	if (!menu || Q_stricmp(menu->window.name, "IngameSWGLChars"))
	{
		return;
	}

	// The preview is on the character screen only; the saber items belong to the weapons tab.
	const itemDef_t *preview = (itemDef_t *)Menu_FindItemByName(menu, "character");
	const qboolean onCharScreen = (qboolean)(preview && (preview->window.flags & WINDOW_VISIBLE));

	auto SetSaberVisibility = [&](qboolean visible) {
		if (weaponsTab)
		{
			Menu_ShowItemByName(menu, "saber", visible);
			Menu_ShowItemByName(menu, "saber2", visible);
			Menu_ShowItemByName(menu, "SaberEdit", visible);
		}
		Menu_ShowItemByName(menu, "previewStance", (qboolean)(visible && onCharScreen));
		};

	qboolean showSabers = qfalse;

	if (Cvar_VariableIntegerValue("ui_npc_menu") >= 1)
	{
		// NPC menu mode: single weapon cvar
		if (!Q_stricmp(Cvar_VariableString("ui_npc_weapon"), "weapon_saber") || !Q_stricmp(Cvar_VariableString("ui_npc_weapon"), "WP_SABER"))
		{
			showSabers = qtrue;
		}
	}
	else
	{
		// Player menu mode: multiple weapon slots
		const char* weapons[] = {
			"ui_weaponOne", "ui_weaponTwo", "ui_weaponThree",
			"ui_weaponFour", "ui_weaponFive", "ui_weaponSix"
		};

		for (int i = 0; i < 6; i++)
		{
			if (!Q_stricmp(Cvar_VariableString(weapons[i]), "weapon_saber") || !Q_stricmp(Cvar_VariableString(weapons[i]), "WP_SABER"))
			{
				showSabers = qtrue;
				break;
			}
		}
	}

	SetSaberVisibility(showSabers);
	UI_UpdateCharPreview(menu);
}

static void UI_RandomRGB(void)
{
	Cvar_Set("ui_char_color_red", va("%i", Q_irand(0, 255)));
	Cvar_Set("ui_char_color_green", va("%i", Q_irand(0, 255)));
	Cvar_Set("ui_char_color_blue", va("%i", Q_irand(0, 255)));
}

static void UI_GetCharacterCustomization(void)
{
	for (int i = 0; i < uiInfo.playerSpeciesCount; i++)
	{
		if (!Q_stricmp(Cvar_VariableString("ui_char_model"), uiInfo.playerSpecies[i].Name))
		{
			uiInfo.playerSpeciesIndex = i;
		}
	}
}

extern saber_colors_t TranslateSaberColor( const char *name );

static void UI_UpdateSaberCvars ( void )
{
	if (!Cvar_VariableIntegerValue("ui_saber_edit") && !Cvar_VariableIntegerValue("ui_npc_menu"))
	{
		Cvar_Set("g_saber_type", Cvar_VariableString("ui_saber_type"));
		Cvar_Set("g_saber", Cvar_VariableString("ui_saber"));
		Cvar_Set("g_saber2", Cvar_VariableString("ui_saber2"));
		Cvar_Set("g_saber_color", Cvar_VariableString("ui_saber_color"));
		Cvar_Set("g_saber2_color", Cvar_VariableString("ui_saber2_color"));
	}

	if (TranslateSaberColor(Cvar_VariableString("ui_saber_color")) >= SABER_RGB)
	{
		char rgbColor[8];

		if(Cvar_VariableIntegerValue("ui_rgb_saber_red") < 0)
			Cvar_Set("ui_rgb_saber_red", 0);
		if (Cvar_VariableIntegerValue("ui_rgb_saber_green") < 0)
			Cvar_Set("ui_rgb_saber_green", 0);
		if (Cvar_VariableIntegerValue("ui_rgb_saber_blue") < 0)
			Cvar_Set("ui_rgb_saber_blue", 0);

		Com_sprintf(rgbColor, 8, "x%02x%02x%02x", Cvar_VariableIntegerValue("ui_rgb_saber_red"),
					(Cvar_VariableIntegerValue("ui_rgb_saber_green")),
					(Cvar_VariableIntegerValue("ui_rgb_saber_blue")));
		if (!Cvar_VariableIntegerValue("ui_npc_menu"))
		{
			Cvar_Set("g_saber_color", rgbColor);
		}
		else
		{
			Cvar_Set("g_NPCsabercolor", rgbColor);
		}
	}

	if (TranslateSaberColor(Cvar_VariableString("ui_saber2_color")) >= SABER_RGB)
	{
		if (Cvar_VariableIntegerValue("ui_rgb_saber2_red") < 0)
			Cvar_Set("ui_rgb_saber2_red", 0);
		if (Cvar_VariableIntegerValue("ui_rgb_saber2_green") < 0)
			Cvar_Set("ui_rgb_saber2_green", 0);
		if (Cvar_VariableIntegerValue("ui_rgb_saber2_blue") < 0)
			Cvar_Set("ui_rgb_saber2_blue", 0);

		char rgbColor[8];
		Com_sprintf(rgbColor, 8, "x%02x%02x%02x", Cvar_VariableIntegerValue("ui_rgb_saber2_red"),
					(Cvar_VariableIntegerValue("ui_rgb_saber2_green")),
					(Cvar_VariableIntegerValue("ui_rgb_saber2_blue")));
		if (!Cvar_VariableIntegerValue("ui_npc_menu"))
		{
			Cvar_Set( "g_saber2_color", rgbColor );
		}
		else
		{
			Cvar_Set("g_NPCsabertwocolor", rgbColor);
		}
	}

	if (!Cvar_VariableString("ui_saber2"))
	{
		client_t* cl = &svs.clients[0];	// 0 because only ever us as a player
		playerState_t* ps;

		ps = cl->gentity->client;

		ps->dualSabers = qfalse;
	}

}

static void UI_RGBSaberCvars(void)
{
	saber_colors_t saberColour = TranslateSaberColor(Cvar_VariableString("ui_saber_color"));

	if (saberColour >= SABER_RGB)
	{

		Cvar_SetValue("ui_rgb_saber_red", (saberColour & 0xff));
		Cvar_SetValue("ui_rgb_saber_green", ((saberColour >> 8) & 0xff));
		Cvar_SetValue("ui_rgb_saber_blue", ((saberColour >> 16) & 0xff));
	}

	saber_colors_t saber2Colour = TranslateSaberColor(Cvar_VariableString("ui_saber2_color"));

	if (saber2Colour >= SABER_RGB)
	{

		Cvar_SetValue("ui_rgb_saber2_red", (saber2Colour & 0xff));
		Cvar_SetValue("ui_rgb_saber2_green", ((saber2Colour >> 8) & 0xff));
		Cvar_SetValue("ui_rgb_saber2_blue", ((saber2Colour >> 16) & 0xff));
	}
}

#ifndef JK2_MODE
static void UI_UpdateFightingStyleChoices ( void )
{
	if (Cvar_VariableIntegerValue("ui_npc_saber"))
		return;
	//
	if (!Q_stricmp("staff",Cvar_VariableString ( "ui_saber_type" )))
	{
		Cvar_Set ( "ui_fightingstylesallowed", "0" );
		Cvar_Set ( "ui_newfightingstyle", "4" );		// SS_STAFF
	}
	else if (!Q_stricmp("dual",Cvar_VariableString ( "ui_saber_type" )))
	{
		Cvar_Set ( "ui_fightingstylesallowed", "0" );
		Cvar_Set ( "ui_newfightingstyle", "3" );		// SS_DUAL
	}
	else
	{
		// Get player state
		client_t	*cl = &svs.clients[0];	// 0 because only ever us as a player
		playerState_t	*pState;

		if (cl && cl->gentity && cl->gentity->client)
		{
			pState = cl->gentity->client;


			// Knows Fast style?
			if (pState->saberStylesKnown & (1<<SS_FAST))
			{
				// And Medium?
				if (pState->saberStylesKnown & (1<<SS_MEDIUM))
				{
					Cvar_Set ( "ui_fightingstylesallowed", "6" );	// Has FAST and MEDIUM, so can only choose STRONG
					Cvar_Set ( "ui_newfightingstyle", "2" );		// STRONG
				}
				else
				{
					Cvar_Set ( "ui_fightingstylesallowed", "1" );	// Has FAST, so can choose from MEDIUM and STRONG
					Cvar_Set ( "ui_newfightingstyle", "1" );		// MEDIUM
				}
			}
			// Knows Medium style?
			else if (pState->saberStylesKnown & (1<<SS_MEDIUM))
			{
				// And Strong?
				if (pState->saberStylesKnown & (1<<SS_STRONG))
				{
					Cvar_Set ( "ui_fightingstylesallowed", "4" );	// Has MEDIUM and STRONG, so can only choose FAST
					Cvar_Set ( "ui_newfightingstyle", "0" );		// FAST
				}
				else
				{
					Cvar_Set ( "ui_fightingstylesallowed", "2" );	// Has MEDIUM, so can choose from FAST and STRONG
					Cvar_Set ( "ui_newfightingstyle", "0" );		// FAST
				}
			}
			// Knows Strong style?
			else if (pState->saberStylesKnown & (1<<SS_STRONG))
			{
				// And Fast
				if (pState->saberStylesKnown & (1<<SS_FAST))
				{
					Cvar_Set ( "ui_fightingstylesallowed", "5" );	// Has STRONG and FAST, so can only take MEDIUM
					Cvar_Set ( "ui_newfightingstyle", "1" );		// MEDIUM
				}
				else
				{
					Cvar_Set ( "ui_fightingstylesallowed", "3" );	// Has STRONG, so can choose from FAST and MEDIUM
					Cvar_Set ( "ui_newfightingstyle", "1" );		// MEDIUM
				}
			}
			else		// They have nothing, which should not happen
			{
				Cvar_Set ( "ui_currentfightingstyle", "1" );		// Default MEDIUM
				Cvar_Set ( "ui_newfightingstyle", "0" );			// FAST??
				Cvar_Set ( "ui_fightingstylesallowed", "0" );		// Default to no new styles allowed
			}

			// Determine current style
			if (pState->saberAnimLevel == SS_FAST)
			{
				Cvar_Set ( "ui_currentfightingstyle", "0" );			// FAST
			}
			else if (pState->saberAnimLevel == SS_STRONG)
			{
				Cvar_Set ( "ui_currentfightingstyle", "2" );			// STRONG
			}
			else
			{
				Cvar_Set ( "ui_currentfightingstyle", "1" );			// default MEDIUM
			}
		}
		else	// No client so this must be first time
		{
			Cvar_Set ( "ui_currentfightingstyle", "1" );		// Default to MEDIUM
			Cvar_Set ( "ui_fightingstylesallowed", "0" );		// Default to no new styles allowed
			Cvar_Set ( "ui_newfightingstyle", "1" );			// MEDIUM
		}
	}
}
#endif // !JK2_MODE

#define MAX_POWER_ENUMS 22

typedef struct {
	const char	*title;
	short	powerEnum;
} powerEnum_t;

static powerEnum_t powerEnums[MAX_POWER_ENUMS] =
{
#ifndef JK2_MODE
	{ "absorb",		FP_ABSORB },
#endif // !JK2_MODE

	{ "heal",			FP_HEAL },
	{ "mindtrick",	FP_TELEPATHY },

#ifndef JK2_MODE
	{ "protect",		FP_PROTECT },
	{ "stasis",		FP_STASIS },
	{ "blast",		FP_BLAST},
	{ "grasp",		FP_GRASP },
#endif // !JK2_MODE

				// Core powers
	{ "jump",			FP_LEVITATION },
	{ "pull",			FP_PULL },
	{ "push",			FP_PUSH },

#ifndef JK2_MODE
	{ "sense",		FP_SEE },
#endif // !JK2_MODE

	{ "speed",		FP_SPEED },
	{ "sabdef",		FP_SABER_DEFENSE },
	{ "saboff",		FP_SABER_OFFENSE },
	{ "sabthrow",		FP_SABERTHROW },

				// Dark powers
#ifndef JK2_MODE
	{ "drain",		FP_DRAIN },
#endif // !JK2_MODE

	{ "grip",			FP_GRIP },
	{ "lightning",	FP_LIGHTNING },
	{ "destruction", FP_DESTRUCTION},
	{ "fear", FP_FEAR},
	{ "strike", FP_LIGHTNING_STRIKE},

#ifndef JK2_MODE
	{ "rage",			FP_RAGE },
#endif // !JK2_MODE
};


// Find the index to the Force Power in powerEnum array
static qboolean UI_GetForcePowerIndex ( const char *forceName, short *forcePowerI )
{
	int i;

	// Find a match for the forceName passed in
	for (i=0;i<MAX_POWER_ENUMS;i++)
	{
		if ( !Q_stricmp(forceName, powerEnums[i].title ) )
		{
			*forcePowerI = i;
			return(qtrue);
		}
	}

	*forcePowerI = FP_UPDATED_NONE;	// Didn't find it

	return(qfalse);
}

// Set the fields for the allocation of Saber Style(Used by Force Power Allocation screen)
static void UI_InitAllocSaberStyle(const char* saberStyle) {

	menuDef_t* menu;
	itemDef_t* item;
	short	stanceIndex = GetIDForString(SaberStyleTable, saberStyle);;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	if (stanceIndex <= 0 || stanceIndex >= SS_NUM_SABER_STYLES)
	{
		return;
	}

	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player
	if (!cl) {
		return;
	}
	playerState_t* pState = cl->gentity->client;
	bool hasStance = (pState->saberStylesKnown & 1 << stanceIndex) != 0;
	bool isSingleSaber = !Q_stricmp(Cvar_VariableString("g_saber_type"), "single");

	char itemName[128];
	Com_sprintf(itemName, sizeof(itemName), "%s_switch", saberStyle);
	item = (itemDef_s*)Menu_FindItemByName(menu, itemName);

	if (item)
	{
		char itemGraphic[128];
		Com_sprintf(itemGraphic, sizeof(itemGraphic), "gfx/menus/stance_switch_%s", hasStance ? "on" : "off");
		item->window.background = ui.R_RegisterShaderNoMip(itemGraphic);
		item->disabled = isSingleSaber? qfalse : qtrue;
		item->disabledHidden = qfalse;
	}

	int stanceValue = 0;
	switch (stanceIndex)
	{
		case SS_FAST:
			stanceValue = 1;
			break;
		case SS_MEDIUM:
			stanceValue = 2;
			break;
		case SS_STRONG:
			stanceValue = 4;
			break;
		case SS_DESANN:
			stanceValue = 8;
			break;
		case SS_TAVION:
			stanceValue = 16;
			break;
		case SS_DUAL:
			stanceValue = 32;
			break;
		case SS_STAFF:
			stanceValue = 64;
			break;
		default:
			break;
	}

	if (hasStance)
	{
		int newValue = Cvar_VariableIntegerValue("ui_saber_styles") + stanceValue;
		ui.Cvar_Set("ui_saber_styles", va("%i", newValue));
	}
}

static void UI_GetSaberStyle()
{
	menuDef_t* menu;
	itemDef_t* item;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	int i;

	for (i = 0; i < SS_NUM_SABER_STYLES; i++)
	{
		if (i == SS_DUAL)
			continue;

		const char* saberStyle = GetStringForID(SaberStyleTable, i);
		char itemName[128];
		Com_sprintf(itemName, sizeof(itemName), "%s_switch", saberStyle);
		item = (itemDef_s*)Menu_FindItemByName(menu, itemName);
		int stanceValue = 0;

		switch (i)
		{
			case SS_FAST:
				stanceValue = 1;
				break;
			case SS_MEDIUM:
				stanceValue = 2;
				break;
			case SS_STRONG:
				stanceValue = 4;
				break;
			case SS_DESANN:
				stanceValue = 8;
				break;
			case SS_TAVION:
				stanceValue = 16;
				break;
			case SS_DUAL:
				stanceValue = 32;
				break;
			case SS_STAFF:
				stanceValue = 64;
				break;
			default:
				stanceValue = 0;
				break;
		}

		bool hasStance = (Cvar_VariableIntegerValue("ui_saber_styles") & stanceValue);

		if (item)
		{
			char itemGraphic[128];
			Com_sprintf(itemGraphic, sizeof(itemGraphic), "gfx/menus/stance_switch_%s", hasStance ? "on" : "off");
			item->window.background = ui.R_RegisterShaderNoMip(itemGraphic);
		}
		

	}
}

//Change cheats flags to update at the end of the menu.
static void UI_Cheats(const char* cheatName)
{
	UI_UpdateCvars();
	if (!Q_stricmp(cheatName, "god"))
	{
		ui_cheats_godMode.integer ^= 1;
		Cvar_Set("ui_cheats_godMode", va("%d",ui_cheats_godMode.integer) );
	}
	else if (!Q_stricmp(cheatName, "noclip"))
	{
		ui_cheats_noClip.integer ^= 1;
		Cvar_Set("ui_cheats_noClip", va("%d", ui_cheats_noClip.integer));
	}
	else if (!Q_stricmp(cheatName, "notarget"))
	{
		ui_cheats_noTarget.integer ^= 1;
		Cvar_Set("ui_cheats_noTarget", va("%d", ui_cheats_noTarget.integer));
	}
	else if (!Q_stricmp(cheatName, "noForce"))
	{
		ui_cheats_noForce.integer ^= 1;
		Cvar_Set("ui_cheats_noForce", va("%d", ui_cheats_noForce.integer));
	}
	else if (!Q_stricmp(cheatName, "undying"))
	{
		ui_cheats_undying.integer ^= 1;
		Cvar_Set("ui_cheats_undying", va("%d", ui_cheats_undying.integer));
	}
	ui.Cmd_ExecuteText(EXEC_NOW, "updateCheatsFlags\n");
}

// Switch the value of a Saber Style then call update method to update UI(Used by Force Power Allocation screen)
static void UI_SwitchSaberStyle(const char* saberStyle) {

	menuDef_t* menu;
	itemDef_t* item;
	short	stanceIndex = GetIDForString(SaberStyleTable, saberStyle);
	menu = Menu_GetFocused();

	if (!menu)
	{
		return;
	}

	if (stanceIndex <= 0 || stanceIndex >= SS_NUM_SABER_STYLES)
	{
		return;
	}

	/* Should the player be allowed to play with this in with dual or staff saber? May be it could be interesting to have Fast/Strong + Staff/Dual but
		I don't think the engine would allow this. Cycle Saber Attack should be updated to reflect this kind of changes first*/
	if (!(Q_stricmp(Cvar_VariableString("g_saber_type"), "single") || Q_stricmp(Cvar_VariableString("g_saber_type"), ""))) {
		return;
	}

	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player
	if (!cl) {
		return;
	}

	int stanceValue = 0;
	switch (stanceIndex)
	{
	case SS_FAST:
		stanceValue = 1;
		break;
	case SS_MEDIUM:
		stanceValue = 2;
		break;
	case SS_STRONG:
		stanceValue = 4;
		break;
	case SS_DESANN:
		stanceValue = 8;
		break;
	case SS_TAVION:
		stanceValue = 16;
		break;
	case SS_DUAL:
		stanceValue = 32;
		break;
	case SS_STAFF:
		stanceValue = 64;
		break;
	default:
		break;
	}

	if (Cvar_VariableIntegerValue("ui_saber_styles") & stanceValue)
	{
		int newValue = Cvar_VariableIntegerValue("ui_saber_styles") - stanceValue;
		ui.Cvar_Set("ui_saber_styles", va("%i", newValue));
	}
	else
	{
		int newValue = Cvar_VariableIntegerValue("ui_saber_styles") + stanceValue;
		ui.Cvar_Set("ui_saber_styles", va("%i", newValue));
	}

	char itemName[128];
	Com_sprintf(itemName, sizeof(itemName), "%s_switch", saberStyle);
	item = (itemDef_s*)Menu_FindItemByName(menu, itemName);
	bool isSingleSaber = !Q_stricmp(Cvar_VariableString("g_saber_type"), "single");

	if (item)
	{
		char itemGraphic[128];
		Com_sprintf(itemGraphic, sizeof(itemGraphic), "gfx/menus/stance_switch_%s", (Cvar_VariableIntegerValue("ui_saber_styles") & stanceValue) ? "on" : "off");
		item->window.background = ui.R_RegisterShaderNoMip(itemGraphic);
		item->disabled = isSingleSaber ? qfalse : qtrue;
		item->disabledHidden = qfalse;
	}

	/*playerState_t* pState = cl->gentity->client;
	bool hasStance = (pState->saberStylesKnown & (1 << stanceIndex) ) != 0;
	//Add or remove the stance
	if (hasStance) {
		int newStanceMap = pState->saberStylesKnown & ~(1 << stanceIndex);
		if (newStanceMap > 1) {
			pState->saberStylesKnown = newStanceMap;
			//If the actual saber style is removed, we need to switch to another.
			if (pState->saberAnimLevel == stanceIndex) {
				//Right shift because there is always a left shift before setting. also Sometimes the player start with 1 in this map?
				int newStance = Q_FindFirstBitIndex(newStanceMap>>1) ;
				pState->saberAnimLevel = newStance;
			}
		}
	}
	else {
		pState->saberStylesKnown |= (1 << stanceIndex);
	}*/

	//return UI_InitAllocSaberStyle(saberStyle);
}

// Set the fields for the allocation of force powers (Used by Force Power Allocation screen)
static void UI_InitAllocForcePowers ( const char *forceName )
{
	menuDef_t	*menu;
	itemDef_t	*item;
	short		forcePowerI=0;
	int			forcelevel;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	if (!UI_GetForcePowerIndex ( forceName, &forcePowerI ))
	{
		return;
	}

	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	// NOTE: this UIScript can be called outside the running game now, so handle that case
	// by getting info frim UIInfo instead of PlayerState
	if( cl )
	{
		playerState_t*		pState = cl->gentity->client;
		forcelevel = pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}
	else
	{
		forcelevel = uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}

	char itemName[128];
	Com_sprintf (itemName, sizeof(itemName), "%s_hexpic", powerEnums[forcePowerI].title);
	item = (itemDef_s *) Menu_FindItemByName(menu, itemName);

	if (item)
	{
		char itemGraphic[128];
		Com_sprintf (itemGraphic, sizeof(itemGraphic), "gfx/menus/hex_pattern_%d",forcelevel >= 6 ? 5 : forcelevel);
		ui.Cvar_Set(va("ui_%s_level", powerEnums[forcePowerI].title), va("%d", forcelevel));
		item->window.background = ui.R_RegisterShaderNoMip(itemGraphic);

		// If maxed out on power - don't allow update
		if (forcelevel>=5)
		{
			Com_sprintf (itemName, sizeof(itemName), "%s_fbutton", powerEnums[forcePowerI].title);
			item = (itemDef_s *) Menu_FindItemByName(menu, itemName);
			if (item)
			{
				item->action = 0;	//you are bad, no action for you!
				item->descText = 0; //no desc either!
			}
		}
	}

	// Set weapons button to inactive
	UI_ForcePowerWeaponsButton(qfalse);
}

// Flip flop between being able to see the text showing the Force Point has or hasn't been allocated (Used by Force Power Allocation screen)
static void UI_SetPowerTitleText ( qboolean showAllocated )
{
	menuDef_t	*menu;
	itemDef_t	*item;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	if (showAllocated)
	{
		// Show the text saying the force point has been allocated
		item = (itemDef_s *) Menu_FindItemByName(menu, "allocated_text");
		if (item)
		{
			item->window.flags |= WINDOW_VISIBLE;
		}

		// Hide text saying the force point needs to be allocated
		item = (itemDef_s *) Menu_FindItemByName(menu, "allocate_text");
		if (item)
		{
			item->window.flags &= ~WINDOW_VISIBLE;
		}
	}
	else
	{
		// Hide the text saying the force point has been allocated
		item = (itemDef_s *) Menu_FindItemByName(menu, "allocated_text");
		if (item)
		{
			item->window.flags &= ~WINDOW_VISIBLE;
		}

		// Show text saying the force point needs to be allocated
		item = (itemDef_s *) Menu_FindItemByName(menu, "allocate_text");
		if (item)
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}
}

#ifndef JK2_MODE
static int UI_CountForcePowers(void)
{
	const client_t* cl = &svs.clients[0];

	if (cl && cl->gentity)
	{
		const playerState_t* ps = cl->gentity->client;
		return		ps->forcePowerLevel[FP_HEAL] +
			ps->forcePowerLevel[FP_TELEPATHY] +
			ps->forcePowerLevel[FP_PROTECT] +
			ps->forcePowerLevel[FP_ABSORB] +
			ps->forcePowerLevel[FP_GRIP] +
			ps->forcePowerLevel[FP_LIGHTNING] +
			ps->forcePowerLevel[FP_RAGE] +
			ps->forcePowerLevel[FP_DRAIN];
	}
	else
	{
		return		uiInfo.forcePowerLevel[FP_HEAL] +
			uiInfo.forcePowerLevel[FP_TELEPATHY] +
			uiInfo.forcePowerLevel[FP_PROTECT] +
			uiInfo.forcePowerLevel[FP_ABSORB] +
			uiInfo.forcePowerLevel[FP_GRIP] +
			uiInfo.forcePowerLevel[FP_LIGHTNING] +
			uiInfo.forcePowerLevel[FP_RAGE] +
			uiInfo.forcePowerLevel[FP_DRAIN];
	}
}
#endif


//. Find weapons button and make active/inactive  (Used by Force Power Allocation screen)
static void UI_ForcePowerWeaponsButton(qboolean activeFlag)
{
	menuDef_t	*menu;
	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	#ifndef JK2_MODE
	if (!activeFlag)
	{
		// total light and dark powers are at maximum level 3    ( 3 levels * ( 4ls + 4ds ) = 24 )
		if (UI_CountForcePowers() >= 24)
			activeFlag = qtrue;
	}
	#endif

	// Find weaponsbutton
	itemDef_t	*item;
	item = (itemDef_s *) Menu_FindItemByName(menu, "weaponbutton");
	if (item)
	{
		// Make it active
		if (activeFlag)
		{
			item->window.flags &= ~WINDOW_INACTIVE;
		}
		else
		{
			item->window.flags |= WINDOW_INACTIVE;
		}
	}
}

void UI_SetItemColor(itemDef_t *item,const char *itemname,const char *name,vec4_t color);

static void UI_SetHexPicLevel( const menuDef_t	*menu,const int forcePowerI,const int powerLevel, const qboolean	goldFlag )
{
	char itemName[128];
	itemDef_t	*item;

	// Find proper hex picture on menu
	Com_sprintf (itemName, sizeof(itemName), "%s_hexpic", powerEnums[forcePowerI].title);
	item = (itemDef_s *) Menu_FindItemByName((menuDef_t	*) menu, itemName);

	// Now give it the proper hex graphic
	if (item)
	{
		char itemGraphic[128];
		if (goldFlag)
		{
			Com_sprintf (itemGraphic, sizeof(itemGraphic), "gfx/menus/hex_pattern_%d_gold",powerLevel >= 4 ? 3 : powerLevel);
		}
		else
		{
			Com_sprintf (itemGraphic, sizeof(itemGraphic),  "gfx/menus/hex_pattern_%d",powerLevel >= 4 ? 3 : powerLevel);
		}

		item->window.background = ui.R_RegisterShaderNoMip(itemGraphic);

		Com_sprintf (itemName, sizeof(itemName), "%s_fbutton", powerEnums[forcePowerI].title);
		item = (itemDef_s *) Menu_FindItemByName((menuDef_t	*)menu, itemName);
		if (item)
		{
			if (goldFlag)
			{
				// Change description text to tell player they can decrement the force point
				item->descText = "@MENUS_REMOVEFP";
			}
			else
			{
				// Change description text to tell player they can increment the force point
				item->descText = "@MENUS_ADDFP";
			}
		}
	}
}

void UI_SetItemVisible(menuDef_t *menu,const char *itemname,qboolean visible);

// if this is the first time into the force power allocation screen, show the INSTRUCTION screen
static void	UI_ForceHelpActive( void )
{
	int	tier_storyinfo = Cvar_VariableIntegerValue( "tier_storyinfo" );

	// First time, show instructions
	if (tier_storyinfo==1)
	{
//		Menus_OpenByName("ingameForceHelp");
		Menus_ActivateByName("ingameForceHelp");
	}
}

#ifndef JK2_MODE
// Set the force levels depending on the level chosen
static void	UI_DemoSetForceLevels( void )
{
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	char	buffer[MAX_STRING_CHARS];

	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player
	playerState_t*		pState = NULL;
	if( cl )
	{
		pState = cl->gentity->client;
	}

	ui.Cvar_VariableStringBuffer( "ui_demo_level", buffer, sizeof(buffer));
	if( Q_stricmp( buffer, "t1_sour")==0 )
	{// NOTE : always set the uiInfo powers
		// level 1 in all core powers
		uiInfo.forcePowerLevel[FP_LEVITATION]=1;
		uiInfo.forcePowerLevel[FP_SPEED]=1;
		uiInfo.forcePowerLevel[FP_PUSH]=1;
		uiInfo.forcePowerLevel[FP_PULL]=1;
		uiInfo.forcePowerLevel[FP_SEE]=1;
		uiInfo.forcePowerLevel[FP_SABER_OFFENSE]=1;
		uiInfo.forcePowerLevel[FP_SABER_DEFENSE]=1;
		uiInfo.forcePowerLevel[FP_SABERTHROW]=1;
		// plus these extras
		uiInfo.forcePowerLevel[FP_HEAL]=1;
		uiInfo.forcePowerLevel[FP_TELEPATHY]=1;
		uiInfo.forcePowerLevel[FP_GRIP]=1;

		// and set the rest to zero
		uiInfo.forcePowerLevel[FP_ABSORB]=0;
		uiInfo.forcePowerLevel[FP_PROTECT]=0;
		uiInfo.forcePowerLevel[FP_DRAIN]=0;
		uiInfo.forcePowerLevel[FP_LIGHTNING]=0;
		uiInfo.forcePowerLevel[FP_RAGE]=0;
		uiInfo.forcePowerLevel[FP_STASIS] = 0;
		uiInfo.forcePowerLevel[FP_DESTRUCTION] = 0;
		uiInfo.forcePowerLevel[FP_GRASP] = 0;
	}
	else
	{
		// level 3 in all core powers
		uiInfo.forcePowerLevel[FP_LEVITATION]=3;
		uiInfo.forcePowerLevel[FP_SPEED]=3;
		uiInfo.forcePowerLevel[FP_PUSH]=3;
		uiInfo.forcePowerLevel[FP_PULL]=3;
		uiInfo.forcePowerLevel[FP_SEE]=3;
		uiInfo.forcePowerLevel[FP_SABER_OFFENSE]=3;
		uiInfo.forcePowerLevel[FP_SABER_DEFENSE]=3;
		uiInfo.forcePowerLevel[FP_SABERTHROW]=3;

		// plus these extras
		uiInfo.forcePowerLevel[FP_HEAL]=1;
		uiInfo.forcePowerLevel[FP_TELEPATHY]=1;
		uiInfo.forcePowerLevel[FP_GRIP]=2;
		uiInfo.forcePowerLevel[FP_LIGHTNING]=1;
		uiInfo.forcePowerLevel[FP_PROTECT]=1;

		// and set the rest to zero

		uiInfo.forcePowerLevel[FP_ABSORB]=0;
		uiInfo.forcePowerLevel[FP_DRAIN]=0;
		uiInfo.forcePowerLevel[FP_RAGE]=0;
		uiInfo.forcePowerLevel[FP_STASIS] = 0;
		uiInfo.forcePowerLevel[FP_DESTRUCTION] = 0;
		uiInfo.forcePowerLevel[FP_GRASP] = 0;
		uiInfo.forcePowerLevel[FP_FEAR] = 0;
		uiInfo.forcePowerLevel[FP_LIGHTNING_STRIKE] = 0;
		uiInfo.forcePowerLevel[FP_BLAST] = 0;
	}

	if (pState)
	{//i am carrying over from a previous level, so get the increased power! (non-core only)
		uiInfo.forcePowerLevel[FP_HEAL] = Q_max(pState->forcePowerLevel[FP_HEAL], uiInfo.forcePowerLevel[FP_HEAL]);
		uiInfo.forcePowerLevel[FP_TELEPATHY]=Q_max(pState->forcePowerLevel[FP_TELEPATHY], uiInfo.forcePowerLevel[FP_TELEPATHY]);
		uiInfo.forcePowerLevel[FP_GRIP]=Q_max(pState->forcePowerLevel[FP_GRIP], uiInfo.forcePowerLevel[FP_GRIP]);
		uiInfo.forcePowerLevel[FP_LIGHTNING]=Q_max(pState->forcePowerLevel[FP_LIGHTNING], uiInfo.forcePowerLevel[FP_LIGHTNING]);
		uiInfo.forcePowerLevel[FP_PROTECT]=Q_max(pState->forcePowerLevel[FP_PROTECT], uiInfo.forcePowerLevel[FP_PROTECT]);

		uiInfo.forcePowerLevel[FP_ABSORB]=Q_max(pState->forcePowerLevel[FP_ABSORB], uiInfo.forcePowerLevel[FP_ABSORB]);
		uiInfo.forcePowerLevel[FP_DRAIN]=Q_max(pState->forcePowerLevel[FP_DRAIN], uiInfo.forcePowerLevel[FP_DRAIN]);
		uiInfo.forcePowerLevel[FP_RAGE]=Q_max(pState->forcePowerLevel[FP_RAGE], uiInfo.forcePowerLevel[FP_RAGE]);

		uiInfo.forcePowerLevel[FP_STASIS] = Q_max(pState->forcePowerLevel[FP_STASIS], uiInfo.forcePowerLevel[FP_STASIS]);

		uiInfo.forcePowerLevel[FP_DESTRUCTION] = Q_max(pState->forcePowerLevel[FP_DESTRUCTION], uiInfo.forcePowerLevel[FP_DESTRUCTION]);

		uiInfo.forcePowerLevel[FP_GRASP] = Q_max(pState->forcePowerLevel[FP_GRASP], uiInfo.forcePowerLevel[FP_GRASP]);

		uiInfo.forcePowerLevel[FP_FEAR] = Q_max(pState->forcePowerLevel[FP_FEAR], uiInfo.forcePowerLevel[FP_FEAR]);

	}
}
#endif // !JK2_MODE

// record the force levels into a cvar so when restoring player from map transition
// the force levels are set up correctly
static void	UI_RecordForceLevels( void )
{
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	const char *s2 = "";
	int i;
	for (i=0;i< NUM_FORCE_POWERS; i++)
	{
		s2 = va("%s %i",s2, uiInfo.forcePowerLevel[i]);
	}
	Cvar_Set( "demo_playerfplvl", s2 );

}

// record the weapons into a cvar so when restoring player from map transition
// the force levels are set up correctly
static void	UI_RecordWeapons( void )
{
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	const char *s2 = "";

	int wpns = 0;
	// always add blaster and saber
	wpns |= (1<<WP_SABER);
	wpns |= (1<<WP_BLASTER_PISTOL);
	wpns |= (1<< uiInfo.selectedWeapon1);
	wpns |= (1<< uiInfo.selectedWeapon2);
	wpns |= (1<< uiInfo.selectedThrowWeapon);
	s2 = va("%i", wpns );

	Cvar_Set( "demo_playerwpns", s2 );

}

// Shut down the help screen in the force power allocation screen
static void UI_ShutdownForceHelp( void )
{
	int i;
	char itemName[128];
	menuDef_t	*menu;
	itemDef_t	*item;
	vec4_t color = { 0.65f, 0.65f, 0.65f, 1.0f};

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	// Not in upgrade mode so turn on all the force buttons, the big forceicon and description text
	if (uiInfo.forcePowerUpdated == FP_UPDATED_NONE)
	{
		// We just decremented a field so turn all buttons back on
		// Make it so all  buttons can be clicked
		for (i=0;i<MAX_POWER_ENUMS;i++)
		{
			Com_sprintf (itemName, sizeof(itemName), "%s_fbutton", powerEnums[i].title);

			UI_SetItemVisible(menu,itemName,qtrue);
		}

		UI_SetItemVisible(menu,"force_icon",qtrue);
		UI_SetItemVisible(menu,"force_desc",qtrue);
		UI_SetItemVisible(menu,"leveltext",qtrue);

		// Set focus on the top left button
		item = (itemDef_s *) Menu_FindItemByName(menu, "absorb_fbutton");
		item->window.flags |= WINDOW_HASFOCUS;

		if (item->onFocus)
		{
			Item_RunScript(item, item->onFocus);
		}

	}
	// In upgrade mode so just turn the deallocate button on
	else
	{
		UI_SetItemVisible(menu,"force_icon",qtrue);
		UI_SetItemVisible(menu,"force_desc",qtrue);
		UI_SetItemVisible(menu,"deallocate_fbutton",qtrue);

		item = (itemDef_s *) Menu_FindItemByName(menu, va("%s_fbutton",powerEnums[uiInfo.forcePowerUpdated].title));
		if (item)
		{
			item->window.flags |= WINDOW_HASFOCUS;
		}

		// Get player state
		client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

		if (!cl)	// No client, get out
		{
			return;
		}

		playerState_t*		pState = cl->gentity->client;


		if (uiInfo.forcePowerUpdated == FP_UPDATED_NONE)
		{
			return;
		}

		// Update level description
		Com_sprintf (
			itemName,
			sizeof(itemName),
			"%s_level%ddesc",
			powerEnums[uiInfo.forcePowerUpdated].title,
			pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]
			);

		item = (itemDef_s *) Menu_FindItemByName(menu, itemName);
		if (item)
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}

	// If one was a chosen force power, high light it again.
	if (uiInfo.forcePowerUpdated>FP_UPDATED_NONE)
	{
		char itemhexName[128];
		char itemiconName[128];
		vec4_t color2 = { 1.0f, 1.0f, 1.0f, 1.0f};

		Com_sprintf (itemhexName, sizeof(itemhexName), "%s_hexpic", powerEnums[uiInfo.forcePowerUpdated].title);
		Com_sprintf (itemiconName, sizeof(itemiconName), "%s_iconpic", powerEnums[uiInfo.forcePowerUpdated].title);

		UI_SetItemColor(item,itemhexName,"forecolor",color2);
		UI_SetItemColor(item,itemiconName,"forecolor",color2);
	}
	else
	{
		// Un-grey-out all icons
		UI_SetItemColor(item,"hexpic","forecolor",color);
		UI_SetItemColor(item,"iconpic","forecolor",color);
	}
}

// Decrement force power level (Used by Force Power Allocation screen)
static void UI_DecrementCurrentForcePower ( void )
{
	menuDef_t	*menu;
	itemDef_t	*item;
	short i;
	vec4_t color = { 0.65f, 0.65f, 0.65f, 1.0f};
	char itemName[128];

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player
	playerState_t*		pState = NULL;
	int forcelevel;

	if( cl )
	{
		pState = cl->gentity->client;
		forcelevel = pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
	}
	else
	{
		forcelevel = uiInfo.forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
	}

	if (uiInfo.forcePowerUpdated == FP_UPDATED_NONE)
	{
		return;
	}

	DC->startLocalSound(uiInfo.uiDC.Assets.forceUnchosenSound, CHAN_AUTO );

	if (forcelevel>0)
	{
		if( pState )
		{
			pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]--;	// Decrement it
			forcelevel = pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
			// Turn off power if level is 0
			if (pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]<1)
			{
				pState->forcePowersKnown &= ~( 1 << powerEnums[uiInfo.forcePowerUpdated].powerEnum );
			}
		}
		else
		{
			uiInfo.forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]--;	// Decrement it
			forcelevel = uiInfo.forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum];
		}
	}

	UI_SetHexPicLevel( menu,uiInfo.forcePowerUpdated,forcelevel,qfalse );

	UI_ShowForceLevelDesc ( powerEnums[uiInfo.forcePowerUpdated].title );

	// We just decremented a field so turn all buttons back on
	// Make it so all  buttons can be clicked
	for (i=0;i<MAX_POWER_ENUMS;i++)
	{
		Com_sprintf (itemName, sizeof(itemName), "%s_fbutton", powerEnums[i].title);
		item = (itemDef_s *) Menu_FindItemByName(menu, itemName);
		if (item)		// This is okay, because core powers don't have a hex button
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}

	// Show point has not been allocated
	UI_SetPowerTitleText( qfalse);

	// Make weapons button inactive
	UI_ForcePowerWeaponsButton(qfalse);

	// Hide the deallocate button
	item = (itemDef_s *) Menu_FindItemByName(menu, "deallocate_fbutton");
	if (item)
	{
		item->window.flags &= ~WINDOW_VISIBLE;	//

		// Un-grey-out all icons
		UI_SetItemColor(item,"hexpic","forecolor",color);
		UI_SetItemColor(item,"iconpic","forecolor",color);
	}

	item = (itemDef_s *) Menu_FindItemByName(menu, va("%s_fbutton",powerEnums[uiInfo.forcePowerUpdated].title));
	if (item)
	{
		item->window.flags |= WINDOW_HASFOCUS;
	}

	uiInfo.forcePowerUpdated = FP_UPDATED_NONE;			// It's as if nothing happened.
}


void Item_MouseEnter(itemDef_t *item, float x, float y);

// Try to increment force power level (Used by Force Power Allocation screen)
static void UI_AffectForcePowerLevel ( const char *forceName )
{
	short forcePowerI=0,i;
	menuDef_t	*menu;
	itemDef_t	*item;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	if (!UI_GetForcePowerIndex ( forceName, &forcePowerI ))
	{
		return;
	}

	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player
	playerState_t*		pState = NULL;
	int	forcelevel;
	if( cl )
	{
		pState = cl->gentity->client;
		forcelevel = pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}
	else
	{
		forcelevel = uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}


	if (forcelevel>2)
	{	// Too big, can't be incremented
		return;
	}

	// Increment power level.
	DC->startLocalSound(uiInfo.uiDC.Assets.forceChosenSound, CHAN_AUTO );

	uiInfo.forcePowerUpdated = forcePowerI;	// Remember which power was updated

	if( pState )
	{
		pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum]++;	// Increment it
		pState->forcePowersKnown |= ( 1 << powerEnums[forcePowerI].powerEnum );
		forcelevel = pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}
	else
	{
		uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum]++;	// Increment it
		forcelevel = uiInfo.forcePowerLevel[powerEnums[forcePowerI].powerEnum];
	}

	UI_SetHexPicLevel( menu,uiInfo.forcePowerUpdated,forcelevel,qtrue );

	UI_ShowForceLevelDesc ( forceName );

	// A field was updated, so make it so others can't be
	if (uiInfo.forcePowerUpdated>FP_UPDATED_NONE)
	{
		vec4_t color = { 0.25f, 0.25f, 0.25f, 1.0f};
		char itemName[128];

		// Make it so none of the other buttons can be clicked
		for (i=0;i<MAX_POWER_ENUMS;i++)
		{
			if (i==uiInfo.forcePowerUpdated)
			{
				continue;
			}

			Com_sprintf (itemName, sizeof(itemName), "%s_fbutton", powerEnums[i].title);
			item = (itemDef_s *) Menu_FindItemByName(menu, itemName);
			if (item)		// This is okay, because core powers don't have a hex button
			{
				item->window.flags &= ~WINDOW_VISIBLE;
			}
		}

		// Show point has been allocated
		UI_SetPowerTitleText ( qtrue );

		// Make weapons button active
		UI_ForcePowerWeaponsButton(qtrue);

		// Make user_info
		Cvar_Set ( "ui_forcepower_inc", va("%d",uiInfo.forcePowerUpdated) );

		// Just grab an item to hand it to the function.
		item = (itemDef_s *) Menu_FindItemByName(menu, "deallocate_fbutton");
		if (item)
		{
			// Show all icons as greyed-out
			UI_SetItemColor(item,"hexpic","forecolor",color);
			UI_SetItemColor(item,"iconpic","forecolor",color);

			item->window.flags |= WINDOW_HASFOCUS;
		}
	}

}

static void UI_DecrementForcePowerLevel( void )
{
	int	forcePowerI = Cvar_VariableIntegerValue( "ui_forcepower_inc" );
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (!cl)	// No client, get out
	{
		return;
	}

	playerState_t*		pState = cl->gentity->client;

	pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum]--;	// Decrement it

}

// Show force level description that matches current player level (Used by Force Power Allocation screen)
static void UI_ShowForceLevelDesc ( const char *forceName )
{
	short forcePowerI=0;
	menuDef_t	*menu;
	itemDef_t	*item;
	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}


	if (!UI_GetForcePowerIndex ( forceName, &forcePowerI ))
	{
		return;
	}

	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (!cl)	// No client, get out
	{
		return;
	}
	playerState_t*		pState = cl->gentity->client;

	char itemName[128];

	// Update level description
	Com_sprintf (
		itemName,
		sizeof(itemName),
		"%s_level%ddesc",
		powerEnums[forcePowerI].title,
		pState->forcePowerLevel[powerEnums[forcePowerI].powerEnum]
		);

	item = (itemDef_s *) Menu_FindItemByName(menu, itemName);
	if (item)
	{
		item->window.flags |= WINDOW_VISIBLE;
	}

}

// Reset force level powers screen to what it was before player upgraded them (Used by Force Power Allocation screen)
static void UI_ResetForceLevels ( void )
{

	// What force ppower had the point added to it?
	if (uiInfo.forcePowerUpdated!=FP_UPDATED_NONE)
	{
		// Get player state
		client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

		if (!cl)	// No client, get out
		{
			return;
		}
		playerState_t*		pState = cl->gentity->client;

		// Decrement that power
		pState->forcePowerLevel[powerEnums[uiInfo.forcePowerUpdated].powerEnum]--;

		menuDef_t	*menu;
		itemDef_t	*item;

		menu = Menu_GetFocused();	// Get current menu

		if (!menu)
		{
			return;
		}

		int i;
		char itemName[128];

		// Make it so all  buttons can be clicked
		for (i=0;i<MAX_POWER_ENUMS;i++)
		{
			Com_sprintf (itemName, sizeof(itemName), "%s_fbutton", powerEnums[i].title);
			item = (itemDef_s *) Menu_FindItemByName(menu, itemName);
			if (item)		// This is okay, because core powers don't have a hex button
			{
				item->window.flags |= WINDOW_VISIBLE;
			}
		}

		UI_SetPowerTitleText( qfalse );

		Com_sprintf (itemName, sizeof(itemName), "%s_fbutton", powerEnums[uiInfo.forcePowerUpdated].title);
		item = (itemDef_s *) Menu_FindItemByName(menu, itemName);
		if (item)
		{
			// Change description text to tell player they can increment the force point
			item->descText = "@MENUS_ADDFP";
		}

		uiInfo.forcePowerUpdated = FP_UPDATED_NONE;
	}

	UI_ForcePowerWeaponsButton(qfalse);
}


#ifndef JK2_MODE
// Set the Players known saber style
static void UI_UpdateFightingStyle ( void )
{
	playerState_t	*pState;
	int				fightingStyle,saberStyle;


	fightingStyle = Cvar_VariableIntegerValue( "ui_newfightingstyle" );

	if (fightingStyle == 1)
	{
		saberStyle = SS_MEDIUM;
	}
	else if (fightingStyle == 2)
	{
		saberStyle = SS_STRONG;
	}
	else if (fightingStyle == 3)
	{
		saberStyle = SS_DUAL;
	}
	else if (fightingStyle == 4)
	{
		saberStyle = SS_STAFF;
	}
	else // 0 is Fast
	{
		saberStyle = SS_FAST;
	}

	// Get player state
	client_t	*cl = &svs.clients[0];	// 0 because only ever us as a player

	// No client, get out
	if (cl && cl->gentity && cl->gentity->client)
	{
		pState = cl->gentity->client;
		pState->saberStylesKnown |= (1<<saberStyle);
	}
	else	// Must be at the beginning of the game so the client hasn't been created, shove data in a cvar
	{
		Cvar_Set ( "g_fighting_style", va("%d",saberStyle) );
	}
}
#endif // !JK2_MODE

static void UI_ResetCharacterListBoxes( void )
{

	itemDef_t *item;
	menuDef_t *menu;
	listBoxDef_t *listPtr;

	menu = Menus_FindByName("characterMenu");


	if (menu)
	{
		item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, "headlistbox");
		if (item)
		{
			listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;
			if( listPtr )
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}

		item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, "torsolistbox");
		if (item)
		{
			listPtr = (listBoxDef_t*)item->typeData;
			if( listPtr )
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}

		item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, "lowerlistbox");
		if (item)
		{
			listPtr = (listBoxDef_t*)item->typeData;
			if( listPtr )
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}

		item = (itemDef_t *) Menu_FindItemByName((menuDef_t *) menu, "colorbox");
		if (item)
		{
			listPtr = (listBoxDef_t*)item->typeData;
			if( listPtr )
			{
				listPtr->cursorPos = 0;
			}
			item->cursorPos = 0;
		}
	}
}

static void UI_RecordForcePowers(const char** args)
{
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	// I feel a great disturbance in the Force, as if millions of coders cried out in terror and were suddenly silenced. I fear I coded this horribly.
	const char* str = va("%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s", "ui_heal_level",
		"ui_jump_level",
		"ui_speed_level",
		"ui_push_level",
		"ui_pull_level",
		"ui_mindtrick_level",
		"ui_grip_level",
		"ui_lightning_level",
		"ui_saboff_level",
		"ui_sabdef_level",
		"ui_sabthrow_level",
		"ui_rage_level",
		"ui_protect_level",
		"ui_absorb_level",
		"ui_drain_level",
		"ui_sense_level",
		"ui_stasis_level",
		"ui_blast_level",
		"ui_grasp_level",
		"ui_destruction_level",
		"ui_strike_level",
		"ui_fear_level");

	const char** powerCvars = &str;

	if (!cl)	// No client, get out
	{
		return;
	}
	if (cl->gentity && cl->gentity->client)
	{
		for (int i = FP_HEAL; i < NUM_FORCE_POWERS; i++)
		{
			const char* power;
			const char* powerCvar;
			String_Parse(args, &power);
			String_Parse(powerCvars, &powerCvar);
			forcePowerRecord[i] = atoi(power);
			Cvar_Set(powerCvar, power);
		}
	}

}

static void UI_CharacterSelect(const char** args)
{
	const char* level;
	String_Parse(args, &level);

	ui.Cvar_Set("ui_mission_mapcode", level);

	if (Q_stricmp(Cvar_VariableString("ui_mission_mapcode"), "") && Q_stricmp(Cvar_VariableString("ui_mission_mapcode"), NULL))
	{
		menuDef_t* menu;
		menu = Menu_GetFocused();
		Menu_ShowItemByName(menu, "StartButton", qtrue);
		Menu_ShowItemByName(menu, "StartButtonOFF", qfalse);
		menu = NULL;
	}
	else
	{
		menuDef_t* menu;
		menu = Menu_GetFocused();
		Menu_ShowItemByName(menu, "StartButton", qfalse);
		Menu_ShowItemByName(menu, "StartButtonOFF", qtrue);
		menu = NULL;
	}

}
/*extern int WP_GetWeaponID(const char* weaponName);
static void UI_ApplyCharWeapons()
{
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (!cl)	// No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;
		int weaponOne = WP_GetWeaponID(Cvar_VariableString("ui_weaponOne"));
		int weaponTwo = WP_GetWeaponID(Cvar_VariableString("ui_weaponTwo"));
		int weaponThree = WP_GetWeaponID(Cvar_VariableString("ui_weaponThree"));
		int weaponFour = WP_GetWeaponID(Cvar_VariableString("ui_weaponFour"));
		int weaponFive = WP_GetWeaponID(Cvar_VariableString("ui_weaponFive"));
		int weaponSix = WP_GetWeaponID(Cvar_VariableString("ui_weaponSix"));

		// Every character gets melee, no matter what
		pState->weapons[WP_MELEE] = 1;
		

		// Again, super ugly, wish I could do it less horribly
		if (weaponOne)
			pState->weapons[weaponOne] = 1;
		if (weaponTwo)
			pState->weapons[weaponTwo] = 1;
		if (weaponThree)
			pState->weapons[weaponThree] = 1;
		if (weaponFour)
			pState->weapons[weaponFour] = 1;
		if (weaponFive)
			pState->weapons[weaponFive] = 1;
		if (weaponSix)
			pState->weapons[weaponSix] = 1;

		if (pState->weapons[WP_SABER] != 1 && Q_stricmp(Cvar_VariableString("ui_saber"), "") && Q_stricmp(Cvar_VariableString("ui_saber"), "empty") && Q_stricmp(Cvar_VariableString("ui_saber"), "none"))
		{
			pState->weapons[WP_SABER] = 1;
		}

		// Reset the secondary saber, just in case
		Cvar_Set("ui_saber2", "");

		weaponOne = 0;
		weaponTwo = 0;
		weaponThree = 0;
		weaponFour = 0;
		weaponFive = 0;
		weaponSix = 0;
	}
}*/

static void UI_ApplySaberStyles()
{
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	int saberStyles = Cvar_VariableIntegerValue("ui_saber_styles");

	if (!cl)	// No client, get out
	{
		return;
	}
	if (saberStyles < 0)
	{
		return;
	}
	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;

		// Reset styles so nothing bleeds over
		pState->saberStylesKnown = 0;

		// Assign saber styles (and set the highest one as the player's new style)
		if (saberStyles & 1)
		{
			pState->saberStylesKnown |= (1 << SS_FAST);
			pState->saberAnimLevel = SS_FAST;
		}
		if (saberStyles & 2)
		{
			pState->saberStylesKnown |= (1 << SS_MEDIUM);
			pState->saberAnimLevel = SS_MEDIUM;
		}
		if (saberStyles & 4)
		{
			pState->saberStylesKnown |= (1 << SS_STRONG);
			pState->saberAnimLevel = SS_STRONG;
		}
		if (saberStyles & 8)
		{
			pState->saberStylesKnown |= (1 << SS_DESANN);
			pState->saberAnimLevel = SS_DESANN;
		}
		if (saberStyles & 16)
		{
			pState->saberStylesKnown |= (1 << SS_TAVION);
			pState->saberAnimLevel = SS_TAVION;
		}
		if (saberStyles & 32)
		{
			pState->saberStylesKnown |= (1 << SS_DUAL);
			pState->saberAnimLevel = SS_DUAL;
		}
		if (saberStyles & 64)
		{
			pState->saberStylesKnown |= (1 << SS_STAFF);
			pState->saberAnimLevel = SS_STAFF;
		}
	}
}

static void UI_ApplyForcePowers()
{
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (!cl)	// No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t* pState = cl->gentity->client;
		for (int i = FP_HEAL; i < NUM_FORCE_POWERS; i++)
		{
			if (forcePowerRecord[i] > 0)
			{
				pState->forcePowersKnown |= (1 << i);
				pState->forcePowerLevel[i] = forcePowerRecord[i];
			}
			else
			{
				pState->forcePowersKnown &= ~(1 << i);
				pState->forcePowerLevel[i] = 0;
			}
		}
	}


}

extern void Menu_SetItemBackground(const menuDef_t* menu, const char* itemName, const char* background);
static void UI_ShowMissionInfo()
{

	menuDef_t* menu = Menu_GetFocused();
	//Menu_ShowItemByName(menu, "CharacterSelection", qfalse);
	if (missionData[Cvar_VariableIntegerValue("ui_mission_topic")][Cvar_VariableIntegerValue("ui_mission")].picCode
	&& Q_stricmp(Cvar_VariableString("ui_mission_code"), "") && Q_stricmp(Cvar_VariableString("ui_mission_code"), NULL))
	{
		Menu_ShowItemByName(menu, "missionInfo", qtrue);

		if (!strncmp(missionData[Cvar_VariableIntegerValue("ui_mission_topic")][Cvar_VariableIntegerValue("ui_mission")].picCode, "levelshots/", 11) || !strncmp(missionData[Cvar_VariableIntegerValue("ui_mission_topic")][Cvar_VariableIntegerValue("ui_mission")].picCode, "levelshots_sav/", 14))
			Menu_SetItemBackground(menu, "MissionPic", missionData[Cvar_VariableIntegerValue("ui_mission_topic")][Cvar_VariableIntegerValue("ui_mission")].picCode);
		else if (ui.R_RegisterShaderNoMip(va("gfx/menus/missions/%s/background", missionData[Cvar_VariableIntegerValue("ui_mission_topic")][Cvar_VariableIntegerValue("ui_mission")].picCode)) != 0)
			Menu_SetItemBackground(menu, "MissionPic", va("gfx/menus/missions/%s/background", missionData[Cvar_VariableIntegerValue("ui_mission_topic")][Cvar_VariableIntegerValue("ui_mission")].picCode));
		else
			Menu_SetItemBackground(menu, "MissionPic", va("levelshots/%s", missionData[Cvar_VariableIntegerValue("ui_mission_topic")][Cvar_VariableIntegerValue("ui_mission")].mapCode));

		Menu_SetItemText(menu, "MissionDesc", missionData[Cvar_VariableIntegerValue("ui_mission_topic")][Cvar_VariableIntegerValue("ui_mission")].descCode);
		Menu_ShowItemByName(menu, "MissionDesc", qtrue);

	}
	else
	{
		Menu_ShowItemByName(menu, "missionInfo", qfalse);
		Menu_ShowItemByName(menu, "MissionDesc", qfalse);
	}

	menu = NULL;

}

static void UI_ClearInventory ( void )
{
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (!cl)	// No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t*		pState = cl->gentity->client;

		// Clear out inventory for the player
		int i;
		for (i=0;i<MAX_INVENTORY;i++)
		{
			pState->inventory[i] = 0;
		}
	}
}

static void UI_GiveInventory ( const int itemIndex, const int amount )
{
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (!cl)	// No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t*	pState = cl->gentity->client;

		if (itemIndex < MAX_INVENTORY)
		{
			pState->inventory[itemIndex]=amount;
		}
	}

}

//. Find weapons allocation screen BEGIN button and make active/inactive
static void UI_WeaponAllocBeginButton(qboolean activeFlag)
{
	menuDef_t	*menu;
	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	int weap =	Cvar_VariableIntegerValue( "weapon_menu" );

	// Find begin button
	itemDef_t	*item;
	item = Menu_GetMatchingItemByNumber(menu, weap, "beginmission");

	if (item)
	{
		// Make it active
		if (activeFlag)
		{
			item->window.flags &= ~WINDOW_INACTIVE;
		}
		else
		{
			item->window.flags |= WINDOW_INACTIVE;
		}
	}
}

// If we have both weapons and the throwable weapon, turn on the begin mission button,
// otherwise, turn it off
static void UI_WeaponsSelectionsComplete( void )
{
	// We need two weapons and one throwable
	if (( uiInfo.selectedWeapon1 != NOWEAPON ) &&
		( uiInfo.selectedWeapon2 != NOWEAPON ) &&
		( uiInfo.selectedThrowWeapon != NOWEAPON ))
	{
		UI_WeaponAllocBeginButton(qtrue);	// Turn it on
	}
	else
	{
		UI_WeaponAllocBeginButton(qfalse);	// Turn it off
	}
}

// if this is the first time into the weapon allocation screen, show the INSTRUCTION screen
static void	UI_WeaponHelpActive( void )
{
	int	tier_storyinfo = Cvar_VariableIntegerValue( "tier_storyinfo" );
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	// First time, show instructions
	if (tier_storyinfo==1)
	{
		UI_SetItemVisible(menu,"weapon_button",qfalse);

		UI_SetItemVisible(menu,"inst_stuff",qtrue);

	}
	// just act like normal
	else
	{
		UI_SetItemVisible(menu,"weapon_button",qtrue);

		UI_SetItemVisible(menu,"inst_stuff",qfalse);
	}
}

static void UI_InitWeaponSelect( void )
{
	UI_WeaponAllocBeginButton(qfalse);
	uiInfo.selectedWeapon1 = NOWEAPON;
	uiInfo.selectedWeapon2 = NOWEAPON;
	uiInfo.selectedThrowWeapon = NOWEAPON;
}

static void UI_ClearWeapons ( void )
{
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (!cl)	// No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t*		pState = cl->gentity->client;

		// Clear out any weapons for the player
		for ( int i = 0; i < MAX_WEAPONS; i++ )
		{
			pState->weapons[i] = 0;
		}

		pState->weapon = WP_NONE;

	}

}

static void UI_GiveWeapon ( const int weaponIndex )
{
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (!cl)	// No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t*	pState = cl->gentity->client;
		//TODO - CORRECT THIS
		if (weaponIndex< 64 )
		{
			pState->weapons[weaponIndex] = 1;
		}
	}
}

static void UI_EquipWeapon ( const int weaponIndex )
{
	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	if (!cl)	// No client, get out
	{
		return;
	}

	if (cl->gentity && cl->gentity->client)
	{
		playerState_t*	pState = cl->gentity->client;

		//TODO - CORRECT THIS
		if (weaponIndex< 64)
		{
			pState->weapon = weaponIndex;
			//force it to change
			//CG_ChangeWeapon( wp );
		}
	}
}

static void	UI_LoadMissionSelectMenu( const char *cvarName )
{
	int holdLevel = (int)trap_Cvar_VariableValue(cvarName);

	// Figure out which tier menu to load
	if ((holdLevel > 0) && (holdLevel < 5))
	{
		UI_LoadMenus("ui/tier1.txt",qfalse);

		Menus_CloseByName("ingameMissionSelect1");
	}
	else if ((holdLevel > 6) && (holdLevel < 10))
	{
		UI_LoadMenus("ui/tier2.txt",qfalse);

		Menus_CloseByName("ingameMissionSelect2");
	}
	else if ((holdLevel > 11) && (holdLevel < 15))
	{
		UI_LoadMenus("ui/tier3.txt",qfalse);

		Menus_CloseByName("ingameMissionSelect3");
	}

}

// Update the player weapons with the chosen weapon
static void	UI_AddWeaponSelection ( const int weaponIndex, const int ammoIndex, const int ammoAmount, const char *iconItemName,const char *litIconItemName, const char *hexBackground, const char *soundfile )
{
	itemDef_s  *item, *iconItem,*litIconItem;
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	iconItem = (itemDef_s *) Menu_FindItemByName(menu, iconItemName );
	litIconItem = (itemDef_s *) Menu_FindItemByName(menu, litIconItemName );

	const char *chosenItemName, *chosenButtonName;

	// has this weapon already been chosen?
	if (weaponIndex == uiInfo.selectedWeapon1)
	{
		UI_RemoveWeaponSelection ( 1 );
		return;
	}
	else if (weaponIndex == uiInfo.selectedWeapon2)
	{
		UI_RemoveWeaponSelection ( 2 );
		return;
	}

	// See if either slot is empty
	if ( uiInfo.selectedWeapon1 == NOWEAPON )
	{
		chosenItemName = "chosenweapon1_icon";
		chosenButtonName = "chosenweapon1_button";
		uiInfo.selectedWeapon1 = weaponIndex;
		uiInfo.selectedWeapon1AmmoIndex = ammoIndex;

		memcpy( uiInfo.selectedWeapon1ItemName,hexBackground,sizeof(uiInfo.selectedWeapon1ItemName));

		//Save the lit and unlit icons for the selected weapon slot
		uiInfo.litWeapon1Icon = litIconItem->window.background;
		uiInfo.unlitWeapon1Icon = iconItem->window.background;

		uiInfo.weapon1ItemButton = uiInfo.runScriptItem;
		uiInfo.weapon1ItemButton->descText = "@MENUS_CLICKREMOVE";
	}
	else if ( uiInfo.selectedWeapon2 == NOWEAPON )
	{
		chosenItemName = "chosenweapon2_icon";
		chosenButtonName = "chosenweapon2_button";
		uiInfo.selectedWeapon2 = weaponIndex;
		uiInfo.selectedWeapon2AmmoIndex = ammoIndex;

		memcpy( uiInfo.selectedWeapon2ItemName,hexBackground,sizeof(uiInfo.selectedWeapon2ItemName));

		//Save the lit and unlit icons for the selected weapon slot
		uiInfo.litWeapon2Icon = litIconItem->window.background;
		uiInfo.unlitWeapon2Icon = iconItem->window.background;

		uiInfo.weapon2ItemButton = uiInfo.runScriptItem;
		uiInfo.weapon2ItemButton->descText = "@MENUS_CLICKREMOVE";
	}
	else	// Both slots are used, can't add it.
	{
		return;
	}

	item = (itemDef_s *) Menu_FindItemByName(menu, chosenItemName );
	if ((item) && (iconItem))
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Turn on chosenweapon button so player can unchoose the weapon
	item = (itemDef_s *) Menu_FindItemByName(menu, chosenButtonName );
	if (item)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Switch hex background to be 'on'
	item = (itemDef_s *) Menu_FindItemByName(menu, hexBackground );
	if (item)
	{
		item->window.foreColor[0] = 0;
		item->window.foreColor[1] = 1;
		item->window.foreColor[2] = 0;
		item->window.foreColor[3] = 1;

	}

	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl)
	{
		// Add weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t*	pState = cl->gentity->client;
			//TODO FIX THIS
			if ((weaponIndex>0) && (weaponIndex< 64))
			{
				pState->weapons[weaponIndex] = 1;
			}

			// Give them ammo too
			if ((ammoIndex>0) && (ammoIndex< MAX_AMMO))
			{
				pState->ammo[ ammoIndex ] = ammoAmount;
			}
		}
	}

	if( soundfile )
	{
		DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL );
	}

	UI_WeaponsSelectionsComplete();	// Test to see if the mission begin button should turn on or off


}


// Update the player weapons with the chosen weapon
static void UI_RemoveWeaponSelection ( const int weaponSelectionIndex )
{
	itemDef_s  *item;
	menuDef_t	*menu;
	const char *chosenItemName, *chosenButtonName,*background;
	int		ammoIndex,weaponIndex;

	menu = Menu_GetFocused();	// Get current menu

	// Which item has it?
	if ( weaponSelectionIndex == 1 )
	{
		chosenItemName = "chosenweapon1_icon";
		chosenButtonName = "chosenweapon1_button";
		background = uiInfo.selectedWeapon1ItemName;
		ammoIndex = uiInfo.selectedWeapon1AmmoIndex;
		weaponIndex = uiInfo.selectedWeapon1;

		if (uiInfo.weapon1ItemButton)
		{
			uiInfo.weapon1ItemButton->descText = "@MENUS_CLICKSELECT";
			uiInfo.weapon1ItemButton = NULL;
		}
	}
	else if ( weaponSelectionIndex == 2 )
	{
		chosenItemName = "chosenweapon2_icon";
		chosenButtonName = "chosenweapon2_button";
		background = uiInfo.selectedWeapon2ItemName;
		ammoIndex = uiInfo.selectedWeapon2AmmoIndex;
		weaponIndex = uiInfo.selectedWeapon2;

		if (uiInfo.weapon2ItemButton)
		{
			uiInfo.weapon2ItemButton->descText = "@MENUS_CLICKSELECT";
			uiInfo.weapon2ItemButton = NULL;
		}
	}
	else
	{
		return;
	}

	// Reset background of upper icon
	item = (itemDef_s *) Menu_FindItemByName( menu, background );
	if ( item )
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.5f;
		item->window.foreColor[2] = 0.0f;
		item->window.foreColor[3] = 1.0f;
	}

	// Hide it icon
	item = (itemDef_s *) Menu_FindItemByName( menu, chosenItemName );
	if ( item )
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Hide button
	item = (itemDef_s *) Menu_FindItemByName( menu, chosenButtonName );
	if ( item )
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Get player state
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl)	// No client, get out
	{

		// Remove weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t*	pState = cl->gentity->client;

			//TODO - CORRECT THIS
			if ((weaponIndex>0) && (weaponIndex< 64))
			{
				pState->weapons[weaponIndex] = 0;
			}

			// Remove ammo too
			if ((ammoIndex>0) && (ammoIndex<MAX_AMMO))
			{	// But don't take it away if the other weapon is using that ammo
				if ( uiInfo.selectedWeapon1AmmoIndex != uiInfo.selectedWeapon2AmmoIndex )
				{
					pState->ammo[ ammoIndex ] = 0;
				}
			}
		}

	}

	// Now do a little clean up
	if ( weaponSelectionIndex == 1 )
	{
		uiInfo.selectedWeapon1 = NOWEAPON;
		memset(uiInfo.selectedWeapon1ItemName,0,sizeof(uiInfo.selectedWeapon1ItemName));
		uiInfo.selectedWeapon1AmmoIndex = 0;
	}
	else if ( weaponSelectionIndex == 2 )
	{
		uiInfo.selectedWeapon2 = NOWEAPON;
		memset(uiInfo.selectedWeapon2ItemName,0,sizeof(uiInfo.selectedWeapon2ItemName));
		uiInfo.selectedWeapon2AmmoIndex = 0;
	}

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	DC->startLocalSound(DC->registerSound("sound/interface/weapon_deselect.mp3", qfalse), CHAN_LOCAL );
#endif

	UI_WeaponsSelectionsComplete();	// Test to see if the mission begin button should turn on or off


}

static void	UI_NormalWeaponSelection ( const int selectionslot )
{
	itemDef_s  *item;
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu
	if (!menu)
	{
		return;
	}

	if (selectionslot == 1)
	{
		item = (itemDef_s *) Menu_FindItemByName( menu, "chosenweapon1_icon" );
		if (item)
		{
			item->window.background = uiInfo.unlitWeapon1Icon;
		}
	}

	if (selectionslot == 2)
	{
		item = (itemDef_s *) Menu_FindItemByName( menu, "chosenweapon2_icon" );
		if (item)
		{
			item->window.background = uiInfo.unlitWeapon2Icon;
		}
	}
}

static void	UI_HighLightWeaponSelection ( const int selectionslot )
{
	itemDef_s  *item;
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu
	if (!menu)
	{
		return;
	}

	if (selectionslot == 1)
	{
		item = (itemDef_s *) Menu_FindItemByName( menu, "chosenweapon1_icon" );
		if (item)
		{
			item->window.background = uiInfo.litWeapon1Icon;
		}
	}

	if (selectionslot == 2)
	{
		item = (itemDef_s *) Menu_FindItemByName( menu, "chosenweapon2_icon" );
		if (item)
		{
			item->window.background = uiInfo.litWeapon2Icon;
		}
	}
}

// Update the player throwable weapons (okay it's a bad description) with the chosen weapon
static void	UI_AddThrowWeaponSelection ( const int weaponIndex, const int ammoIndex, const int ammoAmount, const char *iconItemName,const char *litIconItemName, const char *hexBackground, const char *soundfile )
{
	itemDef_s  *item, *iconItem,*litIconItem;
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	iconItem = (itemDef_s *) Menu_FindItemByName(menu, iconItemName );
	litIconItem = (itemDef_s *) Menu_FindItemByName(menu, litIconItemName );

	const char *chosenItemName, *chosenButtonName;

	// Has a throw weapon already been chosen?
	if (uiInfo.selectedThrowWeapon!=NOWEAPON)
	{
		// Clicked on the selected throwable weapon
		if (uiInfo.selectedThrowWeapon==weaponIndex)
		{	// Deselect it
			UI_RemoveThrowWeaponSelection();
		}
		return;
	}

	chosenItemName = "chosenthrowweapon_icon";
	chosenButtonName = "chosenthrowweapon_button";
	uiInfo.selectedThrowWeapon = weaponIndex;
	uiInfo.selectedThrowWeaponAmmoIndex = ammoIndex;
	uiInfo.weaponThrowButton = uiInfo.runScriptItem;

	if (uiInfo.weaponThrowButton)
	{
		uiInfo.weaponThrowButton->descText = "@MENUS_CLICKREMOVE";
	}

	memcpy( uiInfo.selectedThrowWeaponItemName,hexBackground,sizeof(uiInfo.selectedWeapon1ItemName));

	//Save the lit and unlit icons for the selected weapon slot
	uiInfo.litThrowableIcon = litIconItem->window.background;
	uiInfo.unlitThrowableIcon = iconItem->window.background;

	item = (itemDef_s *) Menu_FindItemByName(menu, chosenItemName );
	if ((item) && (iconItem))
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Turn on throwchosenweapon button so player can unchoose the weapon
	item = (itemDef_s *) Menu_FindItemByName(menu, chosenButtonName );
	if (item)
	{
		item->window.background = iconItem->window.background;
		item->window.flags |= WINDOW_VISIBLE;
	}

	// Switch hex background to be 'on'
	item = (itemDef_s *) Menu_FindItemByName(menu, hexBackground );
	if (item)
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 1.0f;
		item->window.foreColor[3] = 1.0f;

	}

	// Get player state

	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl)	// No client, get out
	{
		// Add weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t*	pState = cl->gentity->client;

			//TODO - CORRECT THIS
			if ((weaponIndex>0) && (weaponIndex< 64))
			{
				pState->weapons[weaponIndex] = 1;
			}

			// Give them ammo too
			if ((ammoIndex>0) && (ammoIndex<MAX_AMMO))
			{
				pState->ammo[ ammoIndex ] = ammoAmount;
			}
		}
	}

	if( soundfile )
	{
		DC->startLocalSound(DC->registerSound(soundfile, qfalse), CHAN_LOCAL );
	}

	UI_WeaponsSelectionsComplete();	// Test to see if the mission begin button should turn on or off

}


// Update the player weapons with the chosen throw weapon
static void UI_RemoveThrowWeaponSelection ( void )
{
	itemDef_s  *item;
	menuDef_t	*menu;
	const char *chosenItemName, *chosenButtonName,*background;

	menu = Menu_GetFocused();	// Get current menu

	// Weapon not chosen
	if ( uiInfo.selectedThrowWeapon == NOWEAPON )
	{
		return;
	}

	chosenItemName = "chosenthrowweapon_icon";
	chosenButtonName = "chosenthrowweapon_button";
	background = uiInfo.selectedThrowWeaponItemName;

	// Reset background of upper icon
	item = (itemDef_s *) Menu_FindItemByName( menu, background );
	if ( item )
	{
		item->window.foreColor[0] = 0.0f;
		item->window.foreColor[1] = 0.0f;
		item->window.foreColor[2] = 0.5f;
		item->window.foreColor[3] = 1.0f;
	}

	// Hide it icon
	item = (itemDef_s *) Menu_FindItemByName( menu, chosenItemName );
	if ( item )
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Hide button
	item = (itemDef_s *) Menu_FindItemByName( menu, chosenButtonName );
	if ( item )
	{
		item->window.flags &= ~WINDOW_VISIBLE;
	}

	// Get player state

	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player

	// NOTE : this UIScript can now be run from outside the game, so don't
	// return out here, just skip this part
	if (cl)	// No client, get out
	{
		// Remove weapon
		if (cl->gentity && cl->gentity->client)
		{
			playerState_t*	pState = cl->gentity->client;

			//TODO - CORRECT THIS
			if ((uiInfo.selectedThrowWeapon>0) && (uiInfo.selectedThrowWeapon< 64))
			{
				pState->weapons[uiInfo.selectedThrowWeapon] = 0;
			}

			// Remove ammo too
			if ((uiInfo.selectedThrowWeaponAmmoIndex>0) && (uiInfo.selectedThrowWeaponAmmoIndex<MAX_AMMO))
			{
				pState->ammo[ uiInfo.selectedThrowWeaponAmmoIndex ] = 0;
			}

		}
	}

	// Now do a little clean up
	uiInfo.selectedThrowWeapon = NOWEAPON;
	memset(uiInfo.selectedThrowWeaponItemName,0,sizeof(uiInfo.selectedThrowWeaponItemName));
	uiInfo.selectedThrowWeaponAmmoIndex = 0;

	if (uiInfo.weaponThrowButton)
	{
		uiInfo.weaponThrowButton->descText = "@MENUS_CLICKSELECT";
		uiInfo.weaponThrowButton = NULL;
	}

#ifndef JK2_MODE
	//FIXME hack to prevent error in jk2 by disabling
	DC->startLocalSound(DC->registerSound("sound/interface/weapon_deselect.mp3", qfalse), CHAN_LOCAL );
#endif

	UI_WeaponsSelectionsComplete();	// Test to see if the mission begin button should turn on or off

}

static void	UI_NormalThrowSelection ( void )
{
	itemDef_s  *item;
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu
	if (!menu)
	{
		return;
	}

	item = (itemDef_s *) Menu_FindItemByName( menu, "chosenthrowweapon_icon" );
	item->window.background = uiInfo.unlitThrowableIcon;
}

static void	UI_HighLightThrowSelection ( void )
{
	itemDef_s  *item;
	menuDef_t	*menu;

	menu = Menu_GetFocused();	// Get current menu
	if (!menu)
	{
		return;
	}

	item = (itemDef_s *) Menu_FindItemByName( menu, "chosenthrowweapon_icon" );
	item->window.background = uiInfo.litThrowableIcon;
}

static void UI_GetSaberCvars ( void )
{
	// Just in case that g_saber_type is null for whatever reason, set it to single
	if (!Q_stricmp(Cvar_VariableString("g_saber_type"), ""))
		Cvar_Set("g_saber_type", "single");

	Cvar_Set("ui_saber_type", Cvar_VariableString("g_saber_type"));
	if (!Cvar_VariableIntegerValue("ui_saber_edit"))
	{
		Cvar_Set("ui_saber", Cvar_VariableString("g_saber"));
		Cvar_Set("ui_saber2", Cvar_VariableString("g_saber2"));
		Cvar_Set("ui_saber_color", Cvar_VariableString("g_saber_color"));
		Cvar_Set("ui_saber2_color", Cvar_VariableString("g_saber2_color"));
	}

	saber_colors_t saberColour = TranslateSaberColor(Cvar_VariableString( "ui_saber_color" ));

	if (saberColour >= SABER_RGB)
	{

		Cvar_SetValue ( "ui_rgb_saber_red", (saberColour & 0xff) );
		Cvar_SetValue ( "ui_rgb_saber_green", ((saberColour >> 8) & 0xff) );
		Cvar_SetValue ( "ui_rgb_saber_blue", ((saberColour >> 16) & 0xff) );
	}

	saber_colors_t saber2Colour = TranslateSaberColor(Cvar_VariableString( "ui_saber2_color" ));

	if (saber2Colour >= SABER_RGB)
	{

		Cvar_SetValue ( "ui_rgb_saber2_red", (saber2Colour & 0xff) );
		Cvar_SetValue ( "ui_rgb_saber2_green", ((saber2Colour >> 8) & 0xff) );
		Cvar_SetValue ( "ui_rgb_saber2_blue", ((saber2Colour >> 16) & 0xff) );
	}

	Cvar_Set ( "ui_newfightingstyle", "0");

}

static void UI_ResetSaberCvars ( void )
{
	if(Cvar_VariableIntegerValue("ui_npc_saber"))
		return;
	Cvar_Set ( "g_saber_type", "single" );
	Cvar_Set ( "g_saber", "single_1" );
	Cvar_Set ( "g_saber2", "" );

	Cvar_Set ( "ui_saber_type", "single" );
	Cvar_Set ( "ui_saber", "single_1" );
	Cvar_Set ( "ui_saber2", "" );
}

extern qboolean ItemParse_asset_model_go( itemDef_t *item, const char *name );
extern qboolean ItemParse_model_g2skin_go( itemDef_t *item, const char *skinName );
static void UI_UpdateCharacterSkin( void )
{
	menuDef_t *menu;
	itemDef_t *item;
	char skin[MAX_CSPATH];

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	item = (itemDef_s *) Menu_FindItemByName(menu, "character");

	if (!item)
	{
		Com_Error( ERR_FATAL, "UI_UpdateCharacterSkin: Could not find item (character) in menu (%s)", menu->window.name);
	}

	Com_sprintf( skin, sizeof( skin ), "models/players/%s/|%s|%s|%s",
										Cvar_VariableString ( "ui_char_model"),
										Cvar_VariableString ( "ui_char_skin_head"),
										Cvar_VariableString ( "ui_char_skin_torso"),
										Cvar_VariableString ( "ui_char_skin_legs")
				);

	ItemParse_model_g2skin_go( item, skin );
}

static void UI_CharacterDefaultSkin(const char* otherSkin)
{
	menuDef_t* menu = Menu_GetFocused();
	if (!menu)
		return;

	itemDef_t* item = (itemDef_t*)Menu_FindItemByName(menu, "character");
	if (!item)
	{
		Com_Error(ERR_FATAL,
			"UI_CharacterDefaultSkin: Could not find item (character) in menu (%s)",
			menu->window.name);
		return;
	}

	const char* modelName = Cvar_VariableString("ui_char_model");
	bool validSkinArg = false;

	auto ShowCustomizationUI = [&](bool hasCustomParts)
		{
			if (hasCustomParts)
			{
				UI_DefaultCharacterColor();
			}
			Menu_ShowItemByName(menu, "skinTabCustom", qfalse);
			Menu_ShowItemByName(menu, "skinTabPresets", hasCustomParts ? qtrue : qfalse);
			Menu_ShowItemByName(menu, "skinTabPresetsOnly", hasCustomParts ? qfalse : qtrue);
			Menu_ShowItemByName(menu, "skinList", qtrue);
		};

	// Parse head|torso|lower
	char headSkin[64] = { 0 }, torsoSkin[64] = { 0 }, lowerSkin[64] = { 0 };
	bool isDefaultRequest = otherSkin && !Q_stricmp(otherSkin, "model_default");

	if (otherSkin && !isDefaultRequest)
	{
		char skinCopy[MAX_CSPATH];
		Q_strncpyz(skinCopy, otherSkin, sizeof(skinCopy));

		char* token = strtok(skinCopy, "|");
		while (token)
		{
			if (!Q_strncmp(token, "head_", 5))
				Q_strncpyz(headSkin, token, sizeof(headSkin));
			else if (!Q_strncmp(token, "torso_", 6))
				Q_strncpyz(torsoSkin, token, sizeof(torsoSkin));
			else if (!Q_strncmp(token, "lower_", 6))
				Q_strncpyz(lowerSkin, token, sizeof(lowerSkin));

			token = strtok(NULL, "|");
		}
	}

	// Find species
	for (int i = 0; i < uiInfo.playerSpeciesCount; ++i)
	{
		auto& species = uiInfo.playerSpecies[i];

		if (Q_stricmp(modelName, species.Name))
			continue;

		uiInfo.playerSpeciesIndex = i;

		bool hasCustomParts =
			species.SkinHeadCount > 0 &&
			species.SkinTorsoCount > 0 &&
			species.SkinLegCount > 0;

		//
		// CASE 1 — Default request, custom parts available
		//
		if (isDefaultRequest && hasCustomParts)
		{
			UI_FeederSelection(FEEDER_PLAYER_SKIN_HEAD, 0, item);
			UI_FeederSelection(FEEDER_PLAYER_SKIN_TORSO, 0, item);
			UI_FeederSelection(FEEDER_PLAYER_SKIN_LEGS, 0, item);

			ShowCustomizationUI(true);
			validSkinArg = true;
			break;
		}

		//
		// CASE 2 — Explicit head|torso|lower provided
		//
		if (hasCustomParts && headSkin[0] && torsoSkin[0] && lowerSkin[0])
		{
			// Select first model skin
			UI_FeederSelection(FEEDER_MODEL_SKINS, 0, item);

			// Head
			for (int h = 0; h < species.SkinHeadCount; ++h)
			{
				if (!Q_stricmp(headSkin, species.SkinHead[h].name))
				{
					UI_FeederSelection(FEEDER_PLAYER_SKIN_HEAD, h, item);
					validSkinArg = true;
					break;
				}
			}

			// Torso
			for (int t = 0; t < species.SkinTorsoCount; ++t)
			{
				if (!Q_stricmp(torsoSkin, species.SkinTorso[t].name))
				{
					UI_FeederSelection(FEEDER_PLAYER_SKIN_TORSO, t, item);
					break;
				}
			}

			// Legs
			for (int l = 0; l < species.SkinLegCount; ++l)
			{
				if (!Q_stricmp(lowerSkin, species.SkinLeg[l].name))
				{
					UI_FeederSelection(FEEDER_PLAYER_SKIN_LEGS, l, item);
					break;
				}
			}

			ShowCustomizationUI(true);
			break;
		}

		//
		// CASE 3 — Fallback behavior
		//
		if (!validSkinArg)
		{
			if (hasCustomParts)
			{
				UI_FeederSelection(FEEDER_PLAYER_SKIN_HEAD, 0, item);
				UI_FeederSelection(FEEDER_PLAYER_SKIN_TORSO, 0, item);
				UI_FeederSelection(FEEDER_PLAYER_SKIN_LEGS, 0, item);
				ShowCustomizationUI(true);
				if (Q_stricmp("model_default", otherSkin))
				{
					for (int j = 0; j < species.SkinCount; ++j)
					{
						if (!Q_stricmp(species.Skin[j].name, otherSkin))
						{
							UI_FeederSelection(FEEDER_MODEL_SKINS, j, item);
							break;
						}
					}
				}
			}
			else
			{
				UI_FeederSelection(FEEDER_MODEL_SKINS, 0, item);

				for (int j = 0; j < species.SkinCount; ++j)
				{
					if (!Q_stricmp(species.Skin[j].name, otherSkin))
					{
						UI_FeederSelection(FEEDER_MODEL_SKINS, j, item);
						break;
					}
				}

				ShowCustomizationUI(false);
			}
		}

		break;
	}

	// Build final skin string
	/*char skin[MAX_CSPATH];
	Com_sprintf(
		skin, sizeof(skin),
		"models/players/%s/%s|%s|%s",
		modelName,
		Cvar_VariableString("ui_char_skin_head"),
		Cvar_VariableString("ui_char_skin_torso"),
		Cvar_VariableString("ui_char_skin_legs")
	);*/

	//ItemParse_model_g2skin_go(item, skin);
}



static void UI_UpdateCharacter( qboolean changedModel )
{
	menuDef_t *menu;
	itemDef_t *item;
	char modelPath[MAX_QPATH];

	menu = Menu_GetFocused();	// Get current menu

	if (!menu)
	{
		return;
	}

	item = (itemDef_s *) Menu_FindItemByName(menu, "character");

	if (!item)
	{
		Com_Error( ERR_FATAL, "UI_UpdateCharacter: Could not find item (character) in menu (%s)", menu->window.name);
	}

	ItemParse_model_g2anim_go( item, ui_char_anim.string );

	Com_sprintf( modelPath, sizeof( modelPath ), "models/players/%s/model.glm", Cvar_VariableString ( "ui_char_model" ) );
	ItemParse_asset_model_go( item, modelPath );

	if ( changedModel )
	{//set all skins to first skin since we don't know you always have all skins
		//FIXME: could try to keep the same spot in each list as you swtich models
		UI_FeederSelection(FEEDER_PLAYER_SKIN_HEAD, 0, item);	//fixme, this is not really the right item!!
		UI_FeederSelection(FEEDER_PLAYER_SKIN_TORSO, 0, item);
		UI_FeederSelection(FEEDER_PLAYER_SKIN_LEGS, 0, item);
		UI_FeederSelection(FEEDER_MODEL_SKINS, 0, item);
		UI_FeederSelection(FEEDER_COLORCHOICES, 0, item);
	}
	UI_UpdateCharacterSkin();

	// The new model walks (ui_char_anim) until its pose is set.
	UI_UpdateCharPreview(menu);
}

void UI_UpdateSaberType( void )
{
	char sType[MAX_QPATH];
	DC->getCVarString( "ui_saber_type", sType, sizeof(sType) );
	if ( Q_stricmp( "single", sType ) == 0 ||
		Q_stricmp( "staff", sType ) == 0 )
	{
		DC->setCVar( "ui_saber2", "" );
	}
}

static void UI_HiltChange(int i)
{
	// Done to fix a cosmetic issue with the menu.
	// If changing to dual from single, do nothing
	// If changing to dual from staff, change to single_1

	if (!i)
	{
		if (!Q_stricmp(Cvar_VariableString("ui_saber_type"), "staff"))
			Cvar_Set("ui_saber", "single_1");
	}
	else
	{
		menuDef_t* menu = Menu_GetFocused();

		// Let's also check to make sure the 2nd hilt disappears if the user was browsing dual then ESC'd out
		if (!Q_stricmp(Cvar_VariableString("ui_saber_type"), "single")
			|| !Q_stricmp(Cvar_VariableString("ui_saber_type"), "staff"))
		{
			Menu_ShowItemByName(menu, "saber2", qfalse);
			Menu_ShowItemByName(menu, "dualsaberedit", qfalse);
		}
	}

}

static void UI_UpdateSaberHilt( qboolean secondSaber )
{
	menuDef_t *menu;
	itemDef_t *item;
	char model[MAX_QPATH];
	char modelPath[MAX_QPATH];
	char skinPath[MAX_QPATH];
	menu = Menu_GetFocused();	// Get current menu (either video or ingame video, I would assume)

	if (!menu)
	{
		return;
	}

	const char *itemName;
	const char *saberCvarName;
	if ( secondSaber )
	{
		itemName = "saber2";
		saberCvarName = "ui_saber2";
	}
	else
	{
		itemName = "saber";
		saberCvarName = "ui_saber";
	}

	item = (itemDef_s *) Menu_FindItemByName(menu, itemName );

	if(!item)
	{
		Com_Error( ERR_FATAL, "UI_UpdateSaberHilt: Could not find item (%s) in menu (%s)", itemName, menu->window.name);
	}

	if (secondSaber)
	{
		if (!Q_stricmp(Cvar_VariableString("ui_saber2"), "") || !Q_stricmp(Cvar_VariableString("ui_saber2"), "empty"))
		{
			item->window.flags &= ~WINDOW_VISIBLE;
		}
		else
		{
			item->window.flags |= WINDOW_VISIBLE;
		}
	}
	DC->getCVarString( saberCvarName, model, sizeof(model) );
	//read this from the sabers.cfg
	if ( UI_SaberModelForSaber( model, modelPath ) )
	{//successfully found a model
		ItemParse_asset_model_go( item, modelPath );//set the model
		//get the customSkin, if any
		//COM_StripExtension( modelPath, skinPath, sizeof(skinPath) );
		//COM_DefaultExtension( skinPath, sizeof( skinPath ), ".skin" );
		if ( UI_SaberSkinForSaber( model, skinPath ) )
		{
			ItemParse_model_g2skin_go( item, skinPath );//apply the skin
		}
		else
		{
			ItemParse_model_g2skin_go( item, NULL );//apply the skin
		}
	}
}

/*
static void UI_UpdateSaberColor( qboolean secondSaber )
{
	int sabernumber;
	if (secondSaber)
		sabernumber = 2;
	else
		sabernumber = 1;

	ui.Cmd_ExecuteText( EXEC_APPEND, va("sabercolor %i %s\n",sabernumber, Cvar_VariableString("g_saber_color")));
}
*/
char GoToMenu[1024];

/*
=================
Menus_SaveGoToMenu
=================
*/
void Menus_SaveGoToMenu(const char *menuTo)
{
	memcpy(GoToMenu, menuTo, sizeof(GoToMenu));
}

/*
=================
UI_CheckVid1Data
=================
*/
void UI_CheckVid1Data(const char *menuTo,const char *warningMenuName)
{
	menuDef_t *menu;
	itemDef_t *applyChanges;

	menu = Menu_GetFocused();	// Get current menu (either video or ingame video, I would assume)

	if (!menu)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: No videoMenu was found. Video data could not be checked\n");
		return;
	}

	applyChanges = (itemDef_s *) Menu_FindItemByName(menu, "applyChanges");

	if (!applyChanges)
	{
//		Menus_CloseAll();
		Menus_OpenByName(menuTo);
		return;
	}

	if ((applyChanges->window.flags & WINDOW_VISIBLE))	// Is the APPLY CHANGES button active?
	{
//		Menus_SaveGoToMenu(menuTo);							// Save menu you're going to
//		Menus_HideItems(menu->window.name);					// HIDE videMenu in case you have to come back
		Menus_OpenByName(warningMenuName);				// Give warning
	}
	else
	{
//		Menus_CloseAll();
//		Menus_OpenByName(menuTo);
	}
}

/*
=================
UI_ResetDefaults
=================
*/
void UI_ResetDefaults( void )
{
	ui.Cmd_ExecuteText( EXEC_APPEND, "cvar_restart\n");
	Controls_SetDefaults();
	ui.Cmd_ExecuteText( EXEC_APPEND, "exec default.cfg\n");
	ui.Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

/*
=======================
UI_SortSaveGames
=======================
*/
static int UI_SortSaveGames( const void *A, const void *B )
{

	const int &a = ((savedata_t*)A)->currentSaveFileDateTime;
	const int &b = ((savedata_t*)B)->currentSaveFileDateTime;

	if (a > b)
	{
		return -1;
	}
	else
	{
		return (a < b);
	}
}

/*
=======================
UI_AdjustSaveGameListBox
=======================
*/
// Yeah I could get fired for this... in a world of good and bad, this is bad
// I wish we passed in the menu item to RunScript(), oh well...
void UI_AdjustSaveGameListBox( int currentLine )
{
	menuDef_t *menu;
	itemDef_t *item;

	// could be in either the ingame or shell load menu (I know, I know it's bad)
	menu = Menus_FindByName("loadgameMenu");
	if( !menu )
	{
		menu = Menus_FindByName("ingameloadMenu");
	}

	if (menu)
	{
		item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, "loadgamelist");
		if (item)
		{
			listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;
			if( listPtr )
			{
				listPtr->cursorPos = currentLine;
			}

			item->cursorPos = currentLine;
		}
	}

}

/*
=================
ReadSaveDirectory
=================
*/
//JLFSAVEGAME MPNOTUSED
void ReadSaveDirectory (void)
{
	int		i;
	char	*holdChar;
	int		len;
	int		fileCnt;
	// Clear out save data
	memset(s_savedata,0,sizeof(s_savedata));
	s_savegame.saveFileCnt = 0;
	Cvar_Set("ui_gameDesc", "" );	// Blank out comment
	Cvar_Set("ui_SelectionOK", "0" );
#ifdef JK2_MODE
	memset( screenShotBuf,0,(SG_SCR_WIDTH * SG_SCR_HEIGHT * 4)); //blank out sshot
#endif


	// Get everything in saves directory
	fileCnt = ui.FS_GetFileList("saves", ".sav", s_savegame.listBuf, LISTBUFSIZE );

	Cvar_Set("ui_ResumeOK", "0" );
	holdChar = s_savegame.listBuf;
	for ( i = 0; i < fileCnt; i++ )
	{
		// strip extension
		len = strlen( holdChar );
		holdChar[len-4] = '\0';

		if	( Q_stricmp("current",holdChar)!=0 )
		{
			time_t result;
			if (Q_stricmp("auto",holdChar)==0)
			{
				Cvar_Set("ui_ResumeOK", "1" );
			}
			else
			{	// Is this a valid file??? & Get comment of file
				result = ui.SG_GetSaveGameComment(holdChar, s_savedata[s_savegame.saveFileCnt].currentSaveFileComments, s_savedata[s_savegame.saveFileCnt].currentSaveFileMap);
				if (result != 0) // ignore Bad save game
				{
					s_savedata[s_savegame.saveFileCnt].currentSaveFileName = holdChar;
					s_savedata[s_savegame.saveFileCnt].currentSaveFileDateTime = result;

					struct tm *localTime;
					localTime = localtime( &result );
					strcpy(s_savedata[s_savegame.saveFileCnt].currentSaveFileDateTimeString,asctime( localTime ) );
					s_savegame.saveFileCnt++;
					if (s_savegame.saveFileCnt == MAX_SAVELOADFILES)
					{
						break;
					}
				}
			}
		}

		holdChar += len + 1;	//move to next item
	}

	qsort( s_savedata, s_savegame.saveFileCnt, sizeof(savedata_t), UI_SortSaveGames );

}

// Write the character cvars (powers, weapons, sabers, colors, stats) as "<cvar> <value>" lines.
static void UI_WriteCharacterCvars(fileHandle_t file)
{
	// list of UI power cvars to write
	const char* cvars[] =
	{
		"ui_jump_level",
		"ui_push_level",
		"ui_pull_level",
		"ui_speed_level",
		"ui_sense_level",

		"ui_absorb_level",
		"ui_heal_level",
		"ui_protect_level",
		"ui_mindtrick_level",
		"ui_stasis_level",
		"ui_grasp_level",
		"ui_blast_level",

		"ui_grip_level",
		"ui_lightning_level",
		"ui_drain_level",
		"ui_rage_level",
		"ui_destruction_level",
		"ui_fear_level",
		"ui_strike_level",

		"ui_saboff_level",
		"ui_sabdef_level",
		"ui_sabthrow_level",


		"ui_npc_weapon",
		"ui_weaponOne",
		"ui_weaponTwo",
		"ui_weaponThree",
		"ui_weaponFour",
		"ui_weaponFive",
		"ui_weaponSix",

		"ui_saber",
		"ui_saber_color",
		"ui_saber2",
		"ui_saber2_color",

		"ui_lightning_color",
		"ui_char_color_red",
		"ui_char_color_green",
		"ui_char_color_blue",

		"ui_health",
		"ui_force",
		"ui_team",
		"ui_npc_type",
		"ui_saber_styles",
	};

	const size_t count = sizeof(cvars) / sizeof(cvars[0]);

	for (size_t i = 0; i < count; ++i)
	{
		const char* value = Cvar_VariableString(cvars[i]);

		// Don't write secondary saber cvars if they are empty (or set to the placeholder "empty")
		if ((Q_stricmp(cvars[i], "ui_saber2") == 0 || Q_stricmp(cvars[i], "ui_saber2_color") == 0))
		{
			if (!value || value[0] == '\0' || !Q_stricmp(value, "empty"))
			{
				if ((Q_stricmp(cvars[i], "ui_saber2") == 0))
				{
					value = "empty";
					Cvar_Set("ui_saber2_color", "blue");
				}
				else
				{
					value = "blue";
				}
			}
		}

		if (Q_stricmp(cvars[i], "ui_saber_color") == 0)
		{
			if (TranslateSaberColor(Cvar_VariableString("ui_saber_color")) >= SABER_RGB)
			{
				char rgbColor[8];

				if (Cvar_VariableIntegerValue("ui_rgb_saber_red") < 0)
					Cvar_Set("ui_rgb_saber_red", 0);
				if (Cvar_VariableIntegerValue("ui_rgb_saber_green") < 0)
					Cvar_Set("ui_rgb_saber_green", 0);
				if (Cvar_VariableIntegerValue("ui_rgb_saber_blue") < 0)
					Cvar_Set("ui_rgb_saber_blue", 0);

				Com_sprintf(rgbColor, 8, "x%02x%02x%02x", Cvar_VariableIntegerValue("ui_rgb_saber_red"),
					(Cvar_VariableIntegerValue("ui_rgb_saber_green")),
					(Cvar_VariableIntegerValue("ui_rgb_saber_blue")));

				value = rgbColor;
			}
		}

		if (Q_stricmp(cvars[i], "ui_saber2_color") == 0)
		{
			if (TranslateSaberColor(Cvar_VariableString("ui_saber2_color")) >= SABER_RGB)
			{
				char rgbColor[8];

				if (Cvar_VariableIntegerValue("ui_rgb_saber2_red") < 0)
					Cvar_Set("ui_rgb_saber2_red", 0);
				if (Cvar_VariableIntegerValue("ui_rgb_saber2_green") < 0)
					Cvar_Set("ui_rgb_saber2_green", 0);
				if (Cvar_VariableIntegerValue("ui_rgb_saber2_blue") < 0)
					Cvar_Set("ui_rgb_saber2_blue", 0);

				Com_sprintf(rgbColor, 8, "x%02x%02x%02x", Cvar_VariableIntegerValue("ui_rgb_saber2_red"),
					(Cvar_VariableIntegerValue("ui_rgb_saber2_green")),
					(Cvar_VariableIntegerValue("ui_rgb_saber2_blue")));

				value = rgbColor;
			}
		}


		// write as: <cvar> <value>\n
		FS_Printf(file, "%s %s\n", cvars[i], value);
	}
}

static void UI_SaveCharacterPowers(void)
{
	static char characterName[MAX_QPATH];
	char code[64];
	char variant[64];

	strncpy(code, UI_Cvar_VariableString("g_charKey"), sizeof(code));
	strncpy(variant, UI_Cvar_VariableString("ui_variant_code"), sizeof(variant));

	// characterfile
	if (!characterfile)
	{
		// NOTE: always saves in working dir if using one...
		if (Cvar_VariableIntegerValue("ui_npc_menu"))
			Com_sprintf(characterName, MAX_QPATH, "ext_data/characters/%s_%s_NPC.cfg", code, variant);
		else
			Com_sprintf(characterName, MAX_QPATH, "ext_data/characters/%s_%s.cfg", code, variant);
		characterfile = FS_FOpenFileWrite(characterName);
	}

	if (characterfile)
	{
		// write header so file is easier to read
		FS_Printf(characterfile, "// character abilities saved by UI\n");
		UI_WriteCharacterCvars(characterfile);
	}

#ifdef DEBUG
	Com_Printf("Writing Character Stats in %s\n", characterName);
#endif
	Com_FlushCharacterFile();
}

void Com_FlushCharacterFile()
{
	char code[64];
	char variant[64];

	strncpy(code, UI_Cvar_VariableString("g_charKey"), sizeof(code));
	strncpy(variant, UI_Cvar_VariableString("ui_variant_code"), sizeof(variant));
	if (!characterfile)
	{
		// nothing to flush, right?
		Com_Printf("No character file available\n");
		return;
	}
	FS_ForceFlush(characterfile);
	FS_FCloseFile(characterfile);
	characterfile = 0;

	static	char	flushedCharactername[MAX_QPATH];
	if (Cvar_VariableIntegerValue("ui_npc_menu"))
		Com_sprintf(flushedCharactername, MAX_QPATH, "ext_data/characters/%s_%s_NPC.cfg", code, variant);
	else
		Com_sprintf(flushedCharactername, MAX_QPATH, "ext_data/characters/%s_%s.cfg", code, variant);

	Com_Printf("saved Character stats to %s\n", flushedCharactername);
}

// Execute the "<cvar> <value>" lines of a character cfg, then update the hilts and the model.
// Return qfalse if the file does not exist.
static qboolean UI_ExecCharacterCfgFile(const char *tryFile)
{
	char* buf = NULL;
	int len = ui.FS_ReadFile(tryFile, (void**)&buf);

	if (len > 0 && buf)
	{
		// Convert file contents to std::string
		std::string content(buf, len);
		ui.FS_FreeFile(buf);
		buf = NULL;

		//
		// Parse file line-by-line and execute commands synchronously
		//
		size_t pos = 0;
		while (pos < content.size())
		{
			size_t lineEnd = content.find_first_of("\r\n", pos);
			std::string line;

			if (lineEnd == std::string::npos)
			{
				line = content.substr(pos);
				pos = content.size();
			}
			else
			{
				line = content.substr(pos, lineEnd - pos);
				pos = lineEnd;
				while (pos < content.size() && (content[pos] == '\r' || content[pos] == '\n'))
					pos++;
			}

			// Trim whitespace
			size_t start = 0;
			while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
				start++;

			size_t end = line.size();
			while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t'))
				end--;

			if (end <= start)
				continue;

			std::string cmd = line.substr(start, end - start);

			// Skip // comments
			if (cmd.size() >= 2 && cmd[0] == '/' && cmd[1] == '/')
				continue;

			// Skip empty/semicolon-only lines
			bool allWhitespaceOrSemicolon = true;
			for (char c : cmd)
			{
				if (c != ' ' && c != '\t' && c != ';')
				{
					allWhitespaceOrSemicolon = false;
					break;
				}
			}
			if (allWhitespaceOrSemicolon)
				continue;

			ui.Cmd_ExecuteText(EXEC_NOW, va("%s\n", cmd.c_str()));
		}
#ifdef DEBUG
	Com_Printf("UI_LoadCharacterCfg: executed %s\n", tryFile);
#endif

		

		//
		// Update saber/hilt UI
		//
		UI_UpdateSaberHilt(qfalse);
		UI_UpdateSaberHilt(qtrue);

		itemDef_t* item;
		menuDef_t* menu;
		modelDef_t* modelPtr;
		char skin[128];

		uiInfo.movesTitleIndex = 0;
		uiInfo.movesBaseAnim = "BOTH_STAND1IDLE1";

		menu = Menus_FindByName("IngameSWGLChars");

		if (menu)
		{
			item = (itemDef_s*)Menu_FindItemByName((menuDef_t*)menu, "character");
			if (item)
			{
				modelPtr = (modelDef_t*)item->typeData;
				if (modelPtr)
				{
					ItemParse_model_g2anim_go(item, uiInfo.movesBaseAnim);
					uiInfo.moveAnimTime = 0;

					Com_sprintf(skin, sizeof(skin), "models/players/%s/|%s|%s|%s",
						Cvar_VariableString("ui_char_model"),
						Cvar_VariableString("ui_char_skin_head"),
						Cvar_VariableString("ui_char_skin_torso"),
						Cvar_VariableString("ui_char_skin_legs")
					);

					uiPreviewSig[0] = 0;
				}
			}
		}

		return qtrue;
	}

	if (buf)
	{
		ui.FS_FreeFile(buf);
		buf = NULL;
	}
	return qfalse;
}

void UI_LoadCharacterCfg(void)
{
	char filename[MAX_QPATH];
	char* buf = NULL;
	int len = 0;

	char code[64];
	char variant[64];

	Q_strncpyz(code, UI_Cvar_VariableString("g_charKey"), sizeof(code));
	Q_strncpyz(variant, UI_Cvar_VariableString("ui_variant_code"), sizeof(variant));

	//
	// Build the three candidate filenames explicitly and safely
	//
	static char file1[MAX_QPATH];
	static char file2[MAX_QPATH];

	// 1. code_variant.cfg
	if (Cvar_VariableIntegerValue("ui_npc_menu"))
		Com_sprintf(file1, sizeof(file1),
			"ext_data/characters/%s_%s_NPC.cfg",
			code, variant);
	else
		Com_sprintf(file1, sizeof(file1),
			"ext_data/characters/%s_%s.cfg",
			code, variant);

	// 2. code_variant_default.cfg
	Com_sprintf(file2, sizeof(file2),
		"ext_data/characters_conf/%s_%s_def.cfg",
		code, variant);

	const char* candidates[3] = {
		file1,
		file2,
		"ext_data/characters/default.cfg"
	};

	for (int i = 0; i < 3; i++)
	{
		if (UI_ExecCharacterCfgFile(candidates[i]))
		{
			return;
		}
	}

	Com_Printf("UI_LoadCharacterCfg: no character cfg found for code '%s' variant '%s'\n",
		code, variant);
}

void UI_LoadCharacterDefaultCfg(void)
{
	char filename[MAX_QPATH];
	char* buf = NULL;
	int len = 0;

	char code[64];
	char variant[64];

	Q_strncpyz(code, UI_Cvar_VariableString("g_charKey"), sizeof(code));
	Q_strncpyz(variant, UI_Cvar_VariableString("ui_variant_code"), sizeof(variant));

	//
	// Build the three candidate filenames explicitly and safely
	//
	static char file[MAX_QPATH];

	// 2. code_variant_default.cfg
	Com_sprintf(file, sizeof(file),
		"ext_data/characters_conf/%s_%s_def.cfg",
		code, variant);

	const char* candidates[2] = {
		file,
		"ext_data/characters/default.cfg"
	};

	for (int i = 0; i < 2; i++)
	{
		if (UI_ExecCharacterCfgFile(candidates[i]))
		{
			UI_SaveCharacterPowers();
			return;
		}
	}

	Com_Printf("UI_LoadCharacterCfg: no character cfg found for code '%s' variant '%s'\n",
		code, variant);
}

/*
=================
Character configurations (character menu, "Configurations" box)

The configurations belong to a variant. The list holds "default configuration" first, then the files
characters_configs/<g_charKey>_<ui_variant_code>/<name>.cfg. Save writes the character cvars to
characters_configs/<g_charKey>_<ui_variant_code>/<ui_char_config_name>.cfg (it
overwrites a file with the same name). An error goes to the cvar ui_char_config_error as
"<serial>|<message>": the cgame ownerdraw shows it for 3 seconds.
=================
*/
#define MAX_CHAR_CONFIGS		64
#define CHAR_CONFIG_DEFAULT		"default configuration"
#define CHAR_CONFIG_NAME_LEN	48


// Path of a configuration of the current variant; an empty name gives the folder.
static const char *UI_CharConfigPath(const char *name)
{
	// Not UI_Cvar_VariableString: it has one static buffer, so the second call overwrites the first.
	const char *folder = va("characters_configs/%s_%s", Cvar_VariableString("g_charKey"), Cvar_VariableString("ui_variant_code"));
	return name[0] ? va("%s/%s.cfg", folder, name) : folder;
}

static void UI_CharConfigError(const char *message)
{
	static int serial;
	Cvar_Set("ui_char_config_error", va("%d|%s", ++serial, message));
}

// Select a line of the list, and show it in the list box of the menu.
static void UI_SelectCharConfig(int index)
{
	uiCharConfigSelected = index;
	Cvar_Set("ui_char_config_name", index > 0 ? uiCharConfigs[index] : "");

	menuDef_t *menu = Menus_FindByName("IngameSWGLChars");
	itemDef_t *item = menu ? (itemDef_t *)Menu_FindItemByName(menu, "charConfigList") : NULL;
	if (item)
	{
		item->cursorPos = index;
	}
}

// Read the list again. Select the configuration "select" if it is in the list, else the default one.
static void UI_RefreshCharConfigs(const char *select)
{
	char list[8192];
	const int count = ui.FS_GetFileList(UI_CharConfigPath(""), ".cfg", list, sizeof(list));
	const char *name = list;

	uiCharConfigCount = 0;
	Q_strncpyz(uiCharConfigs[uiCharConfigCount++], CHAR_CONFIG_DEFAULT, MAX_QPATH);
	for (int i = 0; i < count && uiCharConfigCount < MAX_CHAR_CONFIGS; i++, name += strlen(name) + 1)
	{
		COM_StripExtension(name, uiCharConfigs[uiCharConfigCount++], MAX_QPATH);
	}

	int selected = 0;
	for (int i = 0; select && i < uiCharConfigCount; i++)
	{
		if (!Q_stricmp(uiCharConfigs[i], select))
		{
			selected = i;
		}
	}
	UI_SelectCharConfig(selected);
}

/*
=================
UI_BuildAnimOverrides

Lists the animation overrides for FEEDER_ANIM_OVERRIDES and FEEDER_CHAR_MOVESET: each models/players/<name>/<name>.gla
that the renderer can append to _humanoid.gla (see mdx_merge.h).
=================
*/
static void UI_BuildAnimOverrides(void)
{
	std::vector<byte> baseBuf, weaponsBuf, extraBuf;
	std::vector<std::string> names;

	const mdxaHeader_t *base = UI_ReadGLAHeader(GLA_HUMANOID_PATH, baseBuf);
	if (base)
	{
		// The override frames come after the frames of _humanoid.gla and _weapons.gla.
		int numFrames = LittleLong(base->numFrames);
		const mdxaHeader_t *weapons = UI_ReadGLAHeader(GLA_WEAPONS_PATH, weaponsBuf);
		if (weapons && GLA_CanMerge(base, numFrames, weapons))
		{
			numFrames += LittleLong(weapons->numFrames);
		}

		// The "/" list shows only the folders that have a folder entry in their pk3. The ".gla" list ("<name>/<file>.gla") shows the other folders.
		std::vector<std::string> folders;
		std::vector<char> fileList(256 * 1024);
		const char *extensions[] = { "/", ".gla" };
		for (const char *extension : extensions)
		{
			const int numFiles = ui.FS_GetFileList("models/players", extension, fileList.data(), (int)fileList.size());
			const char *file = fileList.data();
			for (int i = 0; i < numFiles; i++, file += strlen(file) + 1)
			{
				const char *slash = strchr(file, '/');
				const std::string folder = slash ? std::string(file, slash - file) : std::string(file);
				if (std::none_of(folders.begin(), folders.end(), [&](const std::string &f) { return !Q_stricmp(f.c_str(), folder.c_str()); }))
				{
					folders.push_back(folder);
				}
			}
		}

		for (const std::string &folder : folders)
		{
			char name[MAX_QPATH];
			Q_strncpyz(name, folder.c_str(), sizeof(name));
			// _humanoid*.gla are skeletons, not overrides.
			if (!name[0] || name[0] == '.' || !Q_stricmpn(name, "_humanoid", 9))
			{
				continue;
			}

			char path[MAX_QPATH];
			Com_sprintf(path, sizeof(path), "models/players/%s/%s.gla", name, name);
			const mdxaHeader_t *extra = UI_ReadGLAHeader(path, extraBuf);
			if (extra && GLA_CanMerge(base, numFrames, extra))
			{
				names.push_back(name);
			}
		}
	}
	std::sort(names.begin(), names.end(), [](const std::string &a, const std::string &b) { return Q_stricmp(a.c_str(), b.c_str()) < 0; });

	uiAnimOverrides.clear();
	uiAnimOverrides.push_back("none");
	uiAnimOverrides.push_back("default");
	uiAnimOverrides.insert(uiAnimOverrides.end(), names.begin(), names.end());
}

/*
=================
UI_RefreshAnimOverrides

Selects the value of the cheat menu list (ui_animoverride) and of the character menu list (ui_char_moveset).
The list is built again only with rebuildList: the character menu refreshes it at every character change.
=================
*/
static void UI_RefreshAnimOverrides(qboolean rebuildList)
{
	if (rebuildList || uiAnimOverrides.empty())
	{
		UI_BuildAnimOverrides();
	}

	// The selection is the override forced on the player now. A new character has none.
	const char *forcedGLA = "";
	const client_t *cl = &svs.clients[0];	// 0 because only ever us as a player
	if (cl->gentity && cl->gentity->client)
	{
		for (int i = 0; i < cl->gentity->ghoul2.size() && !forcedGLA[0]; i++)
		{
			forcedGLA = cl->gentity->ghoul2[i].mAnimOverride;
		}
	}

	int selected = 0;
	for (int i = 1; forcedGLA[0] && i < (int)uiAnimOverrides.size(); i++)
	{
		char path[MAX_QPATH];
		if (i == 1)
		{
			Q_strncpyz(path, GLA_HUMANOID_PATH, sizeof(path));
		}
		else
		{
			GLA_OverridePath(uiAnimOverrides[i].c_str(), path, sizeof(path));
		}
		if (!Q_stricmp(path, forcedGLA))
		{
			selected = i;
			break;
		}
	}
	Cvar_Set("ui_animoverride", uiAnimOverrides[selected].c_str());

	menuDef_t *menu = Menus_FindByName("IngameSWGLCheat");
	itemDef_t *item = menu ? (itemDef_t *)Menu_FindItemByName(menu, "animOverrideList") : NULL;
	if (item)
	{
		item->cursorPos = selected;
	}

	int movesetSelected = 0;
	const char *moveset = UI_Cvar_VariableString("ui_char_moveset");
	for (int i = 0; i < (int)uiAnimOverrides.size(); i++)
	{
		if (!Q_stricmp(uiAnimOverrides[i].c_str(), moveset))
		{
			movesetSelected = i;
		}
	}
	menu = Menus_FindByName("IngameSWGLChars");
	item = menu ? (itemDef_t *)Menu_FindItemByName(menu, "charMovesetList") : NULL;
	if (item)
	{
		item->cursorPos = movesetSelected;
	}
}

// Selects a line of FEEDER_AMMO: ui_cheats_ammoType gets its ammo type, ui_cheats_ammo the ammo of the player.
static void UI_SelectAmmoType(int index)
{
	if (index < 0 || index >= (int)uiAmmoTypes.size())
	{
		return;
	}
	const int ammoType = uiAmmoTypes[index];
	Cvar_Set("ui_cheats_ammoType", va("%d", ammoType));

	const client_t *cl = &svs.clients[0];	// 0 because only ever us as a player
	if (cl->gentity && cl->gentity->client)
	{
		Cvar_Set("ui_cheats_ammo", va("%d", cl->gentity->client->ammo[ammoType]));
	}
}

/*
=================
UI_RefreshAmmoTypes

Lists the ammo types of the player for FEEDER_AMMO. The ammo data and the names come from the cgame (CG_GET_AMMO_NAME).
Selects the value of ui_cheats_ammoType.
=================
*/
static void UI_RefreshAmmoTypes(void)
{
	uiAmmoTypes.clear();
	uiAmmoNames.clear();
	for (int i = 0; i < MAX_AMMO; i++)
	{
		char name[128];
		if (VM_Call(CG_GET_AMMO_NAME, (intptr_t)i, (intptr_t)name, (intptr_t)sizeof(name)) == qtrue)
		{
			uiAmmoTypes.push_back(i);
			uiAmmoNames.push_back(name);
		}
	}

	int selected = 0;
	const int current = Cvar_VariableIntegerValue("ui_cheats_ammoType");
	for (int i = 0; i < (int)uiAmmoTypes.size(); i++)
	{
		if (uiAmmoTypes[i] == current)
		{
			selected = i;
		}
	}
	UI_SelectAmmoType(selected);

	menuDef_t *menu = Menus_FindByName("IngameSWGLCheat");
	itemDef_t *item = menu ? (itemDef_t *)Menu_FindItemByName(menu, "ammoList") : NULL;
	if (item)
	{
		item->cursorPos = selected;
	}
}

// A name is valid when it can be a file name on all systems.
static qboolean UI_ValidCharConfigName(const char *name)
{
	const size_t len = strlen(name);
	if (len >= CHAR_CONFIG_NAME_LEN || name[0] == '.' || name[len - 1] == '.')
	{
		return qfalse;
	}
	for (const char *c = name; *c; c++)
	{
		if ((unsigned char)*c < 32 || strchr("\\/:*?\"<>|", *c))
		{
			return qfalse;
		}
	}
	return qtrue;
}

static void UI_SaveCharConfig(void)
{
	char name[MAX_QPATH];
	Q_strncpyz(name, UI_Cvar_VariableString("ui_char_config_name"), sizeof(name));

	// Remove the spaces at the start and at the end.
	char *start = name;
	while (*start == ' ')
	{
		start++;
	}
	size_t len = strlen(start);
	while (len > 0 && start[len - 1] == ' ')
	{
		start[--len] = '\0';
	}

	if (!start[0])
	{
		UI_CharConfigError("Enter a configuration name");
		return;
	}
	if (!Q_stricmp(start, CHAR_CONFIG_DEFAULT))
	{
		UI_CharConfigError("This name is reserved");
		return;
	}
	if (!UI_ValidCharConfigName(start))
	{
		UI_CharConfigError("Invalid configuration name");
		return;
	}

	fileHandle_t file = FS_FOpenFileWrite(UI_CharConfigPath(start));
	if (!file)
	{
		UI_CharConfigError("Cannot write this configuration");
		return;
	}
	FS_Printf(file, "// character configuration saved by UI\n");
	UI_WriteCharacterCvars(file);
	// The skin of the character menu: model skin, or head / torso / lower parts.
	FS_Printf(file, "ui_char_skin_head %s\n", Cvar_VariableString("ui_char_skin_head"));
	FS_Printf(file, "ui_char_skin_torso %s\n", Cvar_VariableString("ui_char_skin_torso"));
	FS_Printf(file, "ui_char_skin_legs %s\n", Cvar_VariableString("ui_char_skin_legs"));
	FS_FCloseFile(file);

	UI_RefreshCharConfigs(start);
}

static void UI_LoadCharConfig(void)
{
	if (uiCharConfigSelected <= 0 || uiCharConfigSelected >= uiCharConfigCount)
	{
		UI_LoadCharacterDefaultCfg();
		// Default skin of the model, as a character change.
		UI_RunMenuCommand("uiScript \"char_default_skin\" \"model_default\"");
	}
	else
	{
		UI_ExecCharacterCfgFile(UI_CharConfigPath(uiCharConfigs[uiCharConfigSelected]));
	}

	// Same updates as a character change, after its cfg.
	UI_RunMenuCommand("uiScript char_skin");
	UI_RunMenuCommand("uiScript getsaberstyle");
	UI_RunMenuCommand("uiScript rgbsabercvars");
	UI_RunMenuCommand("uiScript char_weapon");
}

// The first click asks for a confirmation; a second click on the same configuration in 3 seconds deletes it.
static void UI_DeleteCharConfig(void)
{
	const int confirmTime = 3000;

	if (uiCharConfigSelected <= 0 || uiCharConfigSelected >= uiCharConfigCount)
	{
		UI_CharConfigError("The default configuration cannot be deleted");
		return;
	}
	if (uiCharConfigDeleteIndex != uiCharConfigSelected || uiInfo.uiDC.realTime >= uiCharConfigDeleteTime)
	{
		uiCharConfigDeleteIndex = uiCharConfigSelected;
		uiCharConfigDeleteTime = uiInfo.uiDC.realTime + confirmTime;
		UI_CharConfigError("Click Delete one more time to confirm");
		return;
	}
	uiCharConfigDeleteIndex = -1;
	FS_DeleteUserGenFile(UI_CharConfigPath(uiCharConfigs[uiCharConfigSelected]));
	UI_RefreshCharConfigs(NULL);
}
