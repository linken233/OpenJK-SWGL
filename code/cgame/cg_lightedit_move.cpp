// Light edit mode: tool 3 (move), the grab of a light or of a group, and its axis constraints.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>

#define LEDIT_SURF_RANGE		512.0f
#define LEDIT_SURF_DEFAULT		16.0f
#define LEDIT_AXIS_HALF_LEN		1536.0f
#define LEDIT_AXIS_SEGMENTS		24

typedef enum {
	LEDG_FREE = 0,
	LEDG_X,
	LEDG_Y,
	LEDG_Z,
	LEDG_PLANE,
	LEDG_NUM_MODES
} ledGrabMode_t;

typedef struct {
	int				id;
	vec3_t			rel;			// light origin minus the primary origin at the grab start
	rtxLightDesc_t	before;
} ledGrabItem_t;

typedef struct {
	qboolean	active;
	int			id;				// primary light: the one under the crosshair at the grab start
	float		dist;
	vec3_t		offset;			// primary origin minus the view ray point at the grab start
	vec3_t		lastValid;		// last accepted position of the primary light
	vec3_t		target;
	qboolean	targetBad;
	int			mode;			// ledGrabMode_t
	vec3_t		axisOrg;		// primary position when the constraint was chosen
	float		surfDist;		// distance from the primary light to the nearest surface at the grab start
	vec3_t		planeNormal;
	vec3_t		planeHit;
	qboolean	planeValid;
} ledGrab_t;

static ledGrab_t					s_grab;
static std::vector<ledGrabItem_t>	s_items;

static const vec4_t	colAxisX	= { 1.00f, 0.25f, 0.25f, 1.0f };
static const vec4_t	colAxisY	= { 0.25f, 1.00f, 0.30f, 1.0f };
static const vec4_t	colAxisZ	= { 0.35f, 0.50f, 1.00f, 1.0f };
static const vec4_t	colPlane	= { 1.00f, 0.90f, 0.20f, 1.0f };
static const vec4_t	colBad		= { 1.00f, 0.15f, 0.15f, 1.0f };

static const char *LE_ModeName( int mode )
{
	static const char *names[LEDG_NUM_MODES] = { "free", "X axis", "Y axis", "Z axis", "surface plane" };

	return names[mode];
}

void LE_GrabReset( void )
{
	memset( &s_grab, 0, sizeof( s_grab ) );
	s_items.clear();
}

qboolean LE_GrabActive( void )
{
	return s_grab.active;
}

int LE_GrabPrimary( void )
{
	return s_grab.id;
}

// Distance to the nearest surface along the six world axes, or the default when none is near.
static float LE_SurfaceDistance( const vec3_t org )
{
	static const vec3_t	dirs[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	float				best = LEDIT_SURF_RANGE + 1.0f;

	for ( int i = 0; i < 6; i++ )
	{
		trace_t	tr;
		vec3_t	end;

		VectorMA( org, LEDIT_SURF_RANGE, dirs[i], end );
		CG_Trace( &tr, org, vec3_origin, vec3_origin, end, cg.snap->ps.clientNum, CONTENTS_SOLID );
		if ( tr.fraction < 1.0f && !tr.startsolid && !tr.allsolid )
		{
			best = Q_min( best, tr.fraction * LEDIT_SURF_RANGE );
		}
	}
	return ( best > LEDIT_SURF_RANGE ) ? LEDIT_SURF_DEFAULT : Q_max( best, 2.0f );
}

// Starts a grab. primary is a member of ids; dist is its depth along the view axis.
void LE_BeginGrab( int primary, const std::vector<int> &ids, float dist )
{
	rtxLightDesc_t	p;
	vec3_t			hitPoint;

	LE_GrabReset();
	if ( !LE_GetDesc( primary, &p ) )
	{
		return;
	}
	for ( size_t i = 0; i < ids.size(); i++ )
	{
		ledGrabItem_t	it;

		if ( !LE_GetDesc( ids[i], &it.before ) || ( it.before.flags & RTX_LFLAG_DELETED ) )
		{
			continue;
		}
		if ( LE_IsSurface( &it.before ) )
		{
			LE_Msg( "light edit: an emissive surface is part of the map and cannot be moved" );
			continue;
		}
		it.id = ids[i];
		VectorSubtract( it.before.origin, p.origin, it.rel );
		s_items.push_back( it );
	}
	if ( s_items.empty() )
	{
		return;
	}

	s_grab.active = qtrue;
	s_grab.id = primary;
	s_grab.dist = dist;
	VectorMA( cg.refdef.vieworg, dist, cg.refdef.viewaxis[0], hitPoint );
	VectorSubtract( p.origin, hitPoint, s_grab.offset );
	VectorCopy( p.origin, s_grab.lastValid );
	VectorCopy( p.origin, s_grab.target );
	VectorCopy( p.origin, s_grab.axisOrg );
	s_grab.surfDist = LE_SurfaceDistance( p.origin );

	LE_SelClear();
	for ( size_t i = 0; i < s_items.size(); i++ )
	{
		LE_SelAdd( s_items[i].id );
	}
	LE_SelPrimary( primary );

	for ( size_t i = 0; i < s_items.size(); i++ )
	{
		s_api->BeginDrag( s_items[i].id );
	}
}

// Ends the grab. The lights that moved make one undo group.
void LE_EndGrab( void )
{
	if ( !s_grab.active )
	{
		return;
	}
	s_grab.active = qfalse;
	{
		ledBatch	batch;

		for ( size_t i = 0; i < s_items.size(); i++ )
		{
			s_api->EndDrag( s_items[i].id );
		}
	}

	LE_UndoBegin( "move" );
	for ( size_t i = 0; i < s_items.size(); i++ )
	{
		rtxLightDesc_t	after;

		if ( LE_GetDesc( s_items[i].id, &after ) && !LE_DescEqual( &s_items[i].before, &after ) )
		{
			LE_UndoPush( LEDU_SET, s_items[i].id, &s_items[i].before, &after, "move" );
		}
	}
	LE_UndoEnd();
	s_items.clear();
}

// Target of the primary light for the current constraint. Returns qfalse to keep the last position.
static qboolean LE_GrabTarget( vec3_t out )
{
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];

	s_grab.planeValid = qfalse;
	switch ( s_grab.mode )
	{
	case LEDG_FREE:
		VectorMA( eye, s_grab.dist, fwd, out );
		VectorAdd( out, s_grab.offset, out );
		LE_SnapPoint( out, 7 );
		return qtrue;

	case LEDG_X:
	case LEDG_Y:
	case LEDG_Z:
	{
		const int	axis = s_grab.mode - LEDG_X;

		if ( !LE_ClosestPointOnAxis( s_grab.axisOrg, axis, eye, fwd, out ) )
		{
			return qfalse;
		}
		LE_SnapPoint( out, 1 << axis );
		return qtrue;
	}

	case LEDG_PLANE:
	{
		trace_t	tr;
		vec3_t	end;
		int		mask = 0;

		VectorMA( eye, 8192.0f, fwd, end );
		CG_Trace( &tr, eye, vec3_origin, vec3_origin, end, cg.snap->ps.clientNum, CONTENTS_SOLID );
		if ( tr.fraction >= 1.0f || tr.startsolid || tr.allsolid || ( tr.surfaceFlags & SURF_SKY ) )
		{
			return qfalse;
		}
		VectorCopy( tr.endpos, s_grab.planeHit );
		VectorCopy( tr.plane.normal, s_grab.planeNormal );
		s_grab.planeValid = qtrue;
		VectorMA( tr.endpos, s_grab.surfDist, tr.plane.normal, out );
		for ( int i = 0; i < 3; i++ )
		{
			if ( fabsf( tr.plane.normal[i] ) <= 0.9f )
			{
				mask |= 1 << i;
			}
		}
		LE_SnapPoint( out, mask );
		return qtrue;
	}
	}
	return qfalse;
}

// One grab frame: the constraint, the wheel and the move of the whole group.
static void LE_GrabFrame( qboolean prim, qboolean secDown, int wheel, qboolean fine )
{
	vec3_t	t;

	if ( secDown )
	{
		s_grab.mode = ( s_grab.mode + 1 ) % LEDG_NUM_MODES;
		VectorCopy( s_grab.lastValid, s_grab.axisOrg );
		LE_Msg( "light edit: constraint %s", LE_ModeName( s_grab.mode ) );
	}
	if ( s_grab.mode == LEDG_FREE )
	{
		for ( int i = 0; i < abs( wheel ); i++ )
		{
			const float	f = fine ? 1.01f : 1.1f;

			s_grab.dist = ( wheel > 0 ) ? s_grab.dist * f : s_grab.dist / f;
			s_grab.dist = Com_Clamp( 16.0f, 8192.0f, s_grab.dist );
		}
	}
	if ( !prim )
	{
		LE_EndGrab();
		return;
	}

	if ( !LE_GrabTarget( t ) )
	{
		VectorCopy( s_grab.lastValid, t );
	}
	VectorCopy( t, s_grab.target );

	for ( size_t i = 0; i < s_items.size(); i++ )
	{
		vec3_t	p;

		VectorAdd( t, s_items[i].rel, p );
		if ( s_api->PointInSolid( p ) )
		{
			s_grab.targetBad = qtrue;
			return;
		}
	}
	s_grab.targetBad = qfalse;
	VectorCopy( t, s_grab.lastValid );

	ledBatch	batch;

	for ( size_t i = 0; i < s_items.size(); i++ )
	{
		rtxLightDesc_t	cur;
		vec3_t			p;

		VectorAdd( t, s_items[i].rel, p );
		if ( LE_GetDesc( s_items[i].id, &cur ) && !VectorCompare( cur.origin, p ) )
		{
			VectorCopy( p, cur.origin );
			s_api->Set( s_items[i].id, &cur );
		}
	}
}

// Secondary fire without a grab: the targets go back to their original positions.
static void LE_ResetPositions( void )
{
	std::vector<int>	ids;
	int					moved = 0;

	LE_ActionTargets( ids );
	if ( ids.empty() )
	{
		return;
	}

	ledBatch	batch;

	LE_UndoBegin( "reset position" );
	for ( size_t i = 0; i < ids.size(); i++ )
	{
		rtxLightDesc_t	orig, cur;

		if ( !s_api->GetOriginal( ids[i], &orig ) || !LE_GetDesc( ids[i], &cur ) )
		{
			LE_Msg( "light edit: no original position for light %d", ids[i] );
			continue;
		}
		if ( VectorCompare( cur.origin, orig.origin ) )
		{
			continue;
		}
		VectorCopy( orig.origin, cur.origin );
		if ( LE_DoSet( ids[i], &cur, "reset position" ) )
		{
			moved++;
		}
	}
	LE_UndoEnd();
	if ( moved )
	{
		LE_Msg( "light edit: %d light%s back to the original position", moved, moved > 1 ? "s" : "" );
	}
	else
	{
		LE_Msg( "light edit: already at the original position" );
	}
}

void LE_ToolMove( qboolean prim, qboolean priDown, qboolean secDown, int wheel, qboolean fine )
{
	if ( s_grab.active )
	{
		LE_GrabFrame( prim, secDown, wheel, fine );
		return;
	}
	if ( priDown && s_pickId >= 0 )
	{
		std::vector<int>	ids;

		LE_ActionTargets( ids );
		LE_BeginGrab( s_pickId, ids, s_pickT );
	}
	else if ( secDown )
	{
		LE_ResetPositions();
	}
}

// Draws the red target of a refused position, the constraint line and the plane hit.
void LE_DrawGrabWorld( void )
{
	if ( !s_grab.active )
	{
		return;
	}
	if ( s_grab.targetBad )
	{
		float	x1, y1, x2, y2;

		LE_Circle3D( s_grab.target, cg.refdef.viewaxis[1], cg.refdef.viewaxis[2], 8.0f, 24, colBad );
		if ( CG_WorldCoordToScreenCoordFloat( s_grab.lastValid, &x1, &y1 )
			&& CG_WorldCoordToScreenCoordFloat( s_grab.target, &x2, &y2 ) )
		{
			LE_Line( x1, y1, x2, y2, colBad );
		}
	}
	if ( s_grab.mode >= LEDG_X && s_grab.mode <= LEDG_Z )
	{
		const int		axis = s_grab.mode - LEDG_X;
		const float		*col = ( axis == 0 ) ? colAxisX : ( axis == 1 ) ? colAxisY : colAxisZ;
		const float		step = 2.0f * LEDIT_AXIS_HALF_LEN / LEDIT_AXIS_SEGMENTS;

		// Dotted line: every second segment is drawn.
		for ( int i = 0; i < LEDIT_AXIS_SEGMENTS; i += 2 )
		{
			vec3_t	a, b;
			float	x1, y1, x2, y2;

			VectorCopy( s_grab.axisOrg, a );
			VectorCopy( s_grab.axisOrg, b );
			a[axis] += -LEDIT_AXIS_HALF_LEN + step * i;
			b[axis] += -LEDIT_AXIS_HALF_LEN + step * ( i + 1 );
			if ( CG_WorldCoordToScreenCoordFloat( a, &x1, &y1 ) && CG_WorldCoordToScreenCoordFloat( b, &x2, &y2 ) )
			{
				LE_Line( x1, y1, x2, y2, col );
			}
		}
	}
	else if ( s_grab.mode == LEDG_PLANE && s_grab.planeValid )
	{
		vec3_t	ax1, ax2;

		PerpendicularVector( ax1, s_grab.planeNormal );
		CrossProduct( s_grab.planeNormal, ax1, ax2 );
		LE_Circle3D( s_grab.planeHit, ax1, ax2, 12.0f, 24, colPlane );
	}
}

// Help lines of tool 3. The strings stay valid until the next call.
void LE_MoveHelp( const char **name, const char **fire, const char **alt, const char **wheel )
{
	static char	altBuf[96];

	*name = "3 Move";
	if ( s_grab.active )
	{
		Com_sprintf( altBuf, sizeof( altBuf ), "constraint free>X>Y>Z>plane (now %s)", LE_ModeName( s_grab.mode ) );
		*fire = "hold to move; release to drop";
		*alt = altBuf;
		*wheel = ( s_grab.mode == LEDG_FREE ) ? "distance x1.1, walk x1.01" : "none in a constraint";
	}
	else
	{
		*fire = "hold to grab the aimed light (the group if it is selected)";
		*alt = "back to the original position";
		*wheel = "distance while grabbing";
	}
}
