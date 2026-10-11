// Light edit mode: tool 9 (solo and mute), the icon display filter and the still accumulation.
// Mute and solo are not edits: no undo, not saved. The cgame ends them when the mode ends.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>
#include <algorithm>

#define LEDIT_STILL_DELAY_MS	500
#define LEDIT_STILL_CVAR		"pt_accumulation_rendering"

typedef enum {
	LEFI_ALL = 0,
	LEFI_ADDED,
	LEFI_MODIFIED,
	LEFI_ORIGINAL,
	LEFI_DISABLED,
	LEFI_NUM
} ledIconFilter_t;

static const char	*s_filterNames[LEFI_NUM] = { "all", "added", "modified", "original", "disabled" };

static vmCvar_t			ledit_still_accum;
static std::vector<int>	s_muted;			// lights muted from here
static int				s_filter = LEFI_ALL;

static qboolean			s_accumOn = qfalse;	// this module switched the accumulation on
static char				s_accumSaved[32];	// value of the cvar before that
static qboolean			s_stillValid = qfalse;
static vec3_t			s_lastOrg, s_lastFwd, s_lastLeft;
static int				s_stillSince = 0;

/*
=================
Still accumulation
=================
*/
// Puts the accumulation cvar back to the value it had before the module changed it.
void LE_StillAccumRestore( void )
{
	if ( s_accumOn )
	{
		cgi_Cvar_Set( LEDIT_STILL_CVAR, s_accumSaved[0] ? s_accumSaved : "0" );
		s_accumOn = qfalse;
	}
	s_stillValid = qfalse;
}

static qboolean LE_SameVec( const vec3_t a, const vec3_t b )
{
	return (qboolean)( fabsf( a[0] - b[0] ) < 0.001f && fabsf( a[1] - b[1] ) < 0.001f && fabsf( a[2] - b[2] ) < 0.001f );
}

// Once per frame while the mode is active. The accumulation runs when the view and the buttons have been still for 500 ms.
void LE_StillAccumFrame( int buttons )
{
	const float		*org = cg.refdef.vieworg;
	const float		*fwd = cg.refdef.viewaxis[0];
	const float		*left = cg.refdef.viewaxis[1];
	qboolean		moved;

	if ( !ledit_still_accum.integer )
	{
		LE_StillAccumRestore();
		return;
	}
	moved = (qboolean)( !s_stillValid || !LE_SameVec( org, s_lastOrg ) || !LE_SameVec( fwd, s_lastFwd )
		|| !LE_SameVec( left, s_lastLeft ) || ( buttons & ( BUTTON_ATTACK | BUTTON_ALT_ATTACK ) ) || LE_GrabActive() );
	if ( moved )
	{
		VectorCopy( org, s_lastOrg );
		VectorCopy( fwd, s_lastFwd );
		VectorCopy( left, s_lastLeft );
		s_stillValid = qtrue;
		s_stillSince = cg.time;
		if ( s_accumOn )
		{
			cgi_Cvar_Set( LEDIT_STILL_CVAR, s_accumSaved[0] ? s_accumSaved : "0" );
			s_accumOn = qfalse;
		}
		return;
	}
	if ( !s_accumOn && cg.time - s_stillSince >= LEDIT_STILL_DELAY_MS )
	{
		s_accumSaved[0] = 0;
		gi.Cvar_VariableStringBuffer( LEDIT_STILL_CVAR, s_accumSaved, sizeof( s_accumSaved ) );
		cgi_Cvar_Set( LEDIT_STILL_CVAR, "1" );
		s_accumOn = qtrue;
	}
}

/*
=================
Mute, solo and filter
=================
*/
void LE_SoloInit( void )
{
	LE_StillAccumRestore();
	s_muted.clear();
	s_filter = LEFI_ALL;
	s_accumSaved[0] = 0;
	// A map load drops the renderer state; a solo that survives it is ended here.
	if ( s_api && s_api->IsAvailable() && s_api->GetSolo() >= 0 )
	{
		s_api->Solo( -1 );
	}
	cgi_Cvar_Register( &ledit_still_accum, "ledit_still_accum", "0", CVAR_ARCHIVE );
}

void LE_SoloUpdate( void )
{
	cgi_Cvar_Update( &ledit_still_accum );
}

int LE_SoloId( void )
{
	return ( s_api && s_api->IsAvailable() ) ? s_api->GetSolo() : -1;
}

// Ends the solo and unmutes every light that was muted here.
void LE_SoloRelease( void )
{
	if ( s_api && s_api->IsAvailable() )
	{
		if ( s_api->GetSolo() >= 0 )
		{
			s_api->Solo( -1 );
		}
		for ( size_t i = 0; i < s_muted.size(); i++ )
		{
			s_api->Mute( s_muted[i], qfalse );
		}
	}
	s_muted.clear();
}

void LE_SoloEndForAdd( void )
{
	if ( LE_SoloId() >= 0 )
	{
		s_api->Solo( -1 );
		LE_Msg( "light edit: solo ended, so that the new light shows" );
	}
}

qboolean LE_FilterShows( const rtxLightDesc_t *d )
{
	switch ( s_filter )
	{
	case LEFI_ADDED:
		return (qboolean)( d->source == RTX_LSRC_ADDED );
	case LEFI_MODIFIED:
		return (qboolean)( ( d->flags & RTX_LFLAG_MODIFIED ) != 0 );
	case LEFI_ORIGINAL:
		return (qboolean)( d->source != RTX_LSRC_ADDED && !( d->flags & RTX_LFLAG_MODIFIED ) );
	case LEFI_DISABLED:
		return (qboolean)( ( d->flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_DELETED | RTX_LFLAG_COMPACTED ) ) != 0 );
	}
	return qtrue;
}

const char *LE_FilterName( void )
{
	return s_filterNames[s_filter];
}

static void LE_MuteToggle( int id )
{
	std::vector<int>::iterator	it = std::find( s_muted.begin(), s_muted.end(), id );

	if ( it != s_muted.end() )
	{
		s_api->Mute( id, qfalse );
		s_muted.erase( it );
		LE_Msg( "light edit: light %d unmuted", id );
	}
	else if ( s_api->Mute( id, qtrue ) )
	{
		s_muted.push_back( id );
		LE_Msg( "light edit: light %d muted", id );
	}
	else
	{
		LE_Msg( "light edit: mute failed (%s)", s_api->LastError() );
	}
}

void LE_ToolSolo( qboolean priDown, qboolean secDown, int wheel )
{
	if ( wheel )
	{
		s_filter = ( ( s_filter + wheel ) % LEFI_NUM + LEFI_NUM ) % LEFI_NUM;
	}
	if ( priDown )
	{
		if ( LE_SoloId() >= 0 )
		{
			s_api->Solo( -1 );
			LE_Msg( "light edit: solo ended" );
		}
		else if ( s_pickId >= 0 )
		{
			s_api->Solo( s_pickId );
			LE_Msg( "light edit: solo on light %d", s_pickId );
		}
		else
		{
			LE_Msg( "light edit: aim at the light to solo" );
		}
	}
	if ( secDown )
	{
		if ( s_pickId >= 0 )
		{
			LE_MuteToggle( s_pickId );
		}
		else
		{
			LE_Msg( "light edit: aim at the light to mute" );
		}
	}
}

void LE_SoloHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize )
{
	*name = "9 Solo / mute";
	*fire = ( LE_SoloId() >= 0 ) ? "end the solo" : "solo the aimed light (every other light is switched off)";
	*alt = "mute or unmute the aimed light";
	Com_sprintf( wheelBuf, wheelSize, "icon filter: all / added / modified / original / disabled (now %s)", s_filterNames[s_filter] );
}
