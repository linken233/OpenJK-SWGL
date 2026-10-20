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

#include "tr_local.h"

static vkpipeline_t compositing_pipeline;

static void vk_rtx_none_create_pipelines( void )
{
	vk_rtx_create_standard_compute_pipeline( &compositing_pipeline, vk.compute_shader[SHADER_COMPOSITING_COMP], NULL, 0 );
}

static void vk_rtx_none_destroy_pipelines( void )
{
	vk_rtx_destroy_pipeline( &compositing_pipeline );
}

static void vk_rtx_none_filter( VkCommandBuffer cmd_buf )
{
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_SH] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_COCG] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_HF] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_SPEC] );

	BEGIN_PERF_MARKER( cmd_buf, PROFILER_COMPOSITING );

	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &compositing_pipeline );

	qvkCmdDispatch( cmd_buf,
		(vk.gpu_slice_width + 15) / 16,
		(vk.extent_render.height + 15) / 16,
		1 );

	END_PERF_MARKER( cmd_buf, PROFILER_COMPOSITING );

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_DENOISED_COLOR] );
}

static const denoiser_t denoiser_none = {
	"none",
	0,
	vk_rtx_none_create_pipelines,
	vk_rtx_none_destroy_pipelines,
	NULL,
	NULL,
	NULL,
	vk_rtx_none_filter,
	NULL
};

const denoiser_t *vk_rtx_get_denoiser( denoiser_type_t type )
{
	static const denoiser_t *denoisers[NUM_DENOISERS] = {
		&denoiser_none,
		&vk_rtx_denoiser_asvgf,
		&vk_rtx_denoiser_nrd_relax,
		&vk_rtx_denoiser_nrd_reblur
	};

	return denoisers[type];
}

// The cvar value equals the enum value. A value with no implemented denoiser means none.
denoiser_type_t vk_rtx_denoiser_from_cvar( void )
{
	static int warned_value = 0;
	const int value = pt_denoiser->integer;
	const qboolean available = (qboolean)( value >= DENOISER_NONE && value < NUM_DENOISERS &&
		( value < DENOISER_NRD_RELAX || vk_rtx_nrd_available() ) );

	if ( !available && value != warned_value )
		ri.Printf( PRINT_WARNING, "pt_denoiser %d is not available, no denoiser\n", value );

	warned_value = value;
	return available ? (denoiser_type_t)value : DENOISER_NONE;
}

void vk_rtx_create_denoiser_pipelines( void )
{
	for ( int i = 0; i < NUM_DENOISERS; i++ )
		vk_rtx_get_denoiser( (denoiser_type_t)i )->create_pipelines();
}

void vk_rtx_destroy_denoiser_pipelines( void )
{
	for ( int i = 0; i < NUM_DENOISERS; i++ )
		vk_rtx_get_denoiser( (denoiser_type_t)i )->destroy_pipelines();
}

void vk_rtx_invalidate_denoiser_history( void )
{
	for ( int i = 0; i < NUM_DENOISERS; i++ )
	{
		const denoiser_t *denoiser = vk_rtx_get_denoiser( (denoiser_type_t)i );

		if ( denoiser->invalidate_history )
			denoiser->invalidate_history();
	}
}

void vk_rtx_denoisers_prepare_ubo( vkUniformRTX_t *ubo )
{
	for ( int i = 0; i < NUM_DENOISERS; i++ )
	{
		const denoiser_t *denoiser = vk_rtx_get_denoiser( (denoiser_type_t)i );

		if ( denoiser->prepare_ubo )
			denoiser->prepare_ubo( ubo );
	}
}

void vk_rtx_denoisers_end_frame( denoiser_type_t active )
{
	for ( int i = 0; i < NUM_DENOISERS; i++ )
	{
		const denoiser_t *denoiser = vk_rtx_get_denoiser( (denoiser_type_t)i );

		if ( denoiser->end_frame )
			denoiser->end_frame( (qboolean)( i == active ) );
	}
}
