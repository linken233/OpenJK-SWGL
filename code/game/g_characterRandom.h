/*
===========================================================================
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

#ifndef __G_CHARACTER_RANDOM_H__
#define __G_CHARACTER_RANDOM_H__

#include "../qcommon/q_shared.h"

struct gentity_s;

// Model data of an entry of the .npc files.
typedef struct npcModelInfo_s {
	char model[MAX_QPATH];
	char skin[MAX_QPATH];
	char saber[MAX_QPATH];
	char saberColor[MAX_QPATH];
} npcModelInfo_t;

// Read the model data of the NPC <npcName>. Return qfalse if the NPC does not exist.
qboolean NPC_GetModelInfo(const char *npcName, npcModelInfo_t *info);
// Write "head|torso|lower" with random skins of the model. Return qfalse if the model has no part skins.
qboolean NPC_RandomSkinParts(const char *model, char *skin, int skinSize);

// spawnNPC: apply the random parts of the character variant of the NPC menu to the spawner.
void CHA_RandomizeSpawner(struct gentity_s *spawner);
// characterRandom: apply the random parts of the character variant to the player.
void CHA_RandomizePlayer(void);

#endif //__G_CHARACTER_RANDOM_H__
