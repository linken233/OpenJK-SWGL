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

#ifndef VK_ASVGF_H
#define VK_ASVGF_H

#define LIST_ASVGF_PIPELINES \
	PIPELINE_DO( GRADIENT_IMAGE ) \
	PIPELINE_DO( GRADIENT_ATROUS ) \
	PIPELINE_DO( GRADIENT_REPROJECT ) \
	PIPELINE_DO( TEMPORAL ) \
	PIPELINE_DO( ATROUS_LF ) \
	PIPELINE_DO( ATROUS_ITER_0 ) \
	PIPELINE_DO( ATROUS_ITER_1 ) \
	PIPELINE_DO( ATROUS_ITER_2 ) \
	PIPELINE_DO( ATROUS_ITER_3 ) \

// pipelines
enum {
#define PIPELINE_DO( _index ) _index,
	LIST_ASVGF_PIPELINES
#undef PIPELINE_DO
	ASVGF_NUM_PIPELINES
};

#endif // VK_ASVGF_H
