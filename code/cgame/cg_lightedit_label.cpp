// Light edit mode: light labels, the dynamic light dots, and the proposed bind file.
// Everything drawn here goes through the dot and glyph budgets of cg_lightedit.cpp.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>
#include <algorithm>

#define LEDIT_LABEL_MAX			24
#define LEDIT_LABEL_RANGE		1024.0f
#define LEDIT_DYNAMIC_MAX		64
#define LEDIT_BINDS_FILE		"lightedit_binds.cfg"

typedef struct {
	float	d;		// distance to the eye, then the sort key
	int		i;
} ledNear_t;

static vmCvar_t				ledit_label;

static const vec4_t	colDynamic	= { 1.00f, 0.20f, 1.00f, 1.00f };
static const vec4_t	colName		= { 1.00f, 1.00f, 1.00f, 1.00f };
static const vec4_t	colValue	= { 0.70f, 0.70f, 0.70f, 1.00f };
static const vec4_t	colSelName	= { 0.30f, 1.00f, 1.00f, 1.00f };

void LE_LabelInit( void )
{
	cgi_Cvar_Register( &ledit_label, "ledit_label", "1", CVAR_ARCHIVE );
}

void LE_LabelUpdate( void )
{
	cgi_Cvar_Update( &ledit_label );
}

/*
=================
Labels
=================
*/
static bool LE_NearLess( const ledNear_t &a, const ledNear_t &b )
{
	return a.d < b.d;
}

static void LE_LabelStrings( const rtxLightDesc_t *d, char *name, int nameSize, char *value, int valueSize )
{
	if ( d->name[0] )
	{
		Q_strncpyz( name, d->name, nameSize );
	}
	else
	{
		Com_sprintf( name, nameSize, "#%d", d->id );
	}
	char	shape[40];

	LE_ShapeText( d, shape, sizeof( shape ) );
	Com_sprintf( value, valueSize, d->type == RTX_LTYPE_SURFACE ? "%s x%g" : "%s %g", shape, d->intensity );
	if ( d->style )
	{
		char	style[40];

		LE_StyleText( d->style, style, sizeof( style ) );
		Q_strcat( value, valueSize, " style " );
		Q_strcat( value, valueSize, style );
	}
	if ( d->flags & RTX_LFLAG_MUTED )
	{
		Q_strcat( value, valueSize, " muted" );
	}
	if ( d->flags & RTX_LFLAG_COMPACTED )
	{
		Q_strcat( value, valueSize, " compacted" );
	}
	else if ( d->flags & RTX_LFLAG_DISABLED )
	{
		Q_strcat( value, valueSize, " disabled" );
	}
}

// Draws the labels of the aimed light, the selected lights and, in mode 2, the visible lights near the eye.
void LE_DrawLabels( void )
{
	const int	mode = ledit_label.integer;
	const int	n = LE_RecCount();
	const int	h = LE_TextH();
	std::vector<int>	ids;
	std::vector<ledNear_t>	near_;
	std::vector<float>	placed;		// x1 y1 x2 y2 of the drawn labels
	int			priority;

	if ( mode <= 0 || LE_ShowMode() <= 0 )
	{
		return;
	}

	if ( LE_RecDesc( s_pickId ) )
	{
		ids.push_back( s_pickId );
	}
	for ( size_t i = 0; i < s_selList.size(); i++ )
	{
		const int	id = s_selList[i];

		if ( LE_RecDesc( id ) && std::find( ids.begin(), ids.end(), id ) == ids.end() )
		{
			ids.push_back( id );
		}
	}
	priority = (int)ids.size();

	if ( mode >= 2 )
	{
		for ( int i = 0; i < n; i++ )
		{
			const rtxLightDesc_t	*d = LE_RecDesc( i );
			float					sx, sy, depth;
			qboolean				occluded;
			ledNear_t				e;

			if ( !d || !LE_RecScreen( i, &sx, &sy, &depth, &occluded ) || ( occluded && !LE_XrayOn() ) )
			{
				continue;
			}
			if ( !LE_FilterShows( d ) || std::find( ids.begin(), ids.end(), i ) != ids.end() )
			{
				continue;
			}
			vec3_t	rel;

			VectorSubtract( d->origin, cg.refdef.vieworg, rel );
			e.d = VectorLength( rel );
			e.i = i;
			if ( e.d <= LEDIT_LABEL_RANGE )
			{
				near_.push_back( e );
			}
		}
		std::sort( near_.begin(), near_.end(), LE_NearLess );
		for ( size_t i = 0; i < near_.size() && (int)ids.size() < LEDIT_LABEL_MAX; i++ )
		{
			ids.push_back( near_[i].i );
		}
	}
	if ( (int)ids.size() > LEDIT_LABEL_MAX )
	{
		ids.resize( LEDIT_LABEL_MAX );
	}

	for ( size_t k = 0; k < ids.size(); k++ )
	{
		const rtxLightDesc_t	*d = LE_RecDesc( ids[k] );
		float					sx, sy, depth;
		qboolean				occluded;
		char					name[RTX_LIGHTEDIT_NAME_LEN + 4], value[64];

		if ( !d || !LE_RecScreen( ids[k], &sx, &sy, &depth, &occluded ) )
		{
			continue;
		}
		LE_LabelStrings( d, name, sizeof( name ), value, sizeof( value ) );

		const float	size = Com_Clamp( 3.0f, 12.0f, 1200.0f / Q_max( depth, 1.0f ) );
		const float	w = (float)Q_max( LE_TextW( name ), LE_TextW( value ) );
		float		x = sx + size * 0.5f + 5.0f;
		const float	y = sy - (float)h;

		if ( x + w > 636.0f )
		{
			x = sx - size * 0.5f - 5.0f - w;
		}
		if ( x < 2.0f || y < 0.0f || y + 2.0f * h > 480.0f )
		{
			continue;
		}
		if ( (int)k >= priority )
		{
			// A near label never covers a label that is already drawn.
			qboolean	overlap = qfalse;

			for ( size_t j = 0; j + 3 < placed.size(); j += 4 )
			{
				if ( x < placed[j + 2] && x + w > placed[j] && y < placed[j + 3] && y + 2.0f * h > placed[j + 1] )
				{
					overlap = qtrue;
					break;
				}
			}
			if ( overlap )
			{
				continue;
			}
		}
		placed.push_back( x );
		placed.push_back( y );
		placed.push_back( x + w );
		placed.push_back( y + 2.0f * h );

		LE_Text( (int)x, (int)y, name, LE_SelHas( ids[k] ) ? colSelName : colName );
		LE_Text( (int)x, (int)y + h, value, colValue );
	}
}

/*
=================
Dynamic lights
=================
*/
// Small magenta dots for the dynamic lights of the last frame. The emissive surfaces are records: cg_lightedit.cpp draws them.
void LE_DrawExtraLights( void )
{
	const int	mode = LE_ShowMode();
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];

	if ( mode < 3 )
	{
		return;
	}

	vec3_t	origins[LEDIT_DYNAMIC_MAX], colors[LEDIT_DYNAMIC_MAX];
	const int	n = s_api->GetDynamic( LEDIT_DYNAMIC_MAX, origins, colors );

	for ( int i = 0; i < n && i < LEDIT_DYNAMIC_MAX && LE_DotsLeft() > 0; i++ )
	{
		vec3_t	rel;
		float	sx, sy;

		VectorSubtract( origins[i], eye, rel );
		if ( DotProduct( rel, fwd ) > 0.0f && CG_WorldCoordToScreenCoordFloat( origins[i], &sx, &sy )
			&& sx > -4 && sx < 644 && sy > -4 && sy < 484 )
		{
			LE_Dot( sx, sy, 5.0f, colDynamic );
		}
	}
}

// Text for the bottom line when the display filter is above 1.
const char *LE_ShowModeName( void )
{
	const int	mode = LE_ShowMode();

	if ( mode == 2 )
	{
		return "+emissive";
	}
	if ( mode >= 3 )
	{
		return "+emissive +dynamic";
	}
	return "";
}

/*
=================
Bind file
=================
*/
static const char	*s_bindsText =
	"// Light edit binds. Proposed file: the game never runs it.\n"
	"// Use: ledit_writebinds, then exec lightedit_binds.cfg\n"
	"// Keypad keys: the default JKA config leaves them free. Edit the keys to suit.\n"
	"// Enter and leave the mode, save.\n"
	"bind KP_PLUS \"lightedit\"\n"
	"bind KP_INS \"ledit_save\"\n"
	"// Undo and redo (Ctrl+Z and Ctrl+Y work without a bind).\n"
	"bind KP_LEFTARROW \"ledit_undo\"\n"
	"bind KP_RIGHTARROW \"ledit_redo\"\n"
	"// Selection: delete, deselect, go to, numeric menu.\n"
	"bind KP_DEL \"ledit_delete\"\n"
	"bind KP_5 \"ledit_deselect\"\n"
	"bind KP_END \"ledit_goto\"\n"
	"bind KP_MINUS \"ledit_menu\"\n"
	"// Grid, snap and x-ray.\n"
	"bind KP_HOME \"ledit_grid_next\"\n"
	"bind KP_UPARROW \"ledit_snap_toggle\"\n"
	"bind KP_PGUP \"ledit_xray_toggle\"\n"
	"// Labels: 0 none, 1 aimed and selected, 2 all near. Show: 0 none, 1 editable, 2 + emissive, 3 + dynamic.\n"
	"bind KP_DOWNARROW \"toggle ledit_label 0 1 2 0\"\n"
	"bind KP_PGDN \"toggle ledit_show 0 1 2 3 0\"\n"
	"// Sun: it comes from the direction you look.\n"
	"bind KP_SLASH \"ledit_sun_here\"\n"
	"// Property cycling (the JKA defaults).\n"
	"bind [ \"invprev\"\n"
	"bind ] \"invnext\"\n"
	"// Wheel: tool value (the JKA defaults).\n"
	"bind MWHEELUP \"weapnext\"\n"
	"bind MWHEELDOWN \"weapprev\"\n";

// ledit_writebinds [force]: writes lightedit_binds.cfg to the homepath. Never overwrites without "force".
void LE_CmdWriteBinds( void )
{
	const qboolean	force = (qboolean)!Q_stricmp( CG_Argv( 1 ), "force" );
	fileHandle_t	f = 0;
	const int		len = (int)strlen( s_bindsText );

	if ( !force )
	{
		cgi_FS_FOpenFile( LEDIT_BINDS_FILE, &f, FS_READ );
		if ( f )
		{
			cgi_FS_FCloseFile( f );
			LE_Msg( "%s exists: use ledit_writebinds force to overwrite it", LEDIT_BINDS_FILE );
			return;
		}
	}

	f = 0;
	cgi_FS_FOpenFile( LEDIT_BINDS_FILE, &f, FS_WRITE );
	if ( !f )
	{
		LE_Msg( "ledit_writebinds: cannot write %s", LEDIT_BINDS_FILE );
		return;
	}
	cgi_FS_Write( s_bindsText, len, f );
	cgi_FS_FCloseFile( f );
	LE_Msg( "%s written: exec %s to use it", LEDIT_BINDS_FILE, LEDIT_BINDS_FILE );
}
