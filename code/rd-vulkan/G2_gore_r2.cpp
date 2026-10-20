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

#include "G2_gore_r2.h"
#include "../rd-common/tr_common.h"

#ifdef _G2_GORE

R2GoreTextureCoordinates::R2GoreTextureCoordinates()
{
	Com_Memset ( tex, 0, sizeof (tex) );
	Com_Memset ( tex_vbo, 0, sizeof(tex_vbo) );
}

R2GoreTextureCoordinates::~R2GoreTextureCoordinates()
{
	// The backend reads the surface struct and tex of a frame in flight: free them later.
	// Only the main thread reads verts and indexes (R_UpdateGoreVBO), so they go now.
	for ( int i = 0; i < MAX_LODS; i++ )
	{
		if ( tex_vbo[i] )
		{
			if ( tex_vbo[i]->verts )
				ri.Z_Free( tex_vbo[i]->verts );
			tex_vbo[i]->verts = NULL;

			if ( tex_vbo[i]->indexes )
				ri.Z_Free( tex_vbo[i]->indexes );
			tex_vbo[i]->indexes = NULL;

			R_DeferredFree( tex_vbo[i] );
			tex_vbo[i] = NULL;
		}

		else if ( tex[i] )
		{
			R_DeferredFree( tex[i] );
			tex[i] = NULL;
		}
	}
}

#endif