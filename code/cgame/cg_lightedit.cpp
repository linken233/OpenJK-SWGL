// Light edit mode of the RTX path tracer: cgame side.
// Tools, picking, drag, undo/redo, console commands and the 2D overlay.
// The renderer API is in rd-common/rtx_light_edit_api.h.

#include "cg_headers.h"
#include "cg_media.h"
#include "../game/g_lightedit.h"
#include "cg_lightedit.h"
#include "cg_lightedit_local.h"

#include <vector>
#include <algorithm>
#include <stdarg.h>
#include <stdio.h>

#define LEDIT_PICK_MIN_RADIUS	8.0f
#define LEDIT_PICK_SCALE		0.015f
#define LEDIT_OCCLUDE_RANGE		3000.0f
#define LEDIT_MAX_TRACES		1024
#define LEDIT_SURFACE_RANGE		2048.0f
#define LEDIT_SURFACE_ICONS		500
#define LEDIT_FONT_SCALE		0.7f
#define LEDIT_MSG_TIME			4000

typedef struct {
	rtxLightDesc_t	d;
	qboolean		valid;		// Get succeeded and the light is not deleted
	qboolean		onScreen;
	qboolean		occluded;
	float			sx, sy;		// virtual 640x480 screen position
	float			depth;		// distance along the view axis
} ledRec_t;

typedef struct {
	int		id;
	float	t;
	int		surface;	// an emissive surface: it comes after the lights
} ledHit_t;

rtxLightEditAPI_t			*s_api = NULL;
int							s_tool = LEDIT_TOOL_SELECT;
int							s_sel = -1;
std::vector<int>			s_selList;
int							s_pickId = -1;
float						s_pickT = 0.0f;
static qboolean				s_active = qfalse;		// cgame side of the mode
static qboolean				s_leaving = qfalse;		// leave requested, game not yet inactive
static int					s_cycle = 0;
static int					s_cycleBase = -1;
static int					s_wheel = 0;			// wheel steps since the last frame
static int					s_prevButtons = 0;
static float				s_createOffset = 16.0f;
static vec3_t				s_ghost;
static vec3_t				s_ghostHitPos;			// surface point under the ghost, valid when s_ghostHit
static vec3_t				s_ghostNormal;			// surface normal under the ghost, valid when s_ghostHit
static qboolean				s_ghostHit = qfalse;
static qboolean				s_ghostSolid = qfalse;
static int					s_createType = RTX_LTYPE_SPHERE;	// type that tool 2 makes
static qboolean				s_restoreMode = qfalse;	// the compacted lights show and tool 8 can restore them
static rtxLightStats_t		s_stats;

static std::vector<ledRec_t>		s_recs;
static std::vector<ledHit_t>		s_hits;
static std::vector<unsigned char>	s_occ;			// last occlusion result per id

static char			s_msg[160];
static int			s_msgEnd = 0;

static vmCvar_t		ledit_xray;
static vmCvar_t		ledit_show;
static vmCvar_t		ledit_preset_intensity;
static vmCvar_t		ledit_preset_radius;
static vmCvar_t		ledit_preset_color;
static vmCvar_t		ledit_preset_cone_outer;
static vmCvar_t		ledit_preset_cone_inner;
static vmCvar_t		ledit_preset_width;
static vmCvar_t		ledit_preset_height;
static vmCvar_t		ledit_preset_roll;
static vmCvar_t		ledit_preset_twosided;
static vmCvar_t		ledit_preset_style;
static int			s_presetWheel = 0;		// shift + wheel steps since the last frame
static qboolean		s_keyShift = qfalse;	// shift state of the last key down
static int			s_keyShiftTime = 0;		// cg.time of that key

static const vec4_t	colBlue		= { 0.30f, 0.55f, 1.00f, 1.0f };
static const vec4_t	colYellow	= { 1.00f, 0.90f, 0.20f, 1.0f };
static const vec4_t	colGreen	= { 0.20f, 1.00f, 0.30f, 1.0f };
static const vec4_t	colOrange	= { 1.00f, 0.55f, 0.10f, 1.0f };
static const vec4_t	colRed		= { 1.00f, 0.15f, 0.15f, 1.0f };
static const vec4_t	colRedDim	= { 1.00f, 0.15f, 0.15f, 0.45f };
static const vec4_t	colWhite	= { 1.00f, 1.00f, 1.00f, 1.0f };
static const vec4_t	colGrey		= { 0.60f, 0.60f, 0.60f, 1.0f };
static const vec4_t	colDark		= { 0.00f, 0.00f, 0.00f, 0.85f };
static const vec4_t	colPanel	= { 0.00f, 0.00f, 0.00f, 0.45f };
static const vec4_t	colCyan		= { 0.30f, 1.00f, 1.00f, 1.0f };
static const vec4_t	colEmissive	= { 0.70f, 0.70f, 0.70f, 1.0f };

/*
=================
Messages and small helpers
=================
*/
void LE_Msg( const char *fmt, ... )
{
	va_list	args;

	va_start( args, fmt );
	vsnprintf( s_msg, sizeof( s_msg ), fmt, args );
	va_end( args );
	s_msg[sizeof( s_msg ) - 1] = 0;
	s_msgEnd = cg.time + LEDIT_MSG_TIME;
	CG_Printf( "%s\n", s_msg );
}

static rtxLightEditAPI_t *LE_GetAPI( void )
{
	rtxLightEditAPI_t	*api = (rtxLightEditAPI_t *)cgi_R_GetExtension( RTX_LIGHTEDIT_API_NAME );

	if ( api && api->version != RTX_LIGHTEDIT_API_VERSION )
	{
		api = NULL;
	}
	return api;
}

qboolean LE_GetDesc( int id, rtxLightDesc_t *d )
{
	memset( d, 0, sizeof( *d ) );
	return s_api->Get( id, d );
}

// Compares the fields that Set copies.
qboolean LE_DescEqual( const rtxLightDesc_t *a, const rtxLightDesc_t *b )
{
	return (qboolean)( VectorCompare( a->origin, b->origin ) && VectorCompare( a->color, b->color )
		&& a->intensity == b->intensity && a->radius == b->radius
		&& a->type == b->type && VectorCompare( a->dir, b->dir )
		&& a->coneOuter == b->coneOuter && a->coneInner == b->coneInner
		&& a->width == b->width && a->height == b->height && a->roll == b->roll
		&& a->twoSided == b->twoSided && a->style == b->style
		&& !strncmp( a->name, b->name, RTX_LIGHTEDIT_NAME_LEN ) );
}

static const char *LE_SourceStr( const rtxLightDesc_t *d )
{
	if ( d->source == RTX_LSRC_ENTITY )
	{
		return va( "entity %d", d->sourceKey );
	}
	if ( d->source == RTX_LSRC_LGT )
	{
		return va( "lgt %d", d->sourceKey );
	}
	if ( d->source == RTX_LSRC_EMISSIVE )
	{
		return va( "emissive face rank %d", d->sourceKey );
	}
	return "added";
}

const char *LE_FlagsStr( int flags )
{
	return va( "%s%s%s%s%s%s%s", ( flags & RTX_LFLAG_MODIFIED ) ? "MODIFIED " : "",
		( flags & RTX_LFLAG_DISABLED ) ? "DISABLED " : "", ( flags & RTX_LFLAG_DELETED ) ? "DELETED " : "",
		( flags & RTX_LFLAG_COMPACTED ) ? "COMPACTED " : "",
		( flags & RTX_LFLAG_IN_SOLID ) ? "IN_SOLID " : "", ( flags & RTX_LFLAG_DRAGGING ) ? "DRAGGING " : "",
		( flags & RTX_LFLAG_MUTED ) ? "MUTED" : "" );
}

static qboolean LE_NeedActive( void )
{
	if ( !s_active || !s_api )
	{
		LE_Msg( "light edit: not active" );
		return qfalse;
	}
	return qtrue;
}

static qboolean LE_NeedSelection( void )
{
	if ( s_sel < 0 )
	{
		LE_Msg( "light edit: nothing selected" );
		return qfalse;
	}
	return qtrue;
}

/*
=================
Selection
=================
*/
qboolean LE_SelHas( int id )
{
	return (qboolean)( std::find( s_selList.begin(), s_selList.end(), id ) != s_selList.end() );
}

void LE_SelClear( void )
{
	s_selList.clear();
	s_sel = -1;
}

// Appends the light and makes it the primary one.
void LE_SelAdd( int id )
{
	if ( id < 0 )
	{
		return;
	}
	if ( !LE_SelHas( id ) )
	{
		s_selList.push_back( id );
	}
	s_sel = id;
}

// A removed light leaves the selection; the last clicked one that remains becomes primary.
static void LE_SelRemove( int id )
{
	std::vector<int>::iterator	it = std::find( s_selList.begin(), s_selList.end(), id );

	if ( it != s_selList.end() )
	{
		s_selList.erase( it );
	}
	if ( s_sel == id || !LE_SelHas( s_sel ) )
	{
		s_sel = s_selList.empty() ? -1 : s_selList.back();
	}
}

void LE_SelSet( int id )
{
	LE_SelClear();
	LE_SelAdd( id );
}

void LE_SelToggle( int id )
{
	if ( LE_SelHas( id ) )
	{
		LE_SelRemove( id );
	}
	else
	{
		LE_SelAdd( id );
	}
}

void LE_SelPrimary( int id )
{
	if ( LE_SelHas( id ) )
	{
		s_sel = id;
	}
}

// Targets of an action: the selection when the aimed light is a member (or nothing is aimed), else the aimed light.
void LE_ActionTargets( std::vector<int> &out )
{
	out.clear();
	if ( s_pickId >= 0 && !LE_SelHas( s_pickId ) )
	{
		out.push_back( s_pickId );
		return;
	}
	out = s_selList;
}

/*
=================
Actions shared by tools and commands
=================
*/
// Remove on each light, one undo group.
static void LE_DoRemove( const std::vector<int> &ids )
{
	int	done = 0;

	{
		ledBatch	batch;

		LE_UndoBegin( "remove" );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	before, after;

			if ( !LE_GetDesc( ids[i], &before ) )
			{
				continue;
			}
			if ( before.flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_DELETED ) )
			{
				if ( ids.size() == 1 )
				{
					LE_Msg( "light edit: light %d is already removed", ids[i] );
				}
				continue;
			}
			if ( !s_api->Remove( ids[i] ) )
			{
				LE_Msg( "light edit: remove failed (%s)", s_api->LastError() );
				continue;
			}
			LE_GetDesc( ids[i], &after );
			LE_UndoPush( LEDU_REMOVE, ids[i], &before, &after, "remove" );
			if ( after.flags & RTX_LFLAG_DELETED )
			{
				LE_SelRemove( ids[i] );
			}
			done++;
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: removed %d light%s", done, done > 1 ? "s" : "" );
	}
}

// Restore on a disabled or compacted light, one undo entry.
static void LE_DoRestore( int id )
{
	rtxLightDesc_t	before, after;

	if ( !LE_GetDesc( id, &before ) )
	{
		return;
	}
	if ( !( before.flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_COMPACTED ) ) )
	{
		LE_Msg( "light edit: light %d is not removed", id );
		return;
	}
	if ( !s_api->Restore( id ) )
	{
		LE_Msg( "light edit: restore failed (%s)", s_api->LastError() );
		return;
	}
	LE_GetDesc( id, &after );
	LE_UndoPush( LEDU_RESTORE, id, &before, &after, "restore" );
	LE_Msg( "light edit: restored light %d", id );
}

// Set on a light, one undo entry when something changes.
qboolean LE_DoSet( int id, const rtxLightDesc_t *want, const char *label )
{
	rtxLightDesc_t	before, after;

	if ( !LE_GetDesc( id, &before ) )
	{
		return qfalse;
	}
	if ( LE_DescEqual( &before, want ) )
	{
		return qtrue;
	}
	if ( !s_api->Set( id, want ) )
	{
		LE_Msg( "light edit: set failed (%s)", s_api->LastError() );
		return qfalse;
	}
	LE_GetDesc( id, &after );
	LE_UndoPush( LEDU_SET, id, &before, &after, label );
	return qtrue;
}

/*
=================
Mode enter / leave
=================
*/
static void LE_ReadButtons( int *buttons )
{
	usercmd_t	cmd;

	memset( &cmd, 0, sizeof( cmd ) );
	if ( !cgi_GetUserCmd( cgi_GetCurrentCmdNumber(), &cmd ) )
	{
		cmd.buttons = 0;
	}
	*buttons = cmd.buttons;
}

static void LE_LeaveSide( void )
{
	if ( s_active )
	{
		LE_EndGrab();
		LE_SoloRelease();
		LE_StillAccumRestore();
		if ( s_api )
		{
			s_api->End();
		}
	}
	s_active = qfalse;
	s_restoreMode = qfalse;
	LE_CamLeave( LE_CAM_USER_LIGHTEDIT );
	LE_GrabReset();
	s_pickId = -1;
	s_hits.clear();
}

static void LE_EnterSide( void )
{
	s_api = LE_GetAPI();
	if ( !s_api || !s_api->IsAvailable() || !s_api->Begin() )
	{
		LE_Msg( "light edit: %s", s_api ? s_api->LastError() : "needs the RTX renderer" );
		G_LightEdit_SetMode( qfalse );
		s_leaving = qtrue;
		return;
	}
	s_active = qtrue;
	LE_CamEnter( LE_CAM_USER_LIGHTEDIT );
	s_tool = LEDIT_TOOL_SELECT;
	LE_SelClear();
	s_pickId = -1;
	s_cycle = 0;
	s_cycleBase = -1;
	s_wheel = 0;
	LE_GrabReset();
	s_occ.clear();
	LE_ReadButtons( &s_prevButtons );
}

void LE_SetTool( int tool )
{
	if ( tool < LEDIT_TOOL_SELECT || tool > LEDIT_TOOL_SOLO )
	{
		LE_Msg( "light edit: tool %d is not in this version", tool );
		return;
	}
	if ( tool != s_tool )
	{
		LE_EndGrab();
		s_tool = tool;
		s_cycle = 0;
	}
}

/*
=================
Per-frame snapshot and picking
=================
*/
static qboolean LE_Occluded( const vec3_t eye, const vec3_t org, float dist )
{
	trace_t	tr;

	CG_Trace( &tr, eye, vec3_origin, vec3_origin, org, cg.snap->ps.clientNum, CONTENTS_SOLID );
	if ( tr.startsolid || tr.allsolid )
	{
		return qfalse;
	}
	return (qboolean)( tr.fraction < 1.0f && dist * ( 1.0f - tr.fraction ) > 4.0f );
}

static bool LE_HitLess( const ledHit_t &a, const ledHit_t &b )
{
	return a.surface != b.surface ? a.surface < b.surface : a.t < b.t;
}

// Caches every descriptor, projects the lights and builds the sorted list of lights under the crosshair.
static void LE_Snapshot( void )
{
	const int	n = s_api->Count();
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];
	int			traces = 0;

	s_recs.resize( n );
	if ( (int)s_occ.size() < n )
	{
		s_occ.resize( n, 0 );
	}
	s_hits.clear();

	for ( int i = 0; i < n; i++ )
	{
		ledRec_t	&r = s_recs[i];

		r.valid = qfalse;
		r.onScreen = qfalse;
		r.occluded = qfalse;
		memset( &r.d, 0, sizeof( r.d ) );
		if ( !s_api->Get( i, &r.d ) )
		{
			continue;
		}
		const qboolean	compacted = (qboolean)( ( r.d.flags & RTX_LFLAG_COMPACTED ) != 0 );

		// A compacted light is gone, except in the restore mode. A deleted light is gone.
		if ( compacted ? !s_restoreMode : ( r.d.flags & RTX_LFLAG_DELETED ) != 0 )
		{
			continue;
		}
		r.valid = qtrue;
	}

	// The lights go first: the emissive surfaces come after them and use what is left of the trace budget.
	for ( int pass = 0; pass < 2; pass++ )
	for ( int i = 0; i < n; i++ )
	{
		ledRec_t	&r = s_recs[i];
		vec3_t		rel;
		float		t, pr, d2;
		qboolean	hit;

		if ( !r.valid || LE_IsSurface( &r.d ) != ( pass == 1 ) )
		{
			continue;
		}
		const qboolean	compacted = (qboolean)( ( r.d.flags & RTX_LFLAG_COMPACTED ) != 0 );

		VectorSubtract( r.d.origin, eye, rel );
		t = DotProduct( rel, fwd );
		r.depth = t;

		// A surface is shown and aimed with ledit_show 2 and more, within the range of the grey dots.
		if ( t <= 0.0f || ( pass == 1 && ( ledit_show.integer < 2 || VectorLengthSquared( rel ) > LEDIT_SURFACE_RANGE * LEDIT_SURFACE_RANGE ) ) )
		{
			continue;
		}
		if ( CG_WorldCoordToScreenCoordFloat( r.d.origin, &r.sx, &r.sy ) )
		{
			r.onScreen = (qboolean)( r.sx > -32 && r.sx < 672 && r.sy > -32 && r.sy < 512 );
		}

		pr = Q_max( LEDIT_PICK_MIN_RADIUS, LEDIT_PICK_SCALE * t );
		d2 = DotProduct( rel, rel ) - t * t;
		hit = (qboolean)( d2 <= pr * pr );

		if ( ( r.onScreen || hit ) && !ledit_xray.integer && !( r.d.flags & RTX_LFLAG_IN_SOLID )
			&& VectorLength( rel ) <= LEDIT_OCCLUDE_RANGE )
		{
			if ( traces < LEDIT_MAX_TRACES )
			{
				s_occ[i] = LE_Occluded( eye, r.d.origin, VectorLength( rel ) ) ? 1 : 0;
				traces++;
			}
			r.occluded = (qboolean)( s_occ[i] != 0 );
		}
		else if ( !ledit_xray.integer && !( r.d.flags & RTX_LFLAG_IN_SOLID ) && VectorLength( rel ) > LEDIT_OCCLUDE_RANGE )
		{
			// No trace this far: show it only in x-ray mode.
			r.occluded = qtrue;
		}

		// Only tool 8 aims at a compacted light.
		if ( hit && !r.occluded && ( !compacted || s_tool == LEDIT_TOOL_DELETE ) )
		{
			ledHit_t	h;

			h.id = i;
			h.t = t;
			h.surface = pass;
			s_hits.push_back( h );
		}
	}
	std::sort( s_hits.begin(), s_hits.end(), LE_HitLess );

	// Deleted and unknown lights leave the selection.
	for ( size_t i = 0; i < s_selList.size(); )
	{
		const int	id = s_selList[i];

		if ( id >= n || !s_recs[id].valid )
		{
			LE_SelRemove( id );
		}
		else
		{
			i++;
		}
	}
}

// Picks among the hits; tool 1 cycles with the wheel.
static void LE_Pick( int wheel )
{
	const int	n = (int)s_hits.size();

	if ( n == 0 || s_hits[0].id != s_cycleBase )
	{
		s_cycle = 0;
		s_cycleBase = n ? s_hits[0].id : -1;
	}
	if ( s_tool == LEDIT_TOOL_SELECT )
	{
		s_cycle += wheel;
	}
	else
	{
		s_cycle = 0;
	}
	if ( n == 0 )
	{
		s_pickId = -1;
		s_pickT = 0.0f;
		return;
	}
	const int	idx = ( ( s_cycle % n ) + n ) % n;

	s_pickId = s_hits[idx].id;
	s_pickT = s_hits[idx].t;
}

/*
=================
Tools
=================
*/
static void LE_ToolSelect( qboolean priDown, qboolean secDown )
{
	if ( priDown )
	{
		if ( s_pickId >= 0 )
		{
			LE_SelSet( s_pickId );
		}
		else
		{
			// Nothing aimed: the selection and the selected shader empty.
			LE_SelClear();
			LE_EmissiveClear();
		}
	}
	if ( secDown && s_pickId >= 0 )
	{
		LE_SelToggle( s_pickId );
	}
}

static void LE_ParseColor( vec3_t out )
{
	float	c[3] = { 1.0f, 0.9f, 0.8f };

	sscanf( ledit_preset_color.string, "%f %f %f", &c[0], &c[1], &c[2] );
	for ( int i = 0; i < 3; i++ )
	{
		out[i] = Com_Clamp( 0.0f, 1.0f, c[i] );
	}
}

// The preset of a new light: the clipboard of tool 6 when it holds a light, else the ledit_preset_* cvars.
static void LE_CreatePreset( rtxLightDesc_t *d )
{
	memset( d, 0, sizeof( *d ) );
	LE_ParseColor( d->color );
	d->intensity = ledit_preset_intensity.value;
	d->radius = ledit_preset_radius.value;
	d->coneOuter = ledit_preset_cone_outer.value;
	d->coneInner = ledit_preset_cone_inner.value;
	d->width = ledit_preset_width.value;
	d->height = ledit_preset_height.value;
	d->roll = ledit_preset_roll.value;
	d->twoSided = ledit_preset_twosided.integer ? 1 : 0;
	d->style = ledit_preset_style.integer;
	LE_PipettePreset( d );
}

static void LE_CreatePresetStore( const rtxLightDesc_t *d )
{
	rtxLightDesc_t	probe;

	memset( &probe, 0, sizeof( probe ) );
	if ( LE_PipettePreset( &probe ) )
	{
		LE_PipettePresetSet( d );
		return;
	}
	cgi_Cvar_Set( "ledit_preset_intensity", va( "%g", d->intensity ) );
	cgi_Cvar_Set( "ledit_preset_radius", va( "%g", d->radius ) );
	cgi_Cvar_Set( "ledit_preset_color", va( "%.3f %.3f %.3f", d->color[0], d->color[1], d->color[2] ) );
	cgi_Cvar_Set( "ledit_preset_cone_outer", va( "%g", d->coneOuter ) );
	cgi_Cvar_Set( "ledit_preset_cone_inner", va( "%g", d->coneInner ) );
	cgi_Cvar_Set( "ledit_preset_width", va( "%g", d->width ) );
	cgi_Cvar_Set( "ledit_preset_height", va( "%g", d->height ) );
	cgi_Cvar_Set( "ledit_preset_roll", va( "%g", d->roll ) );
	cgi_Cvar_Set( "ledit_preset_twosided", va( "%d", d->twoSided ? 1 : 0 ) );
	cgi_Cvar_Set( "ledit_preset_style", va( "%d", d->style ) );
}

// Shift + wheel: a step on the active property (tool 5, "[" and "]") of the preset.
static void LE_CreatePresetWheel( int wheel )
{
	rtxLightDesc_t	d;
	const int		prop = LE_PropActive();
	char			value[32];

	LE_CreatePreset( &d );
	d.type = s_createType;
	if ( !LE_PropApplies( prop, d.type ) )
	{
		LE_Msg( "light edit: %s does not apply to a %s ([ and ] pick another property)", LE_PropName( prop ), LE_TypeName( d.type ) );
		return;
	}
	// The coarse step: shift is often the walk key, which would make every step fine.
	if ( !LE_PropAdjust( &d, prop, wheel, qfalse ) )
	{
		return;
	}
	LE_CreatePresetStore( &d );
	// The cvars update at the next frame: the clipboard and the cvars give the same value from here.
	LE_PropValueText( &d, prop, value, sizeof( value ) );
	LE_Msg( "light edit: new lights %s %s", LE_PropName( prop ), value );
}

// ledit_preset_step <n>: the command Shift + wheel runs in tool 2, for a bind or a script.
static void LE_CmdPresetStep( void )
{
	if ( !s_active )
	{
		LE_Msg( "light edit: not active" );
		return;
	}
	LE_CreatePresetWheel( Com_Clampi( -8, 8, atoi( CG_Argv( 1 ) ) ) );
}

static void LE_UpdateGhost( void )
{
	trace_t		tr;
	vec3_t		end;
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];

	VectorMA( eye, 8192.0f, fwd, end );
	CG_Trace( &tr, eye, vec3_origin, vec3_origin, end, cg.snap->ps.clientNum, CONTENTS_SOLID );
	if ( tr.fraction < 1.0f && !tr.startsolid && !tr.allsolid && !( tr.surfaceFlags & SURF_SKY ) )
	{
		VectorMA( tr.endpos, s_createOffset, tr.plane.normal, s_ghost );
		VectorCopy( tr.plane.normal, s_ghostNormal );
		VectorCopy( tr.endpos, s_ghostHitPos );
		s_ghostHit = qtrue;
	}
	else
	{
		VectorMA( eye, 256.0f, fwd, s_ghost );
		s_ghostHit = qfalse;
	}
	LE_SnapPoint( s_ghost, 7 );
	s_ghostSolid = s_api->PointInSolid( s_ghost );
}

static void LE_ToolCreate( qboolean priDown, qboolean secDown, int wheel, qboolean fine )
{
	for ( int i = 0; i < abs( wheel ); i++ )
	{
		if ( fine )
		{
			s_createOffset += ( wheel > 0 ) ? 1.0f : -1.0f;
		}
		else
		{
			s_createOffset *= ( wheel > 0 ) ? 2.0f : 0.5f;
		}
		s_createOffset = Com_Clamp( 2.0f, 256.0f, s_createOffset );
	}
	if ( s_presetWheel )
	{
		LE_CreatePresetWheel( s_presetWheel );
		s_presetWheel = 0;
	}
	LE_UpdateGhost();

	if ( secDown )
	{
		s_createType = ( s_createType + 1 ) % ( RTX_LTYPE_RECT + 1 );
		LE_Msg( "light edit: new lights are %ss", LE_TypeName( s_createType ) );
	}
	if ( priDown )
	{
		rtxLightDesc_t	d, after;
		int				id;

		if ( s_ghostSolid )
		{
			LE_Msg( "light edit: cannot add a light outside the world" );
			return;
		}
		LE_CreatePreset( &d );
		VectorCopy( s_ghost, d.origin );
		VectorSet( d.dir, 0.0f, 0.0f, -1.0f );
		if ( s_createType == RTX_LTYPE_SPOT && s_ghostHit )
		{
			LE_SpotCreateDir( s_ghost, s_ghostHitPos, s_ghostNormal, d.dir );
		}
		else if ( s_createType == RTX_LTYPE_RECT && s_ghostHit )
		{
			VectorCopy( s_ghostNormal, d.dir );
		}
		LE_ConvertType( &d, s_createType );
		LE_SoloEndForAdd();
		id = s_api->Add( &d );
		if ( id < 0 )
		{
			LE_Msg( "light edit: add failed (%s)", s_api->LastError() );
			return;
		}
		LE_GetDesc( id, &after );
		LE_UndoPush( LEDU_ADD, id, NULL, &after, "add" );
		LE_SelSet( id );
		LE_Msg( "light edit: added light %d", id );
	}
}

int LE_CreateType( void )
{
	return s_createType;
}

void LE_CreateKeyShift( qboolean shift )
{
	s_keyShift = shift;
	s_keyShiftTime = cg.time;
}

void LE_CreateSetSpot( qboolean spot )
{
	s_createType = spot ? RTX_LTYPE_SPOT : RTX_LTYPE_SPHERE;
}

void LE_CreateSetType( int type )
{
	s_createType = Com_Clampi( RTX_LTYPE_SPHERE, RTX_LTYPE_RECT, type );
}

static void LE_ToolDelete( qboolean priDown, qboolean secDown )
{
	if ( priDown )
	{
		std::vector<int>	ids;

		LE_ActionTargets( ids );
		if ( !ids.empty() )
		{
			LE_DoRemove( ids );
		}
	}
	if ( secDown && s_pickId >= 0 )
	{
		LE_DoRestore( s_pickId );
	}
}

// The menu follows the selection of the game through these cvars: ledit_sel is the primary light,
// ledit_emi_sel the emissive shader; -1 for none. A cvar is written when its value changes.
static int s_published[2] = { -2, -2 };

static void LE_PublishSelection( void )
{
	static const char	*names[2] = { "ledit_sel", "ledit_emi_sel" };
	const int			want[2] = { s_sel, LE_EmissiveSelected() };

	for ( int i = 0; i < 2; i++ )
	{
		if ( want[i] != s_published[i] )
		{
			s_published[i] = want[i];
			cgi_Cvar_Set( names[i], va( "%d", want[i] ) );
		}
	}
}

/*
=================
CG_LightEdit_Frame
=================
*/
void CG_LightEdit_Frame( void )
{
	int			buttons, wheel;
	qboolean	prim, sec, priDown, secDown, fine, useDown;
	const qboolean	gameActive = G_LightEdit_Active();

	cgi_Cvar_Update( &ledit_xray );
	cgi_Cvar_Update( &ledit_show );
	cgi_Cvar_Update( &ledit_preset_intensity );
	cgi_Cvar_Update( &ledit_preset_radius );
	cgi_Cvar_Update( &ledit_preset_color );
	cgi_Cvar_Update( &ledit_preset_cone_outer );
	cgi_Cvar_Update( &ledit_preset_cone_inner );
	cgi_Cvar_Update( &ledit_preset_width );
	cgi_Cvar_Update( &ledit_preset_height );
	cgi_Cvar_Update( &ledit_preset_roll );
	cgi_Cvar_Update( &ledit_preset_twosided );
	cgi_Cvar_Update( &ledit_preset_style );
	LE_GridUpdate();
	LE_SoloUpdate();
	LE_KeysUpdate();
	LE_LabelUpdate();

	if ( s_leaving && !gameActive )
	{
		s_leaving = qfalse;
	}
	if ( gameActive && !s_active && !s_leaving )
	{
		LE_EnterSide();
	}
	else if ( !gameActive && s_active )
	{
		LE_LeaveSide();
	}
	if ( !s_active )
	{
		s_wheel = 0;
		return;
	}
	if ( !cg.snap || !s_api->IsAvailable() )
	{
		LE_Msg( "light edit: the RTX renderer is no longer available" );
		LE_LeaveSide();
		G_LightEdit_SetMode( qfalse );
		s_leaving = qtrue;
		return;
	}

	if ( cg.snap->ps.stats[STAT_HEALTH] <= 0 )
	{
		LE_Msg( "light edit: the player is dead" );
		LE_LeaveSide();
		G_LightEdit_SetMode( qfalse );
		s_leaving = qtrue;
		return;
	}
	LE_CamFrame();

	LE_ReadButtons( &buttons );
	prim = (qboolean)( ( buttons & BUTTON_ATTACK ) != 0 );
	sec = (qboolean)( ( buttons & BUTTON_ALT_ATTACK ) != 0 );
	priDown = (qboolean)( prim && !( s_prevButtons & BUTTON_ATTACK ) );
	secDown = (qboolean)( sec && !( s_prevButtons & BUTTON_ALT_ATTACK ) );
	fine = (qboolean)( ( buttons & BUTTON_WALKING ) != 0 );
	useDown = (qboolean)( ( buttons & BUTTON_USE ) && !( s_prevButtons & BUTTON_USE ) );
	s_prevButtons = buttons;

	wheel = Com_Clampi( -8, 8, s_wheel );
	s_wheel = 0;
	if ( useDown )
	{
		LE_GotoSelection();
	}

	s_api->GetStats( &s_stats );
	LE_Snapshot();
	if ( LE_GrabActive() )
	{
		s_pickId = LE_GrabPrimary();	// the grabbed light stays the picked one
	}
	else
	{
		LE_Pick( wheel );
	}

	switch ( s_tool )
	{
	case LEDIT_TOOL_SELECT:
		LE_ToolSelect( priDown, secDown );
		break;
	case LEDIT_TOOL_CREATE:
		LE_ToolCreate( priDown, secDown, wheel, fine );
		break;
	case LEDIT_TOOL_MOVE:
		LE_ToolMove( prim, priDown, secDown, wheel, fine );
		break;
	case LEDIT_TOOL_ORIENT:
		LE_ToolOrient( priDown, secDown, wheel, fine );
		break;
	case LEDIT_TOOL_PROPS:
		LE_ToolProps( priDown, secDown, wheel, fine );
		break;
	case LEDIT_TOOL_PIPETTE:
		LE_ToolPipette( priDown, secDown, wheel );
		break;
	case LEDIT_TOOL_CLONE:
		LE_ToolClone( priDown, secDown, wheel );
		break;
	case LEDIT_TOOL_DELETE:
		LE_ToolDelete( priDown, secDown );
		break;
	case LEDIT_TOOL_SOLO:
		LE_ToolSolo( priDown, secDown, wheel );
		break;
	}
	LE_StillAccumFrame( buttons );
	LE_PublishSelection();
}

void CG_LightEdit_Init( void )
{
	// The map change drops the renderer state: nothing to release here.
	s_api = LE_GetAPI();
	s_active = qfalse;
	s_restoreMode = qfalse;
	LE_CamInit();
	s_leaving = qfalse;
	s_tool = LEDIT_TOOL_SELECT;
	LE_SelClear();
	s_pickId = -1;
	s_cycle = 0;
	s_cycleBase = -1;
	s_wheel = 0;
	s_presetWheel = 0;
	s_keyShift = qfalse;
	s_prevButtons = 0;
	s_createOffset = 16.0f;
	s_ghostSolid = qfalse;
	s_ghostHit = qfalse;
	VectorSet( s_ghostNormal, 0.0f, 0.0f, 1.0f );
	s_createType = RTX_LTYPE_SPHERE;
	LE_GrabReset();
	LE_CloneInit();
	LE_OrientInit();
	LE_PropsInit();
	LE_PipetteInit();
	LE_SoloInit();
	LE_KeysInit();
	LE_LabelInit();
	LE_EmissiveInit();
	LE_SkyInit();
	memset( &s_stats, 0, sizeof( s_stats ) );
	s_published[0] = s_published[1] = -2;
	s_recs.clear();
	s_hits.clear();
	s_occ.clear();
	LE_UndoClear();
	s_msg[0] = 0;
	s_msgEnd = 0;

	cgi_Cvar_Register( &ledit_xray, "ledit_xray", "0", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_show, "ledit_show", "1", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_intensity, "ledit_preset_intensity", "2000", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_radius, "ledit_preset_radius", "8", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_color, "ledit_preset_color", "1 0.9 0.8", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_cone_outer, "ledit_preset_cone_outer", "35", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_cone_inner, "ledit_preset_cone_inner", "25", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_width, "ledit_preset_width", "32", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_height, "ledit_preset_height", "32", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_roll, "ledit_preset_roll", "0", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_twosided, "ledit_preset_twosided", "0", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_style, "ledit_preset_style", "0", CVAR_ARCHIVE );
	LE_GridInit();
}

qboolean CG_LightEdit_Active( void )
{
	return s_active;
}

// Leaves light edit for another mode. The unsaved changes stay in memory.
void CG_LightEdit_Stop( void )
{
	if ( !s_active && !G_LightEdit_Active() )
	{
		return;
	}
	LE_LeaveSide();
	s_leaving = qtrue;
	G_LightEdit_SetMode( qfalse );
}

/*
=================
Console commands
=================
*/
static void LE_Cmd_LightEdit( void )
{
	char			a1[32], a2[32];

	// CG_Argv returns a static buffer: copy each argument.
	Q_strncpyz( a1, CG_Argv( 1 ), sizeof( a1 ) );
	Q_strncpyz( a2, CG_Argv( 2 ), sizeof( a2 ) );

	const qboolean	discard = (qboolean)( !Q_stricmp( a1, "discard" ) || !Q_stricmp( a2, "discard" ) );
	const qboolean	cur = (qboolean)( s_active || ( G_LightEdit_Active() && !s_leaving ) );
	qboolean		want;

	if ( a1[0] && Q_stricmp( a1, "discard" ) )
	{
		want = (qboolean)( atoi( a1 ) != 0 );
	}
	else
	{
		want = (qboolean)!cur;
	}

	if ( want )
	{
		rtxLightEditAPI_t	*api;

		if ( cur )
		{
			return;
		}
		api = LE_GetAPI();
		if ( !api || !api->IsAvailable() )
		{
			LE_Msg( "light edit: needs the RTX renderer (cl_renderer rdsp-vulkan, r_rtx 1)" );
			return;
		}
		s_api = api;
		s_leaving = qfalse;
		G_LightEdit_SetMode( qtrue );
		if ( G_LightEdit_Active() )
		{
			G_FreeCam_SetMode( qfalse );	// light edit leaves the free camera
		}
		return;
	}

	if ( !cur )
	{
		LE_Msg( "light edit: not active" );
		return;
	}
	if ( s_active && s_api )
	{
		LE_EndGrab();
		if ( discard )
		{
			s_api->Reload();
			LE_UndoClear();
			LE_SelClear();
		}
		else
		{
			rtxLightStats_t	st;

			s_api->GetStats( &st );
			if ( st.unsavedChanges > 0 )
			{
				LE_Msg( "light edit: %d unsaved changes kept in memory until the next map load (ledit_save to write them, lightedit 0 discard to drop them)", st.unsavedChanges );
			}
		}
	}
	LE_LeaveSide();
	s_leaving = qtrue;
	G_LightEdit_SetMode( qfalse );
}

void LE_Cmd_Save( void )
{
	if ( !LE_NeedActive() )
	{
		return;
	}
	if ( s_api->Save() )
	{
		LE_Msg( "light edit: saved" );
	}
	else
	{
		LE_Msg( "light edit: save failed (%s)", s_api->LastError() );
	}
}

static void LE_Cmd_Reload( void )
{
	if ( !LE_NeedActive() )
	{
		return;
	}
	LE_EndGrab();
	if ( s_api->Reload() )
	{
		LE_UndoClear();
		LE_SelClear();
		LE_Msg( "light edit: reloaded" );
	}
	else
	{
		LE_Msg( "light edit: reload failed (%s)", s_api->LastError() );
	}
}

void LE_Cmd_Undo( void )
{
	if ( LE_NeedActive() )
	{
		LE_Undo();
	}
}

void LE_Cmd_Redo( void )
{
	if ( LE_NeedActive() )
	{
		LE_Redo();
	}
}

static void LE_Cmd_History( void )
{
	if ( LE_NeedActive() )
	{
		LE_UndoHistory();
	}
}

void LE_Cmd_Delete( void )
{
	if ( LE_NeedActive() && LE_NeedSelection() )
	{
		LE_DoRemove( s_selList );
	}
}

void LE_Cmd_Deselect( void )
{
	if ( LE_NeedActive() )
	{
		LE_SelClear();
		LE_Msg( "light edit: selection cleared" );
	}
}

// ledit_compact: releases the slots of the removed lights. The lights stay in memory and in the file.
static void LE_Cmd_Compact( void )
{
	int	slots = 0, lights;

	if ( !LE_NeedActive() )
	{
		return;
	}
	LE_EndGrab();
	lights = s_api->Compact( &slots );
	if ( lights < 0 )
	{
		LE_Msg( "light edit: compact failed (%s)", s_api->LastError() );
		return;
	}
	if ( lights == 0 )
	{
		LE_Msg( "light edit: no removed light to compact" );
		return;
	}
	// A compacted light cannot stay selected.
	for ( size_t i = 0; i < s_selList.size(); )
	{
		rtxLightDesc_t	d;

		if ( LE_GetDesc( s_selList[i], &d ) && ( d.flags & RTX_LFLAG_COMPACTED ) )
		{
			LE_SelRemove( s_selList[i] );
		}
		else
		{
			i++;
		}
	}
	s_api->GetStats( &s_stats );
	LE_Msg( "light edit: compacted %d light%s, released %d slot%s, slots %d/%d", lights, lights > 1 ? "s" : "",
		slots, slots != 1 ? "s" : "", s_stats.slotsUsed, s_stats.maxLightPolys );
}

// ledit_restore [0|1]: shows the compacted lights and lets tool 8 restore them.
static void LE_Cmd_RestoreMode( void )
{
	if ( !LE_NeedActive() )
	{
		return;
	}
	s_restoreMode = ( cgi_Argc() > 1 ) ? (qboolean)( atoi( CG_Argv( 1 ) ) != 0 ) : (qboolean)!s_restoreMode;
	if ( s_restoreMode )
	{
		LE_Msg( "light edit: restore mode on, %d compacted light%s (tool 8, Alt restores the aimed one)", s_stats.numCompacted,
			s_stats.numCompacted != 1 ? "s" : "" );
	}
	else
	{
		LE_Msg( "light edit: restore mode off" );
	}
}

static void LE_Cmd_XrayToggle( void )
{
	if ( LE_NeedActive() )
	{
		const int	on = ledit_xray.integer ? 0 : 1;

		cgi_Cvar_Set( "ledit_xray", va( "%d", on ) );
		ledit_xray.integer = on;
		LE_Msg( "light edit: x-ray %s", on ? "on" : "off" );
	}
}

static void LE_Cmd_GridNext( void )
{
	if ( LE_NeedActive() )
	{
		LE_GridNext();
	}
}

static void LE_Cmd_SnapToggle( void )
{
	if ( LE_NeedActive() )
	{
		LE_SnapToggle();
	}
}

// Reverts every selected light, one undo group.
static void LE_Cmd_Revert( void )
{
	int	done = 0;

	if ( !LE_NeedActive() || !LE_NeedSelection() )
	{
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBegin( "revert" );
		for ( size_t i = 0; i < s_selList.size(); i++ )
		{
			rtxLightDesc_t	before, after;

			if ( !LE_GetDesc( s_selList[i], &before ) )
			{
				continue;
			}
			if ( !s_api->Revert( s_selList[i] ) )
			{
				LE_Msg( "light edit: revert failed (%s)", s_api->LastError() );
				continue;
			}
			LE_GetDesc( s_selList[i], &after );
			if ( !LE_DescEqual( &before, &after ) )
			{
				LE_UndoPush( LEDU_SET, s_selList[i], &before, &after, "revert" );
			}
			done++;
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: %d light%s reverted", done, done > 1 ? "s" : "" );
	}
}

// Sets a property on every selected light. An origin moves the group: the primary light gets it.
static void LE_Cmd_Set( void )
{
	rtxLightDesc_t	d, prim;
	vec3_t			delta;
	char			what[32];
	const int		argc = cgi_Argc();
	char			label[48];
	int				done = 0;

	Q_strncpyz( what, CG_Argv( 1 ), sizeof( what ) );
	if ( !LE_NeedActive() || !LE_NeedSelection() )
	{
		return;
	}
	if ( !LE_GetDesc( s_sel, &prim ) )
	{
		return;
	}
	d = prim;
	VectorClear( delta );
	if ( !Q_stricmp( what, "origin" ) && argc >= 5 )
	{
		for ( int i = 0; i < 3; i++ )
		{
			d.origin[i] = atof( CG_Argv( 2 + i ) );
		}
		VectorSubtract( d.origin, prim.origin, delta );
	}
	else if ( !Q_stricmp( what, "color" ) && argc >= 5 )
	{
		for ( int i = 0; i < 3; i++ )
		{
			d.color[i] = Com_Clamp( 0.0f, 1.0f, atof( CG_Argv( 2 + i ) ) );
		}
	}
	else if ( !Q_stricmp( what, "intensity" ) && argc >= 3 )
	{
		d.intensity = atof( CG_Argv( 2 ) );
	}
	else if ( !Q_stricmp( what, "radius" ) && argc >= 3 )
	{
		d.radius = atof( CG_Argv( 2 ) );
	}
	else if ( !Q_stricmp( what, "name" ) && argc >= 3 )
	{
		Q_strncpyz( d.name, CG_Argv( 2 ), sizeof( d.name ) );
	}
	else if ( !Q_stricmp( what, "type" ) && argc >= 3 )
	{
		char	t[16];

		Q_strncpyz( t, CG_Argv( 2 ), sizeof( t ) );
		if ( !Q_stricmp( t, "spot" ) )
		{
			LE_ConvertType( &d, RTX_LTYPE_SPOT );
		}
		else if ( !Q_stricmp( t, "sphere" ) )
		{
			LE_ConvertType( &d, RTX_LTYPE_SPHERE );
		}
		else if ( !Q_stricmp( t, "rect" ) )
		{
			LE_ConvertType( &d, RTX_LTYPE_RECT );
		}
		else
		{
			LE_Msg( "usage: ledit_set type <sphere | spot | rect>" );
			return;
		}
	}
	else if ( ( !Q_stricmp( what, "width" ) || !Q_stricmp( what, "height" ) || !Q_stricmp( what, "roll" )
		|| !Q_stricmp( what, "twosided" ) ) && argc >= 3 )
	{
		const float	val = atof( CG_Argv( 2 ) );

		if ( d.type != RTX_LTYPE_RECT )
		{
			LE_Msg( "light edit: %s applies to rect lights", what );
			return;
		}
		if ( !Q_stricmp( what, "width" ) )
		{
			d.width = Com_Clamp( RTX_LRECT_MIN_SIZE, RTX_LRECT_MAX_SIZE, val );
		}
		else if ( !Q_stricmp( what, "height" ) )
		{
			d.height = Com_Clamp( RTX_LRECT_MIN_SIZE, RTX_LRECT_MAX_SIZE, val );
		}
		else if ( !Q_stricmp( what, "roll" ) )
		{
			d.roll = fmodf( fmodf( val, 360.0f ) + 360.0f, 360.0f );
		}
		else
		{
			d.twoSided = ( val != 0.0f ) ? 1 : 0;
		}
	}
	else if ( !Q_stricmp( what, "style" ) && argc >= 3 )
	{
		d.style = Com_Clampi( 0, RTX_LSTYLE_MAX - 1, atoi( CG_Argv( 2 ) ) );
	}
	else if ( !Q_stricmp( what, "dir" ) && argc >= 5 )
	{
		for ( int i = 0; i < 3; i++ )
		{
			d.dir[i] = atof( CG_Argv( 2 + i ) );
		}
		if ( VectorNormalize( d.dir ) < 0.001f )
		{
			LE_Msg( "light edit: the direction must not be zero" );
			return;
		}
	}
	else if ( !Q_stricmp( what, "cone" ) && argc >= 3 )
	{
		d.coneOuter = Com_Clamp( 1.0f, 89.0f, atof( CG_Argv( 2 ) ) );
		d.coneInner = Com_Clamp( 0.0f, d.coneOuter, ( argc >= 4 ) ? atof( CG_Argv( 3 ) ) : d.coneInner );
	}
	else
	{
		LE_Msg( "usage: ledit_set <origin x y z | color r g b | intensity v | radius v | name s | type sphere|spot|rect | dir x y z | cone outer [inner] | width w | height h | roll r | twosided 0|1 | style n>" );
		return;
	}
	Com_sprintf( label, sizeof( label ), "set %s", what );

	// An emissive surface only has a factor (intensity), a tint (color) and a style.
	const qboolean	surfaceOk = (qboolean)( !Q_stricmp( what, "color" ) || !Q_stricmp( what, "intensity" ) || !Q_stricmp( what, "style" ) );
	int				skipped = 0;

	{
		ledBatch	batch;

		LE_UndoBegin( label );
		for ( size_t i = 0; i < s_selList.size(); i++ )
		{
			rtxLightDesc_t	cur = d;

			// The factor of a surface and the intensity of a light have different units: they do not mix.
			const qboolean	isSurface = LE_IsSurface( LE_RecDesc( s_selList[i] ) );

			if ( ( isSurface && !surfaceOk ) || ( !Q_stricmp( what, "intensity" ) && isSurface != LE_IsSurface( &prim ) ) )
			{
				skipped++;
				continue;
			}

			if ( s_selList[i] != s_sel )
			{
				// Other lights keep their own values except the one that is set.
				if ( !LE_GetDesc( s_selList[i], &cur ) )
				{
					continue;
				}
				if ( !Q_stricmp( what, "origin" ) )
				{
					VectorAdd( cur.origin, delta, cur.origin );
				}
				else if ( !Q_stricmp( what, "color" ) )
				{
					VectorCopy( d.color, cur.color );
				}
				else if ( !Q_stricmp( what, "intensity" ) )
				{
					cur.intensity = d.intensity;
				}
				else if ( !Q_stricmp( what, "radius" ) )
				{
					cur.radius = d.radius;
				}
				else if ( !Q_stricmp( what, "type" ) )
				{
					LE_ConvertType( &cur, d.type );
				}
				else if ( !Q_stricmp( what, "style" ) )
				{
					cur.style = d.style;
				}
				else if ( !Q_stricmp( what, "width" ) || !Q_stricmp( what, "height" ) || !Q_stricmp( what, "roll" )
					|| !Q_stricmp( what, "twosided" ) )
				{
					if ( cur.type != RTX_LTYPE_RECT )
					{
						continue;
					}
					cur.width = !Q_stricmp( what, "width" ) ? d.width : cur.width;
					cur.height = !Q_stricmp( what, "height" ) ? d.height : cur.height;
					cur.roll = !Q_stricmp( what, "roll" ) ? d.roll : cur.roll;
					cur.twoSided = !Q_stricmp( what, "twosided" ) ? d.twoSided : cur.twoSided;
				}
				else if ( !Q_stricmp( what, "dir" ) )
				{
					VectorCopy( d.dir, cur.dir );
				}
				else if ( !Q_stricmp( what, "cone" ) )
				{
					cur.coneOuter = d.coneOuter;
					cur.coneInner = d.coneInner;
				}
				else
				{
					Q_strncpyz( cur.name, d.name, sizeof( cur.name ) );
				}
			}
			if ( LE_DoSet( s_selList[i], &cur, label ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( skipped )
	{
		LE_Msg( "light edit: %d light%s skipped: %s does not apply (an emissive surface takes color, style and its own intensity)",
			skipped, skipped > 1 ? "s" : "", what );
	}
	if ( done )
	{
		LE_Msg( "light edit: %d light%s: %s set", done, done > 1 ? "s" : "", what );
	}
}

static void LE_PrintDesc( const char *title, const rtxLightDesc_t *d )
{
	CG_Printf( "%s origin %.1f %.1f %.1f  color %.3f %.3f %.3f  intensity %.2f  radius %.2f  name '%s'\n", title,
		d->origin[0], d->origin[1], d->origin[2], d->color[0], d->color[1], d->color[2],
		d->intensity, d->radius, d->name );
}

static void LE_Cmd_Get( void )
{
	rtxLightDesc_t	d, orig;

	if ( !LE_NeedActive() || !LE_NeedSelection() || !LE_GetDesc( s_sel, &d ) )
	{
		return;
	}
	if ( s_selList.size() > 1 )
	{
		CG_Printf( "%d lights selected; the primary light follows\n", (int)s_selList.size() );
	}
	{
		char	shape[40], extra[48], style[40];

		LE_ShapeText( &d, shape, sizeof( shape ) );
		LE_StyleText( d.style, style, sizeof( style ) );
		extra[0] = 0;
		if ( d.type == RTX_LTYPE_RECT )
		{
			Com_sprintf( extra, sizeof( extra ), ", roll %.1f, %s", d.roll, d.twoSided ? "two-sided" : "one-sided" );
		}
		CG_Printf( "light %d (%s, %s%s) flags: %s\n", d.id, LE_SourceStr( &d ), shape, extra, LE_FlagsStr( d.flags ) );
		if ( LE_IsSurface( &d ) )
		{
			char	text[96];

			for ( int field = 0; field < 3; field++ )
			{
				LE_SurfaceText( &d, field, text, sizeof( text ) );
				CG_Printf( "  %s\n", text );
			}
		}
		if ( d.style )
		{
			CG_Printf( "  style %s\n", style );
		}
	}
	LE_PrintDesc( "  now:     ", &d );
	if ( ( d.flags & RTX_LFLAG_MODIFIED ) && s_api->GetOriginal( s_sel, &orig ) )
	{
		LE_PrintDesc( "  original:", &orig );
	}
}

qboolean CG_LightEdit_ConsoleCommand( const char *cmd )
{
	static const struct {
		const char	*name;
		void		(*func)( void );
	} commands[] = {
		{ "lightedit",		LE_Cmd_LightEdit },
		{ "ledit_save",		LE_Cmd_Save },
		{ "ledit_reload",	LE_Cmd_Reload },
		{ "ledit_undo",		LE_Cmd_Undo },
		{ "ledit_redo",		LE_Cmd_Redo },
		{ "ledit_history",	LE_Cmd_History },
		{ "ledit_delete",	LE_Cmd_Delete },
		{ "ledit_deselect",	LE_Cmd_Deselect },
		{ "ledit_revert",	LE_Cmd_Revert },
		{ "ledit_set",		LE_Cmd_Set },
		{ "ledit_get",		LE_Cmd_Get },
		{ "ledit_grid_next",	LE_Cmd_GridNext },
		{ "ledit_snap_toggle",	LE_Cmd_SnapToggle },
		{ "ledit_xray_toggle",	LE_Cmd_XrayToggle },
		{ "ledit_compact",	LE_Cmd_Compact },
		{ "ledit_restore",	LE_Cmd_RestoreMode },
		{ "ledit_goto",		LE_CmdGoto },
		{ "ledit_select",	LE_CmdSelect },
		{ "ledit_preset_step",	LE_CmdPresetStep },
		{ "ledit_writebinds",	LE_CmdWriteBinds },
		{ "ledit_emissive_list",	LE_CmdEmissiveList },
		{ "ledit_emissive",	LE_CmdEmissive },
		{ "ledit_emissive_reset",	LE_CmdEmissiveReset },
		{ "ledit_emissive_select",	LE_CmdEmissiveSelect },
		{ "ledit_emissive_surfaces",	LE_CmdEmissiveSurfaces },
		{ "ledit_sky",		LE_CmdSky },
		{ "ledit_sun",		LE_CmdSun },
		{ "ledit_sun_here",	LE_CmdSunHere },
		{ "ledit_sun_color",	LE_CmdSunColor },
		{ "ledit_sun_brightness",	LE_CmdSunBrightness },
		{ "ledit_sun_angle",	LE_CmdSunAngle },
		{ "ledit_sky_reset",	LE_CmdSkyReset },
		{ "ledit_sky_diffuse",	LE_CmdSkyDiffuse },
		{ "ledit_ambient",	LE_CmdAmbient },
		{ "ledit_ambient_reset",	LE_CmdAmbientReset },
	};

	for ( size_t i = 0; i < ARRAY_LEN( commands ); i++ )
	{
		if ( !Q_stricmp( cmd, commands[i].name ) )
		{
			commands[i].func();
			return qtrue;
		}
	}

	if ( s_active )
	{
		if ( !Q_stricmp( cmd, "weapon" ) )
		{
			LE_SetTool( atoi( CG_Argv( 1 ) ) );
			return qtrue;
		}
		if ( !Q_stricmp( cmd, "weapnext" ) || !Q_stricmp( cmd, "weapprev" ) )
		{
			const int	step = !Q_stricmp( cmd, "weapnext" ) ? 1 : -1;

			// The key that runs this command went through the key filter just before: its shift state.
			if ( s_tool == LEDIT_TOOL_CREATE && s_keyShift && cg.time - s_keyShiftTime < 250 )
			{
				s_presetWheel += step;
			}
			else
			{
				s_wheel += step;
			}
			return qtrue;
		}
		if ( !Q_stricmp( cmd, "invnext" ) )
		{
			LE_PropCycle( 1 );
			return qtrue;
		}
		if ( !Q_stricmp( cmd, "invprev" ) )
		{
			LE_PropCycle( -1 );
			return qtrue;
		}
	}
	return qfalse;
}

void CG_LightEdit_InitConsoleCommands( void )
{
	static const char *names[] = {
		"lightedit", "ledit_save", "ledit_reload", "ledit_undo", "ledit_redo", "ledit_history",
		"ledit_delete", "ledit_deselect", "ledit_revert", "ledit_set", "ledit_get",
		"ledit_grid_next", "ledit_snap_toggle", "ledit_xray_toggle", "ledit_compact", "ledit_restore",
		"ledit_goto", "ledit_select", "ledit_preset_step",
		"ledit_writebinds", "ledit_emissive_list", "ledit_emissive", "ledit_emissive_reset",
		"ledit_emissive_select", "ledit_emissive_surfaces",
		"ledit_sky", "ledit_sun", "ledit_sun_here", "ledit_sun_color", "ledit_sun_brightness",
		"ledit_sun_angle", "ledit_sky_reset", "ledit_sky_diffuse", "ledit_ambient", "ledit_ambient_reset"
	};

	for ( size_t i = 0; i < ARRAY_LEN( names ); i++ )
	{
		cgi_AddCommand( names[i] );
	}
}

/*
=================
Overlay
=================
*/
int LE_TextW( const char *s )
{
	return cgi_R_Font_StrLenPixels( s, cgs.media.qhFontSmall, LEDIT_FONT_SCALE, cgs.widthRatioCoef );
}

int LE_TextH( void )
{
	return cgi_R_Font_HeightPixels( cgs.media.qhFontSmall, LEDIT_FONT_SCALE );
}

// A glyph is about one render command. Every text of the overlay counts against this budget per frame.
#define LEDIT_GLYPH_BUDGET		1800
#define LEDIT_GLYPH_RESERVE		1100	// the labels leave this many glyphs to the panel, help and bottom line

static int s_glyphBudget = LEDIT_GLYPH_BUDGET;
static int s_glyphFloor = 0;

void LE_TextFloor( int floorGlyphs )
{
	s_glyphFloor = Q_max( 0, floorGlyphs );
}

// Draws the text, cut at the end of the glyph budget.
void LE_Text( int x, int y, const char *s, const vec4_t col )
{
	char	cut[256];
	int		len = (int)strlen( s );
	const int	left = s_glyphBudget - s_glyphFloor;

	if ( len <= 0 || left <= 0 )
	{
		return;
	}
	if ( len > left )
	{
		Q_strncpyz( cut, s, Q_min( left + 1, (int)sizeof( cut ) ) );
		s = cut;
		len = (int)strlen( cut );
	}
	s_glyphBudget -= len;
	cgi_R_Font_DrawString( x, y, s, col, cgs.media.qhFontSmall, -1, LEDIT_FONT_SCALE, cgs.widthRatioCoef );
}

// Each dot is two render commands. The renderer command buffer is finite: when the overlay
// fills it, the frame drops commands. Keep the dots of one frame under this budget.
#define LEDIT_DOT_BUDGET		2000
#define LEDIT_LINE_MAX_DOTS		64

static int s_dotBudget = LEDIT_DOT_BUDGET;
static int s_dotFloor = 0;		// a drawing with a floor leaves this many dots to the next ones

int LE_DotsLeft( void )
{
	return s_dotBudget - s_dotFloor;
}

void LE_DotFloor( int floorDots )
{
	s_dotFloor = Q_max( 0, floorDots );
}

void LE_Dot( float x, float y, float size, const vec4_t col )
{
	if ( s_dotBudget - s_dotFloor <= 0 )
	{
		return;
	}
	s_dotBudget--;
	CG_FillRect( x - size * 0.5f, y - size * 0.5f, size, size, col );
}

// A filled rectangle counts as one dot.
void LE_Rect( float x, float y, float w, float h, const vec4_t col )
{
	if ( s_dotBudget - s_dotFloor <= 0 )
	{
		return;
	}
	s_dotBudget--;
	CG_FillRect( x, y, w, h, col );
}

const rtxLightDesc_t *LE_RecDesc( int id )
{
	if ( id < 0 || id >= (int)s_recs.size() || !s_recs[id].valid )
	{
		return NULL;
	}
	return &s_recs[id].d;
}

int LE_RecCount( void )
{
	return (int)s_recs.size();
}

qboolean LE_RecScreen( int id, float *sx, float *sy, float *depth, qboolean *occluded )
{
	if ( id < 0 || id >= (int)s_recs.size() || !s_recs[id].valid || !s_recs[id].onScreen )
	{
		return qfalse;
	}
	*sx = s_recs[id].sx;
	*sy = s_recs[id].sy;
	*depth = s_recs[id].depth;
	*occluded = s_recs[id].occluded;
	return qtrue;
}

int LE_ShowMode( void )
{
	return ledit_show.integer;
}

qboolean LE_XrayOn( void )
{
	return (qboolean)( ledit_xray.integer != 0 );
}

// Clips a segment to the virtual screen (Liang-Barsky). Returns qfalse when nothing is left.
static qboolean LE_ClipToScreen( float *x1, float *y1, float *x2, float *y2 )
{
	const float	dx = *x2 - *x1, dy = *y2 - *y1;
	const float	p[4] = { -dx, dx, -dy, dy };
	const float	q[4] = { *x1, 640.0f - *x1, *y1, 480.0f - *y1 };
	float		t0 = 0.0f, t1 = 1.0f;

	for ( int i = 0; i < 4; i++ )
	{
		if ( p[i] == 0.0f )
		{
			if ( q[i] < 0.0f )
			{
				return qfalse;
			}
			continue;
		}
		const float t = q[i] / p[i];
		if ( p[i] < 0.0f )
		{
			t0 = Q_max( t0, t );
		}
		else
		{
			t1 = Q_min( t1, t );
		}
		if ( t0 > t1 )
		{
			return qfalse;
		}
	}
	*x2 = *x1 + t1 * dx;
	*y2 = *y1 + t1 * dy;
	*x1 = *x1 + t0 * dx;
	*y1 = *y1 + t0 * dy;
	return qtrue;
}

// Draws a segment as a row of small dots. Only the visible part is drawn.
void LE_Line( float x1, float y1, float x2, float y2, const vec4_t col )
{
	if ( !LE_ClipToScreen( &x1, &y1, &x2, &y2 ) )
	{
		return;
	}

	const float	dx = x2 - x1, dy = y2 - y1;
	const float	len = sqrtf( dx * dx + dy * dy );
	int			steps = (int)( len / 3.0f ) + 1;

	if ( steps > LEDIT_LINE_MAX_DOTS )
	{
		steps = LEDIT_LINE_MAX_DOTS;
	}
	for ( int i = 0; i <= steps; i++ )
	{
		const float	f = (float)i / (float)steps;

		LE_Dot( x1 + dx * f, y1 + dy * f, 1.5f, col );
	}
}

// A box is four rects: it uses four dots of the budget.
void LE_Box( float x, float y, float half, const vec4_t col )
{
	if ( s_dotBudget - s_dotFloor < 4 )
	{
		return;
	}
	s_dotBudget -= 4;
	CG_FillRect( x - half, y - half, half * 2, 1.0f, col );
	CG_FillRect( x - half, y + half - 1.0f, half * 2, 1.0f, col );
	CG_FillRect( x - half, y - half, 1.0f, half * 2, col );
	CG_FillRect( x + half - 1.0f, y - half, 1.0f, half * 2, col );
}

// Draws a circle of 3D points as projected segments.
void LE_Circle3D( const vec3_t center, const vec3_t ax1, const vec3_t ax2, float radius, int points, const vec4_t col )
{
	float		px = 0, py = 0;
	qboolean	havePrev = qfalse;

	for ( int i = 0; i <= points; i++ )
	{
		const float	a = (float)i * ( 2.0f * M_PI / points );
		vec3_t		p;
		float		x, y;

		VectorMA( center, cosf( a ) * radius, ax1, p );
		VectorMA( p, sinf( a ) * radius, ax2, p );
		if ( CG_WorldCoordToScreenCoordFloat( p, &x, &y ) )
		{
			if ( havePrev )
			{
				LE_Line( px, py, x, y, col );
			}
			px = x;
			py = y;
			havePrev = qtrue;
		}
		else
		{
			havePrev = qfalse;
		}
	}
}

static const float *LE_IconColor( const rtxLightDesc_t *d )
{
	if ( d->flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_COMPACTED ) )
	{
		return colRedDim;
	}
	if ( d->flags & RTX_LFLAG_IN_SOLID )
	{
		return colRed;
	}
	if ( d->flags & RTX_LFLAG_MODIFIED )
	{
		return colOrange;
	}
	if ( d->source == RTX_LSRC_EMISSIVE )
	{
		return colEmissive;
	}
	if ( d->source == RTX_LSRC_ENTITY )
	{
		return colBlue;
	}
	if ( d->source == RTX_LSRC_LGT )
	{
		return colYellow;
	}
	return colGreen;
}

static void LE_DrawIcon( size_t i )
{
	const ledRec_t	&r = s_recs[i];

	if ( !r.valid || !r.onScreen || ( r.occluded && !ledit_xray.integer ) )
	{
		return;
	}
	if ( !LE_FilterShows( &r.d ) && !LE_SelHas( (int)i ) && (int)i != s_pickId )
	{
		return;
	}
	const float	size = Com_Clamp( 3.0f, LE_IsSurface( &r.d ) ? 6.0f : 12.0f, 1200.0f / Q_max( r.depth, 1.0f ) );
	const float	*col = LE_IconColor( &r.d );

	if ( r.d.flags & RTX_LFLAG_MUTED )
	{
		// A muted light is an outline only.
		LE_Box( r.sx, r.sy, size * 0.5f + 2.0f, colGrey );
		return;
	}
	LE_Dot( r.sx, r.sy, size + 2.0f, colDark );
	LE_Dot( r.sx, r.sy, size, col );
	if ( r.d.flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_COMPACTED ) )
	{
		const float	h = size * 0.5f + 2.0f;

		LE_Line( r.sx - h, r.sy - h, r.sx + h, r.sy + h, colRed );
		LE_Line( r.sx - h, r.sy + h, r.sx + h, r.sy - h, colRed );
	}
}

typedef struct {
	float	depth;
	int		id;
} ledIconNear_t;

static bool LE_IconNearLess( const ledIconNear_t &a, const ledIconNear_t &b )
{
	return a.depth < b.depth;
}

// The lights first, then the emissive surfaces (ledit_show 2): the nearest ones, so that they cannot use the whole budget.
static void LE_DrawIcons( void )
{
	std::vector<ledIconNear_t>	surfaces;

	if ( !ledit_show.integer )
	{
		return;
	}
	for ( size_t i = 0; i < s_recs.size(); i++ )
	{
		if ( !LE_IsSurface( &s_recs[i].d ) )
		{
			LE_DrawIcon( i );
		}
		else if ( s_recs[i].valid && s_recs[i].onScreen )
		{
			ledIconNear_t	e;

			e.depth = s_recs[i].depth;
			e.id = (int)i;
			surfaces.push_back( e );
		}
	}
	if ( (int)surfaces.size() > LEDIT_SURFACE_ICONS )
	{
		std::nth_element( surfaces.begin(), surfaces.begin() + LEDIT_SURFACE_ICONS, surfaces.end(), LE_IconNearLess );
		surfaces.resize( LEDIT_SURFACE_ICONS );
	}
	for ( size_t k = 0; k < surfaces.size(); k++ )
	{
		LE_DrawIcon( (size_t)surfaces[k].id );
	}
}

static void LE_DrawSelection( void )
{
	if ( s_pickId >= 0 && s_pickId < (int)s_recs.size() && s_recs[s_pickId].valid && s_recs[s_pickId].onScreen )
	{
		const ledRec_t	&r = s_recs[s_pickId];

		LE_Box( r.sx, r.sy, 9.0f, colWhite );
	}
	if ( s_sel >= 0 && s_sel < (int)s_recs.size() && s_recs[s_sel].valid )
	{
		const ledRec_t	&r = s_recs[s_sel];
		static const vec3_t	ax[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
		const float		radius = Q_max( r.d.radius, 8.0f );

		if ( r.onScreen )
		{
			LE_Box( r.sx, r.sy, 13.0f, colWhite );
			LE_Box( r.sx, r.sy, 14.0f, colDark );
		}
		if ( r.d.type == RTX_LTYPE_SPHERE )
		{
			LE_Circle3D( r.d.origin, ax[0], ax[1], radius, 32, colCyan );
			LE_Circle3D( r.d.origin, ax[0], ax[2], radius, 32, colCyan );
			LE_Circle3D( r.d.origin, ax[1], ax[2], radius, 32, colCyan );
		}
	}
	LE_DrawSpotWires();
	LE_DrawRectWires();
	LE_SurfaceDrawWires();

	// The other selected lights get a frame only.
	for ( size_t i = 0; i < s_selList.size(); i++ )
	{
		const int	id = s_selList[i];

		if ( id == s_sel || id >= (int)s_recs.size() || !s_recs[id].valid || !s_recs[id].onScreen )
		{
			continue;
		}
		LE_Box( s_recs[id].sx, s_recs[id].sy, 12.0f, colCyan );
	}
}

static void LE_DrawToolWorld( void )
{
	if ( s_tool == LEDIT_TOOL_CREATE )
	{
		LE_Circle3D( s_ghost, cg.refdef.viewaxis[1], cg.refdef.viewaxis[2], 8.0f, 24,
			s_ghostSolid ? colRed : colGreen );
		if ( s_createType == RTX_LTYPE_RECT )
		{
			rtxLightDesc_t	ghost;

			LE_CreatePreset( &ghost );
			LE_ConvertType( &ghost, RTX_LTYPE_RECT );
			VectorCopy( s_ghost, ghost.origin );
			VectorSet( ghost.dir, 0.0f, 0.0f, -1.0f );
			if ( s_ghostHit )
			{
				VectorCopy( s_ghostNormal, ghost.dir );
			}
			LE_DrawRectWire( &ghost, s_ghostSolid ? colRed : colGreen, qfalse );
		}
		else if ( s_createType == RTX_LTYPE_SPOT )
		{
			vec3_t	dir;
			rtxLightDesc_t	preset;

			LE_CreatePreset( &preset );
			VectorSet( dir, 0.0f, 0.0f, -1.0f );
			if ( s_ghostHit )
			{
				LE_SpotCreateDir( s_ghost, s_ghostHitPos, s_ghostNormal, dir );
			}
			LE_DrawSpotWire( s_ghost, dir, preset.coneOuter, preset.coneInner, s_ghostSolid ? colRed : colGreen, qfalse );
		}
	}
	else if ( s_tool == LEDIT_TOOL_MOVE )
	{
		LE_DrawGrabWorld();
	}
}

// Top of the right panel: below the help block, so that the two never overlap.
static int s_panelTop = 90;

// Height of the numeric entry, which sits between the help block and the panel.
static int LE_EntryHeight( void )
{
	char	line[80];

	return LE_KeysEntryLine( line, sizeof( line ) ) ? 2 * LE_TextH() + 10 : 0;
}

static void LE_DrawHelp( void )
{
	const char	*name, *fire, *alt, *wheel;
	char		wheelBuf[96];
	int			y = 24;		// below the renderer name that rd-vulkan draws at the top left
	const int	h = LE_TextH();

	switch ( s_tool )
	{
	case LEDIT_TOOL_CREATE:
	{
		static char	createName[32], createAlt[96];

		Com_sprintf( createName, sizeof( createName ), "2 Create (%s)", LE_TypeName( s_createType ) );
		Com_sprintf( createAlt, sizeof( createAlt ), "new lights are %ss: switch to %ss", LE_TypeName( s_createType ),
			LE_TypeName( ( s_createType + 1 ) % ( RTX_LTYPE_RECT + 1 ) ) );
		name = createName;
		alt = createAlt;
		fire = "add a light at the ghost";
		{
			rtxLightDesc_t	preset;
			char			value[32];

			LE_CreatePreset( &preset );
			LE_PropValueText( &preset, LE_PropActive(), value, sizeof( value ) );
			Com_sprintf( wheelBuf, sizeof( wheelBuf ), "offset x2 or /2, walk +-1 (%.0f); shift: %s %s", s_createOffset,
				LE_PropName( LE_PropActive() ), value );
		}
		wheel = wheelBuf;
		break;
	}
	case LEDIT_TOOL_MOVE:
		LE_MoveHelp( &name, &fire, &alt, &wheel );
		break;
	case LEDIT_TOOL_ORIENT:
		LE_OrientHelp( &name, &fire, &alt, wheelBuf, sizeof( wheelBuf ) );
		wheel = wheelBuf;
		break;
	case LEDIT_TOOL_PROPS:
		LE_PropsHelp( &name, &fire, &alt, wheelBuf, sizeof( wheelBuf ) );
		wheel = wheelBuf;
		break;
	case LEDIT_TOOL_PIPETTE:
		LE_PipetteHelp( &name, &fire, &alt, wheelBuf, sizeof( wheelBuf ) );
		wheel = wheelBuf;
		break;
	case LEDIT_TOOL_SOLO:
		LE_SoloHelp( &name, &fire, &alt, wheelBuf, sizeof( wheelBuf ) );
		wheel = wheelBuf;
		break;
	case LEDIT_TOOL_CLONE:
		LE_CloneHelp( &name, &fire, &alt, wheelBuf, sizeof( wheelBuf ) );
		wheel = wheelBuf;
		break;
	case LEDIT_TOOL_DELETE:
		name = "8 Delete / restore";
		fire = "delete the aimed light or emissive surface, or the selection if it is a member";
		alt = s_restoreMode ? "restore the aimed light (compacted ones too)" : "restore the aimed light";
		wheel = "none";
		break;
	default:
		name = "1 Select";
		fire = "select the aimed light or emissive surface (replaces the selection; none: clear)";
		alt = "add the aimed light to the selection, or remove it";
		wheel = "cycle among the lights under the crosshair";
		break;
	}
	LE_Text( 6, y, va( "LIGHT EDIT  tool %s   (keys 1 2 3 4 5 6 7 8 9)", name ), colWhite );
	y += h + 2;
	LE_Text( 6, y, va( "Fire: %s", fire ), colGrey );
	y += h;
	LE_Text( 6, y, va( "Alt: %s", alt ), colGrey );
	y += h;
	LE_Text( 6, y, va( "Wheel: %s", wheel ), colGrey );
	y += h;
	LE_Text( 6, y, "Keys: Ctrl+Z undo  Ctrl+Y redo  Ctrl+S save  Ctrl+D deselect  Del delete  Enter value  Use: go to", colGrey );
	y += h;
	s_panelTop = y + 4;
}

#define LE_PANEL_LINES	18

typedef enum {
	PF_TITLE = 0,
	PF_SOURCE,
	PF_TYPE,
	PF_FLAGS,
	PF_ORIGIN,
	PF_COLOR,
	PF_HSV,
	PF_INTENSITY,
	PF_RADIUS,
	PF_DIR,
	PF_CONE_OUTER,
	PF_CONE_INNER,
	PF_WIDTH,
	PF_HEIGHT,
	PF_ROLL,
	PF_TWOSIDED,
	PF_STYLE,
	PF_NAME,
	PF_SHADER,
	PF_TRIS,
	PF_FACTORS
} ledPanelField_t;

// True when the field has a meaning for the light type.
static qboolean LE_PanelApplies( int type, int field )
{
	if ( type == RTX_LTYPE_SURFACE )
	{
		// An emissive surface keeps its place and its shape: it has a factor, a tint and a style.
		return (qboolean)( field != PF_RADIUS && field != PF_DIR && field != PF_CONE_OUTER && field != PF_CONE_INNER
			&& field != PF_WIDTH && field != PF_HEIGHT && field != PF_ROLL && field != PF_TWOSIDED );
	}
	switch ( field )
	{
	case PF_SHADER:
	case PF_TRIS:
	case PF_FACTORS:
		return qfalse;
	case PF_RADIUS:
		return (qboolean)( type != RTX_LTYPE_RECT );
	case PF_DIR:
		return (qboolean)( type != RTX_LTYPE_SPHERE );
	case PF_CONE_OUTER:
	case PF_CONE_INNER:
		return (qboolean)( type == RTX_LTYPE_SPOT );
	case PF_WIDTH:
	case PF_HEIGHT:
	case PF_ROLL:
	case PF_TWOSIDED:
		return (qboolean)( type == RTX_LTYPE_RECT );
	}
	return qtrue;
}

// Text of one panel line, "label: value".
static void LE_PanelText( const rtxLightDesc_t *d, int field, int activeProp, char *out, int size )
{
	float	h, s, v;

	switch ( field )
	{
	case PF_SOURCE:
		Com_sprintf( out, size, "source: %s", LE_SourceStr( d ) );
		break;
	case PF_TYPE:
	{
		char	shape[40];

		LE_ShapeText( d, shape, sizeof( shape ) );
		Com_sprintf( out, size, "type: %s", shape );
		break;
	}
	case PF_FLAGS:
		Com_sprintf( out, size, "flags: %s", d->flags ? LE_FlagsStr( d->flags ) : "-" );
		break;
	case PF_ORIGIN:
		Com_sprintf( out, size, "origin: %.1f %.1f %.1f", d->origin[0], d->origin[1], d->origin[2] );
		break;
	case PF_COLOR:
		Com_sprintf( out, size, "color: %.3f %.3f %.3f", d->color[0], d->color[1], d->color[2] );
		break;
	case PF_HSV:
	{
		char	hs[24], ss[24], ts[24];

		LE_ColorToHSV( d->color, &h, &s, &v );
		Com_sprintf( hs, sizeof( hs ), activeProp == LEP_HUE ? "[%.0f]" : "%.0f", h );
		Com_sprintf( ss, sizeof( ss ), activeProp == LEP_SAT ? "[%.2f]" : "%.2f", s );
		Com_sprintf( ts, sizeof( ts ), activeProp == LEP_TEMP ? "[%.0fK]" : "%.0fK", LE_ColorTemp( d->color ) );
		Com_sprintf( out, size, "hsv: hue %s sat %s temp %s", hs, ss, ts );
		break;
	}
	case PF_INTENSITY:
		Com_sprintf( out, size, d->type == RTX_LTYPE_SURFACE ? "intensity: x%.3g" : "intensity: %.2f", d->intensity );
		break;
	case PF_SHADER:
	case PF_TRIS:
	case PF_FACTORS:
		LE_SurfaceText( d, field - PF_SHADER, out, size );
		break;
	case PF_RADIUS:
		Com_sprintf( out, size, "emitter radius: %.2f", d->radius );
		break;
	case PF_DIR:
		Com_sprintf( out, size, "%s: %.3f %.3f %.3f", d->type == RTX_LTYPE_RECT ? "normal" : "direction", d->dir[0], d->dir[1], d->dir[2] );
		break;
	case PF_WIDTH:
		Com_sprintf( out, size, "width: %.4g", d->width );
		break;
	case PF_HEIGHT:
		Com_sprintf( out, size, "height: %.4g", d->height );
		break;
	case PF_ROLL:
		Com_sprintf( out, size, "roll: %.1f", d->roll );
		break;
	case PF_TWOSIDED:
		Com_sprintf( out, size, "two-sided: %s", d->twoSided ? "yes" : "no" );
		break;
	case PF_STYLE:
	{
		char	style[40];

		LE_StyleText( d->style, style, sizeof( style ) );
		Com_sprintf( out, size, "style: %s", style );
		break;
	}
	case PF_CONE_OUTER:
		Com_sprintf( out, size, "cone outer: %.1f", d->coneOuter );
		break;
	case PF_CONE_INNER:
		Com_sprintf( out, size, "cone inner: %.1f", d->coneInner );
		break;
	case PF_NAME:
		Com_sprintf( out, size, "name: %s", d->name );
		break;
	}
}

static qboolean LE_PanelEqual( const rtxLightDesc_t *a, const rtxLightDesc_t *b, int field )
{
	switch ( field )
	{
	case PF_SOURCE:			return (qboolean)( a->source == b->source );
	case PF_TYPE:			return (qboolean)( a->type == b->type );
	case PF_FLAGS:			return (qboolean)( a->flags == b->flags );
	case PF_ORIGIN:			return (qboolean)VectorCompare( a->origin, b->origin );
	case PF_COLOR:
	case PF_HSV:			return (qboolean)VectorCompare( a->color, b->color );
	case PF_INTENSITY:		return (qboolean)( a->intensity == b->intensity );
	case PF_RADIUS:			return (qboolean)( a->radius == b->radius );
	case PF_DIR:			return (qboolean)VectorCompare( a->dir, b->dir );
	case PF_CONE_OUTER:		return (qboolean)( a->coneOuter == b->coneOuter );
	case PF_CONE_INNER:		return (qboolean)( a->coneInner == b->coneInner );
	case PF_WIDTH:			return (qboolean)( a->width == b->width );
	case PF_HEIGHT:			return (qboolean)( a->height == b->height );
	case PF_ROLL:			return (qboolean)( a->roll == b->roll );
	case PF_TWOSIDED:		return (qboolean)( a->twoSided == b->twoSided );
	case PF_STYLE:			return (qboolean)( a->style == b->style );
	case PF_NAME:			return (qboolean)!strncmp( a->name, b->name, RTX_LIGHTEDIT_NAME_LEN );
	}
	return qtrue;
}

// Panel line that the active property of tool 5 belongs to, or -1.
static int LE_PanelActiveField( void )
{
	if ( s_tool != LEDIT_TOOL_PROPS )
	{
		return -1;
	}
	switch ( LE_PropActive() )
	{
	case LEP_INTENSITY:		return PF_INTENSITY;
	case LEP_HUE:
	case LEP_SAT:
	case LEP_TEMP:			return PF_HSV;
	case LEP_RADIUS:		return PF_RADIUS;
	case LEP_CONE_OUTER:	return PF_CONE_OUTER;
	case LEP_CONE_INNER:	return PF_CONE_INNER;
	case LEP_WIDTH:			return PF_WIDTH;
	case LEP_HEIGHT:		return PF_HEIGHT;
	case LEP_ROLL:			return PF_ROLL;
	case LEP_TWOSIDED:		return PF_TWOSIDED;
	case LEP_STYLE:			return PF_STYLE;
	}
	return -1;
}

// Draws the selection panel on the right.
static void LE_DrawPanel( void )
{
	if ( s_sel < 0 || s_sel >= (int)s_recs.size() || !s_recs[s_sel].valid )
	{
		// No light selected: the selected emissive shader, else the sky.
		if ( !LE_EmissiveDrawPanel( s_panelTop + LE_EntryHeight() ) )
		{
			LE_SkyDrawPanel( s_panelTop + LE_EntryHeight() );
		}
		return;
	}
	const rtxLightDesc_t	&d = s_recs[s_sel].d;
	rtxLightDesc_t			orig;
	const qboolean			multi = (qboolean)( s_selList.size() > 1 );
	const qboolean			haveOrig = (qboolean)( !multi && ( d.flags & RTX_LFLAG_MODIFIED ) && s_api->GetOriginal( s_sel, &orig ) );
	const int				activeField = LE_PanelActiveField();
	const int				activeProp = ( s_tool == LEDIT_TOOL_PROPS ) ? LE_PropActive() : -1;
	int						fields[LE_PANEL_LINES];
	char					main[LE_PANEL_LINES][128];
	char					was[LE_PANEL_LINES][128];
	const int				h = LE_TextH();
	int						n = 0, maxw = 0, y;

	memset( main, 0, sizeof( main ) );
	memset( was, 0, sizeof( was ) );

	if ( multi )
	{
		Com_sprintf( main[n], sizeof( main[0] ), "%d lights (primary %d)", (int)s_selList.size(), d.id );
	}
	else
	{
		Com_sprintf( main[n], sizeof( main[0] ), d.type == RTX_LTYPE_SURFACE ? "emissive surface %d" : "light %d", d.id );
	}
	fields[n++] = PF_TITLE;

	static const int	order[] = { PF_SOURCE, PF_TYPE, PF_FLAGS, PF_ORIGIN, PF_COLOR, PF_HSV, PF_INTENSITY, PF_RADIUS,
		PF_DIR, PF_CONE_OUTER, PF_CONE_INNER, PF_WIDTH, PF_HEIGHT, PF_ROLL, PF_TWOSIDED, PF_STYLE, PF_NAME,
		PF_SHADER, PF_TRIS, PF_FACTORS };

	for ( size_t k = 0; k < ARRAY_LEN( order ); k++ )
	{
		const int	f = order[k];

		if ( !LE_PanelApplies( d.type, f ) || ( f == PF_STYLE && d.style == 0 && f != activeField ) )
		{
			continue;
		}
		LE_PanelText( &d, f, activeProp, main[n], sizeof( main[0] ) );
		if ( haveOrig && f != PF_SOURCE && f != PF_FLAGS && f != PF_HSV && !LE_PanelEqual( &d, &orig, f )
			&& LE_PanelApplies( orig.type, f ) )
		{
			char	*colon;

			LE_PanelText( &orig, f, -1, was[n], sizeof( was[0] ) );
			colon = strchr( was[n], ':' );
			if ( colon )
			{
				memmove( was[n], colon + 2, strlen( colon + 2 ) + 1 );
			}
		}
		if ( multi )
		{
			// A property that differs between the selected lights shows "-".
			for ( size_t i = 0; i < s_selList.size(); i++ )
			{
				const int	id = s_selList[i];

				if ( id == s_sel || id >= (int)s_recs.size() || !s_recs[id].valid )
				{
					continue;
				}
				if ( !LE_PanelEqual( &s_recs[id].d, &d, f ) )
				{
					char	*colon = strchr( main[n], ':' );

					if ( colon )
					{
						Q_strncpyz( colon + 1, " -", (int)( sizeof( main[0] ) - ( colon + 1 - main[n] ) ) );
					}
					break;
				}
			}
		}
		fields[n++] = f;
	}

	for ( int i = 0; i < n; i++ )
	{
		int	w = LE_TextW( main[i] );

		if ( was[i][0] )
		{
			w += LE_TextW( was[i] ) + 10;
		}
		maxw = Q_max( maxw, w );
	}
	const int	x0 = 640 - 8 - maxw;

	y = s_panelTop + LE_EntryHeight();
	CG_FillRect( x0 - 4, y - 3, maxw + 10, n * h + 6, colPanel );
	for ( int i = 0; i < n; i++ )
	{
		int	xr = 640 - 8;

		if ( was[i][0] )
		{
			xr -= LE_TextW( was[i] );
			LE_Text( xr, y, was[i], colGrey );
			xr -= 10;
		}
		LE_Text( xr - LE_TextW( main[i] ), y, main[i], fields[i] == activeField ? colYellow : colWhite );
		y += h;
	}
	if ( s_tool == LEDIT_TOOL_PROPS )
	{
		LE_PropsDrawGauge( 640.0f - 8.0f - 120.0f, (float)y + 6.0f, 120.0f, &d );
	}
}

// Numeric entry, above the panel: two text lines on one background rectangle.
static void LE_DrawEntry( void )
{
	static const char	*hint = "Enter apply   Esc cancel   Backspace erase";
	char				line[80];
	const int			h = LE_TextH();

	if ( !LE_KeysEntryLine( line, sizeof( line ) ) )
	{
		return;
	}
	const int	w = Q_max( LE_TextW( line ), LE_TextW( hint ) );
	const int	x = 640 - 8 - w;
	const int	y = s_panelTop;

	CG_FillRect( x - 4, y - 3, w + 10, 2 * h + 6, colPanel );
	LE_Text( x, y, line, colYellow );
	LE_Text( x, y + h, hint, colGrey );
}

static void LE_DrawBottom( void )
{
	const int	h = LE_TextH();
	int			y = 480 - h - 6;
	const char	*line;
	char		warn[96];
	char		full[320];

	if ( s_stats.overfullClusters > 0 || s_stats.numInSolid > 0 )
	{
		Com_sprintf( warn, sizeof( warn ), "%s", s_stats.overfullClusters > 0 ? "cluster full  " : "" );
		if ( s_stats.numInSolid > 0 )
		{
			Q_strcat( warn, sizeof( warn ), va( "%d lights in solid", s_stats.numInSolid ) );
		}
		LE_Text( 6, y, warn, colRed );
		y -= h;
	}
	if ( cg.time < s_msgEnd )
	{
		LE_Text( 6, y - h - 2, s_msg, colWhite );
	}
	line = va( "%s | entity %d lgt %d added %d emissive %d modified %d disabled %d compacted %d | slots %d/%d | undo %d redo %d%s",
		s_stats.mapName, s_stats.numEntity, s_stats.numLgt, s_stats.numAdded, s_stats.numEmissive, s_stats.numModified,
		s_stats.numDisabled, s_stats.numCompacted, s_stats.slotsUsed, s_stats.maxLightPolys, LE_UndoDepth(), LE_RedoDepth(),
		s_stats.unsavedChanges > 0 ? " *" : "" );
	Com_sprintf( full, sizeof( full ), "%s | grid %d snap %s", line, LE_GridSize(), LE_SnapOn() ? "on" : "off" );
	if ( s_restoreMode )
	{
		Q_strcat( full, sizeof( full ), " | RESTORE MODE" );
	}
	if ( LE_SoloId() >= 0 )
	{
		Q_strcat( full, sizeof( full ), va( " | SOLO %d", LE_SoloId() ) );
	}
	if ( s_tool == LEDIT_TOOL_SOLO || Q_stricmp( LE_FilterName(), "all" ) )
	{
		Q_strcat( full, sizeof( full ), va( " | icons: %s", LE_FilterName() ) );
	}
	if ( LE_ShowMode() > 1 )
	{
		Q_strcat( full, sizeof( full ), va( " | show %d %s", LE_ShowMode(), LE_ShowModeName() ) );
	}
	LE_Text( 6, y, full, colWhite );
}

qboolean CG_LightEdit_Draw2D( void )
{
	if ( !s_active || !s_api )
	{
		return qfalse;
	}

	// The selection and the tool shapes come first: the icons must not use up their budget.
	s_dotBudget = LEDIT_DOT_BUDGET;
	s_dotFloor = 0;
	LE_DrawSelection();
	LE_DrawToolWorld();
	LE_DrawIcons();
	LE_DrawExtraLights();
	LE_EmissiveDrawHighlight();
	LE_SkyDrawSun();
	s_glyphBudget = LEDIT_GLYPH_BUDGET;
	LE_TextFloor( LEDIT_GLYPH_RESERVE );
	LE_DrawLabels();
	LE_TextFloor( 0 );

	// crosshair
	CG_FillRect( 320 - 8, 240, 5, 1, colWhite );
	CG_FillRect( 320 + 4, 240, 5, 1, colWhite );
	CG_FillRect( 320, 240 - 8, 1, 5, colWhite );
	CG_FillRect( 320, 240 + 4, 1, 5, colWhite );

	LE_DrawHelp();
	LE_DrawPanel();
	LE_DrawEntry();
	LE_DrawBottom();
	CG_DrawCenterString();
	return qtrue;
}
