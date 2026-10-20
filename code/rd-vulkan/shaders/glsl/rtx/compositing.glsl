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

// The composition of a pixel, shared by the shaders that end a denoiser:
// compositing.comp (no denoiser) and nrd_composite.comp (NRD).
// The includer defines the global UBO, the global textures, utils.glsl and brdf.glsl first.

// Loads the surface parameters of the pixel at ipos of the tracer images and composites them
// with the lighting channels. The channels are in render units: the scale of the storage is removed.
// The alpha channel of the result holds the checkerboard flags.
vec4
composite_pixel(ivec2 ipos, vec3 low_freq, vec3 high_freq, vec3 specular)
{
	// Load the surface parameters
	int checkerboard_flags = int(texelFetch(TEX_PT_VIEW_DIRECTION, ipos, 0).w);
	vec3 throughput = texelFetch(TEX_PT_THROUGHPUT, ipos, 0).rgb;

	// Load the other image channels
	vec3 base_color = texelFetch(TEX_PT_BASE_COLOR_A, ipos, 0).rgb;
	vec2 metal_rough = texelFetch(TEX_PT_METALLIC_A, ipos, 0).rg;
	vec4 transparent = texelFetch(TEX_PT_TRANSPARENT, ipos, 0);

	// Composite
	vec3 final_color = composite_color(base_color.rgb, metal_rough.r, throughput, low_freq, high_freq, specular, transparent);

	final_color *= STORAGE_SCALE_HDR;

	return vec4(final_color, checkerboard_flags);
}
