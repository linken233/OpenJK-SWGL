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

#include "g_characters.h"

characterFaction_t factionsData[MAX_FACTIONS];
int loadedFactions;
characterInfo_t charactersData[MAX_CHARACTERS];
int loadedCharacters;
characterTag_t tagsData[MAX_TAGS];
int loadedTags;

qboolean CHA_ListHasItem(const char *list, const char *item)
{
	const size_t itemLength = strlen(item);

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
		if (length == itemLength && length && !Q_stricmpn(list, item, (int)length))
		{
			return qtrue;
		}
		list = end;
	}
	return qfalse;
}

const characterVariant_t *CHA_FindVariant(const char *charKey, const char *variantCode)
{
	for (int c = 0; c < loadedCharacters; c++)
	{
		const characterInfo_t *character = &charactersData[c];
		if (Q_stricmp(character->code, charKey))
		{
			continue;
		}
		for (int v = 0; v < character->variantCount && character->variantList[v].code[0]; v++)
		{
			if (!Q_stricmp(character->variantList[v].code, variantCode))
			{
				return &character->variantList[v];
			}
		}
	}
	return NULL;
}

qboolean CHA_VariantIsRandom(const characterVariant_t *variant)
{
	return (qboolean)(variant && (variant->npcRandom.count || variant->skinRandom
		|| variant->saberRandom.count || variant->saberStaffRandom.count || variant->saberColorRandom.count));
}