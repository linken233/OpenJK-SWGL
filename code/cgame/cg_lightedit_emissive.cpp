// Light edit mode: emissive surfaces and the emissive multiplier per shader.
// An emissive surface is a record like a light (source RTX_LSRC_EMISSIVE): the other files pick, select and
// edit it. This file draws its outline and panel lines. A shader is selected from the menu or by
// ledit_emissive_select; tool 5 then scales it. The commands ledit_emissive* do the same by name.

#include "cg_headers.h"
#include "cg_lightedit.h"
#include "cg_lightedit_local.h"

#include <vector>
#include <algorithm>
#include <math.h>

#define LEDIT_EMI_HILITE_RANGE	4096.0f
#define LEDIT_EMI_HILITE_MAX	300
#define LEDIT_EMI_SCALE_MAX		100.0f
#define LEDIT_EMI_STEP			1.1f
#define LEDIT_EMI_STEP_FINE		1.01f
#define LEDIT_EMI_MIN_UP		0.01f		// a scale of 0 steps up from here
#define LEDIT_EMI_NAME_LEN		96
#define LEDIT_SURF_DOT_FLOOR	1000		// the outlines never use the last 1000 dots of the budget
#define LEDIT_SURF_EXTRA		3			// selected surfaces, besides the primary one, that get an outline
#define LEDIT_SURF_LIST_MAX		200

static int					s_emiSel = -1;			// selected shader, -1 for none
static std::vector<int>		s_recShader;			// shader of each record, -1 when it is not a surface
static int					s_recShaderCount = -1;

static const vec4_t	colHilite	= { 1.00f, 0.90f, 0.20f, 0.90f };
static const vec4_t	colSurfSel	= { 1.00f, 0.90f, 0.20f, 1.0f };
static const vec4_t	colSurfAim	= { 0.80f, 0.80f, 0.80f, 1.0f };
static const vec4_t	colPanelBg	= { 0.00f, 0.00f, 0.00f, 0.45f };
static const vec4_t	colTxt		= { 1.00f, 1.00f, 1.00f, 1.0f };
static const vec4_t	colTxtYel	= { 1.00f, 0.90f, 0.20f, 1.0f };

void LE_EmissiveInit( void )
{
	s_emiSel = -1;
	s_recShader.clear();
	s_recShaderCount = -1;
}

typedef struct {
	float	d;
	int		i;
} ledEmiNear_t;

static bool LE_EmiNearLess( const ledEmiNear_t &a, const ledEmiNear_t &b )
{
	return a.d < b.d;
}

static qboolean LE_EmiReady( void )
{
	return (qboolean)( s_api && s_api->CountEmissiveShaders && s_api->SetEmissiveScale );
}

// The tools that act on a shader show its surfaces and its panel.
static qboolean LE_EmiToolShows( void )
{
	return (qboolean)( s_tool == LEDIT_TOOL_SELECT || s_tool == LEDIT_TOOL_PROPS || s_tool == LEDIT_TOOL_PIPETTE );
}

/*
=================
Emissive surfaces
=================
*/
qboolean LE_IsSurface( const rtxLightDesc_t *d )
{
	return (qboolean)( d && d->source == RTX_LSRC_EMISSIVE );
}

// Description of the surface of a record. Returns qfalse when the record is not one.
qboolean LE_SurfaceInfo( int id, rtxEmissiveSurface_t *out )
{
	memset( out, 0, sizeof( *out ) );
	return (qboolean)( LE_IsSurface( LE_RecDesc( id ) ) && s_api->GetEmissiveSurface( id, out ) );
}

// Fills the shader of each record. It runs again when the number of records changes.
static void LE_SurfaceShaderCache( void )
{
	const int	n = LE_RecCount();

	if ( n == s_recShaderCount && (int)s_recShader.size() == n )
	{
		return;
	}
	s_recShader.assign( n, -1 );
	for ( int i = 0; i < n; i++ )
	{
		rtxEmissiveSurface_t	es;

		if ( LE_SurfaceInfo( i, &es ) )
		{
			s_recShader[i] = es.shader;
		}
	}
	s_recShaderCount = n;
}

// Draws the outline of a surface: the boundary edges of its polygons.
static void LE_SurfaceOutline( int id, const vec4_t col )
{
	rtxEmissiveSurface_t	es;

	if ( !LE_SurfaceInfo( id, &es ) )
	{
		return;
	}
	LE_DotFloor( LEDIT_SURF_DOT_FLOOR );
	for ( int e = 0; e < es.numEdges && e < RTX_LEMI_MAX_EDGES; e++ )
	{
		vec3_t	a, b;
		float	x1, y1, x2, y2;

		VectorCopy( es.edges[e], a );
		VectorCopy( es.edges[e] + 3, b );
		if ( CG_WorldCoordToScreenCoordFloat( a, &x1, &y1 ) && CG_WorldCoordToScreenCoordFloat( b, &x2, &y2 ) )
		{
			LE_Line( x1, y1, x2, y2, col );
		}
	}
	LE_DotFloor( 0 );
}

// The primary surface and three more selected ones get their outline. The aimed one gets it too, with the tools that act on it.
void LE_SurfaceDrawWires( void )
{
	int	extra = 0;

	if ( LE_IsSurface( LE_RecDesc( s_sel ) ) )
	{
		LE_SurfaceOutline( s_sel, colSurfSel );
	}
	for ( size_t i = 0; i < s_selList.size() && extra < LEDIT_SURF_EXTRA; i++ )
	{
		if ( s_selList[i] != s_sel && LE_IsSurface( LE_RecDesc( s_selList[i] ) ) )
		{
			LE_SurfaceOutline( s_selList[i], colSurfSel );
			extra++;
		}
	}
	if ( s_tool != LEDIT_TOOL_CREATE && s_tool != LEDIT_TOOL_MOVE && s_tool != LEDIT_TOOL_ORIENT && s_tool != LEDIT_TOOL_CLONE
		&& s_pickId >= 0 && !LE_SelHas( s_pickId ) && LE_IsSurface( LE_RecDesc( s_pickId ) ) )
	{
		LE_SurfaceOutline( s_pickId, colSurfAim );
	}
}

// Panel lines of a surface: 0 shader, 1 polygons, 2 factors.
void LE_SurfaceText( const rtxLightDesc_t *d, int field, char *out, int size )
{
	rtxEmissiveSurface_t	es;

	if ( !LE_SurfaceInfo( d->id, &es ) )
	{
		out[0] = 0;
		return;
	}
	if ( field == 0 )
	{
		const int	len = (int)strlen( es.shaderName );

		if ( len > 36 )
		{
			Com_sprintf( out, size, "shader: ~%s", es.shaderName + len - 35 );
		}
		else
		{
			Com_sprintf( out, size, "shader: %s", es.shaderName );
		}
	}
	else if ( field == 1 )
	{
		Com_sprintf( out, size, "triangles: %d (face rank %d)", es.numTris, es.rank );
	}
	else
	{
		Com_sprintf( out, size, "factor: surface %.3g x shader %.3g (%d)", d->intensity, es.shaderScale, es.shader );
	}
}

static qboolean LE_EmiInfo( int shader, char *name, int nameSize, float *scale, int *polys )
{
	char	tmp[LEDIT_EMI_NAME_LEN];
	float	sc = 1.0f;
	int		np = 0;

	if ( !name )
	{
		name = tmp;
		nameSize = sizeof( tmp );
	}
	name[0] = 0;
	if ( !LE_EmiReady() || !s_api->GetEmissiveShader( shader, name, nameSize, &sc, &np ) )
	{
		return qfalse;
	}
	if ( scale )
	{
		*scale = sc;
	}
	if ( polys )
	{
		*polys = np;
	}
	return qtrue;
}

// Sets the scale and records one undo entry. The caller opens the undo group.
static qboolean LE_EmiSetScale( int shader, float scale, const char *label )
{
	float	before;

	scale = Com_Clamp( 0.0f, LEDIT_EMI_SCALE_MAX, scale );
	if ( !LE_EmiInfo( shader, NULL, 0, &before, NULL ) )
	{
		return qfalse;
	}
	if ( !s_api->SetEmissiveScale( shader, scale ) )
	{
		LE_Msg( "light edit: emissive scale refused (%s)", s_api->LastError() );
		return qfalse;
	}
	LE_UndoPushScale( shader, before, scale, label );
	return qtrue;
}

// Selects the shader and prints it.
void LE_EmissiveSelect( int shader )
{
	char	name[LEDIT_EMI_NAME_LEN];
	float	scale;
	int		polys;

	s_emiSel = shader;
	if ( shader >= 0 && LE_EmiInfo( shader, name, sizeof( name ), &scale, &polys ) )
	{
		LE_Msg( "light edit: emissive shader %d %s, %d polygons, scale %g", shader, name, polys, scale );
	}
}

int LE_EmissiveSelected( void )
{
	return s_emiSel;
}

void LE_EmissiveClear( void )
{
	s_emiSel = -1;
}

static float LE_EmiStep( float scale, int wheel, qboolean fine )
{
	const float	step = powf( fine ? LEDIT_EMI_STEP_FINE : LEDIT_EMI_STEP, (float)wheel );
	float		out = ( scale < LEDIT_EMI_MIN_UP && wheel > 0 ? LEDIT_EMI_MIN_UP : scale ) * step;

	// The scale stops at 1 when a step crosses it.
	if ( ( scale - 1.0f ) * ( out - 1.0f ) < 0.0f )
	{
		out = 1.0f;
	}
	return Com_Clamp( 0.0f, LEDIT_EMI_SCALE_MAX, out );
}

// Wheel and secondary fire act on the selected shader. Returns qtrue when the selection took the action.
qboolean LE_EmissiveTool( qboolean secDown, int wheel, qboolean fine )
{
	float	scale;

	if ( s_emiSel < 0 || !LE_EmiInfo( s_emiSel, NULL, 0, &scale, NULL ) )
	{
		return qfalse;
	}
	if ( wheel )
	{
		const float	want = LE_EmiStep( scale, wheel, fine );

		if ( want != scale )
		{
			std::vector<int>	key( 1, s_emiSel );

			LE_UndoBeginMerge( "emissive scale", LE_HashTargets( 0x60, key ) );
			if ( LE_EmiSetScale( s_emiSel, want, "emissive scale" ) )
			{
				scale = want;
			}
			LE_UndoEnd();
		}
		return qtrue;
	}
	if ( secDown )
	{
		if ( scale != 1.0f )
		{
			LE_UndoBegin( "emissive reset" );
			LE_EmiSetScale( s_emiSel, 1.0f, "emissive reset" );
			LE_UndoEnd();
		}
		LE_Msg( "light edit: emissive shader %d scale reset to 1", s_emiSel );
		return qtrue;
	}
	return qfalse;
}

// Help texts of tool 5 while the emissive selection is in use.
qboolean LE_EmissiveHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize )
{
	if ( s_emiSel < 0 || !s_selList.empty() || s_pickId >= 0 )
	{
		return qfalse;
	}
	*name = "5 Properties [emissive]";
	*fire = "none (select a surface with tool 1, or a shader with the menu or ledit_emissive_select)";
	*alt = "reset the scale of the selected shader to 1";
	Com_sprintf( wheelBuf, wheelSize, "scale of the selected shader x1.1 per notch, walk x1.01 (0..%g)", LEDIT_EMI_SCALE_MAX );
	return qtrue;
}

// Yellow boxes on the surfaces of the selected shader. The nearest ones only.
void LE_EmissiveDrawHighlight( void )
{
	if ( s_emiSel < 0 || !LE_EmiToolShows() || !LE_EmiReady() )
	{
		return;
	}
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];
	const int	n = LE_RecCount();
	std::vector<ledEmiNear_t>	cand;

	LE_SurfaceShaderCache();
	for ( int i = 0; i < n; i++ )
	{
		vec3_t		rel;
		ledEmiNear_t	e;
		const rtxLightDesc_t	*d = LE_RecDesc( i );

		if ( i >= (int)s_recShader.size() || s_recShader[i] != s_emiSel || !d )
		{
			continue;
		}
		VectorSubtract( d->origin, eye, rel );
		e.d = DotProduct( rel, rel );
		if ( e.d <= LEDIT_EMI_HILITE_RANGE * LEDIT_EMI_HILITE_RANGE && DotProduct( rel, fwd ) > 0.0f )
		{
			e.i = i;
			cand.push_back( e );
		}
	}
	if ( (int)cand.size() > LEDIT_EMI_HILITE_MAX )
	{
		std::nth_element( cand.begin(), cand.begin() + LEDIT_EMI_HILITE_MAX, cand.end(), LE_EmiNearLess );
		cand.resize( LEDIT_EMI_HILITE_MAX );
	}
	for ( size_t k = 0; k < cand.size() && LE_DotsLeft() > 0; k++ )
	{
		vec3_t	p;
		float	sx, sy;

		VectorCopy( LE_RecDesc( cand[k].i )->origin, p );
		if ( CG_WorldCoordToScreenCoordFloat( p, &sx, &sy ) && sx > -4 && sx < 644 && sy > -4 && sy < 484 )
		{
			LE_Box( sx, sy, 5.0f, colHilite );
		}
	}
}

// Panel on the right when a shader is selected and no light is. y is the top of the panel.
qboolean LE_EmissiveDrawPanel( int y )
{
	char	name[LEDIT_EMI_NAME_LEN], shown[44], lines[3][64];
	float	scale;
	int		polys;

	if ( s_emiSel < 0 || !LE_EmiToolShows() || !LE_EmiInfo( s_emiSel, name, sizeof( name ), &scale, &polys ) )
	{
		return qfalse;
	}
	const int	len = (int)strlen( name );

	if ( len > 40 )
	{
		Com_sprintf( shown, sizeof( shown ), "~%s", name + len - 39 );
	}
	else
	{
		Q_strncpyz( shown, name, sizeof( shown ) );
	}
	Com_sprintf( lines[0], sizeof( lines[0] ), "emissive shader %d", s_emiSel );
	Com_sprintf( lines[1], sizeof( lines[1] ), "polygons: %d", polys );
	Com_sprintf( lines[2], sizeof( lines[2] ), "scale: %g%s", scale, scale != 1.0f ? " (was 1)" : "" );

	const int	h = LE_TextH();
	int			maxw = LE_TextW( shown );

	for ( int i = 0; i < 3; i++ )
	{
		maxw = Q_max( maxw, LE_TextW( lines[i] ) );
	}
	CG_FillRect( 640 - 8 - maxw - 4, y - 3, maxw + 10, 4 * h + 6, colPanelBg );
	LE_Text( 640 - 8 - LE_TextW( lines[0] ), y, lines[0], colTxt );
	LE_Text( 640 - 8 - LE_TextW( shown ), y + h, shown, colTxt );
	LE_Text( 640 - 8 - LE_TextW( lines[1] ), y + 2 * h, lines[1], colTxt );
	LE_Text( 640 - 8 - LE_TextW( lines[2] ), y + 3 * h, lines[2], scale != 1.0f ? colTxtYel : colTxt );
	return qtrue;
}

/*
=================
Commands
=================
*/
static qboolean LE_EmiCmdReady( void )
{
	if ( !CG_LightEdit_Active() || !s_api )
	{
		LE_Msg( "light edit: not active" );
		return qfalse;
	}
	if ( !LE_EmiReady() )
	{
		LE_Msg( "light edit: the renderer has no emissive shader list" );
		return qfalse;
	}
	return qtrue;
}

// True when text holds part, ignoring case.
static qboolean LE_EmiContains( const char *text, const char *part )
{
	const size_t	n = strlen( part );

	for ( ; *text; text++ )
	{
		if ( !Q_stricmpn( text, part, (int)n ) )
		{
			return qtrue;
		}
	}
	return qfalse;
}

// Index from a number, an exact name or a unique part of a name. Prints the reason when it fails.
static int LE_EmiFind( const char *arg )
{
	const int	count = s_api->CountEmissiveShaders();
	char		name[LEDIT_EMI_NAME_LEN];
	int			found = -1, matches = 0;

	if ( !arg[0] )
	{
		LE_Msg( "light edit: give a shader index or name (ledit_emissive_list)" );
		return -1;
	}
	if ( arg[strspn( arg, "0123456789" )] == 0 )
	{
		const int	idx = atoi( arg );

		if ( idx < 0 || idx >= count )
		{
			LE_Msg( "light edit: no emissive shader %d (0..%d)", idx, count - 1 );
			return -1;
		}
		return idx;
	}
	for ( int i = 0; i < count; i++ )
	{
		if ( LE_EmiInfo( i, name, sizeof( name ), NULL, NULL ) && !Q_stricmp( name, arg ) )
		{
			return i;
		}
	}
	for ( int i = 0; i < count; i++ )
	{
		if ( LE_EmiInfo( i, name, sizeof( name ), NULL, NULL ) && LE_EmiContains( name, arg ) )
		{
			found = i;
			matches++;
		}
	}
	if ( matches == 1 )
	{
		return found;
	}
	if ( matches )
	{
		LE_Msg( "light edit: %d shaders match \"%s\" (ledit_emissive_list %s)", matches, arg, arg );
	}
	else
	{
		LE_Msg( "light edit: no emissive shader matches \"%s\"", arg );
	}
	return -1;
}

// ledit_emissive_list [filter]: index, name, polygon count and scale; a scale other than 1 has a "*".
void LE_CmdEmissiveList( void )
{
	char	filter[64];
	int		shown = 0, changed = 0;

	if ( !LE_EmiCmdReady() )
	{
		return;
	}
	Q_strncpyz( filter, CG_Argv( 1 ), sizeof( filter ) );
	const int			count = s_api->CountEmissiveShaders();
	std::vector<int>	surfaces( count, 0 );

	LE_SurfaceShaderCache();
	for ( size_t i = 0; i < s_recShader.size(); i++ )
	{
		if ( s_recShader[i] >= 0 && s_recShader[i] < count )
		{
			surfaces[s_recShader[i]]++;
		}
	}
	for ( int i = 0; i < count; i++ )
	{
		char	name[LEDIT_EMI_NAME_LEN];
		float	scale;
		int		polys;

		if ( !LE_EmiInfo( i, name, sizeof( name ), &scale, &polys ) )
		{
			continue;
		}
		if ( scale != 1.0f )
		{
			changed++;
		}
		if ( filter[0] && !LE_EmiContains( name, filter ) )
		{
			continue;
		}
		CG_Printf( "%4d %-48s %5d surfaces %6d polys  scale %g%s\n", i, name, surfaces[i], polys, scale, scale != 1.0f ? " *" : "" );
		shown++;
	}
	LE_Msg( "light edit: %d of %d emissive shaders listed, %d with a scale other than 1", shown, count, changed );
}

// ledit_emissive <index|name> <scale>
void LE_CmdEmissive( void )
{
	if ( !LE_EmiCmdReady() )
	{
		return;
	}
	if ( !CG_Argv( 2 )[0] )
	{
		LE_Msg( "usage: ledit_emissive <index | name> <scale 0..%g>", LEDIT_EMI_SCALE_MAX );
		return;
	}
	const int	shader = LE_EmiFind( CG_Argv( 1 ) );
	const float	scale = Com_Clamp( 0.0f, LEDIT_EMI_SCALE_MAX, (float)atof( CG_Argv( 2 ) ) );
	char		name[LEDIT_EMI_NAME_LEN];

	if ( shader < 0 )
	{
		return;
	}
	LE_UndoBegin( "emissive scale" );
	const qboolean	ok = LE_EmiSetScale( shader, scale, "emissive scale" );

	LE_UndoEnd();
	if ( ok && LE_EmiInfo( shader, name, sizeof( name ), NULL, NULL ) )
	{
		LE_Msg( "light edit: emissive shader %d %s scale %g", shader, name, scale );
	}
}

// ledit_emissive_reset <index|name|all>
void LE_CmdEmissiveReset( void )
{
	int	done = 0;

	if ( !LE_EmiCmdReady() )
	{
		return;
	}
	if ( !Q_stricmp( CG_Argv( 1 ), "all" ) )
	{
		const int	count = s_api->CountEmissiveShaders();

		LE_UndoBegin( "emissive reset" );
		for ( int i = 0; i < count; i++ )
		{
			float	scale;

			if ( LE_EmiInfo( i, NULL, 0, &scale, NULL ) && scale != 1.0f && LE_EmiSetScale( i, 1.0f, "emissive reset" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
		LE_Msg( "light edit: %d emissive shader%s reset to 1", done, done == 1 ? "" : "s" );
		return;
	}
	const int	shader = LE_EmiFind( CG_Argv( 1 ) );

	if ( shader < 0 )
	{
		return;
	}
	LE_UndoBegin( "emissive reset" );
	const qboolean	ok = LE_EmiSetScale( shader, 1.0f, "emissive reset" );

	LE_UndoEnd();
	if ( ok )
	{
		LE_Msg( "light edit: emissive shader %d scale reset to 1", shader );
	}
}

// ledit_emissive_select <index|name|none|sel>: selects a shader (sel: the shader of the selected surface) and empties
// the light selection.
void LE_CmdEmissiveSelect( void )
{
	if ( !LE_EmiCmdReady() )
	{
		return;
	}
	if ( !Q_stricmp( CG_Argv( 1 ), "none" ) )
	{
		s_emiSel = -1;
		return;
	}
	int	shader;

	if ( !Q_stricmp( CG_Argv( 1 ), "sel" ) )
	{
		rtxEmissiveSurface_t	es;

		if ( !LE_SurfaceInfo( s_sel, &es ) )
		{
			LE_Msg( "light edit: no emissive surface selected" );
			return;
		}
		shader = es.shader;
	}
	else
	{
		shader = LE_EmiFind( CG_Argv( 1 ) );
	}
	if ( shader >= 0 )
	{
		LE_SelClear();
		LE_EmissiveSelect( shader );
	}
}

// ledit_emissive_surfaces [index|name]: the emissive surfaces of the shaders that match, or of all shaders.
void LE_CmdEmissiveSurfaces( void )
{
	char	filter[64];
	int		shown = 0, total = 0, matching = -1;

	if ( !LE_EmiCmdReady() )
	{
		return;
	}
	Q_strncpyz( filter, CG_Argv( 1 ), sizeof( filter ) );
	if ( filter[0] && filter[strspn( filter, "0123456789" )] == 0 )
	{
		matching = atoi( filter );
		filter[0] = 0;
	}
	for ( int i = 0; i < LE_RecCount(); i++ )
	{
		rtxEmissiveSurface_t	es;
		const rtxLightDesc_t	*d = LE_RecDesc( i );

		if ( !LE_SurfaceInfo( i, &es ) )
		{
			continue;
		}
		if ( ( matching >= 0 && es.shader != matching ) || ( filter[0] && !LE_EmiContains( es.shaderName, filter ) ) )
		{
			continue;
		}
		total++;
		if ( shown < LEDIT_SURF_LIST_MAX )
		{
			CG_Printf( "%5d shader %3d rank %3d %4d tris  factor %.3g  %s %s\n", i, es.shader, es.rank, es.numTris, d->intensity,
				LE_FlagsStr( d->flags ), es.shaderName );
			shown++;
		}
	}
	LE_Msg( "light edit: %d emissive surface%s listed%s", total, total == 1 ? "" : "s",
		total > shown ? va( " (the first %d are printed)", shown ) : "" );
}
