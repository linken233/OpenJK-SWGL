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

#ifndef __CG_CHARACTERS_H__
#define __CG_CHARACTERS_H__

#include "../game/g_characters.h"
#include "../qcommon/q_shared.h"


extern characterInfo_t selectedCharacters[MAX_CHARACTERS];

void CG_DrawCharactersMenu();

void CG_Characters_CharacterClick_f();
void CG_Characters_VariantClick_f();
void CG_Characters_FactionClick_f();
void CG_Characters_FactionPreviousPage_f();
void CG_Characters_FactionNextPage_f();
void CG_Characters_VariantPreviousPage_f();
void CG_Characters_VariantNextPage_f();
void CG_Characters_PreviousPage_f();
void CG_Characters_NextPage_f();
void CG_Characters_SearchChanged_f();
void CG_Characters_TagPreviousPage_f();
void CG_Characters_TagNextPage_f();
void CG_Characters_ResetSearch_f();
void CG_Characters_ToggleTags_f();
void CG_Characters_ConfigsRefresh_f();
void CG_Characters_Back_f();
void CG_Characters_Portraits_f();
void CG_Characters_Portrait_f();
void CG_DrawCharacterPortrait();

#endif //__CG_CHARACTERS_H__
