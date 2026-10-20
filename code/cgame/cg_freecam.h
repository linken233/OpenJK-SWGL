// Free camera: the camera of light edit (cg_lightedit_cam.cpp) and the tfc mode (cg_freecam.cpp).
#ifndef CG_FREECAM_H
#define CG_FREECAM_H

// Camera users. The camera runs while any user is on.
#define LE_CAM_USER_LIGHTEDIT	0x1
#define LE_CAM_USER_FREECAM		0x2

void		LE_CamInit( void );									// registers ledit_cam_speed, drops the camera
void		LE_CamEnter( int user );							// starts at the current view when no user was on
void		LE_CamLeave( int user );
qboolean	LE_CamActive( void );								// qtrue while the camera replaces the player view
void		LE_CamFrame( void );								// moves the camera and overrides cg.refdef
void		LE_CamSet( const vec3_t eye, const vec3_t angles );	// puts the camera at eye, looking along angles

void		CG_FreeCam_Init( void );							// drops the tfc state
void		CG_FreeCam_InitConsoleCommands( void );				// cgi_AddCommand for tfc
qboolean	CG_FreeCam_ConsoleCommand( const char *cmd );		// qtrue when the command is handled
void		CG_FreeCam_Frame( void );							// once per frame, after cg.refdef is set
qboolean	CG_FreeCam_Active( void );							// qtrue while tfc runs

#endif // CG_FREECAM_H
