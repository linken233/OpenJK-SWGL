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

// tr_surf.c

#include "tr_local.h"

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/

//============================================================================

/*
==============
RB_CheckOverflow
==============
*/
void RB_CheckOverflow( int verts, int indexes ) {
	if (tess.numVertexes + verts < SHADER_MAX_VERTEXES
		&& tess.numIndexes + indexes < SHADER_MAX_INDEXES) {
		return;
	}

	RB_EndSurface();

	if ( verts >= SHADER_MAX_VERTEXES ) {
		Com_Error(ERR_DROP, "RB_CheckOverflow: verts > MAX (%d > %d)", verts, SHADER_MAX_VERTEXES );
	}
	if ( indexes >= SHADER_MAX_INDEXES ) {
		Com_Error(ERR_DROP, "RB_CheckOverflow: indices > MAX (%d > %d)", indexes, SHADER_MAX_INDEXES );
	}

	RB_BeginSurface(tess.shader, tess.fogNum );
}

/*
==============
RB_AddQuadStamp2
==============
*/
void RB_AddQuadStamp2( float x, float y, float w, float h, float s1, float t1, float s2, float t2, color4ub_t color ) {
	int			numIndexes;
	int			numVerts;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 4, 6 );

#ifdef USE_VBO
	tess.surfType = SF_TRIANGLES;
#endif

	numIndexes = tess.numIndexes;
	numVerts = tess.numVertexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[numIndexes + 0] = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	tess.xyz[numVerts + 0][0] = x;
	tess.xyz[numVerts + 0][1] = y;
	tess.xyz[numVerts + 0][2] = 0;

	tess.xyz[numVerts + 1][0] = x + w;
	tess.xyz[numVerts + 1][1] = y;
	tess.xyz[numVerts + 1][2] = 0;

	tess.xyz[numVerts + 2][0] = x + w;
	tess.xyz[numVerts + 2][1] = y + h;
	tess.xyz[numVerts + 2][2] = 0;

	tess.xyz[numVerts + 3][0] = x;
	tess.xyz[numVerts + 3][1] = y + h;
	tess.xyz[numVerts + 3][2] = 0;

	tess.texCoords[0][numVerts + 0][0] = s1;
	tess.texCoords[0][numVerts + 0][1] = t1;
	tess.texCoords[0][numVerts + 1][0] = s2;
	tess.texCoords[0][numVerts + 1][1] = t1;
	tess.texCoords[0][numVerts + 2][0] = s2;
	tess.texCoords[0][numVerts + 2][1] = t2;
	tess.texCoords[0][numVerts + 3][0] = s1;
	tess.texCoords[0][numVerts + 3][1] = t2;

	byteAlias_t *baDest = NULL, *baSource = (byteAlias_t *)color;
	baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 0]; baDest->ui = baSource->ui;
	baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 1]; baDest->ui = baSource->ui;
	baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 2]; baDest->ui = baSource->ui;
	baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 3]; baDest->ui = baSource->ui;

}

/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, color4ub_t color, float s1, float t1, float s2, float t2 ) {
	vec3_t		normal;
	int			numIndexes;
	int			numVerts;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 4, 6 );

#ifdef USE_VBO
	tess.surfType = SF_TRIANGLES;
#endif

	numIndexes = tess.numIndexes;
	numVerts = tess.numVertexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	// triangle indexes for a simple quad
	tess.indexes[numIndexes + 0] = numVerts + 0;
	tess.indexes[numIndexes + 1] = numVerts + 1;
	tess.indexes[numIndexes + 2] = numVerts + 3;
	tess.indexes[numIndexes + 3] = numVerts + 3;
	tess.indexes[numIndexes + 4] = numVerts + 1;
	tess.indexes[numIndexes + 5] = numVerts + 2;

	tess.xyz[numVerts + 0][0] = origin[0] + left[0] + up[0];
	tess.xyz[numVerts + 0][1] = origin[1] + left[1] + up[1];
	tess.xyz[numVerts + 0][2] = origin[2] + left[2] + up[2];

	tess.xyz[numVerts + 1][0] = origin[0] - left[0] + up[0];
	tess.xyz[numVerts + 1][1] = origin[1] - left[1] + up[1];
	tess.xyz[numVerts + 1][2] = origin[2] - left[2] + up[2];

	tess.xyz[numVerts + 2][0] = origin[0] - left[0] - up[0];
	tess.xyz[numVerts + 2][1] = origin[1] - left[1] - up[1];
	tess.xyz[numVerts + 2][2] = origin[2] - left[2] - up[2];

	tess.xyz[numVerts + 3][0] = origin[0] + left[0] - up[0];
	tess.xyz[numVerts + 3][1] = origin[1] + left[1] - up[1];
	tess.xyz[numVerts + 3][2] = origin[2] + left[2] - up[2];

	// constant normal all the way around
	VectorSubtract( vec3_origin, backEnd.viewParms.ori.axis[0], normal );

	tess.normal[numVerts][0] = tess.normal[numVerts + 1][0] = tess.normal[numVerts + 2][0] = tess.normal[numVerts + 3][0] = normal[0];
	tess.normal[numVerts][1] = tess.normal[numVerts + 1][1] = tess.normal[numVerts + 2][1] = tess.normal[numVerts + 3][1] = normal[1];
	tess.normal[numVerts][2] = tess.normal[numVerts + 1][2] = tess.normal[numVerts + 2][2] = tess.normal[numVerts + 3][2] = normal[2];

	// standard square texture coordinates
	tess.texCoords[0][numVerts + 0][0] = tess.texCoords[1][numVerts + 0][0] = s1;
	tess.texCoords[0][numVerts + 0][1] = tess.texCoords[1][numVerts + 0][1] = t1;
	tess.texCoords[0][numVerts + 1][0] = tess.texCoords[1][numVerts + 1][0] = s2;
	tess.texCoords[0][numVerts + 1][1] = tess.texCoords[1][numVerts + 1][1] = t1;
	tess.texCoords[0][numVerts + 2][0] = tess.texCoords[1][numVerts + 2][0] = s2;
	tess.texCoords[0][numVerts + 2][1] = tess.texCoords[1][numVerts + 2][1] = t2;
	tess.texCoords[0][numVerts + 3][0] = tess.texCoords[1][numVerts + 3][0] = s1;
	tess.texCoords[0][numVerts + 3][1] = tess.texCoords[1][numVerts + 3][1] = t2;

	// should this be identity and let the shader specify from entity?
	byteAlias_t *baDest = NULL, *baSource = (byteAlias_t *)color;
	baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 0]; baDest->ui = baSource->ui;
	baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 1]; baDest->ui = baSource->ui;
	baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 2]; baDest->ui = baSource->ui;
	baDest = (byteAlias_t *)&tess.vertexColors[numVerts + 3]; baDest->ui = baSource->ui;
}

/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, color4ub_t color ) {
	RB_AddQuadStampExt( origin, left, up, color, 0, 0, 1, 1 );
}

#ifdef USE_RTX
// Used by the tracer's light-poly debug visualisation.
void RB_AddTriangle( vec3_t a, vec3_t b, vec3_t c, color4ub_t color )
{
	int numIndexes = tess.numIndexes;
	int numVerts = tess.numVertexes;

	tess.numVertexes += 3;
	tess.numIndexes += 3;

	tess.indexes[numIndexes + 0] = numVerts + 0;
	tess.indexes[numIndexes + 1] = numVerts + 1;
	tess.indexes[numIndexes + 2] = numVerts + 2;

	VectorCopy( a, tess.xyz[numVerts + 0] );
	VectorCopy( b, tess.xyz[numVerts + 1] );
	VectorCopy( c, tess.xyz[numVerts + 2] );

	vec3_t normal, tmp;
	VectorSubtract( b, a, normal );
	VectorSubtract( c, a, tmp );
	CrossProduct( normal, tmp, normal );
	VectorNormalize( normal );

	VectorCopy( normal, tess.normal[numVerts + 0] );
	VectorCopy( normal, tess.normal[numVerts + 1] );
	VectorCopy( normal, tess.normal[numVerts + 2] );

	tess.texCoords[0][numVerts + 0][0] = tess.texCoords[1][numVerts + 0][0] = 0.0f;
	tess.texCoords[0][numVerts + 0][1] = tess.texCoords[1][numVerts + 0][1] = 0.0f;

	tess.texCoords[0][numVerts + 1][0] = tess.texCoords[1][numVerts + 1][0] = 1.0f;
	tess.texCoords[0][numVerts + 1][1] = tess.texCoords[1][numVerts + 1][1] = 0.0f;

	tess.texCoords[0][numVerts + 2][0] = tess.texCoords[1][numVerts + 2][0] = 0.5f;
	tess.texCoords[0][numVerts + 2][1] = tess.texCoords[1][numVerts + 2][1] = 1.0f;

	byteAlias_t *baSource = (byteAlias_t *)color;
	((byteAlias_t *)&tess.vertexColors[numVerts + 0])->ui = baSource->ui;
	((byteAlias_t *)&tess.vertexColors[numVerts + 1])->ui = baSource->ui;
	((byteAlias_t *)&tess.vertexColors[numVerts + 2])->ui = baSource->ui;
}
#endif

/*
==============
RB_SurfaceSprite
==============
*/
static void RB_SurfaceSprite( void ) {
	vec3_t		left, up;
	float		radius;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	if ( backEnd.currentEntity->e.rotation == 0 ) {
		VectorScale( backEnd.viewParms.ori.axis[1], radius, left );
		VectorScale( backEnd.viewParms.ori.axis[2], radius, up );
	} else {
		float	s, c;
		float	ang;

		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin( ang );
		c = cos( ang );

		VectorScale( backEnd.viewParms.ori.axis[1], c * radius, left );
		VectorMA( left, -s * radius, backEnd.viewParms.ori.axis[2], left );

		VectorScale( backEnd.viewParms.ori.axis[2], c * radius, up );
		VectorMA( up, s * radius, backEnd.viewParms.ori.axis[1], up );
	}
	if ( backEnd.viewParms.portalView == PV_MIRROR) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}

/*
=======================
RB_SurfaceOrientedQuad
=======================
*/
static void RB_SurfaceOrientedQuad( void )
{
	vec3_t	left, up;
	float	radius;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	// SP cgame sets only axis[0], the normal.
	MakeNormalVectors( backEnd.currentEntity->e.axis[0], left, up );

	if ( backEnd.currentEntity->e.rotation == 0 )
	{
		VectorScale( left, radius, left );
		VectorScale( up, radius, up );
	}
	else
	{
		vec3_t	tempLeft, tempUp;
		float	s, c;
		float	ang;

		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin( ang );
		c = cos( ang );

		// Use a temp so we don't trash the values we'll need later
		VectorScale( left, c * radius, tempLeft );
		VectorMA( tempLeft, -s * radius, up, tempLeft );

		VectorScale( up, c * radius, tempUp );
		VectorMA( tempUp, s * radius, left, up ); // no need to use the temp anymore, so copy into the dest vector ( up )

		// This was copied for safekeeping, we're done, so we can move it back to left
		VectorCopy( tempLeft, left );
	}

	if ( backEnd.viewParms.portalView == PV_MIRROR)
	{
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}

/*
=============
RB_SurfacePolychain
=============
*/
void RB_SurfacePolychain( const srfPoly_t *p ) {
	int		i;
	int		numv;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( p->numVerts, 3*(p->numVerts - 2) );

#ifdef USE_VBO
	tess.surfType = SF_POLY;
#endif

	// fan triangles into the tess array
	numv = tess.numVertexes;
	for ( i = 0; i < p->numVerts; i++ ) {
		VectorCopy( p->verts[i].xyz, tess.xyz[numv] );
		tess.texCoords[0][numv][0] = p->verts[i].st[0];
		tess.texCoords[0][numv][1] = p->verts[i].st[1];
		byteAlias_t *baDest = (byteAlias_t *)&tess.vertexColors[numv++],
					*baSource = (byteAlias_t *)&p->verts[ i ].modulate;
		baDest->i = baSource->i;
	}

	// generate fan indexes into the tess array
	for ( i = 0; i < p->numVerts-2; i++ ) {
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	}

	tess.numVertexes = numv;
}

inline static uint32_t ComputeFinalVertexColor( const byte *colors ) {
	int k;
	byteAlias_t result;
	uint32_t r, g, b;

	for ( k=0; k<4; k++ )
		result.b[k] = colors[k];

	if (tess.shader->lightmapIndex[0] != LIGHTMAP_BY_VERTEX )
		return result.ui;

	if ( r_fullbright->integer ) {
		result.b[0] = 255;
		result.b[1] = 255;
		result.b[2] = 255;
		return result.ui;
	}
	// an optimization could be added here to compute the style[0] (which is always the world normal light)
	r = g = b = 0;
	for( k=0; k<MAXLIGHTMAPS; k++ ) {
		if ( tess.shader->styles[k] < LS_UNUSED ) {
			byte *styleColor = backEndDataBE->styleColors[tess.shader->styles[k]];

			r += (uint32_t)(*colors++) * (uint32_t)(*styleColor++);
			g += (uint32_t)(*colors++) * (uint32_t)(*styleColor++);
			b += (uint32_t)(*colors++) * (uint32_t)(*styleColor);
			colors++;
		}
		else
			break;
	}
	result.b[0] = Com_Clamp( 0, 255, r >> 8 );
	result.b[1] = Com_Clamp( 0, 255, g >> 8 );
	result.b[2] = Com_Clamp( 0, 255, b >> 8 );

	return result.ui;
}

/*
=============
RB_SurfaceTriangles
=============
*/
void RB_SurfaceTriangles( const srfTriangles_t *srf ) {
	int					i;
	const srfVert_t		*dv;
	float				*xyz, *normal, *texCoords0, *texCoords1, *texCoords2, *texCoords3, *texCoords4;
	byte				*color;

#ifdef USE_VBO

	if (tess.allowVBO && srf->vboItemIndex) {

		// transition to vbo render list
		if (tess.vbo_world_index == 0) {
			RB_EndSurface();
			RB_BeginSurface(tess.shader, tess.fogNum);
			// set some dummy parameters for RB_EndSurface
			tess.numIndexes = 1;
			tess.numVertexes = 0;
			VBO_ClearQueue();
		}
		tess.surfType = SF_TRIANGLES;
		tess.vbo_world_index = srf->vboItemIndex;

		VBO_QueueItem(srf->vboItemIndex);
		return; // no need to tesselate anything
	}

	VBO_Flush();
#endif // USE_VBO

	RB_CHECKOVERFLOW( srf->numVerts, srf->numIndexes );

#ifdef USE_VBO
	tess.surfType = SF_TRIANGLES;
#endif

	for ( i = 0 ; i < srf->numIndexes ; i += 3 ) {
		tess.indexes[ tess.numIndexes + i + 0 ] = tess.numVertexes + srf->indexes[ i + 0 ];
		tess.indexes[ tess.numIndexes + i + 1 ] = tess.numVertexes + srf->indexes[ i + 1 ];
		tess.indexes[ tess.numIndexes + i + 2 ] = tess.numVertexes + srf->indexes[ i + 2 ];
	}
	tess.numIndexes += srf->numIndexes;

	dv = srf->verts;
	xyz = tess.xyz[ tess.numVertexes ];
	normal = tess.normal[ tess.numVertexes ];
	texCoords0 = tess.texCoords[0][tess.numVertexes];
	texCoords1 = tess.texCoords[1][tess.numVertexes];
	texCoords2 = tess.texCoords[2][tess.numVertexes];
	texCoords3 = tess.texCoords[3][tess.numVertexes];
	texCoords4 = tess.texCoords[4][tess.numVertexes];
	color = tess.vertexColors[ tess.numVertexes ];

	for ( i = 0 ; i < srf->numVerts ; i++, dv++/*, texCoords0 += 2*//* *NUM_TEX_COORDS*/)
	{
		xyz[0] = dv->xyz[0];
		xyz[1] = dv->xyz[1];
		xyz[2] = dv->xyz[2];
		xyz += 4;

		normal[0] = dv->normal[0];
		normal[1] = dv->normal[1];
		normal[2] = dv->normal[2];
		normal += 4;

#ifdef USE_VK_PBR
		VectorCopy4( dv->qtangent, tess.qtangent[tess.numVertexes + i] );
		VectorCopy4( dv->lightdir, tess.lightdir[tess.numVertexes + i] );
#endif

		texCoords0[0] = dv->st[0];
		texCoords0[1] = dv->st[1];
		texCoords0 += 2;

		// Lightmaps, convert this to a for loop.
		if (tess.shader->lightmapIndex[0] >= 0) {
			texCoords1[0] = dv->lightmap[0][0];
			texCoords1[1] = dv->lightmap[0][1];
			texCoords1 += 2;
		}
		if (tess.shader->lightmapIndex[1] >= 0) {
			texCoords2[0] = dv->lightmap[1][0];
			texCoords2[1] = dv->lightmap[1][1];
			texCoords2 += 2;
		}
		if (tess.shader->lightmapIndex[2] >= 0) {
			texCoords3[0] = dv->lightmap[2][0];
			texCoords3[1] = dv->lightmap[2][1];
			texCoords3 += 2;
		}
		if (tess.shader->lightmapIndex[3] >= 0) {
			texCoords4[0] = dv->lightmap[3][0];
			texCoords4[1] = dv->lightmap[3][1];
			texCoords4 += 2;
		}

		*(unsigned *)color = ComputeFinalVertexColor((byte *)dv->color);
		color += 4;
	}

	tess.numVertexes += srf->numVerts;
}

/*
==============
RB_SurfaceBeam
==============
*/
static void RB_SurfaceBeam(void)
{
#define NUM_BEAM_SEGS 6
	const refEntity_t* e;
	int i;
	vec3_t perpvec, direction, normalized_direction;
	vec3_t oldorigin, origin;
	vec3_t points[NUM_BEAM_SEGS + 1][2];

	e = &backEnd.currentEntity->e;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if (VectorNormalize(normalized_direction) == 0)
		return;

	PerpendicularVector(perpvec, normalized_direction);

	VectorScale(perpvec, 4, perpvec);

	for (i = 0; i <= NUM_BEAM_SEGS; i++)
	{
		RotatePointAroundVector(points[i][0], normalized_direction, perpvec, (360.0 / NUM_BEAM_SEGS) * i);
		VectorAdd(points[i][0], direction, points[i][1]);
	}

	tess.numIndexes = 0;
	tess.numVertexes = 0;

	vk_bind(tr.whiteImage);

	// skinNum selects the colour: 0 red, 1 green, 2 blue
	byte beamRGB[3] = { 255, 0, 0 };
	if (e->skinNum == 1) {
		beamRGB[0] = 0; beamRGB[1] = 255; beamRGB[2] = 0;
	} else if (e->skinNum == 2) {
		beamRGB[0] = 128; beamRGB[1] = 128; beamRGB[2] = 255;
	}

	for (i = 0; i < (NUM_BEAM_SEGS + 1) * 2; i++) {
		tess.svars.colors[0][i][0] = beamRGB[0];
		tess.svars.colors[0][i][1] = beamRGB[1];
		tess.svars.colors[0][i][2] = beamRGB[2];
		tess.svars.colors[0][i][3] = 255;
	}

	for (i = 0; i <= NUM_BEAM_SEGS; i++) {
		VectorCopy(points[i][0], tess.xyz[i * 2 + 0]);
		VectorCopy(points[i][1], tess.xyz[i * 2 + 1]);
	}
	tess.numVertexes = (NUM_BEAM_SEGS + 1) * 2;
	
	vk_bind_pipeline(vk.std_pipeline.surface_beam_pipeline);
	vk_bind_geometry(TESS_XYZ | TESS_RGBA0);
	vk_draw_geometry(DEPTH_RANGE_NORMAL, qfalse);

	tess.numIndexes = 0;
	tess.numVertexes = 0;
}

//------------------
// DoSprite
//------------------
static void DoSprite( vec3_t origin, float radius, float rotation )
{
	float	s, c;
	float	ang;
	vec3_t	left, up;

	ang = M_PI * rotation / 180.0f;
	s = sin( ang );
	c = cos( ang );

	VectorScale( backEnd.viewParms.ori.axis[1], c * radius, left );
	VectorMA( left, -s * radius, backEnd.viewParms.ori.axis[2], left );

	VectorScale( backEnd.viewParms.ori.axis[2], c * radius, up );
	VectorMA( up, s * radius, backEnd.viewParms.ori.axis[1], up );

	if ( backEnd.viewParms.portalView == PV_MIRROR)
	{
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}

//------------------
// RB_SurfaceSaber
//------------------
#define MAX_SABER_GLOW_LENGTH	65536.0f	// longer than any map
#define MAX_SABER_GLOW_SPRITES	512

static void RB_SurfaceSaberGlow()
{
	vec3_t		end;
	refEntity_t *e;

	e = &backEnd.currentEntity->e;

	// A length of +inf (bad SFX saber trail points) never ends the loop, and a radius
	// near 0 makes thousands of sprites: skip bad values, cap the sprite count.
	// The comparisons are false for NaN.
	if ( !( e->saberLength < MAX_SABER_GLOW_LENGTH ) || !( e->radius > -MAX_SABER_GLOW_LENGTH && e->radius < MAX_SABER_GLOW_LENGTH ) )
	{
		static qboolean warned = qfalse;
		if ( !warned ) {
			warned = qtrue;
			ri.Printf( PRINT_DEVELOPER, "RB_SurfaceSaberGlow: skipped a glow, length %f radius %f\n", e->saberLength, e->radius );
		}
		return;
	}

	// Render the glow part of the blade
	int numSprites = 0;
	for ( float i = e->saberLength; i > 0 && numSprites < MAX_SABER_GLOW_SPRITES; i -= e->radius * 0.65f, numSprites++ )
	{
		VectorMA( e->origin, i, e->axis[0], end );

		DoSprite( end, e->radius, 0.0f );//Q_flrand(0.0f, 1.0f) * 360.0f );
		e->radius += 0.017f;
	}

	// Big hilt sprite
	// Please don't kill me Pat...I liked the hilt glow blob, but wanted a subtle pulse.:)  Feel free to ditch it if you don't like it.  --Jeff
	// Please don't kill me Jeff...  The pulse is good, but now I want the halo bigger if the saber is shorter...  --Pat
	DoSprite( e->origin, 5.5f + Q_flrand(0.0f, 1.0f) * 0.25f, 0.0f );//Q_flrand(0.0f, 1.0f) * 360.0f );
}

/*
==============
RB_SurfaceLine
==============
*/
//
//	Values for a proper line render primitive...
//		Width
//		STScale (how many times to loop a texture)
//		alpha
//		RGB
//
//  Values for proper line object...
//		lifetime
//		dscale
//		startalpha, endalpha
//		startRGB, endRGB
//

static void DoLine( const vec3_t start, const vec3_t end, const vec3_t up, float spanWidth )
{
	float		spanWidth2;
	int			vbase;

	RB_CHECKOVERFLOW( 4, 6 );

	vbase = tess.numVertexes;

	spanWidth2 = -spanWidth;

	VectorMA( start, spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];// * 0.25;//wtf??not sure why the code would be doing this
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];// * 0.25;
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];// * 0.25;
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];// * 0.25;
	tess.numVertexes++;

	VectorMA( start, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 1;//backEnd.currentEntity->e.shaderTexCoord[0];
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	VectorMA( end, spanWidth, up, tess.xyz[tess.numVertexes] );

	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = 1;//backEnd.currentEntity->e.shaderTexCoord[1];
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	VectorMA( end, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 1;//backEnd.currentEntity->e.shaderTexCoord[0];
	tess.texCoords[0][tess.numVertexes][1] = 1;//backEnd.currentEntity->e.shaderTexCoord[1];
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}

static void DoLine2( const vec3_t start, const vec3_t end, const vec3_t up, float spanWidth, float spanWidth2 )
{
	int			vbase;

	RB_CHECKOVERFLOW( 4, 6 );

	vbase = tess.numVertexes;

	VectorMA( start, spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];// * 0.25;//wtf??not sure why the code would be doing this
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];// * 0.25;
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];// * 0.25;
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];// * 0.25;
	tess.numVertexes++;

	VectorMA( start, -spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 1;//backEnd.currentEntity->e.shaderTexCoord[0];
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	VectorMA( end, spanWidth2, up, tess.xyz[tess.numVertexes] );

	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = 1;//backEnd.currentEntity->e.shaderTexCoord[1];
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	VectorMA( end, -spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 1;//backEnd.currentEntity->e.shaderTexCoord[0];
	tess.texCoords[0][tess.numVertexes][1] = 1;//backEnd.currentEntity->e.shaderTexCoord[1];
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}

static void DoLine_Oriented( const vec3_t start, const vec3_t end, const vec3_t up, float spanWidth )
{
	float		spanWidth2;
	int			vbase;

	vbase = tess.numVertexes;

	spanWidth2 = -spanWidth;

	// FIXME: use quad stamp?
	VectorMA( start, spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];// * 0.25;
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];// * 0.25;
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];// * 0.25;
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];// * 0.25;
	tess.numVertexes++;

	VectorMA( start, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 1;
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];// * 0.25;
	tess.numVertexes++;

	VectorMA( end, spanWidth, up, tess.xyz[tess.numVertexes] );

	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = backEnd.currentEntity->e.data.line.stscale;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];// * 0.25;
	tess.numVertexes++;

	VectorMA( end, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 1;
	tess.texCoords[0][tess.numVertexes][1] = backEnd.currentEntity->e.data.line.stscale;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];// * 0.25;
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}

//-----------------
// RB_SurfaceLine
//-----------------
static void RB_SurfaceLine( void )
{
	refEntity_t *e;
	vec3_t		right;
	vec3_t		start, end;
	vec3_t		v1, v2;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, end );
	VectorCopy( e->origin, start );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.ori.origin, v1 );
	VectorSubtract( end, backEnd.viewParms.ori.origin, v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	DoLine( start, end, right, e->radius);
}

static void RB_SurfaceOrientedLine( void )
{
	refEntity_t *e;
	vec3_t		right;
	vec3_t		start, end;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, end );
	VectorCopy( e->origin, start );

	// compute side vector
	VectorNormalize( e->axis[1] );
	VectorCopy(e->axis[1], right);
	DoLine_Oriented( start, end, right, e->data.line.width*0.5 );
}

/*
==============
RB_SurfaceCylinder
==============
*/

#define NUM_CYLINDER_SEGMENTS 40

static void AddCylinderVert( const vec3_t xyz, float s, float t, const byte *rgba )
{
	VectorCopy( xyz, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = s;
	tess.texCoords[0][tess.numVertexes][1] = t;
	tess.vertexColors[tess.numVertexes][0] = rgba[0];
	tess.vertexColors[tess.numVertexes][1] = rgba[1];
	tess.vertexColors[tess.numVertexes][2] = rgba[2];
	tess.vertexColors[tess.numVertexes][3] = rgba[3];
	tess.numVertexes++;
}

// e->origin holds the bottom point
// e->oldorigin holds the top point
// e->radius holds the radius, e->backlerp holds the radius of the other end

// A cylinder with one nearly closed end becomes a cone: better texture mapping, half the indexes.
static void RB_SurfaceCone( void )
{
	static vec3_t points[NUM_CYLINDER_SEGMENTS];
	vec3_t		vr, vu, midpoint;
	vec3_t		tapered, base;
	float		detail, length;
	int			i;
	int			segments;
	refEntity_t *e;

	e = &backEnd.currentEntity->e;

	// Work out the detail level of this cylinder
	VectorAdd( e->origin, e->oldorigin, midpoint );
	VectorScale( midpoint, 0.5f, midpoint );

	VectorSubtract( midpoint, backEnd.viewParms.ori.origin, midpoint );
	length = VectorNormalize( midpoint );

	// A rough compensation for the zoom level is enough
	length *= (backEnd.viewParms.fovX / 90.0f);

	detail = 1 - ((float) length / 2048 );
	segments = NUM_CYLINDER_SEGMENTS * detail;

	if ( segments < 8 )
	{
		segments = 8;
	}

	if ( segments > NUM_CYLINDER_SEGMENTS )
	{
		segments = NUM_CYLINDER_SEGMENTS;
	}

	MakeNormalVectors( e->axis[0], vr, vu );

	// Only the larger radius rotates, the smaller one is welded to a point
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

	detail = 360.0f / (float)segments;

	for ( i = 0; i < segments; i++ )
	{
		RotatePointAroundVector( points[i], e->axis[0], vu, detail * i );
		VectorAdd( points[i], base, points[i] );
	}

	// The texture wraps around the whole cone
	detail = 1.0f / (float)segments;

	RB_CHECKOVERFLOW( 2 * (segments+1), 3 * segments );

	int vbase = tess.numVertexes;

	for ( i = 0; i < segments; i++ )
	{
		AddCylinderVert( points[i], detail * i, 1.0f, e->shaderRGBA );

		// The tip repeats per segment: each needs its own texture coordinates
		AddCylinderVert( tapered, detail * i + detail * 0.5f, 0.0f, e->shaderRGBA );
	}

	// The last pair repeats the first points with the wrapped texture coordinates
	AddCylinderVert( points[0], detail * i, 1.0f, e->shaderRGBA );
	AddCylinderVert( tapered, detail * i + detail * 0.5f, 0.0f, e->shaderRGBA );

	for ( i = 0; i < segments; i++ )
	{
		tess.indexes[tess.numIndexes++] = vbase;
		tess.indexes[tess.numIndexes++] = vbase + 1;
		tess.indexes[tess.numIndexes++] = vbase + 2;

		vbase += 2;
	}
}

static void RB_SurfaceCylinder( void )
{
	static vec3_t lower_points[NUM_CYLINDER_SEGMENTS], upper_points[NUM_CYLINDER_SEGMENTS];
	vec3_t		vr, vu, midpoint, v1;
	float		detail, length;
	int			i;
	int			segments;
	refEntity_t *e;

	e = &backEnd.currentEntity->e;

	// One end nearly closed: draw a cone
	if ( !( e->radius < 0.3f && e->backlerp < 0.3f) && ( e->radius < 0.3f || e->backlerp < 0.3f ))
	{
		RB_SurfaceCone();
		return;
	}

	VectorAdd( e->origin, e->oldorigin, midpoint );
	VectorScale( midpoint, 0.5f, midpoint );

	VectorSubtract( midpoint, backEnd.viewParms.ori.origin, midpoint );
	length = VectorNormalize( midpoint );

	length *= (backEnd.viewParms.fovX / 90.0f);

	detail = 1 - ((float) length / 2048 );
	segments = NUM_CYLINDER_SEGMENTS * detail;

	// Fewer than 8 segments pops visibly
	if ( segments < 8 )
	{
		segments = 8;
	}

	if ( segments > NUM_CYLINDER_SEGMENTS )
	{
		segments = NUM_CYLINDER_SEGMENTS;
	}

	MakeNormalVectors( e->axis[0], vr, vu );

	VectorScale( vu, e->radius, v1 );	// size1
	VectorScale( vu, e->backlerp, vu );	// size2

	detail = 360.0f / (float)segments;

	for ( i = 0; i < segments; i++ )
	{
		RotatePointAroundVector( upper_points[i], e->axis[0], vu, detail * i );
		VectorAdd( upper_points[i], e->origin, upper_points[i] );

		RotatePointAroundVector( lower_points[i], e->axis[0], v1, detail * i );
		VectorAdd( lower_points[i], e->oldorigin, lower_points[i] );
	}

	// The texture wraps around the whole cylinder
	detail = 1.0f / (float)segments;

	RB_CHECKOVERFLOW( 2 * (segments+1), 6 * segments );

	int vbase = tess.numVertexes;

	for ( i = 0; i < segments; i++ )
	{
		AddCylinderVert( upper_points[i], detail * i, 1.0f, e->shaderRGBA );
		AddCylinderVert( lower_points[i], detail * i, 0.0f, e->shaderRGBA );
	}

	// The last pair repeats the first points with the wrapped texture coordinates
	AddCylinderVert( upper_points[0], detail * i, 1.0f, e->shaderRGBA );
	AddCylinderVert( lower_points[0], detail * i, 0.0f, e->shaderRGBA );

	for ( i = 0; i < segments; i++ )
	{
		tess.indexes[tess.numIndexes++] = vbase;
		tess.indexes[tess.numIndexes++] = vbase + 1;
		tess.indexes[tess.numIndexes++] = vbase + 2;

		tess.indexes[tess.numIndexes++] = vbase + 2;
		tess.indexes[tess.numIndexes++] = vbase + 1;
		tess.indexes[tess.numIndexes++] = vbase + 3;

		vbase += 2;
	}
}

static vec3_t sh1, sh2;
static float f_count;

#define LIGHTNING_RECURSION_LEVEL 1 // was 2

// these functions are pretty crappy in terms of returning a nice range of rnd numbers, but it's probably good enough?
/*static int Q_rand( int *seed ) {
	*seed = (69069 * *seed + 1);
	return *seed;
}

static float Q_random( int *seed ) {
	return ( Q_rand( seed ) & 0xffff ) / (float)0x10000;
}

static float Q_crandom( int *seed ) {
	return 2.0F * ( Q_random( seed ) - 0.5f );
}
*/
// Up front, we create a random "shape", then apply that to each line segment...and then again to each of those segments...kind of like a fractal
//----------------------------------------------------------------------------
static void CreateShape()
//----------------------------------------------------------------------------
{
	VectorSet( sh1, 0.66f + Q_flrand(-1.0f, 1.0f) * 0.1f,	// fwd
				0.07f + Q_flrand(-1.0f, 1.0f) * 0.025f,
				0.07f + Q_flrand(-1.0f, 1.0f) * 0.025f );

	// it seems to look best to have a point on one side of the ideal line, then the other point on the other side.
	VectorSet( sh2, 0.33f + Q_flrand(-1.0f, 1.0f) * 0.1f,	// fwd
					-sh1[1] + Q_flrand(-1.0f, 1.0f) * 0.02f,	// forcing point to be on the opposite side of the line -- right
					-sh1[2] + Q_flrand(-1.0f, 1.0f) * 0.02f );// up
}

//----------------------------------------------------------------------------
static void ApplyShape( vec3_t start, vec3_t end, vec3_t right, float sradius, float eradius, int count )
//----------------------------------------------------------------------------
{
	vec3_t	point1, point2, fwd;
	vec3_t	rt, up;
	float	perc, dis;

    if ( count < 1 )
	{
		// done recursing
		DoLine2( start, end, right, sradius, eradius );
		return;
	}

    CreateShape();

	VectorSubtract( end, start, fwd );
	dis = VectorNormalize( fwd ) * 0.7f;
	MakeNormalVectors( fwd, rt, up );

	perc = sh1[0];

	VectorScale( start, perc, point1 );
	VectorMA( point1, 1.0f - perc, end, point1 );
	VectorMA( point1, dis * sh1[1], rt, point1 );
	VectorMA( point1, dis * sh1[2], up, point1 );

	// do a quick and dirty interpolation of the radius at that point
	float rads1, rads2;

	rads1 = sradius * 0.666f + eradius * 0.333f;
	rads2 = sradius * 0.333f + eradius * 0.666f;

	// recursion
    ApplyShape( start, point1, right, sradius, rads1, count - 1 );

	perc = sh2[0];

	VectorScale( start, perc, point2 );
	VectorMA( point2, 1.0f - perc, end, point2 );
	VectorMA( point2, dis * sh2[1], rt, point2 );
	VectorMA( point2, dis * sh2[2], up, point2 );

	// recursion
    ApplyShape( point2, point1, right, rads1, rads2, count - 1 );
	ApplyShape( point2, end, right, rads2, eradius, count - 1 );
}

//----------------------------------------------------------------------------
static void DoBoltSeg( vec3_t start, vec3_t end, vec3_t right, float radius )
//----------------------------------------------------------------------------
{
	refEntity_t *e;
	vec3_t fwd, old;
	vec3_t cur, off={10,10,10};
	vec3_t rt, up;
	vec3_t temp;
	int		i;
    float dis, oldPerc = 0.0f, perc, oldRadius, newRadius;

	e = &backEnd.currentEntity->e;

	VectorSubtract( end, start, fwd );
	dis = VectorNormalize( fwd );

	MakeNormalVectors( fwd, rt, up );

	VectorCopy( start, old );

	oldRadius = newRadius = radius;

    for ( i = 20; i <= dis; i+= 20 )
	{
		// because of our large step size, we may not actually draw to the end.  In this case, fudge our percent so that we are basically complete
		if ( i + 20 > dis )
		{
			perc = 1.0f;
		}
		else
		{
			// percentage of the amount of line completed
			perc = (float)i / dis;
		}

		// create our level of deviation for this point
		VectorScale( fwd, Q_crandom(&e->frame) * 3.0f, temp );				// move less in fwd direction, chaos also does not affect this
		VectorMA( temp, Q_crandom(&e->frame) * 7.0f * e->angles[0], rt, temp );	// move more in direction perpendicular to line, angles is really the chaos
		VectorMA( temp, Q_crandom(&e->frame) * 7.0f * e->angles[0], up, temp );	// move more in direction perpendicular to line

		// track our total level of offset from the ideal line
		VectorAdd( off, temp, off );

        // Move from start to end, always adding our current level of offset from the ideal line
		//	Even though we are adding a random offset.....by nature, we always move from exactly start....to end
		VectorAdd( start, off, cur );
		VectorScale( cur, 1.0f - perc, cur );
		VectorMA( cur, perc, end, cur );

		if ( e->renderfx & RF_TAPERED )
		{
			// This does pretty close to perfect tapering since apply shape interpolates the old and new as it goes along.
			//	by using one minus the square, the radius stays fairly constant, then drops off quickly at the very point of the bolt
			oldRadius = radius * (1.0f-oldPerc*oldPerc);
			newRadius = radius * (1.0f-perc*perc);
		}

		// Apply the random shape to our line seg to give it some micro-detail-jaggy-coolness.
        ApplyShape( cur, old, right, newRadius, oldRadius, LIGHTNING_RECURSION_LEVEL );

		// randomly split off to create little tendrils, but don't do it too close to the end and especially if we are not even of the forked variety
        if ( ( e->renderfx & RF_FORKED ) && f_count > 0 && Q_random(&e->frame) > 0.94f && radius * (1.0f - perc) > 0.2f )
		{
			vec3_t newDest;

			f_count--;

			// Pick a point somewhere between the current point and the final endpoint
			VectorAdd( cur, e->oldorigin, newDest );
			VectorScale( newDest, 0.5f, newDest );

			// And then add some crazy offset
			for ( int t = 0; t < 3; t++ )
			{
				newDest[t] += Q_crandom(&e->frame) * 80;
			}

			// we could branch off using OLD and NEWDEST, but that would allow multiple forks...whereas, we just want simpler brancing
            DoBoltSeg( cur, newDest, right, newRadius );
		}

		// Current point along the line becomes our new old attach point
		VectorCopy( cur, old );
		oldPerc = perc;
	}
}

//------------------------------------------
static void RB_SurfaceElectricity()
//------------------------------------------
{
	refEntity_t *e;
	vec3_t		right, fwd;
	vec3_t		start, end;
	vec3_t		v1, v2;
	float		radius, perc = 1.0f, dis;

	e = &backEnd.currentEntity->e;
	radius = e->radius;

	VectorCopy( e->origin, start );

	VectorSubtract( e->oldorigin, start, fwd );
	dis = VectorNormalize( fwd );

	// see if we should grow from start to end
	if ( e->renderfx & RF_GROW )
	{
		perc = 1.0f - ( e->endTime - backEnd.refdef.time ) / e->angles[1]/*duration*/;

		if ( perc > 1.0f )
		{
			perc = 1.0f;
		}
		else if ( perc < 0.0f )
		{
			perc = 0.0f;
		}
	}

	VectorMA( start, perc * dis, fwd, e->oldorigin );
	VectorCopy( e->oldorigin, end );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.ori.origin, v1 );
	VectorSubtract( end, backEnd.viewParms.ori.origin, v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

    DoBoltSeg( start, end, right, radius );
}

//================================================================================

/*
** VectorArrayNormalize
*
* The inputs to this routing seem to always be close to length = 1.0 (about 0.6 to 2.0)
* This means that we don't have to worry about zero length or enormously long vectors.
*/
static void VectorArrayNormalize( vec4_t *normals, unsigned int count )
{
//    assert(count);

#if idppc
    {
        register float half = 0.5;
        register float one  = 1.0;
        float *components = (float *)normals;

        // Vanilla PPC code, but since PPC has a reciprocal square root estimate instruction,
        // runs *much* faster than calling sqrt().  We'll use a single Newton-Raphson
        // refinement step to get a little more precision.  This seems to yeild results
        // that are correct to 3 decimal places and usually correct to at least 4 (sometimes 5).
        // (That is, for the given input range of about 0.6 to 2.0).
        do {
            float x, y, z;
            float B, y0, y1;

            x = components[0];
            y = components[1];
            z = components[2];
            components += 4;
            B = x*x + y*y + z*z;

#ifdef __GNUC__
            asm("frsqrte %0,%1" : "=f" (y0) : "f" (B));
#else
			y0 = __frsqrte(B);
#endif
            y1 = y0 + half*y0*(one - B*y0*y0);

            x = x * y1;
            y = y * y1;
            components[-4] = x;
            z = z * y1;
            components[-3] = y;
            components[-2] = z;
        } while(count--);
    }
#else // No assembly version for this architecture, or C_ONLY defined
	// given the input, it's safe to call VectorNormalizeFast
    while (count--) {
        VectorNormalizeFast(normals[0]);
        normals++;
    }
#endif

}

/*
** LerpMeshVertexes
*/
#if 0
static void LerpMeshVertexes_ ( md3Surface_t *surf, float backlerp )
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	float	*outXyz, *outNormal;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	outXyz = tess.xyz[tess.numVertexes];
	outNormal = tess.normal[tess.numVertexes];

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			newXyz += 4, newNormals += 4,
			outXyz += 4, outNormal += 4)
		{

			outXyz[0] = newXyz[0] * newXyzScale;
			outXyz[1] = newXyz[1] * newXyzScale;
			outXyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )
			outNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			outNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			outXyz += 4, outNormal += 4)
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			outXyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			outXyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			outXyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			outNormal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			outNormal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			outNormal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

//			VectorNormalize (outNormal);
		}
    	VectorArrayNormalize((vec4_t *)tess.normal[tess.numVertexes], numVerts);
   	}
}
#endif

static void VectorLerp( vec3_t a, vec3_t b, float lerp, vec3_t c)
{
	c[0] = a[0] * (1.0f - lerp) + b[0] * lerp;
	c[1] = a[1] * (1.0f - lerp) + b[1] * lerp;
	c[2] = a[2] * (1.0f - lerp) + b[2] * lerp;
}

static void LerpMeshVertexes_scalar(mdvSurface_t *surf, float backlerp)
{
#if 0
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	float	*outXyz, *outNormal;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	outXyz = tess.xyz[tess.numVertexes];
	outNormal = tess.normal[tess.numVertexes];

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			newXyz += 4, newNormals += 4,
			outXyz += 4, outNormal += 4)
		{

			outXyz[0] = newXyz[0] * newXyzScale;
			outXyz[1] = newXyz[1] * newXyzScale;
			outXyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			outNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			outNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			outXyz += 4, outNormal += 4)
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			outXyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			outXyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			outXyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			outNormal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			outNormal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			outNormal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

//			VectorNormalize (outNormal);
		}
		VectorArrayNormalize((vec4_t *)tess.normal[tess.numVertexes], numVerts);
		}
#endif
	float *outXyz;
	float *outNormal;
	mdvVertex_t *newVerts;
	int		vertNum;

	newVerts = surf->verts + backEnd.currentEntity->e.frame * surf->numVerts;

	outXyz =    tess.xyz[tess.numVertexes];
	outNormal = tess.normal[tess.numVertexes];

	if (backlerp == 0)
	{
		//
		// just copy the vertexes
		//

		for (vertNum=0 ; vertNum < surf->numVerts ; vertNum++)
		{
			//vec3_t normal;

			VectorCopy(newVerts->xyz,    outXyz);

			VectorCopy(newVerts->normal, outNormal);
			//VectorCopy(newVerts->normal, normal);
			//*outNormal = R_VboPackNormal(normal);

			newVerts++;
			outXyz += 4;
			outNormal +=4;
		}
	}
	else
	{
		//
		// interpolate and copy the vertex and normal
		//

		mdvVertex_t *oldVerts;

		oldVerts = surf->verts + backEnd.currentEntity->e.oldframe * surf->numVerts;

		for (vertNum=0 ; vertNum < surf->numVerts ; vertNum++)
		{
			//vec3_t normal;

			VectorLerp(newVerts->xyz,    oldVerts->xyz,    backlerp, outXyz);
			VectorLerp(newVerts->normal, oldVerts->normal, backlerp, outNormal);
			VectorNormalize(outNormal);


			//VectorLerp(newVerts->normal, oldVerts->normal, backlerp, normal);
			//VectorNormalize(normal);
			//*outNormal = R_VboPackNormal(normal);

			newVerts++;
			oldVerts++;
			outXyz += 4;
			outNormal += 4;
		}
	}

}

static void LerpMeshVertexes(mdvSurface_t *surf, float backlerp)
{
#if 0
#if idppc_altivec
	if (com_altivec->integer) {
		// must be in a seperate function or G3 systems will crash.
		LerpMeshVertexes_altivec( surf, backlerp );
		return;
	}
#endif // idppc_altivec
#endif
	LerpMeshVertexes_scalar( surf, backlerp );
}

/*
=============
RB_SurfaceMesh
=============
*/
void RB_SurfaceMesh( mdvSurface_t *surface ) {
	uint32_t		j;
	float			backlerp;
	mdvSt_t			*texCoords;
	int				Bob, Doug;
	int				numVerts;

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

#ifdef USE_VBO
	VBO_Flush();
	tess.surfType = SF_MDV;
#endif

	RB_CHECKOVERFLOW( surface->numVerts, surface->numIndexes );

	LerpMeshVertexes (surface, backlerp);

	Bob = tess.numIndexes;
	Doug = tess.numVertexes;
	for (j = 0 ; j < surface->numIndexes ; j++) {
		tess.indexes[Bob + j] = Doug + surface->indexes[j];
	}
	tess.numIndexes += surface->numIndexes;

	texCoords = surface->st;

	numVerts = surface->numVerts;
	for ( j = 0; j < numVerts; j++ ) {
		tess.texCoords[0][Doug + j][0] = texCoords[j].st[0];
		tess.texCoords[0][Doug + j][1] = texCoords[j].st[1];
		// FIXME: fill in lightmapST for completeness?
	}

	tess.numVertexes += surface->numVerts;
}

/*
==============
RB_SurfaceFace
==============
*/
void RB_SurfaceFace( srfSurfaceFace_t *surf ) {
	int			i, j, k;
	unsigned int *indices;
	glIndex_t	*tessIndexes;
	float		*v;
	float		*normal;
	int			ndx;
	int			Bob;
	int			numPoints;
	byteAlias_t	ba;

#ifdef USE_VBO
	if (tess.allowVBO && surf->vboItemIndex) {
		// transition to vbo render list
		if (tess.vbo_world_index == 0) {
			RB_EndSurface();
			RB_BeginSurface(tess.shader, tess.fogNum);
			// set some dummy parameters for RB_EndSurface
			tess.numIndexes = 1;
			tess.numVertexes = 0;
			VBO_ClearQueue();
		}
		tess.surfType = SF_FACE;
		tess.vbo_world_index = surf->vboItemIndex;

		VBO_QueueItem(surf->vboItemIndex);
		return; // no need to tesselate anything
	}

	VBO_Flush();
#endif // USE_VBO

	RB_CHECKOVERFLOW( surf->numPoints, surf->numIndices );

	indices = ( unsigned * ) ( ( ( char  * ) surf ) + surf->ofsIndices );

	Bob = tess.numVertexes;
	tessIndexes = tess.indexes + tess.numIndexes;
	for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
		tessIndexes[i] = indices[i] + Bob;
	}

	tess.numIndexes += surf->numIndices;

	v = surf->points[0];

	ndx = tess.numVertexes;

	numPoints = surf->numPoints;

	{
		if ( surf->normals ) {
			// per-vertex normals for non-coplanar faces
			memcpy( &tess.normal[ tess.numVertexes ], surf->normals, numPoints * sizeof( vec4_t ) );
		} else {
			normal = surf->plane.normal;
			for ( i = 0, ndx = tess.numVertexes; i < numPoints; i++, ndx++ ) {
				VectorCopy( normal, tess.normal[ndx] );
			}
		}
	}

#ifdef USE_VK_PBR
	memcpy( &tess.qtangent[ tess.numVertexes ], surf->qtangents, numPoints * sizeof( vec4_t ) );
	if ( surf->lightdir )
		memcpy( &tess.lightdir[ tess.numVertexes ], surf->lightdir, numPoints * sizeof( vec4_t ) );
#endif

	for ( i = 0, v = surf->points[0], ndx = tess.numVertexes; i < numPoints; i++, v += VERTEXSIZE, ndx++ )
	{
		VectorCopy( v, tess.xyz[ndx]);
		tess.texCoords[0][ndx][0] = v[3];
		tess.texCoords[0][ndx][1] = v[4];
		
		for(k=0;k<MAXLIGHTMAPS;k++)
		{
			if (tess.shader->lightmapIndex[k] >= 0)
			{
				tess.texCoords[k+1][ndx][0] = v[VERTEX_LM+(k*2)];
				tess.texCoords[k+1][ndx][1] = v[VERTEX_LM+(k*2)+1];
			}
			else
			{
				break;
			}
		}

		/*if (tess.shader->lightmapIndex[0] >= 0)
		{
			tess.texCoords[1][ndx][0] = v[5];
			tess.texCoords[1][ndx][1] = v[6];
		}*/

		ba.ui = ComputeFinalVertexColor( (byte *)&v[VERTEX_COLOR] );
		for ( j=0; j<4; j++ )
			tess.vertexColors[ndx][j] = ba.b[j];
	}

	tess.numVertexes += surf->numPoints;
}

static float	LodErrorForVolume( vec3_t local, float radius ) {
	vec3_t		world;
	float		d;

	// never let it go negative
	if ( r_lodCurveError->value < 0 ) {
		return 0;
	}

	world[0] = local[0] * backEnd.ori.axis[0][0] + local[1] * backEnd.ori.axis[1][0] +
		local[2] * backEnd.ori.axis[2][0] + backEnd.ori.origin[0];
	world[1] = local[0] * backEnd.ori.axis[0][1] + local[1] * backEnd.ori.axis[1][1] +
		local[2] * backEnd.ori.axis[2][1] + backEnd.ori.origin[1];
	world[2] = local[0] * backEnd.ori.axis[0][2] + local[1] * backEnd.ori.axis[1][2] +
		local[2] * backEnd.ori.axis[2][2] + backEnd.ori.origin[2];

	VectorSubtract( world, backEnd.viewParms.ori.origin, world );
	d = DotProduct( world, backEnd.viewParms.ori.axis[0] );

	if ( d < 0 ) {
		d = -d;
	}
	d -= radius;
	if ( d < 1 ) {
		d = 1;
	}

	return r_lodCurveError->value / d;
}

#ifdef USE_VBO_GRID
void RB_SurfaceGridEstimate( srfGridMesh_t *cv, int *numVertexes, int *numIndexes )
{
	int		lodWidth, lodHeight;
	float	lodError;
	int		i, used, rows;
	int		nVertexes = 0;
	int		nIndexes = 0;
	int		irows, vrows;

	lodError = r_lodCurveError->value; // fixed quality for VBO

	lodWidth = 1;
	for (i = 1; i < cv->width - 1; i++) {
		if (cv->widthLodError[i] <= lodError) {
			lodWidth++;
		}
	}
	lodWidth++;

	lodHeight = 1;
	for (i = 1; i < cv->height - 1; i++) {
		if (cv->heightLodError[i] <= lodError) {
			lodHeight++;
		}
	}
	lodHeight++;

	used = 0;
	while (used < lodHeight - 1) {
		// see how many rows of both verts and indexes we can add without overflowing
		do {
			vrows = (SHADER_MAX_VERTEXES - tess.numVertexes) / lodWidth;
			irows = (SHADER_MAX_INDEXES - tess.numIndexes) / (lodWidth * 6);

			// if we don't have enough space for at least one strip, flush the buffer
			if (vrows < 2 || irows < 1) {
				nVertexes += tess.numVertexes;
				nIndexes += tess.numIndexes;
				tess.numIndexes = 0;
				tess.numVertexes = 0;
			}
			else {
				break;
			}
		} while (1);

		rows = irows;
		if (vrows < irows + 1) {
			rows = vrows - 1;
		}
		if (used + rows > lodHeight) {
			rows = lodHeight - used;
		}

		tess.numIndexes += (rows - 1) * (lodWidth - 1) * 6;
		tess.numVertexes += rows * lodWidth;
		used += rows - 1;
	}

	*numVertexes = nVertexes + tess.numVertexes;
	*numIndexes = nIndexes + tess.numIndexes;
	tess.numVertexes = 0;
	tess.numIndexes = 0;
}
#endif // USE_VBO_GRID

/*
=============
RB_SurfaceGrid

Just copy the grid of points and triangulate
=============
*/
void RB_SurfaceGrid( srfGridMesh_t *cv ) {
	int		i, j;
	float	*xyz, *normal;
	float	*texCoords0, *texCoords1, *texCoords2, *texCoords3, *texCoords4;
	unsigned char *color;
	srfVert_t	*dv;
	int		rows, irows, vrows;
	int		used;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		numVertexes;

#ifdef USE_VBO_GRID
	if (tess.allowVBO && cv->vboItemIndex) {
		// transition to vbo render list
		if (tess.vbo_world_index == 0) {
			RB_EndSurface();
			RB_BeginSurface(tess.shader, tess.fogNum);
			// set some dummy parameters for RB_EndSurface
			tess.numIndexes = 1;
			tess.numVertexes = 0;
			VBO_ClearQueue();
		}
		tess.surfType = SF_GRID;
		tess.vbo_world_index = cv->vboItemIndex;

		VBO_QueueItem(cv->vboItemIndex);
		return; // no need to tesselate anything
	}

	VBO_Flush();
#else
#ifdef USE_VBO
	VBO_Flush();
#endif
#endif

#ifdef USE_VBO_GRID
	tess.surfType = SF_GRID;

	// determine the allowable discrepance
#ifdef USE_PMLIGHT
	if (cv->vboItemIndex && (tr.mapLoading || (tess.dlightPass && tess.shader->isStaticShader)))
#else
	if (cv->vboItemIndex && tr.mapLoading)
#endif
		lodError = r_lodCurveError->value; // fixed quality for VBO
	else
#endif // USE_VBO_GRID
		lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius ); // determine the allowable discrepance

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;

	// very large grids may have more points or indexes than can be fit
	// in the tess structure, so we may have to issue it in multiple passes

	used = 0;
	rows = 0;
	while ( used < lodHeight - 1 ) {
		// see how many rows of both verts and indexes we can add without overflowing
		do {
			vrows = ( SHADER_MAX_VERTEXES - tess.numVertexes ) / lodWidth;
			irows = ( SHADER_MAX_INDEXES - tess.numIndexes ) / ( lodWidth * 6 );

			// if we don't have enough space for at least one strip, flush the buffer
			if ( vrows < 2 || irows < 1 ) {
				if (tr.mapLoading) {
					// estimate and flush
#ifdef USE_VBO_GRID
					if (cv->vboItemIndex) {
						VBO_PushData( cv->vboItemIndex, &tess );
						tess.numIndexes = 0;
						tess.numVertexes = 0;
					}
					else
#endif // USE_VBO_GRID
						ri.Error(ERR_DROP, "Unexpected grid flush during map loading!\n");
				}
				else {
					RB_EndSurface();
					RB_BeginSurface(tess.shader, tess.fogNum);
				}
			} else {
				break;
			}
		} while ( 1 );

		rows = irows;
		if ( vrows < irows + 1 ) {
			rows = vrows - 1;
		}
		if ( used + rows > lodHeight ) {
			rows = lodHeight - used;
		}

		numVertexes = tess.numVertexes;

		xyz = tess.xyz[numVertexes];
		normal = tess.normal[numVertexes];
		texCoords0 = tess.texCoords[0][numVertexes];
		texCoords1 = tess.texCoords[1][numVertexes];
		texCoords2 = tess.texCoords[2][numVertexes];
		texCoords3 = tess.texCoords[3][numVertexes];
		texCoords4 = tess.texCoords[4][numVertexes];
		color = ( unsigned char * ) &tess.vertexColors[numVertexes];

		for ( i = 0 ; i < rows ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++ ) {
				dv = cv->verts + heightTable[ used + i ] * cv->width
					+ widthTable[ j ];

				xyz[0] = dv->xyz[0];
				xyz[1] = dv->xyz[1];
				xyz[2] = dv->xyz[2];
				xyz += 4;

				texCoords0[0] = dv->st[0];
				texCoords0[1] = dv->st[1];
				texCoords0 += 2;

				// Lightmaps, convert this to a for loop.
				{
					texCoords1[0] = dv->lightmap[0][0];
					texCoords1[1] = dv->lightmap[0][1];
					texCoords1 += 2;
				}
				{
					texCoords2[0] = dv->lightmap[1][0];
					texCoords2[1] = dv->lightmap[1][1];
					texCoords2 += 2;
				}
				{
					texCoords3[0] = dv->lightmap[2][0];
					texCoords3[1] = dv->lightmap[2][1];
					texCoords3 += 2;
				}
				{
					texCoords4[0] = dv->lightmap[3][0];
					texCoords4[1] = dv->lightmap[3][1];
					texCoords4 += 2;
				}

				{
					normal[0] = dv->normal[0];
					normal[1] = dv->normal[1];
					normal[2] = dv->normal[2];
				}
				normal += 4;

#ifdef USE_VK_PBR
				VectorCopy4( dv->qtangent, tess.qtangent[numVertexes + i * lodWidth + j] );
				VectorCopy4( dv->lightdir, tess.lightdir[numVertexes + i * lodWidth + j] );
#endif

				*(unsigned *)color = ComputeFinalVertexColor((byte *)dv->color);
				color += 4;
			}
		}

		// add the indexes
		{
			int		numIndexes;
			int		w, h;

			h = rows - 1;
			w = lodWidth - 1;
			numIndexes = tess.numIndexes;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;

					// vertex order to be reckognized as tristrips
					v1 = numVertexes + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;

					tess.indexes[numIndexes] = v2;
					tess.indexes[numIndexes+1] = v3;
					tess.indexes[numIndexes+2] = v1;

					tess.indexes[numIndexes+3] = v1;
					tess.indexes[numIndexes+4] = v3;
					tess.indexes[numIndexes+5] = v4;
					numIndexes += 6;
				}
			}

			tess.numIndexes = numIndexes;
		}

		tess.numVertexes += rows * lodWidth;

		used += rows - 1;
	}
}

/*
===========================================================================

NULL MODEL

===========================================================================
*/

/*
===================
RB_SurfaceAxis

Draws x/y/z lines from the origin for orientation debugging
===================
*/
static void RB_SurfaceAxis( void ) {
	int i;

	RB_EndSurface();

	vk_bind(tr.whiteImage);
	Com_Memset(tess.xyz, 0, 6 * sizeof(tess.xyz[0]));
	tess.xyz[1][0] = 16.0;
	tess.xyz[3][1] = 16.0;
	tess.xyz[5][2] = 16.0;

	Com_Memset(tess.svars.colors[0], 0, 6 * sizeof(color4ub_t));
	for (i = 0; i < 6; i++)
		tess.svars.colors[0][i][3] = 255;

	tess.svars.colors[0][0][0] = 255;
	tess.svars.colors[0][1][0] = 255;
	tess.svars.colors[0][2][1] = 255;
	tess.svars.colors[0][3][1] = 255;
	tess.svars.colors[0][4][2] = 255;
	tess.svars.colors[0][5][2] = 255;

	tess.numVertexes = 6;

	vk_bind_pipeline(vk.std_pipeline.surface_axis_pipeline);
	// TODO: use common layout and avoid ST0 binding?
	vk_bind_geometry(TESS_XYZ | TESS_RGBA0 | TESS_ST0);
	vk_draw_geometry(DEPTH_RANGE_NORMAL, qfalse);

	tess.numVertexes = 0;
}

//===========================================================================

// Ported from code/rd-vanilla/tr_surface.cpp. Neither touches GL - they only fill tess -
// so they carry over unchanged. Without them RT_LATHE and RT_CLOUDS fell through to the
// default case, which draws debug axes and calls RB_EndSurface() mid-batch.
#define LATHE_SEG_STEP	10
#define BEZIER_STEP		0.05f	// must be in the range of 0 to 1

// FIXME: This function is horribly expensive
static void RB_SurfaceLathe()
{
	refEntity_t *e;
	vec2_t		pt, oldpt, l_oldpt;
	vec2_t		pt2, oldpt2, l_oldpt2;
	float		bezierStep, latheStep;
    float		temp, mu, mum1;
	float		mum13, mu3, group1, group2;
	float		s, c, d = 1.0f, pain = 0.0f;
	int			i, t, vbase;

	e = &backEnd.currentEntity->e;

	if ( e->endTime && e->endTime > backEnd.refdef.time )
	{
		d = 1.0f - ( e->endTime - backEnd.refdef.time ) / 1000.0f;
	}

	if ( e->frame && e->frame + 1000 > backEnd.refdef.time )
	{
		pain = ( backEnd.refdef.time - e->frame ) / 1000.0f;
//		pain *= pain;
		pain = ( 1.0f - pain ) * 0.08f;
	}

	VectorSet2( l_oldpt, e->axis[0][0], e->axis[0][1] );

	// do scalability stuff...r_lodbias 0-3
	int lod = r_lodbias->integer + 1;
	if ( lod > 4 )
	{
		lod = 4;
	}
	if ( lod < 1 )
	{
		lod = 1;
	}
	bezierStep = BEZIER_STEP * lod;
	latheStep = LATHE_SEG_STEP * lod;

	// Do bezier profile strip, then lathe this around to make a 3d model
	for ( mu = 0.0f; mu <= 1.01f * d; mu += bezierStep )
	{
		// Four point curve
		mum1	= 1 - mu;
		mum13	= mum1 * mum1 * mum1;
		mu3		= mu * mu * mu;
		group1	= 3 * mu * mum1 * mum1;
		group2	= 3 * mu * mu *mum1;

		// Calc the current point on the curve
		for ( i = 0; i < 2; i++ )
		{
			l_oldpt2[i] = mum13 * e->axis[0][i] + group1 * e->axis[1][i] + group2 * e->axis[2][i] + mu3 * e->oldorigin[i];
		}

		VectorSet2( oldpt, l_oldpt[0], 0 );
		VectorSet2( oldpt2, l_oldpt2[0], 0 );

		// lathe patch section around in a complete circle
		for ( t = latheStep; t <= 360; t += latheStep )
		{
			VectorSet2( pt, l_oldpt[0], 0 );
			VectorSet2( pt2, l_oldpt2[0], 0 );

			s = sin( DEG2RAD( t ));
			c = cos( DEG2RAD( t ));

			// rotate lathe points
//c -s 0
//s  c 0
//0  0 1
			temp = c * pt[0] - s * pt[1];
			pt[1] = s * pt[0] + c * pt[1];
			pt[0] = temp;
			temp = c * pt2[0] - s * pt2[1];
			pt2[1] = s * pt2[0] + c * pt2[1];
			pt2[0] = temp;

			RB_CHECKOVERFLOW( 4, 6 );

			vbase = tess.numVertexes;

			// Actually generate the necessary verts
			VectorSet( tess.normal[tess.numVertexes], oldpt[0], oldpt[1], l_oldpt[1] );
			VectorAdd( e->origin, tess.normal[tess.numVertexes], tess.xyz[tess.numVertexes] );
			VectorNormalize( tess.normal[tess.numVertexes] );
			i = oldpt[0] * 0.1f + oldpt[1] * 0.1f;
			tess.texCoords[0][tess.numVertexes][0] = (t-latheStep)/360.0f;
			tess.texCoords[0][tess.numVertexes][1] = mu-bezierStep + cos( i + backEnd.refdef.floatTime ) * pain;
			tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
			tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorSet( tess.normal[tess.numVertexes], oldpt2[0], oldpt2[1], l_oldpt2[1] );
			VectorAdd( e->origin, tess.normal[tess.numVertexes], tess.xyz[tess.numVertexes] );
			VectorNormalize( tess.normal[tess.numVertexes] );
			i = oldpt2[0] * 0.1f + oldpt2[1] * 0.1f;
			tess.texCoords[0][tess.numVertexes][0] = (t-latheStep) / 360.0f;
			tess.texCoords[0][tess.numVertexes][1] = mu + cos( i + backEnd.refdef.floatTime ) * pain;
			tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
			tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorSet( tess.normal[tess.numVertexes], pt[0], pt[1], l_oldpt[1] );
			VectorAdd( e->origin, tess.normal[tess.numVertexes], tess.xyz[tess.numVertexes] );
			VectorNormalize( tess.normal[tess.numVertexes] );
			i = pt[0] * 0.1f + pt[1] * 0.1f;
			tess.texCoords[0][tess.numVertexes][0] = t/360.0f;
			tess.texCoords[0][tess.numVertexes][1] = mu-bezierStep + cos( i + backEnd.refdef.floatTime ) * pain;
			tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
			tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorSet( tess.normal[tess.numVertexes], pt2[0], pt2[1], l_oldpt2[1] );
			VectorAdd( e->origin, tess.normal[tess.numVertexes], tess.xyz[tess.numVertexes] );
			VectorNormalize( tess.normal[tess.numVertexes] );
			i = pt2[0] * 0.1f + pt2[1] * 0.1f;
			tess.texCoords[0][tess.numVertexes][0] = t/360.0f;
			tess.texCoords[0][tess.numVertexes][1] = mu + cos( i + backEnd.refdef.floatTime ) * pain;
			tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
			tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			tess.indexes[tess.numIndexes++] = vbase;
			tess.indexes[tess.numIndexes++] = vbase + 1;
			tess.indexes[tess.numIndexes++] = vbase + 3;

			tess.indexes[tess.numIndexes++] = vbase + 3;
			tess.indexes[tess.numIndexes++] = vbase + 2;
			tess.indexes[tess.numIndexes++] = vbase;

			// Shuffle new points to old
			VectorCopy2( pt, oldpt );
			VectorCopy2( pt2, oldpt2 );
		}

		// shuffle lathe points
		VectorCopy2( l_oldpt2, l_oldpt );
	}
}

#define DISK_DEF	4
#define TUBE_DEF	6

static void RB_SurfaceClouds()
{
	// Disk definition
	float diskStripDef[DISK_DEF] = {
				0.0f,
				0.4f,
				0.7f,
				1.0f };

	float diskAlphaDef[DISK_DEF] = {
				1.0f,
				1.0f,
				0.4f,
				0.0f };

	float diskCurveDef[DISK_DEF] = {
				0.0f,
				0.0f,
				0.008f,
				0.02f };

	// tube definition
	float tubeStripDef[TUBE_DEF] = {
				0.0f,
				0.05f,
				0.1f,
				0.5f,
				0.7f,
				1.0f };

	float tubeAlphaDef[TUBE_DEF] = {
				0.0f,
				0.45f,
				1.0f,
				1.0f,
				0.45f,
				0.0f };

	float tubeCurveDef[TUBE_DEF] = {
				0.0f,
				0.004f,
				0.006f,
				0.01f,
				0.006f,
				0.0f };

	refEntity_t *e;
	vec3_t		pt, oldpt;
	vec3_t		pt2, oldpt2;
	float		latheStep = 30.0f;
	float		s, c, temp;
	float		*stripDef, *alphaDef, *curveDef, ct;
	int			i, t, vbase;

	e = &backEnd.currentEntity->e;

	// select which type we shall be doing
	if ( e->renderfx & RF_GROW ) // doing tube type
	{
		ct = TUBE_DEF;
		stripDef = tubeStripDef;
		alphaDef = tubeAlphaDef;
		curveDef = tubeCurveDef;
		e->backlerp *= -1; // needs to be reversed
	}
	else
	{
		ct = DISK_DEF;
		stripDef = diskStripDef;
		alphaDef = diskAlphaDef;
		curveDef = diskCurveDef;
	}

	// do the strip def, then lathe this around to make a 3d model
	for ( i = 0; i < ct - 1; i++ )
	{
		VectorSet( oldpt,	(stripDef[i]	* (e->radius - e->rotation)) + e->rotation,	0, curveDef[i] * e->radius * e->backlerp );
		VectorSet( oldpt2,	(stripDef[i+1]	* (e->radius - e->rotation)) + e->rotation,	0, curveDef[i+1] * e->radius * e->backlerp );

		// lathe section around in a complete circle
		for ( t = latheStep; t <= 360; t += latheStep )
		{
			// rotate every time except last seg
			if ( t < 360.0f )
			{
				VectorCopy( oldpt, pt );
				VectorCopy( oldpt2, pt2 );

				s = sin( DEG2RAD( latheStep ));
				c = cos( DEG2RAD( latheStep ));

				// rotate lathe points
				temp = c * pt[0] - s * pt[1];	// c -s 0
				pt[1] = s * pt[0] + c * pt[1];	// s  c 0
				pt[0] = temp;					// 0  0 1

				temp = c * pt2[0] - s * pt2[1];	 // c -s 0
				pt2[1] = s * pt2[0] + c * pt2[1];// s  c 0
				pt2[0] = temp;					 // 0  0 1
			}
			else
			{
				// just glue directly to the def points.
				VectorSet( pt,	(stripDef[i]	* (e->radius - e->rotation)) + e->rotation,	0, curveDef[i] * e->radius * e->backlerp );
				VectorSet( pt2,	(stripDef[i+1]	* (e->radius - e->rotation)) + e->rotation,	0, curveDef[i+1] * e->radius * e->backlerp );
			}

			RB_CHECKOVERFLOW( 4, 6 );

			vbase = tess.numVertexes;

			// Actually generate the necessary verts
			VectorAdd( e->origin, oldpt, tess.xyz[tess.numVertexes] );
			tess.texCoords[0][tess.numVertexes][0] = tess.xyz[tess.numVertexes][0] * 0.1f;
			tess.texCoords[0][tess.numVertexes][1] = tess.xyz[tess.numVertexes][1] * 0.1f;
			tess.vertexColors[tess.numVertexes][0] =
			tess.vertexColors[tess.numVertexes][1] =
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[0] * alphaDef[i];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorAdd( e->origin, oldpt2, tess.xyz[tess.numVertexes] );
			tess.texCoords[0][tess.numVertexes][0] = tess.xyz[tess.numVertexes][0] * 0.1f;
			tess.texCoords[0][tess.numVertexes][1] = tess.xyz[tess.numVertexes][1] * 0.1f;
			tess.vertexColors[tess.numVertexes][0] =
			tess.vertexColors[tess.numVertexes][1] =
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[0] * alphaDef[i+1];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorAdd( e->origin, pt, tess.xyz[tess.numVertexes] );
			tess.texCoords[0][tess.numVertexes][0] = tess.xyz[tess.numVertexes][0] * 0.1f;
			tess.texCoords[0][tess.numVertexes][1] = tess.xyz[tess.numVertexes][1] * 0.1f;
			tess.vertexColors[tess.numVertexes][0] =
			tess.vertexColors[tess.numVertexes][1] =
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[0] * alphaDef[i];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorAdd( e->origin, pt2, tess.xyz[tess.numVertexes] );
			tess.texCoords[0][tess.numVertexes][0] = tess.xyz[tess.numVertexes][0] * 0.1f;
			tess.texCoords[0][tess.numVertexes][1] = tess.xyz[tess.numVertexes][1] * 0.1f;
			tess.vertexColors[tess.numVertexes][0] =
			tess.vertexColors[tess.numVertexes][1] =
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[0] * alphaDef[i+1];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			tess.indexes[tess.numIndexes++] = vbase;
			tess.indexes[tess.numIndexes++] = vbase + 1;
			tess.indexes[tess.numIndexes++] = vbase + 3;

			tess.indexes[tess.numIndexes++] = vbase + 3;
			tess.indexes[tess.numIndexes++] = vbase + 2;
			tess.indexes[tess.numIndexes++] = vbase;

			// Shuffle new points to old
			VectorCopy2( pt, oldpt );
			VectorCopy2( pt2, oldpt2 );
		}
	}
}

/*
====================
RB_SurfaceEntity

Entities that have a single procedurally generated surface
====================
*/
void RB_SurfaceEntity( const surfaceType_t *surfType ) {
#ifdef USE_VBO
	VBO_Flush();
#endif
	qboolean allow_merge = qtrue;

	switch( backEnd.currentEntity->e.reType ) {
	case RT_SPRITE:
		RB_SurfaceSprite();
		break;
	case RT_ORIENTED_QUAD:
		RB_SurfaceOrientedQuad();
		break;
	case RT_BEAM:
		allow_merge = qfalse;
		RB_SurfaceBeam();
		break;
	case RT_ELECTRICITY:
		RB_SurfaceElectricity();
		break;
	case RT_LINE:
		RB_SurfaceLine();
		break;
	case RT_ORIENTEDLINE:
		RB_SurfaceOrientedLine();
		break;
	case RT_SABER_GLOW:
		RB_SurfaceSaberGlow();
		break;
	case RT_CYLINDER:
		RB_SurfaceCylinder();
		break;
	case RT_LATHE:
		RB_SurfaceLathe();
		break;
	case RT_CLOUDS:
		RB_SurfaceClouds();
		break;
	case RT_ENT_CHAIN:
		{
			static trRefEntity_t	tempEnt = *backEnd.currentEntity;
			//rww - if not static then currentEntity is garbage because
			//this is a local. This was not static in sof2.. but I guess
			//they never check ce.renderfx so it didn't show up.

			const int start = backEnd.currentEntity->e.uRefEnt.uMini.miniStart;
			const int count = backEnd.currentEntity->e.uRefEnt.uMini.miniCount;
			assert( count > 0 );
			backEnd.currentEntity = &tempEnt;

			assert( backEnd.currentEntity->e.renderfx >= 0 );

			for ( int i = 0, j = start; i < count; i++, j++ )
			{
				backEnd.currentEntity->e = backEnd.refdef.entities[j].e;

				assert(backEnd.currentEntity->e.renderfx >= 0);

				RB_SurfaceEntity(surfType);
			}

			allow_merge = qfalse;
		}
		break;
	default:
		allow_merge = qfalse;
		RB_SurfaceAxis();
		break;
	}

#ifdef USE_VBO
	tess.surfType = SF_ENTITY;
#endif

	// FIX ME: just a testing hack. Pretty sure we can merge all of these
	tess.entityMergable = (allow_merge && tess.shader != tr.defaultShader)  ? true : false;
	return;
}

void RB_SurfaceBad( const surfaceType_t *surfType ) {
	ri.Printf( PRINT_ALL, "Bad surface tesselated.\n" );
}

/*
==================
RB_TestZFlare

This is called at surface tesselation time
==================
*/
#if 0
static bool RB_TestZFlare( vec3_t point) {
	int				i;
	vec4_t			eye, clip, normalized, window;

	// if the point is off the screen, don't bother adding it
	// calculate screen coordinates and depth
	R_TransformModelToClip( point, backEnd.ori.modelViewMatrix,
		backEnd.viewParms.projectionMatrix, eye, clip );

	// check to see if the point is completely off screen
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( clip[i] >= clip[3] || clip[i] <= -clip[3] ) {
			return qfalse;
		}
	}

	R_TransformClipToWindow( clip, &backEnd.viewParms, normalized, window );

	if ( window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth
		|| window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight ) {
		return qfalse;	// shouldn't happen, since we check the clip[] above, except for FP rounding
	}

//do test
	float			depth = 0.0f;
	bool			visible;
	float			screenZ;

	// read back the z buffer contents
	if ( r_flares->integer !=1 ) {	//skipping the the z-test
		return true;
	}
	// doing a readpixels is as good as doing a glFinish(), so
	// don't bother with another sync
	glState.finishCalled = qfalse;
	qglReadPixels( backEnd.viewParms.viewportX + window[0],backEnd.viewParms.viewportY + window[1], 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );

	screenZ = backEnd.viewParms.projectionMatrix[14] /
		( ( 2*depth - 1 ) * backEnd.viewParms.projectionMatrix[11] - backEnd.viewParms.projectionMatrix[10] );

	visible = ( -eye[2] - -screenZ ) < 24;
	return visible;
}
#endif

void RB_SurfaceFlare( srfFlare_t *surf ) {
	if (r_flares->integer) {
#ifdef USE_VBO
		VBO_Flush();
		tess.surfType = SF_FLARE;
#endif
		RB_AddFlare(surf, tess.fogNum, surf->origin, surf->color, surf->normal);
	}

#if 0
	vec3_t		left, up;
	float		radius;
	byte		color[4];
	vec3_t		dir;
	vec3_t		origin;
	float		d, dist;

	if ( !r_flares->integer ) {
		return;
	}

	if (!RB_TestZFlare( surf->origin ) ) {
		return;
	}

	// calculate the xyz locations for the four corners
	VectorMA( surf->origin, 3, surf->normal, origin );
	float* snormal = surf->normal;

	VectorSubtract( origin, backEnd.viewParms.ori.origin, dir );
	dist = VectorNormalize( dir );

	d = -DotProduct( dir, snormal );
	if ( d < 0 ) {
		d = -d;
	}

	// fade the intensity of the flare down as the
	// light surface turns away from the viewer
	color[0] = d * 255;
	color[1] = d * 255;
	color[2] = d * 255;
	color[3] = 255;	//only gets used if the shader has cgen exact_vertex!

	radius = tess.shader->portalRange ? tess.shader->portalRange: 30;
	if (dist < 512.0f)
	{
		radius = radius * dist / 512.0f;
	}
	if (radius<5.0f)
	{
		radius = 5.0f;
	}
	VectorScale( backEnd.viewParms.ori.axis[1], radius, left );
	VectorScale( backEnd.viewParms.ori.axis[2], radius, up );
	if ( backEnd.viewParms.portalView == PV_MIRROR) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( origin, left, up, color );
#endif
}

void RB_SurfaceDisplayList( srfDisplayList_t *surf ) {
	// all appropriate state must be set in RB_BeginSurface
	// this isn't implemented yet...
	//qglCallList( surf->listNum );
}

void RB_SurfaceSkip( void *surf ) {
}

#ifdef USE_VBO_MDV
void RB_SurfaceVBOMDVMesh( srfVBOMDVMesh_t *surf )
{
	if ( !surf->vbo || !surf->ibo )
		return;

	tess.surfType = surf->surfaceType;
	tess.vbo_model = surf->vbo;
	tess.ibo_model = surf->ibo;

	int i, mergeForward, mergeBack;
	GLvoid *firstIndexOffset, *lastIndexOffset;

	// merge this into any existing multidraw primitives
	mergeForward = -1;
	mergeBack = -1;
	firstIndexOffset = BUFFER_OFFSET( surf->indexOffset );
	// end of this surface's index range - the merge tests below compare it against
	// other primitives' offsets, so it has to be an offset, not a count.
	lastIndexOffset = BUFFER_OFFSET( surf->indexOffset + surf->numIndexes );

	//if (r_mergeMultidraws->integer)
	{
		i = 0;

		//if (r_mergeMultidraws->integer == 1)
		{
			// lazy merge, only check the last primitive
			if (tess.multiDrawPrimitives)
			{
				i = tess.multiDrawPrimitives - 1;
			}
		}

		for (; i < tess.multiDrawPrimitives; i++)
		{
			if (tess.multiDrawLastIndex[i] == firstIndexOffset)
			{
				mergeBack = i;
			}

			if (lastIndexOffset == tess.multiDrawFirstIndex[i])
			{
				mergeForward = i;
			}
		}
	}

	if (mergeBack != -1 && mergeForward == -1)
	{
		tess.multiDrawNumIndexes[mergeBack] += surf->numIndexes;
		tess.multiDrawLastIndex[mergeBack] = tess.multiDrawFirstIndex[mergeBack] + tess.multiDrawNumIndexes[mergeBack];
		tess.multiDrawMinIndex[mergeBack] = MIN(tess.multiDrawMinIndex[mergeBack], surf->minIndex);
		tess.multiDrawMaxIndex[mergeBack] = MAX(tess.multiDrawMaxIndex[mergeBack], surf->maxIndex);
		//backEnd.pc.c_multidrawsMerged++;
	}
	else if (mergeBack == -1 && mergeForward != -1)
	{
		tess.multiDrawNumIndexes[mergeForward] += surf->numIndexes;
		tess.multiDrawFirstIndex[mergeForward] = (glIndex_t *)firstIndexOffset;
		tess.multiDrawLastIndex[mergeForward] = tess.multiDrawFirstIndex[mergeForward] + tess.multiDrawNumIndexes[mergeForward];
		tess.multiDrawMinIndex[mergeForward] = MIN(tess.multiDrawMinIndex[mergeForward], surf->minIndex);
		tess.multiDrawMaxIndex[mergeForward] = MAX(tess.multiDrawMaxIndex[mergeForward], surf->maxIndex);
		//backEnd.pc.c_multidrawsMerged++;
	}
	else if (mergeBack != -1 && mergeForward != -1)
	{
		tess.multiDrawNumIndexes[mergeBack] += surf->numIndexes + tess.multiDrawNumIndexes[mergeForward];
		tess.multiDrawLastIndex[mergeBack] = tess.multiDrawFirstIndex[mergeBack] + tess.multiDrawNumIndexes[mergeBack];
		tess.multiDrawMinIndex[mergeBack] = MIN(tess.multiDrawMinIndex[mergeBack], MIN(tess.multiDrawMinIndex[mergeForward], surf->minIndex));
		tess.multiDrawMaxIndex[mergeBack] = MAX(tess.multiDrawMaxIndex[mergeBack], MAX(tess.multiDrawMaxIndex[mergeForward], surf->maxIndex));
		tess.multiDrawPrimitives--;

		if (mergeForward != tess.multiDrawPrimitives)
		{
			tess.multiDrawNumIndexes[mergeForward] = tess.multiDrawNumIndexes[tess.multiDrawPrimitives];
			tess.multiDrawFirstIndex[mergeForward] = tess.multiDrawFirstIndex[tess.multiDrawPrimitives];
		}
		//backEnd.pc.c_multidrawsMerged += 2;
	}
	else if (mergeBack == -1 && mergeForward == -1)
	{
		tess.multiDrawNumIndexes[tess.multiDrawPrimitives] = surf->numIndexes;
		tess.multiDrawFirstIndex[tess.multiDrawPrimitives] = (glIndex_t *)firstIndexOffset;
		tess.multiDrawLastIndex[tess.multiDrawPrimitives] = (glIndex_t *)lastIndexOffset;
		tess.multiDrawMinIndex[tess.multiDrawPrimitives] = surf->minIndex;
		tess.multiDrawMaxIndex[tess.multiDrawPrimitives] = surf->maxIndex;
		tess.multiDrawPrimitives++;
	}

	tess.numIndexes = surf->numIndexes;
	tess.numVertexes = surf->numVerts;
}
#endif

void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])( void *) = {
	(void(*)(void*))RB_SurfaceBad,			// SF_BAD,
	(void(*)(void*))RB_SurfaceSkip,			// SF_SKIP,
	(void(*)(void*))RB_SurfaceFace,			// SF_FACE,
	(void(*)(void*))RB_SurfaceGrid,			// SF_GRID,
	(void(*)(void*))RB_SurfaceTriangles,	// SF_TRIANGLES,
	(void(*)(void*))RB_SurfacePolychain,	// SF_POLY,
	(void(*)(void*))RB_SurfaceMesh,			// SF_MDV
	(void(*)(void*))RB_SurfaceGhoul,		// SF_MDX,
	(void(*)(void*))RB_SurfaceFlare,		// SF_FLARE,
	(void(*)(void*))RB_SurfaceEntity,		// SF_ENTITY
#ifdef USE_VBO_MDV
	(void(*)(void*))RB_SurfaceVBOMDVMesh,	// SF_VBO_MDVMESH
#endif
#ifdef USE_VBO_SS
	(void(*)(void*))RB_SurfaceSpritesVBO	// SF_SPRITES
#endif
};