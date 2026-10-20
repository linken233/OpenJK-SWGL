// Light edit and free camera (tfc) mode: game-side state.
// The body is frozen while either mode runs. The cgame reads G_LightEdit_Active(), G_FreeCam_Active() and the ledit_active cvar.

#include "g_local.h"
#include "g_lightedit.h"

extern qboolean	CheatsOk( gentity_t *ent );

#define LEDIT_FLAGS	( FL_GODMODE | FL_NOTARGET | FL_NOFORCE )

// The modes that freeze the body, as bits of s_sources.
#define FREEZE_LIGHTEDIT	0x1
#define FREEZE_FREECAM		0x2

static int		s_sources = 0;
static int		s_savedFlags = 0;
static vec3_t	s_frozenAngles;			// the body view angles while the mode runs
static short	s_rawAngles[3];			// the last command angles before the filter
static qboolean	s_haveRaw = qfalse;
static cvar_t	*s_activeCvar = NULL;

static void LightEdit_SetCvar( const char *value )
{
	if ( !s_activeCvar )
	{
		s_activeCvar = gi.cvar( "ledit_active", "0", CVAR_ROM );
	}
	gi.cvar_set( "ledit_active", value );	// cvar_set forces: it can write a ROM cvar
}

qboolean G_LightEdit_Active( void )
{
	return (qboolean)( ( s_sources & FREEZE_LIGHTEDIT ) != 0 );
}

qboolean G_FreeCam_Active( void )
{
	return (qboolean)( ( s_sources & FREEZE_FREECAM ) != 0 );
}

qboolean G_PlayerFrozen( void )
{
	return (qboolean)( s_sources != 0 );
}

void G_LightEdit_Init( void )
{
	s_sources = 0;
	s_savedFlags = 0;
	s_haveRaw = qfalse;
	LightEdit_SetCvar( "0" );
}

// The next command angles plus the delta angles give the frozen angles again, whatever the mouse did.
static void LightEdit_RestoreView( gentity_t *ent )
{
	if ( !s_haveRaw )
	{
		SetClientViewAngle( ent, s_frozenAngles );
		return;
	}
	for ( int i = 0; i < 3; i++ )
	{
		ent->client->pers.cmd_angles[i] = s_rawAngles[i];
		ent->client->ps.delta_angles[i] = ( ANGLE2SHORT( s_frozenAngles[i] ) - s_rawAngles[i] ) & 0xffff;
	}
	VectorCopy( s_frozenAngles, ent->s.angles );
	VectorCopy( s_frozenAngles, ent->client->ps.viewangles );
}

// The body freezes when the first mode starts and releases when the last one stops.
// Start the new mode before the old one stops, so that a switch does not release the body.
static void Freeze_SetSource( gentity_t *ent, int source, qboolean on )
{
	const int	before = s_sources;

	if ( on )
	{
		if ( !CheatsOk( ent ) )
		{
			return;
		}
		if ( !before )
		{
			s_savedFlags = ent->flags & LEDIT_FLAGS;
			VectorCopy( ent->client->ps.viewangles, s_frozenAngles );
			s_haveRaw = qfalse;
		}
		s_sources |= source;
		ent->flags |= LEDIT_FLAGS;
	}
	else
	{
		s_sources &= ~source;
		if ( before && !s_sources )
		{
			ent->flags = ( ent->flags & ~LEDIT_FLAGS ) | s_savedFlags;
			LightEdit_RestoreView( ent );
		}
	}
	LightEdit_SetCvar( G_LightEdit_Active() ? "1" : "0" );
}

static void LightEdit_Set( gentity_t *ent, qboolean on )
{
	Freeze_SetSource( ent, FREEZE_LIGHTEDIT, on );
	if ( G_LightEdit_Active() == on )
	{
		gi.SendServerCommand( ent - g_entities, on ? "print \"light edit ON\n\"" : "print \"light edit OFF\n\"" );
	}
}

static void FreeCam_Set( gentity_t *ent, qboolean on )
{
	Freeze_SetSource( ent, FREEZE_FREECAM, on );
	if ( G_FreeCam_Active() == on )
	{
		gi.SendServerCommand( ent - g_entities, on ? "print \"free camera ON\n\"" : "print \"free camera OFF\n\"" );
	}
}

// The cgame calls this directly. A console command would run only after the rest of the command buffer.
void G_LightEdit_SetMode( qboolean on )
{
	gentity_t *ent = &g_entities[0];

	if ( !ent->inuse || !ent->client || on == G_LightEdit_Active() )
	{
		return;
	}
	LightEdit_Set( ent, on );
}

// The cgame calls this directly, as G_LightEdit_SetMode.
void G_FreeCam_SetMode( qboolean on )
{
	gentity_t *ent = &g_entities[0];

	if ( !ent->inuse || !ent->client || on == G_FreeCam_Active() )
	{
		return;
	}
	FreeCam_Set( ent, on );
}

void G_LightEdit_Cmd_f( gentity_t *ent )
{
	qboolean	on;

	if ( !ent || !ent->client || ent->s.number != 0 )
	{
		return;
	}
	if ( gi.argc() > 1 )
	{
		on = ( atoi( gi.argv( 1 ) ) != 0 ) ? qtrue : qfalse;
	}
	else
	{
		on = G_LightEdit_Active() ? qfalse : qtrue;
	}
	if ( on == G_LightEdit_Active() )
	{
		return;
	}
	LightEdit_Set( ent, on );
	if ( on )
	{
		Freeze_SetSource( ent, FREEZE_FREECAM, qfalse );	// light edit leaves the free camera
	}
}

void G_LightEdit_FilterUcmd( gentity_t *ent, usercmd_t *ucmd )
{
	if ( !G_PlayerFrozen() || !ent || ent->s.number != 0 )
	{
		return;
	}
	// The body stays still and keeps its angles: the camera uses the move input and the raw angles.
	for ( int i = 0; i < 3; i++ )
	{
		s_rawAngles[i] = ucmd->angles[i];
	}
	s_haveRaw = qtrue;
	ucmd->forwardmove = 0;
	ucmd->rightmove = 0;
	ucmd->upmove = 0;
	ucmd->angles[PITCH] = ANGLE2SHORT( s_frozenAngles[PITCH] ) - ent->client->ps.delta_angles[PITCH];
	ucmd->angles[YAW] = ANGLE2SHORT( s_frozenAngles[YAW] ) - ent->client->ps.delta_angles[YAW];
	ucmd->buttons &= ~( BUTTON_ATTACK | BUTTON_ALT_ATTACK | BUTTON_USE_FORCE | BUTTON_FORCEGRIP
		| BUTTON_FORCE_LIGHTNING | BUTTON_FORCE_DRAIN | BUTTON_FORCE_FOCUS | BUTTON_FORCEGRASP | BUTTON_USE );
}
