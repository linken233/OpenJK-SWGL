/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Sky and sun setting of the map. Include after tr_local.h.
//
// The setting does not write any cvar. The physical sky code asks the effective values here.
// The file layer reads and writes it as the `global` block of maps/<map>.lgt.

#pragma once

#include "rd-common/rtx_light_edit_api.h"

// Sea level transmittance of the earth atmosphere near noon, estimated. The hybrid sun uses it.
#define RTX_SKY_HYBRID_TRANSMITTANCE	0.9f

// Map load. Clear drops the setting. MapLoaded applies the q3map_sun default, when the cvar asks for it.
void		RTX_LightSky_Clear( void );
void		RTX_LightSky_MapLoaded( void );

// True when a setting is in force: a mode other than GLOBAL and pt_sky_per_map 1.
qboolean	RTX_LightSky_Active( void );

// Effective values for the physical sky code.
// Value of physical_sky: 0 skybox, 1 earth. Without a setting, the value of the cvar.
int			RTX_LightSky_PhysicalSky( void );

// True in the HYBRID mode: the skybox, with an analytic sun.
qboolean	RTX_LightSky_Hybrid( void );

// Gives the sun of the setting. False without a setting, and in the SKYBOX mode.
qboolean	RTX_LightSky_Sun( float *azimuth, float *elevation, vec3_t color, float *brightness, float *angle );

// Cloud layer, 0..1. Zero without a setting, in the SKYBOX mode (no sun) and with pt_sky_per_map 0.
float		RTX_LightSky_Diffuse( void );

// Ambient floor for the shaders: colour * intensity * pt_ambient_floor_scale. Zero when the map has none.
// It applies in every mode, GLOBAL included.
void		RTX_LightSky_AmbientFloor( vec3_t out );

// Sun colour for the buffer of the sun colour in the HYBRID mode. envScale is pt_env_scale.
// The physical mode integrates the radiance of the sun disk over its solid angle, which
// gives transmittance * sun_color * sun_brightness, then multiplies by pt_env_scale.
void		RTX_LightSky_HybridSun( float envScale, vec3_t out );

// Checks the cvars of the module. Call it once per frame.
void		RTX_LightSky_CvarCheck( void );

// File layer. SetFromFile takes a mode other than GLOBAL as the content of a global block.
void		RTX_LightSky_Defaults( rtxSkyDesc_t *d, int mode );
void		RTX_LightSky_SetFromFile( const rtxSkyDesc_t *desc );
qboolean	RTX_LightSky_GetForSave( rtxSkyDesc_t *out );
void		RTX_LightSky_Saved( void );

// Implementation of the API functions.
qboolean	RTX_LightSky_Get( rtxSkyDesc_t *out );
qboolean	RTX_LightSky_Set( const rtxSkyDesc_t *desc );
void		RTX_LightSky_Reset( void );

// Console commands.
void		RTX_LightSky_Print_f( void );
void		RTX_LightSky_Set_f( void );
void		RTX_LightSky_Reset_f( void );
