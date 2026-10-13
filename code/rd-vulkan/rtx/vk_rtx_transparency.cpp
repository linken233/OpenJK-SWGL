/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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

#include <vector>
#include <algorithm>


#define MAX_SABER_LIGHTS 128

#define TR_PARTICLE_MAX_NUM    16384
#define TR_BEAM_MAX_NUM        1024
#define TR_SPRITE_MAX_NUM      16384
#define TR_VERTEX_MAX_NUM      ((TR_PARTICLE_MAX_NUM + TR_SPRITE_MAX_NUM) * 4)
#define TR_INDEX_MAX_NUM       ((TR_PARTICLE_MAX_NUM + TR_SPRITE_MAX_NUM) * 6)
#define TR_BEAM_AABB_SIZE      sizeof(VkAabbPositionsKHR)
#define TR_POSITION_SIZE       (3 * sizeof(float))
#define TR_COLOR_SIZE          (4 * sizeof(float))
#define TR_BEAM_INTERSECT_SIZE (12 * sizeof(float))
// Three uvec4 per sprite: material, entity colour, kind; the UVs of a poly triangle; its vertex colours.
#define TR_SPRITE_INFO_UINTS   12
#define TR_SPRITE_INFO_SIZE    (TR_SPRITE_INFO_UINTS * sizeof(uint32_t))

#define LENGTH(a) ((sizeof (a)) / (sizeof(*(a))))

#define VectorAdd3(a,b,c,d) \
        ((d)[0]=(a)[0]+(b)[0]+(c)[0], \
         (d)[1]=(a)[1]+(b)[1]+(c)[1], \
         (d)[2]=(a)[2]+(b)[2]+(c)[2])

struct
{
	size_t vertex_position_host_offset;
	size_t beam_aabb_host_offset;
	size_t particle_color_host_offset;
	size_t beam_color_host_offset;
	size_t sprite_info_host_offset;
	size_t current_upload_size;

	size_t beam_intersect_host_offset;

	size_t sprite_vertex_device_offset;

	size_t host_buffer_size;
	size_t host_frame_size;
	unsigned int particle_num;
	unsigned int beam_num;
	unsigned int sprite_num;
	unsigned int host_frame_index;
	unsigned int host_buffered_frame_num;
	char* mapped_host_buffer;
	char* host_buffer_shadow;
	vkbuffer_t vertex_buffer;
	vkbuffer_t index_buffer;
	vkbuffer_t beam_aabb_buffer;
	vkbuffer_t particle_color_buffer;
	vkbuffer_t beam_color_buffer;
	vkbuffer_t sprite_info_buffer;
	vkbuffer_t beam_intersect_buffer;
	VkBufferView particle_color_buffer_view;
	VkBufferView beam_color_buffer_view;
	VkBufferView sprite_info_buffer_view;
	VkBufferView beam_intersect_buffer_view;
	VkBuffer host_buffer;
	VkDeviceMemory host_buffer_memory;
	VkBufferMemoryBarrier transfer_barriers[6];
} transparency;

// initialization
static void create_buffers(void);
static bool allocate_and_bind_memory_to_buffers(void);
static void create_buffer_views(void);
static void fill_index_buffer(void);


// update
//static void write_particle_geometry(const float* view_matrix, const particle_t* particles, int particle_num);
//static void write_beam_geometry(const entity_t* entities, int entity_num);
static void write_sprite_geometry(const float* view_matrix, const trRefdef_t *refdef);
static void upload_geometry(VkCommandBuffer command_buffer);

cvar_t* cvar_pt_particle_size = NULL;
cvar_t* cvar_pt_beam_width = NULL;
cvar_t* cvar_pt_beam_lights = NULL;
extern cvar_t* cvar_pt_enable_particles;
extern cvar_t* cvar_pt_particle_emissive;
extern cvar_t* cvar_pt_projection;

//void cast_u32_to_f32_color(int color_index, const color_t* pcolor, float* color_f32, float hdr_factor)
//{
//	color_t color;
//	if (color_index < 0)
//		color.u32 = pcolor->u32;
//	else
//		color.u32 = d_8to24table[color_index & 0xff];
//
//	for (int i = 0; i < 3; i++)
//		color_f32[i] = hdr_factor * decode_srgb(color.u8[i]);
//}

bool initialize_transparency()
{
	cvar_pt_particle_size = ri.Cvar_Get("pt_particle_size", "0.5", 0);
	cvar_pt_beam_width = ri.Cvar_Get("pt_beam_width", "1.0", 0);
	cvar_pt_beam_lights = ri.Cvar_Get("pt_beam_lights", "1.0", 0);

	memset(&transparency, 0, sizeof(transparency));

	const size_t particle_vertex_position_max_size = TR_VERTEX_MAX_NUM * TR_POSITION_SIZE;
	const size_t particle_color_size = TR_PARTICLE_MAX_NUM * TR_COLOR_SIZE;
	const size_t particle_data_size = particle_vertex_position_max_size + particle_color_size;

	const size_t beam_aabb_max_size = TR_BEAM_MAX_NUM * TR_BEAM_AABB_SIZE;
	const size_t beam_color_size = TR_BEAM_MAX_NUM * TR_COLOR_SIZE;
	const size_t beam_intersect_size = TR_BEAM_MAX_NUM * TR_BEAM_INTERSECT_SIZE;
	const size_t beam_data_size = beam_aabb_max_size + beam_color_size + beam_intersect_size;

	const size_t sprite_vertex_position_max_size = TR_SPRITE_MAX_NUM * 4 * TR_POSITION_SIZE;
	const size_t sprite_info_size = TR_SPRITE_MAX_NUM * TR_SPRITE_INFO_SIZE;
	const size_t sprite_data_size = sprite_vertex_position_max_size + sprite_info_size;
	
	transparency.host_buffered_frame_num = VK_MAX_SWAPCHAIN_SIZE;
	transparency.host_frame_size = particle_data_size + beam_data_size + sprite_data_size;
	transparency.host_buffer_size = transparency.host_buffered_frame_num * transparency.host_frame_size;

	create_buffers();

	if (allocate_and_bind_memory_to_buffers() != VK_TRUE)
		return false;

	create_buffer_views();
	fill_index_buffer();

	return true;
}

void destroy_transparency()
{
	qvkDestroyBufferView(vk.device, transparency.particle_color_buffer_view, NULL);
	qvkDestroyBufferView(vk.device, transparency.beam_color_buffer_view, NULL);
	qvkDestroyBufferView(vk.device, transparency.sprite_info_buffer_view, NULL);
	qvkDestroyBufferView(vk.device, transparency.beam_intersect_buffer_view, NULL);
	vk_rtx_buffer_destroy(&transparency.vertex_buffer);
	vk_rtx_buffer_destroy(&transparency.index_buffer);
	vk_rtx_buffer_destroy(&transparency.beam_aabb_buffer);
	vk_rtx_buffer_destroy(&transparency.particle_color_buffer);
	vk_rtx_buffer_destroy(&transparency.beam_color_buffer);
	vk_rtx_buffer_destroy(&transparency.sprite_info_buffer);
	vk_rtx_buffer_destroy(&transparency.beam_intersect_buffer);

	qvkDestroyBuffer(vk.device, transparency.host_buffer, NULL);
	qvkFreeMemory(vk.device, transparency.host_buffer_memory, NULL);

	if (transparency.host_buffer_shadow)
	{
		// Was freeing the address of the pointer rather than the block it holds.
		Z_Free(transparency.host_buffer_shadow);
		transparency.host_buffer_shadow = NULL;
	}
}

// Entities the rasterizer tessellates (bolts, rings, cylinders, shields, clouds). The tracer takes each one as
// triangles of the sprite path: one slot per triangle, UVs and colours per corner. Same geometry as rd-vanilla.
typedef struct
{
	rtx_material_t	*mat;
	polyVert_t		v[3];
	bool			lit;	// an albedo layer that the tracer lights (grass), not an effect
} fx_tri_t;

static fx_tri_t			fx_tris[TR_SPRITE_MAX_NUM];
static int				fx_tri_num;
static const trRefdef_t	*fx_refdef;
static const refEntity_t *fx_ent;
static rtx_material_t	*fx_mat;
static bool				fx_lit;
static vec3_t			fx_sh1, fx_sh2;
static int				fx_f_count;
static int				fx_seed;

static float fx_random( void )
{
	fx_seed = 69069 * fx_seed + 1;
	return ( fx_seed & 0xffff ) / (float)0x10000;
}

static float fx_crandom( void )
{
	return 2.0f * ( fx_random() - 0.5f );
}

static void fx_vert( polyVert_t *v, const vec3_t xyz, float s, float t, const byte *rgba )
{
	VectorCopy( xyz, v->xyz );
	v->st[0] = s;
	v->st[1] = t;
	Com_Memcpy( v->modulate, rgba, 4 );
}

static void fx_tri( const polyVert_t *a, const polyVert_t *b, const polyVert_t *c )
{
	if ( fx_tri_num >= TR_SPRITE_MAX_NUM )
		return;

	fx_tri_t *t = fx_tris + fx_tri_num++;

	t->mat = fx_mat;
	t->lit = fx_lit;
	t->v[0] = *a;
	t->v[1] = *b;
	t->v[2] = *c;
}

// The quad of a line: the triangles ( 0 1 2 ) and ( 2 1 3 ).
static void fx_quad_strip( const polyVert_t *v )
{
	fx_tri( v + 0, v + 1, v + 2 );
	fx_tri( v + 2, v + 1, v + 3 );
}

// A quad of a lathe or a cloud: the triangles ( 0 1 3 ) and ( 3 2 0 ).
static void fx_quad_lathe( const polyVert_t *v )
{
	fx_tri( v + 0, v + 1, v + 3 );
	fx_tri( v + 3, v + 2, v + 0 );
}

// A quad stamp, the corners in the order of RB_AddQuadStampExt.
static void fx_quad_stamp( const vec3_t origin, const vec3_t left, const vec3_t up )
{
	polyVert_t	v[4];
	vec3_t		p;

	VectorAdd( origin, left, p );		VectorAdd( p, up, p );			fx_vert( v + 0, p, 0, 0, fx_ent->shaderRGBA );
	VectorSubtract( origin, left, p );	VectorAdd( p, up, p );			fx_vert( v + 1, p, 1, 0, fx_ent->shaderRGBA );
	VectorSubtract( origin, left, p );	VectorSubtract( p, up, p );		fx_vert( v + 2, p, 1, 1, fx_ent->shaderRGBA );
	VectorAdd( origin, left, p );		VectorSubtract( p, up, p );		fx_vert( v + 3, p, 0, 1, fx_ent->shaderRGBA );

	fx_tri( v + 0, v + 1, v + 3 );
	fx_tri( v + 3, v + 1, v + 2 );
}

static void fx_line2( const vec3_t start, const vec3_t end, const vec3_t up, float w0, float w1, float tc0, float tc1 )
{
	polyVert_t	v[4];
	vec3_t		p;

	VectorMA( start, w0, up, p );	fx_vert( v + 0, p, 0, tc0, fx_ent->shaderRGBA );
	VectorMA( start, -w0, up, p );	fx_vert( v + 1, p, 1, tc0, fx_ent->shaderRGBA );
	VectorMA( end, w1, up, p );		fx_vert( v + 2, p, 0, tc1, fx_ent->shaderRGBA );
	VectorMA( end, -w1, up, p );	fx_vert( v + 3, p, 1, tc1, fx_ent->shaderRGBA );

	fx_quad_strip( v );
}

static void fx_create_shape( void )
{
	VectorSet( fx_sh1, 0.66f, 0.08f + Q_flrand( -1.0f, 1.0f ) * 0.02f, 0.08f + Q_flrand( -1.0f, 1.0f ) * 0.02f );
	VectorSet( fx_sh2, 0.33f, -fx_sh1[1] + Q_flrand( -1.0f, 1.0f ) * 0.02f, -fx_sh1[2] + Q_flrand( -1.0f, 1.0f ) * 0.02f );
}

static void fx_apply_shape( const vec3_t start, const vec3_t end, const vec3_t right, float sradius, float eradius, int count, float start_perc, float end_perc )
{
	vec3_t	point1, point2, fwd, rt, up;
	float	perc, dis, rads1, rads2;

	if ( count < 1 )
	{
		fx_line2( start, end, right, sradius, eradius, start_perc, end_perc );
		return;
	}

	fx_create_shape();

	VectorSubtract( end, start, fwd );
	dis = VectorNormalize( fwd ) * 0.7f;
	MakeNormalVectors( fwd, rt, up );

	perc = fx_sh1[0];
	VectorScale( start, perc, point1 );
	VectorMA( point1, 1.0f - perc, end, point1 );
	VectorMA( point1, dis * fx_sh1[1], rt, point1 );
	VectorMA( point1, dis * fx_sh1[2], up, point1 );

	rads1 = sradius * 0.666f + eradius * 0.333f;
	rads2 = sradius * 0.333f + eradius * 0.666f;

	fx_apply_shape( start, point1, right, sradius, rads1, count - 1, start_perc, start_perc * 0.666f + end_perc * 0.333f );

	perc = fx_sh2[0];
	VectorScale( start, perc, point2 );
	VectorMA( point2, 1.0f - perc, end, point2 );
	VectorMA( point2, dis * fx_sh2[1], rt, point2 );
	VectorMA( point2, dis * fx_sh2[2], up, point2 );

	fx_apply_shape( point2, point1, right, rads1, rads2, count - 1, start_perc * 0.333f + end_perc * 0.666f, start_perc * 0.666f + end_perc * 0.333f );
	fx_apply_shape( point2, end, right, rads2, eradius, count - 1, start_perc * 0.333f + end_perc * 0.666f, end_perc );
}

static void fx_bolt_seg( const vec3_t start, const vec3_t end, const vec3_t right, float radius )
{
	const refEntity_t *e = fx_ent;
	vec3_t	fwd, old, cur, rt, up, temp, off = { 10, 10, 10 };
	float	dis, old_perc = 0.0f, perc, old_radius, new_radius;

	VectorSubtract( end, start, fwd );
	dis = VectorNormalize( fwd );

	if ( dis > 2000.0f )
		dis = 2000.0f;

	MakeNormalVectors( fwd, rt, up );
	VectorCopy( start, old );
	new_radius = old_radius = radius;

	for ( int i = 16; i <= dis; i += 16 )
	{
		perc = ( i + 16 > dis ) ? 1.0f : (float)i / dis;

		VectorScale( fwd, fx_crandom() * 3.0f, temp );
		VectorMA( temp, fx_crandom() * 7.0f * e->angles[0], rt, temp );
		VectorMA( temp, fx_crandom() * 7.0f * e->angles[0], up, temp );
		VectorAdd( off, temp, off );

		VectorAdd( start, off, cur );
		VectorScale( cur, 1.0f - perc, cur );
		VectorMA( cur, perc, end, cur );

		if ( e->renderfx & RF_TAPERED )
		{
			old_radius = radius * ( 1.0f - old_perc * old_perc );
			new_radius = radius * ( 1.0f - perc * perc );
		}

		fx_apply_shape( cur, old, right, new_radius, old_radius, 2 - r_lodbias->integer, 0, 1 );

		if ( ( e->renderfx & RF_FORKED ) && fx_f_count > 0 && fx_random() > 0.93f && ( 1.0f - perc ) > 0.8f )
		{
			vec3_t new_dest;

			fx_f_count--;
			VectorAdd( cur, e->oldorigin, new_dest );
			VectorScale( new_dest, 0.5f, new_dest );

			for ( int t = 0; t < 3; t++ )
				new_dest[t] += fx_crandom() * 80.0f;

			fx_bolt_seg( cur, new_dest, right, new_radius );
		}

		VectorCopy( cur, old );
		old_perc = perc;
	}
}

static void fx_electricity( void )
{
	const refEntity_t *e = fx_ent;
	vec3_t	right, fwd, start, end, v1, v2;
	float	perc = 1.0f, dis;

	VectorCopy( e->origin, start );
	VectorSubtract( e->oldorigin, start, fwd );
	dis = VectorNormalize( fwd );

	if ( e->renderfx & RF_GROW )
	{
		perc = 1.0f - ( e->endTime - fx_refdef->time ) / e->angles[1];
		perc = Com_Clamp( 0.0f, 1.0f, perc );
	}

	VectorMA( start, perc * dis, fwd, end );

	VectorSubtract( start, fx_refdef->vieworg, v1 );
	VectorSubtract( end, fx_refdef->vieworg, v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	fx_f_count = 3;
	fx_bolt_seg( start, end, right, e->radius );
}

static void fx_oriented_quad( void )
{
	const refEntity_t *e = fx_ent;
	vec3_t	left, up;

	// SP cgame sets only axis[0], the normal.
	MakeNormalVectors( e->axis[0], left, up );

	if ( e->rotation == 0 )
	{
		VectorScale( left, e->radius, left );
		VectorScale( up, e->radius, up );
	}
	else
	{
		vec3_t	temp_left;
		const float ang = M_PI * e->rotation / 180.0f, s = sin( ang ), c = cos( ang );

		VectorScale( left, c * e->radius, temp_left );
		VectorMA( temp_left, -s * e->radius, up, temp_left );
		VectorScale( up, c * e->radius, up );
		VectorMA( up, s * e->radius, left, up );
		VectorCopy( temp_left, left );
	}

	fx_quad_stamp( e->origin, left, up );
}

// RT_BEAM as in RB_SurfaceBeam: a 6 sided tube of radius 4 between origin and oldorigin. skinNum picks red, green or blue.
static void fx_beam( void )
{
	const refEntity_t *e = fx_ent;
	vec3_t	dir, ndir, perp, pts[7][2];
	byte	rgba[4] = { 0, 0, 0, 255 };

	VectorSubtract( e->oldorigin, e->origin, dir );
	VectorCopy( dir, ndir );

	if ( VectorNormalize( ndir ) == 0 )
		return;

	PerpendicularVector( perp, ndir );
	VectorScale( perp, 4, perp );

	rgba[e->skinNum == 1 ? 1 : e->skinNum == 2 ? 2 : 0] = 255;

	for ( int i = 0; i <= 6; i++ )
	{
		RotatePointAroundVector( pts[i][0], ndir, perp, 60.0f * i );
		VectorAdd( pts[i][0], e->origin, pts[i][0] );
		VectorAdd( pts[i][0], dir, pts[i][1] );
	}

	for ( int i = 0; i < 6; i++ )
	{
		polyVert_t v[4];

		fx_vert( v + 0, pts[i][0], 0, 0, rgba );
		fx_vert( v + 1, pts[i][1], 0, 1, rgba );
		fx_vert( v + 2, pts[i + 1][0], 1, 0, rgba );
		fx_vert( v + 3, pts[i + 1][1], 1, 1, rgba );
		fx_quad_strip( v );
	}
}

#define FX_CYLINDER_SEGMENTS 40

// A cylinder with the end radii e->radius and e->backlerp. A very small end makes it a cone.
static void fx_cylinder( void )
{
	const refEntity_t *e = fx_ent;
	static vec3_t lower[FX_CYLINDER_SEGMENTS], upper[FX_CYLINDER_SEGMENTS];
	vec3_t	vr, vu, v1, mid, tapered, base;
	float	detail, length;
	int		segments;
	const bool cone = !( e->radius < 0.3f && e->backlerp < 0.3f ) && ( e->radius < 0.3f || e->backlerp < 0.3f );

	VectorAdd( e->origin, e->oldorigin, mid );
	VectorScale( mid, 0.5f, mid );
	VectorSubtract( mid, fx_refdef->vieworg, mid );
	length = VectorNormalize( mid ) * ( fx_refdef->fov_x / 90.0f );

	detail = 1.0f - length / 2048.0f;
	segments = (int)( FX_CYLINDER_SEGMENTS * detail );
	segments = MAX( 8, MIN( FX_CYLINDER_SEGMENTS, segments ) );

	MakeNormalVectors( e->axis[0], vr, vu );
	detail = 1.0f / (float)segments;

	if ( cone )
	{
		if ( e->radius < e->backlerp )
		{
			VectorScale( vu, e->backlerp, vu );
			VectorCopy( e->origin, base );
			VectorCopy( e->oldorigin, tapered );
		}
		else
		{
			VectorScale( vu, e->radius, vu );
			VectorCopy( e->origin, tapered );
			VectorCopy( e->oldorigin, base );
		}

		for ( int i = 0; i < segments; i++ )
		{
			polyVert_t a, b, c;
			vec3_t p0, p1;

			RotatePointAroundVector( p0, e->axis[0], vu, 360.0f / segments * i );
			RotatePointAroundVector( p1, e->axis[0], vu, 360.0f / segments * ( ( i + 1 ) % segments ) );
			VectorAdd( p0, base, p0 );
			VectorAdd( p1, base, p1 );

			fx_vert( &a, p0, detail * i, 1.0f, e->shaderRGBA );
			fx_vert( &b, tapered, detail * i + detail * 0.5f, 0.0f, e->shaderRGBA );
			fx_vert( &c, p1, detail * ( i + 1 ), 1.0f, e->shaderRGBA );
			fx_tri( &a, &b, &c );
		}

		return;
	}

	VectorScale( vu, e->radius, v1 );
	VectorScale( vu, e->backlerp, vu );

	for ( int i = 0; i < segments; i++ )
	{
		RotatePointAroundVector( upper[i], e->axis[0], vu, 360.0f / segments * i );
		VectorAdd( upper[i], e->origin, upper[i] );
		RotatePointAroundVector( lower[i], e->axis[0], v1, 360.0f / segments * i );
		VectorAdd( lower[i], e->oldorigin, lower[i] );
	}

	for ( int i = 0; i < segments; i++ )
	{
		const int next = ( i + 1 ) % segments;
		polyVert_t v[4];

		fx_vert( v + 0, upper[i], detail * i, 1.0f, e->shaderRGBA );
		fx_vert( v + 1, lower[i], detail * i, 0.0f, e->shaderRGBA );
		fx_vert( v + 2, upper[next], detail * ( i + 1 ), 1.0f, e->shaderRGBA );
		fx_vert( v + 3, lower[next], detail * ( i + 1 ), 0.0f, e->shaderRGBA );
		fx_quad_strip( v );
	}
}

static void fx_lathe( void )
{
	const refEntity_t *e = fx_ent;
	vec2_t	pt, oldpt, l_oldpt, pt2, oldpt2, l_oldpt2;
	float	d = 1.0f, pain = 0.0f, mu, step_bezier, step_lathe;
	int		lod = r_lodbias->integer + 1;

	lod = MAX( 1, MIN( 4, lod ) );

	if ( e->endTime && e->endTime > fx_refdef->time )
		d = 1.0f - ( e->endTime - fx_refdef->time ) / 1000.0f;

	if ( e->frame && e->frame + 1000 > fx_refdef->time )
		pain = ( 1.0f - ( fx_refdef->time - e->frame ) / 1000.0f ) * 0.08f;

	VectorSet2( l_oldpt, e->axis[0][0], e->axis[0][1] );

	step_bezier = 0.05f * lod;
	step_lathe = 10.0f * lod;

	for ( mu = 0.0f; mu <= 1.01f * d; mu += step_bezier )
	{
		const float mum1 = 1 - mu, mum13 = mum1 * mum1 * mum1, mu3 = mu * mu * mu;
		const float group1 = 3 * mu * mum1 * mum1, group2 = 3 * mu * mu * mum1;

		for ( int i = 0; i < 2; i++ )
			l_oldpt2[i] = mum13 * e->axis[0][i] + group1 * e->axis[1][i] + group2 * e->axis[2][i] + mu3 * e->oldorigin[i];

		VectorSet2( oldpt, l_oldpt[0], 0 );
		VectorSet2( oldpt2, l_oldpt2[0], 0 );

		for ( int t = (int)step_lathe; t <= 360; t += (int)step_lathe )
		{
			const float s = sin( DEG2RAD( t ) ), c = cos( DEG2RAD( t ) );
			polyVert_t v[4];
			vec3_t p;
			float temp;
			int k;

			VectorSet2( pt, l_oldpt[0], 0 );
			VectorSet2( pt2, l_oldpt2[0], 0 );

			temp = c * pt[0] - s * pt[1];		pt[1] = s * pt[0] + c * pt[1];		pt[0] = temp;
			temp = c * pt2[0] - s * pt2[1];		pt2[1] = s * pt2[0] + c * pt2[1];	pt2[0] = temp;

			VectorSet( p, oldpt[0], oldpt[1], l_oldpt[1] );		VectorAdd( e->origin, p, p );
			k = oldpt[0] * 0.1f + oldpt[1] * 0.1f;
			fx_vert( v + 0, p, ( t - step_lathe ) / 360.0f, mu - step_bezier + cos( k + fx_refdef->floatTime ) * pain, e->shaderRGBA );

			VectorSet( p, oldpt2[0], oldpt2[1], l_oldpt2[1] );	VectorAdd( e->origin, p, p );
			k = oldpt2[0] * 0.1f + oldpt2[1] * 0.1f;
			fx_vert( v + 1, p, ( t - step_lathe ) / 360.0f, mu + cos( k + fx_refdef->floatTime ) * pain, e->shaderRGBA );

			VectorSet( p, pt[0], pt[1], l_oldpt[1] );			VectorAdd( e->origin, p, p );
			k = pt[0] * 0.1f + pt[1] * 0.1f;
			fx_vert( v + 2, p, t / 360.0f, mu - step_bezier + cos( k + fx_refdef->floatTime ) * pain, e->shaderRGBA );

			VectorSet( p, pt2[0], pt2[1], l_oldpt2[1] );		VectorAdd( e->origin, p, p );
			k = pt2[0] * 0.1f + pt2[1] * 0.1f;
			fx_vert( v + 3, p, t / 360.0f, mu + cos( k + fx_refdef->floatTime ) * pain, e->shaderRGBA );

			fx_quad_lathe( v );

			VectorCopy2( pt, oldpt );
			VectorCopy2( pt2, oldpt2 );
		}

		VectorCopy2( l_oldpt2, l_oldpt );
	}
}

static void fx_clouds( void )
{
	static const float disk_strip[4] = { 0.0f, 0.4f, 0.7f, 1.0f }, disk_alpha[4] = { 1.0f, 1.0f, 0.4f, 0.0f }, disk_curve[4] = { 0.0f, 0.0f, 0.008f, 0.02f };
	static const float tube_strip[6] = { 0.0f, 0.05f, 0.1f, 0.5f, 0.7f, 1.0f }, tube_alpha[6] = { 0.0f, 0.45f, 1.0f, 1.0f, 0.45f, 0.0f }, tube_curve[6] = { 0.0f, 0.004f, 0.006f, 0.01f, 0.006f, 0.0f };
	const refEntity_t *e = fx_ent;
	const float *strip = disk_strip, *alpha = disk_alpha, *curve = disk_curve;
	int		count = 4;
	float	backlerp = e->backlerp;
	const float step = 30.0f;

	if ( e->renderfx & RF_GROW )
	{
		count = 6;
		strip = tube_strip;
		alpha = tube_alpha;
		curve = tube_curve;
		backlerp = -backlerp;
	}

	for ( int i = 0; i < count - 1; i++ )
	{
		vec3_t oldpt, oldpt2, pt, pt2;

		VectorSet( oldpt, strip[i] * ( e->radius - e->rotation ) + e->rotation, 0, curve[i] * e->radius * backlerp );
		VectorSet( oldpt2, strip[i + 1] * ( e->radius - e->rotation ) + e->rotation, 0, curve[i + 1] * e->radius * backlerp );

		for ( int t = (int)step; t <= 360; t += (int)step )
		{
			polyVert_t v[4];
			vec3_t p[4];
			byte col[4];

			if ( t < 360 )
			{
				const float s = sin( DEG2RAD( step ) ), c = cos( DEG2RAD( step ) );
				float temp;

				VectorCopy( oldpt, pt );
				VectorCopy( oldpt2, pt2 );
				temp = c * pt[0] - s * pt[1];		pt[1] = s * pt[0] + c * pt[1];		pt[0] = temp;
				temp = c * pt2[0] - s * pt2[1];		pt2[1] = s * pt2[0] + c * pt2[1];	pt2[0] = temp;
			}
			else
			{
				VectorSet( pt, strip[i] * ( e->radius - e->rotation ) + e->rotation, 0, curve[i] * e->radius * backlerp );
				VectorSet( pt2, strip[i + 1] * ( e->radius - e->rotation ) + e->rotation, 0, curve[i + 1] * e->radius * backlerp );
			}

			VectorAdd( e->origin, oldpt, p[0] );
			VectorAdd( e->origin, oldpt2, p[1] );
			VectorAdd( e->origin, pt, p[2] );
			VectorAdd( e->origin, pt2, p[3] );

			for ( int k = 0; k < 4; k++ )
			{
				const float a = alpha[i + ( k & 1 )];

				col[0] = col[1] = col[2] = (byte)( e->shaderRGBA[0] * a );
				col[3] = e->shaderRGBA[3];
				fx_vert( v + k, p[k], p[k][0] * 0.1f, p[k][1] * 0.1f, col );
			}

			fx_quad_lathe( v );

			VectorCopy( pt, oldpt );
			VectorCopy( pt2, oldpt2 );
		}
	}
}

#ifdef USE_VBO_SS
typedef struct
{
	float				dist2;
	const sprite_t		*sprite;
	int					group;
} ss_candidate_t;

typedef struct
{
	rtx_material_t				*mat;
	const SurfaceSpriteBlock	*block;
	uint32_t					flags;
	bool						vertex_lit;	// the instance colour is the baked light of the ground
} ss_group_info_t;

static std::vector<ss_candidate_t> ss_candidates;
static ss_group_info_t ss_groups[SS_MAX_GROUP];

static float ss_smoothstep( float e0, float e1, float x )
{
	const float t = Com_Clamp( 0.0f, 1.0f, ( x - e0 ) / MAX( e1 - e0, 0.0001f ) );
	return t * t * ( 3.0f - 2.0f * t );
}

// One surface sprite as a quad, built as surface_sprite_vert.tmpl does for the rasterizer.
static void fx_surface_sprite( const ss_candidate_t *c, const ss_group_info_t *g )
{
	const sprite_t *s = c->sprite;
	const SurfaceSpriteBlock *b = g->block;
	vec3_t	V, offsets[4], p;
	vec2_t	to_camera;
	polyVert_t v[4];
	// The tracer lights the sprite: the baked light of the ground is not part of its albedo.
	byte	rgba[4] = { 255, 255, 255, 255 };

	if ( !g->vertex_lit )
		Com_Memcpy( rgba, s->color, 3 );

	static const float uv[4][2] = { { 1, 1 }, { 1, 0 }, { 0, 0 }, { 0, 1 } };

	VectorSubtract( fx_refdef->vieworg, s->position, V );

	const float dist = VectorLength( V );
	float width = s->widthHeight[0];
	const float height = s->widthHeight[1];

	width += b->fadeScale * ss_smoothstep( b->fadeStartDistance, b->fadeEndDistance, dist ) * width;

	const float hw = width * 0.5f;

	if ( g->flags & SSDEF_FACE_UP )
	{
		VectorSet( offsets[0],  hw, -hw, 0 );
		VectorSet( offsets[1],  hw,  hw, 0 );
		VectorSet( offsets[2], -hw,  hw, 0 );
		VectorSet( offsets[3], -hw, -hw, 0 );
	}
	else
	{
		VectorSet( offsets[0],  hw, 0, 0 );
		VectorSet( offsets[1],  hw, 0, height );
		VectorSet( offsets[2], -hw, 0, height );
		VectorSet( offsets[3], -hw, 0, 0 );
	}

	const float inv = 1.0f / MAX( sqrtf( V[0] * V[0] + V[1] * V[1] ), 0.0001f );
	to_camera[0] = V[0] * inv;
	to_camera[1] = V[1] * inv;

	const float angle = ( s->position[0] + s->position[1] ) * 0.02f + fx_refdef->floatTime * 1000.0f * 0.0015f;
	const float windsway = height * b->windIdle * 0.075f;

	for ( int i = 0; i < 4; i++ )
	{
		const bool lower = ( offsets[i][2] == 0.0f );
		float x = offsets[i][0];
		float ox, oy;

		if ( g->flags & SSDEF_FACE_CAMERA )
		{
			ox = x * to_camera[1];
			oy = -x * to_camera[0];
		}
		else if ( g->flags & SSDEF_FLATTENED )
		{
			ox = x * s->normal[0];
			oy = x * s->normal[1];
		}
		else if ( !( g->flags & SSDEF_FACE_UP ) )
		{
			ox = x * ( s->normal[0] + 3.0f * to_camera[1] ) * 0.25f;
			oy = x * ( s->normal[1] - 3.0f * to_camera[0] ) * 0.25f;
		}
		else
		{
			ox = offsets[i][0];
			oy = offsets[i][1];
		}

		if ( !( g->flags & SSDEF_FACE_UP ) && !lower )
		{
			ox += s->skew[0] + cosf( angle ) * windsway;
			oy += s->skew[1] + sinf( angle ) * windsway;
		}

		VectorSet( p, s->position[0] + ox, s->position[1] + oy, s->position[2] + offsets[i][2] );
		fx_vert( v + i, p, uv[i][0], uv[i][1], rgba );
	}

	fx_tri( v + 0, v + 1, v + 2 );
	fx_tri( v + 2, v + 3, v + 0 );
}

// The surface sprites of the visible world surfaces (grass, reeds). The rasterizer draws them from
// instance buffers; the tracer takes the nearest ones as quads of the sprite path.
static void fx_surface_sprites( const trRefdef_t *refdef )
{
	const auto &ssData = backEndDataBE->ss;

	if ( !r_surfaceSprites->integer || !ssData.groups_count )
		return;

	ss_candidates.clear();

	for ( int gi = 0; gi < (int)ssData.groups_count && gi < SS_MAX_GROUP; gi++ )
	{
		const vk_ss_group_t *group = &ssData.groups[gi];
		ss_group_info_t *info = ss_groups + gi;
		Vk_Pipeline_Def def;

		info->mat = NULL;

		if ( !group->def.shader || !group->num_commands || SS_UNPACK_ENT( group->def.surf_bits ) != REFENTITYNUM_WORLD )
			continue;

		shader_t *shader = group->def.shader;
		rtx_material_t *mat = vk_rtx_shader_to_material( shader );

		if ( !mat || !mat->active || !mat->uploaded[vk.current_frame_index] || !mat->stage[0].bundle[0].image )
			continue;

		vk_get_pipeline_def( shader->stages[0]->vk_pipeline[0], &def );

		if ( def.surface_sprite_flags & SSDEF_FX_SPRITE )
			continue;

		const uint32_t vbo_index = SS_UNPACK_VBO( group->def.surf_bits );
		int count = 0;
		const sprite_t *instances = ( vbo_index < (uint32_t)tr.numVBOs ) ? vk_surface_sprites_cpu_instances( tr.vbos[vbo_index], &count ) : NULL;
		const vk_storage_buffer_t *ssbo = &vk.surface_sprites_ssbo[SS_UNPACK_SSBO_INDEX( group->def.ssbo_bits )];

		if ( !instances || !ssbo->buffer_ptr )
			continue;

		info->mat = mat;
		info->block = (const SurfaceSpriteBlock *)( ssbo->buffer_ptr + SS_UNPACK_SSBO_OFFSET( group->def.ssbo_bits ) );
		info->flags = def.surface_sprite_flags;
		info->vertex_lit = shader->stages[0]->bundle[0].rgbGen == CGEN_VERTEX || shader->stages[0]->bundle[0].rgbGen == CGEN_EXACT_VERTEX;

		const float end2 = info->block->fadeEndDistance * info->block->fadeEndDistance;

		for ( int j = 0; j < group->num_commands; j++ )
		{
			const vk_ss_group_cmd_t *cmd = group->cmd + j;

			for ( int k = cmd->firstInstance; k < cmd->firstInstance + cmd->numInstances && k < count; k++ )
			{
				vec3_t d;

				VectorSubtract( instances[k].position, refdef->vieworg, d );

				const float d2 = VectorLengthSquared( d );

				if ( d2 < end2 )
					ss_candidates.push_back( { d2, instances + k, gi } );
			}
		}
	}

	const int room = ( TR_SPRITE_MAX_NUM - fx_tri_num ) / 2;

	if ( (int)ss_candidates.size() > room )
	{
		std::nth_element( ss_candidates.begin(), ss_candidates.begin() + MAX( room, 0 ), ss_candidates.end(),
			[]( const ss_candidate_t &a, const ss_candidate_t &b ) { return a.dist2 < b.dist2; } );
		ss_candidates.resize( MAX( room, 0 ) );
	}

	for ( const ss_candidate_t &c : ss_candidates )
	{
		fx_mat = ss_groups[c.group].mat;
		fx_lit = true;
		fx_surface_sprite( &c, ss_groups + c.group );
	}

	fx_lit = false;
}
#endif

// The BSP flare surfaces, as in RB_RenderFlare: a quad 3 units off the surface, the colour from the view angle.
// The tracer hides the quad behind geometry, so the rasterizer depth test is not needed.
static void fx_flares( const trRefdef_t *refdef )
{
	static const byte	white[4] = { 255, 255, 255, 255 };
	static refEntity_t	flare_ent;

	if ( !r_flares->integer || !refdef->drawSurfs )
		return;

	for ( int i = 0; i < refdef->numDrawSurfs; i++ )
	{
		const surfaceType_t *type = refdef->drawSurfs[i].surface;

		if ( *type != SF_FLARE )
			continue;

		const srfFlare_t *flare = (const srfFlare_t *)type;
		rtx_material_t *mat = vk_rtx_shader_to_material( flare->shader );
		vec3_t	origin, dir, left, up;

		if ( !mat || !mat->active || !mat->uploaded[vk.current_frame_index] || !mat->stage[0].bundle[0].image )
			continue;

		VectorMA( flare->origin, 3, flare->normal, origin );
		VectorSubtract( origin, refdef->vieworg, dir );

		const float dist = VectorNormalize( dir );
		const float d = fabsf( DotProduct( dir, flare->normal ) );
		float radius = flare->shader->portalRange ? flare->shader->portalRange : 30;

		if ( dist < 512.0f )
			radius = radius * dist / 512.0f;
		if ( radius < 5.0f )
			radius = 5.0f;

		Com_Memcpy( flare_ent.shaderRGBA, white, 4 );
		flare_ent.shaderRGBA[0] = flare_ent.shaderRGBA[1] = flare_ent.shaderRGBA[2] = (byte)( d * 255.0f );

		VectorScale( backEnd.viewParms.ori.axis[1], radius, left );
		VectorScale( backEnd.viewParms.ori.axis[2], radius, up );

		fx_ent = &flare_ent;
		fx_mat = mat;
		fx_quad_stamp( origin, left, up );
	}
}

// Fills fx_tris from the refdef. update_transparency counts them, write_sprite_geometry writes them.
static void tessellate_fx_entities( const trRefdef_t *refdef )
{
	fx_tri_num = 0;
	fx_refdef = refdef;

#ifdef USE_VBO_SS
	fx_surface_sprites( refdef );
#endif

	fx_flares( refdef );

	if ( !r_drawentities->integer )
		return;

	for ( int i = 0; i < refdef->num_entities; i++ )
	{
		const trRefEntity_t *entity = refdef->entities + i;

		switch ( entity->e.reType )
		{
			case RT_ELECTRICITY:
			case RT_ORIENTED_QUAD:
			case RT_CYLINDER:
			case RT_LATHE:
			case RT_CLOUDS:
			case RT_BEAM:
				break;
			default:
				continue;
		}

		shader_t *shader = ( entity->e.reType == RT_BEAM ) ? tr.beamShader : R_GetShaderByHandle( entity->e.customShader );
		rtx_material_t *mat = vk_rtx_shader_to_material( shader );

		if ( !mat || !mat->active || !mat->uploaded[vk.current_frame_index] || !mat->stage[0].bundle[0].image )
			continue;

		fx_ent = &entity->e;
		fx_mat = mat;
		fx_seed = entity->e.frame;

		switch ( entity->e.reType )
		{
			case RT_ELECTRICITY:	fx_electricity();		break;
			case RT_ORIENTED_QUAD:	fx_oriented_quad();		break;
			case RT_CYLINDER:		fx_cylinder();			break;
			case RT_LATHE:			fx_lathe();				break;
			case RT_CLOUDS:			fx_clouds();			break;
			case RT_BEAM:			fx_beam();				break;
			default:				break;
		}
	}
}

void update_transparency(VkCommandBuffer command_buffer, const trRefdef_t *refdef, const float* view_matrix )
{
	transparency.host_frame_index = (transparency.host_frame_index + 1) % transparency.host_buffered_frame_num;
	//particle_num = MIN(particle_num, TR_PARTICLE_MAX_NUM);

	const int particle_num = 0;
	uint32_t beam_num = 0;
	uint32_t sprite_num = 0;
	float radius = 0;

	for ( int i = 0; i < refdef->num_entities; i++ )
	{
		if ( !r_drawentities->integer )
			break;

		trRefEntity_t *entity = refdef->entities + i;
		
		if (entity->e.reType == RT_BEAM)
		{
			//++beam_num;
		}
		else if (entity->e.reType == RT_LINE || entity->e.reType == RT_SPRITE)
		{
			++sprite_num;
		}
		else if (entity->e.reType == RT_SABER_GLOW)
		{
			radius = entity->e.radius;

			for (float j = entity->e.saberLength; j > 0; j -= radius * 0.65f)
			{
				++sprite_num;
				radius += 0.017f;
			}

			++sprite_num;
		}
	}

	tessellate_fx_entities( refdef );
	sprite_num += fx_tri_num;

	// A scene poly (a saber trail, an effect) is a fan of triangles, one sprite slot each.
	for ( int i = 0; i < refdef->numPolys && r_drawentities->integer; i++ )
	{
		if ( refdef->polys[i].numVerts >= 3 )
			sprite_num += refdef->polys[i].numVerts - 2;
	}

	beam_num = MIN(beam_num, TR_BEAM_MAX_NUM);
	sprite_num = MIN(sprite_num, TR_SPRITE_MAX_NUM);

	transparency.beam_num = beam_num;
	transparency.particle_num = particle_num;
	transparency.sprite_num = sprite_num;

	const size_t particle_vertices_size = particle_num * (4 * TR_POSITION_SIZE);
	const size_t sprite_vertices_size = sprite_num * (4 * TR_POSITION_SIZE);

	transparency.vertex_position_host_offset	= 0;
	transparency.particle_color_host_offset		= transparency.vertex_position_host_offset + particle_vertices_size + sprite_vertices_size;
	transparency.sprite_info_host_offset		= transparency.particle_color_host_offset + particle_num * TR_COLOR_SIZE;
	transparency.beam_aabb_host_offset			= transparency.sprite_info_host_offset + sprite_num * TR_SPRITE_INFO_SIZE;
	transparency.beam_color_host_offset			= transparency.beam_aabb_host_offset + beam_num * TR_BEAM_AABB_SIZE;
	transparency.beam_intersect_host_offset		= transparency.beam_color_host_offset + beam_num * TR_COLOR_SIZE;
	transparency.current_upload_size			= transparency.beam_intersect_host_offset + beam_num * TR_BEAM_INTERSECT_SIZE;

	if (particle_num > 0 || beam_num > 0 || sprite_num > 0)
	{
		//write_particle_geometry(view_matrix, particles, particle_num);
		//write_beam_geometry(entities, entity_num);
		write_sprite_geometry(view_matrix, refdef);
		upload_geometry(command_buffer);
	}
}

void vkpt_get_transparency_buffers(
	vkpt_transparency_t ttype,
	vkbuffer_t** vertex_buffer,
	uint64_t* vertex_offset,
	vkbuffer_t** index_buffer,
	uint64_t* index_offset,
	uint32_t* num_vertices,
	uint32_t* num_indices)
{
	*vertex_buffer = &transparency.vertex_buffer;
	*index_buffer = &transparency.index_buffer;
	*index_offset = 0;

	switch (ttype)
	{
	case VKPT_TRANSPARENCY_PARTICLES:
		*vertex_offset = 0;
		*num_vertices = transparency.particle_num * 4;
		*num_indices = transparency.particle_num * 6;
		return;

	case VKPT_TRANSPARENCY_SPRITES:
		*vertex_offset = transparency.sprite_vertex_device_offset;
		*num_vertices = transparency.sprite_num * 4;
		*num_indices = transparency.sprite_num * 6;
		return;

	default:
		*vertex_offset = transparency.sprite_vertex_device_offset;
		*num_vertices = 0;
		*num_indices = 0;
		return;
	}
}

void vkpt_get_beam_aabb_buffer(
	vkbuffer_t** aabb_buffer,
	uint64_t* aabb_offset,
	uint32_t* num_aabbs)
{
	*aabb_buffer = &transparency.beam_aabb_buffer;
	*aabb_offset = 0;
	*num_aabbs = transparency.beam_num;
}

VkBufferView get_transparency_particle_color_buffer_view()
{
	return transparency.particle_color_buffer_view;
}

VkBufferView get_transparency_beam_color_buffer_view()
{
	return transparency.beam_color_buffer_view;
}

VkBufferView get_transparency_sprite_info_buffer_view()
{
	return transparency.sprite_info_buffer_view;
}

VkBufferView get_transparency_beam_intersect_buffer_view()
{
	return transparency.beam_intersect_buffer_view;
}

void get_transparency_counts(int* particle_num, int* beam_num, int* sprite_num)
{
	*particle_num = transparency.particle_num;
	*beam_num = transparency.beam_num;
	*sprite_num = transparency.sprite_num;
}

bool vkpt_build_cylinder_light(light_poly_t* light_list, int* num_lights, int max_lights, world_t *worldData, vec3_t begin, vec3_t end, vec3_t color, float radius, entity_hash_t hash, int *light_entity_ids)
{
	vec3_t dir, norm_dir;
	VectorSubtract(end, begin, dir);
	VectorCopy(dir, norm_dir);
	VectorNormalize(norm_dir);

	vec3_t up = { 0.f, 0.f, 1.f };
	vec3_t left = { 1.f, 0.f, 0.f };
	if (fabsf(norm_dir[2]) < 0.9f)
	{
		CrossProduct(up, norm_dir, left);
		VectorNormalize(left);
		CrossProduct(norm_dir, left, up);
		VectorNormalize(up);
	}
	else
	{
		CrossProduct(norm_dir, left, up);
		VectorNormalize(up);
		CrossProduct(up, norm_dir, left);
		VectorNormalize(left);
	}


	vec3_t vertices[6] = {
		{ 0.f, 1.f, 0.f },
		{ 0.866f, -0.5f, 0.f },
		{ -0.866f, -0.5f, 0.f },
		{ 0.f, -1.f, 1.f },
		{ -0.866f, 0.5f, 1.f },
		{ 0.866f, 0.5f, 1.f },
	};

	const int indices[18] = {
		0, 4, 2,
		2, 4, 3,
		2, 3, 1,
		1, 3, 5,
		1, 5, 0,
		0, 5, 4
	};

	for (int vert = 0; vert < 6; vert++)
	{
		vec3_t transformed;
		VectorCopy(begin, transformed);
		VectorMA(transformed, vertices[vert][0] * radius, up, transformed);
		VectorMA(transformed, vertices[vert][1] * radius, left, transformed);
		VectorMA(transformed, vertices[vert][2], dir, transformed);
		VectorCopy(transformed, vertices[vert]);
	}

	for (int tri = 0; tri < 6; tri++)
	{
		if (*num_lights >= max_lights)
			return false;

		int i0 = indices[tri * 3 + 0];
		int i1 = indices[tri * 3 + 1];
		int i2 = indices[tri * 3 + 2];

		light_poly_t* light = light_list + *num_lights;

		VectorCopy(vertices[i0], light->positions + 0);
		VectorCopy(vertices[i1], light->positions + 3);
		VectorCopy(vertices[i2], light->positions + 6);
		get_triangle_off_center(light->positions, light->off_center, NULL, 1.f);

		light->cluster = BSP_PointLeaf(worldData->nodes, light->off_center)->cluster;
		light->material = NULL;
		light->style = 0;
		light->type = LIGHT_POLYGON;

		VectorCopy(color, light->color);

		if (light->cluster >= 0)
		{
			hash.mesh = tri;
			light_entity_ids[(*num_lights)] = *(uint32_t*)&hash;
			(*num_lights)++;
		}
	}

	return true;
}


static void do_sprite( vec3_t *vertex_positions, vec3_t origin, float radius, float rotation )
{
	float	s, c;
	float	ang;
	vec3_t up, down, left, right;
		
	ang = M_PI * rotation / 180.0f;
	s = sin( ang );
	c = cos( ang );

	VectorScale( backEnd.viewParms.ori.axis[1], c * radius, left );
	VectorMA( left, -s * radius, backEnd.viewParms.ori.axis[2], left );

	VectorScale( backEnd.viewParms.ori.axis[2], c * radius, up );
	VectorMA( up, s * radius, backEnd.viewParms.ori.axis[1], up );

	VectorScale( left, -1.0f, right );
	VectorScale( up, -1.0f, down );

	VectorAdd3( origin, up,   left,  vertex_positions[0] );
	VectorAdd3( origin, down, left,  vertex_positions[1] );
	VectorAdd3( origin, down, right, vertex_positions[2] );
	VectorAdd3( origin, up,   right, vertex_positions[3] );
}

static void do_line( vec3_t *vertex_positions, const vec3_t start, const vec3_t end, const vec3_t up, float spanWidth )
{
	const float spanWidth2 = -spanWidth;

    VectorMA( start, spanWidth2, up, vertex_positions[0] );
    VectorMA( start, spanWidth,  up, vertex_positions[1] );
    VectorMA( end,   spanWidth,  up, vertex_positions[2] );
    VectorMA( end,   spanWidth2, up, vertex_positions[3] );
}

// The shaders of the saber and the bolts: the prefixes of pt_weapon_fx_shaders. Their sprites skip the tone mapper.
static float sprite_view_height, sprite_view_tan;
static vec3_t sprite_view_origin;

static qboolean vk_rtx_is_weapon_fx( const shader_t *shader )
{
	cvar_t *prefixes = pt_weapon_fx_shaders;

	if ( !shader )
		return qfalse;

	for ( const char *p = prefixes->string; *p; )
	{
		while ( *p == ' ' )
			p++;

		const char *end = p;

		while ( *end && *end != ' ' )
			end++;

		if ( end > p && !Q_stricmpn( shader->name, p, (int)( end - p ) ) )
			return qtrue;

		p = end;
	}

	return qfalse;
}

// Bit 0: a weapon effect. Bits 8-15: the mip level of its texture, in sixteenths, from the size of the sprite on
// the screen (the rasterizer filters the glow texture; the tracer reads one level).
static uint32_t vk_rtx_weapon_fx_word( const shader_t *shader, const refEntity_t *e, const vec3_t center )
{
	if ( !vk_rtx_is_weapon_fx( shader ) )
		return 0u;

	float lod = 0.0f;
	const image_t *image = shader->stages[0] ? shader->stages[0]->bundle[0].image[0] : NULL;

	if ( image && e->radius > 0.0f && sprite_view_height > 0.0f )
	{
		vec3_t d;
		VectorSubtract( center, sprite_view_origin, d );

		const float dist = MAX( VectorLength( d ), 1.0f );
		const float pixels = MAX( 2.0f * e->radius * sprite_view_height / ( 2.0f * dist * sprite_view_tan ), 0.001f );

		lod = log2f( MAX( (float)image->width / pixels, 1.0f ) );
	}

	return 1u | ( (uint32_t)( MIN( lod, 15.0f ) * 16.0f ) << 8 );
}

static inline void write_sprite_info( uint32_t *sprite_info, const rtx_material_t *mat, const refEntity_t *e, const shader_t *shader, const vec3_t center )
{
	Com_Memset( sprite_info, 0, TR_SPRITE_INFO_SIZE );
    sprite_info[0] = mat->flags;
	sprite_info[3] = vk_rtx_weapon_fx_word( shader, e, center );
    sprite_info[1] =
        ((uint32_t)e->shaderRGBA[0]      ) |
        ((uint32_t)e->shaderRGBA[1] <<  8) |
        ((uint32_t)e->shaderRGBA[2] << 16) |
        ((uint32_t)e->shaderRGBA[3] << 24);
}

// One triangle of a scene poly: kind 1, then the UVs and the vertex colours of its corners.
static inline void write_poly_info( uint32_t *sprite_info, const rtx_material_t *mat, const polyVert_t *v0, const polyVert_t *v1, const polyVert_t *v2, bool lit = false )
{
	const polyVert_t *v[3] = { v0, v1, v2 };

	Com_Memset( sprite_info, 0, TR_SPRITE_INFO_SIZE );
	sprite_info[0] = mat->flags;
	sprite_info[1] = 0xffffffffu;
	sprite_info[2] = 1u;
	sprite_info[3] = lit ? 2u : 0u;

	for ( int k = 0; k < 3; k++ )
	{
		sprite_info[4 + k] = floatToHalf( v[k]->st[0] ) | ( (uint32_t)floatToHalf( v[k]->st[1] ) << 16 );
		sprite_info[8 + k] = (uint32_t)v[k]->modulate[0] | ( (uint32_t)v[k]->modulate[1] << 8 )
			| ( (uint32_t)v[k]->modulate[2] << 16 ) | ( (uint32_t)v[k]->modulate[3] << 24 );
	}
}

// borrowed from CG_RGBForSaberColor in cg_palyer.c
static void vk_rtx_get_saber_lights_color( vec3_t rgb, refEntity_t *e )
{
	// e->customSkin = saber color type sett in cg_player.c
	switch ( (saber_colors_t)e->customSkin )
	{
		case SABER_RED:
			VectorSet( rgb, 1.0f, 0.2f, 0.2f );
			break;
		case SABER_ORANGE:
			VectorSet( rgb, 1.0f, 0.5f, 0.1f );
			break;
		case SABER_YELLOW:
			VectorSet( rgb, 1.0f, 1.0f, 0.2f );
			break;
		case SABER_GREEN:
			VectorSet( rgb, 0.2f, 1.0f, 0.2f );
			break;
		case SABER_BLUE:
			VectorSet( rgb, 0.2f, 0.4f, 1.0f );
			break;
		case SABER_PURPLE:
			VectorSet( rgb, 0.9f, 0.2f, 1.0f );
			break;
		case SABER_BLACK:
			VectorSet( rgb, 1.0f, 1.0f, 1.0f );
			break;
		case SABER_RGB:
			VectorSet( rgb, 
				e->shaderRGBA[0] / 255.0f,
				e->shaderRGBA[1] / 255.0f,
				e->shaderRGBA[2] / 255.0f
			);
			break;
		// SABER_FLAME1/ELEC1/FLAME2/ELEC2 are MP-only styles; SP's saber_colors_t has
		// SABER_UNSTABLE_RED, SABER_BLACK and SABER_DARKSABER in their place. Their case
		// produced this same default colour upstream, so dropping them changes nothing.
		default:
			VectorSet( rgb, 0.2f, 0.4f, 1.0f );
			break;
	}
}


void vk_rtx_build_saber_lights( light_poly_t *light_list, int *num_lights, 
	int max_lights, world_t *worldData, const trRefdef_t *refdef, float adapted_luminance, int *light_entity_ids )
{
	uint32_t i;

	int num_sabers = 0;

	static trRefEntity_t *sabers[MAX_SABER_LIGHTS];

	for ( i = 0; i < refdef->num_entities; i++ )
	{
		if ( num_sabers == MAX_SABER_LIGHTS )
			break;

		if (refdef->entities[i].e.reType == RT_SABER_GLOW)
			sabers[num_sabers++] = refdef->entities + i;
	}

	if ( num_sabers == 0 )
		return;

	// copy_light multiplies a light without material by 10. The scale below replaces that factor.
	const float saber_scale = MAX( 0.f, pt_light_scale_saber->value ) / 10.f;

	for ( i = 0; i < num_sabers; i++ )
	{
		trRefEntity_t *saber = sabers[i];
        refEntity_t *e = &saber->e;

		// ReSTIR compares the whole 32 bits of this as one word to match a light to its
		// previous-frame self. bsp is never assigned on this path and mesh is filled in
		// by the callee, so without zeroing it first the match rides on whatever the
		// stack held - a saber's reservoirs then reconnect, or don't, at random from one
		// frame to the next, which shows up the moment the blade ignites.
		entity_hash_t hash;
		Com_Memset( &hash, 0, sizeof(hash) );

		hash.entity = (sabers[i] - refdef->entities) + 1; //entity ID
		hash.model = RT_SABER_GLOW;

        vec3_t begin, end, color;
        VectorCopy(e->origin, begin);
        VectorMA(e->origin, e->saberLength, e->axis[0], end);

		vk_rtx_get_saber_lights_color( color, e );
		VectorScale( color, saber_scale, color );

        vkpt_build_cylinder_light( light_list, num_lights, max_lights, worldData, begin, end, color, e->radius, hash, light_entity_ids );
	}
}

static void write_sprite_geometry(const float* view_matrix, const trRefdef_t *refdef) 
{
	if (transparency.sprite_num == 0)
		return;

	const vec3_t view_x = { view_matrix[0], view_matrix[4], view_matrix[8] };
	const vec3_t view_y = { view_matrix[1], view_matrix[5], view_matrix[9] };
	const vec3_t world_y = { 0.f, 0.f, 1.f };

	// TODO: remove vkpt_refdef.fd, it's better to calculate it from the view matrix
	const vec3_t view_origin = { refdef->vieworg[0], refdef->vieworg[1], refdef->vieworg[2] };

	sprite_view_height = (float)refdef->height;
	sprite_view_tan = tanf( DEG2RAD( refdef->fov_y ) * 0.5f );
	VectorCopy( refdef->vieworg, sprite_view_origin );

	const size_t particle_vertex_data_size = transparency.particle_num * 4 * TR_POSITION_SIZE;
	const size_t sprite_vertex_offset = transparency.vertex_position_host_offset + particle_vertex_data_size;

	// TODO: use better alignment?
	vec3_t* vertex_positions = (vec3_t*)(transparency.host_buffer_shadow + sprite_vertex_offset);
	uint32_t* sprite_info = (uint32_t*)(transparency.host_buffer_shadow + transparency.sprite_info_host_offset);

	int sprite_count = 0;
	const int budget = (int)transparency.sprite_num;
	shader_t *shader;
	rtx_material_t *mat;
	uint32_t i;
	float j, radius;

	for ( i = 0; i < refdef->num_entities; i++ )
	{
		trRefEntity_t *entity = refdef->entities + i;

		if (entity->e.reType != RT_LINE && entity->e.reType != RT_SPRITE && entity->e.reType != RT_SABER_GLOW )
			continue;

		shader = R_GetShaderByHandle( entity->e.customShader );
		mat = vk_rtx_shader_to_material( shader );

		if ( !mat || !mat->active || !mat->uploaded[vk.current_frame_index] )
			continue;

		if ( !mat->stage[0].bundle[0].image )
			continue;

		if (entity->e.reType == RT_LINE)
		{
			vec3_t start, end;
			vec3_t v1, v2;
			vec3_t right;

			VectorCopy(entity->e.origin, start);
			VectorCopy(entity->e.oldorigin, end);

			VectorSubtract(start, refdef->vieworg, v1);
			VectorSubtract(end, refdef->vieworg, v2);

			CrossProduct(v1, v2, right);

			if (VectorNormalize(right) <= 0.0001f)
				continue;

			write_sprite_info(sprite_info, mat, &entity->e, shader, entity->e.origin);
			do_line( vertex_positions, start, end, right, entity->e.radius );

			vertex_positions += 4;
			sprite_info += TR_SPRITE_INFO_SIZE / sizeof(uint32_t);
		}
		else if (entity->e.reType == RT_SPRITE )
		{
			write_sprite_info(sprite_info, mat, &entity->e, shader, entity->e.origin);
			do_sprite( vertex_positions, entity->e.origin, entity->e.radius, entity->e.rotation );

			vertex_positions += 4;
			sprite_info += TR_SPRITE_INFO_SIZE / sizeof(uint32_t);
		}
		else if (entity->e.reType == RT_SABER_GLOW)
		{
			vec3_t		end;
			refEntity_t *e;

			e = &entity->e;
			radius = e->radius;

			for ( j = e->saberLength; j > 0; j -= radius * 0.65f)
			{
				VectorMA( e->origin, j, e->axis[0], end );

				write_sprite_info(sprite_info, mat, &entity->e, shader, end);
				do_sprite( vertex_positions, end, e->radius, 0.0f );

				vertex_positions += 4;
				sprite_info += TR_SPRITE_INFO_SIZE / sizeof(uint32_t);

				radius += 0.017f;

				if (++sprite_count >= budget)
					goto done;
			}

			write_sprite_info(sprite_info, mat, &entity->e, shader, e->origin);
			do_sprite( vertex_positions, e->origin, 5.5f + Q_flrand(0.0f, 1.0f) * 0.25f, 0.0f );

			vertex_positions += 4;
			sprite_info += TR_SPRITE_INFO_SIZE / sizeof(uint32_t);
		}

		if (++sprite_count >= budget)
			goto done;
	}

	// The scene polys, as fans. The second triangle of the slot is degenerate: v3 = v0.
	for ( i = 0; i < (uint32_t)refdef->numPolys; i++ )
	{
		const srfPoly_t *poly = refdef->polys + i;

		if ( poly->numVerts < 3 )
			continue;

		shader = R_GetShaderByHandle( poly->hShader );
		mat = vk_rtx_shader_to_material( shader );

		if ( !mat || !mat->active || !mat->stage[0].bundle[0].image )
			continue;

		for ( int k = 1; k + 1 < poly->numVerts; k++ )
		{
			if ( sprite_count >= budget )
				goto done;

			write_poly_info( sprite_info, mat, &poly->verts[0], &poly->verts[k], &poly->verts[k + 1] );
			VectorCopy( poly->verts[0].xyz, vertex_positions[0] );
			VectorCopy( poly->verts[k].xyz, vertex_positions[1] );
			VectorCopy( poly->verts[k + 1].xyz, vertex_positions[2] );
			VectorCopy( poly->verts[0].xyz, vertex_positions[3] );

			vertex_positions += 4;
			sprite_info += TR_SPRITE_INFO_UINTS;
			sprite_count++;
		}
	}

	// The tessellated entities, one triangle per slot. The second triangle of the slot is degenerate: v3 = v0.
	for ( int k = 0; k < fx_tri_num; k++ )
	{
		const fx_tri_t *t = fx_tris + k;

		if ( sprite_count >= budget )
			goto done;

		write_poly_info( sprite_info, t->mat, t->v + 0, t->v + 1, t->v + 2, t->lit );
		VectorCopy( t->v[0].xyz, vertex_positions[0] );
		VectorCopy( t->v[1].xyz, vertex_positions[1] );
		VectorCopy( t->v[2].xyz, vertex_positions[2] );
		VectorCopy( t->v[0].xyz, vertex_positions[3] );

		vertex_positions += 4;
		sprite_info += TR_SPRITE_INFO_UINTS;
		sprite_count++;
	}

done:
	// The slots counted but not written (a material not ready yet) keep no stale sprite.
	for ( ; sprite_count < budget; sprite_count++ )
	{
		Com_Memset( vertex_positions, 0, 4 * sizeof( vec3_t ) );
		Com_Memset( sprite_info, 0, TR_SPRITE_INFO_SIZE );
		vertex_positions += 4;
		sprite_info += TR_SPRITE_INFO_UINTS;
	}
}

static void upload_geometry(VkCommandBuffer command_buffer)
{
	transparency.sprite_vertex_device_offset = transparency.particle_num * 4 * TR_POSITION_SIZE;

    const size_t host_buffer_offset = transparency.host_frame_index * transparency.host_frame_size;

	assert(transparency.current_upload_size > 0);
	memcpy(transparency.mapped_host_buffer + host_buffer_offset, transparency.host_buffer_shadow, transparency.current_upload_size);
	transparency.current_upload_size = 0;

	VkBufferCopy vertices;
	Com_Memset( &vertices, 0, sizeof(VkBufferCopy) );
	vertices.srcOffset = host_buffer_offset + transparency.vertex_position_host_offset;
	vertices.dstOffset = 0;
	vertices.size = (transparency.particle_num + transparency.sprite_num) * 4 * TR_POSITION_SIZE;


	VkBufferCopy beam_aabbs;
	Com_Memset( &beam_aabbs, 0, sizeof(VkBufferCopy) );
	beam_aabbs.srcOffset = host_buffer_offset + transparency.beam_aabb_host_offset;
	beam_aabbs.dstOffset = 0;
	beam_aabbs.size = transparency.beam_num * TR_BEAM_AABB_SIZE;


	VkBufferCopy particle_colors;
	Com_Memset( &particle_colors, 0, sizeof(VkBufferCopy) );
	particle_colors.srcOffset = host_buffer_offset + transparency.particle_color_host_offset;
	particle_colors.dstOffset = 0;
	particle_colors.size = transparency.particle_num * TR_COLOR_SIZE;


	VkBufferCopy beam_colors;
	Com_Memset( &beam_colors, 0, sizeof(VkBufferCopy) );
	beam_colors.srcOffset = host_buffer_offset + transparency.beam_color_host_offset;
	beam_colors.dstOffset = 0;
	beam_colors.size = transparency.beam_num * TR_COLOR_SIZE;


	VkBufferCopy sprite_infos;
	Com_Memset( &sprite_infos, 0, sizeof(VkBufferCopy) );
	sprite_infos.srcOffset = host_buffer_offset + transparency.sprite_info_host_offset;
	sprite_infos.dstOffset = 0;
	sprite_infos.size = transparency.sprite_num * TR_SPRITE_INFO_SIZE;


	VkBufferCopy beam_intersect;
	Com_Memset( &beam_intersect, 0, sizeof(VkBufferCopy) );
	beam_intersect.srcOffset = host_buffer_offset + transparency.beam_intersect_host_offset;
	beam_intersect.dstOffset = 0;
	beam_intersect.size = transparency.beam_num * TR_BEAM_INTERSECT_SIZE;

	if (vertices.size)
		qvkCmdCopyBuffer(command_buffer, transparency.host_buffer, transparency.vertex_buffer.buffer,
			1, &vertices);
	
	if (beam_aabbs.size)
		qvkCmdCopyBuffer(command_buffer, transparency.host_buffer, transparency.beam_aabb_buffer.buffer,
			1, &beam_aabbs);

	if (particle_colors.size)
		qvkCmdCopyBuffer(command_buffer, transparency.host_buffer, transparency.particle_color_buffer.buffer,
			1, &particle_colors);

	if (beam_colors.size)
		qvkCmdCopyBuffer(command_buffer, transparency.host_buffer, transparency.beam_color_buffer.buffer,
			1, &beam_colors);

	if (sprite_infos.size)
		qvkCmdCopyBuffer(command_buffer, transparency.host_buffer, transparency.sprite_info_buffer.buffer,
			1, &sprite_infos);

	if (beam_intersect.size)
		qvkCmdCopyBuffer(command_buffer, transparency.host_buffer, transparency.beam_intersect_buffer.buffer,
			1, &beam_intersect);

	for (size_t i = 0; i < ARRAY_LEN(transparency.transfer_barriers); i++)
	{
		transparency.transfer_barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		transparency.transfer_barriers[i].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		transparency.transfer_barriers[i].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		transparency.transfer_barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transparency.transfer_barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	transparency.transfer_barriers[0].buffer = transparency.vertex_buffer.buffer;
	transparency.transfer_barriers[0].size = vertices.size;
	transparency.transfer_barriers[1].buffer = transparency.particle_color_buffer.buffer;
	transparency.transfer_barriers[1].size = particle_colors.size;
	transparency.transfer_barriers[2].buffer = transparency.beam_color_buffer.buffer;
	transparency.transfer_barriers[2].size = beam_colors.size;
	transparency.transfer_barriers[3].buffer = transparency.sprite_info_buffer.buffer;
	transparency.transfer_barriers[3].size = sprite_infos.size;
	transparency.transfer_barriers[4].buffer = transparency.beam_aabb_buffer.buffer;
	transparency.transfer_barriers[4].size = beam_aabbs.size;
	transparency.transfer_barriers[5].buffer = transparency.beam_intersect_buffer.buffer;
	transparency.transfer_barriers[5].size = beam_intersect.size;
}

static void create_buffers(void)
{
	VkBufferCreateInfo host_buffer_info;
	Com_Memset( &host_buffer_info, 0, sizeof(VkBufferCreateInfo) );
	host_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	host_buffer_info.pNext = NULL;
	host_buffer_info.size = transparency.host_buffered_frame_num * transparency.host_frame_size;
	host_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	qvkCreateBuffer(vk.device, &host_buffer_info, NULL, &transparency.host_buffer);

	vk_rtx_buffer_create(
		&transparency.vertex_buffer, 
		TR_VERTEX_MAX_NUM * sizeof(vec3_t),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vk_rtx_buffer_create(
		&transparency.beam_aabb_buffer,
		TR_BEAM_MAX_NUM * sizeof(VkAabbPositionsKHR),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vk_rtx_buffer_create(
		&transparency.index_buffer,
		TR_INDEX_MAX_NUM * sizeof(uint16_t),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vk_rtx_buffer_create(
		&transparency.particle_color_buffer,
		TR_PARTICLE_MAX_NUM * TR_COLOR_SIZE,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vk_rtx_buffer_create(
		&transparency.beam_color_buffer,
		TR_BEAM_MAX_NUM * TR_COLOR_SIZE,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vk_rtx_buffer_create(
		&transparency.sprite_info_buffer,
		TR_SPRITE_MAX_NUM * TR_SPRITE_INFO_SIZE,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vk_rtx_buffer_create(
		&transparency.beam_intersect_buffer,
		TR_BEAM_MAX_NUM * TR_BEAM_INTERSECT_SIZE,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

static bool allocate_and_bind_memory_to_buffers(void)
{
	VkMemoryRequirements host_buffer_requirements;
	qvkGetBufferMemoryRequirements(vk.device, transparency.host_buffer, &host_buffer_requirements);

	const VkMemoryPropertyFlags host_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	const uint32_t host_memory_type = vk_find_memory_type(host_buffer_requirements.memoryTypeBits, host_flags);

	VkMemoryAllocateInfo host_memory_allocate_info;
	Com_Memset( &host_memory_allocate_info, 0, sizeof(VkMemoryAllocateInfo) );
	host_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	host_memory_allocate_info.pNext = NULL;
	host_memory_allocate_info.allocationSize = host_buffer_requirements.size;
	host_memory_allocate_info.memoryTypeIndex = host_memory_type;

	qvkAllocateMemory(vk.device, &host_memory_allocate_info, NULL, &transparency.host_buffer_memory);

	VkBindBufferMemoryInfo bindings[1];
	Com_Memset( &bindings, 0, sizeof(VkBindBufferMemoryInfo) );

	bindings[0].sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
	bindings[0].pNext = NULL;
	bindings[0].buffer = transparency.host_buffer;
	bindings[0].memory = transparency.host_buffer_memory;
	bindings[0].memoryOffset = 0;

	qvkBindBufferMemory2(vk.device, LENGTH(bindings), bindings);

	const size_t host_buffer_size = transparency.host_buffered_frame_num * transparency.host_frame_size;

	qvkMapMemory(vk.device, transparency.host_buffer_memory, 0, host_buffer_size, 0,
		(void**)&transparency.mapped_host_buffer);

	transparency.host_buffer_shadow = (char*)Z_Malloc(transparency.host_frame_size, TAG_GENERAL);
	
	return true;
}

static void create_buffer_views(void)
{
	VkBufferViewCreateInfo particle_color_view_info;
	Com_Memset( &particle_color_view_info, 0, sizeof(VkBufferViewCreateInfo) );
	particle_color_view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	particle_color_view_info.pNext = NULL;
	particle_color_view_info.buffer = transparency.particle_color_buffer.buffer;
	particle_color_view_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	particle_color_view_info.range = TR_PARTICLE_MAX_NUM * TR_COLOR_SIZE;

	VkBufferViewCreateInfo beam_color_view_info;
	Com_Memset( &beam_color_view_info, 0, sizeof(VkBufferViewCreateInfo) );
	beam_color_view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	beam_color_view_info.pNext = NULL;
	beam_color_view_info.buffer = transparency.beam_color_buffer.buffer;
	beam_color_view_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	beam_color_view_info.range = TR_BEAM_MAX_NUM * TR_COLOR_SIZE;

	VkBufferViewCreateInfo sprite_info_view_info;
	Com_Memset( &sprite_info_view_info, 0, sizeof(VkBufferViewCreateInfo) );
	sprite_info_view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	sprite_info_view_info.pNext = NULL;
	sprite_info_view_info.buffer = transparency.sprite_info_buffer.buffer;
	sprite_info_view_info.format = VK_FORMAT_R32G32B32A32_UINT;
	sprite_info_view_info.range = TR_SPRITE_MAX_NUM * TR_SPRITE_INFO_SIZE;

	VkBufferViewCreateInfo beam_intersect_view_info;
	Com_Memset( &beam_intersect_view_info, 0, sizeof(VkBufferViewCreateInfo) );
	beam_intersect_view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	beam_intersect_view_info.pNext = NULL;
	beam_intersect_view_info.buffer = transparency.beam_intersect_buffer.buffer;
	beam_intersect_view_info.format = VK_FORMAT_R32G32B32A32_UINT;
	beam_intersect_view_info.range = TR_BEAM_MAX_NUM * TR_BEAM_INTERSECT_SIZE;

	qvkCreateBufferView(vk.device, &particle_color_view_info, NULL,
		&transparency.particle_color_buffer_view);

	qvkCreateBufferView(vk.device, &beam_color_view_info, NULL,
		&transparency.beam_color_buffer_view);

	qvkCreateBufferView(vk.device, &sprite_info_view_info, NULL,
		&transparency.sprite_info_buffer_view);

	qvkCreateBufferView(vk.device, &beam_intersect_view_info, NULL,
		&transparency.beam_intersect_buffer_view);
}

static void fill_index_buffer(void)
{
	uint16_t* indices = (uint16_t*)transparency.host_buffer_shadow;

	for (size_t i = 0; i < TR_INDEX_MAX_NUM / 6; i++)
	{
		uint16_t* quad = indices + i * 6;

		const uint16_t base_vertex = i * 4;
#if 1
		quad[0] = base_vertex + 0;
		quad[1] = base_vertex + 1;
		quad[2] = base_vertex + 2;
		quad[3] = base_vertex + 2;
		quad[4] = base_vertex + 3;
		quad[5] = base_vertex + 0;
#else
		quad[0] = base_vertex + 0;
		quad[1] = base_vertex + 1;
		quad[2] = base_vertex + 2;
		quad[3] = base_vertex + 2;
		quad[4] = base_vertex + 1;
		quad[5] = base_vertex + 3;
#endif
	}

	memcpy(transparency.mapped_host_buffer, transparency.host_buffer_shadow, sizeof(uint16_t) * TR_INDEX_MAX_NUM);

	VkCommandBuffer cmd_buf = vkpt_begin_command_buffer(&vk.cmd_buffers_transfer);

	VkBufferMemoryBarrier pre_barrier;
	Com_Memset( &pre_barrier, 0, sizeof(VkBufferMemoryBarrier) );
	pre_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	pre_barrier.pNext = NULL;
	pre_barrier.srcAccessMask = 0;
	pre_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	pre_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	pre_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	pre_barrier.buffer = transparency.index_buffer.buffer;
	pre_barrier.size = VK_WHOLE_SIZE;

	qvkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 1, &pre_barrier, 0, NULL);

	VkBufferCopy region;
	Com_Memset( &region, 0, sizeof(VkBufferCopy) );
	region.size = TR_INDEX_MAX_NUM * sizeof(uint16_t);

	qvkCmdCopyBuffer(cmd_buf, transparency.host_buffer, transparency.index_buffer.buffer, 1, &region);

	VkBufferMemoryBarrier post_barrier;
	Com_Memset( &post_barrier, 0, sizeof(VkBufferMemoryBarrier) );
	post_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	post_barrier.pNext = NULL;
	post_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	post_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	post_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	post_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	post_barrier.buffer = transparency.index_buffer.buffer;
	post_barrier.size = VK_WHOLE_SIZE;

	qvkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 1, &post_barrier, 0, NULL);

	vkpt_submit_command_buffer_simple(cmd_buf, vk.queue_transfer, true);
	vkpt_wait_idle(vk.queue_transfer, &vk.cmd_buffers_transfer);
}
