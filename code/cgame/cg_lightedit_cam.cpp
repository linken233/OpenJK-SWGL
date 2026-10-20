// Free camera of light edit and of tfc (cg_freecam.cpp). The player body stays where it is; the camera flies.
// The view angles are the raw mouse angles plus the delta angles of the snapshot (the game freezes the body angles).

#include "cg_headers.h"
#include "cg_camera.h"
#include "cg_lightedit.h"
#include "cg_lightedit_local.h"

#define LEDIT_CAM_MAX_FRAME		0.1f		// seconds; limits the step after a hitch
#define LEDIT_CAM_PITCH_LIMIT	89.0f

static vmCvar_t		ledit_cam_speed;		// units per second at full move input
static int			s_camUsers = 0;			// LE_CAM_USER_* bits; the camera runs while one is set
static vec3_t		s_camOrg;
static vec3_t		s_camAngOff;			// goto turns the camera with this offset to the mouse angles
static vec3_t		s_camAng;

void LE_CamInit( void )
{
	s_camUsers = 0;
	VectorClear( s_camOrg );
	VectorClear( s_camAngOff );
	VectorClear( s_camAng );
	cgi_Cvar_Register( &ledit_cam_speed, "ledit_cam_speed", "500", CVAR_ARCHIVE );
}

// The camera starts at the view when no user was on. A second user keeps the position.
void LE_CamEnter( int user )
{
	if ( !s_camUsers )
	{
		VectorCopy( cg.refdef.vieworg, s_camOrg );
		VectorClear( s_camAngOff );
		VectorCopy( cg.refdefViewAngles, s_camAng );
	}
	s_camUsers |= user;
}

void LE_CamLeave( int user )
{
	s_camUsers &= ~user;
	if ( !s_camUsers )
	{
		VectorClear( s_camAngOff );
	}
}

qboolean LE_CamActive( void )
{
	return (qboolean)( s_camUsers != 0 && !in_camera );
}

// View angles from the mouse: the raw command angles plus the delta angles of the snapshot.
static void LE_CamMouseAngles( const usercmd_t *cmd, vec3_t out )
{
	for ( int i = 0; i < 3; i++ )
	{
		out[i] = SHORT2ANGLE( (short)( cmd->angles[i] + cg.snap->ps.delta_angles[i] ) );
	}
}

static void LE_CamClampAngles( vec3_t a )
{
	a[PITCH] = Com_Clamp( -LEDIT_CAM_PITCH_LIMIT, LEDIT_CAM_PITCH_LIMIT, AngleNormalize180( a[PITCH] ) );
	a[YAW] = AngleNormalize180( a[YAW] );
	a[ROLL] = 0.0f;
}

// Moves the camera from the move input and puts it in cg.refdef. Runs at the start of the light edit frame.
void LE_CamFrame( void )
{
	usercmd_t	cmd;
	vec3_t		mouse, axis[3], wish;
	float		dt;

	if ( !LE_CamActive() || !cg.snap )
	{
		return;
	}
	cgi_Cvar_Update( &ledit_cam_speed );

	memset( &cmd, 0, sizeof( cmd ) );
	if ( !cgi_GetUserCmd( cgi_GetCurrentCmdNumber(), &cmd ) )
	{
		memset( &cmd, 0, sizeof( cmd ) );
	}
	LE_CamMouseAngles( &cmd, mouse );
	VectorAdd( mouse, s_camAngOff, s_camAng );
	LE_CamClampAngles( s_camAng );
	AnglesToAxis( s_camAng, axis );

	// The client sends 64 while Walk is held and 127 otherwise: Walk halves the speed.
	dt = Com_Clamp( 0.0f, LEDIT_CAM_MAX_FRAME, cg.frametime * 0.001f );
	VectorScale( axis[0], cmd.forwardmove * ( 1.0f / 127.0f ), wish );
	VectorMA( wish, -cmd.rightmove * ( 1.0f / 127.0f ), axis[1], wish );	// axis[1] points left
	wish[2] += cmd.upmove * ( 1.0f / 127.0f );
	VectorMA( s_camOrg, dt * Q_max( 0.0f, ledit_cam_speed.value ), wish, s_camOrg );

	VectorCopy( s_camOrg, cg.refdef.vieworg );
	VectorCopy( s_camAng, cg.refdefViewAngles );
	AxisCopy( axis, cg.refdef.viewaxis );
	cg.refdef.viewContents = 0;
	if ( gi.totalMapContents() & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) )
	{
		cg.refdef.viewContents = CG_PointContents( cg.refdef.vieworg, -1 );
	}

	// The body keeps its frozen angles, and is drawn.
	VectorCopy( cg.snap->ps.viewangles, cg.predicted_player_state.viewangles );
	cg.renderingThirdPerson = qtrue;
}

// Puts the camera at `eye`, looking along `angles`.
void LE_CamSet( const vec3_t eye, const vec3_t angles )
{
	usercmd_t	cmd;
	vec3_t		mouse;

	if ( !LE_CamActive() || !cg.snap )
	{
		return;
	}
	memset( &cmd, 0, sizeof( cmd ) );
	cgi_GetUserCmd( cgi_GetCurrentCmdNumber(), &cmd );
	LE_CamMouseAngles( &cmd, mouse );
	VectorCopy( eye, s_camOrg );
	for ( int i = 0; i < 3; i++ )
	{
		s_camAngOff[i] = AngleNormalize180( ( i == ROLL ? 0.0f : angles[i] ) - mouse[i] );
	}
	VectorAdd( mouse, s_camAngOff, s_camAng );
	LE_CamClampAngles( s_camAng );
	VectorCopy( eye, cg.refdef.vieworg );
	VectorCopy( s_camAng, cg.refdefViewAngles );
	AnglesToAxis( s_camAng, cg.refdef.viewaxis );
}

// The server uses this point as the PVS origin (CG_GetCameraPos / CG_GetCameraAng).
qboolean CG_LightEdit_CameraView( vec3_t org, vec3_t ang )
{
	if ( !LE_CamActive() )
	{
		return qfalse;
	}
	VectorCopy( s_camOrg, org );
	VectorCopy( s_camAng, ang );
	return qtrue;
}
