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

// ========================================================================== //
// The radiance cache: a hash table of cells in world space.
//
// A cell holds D: the diffuse light that arrives at a point of a surface, without
// the albedo of the surface, in the units of bounce_diffuse of get_direct_illumination.
// It has the direct light and the light of all later bounces.
// The light that leaves the surface is emission + albedo * D.
//
// The key of a cell is its position, its level and the side of the surface.
// The level grows with the distance to the camera, so far cells are big.
//
// The include order is: global_ubo.h, utils.glsl, vertex_buffer.h, this file.
// ========================================================================== //

#ifndef RADIANCE_CACHE_H_
#define RADIANCE_CACHE_H_

layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_RC_TAGS )		buffer RC_TAG_BUFFER { uint tags[]; } rc_tags;				// the tag of the key of a cell, 0 means empty
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_RC_ACCUM )		buffer RC_ACCUM_BUFFER { uint sums[]; } rc_accum;			// per cell: R, G, B sums of this frame in fixed point, sample count
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_RC_RESOLVED )	buffer RC_RESOLVED_BUFFER { vec4 cells[]; } rc_resolved;	// per cell: D in rgb, weight in w
layout( set = VERTEX_BUFFER_DESC_SET_IDX, binding = BINDING_OFFSET_RC_LAST_FRAME )	buffer RC_LAST_FRAME_BUFFER { uint frames[]; } rc_last_frame;	// per cell: the frame of its last sample

// Only static world surfaces have cells. Models, movers and the sky change or are not lit.
bool
is_static_world(uint buffer_index)
{
	return buffer_index == VERTEX_BUFFER_WORLD || buffer_index == VERTEX_BUFFER_WORLD_D_MATERIAL;
}

uint
rc_mix(uint x)
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

// The level is 0 below pt_rc_scale_distance. It grows by 1 at each doubling of the distance.
uint
rc_get_level(vec3 position)
{
	float ratio = length(position - global_ubo.cam_pos.xyz) / max(global_ubo.pt_rc_scale_distance, 1.0);

	return uint(clamp(floor(log2(max(ratio, 0.5))) + 1.0, 0.0, float(RC_MAX_LEVEL)));
}

// The dominant axis of the normal and its sign: 6 values.
uint
rc_get_normal_code(vec3 normal)
{
	vec3 a = abs(normal);
	uint axis = (a.x >= a.y && a.x >= a.z) ? 0u : ((a.y >= a.z) ? 1u : 2u);

	return axis * 2u + ((normal[axis] < 0.0) ? 1u : 0u);
}

// The edge length of the cell at a position.
float
rc_get_voxel_size(vec3 position)
{
	return max(global_ubo.pt_rc_voxel_size, 0.001) * exp2(float(rc_get_level(position)));
}

// h1 selects the slot. h2 is the tag that the slot stores, it is never 0.
// The normal is the geometric normal, on the side that the ray came from.
void
rc_get_key(vec3 position, vec3 normal, out uint h1, out uint h2)
{
	uint level = rc_get_level(position);
	float voxel_size = rc_get_voxel_size(position);
	uvec3 q = uvec3(ivec3(floor(position / voxel_size)));
	uint meta = level | (rc_get_normal_code(normal) << 4);

	h1 = rc_mix(q.x + rc_mix(q.y + rc_mix(q.z + rc_mix(meta))));
	h2 = rc_mix(meta ^ 0x9e3779b9u);
	h2 = rc_mix(h2 ^ (q.z * 0x85ebca6bu));
	h2 = rc_mix(h2 ^ (q.y * 0xc2b2ae35u));
	h2 = rc_mix(h2 ^ (q.x * 0x27d4eb2fu));
	h2 = max(h2, 1u);
}

uint
rc_find(uint h1, uint h2)
{
	for(uint i = 0u; i < RC_PROBES; i++)
	{
		uint slot = (h1 + i) & (uint(RC_CAPACITY) - 1u);

		if(rc_tags.tags[slot] == h2)
			return slot;
	}

	return RC_INVALID_SLOT;
}

uint
rc_find_or_insert(uint h1, uint h2)
{
	for(uint i = 0u; i < RC_PROBES; i++)
	{
		uint slot = (h1 + i) & (uint(RC_CAPACITY) - 1u);
		uint old_tag = atomicCompSwap(rc_tags.tags[slot], 0u, h2);

		if(old_tag == 0u || old_tag == h2)
			return slot;
	}

	return RC_INVALID_SLOT;
}

// Adds one sample of D to the sums of this frame. The resolve pass takes it from there.
// A cell takes RC_MAX_SAMPLES_PER_FRAME samples: the sums cannot overflow.
void
rc_add_sample(uint h1, uint h2, vec3 radiance)
{
	uint slot = rc_find_or_insert(h1, h2);
	if(slot == RC_INVALID_SLOT)
		return;

	uint base = slot * 4u;
	if(atomicAdd(rc_accum.sums[base + 3u], 1u) >= RC_MAX_SAMPLES_PER_FRAME)
		return;

	uvec3 scaled = uvec3(radiance * RC_RADIANCE_SCALE + 0.5);
	atomicAdd(rc_accum.sums[base + 0u], scaled.x);
	atomicAdd(rc_accum.sums[base + 1u], scaled.y);
	atomicAdd(rc_accum.sums[base + 2u], scaled.z);
}

// The resolved D of a cell and its weight, in samples. False when the cell does not exist.
bool
rc_fetch(uint h1, uint h2, out vec3 radiance, out float weight)
{
	radiance = vec3(0);
	weight = 0.0;

	uint slot = rc_find(h1, h2);
	if(slot == RC_INVALID_SLOT)
		return false;

	vec4 cell = rc_resolved.cells[slot];
	radiance = cell.rgb;
	weight = cell.w;

	return true;
}

#endif // RADIANCE_CACHE_H_
