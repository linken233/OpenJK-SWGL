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

// GLA merge, renderer side. See mdx_merge.h.
// The merged GLA goes into the model cache like a file from the disk.

#include "../server/exe_headers.h"

#include "tr_common.h"
#include "mdx_merge.h"

// Each renderer: registers a GLA and returns its data in the model cache, or NULL.
const mdxaHeader_t *R_GetRegisteredGLA( const char *path );

// Reads a GLA file. Returns its header, or NULL if the file is missing or not valid.
static const mdxaHeader_t *R_ReadGLA( const char *path, void **buffer )
{
	const int len = ri.FS_ReadFile( path, buffer );
	if ( len <= 0 || !*buffer )
	{
		*buffer = NULL;
		return NULL;
	}

	const mdxaHeader_t *h = GLA_CheckHeader( *buffer, len );
	if ( !h || LittleLong( h->ofsEnd ) > len )
	{
		ri.Printf( PRINT_WARNING, "GLA merge: %s is not a valid GLA file\n", path );
		ri.FS_FreeFile( *buffer );
		*buffer = NULL;
		return NULL;
	}
	return h;
}

static int R_GLABonePoolSize( const mdxaHeader_t *h )
{
	return ( LittleLong( h->ofsEnd ) - LittleLong( h->ofsCompBonePool ) ) / (int)sizeof( mdxaCompQuatBone_t );
}

// Appends the frames of 'extra' to 'base'. Returns a new buffer in GLA file format, or NULL.
// 'name' replaces the internal GLA name if it is not NULL.
static void *R_MergeGLA( const mdxaHeader_t *base, const mdxaHeader_t *extra, const char *name )
{
	const int numBones		= LittleLong( base->numBones );
	const int baseFrames	= LittleLong( base->numFrames );
	const int extraFrames	= LittleLong( extra->numFrames );
	const int ofsFrames		= LittleLong( base->ofsFrames );
	const int basePool		= R_GLABonePoolSize( base );
	const int extraPool		= R_GLABonePoolSize( extra );

	if ( basePool + extraPool > GLA_MAX_BONE_POOL )
	{
		return NULL;
	}

	const int baseFrameBytes	= baseFrames * numBones * 3;
	const int ofsPool			= ( ofsFrames + baseFrameBytes + extraFrames * numBones * 3 + 3 ) & ~3;
	const int ofsEnd			= ofsPool + ( basePool + extraPool ) * (int)sizeof( mdxaCompQuatBone_t );

	byte *out = (byte *)R_Malloc( ofsEnd, TAG_FILESYS, qtrue );

	// Header and skeleton of the base.
	memcpy( out, base, ofsFrames );

	mdxaHeader_t *h = (mdxaHeader_t *)out;
	if ( name )
	{
		Q_strncpyz( h->name, name, sizeof( h->name ) );
	}
	h->numFrames		= LittleLong( baseFrames + extraFrames );
	h->ofsCompBonePool	= LittleLong( ofsPool );
	h->ofsEnd			= LittleLong( ofsEnd );

	// Frames: the base frames, then the extra frames with their pool indices moved after the base pool.
	byte *dst = out + ofsFrames;
	memcpy( dst, (const byte *)base + ofsFrames, baseFrameBytes );
	dst += baseFrameBytes;

	const byte *src = (const byte *)extra + LittleLong( extra->ofsFrames );
	for ( int i = 0; i < extraFrames * numBones; i++, src += 3, dst += 3 )
	{
		const int index = src[0] | ( src[1] << 8 ) | ( src[2] << 16 );
		if ( index >= extraPool )
		{
			R_Free( out );
			return NULL;
		}

		const int moved = index + basePool;
		dst[0] = moved & 0xFF;
		dst[1] = ( moved >> 8 ) & 0xFF;
		dst[2] = ( moved >> 16 ) & 0xFF;
	}

	// Bone pools: the base pool, then the extra pool.
	memcpy( out + ofsPool,
		(const byte *)base + LittleLong( base->ofsCompBonePool ),
		basePool * sizeof( mdxaCompQuatBone_t ) );
	memcpy( out + ofsPool + basePool * sizeof( mdxaCompQuatBone_t ),
		(const byte *)extra + LittleLong( extra->ofsCompBonePool ),
		extraPool * sizeof( mdxaCompQuatBone_t ) );

	return out;
}

// Loads a models/players/*/_humanoid*.gla file with _weapons.gla appended, if _weapons.gla exists.
// Returns qfalse if the file is missing or not valid.
static qboolean R_LoadWeaponsGLA( const char *path, void **buffer )
{
	void *base;
	const mdxaHeader_t *baseHeader = R_ReadGLA( path, &base );
	if ( !baseHeader )
	{
		return qfalse;
	}

	void *weapons;
	const mdxaHeader_t *weaponsHeader = R_ReadGLA( GLA_WEAPONS_PATH, &weapons );
	if ( !weaponsHeader )
	{
		*buffer = base;
		return qtrue;
	}

	void *merged = NULL;
	if ( GLA_CanMerge( baseHeader, LittleLong( baseHeader->numFrames ), weaponsHeader ) )
	{
		merged = R_MergeGLA( baseHeader, weaponsHeader, NULL );
	}

	if ( merged )
	{
		ri.Printf( PRINT_DEVELOPER, "GLA merge: %s frames %d to %d come from %s\n", path,
			LittleLong( baseHeader->numFrames ), LittleLong( ((mdxaHeader_t *)merged)->numFrames ) - 1, GLA_WEAPONS_PATH );
		ri.FS_FreeFile( base );
		*buffer = merged;
	}
	else
	{
		ri.Printf( PRINT_WARNING, "GLA merge: %s does not have the skeleton of %s, or too many frames. It is not used.\n",
			GLA_WEAPONS_PATH, path );
		*buffer = base;
	}

	ri.FS_FreeFile( weapons );
	return qtrue;
}

// Loads the virtual GLA of an animation override: _humanoid.gla (with _weapons.gla), then <name>.gla.
// _humanoid.gla comes from the model cache. It is registered now if it is not loaded yet.
static qboolean R_LoadOverrideGLA( const char *overrideKey, void **buffer )
{
	const mdxaHeader_t *baseHeader = R_GetRegisteredGLA( GLA_HUMANOID_PATH );
	if ( !baseHeader || !GLA_CheckHeader( baseHeader, LittleLong( baseHeader->ofsEnd ) ) )
	{
		return qfalse;
	}

	// The override GLA is models/players/_<key>/_<key>.gla, else models/players/<key>/<key>.gla.
	char overrideName[MAX_QPATH];
	char extraPath[MAX_QPATH];
	Com_sprintf( overrideName, sizeof( overrideName ), "_%s", overrideKey );
	Com_sprintf( extraPath, sizeof( extraPath ), "models/players/%s/%s.gla", overrideName, overrideName );
	if ( ri.FS_ReadFile( extraPath, NULL ) <= 0 )
	{
		Q_strncpyz( overrideName, overrideKey, sizeof( overrideName ) );
		Com_sprintf( extraPath, sizeof( extraPath ), "models/players/%s/%s.gla", overrideName, overrideName );
	}

	// The internal name follows the custom skeletons: "models/players/_humanoid_o_<key>/_humanoid".
	char mergedName[MAX_QPATH];
	Com_sprintf( mergedName, sizeof( mergedName ), "models/players/" GLA_OVERRIDE_SKELETON "%s/_humanoid", overrideKey );

	void *extra;
	const mdxaHeader_t *extraHeader = R_ReadGLA( extraPath, &extra );
	if ( !extraHeader )
	{
		return qfalse;
	}

	void *merged = NULL;
	if ( GLA_CanMerge( baseHeader, LittleLong( baseHeader->numFrames ), extraHeader ) )
	{
		merged = R_MergeGLA( baseHeader, extraHeader, mergedName );
	}

	if ( merged )
	{
		ri.Printf( PRINT_DEVELOPER, "GLA merge: animation override %s: frames %d to %d come from %s\n", overrideName,
			LittleLong( baseHeader->numFrames ), LittleLong( ((mdxaHeader_t *)merged)->numFrames ) - 1, extraPath );
	}
	else
	{
		ri.Printf( PRINT_WARNING, "GLA merge: %s does not have the skeleton of %s, or too many frames\n",
			extraPath, GLA_HUMANOID_PATH );
	}

	ri.FS_FreeFile( extra );
	*buffer = merged;
	return (qboolean)( merged != NULL );
}

/*
================
R_LoadMergedGLA

Loads a GLA that has merged frames: a _humanoid*.gla file in a models/players folder,
or the virtual GLA of an animation override.
Returns qfalse for all other files; the caller then reads the file from the disk.
On qtrue, *buffer is a buffer that ri.FS_FreeFile can release.
================
*/
qboolean R_LoadMergedGLA( const char *path, void **buffer )
{
	char overrideKey[MAX_QPATH];

	if ( GLA_GetOverrideName( path, overrideKey, sizeof( overrideKey ) ) )
	{
		return R_LoadOverrideGLA( overrideKey, buffer );
	}

	if ( GLA_TakesWeapons( path ) )
	{
		return R_LoadWeaponsGLA( path, buffer );
	}

	return qfalse;
}


/*
================
R_GetAnimOverrideGLA

Reads animoverride.cfg in the folder of a _humanoid model.
Returns the path of the virtual GLA for the model, or NULL if it has no animation override.
================
*/
const char *R_GetAnimOverrideGLA( const char *modelPath, const char *animName )
{
	static char glaPath[MAX_QPATH];
	char cfgPath[MAX_QPATH];
	char overrideName[MAX_QPATH] = { 0 };
	char *text;

	if ( Q_stricmp( animName, GLA_HUMANOID_DIR "/_humanoid" ) )
	{
		return NULL;
	}

	Q_strncpyz( cfgPath, modelPath, sizeof( cfgPath ) );
	char *slash = strrchr( cfgPath, '/' );
	if ( !slash )
	{
		return NULL;
	}
	Q_strncpyz( slash + 1, GLA_OVERRIDE_FILE, sizeof( cfgPath ) - (int)( slash + 1 - cfgPath ) );

	if ( ri.FS_ReadFile( cfgPath, (void **)&text ) <= 0 || !text )
	{
		return NULL;
	}

	const char *p = text;
	COM_BeginParseSession();
	for ( const char *token = COM_ParseExt( &p, qtrue ); token[0]; token = COM_ParseExt( &p, qtrue ) )
	{
		if ( !Q_stricmp( token, GLA_OVERRIDE_KEY ) )
		{
			Q_strncpyz( overrideName, COM_ParseExt( &p, qfalse ), sizeof( overrideName ) );
			break;
		}
	}
	COM_EndParseSession();
	ri.FS_FreeFile( text );

	if ( !overrideName[0] )
	{
		ri.Printf( PRINT_WARNING, "%s: no \"" GLA_OVERRIDE_KEY "\" key\n", cfgPath );
		return NULL;
	}

	char extraPath[MAX_QPATH];
	Com_sprintf( extraPath, sizeof( extraPath ), "models/players/%s/%s.gla", overrideName, overrideName );
	if ( ri.FS_ReadFile( extraPath, NULL ) <= 0 )
	{
		ri.Printf( PRINT_WARNING, "%s: %s is missing\n", cfgPath, extraPath );
		return NULL;
	}

	GLA_OverridePath( overrideName, glaPath, sizeof( glaPath ) );
	return glaPath;
}
