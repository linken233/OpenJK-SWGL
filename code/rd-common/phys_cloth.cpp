/*
===========================================================================
Cloth and hair physics: .phys parser and position based solver.
===========================================================================
*/

#include "phys_cloth.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <functional>
#include <map>
#include <queue>
#include <utility>

#define PHYS_GRAVITY		800.0f
#define PHYS_MAX_STEP		( 1.0f / 120.0f )	// lag mode step, and the step of the per step constants
#define PHYS_MAX_SUBSTEPS	4
#define PHYS_CLOTH_STEP		( 1.0f / 480.0f )	// a limb moves less than its radius in one step, so it cannot jump over a particle
#define PHYS_MAX_FIXED_STEPS	32	// cloth steps per frame at most: below 15 fps the cloth gets slow
#define PHYS_MIN_FREE		0.01f

//
// Text helpers
//

static int Phys_Stricmp( const char *a, const char *b )
{
	for ( ; *a && *b; a++, b++ )
	{
		const int ca = tolower( (unsigned char)*a );
		const int cb = tolower( (unsigned char)*b );

		if ( ca != cb )
			return ca - cb;
	}

	return tolower( (unsigned char)*a ) - tolower( (unsigned char)*b );
}

bool Phys_MatchName( const char *pattern, const char *name )
{
	while ( *pattern )
	{
		if ( *pattern == '*' )
		{
			while ( *pattern == '*' )
				pattern++;

			if ( !*pattern )
				return true;

			for ( ; *name; name++ )
			{
				if ( Phys_MatchName( pattern, name ) )
					return true;
			}

			return false;
		}

		if ( !*name )
			return false;

		if ( *pattern != '?' && tolower( (unsigned char)*pattern ) != tolower( (unsigned char)*name ) )
			return false;

		pattern++;
		name++;
	}

	return *name == '\0';
}

// Reads the next non-empty line as a list of tokens. The braces are tokens of their own.
static bool Phys_ReadLine( const char *&p, std::vector<std::string> &tok )
{
	tok.clear();

	while ( *p && tok.empty() )
	{
		while ( *p && *p != '\n' )
		{
			if ( *p == ' ' || *p == '\t' || *p == '\r' )
			{
				p++;
			}
			else if ( p[0] == '/' && p[1] == '/' )
			{
				while ( *p && *p != '\n' )
					p++;
			}
			else if ( p[0] == '/' && p[1] == '*' )
			{
				p += 2;

				while ( *p && !( p[0] == '*' && p[1] == '/' ) )
					p++;

				if ( *p )
					p += 2;
			}
			else if ( *p == '{' || *p == '}' )
			{
				tok.push_back( std::string( 1, *p ) );
				p++;
			}
			else if ( *p == '"' )
			{
				const char *start = ++p;

				while ( *p && *p != '"' && *p != '\n' )
					p++;

				tok.push_back( std::string( start, p ) );

				if ( *p == '"' )
					p++;
			}
			else
			{
				const char *start = p;

				while ( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '{' && *p != '}' && *p != '"' )
					p++;

				tok.push_back( std::string( start, p ) );
			}
		}

		if ( *p == '\n' )
			p++;
	}

	return !tok.empty();
}

//
// Parser
//

void Phys_InitGroup( physGroupDef_t &g, bool hair )
{
	g.hair = hair;
	g.humanoid = false;
	g.modelFit = false;
	g.modelFitScale = 1.0f;
	g.humanoidScale = 1.0f;
	g.colliders.clear();
	g.radiusOverride.clear();
	g.removed.clear();
	g.pins.clear();
	g.surfaces.clear();
	g.enabled = true;
	g.lag = false;

	g.material.clear();
	g.weight = hair ? 0.3f : 0.5f;
	g.stretch = hair ? 1.0f : 0.9f;
	g.stiffness = hair ? 0.3f : 0.15f;
	g.air = hair ? 0.6f : 0.5f;
	g.hold = hair ? 0.5f : 0.0f;
	g.bounce = hair ? 0.5f : 0.8f;
	g.inertia = hair ? 0.5f : 0.7f;
	g.shape = 0.0f;
	g.maxDist = hair ? 8.0f : 24.0f;
	g.maxDistN = 0.0f;
	g.detail = 1.0f;
	g.range = 0.0f;
	g.subdivide = 0;
	g.simSubdivide = 0;

	g.maxSpeed = hair ? 400.0f : 700.0f;
	g.margin = 0.5f;
	g.iterations = 1;
	g.oldDamping = -1.0f;

	Phys_DeriveGroup( g );
}

struct physMaterial_t
{
	const char	*name;
	float		weight, stretch, stiffness, air;
};

static const physMaterial_t s_materials[] =
{
	{ "silk",		0.3f,	0.9f,	0.05f,	0.8f },
	{ "cotton",		0.5f,	0.9f,	0.15f,	0.5f },
	{ "leather",	0.8f,	1.0f,	0.5f,	0.3f },
	{ "heavy",		1.0f,	1.0f,	0.3f,	0.2f },
};

static bool Phys_ApplyMaterial( physGroupDef_t &g, const char *name )
{
	for ( size_t i = 0; i < sizeof( s_materials ) / sizeof( s_materials[0] ); i++ )
	{
		const physMaterial_t &m = s_materials[i];

		if ( Phys_Stricmp( m.name, name ) )
			continue;

		g.material = m.name;
		g.weight = m.weight;
		g.stretch = m.stretch;
		g.stiffness = m.stiffness;
		g.air = m.air;
		return true;
	}

	return false;
}

static float Phys_Clamp( float v, float lo, float hi )
{
	return v < lo ? lo : ( v > hi ? hi : v );
}

// Drag of air 0.5. Air 0 and 1 give this drag divided and multiplied by PHYS_AIR_RANGE.
#define PHYS_AIR_DRAG	0.25f
#define PHYS_AIR_RANGE	12.0f

// The air value that gives this drag.
static float Phys_AirOfDrag( float drag )
{
	if ( drag <= 0.0f )
		return 0.0f;

	return Phys_Clamp( ( logf( drag / PHYS_AIR_DRAG ) / logf( PHYS_AIR_RANGE ) + 1.0f ) * 0.5f, 0.0f, 1.0f );
}

// Old keyword damping: the bounce that gives this damping with the current hold.
static void Phys_BounceOfDamping( physGroupDef_t &g )
{
	const float follow = g.hold * PHYS_MAX_HOLD;
	const float freq = follow > PHYS_DAMP_MIN_FREQ ? follow : PHYS_DAMP_MIN_FREQ;

	g.bounce = 1.0f - g.oldDamping / ( 2.0f * freq );
}

void Phys_DeriveGroup( physGroupDef_t &g )
{
	g.weight = g.weight < 0.0f ? 0.0f : g.weight;
	g.stretch = Phys_Clamp( g.stretch, 0.0f, 1.0f );
	g.stiffness = Phys_Clamp( g.stiffness, 0.0f, 1.0f );
	g.air = Phys_Clamp( g.air, 0.0f, 1.0f );
	g.hold = Phys_Clamp( g.hold, 0.0f, 1.0f );
	g.bounce = Phys_Clamp( g.bounce, 0.0f, 1.0f );
	g.inertia = Phys_Clamp( g.inertia, 0.0f, 1.0f );
	g.shape = Phys_Clamp( g.shape, 0.0f, 1.0f );
	g.detail = Phys_Clamp( g.detail, 0.25f, 8.0f );

	if ( g.subdivide < 0 )
		g.subdivide = 0;
	else if ( g.subdivide > PHYS_MAX_SUBDIVIDE )
		g.subdivide = PHYS_MAX_SUBDIVIDE;

	if ( g.simSubdivide < 0 )
		g.simSubdivide = 0;
	else if ( g.simSubdivide > PHYS_MAX_SUBDIVIDE )
		g.simSubdivide = PHYS_MAX_SUBDIVIDE;

	if ( g.iterations < 1 )
		g.iterations = 1;
	else if ( g.iterations > 16 )
		g.iterations = 16;

	g.gravity = g.weight;
	g.bend = g.stiffness;
	g.drag = g.air > 0.0f ? PHYS_AIR_DRAG * powf( PHYS_AIR_RANGE, 2.0f * g.air - 1.0f ) : 0.0f;
	g.aero = 2.0f * g.air;
	g.wind = g.air;
	g.follow = g.hold * PHYS_MAX_HOLD;
	g.lagFreq = g.follow;
	g.lagDamp = 1.0f - g.bounce;
	g.damping = 2.0f * g.lagDamp * ( g.follow > PHYS_DAMP_MIN_FREQ ? g.follow : PHYS_DAMP_MIN_FREQ );
	g.proxy = 4.0f / g.detail;
}

// Rounded box on a bone. The corner radius is not larger than the smallest half size.
static void Phys_AddBox( physGroupDef_t &g, const char *bone, const float size[3], float round, const float off[3] )
{
	physCollider_t c;
	float smallest = 1e30f;

	c.box = true;
	c.boneA = bone;
	c.boneB = bone;

	for ( int k = 0; k < 3; k++ )
	{
		c.size[k] = fabsf( size[k] );
		c.offA[k] = c.offB[k] = off[k];

		if ( c.size[k] < smallest )
			smallest = c.size[k];
	}

	c.radius = round < 0.0f ? 0.0f : ( round > smallest ? smallest : round );
	g.colliders.push_back( c );
}

static void Phys_AddSphere( physGroupDef_t &g, const char *bone, float radius, float ox, float oy, float oz )
{
	physCollider_t c;

	c.capsule = false;
	c.boneA = bone;
	c.boneB = bone;
	c.offA[0] = c.offB[0] = ox;
	c.offA[1] = c.offB[1] = oy;
	c.offA[2] = c.offB[2] = oz;
	c.radius = radius;
	g.colliders.push_back( c );
}

static void Phys_AddCapsule( physGroupDef_t &g, const char *boneA, const char *boneB, float radius )
{
	physCollider_t c;

	c.capsule = true;
	c.boneA = boneA;
	c.boneB = boneB;
	c.offA[0] = c.offA[1] = c.offA[2] = 0.0f;
	c.offB[0] = c.offB[1] = c.offB[2] = 0.0f;
	c.radius = radius;
	g.colliders.push_back( c );
}

// Colliders for the bones of the stock _humanoid skeleton.
static void Phys_AddHumanoidColliders( std::vector<physCollider_t> &out, float scale )
{
	physGroupDef_t tmp;

	Phys_AddCapsule( tmp, "pelvis", "lower_lumbar", 7.0f );
	Phys_AddCapsule( tmp, "lower_lumbar", "cervical", 6.5f );
	Phys_AddSphere( tmp, "cranium", 7.5f, 0.0f, 0.0f, 3.5f );

	Phys_AddCapsule( tmp, "lhumerus", "lradius", 3.2f );
	Phys_AddCapsule( tmp, "lradius", "lhand", 2.6f );
	Phys_AddCapsule( tmp, "rhumerus", "rradius", 3.2f );
	Phys_AddCapsule( tmp, "rradius", "rhand", 2.6f );

	Phys_AddCapsule( tmp, "lfemurYZ", "ltibia", 5.2f );
	Phys_AddCapsule( tmp, "ltibia", "ltalus", 3.6f );
	Phys_AddCapsule( tmp, "rfemurYZ", "rtibia", 5.2f );
	Phys_AddCapsule( tmp, "rtibia", "rtalus", 3.6f );

	for ( size_t i = 0; i < tmp.colliders.size(); i++ )
	{
		tmp.colliders[i].radius *= scale;
		out.push_back( tmp.colliders[i] );
	}
}

void Phys_ExpandColliders( const physGroupDef_t &g, std::vector<physCollider_t> &out, const std::vector<physCollider_t> *fitted )
{
	out.clear();

	if ( g.modelFit && fitted )
	{
		for ( size_t i = 0; i < fitted->size(); i++ )
		{
			out.push_back( ( *fitted )[i] );
			out.back().kind = 'm';
		}
	}

	if ( g.humanoid )
	{
		const size_t first = out.size();

		Phys_AddHumanoidColliders( out, g.humanoidScale );

		for ( size_t i = first; i < out.size(); i++ )
			out[i].kind = 'h';
	}

	for ( size_t i = 0; i < g.colliders.size(); i++ )
	{
		out.push_back( g.colliders[i] );
		out.back().kind = 'c';
	}

	// "collide radius" and "collide remove" change the generated colliders only. A collide line has its own radius.
	for ( size_t i = 0; i < g.radiusOverride.size(); i++ )
	{
		for ( size_t c = 0; c < out.size(); c++ )
		{
			if ( out[c].kind != 'c' && !Phys_Stricmp( out[c].boneA.c_str(), g.radiusOverride[i].first.c_str() ) )
				out[c].radius = g.radiusOverride[i].second;
		}
	}

	for ( size_t i = 0; i < g.removed.size(); i++ )
	{
		for ( size_t c = 0; c < out.size(); )
		{
			if ( out[c].kind != 'c' && !Phys_Stricmp( out[c].boneA.c_str(), g.removed[i].c_str() ) )
				out.erase( out.begin() + c );
			else
				c++;
		}
	}
}

static bool Phys_ParseAxis( const char *s, float axis[3] )
{
	float sign = 1.0f;

	axis[0] = axis[1] = axis[2] = 0.0f;

	if ( *s == '-' )
	{
		sign = -1.0f;
		s++;
	}
	else if ( *s == '+' )
	{
		s++;
	}

	if ( s[1] )
		return false;

	switch ( tolower( (unsigned char)*s ) )
	{
	case 'x':	axis[0] = sign;	return true;
	case 'y':	axis[1] = sign;	return true;
	case 'z':	axis[2] = sign;	return true;
	}

	return false;
}

static float Phys_Arg( const std::vector<std::string> &tok, size_t i, float fallback )
{
	return i < tok.size() ? (float)atof( tok[i].c_str() ) : fallback;
}

static void Phys_ParseLine( physGroupDef_t &g, const std::vector<std::string> &tok, void ( *print )( const char *msg ) )
{
	const char *key = tok[0].c_str();
	char msg[256];

	if ( !Phys_Stricmp( key, "surfaces" ) )
	{
		for ( size_t i = 1; i < tok.size(); i++ )
			g.surfaces.push_back( tok[i] );
	}
	else if ( !Phys_Stricmp( key, "pin" ) && tok.size() >= 2 )
	{
		physPinRule_t r;
		bool ok = false;

		r.axis[0] = r.axis[1] = r.axis[2] = 0.0f;
		r.pinned = r.free = 0.0f;

		if ( !Phys_Stricmp( tok[1].c_str(), "gradient" ) && tok.size() >= 6 )
		{
			r.type = PHYS_PIN_GRADIENT;
			r.bone = tok[2];
			r.pinned = Phys_Arg( tok, 4, 0.0f );
			r.free = Phys_Arg( tok, 5, 1.0f );
			ok = Phys_ParseAxis( tok[3].c_str(), r.axis ) && r.pinned != r.free;
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "bones" ) && tok.size() >= 3 )
		{
			r.type = PHYS_PIN_BONES;

			for ( size_t i = 2; i < tok.size(); i++ )
				r.bones.push_back( tok[i] );

			ok = true;
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "radius" ) && tok.size() >= 5 )
		{
			r.type = PHYS_PIN_RADIUS;
			r.bone = tok[2];
			r.pinned = Phys_Arg( tok, 3, 0.0f );
			r.free = Phys_Arg( tok, 4, 1.0f );
			r.axis[0] = Phys_Arg( tok, 5, 0.0f );
			r.axis[1] = Phys_Arg( tok, 6, 0.0f );
			r.axis[2] = Phys_Arg( tok, 7, 0.0f );
			ok = r.pinned != r.free;
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "uv" ) && tok.size() >= 4 )
		{
			r.type = PHYS_PIN_UV;
			r.pinned = Phys_Arg( tok, 2, 0.0f );
			r.free = Phys_Arg( tok, 3, 1.0f );
			ok = r.pinned != r.free;
		}

		if ( ok )
			g.pins.push_back( r );
		else if ( print )
			print( "bad pin line\n" );
	}
	else if ( !Phys_Stricmp( key, "collide" ) && tok.size() >= 2 )
	{
		if ( !Phys_Stricmp( tok[1].c_str(), "humanoid" ) )
		{
			g.humanoid = true;
			g.humanoidScale = Phys_Arg( tok, 2, 1.0f );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "model" ) )
		{
			g.modelFit = true;
			g.modelFitScale = Phys_Arg( tok, 2, 1.0f );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "radius" ) && tok.size() >= 4 )
		{
			g.radiusOverride.push_back( std::make_pair( tok[2], Phys_Arg( tok, 3, 1.0f ) ) );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "remove" ) && tok.size() >= 3 )
		{
			g.removed.push_back( tok[2] );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "capsule" ) && tok.size() >= 5 )
		{
			Phys_AddCapsule( g, tok[2].c_str(), tok[3].c_str(), Phys_Arg( tok, 4, 1.0f ) );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "sphere" ) && tok.size() >= 4 )
		{
			Phys_AddSphere( g, tok[2].c_str(), Phys_Arg( tok, 3, 1.0f ), Phys_Arg( tok, 4, 0.0f ), Phys_Arg( tok, 5, 0.0f ), Phys_Arg( tok, 6, 0.0f ) );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "box" ) && tok.size() >= 6 )
		{
			// collide box <bone> <sx> <sy> <sz> [<round>] [<ox> <oy> <oz>]
			const float size[3] = { Phys_Arg( tok, 3, 1.0f ), Phys_Arg( tok, 4, 1.0f ), Phys_Arg( tok, 5, 1.0f ) };
			const float off[3] = { Phys_Arg( tok, 7, 0.0f ), Phys_Arg( tok, 8, 0.0f ), Phys_Arg( tok, 9, 0.0f ) };

			Phys_AddBox( g, tok[2].c_str(), size, Phys_Arg( tok, 6, 0.0f ), off );
		}
		else if ( tok.size() == 4 && ( isdigit( (unsigned char)tok[3][0] ) || tok[3][0] == '.' || tok[3][0] == '-' ) )
		{
			// Short form of a capsule: collide <boneA> <boneB> <radius>
			Phys_AddCapsule( g, tok[1].c_str(), tok[2].c_str(), Phys_Arg( tok, 3, 1.0f ) );
		}
		else if ( print )
		{
			print( "bad collide line\n" );
		}
	}
	else if ( !Phys_Stricmp( key, "reach" ) || !Phys_Stricmp( key, "maxdist" ) )
	{
		g.maxDist = Phys_Arg( tok, 1, g.maxDist );
		g.maxDistN = Phys_Arg( tok, 2, 0.0f );
	}
	else if ( !Phys_Stricmp( key, "material" ) && tok.size() >= 2 )
	{
		if ( !Phys_ApplyMaterial( g, tok[1].c_str() ) && print )
			print( "material is 'silk', 'cotton', 'leather' or 'heavy'\n" );
	}
	else if ( !Phys_Stricmp( key, "mode" ) && tok.size() >= 2 )
	{
		if ( !Phys_Stricmp( tok[1].c_str(), "lag" ) )
			g.lag = true;
		else if ( !Phys_Stricmp( tok[1].c_str(), "cloth" ) )
			g.lag = false;
		else if ( print )
			print( "mode is 'cloth' or 'lag'\n" );
	}
	else if ( !Phys_Stricmp( key, "weight" ) )		g.weight = Phys_Arg( tok, 1, g.weight );
	else if ( !Phys_Stricmp( key, "stretch" ) )		g.stretch = Phys_Arg( tok, 1, g.stretch );
	else if ( !Phys_Stricmp( key, "stiffness" ) )	g.stiffness = Phys_Arg( tok, 1, g.stiffness );
	else if ( !Phys_Stricmp( key, "air" ) )			g.air = Phys_Arg( tok, 1, g.air );
	else if ( !Phys_Stricmp( key, "hold" ) )		g.hold = Phys_Arg( tok, 1, g.hold );
	else if ( !Phys_Stricmp( key, "bounce" ) )
	{
		g.bounce = Phys_Arg( tok, 1, g.bounce );
		g.oldDamping = -1.0f;
	}
	else if ( !Phys_Stricmp( key, "inertia" ) )		g.inertia = Phys_Arg( tok, 1, g.inertia );
	else if ( !Phys_Stricmp( key, "shape" ) )		g.shape = Phys_Arg( tok, 1, g.shape );
	else if ( !Phys_Stricmp( key, "detail" ) )		g.detail = Phys_Arg( tok, 1, g.detail );
	else if ( !Phys_Stricmp( key, "range" ) )		g.range = Phys_Arg( tok, 1, g.range );
	else if ( !Phys_Stricmp( key, "subdivide" ) )	g.subdivide = (int)Phys_Arg( tok, 1, (float)g.subdivide );
	else if ( !Phys_Stricmp( key, "simsubdivide" ) )	g.simSubdivide = (int)Phys_Arg( tok, 1, (float)g.simSubdivide );
	else if ( !Phys_Stricmp( key, "enabled" ) )		g.enabled = Phys_Arg( tok, 1, 1.0f ) != 0.0f;
	// Old keywords: converted to the new ones.
	else if ( !Phys_Stricmp( key, "gravity" ) )		g.weight = Phys_Arg( tok, 1, g.weight );
	else if ( !Phys_Stricmp( key, "bend" ) )		g.stiffness = Phys_Arg( tok, 1, g.stiffness );
	else if ( !Phys_Stricmp( key, "follow" ) || !Phys_Stricmp( key, "lagfreq" ) )
	{
		g.hold = Phys_Arg( tok, 1, g.follow ) / PHYS_MAX_HOLD;

		// A damping line before this line keeps its value.
		if ( g.oldDamping >= 0.0f )
			Phys_BounceOfDamping( g );
	}
	else if ( !Phys_Stricmp( key, "lagdamp" ) )		g.bounce = 1.0f - Phys_Arg( tok, 1, g.lagDamp );
	else if ( !Phys_Stricmp( key, "damping" ) )
	{
		g.oldDamping = Phys_Arg( tok, 1, g.damping );
		Phys_BounceOfDamping( g );
	}
	else if ( !Phys_Stricmp( key, "drag" ) )		g.air = Phys_AirOfDrag( Phys_Arg( tok, 1, g.drag ) );
	else if ( !Phys_Stricmp( key, "aero" ) )		g.air = Phys_Arg( tok, 1, g.aero ) * 0.5f;
	else if ( !Phys_Stricmp( key, "wind" ) )		g.air = Phys_Arg( tok, 1, g.wind );
	else if ( !Phys_Stricmp( key, "proxy" ) )		g.detail = 4.0f / Phys_Clamp( Phys_Arg( tok, 1, g.proxy ), 0.5f, 16.0f );
	// Old keywords that the solver does not use: read, ignored, not written.
	else if ( !Phys_Stricmp( key, "passback" ) )	{}
	// Old keywords of solver values: still read, not written.
	else if ( !Phys_Stricmp( key, "maxspeed" ) )	g.maxSpeed = Phys_Arg( tok, 1, g.maxSpeed );
	else if ( !Phys_Stricmp( key, "margin" ) )		g.margin = Phys_Arg( tok, 1, g.margin );
	else if ( !Phys_Stricmp( key, "iterations" ) )	g.iterations = (int)Phys_Arg( tok, 1, (float)g.iterations );
	else if ( print )
	{
		snprintf( msg, sizeof( msg ), "unknown keyword '%s'\n", key );
		print( msg );
	}

	Phys_DeriveGroup( g );
}

physDef_t *Phys_ParseDef( const char *text, const char *name, void ( *print )( const char *msg ) )
{
	physDef_t *def = new physDef_t;
	std::vector<std::string> tok;
	const char *p = text;

	def->name = name;

	while ( Phys_ReadLine( p, tok ) )
	{
		bool hair;

		if ( !Phys_Stricmp( tok[0].c_str(), "cloth" ) )
		{
			hair = false;
		}
		else if ( !Phys_Stricmp( tok[0].c_str(), "hair" ) )
		{
			hair = true;
		}
		else
		{
			if ( print )
				print( "expected 'cloth' or 'hair'\n" );

			continue;
		}

		physGroupDef_t g;
		Phys_InitGroup( g, hair );

		if ( tok.size() > 1 && tok[1] != "{" )
			g.name = tok[1];

		if ( tok.back() != "{" )
		{
			if ( !Phys_ReadLine( p, tok ) || tok[0] != "{" )
			{
				if ( print )
					print( "expected '{'\n" );

				break;
			}
		}

		bool closed = false;

		while ( Phys_ReadLine( p, tok ) )
		{
			if ( tok[0] == "}" )
			{
				closed = true;
				break;
			}

			Phys_ParseLine( g, tok, print );
		}

		if ( !closed && print )
			print( "missing '}'\n" );

		if ( g.surfaces.empty() )
		{
			if ( print )
				print( "group has no surfaces\n" );

			continue;
		}

		def->groups.push_back( g );
	}

	if ( def->groups.empty() )
	{
		delete def;
		return NULL;
	}

	return def;
}

//
// Vector helpers
//

static inline void Vec3Sub( const float *a, const float *b, float *out )
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

static inline float Vec3Dot( const float *a, const float *b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline void Vec3Normalize( float *v )
{
	const float len = sqrtf( Vec3Dot( v, v ) );

	if ( len > 1e-8f )
	{
		v[0] /= len;
		v[1] /= len;
		v[2] /= len;
	}
}

static inline float Vec3Dist( const float *a, const float *b )
{
	float d[3];

	Vec3Sub( a, b, d );
	return sqrtf( Vec3Dot( d, d ) );
}

//
// Topology
//

struct physQuantKey_t
{
	int x, y, z;

	bool operator<( const physQuantKey_t &o ) const
	{
		if ( x != o.x )	return x < o.x;
		if ( y != o.y )	return y < o.y;
		return z < o.z;
	}
};

static inline physQuantKey_t Phys_Quantize( const float *p, float scale )
{
	physQuantKey_t key;

	key.x = (int)floorf( p[0] * scale + 0.5f );
	key.y = (int)floorf( p[1] * scale + 0.5f );
	key.z = (int)floorf( p[2] * scale + 0.5f );
	return key;
}

void Phys_BuildEdges( physTopology_t &topo )
{
	std::map<std::pair<int, int>, int> edgeIndex;
	std::map<std::pair<int, int>, int> edgeOpposite;	// first opposite vertex seen on an edge
	const int numTris = (int)topo.tris.size() / 3;

	topo.edges.clear();
	topo.edgeRest.clear();
	topo.bends.clear();
	topo.bendRest.clear();
	topo.valence.assign( topo.numParticles, 0.0f );

	for ( int t = 0; t < numTris; t++ )
	{
		const int tri[3] = { topo.tris[t * 3], topo.tris[t * 3 + 1], topo.tris[t * 3 + 2] };

		for ( int k = 0; k < 3; k++ )
		{
			const int a = tri[k];
			const int b = tri[( k + 1 ) % 3];
			const int opposite = tri[( k + 2 ) % 3];
			const std::pair<int, int> key = a < b ? std::make_pair( a, b ) : std::make_pair( b, a );

			topo.valence[a] += 1.0f;

			if ( edgeIndex.find( key ) == edgeIndex.end() )
			{
				edgeIndex[key] = (int)topo.edges.size() / 2;
				topo.edges.push_back( key.first );
				topo.edges.push_back( key.second );
				topo.edgeRest.push_back( Vec3Dist( &topo.bindPos[key.first * 3], &topo.bindPos[key.second * 3] ) );
			}

			std::map<std::pair<int, int>, int>::iterator it = edgeOpposite.find( key );

			if ( it == edgeOpposite.end() )
			{
				edgeOpposite[key] = opposite;
			}
			else if ( it->second != opposite )
			{
				const int c = it->second;
				const int d = opposite;

				topo.bends.push_back( c );
				topo.bends.push_back( d );
				topo.bendRest.push_back( Vec3Dist( &topo.bindPos[c * 3], &topo.bindPos[d * 3] ) );
			}
		}
	}
}

void Phys_BuildTopology( physTopology_t &topo, const physSurfaceInput_t *surfaces, int numSurfaces )
{
	std::map<physQuantKey_t, int> weld;

	topo = physTopology_t();
	topo.vertParticle.resize( numSurfaces );

	// Vertices at the same bind position are one particle, also across surfaces.
	for ( int s = 0; s < numSurfaces; s++ )
	{
		const physSurfaceInput_t &in = surfaces[s];

		topo.vertParticle[s].resize( in.numVerts );

		for ( int i = 0; i < in.numVerts; i++ )
		{
			const float *p = in.xyz + i * 3;
			const physQuantKey_t key = Phys_Quantize( p, 100.0f );
			std::map<physQuantKey_t, int>::iterator it = weld.find( key );

			if ( it == weld.end() )
			{
				const int index = topo.numParticles++;

				weld[key] = index;
				topo.bindPos.push_back( p[0] );
				topo.bindPos.push_back( p[1] );
				topo.bindPos.push_back( p[2] );
				topo.firstVertex.push_back( ( s << 24 ) | i );
				topo.vertParticle[s][i] = index;
			}
			else
			{
				topo.vertParticle[s][i] = it->second;
			}
		}
	}

	for ( int s = 0; s < numSurfaces; s++ )
	{
		const physSurfaceInput_t &in = surfaces[s];

		for ( int t = 0; t < in.numTris; t++ )
		{
			int tri[3];

			for ( int k = 0; k < 3; k++ )
			{
				const int vert = in.tris[t * 3 + k];

				tri[k] = ( vert >= 0 && vert < in.numVerts ) ? topo.vertParticle[s][vert] : -1;
			}

			if ( tri[0] < 0 || tri[1] < 0 || tri[2] < 0 || tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2] )
				continue;

			topo.tris.push_back( tri[0] );
			topo.tris.push_back( tri[1] );
			topo.tris.push_back( tri[2] );
		}
	}

	Phys_BuildEdges( topo );
}

//
// Proxy mesh
//

#define PHYS_PROXY_LINKS	4

void Phys_BuildProxy( physProxy_t &px, const physTopology_t &full, const float *freeFactor, float cell )
{
	const int n = full.numParticles;
	std::map<physQuantKey_t, int> cells;
	std::vector<physQuantKey_t> keys;
	std::vector<float> sum;
	std::vector<float> freeSum;

	px = physProxy_t();

	if ( cell < 0.5f )
		cell = 0.5f;

	px.clusterOf.assign( n, 0 );

	for ( int i = 0; i < n; i++ )
	{
		const physQuantKey_t key = Phys_Quantize( &full.bindPos[i * 3], 1.0f / cell );
		std::map<physQuantKey_t, int>::iterator it = cells.find( key );
		int c;

		if ( it == cells.end() )
		{
			c = (int)keys.size();
			cells[key] = c;
			keys.push_back( key );
			sum.resize( sum.size() + 3, 0.0f );
			freeSum.push_back( 0.0f );
			px.memberCount.push_back( 0 );
		}
		else
		{
			c = it->second;
		}

		px.clusterOf[i] = c;
		px.memberCount[c]++;
		freeSum[c] += freeFactor[i];

		for ( int k = 0; k < 3; k++ )
			sum[c * 3 + k] += full.bindPos[i * 3 + k];
	}

	px.numProxy = (int)keys.size();
	px.rot.assign( px.numProxy * 4, 0.0f );

	for ( int c = 0; c < px.numProxy; c++ )
		px.rot[c * 4 + 3] = 1.0f;
	px.topo.numParticles = px.numProxy;
	px.topo.bindPos.resize( px.numProxy * 3 );
	px.freeFactor.resize( px.numProxy );

	// A cell with a fixed member is fixed when most of it is fixed. When no cell is fixed in that way,
	// every cell with a fixed member is fixed: the simulation mesh always keeps the anchors of the full mesh.
	std::vector<unsigned char> hasFixed( px.numProxy, 0 );

	for ( int i = 0; i < n; i++ )
	{
		if ( freeFactor[i] <= PHYS_MIN_FREE )
			hasFixed[px.clusterOf[i]] = 1;
	}

	bool anyFixed = false;

	for ( int c = 0; c < px.numProxy; c++ )
	{
		const float inv = 1.0f / (float)px.memberCount[c];

		for ( int k = 0; k < 3; k++ )
			px.topo.bindPos[c * 3 + k] = sum[c * 3 + k] * inv;

		px.freeFactor[c] = freeSum[c] * inv;

		if ( hasFixed[c] && px.freeFactor[c] <= 0.5f )
			px.freeFactor[c] = 0.0f;

		if ( px.freeFactor[c] <= PHYS_MIN_FREE )
			anyFixed = true;
	}

	if ( !anyFixed )
	{
		for ( int c = 0; c < px.numProxy; c++ )
		{
			if ( hasFixed[c] )
				px.freeFactor[c] = 0.0f;
		}
	}

	// Each full particle follows its nearest proxy particles.
	px.link.assign( n * PHYS_PROXY_LINKS, -1 );
	px.weight.assign( n * PHYS_PROXY_LINKS, 0.0f );

	for ( int i = 0; i < n; i++ )
	{
		const physQuantKey_t home = keys[px.clusterOf[i]];
		int best[PHYS_PROXY_LINKS];
		float bestDist[PHYS_PROXY_LINKS];

		for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
		{
			best[k] = -1;
			bestDist[k] = 1e30f;
		}

		for ( int dx = -1; dx <= 1; dx++ )
		{
			for ( int dy = -1; dy <= 1; dy++ )
			{
				for ( int dz = -1; dz <= 1; dz++ )
				{
					physQuantKey_t key = home;

					key.x += dx;
					key.y += dy;
					key.z += dz;

					std::map<physQuantKey_t, int>::iterator it = cells.find( key );

					if ( it == cells.end() )
						continue;

					float dist = Vec3Dist( &full.bindPos[i * 3], &px.topo.bindPos[it->second * 3] );

					int c = it->second;

					for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
					{
						if ( dist < bestDist[k] )
						{
							std::swap( dist, bestDist[k] );
							std::swap( c, best[k] );
						}
					}
				}
			}
		}

		float total = 0.0f;

		for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
		{
			if ( best[k] < 0 )
				continue;

			const float w = 1.0f / ( bestDist[k] * bestDist[k] + 1.0f );

			px.link[i * PHYS_PROXY_LINKS + k] = best[k];
			px.weight[i * PHYS_PROXY_LINKS + k] = w;
			total += w;
		}

		for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
			px.weight[i * PHYS_PROXY_LINKS + k] /= total;
	}

	// Triangles of the proxy mesh: the triangles of the full mesh with three different clusters.
	std::map<physQuantKey_t, int> seen;
	const int numTris = (int)full.tris.size() / 3;

	for ( int t = 0; t < numTris; t++ )
	{
		int c[3] = { px.clusterOf[full.tris[t * 3]], px.clusterOf[full.tris[t * 3 + 1]], px.clusterOf[full.tris[t * 3 + 2]] };

		if ( c[0] == c[1] || c[1] == c[2] || c[0] == c[2] )
			continue;

		int sorted[3] = { c[0], c[1], c[2] };

		if ( sorted[0] > sorted[1] )	std::swap( sorted[0], sorted[1] );
		if ( sorted[1] > sorted[2] )	std::swap( sorted[1], sorted[2] );
		if ( sorted[0] > sorted[1] )	std::swap( sorted[0], sorted[1] );

		physQuantKey_t key;

		key.x = sorted[0];
		key.y = sorted[1];
		key.z = sorted[2];

		if ( seen.find( key ) != seen.end() )
			continue;

		seen[key] = 1;
		px.topo.tris.push_back( c[0] );
		px.topo.tris.push_back( c[1] );
		px.topo.tris.push_back( c[2] );
	}

	Phys_BuildEdges( px.topo );

	// An edge of the full mesh between two cells links them too. A thin tube has one cell per slice and no
	// triangle with three cells: without this, its cells have no link.
	std::map<std::pair<int, int>, int> linked;
	std::map<int, std::vector<int> > extraNeighbors;

	for ( size_t e = 0; e < px.topo.edges.size(); e += 2 )
		linked[std::make_pair( px.topo.edges[e], px.topo.edges[e + 1] )] = 1;

	for ( size_t e = 0; e < full.edges.size(); e += 2 )
	{
		const int a = px.clusterOf[full.edges[e]];
		const int b = px.clusterOf[full.edges[e + 1]];

		if ( a == b )
			continue;

		const std::pair<int, int> key = a < b ? std::make_pair( a, b ) : std::make_pair( b, a );

		if ( linked.find( key ) != linked.end() )
			continue;

		linked[key] = 1;
		px.topo.edges.push_back( key.first );
		px.topo.edges.push_back( key.second );
		px.topo.edgeRest.push_back( Vec3Dist( &px.topo.bindPos[key.first * 3], &px.topo.bindPos[key.second * 3] ) );
		extraNeighbors[key.first].push_back( key.second );
		extraNeighbors[key.second].push_back( key.first );
	}

	// These links have no triangle, so no bend constraint. Two links that meet at a cell get one: a chain of
	// cells keeps its bend stiffness.
	std::map<std::pair<int, int>, int> bent;

	for ( size_t e = 0; e < px.topo.bends.size(); e += 2 )
	{
		const int a = px.topo.bends[e], b = px.topo.bends[e + 1];

		bent[a < b ? std::make_pair( a, b ) : std::make_pair( b, a )] = 1;
	}

	for ( std::map<int, std::vector<int> >::const_iterator it = extraNeighbors.begin(); it != extraNeighbors.end(); ++it )
	{
		const std::vector<int> &nb = it->second;

		for ( size_t i = 0; i < nb.size(); i++ )
		{
			for ( size_t j = i + 1; j < nb.size(); j++ )
			{
				const std::pair<int, int> key = nb[i] < nb[j] ? std::make_pair( nb[i], nb[j] ) : std::make_pair( nb[j], nb[i] );

				if ( linked.find( key ) != linked.end() || bent.find( key ) != bent.end() )
					continue;

				bent[key] = 1;
				px.topo.bends.push_back( key.first );
				px.topo.bends.push_back( key.second );
				px.topo.bendRest.push_back( Vec3Dist( &px.topo.bindPos[key.first * 3], &px.topo.bindPos[key.second * 3] ) );
			}
		}
	}
}

void Phys_ProxyReset( physProxy_t &px )
{
	px.rot.assign( px.numProxy * 4, 0.0f );

	for ( int c = 0; c < px.numProxy; c++ )
		px.rot[c * 4 + 3] = 1.0f;
}

void Phys_ProxyTargets( const physProxy_t &px, const float *fullTarget, float *proxyTarget )
{
	const int n = (int)px.clusterOf.size();

	memset( proxyTarget, 0, sizeof( float ) * 3 * px.numProxy );

	for ( int i = 0; i < n; i++ )
	{
		const int c = px.clusterOf[i];

		proxyTarget[c * 3 + 0] += fullTarget[i * 3 + 0];
		proxyTarget[c * 3 + 1] += fullTarget[i * 3 + 1];
		proxyTarget[c * 3 + 2] += fullTarget[i * 3 + 2];
	}

	for ( int c = 0; c < px.numProxy; c++ )
	{
		const float inv = 1.0f / (float)px.memberCount[c];

		proxyTarget[c * 3 + 0] *= inv;
		proxyTarget[c * 3 + 1] *= inv;
		proxyTarget[c * 3 + 2] *= inv;
	}
}

static void Phys_QuatToMatrix( const float *q, float m[3][3] )
{
	const float x = q[0], y = q[1], z = q[2], w = q[3];

	m[0][0] = 1.0f - 2.0f * ( y * y + z * z );	m[0][1] = 2.0f * ( x * y - z * w );			m[0][2] = 2.0f * ( x * z + y * w );
	m[1][0] = 2.0f * ( x * y + z * w );			m[1][1] = 1.0f - 2.0f * ( x * x + z * z );	m[1][2] = 2.0f * ( y * z - x * w );
	m[2][0] = 2.0f * ( x * z - y * w );			m[2][1] = 2.0f * ( y * z + x * w );			m[2][2] = 1.0f - 2.0f * ( x * x + y * y );
}

// The rotation part of A (A = sum of solved offset * target offset^T). q is the start and the result.
// Mueller et al. 2016, "A robust method to extract the rotational part of deformations".
static void Phys_ExtractRotation( const float *A, float *q, int iterations )
{
	for ( int it = 0; it < iterations; it++ )
	{
		float R[3][3];
		float omega[3] = { 0.0f, 0.0f, 0.0f };
		float dot = 0.0f;

		Phys_QuatToMatrix( q, R );

		for ( int k = 0; k < 3; k++ )
		{
			const float r[3] = { R[0][k], R[1][k], R[2][k] };
			const float c[3] = { A[0 * 3 + k], A[1 * 3 + k], A[2 * 3 + k] };

			omega[0] += r[1] * c[2] - r[2] * c[1];
			omega[1] += r[2] * c[0] - r[0] * c[2];
			omega[2] += r[0] * c[1] - r[1] * c[0];
			dot += r[0] * c[0] + r[1] * c[1] + r[2] * c[2];
		}

		const float scale = 1.0f / ( fabsf( dot ) + 1e-9f );

		omega[0] *= scale;
		omega[1] *= scale;
		omega[2] *= scale;

		const float angle = sqrtf( Vec3Dot( omega, omega ) );

		if ( angle < 1e-6f )
			break;

		const float sn = sinf( angle * 0.5f ) / angle;
		const float d[4] = { omega[0] * sn, omega[1] * sn, omega[2] * sn, cosf( angle * 0.5f ) };
		const float r[4] = {
			d[3] * q[0] + q[3] * d[0] + d[1] * q[2] - d[2] * q[1],
			d[3] * q[1] + q[3] * d[1] + d[2] * q[0] - d[0] * q[2],
			d[3] * q[2] + q[3] * d[2] + d[0] * q[1] - d[1] * q[0],
			d[3] * q[3] - d[0] * q[0] - d[1] * q[1] - d[2] * q[2] };
		const float len = sqrtf( r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3] );

		for ( int k = 0; k < 4; k++ )
			q[k] = r[k] / len;
	}
}

#define PHYS_SHAPE_ITERATIONS	8
#define PHYS_SHAPE_MAX			0.9f	// shape 1 moves 90 % of the way to the goal in one step: a zone stays a bit soft

void Phys_ProxyApply( physProxy_t &px, const float *proxyX, const float *proxyTarget,
					  const float *fullTarget, const float *freeFactor, float *outPos )
{
	const int n = (int)px.clusterOf.size();
	const int numEdges = (int)px.topo.edges.size() / 2;
	const bool turn = numEdges > 0 && (int)px.rot.size() == px.numProxy * 4;
	// The turn of each proxy particle: the rotation from the target offsets of its neighbors to their
	// solved offsets. The rotation of the last frame is the start.
	if ( turn )
	{
		std::vector<float> &cov = px.cov;

		cov.assign( px.numProxy * 9, 0.0f );

		for ( int e = 0; e < numEdges; e++ )
		{
			const int a = px.topo.edges[e * 2];
			const int b = px.topo.edges[e * 2 + 1];
			float p[3], t[3];

			Vec3Sub( &proxyX[b * 3], &proxyX[a * 3], p );
			Vec3Sub( &proxyTarget[b * 3], &proxyTarget[a * 3], t );

			for ( int r = 0; r < 3; r++ )
			{
				for ( int c = 0; c < 3; c++ )
				{
					cov[a * 9 + r * 3 + c] += p[r] * t[c];
					cov[b * 9 + r * 3 + c] += p[r] * t[c];
				}
			}
		}

		px.turn.resize( px.numProxy * 9 );

		for ( int c = 0; c < px.numProxy; c++ )
		{
			float m[3][3];

			Phys_ExtractRotation( &cov[c * 9], &px.rot[c * 4], PHYS_SHAPE_ITERATIONS );
			Phys_QuatToMatrix( &px.rot[c * 4], m );
			memcpy( &px.turn[c * 9], m, sizeof( m ) );
		}
	}

	for ( int i = 0; i < n; i++ )
	{
		const float *t = &fullTarget[i * 3];
		float d[3] = { 0.0f, 0.0f, 0.0f };

		for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
		{
			const int c = px.link[i * PHYS_PROXY_LINKS + k];

			if ( c < 0 )
				continue;

			const float w = px.weight[i * PHYS_PROXY_LINKS + k];

			d[0] += w * ( proxyX[c * 3 + 0] - proxyTarget[c * 3 + 0] );
			d[1] += w * ( proxyX[c * 3 + 1] - proxyTarget[c * 3 + 1] );
			d[2] += w * ( proxyX[c * 3 + 2] - proxyTarget[c * 3 + 2] );

			// The offset from the proxy particle turns with it.
			if ( turn )
			{
				const float *m = &px.turn[c * 9];
				const float rel[3] = { t[0] - proxyTarget[c * 3 + 0], t[1] - proxyTarget[c * 3 + 1], t[2] - proxyTarget[c * 3 + 2] };

				for ( int r = 0; r < 3; r++ )
					d[r] += w * ( m[r * 3 + 0] * rel[0] + m[r * 3 + 1] * rel[1] + m[r * 3 + 2] * rel[2] - rel[r] );
			}
		}

		// A fixed vertex stays on the animation.
		float f = freeFactor[i] * 3.0f;

		if ( f > 1.0f )
			f = 1.0f;

		outPos[i * 3 + 0] = t[0] + d[0] * f;
		outPos[i * 3 + 1] = t[1] + d[1] * f;
		outPos[i * 3 + 2] = t[2] + d[2] * f;
	}
}

//
// Solver
//
// Extended position based dynamics on the simulation mesh. A constraint has a compliance, so the
// stiffness does not depend on the number of iterations or on the frame rate.
//
// The model of motion is the model of NvCloth (the solver of the Unity cloth):
// - the cloth takes a part of the change of the speed of the model frame (inertia)
// - the damping reduces the speed relative to the model frame
// - the animation acts only through the fixed particles, the reach, the tethers and the colliders
//

// Compliance of a constraint with stiffness 1 / 1000 (soft). A stiffness of 1 is rigid.
#define PHYS_STRETCH_COMPLIANCE	2e-5f
#define PHYS_BEND_COMPLIANCE	2e-4f
#define PHYS_MAX_FRAME_SPEED	2000.0f	// a faster model frame is a teleport: its speed change is not given to the cloth
#define PHYS_MAX_FRAME_TURN		1.0f	// a larger turn in one frame, radians, is a teleport too
#define PHYS_FRAME_SMOOTH		0.1f	// time constant of the low-pass on the model frame, seconds
#define PHYS_TELEPORT_MIN_DT	0.05f	// the teleport test uses a frame time of this length at least, seconds

static inline float Phys_Compliance( float stiffness, float scale )
{
	if ( stiffness >= 1.0f )
		return 0.0f;

	if ( stiffness < 0.001f )
		stiffness = 0.001f;

	return ( 1.0f / stiffness - 1.0f ) * scale;
}

// The entity axes carry the model scale (ScaleModelAxis). out gets the unit axes. Returns the mean scale.
static float Phys_NormalizeAxes( const float in[3][3], float out[3][3] )
{
	float sum = 0.0f;

	for ( int k = 0; k < 3; k++ )
	{
		const float len = sqrtf( Vec3Dot( in[k], in[k] ) );
		const float inv = len > 1e-6f ? 1.0f / len : 0.0f;

		out[k][0] = in[k][0] * inv;
		out[k][1] = in[k][1] * inv;
		out[k][2] = in[k][2] * inv;
		sum += len;
	}

	return sum > 1e-6f ? sum / 3.0f : 1.0f;
}

// Tethers (long range attachments): each free particle stays within its distance from the nearest pinned
// particle, as the geodesic tethers of NvCloth. The distance is along the edges of the mesh in the bind pose: a
// straight line is shorter than a cloth that is curved in the bind pose (a cape on the back), and then the tether
// pulls the cloth through the body against the edges and the colliders.
static void Phys_BuildTethers( physCloth_t &cloth, const physTopology_t &topo )
{
	const int n = cloth.numParticles;

	cloth.tether.assign( n, -1 );
	cloth.tetherLength.assign( n, 0.0f );

	if ( (int)topo.bindPos.size() != n * 3 )
		return;

	// Neighbors of each particle, from the edges.
	const int numEdges = (int)topo.edgeRest.size();
	std::vector<int> adjStart( n + 1, 0 ), adj( numEdges * 2 );
	std::vector<float> adjLen( numEdges * 2 );

	for ( int e = 0; e < numEdges; e++ )
	{
		adjStart[topo.edges[e * 2] + 1]++;
		adjStart[topo.edges[e * 2 + 1] + 1]++;
	}

	for ( int i = 0; i < n; i++ )
		adjStart[i + 1] += adjStart[i];

	std::vector<int> fill( adjStart.begin(), adjStart.end() - 1 );

	for ( int e = 0; e < numEdges; e++ )
	{
		const int a = topo.edges[e * 2], b = topo.edges[e * 2 + 1];

		adj[fill[a]] = b;
		adjLen[fill[a]++] = topo.edgeRest[e];
		adj[fill[b]] = a;
		adjLen[fill[b]++] = topo.edgeRest[e];
	}

	// Shortest path from all the pinned particles (Dijkstra with several sources).
	std::vector<float> dist( n, 1e30f );
	std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int> >, std::greater<std::pair<float, int> > > open;

	for ( int i = 0; i < n; i++ )
	{
		if ( cloth.pinned[i] )
		{
			dist[i] = 0.0f;
			cloth.tether[i] = i;
			open.push( std::make_pair( 0.0f, i ) );
		}
	}

	while ( !open.empty() )
	{
		const std::pair<float, int> top = open.top();

		open.pop();

		if ( top.first > dist[top.second] )
			continue;

		for ( int k = adjStart[top.second]; k < adjStart[top.second + 1]; k++ )
		{
			const int j = adj[k];
			const float d = top.first + adjLen[k];

			if ( d < dist[j] )
			{
				dist[j] = d;
				cloth.tether[j] = cloth.tether[top.second];
				open.push( std::make_pair( d, j ) );
			}
		}
	}

	// A particle with no path to a pinned particle has no tether.
	for ( int i = 0; i < n; i++ )
	{
		if ( cloth.pinned[i] || dist[i] >= 1e29f )
			cloth.tether[i] = -1;
		else
			cloth.tetherLength[i] = dist[i];
	}
}

void Phys_ClothInit( physCloth_t &cloth, const physGroupDef_t &def, const physTopology_t &topo, const float *freeFactor )
{
	const int numParticles = topo.numParticles;

	cloth.numParticles = numParticles;
	cloth.valid = false;
	cloth.x.assign( numParticles * 3, 0.0f );
	cloth.v.assign( numParticles * 3, 0.0f );
	cloth.prevTarget.assign( numParticles * 3, 0.0f );
	cloth.offPrev.assign( numParticles * 3, 0.0f );
	cloth.offCurr.assign( numParticles * 3, 0.0f );
	cloth.drawX.assign( numParticles * 3, 0.0f );
	cloth.accum = 0.0f;
	cloth.maxDist.assign( numParticles, 0.0f );
	cloth.pinned.assign( numParticles, 1 );
	cloth.loose = true;

	for ( int i = 0; i < numParticles; i++ )
	{
		const float f = freeFactor[i];

		if ( f > PHYS_MIN_FREE )
		{
			cloth.pinned[i] = 0;
			cloth.maxDist[i] = def.maxDist * f;
		}
		else
		{
			cloth.loose = false;
		}
	}

	Phys_BuildTethers( cloth, topo );

	cloth.p.assign( numParticles * 3, 0.0f );
	cloth.tgt.assign( numParticles * 3, 0.0f );
	cloth.acc.clear();
	cloth.targetNormal.clear();
	cloth.lambda.clear();
	cloth.lambdaFollow.assign( numParticles, 0.0f );
	cloth.adjStart.clear();
	cloth.adj.clear();
	cloth.shapeRot.assign( numParticles * 4, 0.0f );

	for ( int i = 0; i < numParticles; i++ )
		cloth.shapeRot[i * 4 + 3] = 1.0f;
}

// The model frame of the cloth: its pinned particles, fitted as one rigid piece to the bind pose, as the
// reference bone of the Chaos cloth. The entity frame follows the legs, and the upper body turns against it,
// so a turn of the entity frame is not a turn of the cloth. With less than 3 pinned particles off a line,
// the frame is the entity frame. q: the start of the fit and its result.
static void Phys_ClothFrame( physCloth_t &cloth, const physTopology_t &topo, const float *target, const float origin[3],
							 const float axis[3][3], int iterations, float outOrigin[3], float outAxis[3][3] )
{
	const int n = cloth.numParticles;

	// A reference bone is rigid: the fit of the pinned particles also turns with the skin around them (a collar
	// with the head), and that turn drags the free particles through the damping.
	if ( cloth.hasReference )
	{
		Phys_NormalizeAxes( cloth.refAxis, outAxis );
		memcpy( outOrigin, cloth.refOrigin, sizeof( float ) * 3 );
		return;
	}

	Phys_NormalizeAxes( axis, outAxis );
	memcpy( outOrigin, origin, sizeof( float ) * 3 );

	if ( (int)topo.bindPos.size() != n * 3 )
		return;

	const float *bind = &topo.bindPos[0];
	float cw[3] = { 0.0f, 0.0f, 0.0f }, cb[3] = { 0.0f, 0.0f, 0.0f };
	int count = 0;

	for ( int i = 0; i < n; i++ )
	{
		if ( !cloth.pinned[i] )
			continue;

		for ( int k = 0; k < 3; k++ )
		{
			cw[k] += target[i * 3 + k];
			cb[k] += bind[i * 3 + k];
		}

		count++;
	}

	if ( count < 3 )
		return;

	for ( int k = 0; k < 3; k++ )
	{
		cw[k] /= (float)count;
		cb[k] /= (float)count;
	}

	// The pins must not be on a line: else the turn around that line is not known.
	float far[3] = { 0.0f, 0.0f, 0.0f }, far2 = 0.0f, area2 = 0.0f;

	for ( int i = 0; i < n; i++ )
	{
		if ( !cloth.pinned[i] )
			continue;

		float d[3];
		Vec3Sub( &bind[i * 3], cb, d );

		if ( Vec3Dot( d, d ) > far2 )
		{
			far2 = Vec3Dot( d, d );
			memcpy( far, d, sizeof( far ) );
		}
	}

	for ( int i = 0; i < n; i++ )
	{
		if ( !cloth.pinned[i] )
			continue;

		float d[3];
		Vec3Sub( &bind[i * 3], cb, d );

		const float x[3] = { far[1] * d[2] - far[2] * d[1], far[2] * d[0] - far[0] * d[2], far[0] * d[1] - far[1] * d[0] };

		if ( Vec3Dot( x, x ) > area2 )
			area2 = Vec3Dot( x, x );
	}

	if ( area2 < 1e-4f * far2 * far2 )
		return;

	float A[9] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

	for ( int i = 0; i < n; i++ )
	{
		if ( !cloth.pinned[i] )
			continue;

		float dw[3], db[3];
		Vec3Sub( &target[i * 3], cw, dw );
		Vec3Sub( &bind[i * 3], cb, db );

		for ( int r = 0; r < 3; r++ )
		{
			for ( int q = 0; q < 3; q++ )
				A[r * 3 + q] += dw[r] * db[q];
		}
	}

	float R[3][3];

	Phys_ExtractRotation( A, cloth.frameQ, iterations );
	Phys_QuatToMatrix( cloth.frameQ, R );

	// An axis row k is the direction of the model axis k in world space: column k of R.
	for ( int k = 0; k < 3; k++ )
	{
		for ( int r = 0; r < 3; r++ )
			outAxis[k][r] = R[r][k];
	}

	memcpy( outOrigin, cw, sizeof( cw ) );
}

void Phys_ClothSetReference( physCloth_t &cloth, const float *origin, const float axis[3][3] )
{
	cloth.hasReference = origin != NULL;

	if ( !origin )
		return;

	memcpy( cloth.refOrigin, origin, sizeof( cloth.refOrigin ) );
	memcpy( cloth.refAxis, axis, sizeof( cloth.refAxis ) );
}

void Phys_ClothReset( physCloth_t &cloth, const physTopology_t &topo, const float *target, const float origin[3], const float axis[3][3] )
{
	if ( cloth.numParticles == 0 )
		return;

	memcpy( &cloth.x[0], target, sizeof( float ) * 3 * cloth.numParticles );
	memcpy( &cloth.prevTarget[0], target, sizeof( float ) * 3 * cloth.numParticles );
	cloth.offPrev.assign( cloth.numParticles * 3, 0.0f );
	cloth.offCurr.assign( cloth.numParticles * 3, 0.0f );
	memcpy( &cloth.drawX[0], target, sizeof( float ) * 3 * cloth.numParticles );
	cloth.accum = 0.0f;
	memset( &cloth.v[0], 0, sizeof( float ) * 3 * cloth.numParticles );

	// The fit starts from no turn: more iterations than in a step, which starts from the last result.
	cloth.frameQ[0] = cloth.frameQ[1] = cloth.frameQ[2] = 0.0f;
	cloth.frameQ[3] = 1.0f;
	Phys_ClothFrame( cloth, topo, target, origin, axis, 50, cloth.frameOrigin, cloth.frameAxis );
	memcpy( cloth.rawOrigin, cloth.frameOrigin, sizeof( cloth.rawOrigin ) );
	memcpy( cloth.rawAxis, cloth.frameAxis, sizeof( cloth.rawAxis ) );
	memcpy( cloth.smoothOrigin, cloth.frameOrigin, sizeof( cloth.smoothOrigin ) );
	memcpy( cloth.smoothAxis, cloth.frameAxis, sizeof( cloth.smoothAxis ) );
	memset( cloth.frameVel, 0, sizeof( cloth.frameVel ) );
	memset( cloth.frameOmega, 0, sizeof( cloth.frameOmega ) );
	memcpy( cloth.appliedOrigin, cloth.frameOrigin, sizeof( cloth.appliedOrigin ) );
	memset( cloth.appliedVel, 0, sizeof( cloth.appliedVel ) );
	memset( cloth.appliedOmega, 0, sizeof( cloth.appliedOmega ) );
	cloth.frameSpeedValid = false;
	cloth.shapeRot.assign( cloth.numParticles * 4, 0.0f );

	for ( int i = 0; i < cloth.numParticles; i++ )
		cloth.shapeRot[i * 4 + 3] = 1.0f;

	cloth.valid = true;
}

// XPBD distance constraint between particles a and b. A pinned particle does not move.
static inline void Phys_SolveDistance( float *p, const unsigned char *pinned, int a, int b, float rest, float alpha, float &lambda )
{
	const float wa = pinned[a] ? 0.0f : 1.0f;
	const float wb = pinned[b] ? 0.0f : 1.0f;

	if ( wa + wb == 0.0f )
		return;

	float d[3];
	Vec3Sub( p + a * 3, p + b * 3, d );

	const float len = sqrtf( Vec3Dot( d, d ) );

	if ( len < 1e-5f )
		return;

	const float dl = ( -( len - rest ) - alpha * lambda ) / ( wa + wb + alpha );
	const float k = dl / len;

	lambda += dl;

	for ( int c = 0; c < 3; c++ )
	{
		p[a * 3 + c] += d[c] * k * wa;
		p[b * 3 + c] -= d[c] * k * wb;
	}
}

// The point of the segment ab that is nearest to pos. Returns the squared distance.
static inline float Phys_NearestOnSegment( const float *pos, const float *a, const float *b, float *q )
{
	float ab[3], ap[3];

	Vec3Sub( b, a, ab );
	Vec3Sub( pos, a, ap );

	const float len2 = Vec3Dot( ab, ab );
	float t = len2 > 1e-6f ? Vec3Dot( ap, ab ) / len2 : 0.0f;

	if ( t < 0.0f )			t = 0.0f;
	else if ( t > 1.0f )	t = 1.0f;

	q[0] = a[0] + ab[0] * t;
	q[1] = a[1] + ab[1] * t;
	q[2] = a[2] + ab[2] * t;

	float d[3];
	Vec3Sub( pos, q, d );
	return Vec3Dot( d, d );
}

// Signed distance of pos from the core shape: the segment or the inner box, < 0 inside the inner box.
// normal gets the direction out of the core. Returns false in hasNormal when pos is on the segment.
static float Phys_CoreDistance( const physCollider_w &c, const float *pos, float *normal, bool &hasNormal )
{
	if ( c.shape == PHYS_SHAPE_BOX )
	{
		float d[3], local[3], out[3];

		Vec3Sub( pos, c.a, d );

		for ( int k = 0; k < 3; k++ )
		{
			local[k] = Vec3Dot( d, c.axis[k] );

			const float clamped = local[k] < -c.half[k] ? -c.half[k] : ( local[k] > c.half[k] ? c.half[k] : local[k] );

			out[k] = local[k] - clamped;
		}

		const float out2 = Vec3Dot( out, out );

		hasNormal = true;

		if ( out2 > 1e-10f )
		{
			const float dist = sqrtf( out2 );

			for ( int k = 0; k < 3; k++ )
				normal[k] = ( c.axis[0][k] * out[0] + c.axis[1][k] * out[1] + c.axis[2][k] * out[2] ) / dist;

			return dist;
		}

		// Inside the inner box: the nearest face gives the way out.
		int best = 0;
		float bestGap = 1e30f;

		for ( int k = 0; k < 3; k++ )
		{
			const float gap = c.half[k] - fabsf( local[k] );

			if ( gap < bestGap )
			{
				bestGap = gap;
				best = k;
			}
		}

		const float sign = local[best] >= 0.0f ? 1.0f : -1.0f;

		for ( int k = 0; k < 3; k++ )
			normal[k] = c.axis[best][k] * sign;

		return -bestGap;
	}

	float q[3];
	const float dist = sqrtf( Phys_NearestOnSegment( pos, c.a, c.b, q ) );

	hasNormal = dist > 1e-5f;

	for ( int k = 0; k < 3; k++ )
		normal[k] = hasNormal ? ( pos[k] - q[k] ) / dist : 0.0f;

	return dist;
}

float Phys_ColliderDistance( const physCollider_w &c, const float *pos, float *normal )
{
	bool hasNormal;

	return Phys_CoreDistance( c, pos, normal, hasNormal ) - c.radius;
}

void Phys_RestDistances( const float *bindPos, int n, const physCollider_w *bindColliders, int numColliders, std::vector<float> &out )
{
	out.resize( n * numColliders );

	for ( int i = 0; i < n; i++ )
	{
		for ( int c = 0; c < numColliders; c++ )
		{
			float normal[3];
			bool hasNormal;
			const float dist = Phys_CoreDistance( bindColliders[c], bindPos + i * 3, normal, hasNormal );

			out[i * numColliders + c] = dist > 0.0f ? dist : 0.0f;
		}
	}
}

void Phys_ClothRestDistances( physCloth_t &cloth, const physTopology_t &topo, const physCollider_w *bindColliders, int numColliders )
{
	Phys_RestDistances( topo.bindPos.empty() ? NULL : &topo.bindPos[0], topo.numParticles, bindColliders, numColliders, cloth.restDist );
}

// Puts pos on the surface of the collider (core + radius), on the side that the direction side points to.
// A capsule keeps the position of pos along its axis. A box uses the face that side points to most.
static void Phys_ExitOnSide( float *pos, const physCollider_w &c, const float *side, float radius )
{
	if ( c.shape == PHYS_SHAPE_BOX )
	{
		float d[3], local[3];
		int best = 0;
		float bestDot = 0.0f;

		Vec3Sub( pos, c.a, d );

		for ( int k = 0; k < 3; k++ )
		{
			local[k] = Vec3Dot( d, c.axis[k] );

			if ( local[k] < -c.half[k] )		local[k] = -c.half[k];
			else if ( local[k] > c.half[k] )	local[k] = c.half[k];

			const float dot = Vec3Dot( side, c.axis[k] );

			if ( fabsf( dot ) > fabsf( bestDot ) )
			{
				bestDot = dot;
				best = k;
			}
		}

		local[best] = bestDot >= 0.0f ? c.half[best] + radius : -c.half[best] - radius;

		for ( int k = 0; k < 3; k++ )
			pos[k] = c.a[k] + c.axis[0][k] * local[0] + c.axis[1][k] * local[1] + c.axis[2][k] * local[2];
		return;
	}

	float q[3], ab[3], dir[3];

	Phys_NearestOnSegment( pos, c.a, c.b, q );
	Vec3Sub( c.b, c.a, ab );

	// The radial part of side, so the particle does not slide along the capsule.
	const float len2 = Vec3Dot( ab, ab );
	const float along = len2 > 1e-6f ? Vec3Dot( side, ab ) / len2 : 0.0f;

	for ( int k = 0; k < 3; k++ )
		dir[k] = side[k] - ab[k] * along;

	float dirLen = sqrtf( Vec3Dot( dir, dir ) );

	if ( dirLen < 0.1f )
	{
		dir[0] = side[0];
		dir[1] = side[1];
		dir[2] = side[2];
		dirLen = 1.0f;
	}

	for ( int k = 0; k < 3; k++ )
		pos[k] = q[k] + dir[k] / dirLen * radius;
}

// Pushes pos out of one collider. rest: distance from the core in the bind pose, < 0 = none. A particle that is
// inside the collider in the bind pose can stay at that depth, so a collider of the stock skeleton does not push
// the rest shape of the cloth out.
// A particle deep inside, on the side opposite to its position at the start of the step (prev), went through the
// collider in this step. It leaves on the side it comes from, not on the nearest side. The side of the animated
// position is not used: when the animation puts a limb through the cloth, neighbor particles have their animated
// positions on the two sides, leave on the two sides, and the edges between them pull across the limb.
// prev NULL: the nearest side.
static bool Phys_Collide( float *pos, const float *tgt, const float *prev, const physCollider_w &c, float margin, float rest, float *crossMax )
{
	float radius = c.radius + margin;

	if ( rest >= 0.0f && rest < radius )
		radius = rest;

	if ( radius < 1e-3f )
		return false;

	float n[3];
	bool hasN;
	const float dist = Phys_CoreDistance( c, pos, n, hasN );

	if ( dist >= radius )
		return false;

	float nS[3];
	bool hasNS = false;
	const float distS = prev ? Phys_CoreDistance( c, prev, nS, hasNS ) : 0.0f;

	if ( hasN && hasNS && dist < radius * 0.5f && distS >= radius * 0.5f && Vec3Dot( n, nS ) < 0.0f )
	{
		const float before[3] = { pos[0], pos[1], pos[2] };

		Phys_ExitOnSide( pos, c, nS, radius );

		// ph_debug: the largest move across a collider.
		if ( crossMax )
		{
			const float d = Vec3Dist( before, pos );

			*crossMax = d > *crossMax ? d : *crossMax;
		}

		return true;
	}

	if ( !hasN )
	{
		// On the axis: leave on the side the particle comes from, else on the side of the animated position.
		float nT[3];
		bool hasNT;

		Phys_CoreDistance( c, tgt, nT, hasNT );

		const float *side = hasNS ? nS : ( hasNT ? nT : NULL );

		n[0] = side ? side[0] : 0.0f;
		n[1] = side ? side[1] : -1.0f;
		n[2] = side ? side[2] : 0.0f;
	}

	const float push = radius - dist;

	pos[0] += n[0] * push;
	pos[1] += n[1] * push;
	pos[2] += n[2] * push;
	return true;
}

// All colliders and the floor, for one particle. rest: its rest distances (numColliders), NULL = none.
static bool Phys_CollideAll( float *pos, const float *tgt, const float *prev, float margin, const physCollider_w *colliders, int numColliders,
							 const float *rest, bool hasGround, float groundZ, float *crossMax = NULL )
{
	bool hit = false;

	for ( int c = 0; c < numColliders; c++ )
		hit |= Phys_Collide( pos, tgt, prev, colliders[c], margin, rest ? rest[c] : -1.0f, crossMax );

	if ( hasGround )
	{
		float floorZ = groundZ + margin;

		if ( tgt[2] < floorZ )
			floorZ = tgt[2];

		if ( pos[2] < floorZ )
		{
			pos[2] = floorZ;
			hit = true;
		}
	}

	return hit;
}

void Phys_ProjectColliders( float *pos, const float *target, const float *freeFactor, int n, float margin,
							const physCollider_w *colliders, int numColliders, const float *restDist, bool hasGround, float groundZ )
{
	for ( int i = 0; i < n; i++ )
	{
		if ( freeFactor[i] > PHYS_MIN_FREE )
			Phys_CollideAll( pos + i * 3, target + i * 3, NULL, margin, colliders, numColliders,
				restDist ? restDist + i * numColliders : NULL, hasGround, groundZ );
	}
}

static inline void Phys_ClampRelative( float *v, const float *animVel, float maxSpeed )
{
	float r[3] = { v[0] - animVel[0], v[1] - animVel[1], v[2] - animVel[2] };
	const float len2 = Vec3Dot( r, r );

	if ( len2 > maxSpeed * maxSpeed )
	{
		const float scale = maxSpeed / sqrtf( len2 );

		r[0] *= scale;
		r[1] *= scale;
		r[2] *= scale;
	}

	v[0] = animVel[0] + r[0];
	v[1] = animVel[1] + r[1];
	v[2] = animVel[2] + r[2];
}

// Shape matching: each particle and its neighbors form a zone. The zone takes the animated shape of its
// particles (tgt), turned to fit the solved positions, and each free particle moves toward its goal.
// Muller et al. 2005, "Meshless deformations based on shape matching".
static void Phys_SolveShape( physCloth_t &cloth, float *p, const float *tgt, float k )
{
	const int n = cloth.numParticles;
	float *goal = &cloth.shapeGoal[0];
	float *count = &cloth.shapeCount[0];

	memset( goal, 0, sizeof( float ) * 3 * n );
	memset( count, 0, sizeof( float ) * n );

	for ( int c = 0; c < n; c++ )
	{
		const int first = cloth.adjStart[c], last = cloth.adjStart[c + 1];

		if ( last - first < 1 )
			continue;

		float pc[3] = { p[c * 3], p[c * 3 + 1], p[c * 3 + 2] };
		float tc[3] = { tgt[c * 3], tgt[c * 3 + 1], tgt[c * 3 + 2] };

		for ( int j = first; j < last; j++ )
		{
			const int o = cloth.adj[j];

			for ( int a = 0; a < 3; a++ )
			{
				pc[a] += p[o * 3 + a];
				tc[a] += tgt[o * 3 + a];
			}
		}

		const float inv = 1.0f / (float)( last - first + 1 );

		for ( int a = 0; a < 3; a++ )
		{
			pc[a] *= inv;
			tc[a] *= inv;
		}

		float A[9] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

		for ( int j = first - 1; j < last; j++ )
		{
			const int o = j < first ? c : cloth.adj[j];
			const float dp[3] = { p[o * 3] - pc[0], p[o * 3 + 1] - pc[1], p[o * 3 + 2] - pc[2] };
			const float dt[3] = { tgt[o * 3] - tc[0], tgt[o * 3 + 1] - tc[1], tgt[o * 3 + 2] - tc[2] };

			for ( int r = 0; r < 3; r++ )
			{
				for ( int q = 0; q < 3; q++ )
					A[r * 3 + q] += dp[r] * dt[q];
			}
		}

		float R[3][3];

		Phys_ExtractRotation( A, &cloth.shapeRot[c * 4], 3 );
		Phys_QuatToMatrix( &cloth.shapeRot[c * 4], R );

		for ( int j = first - 1; j < last; j++ )
		{
			const int o = j < first ? c : cloth.adj[j];
			const float dt[3] = { tgt[o * 3] - tc[0], tgt[o * 3 + 1] - tc[1], tgt[o * 3 + 2] - tc[2] };

			for ( int r = 0; r < 3; r++ )
				goal[o * 3 + r] += pc[r] + R[r][0] * dt[0] + R[r][1] * dt[1] + R[r][2] * dt[2];

			count[o] += 1.0f;
		}
	}

	for ( int i = 0; i < n; i++ )
	{
		if ( cloth.pinned[i] || count[i] == 0.0f )
			continue;

		for ( int a = 0; a < 3; a++ )
			p[i * 3 + a] += ( goal[i * 3 + a] / count[i] - p[i * 3 + a] ) * k;
	}
}

// The colliders at the time t (0 = last frame, 1 = this frame).
static void Phys_BlendColliders( const physCollider_w *from, const physCollider_w *to, int num, float t, physCollider_w *out )
{
	for ( int c = 0; c < num; c++ )
	{
		out[c] = to[c];

		for ( int k = 0; k < 3; k++ )
		{
			out[c].a[k] = from[c].a[k] + ( to[c].a[k] - from[c].a[k] ) * t;
			out[c].b[k] = from[c].b[k] + ( to[c].b[k] - from[c].b[k] ) * t;
		}

		if ( to[c].shape != PHYS_SHAPE_BOX )
			continue;

		for ( int j = 0; j < 3; j++ )
		{
			float *ax = out[c].axis[j];

			for ( int k = 0; k < 3; k++ )
				ax[k] = from[c].axis[j][k] + ( to[c].axis[j][k] - from[c].axis[j][k] ) * t;

			const float len = sqrtf( Vec3Dot( ax, ax ) );

			if ( len > 1e-6f )
			{
				ax[0] /= len;
				ax[1] /= len;
				ax[2] /= len;
			}
			else
			{
				memcpy( ax, to[c].axis[j], sizeof( float ) * 3 );
			}
		}
	}
}

// Turn from the unit axes of the last frame to the unit axes of this frame. An axis row k is the direction of
// the model axis k in world space. R gets the turn in world space, omega the turn speed in radians per second.
// Returns the angle of the turn, radians.
static float Phys_FrameTurn( const float prev[3][3], const float cur[3][3], float dt, float R[3][3], float *omega )
{
	// R = sum over k of cur[k] * prev[k]^T turns the last frame onto this frame.
	for ( int r = 0; r < 3; r++ )
	{
		for ( int c = 0; c < 3; c++ )
			R[r][c] = cur[0][r] * prev[0][c] + cur[1][r] * prev[1][c] + cur[2][r] * prev[2][c];
	}

	const float w[3] = { 0.5f * ( R[2][1] - R[1][2] ), 0.5f * ( R[0][2] - R[2][0] ), 0.5f * ( R[1][0] - R[0][1] ) };
	const float s = sqrtf( Vec3Dot( w, w ) );
	const float cosA = 0.5f * ( R[0][0] + R[1][1] + R[2][2] - 1.0f );
	const float angle = atan2f( s, cosA );

	// At a half turn the axis is not known: the angle is still correct, so the frame is a teleport.
	for ( int k = 0; k < 3; k++ )
		omega[k] = s > 1e-7f ? w[k] / s * angle / dt : 0.0f;

	return angle;
}

static inline void Phys_Rotate( const float R[3][3], float *v )
{
	const float in[3] = { v[0], v[1], v[2] };

	for ( int r = 0; r < 3; r++ )
		v[r] = R[r][0] * in[0] + R[r][1] * in[1] + R[r][2] * in[2];
}

// Speed of the model frame at pos: vel + omega x ( pos - origin ).
static inline void Phys_FrameSpeedAt( const float *vel, const float *omega, const float *origin, const float *pos, float *out )
{
	const float r[3] = { pos[0] - origin[0], pos[1] - origin[1], pos[2] - origin[2] };

	out[0] = vel[0] + omega[1] * r[2] - omega[2] * r[1];
	out[1] = vel[1] + omega[2] * r[0] - omega[0] * r[2];
	out[2] = vel[2] + omega[0] * r[1] - omega[1] * r[0];
}

// ph_debug: the largest correction of each constraint type in this frame.
static inline void Phys_DebugCorr( physCloth_t &cloth, int type, float value )
{
	if ( cloth.debug && value > cloth.dbgCorr[type] )
		cloth.dbgCorr[type] = value;
}

// ph_debug: the largest move of a particle since cloth.dbgP.
static void Phys_DebugMove( physCloth_t &cloth, float *p, int n, int type )
{
	for ( int i = 0; i < n; i++ )
		Phys_DebugCorr( cloth, type, Vec3Dist( &cloth.dbgP[i * 3], &p[i * 3] ) );

	cloth.dbgP.assign( p, p + n * 3 );
}

// A teleport of the model frame: it moves faster than PHYS_MAX_FRAME_SPEED or turns more than PHYS_MAX_FRAME_TURN
// in one frame. The distance is the test, so the time is at least PHYS_TELEPORT_MIN_DT: a normal move in a
// very short frame (1 ms) is not a teleport.
static bool Phys_FrameJump( const float vel[3], float dt, float angle )
{
	const float t = dt > PHYS_TELEPORT_MIN_DT ? dt : PHYS_TELEPORT_MIN_DT;
	const float maxDist = PHYS_MAX_FRAME_SPEED * t;

	return Vec3Dot( vel, vel ) * dt * dt > maxDist * maxDist || angle > PHYS_MAX_FRAME_TURN;
}

// Low-pass on the model frame, time constant PHYS_FRAME_SMOOTH. The frame speed is a difference over one
// frame, and the damping gives it to each particle times its distance from the frame: a pop or a jitter of
// the animation becomes a large push on the far particles. A teleport of the frame is not smoothed.
static void Phys_SmoothFrame( physCloth_t &cloth, float dt, float origin[3], float axis[3][3] )
{
	float R[3][3], omega[3], vel[3];
	const float angle = Phys_FrameTurn( cloth.rawAxis, axis, dt, R, omega );

	for ( int k = 0; k < 3; k++ )
		vel[k] = ( origin[k] - cloth.rawOrigin[k] ) / dt;

	if ( Phys_FrameJump( vel, dt, angle ) )
	{
		// The smoothed frame takes the same rigid jump: it keeps its lag, so the cloth moves with the jump only.
		float rel[3];

		Vec3Sub( cloth.smoothOrigin, cloth.rawOrigin, rel );
		Phys_Rotate( R, rel );

		for ( int k = 0; k < 3; k++ )
		{
			cloth.smoothOrigin[k] = origin[k] + rel[k];
			Phys_Rotate( R, cloth.smoothAxis[k] );
		}

		memcpy( cloth.rawOrigin, origin, sizeof( cloth.rawOrigin ) );
		memcpy( cloth.rawAxis, axis, sizeof( cloth.rawAxis ) );
		memcpy( origin, cloth.smoothOrigin, sizeof( cloth.smoothOrigin ) );
		memcpy( axis, cloth.smoothAxis, sizeof( cloth.smoothAxis ) );
		return;
	}

	memcpy( cloth.rawOrigin, origin, sizeof( cloth.rawOrigin ) );
	memcpy( cloth.rawAxis, axis, sizeof( cloth.rawAxis ) );

	const float a = 1.0f - expf( -dt / PHYS_FRAME_SMOOTH );
	float *s0 = cloth.smoothAxis[0], *s1 = cloth.smoothAxis[1], *s2 = cloth.smoothAxis[2];

	for ( int k = 0; k < 3; k++ )
	{
		cloth.smoothOrigin[k] += ( origin[k] - cloth.smoothOrigin[k] ) * a;
		s0[k] += ( axis[0][k] - s0[k] ) * a;
		s1[k] += ( axis[1][k] - s1[k] ) * a;
	}

	// Orthonormal again: axis 2 from axes 0 and 1, then axis 1 from axes 2 and 0.
	Vec3Normalize( s0 );
	s2[0] = s0[1] * s1[2] - s0[2] * s1[1];
	s2[1] = s0[2] * s1[0] - s0[0] * s1[2];
	s2[2] = s0[0] * s1[1] - s0[1] * s1[0];
	Vec3Normalize( s2 );
	s1[0] = s2[1] * s0[2] - s2[2] * s0[1];
	s1[1] = s2[2] * s0[0] - s2[0] * s0[2];
	s1[2] = s2[0] * s0[1] - s2[1] * s0[0];

	memcpy( origin, cloth.smoothOrigin, sizeof( cloth.smoothOrigin ) );
	memcpy( axis, cloth.smoothAxis, sizeof( cloth.smoothAxis ) );
}

void Phys_ClothStep( physCloth_t &cloth, const physTopology_t &topo, const physGroupDef_t &def,
					 float dt, const float *target, const float windVel[3], const float entOrigin[3], const float axis[3][3],
					 const physCollider_w *frameColliders, const physCollider_w *prevColliders, int numColliders,
					 bool hasGround, float groundZ )
{
	const int n = cloth.numParticles;

	if ( !cloth.valid || dt <= 0.0f || n == 0 )
		return;

	const physCollider_w *colliders = frameColliders;

	if ( prevColliders && numColliders )
		cloth.stepColliders.resize( numColliders );

	// Fixed steps: the same step every frame gives the same result for a different frame time. A step of
	// a different size moves the cloth to a different rest position, and the cloth vibrates.
	const float h = PHYS_CLOTH_STEP;
	const float accumStart = cloth.accum;
	int substeps = (int)( ( accumStart + dt ) / h );
	bool dropped = false;

	if ( substeps > PHYS_MAX_FIXED_STEPS )
	{
		substeps = PHYS_MAX_FIXED_STEPS;
		cloth.accum = 0.0f;
		dropped = true;
	}
	else
	{
		cloth.accum = accumStart + dt - substeps * h;
	}

	// Speed of the model frame in this frame.
	float origin[3], unitAxis[3][3], turn[3][3], frameVel[3], frameOmega[3];
	const float scale = Phys_NormalizeAxes( axis, unitAxis );	// the rest lengths are in model units

	Phys_ClothFrame( cloth, topo, target, entOrigin, axis, PHYS_SHAPE_ITERATIONS, origin, unitAxis );
	Phys_SmoothFrame( cloth, dt, origin, unitAxis );
	float angle = Phys_FrameTurn( cloth.frameAxis, unitAxis, dt, turn, frameOmega );

	for ( int k = 0; k < 3; k++ )
		frameVel[k] = ( origin[k] - cloth.frameOrigin[k] ) / dt;

	// No pinned particle: nothing holds the cloth, so the turn of the entity does not move it. A teleport of the
	// entity still moves it.
	if ( cloth.loose )
	{
		angle = 0.0f;
		memset( turn, 0, sizeof( turn ) );
		memset( frameOmega, 0, sizeof( frameOmega ) );
		turn[0][0] = turn[1][1] = turn[2][2] = 1.0f;
	}

	cloth.teleported = false;
	memset( cloth.dbgCorr, 0, sizeof( cloth.dbgCorr ) );

	if ( Phys_FrameJump( frameVel, dt, angle ) )
	{
		cloth.teleported = true;

		// Teleport: the cloth moves with the frame as one rigid piece and keeps its speed relative to the frame,
		// as a cloth in the local space of NvCloth. The frame speed stays the speed of the last frame.
		const int numVecs = n * 3;

		for ( int i = 0; i < numVecs; i += 3 )
		{
			float *xs[2] = { &cloth.x[i], &cloth.prevTarget[i] };

			for ( int j = 0; j < 2; j++ )
			{
				for ( int k = 0; k < 3; k++ )
					xs[j][k] -= cloth.frameOrigin[k];

				Phys_Rotate( turn, xs[j] );

				for ( int k = 0; k < 3; k++ )
					xs[j][k] += origin[k];
			}

			Phys_Rotate( turn, &cloth.v[i] );
			Phys_Rotate( turn, &cloth.offPrev[i] );
			Phys_Rotate( turn, &cloth.offCurr[i] );
		}

		Phys_Rotate( turn, cloth.frameVel );
		Phys_Rotate( turn, cloth.frameOmega );
		Phys_Rotate( turn, cloth.appliedVel );
		Phys_Rotate( turn, cloth.appliedOmega );
		memcpy( frameVel, cloth.frameVel, sizeof( frameVel ) );
		memcpy( frameOmega, cloth.frameOmega, sizeof( frameOmega ) );
		memcpy( cloth.frameOrigin, origin, sizeof( cloth.frameOrigin ) );
		memcpy( cloth.appliedOrigin, origin, sizeof( cloth.appliedOrigin ) );

		// The colliders do not sweep across the jump.
		prevColliders = NULL;
	}

	if ( cloth.loose )
		memset( frameVel, 0, sizeof( frameVel ) );

	if ( !cloth.frameSpeedValid )
	{
		// First frame after a reset: the cloth takes the measured frame speed as its own, with no change.
		memcpy( cloth.frameVel, frameVel, sizeof( cloth.frameVel ) );
		memcpy( cloth.frameOmega, frameOmega, sizeof( cloth.frameOmega ) );
		memcpy( cloth.appliedVel, frameVel, sizeof( cloth.appliedVel ) );
		memcpy( cloth.appliedOmega, frameOmega, sizeof( cloth.appliedOmega ) );
		memcpy( cloth.appliedOrigin, cloth.frameOrigin, sizeof( cloth.appliedOrigin ) );

		for ( int i = 0; i < n; i++ )
		{
			if ( !cloth.pinned[i] )
				Phys_FrameSpeedAt( frameVel, frameOmega, cloth.frameOrigin, &cloth.x[i * 3], &cloth.v[i * 3] );
		}

		cloth.frameSpeedValid = true;
	}

	// The frame state that the cloth took so far. Each step takes the change to the state at its end time.
	float curOrigin[3], curVel[3], curOmega[3];

	memcpy( curOrigin, cloth.appliedOrigin, sizeof( curOrigin ) );
	memcpy( curVel, cloth.appliedVel, sizeof( curVel ) );
	memcpy( curOmega, cloth.appliedOmega, sizeof( curOmega ) );

	const float take = 1.0f - def.inertia;	// part of the change of the frame speed that the cloth takes
	const float dampK = expf( -def.damping * h );
	const float dragK = 1.0f - expf( -def.drag * h );
	const float margin = def.margin;

	const int numEdges = (int)topo.edgeRest.size();
	const int numBends = def.bend > 0.0f ? (int)topo.bendRest.size() : 0;
	const float alphaStretch = Phys_Compliance( def.stretch, PHYS_STRETCH_COMPLIANCE ) / ( h * h );
	const float alphaBend = Phys_Compliance( def.bend, PHYS_BEND_COMPLIANCE ) / ( h * h );
	const float alphaFollow = def.follow > 0.0f ? 1.0f / ( def.follow * def.follow * h * h ) : -1.0f;

	// Shape: the part of the way to the goal in one iteration. The value is for a step of PHYS_MAX_STEP, so the
	// result does not depend on the iteration count or on the frame rate.
	const float stepScale = h / PHYS_MAX_STEP;
	const float shapeK = ( def.shape > 0.0f && def.iterations > 0 ) ? 1.0f - powf( 1.0f - def.shape * PHYS_SHAPE_MAX, stepScale / def.iterations ) : 0.0f;

	if ( shapeK > 0.0f && (int)cloth.adjStart.size() != n + 1 )
	{
		std::vector<std::vector<int> > nb( n );

		for ( int e = 0; e < numEdges; e++ )
		{
			nb[topo.edges[e * 2]].push_back( topo.edges[e * 2 + 1] );
			nb[topo.edges[e * 2 + 1]].push_back( topo.edges[e * 2] );
		}

		cloth.adjStart.assign( n + 1, 0 );
		cloth.adj.clear();

		for ( int i = 0; i < n; i++ )
		{
			cloth.adjStart[i] = (int)cloth.adj.size();
			cloth.adj.insert( cloth.adj.end(), nb[i].begin(), nb[i].end() );
		}

		cloth.adjStart[n] = (int)cloth.adj.size();
		cloth.shapeGoal.assign( n * 3, 0.0f );
		cloth.shapeCount.assign( n, 0.0f );
	}

	const bool aero = def.aero > 0.0f && !topo.tris.empty();
	const bool aniso = def.maxDistN > 0.0f && def.maxDist > 0.0f && !topo.tris.empty();
	const float ratioN = aniso ? def.maxDistN / def.maxDist : 1.0f;
	const bool tethers = (int)cloth.tether.size() == n;

	float *p = &cloth.p[0];
	float *tgt = &cloth.tgt[0];
	const float *rest = ( numColliders && (int)cloth.restDist.size() == n * numColliders ) ? &cloth.restDist[0] : NULL;

	if ( aero )
		cloth.acc.resize( n * 3 );

	if ( aniso )
	{
		cloth.targetNormal.resize( n * 3 );
		Phys_ComputeNormals( topo, target, &cloth.targetNormal[0] );
	}

	cloth.lambda.resize( numEdges + numBends );

	for ( int s = 0; s < substeps; s++ )
	{
		// The end of the step, between the last frame (0) and this frame (1).
		float blend = dropped ? (float)( s + 1 ) / (float)substeps : ( ( s + 1 ) * h - accumStart ) / dt;

		blend = blend < 0.0f ? 0.0f : ( blend > 1.0f ? 1.0f : blend );

		for ( int i = 0; i < n * 3; i++ )
			tgt[i] = cloth.prevTarget[i] + ( target[i] - cloth.prevTarget[i] ) * blend;

		if ( prevColliders && numColliders )
		{
			Phys_BlendColliders( prevColliders, frameColliders, numColliders, blend, &cloth.stepColliders[0] );
			colliders = &cloth.stepColliders[0];
		}

		// The model frame and its speed at the end of the step. The speed changes linearly from the speed of
		// the last frame to the speed of this frame. The start is the state that the cloth took so far.
		float o1[3], vel1[3], om1[3];

		for ( int k = 0; k < 3; k++ )
		{
			o1[k] = cloth.frameOrigin[k] + ( origin[k] - cloth.frameOrigin[k] ) * blend;
			vel1[k] = cloth.frameVel[k] + ( frameVel[k] - cloth.frameVel[k] ) * blend;
			om1[k] = cloth.frameOmega[k] + ( frameOmega[k] - cloth.frameOmega[k] ) * blend;
		}

		// Push of the air on each triangle, shared between its particles.
		if ( aero )
		{
			float *acc = &cloth.acc[0];
			const int numTris = (int)topo.tris.size() / 3;

			memset( acc, 0, sizeof( float ) * 3 * n );

			for ( int t = 0; t < numTris; t++ )
			{
				const int a = topo.tris[t * 3 + 0];
				const int b = topo.tris[t * 3 + 1];
				const int c = topo.tris[t * 3 + 2];
				float e1[3], e2[3], nrm[3];

				Vec3Sub( &cloth.x[b * 3], &cloth.x[a * 3], e1 );
				Vec3Sub( &cloth.x[c * 3], &cloth.x[a * 3], e2 );

				nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
				nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
				nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];

				const float len = sqrtf( Vec3Dot( nrm, nrm ) );

				if ( len < 1e-6f )
					continue;

				nrm[0] /= len;
				nrm[1] /= len;
				nrm[2] /= len;

				float rel[3];

				for ( int k = 0; k < 3; k++ )
					rel[k] = windVel[k] - ( cloth.v[a * 3 + k] + cloth.v[b * 3 + k] + cloth.v[c * 3 + k] ) * ( 1.0f / 3.0f );

				const float push = Vec3Dot( rel, nrm ) * def.aero * h;
				const int ids[3] = { a, b, c };

				for ( int k = 0; k < 3; k++ )
				{
					acc[ids[k] * 3 + 0] += nrm[0] * push;
					acc[ids[k] * 3 + 1] += nrm[1] * push;
					acc[ids[k] * 3 + 2] += nrm[2] * push;
				}
			}
		}

		// Predict the new positions.
		for ( int i = 0; i < n; i++ )
		{
			if ( cloth.pinned[i] )
			{
				for ( int k = 0; k < 3; k++ )
					p[i * 3 + k] = tgt[i * 3 + k];

				continue;
			}

			const float *x = &cloth.x[i * 3];
			float *v = &cloth.v[i * 3];
			float u0[3], u1[3];

			Phys_FrameSpeedAt( curVel, curOmega, curOrigin, x, u0 );
			Phys_FrameSpeedAt( vel1, om1, o1, x, u1 );

			for ( int k = 0; k < 3; k++ )
			{
				v[k] += ( u1[k] - u0[k] ) * take;
				v[k] = u1[k] + ( v[k] - u1[k] ) * dampK;
			}

			v[2] -= PHYS_GRAVITY * def.gravity * h;

			for ( int k = 0; k < 3; k++ )
			{
				v[k] += ( windVel[k] - v[k] ) * dragK;

				if ( aero )
					v[k] += cloth.acc[i * 3 + k] / ( topo.valence[i] > 0.0f ? topo.valence[i] : 1.0f );

				p[i * 3 + k] = x[k] + v[k] * h;
			}
		}

		memcpy( curOrigin, o1, sizeof( curOrigin ) );
		memcpy( curVel, vel1, sizeof( curVel ) );
		memcpy( curOmega, om1, sizeof( curOmega ) );

		memset( &cloth.lambda[0], 0, sizeof( float ) * ( numEdges + numBends ) );
		memset( &cloth.lambdaFollow[0], 0, sizeof( float ) * n );

		for ( int it = 0; it < def.iterations; it++ )
		{
			if ( cloth.debug )
				cloth.dbgP.assign( p, p + n * 3 );

			for ( int e = 0; e < numEdges; e++ )
				Phys_SolveDistance( p, &cloth.pinned[0], topo.edges[e * 2], topo.edges[e * 2 + 1], topo.edgeRest[e] * scale, alphaStretch, cloth.lambda[e] );

			for ( int b = 0; b < numBends; b++ )
				Phys_SolveDistance( p, &cloth.pinned[0], topo.bends[b * 2], topo.bends[b * 2 + 1], topo.bendRest[b] * scale, alphaBend, cloth.lambda[numEdges + b] );

			if ( cloth.debug )
				Phys_DebugMove( cloth, p, n, PHYS_CORR_EDGES );

			if ( shapeK > 0.0f )
				Phys_SolveShape( cloth, p, tgt, shapeK );

			if ( cloth.debug )
				Phys_DebugMove( cloth, p, n, PHYS_CORR_SHAPE );

			for ( int i = 0; i < n; i++ )
			{
				if ( cloth.pinned[i] )
					continue;

				// Tether: no further from the nearest pinned particle than in the bind pose.
				if ( tethers && cloth.tether[i] >= 0 )
				{
					float d[3];
					Vec3Sub( &p[i * 3], &p[cloth.tether[i] * 3], d );

					const float len = sqrtf( Vec3Dot( d, d ) );
					const float limit = cloth.tetherLength[i] * scale;

					if ( len > limit && len > 1e-5f )
					{
						const float k = limit / len;

						for ( int c = 0; c < 3; c++ )
							p[i * 3 + c] = p[cloth.tether[i] * 3 + c] + d[c] * k;

						Phys_DebugCorr( cloth, PHYS_CORR_TETHER, len - limit );
					}
				}

				float d[3];
				Vec3Sub( &p[i * 3], &tgt[i * 3], d );

				// Soft link to the animated pose (hold), as the anim drive of the Unreal cloth. 0 = off.
				if ( alphaFollow >= 0.0f )
				{
					const float len = sqrtf( Vec3Dot( d, d ) );

					if ( len > 1e-5f )
					{
						const float dl = ( -len - alphaFollow * cloth.lambdaFollow[i] ) / ( 1.0f + alphaFollow );
						const float k = dl / len;

						cloth.lambdaFollow[i] += dl;
						Phys_DebugCorr( cloth, PHYS_CORR_FOLLOW, fabsf( dl ) );

						for ( int c = 0; c < 3; c++ )
						{
							p[i * 3 + c] += d[c] * k;
							d[c] += d[c] * k;
						}
					}
				}

				// Reach: a particle stays inside an ellipsoid around its animated position.
				const float rt = cloth.maxDist[i];
				float e;

				if ( aniso )
				{
					const float *nrm = &cloth.targetNormal[i * 3];
					const float dn = Vec3Dot( d, nrm );
					const float rn = rt * ratioN;
					const float tx = d[0] - nrm[0] * dn, ty = d[1] - nrm[1] * dn, tz = d[2] - nrm[2] * dn;

					e = ( dn * dn ) / ( rn * rn ) + ( tx * tx + ty * ty + tz * tz ) / ( rt * rt );
				}
				else
				{
					e = Vec3Dot( d, d ) / ( rt * rt );
				}

				if ( e > 1.0f )
				{
					const float scale = 1.0f / sqrtf( e );

					Phys_DebugCorr( cloth, PHYS_CORR_REACH, sqrtf( Vec3Dot( d, d ) ) * ( 1.0f - scale ) );

					for ( int c = 0; c < 3; c++ )
						p[i * 3 + c] = tgt[i * 3 + c] + d[c] * scale;
				}
			}

			// Collisions run last, so the result of an iteration is outside the colliders.
			for ( int i = 0; i < n; i++ )
			{
				if ( cloth.pinned[i] )
					continue;

				const float before[3] = { p[i * 3], p[i * 3 + 1], p[i * 3 + 2] };

				Phys_CollideAll( &p[i * 3], &tgt[i * 3], &cloth.x[i * 3], margin, colliders, numColliders,
					rest ? rest + i * numColliders : NULL, hasGround, groundZ, cloth.debug ? &cloth.dbgCorr[PHYS_CORR_CROSS] : NULL );

				if ( cloth.debug )
					Phys_DebugCorr( cloth, PHYS_CORR_COLLIDE, Vec3Dist( before, &p[i * 3] ) );
			}
		}

		// Speed from the position change.
		for ( int i = 0; i < n; i++ )
		{
			float *x = &cloth.x[i * 3];

			for ( int k = 0; k < 3; k++ )
			{
				if ( !cloth.pinned[i] )
					cloth.v[i * 3 + k] = ( p[i * 3 + k] - x[k] ) / h;

				if ( cloth.debug && !cloth.pinned[i] && k == 2 )
					Phys_DebugCorr( cloth, PHYS_CORR_SPEED, sqrtf( Vec3Dot( &cloth.v[i * 3], &cloth.v[i * 3] ) ) );

				x[k] = p[i * 3 + k];
				cloth.offPrev[i * 3 + k] = cloth.offCurr[i * 3 + k];
				cloth.offCurr[i * 3 + k] = x[k] - tgt[i * 3 + k];
			}
		}
	}

	// The draw is one step behind the solver, at the time of the frame minus a step. Between the two last
	// steps the offset from the animation changes smoothly, so a frame with no step or with two steps is smooth.
	const float alpha = dropped ? 1.0f : cloth.accum / h;

	for ( int i = 0; i < n * 3; i++ )
		cloth.drawX[i] = target[i] + cloth.offPrev[i] + ( cloth.offCurr[i] - cloth.offPrev[i] ) * alpha;

	memcpy( &cloth.prevTarget[0], target, sizeof( float ) * 3 * n );
	memcpy( cloth.frameOrigin, origin, sizeof( cloth.frameOrigin ) );
	memcpy( cloth.frameAxis, unitAxis, sizeof( cloth.frameAxis ) );
	memcpy( cloth.frameVel, frameVel, sizeof( cloth.frameVel ) );
	memcpy( cloth.frameOmega, frameOmega, sizeof( cloth.frameOmega ) );
	memcpy( cloth.appliedOrigin, curOrigin, sizeof( cloth.appliedOrigin ) );
	memcpy( cloth.appliedVel, curVel, sizeof( cloth.appliedVel ) );
	memcpy( cloth.appliedOmega, curOmega, sizeof( cloth.appliedOmega ) );
}

//
// Lag mode
//

static void Phys_LagCenters( physLag_t &lag, const float *target, const float *freeFactor, int n )
{
	float ct[3] = { 0.0f, 0.0f, 0.0f }, pivot[3] = { 0.0f, 0.0f, 0.0f };
	float weight = 0.0f;
	int numFixed = 0;

	for ( int i = 0; i < n; i++ )
	{
		const float f = freeFactor[i];

		if ( f > PHYS_MIN_FREE )
		{
			for ( int k = 0; k < 3; k++ )
				ct[k] += target[i * 3 + k] * f;

			weight += f;
		}
		else
		{
			for ( int k = 0; k < 3; k++ )
				pivot[k] += target[i * 3 + k];

			numFixed++;
		}
	}

	for ( int k = 0; k < 3; k++ )
	{
		lag.ct[k] = weight > 0.0f ? ct[k] / weight : 0.0f;
		lag.pivot[k] = numFixed ? pivot[k] / (float)numFixed : lag.ct[k];
	}

	// With no fixed particle the group turns around a point above its mass center.
	if ( !numFixed )
		lag.pivot[2] += 8.0f;

	lag.reach = 0.0f;

	for ( int i = 0; i < n; i++ )
	{
		if ( freeFactor[i] > PHYS_MIN_FREE )
		{
			const float d = Vec3Dist( &target[i * 3], lag.pivot );

			if ( d > lag.reach )
				lag.reach = d;
		}
	}
}

void Phys_LagReset( physLag_t &lag, const float *target, const float *freeFactor, int n )
{
	Phys_LagCenters( lag, target, freeFactor, n );

	for ( int k = 0; k < 3; k++ )
	{
		lag.xc[k] = lag.ct[k];
		lag.vc[k] = 0.0f;
		lag.vct[k] = 0.0f;
	}

	lag.valid = true;
}

void Phys_LagStep( physLag_t &lag, const physGroupDef_t &def, float dt, const float *target,
				   const float *freeFactor, int n, const float windVel[3] )
{
	float prevCt[3];

	for ( int k = 0; k < 3; k++ )
		prevCt[k] = lag.ct[k];

	Phys_LagCenters( lag, target, freeFactor, n );

	if ( !lag.valid || dt <= 0.0f )
		return;

	int substeps = (int)ceilf( dt / PHYS_MAX_STEP );

	if ( substeps < 1 )						substeps = 1;
	else if ( substeps > PHYS_MAX_SUBSTEPS )	substeps = PHYS_MAX_SUBSTEPS;

	const float h = dt / substeps;
	const float dragK = 1.0f - expf( -def.drag * h );
	const float maxSpeed = def.maxSpeed > 0.0f ? def.maxSpeed : 1e9f;
	float omega = def.lagFreq;

	if ( omega * h > 1.0f )
		omega = 1.0f / h;

	const float zeta2 = 2.0f * def.lagDamp * omega;

	// The turn is bounded so that the farthest particle moves by maxdist at most. The mass offset alone does
	// not bound it: the turn grows as the mass gets close to the pivot.
	lag.maxAngle = lag.reach > 1.0f ? def.maxDist / lag.reach : 3.0f;

	for ( int k = 0; k < 3; k++ )
		lag.vct[k] = ( lag.ct[k] - prevCt[k] ) / dt;

	for ( int s = 0; s < substeps; s++ )
	{
		const float blend = (float)( s + 1 ) / (float)substeps;
		float ctNow[3];

		for ( int k = 0; k < 3; k++ )
		{
			ctNow[k] = prevCt[k] + ( lag.ct[k] - prevCt[k] ) * blend;

			float vr = lag.vc[k] - lag.vct[k];

			vr += ( omega * omega * ( ctNow[k] - lag.xc[k] ) - zeta2 * vr ) * h;

			float va = lag.vct[k] + vr;

			va += ( windVel[k] - va ) * dragK;
			lag.vc[k] = va;
		}

		Phys_ClampRelative( lag.vc, lag.vct, maxSpeed );

		for ( int k = 0; k < 3; k++ )
			lag.xc[k] += lag.vc[k] * h;

		// The offset from the animated mass center has an upper limit.
		float off[3];

		Vec3Sub( lag.xc, ctNow, off );

		const float len = sqrtf( Vec3Dot( off, off ) );

		if ( len > def.maxDist && len > 1e-5f )
		{
			const float scale = def.maxDist / len;

			for ( int k = 0; k < 3; k++ )
				lag.xc[k] = ctNow[k] + off[k] * scale;
		}
	}
}

void Phys_LagApply( const physLag_t &lag, const float *target, const float *freeFactor, int n, float *outPos )
{
	float a[3], b[3], delta[3], axis[3];

	Vec3Sub( lag.ct, lag.pivot, a );
	Vec3Sub( lag.xc, lag.pivot, b );
	Vec3Sub( lag.xc, lag.ct, delta );

	const float la = sqrtf( Vec3Dot( a, a ) );
	const float lb = sqrtf( Vec3Dot( b, b ) );
	float angle = 0.0f;
	bool rotate = false;

	if ( la > 1.0f && lb > 1.0f )
	{
		axis[0] = a[1] * b[2] - a[2] * b[1];
		axis[1] = a[2] * b[0] - a[0] * b[2];
		axis[2] = a[0] * b[1] - a[1] * b[0];

		const float sinA = sqrtf( Vec3Dot( axis, axis ) ) / ( la * lb );

		if ( sinA > 1e-6f )
		{
			const float cosA = Vec3Dot( a, b ) / ( la * lb );
			const float len = sqrtf( Vec3Dot( axis, axis ) );

			angle = atan2f( sinA, cosA );

			if ( angle > lag.maxAngle )
				angle = lag.maxAngle;

			axis[0] /= len;
			axis[1] /= len;
			axis[2] /= len;
			rotate = true;
		}
	}

	for ( int i = 0; i < n; i++ )
	{
		const float w = freeFactor[i];
		const float *t = target + i * 3;
		float *out = outPos + i * 3;

		if ( w <= PHYS_MIN_FREE )
		{
			out[0] = t[0];
			out[1] = t[1];
			out[2] = t[2];
			continue;
		}

		if ( !rotate )
		{
			out[0] = t[0] + delta[0] * w;
			out[1] = t[1] + delta[1] * w;
			out[2] = t[2] + delta[2] * w;
			continue;
		}

		// Rodrigues rotation of the offset from the pivot, by a part of the angle.
		float r[3];
		Vec3Sub( t, lag.pivot, r );

		const float th = angle * w;
		const float c = cosf( th ), s = sinf( th );
		const float dot = Vec3Dot( axis, r );
		const float cx = axis[1] * r[2] - axis[2] * r[1];
		const float cy = axis[2] * r[0] - axis[0] * r[2];
		const float cz = axis[0] * r[1] - axis[1] * r[0];

		out[0] = lag.pivot[0] + r[0] * c + cx * s + axis[0] * dot * ( 1.0f - c );
		out[1] = lag.pivot[1] + r[1] * c + cy * s + axis[1] * dot * ( 1.0f - c );
		out[2] = lag.pivot[2] + r[2] * c + cz * s + axis[2] * dot * ( 1.0f - c );
	}
}

#define PHYS_LAG_PASSES		3
#define PHYS_LAG_MIN_DEPTH	0.05f
#define PHYS_LAG_MIN_LEVER	0.25f	// a particle near the pivot moves little: its contact must not throw the mass center far

void Phys_LagCollide( physLag_t &lag, const physGroupDef_t &def, const float *target, const float *freeFactor, int n,
					  const physCollider_w *colliders, int numColliders, const float *restDist, bool hasGround, float groundZ,
					  float *outPos )
{
	if ( !lag.valid || ( numColliders == 0 && !hasGround ) )
		return;

	for ( int pass = 0; pass < PHYS_LAG_PASSES; pass++ )
	{
		// The deepest contact moves the whole mass, so the hair slides on the body as one piece.
		float bestDepth = 0.0f, bestVec[3] = { 0.0f, 0.0f, 0.0f }, bestW = 1.0f;

		for ( int i = 0; i < n; i++ )
		{
			const float w = freeFactor[i];

			if ( w <= PHYS_MIN_FREE )
				continue;

			float pushed[3] = { outPos[i * 3], outPos[i * 3 + 1], outPos[i * 3 + 2] };

			// Lag mode has no position at the start of a step: the animated side, as before.
			if ( !Phys_CollideAll( pushed, target + i * 3, target + i * 3, def.margin, colliders, numColliders,
					restDist ? restDist + i * numColliders : NULL, hasGround, groundZ ) )
				continue;

			float c[3];
			Vec3Sub( pushed, outPos + i * 3, c );

			const float depth = sqrtf( Vec3Dot( c, c ) );

			if ( depth > bestDepth )
			{
				bestDepth = depth;
				bestW = w;
				memcpy( bestVec, c, sizeof( bestVec ) );
			}
		}

		if ( bestDepth < PHYS_LAG_MIN_DEPTH )
			break;

		const float lever = bestW > PHYS_LAG_MIN_LEVER ? bestW : PHYS_LAG_MIN_LEVER;
		float normal[3];

		for ( int k = 0; k < 3; k++ )
		{
			lag.xc[k] += bestVec[k] / lever;
			normal[k] = bestVec[k] / bestDepth;
		}

		// Keep the offset limit of the mass.
		float off[3];

		Vec3Sub( lag.xc, lag.ct, off );

		const float len = sqrtf( Vec3Dot( off, off ) );

		if ( len > def.maxDist && len > 1e-5f )
		{
			for ( int k = 0; k < 3; k++ )
				lag.xc[k] = lag.ct[k] + off[k] * ( def.maxDist / len );
		}

		// Remove the speed into the collider, measured relative to the animation.
		float rel[3];

		Vec3Sub( lag.vc, lag.vct, rel );

		const float into = Vec3Dot( rel, normal );

		if ( into < 0.0f )
		{
			for ( int k = 0; k < 3; k++ )
				lag.vc[k] -= into * normal[k];
		}

		Phys_LagApply( lag, target, freeFactor, n, outPos );
	}

	// What is left is pushed out one particle at a time.
	Phys_ProjectColliders( outPos, target, freeFactor, n, def.margin, colliders, numColliders, restDist, hasGround, groundZ );
}

void Phys_ComputeNormals( const physTopology_t &topo, const float *pos, float *normals )
{
	const int n = topo.numParticles;
	const int numTris = (int)topo.tris.size() / 3;

	memset( normals, 0, sizeof( float ) * 3 * n );

	for ( int t = 0; t < numTris; t++ )
	{
		const int a = topo.tris[t * 3 + 0];
		const int b = topo.tris[t * 3 + 1];
		const int c = topo.tris[t * 3 + 2];
		float e1[3], e2[3], nrm[3];

		Vec3Sub( pos + b * 3, pos + a * 3, e1 );
		Vec3Sub( pos + c * 3, pos + a * 3, e2 );

		nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
		nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
		nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];

		const int ids[3] = { a, b, c };

		for ( int k = 0; k < 3; k++ )
		{
			normals[ids[k] * 3 + 0] += nrm[0];
			normals[ids[k] * 3 + 1] += nrm[1];
			normals[ids[k] * 3 + 2] += nrm[2];
		}
	}

	for ( int i = 0; i < n; i++ )
	{
		float *v = normals + i * 3;
		const float len = sqrtf( Vec3Dot( v, v ) );

		if ( len > 1e-8f )
		{
			v[0] /= len;
			v[1] /= len;
			v[2] /= len;
		}
	}
}

//
// Text of a group
//

static void ( *s_userPrint )( const char *msg ) = NULL;
static int s_parseErrors = 0;

static void Phys_CountingPrint( const char *msg )
{
	s_parseErrors++;

	if ( s_userPrint )
		s_userPrint( msg );
}

bool Phys_ParseGroupText( const char *text, physGroupDef_t &g, void ( *print )( const char *msg ) )
{
	std::vector<std::string> tok;
	const char *p = text;
	const std::string name = g.name;

	s_userPrint = print;
	s_parseErrors = 0;

	Phys_InitGroup( g, g.hair );
	g.name = name;

	while ( Phys_ReadLine( p, tok ) )
	{
		if ( tok[0] == "{" || tok[0] == "}" )
			continue;

		if ( !Phys_Stricmp( tok[0].c_str(), "cloth" ) || !Phys_Stricmp( tok[0].c_str(), "hair" ) )
		{
			const bool hair = !Phys_Stricmp( tok[0].c_str(), "hair" );

			if ( hair != g.hair )
			{
				// The type sets the defaults: start again with the new type.
				Phys_InitGroup( g, hair );
			}

			g.name = ( tok.size() > 1 && tok[1] != "{" ) ? tok[1] : name;
			continue;
		}

		Phys_ParseLine( g, tok, Phys_CountingPrint );
	}

	s_userPrint = NULL;
	return s_parseErrors == 0 && !g.surfaces.empty();
}

static void Phys_Emit( std::string &out, const char *key, float value, float def )
{
	char buf[64];

	if ( fabsf( value - def ) < 1e-6f )
		return;

	// The value starts at column 12 (tabs of 4), with 3 decimals at most. A longer key gets one tab.
	const int pad = 12 - (int)strlen( key );
	std::string tabs( pad > 0 ? ( pad + 3 ) / 4 : 1, '\t' );

	snprintf( buf, sizeof( buf ), "\t%s%s%g\n", key, tabs.c_str(), floor( value * 1000.0 + 0.5 ) / 1000.0 );
	out += buf;
}

static const char *Phys_AxisName( const float axis[3] )
{
	if ( axis[0] > 0.5f )	return "x";
	if ( axis[0] < -0.5f )	return "-x";
	if ( axis[1] > 0.5f )	return "y";
	if ( axis[1] < -0.5f )	return "-y";
	if ( axis[2] < -0.5f )	return "-z";
	return "z";
}

std::string Phys_GroupToText( const physGroupDef_t &g )
{
	physGroupDef_t d;
	std::string out;
	char buf[256];

	Phys_InitGroup( d, g.hair );

	out += g.hair ? "hair " : "cloth ";
	out += g.name.empty() ? "group" : g.name;
	out += "\n{\n\tsurfaces\t";

	for ( size_t i = 0; i < g.surfaces.size(); i++ )
	{
		if ( i )
			out += " ";

		out += g.surfaces[i];
	}

	out += "\n";

	for ( size_t i = 0; i < g.pins.size(); i++ )
	{
		const physPinRule_t &r = g.pins[i];

		if ( r.type == PHYS_PIN_GRADIENT )
		{
			snprintf( buf, sizeof( buf ), "\tpin\t\t\tgradient %s %s %g %g\n", r.bone.c_str(), Phys_AxisName( r.axis ), r.pinned, r.free );
			out += buf;
		}
		else if ( r.type == PHYS_PIN_RADIUS )
		{
			if ( r.axis[0] != 0.0f || r.axis[1] != 0.0f || r.axis[2] != 0.0f )
				snprintf( buf, sizeof( buf ), "\tpin\t\t\tradius %s %g %g %g %g %g\n", r.bone.c_str(), r.pinned, r.free, r.axis[0], r.axis[1], r.axis[2] );
			else
				snprintf( buf, sizeof( buf ), "\tpin\t\t\tradius %s %g %g\n", r.bone.c_str(), r.pinned, r.free );

			out += buf;
		}
		else if ( r.type == PHYS_PIN_BONES )
		{
			out += "\tpin\t\t\tbones";

			for ( size_t b = 0; b < r.bones.size(); b++ )
				out += " " + r.bones[b];

			out += "\n";
		}
		else
		{
			snprintf( buf, sizeof( buf ), "\tpin\t\t\tuv %g %g\n", r.pinned, r.free );
			out += buf;
		}
	}

	if ( g.lag )
		out += "\tmode\t\tlag\n";

	// A material line sets the defaults of the values after it.
	if ( !g.material.empty() && Phys_ApplyMaterial( d, g.material.c_str() ) )
		out += "\tmaterial\t" + g.material + "\n";

	Phys_Emit( out, "weight", g.weight, d.weight );
	Phys_Emit( out, "stretch", g.stretch, d.stretch );
	Phys_Emit( out, "stiffness", g.stiffness, d.stiffness );
	Phys_Emit( out, "air", g.air, d.air );
	Phys_Emit( out, "hold", g.hold, d.hold );
	Phys_Emit( out, "bounce", g.bounce, d.bounce );
	Phys_Emit( out, "inertia", g.inertia, d.inertia );
	Phys_Emit( out, "shape", g.shape, d.shape );

	if ( g.maxDist != d.maxDist || g.maxDistN != d.maxDistN )
	{
		if ( g.maxDistN > 0.0f )
			snprintf( buf, sizeof( buf ), "\treach\t\t%g %g\n", g.maxDist, g.maxDistN );
		else
			snprintf( buf, sizeof( buf ), "\treach\t\t%g\n", g.maxDist );

		out += buf;
	}

	Phys_Emit( out, "detail", g.detail, d.detail );
	Phys_Emit( out, "range", g.range, d.range );
	Phys_Emit( out, "subdivide", (float)g.subdivide, (float)d.subdivide );
	Phys_Emit( out, "simsubdivide", (float)g.simSubdivide, (float)d.simSubdivide );

	// Old solver values: kept when the file sets them.
	Phys_Emit( out, "maxspeed", g.maxSpeed, d.maxSpeed );
	Phys_Emit( out, "margin", g.margin, d.margin );
	Phys_Emit( out, "iterations", (float)g.iterations, (float)d.iterations );

	if ( !g.enabled )
		out += "\tenabled\t\t0\n";

	if ( g.modelFit )
	{
		if ( g.modelFitScale != 1.0f )
			snprintf( buf, sizeof( buf ), "\tcollide\t\tmodel %g\n", g.modelFitScale );
		else
			snprintf( buf, sizeof( buf ), "\tcollide\t\tmodel\n" );

		out += buf;
	}

	if ( g.humanoid )
	{
		if ( g.humanoidScale != 1.0f )
			snprintf( buf, sizeof( buf ), "\tcollide\t\thumanoid %g\n", g.humanoidScale );
		else
			snprintf( buf, sizeof( buf ), "\tcollide\t\thumanoid\n" );

		out += buf;
	}

	for ( size_t i = 0; i < g.radiusOverride.size(); i++ )
	{
		snprintf( buf, sizeof( buf ), "\tcollide\t\tradius %s %g\n", g.radiusOverride[i].first.c_str(), g.radiusOverride[i].second );
		out += buf;
	}

	for ( size_t i = 0; i < g.removed.size(); i++ )
		out += "\tcollide\t\tremove " + g.removed[i] + "\n";

	for ( size_t i = 0; i < g.colliders.size(); i++ )
	{
		const physCollider_t &c = g.colliders[i];

		if ( c.box )
		{
			if ( c.offA[0] != 0.0f || c.offA[1] != 0.0f || c.offA[2] != 0.0f )
				snprintf( buf, sizeof( buf ), "\tcollide\t\tbox %s %g %g %g %g %g %g %g\n", c.boneA.c_str(), c.size[0], c.size[1], c.size[2], c.radius, c.offA[0], c.offA[1], c.offA[2] );
			else if ( c.radius != 0.0f )
				snprintf( buf, sizeof( buf ), "\tcollide\t\tbox %s %g %g %g %g\n", c.boneA.c_str(), c.size[0], c.size[1], c.size[2], c.radius );
			else
				snprintf( buf, sizeof( buf ), "\tcollide\t\tbox %s %g %g %g\n", c.boneA.c_str(), c.size[0], c.size[1], c.size[2] );
		}
		else if ( c.capsule )
		{
			snprintf( buf, sizeof( buf ), "\tcollide\t\tcapsule %s %s %g\n", c.boneA.c_str(), c.boneB.c_str(), c.radius );
		}
		else if ( c.offA[0] != 0.0f || c.offA[1] != 0.0f || c.offA[2] != 0.0f )
		{
			snprintf( buf, sizeof( buf ), "\tcollide\t\tsphere %s %g %g %g %g\n", c.boneA.c_str(), c.radius, c.offA[0], c.offA[1], c.offA[2] );
		}
		else
		{
			snprintf( buf, sizeof( buf ), "\tcollide\t\tsphere %s %g\n", c.boneA.c_str(), c.radius );
		}

		out += buf;
	}

	out += "}\n";
	return out;
}

std::string Phys_DefToText( const physDef_t &def )
{
	std::string out;

	for ( size_t i = 0; i < def.groups.size(); i++ )
	{
		if ( i )
			out += "\n";

		out += Phys_GroupToText( def.groups[i] );
	}

	return out;
}

//
// Subdivided draw mesh
//

typedef std::vector<std::pair<int, float> > physStencil_t;

struct physSubdivEdge_t
{
	int		count;		// triangles on the edge
	int		opp[2];		// points opposite to the edge in the first two triangles
	int		point;		// the new point on the edge
};

// One surface during the build.
struct physSubdivWork_t
{
	std::vector<int>	point;	// per vertex
	std::vector<float>	st;		// 2 per vertex
	std::vector<int>	tris;	// 3 per triangle
};

static inline unsigned long long Phys_PairKey( int a, int b )
{
	if ( a > b )
	{
		const int t = a;

		a = b;
		b = t;
	}

	return ( (unsigned long long)(unsigned int)a << 32 ) | (unsigned int)b;
}

static void Phys_StencilAdd( std::map<int, float> &acc, const physStencil_t &s, float w )
{
	for ( size_t i = 0; i < s.size(); i++ )
		acc[s[i].first] += w * s[i].second;
}

// Drops the very small weights. The sum of the weights stays 1.
static void Phys_StencilFinish( const std::map<int, float> &acc, physStencil_t &out )
{
	float total = 0.0f;

	out.clear();

	for ( std::map<int, float>::const_iterator it = acc.begin(); it != acc.end(); ++it )
	{
		if ( fabsf( it->second ) > 1e-4f )
		{
			out.push_back( *it );
			total += it->second;
		}
	}

	if ( total != 0.0f )
	{
		for ( size_t i = 0; i < out.size(); i++ )
			out[i].second /= total;
	}
}

// One level of Loop subdivision. smooth blends each rule with plain interpolation (0 = interpolation).
static void Phys_SubdivLevel( std::vector<physStencil_t> &pts, std::vector<float> &smooth, std::vector<physSubdivWork_t> &surfs )
{
	const int numOld = (int)pts.size();
	std::map<unsigned long long, physSubdivEdge_t> edges;
	std::vector<std::vector<int> > neighbors( numOld );

	for ( size_t s = 0; s < surfs.size(); s++ )
	{
		const physSubdivWork_t &w = surfs[s];

		for ( size_t t = 0; t + 2 < w.tris.size(); t += 3 )
		{
			const int p[3] = { w.point[w.tris[t]], w.point[w.tris[t + 1]], w.point[w.tris[t + 2]] };

			if ( p[0] == p[1] || p[1] == p[2] || p[0] == p[2] )
				continue;

			for ( int k = 0; k < 3; k++ )
			{
				const int a = p[k], b = p[( k + 1 ) % 3];
				const unsigned long long key = Phys_PairKey( a, b );
				std::map<unsigned long long, physSubdivEdge_t>::iterator it = edges.find( key );

				if ( it == edges.end() )
				{
					physSubdivEdge_t e;

					e.count = 0;
					e.opp[0] = e.opp[1] = -1;
					e.point = -1;
					it = edges.insert( std::make_pair( key, e ) ).first;
					neighbors[a].push_back( b );
					neighbors[b].push_back( a );
				}

				if ( it->second.count < 2 )
					it->second.opp[it->second.count] = p[( k + 2 ) % 3];

				it->second.count++;
			}
		}
	}

	std::vector<physStencil_t> next( numOld );
	std::vector<float> nextSmooth( numOld );

	// The old points move toward their neighbors.
	for ( int p = 0; p < numOld; p++ )
	{
		const float s = smooth[p];
		const std::vector<int> &nb = neighbors[p];
		std::vector<int> boundary;
		std::map<int, float> acc;

		for ( size_t i = 0; i < nb.size(); i++ )
		{
			if ( edges[Phys_PairKey( p, nb[i] )].count == 1 )
				boundary.push_back( nb[i] );
		}

		if ( nb.empty() || s <= 0.0f )
		{
			Phys_StencilAdd( acc, pts[p], 1.0f );
		}
		else if ( !boundary.empty() )
		{
			// A border keeps its curve. A corner or a non manifold point does not move.
			if ( boundary.size() == 2 )
			{
				Phys_StencilAdd( acc, pts[p], 1.0f - s * 0.25f );
				Phys_StencilAdd( acc, pts[boundary[0]], s * 0.125f );
				Phys_StencilAdd( acc, pts[boundary[1]], s * 0.125f );
			}
			else
			{
				Phys_StencilAdd( acc, pts[p], 1.0f );
			}
		}
		else
		{
			const float n = (float)nb.size();
			const float beta = nb.size() == 3 ? 3.0f / 16.0f : 3.0f / ( 8.0f * n );

			Phys_StencilAdd( acc, pts[p], 1.0f - s * n * beta );

			for ( size_t i = 0; i < nb.size(); i++ )
				Phys_StencilAdd( acc, pts[nb[i]], s * beta );
		}

		Phys_StencilFinish( acc, next[p] );
		nextSmooth[p] = s;
	}

	// A new point on each edge.
	for ( std::map<unsigned long long, physSubdivEdge_t>::iterator it = edges.begin(); it != edges.end(); ++it )
	{
		physSubdivEdge_t &e = it->second;
		const int a = (int)( it->first >> 32 );
		const int b = (int)( it->first & 0xFFFFFFFFu );
		const float s = 0.5f * ( smooth[a] + smooth[b] );
		std::map<int, float> acc;
		physStencil_t stencil;

		if ( e.count == 2 && e.opp[0] >= 0 && e.opp[1] >= 0 )
		{
			Phys_StencilAdd( acc, pts[a], 0.5f - s * 0.125f );
			Phys_StencilAdd( acc, pts[b], 0.5f - s * 0.125f );
			Phys_StencilAdd( acc, pts[e.opp[0]], s * 0.125f );
			Phys_StencilAdd( acc, pts[e.opp[1]], s * 0.125f );
		}
		else
		{
			Phys_StencilAdd( acc, pts[a], 0.5f );
			Phys_StencilAdd( acc, pts[b], 0.5f );
		}

		Phys_StencilFinish( acc, stencil );
		e.point = (int)next.size();
		next.push_back( stencil );
		nextSmooth.push_back( s );
	}

	// Each triangle becomes four. A vertex keeps its index; a new vertex goes on each edge of the surface.
	for ( size_t s = 0; s < surfs.size(); s++ )
	{
		physSubdivWork_t &w = surfs[s];
		std::map<unsigned long long, int> mids;
		std::vector<int> tris;
		const size_t numTris = w.tris.size();

		for ( size_t t = 0; t + 2 < numTris; t += 3 )
		{
			const int v[3] = { w.tris[t], w.tris[t + 1], w.tris[t + 2] };

			if ( w.point[v[0]] == w.point[v[1]] || w.point[v[1]] == w.point[v[2]] || w.point[v[0]] == w.point[v[2]] )
				continue;

			int m[3];

			for ( int k = 0; k < 3; k++ )
			{
				const int va = v[k], vb = v[( k + 1 ) % 3];
				const unsigned long long key = Phys_PairKey( va, vb );
				std::map<unsigned long long, int>::iterator it = mids.find( key );

				if ( it != mids.end() )
				{
					m[k] = it->second;
					continue;
				}

				m[k] = (int)w.point.size();
				mids[key] = m[k];
				w.point.push_back( edges[Phys_PairKey( w.point[va], w.point[vb] )].point );
				w.st.push_back( 0.5f * ( w.st[va * 2] + w.st[vb * 2] ) );
				w.st.push_back( 0.5f * ( w.st[va * 2 + 1] + w.st[vb * 2 + 1] ) );
			}

			const int out[12] = { v[0], m[0], m[2],  m[0], v[1], m[1],  m[2], m[1], v[2],  m[0], m[1], m[2] };

			tris.insert( tris.end(), out, out + 12 );
		}

		w.tris.swap( tris );
	}

	pts.swap( next );
	smooth.swap( nextSmooth );
}

// Closes the chunk that starts at firstVert / firstIndex.
static void Phys_SubdivCloseChunk( physSubdivSurface_t &out, int firstVert, int firstIndex )
{
	if ( (int)out.indexes.size() <= firstIndex )
		return;

	out.chunks.push_back( firstVert );
	out.chunks.push_back( (int)out.point.size() - firstVert );
	out.chunks.push_back( firstIndex );
	out.chunks.push_back( (int)out.indexes.size() - firstIndex );
}

void Phys_BuildSubdiv( physSubdiv_t &sd, const physTopology_t &topo, const physSubdivInput_t *surfaces, int numSurfaces,
					   const float *smooth, int levels, int maxChunkVerts, int maxChunkIndexes, int maxVerts )
{
	sd = physSubdiv_t();

	if ( levels <= 0 || topo.numParticles <= 0 || numSurfaces <= 0 )
		return;

	std::vector<physStencil_t> pts( topo.numParticles );
	std::vector<float> sm( smooth, smooth + topo.numParticles );
	std::vector<physSubdivWork_t> work( numSurfaces );

	for ( int i = 0; i < topo.numParticles; i++ )
		pts[i].push_back( std::make_pair( i, 1.0f ) );

	for ( int s = 0; s < numSurfaces; s++ )
	{
		work[s].point = topo.vertParticle[s];
		work[s].st.assign( surfaces[s].st, surfaces[s].st + surfaces[s].numVerts * 2 );
		work[s].tris.assign( surfaces[s].tris, surfaces[s].tris + surfaces[s].numTris * 3 );
	}

	int done = 0;

	for ( ; done < levels; done++ )
	{
		// A level gives about four times the vertices.
		size_t total = 0;

		for ( int s = 0; s < numSurfaces; s++ )
			total += work[s].point.size();

		if ( (int)total * 4 > maxVerts )
			break;

		Phys_SubdivLevel( pts, sm, work );
	}

	if ( !done )
		return;

	sd.levels = done;
	sd.numPoints = (int)pts.size();
	sd.stencilStart.resize( sd.numPoints + 1 );

	for ( int p = 0; p < sd.numPoints; p++ )
	{
		sd.stencilStart[p] = (int)sd.stencilParticle.size();

		for ( size_t k = 0; k < pts[p].size(); k++ )
		{
			sd.stencilParticle.push_back( pts[p][k].first );
			sd.stencilWeight.push_back( pts[p][k].second );
		}
	}

	sd.stencilStart[sd.numPoints] = (int)sd.stencilParticle.size();
	sd.surfaces.resize( numSurfaces );

	// Chunks that fit in one batch of the renderer. A vertex that two chunks use is copied.
	for ( int s = 0; s < numSurfaces; s++ )
	{
		const physSubdivWork_t &w = work[s];
		physSubdivSurface_t &out = sd.surfaces[s];
		std::vector<int> local( w.point.size(), -1 );
		std::vector<int> used;
		int firstVert = 0, firstIndex = 0;

		for ( size_t t = 0; t + 2 < w.tris.size(); t += 3 )
		{
			const int *tri = &w.tris[t];
			const int need = ( local[tri[0]] < 0 ) + ( local[tri[1]] < 0 && tri[1] != tri[0] ) +
				( local[tri[2]] < 0 && tri[2] != tri[0] && tri[2] != tri[1] );

			if ( (int)out.point.size() - firstVert + need > maxChunkVerts || (int)out.indexes.size() - firstIndex + 3 > maxChunkIndexes )
			{
				Phys_SubdivCloseChunk( out, firstVert, firstIndex );

				for ( size_t i = 0; i < used.size(); i++ )
					local[used[i]] = -1;

				used.clear();
				firstVert = (int)out.point.size();
				firstIndex = (int)out.indexes.size();
			}

			for ( int k = 0; k < 3; k++ )
			{
				const int v = tri[k];

				if ( local[v] < 0 )
				{
					local[v] = (int)out.point.size() - firstVert;
					used.push_back( v );
					out.point.push_back( w.point[v] );
					out.st.push_back( w.st[v * 2] );
					out.st.push_back( w.st[v * 2 + 1] );
				}

				out.indexes.push_back( local[v] );
			}
		}

		Phys_SubdivCloseChunk( out, firstVert, firstIndex );
	}
}

void Phys_ApplySubdiv( const physSubdiv_t &sd, const float *in, float *out )
{
	for ( int p = 0; p < sd.numPoints; p++ )
	{
		float x = 0.0f, y = 0.0f, z = 0.0f;

		for ( int k = sd.stencilStart[p]; k < sd.stencilStart[p + 1]; k++ )
		{
			const float *v = &in[sd.stencilParticle[k] * 3];
			const float w = sd.stencilWeight[k];

			x += w * v[0];
			y += w * v[1];
			z += w * v[2];
		}

		out[p * 3 + 0] = x;
		out[p * 3 + 1] = y;
		out[p * 3 + 2] = z;
	}
}

void Phys_ApplySubdivScalar( const physSubdiv_t &sd, const float *in, float *out )
{
	for ( int p = 0; p < sd.numPoints; p++ )
	{
		float x = 0.0f;

		for ( int k = sd.stencilStart[p]; k < sd.stencilStart[p + 1]; k++ )
			x += sd.stencilWeight[k] * in[sd.stencilParticle[k]];

		out[p] = x;
	}
}

void Phys_SubdivSurfaceTris( const physSubdivSurface_t &surf, std::vector<int> &tris )
{
	tris.clear();

	for ( size_t c = 0; c + 3 < surf.chunks.size(); c += 4 )
	{
		const int firstVert = surf.chunks[c], firstIndex = surf.chunks[c + 2], numIndexes = surf.chunks[c + 3];

		for ( int i = 0; i < numIndexes; i++ )
			tris.push_back( firstVert + surf.indexes[firstIndex + i] );
	}
}

void Phys_SubdivTopology( physTopology_t &out, const physSubdiv_t &sd, const physTopology_t &base )
{
	out = physTopology_t();
	out.numParticles = sd.numPoints;
	out.bindPos.resize( sd.numPoints * 3 );
	out.firstVertex.assign( sd.numPoints, -1 );
	out.vertParticle.resize( sd.surfaces.size() );

	Phys_ApplySubdiv( sd, &base.bindPos[0], &out.bindPos[0] );

	std::vector<int> tris;

	for ( size_t s = 0; s < sd.surfaces.size(); s++ )
	{
		const physSubdivSurface_t &surf = sd.surfaces[s];

		out.vertParticle[s] = surf.point;

		for ( size_t v = 0; v < surf.point.size(); v++ )
		{
			if ( out.firstVertex[surf.point[v]] < 0 )
				out.firstVertex[surf.point[v]] = (int)( ( s << 24 ) | v );
		}

		Phys_SubdivSurfaceTris( surf, tris );

		for ( size_t t = 0; t + 2 < tris.size(); t += 3 )
		{
			const int a = surf.point[tris[t]], b = surf.point[tris[t + 1]], c = surf.point[tris[t + 2]];

			if ( a == b || b == c || a == c )
				continue;

			out.tris.push_back( a );
			out.tris.push_back( b );
			out.tris.push_back( c );
		}
	}

	// A point that no drawn vertex uses keeps the first vertex of its base particle.
	for ( int p = 0; p < sd.numPoints; p++ )
	{
		if ( out.firstVertex[p] < 0 )
			out.firstVertex[p] = p < base.numParticles ? base.firstVertex[p] : 0;
	}

	Phys_BuildEdges( out );
}

void Phys_SubdivIdentity( physSubdiv_t &out, const physSubdiv_t &mesh )
{
	out = physSubdiv_t();
	out.levels = mesh.levels;
	out.numPoints = mesh.numPoints;
	out.surfaces = mesh.surfaces;
	out.stencilStart.resize( mesh.numPoints + 1 );
	out.stencilParticle.resize( mesh.numPoints );
	out.stencilWeight.assign( mesh.numPoints, 1.0f );

	for ( int p = 0; p <= mesh.numPoints; p++ )
		out.stencilStart[p] = p;

	for ( int p = 0; p < mesh.numPoints; p++ )
		out.stencilParticle[p] = p;
}
