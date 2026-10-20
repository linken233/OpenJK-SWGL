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

#ifndef PATH_TRACER_TRANSPARENCY_GLSL_
#define PATH_TRACER_TRANSPARENCY_GLSL_

vec4 evaluate_fog(uvec4 fog, float t1, float t2)
{
	vec2 fog_bounds = unpackHalf2x16(fog.z);
	float fog_k2 = unpackHalf2x16(fog.w).x / 65536.0; // same scale as in find_fog_volumes(...)
	vec3 fog_color = unpackHalf4x16(fog.xy).rgb;

	t1 = max(t1, fog_bounds.x);
	t2 = min(t2, fog_bounds.y);

	if (t1 >= t2)
		return vec4(0);

	// The rasterizer fog: T = exp(-k2 * d^2), d = distance inside the volume from its entry point.
	float d1 = t1 - fog_bounds.x;
	float d2 = t2 - fog_bounds.x;
	float alpha = 1.0 - exp((d1 * d1 - d2 * d2) * fog_k2);

	return vec4(fog_color * alpha, alpha);
}

void blend_fogs(in RayPayloadEffects rp, float t1, float t2, inout vec4 accumulated_color)
{
	// Blend the further fog volume first...
	if (rp.fog2.w != 0) // test the density for being nonzero
	{
		vec4 fog_color = evaluate_fog(rp.fog2, t1, t2);
		accumulated_color = alpha_blend_premultiplied(fog_color, accumulated_color);
	}

	// ...Then blend the closer volume.
	if (rp.fog1.w != 0)
	{
		vec4 fog_color = evaluate_fog(rp.fog1, t1, t2);
		accumulated_color = alpha_blend_premultiplied(fog_color, accumulated_color);
	}
}

// One more effect on the ray: out = L + T * behind. The any-hits come in any order: an effect
// nearer than all the others goes in front, else behind.
void update_payload_transparency( inout RayPayloadEffects rp, vec3 L, vec3 T, float hitT ) {
    vec3 acc_T     = unpackHalf4x16(rp.transparency).rgb;
    vec3 acc_L     = unpackHalf4x16(rp.additive).rgb;
    vec2 distances = unpackHalf2x16(rp.distances);

    if (hitT < distances.x || distances.x == 0.0)
    {
        acc_L = L + T * acc_L;
        acc_T = T * acc_T;
        distances.x = hitT;
    }
    else
    {
        acc_L += acc_T * L;
        acc_T *= T;
    }

    distances.y = max(distances.y, hitT);

    rp.transparency = packHalf4x16(vec4(acc_T, 0));
    rp.additive     = packHalf4x16(vec4(acc_L, 0));
    rp.distances    = packHalf2x16(distances);
}

// One more lit sprite, in the order rule of update_payload_transparency, on the albedo layer.
void update_payload_lit( inout RayPayloadEffects rp, vec3 L, float T, float hitT ) {
    vec4 lit = unpackHalf4x16(rp.lit);

    if (hitT < rp.litNearest || rp.litNearest == 0.0)
    {
        lit = vec4(L + T * lit.rgb, T * lit.a);
        rp.litNearest = hitT;
    }
    else
        lit = vec4(lit.rgb + lit.a * L, lit.a * T);

    rp.lit = packHalf4x16(lit);
}

// The effects as a layer over the image: the colour T becomes a coverage (its mean), and L is
// added.
EffectsResult get_payload_transparency(in RayPayloadEffects rp)
{
	EffectsResult result;
    vec3 T          = unpackHalf4x16(rp.transparency).rgb;
    result.alpha    = vec4(0, 0, 0, 1.0 - dot(T, vec3(1.0 / 3.0)));
    result.additive = unpackHalf4x16(rp.additive).rgb;
    result.glow     = unpackHalf4x16(rp.glow).rgb;
    result.fx       = unpackHalf4x16(rp.fx).rgb;
    result.lit      = unpackHalf4x16(rp.lit);
    result.fog      = vec4(0);

    return result;
}

EffectsResult get_payload_transparency_with_fog(in RayPayloadEffects rp, float t_max)
{
    EffectsResult result;

    vec2 distances = unpackHalf2x16(rp.distances);

    vec4 alpha_accumulator = vec4(0);
    float current_dist = t_max;

    if (distances.y > 0)
    {
        blend_fogs(rp, distances.y, t_max, alpha_accumulator);

        vec3 T = unpackHalf4x16(rp.transparency).rgb;
        vec4 ray_transparency = vec4(0, 0, 0, 1.0 - dot(T, vec3(1.0 / 3.0)));
        alpha_accumulator = alpha_blend_premultiplied( ray_transparency, alpha_accumulator);

        current_dist = distances.x;
    }

    blend_fogs(rp, 0, current_dist, alpha_accumulator);

    result.alpha = alpha_accumulator;
    result.additive = unpackHalf4x16(rp.additive).rgb;
    result.glow     = unpackHalf4x16(rp.glow).rgb;
    result.fx       = unpackHalf4x16(rp.fx).rgb;
    result.lit      = unpackHalf4x16(rp.lit);
    result.fog      = vec4(0);

    return result;
}

#endif // PATH_TRACER_TRANSPARENCY_GLSL_
