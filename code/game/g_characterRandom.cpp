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

/*
Random parts of a character variant (.cha):
	npcRandom			{ "npc1" "npc2" ... }	one NPC file is picked: model, skin and saber of this NPC
	skinRandom			true					random head, torso and lower skins of the model
	saberRandom			{ ... }					single saber
	saberDualRandom		{ ... }					left hand saber of a single saber (1 chance in 5)
	saberStaffRandom	{ ... }					staff saber (1 chance in 6)
	saberColorRandom	{ ... }					color of each blade
A saber selected in the menu has priority over the random sabers.
Each spawn and each Apply picks again.
*/

#include "g_local.h"
#include "g_characters.h"
#include "g_characterRandom.h"

extern cvar_t *g_char_model;
extern cvar_t *g_char_skin_head;
extern cvar_t *g_char_skin_torso;
extern cvar_t *g_char_skin_legs;
extern cvar_t *g_saber;
extern cvar_t *g_NPCsaber;
extern void G_ChangePlayerModel(gentity_t *ent, const char *newModel);

static const char *CHA_PickItem(const characterList_t *list)
{
	return list->count ? list->items[Q_irand(0, list->count - 1)] : NULL;
}

// An NPC of the list. The NPCs with the model of the player come last.
static const char *CHA_PickNPC(const characterVariant_t *variant, qboolean excludePlayerModel)
{
	const characterList_t *list = &variant->npcRandom;
	if (!excludePlayerModel || list->count < 2)
	{
		return CHA_PickItem(list);
	}
	const int first = Q_irand(0, list->count - 1);
	for (int i = 0; i < list->count; i++)
	{
		const char *name = list->items[(first + i) % list->count];
		npcModelInfo_t info;
		if (!NPC_GetModelInfo(name, &info) || Q_stricmp(info.model, g_char_model->string))
		{
			return name;
		}
	}
	return list->items[first];
}

// Staff (1 chance in 6), else single saber with a left hand saber (1 chance in 5).
static void CHA_PickSabers(const characterVariant_t *variant, const char **saberOne, const char **saberTwo)
{
	*saberOne = NULL;
	*saberTwo = NULL;
	if (variant->saberStaffRandom.count && (!variant->saberRandom.count || !Q_irand(0, 5)))
	{
		*saberOne = CHA_PickItem(&variant->saberStaffRandom);
		return;
	}
	if (variant->saberRandom.count)
	{
		*saberOne = CHA_PickItem(&variant->saberRandom);
		if (variant->saberDualRandom.count && !Q_irand(0, 4))
		{
			*saberTwo = CHA_PickItem(&variant->saberDualRandom);
		}
	}
}

static const characterVariant_t *CHA_VariantFromCvars(const char *charKeyCvar, const char *variantCvar)
{
	char charKey[64];
	char variantCode[64];
	gi.Cvar_VariableStringBuffer(charKeyCvar, charKey, sizeof(charKey));
	gi.Cvar_VariableStringBuffer(variantCvar, variantCode, sizeof(variantCode));
	const characterVariant_t *variant = CHA_FindVariant(charKey, variantCode);
	return CHA_VariantIsRandom(variant) ? variant : NULL;
}

void CHA_RandomizeSpawner(gentity_t *spawner)
{
	// The UI sets g_NPCcharKey and g_NPCvariant with the other g_NPC cvars (updatenpccvars).
	const characterVariant_t *variant = CHA_VariantFromCvars("g_NPCcharKey", "g_NPCvariant");
	if (!variant)
	{
		return;
	}

	if (variant->npcRandom.count)
	{
		// Model, skin and saber come from the NPC file.
		spawner->NPC_type = G_NewString(CHA_PickNPC(variant, qtrue));
		spawner->NPC_model = NULL;
		spawner->NPC_skin = NULL;
		spawner->NPC_SaberOne = NULL;
		spawner->NPC_SaberTwo = NULL;
		spawner->NPC_SaberOneColor = NULL;
		spawner->NPC_SaberTwoColor = NULL;
	}
	if (variant->skinRandom)
	{
		spawner->NPC_skin = "random";
	}

	if (!g_NPCsaber->string[0])
	{
		const char *saberOne, *saberTwo;
		CHA_PickSabers(variant, &saberOne, &saberTwo);
		if (saberOne)
		{
			spawner->NPC_SaberOne = G_NewString(saberOne);
			spawner->NPC_SaberTwo = saberTwo ? G_NewString(saberTwo) : NULL;
		}
	}
	if (variant->saberColorRandom.count)
	{
		spawner->NPC_SaberOneColor = G_NewString(CHA_PickItem(&variant->saberColorRandom));
		spawner->NPC_SaberTwoColor = G_NewString(CHA_PickItem(&variant->saberColorRandom));
	}
}

// Set the three skin cvars of the player from "head|torso|lower" or from one skin name.
static void CHA_SetPlayerSkin(const char *skin)
{
	char parts[3][MAX_QPATH];
	if (strchr(skin, '|'))
	{
		const char *p = skin;
		for (int i = 0; i < 3; i++)
		{
			const char *end = strchr(p, '|');
			const int length = end ? (int)(end - p) : (int)strlen(p);
			Q_strncpyz(parts[i], p, Q_min(length + 1, (int)sizeof(parts[i])));
			p = end ? end + 1 : p + length;
		}
	}
	else
	{
		const char *name = (!skin[0] || !Q_stricmp(skin, "default")) ? "model_default" : va("model_%s", skin);
		for (int i = 0; i < 3; i++)
		{
			Q_strncpyz(parts[i], name, sizeof(parts[i]));
		}
	}
	gi.cvar_set("g_char_skin_head", parts[0]);
	gi.cvar_set("g_char_skin_torso", parts[1]);
	gi.cvar_set("g_char_skin_legs", parts[2]);
}

void CHA_RandomizePlayer(void)
{
	if (!player || !player->client)
	{
		return;
	}
	const characterVariant_t *variant = CHA_VariantFromCvars("g_charKey", "ui_variant_code");
	if (!variant)
	{
		return;
	}

	// The cvars keep the result for the next maps and the saved games.
	const qboolean menuSaber = (qboolean)(g_saber->string[0] != '\0');
	npcModelInfo_t info;
	memset(&info, 0, sizeof(info));
	if (variant->npcRandom.count)
	{
		const char *name = CHA_PickNPC(variant, qfalse);
		if (NPC_GetModelInfo(name, &info))
		{
			gi.cvar_set("g_char_model", info.model);
			if (Q_stricmp(info.skin, "random"))
			{
				CHA_SetPlayerSkin(info.skin);
			}
		}
		else
		{
			gi.Printf(S_COLOR_YELLOW "characterRandom: cannot find NPC %s\n", name);
		}
	}
	if (variant->skinRandom || !Q_stricmp(info.skin, "random"))
	{
		char skin[MAX_QPATH * 3];
		if (NPC_RandomSkinParts(g_char_model->string, skin, sizeof(skin)))
		{
			CHA_SetPlayerSkin(skin);
		}
	}

	if (!menuSaber)
	{
		const char *saberOne, *saberTwo;
		CHA_PickSabers(variant, &saberOne, &saberTwo);
		if (!saberOne && info.saber[0])
		{
			saberOne = info.saber;
		}
		if (saberOne)
		{
			gi.cvar_set("g_saber", saberOne);
			gi.cvar_set("g_saber2", saberTwo ? saberTwo : "");
		}
		if (info.saberColor[0])
		{
			gi.cvar_set("g_saber_color", info.saberColor);
			gi.cvar_set("g_saber2_color", info.saberColor);
		}
	}
	if (variant->saberColorRandom.count)
	{
		gi.cvar_set("g_saber_color", CHA_PickItem(&variant->saberColorRandom));
		gi.cvar_set("g_saber2_color", CHA_PickItem(&variant->saberColorRandom));
	}

	G_ChangePlayerModel(player, "player");
}
