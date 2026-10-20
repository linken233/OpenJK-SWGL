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

#ifdef USE_VK_PBR

#include "utils/mikktspace/mikktspace.h"

/*
MikkTSpace tangents, ported from JKSunny/EternalJK (branch pbr). The tangent is xyz, the
bitangent sign is w. The normal maps are baked with MikkTSpace, so these tangents match them.
*/

/*
================
R_FixMikktVertIndex

Swaps the second and the third index of a triangle. The mesh importers of the baking tools
swap them to change the face orientation (id tech 3 culls the front faces).
================
*/
static int R_FixMikktVertIndex( const int index )
{
	switch ( index % 3 )
	{
		case 2: return 1;
		case 1: return 2;
		default: return index;
	}
}

static int mikkt_GetNumVerticesOfFace( const SMikkTSpaceContext *context, const int face )
{
	return 3;
}

static void mikkt_StoreTangent( float *out, const float tangent[], const float sign )
{
	VectorCopy( tangent, out );
	out[3] = sign;
}

//
// BSP triangle soup
//
static int mikkt_bsp_tri_Index( const SMikkTSpaceContext *context, const int face, const int vert )
{
	const srfTriangles_t *tri = (const srfTriangles_t *)context->m_pUserData;
	return tri->indexes[face * 3 + R_FixMikktVertIndex( vert )];
}

static int mikkt_bsp_tri_GetNumFaces( const SMikkTSpaceContext *context )
{
	const srfTriangles_t *tri = (const srfTriangles_t *)context->m_pUserData;
	return tri->numIndexes / 3;
}

static void mikkt_bsp_tri_GetPosition( const SMikkTSpaceContext *context, float position[], const int face, const int vert )
{
	const srfTriangles_t *tri = (const srfTriangles_t *)context->m_pUserData;
	VectorCopy( tri->verts[mikkt_bsp_tri_Index( context, face, vert )].xyz, position );
}

static void mikkt_bsp_tri_GetNormal( const SMikkTSpaceContext *context, float normal[], const int face, const int vert )
{
	const srfTriangles_t *tri = (const srfTriangles_t *)context->m_pUserData;
	VectorCopy( tri->verts[mikkt_bsp_tri_Index( context, face, vert )].normal, normal );
}

static void mikkt_bsp_tri_GetTexCoord( const SMikkTSpaceContext *context, float st[], const int face, const int vert )
{
	const srfTriangles_t *tri = (const srfTriangles_t *)context->m_pUserData;
	const srfVert_t *dv = &tri->verts[mikkt_bsp_tri_Index( context, face, vert )];
	st[0] = dv->st[0];
	st[1] = dv->st[1];
}

static void mikkt_bsp_tri_SetTangent( const SMikkTSpaceContext *context, const float tangent[], const float sign, const int face, const int vert )
{
	srfTriangles_t *tri = (srfTriangles_t *)context->m_pUserData;
	mikkt_StoreTangent( tri->verts[mikkt_bsp_tri_Index( context, face, vert )].qtangent, tangent, sign );
}

void vk_mikkt_bsp_tri_generate( srfTriangles_t *tri )
{
	SMikkTSpaceInterface info;
	info.m_getNumFaces			= mikkt_bsp_tri_GetNumFaces;
	info.m_getNumVerticesOfFace	= mikkt_GetNumVerticesOfFace;
	info.m_getPosition			= mikkt_bsp_tri_GetPosition;
	info.m_getNormal			= mikkt_bsp_tri_GetNormal;
	info.m_getTexCoord			= mikkt_bsp_tri_GetTexCoord;
	info.m_setTSpaceBasic		= mikkt_bsp_tri_SetTangent;
	info.m_setTSpace			= NULL;

	SMikkTSpaceContext context;
	context.m_pInterface = &info;
	context.m_pUserData = tri;

	genTangSpaceDefault( &context );
}

//
// BSP face: the normal is the one RB_SurfaceFace draws, per vertex or the plane normal.
//
static int mikkt_bsp_face_Index( const SMikkTSpaceContext *context, const int face, const int vert )
{
	const srfSurfaceFace_t *cv = (const srfSurfaceFace_t *)context->m_pUserData;
	const int *indices = (const int *)((const byte *)cv + cv->ofsIndices);
	return indices[face * 3 + R_FixMikktVertIndex( vert )];
}

static int mikkt_bsp_face_GetNumFaces( const SMikkTSpaceContext *context )
{
	const srfSurfaceFace_t *cv = (const srfSurfaceFace_t *)context->m_pUserData;
	return cv->numIndices / 3;
}

static void mikkt_bsp_face_GetPosition( const SMikkTSpaceContext *context, float position[], const int face, const int vert )
{
	const srfSurfaceFace_t *cv = (const srfSurfaceFace_t *)context->m_pUserData;
	VectorCopy( cv->points[mikkt_bsp_face_Index( context, face, vert )], position );
}

static void mikkt_bsp_face_GetNormal( const SMikkTSpaceContext *context, float normal[], const int face, const int vert )
{
	const srfSurfaceFace_t *cv = (const srfSurfaceFace_t *)context->m_pUserData;

	if ( cv->normals )
		VectorCopy( cv->normals + mikkt_bsp_face_Index( context, face, vert ) * 4, normal );
	else
		VectorCopy( cv->plane.normal, normal );
}

static void mikkt_bsp_face_GetTexCoord( const SMikkTSpaceContext *context, float st[], const int face, const int vert )
{
	const srfSurfaceFace_t *cv = (const srfSurfaceFace_t *)context->m_pUserData;
	const float *v = cv->points[mikkt_bsp_face_Index( context, face, vert )];
	st[0] = v[3];
	st[1] = v[4];
}

static void mikkt_bsp_face_SetTangent( const SMikkTSpaceContext *context, const float tangent[], const float sign, const int face, const int vert )
{
	srfSurfaceFace_t *cv = (srfSurfaceFace_t *)context->m_pUserData;
	mikkt_StoreTangent( cv->qtangents + mikkt_bsp_face_Index( context, face, vert ) * 4, tangent, sign );
}

void vk_mikkt_bsp_face_generate( srfSurfaceFace_t *cv )
{
	cv->qtangents = (float *)Hunk_Alloc( cv->numPoints * sizeof( vec4_t ), h_low );

	SMikkTSpaceInterface info;
	info.m_getNumFaces			= mikkt_bsp_face_GetNumFaces;
	info.m_getNumVerticesOfFace	= mikkt_GetNumVerticesOfFace;
	info.m_getPosition			= mikkt_bsp_face_GetPosition;
	info.m_getNormal			= mikkt_bsp_face_GetNormal;
	info.m_getTexCoord			= mikkt_bsp_face_GetTexCoord;
	info.m_setTSpaceBasic		= mikkt_bsp_face_SetTangent;
	info.m_setTSpace			= NULL;

	SMikkTSpaceContext context;
	context.m_pInterface = &info;
	context.m_pUserData = cv;

	genTangSpaceDefault( &context );
}

//
// MDXM surface
//
typedef struct {
	int							numTriangles;
	const mdxmTriangle_t		*triangles;
	const mdxmVertex_t			*verts;
	const mdxmVertexTexCoord_t	*st;
	vec4_t						*tangents;
} mikktMdxmMesh_t;

static int mikkt_mdxm_Index( const SMikkTSpaceContext *context, const int face, const int vert )
{
	const mikktMdxmMesh_t *mesh = (const mikktMdxmMesh_t *)context->m_pUserData;
	return mesh->triangles[face].indexes[R_FixMikktVertIndex( vert )];
}

static int mikkt_mdxm_GetNumFaces( const SMikkTSpaceContext *context )
{
	const mikktMdxmMesh_t *mesh = (const mikktMdxmMesh_t *)context->m_pUserData;
	return mesh->numTriangles;
}

static void mikkt_mdxm_GetPosition( const SMikkTSpaceContext *context, float position[], const int face, const int vert )
{
	const mikktMdxmMesh_t *mesh = (const mikktMdxmMesh_t *)context->m_pUserData;
	VectorCopy( mesh->verts[mikkt_mdxm_Index( context, face, vert )].vertCoords, position );
}

static void mikkt_mdxm_GetNormal( const SMikkTSpaceContext *context, float normal[], const int face, const int vert )
{
	const mikktMdxmMesh_t *mesh = (const mikktMdxmMesh_t *)context->m_pUserData;
	VectorCopy( mesh->verts[mikkt_mdxm_Index( context, face, vert )].normal, normal );
}

static void mikkt_mdxm_GetTexCoord( const SMikkTSpaceContext *context, float st[], const int face, const int vert )
{
	const mikktMdxmMesh_t *mesh = (const mikktMdxmMesh_t *)context->m_pUserData;
	const mdxmVertexTexCoord_t *tc = &mesh->st[mikkt_mdxm_Index( context, face, vert )];
	st[0] = tc->texCoords[0];
	st[1] = tc->texCoords[1];
}

static void mikkt_mdxm_SetTangent( const SMikkTSpaceContext *context, const float tangent[], const float sign, const int face, const int vert )
{
	mikktMdxmMesh_t *mesh = (mikktMdxmMesh_t *)context->m_pUserData;
	mikkt_StoreTangent( mesh->tangents[mikkt_mdxm_Index( context, face, vert )], tangent, sign );
}

// The tangents of the vertices of one surface, indexed like its vertices.
void vk_mikkt_mdxm_generate( const mdxmSurface_t *surf, vec4_t *tangents )
{
	const mdxmVertex_t *verts = (const mdxmVertex_t *)((const byte *)surf + surf->ofsVerts);

	mikktMdxmMesh_t mesh;
	mesh.numTriangles	= surf->numTriangles;
	mesh.triangles		= (const mdxmTriangle_t *)((const byte *)surf + surf->ofsTriangles);
	mesh.verts			= verts;
	mesh.st				= (const mdxmVertexTexCoord_t *)(verts + surf->numVerts);
	mesh.tangents		= tangents;

	SMikkTSpaceInterface info;
	info.m_getNumFaces			= mikkt_mdxm_GetNumFaces;
	info.m_getNumVerticesOfFace	= mikkt_GetNumVerticesOfFace;
	info.m_getPosition			= mikkt_mdxm_GetPosition;
	info.m_getNormal			= mikkt_mdxm_GetNormal;
	info.m_getTexCoord			= mikkt_mdxm_GetTexCoord;
	info.m_setTSpaceBasic		= mikkt_mdxm_SetTangent;
	info.m_setTSpace			= NULL;

	SMikkTSpaceContext context;
	context.m_pInterface = &info;
	context.m_pUserData = &mesh;

	genTangSpaceDefault( &context );
}

//
// MD3 surface
//
typedef struct {
	const mdvSurface_t	*surf;
	vec4_t				*tangents;
} mikktMdvMesh_t;

static int mikkt_mdv_Index( const SMikkTSpaceContext *context, const int face, const int vert )
{
	const mikktMdvMesh_t *mesh = (const mikktMdvMesh_t *)context->m_pUserData;
	return mesh->surf->indexes[face * 3 + R_FixMikktVertIndex( vert )];
}

static int mikkt_mdv_GetNumFaces( const SMikkTSpaceContext *context )
{
	const mikktMdvMesh_t *mesh = (const mikktMdvMesh_t *)context->m_pUserData;
	return mesh->surf->numIndexes / 3;
}

static void mikkt_mdv_GetPosition( const SMikkTSpaceContext *context, float position[], const int face, const int vert )
{
	const mikktMdvMesh_t *mesh = (const mikktMdvMesh_t *)context->m_pUserData;
	VectorCopy( mesh->surf->verts[mikkt_mdv_Index( context, face, vert )].xyz, position );
}

static void mikkt_mdv_GetNormal( const SMikkTSpaceContext *context, float normal[], const int face, const int vert )
{
	const mikktMdvMesh_t *mesh = (const mikktMdvMesh_t *)context->m_pUserData;
	VectorCopy( mesh->surf->verts[mikkt_mdv_Index( context, face, vert )].normal, normal );
}

static void mikkt_mdv_GetTexCoord( const SMikkTSpaceContext *context, float st[], const int face, const int vert )
{
	const mikktMdvMesh_t *mesh = (const mikktMdvMesh_t *)context->m_pUserData;
	const mdvSt_t *tc = &mesh->surf->st[mikkt_mdv_Index( context, face, vert )];
	st[0] = tc->st[0];
	st[1] = tc->st[1];
}

static void mikkt_mdv_SetTangent( const SMikkTSpaceContext *context, const float tangent[], const float sign, const int face, const int vert )
{
	mikktMdvMesh_t *mesh = (mikktMdvMesh_t *)context->m_pUserData;
	mikkt_StoreTangent( mesh->tangents[mikkt_mdv_Index( context, face, vert )], tangent, sign );
}

// The tangents of the vertices of the first frame of one surface, indexed like its vertices.
void vk_mikkt_mdv_generate( const mdvSurface_t *surf, vec4_t *tangents )
{
	mikktMdvMesh_t mesh;
	mesh.surf		= surf;
	mesh.tangents	= tangents;

	SMikkTSpaceInterface info;
	info.m_getNumFaces			= mikkt_mdv_GetNumFaces;
	info.m_getNumVerticesOfFace	= mikkt_GetNumVerticesOfFace;
	info.m_getPosition			= mikkt_mdv_GetPosition;
	info.m_getNormal			= mikkt_mdv_GetNormal;
	info.m_getTexCoord			= mikkt_mdv_GetTexCoord;
	info.m_setTSpaceBasic		= mikkt_mdv_SetTangent;
	info.m_setTSpace			= NULL;

	SMikkTSpaceContext context;
	context.m_pInterface = &info;
	context.m_pUserData = &mesh;

	genTangSpaceDefault( &context );
}

#endif // USE_VK_PBR
