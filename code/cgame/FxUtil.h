/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#if !defined(FX_PRIMITIVES_H_INC)
	#include "FxPrimitives.h"
#endif

#ifndef FX_UTIL_H_INC
#define FX_UTIL_H_INC


bool	FX_Free( void );	// ditches all active effects;
int		FX_Init( void );	// called in CG_Init to purge the fx system.
void	FX_Add( bool portal );		// called every cgame frame to add all fx into the scene.
void	FX_Stop( void );	// ditches all active effects without touching the templates.

bool	FX_ActiveFx(void);	// returns whether there are any active or scheduled effects
int		FX_ActiveCount( void );	// live effects in the pool

/*
FX cost breakdown for cg_speeds 2, in rdtsc cycles, summed over one report interval.
"update" includes the traces and the scene submission; the report subtracts them.
*/
#if defined(_MSC_VER)
	#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
	#include <x86intrin.h>
#endif

struct SFxSpeeds
{
	int64_t	create;			// AddScheduledEffects, without FX_Add
	int64_t	update;			// FX_Add
	int64_t	trace;			// world traces, BSP part of CG_Trace
	int64_t	clip;			// world traces, entity part of CG_Trace (CG_ClipMoveToEntities)
	int64_t	g2trace;		// ghoul2 traces
	int64_t	contents;		// CG_PointContents in UpdateOrigin
	int64_t	submit;			// AddRefEntityToScene, AddPolyToScene, AddLightToScene

	int		traces, g2traces, contentsCalls;
	int		clipEnts;		// entities traced against by CG_ClipMoveToEntities
	int		submits, polys, lights;
	int		spawned;		// effects allocated
	int		transient;		// effects drawn once outside the pool (FX_TRANSIENT_LIFE)
	int		evicted;		// effects destroyed because the pool was full
	int		liveSum, scheduledSum, frames;
};

extern SFxSpeeds	fxSpeeds;
extern bool			fxSpeedsOn;		// cg_speeds >= 2, set once per frame

// fxpool report: the effect ID that FX_AddPrimitive gives to new primitives. 0 = made by code, not an .efx.
extern int	gFxTallyID;

struct SFxTallyScope
{
	int	mPrev;
	explicit SFxTallyScope( int id ) : mPrev( gFxTallyID )	{ gFxTallyID = id; }
	~SFxTallyScope()										{ gFxTallyID = mPrev; }
};

void	FX_PoolReport( void );

// Projectile effects are played again every frame at the new position. While gFxTransient is set,
// FX_AddPrimitive keeps primitives of FX_TRANSIENT_LIFE ms or less out of the pool: FX_Add draws
// them once in this frame and deletes them.
#define FX_TRANSIENT_LIFE	33

extern bool	gFxTransient;

struct SFxTransientScope
{
	bool	mPrev;
	SFxTransientScope() : mPrev( gFxTransient )	{ gFxTransient = true; }
	~SFxTransientScope()						{ gFxTransient = mPrev; }
};

#define FXS_START( t )			uint64_t t = fxSpeedsOn ? __rdtsc() : 0
#define FXS_STOP( t, field )	if ( fxSpeedsOn ) { fxSpeeds.field += (int64_t)( __rdtsc() - t ); }
#define FXS_COUNT( field )		if ( fxSpeedsOn ) { fxSpeeds.field++; }


CParticle *FX_AddParticle( int clientID, const vec3_t org, const vec3_t vel, const vec3_t accel, float gravity,
							float size1, float size2, float sizeParm,
							float alpha1, float alpha2, float alphaParm,
							const vec3_t rgb1, const vec3_t rgb2, float rgbParm,
							float rotation, float rotationDelta,
							const vec3_t min, const vec3_t max, float elasticity,
							int deathID, int impactID,
							int killTime, qhandle_t shader, int flags, int modelNum = -1, int boltNum = -1 );

CLine *FX_AddLine( int clientID, vec3_t start, vec3_t end,
							float size1, float size2, float sizeParm,
							float alpha1, float alpha2, float alphaParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							int killTime, qhandle_t shader, int impactFX_id, int flags, int modelNum = -1, int boltNum = -1 );

CElectricity *FX_AddElectricity( int clientID, vec3_t start, vec3_t end, float size1, float size2, float sizeParm,
							float alpha1, float alpha2, float alphaParm,
							vec3_t sRGB, vec3_t eRGB, float rgbParm,
							float chaos, int killTime, qhandle_t shader, int flags, int modelNum = -1, int boltNum = -1 );

CTail *FX_AddTail( int clientID, vec3_t org, vec3_t vel, vec3_t accel,
							float size1, float size2, float sizeParm,
							float length1, float length2, float lengthParm,
							float alpha1, float alpha2, float alphaParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							vec3_t min, vec3_t max, float elasticity,
							int deathID, int impactID,
							int killTime, qhandle_t shader, int flags, int modelNum = -1, int boltNum = -1 );

CCylinder *FX_AddCylinder( int clientID, vec3_t start, vec3_t normal,
							float size1s, float size1e, float size1Parm,
							float size2s, float size2e, float size2Parm,
							float length1, float length2, float lengthParm,
							float alpha1, float alpha2, float alphaParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							int killTime, qhandle_t shader, int flags, int modelNum = -1, int boltNum = -1 );

CEmitter *FX_AddEmitter( vec3_t org, vec3_t vel, vec3_t accel,
							float size1, float size2, float sizeParm,
							float alpha1, float alpha2, float alphaParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							vec3_t angs, vec3_t deltaAngs,
							vec3_t min, vec3_t max, float elasticity,
							int deathID, int impactID, int emitterID,
							float density, float variance,
							int killTime, qhandle_t model, int flags );

CLight *FX_AddLight( vec3_t org, float size1, float size2, float sizeParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							int killTime, int flags );

COrientedParticle *FX_AddOrientedParticle( int clientID, vec3_t org, vec3_t norm, vec3_t vel, vec3_t accel,
							float size1, float size2, float sizeParm,
							float alpha1, float alpha2, float alphaParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							float rotation, float rotationDelta,
							vec3_t min, vec3_t max, float bounce,
							int deathID, int impactID,
							int killTime, qhandle_t shader, int flags, int modelNum = -1, int boltNum = -1 );

CPoly *FX_AddPoly( vec3_t *verts, vec2_t *st, int numVerts,
							vec3_t vel, vec3_t accel,
							float alpha1, float alpha2, float alphaParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							vec3_t rotationDelta, float bounce, int motionDelay,
							int killTime, qhandle_t shader, int flags );

CFlash *FX_AddFlash( vec3_t origin, vec3_t sRGB, vec3_t eRGB, float rgbParm,
						int life, qhandle_t shader, int flags );


// Included for backwards compatibility with CHC and for doing quick programmatic effects.
void FX_AddSprite( vec3_t origin, vec3_t vel, vec3_t accel,
							float scale, float dscale,
							float sAlpha, float eAlpha,
							float rotation, float bounce,
							int life, qhandle_t shader, int flags = 0 );

void FX_AddSprite( vec3_t origin, vec3_t vel, vec3_t accel,
							float scale, float dscale,
							float sAlpha, float eAlpha,
							vec3_t sRGB, vec3_t eRGB,
							float rotation, float bounce,
							int life, qhandle_t shader, int flags = 0 );

void FX_AddLine( vec3_t start, vec3_t end, float stScale,
							float width, float dwidth,
							float sAlpha, float eAlpha,
							int life, qhandle_t shader, int flags = 0 );

void FX_AddLine( vec3_t start, vec3_t end, float stScale,
							float width, float dwidth,
							float sAlpha, float eAlpha,
							vec3_t sRGB, vec3_t eRGB,
							int life, qhandle_t shader, int flags = 0 );

void FX_AddQuad( vec3_t origin, vec3_t normal,
						vec3_t vel, vec3_t accel,
						float sradius, float eradius,
						float salpha, float ealpha,
						vec3_t sRGB, vec3_t eRGB,
						float rotation, int life, qhandle_t shader, int flags = 0 );

CBezier *FX_AddBezier( const vec3_t start, const vec3_t end,
						const vec3_t control1, const vec3_t control1Vel,
						const vec3_t control2, const vec3_t control2Vel,
						float size1, float size2, float sizeParm,
						float alpha1, float alpha2, float alphaParm,
						const vec3_t sRGB, const vec3_t eRGB, const float rgbParm,
						int killTime, qhandle_t shader, int flags = 0 );


#endif //FX_UTIL_H_INC