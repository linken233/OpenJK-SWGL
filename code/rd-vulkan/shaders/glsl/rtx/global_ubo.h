/*
Copyright (C) 2018 Christoph Schied
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef  _GLOBAL_UBO_DESCRIPTOR_SET_LAYOUT_H_
#define  _GLOBAL_UBO_DESCRIPTOR_SET_LAYOUT_H_

#include "constants.h"
#include "shader_structs.h"

#define GLOBAL_UBO_BINDING_IDX               0
#define GLOBAL_INSTANCE_BUFFER_BINDING_IDX   1

#define UBO_CVAR_DO( name, default_value ) GLOBAL_UBO_VAR_LIST_DO( FLOAT, name )

#define UBO_CVAR_LIST_FLT \
	UBO_CVAR_DO( flt_antilag_hf,					4		)	/* A-SVGF anti-lag filter strength, [0..inf) */ \
	UBO_CVAR_DO( flt_antilag_lf,					0.2		) \
	UBO_CVAR_DO( flt_antilag_spec,					2		) \
	UBO_CVAR_DO( flt_antilag_spec_motion,			0.004	) /* scaler for motion vector scaled specular anti-blur adjustment */ \
	UBO_CVAR_DO( flt_atrous_depth,					0.5		)	/* wavelet fitler sensitivity to depth, [0..inf) */ \
	UBO_CVAR_DO( flt_atrous_deflicker_lf,			2		)	/* max brightness difference between adjacent pixels in the LF channel, (0..inf) */ \
	UBO_CVAR_DO( flt_atrous_hf,						4		)	/* number of a-trous wavelet filter iterations on the LF channel, [0..4] */ \
	UBO_CVAR_DO( flt_atrous_lf,						4		) \
	UBO_CVAR_DO( flt_atrous_spec,					3		) \
	UBO_CVAR_DO( flt_atrous_lum_hf,					16		)	/* wavelet filter sensitivity to luminance, [0..inf) */ \
	UBO_CVAR_DO( flt_atrous_normal_hf,				64		)	/* wavelet filter sensitivity to normals, [0..inf) */ \
	UBO_CVAR_DO( flt_atrous_normal_lf,				8		) \
	UBO_CVAR_DO( flt_atrous_normal_spec,			1		) \
	UBO_CVAR_DO( flt_fixed_albedo,					0		)	/* if nonzero, replaces surface albedo with that value after filtering */ \
	UBO_CVAR_DO( flt_grad_weapon,					0.25	)	/* gradient scale for the first person weapon, [0..1] */ \
	UBO_CVAR_DO( flt_min_alpha_color_hf,			0.02	)	/* minimum weight for the new frame data, color channel, (0..1] */ \
	UBO_CVAR_DO( flt_min_alpha_color_lf,			0.01	) \
	UBO_CVAR_DO( flt_min_alpha_color_spec,			0.01	) \
	UBO_CVAR_DO( flt_min_alpha_moments_hf,			0.01	)	/* minimum weight for the new frame data, moments channel, (0..1] */  \
	UBO_CVAR_DO( flt_scale_hf,						1		)	/* overall per-channel output scale, [0..inf) */ \
	UBO_CVAR_DO( flt_scale_lf,						1		) \
	UBO_CVAR_DO( flt_scale_overlay,					1.0		)	/* scale for transparent and emissive objects visible with primary rays */ \
	UBO_CVAR_DO( flt_scale_spec,					1		) \
	UBO_CVAR_DO( flt_show_gradients,				0		)	/* debug bitmask: 1 = gradients as an overlay, 2 = raw HF, 4 = raw LF, 8 = raw specular, 16 = LF history, 32 = LF irradiance only, 64 = raw/history LF ratio, 128 = history length */ \
	UBO_CVAR_DO( flt_taa,							1		)	/* temporal anti-aliasing mode: 0 = off, 1 = regular TAA, 2 = temporal upscale: AA_MODE_UPSCALE */ \
	UBO_CVAR_DO( flt_taa_anti_sparkle,				0.25	)	/* strength of the anti-sparkle filter of TAA, [0..1] */ \
	UBO_CVAR_DO( flt_taa_variance,					1.0		)	/* temporal AA variance window scale, 0 means disable NCC, [0..inf) */ \
	UBO_CVAR_DO( flt_taa_history_weight,			0.95	)	/* temporal AA weight of the history sample, [0..1) */ \
	UBO_CVAR_DO( flt_temporal_hf,					1		)	/* temporal filter strength, [0..1] */ \
	UBO_CVAR_DO( flt_temporal_lf,					1		) \
	UBO_CVAR_DO( flt_temporal_spec,					1		) \
	
#define UBO_CVAR_LIST_PT \
	UBO_CVAR_DO( pt_aperture,						2.0		)	/* aperture size for the Depth of Field effect, in world units */ \
	UBO_CVAR_DO( pt_aperture_angle,					0		)	/* rotation of the polygonal aperture, [0..1] */ \
	UBO_CVAR_DO( pt_aperture_type,					0		)	/* number of aperture polygon edges, circular if less than 3 */ \
	UBO_CVAR_DO( pt_beam_softness,					1.0		)	/* beam softness */ \
	UBO_CVAR_DO( pt_bump_scale,						1.0		)	/* scale for normal maps [0..1] */ \
	UBO_CVAR_DO( pt_cameras,						1		)	/* switch for security cameras, 0 or 1 */ \
	UBO_CVAR_DO( pt_direct_polygon_lights,			1		)	/* switch for direct lighting from local polygon lights, 0 or 1 */ \
	UBO_CVAR_DO( pt_direct_roughness_threshold,		0.18	)	/* roughness value where the path tracer switches direct light specular sampling from NDF based to light based, [0..1] */ \
	UBO_CVAR_DO( pt_direct_dyn_lights,			1		)	/* switch for direct lighting from local sphere lights, 0 or 1 */ \
	UBO_CVAR_DO( pt_direct_sun_light,				1		)	/* switch for direct lighting from the sun, 0 or 1 */ \
	UBO_CVAR_DO( pt_explosion_brightness,			4.0		)	/* brightness factor for explosions */ \
	UBO_CVAR_DO( pt_fake_roughness_threshold,		0.20	)	/* roughness value where the path tracer starts switching indirect light specular sampling from NDF based to SH based, [0..1] */ \
	UBO_CVAR_DO( pt_focus,							200		)	/* focal distance for the Depth of Field effect, in world units */ \
	UBO_CVAR_DO( pt_indirect_polygon_lights,		1		)	/* switch for bounce lighting from local polygon lights, 0 or 1 */ \
	UBO_CVAR_DO( pt_indirect_dyn_lights,			1		)	/* switch for bounce lighting from local sphere lights, 0 or 1 */ \
	UBO_CVAR_DO( pt_light_stats,					1		)	/* switch for statistical light PDF correction, 0 or 1 */ \
	UBO_CVAR_DO( pt_max_log_sky_luminance,			-3		)	/* maximum sky luminance, log2 scale, used for polygon light selection, (-inf..inf) */ \
	UBO_CVAR_DO( pt_min_log_sky_luminance,			-10		)	/* minimum sky luminance, log2 scale, used for polygon light selection, (-inf..inf) */ \
	UBO_CVAR_DO( pt_metallic_override,				-1		)	/* overrides metallic parameter of all materials if non-negative, [0..1] */ \
	UBO_CVAR_DO( pt_ndf_trim,						0.9		)	/* trim factor for GGX NDF sampling (0..1] */ \
	UBO_CVAR_DO( pt_num_bounce_rays,				1		)	/* number of bounce rays, valid values are 0 (disabled), 0.5 (half-res diffuse), 1 (full-res diffuse + specular), 2 (two bounces) */ \
	UBO_CVAR_DO( pt_particle_softness,				1.0		)	/* particle softness */ \
	UBO_CVAR_DO( pt_reflect_refract,				2		)	/* number of reflection or refraction bounces: 0, 1 or 2 */ \
	UBO_CVAR_DO( pt_restir,							1		)	/* switch for using RIS or ReSTIR, 0 or 1 */ \
	UBO_CVAR_DO( pt_restir_spatial,					1		)	/* ReSTIR spatial samples */ \
	UBO_CVAR_DO( pt_restir_spatial_cap,				1		)	/* Factor used to cap ReSTIR spatial sample weight */ \
	UBO_CVAR_DO( pt_roughness_override,				-1		)	/* overrides roughness of all materials if non-negative, [0..1] */ \
	UBO_CVAR_DO( pt_specular_anti_flicker,			2		)	/* fade factor for rough reflections of surfaces far away, [0..inf) */ \
	UBO_CVAR_DO( pt_specular_mis,					1		)	/* enables the use of MIS between specular direct lighting and BRDF specular rays */ \
	UBO_CVAR_DO( pt_show_sky,						0		)	/* switch for showing the sky polygons, 0 or 1 */ \
	UBO_CVAR_DO( pt_sun_bounce_range,				2000	)	/* range limiter for indirect lighting from the sun, helps reduce noise, (0..inf) */ \
	UBO_CVAR_DO( pt_sun_specular,					1.0		)	/* scale for the direct specular reflection of the sun */ \
	UBO_CVAR_DO( pt_texture_lod_bias,				0		)	/* LOD bias for textures, (-inf..inf) */ \
	UBO_CVAR_DO( pt_toksvig,						1		)	/* intensity of Toksvig roughness correction, [0..inf) */ \
	UBO_CVAR_DO( pt_thick_glass,					0		)	/* switch for thick glass refraction: 0 (disabled), 1 (reference mode only), 2 (real-time mode) */ \
	UBO_CVAR_DO( pt_water_density,					0.5		)	/* scale for light extinction in water and other media, [0..inf) */ \
	
#define UBO_CVAR_LIST_TM \
	UBO_CVAR_DO( tm_debug,							0		)	/* switch to show the histogram (1) or tonemapping curve (2) */ \
	UBO_CVAR_DO( tm_dyn_range_stops,				7.0		)	/* Effective display dynamic range in linear stops = log2((max+refl)/(darkest+refl)) (eqn. 6), (-inf..0) */ \
	UBO_CVAR_DO( tm_enable,							1		)	/* switch for tone mapping, 0 or 1 */ \
	UBO_CVAR_DO( tm_exposure_bias,					-1.0	)	/* exposure bias, log-2 scale */ \
	UBO_CVAR_DO( tm_exposure_speed_down,			1		)	/* speed of exponential eye adaptation when scene gets darker, 0 means instant */ \
	UBO_CVAR_DO( tm_exposure_speed_up,				2		)	/* speed of exponential eye adaptation when scene gets brighter, 0 means instant */ \
	UBO_CVAR_DO( tm_blend_scale_border,				1		)	/* scale factor for full screen blend intensity, at screen border */ \
	UBO_CVAR_DO( tm_blend_scale_center,				0		)	/* scale factor for full screen blend intensity, at screen center */ \
	UBO_CVAR_DO( tm_blend_scale_fade_exp,			4		)	/* exponent used to interpolate between "border" and "center" factors */ \
	UBO_CVAR_DO( tm_blend_distance_factor,			1.2		)	/* scale for the distance from the screen center when computing full screen blend intensity */ \
	UBO_CVAR_DO( tm_blend_max_alpha,				0.2		)	 /* maximum opacity for full screen blend effects */ \
	UBO_CVAR_DO( tm_high_percentile,				90		)	/* high percentile for computing histogram average, (0..100] */ \
	UBO_CVAR_DO( tm_knee_start,						0.6		)	/* where to switch from a linear to a rational function ramp in the post-tonemapping process, (0..1)  */ \
	UBO_CVAR_DO( tm_low_percentile,					70		)	/* low percentile for computing histogram average, [0..100) */ \
	UBO_CVAR_DO( tm_max_luminance,					1.0		)	/* auto-exposure maximum luminance, (0..inf) */ \
	UBO_CVAR_DO( tm_min_luminance,					0.0002	)	/* auto-exposure minimum luminance, (0..inf) */ \
	UBO_CVAR_DO( tm_noise_blend,					0.5		)	/* Amount to blend noise values between autoexposed and flat image [0..1] */ \
	UBO_CVAR_DO( tm_noise_stops,					-12		)	/* Absolute noise level in photographic stops, (-inf..inf) */ \
	UBO_CVAR_DO( tm_reinhard,						0.5		)	/* blend factor between adaptive curve tonemapper (0) and Reinhard curve (1) */ \
	UBO_CVAR_DO( tm_slope_blur_sigma,				12.0	)	/* sigma for Gaussian blur of tone curve slopes, (0..inf) */ \
	UBO_CVAR_DO( tm_white_point,					10.0	)	/* how bright colors can be before they become white, (0..inf) */ \
	UBO_CVAR_DO( tm_hdr_peak_nits,					800.0	)	/* Exposure value 0 is mapped to this display brightness (post tonemapping) */ \
	UBO_CVAR_DO( tm_hdr_saturation_scale,			100		)	/* HDR mode saturation adjustment, percentage [0..200], with 0% -> desaturated, 100% -> normal, 200% -> oversaturated */ \
	UBO_CVAR_DO( ui_hdr_nits,						300		)	/* HDR mode UI (stretch pic) brightness in nits */ \

#define UBO_CVAR_LIST_RC \
	UBO_CVAR_DO( pt_rc_enable,			0	)	/* switch for the radiance cache update: 0 or 1 */ \
	UBO_CVAR_DO( pt_rc_debug,			0	)	/* radiance cache debug view: 0 = off, 1 = cell radiance times albedo, 2 = cell key colour, 3 = level colour */ \
	UBO_CVAR_DO( pt_rc_voxel_size,		8	)	/* cell size at level 0, in world units */ \
	UBO_CVAR_DO( pt_rc_scale_distance,	256	)	/* distance to the camera where the cell size doubles; it doubles again at each multiple of it */ \
	UBO_CVAR_DO( pt_rc_update_stride,	4	)	/* one update path per stride x stride pixels, [1..RC_MAX_UPDATE_STRIDE] */ \
	UBO_CVAR_DO( pt_rc_bounces,		3	)	/* surfaces per update path that write to the cache, [1..RC_MAX_BOUNCES] */ \
	UBO_CVAR_DO( pt_rc_max_frames_weight,	32	)	/* samples of history a cell keeps, (0..inf) */ \
	UBO_CVAR_DO( pt_rc_stale_frames,	64	)	/* frames without a sample after which a cell is removed */ \
	UBO_CVAR_DO( pt_rc_min_weight,		2	)	/* samples a cell needs before a query trusts it */ \
	UBO_CVAR_DO( pt_rc_min_hit_voxels,	2	)	/* a query needs a bounce hit at least this many cell sizes away: near hits fall back to NEE */ \

#define UBO_CVAR_LIST_GI \
	UBO_CVAR_DO( pt_restir_gi,			0	)	/* ReSTIR GI for the first diffuse bounce: 0 = off, 1 = temporal reuse, 2 = temporal and spatial reuse (with one bounce ray); off while pt_num_bounce_rays is 0.5 */ \
	UBO_CVAR_DO( pt_restir_gi_m_clamp,	20	)	/* temporal cap of the number of candidates a reservoir stands for, [1..65535] */ \
	UBO_CVAR_DO( pt_restir_gi_spatial_samples,	3	)	/* neighbours that the spatial pass takes, [0..8] */ \
	UBO_CVAR_DO( pt_restir_gi_spatial_radius,	16	)	/* radius of the disk of the spatial neighbours, in pixels */ \
	UBO_CVAR_DO( pt_restir_gi_max_age,	30	)	/* frames a sample point lives in the reservoirs before it is dropped, [0..255]; 0 switches the temporal reuse off */ \

#define UBO_CVAR_LIST_FX \
	UBO_CVAR_DO( pt_weapon_fx,			1	)	/* saber and bolt sprites skip the tone mapper and add to the screen as the rasterizer does: 0 = off */ \

#define UBO_CVAR_LIST \
	UBO_CVAR_LIST_FLT \
	UBO_CVAR_LIST_PT \
	UBO_CVAR_LIST_TM \
	UBO_CVAR_LIST_RC \
	UBO_CVAR_LIST_GI \
	UBO_CVAR_LIST_FX \

#ifdef GLSL
	#define FOG_VOLUMES(n)	vec4 n[MAX_FOG_VOLUMES * 3];
#else
	#define FOG_VOLUMES(n)	alignas(16) float n[MAX_FOG_VOLUMES * 3][4];
#endif

#define GLOBAL_UBO_VAR_LIST \
	GLOBAL_UBO_VAR_LIST_DO( MAT4,	 V								) \
	GLOBAL_UBO_VAR_LIST_DO( MAT4,	 invV							) \
	GLOBAL_UBO_VAR_LIST_DO( MAT4,	 V_prev							) \
	GLOBAL_UBO_VAR_LIST_DO( MAT4,	 P								) \
	GLOBAL_UBO_VAR_LIST_DO( MAT4,	 invP							) \
	GLOBAL_UBO_VAR_LIST_DO( MAT4,	 P_prev							) \
	GLOBAL_UBO_VAR_LIST_DO( MAT4,	 invP_prev						) \
	GLOBAL_UBO_VAR_LIST_DO( MAT4,	 shadow_map_VP					) \
	GLOBAL_UBO_VAR_LIST_DO( MAT4,	 environment_rotation_matrix	) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC4,	 cam_pos						) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC4,	 world_center					) \
	GLOBAL_UBO_VAR_LIST_DO( VEC4,	 world_size						) \
	GLOBAL_UBO_VAR_LIST_DO( VEC4,	 world_half_size_inv			) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC2,	 sub_pixel_jitter				) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   prev_adapted_luminance			) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   temporal_blend_factor			) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC4,	 fs_blend_color					) \
	GLOBAL_UBO_VAR_LIST_DO( VEC4,	 fs_colorize					) \
	\
	GLOBAL_UBO_VAR_LIST_DO( INT,	 current_frame_idx				) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 width							) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 height							) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 current_gpu_slice_width		) \
	\
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 medium							) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 time							) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 first_person_model				) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 environment_type				) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC3,	 sun_direction					) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 sun_solid_angle				) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC3,	 sun_tangent					) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 sun_tan_half_angle				) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC3,	 sun_bitangent					) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 sun_bounce_scale				) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC3,	 sun_color						) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 sun_cos_half_angle				) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC3,	 sun_direction_envmap			) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 sun_visible					) \
	\
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 unscaled_width					) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 unscaled_height				) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 prev_gpu_slice_width			) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 render_mode					) \
	\
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   inv_width						) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   inv_height						) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 prev_width						) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 prev_height					) \
	\
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 cluster_debug_index			) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 screen_image_width				) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 screen_image_height			) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 pt_env_scale					) \
	\
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 pt_swap_checkerboard			) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 shadow_map_depth_scale			) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 god_rays_intensity				) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 god_rays_eccentricity			) \
	\
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   cylindrical_hfov				) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   cylindrical_hfov_prev			) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 tonemap_hdr_clamp_strength		) \
	GLOBAL_UBO_VAR_LIST_DO( INT,	 pt_denoiser_flags				) /* DENOISER_FLAG_* of the active denoiser */ \
	GLOBAL_UBO_VAR_LIST_DO( INT,	 tonemap_hdr					) /* the tone mapper writes HDR */ \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 tonemap_per_channel			) /* tm_per_channel, as the tone mapper takes it */ \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 tonemap_contrast			) /* tm_contrast, as the tone mapper takes it */ \
	GLOBAL_UBO_VAR_LIST_DO( INT,	 restir_gi_history_valid		) /* the reservoirs of ReSTIR GI in the images of the previous frame are valid */ \
\
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 num_static_lights				) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 taa_image_width				) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 taa_image_height				) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 taa_output_width				) \
	\
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 taa_output_height				) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 prev_taa_output_width			) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 prev_taa_output_height			) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 bloom_intensity				) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC2,    projection_fov_scale			) \
	GLOBAL_UBO_VAR_LIST_DO( VEC2,    projection_fov_scale_prev		) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC2,    pad1							) \
	GLOBAL_UBO_VAR_LIST_DO( INT,     restir_m_clamp					) \
	GLOBAL_UBO_VAR_LIST_DO( INT,     pt_projection					) \
	\
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   sky_transmittance				) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   sky_phase_g					) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   sky_amb_phase_g				) \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,   sky_scattering					) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC3,	 physical_sky_ground_radiance	) \
	GLOBAL_UBO_VAR_LIST_DO( INT	,	 physical_sky_flags				) \
	\
	GLOBAL_UBO_VAR_LIST_DO( VEC4,	 distortion_cloak_pass0			) /* a full-screen pass of the cloak: scale x, scale y, alpha, GL blend bits (0: no pass) */ \
	GLOBAL_UBO_VAR_LIST_DO( VEC4,	 distortion_cloak_pass1			) \
	GLOBAL_UBO_VAR_LIST_DO( INT,	 distortion_surfaces			) /* the frame has screen distortion surfaces */ \
	\
	UBO_CVAR_LIST /* the CVAR list is not vec4-aligned: the fog array follows it */ \
	\
	GLOBAL_UBO_VAR_LIST_DO( FOG_VOLUMES, fog_volumes ) /* per volume: mins xyz + k2, maxs xyz, colour rgb */ \
	\
	/* Keep these two last: a member added before the CVAR list moves every offset behind it. */ \
	GLOBAL_UBO_VAR_LIST_DO( VEC3,	 ambient_floor					) /* minimum lighting of a surface point (before the albedo), 0: off */ \
	GLOBAL_UBO_VAR_LIST_DO( FLOAT,	 sky_diffuse					) /* 0..1: share of the sun that the cloud layer turns into sky light */

STRUCT (  
	MAT4	( transform )
	MAT4	( transform_prev )

	UINT	( material )
	UINT	( shell )
	INT		( cluster )
	UINT	( source_buffer_idx )
	UINT	( prim_count )

	UINT	( prim_offset_curr_pose_curr_frame )
	UINT	( prim_offset_prev_pose_curr_frame )
	UINT	( prim_offset_curr_pose_prev_frame )
	UINT	( prim_offset_prev_pose_prev_frame )

	INT		( mdxm_matrix_offset_curr )
	INT		( mdxm_matrix_offset_prev )
	UINT	( pad0 )
	UINT	( pad1 )

	UINT	( pad2 )
	INT		( idx_offset )
	FLOAT	( pose_lerp_curr_frame )
	FLOAT	( pose_lerp_prev_frame )

	UINT	( alpha_and_frame )
	UINT	( render_buffer_idx )
	UINT	( render_prim_offset )

, ModelInstance )
#define MODELINSTANCE(n) ModelInstance n;
	
STRUCT (  
	MAT4	( M )
	INT		( frame )
	FLOAT	( padding[3] )
, BspMeshInstance )
#define BSPMESHINSTANCE(n) BspMeshInstance n;

STRUCT ( 
	UINT			( animated_model_indices		[SHADER_MAX_ENTITIES]		)
	MODELINSTANCE	( model_instances				[SHADER_MAX_ENTITIES]		)
	UINT			( model_current_to_prev			[SHADER_MAX_ENTITIES]		)
	UINT			( model_prev_to_current			[SHADER_MAX_ENTITIES]		)
	UINT			( mlight_prev_to_current		[MAX_MODEL_LIGHTS]			)
	UINT            ( tlas_instance_prim_offsets	[MAX_TLAS_INSTANCES]		)
	INT             ( tlas_instance_model_indices	[MAX_TLAS_INSTANCES]		)					  
	UINT			( model_instance_shader_data	[SHADER_MAX_ENTITIES * INSTANCE_SHADER_UINTS] )
, InstanceBuffer ) 

#define GLOBAL_UBO_VAR_LIST_DO( type, name ) type(name)
STRUCT ( 
	GLOBAL_UBO_VAR_LIST
, vkUniformRTX_t )
#undef GLOBAL_UBO_VAR_LIST_DO

#ifdef GLSL
// bindings
layout( set = GLOBAL_UBO_DESC_SET_IDX, binding = GLOBAL_UBO_BINDING_IDX, std140 ) uniform UBO { 
	vkUniformRTX_t global_ubo; 
};

layout( set = GLOBAL_UBO_DESC_SET_IDX, binding = GLOBAL_INSTANCE_BUFFER_BINDING_IDX ) readonly buffer InstanceSSBO { 
	InstanceBuffer instance_buffer; 
};
#endif

#undef UBO_CVAR_DO

#endif /*_GLOBAL_UBO_DESCRIPTOR_SET_LAYOUT_H_*/