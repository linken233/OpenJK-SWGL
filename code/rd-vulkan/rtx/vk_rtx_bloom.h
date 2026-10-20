/*
===========================================================================
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.
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

#ifndef VK_RTX_BLOOM_H
#define VK_RTX_BLOOM_H

#define LIST_BLOOM_SHADERS \
	SHADER_MODULE_DO( SHADER_BLOOM_DOWNSCALE_COMP,	bloom_downscale_comp ) \
	SHADER_MODULE_DO( SHADER_BLOOM_BLUR_COMP,		bloom_blur_comp ) \
	SHADER_MODULE_DO( SHADER_BLOOM_COMPOSITE_COMP,	bloom_composite_comp )

// shaders
enum {
#define SHADER_MODULE_DO( _index, ... ) _index,
	LIST_BLOOM_SHADERS
#undef SHADER_MODULE_DO
	BLOOM_NUM_SHADERS
};

// pipelines
enum {
	BLOOM_DOWNSCALE,
	BLOOM_BLUR,
	BLOOM_COMPOSITE,
	BLOOM_NUM_PIPELINES
};

#endif // VK_RTX_BLOOM_H
