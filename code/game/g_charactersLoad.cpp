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

#include "g_charactersLoad.h"
#include "g_local.h"
#include "g_parseUtils.h"

const char* CHAR_DATA_DIR = "ext_data/characters/";

// Alphabetical order for the menu grid. Both sort on the key
static int CHA_SortFactionsByName(const void* a, const void* b)
{
	return Q_stricmp(((const characterFaction_t*)a)->nameKey, ((const characterFaction_t*)b)->nameKey);
}

static int CHA_SortCharactersByName(const void* a, const void* b)
{
	const characterInfo_t* charA = (const characterInfo_t*)a;
	const characterInfo_t* charB = (const characterInfo_t*)b;
	char nameA[64] = { 0 };
	char nameB[64] = { 0 };
	if (charA->nameKey != NULL && charA->nameKey[0] != '\0')
	{
		Com_sprintf(nameA, sizeof(charA->nameKey), charA->nameKey);
	}
	else if (charA->name != NULL && charA->name[0] != '\0')
	{
		Com_sprintf(nameA, sizeof(charA->name), charA->name);
	}
	else
	{
		Com_sprintf(nameA, sizeof(charA->code), charA->code);
	}

	if (charB->nameKey != NULL && charB->nameKey[0] != '\0')
	{
		Com_sprintf(nameB, sizeof(charB->nameKey), charB->nameKey);
	}
	else if (charB->name != NULL && charB->name[0] != '\0')
	{
		Com_sprintf(nameB, sizeof(charB->name), charB->name);
	}
	else
	{
		Com_sprintf(nameB, sizeof(charB->code), charB->code);
	}

	return Q_stricmp(nameA, nameB);
}
/*
	Faction Loading
	Methods to initialize and load faction data from the faction list file.
*/
#pragma region Faction Load
void CHA_ParseFaction(const char** holdBuf)
{
	const char* token;

	while (holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);

		if (!Q_stricmp(token, "}"))	// End of data for this faction
		{
			loadedFactions++;
			break;
		}
		if (!Q_stricmp(token, "code"))
		{
			ParseStr(holdBuf, factionsData[loadedFactions].code, 16, "faction code");
			continue;
		}
		if (!Q_stricmp(token, "nameKey"))
		{
			ParseStr(holdBuf, factionsData[loadedFactions].nameKey, 64, "faction name key");
			continue;
		}
		if (!Q_stricmp(token, "icon"))
		{
			ParseStr(holdBuf, factionsData[loadedFactions].icon, 64, "faction icon");
			continue;
		}
		SkipRestOfLine(holdBuf);
	}
}

void CHA_ParseFactionFile(const char* buffer)
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
			CHA_ParseFaction(&holdBuf);
		}

	}

	COM_EndParseSession();
}

void CHA_ParseFactionFiles()
{
	//Init Data to all 0
	memset(factionsData, 0, sizeof(characterFaction_t) * MAX_FACTIONS);
	//Read all the externals file
	char	fileList[2048];			//	The list of file names read in
	int		fileNameSize;
	int		fileCount = gi.FS_GetFileList(CHAR_DATA_DIR, ".fac", fileList, sizeof(fileList));

	char* holdChar = fileList;

	Com_Printf("Found %d External files\n", fileCount);
	loadedFactions = 0;
	for (int i = 0; i < fileCount; i++, holdChar += fileNameSize + 1)
	{
		fileNameSize = strlen(holdChar);

		Com_Printf("Parsing %s\n", holdChar);
		char* fileBuffer;
		int fileLen;

		fileLen = gi.FS_ReadFile(va("%s%s", CHAR_DATA_DIR, holdChar), (void**)&fileBuffer);

		if (fileLen == -1)
		{
			Com_Printf("Error reading file\n");
		}
		else
		{
			CHA_ParseFactionFile(fileBuffer);

			gi.FS_FreeFile(fileBuffer);	//let go of the buffer
		}
	}

	qsort(factionsData, loadedFactions, sizeof(characterFaction_t), CHA_SortFactionsByName);
}


#pragma endregion

/*
	Character Loading
	Methods to initialize and load character data from the character list file.
*/
#pragma region Character Load
int variantIndex = 0;

void CHA_ParseForcePowers(const char** holdBuf)
{
	const char* token;
	while (holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);
		if (!Q_stricmp(token, "}")) // End of data for this variant
		{
			break;
		}
		forcePowers_t power = G_ForcePowerForName(token);
		if(power == FP_FIRST)
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: Invalid force power name \"%s\" in character variant \"%s\" for character \"%s\"\n", token, charactersData[loadedCharacters].variantList[variantIndex].code, charactersData[loadedCharacters].code);
			continue;
		};
		int powerValue = 0;
		ParseIntWithLims(holdBuf, &powerValue, 0, NUM_FORCE_POWERS - 1, "force power");
		charactersData[loadedCharacters].variantList[variantIndex].forcePowers[power] = powerValue;
	}
}

void CHA_ParseSkin(const char** holdBuf,characterSkin_s **list,int* count,int* counter)
{
	const char* token;
	//Reallocate Memory if needed
	if ( (*counter) == (*count) )
	{
		int newCount = (*count) + 5;
		characterSkin_s* newList = (characterSkin_s*)realloc(*list, sizeof(characterSkin_s) * newCount);
		if (!newList)
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: failed to reallocate memory for character preset list\n");
			return;
		}
		//Make sure the new memory is cleared
		for (int i = (*count); i < newCount; i++)
		{
			memset(&newList[i], 0, sizeof(characterSkin_s));
		}
		*list = newList;
		*count = newCount;
	}

	if (holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);
		if (Q_stricmp(token, "{"))
		{
			//Show error
			gi.Printf(S_COLOR_RED"Error: Invalid syntax, expected \"{\" and found \"%s\" instead. \n", token);
			return;
		}
	}

	while (holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);
		if (!Q_stricmp(token, "}")) // End of data for this preset
		{
			(*counter)++;
			break;
		}
		if (!Q_stricmp(token, "icon"))
		{
			ParseStr(holdBuf, (*list)[*counter].icon, 64, "skin Icon");
			continue;
		}
		if (!Q_stricmp(token, "skin"))
		{
			ParseStr(holdBuf, (*list)[*counter].skin, 64, "variant skin");
			continue;
		}
	}
}

// Read a list: { "a", "b" ... }. The commas are optional.
static void CHA_ParseList(const char** holdBuf, characterList_t* list, const char* listName)
{
	const char* token = COM_ParseExt(holdBuf, qtrue);
	if (Q_stricmp(token, "{"))
	{
		gi.Printf(S_COLOR_RED"Error: Invalid syntax in %s, expected \"{\" and found \"%s\" instead. \n", listName, token);
		return;
	}
	while (*holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);
		if (!token[0] || !Q_stricmp(token, "}"))
		{
			break;
		}
		char item[64];
		Q_strncpyz(item, token, sizeof(item));
		size_t length = strlen(item);
		while (length && item[length - 1] == ',')
		{
			item[--length] = '\0';
		}
		if (!length)
		{
			continue;
		}
		char (*items)[64] = (char (*)[64])realloc(list->items, sizeof(list->items[0]) * (list->count + 1));
		if (!items)
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: failed to reallocate memory for %s\n", listName);
			return;
		}
		list->items = items;
		Q_strncpyz(list->items[list->count++], item, sizeof(list->items[0]));
	}
}

static void CHA_FreeList(characterList_t* list)
{
	free(list->items);
	list->items = NULL;
	list->count = 0;
}

void CHA_ParseVariant(const char** holdBuf)
{
	const char* token;

	//Reallocate Memory if needed
	if (variantIndex == charactersData[loadedCharacters].variantCount)
	{
		int newCount = charactersData[loadedCharacters].variantCount + 5;
		characterVariant_t* newList = (characterVariant_t*)realloc(charactersData[loadedCharacters].variantList, sizeof(characterVariant_t) * newCount);
		if (!newList)
		{
			gi.Printf(S_COLOR_YELLOW"WARNING: failed to reallocate memory for character variant list\n");
			return;
		}
		//Make sure the new memory is cleared
		for (int i = charactersData[loadedCharacters].variantCount; i < newCount; i++)
		{
			memset(&newList[i], 0, sizeof(characterVariant_t));
		}
		charactersData[loadedCharacters].variantList = newList;
		charactersData[loadedCharacters].variantCount = newCount;
	}

	if (holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);

		if (Q_stricmp(token, "{"))
		{
			//Show error
			gi.Printf(S_COLOR_RED"Error: Invalid syntax, expected \"{\" and found \"%s\" instead. \n",token);
			return;
		}
	}
	int presetIndex = 0;
	int headSkinIndex = 0;
	int torsoSkinIndex = 0;
	int lowerSkinIndex = 0;
	int weaponIndex = 0;

	characterVariant_t* currentVariant = &charactersData[loadedCharacters].variantList[variantIndex];
	while (holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);
		if (!Q_stricmp(token, "}"))	// End of data for this variant
		{
			variantIndex++;
			break;
		}
		if (!Q_stricmp(token, "code"))
		{
			ParseStr(holdBuf, currentVariant->code, 32, "character code");
			continue;
		}
		if (!Q_stricmp(token, "name"))
		{
			ParseStr(holdBuf, currentVariant->name, 64, "variant name");
			continue;
		}
		if (!Q_stricmp(token, "nameKey"))
		{
			ParseStr(holdBuf, currentVariant->nameKey, 64, "variant nameKey");
			continue;
		}
		if (!Q_stricmp(token, "icon"))
		{
			ParseStr(holdBuf, currentVariant->icon, 64, "variant icon");
			continue;
		}
		if (!Q_stricmp(token, "model"))
		{
			ParseStr(holdBuf, currentVariant->model, 64, "variant model");
			continue;
		}
		if (!Q_stricmp(token, "npcName"))
		{
			ParseStr(holdBuf, currentVariant->npcName, 64, "variant npc");
			continue;
		}
		if (!Q_stricmp(token, "descriptionKey"))
		{
			ParseStr(holdBuf, currentVariant->descriptionKey, 64, "variant descriptionKey");
			continue;
		}
		if (!Q_stricmp(token, "preset"))
		{
			CHA_ParseSkin(holdBuf,&currentVariant->presetList, &currentVariant->presetCount,&presetIndex);
			continue;
		}
		if (!Q_stricmp(token, "head"))
		{
			CHA_ParseSkin(holdBuf, &currentVariant->headSkinList, &currentVariant->headSkinCount, &headSkinIndex);
			continue;
		}
		if (!Q_stricmp(token, "torso"))
		{
			CHA_ParseSkin(holdBuf, &currentVariant->torsoSkinList, &currentVariant->torsoSkinCount, &torsoSkinIndex);
			continue;
		}
		if (!Q_stricmp(token, "lower"))
		{
			CHA_ParseSkin(holdBuf, &currentVariant->lowerSkinList, &currentVariant->lowerSkinCount, &lowerSkinIndex);
			continue;
		}
		if (!Q_stricmp(token, "forcePowers"))
		{
			CHA_ParseForcePowers(holdBuf);
			continue;
		}
		if (!Q_stricmp(token, "npcRandom"))
		{
			CHA_ParseList(holdBuf, &currentVariant->npcRandom, "npcRandom");
			continue;
		}
		if (!Q_stricmp(token, "skinRandom"))
		{
			token = COM_ParseExt(holdBuf, qfalse);
			currentVariant->skinRandom = (qboolean)(!Q_stricmp(token, "true") || atoi(token) != 0);
			continue;
		}
		if (!Q_stricmp(token, "saberRandom"))
		{
			CHA_ParseList(holdBuf, &currentVariant->saberRandom, "saberRandom");
			continue;
		}
		if (!Q_stricmp(token, "saberDualRandom"))
		{
			CHA_ParseList(holdBuf, &currentVariant->saberDualRandom, "saberDualRandom");
			continue;
		}
		if (!Q_stricmp(token, "saberStaffRandom"))
		{
			CHA_ParseList(holdBuf, &currentVariant->saberStaffRandom, "saberStaffRandom");
			continue;
		}
		if (!Q_stricmp(token, "saberColorRandom"))
		{
			CHA_ParseList(holdBuf, &currentVariant->saberColorRandom, "saberColorRandom");
			continue;
		}
		if (!Q_stricmp(token, "weapon"))
		{
			if (weaponIndex >= 6) 
			{
				Com_Printf(S_COLOR_YELLOW"WARNING: Too many weapons defined for character variant \"%s\" of character \"%s\". Maximum is 6.\n", currentVariant->code, charactersData[loadedCharacters].code);
				continue;	
			}
			ParseStr(holdBuf, currentVariant->weapons[weaponIndex], 64, "variant weapon");
			weaponIndex++;
			continue;
		}
	}
}

static void CHA_FreeCharacter(characterInfo_t* character)
{
	for (int v = 0; v < character->variantCount && character->variantList; v++)
	{
		characterVariant_t* variant = &character->variantList[v];
		free(variant->presetList);
		free(variant->headSkinList);
		free(variant->torsoSkinList);
		free(variant->lowerSkinList);
		CHA_FreeList(&variant->npcRandom);
		CHA_FreeList(&variant->saberRandom);
		CHA_FreeList(&variant->saberDualRandom);
		CHA_FreeList(&variant->saberStaffRandom);
		CHA_FreeList(&variant->saberColorRandom);
	}
	free(character->variantList);
}

void CHA_ParseCharacter(const char** holdBuf)
{
	const char* token;
	variantIndex = 0;

	while (holdBuf)
	{
		token = COM_ParseExt(holdBuf, qtrue);

		if (!Q_stricmp(token, "}"))	// End of data for this character
		{
			// The lists stop at the first character without a code: a bad block must not hide the next ones.
			characterInfo_t* character = &charactersData[loadedCharacters];
			if (!character->code[0])
			{
				gi.Printf(S_COLOR_YELLOW"WARNING: character without a code in a .cha file, ignored (\"%s\")\n", character->name);
				CHA_FreeCharacter(character);
				memset(character, 0, sizeof(*character));
				break;
			}
			loadedCharacters++;
			break;
		}
		if (!Q_stricmp(token, "code"))
		{
			ParseStr(holdBuf, charactersData[loadedCharacters].code, 32, "character code");
			continue;
		}
		if (!Q_stricmp(token, "name"))
		{
			ParseStr(holdBuf, charactersData[loadedCharacters].name, 64, "character name");
			continue;
		}
		if (!Q_stricmp(token, "nameKey"))
		{
			ParseStr(holdBuf, charactersData[loadedCharacters].nameKey, 64, "character nameKey");
			continue;
		}
		if (!Q_stricmp(token, "icon"))
		{
			ParseStr(holdBuf, charactersData[loadedCharacters].icon, 64, "character icon");
			continue;
		}
		if (!Q_stricmp(token, "factions"))
		{
			ParseStr(holdBuf, charactersData[loadedCharacters].factions, 256, "character factions Codes");
			continue;
		}
		if (!Q_stricmp(token, "tags"))
		{
			ParseStr(holdBuf, charactersData[loadedCharacters].tags, 256, "character factions Codes");
			continue;
		}
		if (!Q_stricmp(token, "Variant"))
		{
			CHA_ParseVariant(holdBuf);
			continue;
		}
		SkipRestOfLine(holdBuf);
	}
}

void CHA_ParseCharacterFile(const char* buffer)
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
			if (loadedCharacters >= MAX_CHARACTERS)
			{
				gi.Printf(S_COLOR_RED"Error: more than %d characters, the next ones are ignored. Increase MAX_CHARACTERS.\n", MAX_CHARACTERS);
				break;
			}
			CHA_ParseCharacter(&holdBuf);
		}

	}

	COM_EndParseSession();
}


static void CHA_FreeCharacters()
{
	for (int i = 0; i < MAX_CHARACTERS; i++)
	{
		CHA_FreeCharacter(&charactersData[i]);
	}
}

static int CHA_SortTagsByCode(const void* a, const void* b)
{
	return Q_stricmp(((const characterTag_t*)a)->code, ((const characterTag_t*)b)->code);
}

// Collect the distinct tags of all the characters, sorted by raw code. "none" is not a tag.
static void CHA_BuildTagList()
{
	memset(tagsData, 0, sizeof(tagsData));
	loadedTags = 0;
	for (int c = 0; c < loadedCharacters; c++)
	{
		const char *list = charactersData[c].tags;
		while (*list)
		{
			while (*list == ',' || *list == ' ')
			{
				list++;
			}
			const char *end = list;
			while (*end && *end != ',')
			{
				end++;
			}
			size_t length = end - list;
			while (length && list[length - 1] == ' ')
			{
				length--;
			}
			if (length && length < sizeof(tagsData[0].code) && !(length == 4 && !Q_stricmpn(list, "none", 4)))
			{
				char code[sizeof(tagsData[0].code)];
				Q_strncpyz(code, list, (int)length + 1);
				qboolean known = qfalse;
				for (int t = 0; t < loadedTags && !known; t++)
				{
					known = (qboolean)!Q_stricmp(tagsData[t].code, code);
				}
				if (!known && loadedTags < MAX_TAGS)
				{
					Q_strncpyz(tagsData[loadedTags++].code, code, sizeof(tagsData[0].code));
				}
			}
			list = end;
		}
	}
	qsort(tagsData, loadedTags, sizeof(characterTag_t), CHA_SortTagsByCode);
}

void CHA_ParseCharacterFiles()
{
	CHA_FreeCharacters();
	//Init Data to all 0
	memset(charactersData, 0, sizeof(characterInfo_t) * MAX_CHARACTERS);
	//Read all the externals file
	static char	fileList[65536];		//	The list of file names read in
	int		fileNameSize;
	int		fileCount = gi.FS_GetFileList(CHAR_DATA_DIR, ".cha", fileList, sizeof(fileList));

	
	char* holdChar = fileList;

	Com_Printf("Found %d External files\n", fileCount);
	loadedCharacters = 0;
	for (int i = 0; i < fileCount; i++, holdChar += fileNameSize + 1)
	{
		fileNameSize = strlen(holdChar);

		Com_Printf("Parsing %s\n", holdChar);
		char* fileBuffer;
		int fileLen;

		fileLen = gi.FS_ReadFile(va("%s%s", CHAR_DATA_DIR, holdChar), (void**)&fileBuffer);

		if (fileLen == -1)
		{
			Com_Printf("Error reading file\n");
		}
		else
		{
			CHA_ParseCharacterFile(fileBuffer);

			gi.FS_FreeFile(fileBuffer);	//let go of the buffer
		}
	}

	qsort(charactersData, loadedCharacters, sizeof(characterInfo_t), CHA_SortCharactersByName);
	CHA_BuildTagList();
}
#pragma endregion
