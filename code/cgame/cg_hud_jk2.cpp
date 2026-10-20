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

// cg_hud_jk2.cpp -- the Jedi Outcast HUD (cg_hudFiles 2 or ui/jk2hud.txt)
// Layout, tic positions and prong behaviour adapted from the JK2 HUD of jaPRO (GPLv2).

#include "cg_headers.h"

#include "cg_media.h"

#define SimpleHud_DrawString( x, y, str, color ) cgi_R_Font_DrawString( x, y, str, color, (int)0x80000000 | cgs.media.qhFontSmall, -1, 1.0f, cgs.widthRatioCoef )

/*
================
JK2 HUD

The round gauges of Jedi Outcast, drawn from the JKO assets: health wedge and armor
rings on the left, ammo or saber style and force tics on the right, and the prongs and
bar behind the weapon, force and inventory selection. Layout from the JA+ JK2 HUD.
================
*/
typedef struct jk2HudTic_s
{
	float	x, y, w, h;	// inside the 80x80 gauge; a negative width mirrors the tic
	int		image;		// index in the jk2HudAmmoTic / jk2HudForceTic handles
} jk2HudTic_t;

#define JK2_HUD_NUM_TICS	14

static const jk2HudTic_t jk2ForceTics[JK2_HUD_NUM_TICS] =
{
	{ 11, 41,  20, 10, 0 }, { 12, 45,  20, 10, 1 }, { 14, 49,  20, 10, 2 }, { 17, 52,  20, 10, 3 },
	{ 22, 55,  10, 10, 4 }, { 28, 57,  10, 20, 5 }, { 34, 59,  10, 10, 6 },
	{ 46, 59, -10, 10, 6 }, { 52, 57, -10, 20, 5 }, { 58, 55, -10, 10, 4 }, { 63, 52, -20, 10, 3 },
	{ 66, 49, -20, 10, 2 }, { 68, 45, -20, 10, 1 }, { 69, 41, -20, 10, 0 },
};

static const jk2HudTic_t jk2AmmoTics[JK2_HUD_NUM_TICS] =
{
	{ 12, 34,  10, 10, 6 }, { 13, 28,  10, 10, 5 }, { 15, 23,  10, 10, 4 }, { 19, 19,  10, 10, 3 },
	{ 23, 15,  10, 10, 2 }, { 29, 12,  10, 10, 1 }, { 34, 11,  10, 10, 0 },
	{ 47, 11, -10, 10, 0 }, { 52, 12, -10, 10, 1 }, { 58, 15, -10, 10, 2 }, { 62, 19, -10, 10, 3 },
	{ 66, 23, -10, 10, 4 }, { 68, 28, -10, 10, 5 }, { 69, 34, -10, 10, 6 },
};

/*
================
CG_JK2HudRequested

cg_hudFiles 2 or the ui/jk2hud.txt entry of the options menu
================
*/
qboolean CG_JK2HudRequested( void )
{
	return ( cg_hudFiles.integer == 2 || !Q_stricmp( cg_hudFiles.string, "ui/jk2hud.txt" ) ) ? qtrue : qfalse;
}

/*
================
CG_JK2HudActive

The JK2 HUD only works with the JKO assets (g_validJKO) and the jk2hud pk3
================
*/
qboolean CG_JK2HudActive( void )
{
	return ( CG_JK2HudRequested() && cg_validJKO.integer && cgs.media.jk2HudLoaded ) ? qtrue : qfalse;
}

/*
================
CG_WarnJK2HudMissing

The JK2 HUD is chosen but its assets are missing: the default HUD is drawn.
cg_hudFiles is not changed, so the choice is kept for when the assets are there.
================
*/
static void CG_WarnJK2HudMissing( void )
{
	if ( CG_JK2HudRequested() && !CG_JK2HudActive() )
	{
		CG_Printf( S_COLOR_YELLOW "JK2 HUD needs the JKO assets, using default\n" );
	}
}

/*
================
CG_RegisterJK2Hud
================
*/
void CG_RegisterJK2Hud( void )
{
	int i;

	cgs.media.jk2HudLoaded = qfalse;
	if ( !cg_validJKO.integer )
	{
		CG_WarnJK2HudMissing();
		return;
	}

	cgs.media.jk2HudLeftFrame	= cgi_R_RegisterShaderNoMip( "jk2hud/leftframe" );
	cgs.media.jk2HudLeftInner	= cgi_R_RegisterShaderNoMip( "jk2hud/leftinner" );
	cgs.media.jk2HudRightFrame	= cgi_R_RegisterShaderNoMip( "jk2hud/rightframe" );
	cgs.media.jk2HudRightInner	= cgi_R_RegisterShaderNoMip( "jk2hud/rightinner" );
	cgs.media.jk2HudHealth		= cgi_R_RegisterShaderNoMip( "gfx/hud/health" );
	cgs.media.jk2HudHealthTic	= cgi_R_RegisterShaderNoMip( "gfx/hud/health_tic" );
	cgs.media.jk2HudArmor1		= cgi_R_RegisterShaderNoMip( "gfx/hud/armor1" );
	cgs.media.jk2HudArmor2		= cgi_R_RegisterShaderNoMip( "gfx/hud/armor2" );
	cgs.media.jk2HudArmorTic	= cgi_R_RegisterShaderNoMip( "gfx/hud/armor_tic" );
	cgs.media.jk2HudSaberStyle[0] = cgi_R_RegisterShaderNoMip( "jk2hud/saber_fast" );
	cgs.media.jk2HudSaberStyle[1] = cgi_R_RegisterShaderNoMip( "jk2hud/saber_med" );
	cgs.media.jk2HudSaberStyle[2] = cgi_R_RegisterShaderNoMip( "jk2hud/saber_strong" );
	cgs.media.jk2HudSaberStyle[3] = cgi_R_RegisterShaderNoMip( "jk2hud/saber_desann" );
	cgs.media.jk2HudSaberStyle[4] = cgi_R_RegisterShaderNoMip( "jk2hud/saber_tavion" );
	for ( i = 0; i < MAX_JK2_HUD_TICS; i++ )
	{
		cgs.media.jk2HudAmmoTic[i]	= cgi_R_RegisterShaderNoMip( va( "gfx/hud/ammo_tick%d", i + 1 ) );
		cgs.media.jk2HudForceTic[i]	= cgi_R_RegisterShaderNoMip( va( "gfx/hud/force_tick%d", i + 1 ) );
	}
	cgs.media.jk2ProngOff			= cgi_R_RegisterShaderNoMip( "jk2hud/prong_off" );
	cgs.media.jk2ProngOnWeapon		= cgi_R_RegisterShaderNoMip( "jk2hud/prong_on_w" );
	cgs.media.jk2ProngOnForce		= cgi_R_RegisterShaderNoMip( "jk2hud/prong_on_f" );
	cgs.media.jk2ProngOnInventory	= cgi_R_RegisterShaderNoMip( "jk2hud/prong_on_i" );
	cgs.media.jk2BackgroundWeapon	= cgi_R_RegisterShaderNoMip( "jk2hud/background" );
	cgs.media.jk2BackgroundForce	= cgi_R_RegisterShaderNoMip( "jk2hud/background_f" );
	cgs.media.jk2BackgroundInventory = cgi_R_RegisterShaderNoMip( "jk2hud/background_i" );

	// A missing shader returns the default one (0): the jk2hud pk3 is not installed
	cgs.media.jk2HudLoaded = ( cgs.media.jk2HudLeftFrame && cgs.media.jk2HudRightFrame
		&& cgs.media.jk2HudLeftInner && cgs.media.jk2HudRightInner
		&& cgs.media.jk2ProngOff && cgs.media.jk2BackgroundWeapon ) ? qtrue : qfalse;
	CG_WarnJK2HudMissing();
}

// Left of the gauge is anchored to the left of the screen, right of the gauge to the right
static float CG_JK2HudRightX( const float x, const float hudRatio )
{
	return SCREEN_WIDTH - ( SCREEN_WIDTH - x ) * hudRatio;
}

static void CG_DrawJK2Health( const float x, const float y, const float hudRatio )
{
	vec4_t			calcColor;
	playerState_t	*ps = &cg.snap->ps;
	int				health = ps->stats[STAT_HEALTH];
	float			healthPercent, armorPercent;

	if ( health > ps->stats[STAT_MAX_HEALTH] )
	{
		health = ps->stats[STAT_MAX_HEALTH];
	}

	healthPercent = (float)health / ps->stats[STAT_MAX_HEALTH];
	if ( healthPercent < 0.0f )
	{
		healthPercent = 0.0f;
	}
	memcpy( calcColor, colorTable[CT_HUD_RED], sizeof( vec4_t ) );
	calcColor[0] *= healthPercent;
	calcColor[1] *= healthPercent;
	calcColor[2] *= healthPercent;
	cgi_R_SetColor( calcColor );
	CG_DrawPic( x, y, 80 * hudRatio, 80, cgs.media.jk2HudHealth );

	// The tic flashes under 20% health, in step with the armor tic
	armorPercent = (float)( ps->stats[STAT_ARMOR] - ps->stats[STAT_MAX_HEALTH] / 2 ) / ( ps->stats[STAT_MAX_HEALTH] / 2 );
	if ( healthPercent > 0.20f )
	{
		cg.HUDHealthFlag = qtrue;
	}
	else if ( cg.HUDTickFlashTime < cg.time )
	{
		cg.HUDTickFlashTime = cg.time + 100;
		if ( armorPercent > 0 && armorPercent < 0.5f )
		{
			cg.HUDHealthFlag = cg.HUDArmorFlag;
		}
		else
		{
			cg.HUDHealthFlag = cg.HUDHealthFlag ? qfalse : qtrue;
		}
	}

	if ( cg.HUDHealthFlag )
	{
		cgi_R_SetColor( colorTable[CT_HUD_RED] );
		CG_DrawPic( x, y, 80 * hudRatio, 80, cgs.media.jk2HudHealthTic );
	}

	cgi_R_SetColor( colorTable[CT_HUD_RED] );
	CG_DrawNumField( x + 16 * hudRatio, y + 40, 3, ps->stats[STAT_HEALTH], 6 * hudRatio, 12, NUM_FONT_SMALL, qfalse );
}

static void CG_DrawJK2Armor( const float x, const float y, const float hudRatio )
{
	vec4_t			calcColor;
	playerState_t	*ps = &cg.snap->ps;
	int				armor = ps->stats[STAT_ARMOR];
	float			armorPercent;

	if ( armor > ps->stats[STAT_MAX_HEALTH] )
	{
		armor = ps->stats[STAT_MAX_HEALTH];
	}

	// Outer ring: second half of the armor
	armorPercent = (float)( armor - ps->stats[STAT_MAX_HEALTH] / 2 ) / ( ps->stats[STAT_MAX_HEALTH] / 2 );
	if ( armorPercent < 0.0f )
	{
		armorPercent = 0.0f;
	}
	memcpy( calcColor, colorTable[CT_HUD_GREEN], sizeof( vec4_t ) );
	calcColor[0] *= armorPercent;
	calcColor[1] *= armorPercent;
	calcColor[2] *= armorPercent;
	cgi_R_SetColor( calcColor );
	CG_DrawPic( x, y, 80 * hudRatio, 80, cgs.media.jk2HudArmor1 );

	// Inner ring: first half, full as soon as the outer ring starts to fill
	if ( armorPercent > 0.0f )
	{
		armorPercent = 1.0f;
	}
	else
	{
		armorPercent = (float)armor / ( ps->stats[STAT_MAX_HEALTH] / 2 );
	}
	memcpy( calcColor, colorTable[CT_HUD_GREEN], sizeof( vec4_t ) );
	calcColor[0] *= armorPercent;
	calcColor[1] *= armorPercent;
	calcColor[2] *= armorPercent;
	cgi_R_SetColor( calcColor );
	CG_DrawPic( x, y, 80 * hudRatio, 80, cgs.media.jk2HudArmor2 );

	// The tic flashes when the inner ring is under 50%
	if ( ps->stats[STAT_ARMOR] )
	{
		if ( armorPercent < 0.5f )
		{
			if ( cg.HUDTickFlashTime < cg.time )
			{
				cg.HUDTickFlashTime = cg.time + 100;
				cg.HUDArmorFlag = cg.HUDArmorFlag ? qfalse : qtrue;
			}
		}
		else
		{
			cg.HUDArmorFlag = qtrue;
		}
	}
	else
	{
		cg.HUDArmorFlag = qfalse;
	}

	if ( cg.HUDArmorFlag )
	{
		cgi_R_SetColor( colorTable[CT_HUD_GREEN] );
		CG_DrawPic( x, y, 80 * hudRatio, 80, cgs.media.jk2HudArmorTic );
	}

	cgi_R_SetColor( colorTable[CT_HUD_GREEN] );
	CG_DrawNumField( x + ( 18 + 14 ) * hudRatio, y + 40 + 14, 3, ps->stats[STAT_ARMOR], 6 * hudRatio, 12, NUM_FONT_SMALL, qfalse );
}

// Lit tics from the last one down; the partial tic is dimmed, the others are black
static void CG_DrawJK2Tics( const jk2HudTic_t *tics, const qhandle_t *images, float value, const float inc,
	const float x, const float y, const float hudRatio )
{
	vec4_t	calcColor;
	int		i;

	for ( i = JK2_HUD_NUM_TICS - 1; i >= 0; i-- )
	{
		if ( value <= 0 )
		{
			memcpy( calcColor, colorTable[CT_BLACK], sizeof( vec4_t ) );
		}
		else
		{
			memcpy( calcColor, colorTable[CT_WHITE], sizeof( vec4_t ) );
			if ( value < inc )
			{
				const float percent = value / inc;
				calcColor[0] *= percent;
				calcColor[1] *= percent;
				calcColor[2] *= percent;
			}
		}

		cgi_R_SetColor( calcColor );
		CG_DrawPic( CG_JK2HudRightX( x + tics[i].x, hudRatio ), y + tics[i].y,
			tics[i].w * hudRatio, tics[i].h, images[tics[i].image] );

		value -= inc;
	}
}

static void CG_DrawJK2Force( const centity_t *cent, const float x, const float y, const float hudRatio )
{
	float		value, inc;

	if ( !cent->gent || !cent->gent->client || !cent->gent->client->ps.forcePowersKnown )
	{
		return;
	}

	// Play the "no force" sound while forceHUDTotalFlashTime is above cg.time
	if ( cg.forceHUDTotalFlashTime > cg.time )
	{
		if ( cg.forceHUDNextFlashTime < cg.time )
		{
			cg.forceHUDNextFlashTime = cg.time + 400;
			cgi_S_StartSound( NULL, 0, CHAN_AUTO, cgs.media.noforceSound );
			cg.forceHUDActive = cg.forceHUDActive ? qfalse : qtrue;
		}
	}
	else
	{
		cg.forceHUDNextFlashTime = 0;
		cg.forceHUDActive = qtrue;
	}

	value = cent->gent->client->ps.forcePower;
	if ( value > cent->gent->client->ps.forcePowerMax )
	{
		value = cent->gent->client->ps.forcePowerMax;
	}
	inc = (float)cent->gent->client->ps.forcePowerMax / JK2_HUD_NUM_TICS;

	CG_DrawJK2Tics( jk2ForceTics, cgs.media.jk2HudForceTic, value, inc, x, y, hudRatio );
}

static void CG_DrawJK2Ammo( const centity_t *cent, const float x, const float y, const float hudRatio )
{
	playerState_t	*ps = &cg.snap->ps;
	float			value, inc;
	vec4_t			numColor;

	if ( !cent->currentState.weapon || !cent->gent )
	{
		return;
	}

	if ( cent->currentState.weapon == WP_SABER )
	{
		// no ammo, the current saber style is shown in the gauge instead
		int index;

		if ( !cg.saberAnimLevelPending && cent->gent->client )
		{//uninitialized after a loadgame, cheat across and get it
			cg.saberAnimLevelPending = cent->gent->client->ps.saberAnimLevel;
		}

		if ( cg.saberAnimLevelPending == SS_FAST )
		{
			index = 0;
		}
		else if ( cg.saberAnimLevelPending == SS_MEDIUM
			|| cg.saberAnimLevelPending == SS_DUAL
			|| cg.saberAnimLevelPending == SS_STAFF )
		{
			index = 1;
		}
		else if ( cg.saberAnimLevelPending == SS_DESANN )
		{
			index = 3;
		}
		else if ( cg.saberAnimLevelPending == SS_TAVION )
		{
			index = 4;
		}
		else
		{
			index = 2;
		}

		// An older jk2hud pk3 has no Desann / Tavion arc: show the strong one
		if ( !cgs.media.jk2HudSaberStyle[index] )
		{
			index = 2;
		}

		cgi_R_SetColor( colorTable[CT_WHITE] );
		CG_DrawPic( CG_JK2HudRightX( x, hudRatio ), y, 80 * hudRatio, 40, cgs.media.jk2HudSaberStyle[index] );
		return;
	}

	if ( cent->currentState.weapon == WP_STUN_BATON )
	{
		return;
	}

	value = ps->ammo[weaponData[cent->currentState.weapon].ammoIndex];
	memcpy( numColor, value > 0 ? colorTable[CT_HUD_ORANGE] : colorTable[CT_RED], sizeof( vec4_t ) );
	cgi_R_SetColor( numColor );

	if ( value < 0 || ( weaponData[cent->currentState.weapon].attackData[0].energyPerShot == 0
		&& weaponData[cent->currentState.weapon].attackData[1].energyPerShot == 0 ) )
	{// infinite ammo
		SimpleHud_DrawString( CG_JK2HudRightX( x + 30, hudRatio ), y + 20, "--", colorTable[CT_HUD_ORANGE] );
		inc = 8.0f / JK2_HUD_NUM_TICS;
		value = 8;
	}
	else
	{
		CG_DrawNumField( CG_JK2HudRightX( x + 30, hudRatio ), y + 26, 3, (int)value, 6 * hudRatio, 12, NUM_FONT_SMALL, qfalse );
		inc = (float)ammoData[weaponData[cent->currentState.weapon].ammoIndex].max / JK2_HUD_NUM_TICS;
	}

	CG_DrawJK2Tics( jk2AmmoTics, cgs.media.jk2HudAmmoTic, value, inc, x, y, hudRatio );
}

/*
================
CG_DrawJK2HUD
================
*/
void CG_DrawJK2HUD( const centity_t *cent, const float hudRatio )
{
	const float y = SCREEN_HEIGHT - 80;

	// Left: inner wire frame, armor, health, metal frame
	cgi_R_SetColor( colorTable[CT_WHITE] );
	CG_DrawPic( 0, y, 80 * hudRatio, 80, cgs.media.jk2HudLeftInner );
	CG_DrawJK2Armor( 0, y, hudRatio );
	CG_DrawJK2Health( 0, y, hudRatio );
	cgi_R_SetColor( colorTable[CT_WHITE] );
	CG_DrawPic( 0, y, 80 * hudRatio, 80, cgs.media.jk2HudLeftFrame );

	// Right: inner wire frame, force, ammo or saber style, metal frame
	cgi_R_SetColor( colorTable[CT_WHITE] );
	CG_DrawPic( CG_JK2HudRightX( SCREEN_WIDTH - 80, hudRatio ), y, 80 * hudRatio, 80, cgs.media.jk2HudRightInner );
	CG_DrawJK2Force( cent, SCREEN_WIDTH - 80, y, hudRatio );
	CG_DrawJK2Ammo( cent, SCREEN_WIDTH - 80, y, hudRatio );
	cgi_R_SetColor( colorTable[CT_WHITE] );
	CG_DrawPic( CG_JK2HudRightX( SCREEN_WIDTH - 80, hudRatio ), y, 80 * hudRatio, 80, cgs.media.jk2HudRightFrame );
}

/*
===================
CG_DrawJK2IconBackground

Bar and prongs behind the weapon, force and inventory selection: the prongs light up
and move out while a selection is shown, the bar opens from its middle line.
===================
*/
void CG_DrawJK2IconBackground( void )
{
	const float	hudRatio = cg_hudRatio.integer ? cgs.widthRatioCoef : 1.0f;
	const float	inTime = cg.inventorySelectTime + WEAPON_SELECT_TIME;
	const float	wpTime = cg.weaponSelectTime + WEAPON_SELECT_TIME;
	const float	fpTime = cg.forcepowerSelectTime + WEAPON_SELECT_TIME;
	const float	x2 = 30.0f;
	const float	y2 = SCREEN_HEIGHT - 70.0f;
	const float	prongLeftX = x2 + 37.0f;
	const float	prongRightX = SCREEN_WIDTH - ( 36.0f + x2 ) * hudRatio;
	// The bar runs from one prong to the other: both ends follow the ratio like the prongs do
	const float	barX = ( x2 + 60.0f ) * hudRatio;
	const float	barWidth = SCREEN_WIDTH - 2.0f * barX;
	qhandle_t	drawType = cgs.media.jk2BackgroundWeapon;
	qhandle_t	prongsOn = cgs.media.jk2ProngOnWeapon;
	float		height, xAdd, t;

	if ( cg.snap->ps.stats[STAT_HEALTH] <= 0 || !cg_drawHUD.integer || cg.zoomMode != 0 )
	{
		return;
	}

	if ( cg.snap->ps.viewEntity > 0 && cg.snap->ps.viewEntity < ENTITYNUM_WORLD )
	{
		return;
	}

	if ( inTime > wpTime )
	{
		drawType = cgs.media.jk2BackgroundInventory;
		prongsOn = cgs.media.jk2ProngOnInventory;
		cg.iconSelectTime = cg.inventorySelectTime;
	}
	else
	{
		cg.iconSelectTime = cg.weaponSelectTime;
	}

	if ( fpTime > inTime && fpTime > wpTime )
	{
		drawType = cgs.media.jk2BackgroundForce;
		prongsOn = cgs.media.jk2ProngOnForce;
		cg.iconSelectTime = cg.forcepowerSelectTime;
	}

	if ( ( cg.iconSelectTime + WEAPON_SELECT_TIME ) < cg.time )
	{// Time is up: close the bar and move the prongs back
		xAdd = 0;
		if ( cg.iconHUDActive )
		{
			t = cg.time - ( cg.iconSelectTime + WEAPON_SELECT_TIME );
			cg.iconHUDPercent = 1.0f - t / 130.0f;
			if ( cg.iconHUDPercent < 0.0f )
			{
				cg.iconHUDActive = qfalse;
				cg.iconHUDPercent = 0.0f;
			}

			xAdd = 8.0f * cg.iconHUDPercent;
			height = 60.0f * cg.iconHUDPercent;
			CG_DrawPic( barX, y2 + 30.0f, barWidth, -height, drawType );		// Top half
			CG_DrawPic( barX, y2 + 30.0f - 2.0f, barWidth, height, drawType );	// Bottom half
		}

		cgi_R_SetColor( colorTable[CT_WHITE] );
		CG_DrawPic( ( prongLeftX + xAdd ) * hudRatio, y2 - 10.0f, 40.0f * hudRatio, 80.0f, cgs.media.jk2ProngOff );
		CG_DrawPic( prongRightX - xAdd * hudRatio, y2 - 10.0f, -40.0f * hudRatio, 80.0f, cgs.media.jk2ProngOff );
		return;
	}

	if ( !cg.iconHUDActive )
	{
		cg.iconHUDPercent = ( cg.time - cg.iconSelectTime ) / 130.0f;
		if ( cg.iconHUDPercent > 1.0f )
		{
			cg.iconHUDActive = qtrue;
			cg.iconHUDPercent = 1.0f;
		}
		else if ( cg.iconHUDPercent < 0.0f )
		{
			cg.iconHUDPercent = 0.0f;
		}
	}
	else
	{
		cg.iconHUDPercent = 1.0f;
	}

	cgi_R_SetColor( colorTable[CT_WHITE] );
	height = 60.0f * cg.iconHUDPercent;
	CG_DrawPic( barX, y2 + 30.0f, barWidth, -height, drawType );			// Top half
	CG_DrawPic( barX, y2 + 30.0f - 2.0f, barWidth, height, drawType );		// Bottom half

	xAdd = 8.0f * cg.iconHUDPercent;
	CG_DrawPic( ( prongLeftX + xAdd ) * hudRatio, y2 - 10.0f, 40.0f * hudRatio, 80.0f, prongsOn );
	CG_DrawPic( prongRightX - xAdd * hudRatio, y2 - 10.0f, -40.0f * hudRatio, 80.0f, prongsOn );
}
