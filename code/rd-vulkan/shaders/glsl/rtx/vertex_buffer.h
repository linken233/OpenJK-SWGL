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

#ifndef _VERTEX_BUFFER_H_
#define _VERTEX_BUFFER_H_

#include "shader_structs.h"

// lights
#define MAX_LIGHT_LISTS         (1 << 14)
#define MAX_LIGHT_LIST_NODES    (1 << 19)
//#define LIGHT_COUNT_HISTORY	16
#define LIGHT_COUNT_HISTORY     3 // one for previous frame rendered, one for current frame rendering, one for upload

#define MAX_LIGHT_POLYS         16384
#define LIGHT_POLY_VEC4S        4
#define MATERIAL_UINTS			6
#define MAX_MDXM_MATRICES       32768

// should match the same constant declared in material.h
#define MAX_PBR_MATERIALS		4096

#define ALIGN_SIZE_4(x, n)  ((x * n + 3) & (~3))

// BINDING OFFSETS
// top level acceleration structure
#define RAY_GEN_ACCEL_STRUCTURE_BINDING_IDX			0
#define RAY_GEN_PARTICLE_COLOR_BUFFER_BINDING_IDX	1
#define RAY_GEN_BEAM_COLOR_BUFFER_BINDING_IDX		2
#define RAY_GEN_SPRITE_INFO_BUFFER_BINDING_IDX		3
#define RAY_GEN_BEAM_INTERSECT_BUFFER_BINDING_IDX	4

#define PRIMITIVE_BUFFER_BINDING_IDX						1
#define POSITION_BUFFER_BINDING_IDX							2

// readback
#define BINDING_OFFSET_READBACK_BUFFER						3

// dynamic vertex
#define BINDING_OFFSET_DYNAMIC_VERTEX						4

// light and material
#define BINDING_OFFSET_LIGHT_BUFFER							5
#define BINDING_LIGHT_COUNTS_HISTORY_BUFFER					6

// tonemap
#define BINDING_OFFSET_TONEMAP_BUFFER						7

// sky
#define BINDING_OFFSET_SUN_COLOR_BUFFER						8
#define BINDING_OFFSET_SUN_COLOR_UBO						9

// light stats
#define BINDING_OFFSET_LIGHT_STATS_BUFFER					10

// mdxm bones
#define BINDING_OFFSET_MDXM_BONE_BUFFER					11

// radiance cache
#define BINDING_OFFSET_RC_TAGS							12
#define BINDING_OFFSET_RC_ACCUM							13
#define BINDING_OFFSET_RC_RESOLVED						14
#define BINDING_OFFSET_RC_LAST_FRAME					15

// debug relations
#define USE_MULTI_WORLD_BUFFERS

#define VERTEX_BUFFER_WORLD					0
#define VERTEX_BUFFER_SKY					1
#define VERTEX_BUFFER_WORLD_D_MATERIAL		2
#define VERTEX_BUFFER_WORLD_D_GEOMETRY		3
#define VERTEX_BUFFER_DEBUG_LIGHT_POLYS		4
#define VERTEX_BUFFER_INSTANCED				5
#define VERTEX_BUFFER_SUB_MODELS			6
#define VERTEX_BUFFER_FIRST_MODEL			7

#define SUN_COLOR_ACCUMULATOR_FIXED_POINT_SCALE 0x100000
#define SKY_COLOR_ACCUMULATOR_FIXED_POINT_SCALE 0x100

// sample_light_counts: light count in cluster used for sampling (may differ from actual light count!)

#define VERTEX_BUFFER_LIST_DO( type, dim, name, size ) \
type name[ ALIGN_SIZE_4( size, dim ) ];

STRUCT (  
	UINT	( image )
	UINT	( rgbGen )
	UINT	( alphaGen )
	UINT	( color )		// rgba8: rgbGen and alphaGen that do not come from the entity or the vertex
	VEC4	( tc_matrix )	// tcMods: u' = x * u + z * v + offset.x, v' = y * u + w * v + offset.y
	VEC4	( tc_offset )
	VEC4	( tc_gen_s )	// w: the tcGen (texCoordGen_t). tcGen vector: s = dot(position, xyz)
	VEC4	( tc_gen_t )	// tcGen vector: t = dot(position, xyz)
, MaterialBundle )
#define MATERIALBUNDLE(n) MaterialBundle n;

STRUCT (  
	MATERIALBUNDLE	( bundle[3] )
	UINT			( tex_mode )
	UINT			( tex_count )
	UINT			( blend )		// GLS blend bits 0-7; STAGE_BLEND_ACTIVE, STAGE_BLEND_LIGHTMAP
	FLOAT			( portal_range_r )	// 1 / portalRange of the shader, for alphaGen portal
, MaterialStage )
#define MATERIALSTAGE(n) MaterialStage n;

STRUCT (
	MAT3X4			( matrices[MAX_MDXM_MATRICES] )
, MDXMMatrixBuffer )

STRUCT ( 
	UINT			( material_table[MAX_PBR_MATERIALS * MATERIAL_UINTS] )
	MATERIALSTAGE	( material_stages[MAX_PBR_MATERIALS * MAX_RTX_STAGES] )
	VEC4			( light_polys[MAX_LIGHT_POLYS * LIGHT_POLY_VEC4S] )
	UINT			( light_list_offsets[MAX_LIGHT_LISTS] )
	UINT			( light_list_lights[MAX_LIGHT_LIST_NODES] )
	FLOAT			( light_styles[MAX_LIGHT_STYLES] )
	UINT			( cluster_debug_mask[MAX_LIGHT_LISTS / 32] )
	UINT			( sky_visibility[MAX_LIGHT_LISTS / 32] )
, LightBuffer )

#undef VERTEX_BUFFER_LIST_DO

#ifndef USE_RTX_GLOBAL_MODEL_VBO
typedef struct {
	vec3_t position;
	vec3_t normal;
	vec2_t texcoord;
	vec4_t tangents;
} model_vertex_t;
#endif

STRUCT (  
	INT		( accumulator[HISTOGRAM_BINS] )
	FLOAT	( curve[HISTOGRAM_BINS] )
	FLOAT	( normalized[HISTOGRAM_BINS] )
	FLOAT	( adapted_luminance )
	FLOAT	( tonecurve )
, ToneMappingBuffer )

STRUCT ( 
	UINT	( material )
	UINT	( cluster )
	FLOAT	( sun_luminance )
	FLOAT	( sky_luminance )
	VEC3	( hdr_color )
	FLOAT	( adapted_luminance )
, ReadbackBuffer )

STRUCT ( 
	IVEC3	( accum_sun_color )
	INT		( pad0 )
	IVEC4	( accum_sky_color )
	VEC3	( sun_color )
	FLOAT	( sun_luminance )
	VEC3	( sky_color )
	FLOAT	( sky_luminance )
, SunColorBuffer )

STRUCT (
	VEC3    ( pos0 )
	UINT    ( material_id )

	VEC3    ( pos1 )
	INT     ( cluster )

	VEC3    ( pos2 )
	UINT    ( shell )

	UVEC3	( normals )
	UINT    ( instance )

	UVEC3   ( tangents )
	/* packed half2x16, with emissive in x component/low 16 bits and
	 * alpha in y component/high 16 bits */
	UINT    ( emissive_and_alpha )

	UVEC4    ( uv0 )
	UVEC4    ( uv1 )
	UVEC4    ( uv2 )

	UVEC2	( custom0 )  // The custom fields store motion for instanced meshes in the animated buffer,
	UVEC2	( custom1 )  // or blend indices and weights for skinned meshes before they're animated.
	UVEC2	( custom2 )

	UINT    ( color0[4] )
	UINT    ( color1[4] )
	UINT    ( color2[4] )

	UVEC2	( pad0 )
, VboPrimitive )

#ifdef GLSL

#ifdef VERTEX_READONLY
#define BUFFER_T readonly buffer
#else
#define BUFFER_T buffer
#endif

STRUCT (  
	UINT	( base_texture )
	MATERIALSTAGE	( stage[MAX_RTX_STAGES] )
	UINT	( normals_texture )
	UINT	( emissive_texture )
	UINT	( physical_texture )
	UINT    ( blend_mode )
	UINT    ( alpha_test_func )
	FLOAT   ( alpha_test_value )
	//FLOAT	( bump_scale )
	//FLOAT	( roughness_override )
	//FLOAT	( metalness_factor )
	//FLOAT	( specular_factor )
	VEC4	( specular_scale )	// contains the commented above see stage->specularScale
	FLOAT	( emissive_factor )
	FLOAT	( emission_scale )	// pt_glow_scale, for what the rasterizer draws without light
	FLOAT	( base_factor )
	FLOAT	( light_style_scale )
	UINT	( num_frames )
	UINT	( next_frame )
, MaterialInfo )

struct LightPolygon
{
	/* Meaning of positions depends on light type:
	 * - static/poly/triangle light: actual positions of vertices
	 * - dynamic light:
	 *   - all types:
	 *       positions[0]: light origin
	 *       positions[1].x: radius
	 *   - spot light:
	 *       positions[1].y: emission profile (uint reinterepreted as float)
	 *       positions[1].z: spot light data, meaning depending on emission profile:
	 *         DYNLIGHT_SPOT_EMISSION_PROFILE_FALLOFF -> contains packed2x16 with cosTotalWidth, cosFalloffStart
	 *         DYNLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE -> contains a half with cosTotalWidth and the texture index
	 *       positions[2]: direction
	 */
	mat3 positions;
	vec3 color;
	float light_style_scale;
	float prev_style_scale;
	float type;
};

struct TextureData {
	int tex0;
	int tex1;
	uint tex0Blend;
	uint tex1Blend;
	bool tex0Color;
	bool tex1Color;
};

// Buffer with indices and vertices
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = PRIMITIVE_BUFFER_BINDING_IDX )		buffer PRIMITIVE_BUFFER {
	VboPrimitive primitives[];
} primitive_buffers[];

// The buffer with just the position data for animated models.
layout(set = VERTEX_BUFFER_DESC_SET_IDX, binding = POSITION_BUFFER_BINDING_IDX) buffer POSITION_BUFFER {
	float positions[];
} instanced_position_buffer;

/* History of light count in cluster, used for sampling.
 * This is used to make gradient estimation work correctly:
 * "The A-SVGF algorithm uses old random numbers to select lights for a subset of pixels,
 * and expects to get the same result if the lighting didn't change."
 * (quoted from discussion on GH PR 227).
 * One way to achieve this is to also keep a history of light numbers and use that for
 * sampling "old" data.
 *
 * We have multiple buffers b/c we may need to access the history for the current or any previous frame.
 * That'd be harder to do with a light_buffer member since that is backed by alternating buffers */
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_LIGHT_COUNTS_HISTORY_BUFFER ) readonly buffer LIGHT_COUNTS_HISTORY_BUFFER {
	uint sample_light_counts[];
} light_counts_history[LIGHT_COUNT_HISTORY];

layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_TONEMAP_BUFFER )				buffer TONE_MAPPING_BUFFER { ToneMappingBuffer tonemap_buffer; };
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_SUN_COLOR_BUFFER )			buffer SUN_COLOR_BUFFER { SunColorBuffer sun_color_buffer; };
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_SUN_COLOR_UBO, std140 )		uniform SUN_COLOR_UBO { SunColorBuffer sun_color_ubo; };
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_LIGHT_STATS_BUFFER )			buffer LIGHT_STATS_BUFFERS { uint stats[]; } light_stats_bufers[3];
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_READBACK_BUFFER )			buffer READBACK_BUFFER { ReadbackBuffer readback; };
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_LIGHT_BUFFER )				buffer LIGHT_BUFFER { LightBuffer light_buffer; };
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_MDXM_BONE_BUFFER )			readonly buffer MDXM_BONE_BUFFER { MDXMMatrixBuffer mdxm_matrix_buffer; };

#undef BUFFER_T

struct Triangle {
	mat3 positions;		// mat3x3
	mat3 positions_prev;// mat3x3
	mat3x2 tex_coords0;	// the texture coordinates; each bundle has its tcGen (bundle_uv)
	mat3x3 normals;
	mat3x3 tangents;
	//mat3x3 binormal;
	mat3x4 color0;		// the colour of bundle 0 of each stage at each vertex (BSP surfaces only)
	mat3x4 color1;
	mat3x4 color2;
	mat3x4 color3;
	bool   vertex_colors;	// color0-3 are valid
	uint tex0;
	uint tex1;
	uint   shell;
	int cluster;
	uint material_id;
	uint   instance_index;
	uint   instance_prim;
	float  emissive_factor;
	float  alpha;
	//float  alpha;
};

vec2 get_uv(uvec4 packed_uvs, int stage) {
    return unpackHalf2x16(packed_uvs[stage]);
}

Triangle
load_triangle(uint buffer_idx, uint prim_id)
{
	// info
	// buffer_idx = gl_InstanceCustomIndexEXT = instance.instance_id = vbo_index
	
	VboPrimitive prim = primitive_buffers[nonuniformEXT(buffer_idx)].primitives[prim_id];

	Triangle t;
	t.positions[0] = prim.pos0;
	t.positions[1] = prim.pos1;
	t.positions[2] = prim.pos2;

	t.positions_prev[0] = t.positions[0] + unpackHalf4x16(prim.custom0).xyz;
	t.positions_prev[1] = t.positions[1] + unpackHalf4x16(prim.custom1).xyz;
	t.positions_prev[2] = t.positions[2] + unpackHalf4x16(prim.custom2).xyz;

	t.normals[0] = decode_normal(prim.normals.x);
	t.normals[1] = decode_normal(prim.normals.y);
	t.normals[2] = decode_normal(prim.normals.z);

	t.tangents[0] = decode_normal(prim.tangents.x);
	t.tangents[1] = decode_normal(prim.tangents.y);
	t.tangents[2] = decode_normal(prim.tangents.z);

	t.tex_coords0[0] = get_uv( prim.uv0, 0 );
	t.tex_coords0[1] = get_uv( prim.uv1, 0 );
	t.tex_coords0[2] = get_uv( prim.uv2, 0 );

	t.material_id = prim.material_id;
	t.shell = prim.shell;
	t.cluster = prim.cluster;
	t.instance_index = prim.instance;
	t.instance_prim = 0;

	// create_poly writes the stage colours of the BSP surfaces. The models have none.
	t.vertex_colors = buffer_idx == VERTEX_BUFFER_WORLD || buffer_idx == VERTEX_BUFFER_WORLD_D_MATERIAL
		|| buffer_idx == VERTEX_BUFFER_WORLD_D_GEOMETRY || buffer_idx == VERTEX_BUFFER_SUB_MODELS;
	if (t.vertex_colors)
	{
		t.color0 = mat3x4(unpackUnorm4x8(prim.color0[0]), unpackUnorm4x8(prim.color1[0]), unpackUnorm4x8(prim.color2[0]));
		t.color1 = mat3x4(unpackUnorm4x8(prim.color0[1]), unpackUnorm4x8(prim.color1[1]), unpackUnorm4x8(prim.color2[1]));
		t.color2 = mat3x4(unpackUnorm4x8(prim.color0[2]), unpackUnorm4x8(prim.color1[2]), unpackUnorm4x8(prim.color2[2]));
		t.color3 = mat3x4(unpackUnorm4x8(prim.color0[3]), unpackUnorm4x8(prim.color1[3]), unpackUnorm4x8(prim.color2[3]));
	}
	else
	{
		t.color0 = t.color1 = t.color2 = t.color3 = mat3x4(vec4(1.0), vec4(1.0), vec4(1.0));
	}
	
	vec2 emissive_and_alpha = unpackHalf2x16(prim.emissive_and_alpha);
	t.emissive_factor = emissive_and_alpha.x;
	t.alpha = emissive_and_alpha.y;

	return t;
}

uint animate_material( uint material, int frame );

Triangle
load_and_transform_triangle(int instance_idx, uint buffer_idx, uint prim_id)
{
	Triangle t = load_triangle(buffer_idx, prim_id);

	if (instance_idx >= 0)
	{
		// Instance of a static mesh: transform the vertices.

		ModelInstance mi = instance_buffer.model_instances[instance_idx];
		
		t.positions[0] = vec3(mi.transform * vec4(t.positions[0], 1.0));
		t.positions[1] = vec3(mi.transform * vec4(t.positions[1], 1.0));
		t.positions[2] = vec3(mi.transform * vec4(t.positions[2], 1.0));

		t.positions_prev[0] = vec3(mi.transform_prev * vec4(t.positions_prev[0], 1.0));
		t.positions_prev[1] = vec3(mi.transform_prev * vec4(t.positions_prev[1], 1.0));
		t.positions_prev[2] = vec3(mi.transform_prev * vec4(t.positions_prev[2], 1.0));

		t.normals[0] = normalize(vec3(mi.transform * vec4(t.normals[0], 0.0)));
		t.normals[1] = normalize(vec3(mi.transform * vec4(t.normals[1], 0.0)));
		t.normals[2] = normalize(vec3(mi.transform * vec4(t.normals[2], 0.0)));

		t.tangents[0] = normalize(vec3(mi.transform * vec4(t.tangents[0], 0.0)));
		t.tangents[1] = normalize(vec3(mi.transform * vec4(t.tangents[1], 0.0)));
		t.tangents[2] = normalize(vec3(mi.transform * vec4(t.tangents[2], 0.0)));

		if (mi.material != 0) {
			t.material_id = mi.material;
			t.shell = mi.shell;
		}
		t.material_id = animate_material(t.material_id, int(mi.alpha_and_frame >> 16));
		t.cluster = mi.cluster;
		t.emissive_factor = 1.0;
		t.alpha *= unpackHalf2x16(mi.alpha_and_frame).x;

		// Store the index of that instance and the prim offset relative to the instance.
		t.instance_index = uint(instance_idx);
		t.instance_prim = prim_id - mi.render_prim_offset;
	}
	else if (buffer_idx == VERTEX_BUFFER_INSTANCED)
	{
		// Instance of an animated or skinned mesh, coming from the primbuf.
		// In this case, `instance_idx` is -1 because it's not a static mesh, 
		// so load the original animated instance to find out its prim offset.

		ModelInstance mi = instance_buffer.model_instances[t.instance_index];
		t.instance_prim = prim_id - mi.render_prim_offset;
	}
	else if (buffer_idx >= VERTEX_BUFFER_WORLD && buffer_idx <= VERTEX_BUFFER_WORLD_D_GEOMETRY)
	{
		// Static BSP primitive.
#ifdef USE_MULTI_WORLD_BUFFERS
		t.instance_index = ~buffer_idx;
#else
		t.instance_index = ~0u;
#endif
		t.instance_prim = prim_id;

		if ( buffer_idx == VERTEX_BUFFER_WORLD_D_MATERIAL )
		{
			//t.material_id = 0;
		}
	}

	return t;
}


#ifndef VERTEX_READONLY
void
store_triangle(Triangle t, uint buffer_idx, uint prim_id)
{
	VboPrimitive prim;

	prim.pos0 = t.positions[0];
	prim.pos1 = t.positions[1];
	prim.pos2 = t.positions[2];

	prim.custom0 = packHalf4x16(vec4(t.positions_prev[0] - t.positions[0], 0));
	prim.custom1 = packHalf4x16(vec4(t.positions_prev[1] - t.positions[1], 0));
	prim.custom2 = packHalf4x16(vec4(t.positions_prev[2] - t.positions[2], 0));

	prim.normals.x = encode_normal(t.normals[0]);
	prim.normals.y = encode_normal(t.normals[1]);
	prim.normals.z = encode_normal(t.normals[2]);

	prim.tangents.x = encode_normal(t.tangents[0]);
	prim.tangents.y = encode_normal(t.tangents[1]);
	prim.tangents.z = encode_normal(t.tangents[2]);

	// Only the first slot holds texture coordinates: each bundle has its tcGen (bundle_uv).
	prim.uv0 = uvec4(packHalf2x16(t.tex_coords0[0]));
	prim.uv1 = uvec4(packHalf2x16(t.tex_coords0[1]));
	prim.uv2 = uvec4(packHalf2x16(t.tex_coords0[2]));

	prim.material_id = t.material_id;
	prim.shell = t.shell;
	prim.cluster = t.cluster;
	prim.instance = t.instance_index;
	prim.emissive_and_alpha = packHalf2x16(vec2(t.emissive_factor, t.alpha));
	
	primitive_buffers[nonuniformEXT(buffer_idx)].primitives[prim_id] = prim;

	if (buffer_idx == VERTEX_BUFFER_INSTANCED)
	{
		for (int vert = 0; vert < 3; vert++)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				instanced_position_buffer.positions[prim_id * 9 + vert * 3 + axis] 
					= t.positions[vert][axis];
			}
		}
	}
}
#endif

uint get_model_instance_shader_uint( in uint instance, in uint offset ) {
	return instance_buffer.model_instance_shader_data[nonuniformEXT(instance * INSTANCE_SHADER_UINTS + offset)];
}

// RB_CalcDisintegrateColors: the squared distance from the impact point (oldorigin, in model space) to the point
// minus the squared burn threshold. A negative value means the point is gone. Words RTX_DISTORT_FIRST + 0-2 hold
// the origin and + 3 the squared threshold, as floats, for an instance with RF_DISINTEGRATE1 or RF_DISINTEGRATE2.
float disintegration_distance( in uint instance, in vec3 world_position )
{
	ModelInstance mi = instance_buffer.model_instances[instance];
	vec3 local_position = vec3( inverse( mi.transform ) * vec4( world_position, 1.0 ) );
	vec3 delta = vec3(
		uintBitsToFloat( get_model_instance_shader_uint( instance, RTX_DISTORT_FIRST ) ),
		uintBitsToFloat( get_model_instance_shader_uint( instance, RTX_DISTORT_FIRST + 1u ) ),
		uintBitsToFloat( get_model_instance_shader_uint( instance, RTX_DISTORT_FIRST + 2u ) ) ) - local_position;

	return dot( delta, delta ) - uintBitsToFloat( get_model_instance_shader_uint( instance, RTX_DISTORT_FIRST + 3u ) );
}

uint get_material_uint( in uint material_index, in uint offset ) {
	return light_buffer.material_table[nonuniformEXT(material_index * MATERIAL_UINTS + offset)];
}

// A brush model instance can hold an animMap frame of its own: a door shows its lock state
// this way. Instance frame n + 1 picks the material of frame n, chained from the material
// of the shader; 0 leaves the animMaps to follow the time.
// Word 4: bits 12-23 first or next frame material, bits 24-30 frame count, bit 31 oneshot.
uint animate_material( uint material, int frame )
{
	uint mat_index = material & MATERIAL_INDEX_MASK;

	if ( frame <= 0 || mat_index == 0 )
		return material;

	uint chain = get_material_uint( mat_index, 4 );
	uint num_frames = ( chain >> 24 ) & 0x7fu;

	if ( num_frames <= 1 )
		return material;

	uint n = uint( frame - 1 );
	n = ( ( chain & 0x80000000u ) != 0u ) ? min( n, num_frames - 1u ) : n % num_frames;

	uint index = ( chain >> 12 ) & MATERIAL_INDEX_MASK;
	for ( uint i = 0u; i < n; i++ )
		index = ( get_material_uint( index, 4 ) >> 12 ) & MATERIAL_INDEX_MASK;

	return ( material & ~MATERIAL_INDEX_MASK ) | index;
}

// The material has a glow stage (word 5, bit 5). Its emission goes to the bloom.
bool is_glow_material( uint material_id )
{
	uint material_index = material_id & MATERIAL_INDEX_MASK;
	uint remapped = get_material_uint( material_index, 4 ) & MATERIAL_INDEX_MASK;

	if ( remapped > 0 )
		material_index = remapped;

	return ( get_material_uint( material_index, 5 ) & 0x20u ) != 0u;
}

MaterialStage get_material_stage( in uint material_index, in uint stage )
{
	return light_buffer.material_stages[nonuniformEXT(material_index * MAX_RTX_STAGES + stage)];
}

MaterialInfo get_material_info( uint material_id ) 
{
	uint material_index = material_id & MATERIAL_INDEX_MASK;

	uint data[MATERIAL_UINTS];

	uint remappedIndex = get_material_uint( material_index, 4 ) & MATERIAL_INDEX_MASK;

	if ( remappedIndex > 0 )
		material_index = remappedIndex;

	data[0] = get_material_uint( material_index, 0 );
	data[1] = get_material_uint( material_index, 1 );
	data[2] = get_material_uint( material_index, 2 );
	data[3] = get_material_uint( material_index, 3 );
	data[5] = get_material_uint(material_index, 5);

	MaterialInfo minfo;
	minfo.stage[0] = get_material_stage(material_index, 0);
	minfo.stage[1] = get_material_stage(material_index, 1);
	minfo.stage[2] = get_material_stage(material_index, 2);
	minfo.stage[3] = get_material_stage(material_index, 3);

	//minfo.base_texture		= data[0] & 0xffff;	// albedo
	minfo.base_texture		= minfo.stage[0].bundle[0].image;
	minfo.emissive_texture	= data[0] >> 16;	// emissive/glow
	minfo.normals_texture	= data[1] & 0xffff;	// normalmap
	minfo.physical_texture	= data[1] >> 16;	// rmo (physical)

	// swizzles to zwxy
	// keep it for compat with non rtx pbr implementation (readability)
	minfo.specular_scale[0] = unpackHalf2x16(data[2]).x;
	minfo.specular_scale[1] = unpackHalf2x16(data[2]).y;
	minfo.specular_scale[2] = unpackHalf2x16(data[3]).x;
	minfo.specular_scale[3] = unpackHalf2x16(data[3]).y;

	minfo.base_factor		= 1.0f;

	uint at = data[5];
	minfo.alpha_test_func  =  at        & 0x3u;
	minfo.blend_mode       = (at >> 2u) & uint(RTX_BLEND_MASK);
	minfo.alpha_test_value = unpackHalf2x16(at).y;

	// Bits 8-15: pt_glow_scale as e, 2^((e - 128) / 16), 0 for 1.0. Bit 5: a glow material, whose
	// emissive texture takes the scale too.
	uint emissive_code = (at >> 8u) & 0xffu;
	minfo.emission_scale  = (emissive_code == 0u) ? 1.0 : exp2((float(emissive_code) - 128.0) / 16.0);
	minfo.emissive_factor = ((at & 0x20u) != 0u) ? minfo.emission_scale : 1.0;

	return minfo;
}

#include "compute/tone_mapping_utils.glsl"

// The display value of a value v of the image, as tone_mapping_apply maps it: the adaptive
// curve (and its knee in SDR), blended with Reinhard by tm_reinhard, then tm_contrast. Before
// the clamp in SDR and the nits scale in HDR.
float tone_map_value( float v )
{
	v = max( v, exp2( min_log_luminance ) );

	float bin = clamp( ( log2( v ) * log_luminance_scale + log_luminance_bias ) * HISTOGRAM_BINS, 0.0, HISTOGRAM_BINS - 1.001 );
	uint left = uint( bin );
	float mapped = exp2( mix( tonemap_buffer.curve[left], tonemap_buffer.curve[left + 1], fract( bin ) ) + global_ubo.tm_exposure_bias );

	if ( global_ubo.tonemap_hdr == 0 && mapped >= global_ubo.tm_knee_start )
	{
		float knee_start = global_ubo.tm_knee_start;
		float knee_w = ( knee_start * ( knee_start - 2.0 ) + global_ubo.tm_white_point ) / ( global_ubo.tm_white_point - 1.0 );
		mapped = ( knee_w * mapped - knee_start * knee_start ) / max( 1e-6, mapped + knee_w - 2.0 * knee_start );
	}

	float s = exp2( global_ubo.tm_exposure_bias - 2.0 ) * v / max( tonemap_buffer.adapted_luminance, 1e-9 );
	float white_point = ( global_ubo.tonemap_hdr != 0 ) ? max( global_ubo.tm_white_point, global_ubo.tm_hdr_peak_nits / 80.0 ) : global_ubo.tm_white_point;
	float reinhard = s * ( 1.0 + s / ( white_point * white_point ) ) / ( 1.0 + s );

	float m = mix( mapped, reinhard, global_ubo.tm_reinhard );

	if ( global_ubo.tonemap_contrast != 1.0 )
		m = 0.18 * pow( max( m, 0.0 ) / 0.18, global_ubo.tonemap_contrast );

	return m;
}

// The value of the image that tone_mapping_apply shows as d. The mapping rises: bisection.
float inverse_tone_map_value( float d )
{
	if ( d <= 0.0 )
		return 0.0;

	float lo = min_log_luminance, hi = max_log_luminance;
	for ( int i = 0; i < 24; i++ )
	{
		float mid = 0.5 * ( lo + hi );
		if ( tone_map_value( exp2( mid ) ) < d )
			lo = mid;
		else
			hi = mid;
	}

	return exp2( 0.5 * ( lo + hi ) );
}

// A colour in screen units (what the rasterizer writes to the screen) to the HDR value that the
// tone mapper shows as this colour: the inverse of the tone mapping, not a scale. On the
// luminance, which keeps the hue, and on each channel with tm_per_channel, as the tone mapper.
vec3 screen_to_hdr_color( vec3 v )
{
	v = srgb_to_linear( max( v, vec3( 0.0 ) ) );

	vec3 hdr = vec3( 0.0 );
	float per_channel = global_ubo.tonemap_per_channel;

	if ( per_channel < 1.0 )
	{
		float d = luminance( v );
		if ( d > 0.0 )
			hdr = v * ( inverse_tone_map_value( d ) / d );
	}

	if ( per_channel > 0.0 )
		hdr = mix( hdr, vec3( inverse_tone_map_value( v.r ), inverse_tone_map_value( v.g ), inverse_tone_map_value( v.b ) ), per_channel );

	return hdr;
}

// One stage with its GL blend: out = L + T * dst.The destination alpha counts as 1.
void blend_stage_layer( uint blend, vec4 src, inout vec3 L, inout vec3 T )
{
	uint sf = blend & 0x0fu;
	uint df = ( blend >> 4 ) & 0x0fu;

	// No blendFunc: the stage replaces the color.
	if ( sf == 0u && df == 0u )
	{
		L = src.rgb;
		T = vec3( 0.0 );
		return;
	}

	vec3 Ls = vec3( 0.0 );
	vec3 Ts = vec3( 0.0 );

	switch ( df )
	{
		case 2u: Ts = vec3( 1.0 );			break;	// ONE
		case 3u: Ts = src.rgb;				break;	// SRC_COLOR
		case 4u: Ts = 1.0 - src.rgb;		break;	// ONE_MINUS_SRC_COLOR
		case 5u: Ts = vec3( src.a );		break;	// SRC_ALPHA
		case 6u: Ts = vec3( 1.0 - src.a );	break;	// ONE_MINUS_SRC_ALPHA
		case 7u: Ts = vec3( 1.0 );			break;	// DST_ALPHA
	}

	switch ( sf )
	{
		case 2u: Ls = src.rgb;						break;	// ONE
		case 3u: Ts += src.rgb;						break;	// DST_COLOR
		case 4u: Ls = src.rgb; Ts -= src.rgb;		break;	// ONE_MINUS_DST_COLOR
		case 5u: Ls = src.rgb * src.a;				break;	// SRC_ALPHA
		case 6u: Ls = src.rgb * ( 1.0 - src.a );	break;	// ONE_MINUS_SRC_ALPHA
		case 7u: Ls = src.rgb;						break;	// DST_ALPHA
	}

	L = Ls + Ts * L;
	T = Ts * T;
}

LightPolygon
get_light_polygon(uint index)
{
	vec4 p0 = vec4( light_buffer.light_polys[ index * LIGHT_POLY_VEC4S + 0 ] );
	vec4 p1 = vec4( light_buffer.light_polys[ index * LIGHT_POLY_VEC4S + 1 ] );
	vec4 p2 = vec4( light_buffer.light_polys[ index * LIGHT_POLY_VEC4S + 2 ] );
	vec4 p3 = vec4( light_buffer.light_polys[ index * LIGHT_POLY_VEC4S + 3 ] );

	LightPolygon light;

	light.positions			= mat3x3( p0.xyz, p1.xyz, p2.xyz );
	light.color				= vec3( p0.w, p1.w, p2.w );
	light.light_style_scale	= p3.x;
	light.prev_style_scale	= p3.y;
	light.type = p3.z;

	return light;
}

vec4 unpackColor(in uint color) {
	return vec4(
		color & 0xff,
		(color & (0xff << 8)) >> 8,
		(color & (0xff << 16)) >> 16,
		(color & (0xff << 24)) >> 24
	);
}
#endif
#endif
