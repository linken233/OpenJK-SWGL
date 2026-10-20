/*
Copyright (C) 2020-2021, NVIDIA CORPORATION. All rights reserved.
Copyright (C) 2021 Frank Richter

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

layout(set = 0, binding = 1)
uniform textureBuffer particle_color_buffer;

layout(set = 0, binding = 2)
uniform textureBuffer beam_color_buffer;

layout(set = 0, binding = 3)
uniform utextureBuffer sprite_texure_buffer;

layout(set = 0, binding = 4)
uniform utextureBuffer beam_info_buffer;

void get_model_index_and_prim_offset(int instanceID, int geometryIndex, out int model_index, out uint prim_offset )
{
	model_index = instance_buffer.tlas_instance_model_indices[instanceID];
	if (model_index >= 0)
	{
		model_index += geometryIndex;
		prim_offset = instance_buffer.model_instances[model_index].render_prim_offset;
	}
	else
	{
		prim_offset = instance_buffer.tlas_instance_prim_offsets[instanceID];
	}
}

void pt_logic_rchit(inout RayPayloadGeometry ray_payload, int primitiveID, int instanceID, int geometryIndex, uint instanceCustomIndex, float hitT, vec2 bary)
{
	int model_index;
	uint prim_offset;
	uint type;

	get_model_index_and_prim_offset(instanceID, geometryIndex, model_index, prim_offset);

	ray_payload.barycentric = bary.xy;
	//ray_payload.primitive_id = primitiveID;
	ray_payload.primitive_id = primitiveID + prim_offset;
	ray_payload.buffer_and_instance_idx = (int(instanceCustomIndex) & 0xffff)
	                                    | (model_index << 16);
	ray_payload.hit_distance = hitT;
}

bool pt_logic_masked(int primitiveID, int instanceID, int geometryIndex, uint instanceCustomIndex, vec2 bary)
{
	int model_index;
	uint prim_offset;
	get_model_index_and_prim_offset(instanceID, geometryIndex, model_index, prim_offset);

	uint prim = primitiveID + prim_offset;
	uint buffer_idx = instanceCustomIndex;

	Triangle triangle = load_and_transform_triangle(model_index, buffer_idx, prim);

	// CGEN_DISINTEGRATION_1: the part of the model that is burnt away does not stop the ray.
	if (buffer_idx == VERTEX_BUFFER_INSTANCED && (get_model_instance_shader_uint(triangle.instance_index, 1) & 0xffu) == 14u)
	{
		vec3 position = triangle.positions * vec3(1.0 - bary.x - bary.y, bary.x, bary.y);

		if (disintegration_distance(triangle.instance_index, position) < 0.0)
			return false;
	}

	MaterialInfo minfo = get_material_info(triangle.material_id);

	if (minfo.base_texture == 0)
		return true;

	if (minfo.alpha_test_func == 0u)
		return true;

	vec2 tex_coord = triangle.tex_coords0 * vec3(1.0 - bary.x - bary.y, bary.x, bary.y);
#if 0
	perturb_tex_coord(triangle.material_id, global_ubo.time, tex_coord);	
	vec4 mask_value = global_textureLod(minfo.base_texture, tex_coord, /* mip_level = */ 0);
	return mask_value.a >= 0.5;
#endif
	vec4 texel = global_textureLod(minfo.base_texture, tex_coord, 0);

	// A texel that contributes nothing must not stop the ray: a fully transparent one on
	// a blended stage, or a black one on an additive stage. These used to return true,
	// which kept the hit and let those texels cast a shadow.
	if (minfo.blend_mode == RTX_BLEND_ALPHA || minfo.blend_mode == RTX_BLEND_ALPHA_PREMUL)
	{
		if (texel.a == 0.0)
			return false;
	}
	else if (minfo.blend_mode == RTX_BLEND_ADDITIVE)
	{
		if (dot(texel.rgb, texel.rgb) == 0.0)
			return false;
	}

	switch (minfo.alpha_test_func)
	{
		// GLS_ATEST_GT_0
		case 1u:
			return texel.a > 0.0;

		// GLS_ATEST_LT_80
		case 2u:
			return texel.a < minfo.alpha_test_value;

		// GLS_ATEST_GE_80 / GLS_ATEST_GE_C0
		case 3u:
			return texel.a >= minfo.alpha_test_value;
	}

	return true;

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

TransparencyHit make_empty_hit()
{
    return TransparencyHit(vec3(0.0), vec3(1.0), vec3(0.0), false);
}

// fx: for a weapon effect (saber, bolt), what the rasterizer would add to the screen. The textures
// are read as it reads them, in gamma, so that the soft glows pile up as they do there.
TransparencyHit pt_logic_sprite(int primitiveID, vec2 bary, out vec3 fx, out vec3 lit_L, out float lit_T)
{
    fx = vec3(0.0);
    lit_L = vec3(0.0);
    lit_T = 1.0;

    const vec3 barycentric = vec3(1.0 - bary.x - bary.y, bary.x, bary.y);

    vec2 uv;
    if ((primitiveID & 1) == 0)
        uv = vec2(1.0, 0.0) * barycentric.x + vec2(0.0, 0.0) * barycentric.y + vec2(0.0, 1.0) * barycentric.z;
    else
        uv = vec2(0.0, 1.0) * barycentric.x + vec2(1.0, 1.0) * barycentric.y + vec2(1.0, 0.0) * barycentric.z;

    const int sprite_index = primitiveID / 2;

    // Three texels per sprite: material, entity colour, kind (1: a triangle of a scene
    // poly); the UVs of its corners; their colours.
    uvec4 info = texelFetch(sprite_texure_buffer, sprite_index * 3);
    MaterialInfo minfo = get_material_info(info.x);

    if (minfo.base_texture == 0)
        return make_empty_hit();

    const bool weapon_fx = ((info.w & 1u) != 0u) && global_ubo.pt_weapon_fx != 0.0;
    const bool lit_sprite = (info.w & 2u) != 0u;

    vec4 shaderRGBA = unpack_rgba8(info.y);

    // The corners of a sprite take the entity colour (RB_AddQuadStamp), those of a scene poly
    // their own colour.
    vec4 corner_color = shaderRGBA;

    if (info.z == 1u)
    {
        uvec4 uvs = texelFetch(sprite_texure_buffer, sprite_index * 3 + 1);
        uvec4 colors = texelFetch(sprite_texure_buffer, sprite_index * 3 + 2);

        uv = unpackHalf2x16(uvs.x) * barycentric.x + unpackHalf2x16(uvs.y) * barycentric.y + unpackHalf2x16(uvs.z) * barycentric.z;
        corner_color = unpack_rgba8(colors.x) * barycentric.x + unpack_rgba8(colors.y) * barycentric.y + unpack_rgba8(colors.z) * barycentric.z;
    }

    // The stages as the rasterizer blends them onto the screen, in screen units:
    // out = L + T * behind (blend_stage_layer). The glow pass takes the glow bundles only.
    vec3 L = vec3(0.0), T = vec3(1.0);
    vec3 glow_L = vec3(0.0), glow_T = vec3(1.0);
    vec3 fx_L = vec3(0.0), fx_T = vec3(1.0);

    for (uint s = 0u; s < MAX_RTX_STAGES; s++)
    {
        MaterialStage stage = minfo.stage[s];

        if ((stage.blend & STAGE_BLEND_ACTIVE) == 0u)
            break;

        // The colour as the rasterizer computes it: the vertex and entity rgbGen and alphaGen,
        // else the colour of the bundle (const, wave, identityLighting). A rgbGen identity
        // stage ignores the entity colour.
        MaterialBundle bundle = stage.bundle[0];
        vec4 color = unpack_rgba8(bundle.color);
        uint rgb_gen = bundle.rgbGen;
        uint alpha_gen = bundle.alphaGen & 0xffu;

        if (rgb_gen == 5u || rgb_gen == 6u)		// CGEN_EXACT_VERTEX, CGEN_VERTEX
            color.rgb = corner_color.rgb;
        else if (rgb_gen == 3u)					// CGEN_ENTITY
            color.rgb = shaderRGBA.rgb;

        if (alpha_gen == 4u)					// AGEN_VERTEX
            color.a = corner_color.a;
        else if (alpha_gen == 2u)				// AGEN_ENTITY
            color.a = shaderRGBA.a;

        vec4 fx_color = color;

        if (bundle.image != 0u)
        {
            // A weapon effect: the mip level the CPU worked out from the size of the sprite on the screen.
            const float lod = weapon_fx ? float((info.w >> 8u) & 0xffu) / 16.0 : 0.0;
            const vec4 tex = global_textureLod(bundle.image, mat2(bundle.tc_matrix.xy, bundle.tc_matrix.zw) * uv + bundle.tc_offset.xy, lod);
            color *= tex;
            fx_color *= tex;
        }

        // The alpha test of the shader is on its first stage.
        if (s == 0u)
        {
            bool pass = true;
            switch (minfo.alpha_test_func)
            {
                case 1u: pass = color.a > 0.0; break;
                case 2u: pass = color.a < minfo.alpha_test_value; break;
                case 3u: pass = color.a >= minfo.alpha_test_value; break;
            }

            if (!pass)
                return make_empty_hit();
        }

        blend_stage_layer(stage.blend, color, L, T);
        blend_stage_layer(stage.blend, fx_color, fx_L, fx_T);

        bool glows = (bundle.alphaGen & BUNDLE_GLOW) != 0u;
        blend_stage_layer(stage.blend, glows ? color : vec4(0.0, 0.0, 0.0, color.a), glow_L, glow_T);
    }

    if (all(lessThanEqual(L, vec3(0.0))) && all(greaterThanEqual(T, vec3(1.0))))
        return make_empty_hit();

    // A lit sprite is an albedo layer: the primary ray multiplies it with the light of the surface behind.
    // The stage colour is in screen (gamma) units, an albedo is linear.
    if (lit_sprite)
    {
        lit_L = srgb_to_linear(clamp(L, vec3(0.0), vec3(1.0)));
        lit_T = clamp(dot(T, vec3(1.0 / 3.0)), 0.0, 1.0);
        return TransparencyHit(vec3(0.0), vec3(1.0), vec3(0.0), true);
    }

    // The effect leaves the layer of the tracer: the tone mapper does not see it. The glow stays for the bloom.
    if (weapon_fx)
    {
        fx = max(fx_L, vec3(0.0)) * minfo.emission_scale;
        L = vec3(0.0);
        T = vec3(1.0);
    }

    // The rasterizer draws the effects in screen units: the HDR value is the one the tone mapper
    // shows as this colour (screen_to_hdr_color). pt_glow_scale scales it, as all that the tracer
    // adds in screen units. The glow goes to the bloom, which takes it back to screen units with
    // the exposure only.
    vec3 hdr = screen_to_hdr_color(max(L, vec3(0.0)) * minfo.emission_scale);
    float glow_to_hdr = global_ubo.prev_adapted_luminance / exp2(global_ubo.tm_exposure_bias - 2.0) * minfo.emission_scale;

    return TransparencyHit(hdr, clamp(T, vec3(0.0), vec3(2.0)), max(glow_L, vec3(0.0)) * glow_to_hdr, true);
}