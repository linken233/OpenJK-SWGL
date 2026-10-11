// Light edit mode: keyboard events (shortcuts and numeric entry), go to the selection, select by id.
// The client offers each key event to CG_LightEdit_KeyEvent while no menu or console is open.
// The function returns qtrue only for the keys that the mode uses. Everything else follows the binds.

#include "cg_headers.h"
#include "../game/g_lightedit.h"
#include "../client/keycodes.h"
#include "../client/vmachine.h"
#include "cg_lightedit.h"
#include "cg_lightedit_local.h"

#include <vector>

#define LEDIT_ENTRY_LEN			48
#define LEDIT_ENTRY_TIMEOUT		30000
#define LEDIT_ENTRY_MAX_VALUES	3
#define LEDIT_GOTO_MIN_DIST		96.0f
#define LEDIT_GOTO_WALL_GAP		8.0f

static qboolean	s_entryOpen = qfalse;
static qboolean	s_entryFresh = qfalse;			// the text is the prefill: a typed character replaces it
static int		s_entryProp = LEP_INTENSITY;
static int		s_entryTime = 0;				// cg.time of the last key
static char		s_entryText[LEDIT_ENTRY_LEN];
static byte		s_consumed[MAX_KEYS];			// keys whose down event the mode consumed

void LE_KeysInit( void )
{
	s_entryOpen = qfalse;
	s_entryFresh = qfalse;
	s_entryTime = 0;
	s_entryText[0] = 0;
	memset( s_consumed, 0, sizeof( s_consumed ) );
}

static void LE_EntryClose( void )
{
	s_entryOpen = qfalse;
	s_entryText[0] = 0;
}

void LE_KeysUpdate( void )
{
	if ( s_entryOpen && ( !CG_LightEdit_Active() || s_selList.empty() || cg.time - s_entryTime > LEDIT_ENTRY_TIMEOUT
		|| cg.time < s_entryTime ) )
	{
		LE_EntryClose();
	}
}

// Line of the open entry, "intensity: 300_". Returns qfalse when no entry is open.
qboolean LE_KeysEntryLine( char *out, int size )
{
	if ( !s_entryOpen )
	{
		return qfalse;
	}
	Com_sprintf( out, size, "%s: %s_", LE_PropName( s_entryProp ), s_entryText );
	return qtrue;
}

static void LE_EntryOpen( void )
{
	rtxLightDesc_t	d;

	if ( s_selList.empty() || !LE_GetDesc( s_sel, &d ) )
	{
		LE_Msg( "light edit: select a light first" );
		return;
	}
	s_entryProp = ( s_tool == LEDIT_TOOL_PROPS ) ? LE_PropActive() : LEP_INTENSITY;
	LE_PropValueText( &d, s_entryProp, s_entryText, sizeof( s_entryText ) );
	s_entryFresh = qtrue;
	s_entryOpen = qtrue;
}

// Reads up to three numbers from the text. Returns the count, or -1 for a malformed text.
static int LE_EntryParse( float *val )
{
	char	*p = s_entryText;
	int		count = 0;

	for ( ;; )
	{
		char	*end;
		double	v;

		while ( *p == ' ' )
		{
			p++;
		}
		if ( !*p )
		{
			break;
		}
		if ( count >= LEDIT_ENTRY_MAX_VALUES )
		{
			return -1;
		}
		v = strtod( p, &end );
		if ( end == p || ( *end && *end != ' ' ) )
		{
			return -1;
		}
		val[count++] = (float)v;
		p = end;
	}
	return count > 0 ? count : -1;
}

// Applies the typed values to the selection through LE_DoSet: one undo group.
static void LE_EntryApply( void )
{
	float	val[LEDIT_ENTRY_MAX_VALUES];
	char	label[48];
	int		count = LE_EntryParse( val );
	int		done = 0;

	if ( count < 0 )
	{
		LE_Msg( "light edit: the value is not valid (numbers separated by spaces)" );
		return;
	}
	Com_sprintf( label, sizeof( label ), "set %s", LE_PropName( s_entryProp ) );
	{
		ledBatch	batch;

		LE_UndoBegin( label );
		for ( size_t i = 0; i < s_selList.size(); i++ )
		{
			rtxLightDesc_t	d;

			if ( !LE_GetDesc( s_selList[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) )
			{
				continue;
			}
			// The factor of a surface and the intensity of a light have different units: they do not mix.
			if ( s_entryProp == LEP_INTENSITY && LE_IsSurface( &d ) != LE_IsSurface( LE_RecDesc( s_sel ) ) )
			{
				continue;
			}
			if ( LE_PropValueApply( &d, s_entryProp, val, count ) && LE_DoSet( s_selList[i], &d, label ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: %d light%s: %s set", done, done > 1 ? "s" : "", LE_PropName( s_entryProp ) );
	}
	else
	{
		LE_Msg( "light edit: %s does not apply to the selection", LE_PropName( s_entryProp ) );
	}
	LE_EntryClose();
}

// Character that a key types into the entry, or 0.
static char LE_EntryChar( int key )
{
	if ( key >= A_0 && key <= A_9 )
	{
		return (char)( '0' + ( key - A_0 ) );
	}
	if ( key >= A_KP_0 && key <= A_KP_9 )
	{
		return (char)( '0' + ( key - A_KP_0 ) );
	}
	switch ( key )
	{
	case A_PERIOD:
	case A_KP_PERIOD:	return '.';
	case A_MINUS:
	case A_KP_MINUS:	return '-';
	case A_SPACE:		return ' ';
	}
	return 0;
}

// A down event while the entry is open. Returns qtrue when the key is consumed.
static qboolean LE_EntryKey( int key, int mods )
{
	const int	len = (int)strlen( s_entryText );
	const char	c = LE_EntryChar( key );

	if ( key == A_ESCAPE )
	{
		LE_EntryClose();
		return qtrue;
	}
	if ( mods & ( CG_KEYMOD_CTRL | CG_KEYMOD_ALT ) )
	{
		return qfalse;
	}
	if ( key == A_ENTER || key == A_KP_ENTER )
	{
		if ( !( mods & CG_KEYMOD_REPEAT ) )
		{
			LE_EntryApply();
		}
		return qtrue;
	}
	if ( key == A_BACKSPACE )
	{
		if ( len > 0 )
		{
			s_entryText[len - 1] = 0;
		}
		s_entryFresh = qfalse;
		return qtrue;
	}
	if ( c )
	{
		if ( s_entryFresh )
		{
			s_entryText[0] = 0;
			s_entryFresh = qfalse;
		}
		const int	n = (int)strlen( s_entryText );

		if ( n < (int)sizeof( s_entryText ) - 1 )
		{
			s_entryText[n] = c;
			s_entryText[n + 1] = 0;
		}
		return qtrue;
	}
	return qfalse;
}

// A down event while no entry is open. Returns qtrue when the key is consumed.
static qboolean LE_ShortcutKey( int key, int mods )
{
	const qboolean	ctrl = (qboolean)( ( mods & CG_KEYMOD_CTRL ) != 0 );
	const qboolean	shift = (qboolean)( ( mods & CG_KEYMOD_SHIFT ) != 0 );
	const qboolean	repeat = (qboolean)( ( mods & CG_KEYMOD_REPEAT ) != 0 );

	if ( mods & CG_KEYMOD_ALT )
	{
		return qfalse;
	}
	if ( ctrl )
	{
		switch ( key )
		{
		case A_CAP_Z:
		case A_LOW_Z:
			// Direct calls: a command in the buffer would wait behind a running script.
			if ( shift )
			{
				LE_Cmd_Redo();
			}
			else
			{
				LE_Cmd_Undo();
			}
			return qtrue;
		case A_CAP_Y:
		case A_LOW_Y:
			LE_Cmd_Redo();
			return qtrue;
		case A_CAP_S:
		case A_LOW_S:
			if ( !repeat )
			{
				LE_Cmd_Save();
			}
			return qtrue;
		case A_CAP_D:
		case A_LOW_D:
			if ( !repeat )
			{
				LE_Cmd_Deselect();
			}
			return qtrue;
		case A_CAP_M:
		case A_LOW_M:
			if ( !repeat )
			{
				cgi_SendConsoleCommand( "ledit_menu\n" );
			}
			return qtrue;
		}
		return qfalse;
	}
	if ( key == A_DELETE )
	{
		if ( !repeat )
		{
			LE_Cmd_Delete();
		}
		return qtrue;
	}
	if ( key == A_ENTER || key == A_KP_ENTER )
	{
		if ( !repeat )
		{
			LE_EntryOpen();
		}
		return qtrue;
	}
	return qfalse;
}

/*
=================
CG_LightEdit_KeyEvent

Called by the client for each key event while no key catcher is active.
mods is a mask of CG_KEYMOD_*. A release returns qtrue for a key whose press was consumed.
=================
*/
qboolean CG_LightEdit_KeyEvent( int key, qboolean down, int mods )
{
	qboolean	eaten;

	if ( key <= 0 || key >= MAX_KEYS )
	{
		return qfalse;
	}
	if ( down )
	{
		LE_CreateKeyShift( (qboolean)( ( mods & CG_KEYMOD_SHIFT ) != 0 ) );
	}
	if ( !down )
	{
		eaten = (qboolean)s_consumed[key];
		s_consumed[key] = 0;
		return (qboolean)( eaten && CG_LightEdit_Active() );
	}
	if ( !CG_LightEdit_Active() || !s_api || !cg.snap )
	{
		LE_EntryClose();
		return qfalse;
	}
	LE_KeysUpdate();
	eaten = s_entryOpen ? LE_EntryKey( key, mods ) : LE_ShortcutKey( key, mods );
	if ( eaten )
	{
		s_entryTime = cg.time;
		s_consumed[key] = 1;
	}
	return eaten;
}

/*
=================
Go to the selection
=================
*/
// The camera goes in front of the selection, on the side where it already is, and looks at its centre.
void LE_GotoSelection( void )
{
	vec3_t		mins, maxs, centre, dir, want, eye, look, angles;
	float		radius = 0.0f, dist, len;
	int			found = 0;
	trace_t		tr;

	for ( size_t i = 0; i < s_selList.size(); i++ )
	{
		rtxLightDesc_t	d;

		if ( !LE_GetDesc( s_selList[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) )
		{
			continue;
		}
		if ( !found )
		{
			VectorCopy( d.origin, mins );
			VectorCopy( d.origin, maxs );
		}
		for ( int k = 0; k < 3; k++ )
		{
			mins[k] = Q_min( mins[k], d.origin[k] );
			maxs[k] = Q_max( maxs[k], d.origin[k] );
		}
		found++;
	}
	if ( !found )
	{
		LE_Msg( "light edit: nothing selected" );
		return;
	}
	VectorAdd( mins, maxs, centre );
	VectorScale( centre, 0.5f, centre );
	for ( size_t i = 0; i < s_selList.size(); i++ )
	{
		rtxLightDesc_t	d;
		vec3_t			rel;

		if ( !LE_GetDesc( s_selList[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) )
		{
			continue;
		}
		VectorSubtract( d.origin, centre, rel );
		radius = Q_max( radius, VectorLength( rel ) + d.radius );
	}
	dist = Q_max( LEDIT_GOTO_MIN_DIST, 3.0f * radius );

	VectorSubtract( cg.refdef.vieworg, centre, dir );
	if ( VectorNormalize( dir ) < 1.0f )
	{
		VectorNegate( cg.refdef.viewaxis[0], dir );
	}
	VectorMA( centre, dist, dir, want );
	VectorCopy( want, eye );
	CG_Trace( &tr, centre, vec3_origin, vec3_origin, want, cg.snap->ps.clientNum, CONTENTS_SOLID );
	if ( tr.fraction < 1.0f && !tr.startsolid && !tr.allsolid )
	{
		len = Q_max( 0.0f, tr.fraction * dist - LEDIT_GOTO_WALL_GAP );
		VectorMA( centre, len, dir, eye );
	}
	VectorSubtract( centre, eye, look );
	if ( VectorLengthSquared( look ) < 1.0f )
	{
		VectorNegate( dir, look );
	}
	vectoangles( look, angles );

	LE_EndGrab();
	LE_CamSet( eye, angles );
	LE_Msg( "light edit: went to %d light%s", found, found > 1 ? "s" : "" );
}

void LE_CmdGoto( void )
{
	if ( !CG_LightEdit_Active() )
	{
		LE_Msg( "light edit: not active" );
		return;
	}
	LE_GotoSelection();
}

// ledit_select <id | none> [add]
void LE_CmdSelect( void )
{
	char			arg[16], mode[16];
	rtxLightDesc_t	d;
	int				id;

	Q_strncpyz( arg, CG_Argv( 1 ), sizeof( arg ) );
	Q_strncpyz( mode, CG_Argv( 2 ), sizeof( mode ) );
	if ( !CG_LightEdit_Active() || !s_api )
	{
		LE_Msg( "light edit: not active" );
		return;
	}
	if ( !Q_stricmp( arg, "none" ) )
	{
		LE_SelClear();
		LE_Msg( "light edit: selection cleared" );
		return;
	}
	if ( !arg[0] || ( arg[0] != '-' && ( arg[0] < '0' || arg[0] > '9' ) ) )
	{
		LE_Msg( "usage: ledit_select <id | none> [add]" );
		return;
	}
	id = atoi( arg );
	if ( id < 0 || !LE_GetDesc( id, &d ) || ( d.flags & ( RTX_LFLAG_DELETED | RTX_LFLAG_COMPACTED ) ) )
	{
		LE_Msg( "light edit: no light %d", id );
		return;
	}
	if ( !Q_stricmp( mode, "add" ) )
	{
		LE_SelAdd( id );
	}
	else
	{
		LE_SelSet( id );
	}
	LE_Msg( "light edit: %d light%s selected", (int)s_selList.size(), s_selList.size() > 1 ? "s" : "" );
}
