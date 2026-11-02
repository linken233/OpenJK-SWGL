/*
===========================================================================
Cloth and hair physics for Ghoul2 models.

A .phys file next to a .glm names groups of surfaces. Each group is
simulated as one piece of cloth. The animated (skinned) vertex positions
are the targets of the solver in rd-common/phys_cloth.cpp. The result goes
to RB_SurfaceGhoul, which draws the surface from the solved positions
instead of skinning it.
===========================================================================
*/

#include "../server/exe_headers.h"

#ifndef __Q_SHARED_H
	#include "../qcommon/q_shared.h"
#endif

#if !defined(TR_LOCAL_H)
	#include "tr_local.h"
#endif

#if !defined(G2_H_INC)
	#include "../ghoul2/G2.h"
#endif

#include "tr_WorldEffects.h"
#include "rd-common/phys_cloth.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

static cvar_t	*ph_enable;
static cvar_t	*ph_debug;
static cvar_t	*ph_range;

#define PHYS_LOD				0		// the solver and the draw both use LOD 0
#define PHYS_STALE_FRAMES		2
#define PHYS_STALE_TIME			0.25f
#define PHYS_TELEPORT_DIST		192.0f
#define PHYS_MAX_WIND			600.0f
#define PHYS_DEFAULT_PROXY		4.0f	// cell size of the simulation mesh when the group does not set one
#define PHYS_MAX_SUBDIV_VERTS	16384	// drawn vertices of a subdivided group, at most
#define PHYS_MAX_SIM_VERTS		8192	// simsubdivide: vertices of the refined simulation mesh, at most
#define PHYS_MIN_PIN_FREE		0.01f	// a particle with this free factor or less is pinned
#define PHYS_REF_MIN_SHARE		0.1f	// reference bone: smallest part of the weight of the pinned vertices
#define PHYS_DBG_SPEED			400.0f	// ph_debug 6: a particle speed that prints a line, units per second

struct physResolvedCollider_t
{
	int		boneA, boneB;
	float	bindA[3];	// bind pose model space, offset included. Box: the center
	float	bindB[3];
	bool	box;		// rounded box on boneA, along the bind pose axes of the model
	float	half[3];	// box: half sizes of the inner box (size - radius)
	float	radius;		// box: corner radius
	char	kind;		// m = model fit, h = humanoid, c = collide line
};

struct physGroupInst_t
{
	const physGroupDef_t				*def;
	const physDef_t						*owner;			// the file that holds the group
	std::vector<int>					matched;		// every surface the patterns match
	std::vector<int>					active;			// matched surfaces that are on
	std::vector<unsigned char>			activeMask;		// per surface of the model
	std::vector<unsigned char>			maskScratch;
	std::vector<int>					slotOfSurface;	// per surface of the model, index in active, or -1
	std::vector<std::vector<float> >	skinXYZ;		// per active surface, 3 per vertex, model space
	std::vector<std::vector<float> >	skinNormal;
	std::vector<std::vector<float> >	outXYZ;
	std::vector<std::vector<float> >	outNormal;
	std::vector<physSurfaceOut_t>		out;
	physTopology_t						topo;			// the simulation mesh before the proxy: the model mesh, or the refined mesh
	physTopology_t						baseTopo;		// simsubdivide: the model mesh. Its particle i is particle i of topo
	physSubdiv_t						simSubdiv;		// simsubdivide: the points of topo from the particles of baseTopo, levels 0 = none
	std::vector<float>					baseTarget;		// simsubdivide: per particle of baseTopo, world space
	std::vector<float>					baseSkin;		// simsubdivide: per particle of baseTopo, animated normal
	std::vector<float>					particleSkin;	// per particle: animated normal, model space
	physProxy_t							proxy;			// the simulation mesh and its links to the full mesh
	physCloth_t							cloth;			// state of the simulation mesh
	physLag_t							lag;
	std::vector<float>					proxyTarget;	// per simulation particle, world space
	std::vector<float>					worldPos;		// per particle, world space
	bool								primed;			// the solver has a start state
	std::vector<physCollider_w>			worldColliders;	// of this frame
	std::vector<physCollider_w>			prevWorldColliders;	// of the last frame: the steps move the colliders between the two
	std::vector<float>					restDist;		// Phys_RestDistances of the particles
	std::vector<float>					freeFactor;		// per particle
	std::vector<float>					target;			// per particle, world space
	std::vector<float>					modelPos;		// per particle, model space
	std::vector<float>					particleNormal;
	std::vector<physResolvedCollider_t>	colliders;
	int									refBone;		// reference bone: its transform is the model frame of the solver, -1 = none
	float								refBind[3];		// bind pose center of the pinned particles: the origin of the frame
	physSubdiv_t						subdiv;			// subdivided draw mesh, levels 0 = none
	std::vector<float>					pointXYZ;		// per refined point, model space
	std::vector<float>					pointNormal;
	std::vector<float>					drawNormal;		// per particle: the normal that the mesh gets
	bool								built;
	bool								usable;
	int									frame;			// frame of the last update
	int									reportTime;		// ph_debug 4
	float								dbgAxis[3][3];	// ph_debug 5: entity axes of the last frame
	float								dbgOrigin[3];
	std::vector<float>					dbgTarget;		// ph_debug 6: animated positions of the last frame
	std::vector<float>					dbgModel, dbgPrevModel;	// ph_debug 6: animated positions in model space, this and the last frame

	physGroupInst_t() : def( NULL ), owner( NULL ), primed( false ), refBone( -1 ), built( false ), usable( false ), frame( -1 ), reportTime( 0 ) {}
};

struct physInstance_t
{
	const void					*header;
	int							epoch;
	const physDef_t				*def;
	int							numSurfaces;
	std::vector<physGroupInst_t> groups;
	std::vector<int>			surfaceGroup;	// per surface of the model, or -1
	int							lastFrame;
	int							lastTime;
	float						lastOrigin[3];

	physInstance_t() : header( NULL ), epoch( 0 ), def( NULL ), numSurfaces( 0 ), lastFrame( -1 ), lastTime( 0 )
	{
		lastOrigin[0] = lastOrigin[1] = lastOrigin[2] = 0.0f;
	}
};

// The renderers keep the mesh header in different places.
static inline const mdxaHeader_t *Phys_AnimHeader( const mdxmHeader_t *header )
{
	const model_t *anim = R_GetModelByHandle( header->animIndex );

	return anim ? anim->data.gla : NULL;
}

static inline const mdxmHeader_t *Phys_Header( const model_t *model )
{
	return ( model && model->data.glm ) ? model->data.glm->header : NULL;
}

// The set of debug buffers of this frame. With the render thread, the backend reads the other set.
static inline int Phys_DebugBuffer( void )
{
	return ( backEndDataBuf[1] && backEndData == backEndDataBuf[1] ) ? 1 : 0;
}

// Provided by tr_ghoul2.cpp, which owns the bone cache.
const mdxaBone_t		&G2Phys_BoneMatrix( CBoneCache *bc, int bone );
const mdxaHeader_t		*G2Phys_BoneHeader( CBoneCache *bc );
void					**G2Phys_BoneSlot( CBoneCache *bc );

static std::map<std::string, physDef_t *>	s_defs;
static int									s_epoch = 1;
static const char							*s_parseName = "";

// Edges of a surface for the outline of the editor. The vertices are welded by position.
struct physOutlineEdge_t
{
	int	a, b;		// vertices
	int	t0, t1;		// triangles on each side. t1: -1 = open edge, -2 = more than two triangles
};

struct physOutlineMesh_t
{
	int								numVerts;
	int								numTris;
	std::vector<physOutlineEdge_t>	edges;

	physOutlineMesh_t() : numVerts( -1 ), numTris( -1 ) {}
};

static std::map<const void *, physOutlineMesh_t>	s_outlineMeshes;

//
// Definition cache
//

static void Phys_Print( const char *msg )
{
	ri.Printf( PRINT_WARNING, "%s: %s", s_parseName, msg );
}

static std::string Phys_LowerKey( const char *name )
{
	std::string key = name;

	for ( size_t i = 0; i < key.size(); i++ )
		key[i] = (char)tolower( (unsigned char)key[i] );

	return key;
}

static std::string Phys_FilePath( const std::string &key )
{
	std::string path = key;
	const size_t dot = path.rfind( '.' );

	if ( dot != std::string::npos )
		path.erase( dot );

	return path + ".phys";
}

static physDef_t *Phys_FindDef( const std::string &key )
{
	std::map<std::string, physDef_t *>::iterator it = s_defs.find( key );

	if ( it != s_defs.end() )
		return it->second;

	const std::string path = Phys_FilePath( key );
	physDef_t *def = NULL;
	char *buffer = NULL;
	const long len = ri.FS_ReadFile( path.c_str(), (void **)&buffer );

	if ( len > 0 && buffer )
	{
		s_parseName = path.c_str();
		def = Phys_ParseDef( buffer, path.c_str(), Phys_Print );

		if ( def && ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: loaded %s, %d group(s)\n", path.c_str(), (int)def->groups.size() );
	}

	if ( buffer )
		ri.FS_FreeFile( buffer );

	s_defs[key] = def;
	return def;
}

static const physDef_t *Phys_GetDef( const model_t *glm )
{
	return Phys_FindDef( Phys_LowerKey( glm->name ) );
}

static void Phys_Invalidate( void )
{
	s_epoch++;
}

static void Phys_ClearDefs( void )
{
	for ( std::map<std::string, physDef_t *>::iterator it = s_defs.begin(); it != s_defs.end(); ++it )
		delete it->second;

	s_defs.clear();
	s_outlineMeshes.clear();
	s_epoch++;
}

void G2Phys_Reload_f( void )
{
	Phys_ClearDefs();
	ri.Printf( PRINT_ALL, "phys: definitions cleared, they load again when a model is drawn\n" );
}

void G2Phys_Init( void )
{
	ph_enable = Cvar_Get( "ph_enable", "1", CVAR_ARCHIVE_ND, "" );
	ph_debug = Cvar_Get( "ph_debug", "0", CVAR_NONE, "" );
	ph_range = Cvar_Get( "ph_range", "1", CVAR_ARCHIVE_ND, "" );
}

void G2Phys_Shutdown( void )
{
	Phys_ClearDefs();
}

void G2Phys_FreeInstance( void *inst )
{
	delete (physInstance_t *)inst;
}

//
// Model data access
//

static const mdxmSurfHierarchy_t *Phys_SurfInfo( const mdxmHeader_t *header, int surfaceNum )
{
	const mdxmHierarchyOffsets_t *offsets = (const mdxmHierarchyOffsets_t *)( (const byte *)header + sizeof( mdxmHeader_t ) );

	return (const mdxmSurfHierarchy_t *)( (const byte *)offsets + offsets->offsets[surfaceNum] );
}

static const mdxaSkel_t *Phys_Skel( const mdxaHeader_t *header, int bone )
{
	const mdxaSkelOffsets_t *offsets = (const mdxaSkelOffsets_t *)( (const byte *)header + sizeof( mdxaHeader_t ) );

	return (const mdxaSkel_t *)( (const byte *)header + sizeof( mdxaHeader_t ) + offsets->offsets[bone] );
}

static int Phys_FindBone( const mdxaHeader_t *header, const std::string &name )
{
	for ( int i = 0; i < header->numBones; i++ )
	{
		if ( !Q_stricmp( Phys_Skel( header, i )->name, name.c_str() ) )
			return i;
	}

	return -1;
}

static void Phys_BindOrigin( const mdxaHeader_t *header, int bone, float out[3] )
{
	const mdxaSkel_t *skel = Phys_Skel( header, bone );

	out[0] = skel->BasePoseMat.matrix[0][3];
	out[1] = skel->BasePoseMat.matrix[1][3];
	out[2] = skel->BasePoseMat.matrix[2][3];
}

// Same blend of bone matrices as the CPU path of RB_SurfaceGhoul.
static void Phys_SkinVertex( CBoneCache *bc, const int *boneRefs, const mdxmVertex_t *v, float *xyz, float *normal )
{
	const int numWeights = G2_GetVertWeights( v );
	float total = 0.0f;

	xyz[0] = xyz[1] = xyz[2] = 0.0f;

	for ( int k = 0; k < numWeights; k++ )
	{
		const mdxaBone_t &bone = G2Phys_BoneMatrix( bc, boneRefs[G2_GetVertBoneIndex( v, k )] );
		const float w = G2_GetVertBoneWeight( v, k, total, numWeights );

		xyz[0] += w * ( DotProduct( bone.matrix[0], v->vertCoords ) + bone.matrix[0][3] );
		xyz[1] += w * ( DotProduct( bone.matrix[1], v->vertCoords ) + bone.matrix[1][3] );
		xyz[2] += w * ( DotProduct( bone.matrix[2], v->vertCoords ) + bone.matrix[2][3] );
	}

	const mdxaBone_t &first = G2Phys_BoneMatrix( bc, boneRefs[G2_GetVertBoneIndex( v, 0 )] );

	normal[0] = DotProduct( first.matrix[0], v->normal );
	normal[1] = DotProduct( first.matrix[1], v->normal );
	normal[2] = DotProduct( first.matrix[2], v->normal );
	VectorNormalize( normal );
}

//
// Instance creation
//

static physInstance_t *Phys_CreateInstance( const model_t *glm )
{
	physInstance_t *inst = new physInstance_t;
	const mdxmHeader_t *header = Phys_Header( glm );

	inst->header = header;
	inst->epoch = s_epoch;
	inst->def = Phys_GetDef( glm );
	inst->numSurfaces = header->numSurfaces;

	if ( !inst->def )
		return inst;

	inst->groups.resize( inst->def->groups.size() );
	inst->surfaceGroup.assign( header->numSurfaces, -1 );

	bool any = false;

	for ( size_t g = 0; g < inst->def->groups.size(); g++ )
	{
		inst->groups[g].def = &inst->def->groups[g];
		inst->groups[g].owner = inst->def;
	}

	for ( int s = 0; s < header->numSurfaces; s++ )
	{
		const char *name = Phys_SurfInfo( header, s )->name;

		for ( size_t g = 0; g < inst->groups.size() && inst->surfaceGroup[s] < 0; g++ )
		{
			const physGroupDef_t &def = inst->def->groups[g];

			if ( !def.enabled )
				continue;

			for ( size_t p = 0; p < def.surfaces.size(); p++ )
			{
				if ( Phys_MatchName( def.surfaces[p].c_str(), name ) )
				{
					inst->surfaceGroup[s] = (int)g;
					inst->groups[g].matched.push_back( s );
					any = true;
					break;
				}
			}
		}
	}

	if ( !any )
	{
		inst->groups.clear();
		inst->surfaceGroup.clear();

		if ( ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: %s has a .phys file but no surface matches\n", glm->name );
	}

	return inst;
}

//
// Group build
//

static float Phys_Clamp01( float f )
{
	return f < 0.0f ? 0.0f : ( f > 1.0f ? 1.0f : f );
}

// The pin factor of a particle: 0 is fixed to the animation, 1 is free.
static float Phys_PinFactor( const physGroupDef_t &def, const mdxaHeader_t *aHeader, const std::vector<int> &ruleBone,
							 const std::vector<std::vector<int> > &ruleBones, const float *bindPos,
							 const mdxmSurface_t *surf, int vertex )
{
	if ( def.pins.empty() )
		return 1.0f;

	const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts );
	const mdxmVertexTexCoord_t *texCoords = (const mdxmVertexTexCoord_t *)&verts[surf->numVerts];
	const int *boneRefs = (const int *)( (const byte *)surf + surf->ofsBoneReferences );
	float result = 1.0f;

	for ( size_t r = 0; r < def.pins.size(); r++ )
	{
		const physPinRule_t &rule = def.pins[r];
		float t = 1.0f;

		if ( rule.type == PHYS_PIN_GRADIENT )
		{
			if ( ruleBone[r] < 0 )
				continue;

			float origin[3];
			Phys_BindOrigin( aHeader, ruleBone[r], origin );

			const float c = ( bindPos[0] - origin[0] ) * rule.axis[0] + ( bindPos[1] - origin[1] ) * rule.axis[1] + ( bindPos[2] - origin[2] ) * rule.axis[2];

			t = Phys_Clamp01( ( c - rule.pinned ) / ( rule.free - rule.pinned ) );
		}
		else if ( rule.type == PHYS_PIN_RADIUS )
		{
			if ( ruleBone[r] < 0 )
				continue;

			float origin[3];
			Phys_BindOrigin( aHeader, ruleBone[r], origin );

			const float d[3] = { bindPos[0] - origin[0] - rule.axis[0], bindPos[1] - origin[1] - rule.axis[1], bindPos[2] - origin[2] - rule.axis[2] };
			const float dist = sqrtf( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );

			t = Phys_Clamp01( ( dist - rule.pinned ) / ( rule.free - rule.pinned ) );
		}
		else if ( rule.type == PHYS_PIN_BONES )
		{
			const mdxmVertex_t *v = &verts[vertex];
			const int numWeights = G2_GetVertWeights( v );
			float total = 0.0f;
			float weight = 0.0f;

			for ( int k = 0; k < numWeights; k++ )
			{
				const int bone = boneRefs[G2_GetVertBoneIndex( v, k )];
				const float w = G2_GetVertBoneWeight( v, k, total, numWeights );

				for ( size_t b = 0; b < ruleBones[r].size(); b++ )
				{
					if ( ruleBones[r][b] == bone )
					{
						weight += w;
						break;
					}
				}
			}

			t = Phys_Clamp01( 1.0f - weight );
		}
		else
		{
			t = Phys_Clamp01( ( texCoords[vertex].texCoords[1] - rule.pinned ) / ( rule.free - rule.pinned ) );
		}

		if ( t < result )
			result = t;
	}

	return result;
}

//
// Colliders fitted to the model: one capsule for each bone that carries body vertices.
//

#define PHYS_MIN_SURFACE_VERTS	20		// a surface with less vertices is a tag or a cap, not a real surface
#define PHYS_FIT_MIN_POINTS		6
#define PHYS_FIT_MAX_COLLIDERS	32

static bool Phys_RealSurface( const mdxmSurface_t *surf )
{
	return surf && surf->numVerts >= PHYS_MIN_SURFACE_VERTS;
}

static bool Phys_SurfaceInGroup( const physDef_t *def, const char *name )
{
	for ( size_t g = 0; g < def->groups.size(); g++ )
	{
		for ( size_t p = 0; p < def->groups[g].surfaces.size(); p++ )
		{
			if ( Phys_MatchName( def->groups[g].surfaces[p].c_str(), name ) )
				return true;
		}
	}

	return false;
}

struct physFitBone_t
{
	std::vector<float>	pts;	// bind pose, 3 per vertex
	int					bone;
};

static float Phys_Percentile( std::vector<float> &v, float f )
{
	if ( v.empty() )
		return 0.0f;

	std::sort( v.begin(), v.end() );

	return v[(size_t)( f * (float)( v.size() - 1 ) )];
}

static void Phys_FitColliders( const model_t *glm, const mdxaHeader_t *aHeader, const physDef_t *def, float scale,
							   std::vector<physCollider_t> &out )
{
	const mdxmHeader_t *header = Phys_Header( glm );
	const int numBones = aHeader->numBones;
	std::vector<std::vector<float> > pts( numBones );

	out.clear();

	// The vertices of the body, with the bone that has most of their weight.
	for ( int s = 0; s < header->numSurfaces; s++ )
	{
		const mdxmSurface_t *surf = (const mdxmSurface_t *)G2_FindSurface( glm, s, PHYS_LOD );

		if ( !Phys_RealSurface( surf ) || Phys_SurfaceInGroup( def, Phys_SurfInfo( header, s )->name ) )
			continue;

		const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts );
		const int *boneRefs = (const int *)( (const byte *)surf + surf->ofsBoneReferences );

		for ( int i = 0; i < surf->numVerts; i++ )
		{
			const int numWeights = G2_GetVertWeights( &verts[i] );
			float total = 0.0f, best = 0.0f;
			int bestBone = -1;

			for ( int k = 0; k < numWeights; k++ )
			{
				const int bone = boneRefs[G2_GetVertBoneIndex( &verts[i], k )];
				const float w = G2_GetVertBoneWeight( &verts[i], k, total, numWeights );

				if ( w > best )
				{
					best = w;
					bestBone = bone;
				}
			}

			if ( bestBone >= 0 && bestBone < numBones && best >= 0.5f )
			{
				pts[bestBone].push_back( verts[i].vertCoords[0] );
				pts[bestBone].push_back( verts[i].vertCoords[1] );
				pts[bestBone].push_back( verts[i].vertCoords[2] );
			}
		}
	}

	std::vector<std::pair<int, physCollider_t> > found;

	for ( int b = 0; b < numBones; b++ )
	{
		const int count = (int)pts[b].size() / 3;

		if ( count < PHYS_FIT_MIN_POINTS )
			continue;

		const mdxaSkel_t *skel = Phys_Skel( aHeader, b );
		float origin[3];

		Phys_BindOrigin( aHeader, b, origin );

		// The child that carries most vertices gives the direction of the capsule.
		int child = -1, childCount = 0;

		for ( int c = 0; c < skel->numChildren; c++ )
		{
			const int n = (int)pts[skel->children[c]].size() / 3;

			if ( n > childCount )
			{
				childCount = n;
				child = skel->children[c];
			}
		}

		float end[3] = { origin[0], origin[1], origin[2] };
		bool capsule = false;

		if ( child >= 0 )
		{
			Phys_BindOrigin( aHeader, child, end );

			const float d[3] = { end[0] - origin[0], end[1] - origin[1], end[2] - origin[2] };

			capsule = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > 2.25f;
		}

		physCollider_t c;
		std::vector<float> dist;

		c.boneA = skel->name;
		c.offA[0] = c.offA[1] = c.offA[2] = 0.0f;
		c.offB[0] = c.offB[1] = c.offB[2] = 0.0f;

		if ( capsule )
		{
			c.capsule = true;
			c.boneB = Phys_Skel( aHeader, child )->name;

			float ab[3] = { end[0] - origin[0], end[1] - origin[1], end[2] - origin[2] };
			const float len2 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2];

			for ( int i = 0; i < count; i++ )
			{
				const float *p = &pts[b][i * 3];
				const float ap[3] = { p[0] - origin[0], p[1] - origin[1], p[2] - origin[2] };
				float t = ( ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2] ) / len2;

				t = t < 0.0f ? 0.0f : ( t > 1.0f ? 1.0f : t );

				const float q[3] = { ap[0] - ab[0] * t, ap[1] - ab[1] * t, ap[2] - ab[2] * t };

				dist.push_back( sqrtf( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] ) );
			}
		}
		else
		{
			// A bone at the end of a chain: a sphere at the center of its vertices.
			float centroid[3] = { 0.0f, 0.0f, 0.0f };

			for ( int i = 0; i < count; i++ )
			{
				for ( int k = 0; k < 3; k++ )
					centroid[k] += pts[b][i * 3 + k] / (float)count;
			}

			c.capsule = false;
			c.boneB = c.boneA;

			for ( int k = 0; k < 3; k++ )
				c.offA[k] = c.offB[k] = centroid[k] - origin[k];

			for ( int i = 0; i < count; i++ )
			{
				const float *p = &pts[b][i * 3];
				const float q[3] = { p[0] - centroid[0], p[1] - centroid[1], p[2] - centroid[2] };

				dist.push_back( sqrtf( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] ) );
			}
		}

		float radius = Phys_Percentile( dist, 0.85f ) * scale;

		radius = radius < 1.0f ? 1.0f : ( radius > 14.0f ? 14.0f : radius );
		c.radius = radius;
		found.push_back( std::make_pair( count, c ) );
	}

	// Keep the colliders with most vertices.
	std::sort( found.begin(), found.end(), []( const std::pair<int, physCollider_t> &a, const std::pair<int, physCollider_t> &b ) { return a.first > b.first; } );

	for ( size_t i = 0; i < found.size() && i < PHYS_FIT_MAX_COLLIDERS; i++ )
		out.push_back( found[i].second );
}

static void Phys_BuildGroup( physGroupInst_t &gi, const model_t *glm, CBoneCache *bc, const char *modelName )
{
	const physGroupDef_t &def = *gi.def;
	const mdxaHeader_t *aHeader = G2Phys_BoneHeader( bc );
	const int numActive = (int)gi.active.size();

	gi.built = true;
	gi.usable = false;

	std::vector<const mdxmSurface_t *> surfs( numActive );
	std::vector<physSurfaceInput_t> inputs( numActive );
	std::vector<std::vector<float> > bind( numActive );

	for ( int s = 0; s < numActive; s++ )
	{
		surfs[s] = (const mdxmSurface_t *)G2_FindSurface( glm, gi.active[s], PHYS_LOD );

		const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surfs[s] + surfs[s]->ofsVerts );

		bind[s].resize( surfs[s]->numVerts * 3 );

		for ( int i = 0; i < surfs[s]->numVerts; i++ )
			VectorCopy( verts[i].vertCoords, &bind[s][i * 3] );

		inputs[s].numVerts = surfs[s]->numVerts;
		inputs[s].xyz = bind[s].empty() ? NULL : &bind[s][0];
		inputs[s].numTris = surfs[s]->numTriangles;
		inputs[s].tris = (const int *)( (const byte *)surfs[s] + surfs[s]->ofsTriangles );
	}

	if ( numActive )
		Phys_BuildTopology( gi.topo, &inputs[0], numActive );
	else
		gi.topo = physTopology_t();

	if ( gi.topo.numParticles == 0 )
		return;

	// Resolve the bones of the pin rules.
	std::vector<int> ruleBone( def.pins.size(), -1 );
	std::vector<std::vector<int> > ruleBones( def.pins.size() );

	for ( size_t r = 0; r < def.pins.size(); r++ )
	{
		if ( def.pins[r].type == PHYS_PIN_GRADIENT || def.pins[r].type == PHYS_PIN_RADIUS )
		{
			ruleBone[r] = Phys_FindBone( aHeader, def.pins[r].bone );

			if ( ruleBone[r] < 0 )
				ri.Printf( PRINT_WARNING, "phys: %s: pin bone '%s' not found\n", modelName, def.pins[r].bone.c_str() );
		}
		else if ( def.pins[r].type == PHYS_PIN_BONES )
		{
			for ( size_t b = 0; b < def.pins[r].bones.size(); b++ )
			{
				const int bone = Phys_FindBone( aHeader, def.pins[r].bones[b] );

				if ( bone >= 0 )
					ruleBones[r].push_back( bone );
				else
					ri.Printf( PRINT_WARNING, "phys: %s: pin bone '%s' not found\n", modelName, def.pins[r].bones[b].c_str() );
			}
		}
	}

	gi.freeFactor.assign( gi.topo.numParticles, 1.0f );

	for ( int i = 0; i < gi.topo.numParticles; i++ )
	{
		const int first = gi.topo.firstVertex[i];
		const int s = first >> 24;
		const int vertex = first & 0xFFFFFF;

		gi.freeFactor[i] = Phys_PinFactor( def, aHeader, ruleBone, ruleBones, &gi.topo.bindPos[i * 3], surfs[s], vertex );
	}

	// Reference bone, as in Chaos: of the bones that move the pinned vertices, the bone nearest the root. A bone
	// with less than PHYS_REF_MIN_SHARE of the weight is not used. At the same depth, the largest weight.
	std::map<int, float> pinWeight;
	float pinTotal = 0.0f;
	int numPinned = 0;

	gi.refBone = -1;
	VectorClear( gi.refBind );

	for ( int i = 0; i < gi.topo.numParticles && aHeader && !def.lag; i++ )
	{
		if ( gi.freeFactor[i] > PHYS_MIN_PIN_FREE )
			continue;

		const int first = gi.topo.firstVertex[i];
		const mdxmSurface_t *surf = surfs[first >> 24];
		const mdxmVertex_t *v = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts ) + ( first & 0xFFFFFF );
		const int *boneRefs = (const int *)( (const byte *)surf + surf->ofsBoneReferences );
		const int numWeights = G2_GetVertWeights( v );
		float total = 0.0f;

		for ( int k = 0; k < numWeights; k++ )
		{
			const float w = G2_GetVertBoneWeight( v, k, total, numWeights );

			pinWeight[boneRefs[G2_GetVertBoneIndex( v, k )]] += w;
			pinTotal += w;
		}

		VectorAdd( gi.refBind, &gi.topo.bindPos[i * 3], gi.refBind );
		numPinned++;
	}

	int refDepth = 1 << 30;
	float refWeight = 0.0f;

	for ( std::map<int, float>::const_iterator it = pinWeight.begin(); it != pinWeight.end(); ++it )
	{
		if ( it->second < pinTotal * PHYS_REF_MIN_SHARE )
			continue;

		int depth = 0;

		for ( int p = Phys_Skel( aHeader, it->first )->parent; p >= 0 && depth < 256; p = Phys_Skel( aHeader, p )->parent )
			depth++;

		if ( depth < refDepth || ( depth == refDepth && it->second > refWeight ) )
		{
			gi.refBone = it->first;
			refDepth = depth;
			refWeight = it->second;
		}
	}

	if ( numPinned )
		VectorScale( gi.refBind, 1.0f / (float)numPinned, gi.refBind );

	if ( ph_debug->integer && gi.refBone >= 0 )
		ri.Printf( PRINT_ALL, "phys: %s group '%s': reference bone %s, %d pinned particle(s)\n", modelName, def.name.c_str(),
			Phys_Skel( aHeader, gi.refBone )->name, numPinned );

	// Texture coordinates and triangles of the surfaces: the input of the subdivisions.
	std::vector<std::vector<float> > st( numActive );
	std::vector<std::vector<int> > sdTris( numActive );
	std::vector<physSubdivInput_t> sdIn( numActive );

	for ( int s = 0; s < numActive; s++ )
	{
		const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surfs[s] + surfs[s]->ofsVerts );
		const mdxmVertexTexCoord_t *tc = (const mdxmVertexTexCoord_t *)&verts[surfs[s]->numVerts];

		st[s].resize( surfs[s]->numVerts * 2 + 2 );

		for ( int i = 0; i < surfs[s]->numVerts; i++ )
		{
			st[s][i * 2] = tc[i].texCoords[0];
			st[s][i * 2 + 1] = tc[i].texCoords[1];
		}

		sdIn[s].numVerts = surfs[s]->numVerts;
		sdIn[s].st = &st[s][0];
		sdIn[s].numTris = surfs[s]->numTriangles;
		sdIn[s].tris = inputs[s].tris;
	}

	// simsubdivide: the simulation runs on a refined mesh. The split is linear, so the mesh keeps the shape of the model.
	gi.baseTopo = physTopology_t();
	gi.simSubdiv = physSubdiv_t();

	if ( def.simSubdivide > 0 && !def.lag )
	{
		const std::vector<float> linear( gi.topo.numParticles, 0.0f );

		Phys_BuildSubdiv( gi.simSubdiv, gi.topo, &sdIn[0], numActive, &linear[0], def.simSubdivide,
			SHADER_MAX_VERTEXES - 200, SHADER_MAX_INDEXES - 600, PHYS_MAX_SIM_VERTS );

		if ( gi.simSubdiv.levels > 0 )
		{
			const std::vector<float> baseFree = gi.freeFactor;

			gi.baseTopo = gi.topo;
			Phys_SubdivTopology( gi.topo, gi.simSubdiv, gi.baseTopo );
			gi.freeFactor.resize( gi.topo.numParticles );
			Phys_ApplySubdivScalar( gi.simSubdiv, &baseFree[0], &gi.freeFactor[0] );
			gi.baseTarget.assign( gi.baseTopo.numParticles * 3, 0.0f );
			gi.baseSkin.assign( gi.baseTopo.numParticles * 3, 0.0f );

			// The refined surfaces are the input of the drawn subdivision.
			for ( int s = 0; s < numActive; s++ )
			{
				const physSubdivSurface_t &surf = gi.simSubdiv.surfaces[s];

				Phys_SubdivSurfaceTris( surf, sdTris[s] );
				sdIn[s].numVerts = (int)surf.point.size();
				sdIn[s].st = surf.st.empty() ? NULL : &surf.st[0];
				sdIn[s].numTris = (int)sdTris[s].size() / 3;
				sdIn[s].tris = sdTris[s].empty() ? NULL : &sdTris[s][0];
			}
		}

		if ( ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: %s group '%s': simsubdivide %d of %d, %d particle(s)\n", modelName, def.name.c_str(),
				gi.simSubdiv.levels, def.simSubdivide, gi.topo.numParticles );
	}

	const int n = gi.topo.numParticles;
	int numFree = 0;

	for ( int i = 0; i < n; i++ )
	{
		if ( gi.freeFactor[i] > 0.01f )
			numFree++;
	}

	gi.proxy = physProxy_t();
	gi.lag = physLag_t();
	gi.primed = false;

	if ( !def.lag )
	{
		// The soft transform of the lag mode has no per particle state. A cloth runs on a coarse mesh.
		// Each simsubdivide level halves the cell, so the proxy keeps the new particles.
		const float cell = ( def.proxy > 0.0f ? def.proxy : PHYS_DEFAULT_PROXY ) / (float)( 1 << gi.simSubdiv.levels );

		Phys_BuildProxy( gi.proxy, gi.topo, &gi.freeFactor[0], cell );
		Phys_ClothInit( gi.cloth, def, gi.proxy.topo, &gi.proxy.freeFactor[0] );
		gi.proxyTarget.assign( gi.proxy.numProxy * 3, 0.0f );
	}

	gi.worldPos.assign( n * 3, 0.0f );

	gi.target.assign( n * 3, 0.0f );
	gi.modelPos.assign( n * 3, 0.0f );
	gi.particleNormal.assign( n * 3, 0.0f );
	gi.particleSkin.assign( n * 3, 0.0f );

	gi.skinXYZ.assign( numActive, std::vector<float>() );
	gi.skinNormal.assign( numActive, std::vector<float>() );
	gi.outXYZ.assign( numActive, std::vector<float>() );
	gi.outNormal.assign( numActive, std::vector<float>() );
	gi.out.assign( numActive, physSurfaceOut_t() );
	gi.slotOfSurface.assign( Phys_Header( glm )->numSurfaces, -1 );

	for ( int s = 0; s < numActive; s++ )
	{
		const int count = surfs[s]->numVerts;

		gi.skinXYZ[s].assign( count * 3, 0.0f );
		gi.skinNormal[s].assign( count * 3, 0.0f );
		gi.outXYZ[s].assign( count * 3, 0.0f );
		gi.outNormal[s].assign( count * 3, 0.0f );
		gi.out[s].numVerts = count;
		gi.out[s].xyz = count ? &gi.outXYZ[s][0] : NULL;
		gi.out[s].normal = count ? &gi.outNormal[s][0] : NULL;
		gi.slotOfSurface[gi.active[s]] = s;
	}

	// Subdivided draw mesh. The pinned particles are not smoothed, so the mesh stays on the body there.
	gi.subdiv = physSubdiv_t();

	if ( def.subdivide > 0 )
	{
		Phys_BuildSubdiv( gi.subdiv, gi.topo, &sdIn[0], numActive, &gi.freeFactor[0], def.subdivide,
			SHADER_MAX_VERTEXES - 200, SHADER_MAX_INDEXES - 600, PHYS_MAX_SUBDIV_VERTS );

		if ( ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: %s group '%s': subdivide %d of %d, %d point(s)\n", modelName, def.name.c_str(),
				gi.subdiv.levels, def.subdivide, gi.subdiv.numPoints );
	}

	// A refined simulation mesh with no more drawn levels: each drawn point is its particle.
	if ( gi.subdiv.levels == 0 && gi.simSubdiv.levels > 0 )
		Phys_SubdivIdentity( gi.subdiv, gi.simSubdiv );

	if ( gi.subdiv.levels > 0 )
	{
		gi.pointXYZ.assign( gi.subdiv.numPoints * 3, 0.0f );
		gi.pointNormal.assign( gi.subdiv.numPoints * 3, 0.0f );
		gi.drawNormal.assign( n * 3, 0.0f );

		for ( int s = 0; s < numActive; s++ )
		{
			const physSubdivSurface_t &sds = gi.subdiv.surfaces[s];
			const int count = (int)sds.point.size();

			gi.outXYZ[s].assign( count * 3, 0.0f );
			gi.outNormal[s].assign( count * 3, 0.0f );
			gi.out[s].numVerts = count;
			gi.out[s].xyz = count ? &gi.outXYZ[s][0] : NULL;
			gi.out[s].normal = count ? &gi.outNormal[s][0] : NULL;
			gi.out[s].st = count ? &sds.st[0] : NULL;
			gi.out[s].chunks = count ? &sds.chunks[0] : NULL;
			gi.out[s].indexes = count ? &sds.indexes[0] : NULL;
			gi.out[s].numChunks = (int)sds.chunks.size() / 4;
			gi.out[s].numIndexes = (int)sds.indexes.size();
		}
	}

	// Colliders.
	gi.colliders.clear();

	std::vector<physCollider_t> expanded;

	std::vector<physCollider_t> fitted;

	if ( def.modelFit && gi.owner )
		Phys_FitColliders( glm, aHeader, gi.owner, def.modelFitScale, fitted );

	Phys_ExpandColliders( def, expanded, &fitted );

	for ( size_t c = 0; c < expanded.size(); c++ )
	{
		const physCollider_t &src = expanded[c];
		physResolvedCollider_t rc;

		rc.boneA = Phys_FindBone( aHeader, src.boneA );
		rc.boneB = ( src.capsule && !src.box ) ? Phys_FindBone( aHeader, src.boneB ) : rc.boneA;
		rc.box = src.box;

		if ( rc.boneA < 0 || rc.boneB < 0 )
		{
			if ( ph_debug->integer )
				ri.Printf( PRINT_ALL, "phys: %s: collider bone '%s' or '%s' not found, skipped\n", modelName, src.boneA.c_str(), src.boneB.c_str() );

			continue;
		}

		float a[3], b[3];
		Phys_BindOrigin( aHeader, rc.boneA, a );
		Phys_BindOrigin( aHeader, rc.boneB, b );

		for ( int k = 0; k < 3; k++ )
		{
			rc.bindA[k] = a[k] + src.offA[k];
			rc.bindB[k] = b[k] + src.offB[k];
		}

		for ( int k = 0; k < 3; k++ )
			rc.half[k] = ( src.box && src.size[k] > src.radius ) ? src.size[k] - src.radius : 0.0f;

		rc.radius = src.radius;
		rc.kind = src.kind;
		gi.colliders.push_back( rc );
	}

	// Depth of each particle in each collider in the bind pose: the rest shape can be that deep.
	std::vector<physCollider_w> bindColliders( gi.colliders.size() );

	for ( size_t c = 0; c < gi.colliders.size(); c++ )
	{
		const physResolvedCollider_t &rc = gi.colliders[c];
		physCollider_w &w = bindColliders[c];

		memset( &w, 0, sizeof( w ) );
		VectorCopy( rc.bindA, w.a );
		VectorCopy( rc.bindB, w.b );
		w.radius = rc.radius;
		w.shape = rc.box ? PHYS_SHAPE_BOX : PHYS_SHAPE_SEGMENT;

		for ( int k = 0; k < 3; k++ )
		{
			w.axis[k][k] = 1.0f;
			w.half[k] = rc.half[k];
		}
	}

	const physCollider_w *bindData = bindColliders.empty() ? NULL : &bindColliders[0];

	Phys_RestDistances( &gi.topo.bindPos[0], n, bindData, (int)bindColliders.size(), gi.restDist );

	if ( !def.lag && gi.proxy.numProxy > 0 )
		Phys_ClothRestDistances( gi.cloth, gi.proxy.topo, bindData, (int)bindColliders.size() );

	gi.usable = true;

	if ( ph_debug->integer )
	{
		ri.Printf( PRINT_ALL, "phys: %s group '%s' (%s): %d surface(s), %d particle(s), %d free, %d solved, %d edge(s), %d collider(s)\n",
			modelName, def.name.c_str(), def.lag ? "lag" : "cloth", numActive, n, numFree,
			def.lag ? 0 : gi.proxy.numProxy,
			(int)gi.proxy.topo.edgeRest.size(), (int)gi.colliders.size() );
	}
}

//
// Per frame update
//

static inline void Phys_ModelToWorld( const trRefEntity_t *ent, const float *p, float *out )
{
	for ( int k = 0; k < 3; k++ )
		out[k] = ent->e.origin[k] + p[0] * ent->e.axis[0][k] + p[1] * ent->e.axis[1][k] + p[2] * ent->e.axis[2][k];
}

static inline void Phys_WorldToModel( const trRefEntity_t *ent, const float *w, float *out )
{
	const float d[3] = { w[0] - ent->e.origin[0], w[1] - ent->e.origin[1], w[2] - ent->e.origin[2] };

	for ( int k = 0; k < 3; k++ )
	{
		const float len2 = DotProduct( ent->e.axis[k], ent->e.axis[k] );

		out[k] = len2 > 1e-6f ? DotProduct( d, ent->e.axis[k] ) / len2 : 0.0f;
	}
}

//
// Debug view of the colliders (ph_debug 2) and views of the editor: thin lines and round dots that face the view.
//

#define PHYS_MAX_DEBUG_QUADS	32768	// quads of one frame, both passes included
#define PHYS_LINE_WIDTH			0.2f
#define PHYS_HIDDEN_ALPHA		0.35f	// alpha of the copy without depth test: the hidden parts stay visible
#define PHYS_EYE_BIAS			0.3f	// move toward the view: a line on a surface is not hidden by that surface

#define PHYS_VIEW_COLLIDERS		1
#define PHYS_VIEW_PINS			2
#define PHYS_VIEW_PARTICLES		4
#define PHYS_VIEW_MAXDIST		8

// Two sets: with the render thread, the backend can still read the set of the previous frame.
static polyVert_t	s_dbgVerts[2][PHYS_MAX_DEBUG_QUADS * 4];
static srfPoly_t	s_dbgPolys[2][PHYS_MAX_DEBUG_QUADS];
static int			s_dbgFrame = -1;
static int			s_dbgCount = 0;
static int			s_dbgBuf = 0;
static float		s_dbgEye[3];		// view origin, in the model space of the entity
static float		s_dbgLeft[3];		// view directions, in the model space of the entity
static float		s_dbgUp[3];
static float		s_dbgWidth = PHYS_LINE_WIDTH;
static float		s_dbgBias = PHYS_EYE_BIAS;

// The editor menu shows the colliders of one group of the edited model.
static qhandle_t	s_editHandle = 0;
static int			s_viewFlags = 0;		// PHYS_VIEW_*
static int			s_viewGroup = -1;		// -1 = all groups
static int			s_viewPin = -1;			// selected pin rule, -1 = none
static int			s_collSelA = -1;		// bones of the selected collider
static int			s_collSelB = -1;
static char			s_collSelKind = 0;

// The left and up directions of the view, in the model space of the entity.
static void Phys_ViewAxes( const trRefEntity_t *ent, float left[3], float up[3] )
{
	for ( int k = 0; k < 3; k++ )
	{
		const float len2 = DotProduct( ent->e.axis[k], ent->e.axis[k] );

		left[k] = len2 > 1e-6f ? DotProduct( tr.refdef.viewaxis[1], ent->e.axis[k] ) / len2 : 0.0f;
		up[k] = len2 > 1e-6f ? DotProduct( tr.refdef.viewaxis[2], ent->e.axis[k] ) / len2 : 0.0f;
	}
}

// Two unit directions perpendicular to the unit vector n.
static void Phys_Perpendiculars( const float n[3], float u[3], float v[3] )
{
	const float ref[3] = { fabsf( n[2] ) < 0.9f ? 0.0f : 1.0f, 0.0f, fabsf( n[2] ) < 0.9f ? 1.0f : 0.0f };

	u[0] = n[1] * ref[2] - n[2] * ref[1];
	u[1] = n[2] * ref[0] - n[0] * ref[2];
	u[2] = n[0] * ref[1] - n[1] * ref[0];
	VectorNormalize( u );
	v[0] = n[1] * u[2] - n[2] * u[1];
	v[1] = n[2] * u[0] - n[0] * u[2];
	v[2] = n[0] * u[1] - n[1] * u[0];
}

// The view of the entity for the lines and dots that follow. width is the width of a line.
static void Phys_DrawBegin( const trRefEntity_t *ent, float width, float bias )
{
	Phys_WorldToModel( ent, tr.viewParms.ori.origin, s_dbgEye );
	Phys_ViewAxes( ent, s_dbgLeft, s_dbgUp );
	s_dbgWidth = width;
	s_dbgBias = bias;
}

// Moves p toward the view. The point stays at the same place on the screen.
static void Phys_TowardEye( const float p[3], float out[3] )
{
	float d[3];

	VectorSubtract( s_dbgEye, p, d );

	const float len = VectorLength( d );
	const float t = len > s_dbgBias * 2.0f ? s_dbgBias / len : 0.0f;

	VectorMA( p, t, d, out );
}

// One quad, two times: with depth test at full alpha, and without depth test at PHYS_HIDDEN_ALPHA.
static void Phys_EmitQuad( const float xyz[4][3], const float st[4][2], const byte col[4] )
{
	if ( s_dbgFrame != tr.frameCount )
	{
		s_dbgFrame = tr.frameCount;
		s_dbgCount = 0;
		s_dbgBuf = Phys_DebugBuffer();
	}

	if ( s_dbgCount + 2 > PHYS_MAX_DEBUG_QUADS || !tr.physLineShader || !tr.physHiddenShader )
		return;

	for ( int pass = 0; pass < 2; pass++ )
	{
		polyVert_t *v = &s_dbgVerts[s_dbgBuf][s_dbgCount * 4];
		srfPoly_t *poly = &s_dbgPolys[s_dbgBuf][s_dbgCount];
		const byte alpha = pass ? (byte)( col[3] * PHYS_HIDDEN_ALPHA ) : col[3];

		for ( int i = 0; i < 4; i++ )
		{
			VectorCopy( xyz[i], v[i].xyz );
			v[i].st[0] = st[i][0];
			v[i].st[1] = st[i][1];
			v[i].modulate[0] = col[0];
			v[i].modulate[1] = col[1];
			v[i].modulate[2] = col[2];
			v[i].modulate[3] = alpha;
		}

		poly->surfaceType = SF_POLY;
		poly->hShader = 0;
		poly->fogIndex = 0;
		poly->numVerts = 4;
		poly->verts = v;
		R_AddDrawSurf( (surfaceType_t *)poly, pass ? tr.physHiddenShader : tr.physLineShader, 0, qfalse );
		s_dbgCount++;
	}
}

// A line from a to b: a strip that faces the view. The texture is soft across the strip.
static void Phys_Line( const float a[3], const float b[3], const byte col[4] )
{
	static const float st[4][2] = { { 0.0f, 0.5f }, { 1.0f, 0.5f }, { 1.0f, 0.5f }, { 0.0f, 0.5f } };
	float pa[3], pb[3], d[3], mid[3], toEye[3], side[3], xyz[4][3];

	Phys_TowardEye( a, pa );
	Phys_TowardEye( b, pb );
	VectorSubtract( pb, pa, d );

	if ( DotProduct( d, d ) < 1e-8f )
		return;

	for ( int k = 0; k < 3; k++ )
		mid[k] = ( pa[k] + pb[k] ) * 0.5f;

	VectorSubtract( s_dbgEye, mid, toEye );
	CrossProduct( d, toEye, side );

	if ( VectorNormalize( side ) < 1e-6f )
		VectorCopy( s_dbgLeft, side );

	const float half = s_dbgWidth * 0.5f;

	for ( int k = 0; k < 3; k++ )
	{
		xyz[0][k] = pa[k] - side[k] * half;
		xyz[1][k] = pa[k] + side[k] * half;
		xyz[2][k] = pb[k] + side[k] * half;
		xyz[3][k] = pb[k] - side[k] * half;
	}

	Phys_EmitQuad( xyz, st, col );
}

// A round dot of the given radius that faces the view.
static void Phys_Point( const float p[3], float radius, const byte col[4] )
{
	static const float corner[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
	static const float st[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
	float c[3], xyz[4][3];

	Phys_TowardEye( p, c );

	for ( int i = 0; i < 4; i++ )
	{
		for ( int k = 0; k < 3; k++ )
			xyz[i][k] = c[k] + ( s_dbgLeft[k] * corner[i][0] + s_dbgUp[k] * corner[i][1] ) * radius;
	}

	Phys_EmitQuad( xyz, st, col );
}

// An arc around center, from angle a0 to a1 in the plane of u and v, with the radii ru and rv.
// Bind pose points when m is set.
static void Phys_DrawArc( const mdxaBone_t *m, const float center[3], const float u[3], const float v[3],
						  float ru, float rv, float a0, float a1, int count, const byte col[4] )
{
	float prev[3];

	for ( int i = 0; i <= count; i++ )
	{
		const float ang = a0 + ( a1 - a0 ) * (float)i / (float)count;
		const float c = cosf( ang ) * ru, s = sinf( ang ) * rv;
		float p[3] = { center[0] + u[0] * c + v[0] * s, center[1] + u[1] * c + v[1] * s, center[2] + u[2] * c + v[2] * s };

		if ( m )
		{
			const float b[3] = { p[0], p[1], p[2] };

			for ( int k = 0; k < 3; k++ )
				p[k] = DotProduct( m->matrix[k], b ) + m->matrix[k][3];
		}

		if ( i )
			Phys_Line( prev, p, col );

		VectorCopy( p, prev );
	}
}

static void Phys_DrawRing( const mdxaBone_t *m, const float center[3], const float u[3], const float v[3],
						   float ru, float rv, int count, const byte col[4] )
{
	Phys_DrawArc( m, center, u, v, ru, rv, 0.0f, 6.2831853f, count, col );
}

// A bind pose point on the animated model.
static void Phys_BindToModel( const mdxaBone_t &m, const float bind[3], float out[3] )
{
	for ( int k = 0; k < 3; k++ )
		out[k] = DotProduct( m.matrix[k], bind ) + m.matrix[k][3];
}

// A rounded box: the faces of the inner box moved out by the corner radius, and a quarter arc at each
// corner between two faces. axes are unit vectors.
static void Phys_DrawBox( const float c[3], const float axes[3][3], const float half[3], float round, const byte col[4] )
{
	if ( round < 0.05f )
	{
		// A sharp box: the 12 edges.
		for ( int k = 0; k < 3; k++ )
		{
			const int i = ( k + 1 ) % 3, j = ( k + 2 ) % 3;

			for ( int s = 0; s < 4; s++ )
			{
				const float si = ( s & 1 ) ? 1.0f : -1.0f, sj = ( s & 2 ) ? 1.0f : -1.0f;
				float p0[3], p1[3];

				for ( int n = 0; n < 3; n++ )
				{
					const float base = c[n] + axes[i][n] * si * ( half[i] + round ) + axes[j][n] * sj * ( half[j] + round );

					p0[n] = base - axes[k][n] * ( half[k] + round );
					p1[n] = base + axes[k][n] * ( half[k] + round );
				}

				Phys_Line( p0, p1, col );
			}
		}

		return;
	}

	// The faces: a rectangle with the size of the inner box on each side.
	for ( int k = 0; k < 3; k++ )
	{
		const int i = ( k + 1 ) % 3, j = ( k + 2 ) % 3;

		for ( int side = 0; side < 2; side++ )
		{
			const float sk = side ? 1.0f : -1.0f;
			float p[4][3];

			for ( int q = 0; q < 4; q++ )
			{
				const float si = ( q == 1 || q == 2 ) ? 1.0f : -1.0f, sj = ( q >= 2 ) ? 1.0f : -1.0f;

				for ( int n = 0; n < 3; n++ )
					p[q][n] = c[n] + axes[k][n] * sk * ( half[k] + round ) + axes[i][n] * si * half[i] + axes[j][n] * sj * half[j];
			}

			for ( int q = 0; q < 4; q++ )
				Phys_Line( p[q], p[( q + 1 ) % 4], col );
		}
	}

	// The corners: three quarter arcs between the faces that meet there.
	for ( int corner = 0; corner < 8; corner++ )
	{
		const float sign[3] = { ( corner & 1 ) ? 1.0f : -1.0f, ( corner & 2 ) ? 1.0f : -1.0f, ( corner & 4 ) ? 1.0f : -1.0f };
		float q[3], dir[3][3];

		for ( int n = 0; n < 3; n++ )
			q[n] = c[n] + axes[0][n] * sign[0] * half[0] + axes[1][n] * sign[1] * half[1] + axes[2][n] * sign[2] * half[2];

		for ( int k = 0; k < 3; k++ )
			VectorScale( axes[k], sign[k], dir[k] );

		for ( int k = 0; k < 3; k++ )
			Phys_DrawArc( NULL, q, dir[k], dir[( k + 1 ) % 3], round, round, 0.0f, 1.5707963f, 4, col );
	}
}

// a and b are the ends of a capsule, or the center of a box with its unit axes, in the model space of the entity.
// Colors: humanoid orange, model fit yellow, collide line green, selected cyan. The editor legend uses the same colors.
static void Phys_DrawCollider( const float a[3], const float b[3], const float axes[3][3], const physResolvedCollider_t &rc, bool selected )
{
	byte col[4] = { 255, 150, 0, 255 };
	const float radius = rc.radius;

	if ( selected )
	{
		col[0] = 0;
		col[1] = 255;
		col[2] = 255;
	}
	else if ( rc.kind == 'm' )
	{
		col[1] = 230;
		col[2] = 60;
	}
	else if ( rc.kind == 'c' )
	{
		col[0] = 60;
		col[1] = 255;
		col[2] = 90;
	}

	s_dbgWidth = selected ? 0.3f : PHYS_LINE_WIDTH;

	if ( rc.box )
	{
		Phys_DrawBox( a, axes, rc.half, radius, col );
		s_dbgWidth = PHYS_LINE_WIDTH;
		return;
	}

	float axis[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
	const float len = sqrtf( DotProduct( axis, axis ) );

	if ( len < 0.5f )
	{
		// A sphere: three great circles.
		static const float ex[3] = { 1.0f, 0.0f, 0.0f }, ey[3] = { 0.0f, 1.0f, 0.0f }, ez[3] = { 0.0f, 0.0f, 1.0f };

		Phys_DrawRing( NULL, a, ex, ey, radius, radius, 32, col );
		Phys_DrawRing( NULL, a, ey, ez, radius, radius, 32, col );
		Phys_DrawRing( NULL, a, ez, ex, radius, radius, 32, col );
		s_dbgWidth = PHYS_LINE_WIDTH;
		return;
	}

	VectorScale( axis, 1.0f / len, axis );

	float u[3], v[3], back[3];

	Phys_Perpendiculars( axis, u, v );
	VectorScale( axis, -1.0f, back );

	// The end circles, four lines along the side, and the round ends.
	Phys_DrawRing( NULL, a, u, v, radius, radius, 24, col );
	Phys_DrawRing( NULL, b, u, v, radius, radius, 24, col );

	for ( int i = 0; i < 4; i++ )
	{
		const float *dir = ( i & 1 ) ? v : u;
		const float s = ( i & 2 ) ? -radius : radius;
		float p0[3], p1[3];

		VectorMA( a, s, dir, p0 );
		VectorMA( b, s, dir, p1 );
		Phys_Line( p0, p1, col );
	}

	Phys_DrawArc( NULL, b, u, axis, radius, radius, 0.0f, 3.1415927f, 12, col );
	Phys_DrawArc( NULL, b, v, axis, radius, radius, 0.0f, 3.1415927f, 12, col );
	Phys_DrawArc( NULL, a, u, back, radius, radius, 0.0f, 3.1415927f, 12, col );
	Phys_DrawArc( NULL, a, v, back, radius, radius, 0.0f, 3.1415927f, 12, col );
	s_dbgWidth = PHYS_LINE_WIDTH;
}

// Editor view of a group: the pin rules (red = fixed, green = free), the free factor of each particle and
// the maxdist limit. Everything is in the model space of the entity.
static void Phys_DrawGroupView( const physGroupInst_t &gi, const trRefEntity_t *ent, CBoneCache *bc )
{
	const physGroupDef_t &def = *gi.def;
	const mdxaHeader_t *aHeader = G2Phys_BoneHeader( bc );
	const int n = gi.topo.numParticles;

	Phys_DrawBegin( ent, PHYS_LINE_WIDTH, PHYS_EYE_BIAS );

	// Free factor: red (fixed), yellow, green (free).
	if ( s_viewFlags & PHYS_VIEW_PARTICLES )
	{
		for ( int i = 0; i < n; i++ )
		{
			const float f = gi.freeFactor[i];
			const byte col[4] = { (byte)( f < 0.5f ? 255 : 510.0f * ( 1.0f - f ) ), (byte)( f < 0.5f ? 510.0f * f : 255 ), 0, 255 };

			Phys_Point( &gi.modelPos[i * 3], 0.4f, col );
		}
	}

	// The limit of some free particles, around their animated position.
	if ( ( s_viewFlags & PHYS_VIEW_MAXDIST ) && def.maxDist > 0.0f )
	{
		const byte col[4] = { 90, 160, 255, 255 };
		int numFree = 0;

		for ( int i = 0; i < n; i++ )
			numFree += gi.freeFactor[i] > 0.01f;

		const int step = numFree > 24 ? numFree / 24 : 1;

		s_dbgWidth = 0.12f;

		for ( int i = 0, k = 0; i < n; i++ )
		{
			if ( gi.freeFactor[i] <= 0.01f || ( k++ % step ) )
				continue;

			const float *nrm = &gi.particleSkin[i * 3];
			const float r = def.lag ? def.maxDist : def.maxDist * gi.freeFactor[i];
			const float rn = ( !def.lag && def.maxDistN > 0.0f ) ? r * def.maxDistN / def.maxDist : r;
			float center[3], u[3], v[3];

			Phys_WorldToModel( ent, &gi.target[i * 3], center );
			Phys_Perpendiculars( nrm, u, v );
			Phys_DrawRing( NULL, center, u, v, r, r, 24, col );
			Phys_DrawRing( NULL, center, u, nrm, r, rn, 24, col );
		}

		s_dbgWidth = PHYS_LINE_WIDTH;
	}

	if ( !( s_viewFlags & PHYS_VIEW_PINS ) || !aHeader )
		return;

	for ( size_t r = 0; r < def.pins.size(); r++ )
	{
		const physPinRule_t &rule = def.pins[r];
		const bool sel = s_viewPin < 0 || s_viewPin == (int)r;
		const bool current = s_viewPin == (int)r;
		const byte fixedCol[4] = { (byte)( sel ? 255 : 120 ), 40, 40, 255 };
		const byte freeCol[4] = { 40, (byte)( sel ? 255 : 120 ), 60, 255 };
		const byte boneCol[4] = { (byte)( sel ? 255 : 130 ), (byte)( sel ? 255 : 130 ), (byte)( sel ? 255 : 130 ), 255 };

		s_dbgWidth = current ? 0.3f : 0.18f;

		if ( rule.type == PHYS_PIN_BONES )
		{
			const byte col[4] = { (byte)( sel ? 255 : 130 ), 60, (byte)( sel ? 255 : 130 ), 255 };

			for ( size_t b = 0; b < rule.bones.size(); b++ )
			{
				const int bone = Phys_FindBone( aHeader, rule.bones[b] );
				float o[3], p[3];

				if ( bone < 0 )
					continue;

				Phys_BindOrigin( aHeader, bone, o );
				Phys_BindToModel( G2Phys_BoneMatrix( bc, bone ), o, p );
				Phys_Point( p, current ? 1.3f : 0.9f, col );
			}

			continue;
		}

		if ( rule.type != PHYS_PIN_GRADIENT && rule.type != PHYS_PIN_RADIUS )
			continue;

		const int bone = Phys_FindBone( aHeader, rule.bone );

		if ( bone < 0 )
			continue;

		const mdxaBone_t &m = G2Phys_BoneMatrix( bc, bone );
		float o[3], p[3];

		Phys_BindOrigin( aHeader, bone, o );
		Phys_BindToModel( m, o, p );
		Phys_Point( p, current ? 1.1f : 0.7f, boneCol );

		if ( rule.type == PHYS_PIN_RADIUS )
		{
			static const float axes[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
			const float c[3] = { o[0] + rule.axis[0], o[1] + rule.axis[1], o[2] + rule.axis[2] };

			for ( int a = 0; a < 3; a++ )
			{
				const float *u = axes[a], *v = axes[( a + 1 ) % 3];

				Phys_DrawRing( &m, c, u, v, rule.pinned, rule.pinned, 40, fixedCol );
				Phys_DrawRing( &m, c, u, v, rule.free, rule.free, 40, freeCol );
			}

			continue;
		}

		// Gradient: a plane across the axis at the fixed and at the free distance. The rings go around the
		// cloth: their center is on the line through the middle of the particles, parallel to the axis.
		float mid[3] = { 0.0f, 0.0f, 0.0f };

		for ( int i = 0; i < n; i++ )
		{
			for ( int k = 0; k < 3; k++ )
				mid[k] += gi.topo.bindPos[i * 3 + k] / (float)n;
		}

		const float along = ( mid[0] - o[0] ) * rule.axis[0] + ( mid[1] - o[1] ) * rule.axis[1] + ( mid[2] - o[2] ) * rule.axis[2];
		const float line[3] = { mid[0] - rule.axis[0] * along, mid[1] - rule.axis[1] * along, mid[2] - rule.axis[2] * along };
		float radius = 0.0f;

		for ( int i = 0; i < n; i++ )
		{
			const float *bp = &gi.topo.bindPos[i * 3];
			const float d[3] = { bp[0] - line[0], bp[1] - line[1], bp[2] - line[2] };
			const float t = DotProduct( d, rule.axis );
			const float q[3] = { d[0] - rule.axis[0] * t, d[1] - rule.axis[1] * t, d[2] - rule.axis[2] * t };
			const float dist = sqrtf( DotProduct( q, q ) );

			radius = dist > radius ? dist : radius;
		}

		radius = radius < 3.0f ? 3.0f : ( radius > 40.0f ? 40.0f : radius ) + 1.0f;

		float u[3], v[3];
		const float lo = rule.pinned < rule.free ? rule.pinned : rule.free;
		const float hi = rule.pinned < rule.free ? rule.free : rule.pinned;
		const float t0 = lo < 0.0f ? lo : 0.0f, t1 = hi > 0.0f ? hi : 0.0f;

		Phys_Perpendiculars( rule.axis, u, v );

		for ( int k = 0; k < 2; k++ )
		{
			const float d = k ? rule.free : rule.pinned;
			const float c[3] = { line[0] + rule.axis[0] * d, line[1] + rule.axis[1] * d, line[2] + rule.axis[2] * d };

			Phys_DrawRing( &m, c, u, v, radius, radius, 48, k ? freeCol : fixedCol );
			Phys_DrawRing( &m, c, u, v, radius * 0.5f, radius * 0.5f, 24, k ? freeCol : fixedCol );
		}

		// The axis, from the bone to the farthest plane, with an arrow head in the direction of the axis.
		float bindA[3], bindB[3], pa[3], pb[3];

		for ( int k = 0; k < 3; k++ )
		{
			bindA[k] = line[k] + rule.axis[k] * t0;
			bindB[k] = line[k] + rule.axis[k] * t1;
		}

		Phys_BindToModel( m, bindA, pa );
		Phys_BindToModel( m, bindB, pb );
		Phys_Line( pa, pb, boneCol );

		for ( int i = 0; i < 4; i++ )
		{
			const float *dir = ( i & 1 ) ? v : u;
			const float s = ( i & 2 ) ? -0.8f : 0.8f;
			float bind[3], barb[3];

			for ( int k = 0; k < 3; k++ )
				bind[k] = bindB[k] - rule.axis[k] * 1.6f + dir[k] * s;

			Phys_BindToModel( m, bind, barb );
			Phys_Line( pb, barb, boneCol );
		}

		// The line from the bone origin to the axis.
		Phys_BindToModel( m, line, pa );
		Phys_Line( p, pa, boneCol );
	}

	s_dbgWidth = PHYS_LINE_WIDTH;
}

static void Phys_UpdateGroup( physInstance_t *inst, physGroupInst_t &gi, const trRefEntity_t *ent, const model_t *glm,
							  CBoneCache *bc, const surfaceInfo_v &slist, bool stale, float dt )
{
	const physGroupDef_t &def = *gi.def;
	const mdxmHeader_t *header = Phys_Header( glm );
	const int numSurfaces = header->numSurfaces;

	// Surfaces that are switched off leave the group.
	std::vector<unsigned char> &mask = gi.maskScratch;

	mask.assign( numSurfaces, 0 );

	for ( size_t i = 0; i < gi.matched.size(); i++ )
	{
		const int s = gi.matched[i];
		const surfaceInfo_t *override = G2_FindOverrideSurface( s, slist );
		const unsigned int offFlags = override ? override->offFlags : Phys_SurfInfo( header, s )->flags;

		if ( !offFlags )
			mask[s] = 1;
	}

	if ( !gi.built || mask != gi.activeMask )
	{
		gi.activeMask = mask;
		gi.active.clear();

		for ( size_t i = 0; i < gi.matched.size(); i++ )
		{
			if ( mask[gi.matched[i]] )
				gi.active.push_back( gi.matched[i] );
		}

		if ( ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: group '%s': %d surface(s) match, %d on\n", gi.def->name.c_str(), (int)gi.matched.size(), (int)gi.active.size() );

		Phys_BuildGroup( gi, glm, bc, glm->name );
		stale = true;
	}

	if ( !gi.usable )
		return;

	const int n = gi.topo.numParticles;
	const int numActive = (int)gi.active.size();

	// Animated positions of every vertex.
	for ( int s = 0; s < numActive; s++ )
	{
		const mdxmSurface_t *surf = (const mdxmSurface_t *)G2_FindSurface( glm, gi.active[s], PHYS_LOD );
		const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts );
		const int *boneRefs = (const int *)( (const byte *)surf + surf->ofsBoneReferences );

		for ( int i = 0; i < surf->numVerts; i++ )
			Phys_SkinVertex( bc, boneRefs, &verts[i], &gi.skinXYZ[s][i * 3], &gi.skinNormal[s][i * 3] );
	}

	// Animated position and normal of each particle. A refined particle blends the particles of the model mesh.
	const bool refined = gi.simSubdiv.levels > 0;
	const physTopology_t &base = refined ? gi.baseTopo : gi.topo;
	float *animPos = refined ? &gi.baseTarget[0] : &gi.target[0];
	float *animNormal = refined ? &gi.baseSkin[0] : &gi.particleSkin[0];

	for ( int i = 0; i < base.numParticles; i++ )
	{
		const int first = base.firstVertex[i];

		Phys_ModelToWorld( ent, &gi.skinXYZ[first >> 24][( first & 0xFFFFFF ) * 3], &animPos[i * 3] );
		VectorCopy( &gi.skinNormal[first >> 24][( first & 0xFFFFFF ) * 3], &animNormal[i * 3] );
	}

	if ( refined )
	{
		Phys_ApplySubdiv( gi.simSubdiv, &gi.baseTarget[0], &gi.target[0] );
		Phys_ApplySubdiv( gi.simSubdiv, &gi.baseSkin[0], &gi.particleSkin[0] );

		for ( int i = 0; i < n; i++ )
		{
			if ( VectorNormalize( &gi.particleSkin[i * 3] ) == 0.0f )
				gi.particleSkin[i * 3 + 2] = 1.0f;
		}
	}

	// Colliders of this frame, in world space.
	gi.prevWorldColliders.swap( gi.worldColliders );

	std::vector<physCollider_w> &colliders = gi.worldColliders;
	const bool editModel = s_viewFlags && ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) && s_editHandle && R_GetModelByHandle( s_editHandle ) == glm &&
		( s_viewGroup < 0 || ( s_viewGroup < (int)inst->def->groups.size() && gi.def == &inst->def->groups[s_viewGroup] ) );
	const bool editView = editModel && ( s_viewFlags & PHYS_VIEW_COLLIDERS );

	const bool drawColliders = ph_debug->integer == 2 || ph_debug->integer == 3 || editView;

	colliders.resize( gi.colliders.size() );

	if ( drawColliders )
		Phys_DrawBegin( ent, PHYS_LINE_WIDTH, PHYS_EYE_BIAS );

	for ( size_t c = 0; c < gi.colliders.size(); c++ )
	{
		const physResolvedCollider_t &rc = gi.colliders[c];
		const mdxaBone_t &ma = G2Phys_BoneMatrix( bc, rc.boneA );
		const mdxaBone_t &mb = G2Phys_BoneMatrix( bc, rc.boneB );
		physCollider_w &w = colliders[c];
		float a[3], b[3], axes[3][3];

		for ( int k = 0; k < 3; k++ )
		{
			a[k] = DotProduct( ma.matrix[k], rc.bindA ) + ma.matrix[k][3];
			b[k] = DotProduct( mb.matrix[k], rc.bindB ) + mb.matrix[k][3];
		}

		Phys_ModelToWorld( ent, a, w.a );
		Phys_ModelToWorld( ent, b, w.b );
		w.radius = rc.radius;
		w.shape = rc.box ? PHYS_SHAPE_BOX : PHYS_SHAPE_SEGMENT;

		if ( rc.box )
		{
			// The bind pose axes of the model, turned by the bone, then in world space.
			for ( int k = 0; k < 3; k++ )
			{
				axes[k][0] = ma.matrix[0][k];
				axes[k][1] = ma.matrix[1][k];
				axes[k][2] = ma.matrix[2][k];
				VectorNormalize( axes[k] );

				for ( int j = 0; j < 3; j++ )
					w.axis[k][j] = axes[k][0] * ent->e.axis[0][j] + axes[k][1] * ent->e.axis[1][j] + axes[k][2] * ent->e.axis[2][j];

				VectorNormalize( w.axis[k] );
				w.half[k] = rc.half[k];
			}
		}

		if ( drawColliders )
			Phys_DrawCollider( a, b, axes, rc, editView && rc.kind == s_collSelKind && rc.boneA == s_collSelA && rc.boneB == s_collSelB );

		if ( ph_debug->integer == 3 && c == 0 )
			ri.Printf( PRINT_ALL, "phys dots: %s origin (%.0f %.0f %.0f) frame %d\n", glm->name, ent->e.origin[0], ent->e.origin[1], ent->e.origin[2], tr.frameCount );
	}

	const physCollider_w *colliderData = colliders.empty() ? NULL : &colliders[0];
	const physCollider_w *prevColliderData = ( colliderData && gi.prevWorldColliders.size() == colliders.size() ) ? &gi.prevWorldColliders[0] : NULL;
	const float *restDist = ( colliderData && gi.restDist.size() == (size_t)n * colliders.size() ) ? &gi.restDist[0] : NULL;
	const bool hasGround = ( ent->e.renderfx & RF_SHADOW_PLANE ) != 0;

	// Model frame of the solver: the reference bone in world space.
	if ( !def.lag && gi.refBone >= 0 )
	{
		const mdxaBone_t &m = G2Phys_BoneMatrix( bc, gi.refBone );
		float o[3], refOrigin[3], refAxis[3][3];

		for ( int k = 0; k < 3; k++ )
			o[k] = DotProduct( m.matrix[k], gi.refBind ) + m.matrix[k][3];

		Phys_ModelToWorld( ent, o, refOrigin );

		// Axis k: the model axis k turned by the bone, then in world space.
		for ( int k = 0; k < 3; k++ )
		{
			for ( int j = 0; j < 3; j++ )
				refAxis[k][j] = m.matrix[0][k] * ent->e.axis[0][j] + m.matrix[1][k] * ent->e.axis[1][j] + m.matrix[2][k] * ent->e.axis[2][j];
		}

		Phys_ClothSetReference( gi.cloth, refOrigin, refAxis );
	}
	else if ( !def.lag )
	{
		Phys_ClothSetReference( gi.cloth, NULL, NULL );
	}

	const int solveStart = ri.Milliseconds();

	gi.cloth.debug = ph_debug->integer == 6;
	const bool reset = stale || !gi.primed;

	if ( reset )
	{
		if ( ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: reset %s at (%.0f %.0f %.0f) facing (%.2f %.2f)\n", glm->name, ent->e.origin[0], ent->e.origin[1], ent->e.origin[2], ent->e.axis[0][0], ent->e.axis[0][1] );

		if ( def.lag )
		{
			Phys_LagReset( gi.lag, &gi.target[0], &gi.freeFactor[0], n );
		}
		else
		{
			Phys_ProxyTargets( gi.proxy, &gi.target[0], &gi.proxyTarget[0] );
			Phys_ProxyReset( gi.proxy );
			Phys_ClothReset( gi.cloth, gi.proxy.topo, &gi.proxyTarget[0], ent->e.origin, ent->e.axis );
		}

		gi.primed = true;
	}
	else if ( dt > 0.0f )
	{
		float windVel[3] = { 0.0f, 0.0f, 0.0f };

		if ( def.wind > 0.0f )
		{
			vec3_t point, dir;
			float speed = 0.0f;

			VectorCopy( ent->e.origin, point );

			if ( R_GetWindVector( dir, point ) && R_GetWindSpeed( speed, point ) )
			{
				speed *= def.wind;

				if ( speed > PHYS_MAX_WIND )
					speed = PHYS_MAX_WIND;

				VectorScale( dir, speed, windVel );
			}
		}

		if ( def.lag )
		{
			Phys_LagStep( gi.lag, def, dt, &gi.target[0], &gi.freeFactor[0], n, windVel );
		}
		else
		{
			Phys_ProxyTargets( gi.proxy, &gi.target[0], &gi.proxyTarget[0] );
			Phys_ClothStep( gi.cloth, gi.proxy.topo, def, dt, &gi.proxyTarget[0], windVel, ent->e.origin, ent->e.axis,
				colliderData, prevColliderData,
				(int)colliders.size(), hasGround, ent->e.shadowPlane );
		}
	}

	// Solved positions of every particle, in world space.
	if ( def.lag )
	{
		Phys_LagApply( gi.lag, &gi.target[0], &gi.freeFactor[0], n, &gi.worldPos[0] );
		Phys_LagCollide( gi.lag, def, &gi.target[0], &gi.freeFactor[0], n, colliderData, (int)colliders.size(), restDist, hasGround, ent->e.shadowPlane,
			&gi.worldPos[0] );
	}
	else
	{
		// The full mesh follows the simulation mesh. The colliders push out the full vertices that a coarse cell takes in.
		Phys_ProxyApply( gi.proxy, &gi.cloth.drawX[0], &gi.proxyTarget[0], &gi.target[0], &gi.freeFactor[0], &gi.worldPos[0] );
		Phys_ProjectColliders( &gi.worldPos[0], &gi.target[0], &gi.freeFactor[0], n, def.margin, colliderData, (int)colliders.size(), restDist, hasGround, ent->e.shadowPlane );
	}

	// ph_debug 6: fast cloth. When a particle moves faster than PHYS_DBG_SPEED, the line gives the largest
	// correction of each constraint type in the frame, the largest move of an animated pinned particle (a pop
	// of the animation) in world and in model space, with the main bone of that particle, and the time of the solver.
	if ( ph_debug->integer == 6 && !def.lag )
	{
		const int solveMs = ri.Milliseconds() - solveStart;
		float pinPop = 0.0f, modelPop = 0.0f;
		int worstPin = -1;

		// The animated positions in model space: a move there is a move of the skeleton, not of the entity.
		gi.dbgModel.resize( n * 3 );

		for ( int i = 0; i < n; i++ )
			Phys_WorldToModel( ent, &gi.target[i * 3], &gi.dbgModel[i * 3] );

		if ( (int)gi.dbgTarget.size() == n * 3 && (int)gi.dbgPrevModel.size() == n * 3 && !reset )
		{
			for ( int i = 0; i < n; i++ )
			{
				if ( gi.freeFactor[i] > PHYS_MIN_PIN_FREE )
					continue;

				const float move = Distance( &gi.target[i * 3], &gi.dbgTarget[i * 3] );
				const float moveModel = Distance( &gi.dbgModel[i * 3], &gi.dbgPrevModel[i * 3] );

				pinPop = move > pinPop ? move : pinPop;

				if ( moveModel > modelPop )
				{
					modelPop = moveModel;
					worstPin = i;
				}
			}

			const float *c = gi.cloth.dbgCorr;

			if ( c[PHYS_CORR_SPEED] > PHYS_DBG_SPEED )
			{
				// The bone with the largest weight on that pinned particle.
				const physTopology_t &base = gi.simSubdiv.levels > 0 ? gi.baseTopo : gi.topo;
				const mdxaHeader_t *boneHeader = G2Phys_BoneHeader( bc );
				const char *pinBone = "-";

				if ( worstPin >= 0 && worstPin < base.numParticles && boneHeader )
				{
					const int first = base.firstVertex[worstPin];
					const mdxmSurface_t *surf = (const mdxmSurface_t *)G2_FindSurface( glm, gi.active[first >> 24], PHYS_LOD );
					const mdxmVertex_t *v = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts ) + ( first & 0xFFFFFF );
					const int *boneRefs = (const int *)( (const byte *)surf + surf->ofsBoneReferences );
					const int numWeights = G2_GetVertWeights( v );
					float total = 0.0f, best = -1.0f;

					for ( int k = 0; k < numWeights; k++ )
					{
						const float w = G2_GetVertBoneWeight( v, k, total, numWeights );

						if ( w > best )
						{
							best = w;
							pinBone = Phys_Skel( boneHeader, boneRefs[G2_GetVertBoneIndex( v, k )] )->name;
						}
					}
				}

				const float *bp = worstPin >= 0 ? &gi.topo.bindPos[worstPin * 3] : vec3_origin;

				ri.Printf( PRINT_ALL, "phys fast: '%s' t %d dt %.1f ms solve %d ms: speed %.0f, pin pop %.1f (model %.1f, bind %.0f %.0f %.0f, bone %s), correction edges %.2f shape %.2f tether %.2f hold %.2f reach %.2f collide %.2f (across %.2f)%s\n",
					def.name.c_str(), tr.refdef.time, dt * 1000.0f, solveMs, c[PHYS_CORR_SPEED], pinPop, modelPop, bp[0], bp[1], bp[2], pinBone,
					c[PHYS_CORR_EDGES], c[PHYS_CORR_SHAPE], c[PHYS_CORR_TETHER], c[PHYS_CORR_FOLLOW], c[PHYS_CORR_REACH], c[PHYS_CORR_COLLIDE], c[PHYS_CORR_CROSS],
					gi.cloth.teleported ? ", teleport" : "" );
			}
		}

		gi.dbgTarget = gi.target;
		gi.dbgPrevModel = gi.dbgModel;
	}

	// ph_debug 5: speed of the model frame (reference bone, or fit of the pinned particles) against the speed of
	// the entity. A frame that moves or turns faster than the entity drags the free particles through the damping.
	if ( ph_debug->integer == 5 && !def.lag && dt > 0.0f && !reset )
	{
		const float *om = gi.cloth.frameOmega;
		const float frameTurn = sqrtf( om[0] * om[0] + om[1] * om[1] + om[2] * om[2] );
		float entTurn = 0.0f;

		for ( int k = 0; k < 3; k++ )
		{
			const float c = DotProduct( ent->e.axis[k], gi.dbgAxis[k] ) / ( VectorLength( ent->e.axis[k] ) * VectorLength( gi.dbgAxis[k] ) + 1e-6f );
			const float a = acosf( c > 1.0f ? 1.0f : ( c < -1.0f ? -1.0f : c ) ) / dt;

			entTurn = a > entTurn ? a : entTurn;
		}

		const float frameSpeed = VectorLength( gi.cloth.frameVel );
		const float entSpeed = Distance( ent->e.origin, gi.dbgOrigin ) / dt;
		const mdxaHeader_t *boneHeader = G2Phys_BoneHeader( bc );

		if ( frameTurn > 1.0f || fabsf( frameSpeed - entSpeed ) > 40.0f )
			ri.Printf( PRINT_ALL, "phys frame: '%s' bone %s t %d dt %.1f ms: frame turn %.2f, entity turn %.2f rad/s, frame speed %.0f, entity speed %.0f\n",
				def.name.c_str(), ( gi.refBone >= 0 && boneHeader ) ? Phys_Skel( boneHeader, gi.refBone )->name : "(fit)", tr.refdef.time, dt * 1000.0f,
				frameTurn, entTurn, frameSpeed, entSpeed );
	}

	VectorCopy( ent->e.axis[0], gi.dbgAxis[0] );
	VectorCopy( ent->e.axis[1], gi.dbgAxis[1] );
	VectorCopy( ent->e.axis[2], gi.dbgAxis[2] );
	VectorCopy( ent->e.origin, gi.dbgOrigin );

	// ph_debug 4: distance of the free particles from the animation, twice per second.
	if ( ph_debug->integer == 4 && tr.refdef.time / 500 != gi.reportTime / 500 )
	{
		float maxDev = 0.0f, sumDev = 0.0f, maxFwd = -1e9f;
		float sumAxis[3] = { 0.0f, 0.0f, 0.0f };
		int numFree = 0;

		for ( int i = 0; i < n; i++ )
		{
			if ( gi.freeFactor[i] <= 0.01f )
				continue;

			const float dev = Distance( &gi.worldPos[i * 3], &gi.target[i * 3] );
			float d[3];

			VectorSubtract( &gi.worldPos[i * 3], &gi.target[i * 3], d );

			// Offset along the forward, left and up axes of the entity.
			for ( int k = 0; k < 3; k++ )
				sumAxis[k] += DotProduct( d, ent->e.axis[k] ) / VectorLength( ent->e.axis[k] );

			const float fwd = DotProduct( d, ent->e.axis[0] ) / VectorLength( ent->e.axis[0] );

			maxFwd = fwd > maxFwd ? fwd : maxFwd;
			maxDev = dev > maxDev ? dev : maxDev;
			sumDev += dev;
			numFree++;
		}

		const float inv = numFree ? 1.0f / numFree : 0.0f;

		gi.reportTime = tr.refdef.time;
		ri.Printf( PRINT_ALL, "phys: %s '%s' t %d: free %d, deviation mean %.2f max %.2f, mean forward %.2f left %.2f up %.2f, max forward %.2f\n",
			glm->name, def.name.c_str(), tr.refdef.time, numFree, sumDev * inv, maxDev, sumAxis[0] * inv, sumAxis[1] * inv, sumAxis[2] * inv, maxFwd );
	}

	// Solved positions, back in model space.
	for ( int i = 0; i < n; i++ )
		Phys_WorldToModel( ent, &gi.worldPos[i * 3], &gi.modelPos[i * 3] );

	Phys_ComputeNormals( gi.topo, &gi.modelPos[0], &gi.particleNormal[0] );

	if ( editModel )
		Phys_DrawGroupView( gi, ent, bc );

	// Subdivided mesh: each refined point blends the solved particles. The normal of a particle is the one
	// that a vertex of the full mesh gets below.
	if ( gi.subdiv.levels > 0 )
	{
		for ( int p = 0; p < n; p++ )
		{
			const float *skin = &gi.particleSkin[p * 3];
			float sim[3] = { gi.particleNormal[p * 3], gi.particleNormal[p * 3 + 1], gi.particleNormal[p * 3 + 2] };
			float *nrm = &gi.drawNormal[p * 3];

			if ( DotProduct( sim, skin ) < 0.0f )
				VectorScale( sim, -1.0f, sim );

			const float w = Phys_Clamp01( gi.freeFactor[p] * 2.0f );

			nrm[0] = skin[0] + ( sim[0] - skin[0] ) * w;
			nrm[1] = skin[1] + ( sim[1] - skin[1] ) * w;
			nrm[2] = skin[2] + ( sim[2] - skin[2] ) * w;
		}

		Phys_ApplySubdiv( gi.subdiv, &gi.modelPos[0], &gi.pointXYZ[0] );
		Phys_ApplySubdiv( gi.subdiv, &gi.drawNormal[0], &gi.pointNormal[0] );

		for ( int p = 0; p < gi.subdiv.numPoints; p++ )
		{
			if ( VectorNormalize( &gi.pointNormal[p * 3] ) == 0.0f )
				gi.pointNormal[p * 3 + 2] = 1.0f;
		}

		for ( int s = 0; s < numActive; s++ )
		{
			const std::vector<int> &point = gi.subdiv.surfaces[s].point;

			for ( size_t i = 0; i < point.size(); i++ )
			{
				VectorCopy( &gi.pointXYZ[point[i] * 3], &gi.outXYZ[s][i * 3] );
				VectorCopy( &gi.pointNormal[point[i] * 3], &gi.outNormal[s][i * 3] );
			}
		}

		gi.frame = tr.frameCount;
		return;
	}

	for ( int s = 0; s < numActive; s++ )
	{
		const std::vector<int> &vp = gi.topo.vertParticle[s];

		for ( size_t i = 0; i < vp.size(); i++ )
		{
			const int p = vp[i];
			float *xyz = &gi.outXYZ[s][i * 3];
			float *nrm = &gi.outNormal[s][i * 3];
			const float *skin = &gi.skinNormal[s][i * 3];
			float sim[3] = { gi.particleNormal[p * 3], gi.particleNormal[p * 3 + 1], gi.particleNormal[p * 3 + 2] };

			VectorCopy( &gi.modelPos[p * 3], xyz );

			// A pinned vertex keeps the animated normal. The cloth is two sided, so the
			// solved normal takes the side of the animated one.
			if ( DotProduct( sim, skin ) < 0.0f )
				VectorScale( sim, -1.0f, sim );

			const float w = Phys_Clamp01( gi.freeFactor[p] * 2.0f );

			nrm[0] = skin[0] + ( sim[0] - skin[0] ) * w;
			nrm[1] = skin[1] + ( sim[1] - skin[1] ) * w;
			nrm[2] = skin[2] + ( sim[2] - skin[2] ) * w;

			if ( VectorNormalize( nrm ) == 0.0f )
				VectorCopy( skin, nrm );
		}
	}

	gi.frame = tr.frameCount;
}

physInstance_t *G2Phys_Update( const trRefEntity_t *ent, CBoneCache *bc, const model_t *glm, const surfaceInfo_v &slist )
{
	if ( !ph_enable->integer || !bc || !glm || !Phys_Header( glm ) )
		return NULL;

	physInstance_t **slot = (physInstance_t **)G2Phys_BoneSlot( bc );

	if ( *slot && ( (*slot)->header != Phys_Header( glm ) || (*slot)->epoch != s_epoch ) )
	{
		delete *slot;
		*slot = NULL;
	}

	if ( !*slot )
		*slot = Phys_CreateInstance( glm );

	physInstance_t *inst = *slot;

	if ( inst->groups.empty() )
		return NULL;

	// Another view of the same frame reuses the solved positions.
	if ( inst->lastFrame == tr.frameCount )
		return inst;

	const float distance = Distance( ent->e.origin, tr.viewParms.ori.origin );
	bool inRange = false;

	for ( size_t g = 0; g < inst->groups.size(); g++ )
	{
		if ( inst->groups[g].def->range <= 0.0f || inst->groups[g].def->range * ph_range->value >= distance )
			inRange = true;
	}

	if ( !inRange )
		return NULL;

	const float dt = ( tr.refdef.time - inst->lastTime ) * 0.001f;
	bool stale = inst->lastFrame < 0 || tr.frameCount - inst->lastFrame > PHYS_STALE_FRAMES || dt < 0.0f || dt > PHYS_STALE_TIME;

	if ( DistanceSquared( ent->e.origin, inst->lastOrigin ) > PHYS_TELEPORT_DIST * PHYS_TELEPORT_DIST )
		stale = true;

	for ( size_t g = 0; g < inst->groups.size(); g++ )
	{
		if ( !inst->groups[g].matched.empty() )
			Phys_UpdateGroup( inst, inst->groups[g], ent, glm, bc, slist, stale, dt );
	}

	inst->lastFrame = tr.frameCount;
	inst->lastTime = tr.refdef.time;
	VectorCopy( ent->e.origin, inst->lastOrigin );

	return inst;
}

const physSurfaceOut_t *G2Phys_GetSurface( const physInstance_t *inst, int surfaceNum )
{
	if ( !inst || inst->lastFrame != tr.frameCount || surfaceNum < 0 || surfaceNum >= (int)inst->surfaceGroup.size() )
		return NULL;

	const int g = inst->surfaceGroup[surfaceNum];

	if ( g < 0 )
		return NULL;

	const physGroupInst_t &gi = inst->groups[g];

	if ( !gi.usable || gi.frame != tr.frameCount || surfaceNum >= (int)gi.slotOfSurface.size() )
		return NULL;

	const int slot = gi.slotOfSurface[surfaceNum];

	return slot >= 0 ? &gi.out[slot] : NULL;
}


//
// Editor: the menu changes the definition of one model and sees the result on the preview.
//

static std::string	s_editKey;
static std::string	s_highlight;
static std::string	s_editMessage;

static void Phys_EditPrint( const char *msg )
{
	s_editMessage += msg;
}

// True when the editor shows the surface.
static bool Phys_HighlightMatch( const char *surfaceName )
{
	if ( s_highlight.empty() || !surfaceName )
		return false;

	// The text is a list of names or patterns separated by spaces.
	const char *p = s_highlight.c_str();

	while ( *p )
	{
		while ( *p == ' ' )
			p++;

		const char *start = p;

		while ( *p && *p != ' ' )
			p++;

		if ( p > start )
		{
			const std::string pattern( start, p );

			if ( Phys_MatchName( pattern.c_str(), surfaceName ) )
				return true;
		}
	}

	return false;
}

// The edges of a surface, with the triangles on each side. Vertices at the same position are one vertex.
static const physOutlineMesh_t &Phys_OutlineMesh( const mdxmSurface_t *surf )
{
	physOutlineMesh_t &mesh = s_outlineMeshes[surf];

	if ( mesh.numVerts == surf->numVerts && mesh.numTris == surf->numTriangles )
		return mesh;

	typedef std::pair<std::pair<int, int>, int> weldKey_t;

	const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts );
	const int *tris = (const int *)( (const byte *)surf + surf->ofsTriangles );
	std::map<weldKey_t, int> weld;
	std::map<std::pair<int, int>, int> edgeIndex;
	std::vector<int> first( surf->numVerts );

	mesh.numVerts = surf->numVerts;
	mesh.numTris = surf->numTriangles;
	mesh.edges.clear();

	for ( int i = 0; i < surf->numVerts; i++ )
	{
		const float *p = verts[i].vertCoords;
		const weldKey_t key( std::make_pair( (int)floorf( p[0] * 100.0f + 0.5f ), (int)floorf( p[1] * 100.0f + 0.5f ) ), (int)floorf( p[2] * 100.0f + 0.5f ) );
		std::map<weldKey_t, int>::iterator it = weld.find( key );

		if ( it == weld.end() )
		{
			weld[key] = i;
			first[i] = i;
		}
		else
		{
			first[i] = it->second;
		}
	}

	for ( int t = 0; t < surf->numTriangles; t++ )
	{
		for ( int k = 0; k < 3; k++ )
		{
			const int va = tris[t * 3 + k], vb = tris[t * 3 + ( k + 1 ) % 3];

			if ( va < 0 || vb < 0 || va >= surf->numVerts || vb >= surf->numVerts || first[va] == first[vb] )
				continue;

			const std::pair<int, int> key = first[va] < first[vb] ? std::make_pair( first[va], first[vb] ) : std::make_pair( first[vb], first[va] );
			std::map<std::pair<int, int>, int>::iterator it = edgeIndex.find( key );

			if ( it == edgeIndex.end() )
			{
				physOutlineEdge_t e;

				e.a = va;
				e.b = vb;
				e.t0 = t;
				e.t1 = -1;
				edgeIndex[key] = (int)mesh.edges.size();
				mesh.edges.push_back( e );
			}
			else
			{
				physOutlineEdge_t &e = mesh.edges[it->second];

				e.t1 = e.t1 == -1 ? t : -2;
			}
		}
	}

	return mesh;
}

/*
==============
G2Phys_SurfaceOutline

The editor shows a surface with a thin outline over its own shader: the open edges and the silhouette from
the view. A simulated surface uses the solved positions of this frame, another surface is skinned here.
Only for the edited model in the scene of the menu.
==============
*/
void G2Phys_SurfaceOutline( const trRefEntity_t *ent, const physInstance_t *inst, CBoneCache *bc, const model_t *glm,
							int surfaceNum, const char *surfaceName )
{
	static std::vector<float>			xyz;
	static std::vector<unsigned char>	front;
	static const byte					cyan[4] = { 0, 255, 255, 255 };

	if ( !ent || !bc || !glm || !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) || !s_editHandle || R_GetModelByHandle( s_editHandle ) != glm )
		return;

	if ( !Phys_HighlightMatch( surfaceName ) )
		return;

	const mdxmSurface_t *surf = (const mdxmSurface_t *)G2_FindSurface( glm, surfaceNum, PHYS_LOD );

	if ( !surf || surf->numVerts <= 0 || surf->numTriangles <= 0 )
		return;

	const physOutlineMesh_t &mesh = Phys_OutlineMesh( surf );
	const physGroupInst_t *gi = NULL;
	int slot = -1;

	xyz.resize( surf->numVerts * 3 );
	front.resize( surf->numTriangles );

	if ( inst && inst->lastFrame == tr.frameCount && surfaceNum < (int)inst->surfaceGroup.size() && inst->surfaceGroup[surfaceNum] >= 0 )
	{
		const physGroupInst_t &g = inst->groups[inst->surfaceGroup[surfaceNum]];

		if ( g.usable && g.frame == tr.frameCount && surfaceNum < (int)g.slotOfSurface.size() && g.slotOfSurface[surfaceNum] >= 0 )
		{
			gi = &g;
			slot = g.slotOfSurface[surfaceNum];
		}
	}

	// The particles of the model mesh have the same index in a refined mesh.
	const physTopology_t *base = gi ? ( gi->simSubdiv.levels > 0 ? &gi->baseTopo : &gi->topo ) : NULL;

	if ( base && (int)base->vertParticle[slot].size() == surf->numVerts )
	{
		const std::vector<int> &vp = base->vertParticle[slot];

		for ( int i = 0; i < surf->numVerts; i++ )
			VectorCopy( &gi->modelPos[vp[i] * 3], &xyz[i * 3] );
	}
	else
	{
		const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts );
		const int *boneRefs = (const int *)( (const byte *)surf + surf->ofsBoneReferences );
		float normal[3];

		for ( int i = 0; i < surf->numVerts; i++ )
			Phys_SkinVertex( bc, boneRefs, &verts[i], &xyz[i * 3], normal );
	}

	Phys_DrawBegin( ent, 0.18f, 1.0f );

	// The side of each triangle that the view sees.
	const int *tris = (const int *)( (const byte *)surf + surf->ofsTriangles );

	for ( int t = 0; t < surf->numTriangles; t++ )
	{
		const int *tri = &tris[t * 3];
		float e1[3], e2[3], nrm[3], toEye[3];

		front[t] = 0;

		if ( tri[0] < 0 || tri[1] < 0 || tri[2] < 0 || tri[0] >= surf->numVerts || tri[1] >= surf->numVerts || tri[2] >= surf->numVerts )
			continue;

		VectorSubtract( &xyz[tri[1] * 3], &xyz[tri[0] * 3], e1 );
		VectorSubtract( &xyz[tri[2] * 3], &xyz[tri[0] * 3], e2 );
		CrossProduct( e1, e2, nrm );
		VectorSubtract( s_dbgEye, &xyz[tri[0] * 3], toEye );
		front[t] = DotProduct( nrm, toEye ) > 0.0f;
	}

	for ( size_t i = 0; i < mesh.edges.size(); i++ )
	{
		const physOutlineEdge_t &e = mesh.edges[i];

		if ( e.t1 == -1 || ( e.t1 >= 0 && front[e.t0] != front[e.t1] ) )
			Phys_Line( &xyz[e.a * 3], &xyz[e.b * 3], cyan );
	}
}

static physDef_t *Phys_EditDef( void )
{
	physDef_t *def = Phys_FindDef( s_editKey );

	if ( !def )
	{
		def = new physDef_t;
		def->name = Phys_FilePath( s_editKey );
		s_defs[s_editKey] = def;
	}

	return def;
}

static int Phys_EditGroupOfSurface( const physDef_t *def, const char *surfaceName )
{
	for ( size_t g = 0; g < def->groups.size(); g++ )
	{
		for ( size_t p = 0; p < def->groups[g].surfaces.size(); p++ )
		{
			if ( Phys_MatchName( def->groups[g].surfaces[p].c_str(), surfaceName ) )
				return (int)g;
		}
	}

	return -1;
}

static int Phys_EditFail( char *out, int outSize, const char *msg )
{
	Q_strncpyz( out, msg, outSize );
	return -1;
}

static void Phys_EditAppend( char *out, int outSize, const std::string &text )
{
	Q_strncpyz( out, text.c_str(), outSize );
}

// The old group with the values of the new group that the solver reads every frame.
// When its text is equal to the new text, the instances need no rebuild.
static physGroupDef_t Phys_WithLiveValues( const physGroupDef_t &oldGroup, const physGroupDef_t &newGroup )
{
	physGroupDef_t g = oldGroup;

	g.material = newGroup.material;
	g.weight = newGroup.weight;
	g.stretch = newGroup.stretch;
	g.stiffness = newGroup.stiffness;
	g.air = newGroup.air;
	g.hold = newGroup.hold;
	g.bounce = newGroup.bounce;
	g.inertia = newGroup.inertia;
	g.shape = newGroup.shape;
	g.maxDistN = newGroup.maxDistN;
	g.range = newGroup.range;
	g.maxSpeed = newGroup.maxSpeed;
	g.margin = newGroup.margin;
	g.iterations = newGroup.iterations;

	// The cell size of the simulation mesh stays: a new detail needs a rebuild.
	Phys_DeriveGroup( g );
	return g;
}

//
// Bone markers: the editor shows the bones of the preview model.
//

#define PHYS_MAX_MARK_BONES	128

static bool			s_markAll = false;
static int			s_markSel = -1;

// One round dot at each bone, in the model space of the entity. Only the scene of the menu shows them.
void G2Phys_AddBoneMarkers( const trRefEntity_t *ent, CBoneCache *bc, const model_t *glm )
{
	if ( ( !s_markAll && s_markSel < 0 ) || !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) || !bc || !s_editHandle )
		return;

	if ( R_GetModelByHandle( s_editHandle ) != glm )
		return;

	const mdxaHeader_t *aHeader = G2Phys_BoneHeader( bc );
	int numBones = aHeader->numBones;

	if ( numBones > PHYS_MAX_MARK_BONES )
		numBones = PHYS_MAX_MARK_BONES;

	Phys_DrawBegin( ent, PHYS_LINE_WIDTH, PHYS_EYE_BIAS );

	for ( int b = 0; b < numBones; b++ )
	{
		const bool selected = ( b == s_markSel );

		if ( !selected && !s_markAll )
			continue;

		const byte col[4] = { selected ? (byte)255 : (byte)0, selected ? (byte)0 : (byte)255, (byte)255, (byte)255 };
		float bind[3], p[3];

		Phys_BindOrigin( aHeader, b, bind );
		Phys_BindToModel( G2Phys_BoneMatrix( bc, b ), bind, p );
		Phys_Point( p, selected ? 1.6f : 0.7f, col );
	}
}

/*
==============
G2Phys_UICommand

Entry of the editor menu. Returns 0 on success, -1 on error. The result or the error message goes to out.
==============
*/
int G2Phys_UICommand( const char *cmd, const char *arg, char *out, int outSize )
{
	if ( !out || outSize <= 0 )
		return -1;

	out[0] = '\0';

	if ( !cmd )
		return -1;

	if ( !arg )
		arg = "";

	if ( !Q_stricmp( cmd, "bonemarks" ) )
	{
		s_markAll = atoi( arg ) != 0;
		return 0;
	}

	if ( !Q_stricmp( cmd, "bonesel" ) )
	{
		s_markSel = atoi( arg );
		return 0;
	}

	if ( !Q_stricmp( cmd, "highlight" ) )
	{
		s_highlight = arg;
		return 0;
	}

	// "<flags> <group> <pin rule>". Flags: 1 colliders, 2 pin rules, 4 free factor of the particles, 8 maxdist.
	if ( !Q_stricmp( cmd, "view" ) )
	{
		int flags = 0, group = -1, pin = -1;

		sscanf( arg, "%d %d %d", &flags, &group, &pin );
		s_viewFlags = flags;
		s_viewGroup = group;
		s_viewPin = pin;
		return 0;
	}

	if ( !Q_stricmp( cmd, "model" ) )
	{
		const qhandle_t handle = RE_RegisterModel( arg );
		const model_t *glm = handle ? R_GetModelByHandle( handle ) : NULL;

		if ( !glm || !Phys_Header( glm ) )
			return Phys_EditFail( out, outSize, "The model is not a Ghoul2 model" );

		s_editHandle = handle;
		s_editKey = Phys_LowerKey( glm->name );
		Phys_EditDef();
		return 0;
	}

	const model_t *glm = s_editHandle ? R_GetModelByHandle( s_editHandle ) : NULL;

	if ( !glm || !Phys_Header( glm ) )
		return Phys_EditFail( out, outSize, "No model selected" );

	const mdxmHeader_t *header = Phys_Header( glm );
	physDef_t *def = Phys_EditDef();
	const int index = atoi( arg );

	if ( !Q_stricmp( cmd, "surfaces" ) )
	{
		std::string text;
		char line[128];

		for ( int s = 0; s < header->numSurfaces; s++ )
		{
			const mdxmSurfHierarchy_t *info = Phys_SurfInfo( header, s );
			const mdxmSurface_t *surf = (const mdxmSurface_t *)G2_FindSurface( glm, s, 0 );

			// Only the real surfaces are listed. A tag, a cap or a helper surface has a few vertices.
			if ( !Phys_RealSurface( surf ) )
				continue;

			Com_sprintf( line, sizeof( line ), "%s\t%d\n", info->name, Phys_EditGroupOfSurface( def, info->name ) );
			text += line;
		}

		Phys_EditAppend( out, outSize, text );
		return 0;
	}

	if ( !Q_stricmp( cmd, "bones" ) )
	{
		const mdxaHeader_t *aHeader = Phys_AnimHeader( header );
		std::string text;
		char line[128];

		if ( !aHeader )
			return Phys_EditFail( out, outSize, "The skeleton is not loaded" );

		for ( int b = 0; b < aHeader->numBones; b++ )
		{
			int depth = 0;

			for ( int p = Phys_Skel( aHeader, b )->parent; p >= 0 && depth < 32; p = Phys_Skel( aHeader, p )->parent )
				depth++;

			Com_sprintf( line, sizeof( line ), "%s\t%d\n", Phys_Skel( aHeader, b )->name, depth );
			text += line;
		}

		Phys_EditAppend( out, outSize, text );
		return 0;
	}

	if ( !Q_stricmp( cmd, "groups" ) )
	{
		std::string text;
		char line[64];

		for ( size_t g = 0; g < def->groups.size(); g++ )
		{
			Com_sprintf( line, sizeof( line ), "%d\t", (int)g );
			text += line;
			text += def->groups[g].name + "\t";

			for ( size_t p = 0; p < def->groups[g].surfaces.size(); p++ )
				text += ( p ? " " : "" ) + def->groups[g].surfaces[p];

			text += "\n";
		}

		Phys_EditAppend( out, outSize, text );
		return 0;
	}

	if ( !Q_stricmp( cmd, "text" ) )
	{
		Phys_EditAppend( out, outSize, Phys_DefToText( *def ) );
		return 0;
	}

	// "<kind> <boneA> <boneB>", or an empty text for no selection.
	if ( !Q_stricmp( cmd, "collsel" ) )
	{
		const mdxaHeader_t *aHeader = Phys_AnimHeader( header );
		char kind[8] = "", boneA[MAX_QPATH] = "", boneB[MAX_QPATH] = "";

		s_collSelA = s_collSelB = -1;
		s_collSelKind = 0;

		if ( aHeader && sscanf( arg, "%7s %63s %63s", kind, boneA, boneB ) == 3 )
		{
			s_collSelKind = kind[0];
			s_collSelA = Phys_FindBone( aHeader, boneA );
			s_collSelB = Phys_FindBone( aHeader, boneB );
		}

		return 0;
	}

	// The values of the group, defaults included: "<key>\t<value>" per line.
	if ( !Q_stricmp( cmd, "values" ) )
	{
		if ( index < 0 || index >= (int)def->groups.size() )
			return Phys_EditFail( out, outSize, "No such group" );

		const physGroupDef_t &g = def->groups[index];
		const struct { const char *key; float value; } values[] = {
			{ "lag", g.lag ? 1.0f : 0.0f },			{ "humanoid", g.humanoid ? 1.0f : 0.0f },
			{ "humanoidscale", g.humanoidScale },	{ "reach", g.maxDist },
			{ "across", g.maxDistN },				{ "weight", g.weight },
			{ "stretch", g.stretch },				{ "stiffness", g.stiffness },
			{ "air", g.air },						{ "hold", g.hold },
			{ "bounce", g.bounce },					{ "inertia", g.inertia },
			{ "shape", g.shape },					{ "detail", g.detail },
			{ "subdivide", (float)g.subdivide },	{ "simsubdivide", (float)g.simSubdivide } };
		std::string text;

		for ( size_t i = 0; i < ARRAY_LEN( values ); i++ )
			text += va( "%s\t%g\n", values[i].key, values[i].value );

		Phys_EditAppend( out, outSize, text );
		return 0;
	}

	// Every collider of the group, the removed ones too:
	// "<kind>\t<boneA>\t<boneB>\t<radius>\t<on>\t<line>\t<shape>\t<sx>\t<sy>\t<sz>\t<ox>\t<oy>\t<oz>" per line.
	// Kind: m = model fit, h = humanoid, c = collide line of the text. Line: index of the collide line among the
	// explicit colliders, or -1. Shape: c = capsule, s = sphere, b = box. Box: half sizes and offset, radius = corner radius.
	if ( !Q_stricmp( cmd, "collist" ) )
	{
		if ( index < 0 || index >= (int)def->groups.size() )
			return Phys_EditFail( out, outSize, "No such group" );

		const physGroupDef_t &g = def->groups[index];
		const mdxaHeader_t *aHeader = Phys_AnimHeader( header );
		std::vector<physCollider_t> fitted, all, humanoid;

		if ( g.modelFit && aHeader )
			Phys_FitColliders( glm, aHeader, def, g.modelFitScale, fitted );

		physGroupDef_t kept = g;

		kept.removed.clear();
		Phys_ExpandColliders( kept, all, &fitted );

		physGroupDef_t onlyHumanoid = kept;

		onlyHumanoid.modelFit = false;
		onlyHumanoid.colliders.clear();
		Phys_ExpandColliders( onlyHumanoid, humanoid );

		const int numFit = (int)fitted.size();
		const int numHumanoid = (int)humanoid.size();
		std::string text;

		for ( int c = 0; c < (int)all.size(); c++ )
		{
			const physCollider_t &col = all[c];
			const char kind = col.kind;
			bool on = true;

			for ( size_t r = 0; r < g.removed.size() && kind != 'c'; r++ )
			{
				if ( !Q_stricmp( g.removed[r].c_str(), col.boneA.c_str() ) )
					on = false;
			}

			text += va( "%c\t%s\t%s\t%g\t%d\t%d\t%c\t%g\t%g\t%g\t%g\t%g\t%g\n", kind, col.boneA.c_str(),
				( col.capsule && !col.box ) ? col.boneB.c_str() : col.boneA.c_str(), col.radius, on ? 1 : 0, kind == 'c' ? c - numFit - numHumanoid : -1,
				col.box ? 'b' : ( col.capsule ? 'c' : 's' ), col.size[0], col.size[1], col.size[2], col.offA[0], col.offA[1], col.offA[2] );
		}

		Phys_EditAppend( out, outSize, text );
		return 0;
	}

	if ( !Q_stricmp( cmd, "add" ) )
	{
		physGroupDef_t g;
		const std::string name = arg;
		const bool hair = Q_stristr( arg, "hair" ) || Q_stristr( arg, "tail" ) || Q_stristr( arg, "braid" );

		if ( !name.length() )
			return Phys_EditFail( out, outSize, "No surface" );

		Phys_InitGroup( g, hair );
		g.name = name;
		g.surfaces.push_back( name );

		if ( hair )
		{
			physPinRule_t r;

			g.lag = true;
			g.maxDist = 4.0f;
			r.type = PHYS_PIN_GRADIENT;
			r.bone = "cranium";
			r.axis[0] = r.axis[1] = 0.0f;
			r.axis[2] = -1.0f;
			r.pinned = -4.0f;
			r.free = 1.0f;
			g.pins.push_back( r );
		}
		else
		{
			g.humanoid = true;
			g.proxy = 4.0f;
		}

		def->groups.push_back( g );
		s_highlight = name;
		Phys_Invalidate();
		Com_sprintf( out, outSize, "%d", (int)def->groups.size() - 1 );
		return 0;
	}

	if ( !Q_stricmp( cmd, "remove" ) )
	{
		if ( index < 0 || index >= (int)def->groups.size() )
			return Phys_EditFail( out, outSize, "No such group" );

		def->groups.erase( def->groups.begin() + index );
		Phys_Invalidate();
		return 0;
	}

	if ( !Q_stricmp( cmd, "get" ) )
	{
		if ( index < 0 || index >= (int)def->groups.size() )
			return Phys_EditFail( out, outSize, "No such group" );

		Phys_EditAppend( out, outSize, Phys_GroupToText( def->groups[index] ) );
		return 0;
	}

	if ( !Q_stricmp( cmd, "set" ) )
	{
		// The group number is the first word of the argument, the text follows after a line break.
		const char *text = strchr( arg, '\n' );

		if ( !text )
			return Phys_EditFail( out, outSize, "No text" );

		if ( index < 0 || index >= (int)def->groups.size() )
			return Phys_EditFail( out, outSize, "No such group" );

		physGroupDef_t g = def->groups[index];

		s_editMessage.clear();

		if ( !Phys_ParseGroupText( text + 1, g, Phys_EditPrint ) )
		{
			Phys_EditAppend( out, outSize, s_editMessage.empty() ? std::string( "The group has no surface" ) : s_editMessage );
			return -1;
		}

		// A change of the values that the solver reads every frame keeps the simulation running.
		const bool rebuild = Phys_GroupToText( Phys_WithLiveValues( def->groups[index], g ) ) != Phys_GroupToText( g );

		def->groups[index] = g;

		if ( rebuild )
			Phys_Invalidate();

		return 0;
	}

	if ( !Q_stricmp( cmd, "save" ) )
	{
		const std::string text = Phys_DefToText( *def );
		const std::string path = Phys_FilePath( s_editKey );
		const std::string body = text.empty() ? std::string( "// no group\n" ) : text;

		ri.FS_WriteFile( path.c_str(), body.c_str(), (int)body.size() );
		Q_strncpyz( out, path.c_str(), outSize );
		return 0;
	}

	if ( !Q_stricmp( cmd, "revert" ) )
	{
		std::map<std::string, physDef_t *>::iterator it = s_defs.find( s_editKey );

		if ( it != s_defs.end() )
		{
			delete it->second;
			s_defs.erase( it );
		}

		Phys_Invalidate();
		Phys_EditDef();
		return 0;
	}

	return Phys_EditFail( out, outSize, "Unknown command" );
}
