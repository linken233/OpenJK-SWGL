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

#include "path_tracer.h"	// contains global_ubo.h -> constants.h
#include "utils.glsl"
#include "raytracer/path_tracer_transparency.glsl"

#define GLOBAL_TEXTURES_DESC_SET_IDX 1
#include "global_textures.h"

#define VERTEX_BUFFER_DESC_SET_IDX 4
#define VERTEX_READONLY 1
#include "vertex_buffer.h"

#define RAY_GEN_DESCRIPTOR_SET_IDX 0
layout( set = 0, binding = RAY_GEN_DESCRIPTOR_SET_IDX ) 
uniform accelerationStructureEXT topLevelAS[TLAS_COUNT];

#include "read_visbuf.glsl"
#include "compute/asvgf.glsl"
#include "brdf.glsl"
#include "water.glsl"

#define DESATURATE_ENVIRONMENT_MAP 1

// Tangent of the half angle of the sun disk at sky_diffuse 1 (about 31 degrees).
#define SKY_DIFFUSE_TAN_HALF_ANGLE 0.6

/* RNG seeds contain 'X' and 'Y' values that are computed w/ a modulo BLUE_NOISE_RES,
 * so the shift values can be chosen to fit BLUE_NOISE_RES - 1
 * (see generate_rng_seed()) */
#define RNG_SEED_SHIFT_X        0u
#define RNG_SEED_SHIFT_Y        8u
#define RNG_SEED_SHIFT_ISODD    16u
#define RNG_SEED_SHIFT_FRAME    17u

#define RNG_PRIMARY_OFF_X   0
#define RNG_PRIMARY_OFF_Y   1
#define RNG_PRIMARY_APERTURE_X   2
#define RNG_PRIMARY_APERTURE_Y   3

#define RNG_NEE_LIGHT_SELECTION(bounce)   		(4 + 0 + 12 * bounce)
#define RNG_NEE_TRI_X(bounce)             		(4 + 1 + 12 * bounce)
#define RNG_NEE_TRI_Y(bounce)             		(4 + 2 + 12 * bounce)
#define RNG_NEE_LIGHT_TYPE(bounce)        		(4 + 3 + 12 * bounce)
#define RNG_BRDF_X(bounce)                		(4 + 4 + 12 * bounce)
#define RNG_BRDF_Y(bounce)                		(4 + 5 + 12 * bounce)
#define RNG_BRDF_FRESNEL(bounce)          		(4 + 6 + 12 * bounce)
#define RNG_SUNLIGHT_X(bounce)			  		(4 + 7 + 12 * bounce)
#define RNG_SUNLIGHT_Y(bounce)			  		(4 + 8 + 12 * bounce)
#define RNG_RESTIR_SP_LIGHT_SELECTION(bounce) 	(4 + 9 + 12 * bounce)
#define RNG_RESTIR_SPATIAL_X(bounce)	  		(4 + 10 + 12 * bounce)
#define RNG_RESTIR_SPATIAL_Y(bounce)	  		(4 + 11 + 12 * bounce)

// The passes above use the indices up to the end of the bounce RC_RNG_BOUNCE_BASE + RC_MAX_BOUNCES - 1
// (the radiance cache update). The indices from here on are free.
#define RNG_RESTIR_GI_BASE						(4 + 12 * (RC_RNG_BOUNCE_BASE + RC_MAX_BOUNCES))
#define RNG_RESTIR_GI_SELECTION					(RNG_RESTIR_GI_BASE + 0)
// The spatial pass: one selection index and an x and y index for each of up to RESTIR_GI_MAX_SPATIAL_SAMPLES neighbours.
#define RNG_RESTIR_GI_SPATIAL_SELECTION			(RNG_RESTIR_GI_BASE + 1)
#define RNG_RESTIR_GI_SPATIAL_X(i)				(RNG_RESTIR_GI_BASE + 2 + 2 * (i))
#define RNG_RESTIR_GI_SPATIAL_Y(i)				(RNG_RESTIR_GI_BASE + 3 + 2 * (i))

#define PRIMARY_RAY_CULL_MASK        (AS_FLAG_OPAQUE | AS_FLAG_TRANSPARENT | AS_FLAG_VIEWER_MODELS | AS_FLAG_VIEWER_WEAPON | AS_FLAG_SKY)
#define REFLECTION_RAY_CULL_MASK     (AS_FLAG_OPAQUE | AS_FLAG_SKY)
#define BOUNCE_RAY_CULL_MASK         (AS_FLAG_OPAQUE | AS_FLAG_SKY | AS_FLAG_CUSTOM_SKY)
#define SHADOW_RAY_CULL_MASK         (AS_FLAG_OPAQUE)

#define BOUNCE_SPECULAR 1

#define MAX_OUTPUT_VALUE 1000

layout(location = RT_PAYLOAD_GEOMETRY) rayPayloadEXT RayPayloadGeometry ray_payload_geometry;
layout(location = RT_PAYLOAD_EFFECTS) rayPayloadEXT RayPayloadEffects ray_payload_effects;

uint rng_seed;

struct Ray {
	vec3 origin, direction;
	float t_min, t_max;
};

vec3
env_map(vec3 direction, bool remove_sun)
{
	direction = (global_ubo.environment_rotation_matrix * vec4(direction, 0)).xyz;

    vec3 envmap = vec3(0);
    if (global_ubo.environment_type == ENVIRONMENT_DYNAMIC)
    {
	    envmap = textureLod(TEX_PHYSICAL_SKY, direction.xzy, 0).rgb;

	    if(remove_sun)
	    {
			// roughly remove the sun from the env map
			envmap = min(envmap, vec3((1 - dot(direction, global_ubo.sun_direction_envmap)) * 200));
		}
		else if(global_ubo.sky_diffuse > 0)
		{
			// The cloud layer hides the sun disk of the view.
			vec3 no_sun = min(envmap, vec3((1 - dot(direction, global_ubo.sun_direction_envmap)) * 200));
			envmap = mix(envmap, no_sun, global_ubo.sky_diffuse);
		}
	}
    else if (global_ubo.environment_type == ENVIRONMENT_STATIC)
    {
        envmap = textureLod(TEX_ENVMAP, direction.xzy, 0).rgb;
#if DESATURATE_ENVIRONMENT_MAP
        float avg = (envmap.x + envmap.y + envmap.z) / 3.0;
        envmap = mix(envmap, avg.xxx, 0.1) * 0.5;
#endif
    }

	// The cloud layer turns the share of the sun that it removes into a uniform sky radiance. This sky
	// gives the same irradiance on a horizontal surface as the removed sun (E = pi * L).
	if(remove_sun && global_ubo.sky_diffuse > 0 && global_ubo.sun_visible != 0)
	{
		float sun_up = max(global_ubo.sun_direction_envmap.z, 0.1);
		envmap += sun_color_ubo.sun_color * (global_ubo.sky_diffuse * sun_up / (M_PI * max(global_ubo.pt_env_scale, 1e-4)));
	}

	return envmap;
}

#include "light_lists.h"

ivec2 get_image_position()
{
	ivec2 pos;
	bool is_even_checkerboard = gl_LaunchIDEXT.z == 0;
	if(global_ubo.pt_swap_checkerboard != 0)
		is_even_checkerboard = !is_even_checkerboard;

	if (is_even_checkerboard) {
		pos.x = int(gl_LaunchIDEXT.x * 2) + int(gl_LaunchIDEXT.y & 1);
	} else {
		pos.x = int(gl_LaunchIDEXT.x * 2 + 1) - int(gl_LaunchIDEXT.y & 1);
	}

	pos.y = int(gl_LaunchIDEXT.y);
	return pos;
}

ivec2 get_image_size()
{
	return ivec2(global_ubo.width, global_ubo.height);
}

bool
found_intersection(RayPayloadGeometry rp)
{
	return rp.primitive_id != ~0u;
}

Triangle get_hit_triangle(RayPayloadGeometry rp)
{
	return load_and_transform_triangle(
		/* instance_idx = */ rp.buffer_and_instance_idx >> 16,
		/* buffer_idx = */ rp.buffer_and_instance_idx & 0xffff,
		rp.primitive_id);
}

uint get_instance_index(RayPayloadGeometry rp)
{
	int instance_idx = rp.buffer_and_instance_idx >> 16;
	uint buffer_idx = rp.buffer_and_instance_idx & 0xffff;

	if (buffer_idx != VERTEX_BUFFER_INSTANCED)
		return ~0u;
	
	Triangle t = load_triangle(buffer_idx, rp.primitive_id);
	return t.instance_index;
}

vec3
get_hit_barycentric( RayPayloadGeometry rp )
{
	vec3 bary;
	bary.yz = rp.barycentric;
	bary.x  = 1.0 - bary.y - bary.z;
	return bary;
}

float
get_rng(uint idx)
{
	uvec3 p = uvec3(rng_seed >> RNG_SEED_SHIFT_X, rng_seed >> RNG_SEED_SHIFT_Y, rng_seed >> RNG_SEED_SHIFT_ISODD);
	p.z = (p.z + idx);
	p &= uvec3(BLUE_NOISE_RES - 1, BLUE_NOISE_RES - 1, NUM_BLUE_NOISE_TEX - 1);

	return min(texelFetch(TEX_BLUE_NOISE, ivec3(p), 0).r, 0.9999999999999);
	//return fract(vec2(get_rng_uint(idx)) / vec2(0xffffffffu));
}

// material kinds
bool
is_water(uint material )
{
	return (material & MATERIAL_KIND_MASK) == MATERIAL_KIND_WATER;
}

bool
is_slime(uint material)
{
	return (material & MATERIAL_KIND_MASK) == MATERIAL_KIND_SLIME;
}

bool
is_lava(uint material)
{
	return (material & MATERIAL_KIND_MASK) == MATERIAL_KIND_LAVA;
}

bool
is_glass(uint material)
{
	return (material & MATERIAL_KIND_MASK) == MATERIAL_KIND_GLASS;
}

bool
is_transparent(uint material )
{
	uint kind = material & MATERIAL_KIND_MASK;
	return kind == MATERIAL_KIND_TRANSPARENT || kind == MATERIAL_KIND_TRANSP_MODEL;
}

bool
is_chrome(uint material)
{
	uint kind = material & MATERIAL_KIND_MASK;
	return kind == MATERIAL_KIND_CHROME || kind == MATERIAL_KIND_CHROME_MODEL;
}

bool
is_sky(uint material)
{
	uint kind = material & MATERIAL_KIND_MASK;
	return kind == MATERIAL_KIND_SKY;
}

bool
is_screen(uint material)
{
	return (material & MATERIAL_KIND_MASK) == MATERIAL_KIND_SCREEN;
}

bool
is_camera(uint material)
{
	return (material & MATERIAL_KIND_MASK) == MATERIAL_KIND_CAMERA;
}

vec3
correct_emissive(uint material_id, vec3 emissive)
{
	return max(vec3(0), emissive.rgb + vec3(EMISSIVE_TRANSFORM_BIAS));
}

#ifndef NO_TRACE_EXT
void trace_geometry_ray( Ray ray, bool cull_back_faces, uint cullMask )
{
	uint rayFlags = 0; // gl_RayFlagsNoOpaqueEXT
	if ( !cull_back_faces )	// reversed?
		rayFlags |= gl_RayFlagsCullBackFacingTrianglesEXT;
	rayFlags |= gl_RayFlagsSkipProceduralPrimitives;

	ray_payload_geometry.barycentric = vec2(0);
	ray_payload_geometry.primitive_id = ~0u;
	ray_payload_geometry.buffer_and_instance_idx = 0;
	ray_payload_geometry.hit_distance = 0;

	traceRayEXT( topLevelAS[TLAS_INDEX_GEOMETRY], rayFlags, cullMask,
			SBT_RCHIT_GEOMETRY /*sbtRecordOffset*/, 0 /*sbtRecordStride*/, SBT_RMISS_EMPTY /*missIndex*/,
			ray.origin, ray.t_min, ray.direction, ray.t_max, RT_PAYLOAD_GEOMETRY);
}

// Finds the two nearest fog volumes along the ray: rp.fog1 (nearer) and rp.fog2 (further).
// hdr_colors: the colours go to HDR units (blended before the tone mapper) or stay in display units.
// A volume that holds the ray start is rp.fog1 with t_in = ray.t_min. The fog depth is the
// distance inside the volume, as in the rasterizer: T = exp(-k2 * d^2).
void find_fog_volumes(inout RayPayloadEffects rp, Ray ray, bool hdr_colors)
{
	vec3 inv_dir = vec3(1.0) / ray.direction;
	for (int i = 0; i < MAX_FOG_VOLUMES; i++)
	{
		const vec4 vmin   = global_ubo.fog_volumes[i * 3 + 0];	// xyz = mins, w = k2 (0: end of the list)
		const vec4 vmax   = global_ubo.fog_volumes[i * 3 + 1];	// xyz = maxs
		const vec4 vcolor = global_ubo.fog_volumes[i * 3 + 2];	// rgb = colour in screen units

		if (vmin.w == 0.0)
			break;

		vec3 t1 = (vmin.xyz - ray.origin) * inv_dir;
		vec3 t2 = (vmax.xyz - ray.origin) * inv_dir;
		vec3 tn = min(t1, t2);
		vec3 tf = max(t1, t2);
		float t_in = max(max(max(tn.x, tn.y), tn.z), ray.t_min);
		float t_out = min(min(min(tf.x, tf.y), tf.z), ray.t_max);

		if (t_out > t_in)
		{
			vec2 first_t_min_max = unpackHalf2x16(rp.fog1.z);
			vec2 second_t_min_max = unpackHalf2x16(rp.fog2.z);

			bool replaces_first = rp.fog1.w == 0u || t_in < first_t_min_max.x;
			bool replaces_second = rp.fog2.w == 0u || t_in < second_t_min_max.x;

			if (replaces_first || replaces_second)
			{
				uvec4 packed;
				packed.xy = packHalf4x16(vec4(vcolor.rgb, 0));
				packed.z = packHalf2x16(vec2(t_in, t_out));
				// k2 is small: scale it out of the fp16 denormal range
				packed.w = packHalf2x16(vec2(vmin.w * 65536.0, 0.0));

				if (replaces_first)
				{
					rp.fog2 = rp.fog1;
					rp.fog1 = packed;
				}
				else
					rp.fog2 = packed;
			}
		}
	}

	// The colours go from screen units to HDR once, for the two volumes that stay.
	if (!hdr_colors)
		return;

	if (rp.fog1.w != 0u)
		rp.fog1.xy = packHalf4x16(vec4(screen_to_hdr_color(unpackHalf4x16(rp.fog1.xy).rgb), 0));
	if (rp.fog2.w != 0u)
		rp.fog2.xy = packHalf4x16(vec4(screen_to_hdr_color(unpackHalf4x16(rp.fog2.xy).rgb), 0));
}

// The light that a lit sprite gets where no surface lights it: the sky above.
vec3
lit_sprite_sky_light()
{
	return env_map(vec3(0, 0, 1), true) * global_ubo.pt_env_scale;
}

// Rays other than the primary one have no lit surface behind the sprite: the sprites take the sky light.
void
fold_lit_sprites(inout EffectsResult result)
{
	result.additive += result.lit.rgb * lit_sprite_sky_light();
	result.alpha.a = 1.0 - (1.0 - result.alpha.a) * result.lit.a;
	result.lit = vec4(0, 0, 0, 1);
}

EffectsResult 
trace_effects_ray(Ray ray, bool skip_procedural, bool display_fog) 
{
	uint rayFlags = 0;
	if (skip_procedural)
		rayFlags |= gl_RayFlagsSkipProceduralPrimitives;
	
	uint instance_mask = AS_FLAG_EFFECTS;

	ray_payload_effects.transparency = packHalf4x16(vec4(1));	// T: nothing in front
	ray_payload_effects.additive     = uvec2(0);
	ray_payload_effects.glow         = uvec2(0);
	ray_payload_effects.fx           = uvec2(0);
	ray_payload_effects.lit          = packHalf4x16(vec4(0, 0, 0, 1));
	ray_payload_effects.litNearest   = 0.0;
	ray_payload_effects.distances = 0;
	ray_payload_effects.fog1 = uvec4(0);
	ray_payload_effects.fog2 = uvec4(0);
	ray_payload_effects.rayTmax = ray.t_max;

	if (!skip_procedural)
		find_fog_volumes(ray_payload_effects, ray, !display_fog);

	traceRayEXT( topLevelAS[TLAS_INDEX_EFFECTS], rayFlags, instance_mask,
			SBT_RCHIT_EFFECTS /*sbtRecordOffset*/, 0 /*sbtRecordStride*/, SBT_RMISS_EMPTY /*missIndex*/,
			ray.origin, ray.t_min, ray.direction, ray.t_max, RT_PAYLOAD_EFFECTS);

	if (skip_procedural)
	{
		EffectsResult result = get_payload_transparency(ray_payload_effects);
		fold_lit_sprites(result);
		return result;
	}

	if (!display_fog)
	{
		EffectsResult result = get_payload_transparency_with_fog(ray_payload_effects, ray.t_max);
		fold_lit_sprites(result);
		return result;
	}

	// The primary ray: the fog is a layer over the finished image, blended after the tone mapper as the rasterizer does.
	EffectsResult result = get_payload_transparency(ray_payload_effects);
	blend_fogs(ray_payload_effects, 0, ray.t_max, result.fog);
	return result;
}

Ray get_shadow_ray( vec3 p1, vec3 p2, float tmin )
{
	vec3 l = p2 - p1;
	float dist = length(l);
	l /= dist;

	Ray ray;
	ray.origin = p1 + l * tmin;
	ray.t_min = 0;
	ray.t_max = dist - tmin - 0.01;
	ray.direction = l;

	return ray;
}

float
trace_shadow_ray(Ray ray, int cull_mask)
{
	const uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipProceduralPrimitives;

	ray_payload_geometry.barycentric = vec2(0);
	ray_payload_geometry.primitive_id = ~0u;
	ray_payload_geometry.buffer_and_instance_idx = 0;
	ray_payload_geometry.hit_distance = -1;

	traceRayEXT( topLevelAS[TLAS_INDEX_GEOMETRY], rayFlags, cull_mask,
			SBT_RCHIT_GEOMETRY /*sbtRecordOffset*/, 0 /*sbtRecordStride*/, SBT_RMISS_EMPTY /*missIndex*/,
			ray.origin, ray.t_min, ray.direction, ray.t_max, RT_PAYLOAD_GEOMETRY);

	return found_intersection(ray_payload_geometry) ? 0.0 : 1.0;
}


vec3
trace_caustic_ray( Ray ray, int surface_medium )
{
	ray_payload_geometry.barycentric = vec2(0);
	ray_payload_geometry.primitive_id = ~0u;
	ray_payload_geometry.buffer_and_instance_idx = 0;
	ray_payload_geometry.hit_distance = -1;

	uint rayFlags = gl_RayFlagsCullBackFacingTrianglesEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipProceduralPrimitives;
	uint instance_mask = AS_FLAG_TRANSPARENT;

	traceRayEXT(topLevelAS[TLAS_INDEX_GEOMETRY], rayFlags, instance_mask, SBT_RCHIT_GEOMETRY, 0, SBT_RMISS_EMPTY,
			ray.origin, ray.t_min, ray.direction, ray.t_max, RT_PAYLOAD_GEOMETRY);

	float extinction_distance = ray.t_max - ray.t_min;
	vec3 throughput = vec3(1);

	if(found_intersection(ray_payload_geometry))
	{
		Triangle triangle = get_hit_triangle(ray_payload_geometry);	

		vec3 geo_normal = triangle.normals[0];
		bool is_vertical = abs(geo_normal.z) < 0.1;

#if 0
		if((is_water(triangle.material_id) || is_slime(triangle.material_id)) && !is_vertical)
		{
			vec3 position = ray.origin + ray.direction * ray_payload_geometry.hit_distance;
			vec3 w = get_water_normal(triangle.material_id, geo_normal, triangle.tangents, position, true);

			float caustic = clamp((1 - pow(clamp(1 - length(w.xz), 0, 1), 2)) * 100, 0, 8);
			caustic = mix(1, caustic, clamp(ray_payload_geometry.hit_distance * 0.02, 0, 1));
			throughput = vec3(caustic);

			if(surface_medium != MEDIUM_NONE)
			{
				extinction_distance = ray_payload_geometry.hit_distance;
			}
			else
			{
				if(is_water(triangle.material_id))
					surface_medium = MEDIUM_WATER;
				else
					surface_medium = MEDIUM_SLIME;

				extinction_distance = max(0, ray.t_max - ray_payload_geometry.hit_distance);
			}
		}
		else 
#endif			
		if(is_glass(triangle.material_id) || is_water(triangle.material_id) && is_vertical)
		{
			vec3 bary = get_hit_barycentric(ray_payload_geometry);
			vec2 tex_coord = triangle.tex_coords0 * bary;

			MaterialInfo minfo = get_material_info(triangle.material_id);

	    	vec3 base_color = vec3(minfo.base_factor);
	    	if (minfo.base_texture > 0)
	    		base_color *= global_textureLod(minfo.base_texture, tex_coord, 2).rgb;
	    	base_color = clamp(base_color, vec3(0), vec3(1));

			throughput = base_color;
		}
		else
		{
			throughput = vec3(clamp(1.0 - triangle.alpha, 0.0, 1.0));
		}
	}

	//return vec3(caustic);
	return extinction(surface_medium, extinction_distance) * throughput;
}
#endif

vec3 rgbToNormal(vec3 rgb, out float len)
{
    vec3 n = vec3(rgb.xy * 2 - 1, rgb.z);

    len = length(n);
    return len > 0 ? n / len : vec3(0);
}


float
AdjustRoughnessToksvig(float roughness, float normalMapLen, float mip_level)
{
	float effect = global_ubo.pt_toksvig * clamp(mip_level, 0, 1);
    float shininess = RoughnessSquareToSpecPower(roughness) * effect; // not squaring the roughness here - looks better this way
    float ft = normalMapLen / mix(shininess, 1.0f, normalMapLen);
    ft = max(ft, 0.01f);
    return SpecPowerToRoughnessSquare(ft * shininess / effect);
}


#ifndef NO_TRACE_EXT
float
get_specular_sampled_lighting_weight(float roughness, vec3 N, vec3 V, vec3 L, float pdfw)
{
    float ggxVndfPdf = ImportanceSampleGGX_VNDF_PDF(max(roughness, 0.01), N, V, L);
  
    // Balance heuristic assuming one sample from each strategy: light sampling and BRDF sampling
    return clamp(pdfw / (pdfw + ggxVndfPdf), 0, 1);
}

float get_unshadowed_env_path_contrib(
        vec3 normal,
        vec3 view_direction,
        float phong_exp,
        float phong_scale,
        float phong_weight,
        vec2 rng)
{
    vec3 direction = global_ubo.sun_direction;
    float NoL = dot(direction , normal);
    if(NoL <= 0.0001) return 0.0;

    float specular = phong(normal, direction, view_direction, phong_exp) * phong_scale;
    float m = mix(1.0, specular, phong_weight);

    float light_lum = sun_color_ubo.sun_luminance;// / global_ubo.sun_solid_angle;

    m *= abs(light_lum); // abs because sky lights have negative color

    return m;
}

void
get_direct_illumination(
	vec3 position, 
	vec3 normal, 
	vec3 geo_normal, 
	uint cluster_idx, 
	uint material_id,
	int shadow_cull_mask, 
	vec3 view_direction, 
	vec3 albedo,
	vec3 base_reflectivity,
	float specular_factor,
	float roughness, 
	int surface_medium, 
	bool enable_caustics, 
	float direct_specular_weight, 
	bool enable_polygonal,
	bool enable_dynamic,
	bool is_gradient, 
	int bounce,
	out vec3 diffuse,
	out vec3 specular)
{
	diffuse = vec3(0);
	specular = vec3(0);

	vec3 pos_on_light;

	vec3 contrib = vec3(0);

	float alpha = square(roughness);
	float phong_exp = RoughnessSquareToSpecPower(alpha);
	float phong_scale = min(100, 1 / (M_PI * square(alpha)));
	float phong_weight = clamp(specular_factor * luminance(base_reflectivity) / (luminance(base_reflectivity) + luminance(albedo)), 0, 0.9);

	int light_index = -1;
	float light_pdfw = 0;
	bool polygonal_light_is_sky = false;

	vec3 rng = vec3(
		get_rng(RNG_NEE_LIGHT_SELECTION(bounce)),
		get_rng(RNG_NEE_TRI_X(bounce)),
		get_rng(RNG_NEE_TRI_Y(bounce)));

	/* polygonal light illumination */
	if(enable_polygonal || enable_dynamic)
	{
		sample_lights(
			cluster_idx,
			position, 
			normal, 
			geo_normal, 
			view_direction, 
			phong_exp, 
			phong_scale,
			phong_weight, 
			is_gradient, 
			pos_on_light,
			contrib,
			light_index,
			light_pdfw,
			polygonal_light_is_sky,
			rng);
	}

	bool is_polygonal = true;
	float vis = 1.0;

	float spec_polygonal = phong(normal, normalize(pos_on_light - position), view_direction, phong_exp) * phong_scale;

	float l_polygonal  = luminance(abs(contrib)) * mix(1, spec_polygonal, phong_weight);

	bool null_light = (l_polygonal == 0);

	pos_on_light = null_light ? position : pos_on_light;

	Ray shadow_ray = get_shadow_ray(position - view_direction * 0.01, pos_on_light, 0);
	
	vis *= trace_shadow_ray(shadow_ray, null_light ? 0 : shadow_cull_mask);
#ifdef ENABLE_SHADOW_CAUSTICS
	if(enable_caustics)
	{
		contrib *= trace_caustic_ray(shadow_ray, surface_medium);
	}
#endif

	/* 
		Accumulate light shadowing statistics to guide importance sampling on the next frame.
		Inspired by paper called "Adaptive Shadow Testing for Ray Tracing" by G. Ward, EUROGRAPHICS 1994.

		The algorithm counts the shadowed and unshadowed rays towards each light, per cluster,
		per surface orientation in each cluster. Orientation helps improve accuracy in cases 
		when a single cluster has different parts which have the same light mostly shadowed and 
		mostly unshadowed.

		On the next frame, the light CDF is built using the counts from this frame, or the frame
		before that in case of gradient rays. See light_lists.h for more info.

		Only applies to polygonal polygon lights (i.e. no model or beam lights) because the spherical
		polygon lights do not have polygonal indices, and it would be difficult to map them 
		between frames.
	*/
	
	
	if(global_ubo.pt_light_stats != 0 
		&& !null_light
		&& light_index >= 0
		&& light_index < global_ubo.num_static_lights)
	{
		uint addr = get_light_stats_addr(cluster_idx, light_index, get_primary_direction(normal));

		// Offset 0 is unshadowed rays,
		// Offset 1 is shadowed rays
		if(vis == 0) addr += 1;

		// Increment the ray counter
		atomicAdd(light_stats_bufers[global_ubo.current_frame_idx % NUM_LIGHT_STATS_BUFFERS].stats[addr], 1);
	}

	if(null_light)
		return;

	vec3 radiance = vis * contrib;

	vec3 L = pos_on_light - position;
	L = normalize(L);

	if(direct_specular_weight > 0 && polygonal_light_is_sky && global_ubo.pt_specular_mis != 0)
	{
		// MIS with direct specular and indirect specular.
		// Only applied to sky lights, for two reasons:
		//  1) Non-sky lights are trimmed to match the light texture, and indirect rays don't see that;
		//  2) Non-sky lights are usually away from walls, so the direct sampling issue is not as pronounced.

		direct_specular_weight *= get_specular_sampled_lighting_weight(roughness,
			normal, -view_direction, L, light_pdfw);
	}

	vec3 F = vec3(0);

	if(vis > 0 && direct_specular_weight > 0)
	{
		vec3 specular_brdf = GGX_times_NdotL(view_direction, normalize(pos_on_light - position),
			normal, roughness, base_reflectivity, 0.0, specular_factor, F);
		specular = radiance * specular_brdf * direct_specular_weight;
	}

	float NdotL = max(0, dot(normal, L));

	float diffuse_brdf = NdotL / M_PI;
	diffuse = radiance * diffuse_brdf * (vec3(1.0) - F);
}

void
get_sunlight(
	uint cluster_idx, 
	uint material_id,
	vec3 position, 
	vec3 normal, 
	vec3 geo_normal, 
	vec3 view_direction, 
	vec3 base_reflectivity,
	float specular_factor,
	float roughness, 
	int surface_medium, 
	bool enable_caustics, 
	out vec3 diffuse, 
	out vec3 specular, 
	int shadow_cull_mask)
{
	diffuse = vec3(0);
	specular = vec3(0);

	if(global_ubo.sun_visible == 0 || global_ubo.sky_diffuse >= 1.0)
		return;

	// The cloud layer widens the sun disk (the shadows soften) and removes the share that sky_diffuse sets.
	float sun_tan_half_angle = mix(global_ubo.sun_tan_half_angle, SKY_DIFFUSE_TAN_HALF_ANGLE,
		global_ubo.sky_diffuse * global_ubo.sky_diffuse);

#if 1
	//bool visible = (cluster_idx == ~0u) || (get_sky_visibility(cluster_idx >> 5) & (1 << (cluster_idx & 31))) != 0;
	bool visible = (cluster_idx == ~0u) || (light_buffer.sky_visibility[cluster_idx >> 5] & (1 << (cluster_idx & 31))) != 0;
	if(!visible)
		return;
#else
	bool visible = true;
#endif

	vec2 rng3 = vec2(get_rng(RNG_SUNLIGHT_X(0)), get_rng(RNG_SUNLIGHT_Y(0)));
	vec2 disk = sample_disk(rng3);
	disk.xy *= sun_tan_half_angle;

	vec3 direction = normalize(global_ubo.sun_direction + global_ubo.sun_tangent * disk.x + global_ubo.sun_bitangent * disk.y);

	float NdotL = dot(direction, normal);
	float GNdotL = dot(direction, geo_normal);

	if(NdotL <= 0 || GNdotL <= 0)
		return;

	Ray shadow_ray = get_shadow_ray(position - view_direction * 0.01, position + direction * 10000, 0);
 
	float vis = trace_shadow_ray(shadow_ray, shadow_cull_mask);

	if(vis == 0)
		return;

#ifdef ENABLE_SUN_SHAPE
	// Fetch the sun color from the environment map. 
	// This allows us to get properly shaped shadows from the sun that is partially occluded
	// by clouds or landscape.

	vec3 envmap_direction = (global_ubo.environment_rotation_matrix * vec4(direction, 0)).xyz;
	
    vec3 envmap = textureLod(TEX_PHYSICAL_SKY, envmap_direction.xzy, 0).rgb;

    vec3 radiance = (global_ubo.sun_solid_angle * global_ubo.pt_env_scale) * envmap;

	// A widened disk lies outside the shape of the sun: the average colour takes over.
	radiance = mix(radiance, sun_color_ubo.sun_color, global_ubo.sky_diffuse);
#else
    // Fetch the average sun color from the resolved UBO - it's faster.

    vec3 radiance = sun_color_ubo.sun_color;
#endif

	radiance *= 1.0 - global_ubo.sky_diffuse;

#ifdef ENABLE_SHADOW_CAUSTICS
	if(enable_caustics)
	{
    	radiance *= trace_caustic_ray(shadow_ray, surface_medium);
	}
#endif

	vec3 F = vec3(0);

    if(global_ubo.pt_sun_specular > 0)
    {
		float NoH_offset = 0.5 * square(sun_tan_half_angle);
		vec3 specular_brdf = GGX_times_NdotL(view_direction, global_ubo.sun_direction,
			normal,roughness, base_reflectivity, NoH_offset, specular_factor, F);
    	specular = radiance * specular_brdf;
	}

	float diffuse_brdf = NdotL / M_PI;
	diffuse = radiance * diffuse_brdf * (vec3(1.0) - F);
}
#endif

vec3 clamp_output(vec3 c)
{
	if(any(isnan(c)) || any(isinf(c)))
		return vec3(0);
	else 
		return clamp(c, vec3(0), vec3(MAX_OUTPUT_VALUE));
}

vec3
sample_emissive_texture(uint material_id, MaterialInfo minfo, vec2 tex_coord, vec2 tex_coord_x, vec2 tex_coord_y, float mip_level)
{
	if (minfo.emissive_texture != 0)
    {
        vec4 image3;
	    if (mip_level >= 0)
	        image3 = global_textureLod(minfo.emissive_texture, tex_coord, mip_level);
	    else
	        image3 = global_textureGrad(minfo.emissive_texture, tex_coord, tex_coord_x, tex_coord_y);

    	vec3 corrected = correct_emissive(material_id, image3.rgb);

	    return corrected * minfo.emissive_factor;
	}

	return vec3(0);
}

vec3 get_emissive_shell(uint material_id, uint shell)
{
	vec3 c = vec3(0);

#if 0
	if((shell & SHELL_MASK) != 0)
	{ 
		if ((shell & SHELL_HALF_DAM) != 0)
		{
			c.r = 0.56f;
			c.g = 0.59f;
			c.b = 0.45f;
		}
		if ((shell & SHELL_DOUBLE) != 0)
		{
			c.r = 0.9f;
			c.g = 0.7f;
		}
		if ((shell & SHELL_LITE_GREEN) != 0)
		{
			c.r = 0.7f;
			c.g = 1.0f;
			c.b = 0.7f;
		}
	    if((shell & SHELL_RED) != 0) c.r += 1;
	    if((shell & SHELL_GREEN) != 0) c.g += 1;
	    if((shell & SHELL_BLUE) != 0) c.b += 1;

	    if((material_id & MATERIAL_FLAG_WEAPON) != 0) c *= 0.2;
	}
#endif

	if(tonemap_buffer.adapted_luminance > 0)
			c.rgb *= tonemap_buffer.adapted_luminance * 100;

    return c;
}

bool get_is_gradient(ivec2 ipos)
{
	if((global_ubo.pt_denoiser_flags & DENOISER_FLAG_GRADIENTS) != 0)
	{
		uint u = texelFetch(TEX_ASVGF_GRAD_SMPL_POS_A, ipos / GRAD_DWN, 0).r;

		ivec2 grad_strata_pos = ivec2(
				u >> (STRATUM_OFFSET_SHIFT * 0),
				u >> (STRATUM_OFFSET_SHIFT * 1)) & STRATUM_OFFSET_MASK;

		return (u > 0 && all(equal(grad_strata_pos, ipos % GRAD_DWN)));
	}
	
	return false;
}

// ========================================================================== //
// Shading of a surface that a bounce ray hit. The bounce pass and the radiance
// cache update pass share these functions. The caller chooses the normals.
// ========================================================================== //

vec3
get_bounce_base_color(MaterialInfo minfo, vec2 tex_coord)
{
	vec3 base_color = vec3(minfo.base_factor);
	if (minfo.base_texture != 0)
		base_color *= global_textureLod(minfo.base_texture, tex_coord, 2).rgb;

	return clamp(base_color, vec3(0), vec3(1));
}

// The emission of the surface, before the spotlight term.
vec3
get_bounce_emissive(Triangle triangle, MaterialInfo minfo, vec2 tex_coord, vec3 base_color, float mip_level)
{
	vec3 emissive = sample_emissive_texture(triangle.material_id, minfo, tex_coord, vec2(0), vec2(0), mip_level);
	emissive *= triangle.emissive_factor;
	emissive += get_emissive_shell(triangle.material_id, triangle.shell) * base_color;

	return emissive;
}

// Matches the spotlight term of sample_light_lists() in light_lists.h.
float
get_bounce_spotlight(vec3 direction, vec3 normal)
{
	return sqrt(max(0, -dot(direction, normal)));
}

// Analytic lights are processed by NEE: the bounce ray must not add their emission again.
// bounce_index 0: the direct lighting pass has sampled them, see `pt_direct_polygon_lights`.
// bounce_index 1: the pass of the first bounce has sampled them, see `pt_indirect_polygon_lights`.
bool
is_analytic_light_hit(uint material_id, int bounce_index)
{
	return (material_id & MATERIAL_FLAG_LIGHT) != 0 &&
		((bounce_index == 0) && (global_ubo.pt_direct_polygon_lights >= 0) ||
		 (bounce_index == 1) && (global_ubo.pt_indirect_polygon_lights >= 0));
}

// Diffuse NEE at a bounce hit, with the settings of bounce 1.
void
get_bounce_direct_lighting(
	vec3 position,
	vec3 normal,
	vec3 geo_normal,
	uint cluster_idx,
	uint material_id,
	int shadow_cull_mask,
	vec3 direction,
	vec3 base_color,
	bool is_gradient,
	int rng_bounce,
	out vec3 diffuse)
{
	vec3 specular;
	get_direct_illumination(
		position,
		normal,
		geo_normal,
		cluster_idx,
		material_id,
		shadow_cull_mask,
		direction,
		base_color,
		vec3(0), // base_reflectivity
		0.0, // specular_factor
		1.0, // roughness
		MEDIUM_NONE,
		false, // enable_caustics
		0.0, // direct_specular_weight
		global_ubo.pt_indirect_polygon_lights > 0,
		global_ubo.pt_indirect_dyn_lights > 0,
		is_gradient,
		rng_bounce,
		diffuse,
		specular);
}

// Diffuse sun light at a bounce hit.
void
get_bounce_sun_lighting(
	vec3 position,
	vec3 normal,
	vec3 geo_normal,
	uint cluster_idx,
	uint material_id,
	int shadow_cull_mask,
	vec3 direction,
	out vec3 diffuse)
{
	vec3 specular;
	get_sunlight(
		cluster_idx,
		material_id,
		position,
		normal,
		geo_normal,
		direction,
		vec3(0), // base_reflectivity
		0.0, // specular_factor
		1.0, // roughness
		MEDIUM_NONE,
		false, // enable_caustics
		diffuse,
		specular,
		shadow_cull_mask);
}

vec4 unpack_rgba8(uint p)
{
    return vec4(
        float((p >>  0) & 255u),
        float((p >>  8) & 255u),
        float((p >> 16) & 255u),
        float((p >> 24) & 255u)
    ) * (1.0 / 255.0);
}

// The rgbGen of the bundle, or the one the entity forces.
uint bundle_rgb_gen( in uint instance_index, in MaterialStage stage, in uint bundle )
{
	uint forceRGBGen = (instance_index != ~0u) ? (get_model_instance_shader_uint(instance_index, 1) & 0xffu) : 0u;

	return (forceRGBGen != 0u) ? forceRGBGen : stage.bundle[bundle].rgbGen;
}

// CGEN_DISINTEGRATION_1 and 2: the colour factor of RB_CalcDisintegrateColors / gen_vert.tmpl at a point of the entity.
vec4 disintegration_color( in uint instance_index, in vec3 position, in uint rgbGen )
{
	if ( instance_index == ~0u || ( rgbGen != 14u && rgbGen != 15u ) )
		return vec4(1.0);

	float d = disintegration_distance( instance_index, position );

	if ( d < 0.0 )
		return vec4(0.0);

	if ( rgbGen == 15u )
		return vec4(1.0);

	if ( d < 60.0 )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	if ( d < 150.0 )
		return vec4( vec3( 0.435295 ), 1.0 );
	if ( d < 180.0 )
		return vec4( vec3( 0.6862745 ), 1.0 );

	return vec4(1.0);
}

vec4 calc_color( in uint instance_index, in MaterialStage stage, in uint bundle )
{
	vec4 base_color = unpack_rgba8(stage.bundle[bundle].color);
	vec4 shaderRGBA = vec4(0.0);

	if (instance_index != ~0u)
		shaderRGBA = unpack_rgba8(get_model_instance_shader_uint(instance_index, 0));

	uint rgbGen = bundle_rgb_gen(instance_index, stage, bundle);

	// CGEN_ENTITY || CGEN_LIGHTING_DIFFUSE_ENTITY
	if (rgbGen == 3 || rgbGen == 10)
	{
		base_color = shaderRGBA;
	}

	return base_color;
}

// The tcMods of the bundle move the stage UV.
vec4 sample_bundle(MaterialBundle bundle, vec3 position, vec2 uv, vec2 uvx, vec2 uvy, float mip)
{
	mat2 m = mat2(bundle.tc_matrix.xy, bundle.tc_matrix.zw);
	uv = m * uv + bundle.tc_offset.xy;

	// tcMod turb, as ModTexCoords in gen_vert.tmpl
	if (bundle.tc_offset.z != 0.0)
		uv += sin(vec2(position.x + position.z, position.y) * (2.0 * M_PI / 1024.0) + vec2(bundle.tc_offset.w * 2.0 * M_PI)) * bundle.tc_offset.z;

	if (mip >= 0.0)
		return global_textureLod(bundle.image, uv, mip);

	return global_textureGrad(bundle.image, uv, m * uvx, m * uvy);
}

// One bundle onto the bundles before it, with the multitexture mode of the stage.
vec4 combine_bundle(vec4 acc, vec4 c, uint tex_mode)
{
	switch (tex_mode)
	{
		default:
		case 0u: // MODULATE
			return acc * c;
		case 1u: // ADD_IDENTITY
		case 2u: // ADD
		case 3u: // ALPHA
		case 4u: // ONE_MINUS_ALPHA
			return vec4(acc.rgb + c.rgb, acc.a * c.a);
		case 5u: // MIX_ALPHA
			return mix(acc, c, c.a);
		case 6u: // MIX_ONE_MINUS_ALPHA
			return mix(c, acc, c.a);
		case 7u: // DST_COLOR_SRC_ALPHA
			return (c + c.a) * acc;
	}
}

// What a stage needs to know about the hit point.
struct StageContext
{
	uint	instance_index;
	vec3	position;
	vec3	normal;
	vec4	vertex_color[MAX_RTX_STAGES];	// bundle 0 of each stage (BSP surfaces only)
	bool	vertex_colors;
	vec2	uv;								// the texture coordinates of the surface
	vec2	uv_x, uv_y;						// their gradients
	vec3	footprint_x, footprint_y;		// the pixel on the surface, in world units
	float	mip;							// < 0: sample with the gradients
};

StageContext stage_context( uint instance_index, Triangle triangle, vec3 bary, vec3 normal,
	vec2 uv, vec2 uv_x, vec2 uv_y, vec3 footprint_x, vec3 footprint_y, float mip )
{
	StageContext ctx;
	ctx.instance_index = instance_index;
	ctx.position = triangle.positions * bary;
	ctx.normal = normal;
	ctx.uv = uv;
	ctx.uv_x = uv_x;
	ctx.uv_y = uv_y;
	ctx.footprint_x = footprint_x;
	ctx.footprint_y = footprint_y;
	ctx.mip = mip;
	ctx.vertex_color[0] = triangle.color0 * bary;
	ctx.vertex_color[1] = triangle.color1 * bary;
	ctx.vertex_color[2] = triangle.color2 * bary;
	ctx.vertex_color[3] = triangle.color3 * bary;
	ctx.vertex_colors = triangle.vertex_colors;
	return ctx;
}

// RB_CalcSpecularAlpha: the light reflected to the viewer, to the 4th power. A model reflects
// the light of its entity, the world a fixed light.
float specular_alpha( StageContext ctx )
{
	vec3 light_dir = ( ctx.instance_index != ~0u )
		? decode_normal( get_model_instance_shader_uint( ctx.instance_index, 2 ) )
		: normalize( vec3( -960.0, 1980.0, 96.0 ) - ctx.position );

	vec3 reflected = ctx.normal * ( 2.0 * dot( ctx.normal, light_dir ) ) - light_dir;
	float l = dot( reflected, normalize( global_ubo.cam_pos.xyz - ctx.position ) );

	if ( l < 0.0 )
		return 0.0;

	l *= l;
	return min( l * l, 1.0 );
}

// The alphaGen that depend on the vertex, the entity or the view. vk_rtx_bundle_color leaves
// them at 1.
float bundle_alpha( StageContext ctx, uint s, MaterialStage stage, uint b, float a )
{
	float entity_alpha = ( ctx.instance_index != ~0u ) ? unpack_rgba8( get_model_instance_shader_uint( ctx.instance_index, 0 ) ).a : 1.0;

	switch ( stage.bundle[b].alphaGen & 0xffu )
	{
		case 2u: return entity_alpha;										// AGEN_ENTITY
		case 3u: return 1.0 - entity_alpha;									// AGEN_ONE_MINUS_ENTITY
		case 4u: return ctx.vertex_colors ? ctx.vertex_color[s].a : a;		// AGEN_VERTEX
		case 5u: return ctx.vertex_colors ? 1.0 - ctx.vertex_color[s].a : a;	// AGEN_ONE_MINUS_VERTEX
		case 6u: return specular_alpha( ctx );								// AGEN_LIGHTING_SPECULAR
		case 8u: return clamp( length( ctx.position - global_ubo.cam_pos.xyz ) * stage.portal_range_r, 0.0, 1.0 );	// AGEN_PORTAL
	}
	return a;
}

// RB_CalcEnvironmentTexCoords of vanilla at the point p, in the space of the model as the
// rasterizer computes it. A first person model reflects the light of its entity instead of the
// view (word 1 of the instance data, bit 8).
vec2 environment_uv( StageContext ctx, vec3 p )
{
	vec3 normal = ctx.normal;
	vec3 viewer = normalize( global_ubo.cam_pos.xyz - p );

	if ( ctx.instance_index != ~0u )
	{
		mat3 to_model = transpose( mat3( instance_buffer.model_instances[ctx.instance_index].transform ) );
		normal = normalize( to_model * normal );

		if ( ( get_model_instance_shader_uint( ctx.instance_index, 1 ) & 0x100u ) != 0u )
		{
			vec3 light_dir = decode_normal( get_model_instance_shader_uint( ctx.instance_index, 2 ) );
			float dl = dot( normal, light_dir );
			return normal.xy * dl - light_dir.xy;
		}

		viewer = normalize( to_model * viewer );
	}

	float d = dot( normal, viewer );
	return normal.xy * d - 0.5 * viewer.xy;
}

// The texture coordinates of a bundle from its tcGen, as ComputeTexCoords, and their
// gradients over the pixel. The tcMods come after, in sample_bundle.
vec2 bundle_uv( StageContext ctx, MaterialBundle bundle, out vec2 uv_x, out vec2 uv_y )
{
	switch ( uint( bundle.tc_gen_s.w ) )
	{
		case 1u:	// TCGEN_IDENTITY
			uv_x = uv_y = vec2( 0.0 );
			return vec2( 0.0 );

		case 7u:	// TCGEN_ENVIRONMENT_MAPPED
		{
			vec2 uv = environment_uv( ctx, ctx.position );
			uv_x = environment_uv( ctx, ctx.position + ctx.footprint_x ) - uv;
			uv_y = environment_uv( ctx, ctx.position + ctx.footprint_y ) - uv;
			return uv;
		}

		case 9u:	// TCGEN_VECTOR
			uv_x = vec2( dot( ctx.footprint_x, bundle.tc_gen_s.xyz ), dot( ctx.footprint_x, bundle.tc_gen_t.xyz ) );
			uv_y = vec2( dot( ctx.footprint_y, bundle.tc_gen_s.xyz ), dot( ctx.footprint_y, bundle.tc_gen_t.xyz ) );
			return vec2( dot( ctx.position, bundle.tc_gen_s.xyz ), dot( ctx.position, bundle.tc_gen_t.xyz ) );
	}

	// TCGEN_TEXTURE. A lightmap bundle is not sampled.
	uv_x = ctx.uv_x;
	uv_y = ctx.uv_y;
	return ctx.uv;
}

// The light of the rasterizer for stage s of a blended surface, which the tracer does not
// light: the vertex light of a BSP surface (rgbGen exactVertex under RTX), else 1.
vec3 raster_light( StageContext ctx, uint s )
{
	return ctx.vertex_colors ? ctx.vertex_color[s].rgb : vec3( 1.0 );
}

// A stage as the rasterizer draws it, with its light at 0 (src0) and at 1 (src1). The light is
// a lightmap bundle, or the vertex colour or diffuse light of the rgbGen: under RTX the world
// is vertex lit, so the rgbGen of the lit texture is exactVertex. glow is the glow pass.
// A blended model (an absorb shell) is what the rasterizer blends on the gamma values of the texture: the stages
// of a layer are sampled as those, not as the linear values of the tracer.
bool layer_raster_blend = false;

// The alpha of an RF_ALPHA_FADE entity while its layer is sampled, else < 0.
float layer_fade_alpha = -1.0;

void sample_material_stage_light(
	StageContext ctx,
	uint s,
	MaterialStage stage,
	out vec4 src0,
	out vec4 src1,
	out vec4 glow)
{
	src0 = vec4(1.0);
	src1 = vec4(1.0);
	glow = vec4(0.0);
	bool first = true;
	bool glow_first = true;

	for (uint b = 0u; b <= min(stage.tex_count, 2u); b++)
	{
		vec4 c0, c1;

		if ((stage.bundle[b].alphaGen & BUNDLE_SKIP) != 0u)
		{
			c0 = vec4(0.0, 0.0, 0.0, 1.0);
			c1 = vec4(1.0);
		}
		else
		{
			vec4 c = calc_color(ctx.instance_index, stage, b);
			c *= disintegration_color(ctx.instance_index, ctx.position, bundle_rgb_gen(ctx.instance_index, stage, b));
			c.a = bundle_alpha(ctx, s, stage, b, c.a);
				if (layer_fade_alpha >= 0.0)
					c.a = layer_fade_alpha;
			if (stage.bundle[b].image != 0u)
			{
				vec2 uv_x, uv_y;
				vec2 uv = bundle_uv(ctx, stage.bundle[b], uv_x, uv_y);
				vec4 smp = sample_bundle(stage.bundle[b], ctx.position, uv, uv_x, uv_y, ctx.mip);
				if (layer_raster_blend)
					smp.rgb = pow(smp.rgb, vec3(1.0 / 2.2));
				c *= smp;
			}

			if (stage.tex_mode == 3u)			// ALPHA
				c.rgb *= c.a;
			else if (stage.tex_mode == 4u)		// ONE_MINUS_ALPHA
				c.rgb *= 1.0 - c.a;

			// CGEN_EXACT_VERTEX, CGEN_VERTEX, CGEN_LIGHTING_DIFFUSE(_ENTITY): the colour times the
			// light. CGEN_ONE_MINUS_VERTEX: the colour times 1 - the light.
			uint gen = bundle_rgb_gen(ctx.instance_index, stage, b);
			vec4 unlit = vec4(0.0, 0.0, 0.0, c.a);
			bool lit = gen == 5u || gen == 6u || gen == 9u || gen == 10u;
			c0 = lit ? unlit : c;
			c1 = (gen == 7u) ? unlit : c;

			vec4 g = ((stage.bundle[b].alphaGen & BUNDLE_GLOW) != 0u) ? c : unlit;
			glow = glow_first ? g : combine_bundle(glow, g, stage.tex_mode);
			glow_first = false;
		}

		if (first)
		{
			src0 = c0;
			src1 = c1;
			first = false;
			continue;
		}

		src0 = combine_bundle(src0, c0, stage.tex_mode);
		src1 = combine_bundle(src1, c1, stage.tex_mode);
	}
}

// GL blend factor of a GLS_SRCBLEND_* (src = true) or GLS_DSTBLEND_* value, shifted to 1-9.
vec4 blend_factor( uint f, bool src, vec4 s, vec4 d )
{
	switch ( f )
	{
		case 1u: return vec4( 0.0 );
		case 2u: return vec4( 1.0 );
		case 3u: return src ? d : s;
		case 4u: return src ? 1.0 - d : 1.0 - s;
		case 5u: return vec4( s.a );
		case 6u: return vec4( 1.0 - s.a );
		case 7u: return vec4( d.a );
		case 8u: return vec4( 1.0 - d.a );
		case 9u: return vec4( vec3( min( s.a, 1.0 - d.a ) ), 1.0 );
	}
	return vec4( 1.0 );
}

// The stages blended the way the rasterizer blends them, with the light of the rasterizer at 0
// and at 1. The result is L * A + B: with L = 0 it is B, what the rasterizer draws without
// light (glow and additive stages, fullbright stages), and with L = 1 it is A + B. A is the
// albedo the tracer lights and B the emission, in screen units. A lightmap stage is the light
// with its blendFunc, except x2 (GL_DST_COLOR GL_SRC_COLOR), which undoes the overbright shift
// of the GL lightmap and only multiplies by the light. The glow is the glow pass of the
// rasterizer: the same blends, with the bundles that do not glow drawn in black.
vec4 compose_material_stages(
	StageContext ctx,
	MaterialInfo minfo,
	out vec3 glow,
	out vec3 emission )
{
	vec4 dst0 = vec4( 1.0 );
	vec4 dst1 = vec4( 1.0 );
	vec4 glow_dst = vec4( 0.0 );
	bool first = true;
	bool glow_first = true;

	for ( uint s = 0u; s < MAX_RTX_STAGES; s++ )
	{
		MaterialStage stage = minfo.stage[s];

		if ( ( stage.blend & STAGE_BLEND_ACTIVE ) == 0u )
			break;

		uint sf = stage.blend & 0x0fu;
		uint df = ( stage.blend >> 4 ) & 0x0fu;
		bool no_blend = sf == 0u && df == 0u;

		if ( ( stage.blend & STAGE_BLEND_LIGHTMAP ) != 0u )
		{
			const vec4 light0 = vec4( 0.0, 0.0, 0.0, 1.0 );
			const vec4 light1 = vec4( 1.0 );

			if ( first || no_blend )
			{
				dst0 = light0;
				dst1 = light1;
			}
			else if ( sf == 3u && df == 3u )
				dst0.rgb = vec3( 0.0 );
			else
			{
				dst0 = light0 * blend_factor( sf, true, light0, dst0 ) + dst0 * blend_factor( df, false, light0, dst0 );
				dst1 = light1 * blend_factor( sf, true, light1, dst1 ) + dst1 * blend_factor( df, false, light1, dst1 );
			}
			first = false;
			continue;
		}

		vec4 src0, src1, glow_src;
		sample_material_stage_light( ctx, s, stage, src0, src1, glow_src );

		if ( glow_first || no_blend )
			glow_dst = glow_src;
		else
			glow_dst = glow_src * blend_factor( sf, true, glow_src, glow_dst ) + glow_dst * blend_factor( df, false, glow_src, glow_dst );
		glow_first = false;

		// The first stage, or a stage without blendFunc, replaces the color.
		if ( first || no_blend )
		{
			dst0 = src0;
			dst1 = src1;
		}
		else
		{
			dst0 = src0 * blend_factor( sf, true, src0, dst0 ) + dst0 * blend_factor( df, false, src0, dst0 );
			dst1 = src1 * blend_factor( sf, true, src1, dst1 ) + dst1 * blend_factor( df, false, src1, dst1 );
		}
		first = false;
	}

	glow = clamp( glow_dst.rgb, vec3( 0.0 ), vec3( 1.0 ) );
	emission = max( dst0.rgb, vec3( 0.0 ) );
	return vec4( clamp( dst1.rgb - dst0.rgb, vec3( 0.0 ), vec3( 1.0 ) ), clamp( dst1.a, 0.0, 1.0 ) );
}

// A surface seen from a side its shader culls (cull front, the default; cull back). The side is the one of the
// vertex normals, not the winding: the tracer does not know the winding of a model.
bool model_side_culled( uint instance_index, Triangle triangle, vec3 bary, vec3 direction )
{
	// A world surface culls its back side. A brush model is not a mesh: it keeps both sides.
	const uint flags = ( instance_index != ~0u ) ? get_model_instance_shader_uint( instance_index, 1 ) : 0x200u;

	if ( ( flags & ( 0x200u | INSTANCE_CULL_NONE ) ) != 0x200u )
		return false;

	vec3 n = cross( triangle.positions[1] - triangle.positions[0], triangle.positions[2] - triangle.positions[1] );
	const vec3 vn = triangle.normals * bary;

	if ( dot( n, vn ) < 0.0 )
		n = -n;

	const bool faces_away = dot( n, direction ) > 0.0;

	return ( flags & INSTANCE_CULL_FRONT ) != 0u ? !faces_away : faces_away;
}

// A surface blended onto the framebuffer: stage 0 blends and the alpha test is off. The
// rasterizer draws it over what is behind, so the primary ray goes through it.
bool is_blended_surface( uint material_id, uint instance_index )
{
	uint kind = material_id & MATERIAL_KIND_MASK;
	if ( kind != 0u && kind != MATERIAL_KIND_REGULAR )
		return false;

	// A model of an RF_ALPHA_FADE entity blends whatever its shader says.
	if ( instance_index != ~0u && ( get_model_instance_shader_uint( instance_index, 1 ) & INSTANCE_ALPHA_FADE ) != 0u )
		return true;

	MaterialInfo minfo = get_material_info( material_id );

	return minfo.blend_mode != RTX_BLEND_OPAQUE;
}

// The alphaFunc of stage 0, as the rasterizer discards the fragment.
bool layer_alpha_test_pass( MaterialInfo minfo, float a )
{
	switch ( minfo.alpha_test_func )
	{
		case 1u: return a > 0.0;						// GLS_ATEST_GT_0
		case 2u: return a < minfo.alpha_test_value;		// GLS_ATEST_LT_80
		case 3u: return a >= minfo.alpha_test_value;	// GLS_ATEST_GE_80 / GLS_ATEST_GE_C0
	}
	return true;
}

// The stages of a blended surface, in screen units: out = L + T * behind. The tracer does not
// light the layer: its stages take the light of the rasterizer (raster_light).
void blended_surface_layer(
	uint instance_index,
	Triangle triangle,
	vec3 bary,
	out vec3 L,
	out vec3 T )
{
	MaterialInfo minfo = get_material_info( triangle.material_id );
	StageContext ctx = stage_context( instance_index, triangle, bary, normalize( triangle.normals * bary ),
		triangle.tex_coords0 * bary, vec2( 0.0 ), vec2( 0.0 ), vec3( 0.0 ), vec3( 0.0 ), 0.0 );

	L = vec3( 0.0 );
	T = vec3( 1.0 );
	layer_raster_blend = ( instance_index != ~0u ) && ( ( get_model_instance_shader_uint( instance_index, 1 ) & 0x200u ) != 0u );

	// RF_ALPHA_FADE: every stage blends with the entity alpha (SRC_ALPHA, ONE_MINUS_SRC_ALPHA) and the light of the entity.
	const bool fade = ( instance_index != ~0u ) && ( ( get_model_instance_shader_uint( instance_index, 1 ) & INSTANCE_ALPHA_FADE ) != 0u );
	vec3 fade_light = vec3( 1.0 );

	if ( fade )
	{
		const vec4 packed_light = unpack_rgba8( get_model_instance_shader_uint( instance_index, 0 ) );
		const vec3 ambient = unpack_rgba8( get_model_instance_shader_uint( instance_index, RTX_DISTORT_FIRST ) ).rgb;
		const vec3 directed = unpack_rgba8( get_model_instance_shader_uint( instance_index, RTX_DISTORT_FIRST + 1u ) ).rgb;
		const vec3 light_dir = decode_normal( get_model_instance_shader_uint( instance_index, 2 ) );

		layer_fade_alpha = packed_light.a;
		fade_light = min( ambient + directed * max( dot( ctx.normal, light_dir ), 0.0 ), vec3( 1.0 ) );
	}

	for ( uint s = 0u; s < MAX_RTX_STAGES; s++ )
	{
		MaterialStage stage = minfo.stage[s];

		if ( ( stage.blend & STAGE_BLEND_ACTIVE ) == 0u )
			break;

		vec3 light = fade ? fade_light : raster_light( ctx, s );

		if ( ( stage.blend & STAGE_BLEND_LIGHTMAP ) != 0u )
		{
			// x2 only multiplies by the light, as in compose_material_stages.
			if ( ( stage.blend & 0xffu ) == 0x33u )
			{
				L *= light;
				T *= light;
			}
			else
				blend_stage_layer( stage.blend, vec4( light, 1.0 ), L, T );
			continue;
		}

		vec4 src0, src1, glow_src;
		sample_material_stage_light( ctx, s, stage, src0, src1, glow_src );
		if ( s == 0u && !layer_alpha_test_pass( minfo, src1.a ) )
			continue;
		blend_stage_layer( fade ? 0x65u : stage.blend, vec4( src0.rgb + light * ( src1.rgb - src0.rgb ), src1.a ), L, T );
	}

	layer_raster_blend = false;
	layer_fade_alpha = -1.0;

	// What the layer adds is drawn without the light of the tracer: pt_glow_scale, as the
	// emission of the opaque surfaces.
	// T above 1 brightens what is behind (blendFunc GL_DST_COLOR GL_ONE, the cloak): the alpha goes below 0.
	// The rasterizer multiplies gamma values, so the gain on linear light is T^2.2.
	T = clamp( T, vec3( 0.0 ), vec3( 2.0 ) );
	T = mix( T, pow( T, vec3( 2.2 ) ), greaterThan( T, vec3( 1.0 ) ) );
	L = max( L, vec3( 0.0 ) ) * minfo.emission_scale;
}

// The surfaces drawn without the depth test (RF_NODEPTH, the force sight shell) along the whole ray, as blended layers:
// the walls do not hide them. They are the distortion instances flagged INSTANCE_NODEPTH.
void trace_nodepth_layers( Ray ray, inout vec3 layer_L, inout vec3 layer_T )
{
	if ( global_ubo.distortion_surfaces == 0 )
		return;

	// The payload of the primary ray stays for the caller.
	const RayPayloadGeometry primary_payload = ray_payload_geometry;

	ray.t_max = PRIMARY_RAY_T_MAX;

	for ( int hit = 0; hit < MAX_DISTORTION_HITS; hit++ )
	{
		trace_geometry_ray( ray, true, AS_FLAG_DISTORTION );

		if ( !found_intersection( ray_payload_geometry ) )
			break;

		ray.t_min = ray_payload_geometry.hit_distance + 0.01;

		const uint instance = get_instance_index( ray_payload_geometry );

		if ( ( get_model_instance_shader_uint( instance, 1 ) & INSTANCE_NODEPTH ) == 0u )
			continue;

		if ( model_side_culled( instance, get_hit_triangle( ray_payload_geometry ), get_hit_barycentric( ray_payload_geometry ), ray.direction ) )
			continue;

		vec3 L, T;
		blended_surface_layer( instance, get_hit_triangle( ray_payload_geometry ), get_hit_barycentric( ray_payload_geometry ), L, T );

		layer_L += layer_T * L;
		layer_T *= T;
	}

	ray_payload_geometry = primary_payload;
}

// Screen units (what the tone mapper shows as 1) to HDR units, at the current exposure.
float screen_to_hdr()
{
	if ( global_ubo.prev_adapted_luminance <= 0.0 )
		return 0.0;

	return global_ubo.prev_adapted_luminance / exp2( global_ubo.tm_exposure_bias - 2.0 );
}

// The blended layers in front of the primary surface (L, T in HDR units) over the
// transparent image, which is premultiplied and goes over the lit surface.
vec4 apply_blended_layers( vec4 transparent, vec3 L, vec3 T )
{
	float t = dot( T, vec3( 1.0 / 3.0 ) );

	transparent.rgb = L + T * transparent.rgb;
	transparent.a = 1.0 - t * ( 1.0 - transparent.a );

	return transparent;
}

// A float of the distortion words of an instance (see fill_model_instance_distortion).
float distortion_word( uint instance, uint n )
{
	return uintBitsToFloat( get_model_instance_shader_uint( instance, RTX_DISTORT_FIRST + n ) );
}

// The screen texture coordinate of a force push surface at a point: the stage texture coordinate of
// the point through the tcMods and the crop of the screen, as refraction.tmpl does for r_distortionStyle 1.
vec2 distortion_crop_uv( uint instance, Triangle triangle, vec3 bary )
{
	const mat2 m = mat2( distortion_word( instance, 0u ), distortion_word( instance, 1u ), distortion_word( instance, 2u ), distortion_word( instance, 3u ) );
	vec2 uv = m * ( triangle.tex_coords0 * bary ) + vec2( distortion_word( instance, 4u ), distortion_word( instance, 5u ) );

	// tcMod turb of the vertex shader: the position is in the space of the model.
	const float amplitude = distortion_word( instance, 6u );

	if ( amplitude != 0.0 )
	{
		const mat4 transform = instance_buffer.model_instances[instance].transform;
		const vec3 p = inverse( mat3( transform ) ) * ( triangle.positions * bary - transform[3].xyz );

		uv += sin( vec2( p.x + p.z, p.y ) * ( 2.0 * M_PI / 1024.0 ) + vec2( distortion_word( instance, 7u ) * 2.0 * M_PI ) ) * amplitude;
	}

	return uv;
}

// The screen distortion surfaces (force push, cloak) along the ray, up to ray.t_max. They do not stop the
// ray and the tracer does not light them: the distortion pass warps the finished image where they are.
// Returns for the two nearest: the screen texture coordinate of each, packed, in x and y; their
// instance + 1 in the halves of z. Zero in z: no surface.
uvec4 trace_distortion_layers( Ray ray )
{
	// The loop reuses the payload of the primary ray: the caller still reads the hit of the primary surface.
	const RayPayloadGeometry primary_payload = ray_payload_geometry;
	uvec3 record = uvec3( 0u );
	uint count = 0u;
	bool cloak = false;

	for ( int hit = 0; hit < MAX_DISTORTION_HITS && count < MAX_DISTORTION_LAYERS; hit++ )
	{
		trace_geometry_ray( ray, true, AS_FLAG_DISTORTION );

		if ( !found_intersection( ray_payload_geometry ) )
			break;

		ray.t_min = ray_payload_geometry.hit_distance + 0.01;

		const uint instance = get_instance_index( ray_payload_geometry );
		const uint kind = get_model_instance_shader_uint( instance, 1 );
		vec2 uv = vec2( 0.0 );

		if ( ( kind & INSTANCE_NODEPTH ) != 0u )
			continue;	// a layer of the primary rays, see trace_nodepth_layers

		if ( ( kind & INSTANCE_DISTORT_CROP ) != 0u )
			uv = distortion_crop_uv( instance, get_hit_triangle( ray_payload_geometry ), get_hit_barycentric( ray_payload_geometry ) );
		else if ( ( kind & INSTANCE_DISTORT_CLOAK ) == 0u || cloak )
			continue;	// the passes of the cloak apply once to a pixel

		cloak = cloak || ( kind & INSTANCE_DISTORT_CLOAK ) != 0u;
		record[count] = packUnorm2x16( clamp( ( uv + 0.5 ) * 0.5, vec2( 0.0 ), vec2( 1.0 ) ) );
		record.z |= ( ( instance + 1u ) & 0xffffu ) << ( 16u * count );
		count++;
	}

	ray_payload_geometry = primary_payload;

	return uvec4( record, 0u );
}

void get_material(
	uint instance_index,
	Triangle triangle,
	vec3 bary,
	vec2 tex_coord,
	vec2 tex_coord_x,
	vec2 tex_coord_y,
	vec3 footprint_x,
	vec3 footprint_y,
	float mip_level,
	vec3 geo_normal,
	out vec3 geo_tangent,
	out vec3 base_color,
	out vec3 normal,
	out float metallic,
	out float roughness,
	out vec3 emissive,
	out float specular_factor) 
{
	MaterialInfo minfo = get_material_info( triangle.material_id );

	float effective_mip = mip_level;
	float normalMapLen = 0.0;

	if ( minfo.normals_texture != 0 ) 
	{
		vec3 n;

		if ( mip_level >= 0 )
			n = global_textureLod( minfo.normals_texture, tex_coord, mip_level ).agb;
		else 
			n = global_textureGrad( minfo.normals_texture, tex_coord, tex_coord_x, tex_coord_y ).agb;

		vec3 local_normal = rgbToNormal(n, normalMapLen);

		n -= vec3(0.5);

		//n.xy *= u_NormalScale.xy;
		n.xy *= 1.0;
		n.z = sqrt(clamp((0.25 - n.x * n.x) - n.y * n.y, 0.0, 1.0));
		
		// The vertex normal before the flip to the viewer; the tangent frame is built on it.
		vec3 vertex_normal = normalize(triangle.normals * bary);
		geo_tangent = normalize(triangle.tangents * bary);
		geo_tangent = normalize(geo_tangent - vertex_normal * dot(vertex_normal, geo_tangent));

		// The tangent has no handedness: take it from the texture coordinates of the triangle,
		// as the w of the qtangent of the rasterizer (sign of dot(N x T, dP/dt)).
		vec3 dp1 = triangle.positions[1] - triangle.positions[0];
		vec3 dp2 = triangle.positions[2] - triangle.positions[0];
		vec2 dt1 = triangle.tex_coords0[1] - triangle.tex_coords0[0];
		vec2 dt2 = triangle.tex_coords0[2] - triangle.tex_coords0[0];
		float det = dt1.x * dt2.y - dt2.x * dt1.y;
		float handedness = 1.0;
		if (abs(det) > 1e-12)
		{
			vec3 dp_dt = (dp2 * dt1.x - dp1 * dt2.x) / det;
			handedness = dot(cross(vertex_normal, geo_tangent), dp_dt) < 0.0 ? -1.0 : 1.0;
		}

		vec3 bitangent = cross(vertex_normal, geo_tangent) * handedness;
		n = geo_tangent * n.x + bitangent * n.y + geo_normal * n.z;
		
#if 0
		float bump_scale = global_ubo.pt_bump_scale; //  * minfo.bump_scale;
		if(is_glass(triangle.material_id))
        	bump_scale *= 0.2;

		normal = normalize(mix(geo_normal, normal, bump_scale));
#else
		normal = normalize(n);

		// A bumped normal below the horizon of the surface lights it from behind and shows black.
		float horizon = dot(normal, geo_normal);
		if (horizon < 0.05)
			normal = normalize(normal + geo_normal * (0.05 - horizon));

    	if (effective_mip < 0)
    	{
        	ivec2 texSize = global_textureSize(minfo.normals_texture, 0);
        	vec2 tx = tex_coord_x * texSize;
        	vec2 ty = tex_coord_y * texSize;
        	float d = max(dot(tx, tx), dot(ty, ty));
        	effective_mip = 0.5 * log2(d);
        }
#endif
	}
	else 
	{
		geo_tangent = normalize(triangle.tangents * bary);
		normal = geo_normal;
	}

	metallic = 0;
    roughness = 1;

	// A q3map_surfacelight surface too: GL draws it like any other surface. Its light polys light
	// the scene.
	vec3 glow, emission;
	StageContext ctx = stage_context( instance_index, triangle, bary, geo_normal, tex_coord, tex_coord_x, tex_coord_y, footprint_x, footprint_y, mip_level );
	vec4 albedo = compose_material_stages( ctx, minfo, glow, emission );

	base_color = albedo.rgb * minfo.base_factor;
	base_color = clamp(base_color, vec3(0.0), vec3(1.0));

	float spec_mix_bias = 0.08;
	float avgrgb = 0.8;
	float AO = 1.0;


	if ( minfo.physical_texture != 0 ) 
	{
		vec4 ORMS;

		if ( mip_level >= 0 )
			ORMS = global_textureLod( minfo.physical_texture, tex_coord, mip_level );
		else 
			ORMS = global_textureGrad( minfo.physical_texture, tex_coord, tex_coord_x, tex_coord_y );

		//
		// specularScale <rgb> <gloss>
		// or specularScale <r> <g> <b>
		// or specularScale <r> <g> <b> <gloss>
		// or specularScale <metalness> <specular> <unused> <gloss> when metal roughness workflow is used
		//
#if 0	
		ORMS.xyzw *= minfo.specular_scale.zwxy;
		metallic = ORMS.z;
		avgrgb = (base_color.r + base_color.g + base_color.b) / 3;
		//specular.rgb = mix( vec3( 0.08 ) * ORMS.w, base_color.rgb, ORMS.z );
		spec_mix_bias *= ORMS.w;
		base_color.rgb *= vec3( 1.0 - ORMS.z );	

		//roughness = mix( 0.01, 1.0, ORMS.y );
		roughness = clamp(ORMS.y, 0, 1);
		AO = min( ORMS.x, AO );
#else
		avgrgb = (base_color.r + base_color.g + base_color.b) / 3;
		spec_mix_bias *= ORMS.w;
		metallic = clamp( ORMS.z * minfo.specular_scale.x , 0, 1);
		roughness = ORMS.y * minfo.specular_scale.w;
		roughness = clamp(roughness, 0, 1);
		AO = min( ORMS.x, AO ) * minfo.specular_scale.z;
		//base_color.rgb *= vec3( 1.0 - ORMS.z );	
#endif
	}

    bool is_mirror = (roughness < MAX_MIRROR_ROUGHNESS) && (is_chrome(triangle.material_id) || is_screen(triangle.material_id));

    if (normalMapLen > 0 && global_ubo.pt_toksvig > 0 && effective_mip > 0 && !is_mirror)
    {
        roughness = AdjustRoughnessToksvig(roughness, normalMapLen, effective_mip);
    }

    if ( global_ubo.pt_roughness_override >= 0 ) roughness = global_ubo.pt_roughness_override;
    if ( global_ubo.pt_metallic_override >= 0 ) metallic = global_ubo.pt_metallic_override;

    // The specular factor parameter should only affect dielectrics, so make it 1.0 for metals
#if 0
	float specular_factor = 1.0;
    specular_factor = mix(specular_factor, 1.0, metallic);
#else
	specular_factor = mix(minfo.specular_scale.y, 1.0, metallic);
	//specular = mix(0.08, 1.0, metallic) * minfo.specular_scale.y;
#endif

	base_color.rgb = AO * base_color.rgb;

	// What the rasterizer draws without light is in screen units, as the rasterizer adds it to
	// the screen: 1 is the white of the screen at this exposure. A fixed radiance saturates in a
	// dark room, and the soft halo of a glow texture becomes a flat shape. pt_glow_scale is
	// relative to that.
	emissive = screen_to_hdr_color( correct_emissive( triangle.material_id, emission ) * minfo.emission_scale );

	emissive += get_emissive_shell(triangle.material_id, triangle.shell) * base_color * (1 - metallic * 0.9);
}

// Anisotropic texture sampling algorithm from 
// "Improved Shader and Texture Level of Detail Using Ray Cones"
// by T. Akenine-Moller et al., JCGT Vol. 10, No. 1, 2021.
// See section 5. Anisotropic Lookups.
void compute_anisotropic_texture_gradients(
	vec3 intersection,
	vec3 normal,
	vec3 ray_direction,
	float cone_radius,
	mat3 positions,
	mat3x2 tex_coords,
	vec2 tex_coords_at_intersection,
	out vec2 texGradient1,
	out vec2 texGradient2,
	out vec3 footprint1,		// the axes of the pixel on the surface, in world units: the
	out vec3 footprint2,		// gradients of the other tcGens (bundle_uv)
	out float fwidth_depth)
{
	// Compute ellipse axes.
	vec3 a1 = ray_direction - dot(normal, ray_direction) * normal;
	vec3 p1 = a1 - dot(ray_direction, a1) * ray_direction;
	a1 *= cone_radius / max(0.0001, length(p1));

	vec3 a2 = cross(normal, a1);
	vec3 p2 = a2 - dot(ray_direction, a2) * ray_direction;
	a2 *= cone_radius / max(0.0001, length(p2));

	// Compute texture coordinate gradients.
	vec3 eP, delta = intersection - positions[0];
	vec3 e1 = positions[1] - positions[0];
	vec3 e2 = positions[2] - positions[0];
	float inv_tri_area = 1.0 / dot(normal, cross(e1, e2));

	eP = delta + a1;
	float u1 = dot(normal, cross(eP, e2)) * inv_tri_area;
	float v1 = dot(normal, cross(e1, eP)) * inv_tri_area;
	texGradient1 = (1.0-u1-v1) * tex_coords[0] + u1 * tex_coords[1] +
		v1 * tex_coords[2] - tex_coords_at_intersection;

	eP = delta + a2;
	float u2 = dot(normal, cross(eP, e2)) * inv_tri_area;
	float v2 = dot(normal, cross(e1, eP)) * inv_tri_area;
	texGradient2 = (1.0-u2-v2) * tex_coords[0] + u2 * tex_coords[1] +
		v2 * tex_coords[2] - tex_coords_at_intersection;

	footprint1 = a1;
	footprint2 = a2;

	fwidth_depth = 1.0 / max(0.1, abs(dot(a1, ray_direction)) + abs(dot(a2, ray_direction)));
}
