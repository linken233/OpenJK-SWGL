/*
===========================================================================
Menu to edit the cloth and hair physics (.phys) of the player model.

The renderer owns the definition. This file keeps the lists of the menu, the
text of the group that is edited and the text area. The menu is
ui/physedit.menu, it opens with the console command ph_menu.
===========================================================================
*/

#include "../server/exe_headers.h"

#include "ui_local.h"
#include "ui_shared.h"
#include "../qcommon/q_shared.h"

#include <map>
#include <string>
#include <vector>

#define FEEDER_PHYS_UNUSED		0x30
#define FEEDER_PHYS_USED		0x31
#define FEEDER_PHYS_BONES		0x32
#define FEEDER_PHYS_COLLIDERS	0x33
#define FEEDER_PHYS_PINS		0x34

#define UI_PHYS_TEXT		4000
#define UI_PHYS_HELP		4001
#define UI_PHYS_STATUS		4002
#define UI_PHYS_SETTINGS	4003	// sliders of the values of the group
#define UI_PHYS_COLLSET		4004	// sliders of the selected collider
#define UI_PHYS_PINSET		4005	// sliders of the selected pin rule

#define PHYS_TEXT_MAX		6000
#define PHYS_REPLY_MAX		16384
#define PHYS_ROW_H			14.0f

// Flags of the "view" command of the renderer.
#define PHYS_VIEW_COLLIDERS		1
#define PHYS_VIEW_PINS			2
#define PHYS_VIEW_PARTICLES		4
#define PHYS_VIEW_MAXDIST		8

// A slider of the settings panel. Mode: 0 = both modes, 1 = cloth only, 2 = lag only.
struct physSliderDef_t
{
	const char	*key;
	const char	*label;
	float		min, max;
	int			decimals;
	int			mode;
	const char	*hint;		// shown in the status bar when the cursor is on the row
	const char	*zero;		// shown in place of the value 0, when 0 has its own meaning
};

static const physSliderDef_t s_settingSliders[] =
{
	{ "weight",		"Weight",		0.0f,	2.0f,	2, 1, "Weight of the cloth. Up: it hangs lower. Down: it is light and floats." },
	{ "stretch",	"Stretch",		0.0f,	1.0f,	2, 1, "Resistance to stretch. 1: the cloth does not stretch. Down: it is elastic." },
	{ "stiffness",	"Stiffness",	0.0f,	1.0f,	2, 1, "Resistance to folds. Up: leather. Down: silk, small folds." },
	{ "air",		"Air",			0.0f,	1.0f,	2, 0, "Effect of the air. Up: the cloth fills with air, moves slowly and flaps in the wind. 0: no air." },
	{ "hold",		"Hold",			0.0f,	1.0f,	2, 0, "Pull back to the animated shape. Up: the cloth stays on the animation. 0: off, the cloth swings freely.", "off" },
	{ "bounce",		"Bounce",		0.0f,	1.0f,	2, 0, "0: the cloth stops without a bounce. Up: it swings for a long time." },
	{ "inertia",	"Inertia",		0.0f,	1.0f,	2, 1, "Up: the cloth trails behind when the character starts, stops or turns. Down: it moves with the body." },
	{ "shape",		"Shape",		0.0f,	1.0f,	2, 1, "Up: a tube (lekku, tail) stays round when it swings. 0: off.", "off" },
	{ "reach",		"Reach",		0.0f,	128.0f,	1, 0, "Largest distance from the animation, along the cloth. Up: the cloth goes far. Down: it stays near." },
	{ "across",		"Across",		0.0f,	128.0f,	1, 1, "Largest distance from the animation, across the cloth. Down: the cloth does not go into the body.", "= Reach" },
	{ "detail",		"Detail",		0.25f,	4.0f,	2, 1, "Density of the simulation mesh. Up: more folds, slower. Down: fewer folds, faster." },
	{ "subdivide",	"Subdivide",	0.0f,	2.0f,	0, 0, "Rounds the drawn mesh. 1 or 2: smoother, more vertices to draw. 0: the mesh of the model.", "off" },
	{ "simsubdivide", "Sim split",	0.0f,	2.0f,	0, 1, "Cuts each triangle of the simulation mesh in 4 per level. Up: smaller folds, slower. 0: the mesh of the model.", "off" },
};

static const physSliderDef_t s_colliderSliders[] =
{
	{ "radius",			"Radius",		0.0f,	12.0f,	1, 0, "Radius of the selected collider." },
	{ "humanoidscale",	"Body scale",	0.3f,	2.0f,	2, 0, "Size of every body collider (collide humanoid)." },
};

// The selected collider is a box: half sizes, corner radius and offset, in the bind pose axes of the model.
static const physSliderDef_t s_boxSliders[] =
{
	{ "sx",		"Size X",	0.0f,	24.0f,	1, 0, "Half size of the box along X of the model. Up: wider. Down: thinner." },
	{ "sy",		"Size Y",	0.0f,	24.0f,	1, 0, "Half size of the box along Y of the model. Up: wider. Down: thinner." },
	{ "sz",		"Size Z",	0.0f,	24.0f,	1, 0, "Half size of the box along Z of the model (up). Up: taller. Down: flatter." },
	{ "round",	"Round",	0.0f,	12.0f,	1, 0, "Radius of the round edges and corners, at most the smallest half size. 0: sharp corners." },
	{ "ox",		"Offset X",	-24.0f,	24.0f,	1, 0, "Move of the box center from the bone origin, along X of the model." },
	{ "oy",		"Offset Y",	-24.0f,	24.0f,	1, 0, "Move of the box center from the bone origin, along Y of the model." },
	{ "oz",		"Offset Z",	-24.0f,	24.0f,	1, 0, "Move of the box center from the bone origin, along Z of the model." },
};

#define PHYS_BOX_ROW_H		12.0f	// the box has more sliders: smaller rows

// The fixed and the free value of the selected pin rule, per type: gradient, radius, uv.
static const physSliderDef_t s_pinSliders[3][2] =
{
	{ { "fixed", "Fixed at",	-40.0f,	40.0f,	1, 0, "Distance along the axis where the cloth is fixed (red ring)." },
	  { "free", "Free at",		-40.0f,	40.0f,	1, 0, "Distance along the axis where the cloth is free (green ring)." } },
	{ { "fixed", "Fixed in",	0.0f,	40.0f,	1, 0, "Inside this distance from the center the cloth is fixed (red sphere)." },
	  { "free", "Free out",		0.0f,	40.0f,	1, 0, "Outside this distance from the center the cloth is free (green sphere)." } },
	{ { "fixed", "Fixed at",	0.0f,	1.0f,	2, 0, "Texture coordinate v where the cloth is fixed." },
	  { "free", "Free at",		0.0f,	1.0f,	2, 0, "Texture coordinate v where the cloth is free." } },
};

static const char *s_previewAnims[] = { "BOTH_STAND1", "BOTH_WALK1", "BOTH_RUN1", "BOTH_RUNBACK1", "BOTH_JUMP1" };

extern qboolean ItemParse_model_g2anim_go( itemDef_t *item, const char *animName );
extern qboolean ItemParse_asset_model_go( itemDef_t *item, const char *name );
extern qboolean ItemParse_model_g2skin_go( itemDef_t *item, const char *skinName );
menuDef_t	*Menus_FindByName( const char *p );

struct physUsedEntry_t
{
	int			index;		// group index in the definition
	std::string	name;
	std::string	surfaces;
};

struct physColliderEntry_t
{
	char		kind;		// m = model fit, h = humanoid, c = collide line of the text
	std::string	boneA;
	std::string	boneB;
	float		radius;		// box: corner radius
	bool		on;
	int			line;		// kind c: index among the collide lines of the text
	char		shape;		// c = capsule, s = sphere, b = box
	float		size[3];	// box: half sizes
	float		off[3];		// box: offset of the center from the bone
};

// A pin line of the text.
struct physPinEntry_t
{
	std::vector<std::string>	tok;	// pin <type> ...
	int							first;	// token of the fixed value, -1 = none (pin bones)
};

// The rectangle of a slider panel, for the mouse.
struct physSliderPanel_t
{
	float	x, y, w, h;
	float	barX, barW;
	int		rows;
	int		paintTime;	// DC->realTime of the last paint: the panel is shown
};

static struct
{
	std::string					modelPath;
	std::vector<std::string>	unused;
	std::vector<physUsedEntry_t> used;
	std::vector<std::string>	bones;			// with the indent of the hierarchy
	std::vector<std::string>	boneNames;
	int							selBone;
	int							selUnused;
	int							selUsed;			// index in used, -1 = none
	char						text[PHYS_TEXT_MAX];
	int							textLen;
	int							caret;
	int							anchor;				// other end of the selection, equal to caret when there is none
	int							caretTime;			// DC->realTime of the last key or click: the blink starts again
	bool						followCaret;		// the next paint scrolls to the caret
	bool						textDrag;			// mouse 1 is down on the text
	int							clickTime;			// for the double and triple click
	int							clickPos;
	int							clickCount;
	int							scroll;				// first line shown
	rectDef_t					textRect;
	float						textScale;
	int							textFont;
	bool						statusError;
	std::string					status;
	int							helpScroll;
	std::map<std::string, float>		values;		// of the selected group, defaults included
	std::map<std::string, float>		loadedValues;	// values when the group loads: the slider ends
	std::vector<physColliderEntry_t>	colliders;	// of the selected group
	int							selCollider;
	std::vector<physPinEntry_t>	pins;				// of the text of the selected group
	int							selPin;
	physSliderPanel_t			panels[3];			// UI_PHYS_SETTINGS, UI_PHYS_COLLSET, UI_PHYS_PINSET
	int							dragPanel;			// -1 = no drag
	int							dragRow;
	int							shownView;			// last view flags sent to the renderer
	int							shownAnim;
	int							shownSpin;
} s_phys;

// The help is the text of docs/phys-format.md, made by docs/phys/build_pk3.py.
static std::vector<std::string>	s_helpRaw;
static std::vector<std::string>	s_helpWrapped;
static float					s_helpWrapWidth = -1.0f;

static void Phys_LoadHelp( void )
{
	fileHandle_t f;
	const int len = ui.FS_FOpenFile( "ui/physhelp.txt", &f, FS_READ );

	s_helpRaw.clear();
	s_helpWrapWidth = -1.0f;

	if ( len <= 0 )
	{
		s_helpRaw.push_back( "ui/physhelp.txt is missing." );
		return;
	}

	std::string data( len, '\0' );

	ui.FS_Read( &data[0], len, f );
	ui.FS_FCloseFile( f );

	std::string line;

	for ( size_t i = 0; i < data.size(); i++ )
	{
		if ( data[i] == '\n' )
		{
			s_helpRaw.push_back( line );
			line.clear();
		}
		else if ( data[i] != '\r' )
		{
			line += data[i];
		}
	}

	if ( !line.empty() )
		s_helpRaw.push_back( line );
}

// Cuts the lines of the help to the width of the panel.
static void Phys_WrapHelp( float width, float scale, int font )
{
	if ( width == s_helpWrapWidth )
		return;

	s_helpWrapWidth = width;
	s_helpWrapped.clear();

	for ( size_t i = 0; i < s_helpRaw.size(); i++ )
	{
		const std::string &raw = s_helpRaw[i];
		size_t indentLen = 0;

		while ( indentLen < raw.size() && raw[indentLen] == ' ' )
			indentLen++;

		const std::string indent = raw.substr( 0, indentLen );
		std::string line = indent;
		size_t pos = indentLen;

		if ( raw.empty() )
		{
			s_helpWrapped.push_back( "" );
			continue;
		}

		while ( pos < raw.size() )
		{
			size_t end = raw.find( ' ', pos );

			if ( end == std::string::npos )
				end = raw.size();

			const std::string word = raw.substr( pos, end - pos );
			const std::string candidate = ( line.size() > indent.size() ) ? line + " " + word : line + word;

			if ( line.size() > indent.size() && DC->textWidth( candidate.c_str(), scale, font ) > width )
			{
				s_helpWrapped.push_back( line );
				line = indent + word;
			}
			else
			{
				line = candidate;
			}

			pos = end + 1;
		}

		s_helpWrapped.push_back( line );
	}
}

static int Phys_Cmd( const char *cmd, const char *arg, char *out, int outSize )
{
	if ( !ui.PhysCommand )
	{
		Q_strncpyz( out, "This renderer has no phys editor", outSize );
		return -1;
	}

	return ui.PhysCommand( cmd, arg, out, outSize );
}

static void Phys_InsertText( const char *text );

static void Phys_SetStatus( const char *msg, bool error )
{
	s_phys.status = msg;
	s_phys.statusError = error;
}

static void Phys_SetText( const char *text )
{
	Q_strncpyz( s_phys.text, text, sizeof( s_phys.text ) );
	s_phys.textLen = (int)strlen( s_phys.text );
	s_phys.caret = 0;
	s_phys.anchor = 0;
	s_phys.scroll = 0;
	s_phys.textDrag = false;
	s_phys.followCaret = true;
}

// Splits "a\tb\n" lines into fields.
static void Phys_Lines( const char *reply, std::vector<std::vector<std::string> > &lines )
{
	lines.clear();

	const char *p = reply;

	while ( *p )
	{
		std::vector<std::string> fields;
		std::string field;

		while ( *p && *p != '\n' )
		{
			if ( *p == '\t' )
			{
				fields.push_back( field );
				field.clear();
			}
			else
			{
				field += *p;
			}

			p++;
		}

		fields.push_back( field );
		lines.push_back( fields );

		if ( *p == '\n' )
			p++;
	}
}

static void Phys_Highlight( const char *surfaces )
{
	char reply[256];

	Phys_Cmd( "highlight", surfaces, reply, sizeof( reply ) );
}

static void Phys_RefreshLists( void )
{
	static char reply[PHYS_REPLY_MAX];
	std::vector<std::vector<std::string> > lines;

	s_phys.unused.clear();
	s_phys.used.clear();

	if ( Phys_Cmd( "surfaces", "", reply, sizeof( reply ) ) == 0 )
	{
		Phys_Lines( reply, lines );

		for ( size_t i = 0; i < lines.size(); i++ )
		{
			if ( lines[i].size() >= 2 && atoi( lines[i][1].c_str() ) < 0 )
				s_phys.unused.push_back( lines[i][0] );
		}
	}

	if ( Phys_Cmd( "groups", "", reply, sizeof( reply ) ) == 0 )
	{
		Phys_Lines( reply, lines );

		for ( size_t i = 0; i < lines.size(); i++ )
		{
			if ( lines[i].size() >= 3 )
			{
				physUsedEntry_t e;

				e.index = atoi( lines[i][0].c_str() );
				e.name = lines[i][1];
				e.surfaces = lines[i][2];
				s_phys.used.push_back( e );
			}
		}
	}

	if ( s_phys.bones.empty() && Phys_Cmd( "bones", "", reply, sizeof( reply ) ) == 0 )
	{
		Phys_Lines( reply, lines );

		for ( size_t i = 0; i < lines.size(); i++ )
		{
			if ( lines[i].size() >= 2 )
			{
				s_phys.boneNames.push_back( lines[i][0] );
				s_phys.bones.push_back( std::string( atoi( lines[i][1].c_str() ) * 2, ' ' ) + lines[i][0] );
			}
		}
	}

	if ( s_phys.selUnused >= (int)s_phys.unused.size() )
		s_phys.selUnused = -1;

	if ( s_phys.selUsed >= (int)s_phys.used.size() )
		s_phys.selUsed = -1;
}

//
// Values and colliders of the selected group
//

static int Phys_SelectedGroup( void )
{
	return ( s_phys.selUsed >= 0 && s_phys.selUsed < (int)s_phys.used.size() ) ? s_phys.used[s_phys.selUsed].index : -1;
}

static float Phys_Value( const char *key )
{
	std::map<std::string, float>::const_iterator it = s_phys.values.find( key );

	return it != s_phys.values.end() ? it->second : 0.0f;
}

// What the preview shows, from ui_ph_colliders and ui_ph_pins (0 off, 1 pins, 2 + particles, 3 + max dist).
static int Phys_ViewFlags( void )
{
	static const int pinFlags[] = { 0, PHYS_VIEW_PINS, PHYS_VIEW_PINS | PHYS_VIEW_PARTICLES, PHYS_VIEW_PINS | PHYS_VIEW_PARTICLES | PHYS_VIEW_MAXDIST };
	const int pins = (int)Cvar_VariableValue( "ui_ph_pins" );

	return ( Cvar_VariableValue( "ui_ph_colliders" ) != 0.0f ? PHYS_VIEW_COLLIDERS : 0 ) | pinFlags[( pins >= 0 && pins < 4 ) ? pins : 0];
}

// The renderer draws the view of the selected group, and the selected collider and pin rule in other colors.
static void Phys_SendView( void )
{
	char reply[64];

	Phys_Cmd( "view", va( "%d %d %d", s_phys.shownView, Phys_SelectedGroup(), s_phys.selPin ), reply, sizeof( reply ) );

	if ( s_phys.selCollider >= 0 && s_phys.selCollider < (int)s_phys.colliders.size() )
	{
		const physColliderEntry_t &c = s_phys.colliders[s_phys.selCollider];

		Phys_Cmd( "collsel", va( "%c %s %s", c.kind, c.boneA.c_str(), c.boneB.c_str() ), reply, sizeof( reply ) );
	}
	else
	{
		Phys_Cmd( "collsel", "", reply, sizeof( reply ) );
	}
}

static void Phys_LoadPins( void );

static void Phys_LoadGroupState( void )
{
	static char reply[PHYS_REPLY_MAX];
	std::vector<std::vector<std::string> > lines;
	const int group = Phys_SelectedGroup();

	s_phys.values.clear();
	s_phys.colliders.clear();
	Phys_LoadPins();

	if ( group < 0 )
	{
		s_phys.selCollider = -1;
		Phys_SendView();
		return;
	}

	const char *arg = va( "%d", group );

	if ( Phys_Cmd( "values", arg, reply, sizeof( reply ) ) == 0 )
	{
		Phys_Lines( reply, lines );

		for ( size_t i = 0; i < lines.size(); i++ )
		{
			if ( lines[i].size() >= 2 )
				s_phys.values[lines[i][0]] = (float)atof( lines[i][1].c_str() );
		}
	}

	s_phys.loadedValues = s_phys.values;

	if ( Phys_Cmd( "collist", arg, reply, sizeof( reply ) ) == 0 )
	{
		Phys_Lines( reply, lines );

		for ( size_t i = 0; i < lines.size(); i++ )
		{
			if ( lines[i].size() < 6 )
				continue;

			physColliderEntry_t c;

			c.kind = lines[i][0].empty() ? 'c' : lines[i][0][0];
			c.boneA = lines[i][1];
			c.boneB = lines[i][2];
			c.radius = (float)atof( lines[i][3].c_str() );
			c.on = atoi( lines[i][4].c_str() ) != 0;
			c.line = atoi( lines[i][5].c_str() );
			c.shape = ( lines[i].size() > 6 && !lines[i][6].empty() ) ? lines[i][6][0] : ( c.boneA == c.boneB ? 's' : 'c' );

			for ( int k = 0; k < 3; k++ )
			{
				c.size[k] = lines[i].size() > (size_t)( 7 + k ) ? (float)atof( lines[i][7 + k].c_str() ) : 0.0f;
				c.off[k] = lines[i].size() > (size_t)( 10 + k ) ? (float)atof( lines[i][10 + k].c_str() ) : 0.0f;
			}

			s_phys.colliders.push_back( c );
		}
	}

	if ( s_phys.selCollider >= (int)s_phys.colliders.size() )
		s_phys.selCollider = -1;

	Phys_SendView();
}

//
// Changes of the group text made by the sliders and the collider list
//

static void Phys_SplitText( std::vector<std::string> &lines )
{
	lines.clear();

	std::string line;

	for ( int i = 0; i < s_phys.textLen; i++ )
	{
		if ( s_phys.text[i] == '\n' )
		{
			lines.push_back( line );
			line.clear();
		}
		else if ( s_phys.text[i] != '\r' )
		{
			line += s_phys.text[i];
		}
	}

	lines.push_back( line );
}

// Replaces the text and keeps the caret and the scroll.
static void Phys_JoinText( const std::vector<std::string> &lines )
{
	std::string text;

	for ( size_t i = 0; i < lines.size(); i++ )
		text += ( i ? "\n" : "" ) + lines[i];

	const int caret = s_phys.caret;
	const int scroll = s_phys.scroll;

	Q_strncpyz( s_phys.text, text.c_str(), sizeof( s_phys.text ) );
	s_phys.textLen = (int)strlen( s_phys.text );
	s_phys.caret = caret < s_phys.textLen ? caret : s_phys.textLen;
	s_phys.anchor = s_phys.caret;
	s_phys.scroll = scroll;
}

static void Phys_Tokens( const std::string &line, std::vector<std::string> &tok )
{
	tok.clear();

	size_t i = 0;

	while ( i < line.size() )
	{
		while ( i < line.size() && ( line[i] == ' ' || line[i] == '\t' ) )
			i++;

		if ( i >= line.size() || ( line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/' ) )
			break;

		const size_t start = i;

		while ( i < line.size() && line[i] != ' ' && line[i] != '\t' )
			i++;

		tok.push_back( line.substr( start, i - start ) );
	}
}

// The pin lines of the text. The order is the order of the rules in the renderer.
static void Phys_LoadPins( void )
{
	std::vector<std::string> lines;

	s_phys.pins.clear();
	Phys_SplitText( lines );

	for ( size_t i = 0; i < lines.size(); i++ )
	{
		physPinEntry_t e;

		Phys_Tokens( lines[i], e.tok );

		if ( e.tok.size() < 2 || Q_stricmp( e.tok[0].c_str(), "pin" ) )
			continue;

		const char *type = e.tok[1].c_str();

		// pin gradient <bone> <axis> <fixed> <free> / pin radius <bone> <fixed> <free> [x y z] / pin uv <fixed> <free>
		if ( !Q_stricmp( type, "gradient" ) && e.tok.size() >= 6 )
			e.first = 4;
		else if ( !Q_stricmp( type, "radius" ) && e.tok.size() >= 5 )
			e.first = 3;
		else if ( !Q_stricmp( type, "uv" ) && e.tok.size() >= 4 )
			e.first = 2;
		else
			e.first = -1;

		s_phys.pins.push_back( e );
	}

	if ( s_phys.selPin >= (int)s_phys.pins.size() )
		s_phys.selPin = -1;
}

// Writes the tokens of the n-th pin line back to the text.
static void Phys_SetPinLine( int n, const std::vector<std::string> &tok )
{
	std::vector<std::string> lines, t;
	int count = 0;

	Phys_SplitText( lines );

	for ( size_t i = 0; i < lines.size(); i++ )
	{
		Phys_Tokens( lines[i], t );

		if ( t.size() < 2 || Q_stricmp( t[0].c_str(), "pin" ) || count++ != n )
			continue;

		std::string line = "	pin			";

		for ( size_t k = 1; k < tok.size(); k++ )
			line += ( k > 1 ? " " : "" ) + tok[k];

		lines[i] = line;
		break;
	}

	Phys_JoinText( lines );
}

static std::string Phys_Num( float v, int decimals )
{
	std::string s = va( "%.*f", decimals, v );

	if ( s.find( '.' ) != std::string::npos )
	{
		while ( s[s.size() - 1] == '0' )
			s.erase( s.size() - 1 );

		if ( s[s.size() - 1] == '.' )
			s.erase( s.size() - 1 );
	}

	return s;
}

// The kind of a collide line: humanoid, model, radius, remove, or explicit for a capsule or a sphere.
static bool Phys_IsCollide( const std::vector<std::string> &tok, const char *kind )
{
	if ( tok.size() < 2 || Q_stricmp( tok[0].c_str(), "collide" ) )
		return false;

	if ( Q_stricmp( kind, "explicit" ) )
		return !Q_stricmp( tok[1].c_str(), kind );

	return Q_stricmp( tok[1].c_str(), "humanoid" ) && Q_stricmp( tok[1].c_str(), "model" ) &&
		Q_stricmp( tok[1].c_str(), "radius" ) && Q_stricmp( tok[1].c_str(), "remove" );
}

// Index of the line before the closing brace: a new line goes there.
static int Phys_InsertLine( const std::vector<std::string> &lines )
{
	for ( int i = (int)lines.size() - 1; i >= 0; i-- )
	{
		std::vector<std::string> tok;

		Phys_Tokens( lines[i], tok );

		if ( tok.size() == 1 && tok[0] == "}" )
			return i;
	}

	return (int)lines.size();
}

// Sets "<key> <value>" in the text: replaces the line of the key, or adds one.
static void Phys_SetTextLine( const char *key, const std::string &value )
{
	std::vector<std::string> lines, tok;

	Phys_SplitText( lines );

	const std::string line = std::string( "\t" ) + key + "\t\t" + value;

	for ( size_t i = 0; i < lines.size(); i++ )
	{
		Phys_Tokens( lines[i], tok );

		if ( !tok.empty() && !Q_stricmp( tok[0].c_str(), key ) )
		{
			lines[i] = line;
			Phys_JoinText( lines );
			return;
		}
	}

	lines.insert( lines.begin() + Phys_InsertLine( lines ), line );
	Phys_JoinText( lines );
}

// Sets or removes the collide line of one kind and one bone ("" = any bone). An empty value removes it.
static void Phys_SetCollideLine( const char *kind, const char *bone, const std::string &value, bool remove )
{
	std::vector<std::string> lines, tok;
	bool found = false;

	Phys_SplitText( lines );

	for ( size_t i = 0; i < lines.size(); )
	{
		Phys_Tokens( lines[i], tok );

		if ( Phys_IsCollide( tok, kind ) && ( !bone[0] || ( tok.size() >= 3 && !Q_stricmp( tok[2].c_str(), bone ) ) ) )
		{
			if ( remove )
			{
				lines.erase( lines.begin() + i );
				continue;
			}

			lines[i] = std::string( "\tcollide\t\t" ) + kind + ( bone[0] ? std::string( " " ) + bone : "" ) + ( value.empty() ? "" : " " + value );
			found = true;
			break;
		}

		i++;
	}

	if ( !found && !remove )
		lines.insert( lines.begin() + Phys_InsertLine( lines ), std::string( "\tcollide\t\t" ) + kind + ( bone[0] ? std::string( " " ) + bone : "" ) + ( value.empty() ? "" : " " + value ) );

	Phys_JoinText( lines );
}

// The n-th explicit collide line (capsule, sphere or the short form). An empty value removes it, else it is the new radius.
static void Phys_SetExplicitLine( int n, const std::string &radius )
{
	std::vector<std::string> lines, tok;
	int count = 0;

	Phys_SplitText( lines );

	for ( size_t i = 0; i < lines.size(); i++ )
	{
		Phys_Tokens( lines[i], tok );

		if ( !Phys_IsCollide( tok, "explicit" ) )
			continue;

		if ( count++ != n )
			continue;

		if ( radius.empty() )
		{
			lines.erase( lines.begin() + i );
		}
		else
		{
			// collide capsule A B r / collide sphere A r [x y z] / collide A B r
			const size_t at = !Q_stricmp( tok[1].c_str(), "capsule" ) ? 4 : 3;

			if ( tok.size() > at )
				tok[at] = radius;

			std::string line = "\tcollide\t\t";

			for ( size_t k = 1; k < tok.size(); k++ )
				line += ( k > 1 ? " " : "" ) + tok[k];

			lines[i] = line;
		}

		break;
	}

	Phys_JoinText( lines );
}

// Writes the n-th explicit collide line as a box line with the values of the entry.
static void Phys_SetBoxLine( const physColliderEntry_t &c )
{
	std::vector<std::string> lines, tok;
	int count = 0;

	Phys_SplitText( lines );

	for ( size_t i = 0; i < lines.size(); i++ )
	{
		Phys_Tokens( lines[i], tok );

		if ( !Phys_IsCollide( tok, "explicit" ) || count++ != c.line )
			continue;

		// collide box <bone> <sx> <sy> <sz> [<round>] [<ox> <oy> <oz>]
		std::string line = "\tcollide\t\tbox " + c.boneA;
		const bool offset = c.off[0] != 0.0f || c.off[1] != 0.0f || c.off[2] != 0.0f;

		for ( int k = 0; k < 3; k++ )
			line += " " + Phys_Num( c.size[k], 1 );

		if ( offset || c.radius != 0.0f )
			line += " " + Phys_Num( c.radius, 1 );

		if ( offset )
		{
			for ( int k = 0; k < 3; k++ )
				line += " " + Phys_Num( c.off[k], 1 );
		}

		lines[i] = line;
		break;
	}

	Phys_JoinText( lines );
}

// Applies the text to the model without reading it back, so the caret stays.
static bool Phys_ApplyText( void )
{
	static char reply[PHYS_REPLY_MAX];
	const int group = Phys_SelectedGroup();

	if ( group < 0 )
		return false;

	std::string arg = va( "%d\n", group );

	arg += s_phys.text;

	if ( Phys_Cmd( "set", arg.c_str(), reply, sizeof( reply ) ) != 0 )
	{
		Phys_SetStatus( reply, true );
		return false;
	}

	Phys_SetStatus( "Applied to the model", false );
	Phys_LoadGroupState();
	return true;
}

static const physSliderDef_t &Phys_SliderDef( int panel, int index );

static void Phys_SetSlider( int panel, int row, float value )
{
	const physSliderDef_t &def = Phys_SliderDef( panel, row );
	const std::string text = Phys_Num( value, def.decimals );

	if ( panel == 2 )
	{
		if ( s_phys.selPin < 0 || s_phys.selPin >= (int)s_phys.pins.size() || s_phys.pins[s_phys.selPin].first < 0 )
			return;

		physPinEntry_t &e = s_phys.pins[s_phys.selPin];
		std::string &field = e.tok[e.first + row];

		if ( Phys_Num( (float)atof( field.c_str() ), def.decimals ) == text )
			return;

		field = text;
		Phys_SetPinLine( s_phys.selPin, e.tok );
	}
	else if ( panel == 0 )
	{
		if ( Phys_Num( Phys_Value( def.key ), def.decimals ) == text )
			return;

		s_phys.values[def.key] = (float)atof( text.c_str() );

		// One line holds both: reach <along the cloth> [<across the cloth>]
		if ( !Q_stricmp( def.key, "reach" ) || !Q_stricmp( def.key, "across" ) )
		{
			const float n = Phys_Value( "across" );

			Phys_SetTextLine( "reach", Phys_Num( Phys_Value( "reach" ), 1 ) + ( n > 0.0f ? " " + Phys_Num( n, 1 ) : "" ) );
		}
		else
		{
			Phys_SetTextLine( def.key, text );
		}
	}
	else if ( !Q_stricmp( def.key, "humanoidscale" ) )
	{
		if ( Phys_Value( "humanoid" ) == 0.0f || Phys_Num( Phys_Value( "humanoidscale" ), def.decimals ) == text )
			return;

		s_phys.values["humanoidscale"] = (float)atof( text.c_str() );
		Phys_SetCollideLine( "humanoid", "", text == "1" ? "" : text, false );
	}
	else
	{
		if ( s_phys.selCollider < 0 || s_phys.selCollider >= (int)s_phys.colliders.size() )
			return;

		physColliderEntry_t &c = s_phys.colliders[s_phys.selCollider];

		if ( c.shape == 'b' )
		{
			// Rows: size x y z, round, offset x y z.
			float *field = row < 3 ? &c.size[row] : ( row == 3 ? &c.radius : &c.off[row - 4] );

			if ( c.kind != 'c' || Phys_Num( *field, def.decimals ) == text )
				return;

			*field = (float)atof( text.c_str() );
			Phys_SetBoxLine( c );
			Phys_ApplyText();
			return;
		}

		if ( Phys_Num( c.radius, def.decimals ) == text )
			return;

		c.radius = (float)atof( text.c_str() );

		if ( c.kind == 'c' )
			Phys_SetExplicitLine( c.line, text );
		else
			Phys_SetCollideLine( "radius", c.boneA.c_str(), text, false );
	}

	Phys_ApplyText();
}

// Switches the selected collider on or off.
static void Phys_ToggleCollider( void )
{
	if ( s_phys.selCollider < 0 || s_phys.selCollider >= (int)s_phys.colliders.size() )
	{
		Phys_SetStatus( "Select a collider", true );
		return;
	}

	const physColliderEntry_t c = s_phys.colliders[s_phys.selCollider];

	if ( !c.on )
		Phys_SetCollideLine( "remove", c.boneA.c_str(), "", true );
	else if ( c.kind == 'c' )
		Phys_SetExplicitLine( c.line, "" );
	else
		Phys_SetCollideLine( "remove", c.boneA.c_str(), "", false );

	if ( Phys_ApplyText() )
	{
		if ( c.on && c.kind == 'c' )
			Phys_SetStatus( "The collide line is removed. Back drops the change.", false );
		else if ( c.on )
			Phys_SetStatus( va( "Every collider that starts on %s is off", c.boneA.c_str() ), false );
	}
}

static void Phys_SelectUsed( int index )
{
	static char reply[PHYS_REPLY_MAX];

	s_phys.selUsed = index;
	s_phys.selCollider = -1;
	s_phys.selPin = -1;

	if ( index < 0 || index >= (int)s_phys.used.size() )
	{
		Phys_SetText( "" );
		Phys_Highlight( "" );
		Phys_LoadGroupState();
		return;
	}

	char arg[16];

	Com_sprintf( arg, sizeof( arg ), "%d", s_phys.used[index].index );

	if ( Phys_Cmd( "get", arg, reply, sizeof( reply ) ) == 0 )
		Phys_SetText( reply );
	else
		Phys_SetStatus( reply, true );

	Phys_Highlight( s_phys.used[index].surfaces.c_str() );
	Phys_LoadGroupState();
}

static itemDef_t *Phys_PreviewItem( void )
{
	menuDef_t *menu = Menus_FindByName( "physEdit" );

	return menu ? Menu_FindItemByName( menu, "character" ) : NULL;
}

static const char *Phys_PreviewAnim( void )
{
	const int anim = (int)Cvar_VariableValue( "ui_ph_anim" );

	return s_previewAnims[( anim >= 0 && anim < (int)ARRAY_LEN( s_previewAnims ) ) ? anim : 0];
}

// Animation and turn speed of the preview, from ui_ph_anim and ui_ph_spin.
static void Phys_PollPreview( void )
{
	const int anim = (int)Cvar_VariableValue( "ui_ph_anim" );
	const int spin = (int)Cvar_VariableValue( "ui_ph_spin" );
	itemDef_t *item = Phys_PreviewItem();

	if ( !item || !item->typeData )
		return;

	modelDef_t *modelPtr = (modelDef_t *)item->typeData;

	if ( spin != s_phys.shownSpin )
	{
		static const int speed[] = { 0, 40, 12 };	// milliseconds per degree

		s_phys.shownSpin = spin;
		modelPtr->rotationSpeed = speed[( spin >= 0 && spin < 3 ) ? spin : 0];
	}

	if ( anim != s_phys.shownAnim && ( item->flags & ITF_G2VALID ) && item->ghoul2.size() )
	{
		s_phys.shownAnim = anim;
		ItemParse_model_g2anim_go( item, Phys_PreviewAnim() );
		DC->g2hilev_SetAnim( &item->ghoul2[0], "model_root", modelPtr->g2anim, qtrue );
	}
}

// The zoom and the move of the preview come from the character_control item (ITEM_TYPE_MODEL_CONTROL).
static void Phys_ResetPreviewView( void )
{
	itemDef_t *item = Phys_PreviewItem();

	if ( item && item->typeData )
	{
		modelDef_t *m = (modelDef_t *)item->typeData;

		m->viewZoom = 1.0f;
		m->viewPan[0] = m->viewPan[1] = 0.0f;
	}
}

static void Phys_UpdatePreview( void )
{
	itemDef_t *item = Phys_PreviewItem();

	if ( !item )
		return;

	ItemParse_model_g2anim_go( item, Phys_PreviewAnim() );
	ItemParse_asset_model_go( item, s_phys.modelPath.c_str() );
	s_phys.shownAnim = (int)Cvar_VariableValue( "ui_ph_anim" );
	s_phys.shownSpin = -1;
	Phys_ResetPreviewView();

	// The skin of the player: it sets the surfaces on and off like in the game.
	ItemParse_model_g2skin_go( item, va( "models/players/%s/|%s|%s|%s", Cvar_VariableString( "g_char_model" ),
		Cvar_VariableString( "g_char_skin_head" ), Cvar_VariableString( "g_char_skin_torso" ), Cvar_VariableString( "g_char_skin_legs" ) ) );
}

// Parses the menu file when the menu is not there. A game load resets the menus (UI_LoadMenus).
void UI_Phys_LoadMenu( void )
{
	extern void UI_ParseMenu( const char *menuFile );

	if ( !Menus_FindByName( "physEdit" ) )
		UI_ParseMenu( "ui/physedit.menu" );
}

static void Phys_Open( void )
{
	char reply[256];

	s_phys.selUnused = -1;
	s_phys.selUsed = -1;
	s_phys.selBone = -1;
	s_phys.selCollider = -1;
	s_phys.dragPanel = -1;
	s_phys.values.clear();
	s_phys.colliders.clear();
	s_phys.shownView = Phys_ViewFlags();
	s_phys.pins.clear();
	s_phys.selPin = -1;
	s_phys.bones.clear();
	s_phys.boneNames.clear();
	s_phys.helpScroll = 0;
	Phys_LoadHelp();
	s_phys.modelPath = va( "models/players/%s/model.glm", Cvar_VariableString( "g_char_model" ) );

	Phys_SetText( "" );
	Phys_UpdatePreview();

	if ( Phys_Cmd( "model", s_phys.modelPath.c_str(), reply, sizeof( reply ) ) != 0 )
	{
		Phys_SetStatus( reply, true );
		s_phys.unused.clear();
		s_phys.used.clear();
		return;
	}

	Phys_SetStatus( va( "Editing %s", s_phys.modelPath.c_str() ), false );
	Phys_RefreshLists();
	Phys_SendView();
}

// Console test: the text with | for the line breaks.
void UI_Phys_SetText( const char *text )
{
	std::string t = text;

	for ( size_t i = 0; i < t.size(); i++ )
	{
		if ( t[i] == '|' )
			t[i] = '\n';
	}

	Phys_SetText( t.c_str() );
}

qboolean UI_Phys_Script( const char *name, const char **args )
{
	static char reply[PHYS_REPLY_MAX];

	if ( !Q_stricmp( name, "physOpen" ) )
	{
		Phys_Open();
		return qtrue;
	}

	if ( !Q_stricmp( name, "physRefresh" ) )
	{
		if ( s_phys.selUsed < 0 )
		{
			Phys_SetStatus( "Select a group in Used", true );
			return qtrue;
		}

		// "<group>\n<text>"
		std::string arg = va( "%d\n", s_phys.used[s_phys.selUsed].index );

		arg += s_phys.text;

		if ( Phys_Cmd( "set", arg.c_str(), reply, sizeof( reply ) ) == 0 )
		{
			const int selCollider = s_phys.selCollider;
			const int selPin = s_phys.selPin;

			Phys_SetStatus( "Applied to the model", false );
			Phys_RefreshLists();
			Phys_SelectUsed( s_phys.selUsed );
			s_phys.selCollider = selCollider < (int)s_phys.colliders.size() ? selCollider : -1;
			s_phys.selPin = selPin < (int)s_phys.pins.size() ? selPin : -1;
			Phys_SendView();
		}
		else
		{
			Phys_SetStatus( reply, true );
		}

		return qtrue;
	}

	if ( !Q_stricmp( name, "physInsertBone" ) )
	{
		if ( s_phys.selBone < 0 || s_phys.selBone >= (int)s_phys.boneNames.size() )
		{
			Phys_SetStatus( "Select a bone in Bones", true );
			return qtrue;
		}

		const std::string &bone = s_phys.boneNames[s_phys.selBone];

		Phys_InsertText( bone.c_str() );

		Phys_SetStatus( va( "Inserted %s", bone.c_str() ), false );
		return qtrue;
	}

	if ( !Q_stricmp( name, "physAdd" ) )
	{
		if ( s_phys.selUnused < 0 || s_phys.selUnused >= (int)s_phys.unused.size() )
		{
			Phys_SetStatus( "Select a surface in Unused", true );
			return qtrue;
		}

		if ( Phys_Cmd( "add", s_phys.unused[s_phys.selUnused].c_str(), reply, sizeof( reply ) ) == 0 )
		{
			const int group = atoi( reply );

			Phys_RefreshLists();
			s_phys.selUnused = -1;

			for ( size_t i = 0; i < s_phys.used.size(); i++ )
			{
				if ( s_phys.used[i].index == group )
					Phys_SelectUsed( (int)i );
			}

			Phys_SetStatus( "Group added. Edit it and press Refresh", false );
			Cvar_Set( "ui_ph_tab", "1" );
		}
		else
		{
			Phys_SetStatus( reply, true );
		}

		return qtrue;
	}

	if ( !Q_stricmp( name, "physRemove" ) )
	{
		if ( s_phys.selUsed < 0 )
		{
			Phys_SetStatus( "Select a group in Used", true );
			return qtrue;
		}

		char arg[16];

		Com_sprintf( arg, sizeof( arg ), "%d", s_phys.used[s_phys.selUsed].index );

		if ( Phys_Cmd( "remove", arg, reply, sizeof( reply ) ) == 0 )
		{
			Phys_RefreshLists();
			Phys_SelectUsed( -1 );
			Phys_SetStatus( "Group removed", false );
		}
		else
		{
			Phys_SetStatus( reply, true );
		}

		return qtrue;
	}

	if ( !Q_stricmp( name, "physSave" ) )
	{
		if ( Phys_Cmd( "save", "", reply, sizeof( reply ) ) == 0 )
			Com_Printf( "phys: written %s\n", reply );
		else
			Com_Printf( S_COLOR_RED "phys: %s\n", reply );

		Phys_Highlight( "" );
		return qtrue;
	}

	if ( !Q_stricmp( name, "physRevert" ) )
	{
		Phys_Cmd( "revert", "", reply, sizeof( reply ) );
		Phys_Highlight( "" );
		return qtrue;
	}

	if ( !Q_stricmp( name, "physClearHighlight" ) )
	{
		Phys_Highlight( "" );
		Phys_Cmd( "bonemarks", "0", reply, sizeof( reply ) );
		Phys_Cmd( "bonesel", "-1", reply, sizeof( reply ) );
		return qtrue;
	}

	if ( !Q_stricmp( name, "physBonesOn" ) )
	{
		Phys_Cmd( "bonemarks", "1", reply, sizeof( reply ) );
		return qtrue;
	}

	if ( !Q_stricmp( name, "physResetView" ) )
	{
		Phys_ResetPreviewView();
		return qtrue;
	}

	if ( !Q_stricmp( name, "physCollToggle" ) )
	{
		Phys_ToggleCollider();
		return qtrue;
	}

	// The menu closes: the colliders and the turn of the preview stop.
	if ( !Q_stricmp( name, "physClose" ) )
	{
		itemDef_t *item = Phys_PreviewItem();

		Phys_Cmd( "view", "0 -1 -1", reply, sizeof( reply ) );
		Phys_Cmd( "collsel", "", reply, sizeof( reply ) );

		if ( item && item->typeData )
			( (modelDef_t *)item->typeData )->rotationSpeed = 0;

		s_phys.dragPanel = -1;
		return qtrue;
	}

	return qfalse;
}

//
// Feeders
//

int UI_Phys_FeederCount( float feederID )
{
	if ( feederID == FEEDER_PHYS_UNUSED )
		return (int)s_phys.unused.size();

	if ( feederID == FEEDER_PHYS_USED )
		return (int)s_phys.used.size();

	if ( feederID == FEEDER_PHYS_BONES )
		return (int)s_phys.bones.size();

	if ( feederID == FEEDER_PHYS_COLLIDERS )
		return (int)s_phys.colliders.size();

	if ( feederID == FEEDER_PHYS_PINS )
		return (int)s_phys.pins.size();

	return 0;
}

const char *UI_Phys_FeederItemText( float feederID, int index, int column )
{
	static char text[256];

	if ( feederID == FEEDER_PHYS_UNUSED && index >= 0 && index < (int)s_phys.unused.size() )
		return s_phys.unused[index].c_str();

	if ( feederID == FEEDER_PHYS_BONES && index >= 0 && index < (int)s_phys.bones.size() )
		return s_phys.bones[index].c_str();

	if ( feederID == FEEDER_PHYS_USED && index >= 0 && index < (int)s_phys.used.size() )
	{
		const physUsedEntry_t &e = s_phys.used[index];

		Com_sprintf( text, sizeof( text ), "%s  [%s]", e.name.c_str(), e.surfaces.c_str() );
		return text;
	}

	if ( feederID == FEEDER_PHYS_COLLIDERS && index >= 0 && index < (int)s_phys.colliders.size() )
	{
		const physColliderEntry_t &c = s_phys.colliders[index];
		const char *kind = c.kind == 'h' ? "body" : ( c.kind == 'm' ? "fit" : "line" );

		if ( c.shape == 'b' )
			Com_sprintf( text, sizeof( text ), "%s %s  box %sx%sx%s  %s", c.on ? "ON " : "off", c.boneA.c_str(), Phys_Num( c.size[0], 1 ).c_str(),
				Phys_Num( c.size[1], 1 ).c_str(), Phys_Num( c.size[2], 1 ).c_str(), kind );
		else if ( c.boneA == c.boneB )
			Com_sprintf( text, sizeof( text ), "%s %s  r%s  %s", c.on ? "ON " : "off", c.boneA.c_str(), Phys_Num( c.radius, 1 ).c_str(), kind );
		else
			Com_sprintf( text, sizeof( text ), "%s %s-%s  r%s  %s", c.on ? "ON " : "off", c.boneA.c_str(), c.boneB.c_str(), Phys_Num( c.radius, 1 ).c_str(), kind );

		return text;
	}

	// "gradient lower_lumbar -z  fixed 1  free 12"
	if ( feederID == FEEDER_PHYS_PINS && index >= 0 && index < (int)s_phys.pins.size() )
	{
		const physPinEntry_t &e = s_phys.pins[index];
		const size_t end = e.first < 0 ? e.tok.size() : (size_t)e.first;
		std::string s;

		for ( size_t k = 1; k < end; k++ )
			s += ( k > 1 ? " " : "" ) + e.tok[k];

		if ( e.first >= 0 )
		{
			s += "  fixed " + e.tok[e.first] + "  free " + e.tok[e.first + 1];

			for ( size_t k = e.first + 2; k < e.tok.size(); k++ )
				s += " " + e.tok[k];
		}

		Q_strncpyz( text, s.c_str(), sizeof( text ) );
		return text;
	}

	return "";
}

qboolean UI_Phys_FeederSelection( float feederID, int index )
{
	if ( feederID == FEEDER_PHYS_UNUSED )
	{
		s_phys.selUnused = index;

		if ( index >= 0 && index < (int)s_phys.unused.size() )
			Phys_Highlight( s_phys.unused[index].c_str() );

		return qtrue;
	}

	if ( feederID == FEEDER_PHYS_USED )
	{
		Phys_SelectUsed( index );
		return qtrue;
	}

	if ( feederID == FEEDER_PHYS_BONES )
	{
		char reply[64];
		char arg[16];

		s_phys.selBone = index;
		Com_sprintf( arg, sizeof( arg ), "%d", index );
		Phys_Cmd( "bonesel", arg, reply, sizeof( reply ) );
		return qtrue;
	}

	if ( feederID == FEEDER_PHYS_COLLIDERS )
	{
		s_phys.selCollider = index;
		Phys_SendView();
		return qtrue;
	}

	if ( feederID == FEEDER_PHYS_PINS )
	{
		s_phys.selPin = index;
		Phys_SendView();
		return qtrue;
	}

	return qfalse;
}

//
// Owner draws
//

static int Phys_LineHeight( void )
{
	return DC->textHeight( "Ag", s_phys.textScale, s_phys.textFont ) + 2;
}

// Start of the line, and its length.
static int Phys_LineStart( int line )
{
	int start = 0;

	for ( int l = 0; l < line && start < s_phys.textLen; l++ )
	{
		while ( start < s_phys.textLen && s_phys.text[start] != '\n' )
			start++;

		if ( start < s_phys.textLen )
			start++;
	}

	return start;
}

static int Phys_LineEnd( int start )
{
	while ( start < s_phys.textLen && s_phys.text[start] != '\n' )
		start++;

	return start;
}

static int Phys_LineOfCaret( void )
{
	int line = 0;

	for ( int i = 0; i < s_phys.caret && i < s_phys.textLen; i++ )
	{
		if ( s_phys.text[i] == '\n' )
			line++;
	}

	return line;
}

static int Phys_LineCount( void )
{
	int lines = 1;

	for ( int i = 0; i < s_phys.textLen; i++ )
	{
		if ( s_phys.text[i] == '\n' )
			lines++;
	}

	return lines;
}

// The text from start to end as it is drawn: a tab is four spaces.
static std::string Phys_Shown( int start, int end )
{
	std::string shown;

	for ( int i = start; i < end && i < s_phys.textLen; i++ )
	{
		if ( s_phys.text[i] == '\t' )
			shown += "    ";
		else
			shown += s_phys.text[i];
	}

	return shown;
}

// Width of the text from start to end. The width includes the spaces at the end of the text.
static float Phys_ShownWidth( int start, int end )
{
	return DC->textWidth( ( Phys_Shown( start, end ) + "|" ).c_str(), s_phys.textScale, s_phys.textFont ) -
		DC->textWidth( "|", s_phys.textScale, s_phys.textFont );
}

static void Phys_SelRange( int &a, int &b )
{
	a = s_phys.anchor < s_phys.caret ? s_phys.anchor : s_phys.caret;
	b = s_phys.anchor < s_phys.caret ? s_phys.caret : s_phys.anchor;

	if ( a < 0 )
		a = 0;

	if ( b > s_phys.textLen )
		b = s_phys.textLen;
}

static int Phys_PosAt( float cursorX, float cursorY );

static void Phys_PaintText( float x, float y, float w, float h, float scale, vec4_t color, int style, int font )
{
	static const vec4_t back = { 0.04f, 0.04f, 0.04f, 0.95f };
	static const vec4_t border = { 1.0f, 0.682f, 0.0f, 0.6f };
	static const vec4_t selBack = { 0.2f, 0.35f, 0.7f, 0.75f };

	s_phys.textRect.x = x;
	s_phys.textRect.y = y;
	s_phys.textRect.w = w;
	s_phys.textRect.h = h;
	s_phys.textScale = scale;
	s_phys.textFont = font;

	// A drag with mouse 1 moves the caret. The anchor stays where the click was.
	if ( s_phys.textDrag )
	{
		if ( ui.Key_IsDown( A_MOUSE1 ) )
		{
			const int pos = Phys_PosAt( DC->cursorx, DC->cursory );

			if ( pos != s_phys.caret )
			{
				s_phys.caret = pos;
				s_phys.caretTime = DC->realTime;
				s_phys.followCaret = true;
			}
		}
		else
		{
			s_phys.textDrag = false;
		}
	}

	DC->fillRect( x, y, w, h, back );
	DC->drawRect( x, y, w, h, 1, border );

	const int lineH = Phys_LineHeight();
	const int visible = lineH > 0 ? (int)( ( h - 4 ) / lineH ) : 1;
	const int lineCount = Phys_LineCount();

	// The view follows the caret after a key or a click. The mouse wheel scrolls freely.
	if ( s_phys.followCaret )
	{
		const int caretLine = Phys_LineOfCaret();

		if ( caretLine < s_phys.scroll )
			s_phys.scroll = caretLine;
		else if ( caretLine >= s_phys.scroll + visible )
			s_phys.scroll = caretLine - visible + 1;

		s_phys.followCaret = false;
	}

	if ( s_phys.scroll > lineCount - 1 )
		s_phys.scroll = lineCount - 1;

	if ( s_phys.scroll < 0 )
		s_phys.scroll = 0;

	int selA, selB;

	Phys_SelRange( selA, selB );

	const int caretLine = Phys_LineOfCaret();
	const float textH = (float)DC->textHeight( "Ag", scale, font );
	int start = Phys_LineStart( s_phys.scroll );

	for ( int row = 0; row < visible && start <= s_phys.textLen; row++ )
	{
		const int end = Phys_LineEnd( start );
		const float ty = y + 2 + row * lineH;

		// The selection behind the text. A selected line break is a space at the end of the line.
		if ( selA < selB && selA <= end && selB >= start )
		{
			const int a = selA > start ? selA : start;
			const int b = selB < end ? selB : end;
			const float x0 = Phys_ShownWidth( start, a );
			float x1 = Phys_ShownWidth( start, b );

			if ( selB > end && end < s_phys.textLen )
				x1 += DC->textWidth( " ", scale, font );

			if ( x1 > x0 )
				DC->fillRect( x + 3 + x0, ty + 1, x1 - x0, (float)lineH, selBack );
		}

		DC->drawText( x + 3, ty, scale, color, Phys_Shown( start, end ).c_str(), (int)w - 6, style, font );

		// The caret: a bar of one pixel on the pixel grid.
		if ( s_phys.scroll + row == caretLine && ( ( DC->realTime - s_phys.caretTime ) / 500 ) % 2 == 0 )
		{
			const float px = DC->xscale > 0.0f ? 1.0f / DC->xscale : 1.0f;
			const float py = DC->yscale > 0.0f ? 1.0f / DC->yscale : 1.0f;
			const int caret = s_phys.caret < end ? s_phys.caret : end;
			const float cx = floorf( ( x + 3 + Phys_ShownWidth( start, caret ) ) / px + 0.5f ) * px;
			const float cy = floorf( ( ty + 2 ) / py + 0.5f ) * py;

			DC->fillRect( cx, cy, px, floorf( textH / py + 0.5f ) * py, color );
		}

		start = end + 1;
	}
}

static void Phys_PaintHelp( float x, float y, float w, float h, float scale, vec4_t color, int style, int font )
{
	static const vec4_t back = { 0.04f, 0.04f, 0.04f, 0.9f };
	const int lineH = DC->textHeight( "Ag", scale, font ) + 2;
	const int visible = lineH > 0 ? (int)( ( h - 4 ) / lineH ) : 1;

	Phys_WrapHelp( w - 8, scale, font );

	const int count = (int)s_helpWrapped.size();

	if ( s_phys.helpScroll > count - visible )
		s_phys.helpScroll = count - visible;

	if ( s_phys.helpScroll < 0 )
		s_phys.helpScroll = 0;

	DC->fillRect( x, y, w, h, back );

	for ( int row = 0; row < visible && s_phys.helpScroll + row < count; row++ )
		DC->drawText( x + 3, y + 2 + row * lineH, scale, color, s_helpWrapped[s_phys.helpScroll + row].c_str(), 0, style, font );
}

//
// Slider panels. Panel 0 = values of the group, panel 1 = the selected collider.
//

// The type of the selected pin rule: 0 gradient, 1 radius, 2 uv, -1 no slider.
static int Phys_PinType( void )
{
	if ( s_phys.selPin < 0 || s_phys.selPin >= (int)s_phys.pins.size() || s_phys.pins[s_phys.selPin].first < 0 )
		return -1;

	const char *type = s_phys.pins[s_phys.selPin].tok[1].c_str();

	return !Q_stricmp( type, "gradient" ) ? 0 : ( !Q_stricmp( type, "radius" ) ? 1 : 2 );
}

// The selected collider is a box of a collide line.
static bool Phys_SelectedBox( void )
{
	return s_phys.selCollider >= 0 && s_phys.selCollider < (int)s_phys.colliders.size() && s_phys.colliders[s_phys.selCollider].shape == 'b';
}

static float Phys_RowH( int panel )
{
	return ( panel == 1 && Phys_SelectedBox() ) ? PHYS_BOX_ROW_H : PHYS_ROW_H;
}

static const physSliderDef_t &Phys_SliderDef( int panel, int index )
{
	if ( panel == 2 )
	{
		const int type = Phys_PinType();

		return s_pinSliders[type < 0 ? 0 : type][index];
	}

	if ( panel == 1 && Phys_SelectedBox() )
		return s_boxSliders[index];

	return panel == 0 ? s_settingSliders[index] : s_colliderSliders[index];
}

// The sliders shown: a group in lag mode has other values than a cloth.
static void Phys_SliderRows( int panel, std::vector<int> &rows )
{
	rows.clear();

	if ( panel == 1 )
	{
		const int count = Phys_SelectedBox() ? (int)ARRAY_LEN( s_boxSliders ) : (int)ARRAY_LEN( s_colliderSliders );

		for ( int i = 0; i < count; i++ )
			rows.push_back( i );

		return;
	}

	if ( panel == 2 )
	{
		if ( Phys_PinType() >= 0 )
		{
			rows.push_back( 0 );
			rows.push_back( 1 );
		}

		return;
	}

	if ( Phys_SelectedGroup() < 0 )
		return;

	const bool lag = Phys_Value( "lag" ) != 0.0f;

	for ( int i = 0; i < (int)ARRAY_LEN( s_settingSliders ); i++ )
	{
		const int mode = s_settingSliders[i].mode;

		if ( mode == 0 || ( mode == 1 && !lag ) || ( mode == 2 && lag ) )
			rows.push_back( i );
	}
}

static bool Phys_SliderEnabled( int panel, int index )
{
	if ( Phys_SelectedGroup() < 0 )
		return false;

	if ( panel == 0 )
		return true;

	if ( panel == 2 )
		return Phys_PinType() >= 0;

	if ( index == 0 || Phys_SelectedBox() )
		return s_phys.selCollider >= 0 && s_phys.selCollider < (int)s_phys.colliders.size();

	return Phys_Value( "humanoid" ) != 0.0f;
}

static float Phys_SliderValue( int panel, int index )
{
	if ( panel == 0 )
		return Phys_Value( s_settingSliders[index].key );

	if ( panel == 2 )
	{
		if ( Phys_PinType() < 0 )
			return 0.0f;

		const physPinEntry_t &e = s_phys.pins[s_phys.selPin];

		return (float)atof( e.tok[e.first + index].c_str() );
	}

	if ( Phys_SelectedBox() )
	{
		const physColliderEntry_t &c = s_phys.colliders[s_phys.selCollider];

		return index < 3 ? c.size[index] : ( index == 3 ? c.radius : c.off[index - 4] );
	}

	if ( index == 0 )
		return ( s_phys.selCollider >= 0 && s_phys.selCollider < (int)s_phys.colliders.size() ) ? s_phys.colliders[s_phys.selCollider].radius : 0.0f;

	return Phys_Value( "humanoidscale" );
}

// End of the bar. A value of the group that is larger when the group loads makes the bar longer.
static float Phys_SliderMax( int panel, const physSliderDef_t &def )
{
	if ( panel != 0 )
		return def.max;

	std::map<std::string, float>::const_iterator it = s_phys.loadedValues.find( def.key );

	return ( it != s_phys.loadedValues.end() && it->second > def.max ) ? it->second : def.max;
}

// Place of a value on the bar, 0 to 1.
static float Phys_SliderT( int panel, const physSliderDef_t &def, float value )
{
	const float t = ( value - def.min ) / ( Phys_SliderMax( panel, def ) - def.min );

	return t < 0.0f ? 0.0f : ( t > 1.0f ? 1.0f : t );
}

struct physLegend_t
{
	const char	*label;
	vec4_t		color;
};

// A color square and a label for each entry, on as many lines as the width needs.
static void Phys_PaintLegend( const physLegend_t *legend, int count, float x, float y, float w, float scale, int style, int font )
{
	float lx = x + 2, ly = y;

	for ( int i = 0; i < count; i++ )
	{
		const float itemW = 12.0f + DC->textWidth( legend[i].label, scale, font );

		if ( lx > x + 2 && lx + itemW > x + w )
		{
			lx = x + 2;
			ly += PHYS_ROW_H;
		}

		DC->fillRect( lx, ly + 4, 7, 7, legend[i].color );
		DC->drawText( lx + 10, ly, scale, (float *)legend[i].color, legend[i].label, 0, style, font );
		lx += itemW + 4.0f;
	}
}

static void Phys_PaintSliders( int panel, float x, float y, float w, float h, float scale, vec4_t color, int style, int font )
{
	static const vec4_t barBack = { 0.1f, 0.05f, 0.0f, 0.9f };
	static const vec4_t barBorder = { 1.0f, 0.682f, 0.0f, 0.5f };
	static const vec4_t barFill = { 1.0f, 0.682f, 0.0f, 0.35f };
	const vec4_t dim = { color[0] * 0.4f, color[1] * 0.4f, color[2] * 0.4f, color[3] };
	physSliderPanel_t &p = s_phys.panels[panel];
	std::vector<int> rows;

	Phys_SliderRows( panel, rows );

	p.x = x;
	p.y = y;
	p.w = w;
	p.h = h;
	p.barX = x + 60.0f;
	p.barW = w - 60.0f - 34.0f;
	p.rows = (int)rows.size();
	p.paintTime = DC->realTime;

	if ( rows.empty() )
	{
		const char *msg = ( panel == 2 && Phys_SelectedGroup() >= 0 ) ? "Select a pin rule (not pin bones)" : "Select a group in Used";

		DC->drawText( x + 2, y + 2, scale, (float *)dim, msg, (int)w, style, font );

		if ( panel != 2 )
			return;
	}

	const float rowH = Phys_RowH( panel );

	for ( size_t r = 0; r < rows.size(); r++ )
	{
		const physSliderDef_t &def = Phys_SliderDef( panel, rows[r] );
		const bool enabled = Phys_SliderEnabled( panel, rows[r] );
		const float value = Phys_SliderValue( panel, rows[r] );
		const float ry = y + r * rowH;
		const float t = Phys_SliderT( panel, def, value );

		DC->drawText( x + 2, ry, scale, enabled ? color : (float *)dim, def.label, 0, style, font );
		// The bar and its handle keep their place in a row of any height.
		const float barY = ry + rowH * 0.28f, barH = rowH * 0.57f;

		DC->fillRect( p.barX, barY, p.barW, barH, barBack );
		DC->fillRect( p.barX, barY, p.barW * t, barH, barFill );
		DC->drawRect( p.barX, barY, p.barW, barH, 1, barBorder );

		if ( enabled )
		{
			DC->fillRect( p.barX + p.barW * t - 1.5f, ry + rowH * 0.14f, 3, rowH * 0.86f, color );
			const std::string shown = ( def.zero && Phys_Num( value, def.decimals ) == "0" ) ? def.zero : Phys_Num( value, def.decimals );

			DC->drawText( p.barX + p.barW + 4, ry, scale, color, shown.c_str(), 0, style, font );
		}
	}

	// What the values give: the sag from the weight, against the reach.
	if ( panel == 0 )
	{
		const float ry = y + rows.size() * PHYS_ROW_H + 4;
		const float follow = Phys_Value( "hold" ) * 20.0f;
		const bool lag = Phys_Value( "lag" ) != 0.0f;

		if ( !lag && follow > 0.0f )
		{
			const float sag = Phys_Value( "weight" ) * 800.0f / ( follow * follow );
			const float reach = Phys_Value( "reach" );
			vec4_t info = { 0.8f, 1.0f, 0.8f, 1.0f };

			if ( sag > reach * 0.8f )
			{
				info[0] = 1.0f;
				info[1] = 0.6f;
				info[2] = 0.4f;
			}

			DC->drawText( x + 2, ry, scale, info, va( "Sag %s of reach %s", Phys_Num( sag, 1 ).c_str(), Phys_Num( reach, 1 ).c_str() ), 0, style, font );
		}
	}

	// The colors on the preview (Phys_DrawCollider and Phys_DrawGroupView in the renderer).
	static const physLegend_t colliderLegend[] = {
		{ "body",	{ 1.0f, 0.59f, 0.0f, 1.0f } },
		{ "fit",	{ 1.0f, 0.9f, 0.24f, 1.0f } },
		{ "line",	{ 0.24f, 1.0f, 0.35f, 1.0f } },
		{ "selected", { 0.0f, 1.0f, 1.0f, 1.0f } } };
	static const physLegend_t pinLegend[] = {
		{ "fixed",		{ 1.0f, 0.16f, 0.16f, 1.0f } },
		{ "half",		{ 1.0f, 1.0f, 0.0f, 1.0f } },
		{ "free",		{ 0.16f, 1.0f, 0.24f, 1.0f } },
		{ "bone, axis",	{ 1.0f, 1.0f, 1.0f, 1.0f } },
		{ "pin bones",	{ 1.0f, 0.24f, 1.0f, 1.0f } },
		{ "max dist",	{ 0.35f, 0.63f, 1.0f, 1.0f } },
		{ "goes back",	{ 0.75f, 0.27f, 1.0f, 1.0f } } };
	const float ly = y + ( rows.empty() ? 1 : rows.size() ) * rowH + 4;

	if ( panel == 1 )
		Phys_PaintLegend( colliderLegend, ARRAY_LEN( colliderLegend ), x, ly, w, scale, style, font );
	else if ( panel == 2 )
		Phys_PaintLegend( pinLegend, ARRAY_LEN( pinLegend ), x, ly, w, scale, style, font );
}

static void Phys_DragTo( float cursorX )
{
	std::vector<int> rows;

	// The selection can change the sliders of the panel.
	Phys_SliderRows( s_phys.dragPanel, rows );

	bool shown = false;

	for ( size_t i = 0; i < rows.size(); i++ )
		shown |= rows[i] == s_phys.dragRow;

	if ( !shown )
	{
		s_phys.dragPanel = -1;
		return;
	}

	const physSliderPanel_t &p = s_phys.panels[s_phys.dragPanel];
	const physSliderDef_t &def = Phys_SliderDef( s_phys.dragPanel, s_phys.dragRow );
	float t = p.barW > 0.0f ? ( cursorX - p.barX ) / p.barW : 0.0f;

	if ( t < 0.0f )			t = 0.0f;
	else if ( t > 1.0f )	t = 1.0f;

	Phys_SetSlider( s_phys.dragPanel, s_phys.dragRow, def.min + ( Phys_SliderMax( s_phys.dragPanel, def ) - def.min ) * t );
}

// Mouse 1 starts a drag on a bar. The wheel moves the value of the row under the cursor.
static qboolean Phys_SliderKey( int panel, int key )
{
	const physSliderPanel_t &p = s_phys.panels[panel];
	std::vector<int> rows;

	Phys_SliderRows( panel, rows );

	const int r = (int)( ( DC->cursory - p.y ) / Phys_RowH( panel ) );

	if ( DC->cursory < p.y || r < 0 || r >= (int)rows.size() || !Phys_SliderEnabled( panel, rows[r] ) )
		return qfalse;

	const physSliderDef_t &def = Phys_SliderDef( panel, rows[r] );

	if ( key == A_MOUSE1 )
	{
		if ( DC->cursorx < p.barX - 4 || DC->cursorx > p.barX + p.barW + 4 )
			return qfalse;

		s_phys.dragPanel = panel;
		s_phys.dragRow = rows[r];
		Phys_DragTo( DC->cursorx );
		return qtrue;
	}

	if ( key == A_MWHEELUP || key == A_MWHEELDOWN )
	{
		// One step is 10 % of the value: fine at small values. At least one shown digit, at most 1/50 of the bar.
		const float max = Phys_SliderMax( panel, def );
		const float unit = powf( 10.0f, (float)-def.decimals );
		const float old = Phys_SliderValue( panel, rows[r] );
		float step = fabsf( old ) * 0.1f;

		if ( step > ( max - def.min ) / 50.0f )	step = ( max - def.min ) / 50.0f;
		if ( step < unit )						step = unit;

		const float value = old + ( key == A_MWHEELUP ? step : -step );

		Phys_SetSlider( panel, rows[r], value < def.min ? def.min : ( value > max ? max : value ) );
		return qtrue;
	}

	return qfalse;
}

// Every frame: the drag of a slider, the collider view and the preview.
static void Phys_Poll( void )
{
	if ( s_phys.dragPanel >= 0 )
	{
		if ( ui.Key_IsDown( A_MOUSE1 ) )
			Phys_DragTo( DC->cursorx );
		else
			s_phys.dragPanel = -1;
	}

	const int show = Phys_ViewFlags();

	if ( show != s_phys.shownView )
	{
		s_phys.shownView = show;
		Phys_SendView();
	}

	Phys_PollPreview();
}

// The hint of the slider row under the cursor, in a panel shown this frame, or NULL.
static const char *Phys_HoverHint( void )
{
	for ( int panel = 0; panel < 3; panel++ )
	{
		const physSliderPanel_t &p = s_phys.panels[panel];
		std::vector<int> rows;

		if ( p.paintTime != DC->realTime || DC->cursorx < p.x || DC->cursorx > p.x + p.w || DC->cursory < p.y )
			continue;

		Phys_SliderRows( panel, rows );

		const int r = (int)( ( DC->cursory - p.y ) / Phys_RowH( panel ) );

		if ( r >= 0 && r < (int)rows.size() )
			return Phys_SliderDef( panel, rows[r] ).hint;
	}

	return NULL;
}

// The hint in place of the status: a smaller text on two lines at most.
static void Phys_PaintHint( const char *hint, float x, float y, float w, float scale, int style, int font )
{
	static const vec4_t color = { 0.7f, 0.85f, 1.0f, 1.0f };
	const float small = scale * 0.72f;
	const float lineH = (float)DC->textHeight( "Ag", small, font ) + 1.0f;
	std::string line, rest = hint;
	int count = 0;

	y -= 6.0f;

	while ( !rest.empty() && count < 2 )
	{
		size_t cut = rest.size();

		// The longest start of the text that fits, cut at a space.
		while ( DC->textWidth( rest.substr( 0, cut ).c_str(), small, font ) > w )
		{
			const size_t space = rest.rfind( ' ', cut - 1 );

			if ( space == std::string::npos || space == 0 )
				break;

			cut = space;
		}

		DC->drawText( x, y + count * lineH, small, (float *)color, rest.substr( 0, cut ).c_str(), (int)w, style, font );
		rest = cut < rest.size() ? rest.substr( cut + 1 ) : "";
		count++;
	}
}

qboolean UI_Phys_OwnerDraw( int ownerDraw, float x, float y, float w, float h, float scale, vec4_t color, int style, int font )
{
	switch ( ownerDraw )
	{
	case UI_PHYS_SETTINGS:
		Phys_PaintSliders( 0, x, y, w, h, scale, color, style, font );
		return qtrue;

	case UI_PHYS_COLLSET:
		Phys_PaintSliders( 1, x, y, w, h, scale, color, style, font );
		return qtrue;

	case UI_PHYS_PINSET:
		Phys_PaintSliders( 2, x, y, w, h, scale, color, style, font );
		return qtrue;

	case UI_PHYS_TEXT:
		Phys_PaintText( x, y, w, h, scale, color, style, font );
		return qtrue;

	case UI_PHYS_HELP:
		Phys_PaintHelp( x, y, w, h, scale, color, style, font );
		return qtrue;

	case UI_PHYS_STATUS:
	{
		vec4_t c = { 0.8f, 1.0f, 0.8f, 1.0f };

		Phys_Poll();

		if ( s_phys.statusError )
		{
			c[0] = 1.0f;
			c[1] = 0.4f;
			c[2] = 0.4f;
		}

		const char *hint = Phys_HoverHint();

		if ( hint )
			Phys_PaintHint( hint, x, y, w, scale, style, font );
		else
			DC->drawText( x, y, scale, c, s_phys.status.c_str(), (int)w, style, font );

		return qtrue;
	}
	}

	return qfalse;
}

//
// Keys of the text area
//

// Removes the selected text. The caret goes to the start of it.
static bool Phys_DeleteSelection( void )
{
	int a, b;

	Phys_SelRange( a, b );

	if ( a >= b )
		return false;

	memmove( s_phys.text + a, s_phys.text + b, s_phys.textLen - b + 1 );
	s_phys.textLen -= b - a;
	s_phys.caret = s_phys.anchor = a;
	return true;
}

// Puts text at the caret, in place of the selection. A pasted text keeps its tabs and line breaks.
static void Phys_InsertText( const char *text )
{
	Phys_DeleteSelection();

	for ( const char *p = text; *p; p++ )
	{
		char c = *p;

		if ( c == '\r' )
		{
			if ( p[1] == '\n' )
				continue;

			c = '\n';
		}

		if ( c != '\n' && c != '\t' && ( (unsigned char)c < 32 || (unsigned char)c >= 127 ) )
			continue;

		if ( s_phys.textLen >= PHYS_TEXT_MAX - 1 )
			break;

		memmove( s_phys.text + s_phys.caret + 1, s_phys.text + s_phys.caret, s_phys.textLen - s_phys.caret + 1 );
		s_phys.text[s_phys.caret] = c;
		s_phys.caret++;
		s_phys.textLen++;
	}

	s_phys.anchor = s_phys.caret;
}

static void Phys_Insert( char c )
{
	const char text[2] = { c, '\0' };

	Phys_InsertText( text );
}

static void Phys_Backspace( void )
{
	if ( Phys_DeleteSelection() || s_phys.caret <= 0 )
		return;

	memmove( s_phys.text + s_phys.caret - 1, s_phys.text + s_phys.caret, s_phys.textLen - s_phys.caret + 1 );
	s_phys.caret--;
	s_phys.textLen--;
	s_phys.anchor = s_phys.caret;
}

static void Phys_DeleteForward( void )
{
	if ( Phys_DeleteSelection() || s_phys.caret >= s_phys.textLen )
		return;

	memmove( s_phys.text + s_phys.caret, s_phys.text + s_phys.caret + 1, s_phys.textLen - s_phys.caret );
	s_phys.textLen--;
	s_phys.anchor = s_phys.caret;
}

// Moves the caret. With extend (Shift), the anchor stays and the selection grows.
static void Phys_MoveCaret( int pos, bool extend )
{
	s_phys.caret = pos < 0 ? 0 : ( pos > s_phys.textLen ? s_phys.textLen : pos );

	if ( !extend )
		s_phys.anchor = s_phys.caret;
}

static bool Phys_WordChar( char c )
{
	return isalnum( (unsigned char)c ) || c == '_' || c == '.' || c == '-' || c == '*' || c == '?';
}

// Ctrl + left: to the start of the word before the caret.
static int Phys_WordLeft( int pos )
{
	while ( pos > 0 && !Phys_WordChar( s_phys.text[pos - 1] ) )
		pos--;

	while ( pos > 0 && Phys_WordChar( s_phys.text[pos - 1] ) )
		pos--;

	return pos;
}

// Ctrl + right: to the start of the next word.
static int Phys_WordRight( int pos )
{
	while ( pos < s_phys.textLen && Phys_WordChar( s_phys.text[pos] ) )
		pos++;

	while ( pos < s_phys.textLen && !Phys_WordChar( s_phys.text[pos] ) )
		pos++;

	return pos;
}

static void Phys_MoveVertical( int delta, bool extend )
{
	const int line = Phys_LineOfCaret();
	const int target = line + delta;

	if ( target < 0 || target >= Phys_LineCount() )
		return;

	const int col = s_phys.caret - Phys_LineStart( line );
	const int start = Phys_LineStart( target );
	const int len = Phys_LineEnd( start ) - start;

	Phys_MoveCaret( start + ( col < len ? col : len ), extend );
}

// The position in the text under the cursor. Above or below the area it is on the line before or after.
static int Phys_PosAt( float cursorX, float cursorY )
{
	const int lineH = Phys_LineHeight();

	if ( lineH <= 0 )
		return s_phys.caret;

	const float localY = cursorY - s_phys.textRect.y - 2;
	int line = s_phys.scroll + (int)floorf( localY / lineH );

	if ( line < 0 )
		line = 0;

	if ( line >= Phys_LineCount() )
		line = Phys_LineCount() - 1;

	const int start = Phys_LineStart( line );
	const int end = Phys_LineEnd( start );
	const float localX = cursorX - s_phys.textRect.x - 3;
	int col = 0;

	// The caret goes to the nearest side of the character.
	while ( start + col < end )
	{
		const float widthBefore = Phys_ShownWidth( start, start + col );
		const float width = Phys_ShownWidth( start, start + col + 1 );

		if ( localX < ( width + widthBefore ) * 0.5f )
			break;

		col++;
	}

	return start + col;
}

// A click: one places the caret (Shift extends the selection), two select a word, three select the line.
static void Phys_Click( void )
{
	const int pos = Phys_PosAt( DC->cursorx, DC->cursory );

	if ( DC->realTime - s_phys.clickTime < 400 && abs( pos - s_phys.clickPos ) <= 1 && s_phys.clickCount < 3 )
		s_phys.clickCount++;
	else
		s_phys.clickCount = 1;

	s_phys.clickTime = DC->realTime;
	s_phys.clickPos = pos;

	if ( s_phys.clickCount == 1 )
	{
		Phys_MoveCaret( pos, ui.Key_IsDown( A_SHIFT ) != qfalse );
		s_phys.textDrag = true;
	}
	else if ( s_phys.clickCount == 2 )
	{
		int a = pos, b = pos;

		if ( pos < s_phys.textLen && Phys_WordChar( s_phys.text[pos] ) )
		{
			while ( a > 0 && Phys_WordChar( s_phys.text[a - 1] ) )
				a--;

			while ( b < s_phys.textLen && Phys_WordChar( s_phys.text[b] ) )
				b++;
		}
		else if ( pos > 0 && Phys_WordChar( s_phys.text[pos - 1] ) )
		{
			while ( a > 0 && Phys_WordChar( s_phys.text[a - 1] ) )
				a--;
		}
		else if ( pos < s_phys.textLen )
		{
			b = pos + 1;
		}

		s_phys.anchor = a;
		s_phys.caret = b;
		s_phys.textDrag = false;
	}
	else
	{
		const int start = Phys_LineStart( Phys_LineOfCaret() );
		const int end = Phys_LineEnd( start );

		s_phys.anchor = start;
		s_phys.caret = end < s_phys.textLen ? end + 1 : end;
		s_phys.textDrag = false;
	}
}

static void Phys_CopySelection( void )
{
	int a, b;

	Phys_SelRange( a, b );

	if ( a >= b || !ui.SetClipboardData )
		return;

	const std::string text( s_phys.text + a, b - a );

	ui.SetClipboardData( text.c_str() );
}

static void Phys_Paste( void )
{
	static char buf[PHYS_TEXT_MAX];

	if ( !ui.GetClipboardData )
		return;

	buf[0] = '\0';
	ui.GetClipboardData( buf, sizeof( buf ) );
	Phys_InsertText( buf );
}

#define PHYS_CTRL( c )	( ( c ) - 'a' + 1 )

qboolean UI_Phys_HandleKey( int ownerDraw, int key )
{
	if ( ownerDraw == UI_PHYS_SETTINGS )
		return Phys_SliderKey( 0, key );

	if ( ownerDraw == UI_PHYS_COLLSET )
		return Phys_SliderKey( 1, key );

	if ( ownerDraw == UI_PHYS_PINSET )
		return Phys_SliderKey( 2, key );

	if ( ownerDraw == UI_PHYS_HELP )
	{
		if ( key == A_MWHEELUP )
			s_phys.helpScroll -= 2;
		else if ( key == A_MWHEELDOWN )
			s_phys.helpScroll += 2;
		else
			return qfalse;

		return qtrue;
	}

	if ( ownerDraw != UI_PHYS_TEXT )
		return qfalse;

	const bool shift = ui.Key_IsDown( A_SHIFT ) != qfalse;
	const bool ctrl = ui.Key_IsDown( A_CTRL ) != qfalse;
	const bool alt = ui.Key_IsDown( A_ALT ) != qfalse;

	// Ctrl + letter comes as a control character (1 to 26). Backspace comes as Ctrl + H.
	if ( key & K_CHAR_FLAG )
	{
		const int c = key & ~K_CHAR_FLAG;

		s_phys.caretTime = DC->realTime;
		s_phys.followCaret = true;

		if ( c == PHYS_CTRL( 'h' ) )
		{
			Phys_Backspace();
		}
		else if ( c == PHYS_CTRL( 'a' ) )
		{
			s_phys.anchor = 0;
			s_phys.caret = s_phys.textLen;
			s_phys.followCaret = false;
		}
		else if ( c == PHYS_CTRL( 'c' ) )
		{
			Phys_CopySelection();
		}
		else if ( c == PHYS_CTRL( 'x' ) )
		{
			Phys_CopySelection();
			Phys_DeleteSelection();
		}
		else if ( c == PHYS_CTRL( 'v' ) )
		{
			Phys_Paste();
		}
		else if ( c >= 32 && c < 127 && ( !ctrl || alt ) )
		{
			// Ctrl + Alt is AltGr: it types the braces on some keyboards.
			Phys_Insert( (char)c );
		}

		return qtrue;
	}

	switch ( key )
	{
	case A_BACKSPACE:
	case A_SHIFT:
	case A_CTRL:
	case A_ALT:
		return qtrue;	// the char of the backspace key is handled above
	case A_MWHEELUP:
		s_phys.scroll -= 2;
		return qtrue;
	case A_MWHEELDOWN:
		s_phys.scroll += 2;
		return qtrue;
	case A_MOUSE1:
		s_phys.caretTime = DC->realTime;
		s_phys.followCaret = true;
		Phys_Click();
		return qtrue;
	}

	s_phys.caretTime = DC->realTime;
	s_phys.followCaret = true;

	switch ( key )
	{
	case A_ENTER:
	case A_KP_ENTER:	Phys_Insert( '\n' );		return qtrue;
	case A_TAB:			Phys_Insert( '\t' );		return qtrue;
	case A_DELETE:		Phys_DeleteForward();		return qtrue;
	case A_CURSOR_LEFT:
	{
		int a, b;

		Phys_SelRange( a, b );

		if ( a < b && !shift )
			Phys_MoveCaret( a, false );
		else
			Phys_MoveCaret( ctrl ? Phys_WordLeft( s_phys.caret ) : s_phys.caret - 1, shift );

		return qtrue;
	}
	case A_CURSOR_RIGHT:
	{
		int a, b;

		Phys_SelRange( a, b );

		if ( a < b && !shift )
			Phys_MoveCaret( b, false );
		else
			Phys_MoveCaret( ctrl ? Phys_WordRight( s_phys.caret ) : s_phys.caret + 1, shift );

		return qtrue;
	}
	case A_CURSOR_UP:	Phys_MoveVertical( -1, shift );	return qtrue;
	case A_CURSOR_DOWN:	Phys_MoveVertical( 1, shift );	return qtrue;
	case A_HOME:
		Phys_MoveCaret( ctrl ? 0 : Phys_LineStart( Phys_LineOfCaret() ), shift );
		return qtrue;
	case A_END:
		Phys_MoveCaret( ctrl ? s_phys.textLen : Phys_LineEnd( Phys_LineStart( Phys_LineOfCaret() ) ), shift );
		return qtrue;
	}

	return qfalse;
}
