// Light edit mode: state and helpers that the cg_lightedit*.cpp files share.
// Not for use outside the light edit files; the public interface is cg_lightedit.h.
#ifndef CG_LIGHTEDIT_LOCAL_H
#define CG_LIGHTEDIT_LOCAL_H

#include "../rd-common/rtx_light_edit_api.h"
#include <vector>

#define LEDIT_TOOL_SELECT		1
#define LEDIT_TOOL_CREATE		2
#define LEDIT_TOOL_MOVE			3
#define LEDIT_TOOL_ORIENT		4
#define LEDIT_TOOL_PROPS		5
#define LEDIT_TOOL_PIPETTE		6
#define LEDIT_TOOL_CLONE		7
#define LEDIT_TOOL_DELETE		8
#define LEDIT_TOOL_SOLO			9

// Shared state (cg_lightedit.cpp).
extern rtxLightEditAPI_t		*s_api;
extern int						s_sel;			// primary selected light, -1 for none
extern std::vector<int>			s_selList;		// selected lights in selection order; s_sel is a member
extern int						s_pickId;		// light under the crosshair, -1 for none
extern float					s_pickT;		// its depth along the view axis
extern int						s_tool;

// One cluster rebuild for all the Set / Add calls of a scope.
struct ledBatch
{
	ledBatch()	{ s_api->BeginBatch(); }
	~ledBatch()	{ s_api->EndBatch(); }
};

void		LE_Msg( const char *fmt, ... );
qboolean	LE_GetDesc( int id, rtxLightDesc_t *d );
qboolean	LE_DescEqual( const rtxLightDesc_t *a, const rtxLightDesc_t *b );
qboolean	LE_DoSet( int id, const rtxLightDesc_t *want, const char *label );	// Set plus one undo entry
const char	*LE_FlagsStr( int flags );							// "MODIFIED DISABLED ..."; valid until the next va call

extern qboolean CG_WorldCoordToScreenCoordFloat( vec3_t worldCoord, float *x, float *y );

// Selection.
qboolean	LE_SelHas( int id );
void		LE_SelClear( void );
void		LE_SelSet( int id );					// replaces the selection
void		LE_SelAdd( int id );					// appends, becomes primary
void		LE_SelToggle( int id );
void		LE_SelPrimary( int id );				// makes a member the primary light
void		LE_ActionTargets( std::vector<int> &out );	// aimed light's group, the aimed light, or the selection
void		LE_SetTool( int tool );

// 2D drawing within the dot budget (cg_lightedit.cpp).
void		LE_Line( float x1, float y1, float x2, float y2, const vec4_t col );
void		LE_Circle3D( const vec3_t center, const vec3_t ax1, const vec3_t ax2, float radius, int points, const vec4_t col );

// Undo groups (cg_lightedit_undo.cpp).
typedef enum {
	LEDU_SET = 0,
	LEDU_ADD,
	LEDU_REMOVE,
	LEDU_RESTORE,
	LEDU_EMISSIVE,		// emissive scale of a shader; kinds from here on are not lights
	LEDU_SKY			// sky and sun of the map
} ledUndoKind_t;

void		LE_UndoClear( void );
void		LE_UndoBegin( const char *label );		// opens a group; the pushes that follow join it
void		LE_UndoEnd( void );						// closes the group; an empty group is dropped
void		LE_UndoPush( int kind, int id, const rtxLightDesc_t *before, const rtxLightDesc_t *after, const char *label );
void		LE_UndoPushScale( int shader, float before, float after, const char *label );
void		LE_UndoPushSky( const rtxSkyDesc_t *before, const rtxSkyDesc_t *after, const char *label );
int			LE_UndoDepth( void );
int			LE_RedoDepth( void );
void		LE_UndoBeginMerge( const char *label, unsigned key );	// like Begin; joins the top group when it has this key and is under 500 ms old
void		LE_Undo( void );
void		LE_Redo( void );
void		LE_UndoHistory( void );

// Grid, snap and constraint maths (cg_lightedit_grid.cpp).
void		LE_GridInit( void );					// registers the cvars
void		LE_GridUpdate( void );					// once per frame
void		LE_SpotCreateDir( const vec3_t ghost, const vec3_t hitPos, const vec3_t normal, vec3_t dir );	// aim of a new spot over a surface hit
int			LE_GridSize( void );
qboolean	LE_SnapOn( void );
void		LE_SnapPoint( vec3_t p, int axisMask );	// rounds the axes whose bit is set; no-op when snap is off
void		LE_GridNext( void );					// cycles the grid size and prints it
void		LE_SnapToggle( void );
qboolean	LE_ClosestPointOnAxis( const vec3_t axisOrg, int axis, const vec3_t rayOrg, const vec3_t rayDir, vec3_t out );

// Tool 3 and the grab (cg_lightedit_move.cpp).
void		LE_GrabReset( void );
qboolean	LE_GrabActive( void );
int			LE_GrabPrimary( void );
void		LE_BeginGrab( int primary, const std::vector<int> &ids, float dist );
void		LE_EndGrab( void );
void		LE_ToolMove( qboolean prim, qboolean priDown, qboolean secDown, int wheel, qboolean fine );
void		LE_DrawGrabWorld( void );
void		LE_MoveHelp( const char **name, const char **fire, const char **alt, const char **wheel );

// Tool 7 (cg_lightedit_clone.cpp).
void		LE_CloneInit( void );
void		LE_ToolClone( qboolean priDown, qboolean secDown, int wheel );
void		LE_CloneHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );

// Hash of an action kind and its targets: merges the wheel changes of one gesture into one undo group.
static inline unsigned LE_HashTargets( int kind, const std::vector<int> &ids )
{
	unsigned	h = 2166136261u ^ (unsigned)kind;

	for ( size_t i = 0; i < ids.size(); i++ )
	{
		h = ( h ^ (unsigned)( ids[i] + 1 ) ) * 16777619u;
	}
	return h ? h : 1u;
}

// Overlay helpers that count against the dot budget (cg_lightedit.cpp).
void		LE_Dot( float x, float y, float size, const vec4_t col );
void		LE_Box( float x, float y, float half, const vec4_t col );
void		LE_Rect( float x, float y, float w, float h, const vec4_t col );	// one dot of the budget
int			LE_DotsLeft( void );
void		LE_DotFloor( int floorDots );						// the dots below this count stay free; 0 clears it
const rtxLightDesc_t	*LE_RecDesc( int id );						// this frame's descriptor, NULL when unknown
void		LE_CreateSetSpot( qboolean spot );					// creation type of tool 2
void		LE_CreateSetType( int type );						// creation type of tool 2: an RTX_LTYPE_*

// Aim, cones and tool 4 (cg_lightedit_spot.cpp).
qboolean	LE_CrosshairHit( vec3_t pos, vec3_t normal );		// world hit under the crosshair; qfalse for sky or nothing
void		LE_SnapDir( vec3_t dir );							// rounds yaw and pitch to ledit_angle_snap
void		LE_ConeAdd( rtxLightDesc_t *d, qboolean inner, float delta );
float		LE_AngleStep( qboolean fine );
void		LE_DrawSpotWire( const vec3_t org, const vec3_t dir, float outer, float inner, const vec4_t col, qboolean full );
void		LE_DrawSpotWires( void );							// selected spots, and the aimed one for tools 4 and 5
void		LE_ToolOrient( qboolean priDown, qboolean secDown, int wheel, qboolean fine );
void		LE_OrientHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );
void		LE_OrientInit( void );

// Tool 5 (cg_lightedit_props.cpp).
typedef enum {
	LEP_INTENSITY = 0,
	LEP_HUE,
	LEP_SAT,
	LEP_TEMP,
	LEP_RADIUS,
	LEP_CONE_OUTER,
	LEP_CONE_INNER,
	LEP_WIDTH,
	LEP_HEIGHT,
	LEP_ROLL,
	LEP_TWOSIDED,
	LEP_STYLE,
	LEP_NUM
} ledProp_t;

void		LE_PropsInit( void );
qboolean	LE_PropAdjust( rtxLightDesc_t *d, int prop, int wheel, qboolean fine );	// wheel step on a description
int			LE_CreateType( void );								// type that tool 2 makes
void		LE_CreateKeyShift( qboolean shift );				// key filter: shift state of the last key down
void		LE_PropCycle( int dir );							// invnext / invprev; skips the properties that do not apply to the primary light
qboolean	LE_PropApplies( int prop, int type );				// qfalse when the property has no meaning for the light type
int			LE_PropActive( void );
const char	*LE_PropName( int prop );
void		LE_ColorToHSV( const vec3_t rgb, float *h, float *s, float *v );
float		LE_ColorTemp( const vec3_t rgb );					// nearest blackbody temperature, in K
void		LE_ToolProps( qboolean priDown, qboolean secDown, int wheel, qboolean fine );
void		LE_PropsHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );
void		LE_PropsDrawGauge( float x, float y, float w, const rtxLightDesc_t *d );
void		LE_PropValueText( const rtxLightDesc_t *d, int prop, char *out, int size );	// current value, for the numeric entry
qboolean	LE_PropValueApply( rtxLightDesc_t *d, int prop, const float *val, int count );	// typed values

// Rectangle lights and light styles (cg_lightedit_rect.cpp).
const char	*LE_TypeName( int type );							// "sphere", "spot" or "rect"
void		LE_ShapeText( const rtxLightDesc_t *d, char *out, int size );	// type, with the size of a rect
const char	*LE_StyleName( int style );
void		LE_StyleText( int style, char *out, int size );		// "3 candle"
void		LE_ConvertType( rtxLightDesc_t *d, int type );		// sets the type and the fields it needs
float		LE_RectSizeStep( float size, int wheel, qboolean fine );
float		LE_RollStep( float roll, int wheel, qboolean fine );
void		LE_DrawRectWire( const rtxLightDesc_t *d, const vec4_t col, qboolean full );
void		LE_DrawRectWires( void );							// selected rects, and the aimed one for tools 4 and 5

// Tool 6 (cg_lightedit_pipette.cpp).
void		LE_PipetteInit( void );
qboolean	LE_PipettePreset( rtxLightDesc_t *d );				// clipboard as a preset for tool 2; qfalse when empty
void		LE_PipettePresetSet( const rtxLightDesc_t *d );		// edited preset back into the clipboard
void		LE_ToolPipette( qboolean priDown, qboolean secDown, int wheel );
void		LE_PipetteHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );

// Tool 9, mute and solo, display filter, still accumulation (cg_lightedit_solo.cpp).
void		LE_SoloInit( void );								// registers the cvar, drops the state
void		LE_SoloUpdate( void );								// once per frame
void		LE_SoloRelease( void );								// ends the solo, unmutes the lights muted here
void		LE_SoloEndForAdd( void );							// ends the solo before an Add
int			LE_SoloId( void );									// -1 for none
qboolean	LE_FilterShows( const rtxLightDesc_t *d );
const char	*LE_FilterName( void );
void		LE_ToolSolo( qboolean priDown, qboolean secDown, int wheel );
void		LE_SoloHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );
void		LE_StillAccumFrame( int buttons );
void		LE_StillAccumRestore( void );

// Keys, numeric entry, go to the selection, select by id (cg_lightedit_keys.cpp).
void		LE_Cmd_Save( void );
void		LE_Cmd_Undo( void );
void		LE_Cmd_Redo( void );
void		LE_Cmd_Delete( void );
void		LE_Cmd_Deselect( void );
void		LE_KeysInit( void );								// closes the entry
void		LE_KeysUpdate( void );								// once per frame; closes the entry when it is stale
qboolean	LE_KeysEntryLine( char *out, int size );			// text of the open entry; qfalse when none
void		LE_GotoSelection( void );							// camera in front of the selection
void		LE_CmdGoto( void );									// ledit_goto
void		LE_CmdSelect( void );								// ledit_select <id | none> [add]

float		LE_AngleSnap( void );								// ledit_angle_snap, degrees (cg_lightedit_grid.cpp)

// Text within the glyph budget (cg_lightedit.cpp).
int			LE_TextW( const char *s );
int			LE_TextH( void );
void		LE_Text( int x, int y, const char *s, const vec4_t col );	// stops when the glyph budget is used
void		LE_TextFloor( int floorGlyphs );					// the glyphs below this count stay free; 0 clears it

// This frame's records and display state (cg_lightedit.cpp).
int			LE_RecCount( void );
qboolean	LE_RecScreen( int id, float *sx, float *sy, float *depth, qboolean *occluded );	// qfalse when invalid or off screen
int			LE_ShowMode( void );								// ledit_show
qboolean	LE_XrayOn( void );

// Labels, emissive and dynamic dots, bind file (cg_lightedit_label.cpp).
void		LE_LabelInit( void );								// registers ledit_label, drops the emissive cache
void		LE_LabelUpdate( void );								// once per frame
void		LE_DrawLabels( void );
void		LE_DrawExtraLights( void );
const char	*LE_ShowModeName( void );
void		LE_CmdWriteBinds( void );							// ledit_writebinds [force]

// Emissive surfaces (records of the source RTX_LSRC_EMISSIVE) and the multiplier per shader (cg_lightedit_emissive.cpp).
void		LE_EmissiveInit( void );							// drops the selection and the caches
qboolean	LE_IsSurface( const rtxLightDesc_t *d );			// the record is an emissive surface
qboolean	LE_SurfaceInfo( int id, rtxEmissiveSurface_t *out );	// description of a surface; qfalse (zeroed) when id is not one
void		LE_SurfaceDrawWires( void );						// outline of the selected surfaces, and of the aimed one
void		LE_SurfaceText( const rtxLightDesc_t *d, int field, char *out, int size );	// panel line: field 0 shader, 1 polygons, 2 factors
void		LE_EmissiveSelect( int shader );					// selects a shader and prints it
int			LE_EmissiveSelected( void );						// selected shader, -1 for none
void		LE_EmissiveClear( void );							// no shader selected
qboolean	LE_EmissiveTool( qboolean secDown, int wheel, qboolean fine );	// tool 5 with the emissive selection; qtrue when used
qboolean	LE_EmissiveHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );
void		LE_EmissiveDrawHighlight( void );
qboolean	LE_EmissiveDrawPanel( int y );						// qfalse when no shader is selected
void		LE_CmdEmissiveList( void );							// ledit_emissive_list [filter]
void		LE_CmdEmissive( void );								// ledit_emissive <index|name> <scale>
void		LE_CmdEmissiveReset( void );						// ledit_emissive_reset <index|name|all>
void		LE_CmdEmissiveSelect( void );						// ledit_emissive_select <index|name|none|sel>
void		LE_CmdEmissiveSurfaces( void );						// ledit_emissive_surfaces [index|name]

// Sky and sun of the map (cg_lightedit_sky.cpp).
void		LE_SkyInit( void );
qboolean	LE_SkyApply( const rtxSkyDesc_t *desc );			// SetSky, or ResetSky for the mode GLOBAL; no undo entry
void		LE_SkyDrawSun( void );								// marker at the sun direction
void		LE_SkyDrawPanel( int y );							// sky lines for the panel
void		LE_CmdSky( void );									// ledit_sky [global|skybox|physical|hybrid]
void		LE_CmdSun( void );									// ledit_sun <azimuth> <elevation>
void		LE_CmdSunHere( void );
void		LE_CmdSunColor( void );								// ledit_sun_color <r> <g> <b>
void		LE_CmdSunBrightness( void );
void		LE_CmdSunAngle( void );
void		LE_CmdSkyReset( void );
void		LE_CmdSkyDiffuse( void );							// ledit_sky_diffuse <0-1>
void		LE_CmdAmbient( void );								// ledit_ambient <r> <g> <b> [intensity]
void		LE_CmdAmbientReset( void );

// Free camera (cg_lightedit_cam.cpp, declared in cg_freecam.h).
#include "cg_freecam.h"

#endif // CG_LIGHTEDIT_LOCAL_H
