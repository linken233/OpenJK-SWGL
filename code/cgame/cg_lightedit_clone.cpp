// Light edit mode: tool 7 (clone).

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>
#include <string.h>

#define LEDIT_MAX_COPIES		32

static int	s_copies = 1;		// copies of the array made by the secondary fire

void LE_CloneInit( void )
{
	s_copies = 1;
}

// Adds one copy of src at org. The copy is as bright as the source. Returns the new id, or -1.
static int LE_CloneLight( const rtxLightDesc_t &src, const vec3_t org )
{
	rtxLightDesc_t	d = src;
	rtxLightDesc_t	after;
	const float		scaleSrc = s_api->IntensityScale( src.id );
	const float		scaleNew = s_api->IntensityScale( -1 );
	const size_t	len = strlen( d.name );
	int				id;

	VectorCopy( org, d.origin );
	if ( scaleSrc > 0.0f && scaleNew > 0.0f )
	{
		d.intensity = src.intensity * scaleSrc / scaleNew;
	}
	if ( len > 0 && len + strlen( "_copy" ) < sizeof( d.name ) )
	{
		Q_strcat( d.name, sizeof( d.name ), "_copy" );
	}
	id = s_api->Add( &d );
	if ( id < 0 )
	{
		LE_Msg( "light edit: clone failed (%s)", s_api->LastError() );
		return -1;
	}
	if ( LE_GetDesc( id, &after ) )
	{
		LE_UndoPush( LEDU_ADD, id, NULL, &after, "clone" );
	}
	return id;
}

// Sources of a clone: the descriptors of the targets that still exist.
static void LE_CloneSources( std::vector<rtxLightDesc_t> &out )
{
	std::vector<int>	ids;

	LE_ActionTargets( ids );
	for ( size_t i = 0; i < ids.size(); i++ )
	{
		rtxLightDesc_t	d;

		if ( LE_GetDesc( ids[i], &d ) && !( d.flags & RTX_LFLAG_DELETED ) )
		{
			if ( LE_IsSurface( &d ) )
			{
				LE_Msg( "light edit: clone does not apply to an emissive surface" );
				continue;
			}
			out.push_back( d );
		}
	}
}

static void LE_CloneSelect( const std::vector<int> &ids )
{
	LE_SelClear();
	for ( size_t i = 0; i < ids.size(); i++ )
	{
		LE_SelAdd( ids[i] );
	}
}

// Primary fire: copies in place, then the copies are grabbed.
static void LE_CloneAndGrab( void )
{
	std::vector<rtxLightDesc_t>	src;
	std::vector<int>			clones;
	size_t						primary = 0;

	LE_CloneSources( src );
	if ( src.empty() )
	{
		LE_Msg( "light edit: nothing to clone" );
		return;
	}
	LE_SoloEndForAdd();
	{
		ledBatch	batch;

		LE_UndoBegin( "clone" );
		for ( size_t i = 0; i < src.size(); i++ )
		{
			const int	id = LE_CloneLight( src[i], src[i].origin );

			if ( id < 0 )
			{
				break;
			}
			if ( src[i].id == s_pickId )
			{
				primary = clones.size();
			}
			clones.push_back( id );
		}
		LE_UndoEnd();
	}
	if ( clones.empty() )
	{
		return;
	}
	LE_CloneSelect( clones );
	LE_Msg( "light edit: cloned %d light%s", (int)clones.size(), clones.size() > 1 ? "s" : "" );

	vec3_t	rel;
	float	dist = s_pickT;

	if ( s_pickId < 0 )
	{
		VectorSubtract( src[primary].origin, cg.refdef.vieworg, rel );
		dist = Q_max( 16.0f, DotProduct( rel, cg.refdef.viewaxis[0] ) );
	}
	LE_SetTool( LEDIT_TOOL_MOVE );
	LE_BeginGrab( clones[primary], clones, dist );
}

// Secondary fire: s_copies copies at k grid steps along the world axis nearest to the view right vector.
static void LE_CloneArray( void )
{
	std::vector<rtxLightDesc_t>	src;
	std::vector<int>			clones;
	vec3_t						step;
	int							skipped = 0;
	qboolean					failed = qfalse;

	LE_CloneSources( src );
	if ( src.empty() )
	{
		LE_Msg( "light edit: nothing to clone" );
		return;
	}

	const float	*left = cg.refdef.viewaxis[1];		// the view right vector is the opposite
	const int	axis = ( fabsf( left[0] ) >= fabsf( left[1] ) ) ? 0 : 1;
	const float	sign = ( left[axis] > 0.0f ) ? -1.0f : 1.0f;

	VectorClear( step );
	step[axis] = sign * (float)LE_GridSize();

	LE_SoloEndForAdd();
	{
		ledBatch	batch;

		LE_UndoBegin( "clone array" );
		for ( int k = 1; k <= s_copies && !failed; k++ )
		{
			for ( size_t i = 0; i < src.size(); i++ )
			{
				vec3_t	org;
				int		id;

				VectorMA( src[i].origin, (float)k, step, org );
				LE_SnapPoint( org, 7 );
				if ( s_api->PointInSolid( org ) )
				{
					skipped++;
					continue;
				}
				id = LE_CloneLight( src[i], org );
				if ( id < 0 )
				{
					failed = qtrue;
					break;
				}
				clones.push_back( id );
			}
		}
		LE_UndoEnd();
	}
	if ( !clones.empty() )
	{
		LE_CloneSelect( clones );
	}
	LE_Msg( "light edit: %d cop%s along %c%s%s", (int)clones.size(), clones.size() == 1 ? "y" : "ies",
		sign < 0.0f ? '-' : '+', axis == 0 ? "X" : "Y", skipped ? va( " (%d skipped, outside the world)", skipped ) : "" );
}

void LE_ToolClone( qboolean priDown, qboolean secDown, int wheel )
{
	s_copies = Com_Clampi( 1, LEDIT_MAX_COPIES, s_copies + wheel );
	if ( priDown )
	{
		LE_CloneAndGrab();
	}
	else if ( secDown )
	{
		LE_CloneArray();
	}
}

void LE_CloneHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize )
{
	*name = "7 Clone";
	*fire = "copy the aimed light (or the selection) and grab the copies";
	*alt = va( "%d copies, one grid step (%d) apart along X or Y", s_copies, LE_GridSize() );
	Com_sprintf( wheelBuf, wheelSize, "number of copies 1..%d (now %d)", LEDIT_MAX_COPIES, s_copies );
}
