// Light edit mode: spots. Aim maths, the wire cone and tool 4 (orientation).

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>

#define LEDIT_RAD2DEG			57.2957795f
#define LEDIT_DEG2RAD			0.0174532925f
#define LEDIT_CONE_SEGS			24
#define LEDIT_CONE_GENERATORS	8
#define LEDIT_CONE_MAXLEN		512.0f
#define LEDIT_CONE_DEFLEN		128.0f
#define LEDIT_CONE_RIM_MAX		256.0f
#define LEDIT_CONE_DOT_FLOOR	1000		// the cones never use the last 1000 dots of the budget
#define LEDIT_CONE_EXTRA		3			// selected spots, besides the primary one, that get a cone

typedef enum {
	LEO_ROLL = 0,
	LEO_WIDTH,
	LEO_HEIGHT,
	LEO_NUM
} ledRectTarget_t;

static qboolean		s_editInner = qfalse;	// the wheel edits the inner angle
static int			s_rectTarget = LEO_ROLL;	// what the wheel edits on a rect

static const char	*s_rectTargetNames[LEO_NUM] = { "roll", "width", "height" };

static const vec4_t	colCone		= { 1.00f, 0.90f, 0.30f, 1.0f };
static const vec4_t	colAim		= { 1.00f, 0.55f, 0.10f, 1.0f };

void LE_OrientInit( void )
{
	s_editInner = qfalse;
	s_rectTarget = LEO_ROLL;
}

/*
=================
Aim maths
=================
*/
// World point under the crosshair. Sky and an empty view give qfalse.
qboolean LE_CrosshairHit( vec3_t pos, vec3_t normal )
{
	trace_t		tr;
	vec3_t		end;
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];

	VectorMA( eye, 8192.0f, fwd, end );
	CG_Trace( &tr, eye, vec3_origin, vec3_origin, end, cg.snap->ps.clientNum, CONTENTS_SOLID );
	if ( tr.fraction >= 1.0f || tr.startsolid || tr.allsolid || ( tr.surfaceFlags & SURF_SKY ) )
	{
		return qfalse;
	}
	VectorCopy( tr.endpos, pos );
	VectorCopy( tr.plane.normal, normal );
	return qtrue;
}

// Rounds the yaw and the pitch of a unit direction to multiples of ledit_angle_snap.
void LE_SnapDir( vec3_t dir )
{
	const float	snap = LE_AngleSnap();
	float		yaw, pitch, cp;

	if ( snap <= 0.0f )
	{
		return;
	}
	yaw = atan2f( dir[1], dir[0] ) * LEDIT_RAD2DEG;
	pitch = asinf( Com_Clamp( -1.0f, 1.0f, dir[2] ) ) * LEDIT_RAD2DEG;
	yaw = floorf( yaw / snap + 0.5f ) * snap * LEDIT_DEG2RAD;
	pitch = Com_Clamp( -90.0f, 90.0f, floorf( pitch / snap + 0.5f ) * snap ) * LEDIT_DEG2RAD;
	cp = cosf( pitch );
	dir[0] = cp * cosf( yaw );
	dir[1] = cp * sinf( yaw );
	dir[2] = sinf( pitch );
}

// Step of the wheel on a cone angle: the angle snap, or 1 degree when walking or when the snap is off.
float LE_AngleStep( qboolean fine )
{
	const float	snap = LE_AngleSnap();

	return ( fine || snap <= 0.0f ) ? 1.0f : snap;
}

// Outer angle 1..89. Inner angle 0..outer. A lower outer angle pulls the inner angle down.
void LE_ConeAdd( rtxLightDesc_t *d, qboolean inner, float delta )
{
	if ( inner )
	{
		d->coneInner = Com_Clamp( 0.0f, d->coneOuter, d->coneInner + delta );
		return;
	}
	d->coneOuter = Com_Clamp( 1.0f, 89.0f, d->coneOuter + delta );
	if ( d->coneInner > d->coneOuter )
	{
		d->coneInner = d->coneOuter;
	}
}

/*
=================
Wire cone
=================
*/
// A segment between two world points, drawn when both are in front of the camera.
static void LE_WireLine( const vec3_t a, const vec3_t b, const vec4_t col )
{
	vec3_t	pa, pb;
	float	x1, y1, x2, y2;

	VectorCopy( a, pa );
	VectorCopy( b, pb );
	if ( CG_WorldCoordToScreenCoordFloat( pa, &x1, &y1 ) && CG_WorldCoordToScreenCoordFloat( pb, &x2, &y2 ) )
	{
		LE_Line( x1, y1, x2, y2, col );
	}
}

/*
LE_DrawSpotWire
Cone of a spot: the axis to the world (512 units at most, 128 without a hit), the rim circle,
8 generators, the inner circle and the circle where the axis hits the world.
A full cone costs at most LEDIT_CONE_DOT_FLOOR dots; without full it is the axis and the rim only.
*/
void LE_DrawSpotWire( const vec3_t org, const vec3_t dir, float outer, float inner, const vec4_t col, qboolean full )
{
	trace_t		tr;
	vec3_t		end, u, v, center, p, hitN;
	vec4_t		dim;
	float		len, tanO, rimDist, rimR;
	qboolean	hit;

	if ( VectorLengthSquared( dir ) < 0.25f )
	{
		return;
	}
	VectorMA( org, LEDIT_CONE_MAXLEN, dir, end );
	CG_Trace( &tr, org, vec3_origin, vec3_origin, end, cg.snap->ps.clientNum, CONTENTS_SOLID );
	hit = (qboolean)( tr.fraction < 1.0f && !tr.startsolid && !tr.allsolid );
	len = hit ? Q_max( tr.fraction * LEDIT_CONE_MAXLEN, 8.0f ) : LEDIT_CONE_DEFLEN;

	PerpendicularVector( u, dir );
	CrossProduct( dir, u, v );

	tanO = tanf( Com_Clamp( 1.0f, 89.0f, outer ) * LEDIT_DEG2RAD );
	rimDist = len;
	if ( rimDist * tanO > LEDIT_CONE_RIM_MAX )
	{
		rimDist = LEDIT_CONE_RIM_MAX / tanO;
	}
	rimR = rimDist * tanO;
	VectorMA( org, rimDist, dir, center );
	dim[0] = col[0] * 0.6f;
	dim[1] = col[1] * 0.6f;
	dim[2] = col[2] * 0.6f;
	dim[3] = col[3];

	LE_DotFloor( LEDIT_CONE_DOT_FLOOR );

	VectorMA( org, len, dir, p );
	LE_WireLine( org, p, col );
	LE_Circle3D( center, u, v, rimR, LEDIT_CONE_SEGS, col );

	if ( full )
	{
		for ( int k = 0; k < LEDIT_CONE_GENERATORS; k++ )
		{
			const float	a = (float)k * ( 2.0f * M_PI / LEDIT_CONE_GENERATORS );

			VectorMA( center, cosf( a ) * rimR, u, p );
			VectorMA( p, sinf( a ) * rimR, v, p );
			LE_WireLine( org, p, dim );
		}
		if ( inner >= 1.0f && inner < outer )
		{
			LE_Circle3D( center, u, v, rimDist * tanf( inner * LEDIT_DEG2RAD ), LEDIT_CONE_SEGS, dim );
		}
		if ( hit && !( tr.surfaceFlags & SURF_SKY ) )
		{
			VectorCopy( tr.plane.normal, hitN );
			PerpendicularVector( u, hitN );
			CrossProduct( hitN, u, v );
			VectorMA( tr.endpos, 0.5f, hitN, p );
			LE_Circle3D( p, u, v, Q_min( len * tanO, LEDIT_CONE_RIM_MAX ), LEDIT_CONE_SEGS, colAim );
		}
	}
	LE_DotFloor( 0 );
}

static void LE_DrawSpotOf( int id, qboolean full )
{
	const rtxLightDesc_t	*d = LE_RecDesc( id );

	if ( d && d->type == RTX_LTYPE_SPOT && !( d->flags & RTX_LFLAG_DELETED ) )
	{
		LE_DrawSpotWire( d->origin, d->dir, d->coneOuter, d->coneInner, colCone, full );
	}
}

// The primary light gets the full cone. Other selected spots, up to three, get the axis and the rim.
void LE_DrawSpotWires( void )
{
	int	extra = 0;

	if ( s_sel >= 0 )
	{
		LE_DrawSpotOf( s_sel, qtrue );
	}
	for ( size_t i = 0; i < s_selList.size() && extra < LEDIT_CONE_EXTRA; i++ )
	{
		if ( s_selList[i] != s_sel )
		{
			LE_DrawSpotOf( s_selList[i], qfalse );
			extra++;
		}
	}
	if ( ( s_tool == LEDIT_TOOL_ORIENT || s_tool == LEDIT_TOOL_PROPS ) && s_pickId >= 0 && !LE_SelHas( s_pickId ) )
	{
		LE_DrawSpotOf( s_pickId, qfalse );
	}
}

/*
=================
Tool 4: orientation
=================
*/
// The light that decides what the secondary fire does: the aimed one, else the primary selected one.
static const rtxLightDesc_t *LE_OrientSubject( void )
{
	return LE_RecDesc( s_pickId >= 0 ? s_pickId : s_sel );
}

// Primary fire: every target spot looks at the crosshair hit.
static void LE_OrientAim( const std::vector<int> &ids )
{
	vec3_t	hit, nrm;
	int		done = 0;

	if ( ids.empty() )
	{
		LE_Msg( "light edit: aim at a spot or a rect, or select one" );
		return;
	}
	if ( !LE_CrosshairHit( hit, nrm ) )
	{
		LE_Msg( "light edit: nothing to aim at" );
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBegin( "aim" );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d;
			vec3_t			dir;

			if ( !LE_GetDesc( ids[i], &d ) || ( d.type != RTX_LTYPE_SPOT && d.type != RTX_LTYPE_RECT ) )
			{
				continue;
			}
			VectorSubtract( hit, d.origin, dir );
			if ( VectorNormalize( dir ) < 1.0f )
			{
				continue;
			}
			LE_SnapDir( dir );
			VectorCopy( dir, d.dir );
			if ( LE_DoSet( ids[i], &d, "aim" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( !done )
	{
		LE_Msg( "light edit: no spot or rect to aim (sphere: alt converts it to a spot)" );
	}
}

// Secondary fire on a sphere: converts the target spheres to spots that look at the crosshair hit, or down.
static void LE_OrientConvert( const std::vector<int> &ids )
{
	vec3_t	hit, nrm;
	int		done = 0;
	const qboolean	haveHit = LE_CrosshairHit( hit, nrm );

	{
		ledBatch	batch;

		LE_UndoBegin( "to spot" );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d;
			vec3_t			dir;

			if ( !LE_GetDesc( ids[i], &d ) || d.type != RTX_LTYPE_SPHERE )
			{
				continue;
			}
			VectorSet( dir, 0.0f, 0.0f, -1.0f );
			if ( haveHit )
			{
				VectorSubtract( hit, d.origin, dir );
				if ( VectorNormalize( dir ) < 1.0f )
				{
					VectorSet( dir, 0.0f, 0.0f, -1.0f );
				}
				else
				{
					LE_SnapDir( dir );
				}
			}
			d.type = RTX_LTYPE_SPOT;
			VectorCopy( dir, d.dir );
			if ( d.coneOuter < 1.0f )
			{
				d.coneOuter = 35.0f;
				d.coneInner = 25.0f;
			}
			if ( LE_DoSet( ids[i], &d, "to spot" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: %d light%s converted to spot%s", done, done > 1 ? "s" : "", done > 1 ? "s" : "" );
	}
}

// Wheel: the edited cone angle of every target spot, in one merged undo group per gesture.
static void LE_OrientWheel( const std::vector<int> &ids, int wheel, qboolean fine )
{
	const float	delta = (float)wheel * LE_AngleStep( fine );
	int			spots = 0;

	if ( ids.empty() )
	{
		LE_Msg( "light edit: aim at a spot or a rect, or select one" );
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBeginMerge( "cone", LE_HashTargets( 0x40 + ( s_editInner ? 1 : 0 ), ids ) );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d;

			if ( !LE_GetDesc( ids[i], &d ) || d.type != RTX_LTYPE_SPOT )
			{
				continue;
			}
			LE_ConeAdd( &d, s_editInner, delta );
			LE_DoSet( ids[i], &d, "cone" );
			spots++;
		}
		LE_UndoEnd();

		LE_UndoBeginMerge( s_rectTargetNames[s_rectTarget], LE_HashTargets( 0x48 + s_rectTarget, ids ) );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d;

			if ( !LE_GetDesc( ids[i], &d ) || d.type != RTX_LTYPE_RECT )
			{
				continue;
			}
			if ( s_rectTarget == LEO_ROLL )
			{
				d.roll = LE_RollStep( d.roll, wheel, fine );
			}
			else if ( s_rectTarget == LEO_WIDTH )
			{
				d.width = LE_RectSizeStep( d.width, wheel, fine );
			}
			else
			{
				d.height = LE_RectSizeStep( d.height, wheel, fine );
			}
			LE_DoSet( ids[i], &d, s_rectTargetNames[s_rectTarget] );
			spots++;
		}
		LE_UndoEnd();
	}
	if ( !spots )
	{
		LE_Msg( "light edit: no spot or rect among the targets" );
	}
}

void LE_ToolOrient( qboolean priDown, qboolean secDown, int wheel, qboolean fine )
{
	std::vector<int>		ids;
	const rtxLightDesc_t	*subject = LE_OrientSubject();

	LE_ActionTargets( ids );
	if ( priDown )
	{
		LE_OrientAim( ids );
	}
	if ( secDown )
	{
		if ( subject && subject->type == RTX_LTYPE_SPHERE )
		{
			LE_OrientConvert( ids );
		}
		else if ( subject && subject->type == RTX_LTYPE_RECT )
		{
			s_rectTarget = ( s_rectTarget + 1 ) % LEO_NUM;
			LE_Msg( "light edit: the wheel edits the %s of the rect", s_rectTargetNames[s_rectTarget] );
		}
		else
		{
			s_editInner = (qboolean)!s_editInner;
			LE_Msg( "light edit: the wheel edits the %s cone angle", s_editInner ? "inner" : "outer" );
		}
	}
	if ( wheel )
	{
		LE_OrientWheel( ids, wheel, fine );
	}
}

void LE_OrientHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize )
{
	const rtxLightDesc_t	*subject = LE_OrientSubject();
	const float				snap = LE_AngleSnap();

	static char	fireBuf[96], altBuf[96];

	*name = "4 Orient (spots, rects)";
	if ( snap > 0.0f )
	{
		Com_sprintf( fireBuf, sizeof( fireBuf ), "aim the spots and rects at the crosshair (direction snapped to %.0f deg)", snap );
	}
	else
	{
		Com_sprintf( fireBuf, sizeof( fireBuf ), "aim the spots and rects at the crosshair" );
	}
	*fire = fireBuf;
	if ( subject && subject->type == RTX_LTYPE_RECT )
	{
		Com_sprintf( altBuf, sizeof( altBuf ), "the wheel edits the %s of the rect; alt switches to the %s",
			s_rectTargetNames[s_rectTarget], s_rectTargetNames[( s_rectTarget + 1 ) % LEO_NUM] );
		*alt = altBuf;
		if ( s_rectTarget == LEO_ROLL )
		{
			Com_sprintf( wheelBuf, wheelSize, "roll +-%.0f deg, walk 1 (now %.1f)", LE_AngleStep( qfalse ), subject->roll );
		}
		else
		{
			Com_sprintf( wheelBuf, wheelSize, "%s x1.1, walk x1.01, grid snap (now %.4g)", s_rectTargetNames[s_rectTarget],
				s_rectTarget == LEO_WIDTH ? subject->width : subject->height );
		}
		return;
	}
	if ( subject && subject->type == RTX_LTYPE_SPHERE )
	{
		*alt = "sphere: alt converts it to a spot";
	}
	else
	{
		Com_sprintf( altBuf, sizeof( altBuf ), "the wheel edits the %s angle; alt switches to the %s",
			s_editInner ? "inner" : "outer", s_editInner ? "outer" : "inner" );
		*alt = altBuf;
	}
	Com_sprintf( wheelBuf, wheelSize, "%s cone angle +-%.0f deg, walk 1 (now %.0f / %.0f)", s_editInner ? "inner" : "outer",
		LE_AngleStep( qfalse ), subject ? subject->coneOuter : 0.0f, subject ? subject->coneInner : 0.0f );
}
