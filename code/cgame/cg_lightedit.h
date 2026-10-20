// Light edit mode of the RTX path tracer: cgame side (tools, picking, overlay, undo).
// The renderer API is in rd-common/rtx_light_edit_api.h. The game side is in game/g_lightedit.cpp.
#ifndef CG_LIGHTEDIT_H
#define CG_LIGHTEDIT_H

void		CG_LightEdit_Init( void );					// resets all state, gets the API, registers the cvars
void		CG_LightEdit_InitConsoleCommands( void );	// cgi_AddCommand for each command
qboolean	CG_LightEdit_ConsoleCommand( const char *cmd );	// qtrue when the command is handled
void		CG_LightEdit_Frame( void );					// once per frame, after cg.refdef is set
qboolean	CG_LightEdit_Draw2D( void );				// qtrue when the overlay replaces the HUD
qboolean	CG_LightEdit_Active( void );				// qtrue while the cgame side of the mode runs
void		CG_LightEdit_Stop( void );					// leaves the mode for another mode (tfc)
qboolean	CG_LightEdit_KeyEvent( int key, qboolean down, int mods );	// qtrue when the key is consumed (CG_KEY_EVENT)

qboolean	CG_LightEdit_CameraView( vec3_t org, vec3_t ang );	// qtrue when the camera is the server view point (CG_CAMERA_POS)

#endif // CG_LIGHTEDIT_H
