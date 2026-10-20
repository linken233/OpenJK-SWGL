/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Light styles of the RTX renderer. The cgame sets styleColors[] each frame (RE_SetLightStyle).
// The shaders scale a light by the style value, and use the previous value for gradient pixels.

#include "../tr_local.h"
#include "rtx_light_style.h"

static float		g_prevScale[MAX_LIGHT_STYLES];
static qboolean		g_havePrev = qfalse;

static float CurrentScale( int style )
{
	const byte *c = backEndDataBE->styleColors[style];	// backend: the colors at the time of the swap
	const float s = ( c[0] + c[1] + c[2] ) * ( 1.0f / ( 3.0f * 255.0f ) );

	return s < 0.0f ? 0.0f : s > 1.0f ? 1.0f : s;
}

float RTX_LightStyle_Scale( int style, qboolean prev )
{
	if ( style <= 0 || style >= MAX_LIGHT_STYLES || !pt_light_styles->integer )
		return 1.0f;

	if ( prev && g_havePrev )
		return g_prevScale[style];

	return CurrentScale( style );
}

void RTX_LightStyle_EndFrame( void )
{
	for ( int i = 0; i < MAX_LIGHT_STYLES; i++ )
		g_prevScale[i] = CurrentScale( i );

	g_havePrev = qtrue;
}
