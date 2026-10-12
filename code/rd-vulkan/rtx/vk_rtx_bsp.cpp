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
#include "conversion.h"
#include "rtx_light_edit.h"
#include "rtx_light_file.h"
#include <algorithm>
#include <vector>
#include <unordered_map>

// uncomment the define to visualize polygonal lights by rednering debug triangles 
// the value represents the offset along the light�s normal direction
// #define DEBUG_POLY_LIGHTS 3.0f

static void vk_bind_storage_buffer( vkdescriptor_t *descriptor, uint32_t binding, VkShaderStageFlagBits stage, VkBuffer buffer )
{
	const uint32_t count = 1;

	assert( buffer != NULL );

	vk_rtx_add_descriptor_buffer( descriptor, count, binding, stage, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER );
	vk_rtx_set_descriptor_update_size( descriptor, binding, stage, count );
	vk_rtx_bind_descriptor_buffer( descriptor, binding, stage, buffer );
}

static void vk_bind_primitive_buffer( vkdescriptor_t *descriptor, uint32_t binding, VkShaderStageFlagBits stage )
{
	const uint32_t count = 7;

	vk_rtx_add_descriptor_buffer( descriptor, count, binding, stage, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER );
	vk_rtx_set_descriptor_update_size( descriptor, binding, stage, count );

	// world
	vk_rtx_bind_descriptor_buffer_element(descriptor, binding, stage, VERTEX_BUFFER_WORLD,				tr.world->geometry.world_static.buffer[0].buffer);
	vk_rtx_bind_descriptor_buffer_element(descriptor, binding, stage, VERTEX_BUFFER_SKY,				tr.world->geometry.sky_static.buffer[0].buffer);
	vk_rtx_bind_descriptor_buffer_element(descriptor, binding, stage, VERTEX_BUFFER_WORLD_D_MATERIAL,	tr.world->geometry.world_dynamic_material.buffer[0].buffer);
	vk_rtx_bind_descriptor_buffer_element(descriptor, binding, stage, VERTEX_BUFFER_WORLD_D_GEOMETRY,	tr.world->geometry.world_dynamic_geometry.buffer[0].buffer);
#ifdef DEBUG_POLY_LIGHTS
	vk_rtx_bind_descriptor_buffer_element(descriptor, binding, stage, VERTEX_BUFFER_DEBUG_LIGHT_POLYS,	tr.world->geometry.debug_light_polys.buffer[0].buffer);
#endif
	vk_rtx_bind_descriptor_buffer_element(descriptor, binding, stage, VERTEX_BUFFER_SUB_MODELS,			tr.world->geometry.world_submodels.buffer[0].buffer);
	
	// instanced
	vk_rtx_bind_descriptor_buffer_element(descriptor, binding, stage, VERTEX_BUFFER_INSTANCED,			vk.buf_primitive_instanced.buffer );
}

static void vk_bind_texel_buffer( vkdescriptor_t *descriptor, uint32_t binding, VkShaderStageFlagBits stage, VkBufferView view )
{
	const uint32_t count = 1;

	vk_rtx_add_descriptor_buffer( descriptor, count, binding, stage, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER );
	vk_rtx_set_descriptor_update_size( descriptor, binding, stage, count );
	vk_rtx_bind_descriptor_buffer_view( descriptor, binding, stage, view );
}

static void vk_bind_uniform_buffer( vkdescriptor_t *descriptor, uint32_t binding, VkShaderStageFlagBits stage, VkBuffer buffer )
{
	const uint32_t count = 1;

	assert( buffer != NULL );

	vk_rtx_add_descriptor_buffer( descriptor, count, binding, stage, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	vk_rtx_set_descriptor_update_size( descriptor, binding, stage, count );
	vk_rtx_bind_descriptor_buffer( descriptor, binding, stage, buffer );
}

static void vk_bind_storage_image( vkdescriptor_t *descriptor, uint32_t binding, VkShaderStageFlagBits stage, VkImageView view )
{
	vk_rtx_add_descriptor_image( descriptor, binding, stage );
	vk_rtx_bind_descriptor_image_sampler( descriptor, binding, stage, VK_NULL_HANDLE, view, 0 );
}

static void vk_bind_sampler( vkdescriptor_t *descriptor, uint32_t binding, VkShaderStageFlagBits stage, VkSampler sampler, VkImageView view, VkImageLayout layout )
{
	const uint32_t count = 1;

	vk_rtx_add_descriptor_sampler( descriptor, binding, stage, count, layout ); 
	vk_rtx_set_descriptor_update_size( descriptor, binding, stage, count );
	vk_rtx_bind_descriptor_image_sampler( descriptor, binding, stage, sampler, view, 0 );
}

static void vk_create_vertex_buffer_descriptor( world_t& worldData, uint32_t index, uint32_t prev_index ) 
{	
	vkdescriptor_t *descriptor = vk_rtx_init_descriptor( & vk.desc_set_vertex_buffer[index] );

	vk_bind_primitive_buffer( descriptor, PRIMITIVE_BUFFER_BINDING_IDX,				VK_SHADER_STAGE_ALL );
	vk_bind_storage_buffer( descriptor, POSITION_BUFFER_BINDING_IDX,				VK_SHADER_STAGE_ALL, vk.buf_positions_instanced.buffer );

	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_READBACK_BUFFER,				VK_SHADER_STAGE_ALL, vk.buf_readback.buffer );
	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_DYNAMIC_VERTEX,				VK_SHADER_STAGE_ALL, vk.buf_readback.buffer );	// can be reomved
	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_LIGHT_BUFFER,				VK_SHADER_STAGE_ALL, vk.buf_light.buffer );
	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_TONEMAP_BUFFER,				VK_SHADER_STAGE_ALL, vk.buf_tonemap.buffer );
	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_SUN_COLOR_BUFFER,			VK_SHADER_STAGE_ALL, vk.buf_sun_color.buffer );
	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_MDXM_BONE_BUFFER,			VK_SHADER_STAGE_ALL, vk.buf_mdxm_matrices.buffer );
	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_RC_TAGS,						VK_SHADER_STAGE_ALL, vk.buf_rc_tags.buffer );
	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_RC_ACCUM,					VK_SHADER_STAGE_ALL, vk.buf_rc_accum.buffer );
	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_RC_RESOLVED,					VK_SHADER_STAGE_ALL, vk.buf_rc_resolved.buffer );
	vk_bind_storage_buffer( descriptor, BINDING_OFFSET_RC_LAST_FRAME,				VK_SHADER_STAGE_ALL, vk.buf_rc_last_frame.buffer );
	vk_bind_uniform_buffer( descriptor, BINDING_OFFSET_SUN_COLOR_UBO,				VK_SHADER_STAGE_ALL, vk.buf_sun_color.buffer );

	// light stats
	{
		uint32_t i, light_stats_index;
		light_stats_index = vk_rtx_add_descriptor_buffer( descriptor, NUM_LIGHT_STATS_BUFFERS, BINDING_OFFSET_LIGHT_STATS_BUFFER, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER );
		vk_rtx_set_descriptor_update_size( descriptor, BINDING_OFFSET_LIGHT_STATS_BUFFER, VK_SHADER_STAGE_ALL, NUM_LIGHT_STATS_BUFFERS );

		for (i = 0; i < NUM_LIGHT_STATS_BUFFERS; i++)
		{
			assert( vk.buf_light_stats[i].buffer != NULL );

			descriptor->data[light_stats_index].buffer[i] = { vk.buf_light_stats[i].buffer, 0, vk.buf_light_stats[i].size };
		}
	}

	// light count history
	{
		uint32_t i, light_count_history_index;
		light_count_history_index = vk_rtx_add_descriptor_buffer( descriptor, LIGHT_COUNT_HISTORY, BINDING_LIGHT_COUNTS_HISTORY_BUFFER, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER );
		vk_rtx_set_descriptor_update_size( descriptor, BINDING_LIGHT_COUNTS_HISTORY_BUFFER, VK_SHADER_STAGE_ALL, LIGHT_COUNT_HISTORY );

		for ( i = 0; i < LIGHT_COUNT_HISTORY; i++ ) 
		{
			assert( vk.buf_light_counts_history[i].buffer != NULL );

			descriptor->data[light_count_history_index].buffer[i] = { 
				vk.buf_light_counts_history[i].buffer, 
				0, 
				vk.buf_light_counts_history[i].size 
			};
		}
	}

	vk_rtx_create_descriptor( descriptor );
	vk_rtx_update_descriptor( descriptor );
}

static void vk_create_rt_descriptor( uint32_t index, uint32_t prev_index ) 
{
	VkAccelerationStructureKHR tlas[TLAS_COUNT];
	Com_Memset(tlas, 0, sizeof(VkAccelerationStructureKHR) * TLAS_COUNT);
	tlas[TLAS_INDEX_GEOMETRY] = vk.tlas_geometry[index].accel;
	tlas[TLAS_INDEX_EFFECTS] = vk.tlas_effects[index].accel;

	vkdescriptor_t *descriptor = vk_rtx_init_descriptor( &vk.rt_descriptor_set[index] );

	vk_rtx_add_descriptor_as( descriptor, RAY_GEN_ACCEL_STRUCTURE_BINDING_IDX, VK_SHADER_STAGE_RAYGEN_BIT_KHR, TLAS_COUNT );
	vk_rtx_bind_descriptor_as( descriptor, RAY_GEN_ACCEL_STRUCTURE_BINDING_IDX, VK_SHADER_STAGE_RAYGEN_BIT_KHR, tlas, TLAS_COUNT );

	VkBufferView particle_color_buffer_view = get_transparency_particle_color_buffer_view();
	VkBufferView beam_color_buffer_view		= get_transparency_beam_color_buffer_view();
	VkBufferView sprite_info_buffer_view	= get_transparency_sprite_info_buffer_view();
	VkBufferView beam_intersect_buffer_view = get_transparency_beam_intersect_buffer_view();

	vk_bind_texel_buffer( descriptor, RAY_GEN_PARTICLE_COLOR_BUFFER_BINDING_IDX,	VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		particle_color_buffer_view );
	vk_bind_texel_buffer( descriptor, RAY_GEN_BEAM_COLOR_BUFFER_BINDING_IDX,		VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		beam_color_buffer_view );
	vk_bind_texel_buffer( descriptor, RAY_GEN_SPRITE_INFO_BUFFER_BINDING_IDX,		VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		sprite_info_buffer_view );
	vk_bind_texel_buffer( descriptor, RAY_GEN_BEAM_INTERSECT_BUFFER_BINDING_IDX,	VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	beam_intersect_buffer_view );

	vk_rtx_create_descriptor( descriptor );
	vk_rtx_update_descriptor( descriptor );
}

void vk_rtx_create_rt_descriptors( world_t& worldData )
{
	uint32_t i, prev_index;

	for ( i = 0; i < vk.swapchain_image_count; i++ ) 
	{
		prev_index = (i + (vk.swapchain_image_count - 1)) % vk.swapchain_image_count;

		vk_create_rt_descriptor( i, prev_index );
		vk_create_vertex_buffer_descriptor( worldData, i, prev_index );
	}
}

void vk_rtx_destroy_rt_descriptors( void )
{
	uint32_t i;

	for ( i = 0; i < vk.swapchain_image_count; i++ ) 
	{
		vk_rtx_destroy_descriptor( &vk.rt_descriptor_set[i] );
		vk_rtx_destroy_descriptor( &vk.desc_set_vertex_buffer[i] );
	}
}

static void vk_rtx_create_primary_rays_resources( world_t& worldData ) 
{
	//vkpt_physical_sky_initialize();

	vk_rtx_create_rt_descriptors( worldData);
	vk_rtx_radiance_cache_invalidate();
	vk_rtx_restir_gi_invalidate();

	vk_rtx_create_shader_modules();
	vk_rtx_create_rt_pipelines();

	vk_load_final_blit_shader();
	vk_rtx_create_final_blit_pipeline();

	vk_rtx_create_compute_pipelines();
}

void vk_rtx_destroy_primary_rays_resources( void ) 
{
	//vkpt_physical_sky_destroy();

	vk_rtx_destroy_compute_pipelines();
	vk_rtx_destroy_rt_pipelines();

	vk_rtx_destroy_rt_descriptors();

	vk_rtx_destroy_accel_all();

	vk_rtx_clear_material_list();
}

//
// bsp loading
//

#define PlaneDiff(v,p)   (DotProduct(v,(p)->normal)-(p)->dist)

typedef struct {
    vec3_t position;  // 3 floats: x, y, z
    vec2_t st;        // 2 floats: texture coords u, v
} tri_vertex_t;

typedef struct {
	float x, y;

} point2_t;
#define MAX_POLY_VERTS 32
typedef struct {
	point2_t v[MAX_POLY_VERTS];
	int len;
} rt_poly_t;

static inline float
dot2(point2_t* a, point2_t* b)
{
	return a->x * b->x + a->y * b->y;
}

static inline float
cross2(point2_t* a, point2_t* b)
{
	return a->x * b->y - a->y * b->x;
}

static inline point2_t
vsub(point2_t* a, point2_t* b)
{
	point2_t res;
	res.x = a->x - b->x;
	res.y = a->y - b->y;
	return res;
}

/* tells if vec c lies on the left side of directed edge a->b
 * 1 if left, -1 if right, 0 if colinear
 */
static int
left_of(point2_t* a, point2_t* b, point2_t* c)
{
	float x;
	point2_t tmp1 = vsub(b, a);
	point2_t tmp2 = vsub(c, b);
	x = cross2(&tmp1, &tmp2);
	return x < 0 ? -1 : x > 0;
}

static int
line_sect(point2_t* x0, point2_t* x1, point2_t* y0, point2_t* y1, point2_t* res)
{
	point2_t dx, dy, d;
	dx = vsub(x1, x0);
	dy = vsub(y1, y0);
	d = vsub(x0, y0);
	/* x0 + a dx = y0 + b dy ->
	   x0 X dx = y0 X dx + b dy X dx ->
	   b = (x0 - y0) X dx / (dy X dx) */
	float dyx = cross2(&dy, &dx);
	if (!dyx) return 0;
	dyx = cross2(&d, &dx) / dyx;
	if (dyx <= 0 || dyx >= 1) return 0;

	res->x = y0->x + dyx * dy.x;
	res->y = y0->y + dyx * dy.y;
	return 1;
}

static void
poly_append(rt_poly_t* p, point2_t* v)
{
	assert(p->len < MAX_POLY_VERTS - 1);
	p->v[p->len++] = *v;
}

static int
poly_winding(rt_poly_t* p)
{
	return left_of(p->v, p->v + 1, p->v + 2);
}

static void
poly_edge_clip(rt_poly_t* sub, point2_t* x0, point2_t* x1, int left, rt_poly_t* res)
{
	int i, side0, side1;
	point2_t tmp;
	point2_t* v0 = sub->v + sub->len - 1;
	point2_t* v1;
	res->len = 0;

	side0 = left_of(x0, x1, v0);
	if (side0 != -left) poly_append(res, v0);

	for (i = 0; i < sub->len; i++) {
		v1 = sub->v + i;
		side1 = left_of(x0, x1, v1);
		if (side0 + side1 == 0 && side0)
			/* last point and current straddle the edge */
			if (line_sect(x0, x1, v0, v1, &tmp))
				poly_append(res, &tmp);
		if (i == sub->len - 1) break;
		if (side1 != -left) poly_append(res, v1);
		v0 = v1;
		side0 = side1;
	}
}

static inline vec_t PlaneDiffFast( vec3_t v, cplane_t *p )
{
    // fast axial cases
    if ( p->type < 3 )
        return v[p->type] - p->dist;

    // slow generic case
    return PlaneDiff( v, p );
}

// similar to R_PointInLeaf
mnode_t *BSP_PointLeaf( mnode_t *node, vec3_t p )
{
    float d;

    while ( node->plane ) 
	{
		//if ( node->contents != -1 )
		//	break;
        d = PlaneDiffFast( p, node->plane );
        if ( d < 0 )
            node = node->children[1];
        else
            node = node->children[0];
    }

    return node;

}

int vk_get_surface_cluster( world_t &worldData, surfaceType_t *surf ) 
{
	int i, j;

	for ( i = 0; i < worldData.numnodes; i++) 
	{
		mnode_t* node = &worldData.nodes[i];

		if ( node->contents == -1 ) 
			continue;

		msurface_t** mark = node->firstmarksurface;

		for ( j = 0; j < node->nummarksurfaces; j++ )
		{
			if ( mark[j]->data == surf ) 
				return node->cluster;
		}
	}

	return -1;
}

#define FOREACH_BIT_BEGIN(SET,ROWSIZE,VAR) \
	for (int _byte_idx = 0; _byte_idx < ROWSIZE; _byte_idx++) { \
	if (SET[_byte_idx]) { \
		for (int _bit_idx = 0; _bit_idx < 8; _bit_idx++) { \
			if (SET[_byte_idx] & (1 << _bit_idx)) { \
				int VAR = (_byte_idx << 3) | _bit_idx;

#define FOREACH_BIT_END  } } } }

// Computes a point at a small distance above the center of the triangle.
// Returns qfalse if the triangle is degenerate, qtrue otherwise.
void get_triangle_norm( const float* positions, float* normal )
{
	const float* v0 = positions + 0;
	const float* v1 = positions + 3;
	const float* v2 = positions + 6;


	// Compute the normal

	vec3_t e1, e2;
	VectorSubtract(v1, v0, e1);
	VectorSubtract(v2, v0, e2);
	CrossProduct(e1, e2, normal);
	VectorNormalize(normal);
}


#if 0
void vk_compute_cluster_aabbs( world_t &worldData, mnode_t* node, int numClusters ) 
{
	if (node->contents == -1) {
		vk_compute_cluster_aabbs( worldData, node->children[0], numClusters );
		vk_compute_cluster_aabbs( worldData, node->children[1], numClusters );
		return;
	}

	if ( node->cluster < 0 || node->cluster >= numClusters )
		return;

	aabb_t *aabb = worldData.cluster_aabbs + node->cluster;

	aabb->mins[0] = MIN(aabb->mins[0], node->mins[0]);
	aabb->mins[1] = MIN(aabb->mins[1], node->mins[1]);
	aabb->mins[2] = MIN(aabb->mins[2], node->mins[2]);

	aabb->maxs[0] = MAX(aabb->maxs[0], node->maxs[0]);
	aabb->maxs[1] = MAX(aabb->maxs[1], node->maxs[1]);
	aabb->maxs[2] = MAX(aabb->maxs[2], node->maxs[2]);

}
#else
static void
vk_compute_cluster_aabbs( world_t &worldData )
{
	worldData.cluster_aabbs	= (aabb_t*)calloc(worldData.numClusters, sizeof(aabb_t));
	for ( int i = 0; i < worldData.numClusters; i++ ) 
	{
		VectorSet(worldData.cluster_aabbs[i].mins, FLT_MAX, FLT_MAX, FLT_MAX );
		VectorSet(worldData.cluster_aabbs[i].maxs, -FLT_MAX, -FLT_MAX, -FLT_MAX );
	}

	for (uint32_t prim_idx = 0; prim_idx < worldData.geometry.world_static.geom_opaque.prim_counts[0]; prim_idx++)
	{
		int c = worldData.geometry.world_static.primitives[prim_idx].cluster;

		if(c < 0 || c >= worldData.numClusters)
			continue;

		aabb_t* aabb = worldData.cluster_aabbs + c;
		
		const VboPrimitive* prim = worldData.geometry.world_static.primitives + prim_idx;

		for (int i = 0; i < 3; i++)
		{
			const float* position;
			switch(i)
			{
			case 0:  position = prim->pos0; break;
			case 1:  position = prim->pos1; break;
			default: position = prim->pos2; break;
			}

			aabb->mins[0] = MIN(aabb->mins[0], position[0]);
			aabb->mins[1] = MIN(aabb->mins[1], position[1]);
			aabb->mins[2] = MIN(aabb->mins[2], position[2]);

			aabb->maxs[0] = MAX(aabb->maxs[0], position[0]);
			aabb->maxs[1] = MAX(aabb->maxs[1], position[1]);
			aabb->maxs[2] = MAX(aabb->maxs[2], position[2]);
		}
	}
}
#endif

void append_aabb( const VboPrimitive* primitives, uint32_t numprims, float* aabb_min, float* aabb_max )
{
	for ( uint32_t prim_idx = 0; prim_idx < numprims; prim_idx++ )
	{
		const VboPrimitive* prim = primitives + prim_idx;

		for ( uint32_t vert_idx = 0; vert_idx < 3; vert_idx++ )
		{
			const float* position;
			switch (vert_idx)
			{
			case 0:  position = prim->pos0; break;
			case 1:  position = prim->pos1; break;
			default: position = prim->pos2; break;
			}

			aabb_min[0] = MIN(aabb_min[0], position[0]);
			aabb_min[1] = MIN(aabb_min[1], position[1]);
			aabb_min[2] = MIN(aabb_min[2], position[2]);

			aabb_max[0] = MAX(aabb_max[0], position[0]);
			aabb_max[1] = MAX(aabb_max[1], position[1]);
			aabb_max[2] = MAX(aabb_max[2], position[2]);
		}
	}
}

void compute_aabb( const VboPrimitive* primitives, uint32_t numprims, float* aabb_min, float* aabb_max )
{
	VectorSet( aabb_min, FLT_MAX, FLT_MAX, FLT_MAX );
	VectorSet( aabb_max, -FLT_MAX, -FLT_MAX, -FLT_MAX );

	append_aabb( primitives, numprims, aabb_min, aabb_max );
}

#if 0
static void
remove_collinear_edges(float* positions, float* tex_coords, int* num_vertices)
{
	int num_vertices_local = *num_vertices;

	for (int i = 1; i < num_vertices_local;)
	{
		float* p0 = positions + (i - 1) * 3;
		float* p1 = positions + (i % num_vertices_local) * 3;
		float* p2 = positions + ((i + 1) % num_vertices_local) * 3;

		vec3_t e1, e2;
		VectorSubtract(p1, p0, e1);
		VectorSubtract(p2, p1, e2);
		float l1 = VectorLength(e1);
		float l2 = VectorLength(e2);

		qboolean remove = qfalse;
		if (l1 == 0)
		{
			remove = qtrue;
		}
		else if (l2 > 0)
		{
			VectorScale(e1, 1.f / l1, e1);
			VectorScale(e2, 1.f / l2, e2);

			float dot = DotProduct(e1, e2);
			if (dot > 0.999f)
				remove = qtrue;
		}

		if (remove)
		{
			if (num_vertices_local - i >= 1)
			{
				memcpy(p1, p2, (num_vertices_local - i - 1) * 3 * sizeof(float));

				if (tex_coords)
				{
					float* t1 = tex_coords + (i % num_vertices_local) * 2;
					float* t2 = tex_coords + ((i + 1) % num_vertices_local) * 2;
					memcpy(t1, t2, (num_vertices_local - i - 1) * 2 * sizeof(float));
				}
			}

			num_vertices_local--;
		}
		else
		{
			i++;
		}
	}

	*num_vertices = num_vertices_local;
}
#endif

static int belongs_to_model( world_t &worldData, msurface_t *surf )
{
	for (int i = 0; i < worldData.num_bmodels; i++) 
	{
		if ( i == 0 )
			continue;	// 0 = world ?

		if (surf >= worldData.bmodels[i].firstSurface
		&& surf < worldData.bmodels[i].firstSurface + worldData.bmodels[i].numSurfaces)
			return 1;
	}
	return 0;
}

// Computes a point at a small distance above the center of the triangle.
// Returns qfalse if the triangle is degenerate, qtrue otherwise.
qboolean 
get_triangle_off_center(const float* positions, float* center, float* anti_center, float offset )
{
	const float* v0 = positions + 0;
	const float* v1 = positions + 3;
	const float* v2 = positions + 6;

	// Compute the triangle center

	VectorCopy(v0, center);
	VectorAdd(center, v1, center);
	VectorAdd(center, v2, center);
	VectorScale(center, 1.f / 3.f, center);

	// Compute the normal

	vec3_t e1, e2, normal;
	VectorSubtract(v1, v0, e1);
	VectorSubtract(v2, v0, e2);
	CrossProduct(e1, e2, normal);
	float length = VectorNormalize(normal);

	// Offset the center by one normal to make sure that the point is
	// inside a BSP leaf and not on a boundary plane.
	VectorScale(normal, offset, normal);
	VectorAdd(center, normal, center);

	if (anti_center)
	{
		VectorMA(center, -2.f, normal, anti_center);
	}

	return (qboolean)(length > 0.f);
}

static bool
get_surf_plane_equation( float* plane, tri_vertex_t *vertices, int *indices, int numIndices, int numVertices )
{
	// Go over multiple planes defined by different edge pairs of the surface.
	// Some of the edges may be collinear or almost-collinear to each other,
	// so we can't just take the first pair of edges.
	// We can't even take the first pair of edges with nonzero cross product
	// because of numerical issues - they may be almost-collinear.
	// So this function finds the plane equation from the triangle with the
	// largest area, i.e. the longest cross product.
	int i0, i1, i2;
	float *v0, *v1, *v2;

	float maxlen = 0.f;
	for ( int  i = 0; i < numIndices; i += 3 )
	{
		// mostly likely incorrect,dot it later
		i0 = indices[i + 0];
		i1 = indices[i + 1];
		i2 = indices[i + 2];

		if ( i0 >= numVertices || i1 >= numVertices || i2 >= numVertices )
			continue;

		//float* v0 = surf->points[i0];
		//float* v1 = surf->points[i1];
		//float* v2 = surf->points[i2];
		v0 = vertices[i0].position;
		v1 = vertices[i1].position;
		v2 = vertices[i2].position;


		vec3_t e0, e1;
		VectorSubtract(v1, v0, e0);
		//VectorSubtract(v2, v1, e1);
		VectorSubtract(v2, v0, e1);
		vec3_t normal;
		CrossProduct(e0, e1, normal);
		float len = VectorLength(normal);
		if (len > maxlen)
		{
			VectorScale(normal, 1.0f / len, plane);
			plane[3] = -DotProduct(plane, v0);
			maxlen = len;
		}
	}
	return (maxlen > 0.f);
}

static void
clip_polygon(rt_poly_t* input, rt_poly_t* clipper, rt_poly_t* output)
{
	int i;
	rt_poly_t p1_m, p2_m;
	rt_poly_t* p1 = &p1_m;
	rt_poly_t* p2 = &p2_m;
	rt_poly_t* tmp;

	int dir = poly_winding(clipper);
	poly_edge_clip(input, clipper->v + clipper->len - 1, clipper->v, dir, p2);
	for (i = 0; i < clipper->len - 1; i++) {
		tmp = p2; p2 = p1; p1 = tmp;
		if (p1->len == 0) {
			p2->len = 0;
			break;
		}
		poly_edge_clip(p1, clipper->v + i, clipper->v + i + 1, dir, p2);
	}

	output->len = p2->len;
	if (output->len)
		memcpy(output->v, p2->v, p2->len * sizeof(point2_t));
}

static light_poly_t*
append_light_poly( int *num_lights, int *allocated, light_poly_t **lights )
{
	if (*num_lights == *allocated)
	{
		*allocated = MAX(*allocated * 2, 128);

		// fail ?
		*lights = (light_poly_t*)realloc(*lights, *allocated * sizeof(light_poly_t));
	}
	return *lights + (*num_lights)++;
}

static void get_surface_light_contribution( int surfaceLight, light_poly_t *light )
{
	if ( surfaceLight <= 0 || !light )
		return;

	vec3_t v0, v1, v2;
	VectorCopy( light->positions + 0, v0 );
	VectorCopy( light->positions + 3, v1 );
	VectorCopy( light->positions + 6, v2 );

	float scale = 1.5f;

	vec3_t e0, e1, cross;
	VectorSubtract( v1, v0, e0 );
	VectorSubtract( v2, v0, e1 );
	CrossProduct( e0, e1, cross );

	float area = 0.5f * VectorLength( cross );

	// optional: normalize area to avoid huge surfaces dominating
	const float max_expected_area = 2048.0f;
	float normalized_area = fminf( area / max_expected_area, 1.0f );

	// compress surfacelight values using logarithmic scale
	float surfacelight_scale = log2f((float)surfaceLight + 1.0f); // log2(1 + x) to avoid log(0)

	light->emissive_factor += surfacelight_scale * normalized_area * scale;
	light->emissive_factor = MIN( light->emissive_factor, 4.0f );
}

static void collect_one_light_poly_entire_texture(  world_t &worldData, rtx_material_t *material, int model_idx, 
													const vec3_t light_color, float emissive_factor, int light_style,
													int* num_lights, int* allocated_lights, light_poly_t** lights,
													tri_vertex_t *vertices, int *indices, int numIndices, int numVertices )
{
	float *v0, *v1, *v2;
	int i, i0, i1, i2;
	vec3_t e0, e1, normal;

	for ( i = 0; i < numIndices; i += 3 )
	{
		i0 = indices[i + 0];
		i1 = indices[i + 1];
		i2 = indices[i + 2];

		if ( i0 >= numVertices || i1 >= numVertices || i2 >= numVertices )
			continue;

		v0 = vertices[i0].position;
		v1 = vertices[i1].position;
		v2 = vertices[i2].position;

		light_poly_t light;
		VectorCopy( v0, light.positions + 0 );
		VectorCopy( v2, light.positions + 3 );
		VectorCopy( v1, light.positions + 6 );

		// add offset
		VectorSubtract( v1, v0, e0 );
		VectorSubtract( v2, v0, e1 );
		CrossProduct( e0, e1, normal );
		VectorNormalize( normal );
		const float offset = -0.05f; 
		for ( int j = 0; j < 3; j++ ) 
		{
			VectorMA(light.positions + j * 3, offset, normal, light.positions + j * 3);
		}

		VectorScale( light_color, emissive_factor, light.color );

		light.material = material;
		light.style = light_style;
		light.type = LIGHT_POLYGON;
		light.emissive_factor = emissive_factor;

		if ( material->surface_light > 0 )
			get_surface_light_contribution( material->surface_light, &light );

		if ( !get_triangle_off_center( light.positions, light.off_center, NULL, 1.f ) )
			continue;

		if ( model_idx >= 0 )
			light.cluster = -1;
		else
			light.cluster = BSP_PointLeaf( worldData.nodes, light.off_center )->cluster;

		if ( model_idx >= 0 || light.cluster >= 0 )
		{
			light_poly_t *list_light = append_light_poly( num_lights, allocated_lights, lights );
			memcpy(list_light, &light, sizeof(light_poly_t));
		}
	}
}

static qboolean vk_rtx_compute_bary_weights( const point2_t p, const vec2_t a, const vec2_t b, const vec2_t c, vec3_t weights )
{
	float v0[2], v1[2], v2[2];
	float d00, d01, d11, d20, d21;
	float denom, inv_denom;
	float u, v, w;

	// edge vectors
	v0[0] = b[0] - a[0]; v0[1] = b[1] - a[1];
	v1[0] = c[0] - a[0]; v1[1] = c[1] - a[1];
	v2[0] = p.x - a[0];  v2[1] = p.y - a[1];

	d00 = v0[0] * v0[0] + v0[1] * v0[1];
	d01 = v0[0] * v1[0] + v0[1] * v1[1];
	d11 = v1[0] * v1[0] + v1[1] * v1[1];
	d20 = v2[0] * v0[0] + v2[1] * v0[1];
	d21 = v2[0] * v1[0] + v2[1] * v1[1];

	// return false when barycentric coordinates is degnerate (zero area)
	denom = d00 * d11 - d01 * d01;
	if ( fabsf( denom ) < 1e-8f )
		return qfalse;

	inv_denom = 1.0f / denom;
	v = (d11 * d20 - d01 * d21) * inv_denom;
	w = (d00 * d21 - d01 * d20) * inv_denom;
	u = 1.0f - v - w;

	weights[0] = u;
	weights[1] = v;
	weights[2] = w;

	return qtrue;
}

static void collect_one_light_poly( world_t &worldData, rtx_material_t *material, int model_idx,
									const vec2_t min_light_texcoord, const vec2_t max_light_texcoord,
									const vec3_t light_color, float emissive_factor, int light_style,
									int *num_lights, int *allocated_lights, light_poly_t **lights,
									tri_vertex_t *vertices, int *indices, int numIndices, int numVertices )
{
	float *v0, *v1, *v2;
	int i, j, k, i0, i1, i2, axis;
	int tile_u, tile_v, min_tile_u, max_tile_u, min_tile_v, max_tile_v;
	vec2_t uv0, uv1, uv2, tri_min_uv, tri_max_uv;
	vec2_t min_tc, max_tc;
	vec3_t e0, e1, normal, weights;
	vec3_t tri_positions[3], instance_positions[MAX_POLY_VERTS];
	rt_poly_t tri_uv_poly, clipper, clipped;
	
	for ( i = 0; i < numIndices; i += 3 ) 
	{
		i0 = indices[i + 0];
		i1 = indices[i + 1];
		i2 = indices[i + 2];

		if ( i0 >= numVertices || i1 >= numVertices || i2 >= numVertices )
			continue;

		v0 = vertices[i0].position;
		v1 = vertices[i1].position;
		v2 = vertices[i2].position;

		VectorSet2( uv0, vertices[i0].st[0], vertices[i0].st[1] );
		VectorSet2( uv1, vertices[i1].st[0], vertices[i1].st[1] );
		VectorSet2( uv2, vertices[i2].st[0], vertices[i2].st[1] );

		// get UV aabb
		VectorSet2( tri_min_uv, 
			fminf( fminf( uv0[0], uv1[0] ), uv2[0] ),
			fminf( fminf( uv0[1], uv1[1] ), uv2[1] )
		);
		VectorSet2( tri_max_uv, 
			fmaxf( fmaxf( uv0[0], uv1[0] ), uv2[0] ),
			fmaxf( fmaxf( uv0[1], uv1[1] ), uv2[1] )
		);

		min_tile_u = (int)floorf( tri_min_uv[0] );
		max_tile_u = (int)ceilf( tri_max_uv[0] );
		min_tile_v = (int)floorf( tri_min_uv[1] );
		max_tile_v = (int)ceilf( tri_max_uv[1] );

		tri_uv_poly.len = 3;
		tri_uv_poly.v[0] = { uv0[0], uv0[1] };
		tri_uv_poly.v[1] = { uv1[0], uv1[1] };
		tri_uv_poly.v[2] = { uv2[0], uv2[1] };

		// world-space positions
		VectorCopy( v0, tri_positions[0] );
		VectorCopy( v1, tri_positions[1] );
		VectorCopy( v2, tri_positions[2] );

		for ( tile_u = min_tile_u; tile_u < max_tile_u; tile_u++ ) 
		{
			for ( tile_v = min_tile_v; tile_v < max_tile_v; tile_v++ ) 
			{
				// glow bounds for this tile
				VectorSet2( min_tc, tile_u + min_light_texcoord[0], tile_v + min_light_texcoord[1] );
				VectorSet2( max_tc, tile_u + max_light_texcoord[0], tile_v + max_light_texcoord[1] );

				clipper.len = 4;
				clipper.v[0] = { min_tc[0], min_tc[1] };
				clipper.v[1] = { max_tc[0], min_tc[1] };
				clipper.v[2] = { max_tc[0], max_tc[1] };
				clipper.v[3] = { min_tc[0], max_tc[1] };

				clip_polygon( &tri_uv_poly, &clipper, &clipped );
				if (clipped.len < 3)
					continue;

				// reproject each clipped vertex from UV to world-space using barycentric interpolation
				for (  j = 0; j < clipped.len; j++ ) {
					Com_Memset( instance_positions[j], 0, sizeof(vec3_t) );

					if ( !vk_rtx_compute_bary_weights( clipped.v[j], uv0, uv1, uv2, weights ) )
						continue;

					for ( k = 0; k < 3; k++ ) 
					{
						for ( axis = 0; axis < 3; axis++ ) 
						{
							instance_positions[j][axis] += tri_positions[k][axis] * weights[k];
						}
					}
				}

				// triangulate (fan)
				for ( j = 1; j < clipped.len - 1; j++ ) 
				{
					light_poly_t *light = append_light_poly( num_lights, allocated_lights, lights );
					light->material = material;
					light->style = light_style;
					light->type = LIGHT_POLYGON;
					light->emissive_factor = emissive_factor;


					VectorCopy( instance_positions[0],		light->positions + 0 );
					VectorCopy( instance_positions[j + 1],	light->positions + 3 );
					VectorCopy( instance_positions[j],		light->positions + 6 );
					VectorScale( light_color, emissive_factor, light->color );

					// add offset
					VectorSubtract( light->positions + 3, light->positions + 0, e0);
					VectorSubtract( light->positions + 6, light->positions + 0, e1);
					CrossProduct( e0, e1, normal );
					VectorNormalize( normal );
					const float offset = 0.05f; 
					for ( k = 0; k < 3; k++ )
					{
						VectorMA( light->positions + k * 3, offset, normal, light->positions + k * 3 );
					}

					if ( material->surface_light > 0 )
						get_surface_light_contribution( material->surface_light, light );

					get_triangle_off_center( light->positions, light->off_center, NULL, 1.f  );
	
					if ( model_idx < 0 )
					{
						// Find the cluster for this triangle
						light->cluster = BSP_PointLeaf(worldData.nodes, light->off_center)->cluster;

						if (light->cluster < 0)
						{
							// Cluster not found - which happens sometimes.
							// The lighting system can't work with lights that have no cluster, so remove the triangle.
							(*num_lights)--;
						}
					}
					else
					{
						// It's a model: cluster will be determined after model instantiation.
						light->cluster = -1;
					}
				}
			}
		}
	}
}

static bool
collect_frames_emissive_info( shader_t *shader, rtx_material_t *material, bool *entire_texture_emissive, vec2_t min_light_texcoord, vec2_t max_light_texcoord, vec3_t light_color )
{
	*entire_texture_emissive = false;
	min_light_texcoord[0] = min_light_texcoord[1] = 1.0f;
	max_light_texcoord[0] = max_light_texcoord[1] = 0.0f;

	bool any_emissive_valid = false;

	if ( material == NULL )
		return false;

	uint32_t emissive_image_index = material->emissive;

	if ( !emissive_image_index )
		return false;

	const image_t *image = tr.images.items[emissive_image_index];

	if ( !image )
		return false;

	if ( !any_emissive_valid )
	{
		// emissive light color of first frame
		memcpy(light_color, image->light_color, sizeof(vec3_t));
	}
	any_emissive_valid = true;

	*entire_texture_emissive |= image->entire_texture_emissive;
	min_light_texcoord[0] = MIN(min_light_texcoord[0], image->min_light_texcoord[0]);
	min_light_texcoord[1] = MIN(min_light_texcoord[1], image->min_light_texcoord[1]);
	max_light_texcoord[0] = MAX(max_light_texcoord[0], image->max_light_texcoord[0]);
	max_light_texcoord[1] = MAX(max_light_texcoord[1], image->max_light_texcoord[1]);

	return any_emissive_valid;
}

static float compute_emissive( rtx_material_t *material )
{
	if ( material == NULL )
		return 1.f;

	return (float)material->emissive_factor;
}

static void collect_light_polys_from_triangles( 
	world_t &worldData, int model_idx, int* num_lights, int* allocated_lights, light_poly_t** lights,  shader_t *shader,
	tri_vertex_t *vertices, int *indices, int numIndices, int numVertices
	)
{
		rtx_material_t *material = vk_rtx_get_material( shader->index );

		// This is the whole chain a surface has to pass to become a light source. Every
		// step of it fails silently, which is why an emissive surface can glow and light
		// nothing with no way to tell where it dropped out.
		if ( pt_verbose->integer )
		{
			if ( material == NULL )
				Com_Printf( "light poly: %s - no material slot\n", shader->name );
			else if ( !material->active )
				Com_Printf( "light poly: %s - material never built\n", shader->name );
			else if ( !material->emissive )
				Com_Printf( "light poly: %s - no emissive texture (no glow stage, surfacelight %i)\n",
					shader->name, shader->surfacelight );
			else
			{
				const image_t *img = tr.images.items[material->emissive];

				Com_Printf( "light poly: %s - emissive %s colour %.3f %.3f %.3f factor %.2f\n",
					shader->name, img ? img->imgName : "<none>",
					img ? img->light_color[0] : 0.f, img ? img->light_color[1] : 0.f,
					img ? img->light_color[2] : 0.f, material->emissive_factor );
			}
		}

		if ( material == NULL )
			return;

		bool entire_texture_emissive;
		vec2_t min_light_texcoord;
		vec2_t max_light_texcoord;
		vec3_t light_color;

		if ( !collect_frames_emissive_info( shader, material, &entire_texture_emissive, min_light_texcoord, max_light_texcoord, light_color ) )
		{
			// This algorithm relies on information from the emissive texture,
			// specifically the extents of the emissive pixels in that texture.
			// Ignore surfaces that don't have an emissive texture attached.
			return;
		}

		float emissive_factor = compute_emissive( material );
		if ( emissive_factor == 0 )
			return;

		int light_style = 0;
		
		int light_ctr = *num_lights;	// debug

		if ( entire_texture_emissive )
		{
			collect_one_light_poly_entire_texture(  worldData, material, model_idx, light_color, emissive_factor, light_style,
													num_lights, allocated_lights, lights, 
													vertices, indices, numIndices, numVertices );

			int num_light_polys = (*num_lights - light_ctr);
			if ( num_light_polys > 128 )
				Com_Printf( "verts: %d - light-polys: %d ", numVertices, num_light_polys);
			return;
		}

		vec4_t plane;
		if ( !get_surf_plane_equation( plane, vertices, indices, numIndices, numVertices ) )
		{
			// It's possible that some polygons in the game are degenerate, ignore these.
			return;
		}

		collect_one_light_poly( worldData, material, model_idx,
							   min_light_texcoord, max_light_texcoord,
							   light_color, emissive_factor, light_style,
							   num_lights, allocated_lights, lights,
							   vertices, indices, numIndices, numVertices );

		int num_light_polys = (*num_lights - light_ctr);
		if ( num_light_polys > 128 )
			Com_Printf( "verts: %d - light-polys: %d ", numVertices, num_light_polys);
}

static void collect_face_light_polys( srfSurfaceFace_t *surf, shader_t *shader,
	world_t &worldData, int model_idx, int* num_lights, int* allocated_lights, light_poly_t** lights )
{
	int *indices = (int *)((byte *)surf + surf->ofsIndices);

	tri_vertex_t *vertices = (tri_vertex_t*)malloc(surf->numPoints * sizeof(tri_vertex_t));
	for (int i = 0; i < surf->numPoints; i++) {
		VectorCopy( surf->points[i] + 0, vertices[i].position); // copy vec3 position
		VectorCopy2( surf->points[i] + 6, vertices[i].st);      // copy vec2 tex coords
	}

	collect_light_polys_from_triangles( 
		worldData, model_idx, num_lights, allocated_lights, lights, 
		shader, vertices, indices, surf->numIndices, surf->numPoints 
	);
}

static void collect_tri_light_polys( srfTriangles_t *tri, shader_t *shader,
	world_t &worldData, int model_idx, int* num_lights, int* allocated_lights, light_poly_t** lights )
{
	int *indices = tri->indexes;

	tri_vertex_t *vertices = (tri_vertex_t*)malloc(tri->numVerts  * sizeof(tri_vertex_t));
	for (int i = 0; i < tri->numVerts; i++) {
		VectorCopy( tri->verts[i].xyz, vertices[i].position); // copy vec3 position
		VectorCopy2( tri->verts[i].st + 6, vertices[i].st);      // copy vec2 tex coords
	}

	collect_light_polys_from_triangles( 
		worldData, model_idx, num_lights, allocated_lights, lights, 
		shader, vertices, indices, tri->numIndexes, tri->numVerts 
	);
}

static void collect_grid_light_polys( srfGridMesh_t *cv, shader_t *shader,
    world_t &worldData, int model_idx, int* num_lights, int* allocated_lights, light_poly_t** lights )
{
    int widthTable[MAX_GRID_SIZE];
    int heightTable[MAX_GRID_SIZE];
    float lodError;
    int lodWidth = 0, lodHeight = 0;

#if 0
	// determine the allowable discrepance
    lodError = LodErrorForVolume(cv->lodOrigin, cv->lodRadius);
#else
	lodError = r_lodCurveError->value;
#endif

	// determine which rows and columns of the subdivision
	// we are actually going to use
    widthTable[0] = 0;
    lodWidth = 1;
    for (int i = 1; i < cv->width - 1; i++) {
        if (cv->widthLodError[i] <= lodError) {
            widthTable[lodWidth++] = i;
        }
    }
    widthTable[lodWidth++] = cv->width - 1;

    heightTable[0] = 0;
    lodHeight = 1;
    for (int i = 1; i < cv->height - 1; i++) {
        if (cv->heightLodError[i] <= lodError) {
            heightTable[lodHeight++] = i;
        }
    }
    heightTable[lodHeight++] = cv->height - 1;

    const int numVerts = lodWidth * lodHeight;
    const int numIndices = (lodWidth - 1) * (lodHeight - 1) * 6;

    // fill vertices 
    tri_vertex_t *vertices = (tri_vertex_t*)malloc(numVerts * sizeof(tri_vertex_t));
    int vertIndex = 0;

    for (int h = 0; h < lodHeight; ++h) {
        for (int w = 0; w < lodWidth; ++w) {
            int gridIndex = heightTable[h] * cv->width + widthTable[w];
            srfVert_t *dv = &cv->verts[gridIndex];

            VectorCopy(dv->xyz, vertices[vertIndex].position);
            VectorCopy2(dv->st, vertices[vertIndex].st); 
            vertIndex++;
        }
    }

    // fill indices
    int *indices = (int *)malloc(numIndices * sizeof(int));
    int idx = 0;
    for (int h = 0; h < lodHeight - 1; ++h) {
        for (int w = 0; w < lodWidth - 1; ++w) {
            int v0 = (h + 0) * lodWidth + (w + 0);
            int v1 = (h + 0) * lodWidth + (w + 1);
            int v2 = (h + 1) * lodWidth + (w + 0);
            int v3 = (h + 1) * lodWidth + (w + 1);

            indices[idx++] = v0;
            indices[idx++] = v2;
            indices[idx++] = v1;

            indices[idx++] = v1;
            indices[idx++] = v2;
            indices[idx++] = v3;
        }
    }

    collect_light_polys_from_triangles(
        worldData, model_idx, num_lights, allocated_lights, lights,
        shader, vertices, indices, numIndices, numVerts
    );
}

static void collect_light_polys( world_t &worldData, int model_idx, int* num_lights, int* allocated_lights, light_poly_t** lights )
{
	msurface_t *surf, *surfaces;
	uint32_t i, num_surfaces;
	srfSurfaceFace_t *face;
	srfTriangles_t *tris;
	srfGridMesh_t *grid;

	surfaces		= model_idx < 0 ? worldData.surfaces : worldData.bmodels[model_idx].firstSurface;
	num_surfaces	= model_idx < 0 ? worldData.numsurfaces : worldData.bmodels[model_idx].numSurfaces;

	for ( i = 0; i < num_surfaces; i++ ) 
	{
		surf = surfaces + i;

		// Don't create light polys from SKY surfaces, those are handled separately.
		// Sometimes, textures with a light fixture are used on sky polys (like in rlava1),
		// and that leads to subdivision of those sky polys into a large number of lights.
		if ( RB_IsSky(surf->shader) )
			continue;

		// A Force Sight surface must not light the map while it is hidden.
		if ( surf->shader->surfaceFlags & SURF_FORCESIGHT )
			continue;

		if ( model_idx < 0 && belongs_to_model( worldData, surf ) )
			continue;

		const int first_light = *num_lights;

		face = (srfSurfaceFace_t *)surf->data;
		if ( face->surfaceType == SF_FACE ) {
			collect_face_light_polys(  face, surf->shader, worldData, model_idx, num_lights, allocated_lights, lights );
		}
		else
		{
			tris = (srfTriangles_t *)surf->data;
			if ( tris->surfaceType == SF_TRIANGLES) {
				collect_tri_light_polys( tris, surf->shader , worldData, model_idx, num_lights, allocated_lights, lights );
			}
			else
			{
				grid = (srfGridMesh_t *)surf->data;
				if ( grid->surfaceType == SF_GRID ) {
					collect_grid_light_polys(  grid, surf->shader, worldData, model_idx, num_lights, allocated_lights, lights );
				}
			}
		}

		// The light edit groups the polygons by surface.
		for ( int light = first_light; light < *num_lights; light++ )
			(*lights)[light].surface = (int)i;
	}
}

static void
get_aabb_corner(const aabb_t* aabb, int corner_idx, vec3_t corner)
{
	corner[0] = (corner_idx & 1) ? aabb->maxs[0] : aabb->mins[0];
	corner[1] = (corner_idx & 2) ? aabb->maxs[1] : aabb->mins[1];
	corner[2] = (corner_idx & 4) ? aabb->maxs[2] : aabb->mins[2];
}

static bool
light_affects_cluster(light_poly_t* light, const aabb_t* aabb)
{
	// Empty cluster, nothing is visible
	if (aabb->mins[0] > aabb->maxs[0])
		return false;

	// The test below culls clusters sitting behind the light's plane, which only means
	// anything for a polygon. A sphere has no facing and positions[3..8] hold its radius
	// and spot data rather than two more vertices, so reading them as a triangle would
	// cull it on noise. The PVS has already narrowed this down.
	if (light->type != LIGHT_POLYGON)
		return true;

	const float* v0 = light->positions + 0;
	const float* v1 = light->positions + 3;
	const float* v2 = light->positions + 6;
	
	// Get the light plane equation
	vec3_t e1, e2, normal;
	VectorSubtract(v1, v0, e1);
	VectorSubtract(v2, v0, e2);
	CrossProduct(e1, e2, normal);
	VectorNormalize(normal);
	
	float plane_distance = -DotProduct(normal, v0);

	bool all_culled = true;

	// If all 8 corners of the cluster's AABB are behind the light, it's definitely invisible
	for (int corner_idx = 0; corner_idx < 8; corner_idx++)
	{
		vec3_t corner;
		get_aabb_corner(aabb, corner_idx, corner);

		float side = DotProduct(normal, corner) + plane_distance;
		if (side > 0)
			all_culled = false;
	}

	if (all_culled)
	{
		return false;
	}

	return true;
}

// JKA maps do not mark their light surfaces. textures/bespin/n_light01 and its kin carry
// neither a glow stage nor q3map_surfacelight, so collect_light_polys finds nothing at
// all and the interiors come out black - a panel glows because its texture is bright and
// lights nothing because no source was ever created for it. The lighting those maps were
// actually built with lives in the entity lump: the `light` entities q3map2 baked the
// lightmaps from. Read them and hand the tracer the sources the mapper placed.
//
// Spotlights (a `light` with a `target`) are taken as point lights for now. That is too
// generous rather than too dark, and still much closer than nothing.
#define ENTITY_LIGHT_RADIUS 16.0f

// A spot is aimed at the entity its `target` names. q3map2 takes `radius` as the radius of the
// lit disc at the target and widens it by 16 units.
#define SPOT_DEFAULT_RADIUS	64.0f
#define SPOT_RADIUS_MARGIN	16.0f
#define SPOT_EDGE_WIDTH		32.0f

typedef struct {
	char	name[64];
	vec3_t	origin;
} entity_target_t;

typedef struct {
	int		light;			// index in worldData.light_polys
	char	target[64];
	float	radius;
} pending_spot_t;

// Turns the point light into a spot aimed at `target_origin`. The spot's irradiance is
// 2 * falloff / d^2 whatever the emitter radius, so half the intensity gives a point light's
// brightness on the axis.
static void make_entity_spot( light_poly_t *light, const vec3_t target_origin, float intensity,
	const vec3_t color, float radius )
{
	vec3_t	dir;

	VectorSubtract( target_origin, light->positions + 0, dir );
	const float dist = VectorNormalize( dir );

	if ( dist < 1.0f )
		return;

	// Tangent of the half angle at the edge of the cone, and where the soft edge starts.
	const float edge = ( radius + SPOT_RADIUS_MARGIN ) / dist;
	const float start = MAX( 0.0f, ( radius + SPOT_RADIUS_MARGIN - SPOT_EDGE_WIDTH ) / dist );
	const float cos_total = 1.0f / sqrtf( 1.0f + edge * edge );
	const float cos_start = 1.0f / sqrtf( 1.0f + start * start );

	VectorCopy( dir, light->positions + 6 );
	light->positions[4] = uintBitsToFloat( DYNLIGHT_SPOT_EMISSION_PROFILE_FALLOFF );
	light->positions[5] = uintBitsToFloat( floatToHalf( cos_total ) | ( floatToHalf( cos_start ) << 16 ) );
	VectorScale( color, intensity * 0.5f, light->color );
	light->type = LIGHT_SPOT;
}

static int collect_entity_lights( world_t &worldData )
{
	std::vector<entity_target_t>	targets;
	std::vector<pending_spot_t>		spots;

	const char	*p = worldData.entityString;
	char		keyname[MAX_TOKEN_CHARS];
	char		value[MAX_TOKEN_CHARS];
	int			added = 0;
	int			in_solid = 0;
	int			entities = 0;
	int			lightish = 0;
	int			light_rank = 0;		// index among all `light` entities, kept or not

	if ( !p )
	{
		Com_Printf( "rtx: entity lights - no entity string\n" );
		return 0;
	}

	COM_BeginParseSession( "collect_entity_lights" );

	while ( 1 )
	{
		const char *token = COM_ParseExt( &p, qtrue );

		if ( !token[0] )
			break;

		if ( token[0] != '{' )
			continue;

		entities++;

		qboolean	is_light = qfalse;
		qboolean	is_spot = qfalse;
		qboolean	has_origin = qfalse;
		int			rank = -1;
		char		targetname[64] = "";
		char		target[64] = "";
		float		spot_radius = SPOT_DEFAULT_RADIUS;
		vec3_t		origin = { 0.0f, 0.0f, 0.0f };
		vec3_t		color = { 1.0f, 1.0f, 1.0f };
		float		intensity = 300.0f;	// q3map2's default when the key is absent
		int			style = 0;

		while ( 1 )
		{
			token = COM_ParseExt( &p, qtrue );

			if ( !token[0] || token[0] == '}' )
				break;

			Q_strncpyz( keyname, token, sizeof(keyname) );

			token = COM_ParseExt( &p, qtrue );

			if ( !token[0] || token[0] == '}' )
				break;

			Q_strncpyz( value, token, sizeof(value) );

			if ( !Q_stricmp( keyname, "classname" ) )
			{
				is_light = (qboolean)( Q_stricmp( value, "light" ) == 0 );

				if ( is_light )
					rank = light_rank++;

				// Anything light-ish, so the log can tell "my parser is broken" from
				// "q3map2 stripped the light entities out of this BSP".
				if ( Q_stristr( value, "light" ) )
				{
					lightish++;

					if ( lightish <= 8 )
						Com_Printf( "rtx: entity light candidate: %s\n", value );
				}
			}
			else if ( !Q_stricmp( keyname, "target" ) )
			{
				is_spot = qtrue;
				Q_strncpyz( target, value, sizeof(target) );
			}
			else if ( !Q_stricmp( keyname, "targetname" ) )
				Q_strncpyz( targetname, value, sizeof(targetname) );
			else if ( !Q_stricmp( keyname, "radius" ) )
				spot_radius = atof( value );
			else if ( !Q_stricmp( keyname, "origin" ) )
				has_origin = (qboolean)( sscanf( value, "%f %f %f", &origin[0], &origin[1], &origin[2] ) == 3 );
			else if ( !Q_stricmp( keyname, "light" ) || !Q_stricmp( keyname, "_light" ) )
				intensity = atof( value );
			else if ( !Q_stricmp( keyname, "style" ) )
				style = MAX( 0, MIN( atoi( value ), RTX_LSTYLE_MAX - 1 ) );
			else if ( !Q_stricmp( keyname, "_color" ) || !Q_stricmp( keyname, "color" ) )
				sscanf( value, "%f %f %f", &color[0], &color[1], &color[2] );
		}

		if ( targetname[0] && has_origin )
		{
			entity_target_t t;

			Q_strncpyz( t.name, targetname, sizeof(t.name) );
			VectorCopy( origin, t.origin );
			targets.push_back( t );
		}

		if ( !is_light || !has_origin || intensity <= 0.0f )
			continue;

		// Some maps give the colour as bytes rather than the usual 0..1.
		float peak = MAX( color[0], MAX( color[1], color[2] ) );

		if ( peak > 1.0f )
			VectorScale( color, 1.0f / 255.0f, color );

		// A light inside a brush reaches nothing and only costs sampling. The cluster is
		// also what the PVS bucketing in collect_cluster_lights keys off.
		int cluster = BSP_PointLeaf( worldData.nodes, origin )->cluster;

		if ( cluster < 0 )
		{
			in_solid++;
			RTX_LightEdit_SetLoadedStyle( RTX_LightEdit_RegisterLoaded( RTX_LSRC_ENTITY, rank, origin, color, intensity, -1, 0.0f, 0.0f ), style );
			continue;
		}

		light_poly_t *light = append_light_poly( &worldData.num_light_polys,
			&worldData.allocated_light_polys, &worldData.light_polys );

		Com_Memset( light, 0, sizeof(*light) );

		VectorCopy( origin, light->positions + 0 );
		VectorCopy( origin, light->off_center );

		light->positions[3] = ENTITY_LIGHT_RADIUS;

		// A sphere light's irradiance is colour * r^2 / d^2, so dividing the colour by
		// r^2 here makes the emitter's size irrelevant at a distance. copy_light applies
		// pt_light_scale_edit at each frame for an added light.
		VectorScale( color, intensity
			/ ( ENTITY_LIGHT_RADIUS * ENTITY_LIGHT_RADIUS ), light->color );

		light->cluster = cluster;
		light->type = LIGHT_SPHERE;
		light->emissive_factor = 1.0f;
		light->material = NULL;
		light->style = style;
		light->ent_class = is_spot ? LIGHT_ENT_SPOT : LIGHT_ENT_UNSET;

		if ( is_spot )
		{
			pending_spot_t s;

			s.light = worldData.num_light_polys - 1;
			Q_strncpyz( s.target, target, sizeof(s.target) );
			s.radius = spot_radius > 0.0f ? spot_radius : SPOT_DEFAULT_RADIUS;
			spots.push_back( s );
		}

		RTX_LightEdit_SetLoadedStyle( RTX_LightEdit_RegisterLoaded( RTX_LSRC_ENTITY, rank, origin, color, intensity,
			worldData.num_light_polys - 1, 0.0f, 0.0f ), style );

		added++;
	}

	COM_EndParseSession();

	// The target can come after the light in the lump, so resolve once every entity is read.
	// A spot whose target is missing stays a point light.
	int aimed = 0;

	for ( size_t i = 0; i < spots.size(); i++ )
	{
		for ( size_t j = 0; j < targets.size(); j++ )
		{
			if ( Q_stricmp( spots[i].target, targets[j].name ) )
				continue;

			light_poly_t *light = worldData.light_polys + spots[i].light;
			vec3_t color;

			// Undo the sphere's 1 / r^2 to get the entity's colour * intensity back.
			VectorScale( light->color, ENTITY_LIGHT_RADIUS * ENTITY_LIGHT_RADIUS, color );
			make_entity_spot( light, targets[j].origin, 1.0f, color, spots[i].radius );

			if ( light->type == LIGHT_SPOT )
				aimed++;

			break;
		}
	}

	Com_Printf( "rtx: %i of %i spot lights aimed at their target\n", aimed, (int)spots.size() );

	Com_Printf( "rtx: %i entity lights added from %i entities (%i light-ish classnames, %i inside solid)\n",
		added, entities, lightish, in_solid );

	return added;
}

// A rough sampling weight of a light for a cluster: light_sampling_mass of light_lists.h at the point
// of the cluster box nearest to the light, without the BRDF, the light style and the shadow statistics.
static float light_cluster_weight( const light_poly_t *light, float lum, const aabb_t *aabb )
{
	vec3_t center, nearest, d;

	if ( light->type == LIGHT_POLYGON )
	{
		VectorAdd( light->positions + 0, light->positions + 3, center );
		VectorAdd( center, light->positions + 6, center );
		VectorScale( center, 1.0f / 3.0f, center );
	}
	else
		VectorCopy( light->positions + 0, center );

	for ( int i = 0; i < 3; i++ )
		nearest[i] = Com_Clamp( aabb->mins[i], aabb->maxs[i], center[i] );

	VectorSubtract( center, nearest, d );
	const float dist2 = MAX( DotProduct( d, d ), 16.0f * 16.0f );	// a light in the cluster is near, not infinite

	float solid_angle;
	if ( light->type == LIGHT_POLYGON )
	{
		vec3_t e1, e2, n;
		VectorSubtract( light->positions + 3, light->positions + 0, e1 );
		VectorSubtract( light->positions + 6, light->positions + 0, e2 );
		CrossProduct( e1, e2, n );
		solid_angle = 0.5f * VectorLength( n ) / dist2;
	}
	else if ( light->type == LIGHT_SPHERE )
		solid_angle = M_PI * light->positions[3] * light->positions[3] / dist2;
	else
		solid_angle = 2.0f / dist2;

	return lum * MIN( solid_angle, 2.0f * (float)M_PI );
}

// The static light lists take at most this part of the light list buffer: the model lights take the rest each frame.
#define STATIC_LIGHT_LIST_NODES		( MAX_LIGHT_LIST_NODES / 2 )

// Gives the number of clusters whose list was cut. A quiet call prints nothing.
// A cluster takes the lights of the clusters in its PVS. When the lists do not fit in
// STATIC_LIGHT_LIST_NODES, or a list is longer than RTX_MAX_LIGHTS_PER_CLUSTER, a cluster
// keeps the lights with the largest light_cluster_weight.
static int collect_cluster_lights( world_t &worldData, qboolean quiet = qfalse )
{
	const int num_clusters = worldData.numClusters;
	std::vector<int> counts( num_clusters + 1, 0 );
	std::vector<int> offsets( num_clusters + 1, 0 );
	std::vector<int> candidates;

	// Pass 0 counts the lights of each cluster, pass 1 writes them.
	for ( int pass = 0; pass < 2; pass++ )
	{
		if ( pass == 1 )
		{
			for ( int c = 0; c < num_clusters; c++ )
				offsets[c + 1] = offsets[c] + counts[c];

			candidates.resize( offsets[num_clusters] );
			std::fill( counts.begin(), counts.end(), 0 );
		}

		int *count = counts.data();
		int *list = candidates.data();

		for ( int nlight = 0; nlight < worldData.num_light_polys; nlight++ )
		{
			light_poly_t *light = worldData.light_polys + nlight;

			if ( light->cluster < 0 )
				continue;

			const byte *pvs = (const byte*)BSP_GetPvs( &worldData, light->cluster );
			if ( !pvs )
				continue;

			FOREACH_BIT_BEGIN( pvs, worldData.clusterBytes, other_cluster )
				if ( other_cluster < num_clusters && light_affects_cluster( light, worldData.cluster_aabbs + other_cluster ) )
				{
					if ( pass == 1 )
						list[offsets[other_cluster] + count[other_cluster]] = nlight;
					count[other_cluster]++;
				}
			FOREACH_BIT_END
		}
	}

	// The largest list length that fits in the buffer.
	auto total_for = [&]( int cap ) -> long long
	{
		long long total = 0;
		for ( int c = 0; c < num_clusters; c++ )
			total += MIN( counts[c], cap );
		return total;
	};

	const long long total_all = offsets[num_clusters];
	int cap = RTX_MAX_LIGHTS_PER_CLUSTER;

	if ( total_for( cap ) > STATIC_LIGHT_LIST_NODES )
	{
		int lo = 1, hi = cap;
		while ( lo < hi )
		{
			const int mid = ( lo + hi + 1 ) / 2;
			if ( total_for( mid ) <= STATIC_LIGHT_LIST_NODES )
				lo = mid;
			else
				hi = mid - 1;
		}
		cap = lo;
	}

	// The luminance of each light, with the scale of the light buffer.
	std::vector<float> lums( worldData.num_light_polys );
	for ( int nlight = 0; nlight < worldData.num_light_polys; nlight++ )
	{
		const light_poly_t *light = worldData.light_polys + nlight;
		lums[nlight] = fabsf( 0.299f * light->color[0] + 0.587f * light->color[1] + 0.114f * light->color[2] ) * vk_rtx_light_scale( light, qtrue );
	}

	worldData.num_cluster_lights = (int)total_for( cap );
	worldData.cluster_lights = (int*)Z_Malloc( MAX( 1, worldData.num_cluster_lights ) * sizeof(int), TAG_GENERAL, qtrue );
	worldData.cluster_light_offsets = (int*)Z_Malloc( ( num_clusters + 1 ) * sizeof(int), TAG_GENERAL, qtrue );

	std::vector< std::pair<float, int> > ranked;
	int cut_clusters = 0;
	int list_offset = 0;

	for ( int c = 0; c < num_clusters; c++ )
	{
		const int *src = candidates.data() + offsets[c];
		int *dst = worldData.cluster_lights + list_offset;
		const int count = counts[c];

		worldData.cluster_light_offsets[c] = list_offset;

		if ( count <= cap )
		{
			memcpy( dst, src, count * sizeof(int) );
			list_offset += count;
			continue;
		}

		// Keep the `cap` strongest lights, in the order of the light index.
		const aabb_t *aabb = worldData.cluster_aabbs + c;
		ranked.resize( count );
		for ( int i = 0; i < count; i++ )
			ranked[i] = std::make_pair( -light_cluster_weight( worldData.light_polys + src[i], lums[src[i]], aabb ), src[i] );

		std::nth_element( ranked.begin(), ranked.begin() + cap, ranked.end() );
		for ( int i = 0; i < cap; i++ )
			dst[i] = ranked[i].second;
		std::sort( dst, dst + cap );

		list_offset += cap;
		cut_clusters++;
	}
	worldData.cluster_light_offsets[num_clusters] = list_offset;

	if ( !quiet )
	{
		Com_Printf( S_COLOR_MAGENTA "\n\n\nTotal interactions: %d,\n\n", worldData.num_cluster_lights );

		if ( cut_clusters )
			Com_Printf( S_COLOR_YELLOW "rtx: %lld light list entries for %d entries of room: %d lights per cluster at most, %d of %d lists cut to their strongest lights\n",
				total_all, STATIC_LIGHT_LIST_NODES, cap, cut_clusters, num_clusters );
	}

	return cut_clusters;
}

// Frees the previous lists first. Used by the light editor.
int vk_rtx_rebuild_cluster_lights( world_t &worldData, qboolean quiet )
{
	if ( worldData.cluster_lights )
		Z_Free( worldData.cluster_lights );

	if ( worldData.cluster_light_offsets )
		Z_Free( worldData.cluster_light_offsets );

	worldData.cluster_lights = NULL;
	worldData.cluster_light_offsets = NULL;

	return collect_cluster_lights( worldData, quiet );
}

// A surface that only Force Sight shows goes to its own BLAS, which the TLAS holds only while Force Sight is on.
static int filter_forcesight( shader_t *shader )
{
	return ( shader->surfaceFlags & SURF_FORCESIGHT ) ? 1 : 0;
}

static int filter_opaque( shader_t *shader )
{
	if ( ( shader->surfaceFlags & SURF_FORCESIGHT ) || RB_IsTransparent( shader ) || RB_IsMasked( shader ) ) 
		return 0;

	return 1;
}

static int filter_transparent( shader_t *shader )
{
	if ( ( shader->surfaceFlags & SURF_FORCESIGHT ) || !RB_IsTransparent( shader ) || RB_IsMasked( shader ) ) 
		return 0;

	return 1;
}

static int filter_masked( shader_t *shader )
{
	if ( ( shader->surfaceFlags & SURF_FORCESIGHT ) || !RB_IsMasked( shader ) || RB_IsTransparent( shader ) ) 
		return 0;

	return 1;
}

static int filter_static( shader_t *shader )
{
	qboolean is_geom_dynamic = RB_IsDynamicGeometry( shader );
	qboolean is_mat_dynamic = RB_IsDynamicMaterial( shader );
	qboolean is_sky = RB_IsSky(shader);

	if ( !is_geom_dynamic && !is_mat_dynamic && !is_sky )
		return 1;

	return 0;
}

// The static world geometry holds the Force Sight surfaces too, even those with a dynamic material.
static int filter_static_or_forcesight( shader_t *shader )
{
	return ( ( shader->surfaceFlags & SURF_FORCESIGHT ) || filter_static( shader ) ) ? 1 : 0;
}

static int filter_dynamic_geometry(shader_t *shader)
{
    qboolean is_geom_dynamic = RB_IsDynamicGeometry(shader);
    qboolean is_mat_dynamic = RB_IsDynamicMaterial(shader);
    qboolean is_sky = RB_IsSky(shader);

    if (is_geom_dynamic)
        return 1;

    return 0;
}

static int filter_dynamic_material( shader_t *shader )
{
    qboolean is_geom_dynamic = RB_IsDynamicGeometry(shader);
    qboolean is_mat_dynamic = RB_IsDynamicMaterial(shader);
    qboolean is_sky = RB_IsSky(shader);

    if (!is_geom_dynamic && is_mat_dynamic)
        return 1;

    return 0;
}

static int filter_sky( shader_t *shader )
{
	qboolean is_sky = RB_IsSky(shader);

	if ( !is_sky )
		return 0;

	return 1;
}

static int skip_invalid_surface( shader_t *shader )
{
	qboolean is_sky = RB_IsSky(shader);

	if ( is_sky )
		return 0;

	if ( shader->surfaceFlags == SURF_NODRAW /*|| shader->surfaceFlags == SURF_NONE*///SURF_SKIP
		|| shader->stages[0] == NULL 
		|| !shader->stages[0]->active )
		return 1;

	return 0;
}

static void vk_rtx_estimate_geometry_recursive( world_t &worldData, mnode_t *node, vk_geometry_data_t *geom, int (*filter)(shader_t*) )
{
	uint32_t	i, j;
	msurface_t	*surf, **mark;

	if ( node->contents == -1 ) 
	{
		for ( i = 0; i < 2; i++ ) 
		{
			vk_rtx_estimate_geometry_recursive( worldData, node->children[i], geom, filter );
		}
	}

	mark = node->firstmarksurface;

	for ( j = 0; j < node->nummarksurfaces; j++ ) 
	{
		surf = mark[j];

		shader_t* shader = tr.shaders[surf->shader->index];

		if ( !filter( shader ) )
			continue;

		tess.shader = shader;
		tess.allowVBO = qfalse;

		tess.numVertexes = tess.numIndexes = 0;
		
		// process surface
		rb_surfaceTable[*surf->data](surf->data);	

		const uint32_t num_clusters = (tess.numIndexes / 3);

		geom->host.surf_count++;
		geom->num_primitives_allocated += tess.numIndexes / 3;
	}
}

static void vk_rtx_estimate_geometry( world_t &worldData, mnode_t *node, vk_geometry_data_t *geom, int (*filter)(shader_t*) )
{ 
	// should already be done in vk_rtx_reset_world_geometry() with memset
	geom->host.surf_count = 0;
	geom->host.surf_offset = 0;

	geom->num_primitives_allocated = 0;
	geom->num_primitives = 0;

	vk_rtx_estimate_geometry_recursive( worldData, node, geom, filter );

	geom->primitives	= (VboPrimitive*)calloc( MAX( 1, (geom->num_primitives_allocated )),  sizeof(VboPrimitive) );
	Com_Memset( geom->primitives, 0, MAX( 1, (geom->num_primitives_allocated )) * sizeof(VboPrimitive) );

	// ~sunny, might be usfull to keep for dynamic surfaces
	const uint32_t surf_count_allocated = MAX( 1, (geom->host.surf_count ));
	geom->host.dynamic_surfs = (vk_geometry_dynamic_surf_t*)calloc( surf_count_allocated, sizeof(vk_geometry_dynamic_surf_t) );
	Com_Memset( geom->host.dynamic_surfs, 0, surf_count_allocated * sizeof(vk_geometry_dynamic_surf_t) );
}

static void vk_rtx_estimate_bmodels( world_t& worldData, vk_geometry_data_t *geom )
{
	geom->host.surf_count = 0;
	geom->host.surf_offset = 0;

	geom->num_primitives_allocated = 0;
	geom->num_primitives = 0;

	for ( int model = 0; model < worldData.num_bmodels; model++ ) 
	{
		bmodel_t *bmodel = &worldData.bmodels[model];
		msurface_t	*surf;

		for (int i = 0; i < bmodel->numSurfaces; i++) 
		{
			surf = bmodel->firstSurface + i;

			shader_t* shader = tr.shaders[surf->shader->index];

			tess.numVertexes = tess.numIndexes = 0;
			tess.shader = shader;
			tess.allowVBO = qfalse;
			rb_surfaceTable[*surf->data](surf->data);

			geom->num_primitives += tess.numIndexes / 3;

			tess.numVertexes = tess.numIndexes = 0;
		}
	}

	geom->primitives	= (VboPrimitive*)calloc( MAX( 1, (geom->num_primitives )),  sizeof(VboPrimitive) );
	Com_Memset( geom->primitives, 0, MAX( 1, (geom->num_primitives )) * sizeof(VboPrimitive) );

	/*if ( geom->dynamic_flags )
	{
		geom->host.dynamic_surfs = (vk_geometry_dynamic_surf_t*)calloc( geom->host.surf_count, sizeof(vk_geometry_dynamic_surf_t) );
	}*/
}

#define clamp(a,b,c)    ((a)<(b)?(a)=(b):(a)>(c)?(a)=(c):(a))

uint32_t
encode_normal(const vec3_t normal)
{
	float invL1Norm = 1.0f / (fabsf(normal[0]) + fabsf(normal[1]) + fabsf(normal[2]));

	vec2_t p = { normal[0] * invL1Norm, normal[1] * invL1Norm };
	vec2_t pp = { p[0], p[1] };

	if (normal[2] < 0.f)
	{
		pp[0] = (1.f - fabsf(p[1])) * ((p[0] >= 0.f) ? 1.f : -1.f);
		pp[1] = (1.f - fabsf(p[0])) * ((p[1] >= 0.f) ? 1.f : -1.f);
	}

	pp[0] = pp[0] * 0.5f + 0.5f;
	pp[1] = pp[1] * 0.5f + 0.5f;

	clamp(pp[0], 0.f, 1.f);
	clamp(pp[1], 0.f, 1.f);

	uint32_t ux = (uint32_t)(pp[0] * 0xffffu);
	uint32_t uy = (uint32_t)(pp[1] * 0xffffu);

	return ux | (uy << 16);
}

// create_poly(worldData, surf, material_id, geom, surface_prims);
static uint32_t create_poly( vk_geometry_data_t *geom, rtx_material_t *material, uint32_t material_id, VboPrimitive *primitives_out )
{
	int i0, i1, i2;
	const shaderStage_t *pStage;

	const float emissive_factor = compute_emissive(material);

	float alpha = 1.f;
	//if (MAT_IsKind(material_id, MATERIAL_KIND_TRANSPARENT))
	//	alpha = (texinfo->c.flags & SURF_TRANS33) ? 0.33f : (texinfo->c.flags & SURF_TRANS66) ? 0.66f : 1.0f;

	const uint32_t emissive_and_alpha = floatToHalf(emissive_factor) | (floatToHalf(alpha) << 16);

	int numTris = tess.numIndexes / 3;

	VboPrimitive *base_primitive = primitives_out;

	for ( uint32_t i = 0; i < numTris; ++i ) 
	{
		memset(primitives_out, 0, sizeof(VboPrimitive));

		uint32_t idx_base = i * 3;

		i0 = tess.indexes[idx_base + 0];
		i1 = tess.indexes[idx_base + 1];
		i2 = tess.indexes[idx_base + 2];

		VectorCopy( tess.xyz[i0], primitives_out->pos0 );
		VectorCopy( tess.xyz[i1], primitives_out->pos1 );
		VectorCopy( tess.xyz[i2], primitives_out->pos2 );

		primitives_out->normals[0] = encode_normal( tess.normal[i0] );
		primitives_out->normals[1] = encode_normal( tess.normal[i1] );
		primitives_out->normals[2] = encode_normal( tess.normal[i2] );

		primitives_out->tangents[0] = encode_normal( tess.qtangent[i0] );
		primitives_out->tangents[1] = encode_normal( tess.qtangent[i1] );
		primitives_out->tangents[2] = encode_normal( tess.qtangent[i2] );

		// The texture coordinates of the surface. The tracer computes those of each bundle from
		// its tcGen and tcMods.
		primitives_out->uv0[0] = floatToHalf( tess.texCoords[0][i0][0] ) | ( floatToHalf( tess.texCoords[0][i0][1] ) << 16 );
		primitives_out->uv1[0] = floatToHalf( tess.texCoords[0][i1][0] ) | ( floatToHalf( tess.texCoords[0][i1][1] ) << 16 );
		primitives_out->uv2[0] = floatToHalf( tess.texCoords[0][i2][0] ) | ( floatToHalf( tess.texCoords[0][i2][1] ) << 16 );

		primitives_out->material_id = material_id; //(material_flags & ~MATERIAL_INDEX_MASK) | (material_index & MATERIAL_INDEX_MASK);
		primitives_out->emissive_and_alpha = emissive_and_alpha;
		primitives_out->instance = 0;

		++primitives_out;
	}

	for ( uint32_t stage = 0; stage < MAX_RTX_STAGES; stage++ ) 
	{
		pStage = tess.shader->stages[stage];

		if ( !pStage || !pStage->active )
			break;

		// The colour of bundle 0 of every stage: the vertex light of rgbGen exactVertex and the
		// vertex alpha, which the tracer reads for alphaGen vertex and for the blended surfaces.
		ComputeColors( 0, tess.svars.colors[0], pStage, 0 );

		primitives_out = base_primitive;
		for ( uint32_t prim = 0; prim < numTris; ++prim ) 
		{
			uint32_t idx_base = prim * 3;

			i0 = tess.indexes[idx_base + 0];
			i1 = tess.indexes[idx_base + 1];
			i2 = tess.indexes[idx_base + 2];

			{
				primitives_out->color0[stage] = tess.svars.colors[0][i0][0] | tess.svars.colors[0][i0][1] << 8 | tess.svars.colors[0][i0][2] << 16 | tess.svars.colors[0][i0][3] << 24;
				primitives_out->color1[stage] = tess.svars.colors[0][i1][0] | tess.svars.colors[0][i1][1] << 8 | tess.svars.colors[0][i1][2] << 16 | tess.svars.colors[0][i1][3] << 24;
				primitives_out->color2[stage] = tess.svars.colors[0][i2][0] | tess.svars.colors[0][i2][1] << 8 | tess.svars.colors[0][i2][2] << 16 | tess.svars.colors[0][i2][3] << 24;
			}

			++primitives_out;
		}
	}

	return numTris;
}

// A triangle takes the cluster of its centre, which is wrong for the rest of a triangle that spans
// several clusters (kejim_post: a floor triangle from cluster 3769 into 2806). The static world
// triangles are cut along the BSP planes where the cluster changes, so each piece has its own.
struct bary_t { float b[3]; };
typedef std::vector<bary_t> bary_poly_t;

static std::unordered_map<const mnode_t*, int> s_node_cluster;
static uint32_t s_split_triangles, s_split_added;

// The cluster shared by every open leaf under the node; -1 when all are solid, -2 when they differ.
static int vk_rtx_node_cluster( const mnode_t *node )
{
	if ( !node->plane )
		return node->cluster;

	auto it = s_node_cluster.find( node );
	if ( it != s_node_cluster.end() )
		return it->second;

	const int a = vk_rtx_node_cluster( node->children[0] );
	const int b = vk_rtx_node_cluster( node->children[1] );
	const int c = ( a == -1 ) ? b : ( b == -1 ) ? a : ( a == b ) ? a : -2;

	s_node_cluster[node] = c;
	return c;
}

static void vk_rtx_clip_cluster( const mnode_t *node, const bary_poly_t &poly, const float P[3][3], const float *n,
	float shift, std::vector<bary_poly_t> &out )
{
	if ( poly.size() < 3 || out.size() > 128 )
		return;

	if ( !node->plane || vk_rtx_node_cluster( node ) != -2 )
	{
		out.push_back( poly );
		return;
	}

	// Distance to the plane of the triangle moved to the open side, so that a triangle on a
	// leaf boundary is not split by that boundary.
	const cplane_t *pl = node->plane;
	float dv[3];
	for ( int i = 0; i < 3; i++ )
		dv[i] = DotProduct( P[i], pl->normal ) - pl->dist + shift * DotProduct( n, pl->normal );

	const size_t count = poly.size();
	std::vector<float> d( count );
	int nfront = 0;
	for ( size_t i = 0; i < count; i++ )
	{
		d[i] = poly[i].b[0] * dv[0] + poly[i].b[1] * dv[1] + poly[i].b[2] * dv[2];
		nfront += d[i] >= 0.f;
	}

	if ( nfront == (int)count ) { vk_rtx_clip_cluster( node->children[0], poly, P, n, shift, out ); return; }
	if ( nfront == 0 )          { vk_rtx_clip_cluster( node->children[1], poly, P, n, shift, out ); return; }

	bary_poly_t front, back;
	for ( size_t i = 0; i < count; i++ )
	{
		const size_t j = ( i + 1 ) % count;
		( d[i] >= 0.f ? front : back ).push_back( poly[i] );

		if ( ( d[i] >= 0.f ) != ( d[j] >= 0.f ) )
		{
			const float t = d[i] / ( d[i] - d[j] );
			bary_t m;
			for ( int k = 0; k < 3; k++ )
				m.b[k] = poly[i].b[k] + ( poly[j].b[k] - poly[i].b[k] ) * t;
			front.push_back( m );
			back.push_back( m );
		}
	}

	vk_rtx_clip_cluster( node->children[0], front, P, n, shift, out );
	vk_rtx_clip_cluster( node->children[1], back, P, n, shift, out );
}

static void vk_rtx_decode_normal( uint32_t e, float *out )
{
	float x = ( (e & 0xffffu) / 65535.f ) * 2.f - 1.f;
	float y = ( (e >> 16) / 65535.f ) * 2.f - 1.f;
	float z = 1.f - fabsf( x ) - fabsf( y );

	if ( z < 0.f )
	{
		const float tx = ( 1.f - fabsf( y ) ) * ( x >= 0.f ? 1.f : -1.f );
		const float ty = ( 1.f - fabsf( x ) ) * ( y >= 0.f ? 1.f : -1.f );
		x = tx;
		y = ty;
	}

	VectorSet( out, x, y, z );
	VectorNormalize( out );
}

static uint32_t vk_rtx_lerp_normal( const uint32_t e[3], const bary_t &w )
{
	vec3_t sum = { 0.f, 0.f, 0.f }, v;

	for ( int i = 0; i < 3; i++ )
	{
		vk_rtx_decode_normal( e[i], v );
		VectorMA( sum, w.b[i], v, sum );
	}

	if ( VectorNormalize( sum ) == 0.f )
		vk_rtx_decode_normal( e[0], sum );

	return encode_normal( sum );
}

static uint32_t vk_rtx_lerp_half2( const uint32_t h[3], const bary_t &w )
{
	float u = 0.f, v = 0.f;

	for ( int i = 0; i < 3; i++ )
	{
		u += w.b[i] * halfToFloat( (uint16_t)( h[i] & 0xffffu ) );
		v += w.b[i] * halfToFloat( (uint16_t)( h[i] >> 16 ) );
	}

	return floatToHalf( u ) | ( floatToHalf( v ) << 16 );
}

static uint32_t vk_rtx_lerp_color( const uint32_t c[3], const bary_t &w )
{
	uint32_t out = 0;

	for ( int k = 0; k < 4; k++ )
	{
		float v = 0.f;
		for ( int i = 0; i < 3; i++ )
			v += w.b[i] * (float)( ( c[i] >> ( k * 8 ) ) & 0xffu );
		out |= (uint32_t)( v + 0.5f ) << ( k * 8 );
	}

	return out;
}

// The piece of a primitive with the corners at these weights of its own corners.
static void vk_rtx_primitive_piece( const VboPrimitive &s, const bary_t corner[3], VboPrimitive &o )
{
	o = s;

	const float *sp[3] = { s.pos0, s.pos1, s.pos2 };
	float *op[3] = { o.pos0, o.pos1, o.pos2 };
	const uint32_t uv[3] = { s.uv0[0], s.uv1[0], s.uv2[0] };
	uint32_t *ouv[3] = { &o.uv0[0], &o.uv1[0], &o.uv2[0] };
	const uint32_t *sc[3] = { s.color0, s.color1, s.color2 };
	uint32_t *oc[3] = { o.color0, o.color1, o.color2 };

	for ( int k = 0; k < 3; k++ )
	{
		const bary_t &w = corner[k];

		for ( int a = 0; a < 3; a++ )
			op[k][a] = w.b[0] * sp[0][a] + w.b[1] * sp[1][a] + w.b[2] * sp[2][a];

		o.normals[k] = vk_rtx_lerp_normal( s.normals, w );
		o.tangents[k] = vk_rtx_lerp_normal( s.tangents, w );
		*ouv[k] = vk_rtx_lerp_half2( uv, w );

		for ( int st = 0; st < 4; st++ )
		{
			const uint32_t c[3] = { sc[0][st], sc[1][st], sc[2][st] };
			oc[k][st] = vk_rtx_lerp_color( c, w );
		}
	}
}

// Room for `needed` primitives; the splits make the world geometry larger than its estimate.
static void vk_rtx_reserve_prims( vk_geometry_data_t *geom, uint32_t needed )
{
	if ( needed <= geom->num_primitives_allocated )
		return;

	const uint32_t allocated = needed + 4096;
	geom->primitives = (VboPrimitive*)realloc( geom->primitives, allocated * sizeof(VboPrimitive) );
	Com_Memset( geom->primitives + geom->num_primitives_allocated, 0, ( allocated - geom->num_primitives_allocated ) * sizeof(VboPrimitive) );
	geom->num_primitives_allocated = allocated;
}

// Cuts the primitives [first, first + count) of the geometry at the cluster boundaries. Returns the new count.
static uint32_t vk_rtx_split_prims_by_cluster( world_t &worldData, vk_geometry_data_t *geom, uint32_t first, uint32_t count )
{
	const std::vector<VboPrimitive> src( geom->primitives + first, geom->primitives + first + count );
	std::vector<VboPrimitive> dst;
	dst.reserve( count );

	for ( const VboPrimitive &p : src )
	{
		const float P[3][3] = { { p.pos0[0], p.pos0[1], p.pos0[2] }, { p.pos1[0], p.pos1[1], p.pos1[2] }, { p.pos2[0], p.pos2[1], p.pos2[2] } };

		vec3_t e1, e2, n, c;
		VectorSubtract( P[1], P[0], e1 );
		VectorSubtract( P[2], P[0], e2 );
		CrossProduct( e1, e2, n );

		std::vector<bary_poly_t> pieces;

		if ( VectorNormalize( n ) > 0.f )
		{
			// The side of the triangle that has air: the one with more open points on a grid
			// (the centre alone can lie in a solid, as on the kejim_post floor).
			const float step = 0.01f;
			const int grid = 6;
			int open[2] = { 0, 0 };
			vec3_t q;

			for ( int a = 0; a <= grid; a++ )
				for ( int b = 0; a + b <= grid; b++ )
				{
					const float w[3] = { (float)a / grid, (float)b / grid, 1.f - (float)( a + b ) / grid };
					for ( int k = 0; k < 3; k++ )
						c[k] = w[0] * P[0][k] + w[1] * P[1][k] + w[2] * P[2][k];

					VectorMA( c, step, n, q );
					open[0] += BSP_PointLeaf( worldData.nodes, q )->cluster >= 0;
					VectorMA( c, -step, n, q );
					open[1] += BSP_PointLeaf( worldData.nodes, q )->cluster >= 0;
				}

			const float shift = ( open[0] == 0 && open[1] == 0 ) ? 0.f : ( open[0] >= open[1] ) ? step : -step;

			if ( shift != 0.f )
			{
				bary_poly_t whole = { { { 1.f, 0.f, 0.f } }, { { 0.f, 1.f, 0.f } }, { { 0.f, 0.f, 1.f } } };
				vk_rtx_clip_cluster( worldData.nodes, whole, P, n, shift, pieces );
			}
		}

		if ( pieces.size() <= 1 || pieces.size() > 128 )
		{
			dst.push_back( p );
			continue;
		}

		s_split_triangles++;

		for ( const bary_poly_t &poly : pieces )
		{
			for ( size_t i = 1; i + 1 < poly.size(); i++ )
			{
				const bary_t corner[3] = { poly[0], poly[i], poly[i + 1] };
				VboPrimitive o;
				vk_rtx_primitive_piece( p, corner, o );
				dst.push_back( o );
			}
		}
	}

	if ( dst.size() == count )
		return count;

	s_split_added += (uint32_t)dst.size() - count;

	vk_rtx_reserve_prims( geom, first + (uint32_t)dst.size() );
	memcpy( geom->primitives + first, dst.data(), dst.size() * sizeof(VboPrimitive) );
	return (uint32_t)dst.size();
}

static void vk_rtx_collect_surfaces( uint32_t *prim_ctr, vk_geometry_data_t *geom, int type, world_t &worldData, mnode_t *node, int model_idx,
	int (*filter)(shader_t*), int (*filter_visibiliy)(shader_t*) )
{
	uint32_t	i, j;
	msurface_t	*surf, **mark;

	if ( node->contents == -1 ) 
	{
		for ( i = 0; i < 2; i++ ) 
		{
			vk_rtx_collect_surfaces( prim_ctr, geom, type, worldData, node->children[i], model_idx, filter, filter_visibiliy );
		}
	}

	vk_geometry_data_accel_t *accel = &geom->accel[type];
	
	mark = node->firstmarksurface;

	for ( j = 0; j < node->nummarksurfaces; j++ ) 
	{
		surf = mark[j];
		surf->notBrush = qtrue;

		if ( model_idx < 0 && belongs_to_model( worldData, surf ) )
			continue;

		shader_t* shader = tr.shaders[surf->shader->index];

		if ( surf->added || surf->skip )	
			continue;

		if ( !filter_visibiliy( shader ) )
			continue;

		rtx_material_t *mat = vk_rtx_shader_to_material( shader );
		uint32_t material_id = mat ? mat->flags : 0;

		if ( RB_IsSky(shader) )
		{
			if ( !filter( shader ) )
				continue;

			material_id = MAT_SetKind(material_id, MATERIAL_KIND_SKY);

			// ~sunny, force for now
			if ( (material_id & MATERIAL_FLAG_LIGHT) == 0 )
			{
				material_id |= MATERIAL_FLAG_LIGHT;
			}
		}
		else 
		{
			{	// ~sunny, debug
				// A glass shader that blends is composed with its GL factors as a layer in
				// primary_rays. The Q2RTX glass refracts and reflects it like a mirror.
				// Disabled: name rules of the reflection test.
				//if ( strstr( shader->name, "glass" ) && ( !mat || mat->blend_mode == RTX_BLEND_OPAQUE ) )
				//	material_id = MAT_SetKind(material_id, MATERIAL_KIND_GLASS);

				//if ( strstr( shader->name, "botton") || strstr( shader->name, "door02") )
				//	material_id = MAT_SetKind(material_id, MATERIAL_KIND_GLASS);

				//if ( shader->sort == SS_PORTAL && strstr( shader->name, "mirror" ) != NULL ) 
				//	material_id |= MATERIAL_FLAG_MIRROR;
			}

			if ( !filter( shader ) )
				continue;
		}

		if ( skip_invalid_surface( shader ) || *surf->data == SF_BAD || *surf->data == SF_SKIP) 
		{
			surf->skip = qtrue;
			continue;
		}

		tess.numVertexes = tess.numIndexes = 0;
		tess.shader = shader;
		tess.allowVBO = qfalse;

		rb_surfaceTable[*surf->data](surf->data);	// process surface

		if ( tess.numIndexes == 0 ) 
			continue;

		vk_geometry_dynamic_surf_t *dsurf = NULL;

		if ( geom->dynamic_flags )
		{
			dsurf = geom->host.dynamic_surfs + geom->host.surf_offset;

			dsurf->surf = surf;
			dsurf->shader = shader;
			dsurf->cluster = node->cluster;
			dsurf->fogIndex = 0;
			dsurf->prim_offset = *prim_ctr;

			geom->host.surf_offset++;
			accel->surf_count++;
		}





		vk_rtx_reserve_prims( geom, *prim_ctr + tess.numIndexes / 3 );
		VboPrimitive* surface_prims = geom->primitives + *prim_ctr;
		uint32_t prims_in_surface = create_poly( geom, mat, material_id, surface_prims );

		if ( dsurf )
			dsurf->prim_count = prims_in_surface;
		else
		{
			prims_in_surface = vk_rtx_split_prims_by_cluster( worldData, geom, *prim_ctr, prims_in_surface );
			surface_prims = geom->primitives + *prim_ctr;
		}

		for (uint32_t k = 0; k < prims_in_surface; ++k) {
			//if (model_idx < 0) world, sub bmodels have sep collector
			{
				float positions[9];
				VectorCopy(surface_prims[k].pos0, positions + 0);
				VectorCopy(surface_prims[k].pos1, positions + 3);
				VectorCopy(surface_prims[k].pos2, positions + 6);

				vec3_t center, anti_center;
				get_triangle_off_center(positions, center, anti_center, 0.01f);

				// The first leaf to list a surface claims it, so that cluster is wrong for most of a big floor.
				// Look up the triangle, on both sides: a Quake 3 floor winding puts `center` below it.
				int cluster = BSP_PointLeaf(worldData.nodes, center)->cluster;

				if (cluster < 0)
					cluster = BSP_PointLeaf(worldData.nodes, anti_center)->cluster;

				if (cluster < 0) {
					get_triangle_off_center(positions, center, anti_center, 1.f);
					cluster = BSP_PointLeaf(worldData.nodes, center)->cluster;

					if (cluster < 0)
						cluster = BSP_PointLeaf(worldData.nodes, anti_center)->cluster;
				}

				if (cluster < 0 && model_idx < 0)
					cluster = node->cluster;

				surface_prims[k].cluster = cluster;
			}

			//else{ // sub bmodels
			//	surface_prims[k].cluster = -1;
			//}
		}

		tess.numVertexes = tess.numIndexes = 0;	// ~sunny, redundant?

		surf->added = qtrue;

		*prim_ctr += prims_in_surface;
	}	
}

static void vk_rtx_face_bounds( const srfSurfaceFace_t *face, vec3_t mins, vec3_t maxs )
{
	ClearBounds( mins, maxs );
	for ( int i = 0; i < face->numPoints; i++ )
		AddPointToBounds( face->points[i], mins, maxs );
}

// GL draws the coplanar faces of brush models in sort order with a LEQUAL depth test, so the
// face drawn last is visible (kejim_post: display_green over display_none). The tracer hits one
// of them at random. The result is the distance to move the face along its normal: one step for
// each face on the same plane in another brush model that GL draws before it.
static float vk_rtx_coplanar_offset( world_t &worldData, int model, const msurface_t *surf )
{
	const float step = 0.01f;

	if ( *surf->data != SF_FACE )
		return 0.f;

	const srfSurfaceFace_t *face = (const srfSurfaceFace_t *)surf->data;
	vec3_t mins, maxs;
	vk_rtx_face_bounds( face, mins, maxs );

	int rank = 0;

	for ( int m = 1; m < worldData.num_bmodels; m++ )
	{
		const bmodel_t *other = &worldData.bmodels[m];

		if ( m == model )
			continue;
		if ( other->bounds[0][0] > maxs[0] || other->bounds[0][1] > maxs[1] || other->bounds[0][2] > maxs[2]
			|| other->bounds[1][0] < mins[0] || other->bounds[1][1] < mins[1] || other->bounds[1][2] < mins[2] )
			continue;

		for ( int i = 0; i < other->numSurfaces; i++ )
		{
			const msurface_t *surf2 = other->firstSurface + i;

			if ( *surf2->data != SF_FACE )
				continue;

			// GL sort key: the shader, then the entity.
			const int a = surf->shader->sortedIndex, b = surf2->shader->sortedIndex;
			if ( b > a || ( b == a && m > model ) )
				continue;

			const srfSurfaceFace_t *face2 = (const srfSurfaceFace_t *)surf2->data;
			if ( DotProduct( face->plane.normal, face2->plane.normal ) < 0.999f || fabsf( face->plane.dist - face2->plane.dist ) > 0.1f )
				continue;

			// The faces must overlap on the plane, not only touch.
			vec3_t mins2, maxs2;
			vk_rtx_face_bounds( face2, mins2, maxs2 );

			int overlap = 0;
			for ( int k = 0; k < 3; k++ )
			{
				const float lo = MAX( mins[k], mins2[k] ), hi = MIN( maxs[k], maxs2[k] );
				if ( hi < lo )
					break;
				if ( hi - lo > 0.1f )
					overlap++;
			}
			if ( overlap >= 2 )
				rank++;
		}
	}

	return rank * step;
}

static void vk_rtx_collect_bmodel_surfaces( uint32_t *prim_ctr, world_t &worldData, vk_geometry_data_t *geom, int type, bmodel_t *bmodel )
{
	msurface_t	*surf;
	const int	model = (int)( bmodel - worldData.bmodels );

	for ( int i = 0; i < bmodel->numSurfaces; i++ ) 
	{
		surf = bmodel->firstSurface + i;

		shader_t* shader = tr.shaders[surf->shader->index];

		if ( surf->added || surf->skip )	
			continue;

		if ( skip_invalid_surface( shader ) || *surf->data == SF_BAD || *surf->data == SF_SKIP)
		{
			surf->skip = qtrue;
			continue;
		}

		tess.numVertexes = tess.numIndexes = 0;
		tess.shader = shader;
		tess.allowVBO = qfalse;
		rb_surfaceTable[*surf->data](surf->data);

		// ~sunny, rework this
		rtx_material_t *mat = vk_rtx_shader_to_material( tess.shader );
		uint32_t material_id = (mat != NULL) ? mat->flags : 0U;

		VboPrimitive* surface_prims = geom->primitives + *prim_ctr;
		uint32_t prims_in_surface = create_poly( geom, mat, material_id, surface_prims );

		const float offset = vk_rtx_coplanar_offset( worldData, model, surf );
		if ( offset > 0.f )
		{
			const float *normal = ( (const srfSurfaceFace_t *)surf->data )->plane.normal;

			for ( uint32_t k = 0; k < prims_in_surface; ++k )
			{
				VectorMA( surface_prims[k].pos0, offset, normal, surface_prims[k].pos0 );
				VectorMA( surface_prims[k].pos1, offset, normal, surface_prims[k].pos1 );
				VectorMA( surface_prims[k].pos2, offset, normal, surface_prims[k].pos2 );
			}
		}

		for (uint32_t k = 0; k < prims_in_surface; ++k) {
			//if (model_idx < 0) world, sub bmodels have sep collector
			{
				float positions[9];
				VectorCopy(surface_prims[k].pos0, positions + 0);
				VectorCopy(surface_prims[k].pos1, positions + 3);
				VectorCopy(surface_prims[k].pos2, positions + 6);

				vec3_t center, anti_center;
				get_triangle_off_center(positions, center, anti_center, 0.01f);

				int cluster = BSP_PointLeaf(worldData.nodes, center)->cluster;
				//int cluster = (model_idx < 0) ? node->cluster : -1;

				if (cluster < 0) {
					get_triangle_off_center(positions, center, anti_center, 1.f);
					cluster = BSP_PointLeaf(worldData.nodes, center)->cluster;
				}

				surface_prims[k].cluster = cluster;
			}

			//else{ // sub bmodels
			//	surface_prims[k].cluster = -1;
			//}
		}

		tess.numVertexes = tess.numIndexes = 0;

		*prim_ctr += prims_in_surface;
	}
}

// The surface tessellation of the world knows no deforms. The wave and move scale of a surface
// comes from the current time, so the path tracer deforms each surface again every frame.
// The autosprite deforms leave the normals and tangents of their quads without a meaning.
static void vk_rtx_fix_autosprite_frames( const shader_t *shader )
{
	vec3_t	tangent;
	int		i, j;

	for ( i = 0; i < shader->numDeforms; i++ )
	{
		const deformStage_t *ds = shader->deforms[i];

		if ( ds->deformation == DEFORM_AUTOSPRITE )
		{
			// S runs along -left in RB_AddQuadStamp, which sets the normal.
			VectorNegate( backEnd.viewParms.ori.axis[1], tangent );

			for ( j = 0; j < tess.numVertexes; j++ )
			{
				VectorCopy( tangent, tess.qtangent[j] );
				tess.qtangent[j][3] = 1.0f;
			}
		}
		else if ( ds->deformation == DEFORM_AUTOSPRITE2 )
		{
			// Each quad keeps its vertices and gets a new plane.
			for ( j = 0; j + 3 < tess.numVertexes; j += 4 )
			{
				vec3_t	e1, e2, n;
				int		k;

				VectorSubtract( tess.xyz[j + 1], tess.xyz[j], e1 );
				VectorSubtract( tess.xyz[j + 2], tess.xyz[j], e2 );
				CrossProduct( e1, e2, n );

				if ( VectorNormalize( n ) == 0.0f )
					continue;

				if ( DotProduct( n, tess.normal[j] ) < 0.0f )
					VectorNegate( n, n );

				for ( k = 0; k < 4; k++ )
				{
					const float d = DotProduct( tess.qtangent[j + k], n );

					VectorCopy( n, tess.normal[j + k] );
					VectorMA( tess.qtangent[j + k], -d, n, tess.qtangent[j + k] );

					if ( VectorNormalize( tess.qtangent[j + k] ) == 0.0f )
						VectorCopy( e1, tess.qtangent[j + k] );
				}
			}
		}
	}
}

void vk_rtx_deform_world_geometry( trRefdef_t *refdef )
{
	static VboPrimitive	*scratch = NULL;
	static uint32_t		scratch_size = 0;

	vk_geometry_data_t	*geom;
	vk_geometry_host_t	*host;
	uint32_t			ring, i, k;

	if ( !tr.world )
		return;

	geom = &tr.world->geometry.world_dynamic_geometry;
	host = &geom->host;
	ring = vk.current_frame_index;
	host->num_deform_copies = 0;

	if ( !geom->primitives || !host->dynamic_surfs || geom->num_primitives == 0 || geom->buffer[0].buffer == VK_NULL_HANDLE )
		return;

	if ( !host->deform_copies )
		host->deform_copies = (VkBufferCopy*)calloc( MAX( 1, host->surf_count ) * 2, sizeof(VkBufferCopy) );

	if ( geom->deform_staging[ring].buffer == VK_NULL_HANDLE )
	{
		const size_t size = geom->vertex_data_offset + geom->num_primitives * sizeof(prim_positions_t);

		if ( vk_rtx_buffer_create( &geom->deform_staging[ring], size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) != VK_SUCCESS )
			return;

		geom->deform_staging_data[ring] = (uint8_t*)buffer_map( &geom->deform_staging[ring] );
	}

	// The deforms read the entity, the orientation and the times of the backend.
	trRefEntity_t *saved_entity = backEnd.currentEntity;
	const orientationr_t saved_ori = backEnd.ori;

	backEnd.currentEntity = &tr.worldEntity;
	backEnd.ori = backEnd.viewParms.world;
#ifdef USE_VBO
	tess.vbo_world_index = 0;
#endif

	uint8_t *staging = geom->deform_staging_data[ring];

	for ( i = 0; i < host->surf_offset; i++ )
	{
		vk_geometry_dynamic_surf_t *dsurf = host->dynamic_surfs + i;
		shader_t		*shader = (shader_t*)dsurf->shader;
		msurface_t		*surf = (msurface_t*)dsurf->surf;
		VboPrimitive	*prims = geom->primitives + dsurf->prim_offset;

		if ( !shader->numDeforms || dsurf->prim_count == 0 )
			continue;

		RB_BeginSurface( shader, dsurf->fogIndex );
		tess.shader = shader;
		tess.allowVBO = qfalse;

		rb_surfaceTable[*surf->data]( surf->data );

		// A curve can use another level of detail than at the load of the map.
		if ( (uint32_t)tess.numIndexes / 3 != dsurf->prim_count )
			continue;

		RB_DeformTessGeometry();
		vk_rtx_fix_autosprite_frames( shader );

		if ( (uint32_t)tess.numIndexes / 3 != dsurf->prim_count )
			continue;

		if ( scratch_size < dsurf->prim_count )
		{
			scratch = (VboPrimitive*)realloc( scratch, dsurf->prim_count * sizeof(VboPrimitive) );
			scratch_size = dsurf->prim_count;
		}

		create_poly( geom, vk_rtx_shader_to_material( shader ), prims[0].material_id, scratch );

		for ( k = 0; k < dsurf->prim_count; k++ )
		{
			VboPrimitive		*np = scratch + k;
			const VboPrimitive	*op = prims + k;

			np->cluster = op->cluster;

			// The motion to the previous position, as half floats like the models.
			if ( dsurf->deformed )
			{
				const float *oldp[3] = { op->pos0, op->pos1, op->pos2 };
				const float *newp[3] = { np->pos0, np->pos1, np->pos2 };
				unsigned int *custom[3] = { np->custom0, np->custom1, np->custom2 };

				for ( int v = 0; v < 3; v++ )
				{
					custom[v][0] = floatToHalf( oldp[v][0] - newp[v][0] ) | ( floatToHalf( oldp[v][1] - newp[v][1] ) << 16 );
					custom[v][1] = floatToHalf( oldp[v][2] - newp[v][2] );
				}
			}
		}

		Com_Memcpy( prims, scratch, dsurf->prim_count * sizeof(VboPrimitive) );
		dsurf->deformed = qtrue;

		Com_Memcpy( staging + dsurf->prim_offset * sizeof(VboPrimitive), prims, dsurf->prim_count * sizeof(VboPrimitive) );

		prim_positions_t *positions = (prim_positions_t*)( staging + geom->vertex_data_offset ) + dsurf->prim_offset;  // NOLINT(clang-diagnostic-cast-align)

		for ( k = 0; k < dsurf->prim_count; k++ )
		{
			VectorCopy( prims[k].pos0, positions[k][0] );
			VectorCopy( prims[k].pos1, positions[k][1] );
			VectorCopy( prims[k].pos2, positions[k][2] );
		}

		VkBufferCopy *copy = host->deform_copies + host->num_deform_copies++;
		copy->srcOffset = copy->dstOffset = dsurf->prim_offset * sizeof(VboPrimitive);
		copy->size = dsurf->prim_count * sizeof(VboPrimitive);

		copy = host->deform_copies + host->num_deform_copies++;
		copy->srcOffset = copy->dstOffset = geom->vertex_data_offset + dsurf->prim_offset * sizeof(prim_positions_t);
		copy->size = dsurf->prim_count * sizeof(prim_positions_t);
	}

	tess.numVertexes = tess.numIndexes = 0;
	backEnd.currentEntity = saved_entity;
	backEnd.ori = saved_ori;
}

// Copies the deformed surfaces to the device and builds the BLAS of the geometry again.
void vk_rtx_rebuild_world_deforms( VkCommandBuffer cmd_buf )
{
	vk_geometry_data_t	*geom;
	const uint32_t		ring = vk.current_frame_index;

	if ( !tr.world )
		return;

	geom = &tr.world->geometry.world_dynamic_geometry;

	if ( geom->host.num_deform_copies == 0 || geom->buffer[0].buffer == VK_NULL_HANDLE )
		return;

	qvkCmdCopyBuffer( cmd_buf, geom->deform_staging[ring].buffer, geom->buffer[0].buffer,
		geom->host.num_deform_copies, geom->host.deform_copies );

	BUFFER_BARRIER( cmd_buf, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
		geom->buffer[0].buffer, 0, VK_WHOLE_SIZE );

	build_model_blas( cmd_buf, &geom->geom_opaque, geom->vertex_data_offset, &geom->buffer[0] );
	build_model_blas( cmd_buf, &geom->geom_transparent, geom->vertex_data_offset, &geom->buffer[0] );
	build_model_blas( cmd_buf, &geom->geom_masked, geom->vertex_data_offset, &geom->buffer[0] );
}

static void vk_rtx_reset_world_geometry( vk_geometry_data_t *geom )
{
	uint32_t i;

	if ( geom == NULL )
		return;

	// host
	if ( geom->host.dynamic_surfs )
		free( geom->host.dynamic_surfs );

	if ( geom->host.deform_copies )
		free( geom->host.deform_copies );

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		if ( geom->deform_staging_data[i] )
			buffer_unmap( &geom->deform_staging[i] );

		vk_rtx_buffer_destroy( &geom->deform_staging[i] );
		geom->deform_staging_data[i] = NULL;
	}

	if ( geom->primitives )
		free( geom->primitives );

	Com_Memset( &geom->host, 0, sizeof(vk_geometry_host_t) );
	
	// device
	uint32_t num_instances = geom->dynamic_flags ? NUM_COMMAND_BUFFERS : 1; // ~sunny, keep this for now

	for ( i = 0; i < num_instances; i++ )
	{
		vk_rtx_buffer_destroy( &geom->buffer[i] );
	}

	Com_Memset( geom, 0, sizeof(geom) );
}

void vk_rtx_reset_world_geometries( world_t *world ) 
{
	free( world->cluster_aabbs );
	world->cluster_aabbs = NULL;

	Com_Memset( &world->world_aabb, 0, sizeof(aabb_t) );

	vk_rtx_reset_world_geometry( &world->geometry.sky_static );
	vk_rtx_reset_world_geometry( &world->geometry.world_static );
	vk_rtx_reset_world_geometry( &world->geometry.world_dynamic_material );
	vk_rtx_reset_world_geometry( &world->geometry.world_dynamic_geometry );
	vk_rtx_reset_world_geometry( &world->geometry.world_submodels );
#ifdef DEBUG_POLY_LIGHTS 
	vk_rtx_reset_world_geometry( &world->geometry.debug_light_polys );
#endif
	Com_Memset( &world->geometry, 0, sizeof(vkgeometry_t) );
}

static void vk_rtx_init_geometry( vk_geometry_data_t *geom, uint32_t blas_type_flags, uint32_t dynamic_flags, qboolean fast_build, qboolean allow_update )
{
	geom->blas_type_flags	= blas_type_flags;	// ~sunny, unused?
	geom->dynamic_flags		= dynamic_flags;	// ~sunny, unused?
	geom->fast_build		= fast_build;		// ~sunny, unused?
	geom->allow_update		= allow_update;		// ~sunny, unused?

	vkpt_init_model_geometry( &geom->geom_opaque,		1	);
	vkpt_init_model_geometry( &geom->geom_transparent,	1	);
	vkpt_init_model_geometry( &geom->geom_masked,		1	);
	vkpt_init_model_geometry( &geom->geom_forcesight,	1	);
}

static void vk_rtx_set_geomertry_accel_offsets( vk_geometry_data_t *geom, int type )
{
	vk_geometry_data_accel_t *accel = &geom->accel[type];

	accel->surf_offset			= geom->host.surf_offset;
}

// ~sunny, needs rework
static void vk_rtx_inject_light_poly_debug( vk_geometry_data_t *geom, world_t& worldData, float offset )
{
#if 0
	vk_geometry_data_accel_t *accel = &geom->accel[BLAS_TYPE_OPAQUE];

	for ( int nlight = 0; nlight < worldData.num_light_polys; nlight++ )
	{
		tess.numVertexes = tess.numIndexes = 0;
		tess.shader->sort	= 10;

		light_poly_t *light = worldData.light_polys + nlight;

		vec3_t a = { light->positions[0], light->positions[1], light->positions[2] };
		vec3_t b = { light->positions[3], light->positions[4], light->positions[5] };
		vec3_t c = { light->positions[6], light->positions[7], light->positions[8] };

		if ( offset > .0f )
		{
			// Get the light plane equation
			vec3_t e0, e1, normal;
			VectorSubtract(b, a, e0);
			VectorSubtract(c, a, e1);
			CrossProduct(e0, e1, normal);
			VectorNormalize(normal);

			for ( int j = 0; j < 3; j++ ) {
				VectorMA(a, offset, normal, a);
				VectorMA(b, offset, normal, b);
				VectorMA(c, offset, normal, c);
			}
		}

		int cluster = BSP_PointLeaf( worldData.nodes, a)->cluster;

		if ( cluster > 0 )
		{
			tess.shader = tr.whiteShader;
		}
		else {
			tess.shader	= tr.whiteShader;
		}

		color4ub_t color = { 255, 255, 0, 255 };
		RB_AddTriangle( a, b, c, color );

		const uint32_t num_clusters = (tess.numIndexes / 3);

		assert( geom->host.xyz_offset + tess.numVertexes <= geom->host.xyz_count );
		assert( geom->host.idx_offset + tess.numIndexes <= geom->host.idx_count );
		assert( geom->host.cluster_offset + num_clusters <= geom->host.cluster_count );

		vk_rtx_bind_indicies( geom->host.idx + geom->host.idx_offset, accel->xyz_count );
		vk_rtx_bind_vertices( geom->host.xyz + geom->host.xyz_offset, cluster );
		vk_rtx_bind_cluster( geom->host.cluster + geom->host.cluster_offset, num_clusters, cluster );

		geom->host.cluster_offset += num_clusters;
		geom->host.idx_offset += tess.numIndexes;
		geom->host.xyz_offset += tess.numVertexes;

		accel->idx_count += tess.numIndexes;
		accel->xyz_count += tess.numVertexes;

		accel->cluster_count += num_clusters;
	}
#endif
}

// ~sunny, re-check this
static void
mark_clusters_with_sky( const vk_geometry_data_t* geom, uint8_t* clusters_with_sky)
{
	uint32_t prim_idx;

	for ( prim_idx = 0; prim_idx < geom->geom_opaque.prim_counts[0]; prim_idx++ )
	{
		uint32_t prim = geom->geom_opaque.prim_offsets[0] + prim_idx;

		int cluster = geom->primitives[prim].cluster;

		if ( cluster < VIS_MAX_BYTES )
			clusters_with_sky[cluster >> 3] |= (1 << (cluster & 7));
	}
}

static void
compute_sky_visibility( world_t &worldData )
{
	memset(worldData.sky_visibility, 0, VIS_MAX_BYTES);

	vk_geometry_data_t *sky_static = &worldData.geometry.sky_static;

	if ( sky_static->num_primitives == 0 )
		return; 

	uint32_t numclusters = worldData.numClusters;

	uint8_t clusters_with_sky[VIS_MAX_BYTES] = { 0 };

	mark_clusters_with_sky( sky_static, clusters_with_sky );

	for (uint32_t cluster = 0; cluster < numclusters; cluster++)
	{
		if (clusters_with_sky[cluster >> 3] & (1 << (cluster & 7)))
		{
			byte* mask = BSP_GetPvs( &worldData, (int)cluster);

			for ( int i = 0; i < worldData.clusterBytes; i++ )
				worldData.sky_visibility[i] |= mask[i];
		}
	}
}

// Splits the light entities into spot, sky and ambient. A light that is not a spot is a sky light
// when its cluster is in the PVS of a cluster that has sky surfaces. It needs sky_visibility.
static void classify_entity_lights( world_t &worldData )
{
	int	counts[LIGHT_ENT_AMBIENT + 1] = { 0 };

	for ( int i = 0; i < worldData.num_light_polys; i++ )
	{
		light_poly_t *light = worldData.light_polys + i;

		if ( light->material || light->ent_class == LIGHT_ENT_EDIT )
			continue;

		if ( light->ent_class != LIGHT_ENT_SPOT )
		{
			const int c = light->cluster;
			const qboolean sky = ( c >= 0 && ( worldData.sky_visibility[c >> 3] & ( 1 << ( c & 7 ) ) ) ) ? qtrue : qfalse;

			light->ent_class = sky ? LIGHT_ENT_SKY : LIGHT_ENT_AMBIENT;
		}

		counts[light->ent_class]++;
	}

	Com_Printf( "rtx: entity lights: %i spot, %i sky, %i ambient\n",
		counts[LIGHT_ENT_SPOT], counts[LIGHT_ENT_SKY], counts[LIGHT_ENT_AMBIENT] );
}

#ifdef DEBUG_POLY_LIGHTS
static void vk_rtx_build_debug_light_poly_mesh( world_t &worldData, vk_geometry_data_t *geom )
{
	geom->host.surf_count = 0;
	geom->host.surf_offset = 0;

	geom->primitives = (VboPrimitive*)calloc( MAX( 1, (geom->num_primitives_allocated )),  sizeof(VboPrimitive) );
	Com_Memset( geom->primitives, 0, MAX( 1, (geom->num_primitives_allocated)) * sizeof(VboPrimitive) );

	VboPrimitive *primitives_out = geom->primitives + 0;

	rtx_material_t *mat = vk_rtx_shader_to_material( tr.shaders[0] );

	for ( int nlight = 0; nlight < geom->num_primitives; nlight++ )
	{
		light_poly_t *light = worldData.light_polys + nlight;

		memset(primitives_out, 0, sizeof(VboPrimitive));

		vec3_t a { light->positions[0], light->positions[1], light->positions[2] };
		vec3_t b { light->positions[3], light->positions[4], light->positions[5] };
		vec3_t c { light->positions[6], light->positions[7], light->positions[8] };

		if ( DEBUG_POLY_LIGHTS > .0f )
		{
			// Get the light plane equation
			vec3_t e0, e1, normal;
			VectorSubtract(b, a, e0);
			VectorSubtract(c, a, e1);
			CrossProduct(e0, e1, normal);
			VectorNormalize(normal);

			for ( int j = 0; j < 3; j++ ) {
				VectorMA(a, DEBUG_POLY_LIGHTS, normal, a);
				VectorMA(b, DEBUG_POLY_LIGHTS, normal, b);
				VectorMA(c, DEBUG_POLY_LIGHTS, normal, c);
			}

			primitives_out->normals[0] = encode_normal( normal );
			primitives_out->normals[1] = encode_normal( normal );
			primitives_out->normals[2] = encode_normal( normal );
		}

		int cluster = BSP_PointLeaf( worldData.nodes, a)->cluster;

		VectorCopy( a, primitives_out->pos0 );
		VectorCopy( b, primitives_out->pos1 );
		VectorCopy( c, primitives_out->pos2 );

		float alpha = 1.f;
		float emissive = 0.f;

		primitives_out->material_id = mat->flags;
		primitives_out->emissive_and_alpha = floatToHalf(emissive) | (floatToHalf(alpha) << 16);
		primitives_out->instance = 0;

		++primitives_out;
	}
}
#endif

void R_PreparePT( world_t &worldData ) 
{
	if ( !vk.rtxActive )
		return;

	uint32_t i, prim_ctr, first_prim;

	// polygonal lights
	worldData.num_light_polys = 0;
	worldData.allocated_light_polys = 0;
	worldData.light_polys = NULL;

	// reset acceleration structures, redundant
	vk_debug( "rtx world: reset geometries\n" );
	vk_rtx_reset_world_geometries( tr.world );

	vk_geometry_data_t *sky_static				= &worldData.geometry.sky_static;
	vk_geometry_data_t *world_static			= &worldData.geometry.world_static;
	vk_geometry_data_t *world_dynamic_material	= &worldData.geometry.world_dynamic_material;
	vk_geometry_data_t *world_dynamic_geometry	= &worldData.geometry.world_dynamic_geometry;
	vk_geometry_data_t *world_submodels			= &worldData.geometry.world_submodels;
#ifdef DEBUG_POLY_LIGHTS 
	vk_geometry_data_t *debug_light_polys		= &worldData.geometry.debug_light_polys;
#endif

	// initialize .. 								type (unused)			dynamic_flags(unused)		fast_build (unused)  allow_update (unused)	
	vk_rtx_init_geometry( sky_static,				BLAS_TYPE_FLAG_OPAQUE,	BLAS_DYNAMIC_FLAG_NONE,		qtrue,				 qfalse	);
	vk_rtx_init_geometry( world_static,				BLAS_TYPE_FLAG_ALL,		BLAS_DYNAMIC_FLAG_NONE,		qtrue,				 qfalse	);
	vk_rtx_init_geometry( world_dynamic_material,	BLAS_TYPE_FLAG_ALL,		BLAS_DYNAMIC_FLAG_ALL,		qfalse,				 qtrue	);
	vk_rtx_init_geometry( world_dynamic_geometry,	BLAS_TYPE_FLAG_ALL,		BLAS_DYNAMIC_FLAG_ALL,		qfalse,				 qtrue	);
	vk_rtx_init_geometry( world_submodels,			BLAS_TYPE_FLAG_OPAQUE,	BLAS_DYNAMIC_FLAG_NONE,		qtrue,				 qfalse	);
#ifdef DEBUG_POLY_LIGHTS 
	vk_rtx_init_geometry( debug_light_polys,		BLAS_TYPE_FLAG_OPAQUE,	BLAS_DYNAMIC_FLAG_NONE,		qtrue,				 qfalse	);
#endif

	// esitmate size of world geometries
	vk_rtx_estimate_geometry( worldData, worldData.nodes, sky_static,				filter_sky );
	vk_rtx_estimate_geometry( worldData, worldData.nodes, world_static,				filter_static_or_forcesight );
	vk_rtx_estimate_geometry( worldData, worldData.nodes, world_dynamic_material,	filter_dynamic_material );
	vk_rtx_estimate_geometry( worldData, worldData.nodes, world_dynamic_geometry,	filter_dynamic_geometry );
	vk_rtx_estimate_bmodels( worldData, world_submodels );

	// Q2RTX, collects these into one buffer, 
	// here use separate buffers, for now (idea for dynamic shaders)

	s_node_cluster.clear();
	s_split_triangles = s_split_added = 0;

	// sky
	prim_ctr = 0;
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, sky_static, BLAS_TYPE_OPAQUE, worldData, worldData.nodes, -1, filter_sky, filter_opaque );
		vkpt_append_model_geometry(&sky_static->geom_opaque, prim_ctr - first_prim, first_prim, "sky opqaue");
	sky_static->num_primitives = prim_ctr;

	// opaque
	prim_ctr = 0;
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_static, BLAS_TYPE_OPAQUE, worldData, worldData.nodes, -1, filter_static, filter_opaque );
		vkpt_append_model_geometry(&world_static->geom_opaque, prim_ctr - first_prim, first_prim, "bsp static opaque");
	vk_rtx_set_geomertry_accel_offsets( world_static, BLAS_TYPE_TRANSPARENT );
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_static, BLAS_TYPE_TRANSPARENT, worldData, worldData.nodes, -1, filter_static, filter_transparent );
		vkpt_append_model_geometry(&world_static->geom_transparent, prim_ctr - first_prim, first_prim, "bsp static transparent");
	vk_rtx_set_geomertry_accel_offsets( world_static, BLAS_TYPE_MASKED );
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_static, BLAS_TYPE_MASKED, worldData, worldData.nodes, -1, filter_static, filter_masked );
		vkpt_append_model_geometry(&world_static->geom_masked, prim_ctr - first_prim, first_prim, "bsp static masked");
	vk_rtx_set_geomertry_accel_offsets( world_static, BLAS_TYPE_TRANSPARENT );
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_static, BLAS_TYPE_TRANSPARENT, worldData, worldData.nodes, -1, filter_static_or_forcesight, filter_forcesight );
		vkpt_append_model_geometry(&world_static->geom_forcesight, prim_ctr - first_prim, first_prim, "bsp static forcesight");
	world_static->num_primitives = prim_ctr;


	// dynamic material
	prim_ctr = 0;
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_dynamic_material, BLAS_TYPE_OPAQUE, worldData, worldData.nodes, -1, filter_dynamic_material, filter_opaque );
		vkpt_append_model_geometry(&world_dynamic_material->geom_opaque, prim_ctr - first_prim, first_prim, "bsp d material opaque");
	vk_rtx_set_geomertry_accel_offsets( world_dynamic_material, BLAS_TYPE_TRANSPARENT );
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_dynamic_material, BLAS_TYPE_TRANSPARENT, worldData, worldData.nodes, -1, filter_dynamic_material, filter_transparent );
		vkpt_append_model_geometry(&world_dynamic_material->geom_transparent, prim_ctr - first_prim, first_prim, "bsp d material transparent");
	vk_rtx_set_geomertry_accel_offsets( world_dynamic_material, BLAS_TYPE_MASKED );
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_dynamic_material, BLAS_TYPE_MASKED, worldData, worldData.nodes, -1, filter_dynamic_material, filter_masked );
		vkpt_append_model_geometry(&world_dynamic_material->geom_masked, prim_ctr - first_prim, first_prim, "bsp d material masked");
	world_dynamic_material->num_primitives = prim_ctr;

	// dynamic geometry
	prim_ctr = 0;
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_dynamic_geometry, BLAS_TYPE_OPAQUE, worldData, worldData.nodes, -1, filter_dynamic_geometry, filter_opaque );
		vkpt_append_model_geometry(&world_dynamic_geometry->geom_opaque, prim_ctr - first_prim, first_prim, "bsp d geometry opaque");
	vk_rtx_set_geomertry_accel_offsets( world_dynamic_geometry, BLAS_TYPE_TRANSPARENT );
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_dynamic_geometry, BLAS_TYPE_TRANSPARENT, worldData, worldData.nodes, -1, filter_dynamic_geometry, filter_transparent );
		vkpt_append_model_geometry(&world_dynamic_geometry->geom_transparent, prim_ctr - first_prim, first_prim, "bsp d geomatry transparent");
	vk_rtx_set_geomertry_accel_offsets( world_dynamic_geometry, BLAS_TYPE_MASKED );
	first_prim = prim_ctr;
	vk_rtx_collect_surfaces( &prim_ctr, world_dynamic_geometry, BLAS_TYPE_MASKED, worldData, worldData.nodes, -1, filter_dynamic_geometry, filter_masked );
		vkpt_append_model_geometry(&world_dynamic_geometry->geom_masked, prim_ctr - first_prim, first_prim, "bsp d geomatry masked");
	world_dynamic_geometry->num_primitives = prim_ctr;

	Com_Printf( "rtx: %u world triangles split at cluster boundaries, %u triangles added\n", s_split_triangles, s_split_added );

	// sub brush models
	prim_ctr = 0;
	for ( i = 0; i < worldData.num_bmodels; i++ )
	{
		bmodel_t *bmodel = &worldData.bmodels[i];
		first_prim = prim_ctr;
		vk_rtx_collect_bmodel_surfaces( &prim_ctr, worldData, world_submodels, BLAS_TYPE_OPAQUE, bmodel );
		vkpt_init_model_geometry(&bmodel->geometry, 1);
		vkpt_append_model_geometry( &bmodel->geometry, prim_ctr - first_prim, first_prim, "bsp_model");
	}
	world_submodels->num_primitives = prim_ctr;

	for ( i = 0; i < worldData.num_bmodels; i++ ) 
	{
		bmodel_t *bmodel = &worldData.bmodels[i];

		// ~sunny, recheck this
		compute_aabb( world_submodels->primitives + bmodel->geometry.prim_offsets[0], bmodel->geometry.prim_counts[0], bmodel->aabb_min, bmodel->aabb_max);
		VectorAdd( bmodel->aabb_min, bmodel->aabb_max, bmodel->center );
		VectorScale( bmodel->center, 0.5f, bmodel->center );
	}

	compute_aabb( world_static->primitives + world_static->geom_opaque.prim_offsets[0],			world_static->geom_opaque.prim_counts[0],		worldData.world_aabb.mins, worldData.world_aabb.maxs );
	append_aabb(  world_static->primitives + world_static->geom_transparent.prim_offsets[0],	world_static->geom_transparent.prim_counts[0],	worldData.world_aabb.mins, worldData.world_aabb.maxs );
	append_aabb(  world_static->primitives + world_static->geom_masked.prim_offsets[0],			world_static->geom_masked.prim_counts[0],		worldData.world_aabb.mins, worldData.world_aabb.maxs );

	vec3_t margin = { 1.f, 1.f, 1.f };
	VectorSubtract( worldData.world_aabb.mins, margin, worldData.world_aabb.mins );
	VectorAdd( worldData.world_aabb.maxs, margin, worldData.world_aabb.maxs );

	vk_compute_cluster_aabbs( worldData );

	RTX_LightEdit_Reset( worldData );

	collect_light_polys( worldData, -1, &worldData.num_light_polys, &worldData.allocated_light_polys, &worldData.light_polys );

	Com_Printf( "rtx: %i light polys collected from %i world surfaces\n",
		worldData.num_light_polys, worldData.numsurfaces );

	// A map that kept its own lights needs no reconstruction. One that did not gets its
	// lights rebuilt from the baked lightmaps. The .lgt file is read in both cases: the
	// lgt lights only when the map has no entity light, the edits always.
	const int entity_lights = collect_entity_lights( worldData );

	if ( entity_lights == 0 )
		R_LightGen_EnsureForMap( worldData.baseName );

	RTX_LightFile_Load( worldData, entity_lights > 0 ? qtrue : qfalse );

#ifdef DEBUG_POLY_LIGHTS
	debug_light_polys->num_primitives = worldData.num_light_polys;
	debug_light_polys->num_primitives_allocated = worldData.num_light_polys;
	vk_rtx_build_debug_light_poly_mesh( worldData, debug_light_polys );
	vkpt_append_model_geometry( &debug_light_polys->geom_opaque, worldData.num_light_polys, 0, "bsp_model");
#endif

	for ( i = 0; i < worldData.num_bmodels; i++ ) 
	{
		bmodel_t *bmodel = &worldData.bmodels[i];

		bmodel->num_light_polys = 0;
		bmodel->allocated_light_polys = 0;
		bmodel->light_polys = NULL;

		collect_light_polys( worldData, i, &bmodel->num_light_polys, &bmodel->allocated_light_polys, &bmodel->light_polys );
	
		bmodel->transparent = false; // is_model_transparent(wm, model);
		// One BLAS holds the whole brush model. With a masked surface in it (a func_ grate)
		// it takes the masked any-hit, which keeps every hit on the other surfaces.
		bmodel->masked = false;
		for ( int s = 0; s < bmodel->numSurfaces; s++ )
		{
			const msurface_t *surf = bmodel->firstSurface + s;

			if ( !surf->skip && surf->shader && RB_IsMasked( tr.shaders[surf->shader->index] ) )
			{
				bmodel->masked = true;
				break;
			}
		}
		vk_debug( "rtx world: bmodel %i, %i light polys\n", i, bmodel->num_light_polys );
	}

#ifdef DEBUG_POLY_LIGHTS
	vk_rtx_inject_light_poly_debug( world_static, worldData, DEBUG_POLY_LIGHTS );
#endif

	vk_debug( "rtx world: cluster lights\n" );
	collect_cluster_lights( worldData );

	vk_debug( "rtx world: sky visibility\n" );
	compute_sky_visibility( worldData );
	classify_entity_lights( worldData );
	RTX_LightEdit_FinalizeLoad( worldData );
	RTX_LightFile_Apply( worldData );

	vk_debug( "rtx world: light buffers\n" );
	vkpt_light_buffers_create( worldData  );

	// create buffers and upload and create blas
	vk_debug( "rtx world: vertex buffer upload\n" );
	vkpt_vertex_buffer_upload_bsp_mesh( worldData );

	vk_debug( "rtx world: envmap\n" );
	vk_rtx_prepare_envmap( worldData );

	vk_debug( "rtx world: physical sky\n" );
	vkpt_physical_sky_initialize();

	vk_debug( "rtx world: primary ray resources\n" );
	vk_rtx_create_primary_rays_resources( worldData );

	vk_debug( "rtx world: done\n" );
}