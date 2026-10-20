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

#ifndef VK_RTX_RADIANCE_CACHE_H
#define VK_RTX_RADIANCE_CACHE_H

void		vk_rtx_radiance_cache_create_buffers( void );
void		vk_rtx_radiance_cache_destroy_buffers( void );
void		vk_rtx_radiance_cache_create_pipelines( void );
void		vk_rtx_radiance_cache_destroy_pipelines( void );
void		vk_rtx_radiance_cache_invalidate( void );
void		vk_rtx_radiance_cache_update( VkCommandBuffer cmd_buf );
void		vk_rtx_radiance_cache_resolve( VkCommandBuffer cmd_buf );
void		vk_rtx_radiance_cache_debug( VkCommandBuffer cmd_buf );

#endif // VK_RTX_RADIANCE_CACHE_H
