// Free camera mode (tfc): the light edit camera with no light edit tools. Only the map and the entities are drawn.
// The game side is G_FreeCam_* in game/g_lightedit.cpp, which freezes the body.

#include "cg_headers.h"
#include "../game/g_lightedit.h"
#include "cg_lightedit.h"
#include "cg_freecam.h"

static qboolean	s_on = qfalse;		// the camera runs for this mode; follows the game side each frame

void CG_FreeCam_Init( void )
{
	s_on = qfalse;
}

qboolean CG_FreeCam_Active( void )
{
	return s_on;
}

// Starts the camera when the game side turns the mode on, and stops it when the game side turns the mode off.
void CG_FreeCam_Frame( void )
{
	const qboolean	gameOn = G_FreeCam_Active();

	if ( gameOn && !s_on )
	{
		s_on = qtrue;
		LE_CamEnter( LE_CAM_USER_FREECAM );
	}
	else if ( !gameOn && s_on )
	{
		s_on = qfalse;
		LE_CamLeave( LE_CAM_USER_FREECAM );
	}
	if ( !s_on || !cg.snap )
	{
		return;
	}

	if ( cg.snap->ps.stats[STAT_HEALTH] <= 0 )
	{
		G_FreeCam_SetMode( qfalse );
		s_on = qfalse;
		LE_CamLeave( LE_CAM_USER_FREECAM );
		return;
	}
	LE_CamFrame();
}

// tfc [0|1]: toggles the mode, or sets it. Entering the mode leaves light edit.
static void FreeCam_Cmd( void )
{
	qboolean	want;

	if ( CG_Argv( 1 )[0] )
	{
		want = (qboolean)( atoi( CG_Argv( 1 ) ) != 0 );
	}
	else
	{
		want = (qboolean)!G_FreeCam_Active();
	}
	if ( want == G_FreeCam_Active() )
	{
		return;
	}
	G_FreeCam_SetMode( want );
	if ( want && G_FreeCam_Active() )
	{
		CG_LightEdit_Stop();
	}
}

qboolean CG_FreeCam_ConsoleCommand( const char *cmd )
{
	if ( !Q_stricmp( cmd, "tfc" ) )
	{
		FreeCam_Cmd();
		return qtrue;
	}
	return qfalse;
}

void CG_FreeCam_InitConsoleCommands( void )
{
	cgi_AddCommand( "tfc" );
}
