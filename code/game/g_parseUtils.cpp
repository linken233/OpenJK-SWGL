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



#include "g_parseUtils.h"
#include "g_local.h"

/*
	Parsing Functions
	Methods to assist in parsing the character, weapons and faction list files.
*/
#pragma region Parsing Functions

/*
----------------------------------------------------------
	Parsing Functions
----------------------------------------------------------
*/
void ParseInt(const char** holdBuf, int* dest)
{
	int		tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	*dest = tokenInt;
}

void ParseIntWithLims(const char** holdBuf, int* dest, int min, int max, char* identifier)
{
	int		tokenInt;

	if (COM_ParseInt(holdBuf, &tokenInt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	if ((tokenInt < min) || (tokenInt > max))
	{
		gi.Printf(S_COLOR_YELLOW"WARNING: bad %s in external weapon data '%d'\n", identifier, tokenInt);
		return;
	}

	*dest = tokenInt;
}

void ParseFlt(const char** holdBuf, float* dest)
{
	float	tokenFlt;

	if (COM_ParseFloat(holdBuf, &tokenFlt))
	{
		SkipRestOfLine(holdBuf);
		return;
	}

	*dest = tokenFlt;
}

//--------------------------------------------
void ParseStr(const char** holdBuf, char* dest, int maxLen, char* identifier)
{
	int len;
	const char* tokenStr;

	if (COM_ParseString(holdBuf, &tokenStr))
	{
		return;
	}

	len = strlen(tokenStr);
	len++;
	if (len > maxLen)
	{
		len = maxLen;
		gi.Printf(S_COLOR_YELLOW"WARNING: %s too long in external WEAPONS.DAT '%s'\n", identifier, tokenStr);
	}

	Q_strncpyz(dest, tokenStr, len);
}

#pragma endregion

#pragma region Force Powers
typedef struct forcePowerLookup_s {
	const char* name;
	forcePowers_t power;
} forcePowerLookup_t;

const forcePowerLookup_t forcePowerTable[] = {
	// Pouvoirs de base / JK2
	{ "FP_HEAL", FP_HEAL },
	{ "FP_LEVITATION", FP_LEVITATION },
	{ "FP_SPEED", FP_SPEED },
	{ "FP_PUSH", FP_PUSH },
	{ "FP_PULL", FP_PULL },
	{ "FP_TELEPATHY", FP_TELEPATHY },
	{ "FP_GRIP", FP_GRIP },
	{ "FP_LIGHTNING", FP_LIGHTNING },
	{ "FP_SABERTHROW", FP_SABERTHROW },
	{ "FP_SABER_DEFENSE", FP_SABER_DEFENSE },
#ifndef JK2_MODE
	{ "FP_SABER_OFFENSE", FP_SABER_OFFENSE },

	{ "FP_RAGE", FP_RAGE },
	{ "FP_PROTECT", FP_PROTECT },
	{ "FP_ABSORB", FP_ABSORB },
	{ "FP_DRAIN", FP_DRAIN },
	{ "FP_SEE", FP_SEE },

	// SWGL Powers (Light)
	{ "FP_STASIS", FP_STASIS },
	{ "FP_BLAST", FP_BLAST },
	{ "FP_GRASP", FP_GRASP },

	// SWGL Powers (Dark)
	{ "FP_DESTRUCTION", FP_DESTRUCTION },
	{ "FP_LIGHTNING_STRIKE", FP_LIGHTNING_STRIKE },
	{ "FP_FEAR", FP_FEAR },
#endif
	{ NULL, (forcePowers_t)0 } // Marqueur de fin de tableau
}; 

forcePowers_t G_ForcePowerForName(const char* name)
{
	for (int i = 0; forcePowerTable[i].name != NULL; i++)
	{
		if (!Q_stricmp(name, forcePowerTable[i].name))
		{
			return forcePowerTable[i].power;
		}
	}
	return FP_FIRST; // Return a default value if not found
}

#pragma endregion