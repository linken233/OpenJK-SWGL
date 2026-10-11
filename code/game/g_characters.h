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

#ifndef __G_CHARACTERS_H__
#define __G_CHARACTERS_H__

#include "../qcommon/q_shared.h"

#define MAX_CHARACTERS 2048
#define MAX_FACTIONS 64
#define MAX_TAGS 256

#pragma region Character Data Structures
typedef struct characterFaction_s {
	char code[16];
	char nameKey[64];
	char icon[64];
	qboolean selectedFilter = qfalse;
} characterFaction_t;

typedef struct characterTag_s {
	char code[32];
	qboolean selectedFilter = qfalse;
} characterTag_t;

typedef struct characterSkin_s {
	char icon[64];
	char skin[64];
} characterSkin_t;

// A list of names in a .cha file: name { "a" "b" ... }
typedef struct characterList_s {
	char (*items)[64];
	int count;
} characterList_t;

typedef struct characterVariant_s {
	char code[32];
	char nameKey[64];
	char name[64];
	char npcName[64];
	char descriptionKey[64];
	char icon[64];
	char model[64];

	characterSkin_s *presetList;
	int presetCount;

	characterSkin_s* lowerSkinList;
	int lowerSkinCount;

	characterSkin_s* torsoSkinList;
	int torsoSkinCount;

	characterSkin_s* headSkinList;
	int headSkinCount;

	char weapons[64][6];
	int forcePowers[NUM_FORCE_POWERS];
	// Random parts, picked again at each spawn or Apply (g_characterRandom.cpp)
	characterList_t npcRandom;			// NPC files, one is picked
	qboolean skinRandom;				// random head, torso and lower skins of the model
	characterList_t saberRandom;		// single sabers
	characterList_t saberDualRandom;	// left hand sabers of a single saber
	characterList_t saberStaffRandom;	// staff sabers
	characterList_t saberColorRandom;	// saber colors
	//TODO Add mor fields (Saber - Abilities - etc)
} characterVariant_t;

typedef struct characterInfo_s{
	char code[32];
	char nameKey[64];
	char name[64];
	char icon[64];
	char factions[256]; //LIST OF CODE OF FACTIONS THAT THIS CHARACTER BELONGS TO (SEPARATED BY COMMA)
	char tags[256];
	
	characterVariant_s* variantList;
	int variantCount;
} characterInfo_t;
#pragma endregion

#pragma region Global Character Data
extern characterFaction_t factionsData[MAX_FACTIONS];
extern int loadedFactions;
extern characterInfo_t charactersData[MAX_CHARACTERS];
extern int loadedCharacters;
extern characterTag_t tagsData[MAX_TAGS];	// every distinct tag of the characters, sorted by code
extern int loadedTags;

// True if the comma separated list contains this exact item (case insensitive).
qboolean CHA_ListHasItem(const char *list, const char *item);
// The variant <variantCode> of the character <charKey>, or NULL.
const characterVariant_t *CHA_FindVariant(const char *charKey, const char *variantCode);
// True if the variant has a random part.
qboolean CHA_VariantIsRandom(const characterVariant_t *variant);

#pragma endregion

#pragma region Faction Functions
#pragma endregion

#pragma region Character Functions
#pragma endregion

#endif //__G_CHARACTERS_H__
