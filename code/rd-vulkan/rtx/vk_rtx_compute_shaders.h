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

#ifndef VK_RTX_COMPUTE_SHADERS_H
#define VK_RTX_COMPUTE_SHADERS_H

#define LIST_RTX_COMPUTE_SHADERS \
	SHADER_MODULE_DO( SHADER_ASVGF_GRADIENT_IMG_COMP,			asvgf_gradient_img_comp		 ) \
	SHADER_MODULE_DO( SHADER_ASVGF_GRADIENT_ATROUS_COMP,		asvgf_gradient_atrous_comp	 ) \
	SHADER_MODULE_DO( SHADER_ASVGF_GRADIENT_REPROJECT_COMP,		asvgf_gradient_reproject_comp) \
	SHADER_MODULE_DO( SHADER_ASVGF_ATROUS_COMP,					asvgf_atrous_comp			 ) \
	SHADER_MODULE_DO( SHADER_ASVGF_LF_COMP,						asvgf_lf_comp				 ) \
	SHADER_MODULE_DO( SHADER_ASVGF_TEMPORAL_COMP,				asvgf_temporal_comp			 ) \
	SHADER_MODULE_DO( SHADER_CHECKERBOARD_INTERLEAVE_COMP,		checkerboard_interleave_comp ) \
	SHADER_MODULE_DO( SHADER_ASVGF_TAAU_COMP,					asvgf_taau_comp				 ) \
	SHADER_MODULE_DO( SHADER_COMPOSITING_COMP,					compositing_comp			 ) \
	SHADER_MODULE_DO( SHADER_SHADOW_MAP_VERT,					shadow_map_vert				 ) \
	SHADER_MODULE_DO( SHADER_GOD_RAYS_COMP,						god_rays_comp				 ) \
	SHADER_MODULE_DO( SHADER_GOD_RAYS_FILTER_COMP,				god_rays_filter_comp		 ) \
	SHADER_MODULE_DO( SHADER_INSTANCE_GEOMETRY_COMP,			instance_geometry_comp		 ) 	SHADER_MODULE_DO( SHADER_RADIANCE_CACHE_RESOLVE_COMP,		radiance_cache_resolve_comp	 ) 	SHADER_MODULE_DO( SHADER_RADIANCE_CACHE_DEBUG_COMP,			radiance_cache_debug_comp	 ) \
	SHADER_MODULE_DO( SHADER_NRD_PREPARE_COMP,					nrd_prepare_comp			 ) \
	SHADER_MODULE_DO( SHADER_NRD_COMPOSITE_COMP,				nrd_composite_comp			 ) \
	SHADER_MODULE_DO( SHADER_DISTORTION_COMP,					distortion_comp				 ) \

// shaders
enum {
#define SHADER_MODULE_DO( _index, ... ) _index,
	LIST_RTX_COMPUTE_SHADERS
#undef SHADER_MODULE_DO
	NUM_RTX_COMPUTE_SHADER_MODULES
};

void		vk_load_rtx_compute_shaders( void );

#endif // VK_RTX_COMPUTE_SHADERS_H
