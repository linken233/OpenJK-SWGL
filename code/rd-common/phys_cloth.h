/*
===========================================================================
Cloth and hair physics: .phys parser and position based solver.

This module does not depend on any renderer. The renderer builds a
topology from the mesh surfaces named by a .phys group, then calls the
solver once per frame with the skinned (animated) vertex positions as
targets.
===========================================================================
*/

#ifndef PHYS_CLOTH_H
#define PHYS_CLOTH_H

#include <string>
#include <utility>
#include <vector>

//
// .phys definition
//

enum physPinType_t
{
	PHYS_PIN_GRADIENT,	// distance from a bone along an axis, in bind pose model space
	PHYS_PIN_BONES,		// weight of the vertex on the listed bones
	PHYS_PIN_UV,		// texture coordinate v
	PHYS_PIN_RADIUS		// distance from a bone origin, with an offset
};

struct physPinRule_t
{
	physPinType_t				type;
	std::string					bone;		// PHYS_PIN_GRADIENT: reference bone
	float						axis[3];	// PHYS_PIN_GRADIENT: unit axis. PHYS_PIN_RADIUS: offset from the bone
	float						pinned;		// distance or v value where the vertex is fixed
	float						free;		// distance or v value where the vertex is free
	std::vector<std::string>	bones;		// PHYS_PIN_BONES
};

struct physCollider_t
{
	bool		capsule;	// false: sphere on boneA, or a box when box is set
	bool		box;		// rounded box on boneA: center at offA, half sizes in size, corner radius in radius
	std::string	boneA;
	std::string	boneB;
	float		offA[3];	// offsets in bind pose model space
	float		offB[3];
	float		size[3];	// box: half sizes along the bind pose axes X Y Z
	float		radius;
	char		kind;		// set by Phys_ExpandColliders: m = model fit, h = humanoid, c = collide line

	physCollider_t() : capsule( false ), box( false ), radius( 0.0f ), kind( 'c' )
	{
		for ( int k = 0; k < 3; k++ )
			offA[k] = offB[k] = size[k] = 0.0f;
	}
};

struct physGroupDef_t
{
	std::string					name;
	bool						hair;
	bool						enabled;
	std::vector<std::string>	surfaces;	// names or wildcard patterns
	std::vector<physPinRule_t>	pins;
	std::vector<physCollider_t>	colliders;		// explicit colliders
	bool						humanoid;		// adds the colliders of the stock humanoid skeleton
	float						humanoidScale;	// multiplier of the radius of those colliders
	bool						modelFit;		// adds capsules fitted to the body surfaces of the model
	float						modelFitScale;	// multiplier of the fitted radius
	std::vector<std::pair<std::string, float> >	radiusOverride;	// bone name, radius
	std::vector<std::string>	removed;		// bone names: the collider that starts on the bone is removed

	bool	lag;		// mode lag: one soft transform for the whole group, no per vertex solver

	// The keywords of the file. Phys_DeriveGroup sets the solver values from them.
	std::string	material;	// preset of weight, stretch, stiffness and air, empty = none
	float	weight;		// multiplier of 800 units per second squared
	float	stretch;	// edge stiffness, 0 to 1
	float	stiffness;	// bend stiffness, 0 to 1
	float	air;		// 0 to 1: drag, push of the air on the faces and effect of the map wind
	float	hold;		// 0 to 1: pull back to the animated shape, 1 = 20 radians per second
	float	bounce;		// 0 to 1: 0 = stops without a bounce, 1 = no brake
	float	inertia;	// part of the change of the model frame speed that the cloth does not take, 0 to 1 (NvCloth inertia)
	float	shape;		// each particle zone keeps its animated shape, so a tube stays round: 0 = off, 1 = full
	float	maxDist;	// reach: backstop radius of a free vertex, in the surface plane
	float	maxDistN;	// reach across the cloth, along the surface normal, 0 = same as maxDist
	float	detail;		// density of the simulation mesh, 1 = cells of 4 units
	float	range;		// maximum distance from the view that runs the solver
	int		subdivide;	// levels of Loop subdivision of the drawn mesh, 0 to PHYS_MAX_SUBDIVIDE
	int		simSubdivide;	// levels of linear subdivision of the simulation mesh, 0 to PHYS_MAX_SUBDIVIDE

	// Solver values. They are not keywords of the file.
	float	proxy;		// cell size of the simulation mesh, in units
	float	maxSpeed;	// mode lag: largest speed relative to the animation, units per second
	float	gravity;	// multiplier of 800 units per second squared
	float	damping;	// loss of the speed relative to the model frame, 1 per second
	float	drag;		// pull of the speed toward the wind, 1 per second
	float	aero;		// push of the relative air flow on a triangle, 1 per second
	float	wind;		// multiplier of the map wind
	float	bend;		// bend stiffness, 0 to 1
	float	follow;		// natural frequency of the soft link to the animated pose, radians per second, 0 = none
	float	lagFreq;	// mode lag: natural frequency, radians per second
	float	lagDamp;	// mode lag: damping ratio, 1 = critical
	float	margin;		// extra distance kept from the colliders
	int		iterations;
	float	oldDamping;	// old keyword damping, -1 = none: a later follow line keeps this damping
};

#define PHYS_MAX_HOLD		20.0f	// natural frequency of hold 1, radians per second
#define PHYS_DAMP_MIN_FREQ	6.0f	// bounce gives a damping ratio of max( follow, this frequency )

// Sets the solver values from the keywords.
void		Phys_DeriveGroup( physGroupDef_t &g );

#define PHYS_MAX_SUBDIVIDE	2

struct physDef_t
{
	std::string					name;
	std::vector<physGroupDef_t>	groups;
};

// Sets the defaults of a group.
void		Phys_InitGroup( physGroupDef_t &g, bool hair );

// Parses the lines of one group, with or without the header and the braces. Returns false on a syntax error.
bool		Phys_ParseGroupText( const char *text, physGroupDef_t &g, void ( *print )( const char *msg ) );

// The text of a group or of a file. Values equal to the default are not written.
std::string	Phys_GroupToText( const physGroupDef_t &g );
std::string	Phys_DefToText( const physDef_t &def );

// The colliders of a group: the humanoid template, with the radius overrides, and the explicit colliders.
void		Phys_ExpandColliders( const physGroupDef_t &g, std::vector<physCollider_t> &out, const std::vector<physCollider_t> *fitted = 0 );

// Returns NULL when the text has no valid group. Messages go to the print callback.
physDef_t	*Phys_ParseDef( const char *text, const char *name, void ( *print )( const char *msg ) );

bool		Phys_MatchName( const char *pattern, const char *name );

//
// Topology
//

struct physSurfaceInput_t
{
	int			numVerts;
	const float	*xyz;		// 3 floats per vertex, bind pose model space
	int			numTris;
	const int	*tris;		// 3 indexes per triangle
};

struct physTopology_t
{
	int							numParticles;
	std::vector<float>			bindPos;		// 3 per particle
	std::vector<int>			firstVertex;	// per particle: (surface << 24) | vertex of the first user
	std::vector<std::vector<int> >	vertParticle;	// [surface][vertex]
	std::vector<int>			edges;			// 2 per edge
	std::vector<float>			edgeRest;
	std::vector<int>			bends;			// 2 per bend constraint
	std::vector<float>			bendRest;
	std::vector<int>			tris;			// 3 per triangle, particle indexes
	std::vector<float>			valence;		// per particle: number of triangles
};

void	Phys_BuildTopology( physTopology_t &topo, const physSurfaceInput_t *surfaces, int numSurfaces );

// Builds edges, bend constraints and valence from bindPos and tris.
void	Phys_BuildEdges( physTopology_t &topo );

//
// Simulation mesh (the "proxy"): a coarse set of particles that carries the full mesh. The solver runs on it.
//

struct physProxy_t
{
	int					numProxy;
	physTopology_t		topo;			// proxy particles
	std::vector<int>	clusterOf;		// per full particle
	std::vector<int>	link;			// 4 per full particle: proxy index, or -1
	std::vector<float>	weight;			// 4 per full particle
	std::vector<float>	freeFactor;		// per proxy particle
	std::vector<int>	memberCount;	// per proxy particle
	std::vector<float>	rot;			// per proxy particle: turn from the animation, quaternion x y z w (last frame)
	std::vector<float>	turn, cov;		// work buffers of Phys_ProxyApply: 9 per proxy particle

	physProxy_t() : numProxy( 0 ) {}
};

void	Phys_BuildProxy( physProxy_t &proxy, const physTopology_t &full, const float *freeFactor, float cell );

// Sets the turn of each proxy particle to none.
void	Phys_ProxyReset( physProxy_t &proxy );

// Mean of the member targets of each proxy particle.
void	Phys_ProxyTargets( const physProxy_t &proxy, const float *fullTarget, float *proxyTarget );

// Full positions: the target plus the weighted displacement of the proxy particles. The offset of a full
// particle from a proxy particle also turns with that proxy particle and its neighbors.
void	Phys_ProxyApply( physProxy_t &proxy, const float *proxyX, const float *proxyTarget,
						 const float *fullTarget, const float *freeFactor, float *outPos );

//
// Solver
//

enum physShape_t
{
	PHYS_SHAPE_SEGMENT,		// capsule, or sphere when a == b
	PHYS_SHAPE_BOX			// rounded box
};

// Constraint types of physCloth_t::dbgCorr.
enum physCorr_t
{
	PHYS_CORR_EDGES,	// stretch and bend
	PHYS_CORR_SHAPE,
	PHYS_CORR_TETHER,
	PHYS_CORR_FOLLOW,
	PHYS_CORR_REACH,
	PHYS_CORR_COLLIDE,
	PHYS_CORR_CROSS,	// exit across a collider, on the side of the animated position
	PHYS_CORR_SPEED		// largest particle speed, units per second
};

// A collider is the set of points within radius of a core shape: a segment or an inner box.
struct physCollider_w
{
	int		shape;		// physShape_t
	float	a[3];		// world space segment start, or box center
	float	b[3];		// segment end
	float	axis[3][3];	// box: unit axes
	float	half[3];	// box: half sizes of the inner box
	float	radius;
};

// Signed distance of pos from the surface of the collider, < 0 inside. normal gets the direction out of the
// collider; it is zero when pos is on the segment of a capsule.
float	Phys_ColliderDistance( const physCollider_w &c, const float *pos, float *normal );

struct physCloth_t
{
	int					numParticles;
	bool				valid;				// false until Phys_ClothReset
	std::vector<float>	x;					// 3 per particle, world space
	std::vector<float>	v;
	std::vector<float>	prevTarget;
	std::vector<float>	offPrev, offCurr;	// offset from the animation after the last two steps
	std::vector<float>	drawX;				// animation of the frame plus the offset of the cloth: draw this
	float				accum;				// time not simulated yet, less than one step
	std::vector<float>	maxDist;			// per particle backstop, 0 = pinned
	std::vector<unsigned char> pinned;
	std::vector<int>	tether;				// per particle: the nearest pinned particle in the bind pose, -1 = none
	std::vector<float>	tetherLength;		// per particle: distance to that particle in the bind pose
	bool				loose;				// no pinned particle: the model frame is the world

	// Model frame of the last frame (Phys_ClothFrame), and its speed.
	float				frameOrigin[3];
	float				frameAxis[3][3];	// unit axes
	float				frameVel[3];
	float				frameOmega[3];		// turn speed, radians per second, around this axis
	float				frameQ[4];			// turn of the pinned particles from the bind pose, quaternion x y z w

	// Reference bone (Phys_ClothSetReference): when set, it is the model frame, in place of the fit of the pinned particles.
	bool				hasReference;
	float				refOrigin[3];
	float				refAxis[3][3];

	// Low-pass on the model frame (Phys_SmoothFrame): the frame before the filter and after it.
	float				rawOrigin[3];
	float				rawAxis[3][3];
	float				smoothOrigin[3];
	float				smoothAxis[3][3];
	bool				teleported;			// the last step moved the cloth as one rigid piece (ph_debug)

	// ph_debug: when debug is set, a step keeps the largest correction of each constraint type (physCorr_t),
	// in units, and the largest particle speed.
	bool				debug;
	float				dbgCorr[8];
	std::vector<float>	dbgP;

	// Frame state that the cloth took at the end of the last step. The next step starts from it, so no part
	// of a speed change is lost when a frame ends between two steps.
	float				appliedOrigin[3];
	float				appliedVel[3];
	float				appliedOmega[3];
	bool				frameSpeedValid;	// false after a reset: the next frame measures the speed and takes no change

	// Work buffers of Phys_ClothStep, kept between frames so a step does not allocate.
	std::vector<float>	p, tgt, acc, targetNormal, lambda, lambdaFollow;
	std::vector<physCollider_w> stepColliders;	// the colliders at the time of the step
	std::vector<float>	restDist;			// Phys_RestDistances of the particles, empty = none

	// Shape: each particle and its neighbors keep the animated shape of their zone.
	std::vector<int>	adjStart, adj;		// neighbors of each particle, from the edges
	std::vector<float>	shapeRot;			// 4 per particle: turn of its zone, quaternion x y z w
	std::vector<float>	shapeGoal, shapeCount;

	physCloth_t() : numParticles( 0 ), valid( false ), accum( 0.0f ), loose( false ), hasReference( false ), teleported( false ), debug( false ), frameSpeedValid( false )
	{
		for ( int k = 0; k < 8; k++ )
			dbgCorr[k] = 0.0f;
	}
};

// Sets the model frame of this frame: the world transform of the reference bone. origin NULL = no reference bone,
// the frame is the fit of the pinned particles. Call it before Phys_ClothReset and Phys_ClothStep.
void	Phys_ClothSetReference( physCloth_t &cloth, const float *origin, const float axis[3][3] );

// freeFactor: 0 = pinned, 1 = free, per particle. topo gives the bind pose of the tethers.
void	Phys_ClothInit( physCloth_t &cloth, const physGroupDef_t &def, const physTopology_t &topo, const float *freeFactor );

// Snap every particle to the target. Clears the velocity. origin and axis: the entity frame.
void	Phys_ClothReset( physCloth_t &cloth, const physTopology_t &topo, const float *target, const float origin[3], const float axis[3][3] );

// Runs fixed steps of PHYS_CLOTH_STEP for the time dt. The time that is less than one step stays for the next
// call. drawX gets the result at the time of target.
// origin and axis: the entity frame of this frame. The model frame is the frame of the pinned particles
// (Phys_ClothFrame). The cloth takes a part of the change of its speed (inertia), and the damping reduces
// the speed relative to it, as in NvCloth.
// prevColliders: the colliders of the last frame, NULL = they do not move. Each step uses the colliders
// at its time, so a fast collider does not jump over the particles.
void	Phys_ClothStep( physCloth_t &cloth, const physTopology_t &topo, const physGroupDef_t &def,
						float dt, const float *target, const float windVel[3], const float origin[3], const float axis[3][3],
						const physCollider_w *colliders, const physCollider_w *prevColliders, int numColliders,
						bool hasGround, float groundZ );

//
// Lag mode: the free particles move as one mass on a spring. It rotates around the pivot.
//

struct physLag_t
{
	bool	valid;
	float	xc[3];		// mass center, world space
	float	vc[3];
	float	ct[3];		// animated mass center
	float	vct[3];		// speed of the animated mass center
	float	pivot[3];	// mean of the fixed particles
	float	reach;		// distance of the farthest free particle from the pivot
	float	maxAngle;	// largest turn around the pivot, radians: keeps the farthest particle within maxdist

	physLag_t() : valid( false ), reach( 0.0f ), maxAngle( 3.0f ) {}
};

void	Phys_LagReset( physLag_t &lag, const float *target, const float *freeFactor, int n );
void	Phys_LagStep( physLag_t &lag, const physGroupDef_t &def, float dt, const float *target,
					  const float *freeFactor, int n, const float windVel[3] );
void	Phys_LagApply( const physLag_t &lag, const float *target, const float *freeFactor, int n, float *outPos );

// Distance of each particle from the core of each collider in the bind pose (n * numColliders). A particle that is
// inside a collider in the bind pose can stay at that depth: the collider does not push the rest shape out.
// bindColliders: the colliders in the bind pose, in the space of bindPos.
void	Phys_RestDistances( const float *bindPos, int n, const physCollider_w *bindColliders, int numColliders, std::vector<float> &out );

// Rest distances of the particles of a cloth.
void	Phys_ClothRestDistances( physCloth_t &cloth, const physTopology_t &topo, const physCollider_w *bindColliders, int numColliders );

// Pushes the free particles out of the colliders and above the floor.
// restDist (from Phys_RestDistances, NULL = none): a particle can be as deep as in the bind pose.
void	Phys_ProjectColliders( float *pos, const float *target, const float *freeFactor, int n, float margin,
							   const physCollider_w *colliders, int numColliders, const float *restDist, bool hasGround, float groundZ );

// Lag mode, after Phys_LagApply: a contact moves the mass center and removes the speed into the collider,
// then the positions are rebuilt. Only what is left is pushed out of the colliders one particle at a time.
// restDist: see Phys_ProjectColliders.
void	Phys_LagCollide( physLag_t &lag, const physGroupDef_t &def, const float *target, const float *freeFactor, int n,
						 const physCollider_w *colliders, int numColliders, const float *restDist, bool hasGround, float groundZ,
						 float *outPos );

// Area weighted particle normals from the given positions (3 per particle).
void	Phys_ComputeNormals( const physTopology_t &topo, const float *pos, float *normals );

//
// Subdivided draw mesh (subdivide): Loop subdivision of the full mesh. Every refined point is a fixed
// blend of particles (a stencil), so a frame only blends the solved positions and normals.
//

struct physSubdivInput_t
{
	int			numVerts;
	const float	*st;		// 2 floats per vertex
	int			numTris;
	const int	*tris;		// 3 indexes per triangle
};

struct physSubdivSurface_t
{
	std::vector<int>	point;		// per drawn vertex: refined point
	std::vector<float>	st;			// 2 per drawn vertex
	std::vector<int>	chunks;		// 4 per chunk: first vertex, vertex count, first index, index count
	std::vector<int>	indexes;	// per chunk, from the first vertex of the chunk
};

struct physSubdiv_t
{
	int									levels;		// 0 = no subdivided mesh
	int									numPoints;
	std::vector<int>					stencilStart;	// numPoints + 1
	std::vector<int>					stencilParticle;
	std::vector<float>					stencilWeight;
	std::vector<physSubdivSurface_t>	surfaces;

	physSubdiv_t() : levels( 0 ), numPoints( 0 ) {}
};

// smooth: per particle, 0 keeps the point on the mesh (pinned), 1 is full Loop smoothing.
// A chunk has at most maxChunkVerts vertices and maxChunkIndexes indexes. Fewer levels are used
// when the refined mesh has more than maxVerts drawn vertices.
void	Phys_BuildSubdiv( physSubdiv_t &sd, const physTopology_t &topo, const physSubdivInput_t *surfaces, int numSurfaces,
						  const float *smooth, int levels, int maxChunkVerts, int maxChunkIndexes, int maxVerts );

// out[point] = blend of in[particle], 3 floats each.
void	Phys_ApplySubdiv( const physSubdiv_t &sd, const float *in, float *out );

// out[point] = blend of in[particle], 1 float each.
void	Phys_ApplySubdivScalar( const physSubdiv_t &sd, const float *in, float *out );

// The topology of the refined mesh: one particle per refined point. The first base.numParticles points are
// the particles of base, at the same index.
void	Phys_SubdivTopology( physTopology_t &out, const physSubdiv_t &sd, const physTopology_t &base );

// The triangles of a refined surface, indexes of its drawn vertices (sd.surfaces[s].point).
void	Phys_SubdivSurfaceTris( const physSubdivSurface_t &surf, std::vector<int> &tris );

// A draw mesh of the refined surfaces of mesh: each drawn point is the particle of the same index.
void	Phys_SubdivIdentity( physSubdiv_t &out, const physSubdiv_t &mesh );

#endif
