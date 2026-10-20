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

#ifndef VK_RTX_DENOISER_H
#define VK_RTX_DENOISER_H

typedef enum {
	DENOISER_NONE,
	DENOISER_ASVGF,
	DENOISER_NRD_RELAX,
	DENOISER_NRD_REBLUR,
	NUM_DENOISERS
} denoiser_type_t;

typedef struct denoiser_s {
	const char	*name;
	uint32_t	flags;										// DENOISER_FLAG_*: the output format the tracer writes
	void		(*create_pipelines)( void );
	void		(*destroy_pipelines)( void );
	void		(*invalidate_history)( void );				// the next frame has no valid history, may be NULL
	void		(*prepare_ubo)( vkUniformRTX_t *ubo );		// may be NULL
	void		(*pre_lighting)( VkCommandBuffer cmd_buf );	// before direct and indirect lighting, may be NULL
	void		(*filter)( VkCommandBuffer cmd_buf );		// raw channels and G-buffer to the composed color image
	void		(*end_frame)( qboolean active );			// active: this denoiser ran in the frame, may be NULL
} denoiser_t;

const denoiser_t *vk_rtx_get_denoiser( denoiser_type_t type );
denoiser_type_t vk_rtx_denoiser_from_cvar( void );
void		vk_rtx_create_denoiser_pipelines( void );
void		vk_rtx_destroy_denoiser_pipelines( void );
void		vk_rtx_invalidate_denoiser_history( void );
void		vk_rtx_denoisers_prepare_ubo( vkUniformRTX_t *ubo );
void		vk_rtx_denoisers_end_frame( denoiser_type_t active );

extern const denoiser_t vk_rtx_denoiser_asvgf;
extern const denoiser_t vk_rtx_denoiser_nrd_relax;
extern const denoiser_t vk_rtx_denoiser_nrd_reblur;

#endif // VK_RTX_DENOISER_H
