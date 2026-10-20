/*
===========================================================================
Copyright (C) 2026, OpenJK-SWGL contributors

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

// GLA merge: the renderer appends the frames of other GLA files to a base GLA.
// The game reads the same files to find the frame offsets. Both use these
// checks, so that they make the same decision.
//
// 1. _weapons.gla is appended to each models/players/*/_humanoid*.gla.
// 2. A model folder can contain animoverride.cfg with "overridename <name>".
//    The model then uses models/players/_humanoid_o_<key>/_humanoid_o_<key>.gla,
//    a virtual GLA: _humanoid.gla (with _weapons.gla), then <name>.gla.
//    <key> is <name> without its leading '_': "_plx1" and "plx1" give _humanoid_o_plx1.

#pragma once

#include "mdx_format.h"

#define GLA_HUMANOID_DIR		"models/players/_humanoid"
#define GLA_HUMANOID_PATH		GLA_HUMANOID_DIR "/_humanoid.gla"
#define GLA_WEAPONS_DIR			"models/weapons2/_animations"
#define GLA_WEAPONS_PATH		GLA_WEAPONS_DIR "/_weapons.gla"
#define GLA_OVERRIDE_FILE		"animoverride.cfg"
#define GLA_OVERRIDE_KEY		"overridename"
#define GLA_OVERRIDE_SKELETON	"_humanoid_o_"

// animation_t::firstFrame is an unsigned short.
#define GLA_MAX_MERGED_FRAMES	65535
// A frame index keeps a bone pool index in 3 bytes.
#define GLA_MAX_BONE_POOL		(1 << 24)

// Returns the header if the first 'len' bytes of 'buf' hold a valid header and skeleton, else NULL.
inline const mdxaHeader_t *GLA_CheckHeader( const void *buf, int len )
{
	const mdxaHeader_t *h = (const mdxaHeader_t *)buf;

	if ( !buf || len < (int)sizeof( mdxaHeader_t ) )
		return NULL;
	if ( LittleLong( h->ident ) != MDXA_IDENT || LittleLong( h->version ) != MDXA_VERSION )
		return NULL;

	const int numBones	= LittleLong( h->numBones );
	const int ofsFrames	= LittleLong( h->ofsFrames );
	const int ofsPool	= LittleLong( h->ofsCompBonePool );
	const int ofsEnd	= LittleLong( h->ofsEnd );

	// The merge keeps the layout: header, skeleton, frames, bone pool.
	// Ghoul2 reads the skeleton offsets directly after the header.
	if ( numBones <= 0 || LittleLong( h->numFrames ) <= 0 )
		return NULL;
	if ( ofsFrames > len )
		return NULL;
	if ( (int)sizeof( mdxaHeader_t ) + numBones * (int)sizeof( int ) > ofsFrames )
		return NULL;
	if ( ofsFrames + LittleLong( h->numFrames ) * numBones * 3 > ofsPool || ofsPool > ofsEnd )
		return NULL;

	return h;
}

// Returns the bone of a header from GLA_CheckHeader.
inline const mdxaSkel_t *GLA_GetSkel( const mdxaHeader_t *h, int bone )
{
	const mdxaSkelOffsets_t *offsets = (const mdxaSkelOffsets_t *)((const byte *)h + sizeof( mdxaHeader_t ));
	return (const mdxaSkel_t *)((const byte *)h + sizeof( mdxaHeader_t ) + LittleLong( offsets->offsets[bone] ));
}

// Returns qtrue if 'extra' can be appended to 'base'.
// Both are headers from GLA_CheckHeader. 'baseFrames' is the frame count of the base after its own merges.
inline qboolean GLA_CanMerge( const mdxaHeader_t *base, int baseFrames, const mdxaHeader_t *extra )
{
	const int numBones = LittleLong( base->numBones );

	if ( LittleLong( extra->numBones ) != numBones )
		return qfalse;
	if ( baseFrames + LittleLong( extra->numFrames ) > GLA_MAX_MERGED_FRAMES )
		return qfalse;

	const int skelEnd = LittleLong( base->ofsFrames );
	const int extraSkelEnd = LittleLong( extra->ofsFrames );
	for ( int i = 0; i < numBones; i++ )
	{
		const mdxaSkel_t *a = GLA_GetSkel( base, i );
		const mdxaSkel_t *b = GLA_GetSkel( extra, i );

		// The skeleton data must be in the bytes that GLA_CheckHeader accepted.
		if ( (const byte *)a < (const byte *)(base + 1) || (const byte *)a + sizeof( mdxaSkel_t ) > (const byte *)base + skelEnd )
			return qfalse;
		if ( (const byte *)b < (const byte *)(extra + 1) || (const byte *)b + sizeof( mdxaSkel_t ) > (const byte *)extra + extraSkelEnd )
			return qfalse;
		if ( Q_stricmp( a->name, b->name ) || LittleLong( a->parent ) != LittleLong( b->parent ) )
			return qfalse;
	}
	return qtrue;
}

// Gets <key> from "models/players/_humanoid_o_<key>/_humanoid_o_<key>.gla".
// Returns qfalse if 'path' does not have this form.
inline qboolean GLA_GetOverrideName( const char *path, char *name, int nameSize )
{
	static const char prefix[] = "models/players/" GLA_OVERRIDE_SKELETON;
	const int prefixLen = (int)strlen( prefix );

	if ( Q_stricmpn( path, prefix, prefixLen ) )
		return qfalse;

	const char *start = path + prefixLen;
	const char *slash = strchr( start, '/' );
	if ( !slash || slash == start || slash - start >= nameSize )
		return qfalse;

	Q_strncpyz( name, start, (int)(slash - start) + 1 );

	char file[MAX_QPATH];
	Com_sprintf( file, sizeof( file ), GLA_OVERRIDE_SKELETON "%s.gla", name );
	return (qboolean)!Q_stricmp( slash + 1, file );
}

// Writes the skeleton folder of an override: "_humanoid_o_<key>".
inline void GLA_OverrideSkeleton( const char *name, char *skeleton, int skeletonSize )
{
	while ( *name == '_' )
		name++;
	Com_sprintf( skeleton, skeletonSize, GLA_OVERRIDE_SKELETON "%s", name );
}

// Writes the virtual GLA path of an override: "models/players/_humanoid_o_<key>/_humanoid_o_<key>.gla".
inline void GLA_OverridePath( const char *name, char *path, int pathSize )
{
	char skeleton[MAX_QPATH];
	GLA_OverrideSkeleton( name, skeleton, sizeof( skeleton ) );
	Com_sprintf( path, pathSize, "models/players/%s/%s.gla", skeleton, skeleton );
}

// Returns qtrue for "models/players/<folder>/_humanoid*.gla": _weapons.gla is appended to it.
// The virtual GLA of an override gets the weapons from its _humanoid.gla.
inline qboolean GLA_TakesWeapons( const char *path )
{
	static const char prefix[] = "models/players/";
	const int prefixLen = (int)strlen( prefix );
	char overrideName[MAX_QPATH];

	if ( Q_stricmpn( path, prefix, prefixLen ) || GLA_GetOverrideName( path, overrideName, sizeof( overrideName ) ) )
		return qfalse;

	const char *file = strchr( path + prefixLen, '/' );
	if ( !file || strchr( file + 1, '/' ) )
		return qfalse;
	file++;

	const int len = (int)strlen( file );
	return (qboolean)( !Q_stricmpn( file, "_humanoid", 9 ) && len > 4 && !Q_stricmp( file + len - 4, ".gla" ) );
}
