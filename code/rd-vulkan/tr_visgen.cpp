/*
===========================================================================
Copyright (C) 1999 - 2007, id Software, Inc. and GtkRadiant contributors (q3map2)
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

// PVS generation for a map without a vis lump, ported from q3map2 (GtkRadiant):
// portals.c (MakeTreePortals), prtfile.c (the cluster portals), vis.c and visflow.c (fast vis).
// The tree in the BSP file is the tree that q3map2 makes the portals from, and a leaf
// with the cluster -1 is an opaque leaf. Thus the portals are the ones of the .prt file.

#include "tr_visgen.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

namespace {

const double ON_EPSILON				= 0.1;
const double CLIP_EPSILON			= 0.1;
const double BASE_WINDING_EPSILON	= 0.001;
const double SPLIT_WINDING_EPSILON	= 0.001;
const double EDGE_LENGTH			= 0.2;
const double SIDESPACE				= 8.0;
const double MAX_WORLD_COORD		= 65536.0;

enum { SIDE_FRONT, SIDE_BACK, SIDE_ON };

struct Vec3 {
	double v[3];
};

struct Plane {
	double normal[3];
	double dist;
};

typedef std::vector<Vec3> Winding;	// empty: no winding

inline double Dot( const double *a, const double *b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline Plane Flip( const Plane &p )
{
	Plane f = { { -p.normal[0], -p.normal[1], -p.normal[2] }, -p.dist };
	return f;
}

// A large square on the plane (q3map2 BaseWindingForPlane).
Winding BaseWindingForPlane( const Plane &plane )
{
	int axis = 0;
	double max = -1.0;

	for ( int i = 0; i < 3; i++ )
	{
		const double v = fabs( plane.normal[i] );
		if ( v > max )
		{
			axis = i;
			max = v;
		}
	}

	double up[3] = { 0, 0, 0 };
	if ( axis == 2 )
		up[0] = 1;
	else
		up[2] = 1;

	const double d = Dot( up, plane.normal );
	for ( int i = 0; i < 3; i++ )
		up[i] -= d * plane.normal[i];

	const double len = sqrt( Dot( up, up ) );
	for ( int i = 0; i < 3; i++ )
		up[i] /= len;

	double right[3] = {
		up[1] * plane.normal[2] - up[2] * plane.normal[1],
		up[2] * plane.normal[0] - up[0] * plane.normal[2],
		up[0] * plane.normal[1] - up[1] * plane.normal[0] };

	double org[3];
	for ( int i = 0; i < 3; i++ )
	{
		org[i] = plane.normal[i] * plane.dist;
		up[i] *= MAX_WORLD_COORD * 2;
		right[i] *= MAX_WORLD_COORD * 2;
	}

	Winding w( 4 );
	for ( int i = 0; i < 3; i++ )
	{
		w[0].v[i] = org[i] - right[i] + up[i];
		w[1].v[i] = org[i] + right[i] + up[i];
		w[2].v[i] = org[i] + right[i] - up[i];
		w[3].v[i] = org[i] - right[i] - up[i];
	}

	return w;
}

// Splits a winding by a plane (q3map2 ClipWindingEpsilon). A part that is not there is empty.
void ClipWinding( const Winding &in, const Plane &plane, double epsilon, Winding *front, Winding *back )
{
	const size_t n = in.size();
	std::vector<double> dists( n + 1 );
	std::vector<int> sides( n + 1 );
	int counts[3] = { 0, 0, 0 };

	for ( size_t i = 0; i < n; i++ )
	{
		const double d = Dot( in[i].v, plane.normal ) - plane.dist;
		dists[i] = d;
		sides[i] = d > epsilon ? SIDE_FRONT : d < -epsilon ? SIDE_BACK : SIDE_ON;
		counts[sides[i]]++;
	}
	dists[n] = dists[0];
	sides[n] = sides[0];

	if ( front )
		front->clear();
	if ( back )
		back->clear();

	if ( !counts[SIDE_FRONT] )
	{
		if ( back )
			*back = in;
		return;
	}

	if ( !counts[SIDE_BACK] )
	{
		if ( front )
			*front = in;
		return;
	}

	for ( size_t i = 0; i < n; i++ )
	{
		const Vec3 &p1 = in[i];

		if ( sides[i] == SIDE_ON )
		{
			if ( front )
				front->push_back( p1 );
			if ( back )
				back->push_back( p1 );
			continue;
		}

		if ( sides[i] == SIDE_FRONT && front )
			front->push_back( p1 );
		if ( sides[i] == SIDE_BACK && back )
			back->push_back( p1 );

		if ( sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i] )
			continue;

		// The split point. An axial plane gives the exact coordinate.
		const Vec3 &p2 = in[( i + 1 ) % n];
		const double t = dists[i] / ( dists[i] - dists[i + 1] );
		Vec3 mid;

		for ( int j = 0; j < 3; j++ )
		{
			if ( plane.normal[j] == 1.0 )
				mid.v[j] = plane.dist;
			else if ( plane.normal[j] == -1.0 )
				mid.v[j] = -plane.dist;
			else
				mid.v[j] = p1.v[j] + t * ( p2.v[j] - p1.v[j] );
		}

		if ( front )
			front->push_back( mid );
		if ( back )
			back->push_back( mid );
	}
}

// Keeps the part of the winding in front of the plane (q3map2 ChopWindingInPlace).
void ChopWinding( Winding &w, const Plane &plane, double epsilon )
{
	if ( w.empty() )
		return;

	Winding front;
	ClipWinding( w, plane, epsilon, &front, NULL );
	w.swap( front );
}

// True when the winding has less than three edges longer than EDGE_LENGTH (q3map2 WindingIsTiny).
bool WindingIsTiny( const Winding &w )
{
	int edges = 0;

	for ( size_t i = 0; i < w.size(); i++ )
	{
		const Vec3 &a = w[i];
		const Vec3 &b = w[( i + 1 ) % w.size()];
		const double d[3] = { b.v[0] - a.v[0], b.v[1] - a.v[1], b.v[2] - a.v[2] };

		if ( sqrt( Dot( d, d ) ) > EDGE_LENGTH && ++edges == 3 )
			return false;
	}

	return true;
}

/*
=================
Tree portals (q3map2 portals.c)
=================
*/

struct TreeNode {
	int		plane;			// -1 for a leaf
	int		children[2];
	int		parent;
	int		portals;		// the first portal of the list, -1 for none
	int		cluster;
	bool	opaque;
};

struct TreePortal {
	Plane	plane;
	int		onnode;			// the node that made the portal, -1 for a portal of the head node
	int		nodes[2];		// nodes[0] is on the front side of the plane
	int		next[2];		// the next portal in the list of nodes[i]
	Winding	winding;
};

class PortalBuilder {
public:
	std::vector<TreeNode>	nodes;
	std::vector<TreePortal>	portals;
	std::vector<Plane>		planes;
	int						outside;		// the node outside the world
	bool					failed;

	PortalBuilder() : outside( -1 ), failed( false ) {}

	const Plane &NodePlane( int node ) const { return planes[nodes[node].plane]; }

	int AllocPortal()
	{
		TreePortal p;
		p.onnode = -1;
		p.nodes[0] = p.nodes[1] = -1;
		p.next[0] = p.next[1] = -1;
		portals.push_back( p );
		return (int)portals.size() - 1;
	}

	void AddPortalToNodes( int p, int front, int back )
	{
		TreePortal &portal = portals[p];

		portal.nodes[0] = front;
		portal.next[0] = nodes[front].portals;
		nodes[front].portals = p;

		portal.nodes[1] = back;
		portal.next[1] = nodes[back].portals;
		nodes[back].portals = p;
	}

	void RemovePortalFromNode( int p, int node )
	{
		int *link = &nodes[node].portals;

		for ( ;; )
		{
			const int t = *link;

			if ( t < 0 )
			{
				failed = true;
				return;
			}

			if ( t == p )
				break;

			if ( portals[t].nodes[0] == node )
				link = &portals[t].next[0];
			else if ( portals[t].nodes[1] == node )
				link = &portals[t].next[1];
			else
			{
				failed = true;
				return;
			}
		}

		TreePortal &portal = portals[p];

		if ( portal.nodes[0] == node )
		{
			*link = portal.next[0];
			portal.nodes[0] = -1;
		}
		else if ( portal.nodes[1] == node )
		{
			*link = portal.next[1];
			portal.nodes[1] = -1;
		}
	}

	// Six portals that face the outside node, on the bounds of the world.
	void MakeHeadnodePortals( const double mins[3], const double maxs[3] )
	{
		double bounds[2][3];
		int side[6];
		Plane bplanes[6];

		for ( int i = 0; i < 3; i++ )
		{
			bounds[0][i] = mins[i] - SIDESPACE;
			bounds[1][i] = maxs[i] + SIDESPACE;
		}

		for ( int i = 0; i < 3; i++ )
		{
			for ( int j = 0; j < 2; j++ )
			{
				const int n = j * 3 + i;
				Plane &pl = bplanes[n];

				pl.normal[0] = pl.normal[1] = pl.normal[2] = 0.0;
				if ( j )
				{
					pl.normal[i] = -1.0;
					pl.dist = -bounds[j][i];
				}
				else
				{
					pl.normal[i] = 1.0;
					pl.dist = bounds[j][i];
				}

				side[n] = AllocPortal();
				portals[side[n]].plane = pl;
				portals[side[n]].winding = BaseWindingForPlane( pl );
				AddPortalToNodes( side[n], 0, outside );
			}
		}

		for ( int i = 0; i < 6; i++ )
		{
			for ( int j = 0; j < 6; j++ )
			{
				if ( j != i )
					ChopWinding( portals[side[i]].winding, bplanes[j], ON_EPSILON );
			}
		}
	}

	// The winding of the node plane, clipped by the planes of all the parents.
	Winding BaseWindingForNode( int node )
	{
		Winding w = BaseWindingForPlane( NodePlane( node ) );

		for ( int n = nodes[node].parent; n >= 0 && !w.empty(); n = nodes[n].parent )
		{
			const Plane &plane = NodePlane( n );

			if ( nodes[n].children[0] == node )
				ChopWinding( w, plane, BASE_WINDING_EPSILON );
			else
				ChopWinding( w, Flip( plane ), BASE_WINDING_EPSILON );

			node = n;
		}

		return w;
	}

	// The new portal: the node plane winding, clipped by the parents and by the portals of the node.
	void MakeNodePortal( int node )
	{
		Winding w = BaseWindingForNode( node );

		for ( int p = nodes[node].portals; p >= 0 && !w.empty(); )
		{
			const TreePortal &portal = portals[p];
			int side;

			if ( portal.nodes[0] == node )
				side = 0;
			else if ( portal.nodes[1] == node )
				side = 1;
			else
			{
				failed = true;
				return;
			}

			ChopWinding( w, side ? Flip( portal.plane ) : portal.plane, CLIP_EPSILON );
			p = portal.next[side];
		}

		if ( w.empty() || WindingIsTiny( w ) )
			return;

		const int p = AllocPortal();
		portals[p].plane = NodePlane( node );
		portals[p].onnode = node;
		portals[p].winding.swap( w );
		AddPortalToNodes( p, nodes[node].children[0], nodes[node].children[1] );
	}

	// Moves or splits the portals of the node to its children.
	void SplitNodePortals( int node )
	{
		const Plane plane = NodePlane( node );
		const int f = nodes[node].children[0];
		const int b = nodes[node].children[1];
		int next;

		for ( int p = nodes[node].portals; p >= 0; p = next )
		{
			int side;

			if ( portals[p].nodes[0] == node )
				side = 0;
			else if ( portals[p].nodes[1] == node )
				side = 1;
			else
			{
				failed = true;
				return;
			}

			next = portals[p].next[side];
			const int other = portals[p].nodes[!side];

			RemovePortalFromNode( p, portals[p].nodes[0] );
			RemovePortalFromNode( p, portals[p].nodes[1] );

			Winding front, back;
			ClipWinding( portals[p].winding, plane, SPLIT_WINDING_EPSILON, &front, &back );

			if ( !front.empty() && WindingIsTiny( front ) )
				front.clear();
			if ( !back.empty() && WindingIsTiny( back ) )
				back.clear();

			if ( front.empty() && back.empty() )
				continue;

			if ( front.empty() )
			{
				if ( side == 0 )
					AddPortalToNodes( p, b, other );
				else
					AddPortalToNodes( p, other, b );
				continue;
			}

			if ( back.empty() )
			{
				if ( side == 0 )
					AddPortalToNodes( p, f, other );
				else
					AddPortalToNodes( p, other, f );
				continue;
			}

			// The winding is split: p keeps the front part, a new portal takes the back part.
			const int np = AllocPortal();
			portals[np].plane = portals[p].plane;
			portals[np].onnode = portals[p].onnode;
			portals[np].winding.swap( back );
			portals[p].winding.swap( front );

			if ( side == 0 )
			{
				AddPortalToNodes( p, f, other );
				AddPortalToNodes( np, b, other );
			}
			else
			{
				AddPortalToNodes( p, other, f );
				AddPortalToNodes( np, other, b );
			}
		}

		nodes[node].portals = -1;
	}

	void MakeTreePortals()
	{
		std::vector<int> stack( 1, 0 );

		while ( !stack.empty() && !failed )
		{
			const int node = stack.back();
			stack.pop_back();

			if ( nodes[node].plane < 0 )
				continue;

			MakeNodePortal( node );
			SplitNodePortals( node );

			// The front child first, as the recursion of q3map2.
			stack.push_back( nodes[node].children[1] );
			stack.push_back( nodes[node].children[0] );
		}
	}
};

/*
=================
Fast vis (q3map2 vis.c, visflow.c)
=================
*/

// A portal from a cluster to the cluster `leaf`. The plane faces into `leaf`.
// The points of the winding are in a shared array. The vis step uses plain arrays:
// the checked iterators of a Debug build make it 50 times slower.
struct VisPortal {
	Plane	plane;
	int		leaf;
	int		firstPoint;
	int		numPoints;
};

// True when q is in front of p and p is behind q (q3map2 BasePortalVis).
bool PortalFront( const VisPortal *p, const VisPortal *q, const Vec3 *points )
{
	const Vec3 *pt = points + q->firstPoint;
	int i;

	for ( i = 0; i < q->numPoints; i++ )
	{
		if ( Dot( pt[i].v, p->plane.normal ) - p->plane.dist > ON_EPSILON )
			break;
	}

	if ( i == q->numPoints )
		return false;

	pt = points + p->firstPoint;

	for ( i = 0; i < p->numPoints; i++ )
	{
		if ( Dot( pt[i].v, q->plane.normal ) - q->plane.dist < -ON_EPSILON )
			return true;
	}

	return false;
}

#define VISGEN_SET_BIT( row, bit )	( ( row )[( bit ) >> 3] |= (uint8_t)( 1 << ( ( bit ) & 7 ) ) )

} // namespace

bool VisGen_Build( const visgenPlane_t *planes, int numPlanes, const visgenNode_t *nodes, int numNodes,
	const int *leafClusters, int numLeafs, int numClusters, int numThreads,
	std::vector<uint8_t> &vis, int &clusterBytes, visgenStats_t *stats )
{
	typedef std::chrono::steady_clock clock;

	if ( numPlanes <= 0 || numNodes <= 0 || numLeafs <= 0 || numClusters <= 0 )
		return false;

	const clock::time_point t0 = clock::now();

	// The tree: the nodes, then the leafs, then the outside node.
	PortalBuilder builder;
	builder.planes.resize( numPlanes );
	for ( int i = 0; i < numPlanes; i++ )
	{
		for ( int j = 0; j < 3; j++ )
			builder.planes[i].normal[j] = planes[i].normal[j];
		builder.planes[i].dist = planes[i].dist;
	}

	builder.nodes.resize( numNodes + numLeafs + 1 );
	builder.outside = numNodes + numLeafs;

	for ( size_t i = 0; i < builder.nodes.size(); i++ )
	{
		TreeNode &n = builder.nodes[i];
		n.plane = -1;
		n.children[0] = n.children[1] = -1;
		n.parent = -1;
		n.portals = -1;
		n.cluster = -1;
		n.opaque = false;
	}

	for ( int i = 0; i < numNodes; i++ )
	{
		TreeNode &n = builder.nodes[i];

		if ( nodes[i].planeNum < 0 || nodes[i].planeNum >= numPlanes )
			return false;

		n.plane = nodes[i].planeNum;

		for ( int j = 0; j < 2; j++ )
		{
			const int c = nodes[i].children[j];
			const int child = c >= 0 ? c : numNodes + ( -1 - c );

			if ( child < 0 || child >= numNodes + numLeafs )
				return false;

			n.children[j] = child;
			builder.nodes[child].parent = i;
		}
	}

	for ( int i = 0; i < numLeafs; i++ )
	{
		TreeNode &leaf = builder.nodes[numNodes + i];
		leaf.cluster = leafClusters[i] < numClusters ? leafClusters[i] : -1;
		leaf.opaque = leafClusters[i] < 0;
	}

	{
		double mins[3], maxs[3];
		for ( int i = 0; i < 3; i++ )
		{
			mins[i] = nodes[0].mins[i];
			maxs[i] = nodes[0].maxs[i];
		}

		builder.MakeHeadnodePortals( mins, maxs );
	}

	builder.MakeTreePortals();

	if ( builder.failed )
		return false;

	// The portals between two clusters (q3map2 WritePortalFile, LoadPortals).
	std::vector< std::vector<VisPortal> > clusterPortals( numClusters );
	std::vector<Vec3> points;

	for ( int i = 0; i < numLeafs; i++ )
	{
		const int leaf = numNodes + i;

		if ( builder.nodes[leaf].opaque )
			continue;

		for ( int p = builder.nodes[leaf].portals; p >= 0; )
		{
			const TreePortal &portal = builder.portals[p];
			const int side = portal.nodes[1] == leaf ? 1 : 0;
			const int next = portal.next[side];

			if ( side == 0 && portal.onnode >= 0 && !portal.winding.empty() )
			{
				const TreeNode &a = builder.nodes[portal.nodes[0]];
				const TreeNode &b = builder.nodes[portal.nodes[1]];

				if ( !a.opaque && !b.opaque && a.cluster >= 0 && b.cluster >= 0 && a.cluster != b.cluster )
				{
					const int firstPoint = (int)points.size();
					const int numPoints = (int)portal.winding.size();
					points.insert( points.end(), portal.winding.begin(), portal.winding.end() );

					VisPortal forward = { Flip( portal.plane ), b.cluster, firstPoint, numPoints };
					VisPortal backward = { portal.plane, a.cluster, firstPoint, numPoints };

					clusterPortals[a.cluster].push_back( forward );
					clusterPortals[b.cluster].push_back( backward );
				}
			}

			p = next;
		}
	}

	// The portals of a cluster are all[first[c]] to all[first[c + 1] - 1].
	std::vector<int> firstVec( numClusters + 1, 0 );
	for ( int c = 0; c < numClusters; c++ )
		firstVec[c + 1] = firstVec[c] + (int)clusterPortals[c].size();

	const int numPortals = firstVec[numClusters];
	std::vector<VisPortal> allVec;
	allVec.reserve( numPortals );
	for ( int c = 0; c < numClusters; c++ )
		allVec.insert( allVec.end(), clusterPortals[c].begin(), clusterPortals[c].end() );

	const clock::time_point t1 = clock::now();

	// One PVS row for each cluster: the cluster, and the clusters that the flood of its portals reaches (q3map2 ClusterMerge).
	clusterBytes = ( ( numClusters + 63 ) & ~63 ) >> 3;
	vis.assign( (size_t)numClusters * clusterBytes, 0 );

	const int *first = firstVec.data();
	const VisPortal *all = allVec.data();
	const Vec3 *pointData = points.data();
	uint8_t *visData = vis.data();
	const int rowBytes = clusterBytes;
	std::atomic<int> nextCluster( 0 );

	auto worker = [&]()
	{
		// Generation stamps: a portal is flooded, or its front test is known, for the current source portal.
		// A portal goes on the stack at most once for each source portal.
		std::vector<uint32_t> floodGenVec( numPortals + 1, 0 ), frontGenVec( numPortals + 1, 0 );
		std::vector<uint8_t> frontValVec( numPortals + 1, 0 );
		std::vector<int> stackVec( numPortals + 1, 0 );
		uint32_t *floodGen = floodGenVec.data();
		uint32_t *frontGen = frontGenVec.data();
		uint8_t *frontVal = frontValVec.data();
		int *stack = stackVec.data();
		uint32_t gen = 0;

		for ( ;; )
		{
			const int c = nextCluster++;
			if ( c >= numClusters )
				break;

			uint8_t *row = visData + (size_t)c * rowBytes;
			VISGEN_SET_BIT( row, c );

			for ( int pi = first[c]; pi < first[c + 1]; pi++ )
			{
				const VisPortal *p = all + pi;
				int depth = 0;
				gen++;

				VISGEN_SET_BIT( row, p->leaf );
				stack[depth++] = p->leaf;

				while ( depth )
				{
					const int leaf = stack[--depth];

					for ( int qi = first[leaf]; qi < first[leaf + 1]; qi++ )
					{
						if ( qi == pi || floodGen[qi] == gen )
							continue;

						if ( frontGen[qi] != gen )
						{
							frontGen[qi] = gen;
							frontVal[qi] = PortalFront( p, all + qi, pointData ) ? 1 : 0;
						}

						if ( !frontVal[qi] )
							continue;

						floodGen[qi] = gen;
						VISGEN_SET_BIT( row, all[qi].leaf );
						stack[depth++] = all[qi].leaf;
					}
				}
			}
		}
	};

	if ( numThreads < 1 )
		numThreads = 1;

	std::vector<std::thread> threads;
	for ( int i = 1; i < numThreads; i++ )
		threads.emplace_back( worker );
	worker();
	for ( std::thread &t : threads )
		t.join();

	const clock::time_point t2 = clock::now();

	if ( stats )
	{
		size_t visible = 0;
		for ( size_t i = 0; i < vis.size(); i++ )
		{
			for ( uint8_t b = vis[i]; b; b &= b - 1 )
				visible++;
		}

		stats->clusterPortals = numPortals;
		stats->avgVisible = (double)visible / numClusters;
		stats->msPortals = (int)std::chrono::duration_cast<std::chrono::milliseconds>( t1 - t0 ).count();
		stats->msVis = (int)std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
	}

	return true;
}
