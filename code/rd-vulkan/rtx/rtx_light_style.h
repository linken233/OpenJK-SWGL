/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Light styles of the RTX renderer. Include after tr_local.h.

#pragma once

// Scale 0..1 of a light style, from styleColors[]. Style 0 and pt_light_styles 0 give 1.
// With prev, gives the scale of the previous frame.
float	RTX_LightStyle_Scale( int style, qboolean prev );

// Keeps the scales of this frame as the previous scales. Call it once after the light upload.
void	RTX_LightStyle_EndFrame( void );
