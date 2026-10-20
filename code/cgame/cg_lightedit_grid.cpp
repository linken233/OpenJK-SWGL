// Light edit mode: grid, snap cvars and the maths of the axis constraints.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

static vmCvar_t		ledit_grid;
static vmCvar_t		ledit_snap;
static vmCvar_t		ledit_angle_snap;		// degrees; 0 turns the angle snap off
static vmCvar_t		ledit_spot_near;		// units; a new spot this close to a surface faces away from it; 0 turns it off

static const int	s_gridSizes[] = { 1, 2, 4, 8, 16, 32, 64, 128 };

void LE_GridInit( void )
{
	cgi_Cvar_Register( &ledit_grid, "ledit_grid", "16", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_snap, "ledit_snap", "0", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_angle_snap, "ledit_angle_snap", "15", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_spot_near, "ledit_spot_near", "64", 0 );
}

void LE_GridUpdate( void )
{
	cgi_Cvar_Update( &ledit_grid );
	cgi_Cvar_Update( &ledit_snap );
	cgi_Cvar_Update( &ledit_angle_snap );
	cgi_Cvar_Update( &ledit_spot_near );
}

// Aim of a new spot over a surface hit: away from the surface (+normal) when the ghost is
// within ledit_spot_near of the surface plane, else into it (-normal).
void LE_SpotCreateDir( const vec3_t ghost, const vec3_t hitPos, const vec3_t normal, vec3_t dir )
{
	vec3_t	d;

	VectorSubtract( ghost, hitPos, d );
	if ( ledit_spot_near.value > 0.0f && fabsf( DotProduct( d, normal ) ) <= ledit_spot_near.value )
	{
		VectorCopy( normal, dir );
	}
	else
	{
		VectorNegate( normal, dir );
	}
}

// A value that is not in the list gives 16.
int LE_GridSize( void )
{
	for ( size_t i = 0; i < ARRAY_LEN( s_gridSizes ); i++ )
	{
		if ( ledit_grid.integer == s_gridSizes[i] )
		{
			return s_gridSizes[i];
		}
	}
	return 16;
}

float LE_AngleSnap( void )
{
	return Com_Clamp( 0.0f, 90.0f, ledit_angle_snap.value );
}

qboolean LE_SnapOn( void )
{
	return (qboolean)( ledit_snap.integer != 0 );
}

void LE_SnapPoint( vec3_t p, int axisMask )
{
	const float	g = (float)LE_GridSize();

	if ( !LE_SnapOn() )
	{
		return;
	}
	for ( int i = 0; i < 3; i++ )
	{
		if ( axisMask & ( 1 << i ) )
		{
			p[i] = floorf( p[i] / g + 0.5f ) * g;
		}
	}
}

void LE_GridNext( void )
{
	const int	cur = LE_GridSize();
	int			next = s_gridSizes[0];

	for ( size_t i = 0; i < ARRAY_LEN( s_gridSizes ); i++ )
	{
		if ( s_gridSizes[i] == cur )
		{
			next = s_gridSizes[( i + 1 ) % ARRAY_LEN( s_gridSizes )];
			break;
		}
	}
	cgi_Cvar_Set( "ledit_grid", va( "%d", next ) );
	ledit_grid.integer = next;		// the cvar value reaches the module at the next update
	LE_Msg( "light edit: grid %d", next );
}

void LE_SnapToggle( void )
{
	const int	on = LE_SnapOn() ? 0 : 1;

	cgi_Cvar_Set( "ledit_snap", va( "%d", on ) );
	ledit_snap.integer = on;
	LE_Msg( "light edit: snap %s", on ? "on" : "off" );
}

// Point of the axis line (axisOrg, world axis) closest to the view ray. rayDir is a unit vector.
// Returns qfalse when the two lines are nearly parallel.
qboolean LE_ClosestPointOnAxis( const vec3_t axisOrg, int axis, const vec3_t rayOrg, const vec3_t rayDir, vec3_t out )
{
	vec3_t		w;
	const float	b = rayDir[axis];			// axis . ray, the axis is a unit vector
	const float	denom = 1.0f - b * b;

	if ( denom < 0.001f )
	{
		return qfalse;
	}
	VectorSubtract( axisOrg, rayOrg, w );

	const float	d = w[axis];				// axis . w
	const float	e = DotProduct( rayDir, w );
	const float	s = Com_Clamp( -16384.0f, 16384.0f, ( b * e - d ) / denom );

	VectorCopy( axisOrg, out );
	out[axis] += s;
	return qtrue;
}
