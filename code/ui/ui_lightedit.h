/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// In-game menu of the RTX light edit mode. The menu text is ui/lightedit.menu, or the copy
// that ui_lightedit.cpp holds.

#pragma once

// Handles the console command ledit_menu. Returns qtrue when the command is handled.
qboolean	UI_LightEdit_ConsoleCommand( void );

// Handles the uiScript names that start with "ledit_". Returns qtrue when the script is handled.
qboolean	UI_LightEdit_RunScript( const char *name );

// Feeder FEEDER_LIGHTEDIT.
int			UI_LightEdit_FeederCount( void );
const char	*UI_LightEdit_FeederItemText( int index, int column );
void		UI_LightEdit_FeederSelection( int index );

// Feeder FEEDER_LIGHTEDIT_EMI: the emissive shaders.
int			UI_LightEdit_EmiFeederCount( void );
const char	*UI_LightEdit_EmiFeederItemText( int index, int column );
void		UI_LightEdit_EmiFeederSelection( int index );

// Ownerdraw items UI_LIGHTEDIT_SWATCH, UI_LIGHTEDIT_STATS and UI_LIGHTEDIT_SKY.
void		UI_LightEdit_DrawSwatch( float x, float y, float w, float h );
void		UI_LightEdit_DrawStats( float x, float y, float scale, const vec4_t color, int fontIndex );
void		UI_LightEdit_DrawSky( float x, float y, float scale, const vec4_t color, int fontIndex );
