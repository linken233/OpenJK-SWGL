// Light edit and free camera (tfc) mode: game-side state (the player body is frozen, harmless and untargetable; the cgame flies a free camera).
#ifndef G_LIGHTEDIT_H
#define G_LIGHTEDIT_H

qboolean	G_LightEdit_Active( void );			// light edit is on
qboolean	G_FreeCam_Active( void );			// free camera (tfc) is on
qboolean	G_PlayerFrozen( void );				// light edit or free camera is on: the body is frozen
void		G_LightEdit_Init( void );
void		G_LightEdit_SetMode( qboolean on );
void		G_FreeCam_SetMode( qboolean on );
void		G_LightEdit_Cmd_f( gentity_t *ent );
void		G_LightEdit_FilterUcmd( gentity_t *ent, usercmd_t *ucmd );

#endif // G_LIGHTEDIT_H
