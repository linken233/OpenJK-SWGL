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

#include "bg_public.h"
#include "b_local.h"

//custom anims:

extern qboolean PM_RunningAnim(int anim);

#define	DROIDEKA_SHIELD_SIZE	50
#define TURN_ON					0x00000000
#define TURN_OFF				0x00000100



////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool DekaShield_IsOn()
{
	return (NPC->flags&FL_SHIELDED);
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void DekaShield_TurnOn()
{
	if (!DekaShield_IsOn())
	{
		NPC->flags |= FL_SHIELDED;
		NPC->client->ps.powerups[PW_GALAK_SHIELD] = Q3_INFINITE;
		//gi.G2API_SetSurfaceOnOff(&NPC->ghoul2[NPC->playerModel], "shield", TURN_ON);
		gi.G2API_SetSurfaceOnOff(&NPC->ghoul2[NPC->playerModel], "force_shield", TURN_ON);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void DekaShield_TurnOff()
{
	if ( DekaShield_IsOn())
	{
		NPC->flags &= ~FL_SHIELDED;
		NPC->client->ps.powerups[PW_GALAK_SHIELD] = 0;
		//gi.G2API_SetSurfaceOnOff(&NPC->ghoul2[NPC->playerModel], "shield", TURN_OFF);
		gi.G2API_SetSurfaceOnOff(&NPC->ghoul2[NPC->playerModel], "force_shield", TURN_OFF);
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// Push A Particular Ent
////////////////////////////////////////////////////////////////////////////////////////
void DekaShield_PushEnt(gentity_t* pushed, vec3_t smackDir)
{
	G_Damage(pushed, NPC, NPC, smackDir, NPC->currentOrigin, (g_spskill->integer+1)*Q_irand( 5, 10), DAMAGE_NO_KNOCKBACK, MOD_ELECTROCUTE);
	G_Throw(pushed, smackDir, 10);

	// Make Em Electric
	//------------------
 	pushed->s.powerups |= (1 << PW_SHOCKED);
	if (pushed->client)
	{
		pushed->client->ps.powerups[PW_SHOCKED] = level.time + 1000;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// Go Through All The Ents Within The Radius Of The Shield And Push Them
////////////////////////////////////////////////////////////////////////////////////////
void DekaShield_PushRadiusEnts()
{
	int			numEnts;
	gentity_t*	radiusEnts[128];
	const float	radius = DROIDEKA_SHIELD_SIZE;
	vec3_t		mins, maxs;
	vec3_t		smackDir;
	float		smackDist;

	for (int i = 0; i < 3; i++ )
	{
		mins[i] = NPC->currentOrigin[i] - radius;
		maxs[i] = NPC->currentOrigin[i] + radius;
	}

	numEnts = gi.EntitiesInBox(mins, maxs, radiusEnts, 128);
	for (int entIndex=0; entIndex<numEnts; entIndex++)
	{
		// Only Clients
		//--------------
		if (!radiusEnts[entIndex] || !radiusEnts[entIndex]->client)
		{
			continue;
		}

		// Don't Push Away Other Droidekas
		//---------------------------------------
		if (radiusEnts[entIndex]->client->NPC_class==NPC->client->NPC_class)
		{
			continue;
		}

		// Should Have Already Pushed The Enemy If He Touched Us
		//-------------------------------------------------------
		if (NPC->enemy &&  NPCInfo->touchedByPlayer==NPC->enemy && radiusEnts[entIndex]==NPC->enemy)
		{
			continue;
		}

		// Do The Vector Distance Test
		//-----------------------------
		VectorSubtract(radiusEnts[entIndex]->currentOrigin, NPC->currentOrigin, smackDir);
		smackDist = VectorNormalize(smackDir);
		if (smackDist<radius)
		{
			DekaShield_PushEnt(radiusEnts[entIndex], smackDir);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
void DekaShield_Update()
{
	// Shields Go When You Die
	//-------------------------
	if (NPC->health<=0)
	{
		if (DekaShield_IsOn())
		{
			DekaShield_TurnOff();
		}
		return;
	}


	// Recharge Shields
	//------------------
 	NPC->client->ps.stats[STAT_ARMOR] += 1;
	if (NPC->client->ps.stats[STAT_ARMOR]>250)
	{
		NPC->client->ps.stats[STAT_ARMOR] = 250;
	}

	// If We Have Enough, Turn The Shield On
	//----------------------------------------------------------------------------
 	if (NPC->client->ps.stats[STAT_ARMOR]>100)
	{
		// Droidekas turn off their shields when they're rolling
		if (!PM_RunningAnim(NPC->client->ps.legsAnim))
			DekaShield_TurnOn();
		else
			DekaShield_TurnOff();

		if (DekaShield_IsOn())
		{
			// Update Our Shader Value
			//-------------------------
	 	 	NPC->client->renderInfo.customRGBA[0] =
			NPC->client->renderInfo.customRGBA[1] =
			NPC->client->renderInfo.customRGBA[2] =
  			NPC->client->renderInfo.customRGBA[3] = (NPC->client->ps.stats[STAT_ARMOR] - 100);


			// If Touched By An Enemy, ALWAYS Shove Them
			//-------------------------------------------
			if (NPC->enemy &&  NPCInfo->touchedByPlayer==NPC->enemy)
			{
				vec3_t dir;
				VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, dir);
				VectorNormalize(dir);
				DekaShield_PushEnt(NPC->enemy, dir);
			}

			if (NPC->enemy)
			{
				NPC->client->leader = NPC->enemy;
			}

			// Push Anybody Else Near
			//------------------------
			DekaShield_PushRadiusEnts();
		}
	}

	// Shields Gone
	//--------------
	else
	{
		DekaShield_TurnOff();

	}
}