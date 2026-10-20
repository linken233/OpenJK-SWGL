// Light edit mode: rectangle lights and light styles. Type names, type conversion, the wire
// rectangle, the size step of the wheel, and the style names.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <math.h>

#define LEDIT_RECT_DOT_FLOOR	1000		// the rectangles never use the last 1000 dots of the budget
#define LEDIT_RECT_EXTRA		3			// selected rects, besides the primary one, that get a wire
#define LEDIT_RECT_ARROW_MIN	16.0f
#define LEDIT_RECT_ARROW_MAX	96.0f

static const vec4_t	colRect		= { 0.40f, 1.00f, 0.70f, 1.0f };

/*
=================
Names
=================
*/
const char *LE_TypeName( int type )
{
	switch ( type )
	{
	case RTX_LTYPE_SPOT:	return "spot";
	case RTX_LTYPE_RECT:	return "rect";
	case RTX_LTYPE_SURFACE:	return "surface";
	}
	return "sphere";
}

// "sphere", "spot" or "rect 32x16".
void LE_ShapeText( const rtxLightDesc_t *d, char *out, int size )
{
	if ( d->type == RTX_LTYPE_RECT )
	{
		Com_sprintf( out, size, "rect %.4gx%.4g", d->width, d->height );
		return;
	}
	Com_sprintf( out, size, "%s", LE_TypeName( d->type ) );
}

// Names follow the defaultStyles table of code/game/g_spawn.cpp. 32..63 are the switchable styles.
const char *LE_StyleName( int style )
{
	static const char	*names[] = {
		"steady", "flicker", "slow strong pulse", "candle", "fast strobe", "gentle pulse", "flicker 2",
		"candle 2", "candle 3", "slow strobe", "fluorescent flicker", "slow pulse", "fast pulse", "test blend"
	};

	if ( style >= 0 && style < (int)ARRAY_LEN( names ) )
	{
		return names[style];
	}
	if ( style >= LS_NUM_STYLES && style < RTX_LSTYLE_MAX )
	{
		return va( "switch %d", style - LS_NUM_STYLES );
	}
	return va( "style %d", style );
}

// "3 candle".
void LE_StyleText( int style, char *out, int size )
{
	Com_sprintf( out, size, "%d %s", style, LE_StyleName( style ) );
}

/*
=================
Type conversion
=================
*/
// Gives a light the fields that its new type needs. A direction that is not usable becomes "down".
void LE_ConvertType( rtxLightDesc_t *d, int type )
{
	d->type = type;
	if ( type == RTX_LTYPE_SPHERE )
	{
		return;
	}
	if ( VectorLengthSquared( d->dir ) < 0.25f || VectorNormalize( d->dir ) < 0.5f )
	{
		VectorSet( d->dir, 0.0f, 0.0f, -1.0f );
	}
	if ( type == RTX_LTYPE_SPOT && d->coneOuter < 1.0f )
	{
		d->coneOuter = 35.0f;
		d->coneInner = 25.0f;
	}
	if ( type == RTX_LTYPE_RECT )
	{
		if ( d->width < RTX_LRECT_MIN_SIZE )
		{
			d->width = RTX_LRECT_DEFAULT_SIZE;
		}
		if ( d->height < RTX_LRECT_MIN_SIZE )
		{
			d->height = RTX_LRECT_DEFAULT_SIZE;
		}
	}
}

/*
=================
Wheel steps
=================
*/
// New size after wheel notches: x1.1 (x1.01 walking), rounded to the grid when snap is on.
// The result always moves at least one grid step in the wheel direction.
float LE_RectSizeStep( float size, int wheel, qboolean fine )
{
	float	s = size * powf( fine ? 1.01f : 1.1f, (float)wheel );

	if ( !fine && LE_SnapOn() && LE_GridSize() > 0 )
	{
		const float	g = (float)LE_GridSize();

		s = floorf( s / g + 0.5f ) * g;
		if ( wheel > 0 && s <= size )
		{
			s = size + g;
		}
		else if ( wheel < 0 && s >= size )
		{
			s = size - g;
		}
	}
	return Com_Clamp( RTX_LRECT_MIN_SIZE, RTX_LRECT_MAX_SIZE, s );
}

float LE_RollStep( float roll, int wheel, qboolean fine )
{
	roll = fmodf( roll + (float)wheel * LE_AngleStep( fine ), 360.0f );
	return roll < 0.0f ? roll + 360.0f : roll;
}

/*
=================
Wire rectangle
=================
*/
static void LE_RectSeg( const vec3_t a, const vec3_t b, const vec4_t col )
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
LE_DrawRectWire
Four edges, the normal arrow from the centre. With full: a tick along the U axis (the roll),
and for a two-sided rect the two diagonals and a short arrow on the back.
The wire never uses the last LEDIT_RECT_DOT_FLOOR dots of the budget.
*/
void LE_DrawRectWire( const rtxLightDesc_t *d, const vec4_t col, qboolean full )
{
	vec3_t	n, u, v, c[4], tip, p, q;
	vec4_t	dim;
	float	hw, hh, arrow;

	if ( VectorLengthSquared( d->dir ) < 0.25f )
	{
		return;
	}
	VectorCopy( d->dir, n );
	VectorNormalize( n );
	RTX_LightRectAxes( n, d->roll, u, v );
	hw = Com_Clamp( RTX_LRECT_MIN_SIZE, RTX_LRECT_MAX_SIZE, d->width ) * 0.5f;
	hh = Com_Clamp( RTX_LRECT_MIN_SIZE, RTX_LRECT_MAX_SIZE, d->height ) * 0.5f;
	dim[0] = col[0] * 0.6f;
	dim[1] = col[1] * 0.6f;
	dim[2] = col[2] * 0.6f;
	dim[3] = col[3];

	for ( int k = 0; k < 4; k++ )
	{
		const float	su = ( k == 1 || k == 2 ) ? hw : -hw;
		const float	sv = ( k >= 2 ) ? hh : -hh;

		VectorMA( d->origin, su, u, c[k] );
		VectorMA( c[k], sv, v, c[k] );
	}

	LE_DotFloor( LEDIT_RECT_DOT_FLOOR );
	for ( int k = 0; k < 4; k++ )
	{
		LE_RectSeg( c[k], c[( k + 1 ) & 3], col );
	}

	arrow = Com_Clamp( LEDIT_RECT_ARROW_MIN, LEDIT_RECT_ARROW_MAX, Q_max( hw, hh ) );
	VectorMA( d->origin, arrow, n, tip );
	LE_RectSeg( d->origin, tip, col );
	VectorMA( tip, -arrow * 0.25f, n, p );
	VectorMA( p, arrow * 0.12f, u, q );
	LE_RectSeg( tip, q, col );
	VectorMA( p, -arrow * 0.12f, u, q );
	LE_RectSeg( tip, q, col );

	if ( full )
	{
		VectorMA( d->origin, hw, u, p );
		LE_RectSeg( d->origin, p, dim );
		if ( d->twoSided )
		{
			LE_RectSeg( c[0], c[2], dim );
			LE_RectSeg( c[1], c[3], dim );
			VectorMA( d->origin, -arrow * 0.5f, n, p );
			LE_RectSeg( d->origin, p, dim );
		}
	}
	LE_DotFloor( 0 );
}

static void LE_DrawRectOf( int id, qboolean full )
{
	const rtxLightDesc_t	*d = LE_RecDesc( id );

	if ( d && d->type == RTX_LTYPE_RECT && !( d->flags & RTX_LFLAG_DELETED ) )
	{
		LE_DrawRectWire( d, colRect, full );
	}
}

// The primary light gets the full wire. Other selected rects, up to three, get the edges and the arrow.
void LE_DrawRectWires( void )
{
	int	extra = 0;

	if ( s_sel >= 0 )
	{
		LE_DrawRectOf( s_sel, qtrue );
	}
	for ( size_t i = 0; i < s_selList.size() && extra < LEDIT_RECT_EXTRA; i++ )
	{
		if ( s_selList[i] != s_sel )
		{
			LE_DrawRectOf( s_selList[i], qfalse );
			extra++;
		}
	}
	if ( ( s_tool == LEDIT_TOOL_ORIENT || s_tool == LEDIT_TOOL_PROPS ) && s_pickId >= 0 && !LE_SelHas( s_pickId ) )
	{
		LE_DrawRectOf( s_pickId, qfalse );
	}
}
