// Light edit mode: tool 5 (properties). The wheel changes the active property of the targets,
// the primary fire copies it from the aimed light, the secondary fire resets it.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>
#include <math.h>

#define LEDIT_TEMP_MIN			1000.0f
#define LEDIT_TEMP_MAX			12000.0f
#define LEDIT_TEMP_SEARCH_STEP	50.0f
#define LEDIT_INTENSITY_LOG_MAX	5.0f		// gauge: 10^5 at the right end
#define LEDIT_RADIUS_GAUGE_MAX	64.0f

static int				s_prop = LEP_INTENSITY;

static const vec4_t		colGaugeBack	= { 0.00f, 0.00f, 0.00f, 0.70f };
static const vec4_t		colGaugeFill	= { 0.30f, 1.00f, 1.00f, 1.0f };
static const vec4_t		colGaugeMark	= { 1.00f, 1.00f, 1.00f, 1.0f };

static const char		*s_propNames[LEP_NUM] = {
	"intensity", "hue", "saturation", "temperature", "radius", "cone outer", "cone inner",
	"width", "height", "roll", "two-sided", "style"
};

void LE_PropsInit( void )
{
	s_prop = LEP_INTENSITY;
}

int LE_PropActive( void )
{
	return s_prop;
}

const char *LE_PropName( int prop )
{
	return s_propNames[Com_Clampi( 0, LEP_NUM - 1, prop )];
}

// Cones belong to spots, the rect fields to rects, the emitter radius to the other types. Every other property applies to all.
qboolean LE_PropApplies( int prop, int type )
{
	// An emissive surface has a factor (intensity), a tint and a style.
	if ( type == RTX_LTYPE_SURFACE )
	{
		return (qboolean)( prop == LEP_INTENSITY || prop == LEP_HUE || prop == LEP_SAT || prop == LEP_TEMP || prop == LEP_STYLE );
	}
	switch ( prop )
	{
	case LEP_RADIUS:
		return (qboolean)( type != RTX_LTYPE_RECT );
	case LEP_CONE_OUTER:
	case LEP_CONE_INNER:
		return (qboolean)( type == RTX_LTYPE_SPOT );
	case LEP_WIDTH:
	case LEP_HEIGHT:
	case LEP_ROLL:
	case LEP_TWOSIDED:
		return (qboolean)( type == RTX_LTYPE_RECT );
	}
	return qtrue;
}

void LE_PropCycle( int dir )
{
	const rtxLightDesc_t	*d = LE_RecDesc( s_sel >= 0 ? s_sel : s_pickId );
	// Tool 2 edits the preset of the type it makes, not the selection.
	const int				type = ( s_tool == LEDIT_TOOL_CREATE ) ? LE_CreateType() : ( d ? d->type : -1 );

	for ( int i = 0; i < LEP_NUM; i++ )
	{
		s_prop = ( s_prop + ( dir > 0 ? 1 : LEP_NUM - 1 ) ) % LEP_NUM;
		if ( type < 0 || LE_PropApplies( s_prop, type ) )
		{
			break;
		}
	}
	LE_Msg( "light edit: property %s", LE_PropName( s_prop ) );
}

/*
=================
Colour maths
=================
*/
void LE_ColorToHSV( const vec3_t rgb, float *h, float *s, float *v )
{
	const float	mx = Q_max( rgb[0], Q_max( rgb[1], rgb[2] ) );
	const float	mn = Q_min( rgb[0], Q_min( rgb[1], rgb[2] ) );
	const float	delta = mx - mn;
	float		hue = 0.0f;

	if ( delta > 0.0f )
	{
		if ( mx == rgb[0] )
		{
			hue = ( rgb[1] - rgb[2] ) / delta;
		}
		else if ( mx == rgb[1] )
		{
			hue = 2.0f + ( rgb[2] - rgb[0] ) / delta;
		}
		else
		{
			hue = 4.0f + ( rgb[0] - rgb[1] ) / delta;
		}
		hue *= 60.0f;
		if ( hue < 0.0f )
		{
			hue += 360.0f;
		}
	}
	*h = hue;
	*s = ( mx > 0.0f ) ? delta / mx : 0.0f;
	*v = mx;
}

static void LE_HSVToColor( float h, float s, float v, vec3_t rgb )
{
	const float	hh = fmodf( h, 360.0f ) / 60.0f;
	const int	i = (int)floorf( hh );
	const float	f = hh - (float)i;
	const float	p = v * ( 1.0f - s );
	const float	q = v * ( 1.0f - s * f );
	const float	t = v * ( 1.0f - s * ( 1.0f - f ) );

	switch ( i % 6 )
	{
	case 0:		VectorSet( rgb, v, t, p );	break;
	case 1:		VectorSet( rgb, q, v, p );	break;
	case 2:		VectorSet( rgb, p, v, t );	break;
	case 3:		VectorSet( rgb, p, q, v );	break;
	case 4:		VectorSet( rgb, t, p, v );	break;
	default:	VectorSet( rgb, v, p, q );	break;
	}
	for ( int k = 0; k < 3; k++ )
	{
		rgb[k] = Com_Clamp( 0.0f, 1.0f, rgb[k] );
	}
}

// Blackbody colour for a temperature in K (approximation for 1000..12000 K), scaled so that the largest channel is 1.
static void LE_Blackbody( float kelvin, vec3_t out )
{
	const float	t = Com_Clamp( LEDIT_TEMP_MIN, LEDIT_TEMP_MAX, kelvin ) / 100.0f;
	float		r, g, b, mx;

	r = ( t <= 66.0f ) ? 255.0f : 329.698727446f * powf( t - 60.0f, -0.1332047592f );
	g = ( t <= 66.0f ) ? 99.4708025861f * logf( t ) - 161.1195681661f : 288.1221695283f * powf( t - 60.0f, -0.0755148492f );
	if ( t >= 66.0f )
	{
		b = 255.0f;
	}
	else
	{
		b = ( t <= 19.0f ) ? 0.0f : 138.5177312231f * logf( t - 10.0f ) - 305.0447927307f;
	}
	r = Com_Clamp( 0.0f, 255.0f, r );
	g = Com_Clamp( 0.0f, 255.0f, g );
	b = Com_Clamp( 0.0f, 255.0f, b );
	mx = Q_max( r, Q_max( g, b ) );
	VectorSet( out, r / mx, g / mx, b / mx );
}

// Temperature whose blackbody colour is the nearest to the colour (compared with the largest channel at 1).
float LE_ColorTemp( const vec3_t rgb )
{
	const float	mx = Q_max( rgb[0], Q_max( rgb[1], rgb[2] ) );
	vec3_t		n;
	float		best = 6500.0f, bestD = 1e30f;

	if ( mx <= 0.0f )
	{
		return best;
	}
	VectorScale( rgb, 1.0f / mx, n );
	for ( float k = LEDIT_TEMP_MIN; k <= LEDIT_TEMP_MAX; k += LEDIT_TEMP_SEARCH_STEP )
	{
		vec3_t	c;
		float	d;

		LE_Blackbody( k, c );
		d = ( c[0] - n[0] ) * ( c[0] - n[0] ) + ( c[1] - n[1] ) * ( c[1] - n[1] ) + ( c[2] - n[2] ) * ( c[2] - n[2] );
		if ( d < bestD )
		{
			bestD = d;
			best = k;
		}
	}
	return best;
}

/*
=================
Property operations
=================
*/
// Wheel step on one light. Returns qfalse when the property does not apply to it.
qboolean LE_PropAdjust( rtxLightDesc_t *d, int prop, int wheel, qboolean fine )
{
	const float	n = (float)wheel;
	float		h, s, v;

	switch ( prop )
	{
	case LEP_INTENSITY:
		d->intensity = Q_max( 0.0f, d->intensity * powf( fine ? 1.01f : 1.1f, n ) );
		return qtrue;
	case LEP_HUE:
	case LEP_SAT:
		LE_ColorToHSV( d->color, &h, &s, &v );
		if ( v <= 0.0f )
		{
			return qfalse;
		}
		if ( prop == LEP_HUE )
		{
			h = fmodf( h + n * ( fine ? 1.0f : 10.0f ) + 3600.0f * 8.0f, 360.0f );
		}
		else
		{
			s = Com_Clamp( 0.0f, 1.0f, s + n * ( fine ? 0.01f : 0.05f ) );
		}
		LE_HSVToColor( h, s, v, d->color );
		return qtrue;
	case LEP_TEMP:
		LE_Blackbody( LE_ColorTemp( d->color ) + n * ( fine ? 50.0f : 250.0f ), d->color );
		return qtrue;
	case LEP_RADIUS:
		if ( !LE_PropApplies( prop, d->type ) )
		{
			return qfalse;
		}
		d->radius = Q_max( 0.5f, d->radius + n * ( fine ? 0.1f : 1.0f ) );
		return qtrue;
	case LEP_CONE_OUTER:
	case LEP_CONE_INNER:
		if ( d->type != RTX_LTYPE_SPOT )
		{
			return qfalse;
		}
		LE_ConeAdd( d, (qboolean)( prop == LEP_CONE_INNER ), n * LE_AngleStep( fine ) );
		return qtrue;
	case LEP_WIDTH:
	case LEP_HEIGHT:
		if ( d->type != RTX_LTYPE_RECT )
		{
			return qfalse;
		}
		if ( prop == LEP_WIDTH )
		{
			d->width = LE_RectSizeStep( d->width, wheel, fine );
		}
		else
		{
			d->height = LE_RectSizeStep( d->height, wheel, fine );
		}
		return qtrue;
	case LEP_ROLL:
		if ( d->type != RTX_LTYPE_RECT )
		{
			return qfalse;
		}
		d->roll = LE_RollStep( d->roll, wheel, fine );
		return qtrue;
	case LEP_TWOSIDED:
		if ( d->type != RTX_LTYPE_RECT )
		{
			return qfalse;
		}
		d->twoSided = !d->twoSided;
		return qtrue;
	case LEP_STYLE:
		d->style = ( ( d->style + wheel ) % RTX_LSTYLE_MAX + RTX_LSTYLE_MAX ) % RTX_LSTYLE_MAX;
		return qtrue;
	}
	return qfalse;
}

// Gives dst the value of the property of src. The intensity keeps the emitted power.
static qboolean LE_PropCopy( rtxLightDesc_t *dst, const rtxLightDesc_t *src, int prop )
{
	float	h, s, v, sh, ss, sv;

	// A surface and a light share the tint and the style. The factor goes between two surfaces only.
	if ( ( dst->type == RTX_LTYPE_SURFACE || src->type == RTX_LTYPE_SURFACE )
		&& ( !LE_PropApplies( prop, dst->type ) || !LE_PropApplies( prop, src->type ) ) )
	{
		return qfalse;
	}
	if ( prop == LEP_INTENSITY && ( dst->type == RTX_LTYPE_SURFACE ) != ( src->type == RTX_LTYPE_SURFACE ) )
	{
		return qfalse;
	}
	switch ( prop )
	{
	case LEP_INTENSITY:
	{
		if ( dst->type == RTX_LTYPE_SURFACE )
		{
			dst->intensity = src->intensity;
			return qtrue;
		}
		const float	scaleSrc = s_api->IntensityScale( src->id );
		const float	scaleDst = s_api->IntensityScale( dst->id );

		dst->intensity = ( scaleSrc > 0.0f && scaleDst > 0.0f ) ? src->intensity * scaleSrc / scaleDst : src->intensity;
		return qtrue;
	}
	case LEP_HUE:
	case LEP_SAT:
		LE_ColorToHSV( dst->color, &h, &s, &v );
		LE_ColorToHSV( src->color, &sh, &ss, &sv );
		if ( v <= 0.0f )
		{
			return qfalse;
		}
		LE_HSVToColor( prop == LEP_HUE ? sh : h, prop == LEP_SAT ? ss : s, v, dst->color );
		return qtrue;
	case LEP_TEMP:
		LE_Blackbody( LE_ColorTemp( src->color ), dst->color );
		return qtrue;
	case LEP_RADIUS:
		if ( dst->type == RTX_LTYPE_RECT )
		{
			return qfalse;
		}
		dst->radius = src->radius;
		return qtrue;
	case LEP_WIDTH:
	case LEP_HEIGHT:
	case LEP_ROLL:
	case LEP_TWOSIDED:
		if ( dst->type != RTX_LTYPE_RECT || src->type != RTX_LTYPE_RECT )
		{
			return qfalse;
		}
		if ( prop == LEP_WIDTH )
		{
			dst->width = src->width;
		}
		else if ( prop == LEP_HEIGHT )
		{
			dst->height = src->height;
		}
		else if ( prop == LEP_ROLL )
		{
			dst->roll = src->roll;
		}
		else
		{
			dst->twoSided = src->twoSided;
		}
		return qtrue;
	case LEP_STYLE:
		dst->style = src->style;
		return qtrue;
	case LEP_CONE_OUTER:
	case LEP_CONE_INNER:
		if ( dst->type != RTX_LTYPE_SPOT || src->type != RTX_LTYPE_SPOT )
		{
			return qfalse;
		}
		if ( prop == LEP_CONE_OUTER )
		{
			dst->coneOuter = src->coneOuter;
			dst->coneInner = Q_min( dst->coneInner, dst->coneOuter );
		}
		else
		{
			dst->coneInner = Com_Clamp( 0.0f, dst->coneOuter, src->coneInner );
		}
		return qtrue;
	}
	return qfalse;
}

/*
=================
Typed values (numeric entry)
=================
*/
// Text of the current value of a property: what the entry starts with.
void LE_PropValueText( const rtxLightDesc_t *d, int prop, char *out, int size )
{
	float	h, s, v, val = 0.0f;
	char	*p;

	switch ( prop )
	{
	case LEP_INTENSITY:		val = d->intensity;		break;
	case LEP_HUE:
	case LEP_SAT:
		LE_ColorToHSV( d->color, &h, &s, &v );
		val = ( prop == LEP_HUE ) ? h : s;
		break;
	case LEP_TEMP:			val = LE_ColorTemp( d->color );	break;
	case LEP_RADIUS:		val = d->radius;		break;
	case LEP_CONE_OUTER:	val = d->coneOuter;		break;
	case LEP_CONE_INNER:	val = d->coneInner;		break;
	case LEP_WIDTH:			val = d->width;			break;
	case LEP_HEIGHT:		val = d->height;		break;
	case LEP_ROLL:			val = d->roll;			break;
	case LEP_TWOSIDED:		val = (float)d->twoSided;	break;
	case LEP_STYLE:			val = (float)d->style;	break;
	}
	Com_sprintf( out, size, "%.2f", val );
	p = strchr( out, '.' );
	if ( p )
	{
		for ( char *e = out + strlen( out ) - 1; e > p && *e == '0'; e-- )
		{
			*e = 0;
		}
		if ( p[1] == 0 )
		{
			*p = 0;
		}
	}
}

// Gives a light the typed values of a property. Hue takes "hue [saturation [value]]",
// cone outer takes "outer [inner]". Returns qfalse when the property does not apply.
qboolean LE_PropValueApply( rtxLightDesc_t *d, int prop, const float *val, int count )
{
	float	h, s, v;

	if ( count < 1 || ( d->type == RTX_LTYPE_SURFACE && !LE_PropApplies( prop, d->type ) ) )
	{
		return qfalse;
	}
	switch ( prop )
	{
	case LEP_INTENSITY:
		d->intensity = Q_max( 0.0f, val[0] );
		return qtrue;
	case LEP_HUE:
	case LEP_SAT:
		LE_ColorToHSV( d->color, &h, &s, &v );
		if ( prop == LEP_HUE )
		{
			h = fmodf( fmodf( val[0], 360.0f ) + 360.0f, 360.0f );
			if ( count >= 2 )
			{
				s = Com_Clamp( 0.0f, 1.0f, val[1] );
			}
			if ( count >= 3 )
			{
				v = Com_Clamp( 0.0f, 1.0f, val[2] );
			}
		}
		else
		{
			s = Com_Clamp( 0.0f, 1.0f, val[0] );
		}
		if ( v <= 0.0f )
		{
			return qfalse;
		}
		LE_HSVToColor( h, s, v, d->color );
		return qtrue;
	case LEP_TEMP:
		LE_Blackbody( val[0], d->color );
		return qtrue;
	case LEP_RADIUS:
		if ( d->type == RTX_LTYPE_RECT )
		{
			return qfalse;
		}
		d->radius = Q_max( 0.5f, val[0] );
		return qtrue;
	case LEP_WIDTH:
	case LEP_HEIGHT:
	case LEP_ROLL:
	case LEP_TWOSIDED:
		if ( d->type != RTX_LTYPE_RECT )
		{
			return qfalse;
		}
		if ( prop == LEP_WIDTH )
		{
			d->width = Com_Clamp( RTX_LRECT_MIN_SIZE, RTX_LRECT_MAX_SIZE, val[0] );
		}
		else if ( prop == LEP_HEIGHT )
		{
			d->height = Com_Clamp( RTX_LRECT_MIN_SIZE, RTX_LRECT_MAX_SIZE, val[0] );
		}
		else if ( prop == LEP_ROLL )
		{
			d->roll = fmodf( fmodf( val[0], 360.0f ) + 360.0f, 360.0f );
		}
		else
		{
			d->twoSided = ( val[0] != 0.0f ) ? 1 : 0;
		}
		return qtrue;
	case LEP_STYLE:
		d->style = Com_Clampi( 0, RTX_LSTYLE_MAX - 1, (int)floorf( val[0] + 0.5f ) );
		return qtrue;
	case LEP_CONE_OUTER:
		if ( d->type != RTX_LTYPE_SPOT )
		{
			return qfalse;
		}
		d->coneOuter = Com_Clamp( 1.0f, 89.0f, val[0] );
		d->coneInner = Com_Clamp( 0.0f, d->coneOuter, ( count >= 2 ) ? val[1] : d->coneInner );
		return qtrue;
	case LEP_CONE_INNER:
		if ( d->type != RTX_LTYPE_SPOT )
		{
			return qfalse;
		}
		d->coneInner = Com_Clamp( 0.0f, d->coneOuter, val[0] );
		return qtrue;
	}
	return qfalse;
}

/*
=================
Tool
=================
*/
static void LE_PropsWheel( const std::vector<int> &ids, int wheel, qboolean fine )
{
	int	done = 0;

	if ( ids.empty() )
	{
		LE_Msg( "light edit: aim at a light or select some" );
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBeginMerge( LE_PropName( s_prop ), LE_HashTargets( 0x50 + s_prop, ids ) );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d;

			if ( !LE_GetDesc( ids[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) )
			{
				continue;
			}
			if ( LE_PropAdjust( &d, s_prop, wheel, fine ) && LE_DoSet( ids[i], &d, LE_PropName( s_prop ) ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( !done )
	{
		LE_Msg( "light edit: %s does not apply to the targets", LE_PropName( s_prop ) );
	}
}

// Primary fire: the property of the aimed light goes to every other selected light.
static void LE_PropsCopyToSelection( void )
{
	rtxLightDesc_t	src;
	int				done = 0;

	if ( s_pickId < 0 || !LE_GetDesc( s_pickId, &src ) )
	{
		LE_Msg( "light edit: aim at the light to copy from" );
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBegin( va( "copy %s", LE_PropName( s_prop ) ) );
		for ( size_t i = 0; i < s_selList.size(); i++ )
		{
			rtxLightDesc_t	d;

			if ( s_selList[i] == s_pickId || !LE_GetDesc( s_selList[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) )
			{
				continue;
			}
			if ( LE_PropCopy( &d, &src, s_prop ) && LE_DoSet( s_selList[i], &d, "copy property" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: %s of light %d copied to %d light%s", LE_PropName( s_prop ), s_pickId, done, done > 1 ? "s" : "" );
	}
	else
	{
		LE_Msg( "light edit: select the lights to change, then aim at the one to copy from" );
	}
}

// Secondary fire: the property goes back to its original value on the targets.
static void LE_PropsReset( const std::vector<int> &ids )
{
	int	done = 0;

	if ( ids.empty() )
	{
		LE_Msg( "light edit: aim at a light or select some" );
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBegin( va( "reset %s", LE_PropName( s_prop ) ) );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d, orig;

			if ( !LE_GetDesc( ids[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) || !s_api->GetOriginal( ids[i], &orig ) )
			{
				continue;
			}
			if ( s_prop == LEP_INTENSITY )
			{
				d.intensity = orig.intensity;
			}
			else if ( s_prop == LEP_TEMP )
			{
				VectorCopy( orig.color, d.color );
			}
			else if ( !LE_PropCopy( &d, &orig, s_prop ) )
			{
				continue;
			}
			if ( LE_DoSet( ids[i], &d, "reset property" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: %s reset on %d light%s", LE_PropName( s_prop ), done, done > 1 ? "s" : "" );
	}
}

void LE_ToolProps( qboolean priDown, qboolean secDown, int wheel, qboolean fine )
{
	std::vector<int>	ids;

	LE_ActionTargets( ids );
	if ( ids.empty() && LE_EmissiveTool( secDown, wheel, fine ) )
	{
		return;
	}
	if ( wheel )
	{
		LE_PropsWheel( ids, wheel, fine );
	}
	if ( priDown )
	{
		LE_PropsCopyToSelection();
	}
	if ( secDown )
	{
		LE_PropsReset( ids );
	}
}

void LE_PropsHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize )
{
	static const char	*wheelText[LEP_NUM] = {
		"x1.1 per notch, walk x1.01",
		"+-10 deg, walk 1",
		"+-0.05, walk 0.01",
		"+-250 K, walk 50 K",
		"+-1, walk 0.1 (min 0.5)",
		"+-angle snap, walk 1 deg (1..89)",
		"+-angle snap, walk 1 deg (0..outer)",
		"x1.1 per notch, walk x1.01, grid snap",
		"x1.1 per notch, walk x1.01, grid snap",
		"+-angle snap, walk 1 deg",
		"toggles",
		"+-1 (0 steady, 32..63 switch)"
	};

	static char	nameBuf[48], fireBuf[128], altBuf[96];

	if ( LE_EmissiveHelp( name, fire, alt, wheelBuf, wheelSize ) )
	{
		return;
	}
	Com_sprintf( nameBuf, sizeof( nameBuf ), "5 Properties [%s]", LE_PropName( s_prop ) );
	Com_sprintf( fireBuf, sizeof( fireBuf ), "copy the %s of the aimed light to the selection", LE_PropName( s_prop ) );
	Com_sprintf( altBuf, sizeof( altBuf ), "reset the %s to its original value", LE_PropName( s_prop ) );
	*name = nameBuf;
	*fire = fireBuf;
	*alt = altBuf;
	Com_sprintf( wheelBuf, wheelSize, "%s %s; [ ] picks the property", LE_PropName( s_prop ), wheelText[s_prop] );
}

// Gauge of the active property. It uses three dots of the budget.
void LE_PropsDrawGauge( float x, float y, float w, const rtxLightDesc_t *d )
{
	float	h, s, v, frac = 0.0f;

	switch ( s_prop )
	{
	case LEP_INTENSITY:
		frac = log10f( Q_max( d->intensity, 1.0f ) ) / LEDIT_INTENSITY_LOG_MAX;
		break;
	case LEP_HUE:
	case LEP_SAT:
		LE_ColorToHSV( d->color, &h, &s, &v );
		frac = ( s_prop == LEP_HUE ) ? h / 360.0f : s;
		break;
	case LEP_TEMP:
		frac = ( LE_ColorTemp( d->color ) - LEDIT_TEMP_MIN ) / ( LEDIT_TEMP_MAX - LEDIT_TEMP_MIN );
		break;
	case LEP_RADIUS:
		frac = d->radius / LEDIT_RADIUS_GAUGE_MAX;
		break;
	case LEP_CONE_OUTER:
		frac = d->coneOuter / 89.0f;
		break;
	case LEP_CONE_INNER:
		frac = d->coneInner / 89.0f;
		break;
	case LEP_WIDTH:
		frac = d->width / 256.0f;
		break;
	case LEP_HEIGHT:
		frac = d->height / 256.0f;
		break;
	case LEP_ROLL:
		frac = d->roll / 360.0f;
		break;
	case LEP_TWOSIDED:
		frac = d->twoSided ? 1.0f : 0.0f;
		break;
	case LEP_STYLE:
		frac = (float)d->style / (float)( RTX_LSTYLE_MAX - 1 );
		break;
	}
	frac = Com_Clamp( 0.0f, 1.0f, frac );
	LE_Rect( x - 1.0f, y - 1.0f, w + 2.0f, 7.0f, colGaugeBack );
	LE_Rect( x, y, w * frac, 5.0f, colGaugeFill );
	LE_Rect( x + w * frac - 1.0f, y - 2.0f, 2.0f, 9.0f, colGaugeMark );
}
