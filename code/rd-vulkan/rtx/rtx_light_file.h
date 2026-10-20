/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// The .lgt v2 file layer of the light edit mode. Include after tr_local.h.

#pragma once

#include <string>
#include "rd-common/rtx_light_edit_api.h"

// Reads maps/<map>.lgt. Registers the lgt lights when the map has no entity light.
// Call it after collect_entity_lights and before collect_cluster_lights.
void		RTX_LightFile_Load( world_t &w, qboolean hasEntityLights );

// Applies the entity overrides and the added lights. Call it after RTX_LightEdit_FinalizeLoad.
void		RTX_LightFile_Apply( world_t &w );

qboolean	RTX_LightFile_Save( void );
qboolean	RTX_LightFile_Reload( void );

// pt_lightgen guard. Gives qfalse when the file holds light edits and force is not set.
// With force, copies the file to .lgt.bak first.
qboolean	RTX_LightFile_CheckRegenerate( const char *mapname, qboolean force );

// Text of the emissive and global blocks of the file of a map. pt_lightgen writes it back.
std::string	RTX_LightFile_KeptBlocks( const char *mapname );

// Text of the global block for a sky setting.
void		RTX_LightFile_FormatSkyBlock( std::string &out, const rtxSkyDesc_t &sky );
