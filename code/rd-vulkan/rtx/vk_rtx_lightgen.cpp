/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Reconstructs point lights from a map's baked lightmaps.
//
// q3map2 strips `light` entities from the BSP at compile time, so a shipped map holds no
// description of what lit it - only the result, baked into the lightmaps and the
// lightgrid. That leaves the path tracer with nothing to sample and the interiors black.
//
// A light near a surface leaves a bright spot on it. The centre of that spot gives the
// light's projection onto the surface, which is two coordinates out of three; the
// distance along the normal is missing. But a light in a room leaves such a spot on the
// floor, on the walls and on the ceiling, and the rays cast from each spot along its own
// surface normal all pass through the light. Intersecting them recovers the third
// coordinate without assuming anything about the falloff law, which matters because a
// q3map2 light can be inverse-square or linear.
//
// It also validates itself: rays that fail to converge are not a light and get dropped.
// That is the reason to prefer this over fitting a falloff curve to one spot - lightmaps
// are 8-bit, so a bright spot clips, and the flat plateau destroys exactly the part of
// the profile a curve fit needs. A clipped plateau still has a usable centroid.
//
// The result goes to maps/<name>.lgt. RTX_LightFile_Load reads it back at map load
// (rtx_light_file.cpp). The lights are used when the map has no light entities of its own.

#include "../tr_local.h"
#include "vk_rtx.h"
#include "rtx_light_edit.h"
#include "rtx_light_file.h"

#include "../../qcommon/qfiles.h"

#define LIGHTGEN_LM_SIZE			128		// q3map2 lightmap page, fixed
#define LIGHTGEN_MAX_RAYS			8192
#define LIGHTGEN_MAX_LIGHTS			1024
#define LIGHTGEN_MAX_BLOCK			64		// a bigger block is a wall, not a fixture

// A spot has to be this bright to be considered lit by something, and this close to its
// face's brightest texel to be a peak rather than a point on a slope.
#define LIGHTGEN_MIN_PEAK			0.45f
#define LIGHTGEN_PEAK_FRACTION		0.85f

// Nearly parallel rays intersect badly, and a light further away than this is almost
// certainly two unrelated spots lining up by chance.
#define LIGHTGEN_MIN_RAY_SIN		0.30f
#define LIGHTGEN_MAX_DISTANCE		1600.0f
#define LIGHTGEN_MAX_MISS			40.0f	// how close two rays must pass to agree
#define LIGHTGEN_CLUSTER_RADIUS		56.0f

// Emitter radius of a loaded light. Keep it small so the sphere does not sink into the
// surface it sits on.
#define LIGHTGEN_EMITTER_RADIUS		4.0f

typedef struct {
	vec3_t	origin;		// the spot centre, on the surface
	vec3_t	normal;		// face normal, so the light lies along +normal
	vec3_t	color;
	float	peak;		// 0..1, the spot's brightest texel
} lightgen_ray_t;

typedef struct {
	vec3_t	origin;
	vec3_t	color;
	float	intensity;
	int		rays;
	float	error;		// mean miss distance of the pairs that voted for it
} lightgen_light_t;

typedef struct {
	const byte			*lightmaps;
	int					num_lightmaps;
	const dsurface_t	*surfaces;
	int					num_surfaces;

	lightgen_ray_t		*rays;
	int					num_rays;
} lightgen_context_t;

static float lightgen_luminance( const byte *texel )
{
	return ( texel[0] * 0.2126f + texel[1] * 0.7152f + texel[2] * 0.0722f ) * (1.0f / 255.0f);
}

static const byte *lightgen_texel( const lightgen_context_t *ctx, int page, int x, int y )
{
	if ( x < 0 || y < 0 || x >= LIGHTGEN_LM_SIZE || y >= LIGHTGEN_LM_SIZE )
		return NULL;

	return ctx->lightmaps + ( (size_t)page * LIGHTGEN_LM_SIZE * LIGHTGEN_LM_SIZE
		+ (size_t)y * LIGHTGEN_LM_SIZE + x ) * 3;
}

/*
=================
lightgen_collect_face

Walks one planar face's lightmap block, finds the bright spots on it and turns each into
a ray. dsurface_t hands over the mapping for free: world = lightmapOrigin + s*vecs[0] +
t*vecs[1], with vecs[2] the normal. q3map2 writes the lightmap's own axes there, so there
is no UV inversion to do and the result is exact.
=================
*/
static void lightgen_collect_face( lightgen_context_t *ctx, const dsurface_t *surf )
{
	static float	lum[LIGHTGEN_MAX_BLOCK * LIGHTGEN_MAX_BLOCK];
	static byte		taken[LIGHTGEN_MAX_BLOCK * LIGHTGEN_MAX_BLOCK];

	const int w = LittleLong( surf->lightmapWidth );
	const int h = LittleLong( surf->lightmapHeight );
	const int page = LittleLong( surf->lightmapNum[0] );
	const int ox = LittleLong( surf->lightmapX[0] );
	const int oy = LittleLong( surf->lightmapY[0] );

	if ( w <= 2 || h <= 2 )
		return;		// too small to tell a peak from an edge

	if ( w > LIGHTGEN_MAX_BLOCK || h > LIGHTGEN_MAX_BLOCK )
		return;

	if ( page < 0 || page >= ctx->num_lightmaps )
		return;

	float block_peak = 0.0f;

	for ( int t = 0; t < h; t++ )
	{
		for ( int s = 0; s < w; s++ )
		{
			const byte *texel = lightgen_texel( ctx, page, ox + s, oy + t );
			const float l = texel ? lightgen_luminance( texel ) : 0.0f;

			lum[t * w + s] = l;
			block_peak = MAX( block_peak, l );
		}
	}

	if ( block_peak < LIGHTGEN_MIN_PEAK )
		return;

	Com_Memset( taken, 0, (size_t)w * h );

	const float accept = MAX( LIGHTGEN_MIN_PEAK, block_peak * LIGHTGEN_PEAK_FRACTION );

	for ( int t = 1; t < h - 1; t++ )
	{
		for ( int s = 1; s < w - 1; s++ )
		{
			const int at = t * w + s;

			if ( taken[at] || lum[at] < accept )
				continue;

			// A local maximum: no neighbour brighter. Ties are fine - a clipped plateau
			// is all ties, and the centroid below is what resolves it.
			bool is_peak = true;

			for ( int dy = -1; dy <= 1 && is_peak; dy++ )
			{
				for ( int dx = -1; dx <= 1; dx++ )
				{
					if ( lum[(t + dy) * w + (s + dx)] > lum[at] )
					{
						is_peak = false;
						break;
					}
				}
			}

			if ( !is_peak )
				continue;

			// Spread over the connected texels at half the peak or brighter and take the
			// luminance-weighted centre of those. This is what survives clipping.
			const float half = lum[at] * 0.5f;

			double	sum_w = 0.0, sum_s = 0.0, sum_t = 0.0;
			double	ring_w = 0.0;
			vec3_t	color, ring;

			VectorClear( color );
			VectorClear( ring );

			for ( int yy = MAX( 0, t - 8 ); yy < MIN( h, t + 9 ); yy++ )
			{
				for ( int xx = MAX( 0, s - 8 ); xx < MIN( w, s + 9 ); xx++ )
				{
					const int idx = yy * w + xx;

					if ( lum[idx] < half )
						continue;

					taken[idx] = 1;

					sum_w += lum[idx];
					sum_s += lum[idx] * xx;
					sum_t += lum[idx] * yy;

					const byte *texel = lightgen_texel( ctx, page, ox + xx, oy + yy );

					if ( !texel )
						continue;

					color[0] += texel[0] * lum[idx];
					color[1] += texel[1] * lum[idx];
					color[2] += texel[2] * lum[idx];

					// The core of a bright spot is clipped to white, so it carries no
					// hue. Take the colour from the ring around it instead. Skip any
					// texel with a channel at or near 255.
					if ( lum[idx] < lum[at] * 0.85f
						&& texel[0] < 250 && texel[1] < 250 && texel[2] < 250 )
					{
						ring[0] += texel[0];
						ring[1] += texel[1];
						ring[2] += texel[2];
						ring_w += 1.0;
					}
				}
			}

			if ( sum_w <= 0.0 || ctx->num_rays >= LIGHTGEN_MAX_RAYS )
				continue;

			const float cs = (float)( sum_s / sum_w );
			const float ct = (float)( sum_t / sum_w );

			lightgen_ray_t *ray = ctx->rays + ctx->num_rays++;

			VectorCopy( surf->lightmapOrigin, ray->origin );
			VectorMA( ray->origin, cs, surf->lightmapVecs[0], ray->origin );
			VectorMA( ray->origin, ct, surf->lightmapVecs[1], ray->origin );

			VectorCopy( surf->lightmapVecs[2], ray->normal );
			VectorNormalize( ray->normal );

			if ( ring_w > 0.0 )
				VectorScale( ring, 1.0f / (float)ring_w, ray->color );
			else
				VectorScale( color, 1.0f / (float)sum_w, ray->color );

			const float hue_peak = MAX( ray->color[0], MAX( ray->color[1], ray->color[2] ) );

			if ( hue_peak > 0.0f )
				VectorScale( ray->color, 1.0f / hue_peak, ray->color );
			else
				VectorSet( ray->color, 1.0f, 1.0f, 1.0f );

			ray->peak = lum[at];
		}
	}
}

/*
=================
lightgen_ray_meeting

Closest approach of two rays. False when they are too parallel to trust, when the meeting
lies behind either surface, or when they miss by more than the tolerance.
=================
*/
static bool lightgen_ray_meeting( const lightgen_ray_t *a, const lightgen_ray_t *b, vec3_t out, float *miss )
{
	vec3_t w0;

	VectorSubtract( a->origin, b->origin, w0 );

	const float d = DotProduct( a->normal, b->normal );
	const float sin2 = 1.0f - d * d;

	if ( sin2 < LIGHTGEN_MIN_RAY_SIN * LIGHTGEN_MIN_RAY_SIN )
		return false;

	const float e = DotProduct( a->normal, w0 );
	const float f = DotProduct( b->normal, w0 );

	const float ta = ( d * f - e ) / sin2;
	const float tb = ( f - d * e ) / sin2;

	if ( ta <= 1.0f || tb <= 1.0f )
		return false;		// behind one of the faces

	if ( ta > LIGHTGEN_MAX_DISTANCE || tb > LIGHTGEN_MAX_DISTANCE )
		return false;

	vec3_t pa, pb, diff;

	VectorMA( a->origin, ta, a->normal, pa );
	VectorMA( b->origin, tb, b->normal, pb );
	VectorSubtract( pa, pb, diff );

	*miss = VectorLength( diff );

	if ( *miss > LIGHTGEN_MAX_MISS )
		return false;

	VectorAdd( pa, pb, out );
	VectorScale( out, 0.5f, out );

	return true;
}

/*
=================
lightgen_solve

Every pair of rays that agrees on a meeting point votes for it, and the votes are
clustered. A cluster resting on a single pair is a coincidence, so two are required.
=================
*/
static int lightgen_solve( lightgen_context_t *ctx, lightgen_light_t *lights, int max_lights )
{
	int num_lights = 0;

	for ( int i = 0; i < ctx->num_rays; i++ )
	{
		for ( int j = i + 1; j < ctx->num_rays; j++ )
		{
			vec3_t	meet;
			float	miss;

			if ( !lightgen_ray_meeting( ctx->rays + i, ctx->rays + j, meet, &miss ) )
				continue;

			int found = -1;

			for ( int k = 0; k < num_lights; k++ )
			{
				vec3_t delta;

				VectorSubtract( lights[k].origin, meet, delta );

				if ( VectorLength( delta ) < LIGHTGEN_CLUSTER_RADIUS )
				{
					found = k;
					break;
				}
			}

			if ( found < 0 )
			{
				if ( num_lights >= max_lights )
					continue;

				found = num_lights++;

				Com_Memset( lights + found, 0, sizeof(lights[0]) );
				VectorCopy( meet, lights[found].origin );
			}

			lightgen_light_t *l = lights + found;

			// Running mean of the meeting points and of the contributing colour.
			const float n = (float)( l->rays + 1 );

			for ( int c = 0; c < 3; c++ )
			{
				const float pair_color = ( ctx->rays[i].color[c] + ctx->rays[j].color[c] ) * 0.5f;

				l->origin[c] += ( meet[c] - l->origin[c] ) / n;
				l->color[c] += ( pair_color - l->color[c] ) / n;
			}

			l->error += ( miss - l->error ) / n;
			l->rays++;
		}
	}

	// Drop the clusters resting on one pair, and put an intensity on what is left.
	int kept = 0;

	for ( int k = 0; k < num_lights; k++ )
	{
		if ( lights[k].rays < 2 )
			continue;

		if ( kept != k )
			lights[kept] = lights[k];

		lightgen_light_t *l = lights + kept++;

		// E(0) = I / h^2 on the spot directly below the light, so I = E(0) * h^2. Take
		// the brightest spot that faces it, which is the one closest to being below it.
		float best = 0.0f;

		for ( int i = 0; i < ctx->num_rays; i++ )
		{
			vec3_t delta;

			VectorSubtract( l->origin, ctx->rays[i].origin, delta );

			const float h = VectorLength( delta );

			if ( h < 1.0f || h > LIGHTGEN_MAX_DISTANCE )
				continue;

			if ( DotProduct( delta, ctx->rays[i].normal ) < h * 0.9f )
				continue;		// this spot is not below that light

			best = MAX( best, ctx->rays[i].peak * h * h );
		}

		l->intensity = best;

		const float peak = MAX( l->color[0], MAX( l->color[1], l->color[2] ) );

		if ( peak > 0.0f )
			VectorScale( l->color, 1.0f / peak, l->color );
		else
			VectorSet( l->color, 1.0f, 1.0f, 1.0f );
	}

	return kept;
}

/*
=================
lightgen_count_light_entities

The reconstruction is only for maps that shipped without their lights. If the entity lump
still holds them, they are the truth and guessing would be worse. Crude on purpose: it
only has to answer that one question.
=================
*/
static int lightgen_count_light_entities( const char *entities, int len )
{
	char	token[MAX_TOKEN_CHARS];
	int		count = 0;
	int		n = 0;
	bool	in_string = false;
	bool	expect_value = false;

	for ( int i = 0; i < len; i++ )
	{
		const char c = entities[i];

		if ( c == '"' )
		{
			if ( in_string )
			{
				token[n] = '\0';

				if ( expect_value )
				{
					if ( !Q_stricmp( token, "light" ) )
						count++;

					expect_value = false;
				}
				else if ( !Q_stricmp( token, "classname" ) )
				{
					expect_value = true;
				}

				in_string = false;
			}
			else
			{
				n = 0;
				in_string = true;
			}

			continue;
		}

		if ( in_string && n < (int)sizeof(token) - 1 )
			token[n++] = c;
	}

	return count;
}

/*
=================
R_GenerateLightFile

Reads the BSP straight off disk rather than from the loaded world: the lightmap UVs in
memory have already been remapped into an atlas, and a recompute command should not
depend on which map happens to be loaded.
=================
*/
static void R_GenerateLightFile( const char *mapname, qboolean only_if_no_entities )
{
	char	bsppath[MAX_QPATH];
	char	lgtpath[MAX_QPATH];
	void	*buffer = NULL;

	Com_sprintf( bsppath, sizeof(bsppath), "maps/%s.bsp", mapname );

	const long len = ri.FS_ReadFile( bsppath, &buffer );

	if ( len <= 0 || !buffer )
	{
		Com_Printf( "lightgen: cannot read %s\n", bsppath );
		return;
	}

	const byte		*base = (const byte *)buffer;
	const dheader_t	*header = (const dheader_t *)base;

	if ( LittleLong( header->ident ) != BSP_IDENT || LittleLong( header->version ) != BSP_VERSION )
	{
		Com_Printf( "lightgen: %s is not a BSP this build understands\n", bsppath );
		ri.FS_FreeFile( buffer );
		return;
	}

	const lump_t *l_ents = header->lumps + LUMP_ENTITIES;
	const lump_t *l_surf = header->lumps + LUMP_SURFACES;
	const lump_t *l_lm   = header->lumps + LUMP_LIGHTMAPS;

	if ( only_if_no_entities )
	{
		const int lights = lightgen_count_light_entities(
			(const char *)( base + LittleLong( l_ents->fileofs ) ), LittleLong( l_ents->filelen ) );

		if ( lights > 0 )
		{
			Com_Printf( "lightgen: %s keeps %i light entities, nothing to reconstruct\n", mapname, lights );
			ri.FS_FreeFile( buffer );
			return;
		}
	}

	lightgen_context_t ctx;

	Com_Memset( &ctx, 0, sizeof(ctx) );

	ctx.lightmaps = base + LittleLong( l_lm->fileofs );
	ctx.num_lightmaps = LittleLong( l_lm->filelen ) / ( LIGHTGEN_LM_SIZE * LIGHTGEN_LM_SIZE * 3 );
	ctx.surfaces = (const dsurface_t *)( base + LittleLong( l_surf->fileofs ) );
	ctx.num_surfaces = LittleLong( l_surf->filelen ) / (int)sizeof(dsurface_t);

	if ( ctx.num_lightmaps == 0 )
	{
		Com_Printf( "lightgen: %s has no internal lightmaps, nothing to read\n", mapname );
		ri.FS_FreeFile( buffer );
		return;
	}

	ctx.rays = (lightgen_ray_t *)Z_Malloc( LIGHTGEN_MAX_RAYS * sizeof(lightgen_ray_t),
		TAG_TEMP_WORKSPACE, qfalse );

	int planar = 0;

	for ( int i = 0; i < ctx.num_surfaces; i++ )
	{
		const dsurface_t *surf = ctx.surfaces + i;

		// Planar faces only. A patch or a triangle soup has no single lightmap axis, so
		// the free world mapping above does not hold for them.
		if ( LittleLong( surf->surfaceType ) != MST_PLANAR )
			continue;

		planar++;
		lightgen_collect_face( &ctx, surf );
	}

	lightgen_light_t *lights = (lightgen_light_t *)Z_Malloc( LIGHTGEN_MAX_LIGHTS * sizeof(lightgen_light_t),
		TAG_TEMP_WORKSPACE, qfalse );

	const int num_lights = lightgen_solve( &ctx, lights, LIGHTGEN_MAX_LIGHTS );

	Com_Printf( "lightgen: %s - %i planar faces, %i bright spots, %i lights\n",
		mapname, planar, ctx.num_rays, num_lights );

	// Text on purpose: this is a guess, and a guess should be readable and correctable
	// by hand.
	Com_sprintf( lgtpath, sizeof(lgtpath), "maps/%s.lgt", mapname );

	// The emissive and global blocks of the old file stay in the new one.
	const std::string kept = RTX_LightFile_KeptBlocks( mapname );

	fileHandle_t f = ri.FS_FOpenFileWrite( lgtpath, qtrue );

	if ( f )
	{
		char line[256];

		Com_sprintf( line, sizeof(line),
			"// reconstructed from the baked lightmaps of %s.bsp\n"
			"// %i bright spots on %i planar faces\n"
			"lights %i\n", mapname, ctx.num_rays, planar, num_lights );
		ri.FS_Write( line, (int)strlen( line ), f );

		for ( int i = 0; i < num_lights; i++ )
		{
			const lightgen_light_t *lgt = lights + i;

			Com_sprintf( line, sizeof(line),
				"{\n"
				"\torigin %.1f %.1f %.1f\n"
				"\tcolor %.3f %.3f %.3f\n"
				"\tintensity %.1f\n"
				"\trays %i\n"
				"\terror %.1f\n"
				"}\n",
				lgt->origin[0], lgt->origin[1], lgt->origin[2],
				lgt->color[0], lgt->color[1], lgt->color[2],
				lgt->intensity, lgt->rays, lgt->error );
			ri.FS_Write( line, (int)strlen( line ), f );
		}

		if ( !kept.empty() )
			ri.FS_Write( kept.c_str(), (int)kept.size(), f );

		ri.FS_FCloseFile( f );

		Com_Printf( "lightgen: wrote %s\n", lgtpath );
	}
	else
	{
		Com_Printf( "lightgen: cannot write %s\n", lgtpath );
	}

	Z_Free( lights );
	Z_Free( ctx.rays );
	ri.FS_FreeFile( buffer );
}

/*
=================
R_LightGen_f

pt_lightgen [mapname] [force] - defaults to the loaded map. A file that holds light edits
is kept unless force is given.
=================
*/
void R_LightGen_f( void )
{
	char mapname[MAX_QPATH];
	const qboolean force = (qboolean)( ri.Cmd_Argc() > 1 && !Q_stricmp( ri.Cmd_Argv( ri.Cmd_Argc() - 1 ), "force" ) );

	if ( ri.Cmd_Argc() > 1 && Q_stricmp( ri.Cmd_Argv( 1 ), "force" ) )
	{
		Q_strncpyz( mapname, ri.Cmd_Argv( 1 ), sizeof(mapname) );
	}
	else if ( tr.world )
	{
		Q_strncpyz( mapname, tr.world->baseName, sizeof(mapname) );
	}
	else
	{
		Com_Printf( "usage: pt_lightgen <mapname>\n" );
		return;
	}

	if ( !RTX_LightFile_CheckRegenerate( mapname, force ) )
		return;

	R_GenerateLightFile( mapname, qfalse );
}

/*
=================
R_LightGen_EnsureForMap

Called at map load. Only reconstructs when the map kept no lights of its own, and only
when the file is not already there.
=================
*/
void R_LightGen_EnsureForMap( const char *mapname )
{
	char lgtpath[MAX_QPATH];

	Com_sprintf( lgtpath, sizeof(lgtpath), "maps/%s.lgt", mapname );

	if ( ri.FS_ReadFile( lgtpath, NULL ) > 0 )
		return;

	R_GenerateLightFile( mapname, qtrue );
}
