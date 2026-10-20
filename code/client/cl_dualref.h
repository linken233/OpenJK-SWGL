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

#pragma once

// g_FastRendererSwitch: returns one refexport_t that drives both renderers.
// The first renderer owns the ghoul2 instances and answers the queries.
const refexport_t *CL_DualRef_Init( const refexport_t *first, const char *firstName, const refexport_t *second, const char *secondName );

void CL_DualRef_Shutdown( void );		// after the renderer DLLs are unloaded
const char *CL_DualRef_ShownName( void );	// library name of the renderer on screen, NULL without g_FastRendererSwitch
void CL_SwitchRenderer_f( void );	// g_SwitchRenderer
void CL_ShowSplit_f( void );		// g_ShowSplit
