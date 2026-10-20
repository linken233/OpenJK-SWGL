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

// PVS generation for a map without a vis lump. It has no dependency on the renderer.

#pragma once

#include <cstdint>
#include <vector>

// The layout of dplane_t in the BSP file.
struct visgenPlane_t {
	float	normal[3];
	float	dist;
};

// The layout of dnode_t in the BSP file. A negative child is -(leaf + 1).
struct visgenNode_t {
	int		planeNum;
	int		children[2];
	int		mins[3];
	int		maxs[3];
};

struct visgenStats_t {
	int		clusterPortals;		// portals between two clusters, one for each direction
	double	avgVisible;			// clusters in a PVS row, on average
	int		msPortals;			// time of the portal step
	int		msVis;				// time of the vis step
};

// Builds the PVS with the method of q3map2 -vis -fast: the portals of the BSP tree (MakeTreePortals),
// then for each portal the flood through the portals in front of it (BasePortalVis, SimpleFlood).
// leafClusters holds the cluster of each leaf, -1 for an opaque leaf.
// On success, vis holds numClusters rows of clusterBytes bytes.
bool VisGen_Build( const visgenPlane_t *planes, int numPlanes, const visgenNode_t *nodes, int numNodes,
	const int *leafClusters, int numLeafs, int numClusters, int numThreads,
	std::vector<uint8_t> &vis, int &clusterBytes, visgenStats_t *stats );
