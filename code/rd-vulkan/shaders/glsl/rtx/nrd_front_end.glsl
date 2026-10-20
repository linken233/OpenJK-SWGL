/*
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

// This software contains source code provided by NVIDIA Corporation.

// The packing functions of the NRD front end (nrd/Shaders/NRD.hlsli, NRD v4.17.3), ported to GLSL.
// They use the options of the NRD build: NRD_NORMAL_ENCODING 2 (R10G10B10A2 oct-packed) and
// NRD_ROUGHNESS_ENCODING 1 (linear roughness). The flat <-> checkerboard mapping of the tracer images is here too.

// ========================================================================== //
// NRD front end
// ========================================================================== //

#define NRD_FP16_MAX 65504.0

// Port of _NRD_EncodeNormalRoughness101010.
vec3
nrd_encode_normal_roughness_101010(vec3 n, float roughness)
{
	n /= abs(n.x) + abs(n.y) + abs(n.z);

	vec3 r;
	r.y = n.y * 0.5 + 0.5;
	r.x = n.x * 0.5 + r.y;
	r.y -= n.x * 0.5;

	// The roughness and the sign of n.z share the z channel.
	roughness = max(roughness, 1.5 / 512.0);
	float s = n.z < 0 ? -roughness : roughness;
	r.z = s * 0.5 + 0.5;

	return r;
}

// Port of NRD_FrontEnd_PackNormalAndRoughness for NRD_ROUGHNESS_ENCODING_LINEAR.
vec4
nrd_pack_normal_and_roughness(vec3 n, float roughness, float material_id)
{
	vec4 p;
	p.xyz = nrd_encode_normal_roughness_101010(n, roughness);
	p.w = clamp(material_id / 3.0, 0.0, 1.0);
	return p;
}

// Port of RELAX_FrontEnd_PackRadianceAndHitDist with sanitize = true.
vec4
nrd_relax_pack_radiance_and_hit_dist(vec3 radiance, float hit_dist)
{
	bool radiance_invalid = any(isnan(radiance)) || any(isinf(radiance));
	bool hit_dist_invalid = isnan(hit_dist) || isinf(hit_dist);

	radiance = radiance_invalid ? vec3(0) : clamp(radiance, vec3(0), vec3(NRD_FP16_MAX));
	hit_dist = hit_dist_invalid ? 0.0 : clamp(hit_dist, 0.0, NRD_FP16_MAX);

	return vec4(radiance, hit_dist);
}

#define NRD_EPS 1e-6

// Port of _NRD_LinearToYCoCg.
vec3
nrd_linear_to_ycocg(vec3 color)
{
	float Y = dot(color, vec3(0.25, 0.5, 0.25));
	float Co = dot(color, vec3(0.5, 0.0, -0.5));
	float Cg = dot(color, vec3(-0.25, 0.5, -0.25));

	return vec3(Y, Co, Cg);
}

// Port of _NRD_YCoCgToLinear.
vec3
nrd_ycocg_to_linear(vec3 color)
{
	float t = color.x - color.z;

	vec3 r;
	r.y = color.x + color.z;
	r.x = t + color.y;
	r.z = t - color.y;

	return max(r, vec3(0));
}

// Port of _NRD_GetSpecMagicCurve.
float
nrd_get_spec_magic_curve(float roughness, float power)
{
	float f = 1.0 - exp2(-200.0 * roughness * roughness);
	f *= pow(clamp(roughness, 0.0, 1.0), power);

	return f;
}

// Port of REBLUR_FrontEnd_GetNormHitDist and _REBLUR_GetHitDistanceNormalization.
// Only for a lobe that the tracer traced: the result is never 0.
float
nrd_reblur_get_norm_hit_dist(float hit_dist, float view_z, vec3 hit_dist_params, float roughness)
{
	float smc = nrd_get_spec_magic_curve(roughness, 0.5);
	float f = (hit_dist_params.x + abs(view_z) * hit_dist_params.y) * mix(hit_dist_params.z, 1.0, smc);

	hit_dist = clamp(hit_dist / f, 0.0, 1.0);

	return max(hit_dist, NRD_EPS);
}

// Port of REBLUR_FrontEnd_PackRadianceAndNormHitDist with sanitize = true.
vec4
nrd_reblur_pack_radiance_and_norm_hit_dist(vec3 radiance, float norm_hit_dist)
{
	bool radiance_invalid = any(isnan(radiance)) || any(isinf(radiance));
	bool hit_dist_invalid = isnan(norm_hit_dist) || isinf(norm_hit_dist);

	radiance = radiance_invalid ? vec3(0) : clamp(radiance, vec3(0), vec3(NRD_FP16_MAX));
	norm_hit_dist = hit_dist_invalid ? 0.0 : clamp(norm_hit_dist, 0.0, 1.0);

	return vec4(nrd_linear_to_ycocg(radiance), norm_hit_dist);
}

// Port of REBLUR_BackEnd_UnpackRadianceAndNormHitDist: the radiance of the result.
vec3
nrd_reblur_unpack_radiance(vec4 data)
{
	return nrd_ycocg_to_linear(data.xyz);
}

// The settings of the frame for nrd_prepare.comp and nrd_composite.comp (the push constants of nrd_pipelines in vk_rtx_nrd.cpp).
layout(push_constant) uniform NrdPushConstants
{
	vec4 hit_dist_params;	// ReBLUR: A, B, C of the normalized hit distance
	uint mode;				// 0 ReLAX, 1 ReBLUR
	uint direct;			// 1: the direct diffuse goes through NRD
	uint validation;		// 1: show OUT_VALIDATION
} nrd_pc;

// ========================================================================== //
// Checkerboard layout of the tracer images
// ========================================================================== //

// The tracer stores the even field in the left half of an image and the odd field in the right half.
// The checkerboard interleave pass reads the same mapping. The fields swap when pt_swap_checkerboard is set.

// The position in the tracer images of the pixel of the flat image.
ivec2
nrd_flat_to_checker(ivec2 flat_pos)
{
	bool px_equals_py = (flat_pos.x & 1) == (flat_pos.y & 1);
	bool is_even = (global_ubo.pt_swap_checkerboard != 0) ? !px_equals_py : px_equals_py;

	ivec2 pos = ivec2(flat_pos.x / 2, flat_pos.y);
	if(!is_even)
		pos.x += global_ubo.width / 2;

	return pos;
}

// The position in the flat image of the pixel of the tracer images.
ivec2
nrd_checker_to_flat(ivec2 pos)
{
	int half_width = global_ubo.width / 2;
	bool left_half = pos.x < half_width;
	int x = left_half ? pos.x : pos.x - half_width;

	// Left half: (px == py) is true without a swap. Right half: it is false without a swap.
	bool px_equals_py = left_half ? (global_ubo.pt_swap_checkerboard == 0) : (global_ubo.pt_swap_checkerboard != 0);
	int py = pos.y & 1;
	int px = px_equals_py ? py : 1 - py;

	return ivec2(min(x * 2 + px, global_ubo.width - 1), pos.y);
}
