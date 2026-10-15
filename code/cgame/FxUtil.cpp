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

#include "common_headers.h"

#if !defined(FX_SCHEDULER_H_INC)
	#include "FxScheduler.h"
#endif

vec3_t	WHITE = {1.0f, 1.0f, 1.0f};

struct SEffectList
{
	CEffect *mEffect;
	int		mKillTime;
	bool	mPortal;
	short	mEffectID;		// effect template ID, 0 = made by code
};

// fxpool report counters, per effect ID, since the last report.
int				gFxTallyID = 0;
static int		fxPoolSpawned[FX_MAX_EFFECTS];
static int		fxPoolEvicted[FX_MAX_EFFECTS];
static int		fxPoolTransient[FX_MAX_EFFECTS];
static int		fxPoolEvictedNew;		// evicted in the same frame they were made
static int		fxPoolSince;			// theFxHelper.mTime of the last report

// Transient primitives (see FX_TRANSIENT_LIFE): drawn once by the next FX_Add, then deleted.
bool			gFxTransient = false;
static std::vector<SEffectList>	fxTransient;

static void FX_FreeTransient( void )
{
	for ( size_t i = 0; i < fxTransient.size(); i++ )
	{
		delete fxTransient[i].mEffect;
	}
	fxTransient.clear();
}

#define PI		3.14159f

SEffectList		effectList[MAX_EFFECTS];
static int		fxAllocCursor = 0;	// next slot FX_GetValidEffect tries
static SEffectList	*fxUpdating = NULL;	// slot that FX_Add updates or frees now
SFxHelper		theFxHelper;

int				activeFx = 0;
int				mMax = 0;
int				mMaxTime = 0;
int				drawnFx;
int				mParticles;
int				mOParticles;
int				mLines;
int				mTails;
qboolean		fxInitialized = qfalse;

SFxSpeeds		fxSpeeds;
bool			fxSpeedsOn = false;

//-------------------------
// FX_Free
//
// Frees all FX
//-------------------------
bool FX_Free( void )
{
	for ( int i = 0; i < MAX_EFFECTS; i++ )
	{
		if ( effectList[i].mEffect )
		{
			delete effectList[i].mEffect;
		}

		effectList[i].mEffect = 0;
	}

	activeFx = 0;
	FX_FreeTransient();

	theFxScheduler.Clean();
	return true;
}

//-------------------------
// FX_Stop
//
// Frees all active FX but leaves the templates
//-------------------------
void FX_Stop( void )
{
	for ( int i = 0; i < MAX_EFFECTS; i++ )
	{
		if ( effectList[i].mEffect )
		{
			delete effectList[i].mEffect;
		}

		effectList[i].mEffect = 0;
	}

	activeFx = 0;
	FX_FreeTransient();

	theFxScheduler.Clean(false);
}

//-------------------------
// FX_Init
//
// Preps system for use
//-------------------------
int	FX_Init( void )
{
	if ( fxInitialized == qfalse )
	{
		fxInitialized = qtrue;

		for ( int i = 0; i < MAX_EFFECTS; i++ )
		{
			effectList[i].mEffect = 0;
		}
	}

	FX_Free();

	mMax = 0;
	mMaxTime = 0;

	fxAllocCursor = 0;
	theFxHelper.Init();

	// ( nothing to see here, go away )
	//
	extern void FX_CopeWithAnyLoadedSaveGames();
	FX_CopeWithAnyLoadedSaveGames();

	return true;
}


//-------------------------
// FX_FreeMember
//-------------------------
static void FX_FreeMember( SEffectList *obj )
{
	obj->mEffect->Die();
	delete obj->mEffect;
	obj->mEffect = 0;

	activeFx--;
}


//-------------------------
// FX_GetValidEffect
//
// Finds an unused effect slot. The pool is used as a ring: slots are given in cursor order,
// so when the pool is full the slot at the cursor holds an effect at least one lap old.
// The old code evicted slot 0 every time, so each new effect killed the one made just before.
//-------------------------
static SEffectList *FX_GetValidEffect()
{
	if ( activeFx < MAX_EFFECTS )
	{
		for ( int i = 0; i < MAX_EFFECTS; i++ )
		{
			SEffectList *ef = &effectList[fxAllocCursor];

			if ( ++fxAllocCursor == MAX_EFFECTS )
			{
				fxAllocCursor = 0;
			}
			if ( ef->mEffect == 0 )
			{
				return ef;
			}
		}
	}

	// Report the error, but at most once a second. Once the pool is full this is reached on
	// every single allocation, and the formatting plus the console write cost far more than
	// the exhaustion being reported - enough to dominate a profile taken in a build that
	// still compiles this in.
#ifndef FINAL_BUILD
	static int nextOutOfEffectsPrint = 0;

	if ( theFxHelper.mTime >= nextOutOfEffectsPrint
		|| theFxHelper.mTime + 1000 < nextOutOfEffectsPrint )	// map change, time went back
	{
		nextOutOfEffectsPrint = theFxHelper.mTime + 1000;
		theFxHelper.Print( "FX system out of effects\n" );
	}
#endif

	// Evict the effect at the cursor. Its Die() can spawn death effects, which evict further on.
	// Do not evict the effect that FX_Add updates: its code continues on the freed object.
	SEffectList *victim = &effectList[fxAllocCursor];

	if ( ++fxAllocCursor == MAX_EFFECTS )
	{
		fxAllocCursor = 0;
	}

	if ( victim == fxUpdating )
	{
		victim = &effectList[fxAllocCursor];

		if ( ++fxAllocCursor == MAX_EFFECTS )
		{
			fxAllocCursor = 0;
		}
	}

	FXS_COUNT( evicted );
	fxPoolEvicted[victim->mEffectID]++;
	if ( victim->mEffect->GetTimeStart() == theFxHelper.mTime )
	{
		fxPoolEvictedNew++;
	}
	FX_FreeMember( victim );

	return victim;
}


//-------------------------
// FX_ActiveFx
//
// Returns whether these are any active or scheduled effects
//-------------------------
bool FX_ActiveFx(void)
{
	return ((activeFx > 0) || (theFxScheduler.NumScheduledFx() > 0));
}

int FX_ActiveCount( void )
{
	return activeFx;
}


//-------------------------
// FX_Add
//
// Adds all fx to the view
//-------------------------
void FX_Add( bool portal )
{
	int			i;
	SEffectList	*ef;

	drawnFx = 0;
	mParticles = 0;
	mOParticles = 0;
	mLines = 0;
	mTails = 0;

	int numFx = activeFx;	//but stop when there can't be any more left!
	for ( i = 0, ef = effectList; i < MAX_EFFECTS && numFx; i++, ef++ )
	{
		if ( ef->mEffect != 0)
		{
			--numFx;
			if (portal != ef->mPortal)
			{
				continue;	//this one does not render in this scene
			}
			// Effect is active
			fxUpdating = ef;
			if ( theFxHelper.mTime > ef->mKillTime )
			{
				// Clean up old effects, calling any death effects as needed
				// this flag just has to be cleared otherwise death effects might not happen correctly
				ef->mEffect->ClearFlags( FX_KILL_ON_IMPACT );
				FX_FreeMember( ef );
			}
			else
			{
				if ( ef->mEffect->Update() == false )
				{
					// We've been marked for death
					FX_FreeMember( ef );
				}
			}
			fxUpdating = NULL;
		}
	}

	// Transient primitives: draw once, then delete. The main pass deletes any that are left.
	for ( size_t t = 0; t < fxTransient.size(); t++ )
	{
		CEffect *effect = fxTransient[t].mEffect;

		if ( effect == 0 || ( portal != fxTransient[t].mPortal && portal ) )
		{
			continue;
		}
		fxTransient[t].mEffect = 0;
		if ( portal == fxTransient[t].mPortal )
		{
			effect->Update();
		}
		effect->ClearFlags( FX_KILL_ON_IMPACT );
		effect->Die();
		delete effect;
	}
	if ( !portal )
	{
		fxTransient.clear();
	}

	if ( fx_debug.integer == 2 && !portal )
	{
		if (theFxHelper.mFrameTime > 100 || theFxHelper.mFrameTime < 5)
		theFxHelper.Print( "theFxHelper.mFrameTime = %i\n", theFxHelper.mFrameTime );
	}
	if ( fx_debug.integer == 1 && !portal )
	{
		if ( theFxHelper.mTime > mMaxTime )
		{
			// decay pretty harshly when we do it
			mMax *= 0.9f;
			mMaxTime = theFxHelper.mTime + 200; // decay 5 times a second if we haven't set a new max
		}
		if ( activeFx > mMax )
		{
			// but we can never be less that the current activeFx count
			mMax = activeFx;
			mMaxTime = theFxHelper.mTime + 4000; // since we just increased the max, hold it for at least 4 seconds
		}

		// Particles
		if ( mParticles > 500 )
		{
			theFxHelper.Print( ">Particles  ^1%4i  ", mParticles );
		}
		else if ( mParticles > 250 )
		{
			theFxHelper.Print( ">Particles  ^3%4i  ", mParticles );
		}
		else
		{
			theFxHelper.Print( ">Particles  %4i  ", mParticles );
		}

		// Lines
		if ( mLines > 500 )
		{
			theFxHelper.Print( ">Lines ^1%4i\n", mLines );
		}
		else if ( mLines > 250 )
		{
			theFxHelper.Print( ">Lines ^3%4i\n", mLines );
		}
		else
		{
			theFxHelper.Print( ">Lines %4i\n", mLines );
		}

		// OParticles
		if ( mOParticles > 500 )
		{
			theFxHelper.Print( ">OParticles ^1%4i  ", mOParticles );
		}
		else if ( mOParticles > 250 )
		{
			theFxHelper.Print( ">OParticles ^3%4i  ", mOParticles );
		}
		else
		{
			theFxHelper.Print( ">OParticles %4i  ", mOParticles );
		}

		// Tails
		if ( mTails > 400 )
		{
			theFxHelper.Print( ">Tails ^1%4i\n", mTails );
		}
		else if ( mTails > 200 )
		{
			theFxHelper.Print( ">Tails ^3%4i\n", mTails );
		}
		else
		{
			theFxHelper.Print( ">Tails %4i\n", mTails );
		}

		// Active
		if ( activeFx > 600 )
		{
			theFxHelper.Print( ">Active     ^1%4i  ", activeFx );
		}
		else if ( activeFx > 400 )
		{
			theFxHelper.Print( ">Active     ^3%4i  ", activeFx );
		}
		else
		{
			theFxHelper.Print( ">Active     %4i  ", activeFx );
		}

		// Drawn
		if ( drawnFx > 600 )
		{
			theFxHelper.Print( ">Drawn ^1%4i  ", drawnFx );
		}
		else if ( drawnFx > 400 )
		{
			theFxHelper.Print( ">Drawn ^3%4i  ", drawnFx );
		}
		else
		{
			theFxHelper.Print( ">Drawn %4i  ", drawnFx );
		}

		// Max
		if ( mMax > 600 )
		{
			theFxHelper.Print( ">Max ^1%4i  ", mMax );
		}
		else if ( mMax > 400 )
		{
			theFxHelper.Print( ">Max ^3%4i  ", mMax );
		}
		else
		{
			theFxHelper.Print( ">Max %4i  ", mMax );
		}

		// Scheduled
		if ( theFxScheduler.NumScheduledFx() > 100 )
		{
			theFxHelper.Print( ">Scheduled ^1%4i\n", theFxScheduler.NumScheduledFx() );
		}
		else if ( theFxScheduler.NumScheduledFx() > 50 )
		{
			theFxHelper.Print( ">Scheduled ^3%4i\n", theFxScheduler.NumScheduledFx() );
		}
		else
		{
			theFxHelper.Print( ">Scheduled %4i\n", theFxScheduler.NumScheduledFx() );
		}
	}
}


//-------------------------
// FX_AddPrimitive
//
// Note - in the editor, this function may change *pEffect to NULL, indicating that
// all effects are being stopped.
//-------------------------
extern bool gEffectsInPortal;	//from FXScheduler.cpp so i don't have to pass it in on EVERY FX_ADD*
void FX_AddPrimitive( CEffect *pEffect, int killTime )
{
	if ( gFxTransient && killTime <= FX_TRANSIENT_LIFE )
	{
		SEffectList transient;

		transient.mEffect = pEffect;
		transient.mKillTime = theFxHelper.mTime + killTime;
		transient.mPortal = gEffectsInPortal;
		transient.mEffectID = (short)gFxTallyID;
		fxTransient.push_back( transient );
		FXS_COUNT( transient );
		fxPoolTransient[gFxTallyID]++;

		pEffect->SetTimeStart( theFxHelper.mTime );
		pEffect->SetTimeEnd( theFxHelper.mTime + killTime );
		return;
	}

	SEffectList *item = FX_GetValidEffect();
	FXS_COUNT( spawned );
	fxPoolSpawned[gFxTallyID]++;

	item->mEffect = pEffect;
	item->mEffectID = (short)gFxTallyID;
	item->mKillTime = theFxHelper.mTime + killTime;
	item->mPortal = gEffectsInPortal;	//global set in AddScheduledEffects

	activeFx++;

	// Stash these in the primitive so it has easy access to the vals
	pEffect->SetTimeStart( theFxHelper.mTime );
	pEffect->SetTimeEnd( theFxHelper.mTime + killTime );
}

//-------------------------
// FX_PoolReport
//
// "fxpool" command: per effect file, the primitives spawned and evicted per second since the
// last call, and the primitives live and scheduled now. Prints the top effects by spawn rate
// and by live count.
//-------------------------
static void FX_PoolPrintTop( const char *title, const int *key, const int *live, const int *sched, float perSec )
{
	static const int TOP = 12;
	int order[TOP];
	int n = 0;

	for ( int id = 0; id < FX_MAX_EFFECTS; id++ )
	{
		if ( key[id] <= 0 )
		{
			continue;
		}
		// Insertion into the sorted top list
		int pos = ( n < TOP ) ? n++ : TOP;
		while ( pos > 0 && key[order[pos - 1]] < key[id] )
		{
			if ( pos < TOP )
			{
				order[pos] = order[pos - 1];
			}
			pos--;
		}
		if ( pos < TOP )
		{
			order[pos] = id;
		}
	}

	CG_Printf( "%s\n   spawn/s   trans/s   evict/s    live   sched  effect\n", title );
	for ( int i = 0; i < n; i++ )
	{
		const int id = order[i];
		CG_Printf( "  %8.0f  %8.0f  %8.0f  %6i  %6i  %s\n",
			fxPoolSpawned[id] * perSec, fxPoolTransient[id] * perSec, fxPoolEvicted[id] * perSec, live[id], sched[id],
			theFxScheduler.GetEffectName( id ) );
	}
}

void FX_PoolReport( void )
{
	static int live[FX_MAX_EFFECTS], sched[FX_MAX_EFFECTS], made[FX_MAX_EFFECTS];
	int totalSpawned = 0, totalTransient = 0, totalEvicted = 0, totalSched = 0;

	memset( live, 0, sizeof( live ) );
	memset( sched, 0, sizeof( sched ) );

	for ( int i = 0; i < MAX_EFFECTS; i++ )
	{
		if ( effectList[i].mEffect )
		{
			live[effectList[i].mEffectID]++;
		}
	}
	theFxScheduler.CountScheduledFx( sched );

	for ( int id = 0; id < FX_MAX_EFFECTS; id++ )
	{
		made[id] = fxPoolSpawned[id] + fxPoolTransient[id];
		totalSpawned += fxPoolSpawned[id];
		totalTransient += fxPoolTransient[id];
		totalEvicted += fxPoolEvicted[id];
		totalSched += sched[id];
	}

	const int elapsed = theFxHelper.mTime - fxPoolSince;
	const float perSec = ( elapsed > 0 ) ? 1000.0f / elapsed : 0.0f;

	CG_Printf( "fxpool over %.1f s: live %i/%i  scheduled %i  spawned %.0f/s  transient %.0f/s  evicted %.0f/s (%i in their first frame)\n",
		elapsed * 0.001f, activeFx, MAX_EFFECTS, totalSched,
		totalSpawned * perSec, totalTransient * perSec, totalEvicted * perSec, fxPoolEvictedNew );
	FX_PoolPrintTop( "top by spawn rate (pool + transient):", made, live, sched, perSec );
	FX_PoolPrintTop( "top by live count:", live, live, sched, perSec );
	FX_PoolPrintTop( "top by scheduled count:", sched, live, sched, perSec );
	CG_Printf( "(code) = primitives made directly by game code, not by an .efx file\n" );

	memset( fxPoolSpawned, 0, sizeof( fxPoolSpawned ) );
	memset( fxPoolEvicted, 0, sizeof( fxPoolEvicted ) );
	memset( fxPoolTransient, 0, sizeof( fxPoolTransient ) );
	fxPoolEvictedNew = 0;
	fxPoolSince = theFxHelper.mTime;
}


//-------------------------
//  FX_AddParticle
//-------------------------
CParticle *FX_AddParticle(  int clientID, const vec3_t org, const vec3_t vel, const vec3_t accel, float gravity,
							float size1, float size2, float sizeParm,
							float alpha1, float alpha2, float alphaParm,
							const vec3_t sRGB, const vec3_t eRGB, float rgbParm,
							float rotation, float rotationDelta,
							const vec3_t min, const vec3_t max, float elasticity,
							int deathID, int impactID,
							int killTime, qhandle_t shader, int flags, int modelNum, int boltNum )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding effects when the system is paused
		return 0;
	}

	CParticle *fx = new CParticle;

	if ( fx )
	{
		if (flags&FX_RELATIVE && clientID>=0)
		{
			fx->SetOrigin1( NULL );
			fx->SetOrgOffset( org );
			fx->SetClient( clientID, modelNum, boltNum );
		}
		else
		{
			fx->SetOrigin1( org );
		}
		fx->SetVel( vel );
		fx->SetAccel( accel );
		fx->SetGravity( gravity );

		// RGB----------------
		fx->SetRGBStart( sRGB );
		fx->SetRGBEnd( eRGB );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size----------------
		fx->SetSizeStart( size1 );
		fx->SetSizeEnd( size2 );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetFlags( flags );
		fx->SetShader( shader );
		fx->SetRotation( rotation );
		fx->SetRotationDelta( rotationDelta );
		fx->SetElasticity( elasticity );
		fx->SetMin( min );
		fx->SetMax( max );
		fx->SetDeathFxID( deathID );
		fx->SetImpactFxID( impactID );

		FX_AddPrimitive( fx, killTime );
		// in the editor, fx may now be NULL
	}

	return fx;
}


//-------------------------
//  FX_AddLine
//-------------------------
CLine *FX_AddLine( int clientID, vec3_t start, vec3_t end, float size1, float size2, float sizeParm,
									float alpha1, float alpha2, float alphaParm,
									vec3_t sRGB, vec3_t eRGB, float rgbParm,
									int killTime, qhandle_t shader, int impactFX_id, int flags, int modelNum, int boltNum )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding new effects when the system is paused
		return 0;
	}

	CLine *fx = new CLine;

	if ( fx )
	{
		if (flags&FX_RELATIVE && clientID>=0)
		{
			fx->SetOrigin1( NULL );
			fx->SetOrgOffset( start ); //offset from bolt pos
			fx->SetVel( end );	//vel is the vector offset from bolt+orgOffset
			fx->SetClient( clientID, modelNum, boltNum );
		}
		else
		{
			fx->SetOrigin1( start );
			fx->SetOrigin2( end );
		}
		// RGB----------------
		fx->SetRGBStart( sRGB );
		fx->SetRGBEnd( eRGB );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size----------------
		fx->SetSizeStart( size1 );
		fx->SetSizeEnd( size2 );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetShader( shader );
		fx->SetFlags( flags );

		fx->SetSTScale( 1.0f, 1.0f );
		fx->SetImpactFxID( impactFX_id );

		FX_AddPrimitive( fx, killTime );
		// in the editor, fx may now be NULL
	}

	return fx;
}

//-------------------------
//  FX_AddElectricity
//-------------------------
CElectricity *FX_AddElectricity( int clientID, vec3_t start, vec3_t end, float size1, float size2, float sizeParm,
									float alpha1, float alpha2, float alphaParm,
									vec3_t sRGB, vec3_t eRGB, float rgbParm,
									float chaos, int killTime, qhandle_t shader, int flags, int modelNum, int boltNum )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding new effects when the system is paused
		return 0;
	}

	CElectricity *fx = new CElectricity;

	if ( fx )
	{
		if (flags&FX_RELATIVE && clientID>=0)
		{
			fx->SetOrigin1( NULL );
			fx->SetOrgOffset( start );//offset
			fx->SetVel( end );	//vel is the vector offset from bolt+orgOffset
			fx->SetClient( clientID, modelNum, boltNum );
		}
		else
		{
			fx->SetOrigin1( start );
			fx->SetOrigin2( end );
		}

		// RGB----------------
		fx->SetRGBStart( sRGB );
		fx->SetRGBEnd( eRGB );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size----------------
		fx->SetSizeStart( size1 );
		fx->SetSizeEnd( size2 );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetShader( shader );
		fx->SetFlags( flags );
		fx->SetChaos( chaos );

		fx->SetSTScale( 1.0f, 1.0f );

		FX_AddPrimitive( fx, killTime );
		// in the editor, fx may now be NULL?
		if ( fx )
		{
			fx->Initialize();
		}
	}

	return fx;
}


//-------------------------
//  FX_AddTail
//-------------------------
CTail *FX_AddTail( int clientID, vec3_t org, vec3_t vel, vec3_t accel,
							float size1, float size2, float sizeParm,
							float length1, float length2, float lengthParm,
							float alpha1, float alpha2, float alphaParm,
							vec3_t sRGB, vec3_t eRGB, float rgbParm,
							vec3_t min, vec3_t max, float elasticity,
							int deathID, int impactID,
							int killTime, qhandle_t shader, int flags, int modelNum, int boltNum )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding effects when the system is paused
		return 0;
	}

	CTail *fx = new CTail;

	if ( fx )
	{
		if (flags&FX_RELATIVE && clientID>=0)
		{
			fx->SetOrigin1( NULL );
			fx->SetOrgOffset( org );
			fx->SetClient( clientID, modelNum, boltNum );
		}
		else
		{
			fx->SetOrigin1( org );
		}
		fx->SetVel( vel );
		fx->SetAccel( accel );
		// RGB----------------
		fx->SetRGBStart( sRGB );
		fx->SetRGBEnd( eRGB );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size----------------
		fx->SetSizeStart( size1 );
		fx->SetSizeEnd( size2 );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Length----------------
		fx->SetLengthStart( length1 );
		fx->SetLengthEnd( length2 );

		if (( flags & FX_LENGTH_PARM_MASK ) == FX_LENGTH_WAVE )
		{
			fx->SetLengthParm( lengthParm * PI * 0.001f );
		}
		else if ( flags & FX_LENGTH_PARM_MASK )
		{
			fx->SetLengthParm( lengthParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetFlags( flags );
		fx->SetShader( shader );
		fx->SetElasticity( elasticity );
		fx->SetMin( min );
		fx->SetMax( max );
		fx->SetSTScale( 1.0f, 1.0f );
		fx->SetDeathFxID( deathID );
		fx->SetImpactFxID( impactID );

		FX_AddPrimitive( fx, killTime );
		// in the editor, fx may now be NULL
	}

	return fx;
}

//-------------------------
//  FX_AddCylinder
//-------------------------
CCylinder *FX_AddCylinder( int clientID, vec3_t start, vec3_t normal,
							float size1s, float size1e, float sizeParm,
							float size2s, float size2e, float size2Parm,
							float length1, float length2, float lengthParm,
							float alpha1, float alpha2, float alphaParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							int killTime, qhandle_t shader, int flags, int modelNum, int boltNum )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding new effects when the system is paused
		return 0;
	}

	CCylinder *fx = new CCylinder;

	if ( fx )
	{
		if (flags&FX_RELATIVE && clientID>=0)
		{
			fx->SetOrigin1( NULL );
			fx->SetOrgOffset( start );//offset
			//NOTE: relative version doesn't ever use normal!
			//fx->SetNormal( normal );
			fx->SetClient( clientID, modelNum, boltNum );
		}
		else
		{
			fx->SetOrigin1( start );
			fx->SetNormal( normal );
		}

		// RGB----------------
		fx->SetRGBStart( rgb1 );
		fx->SetRGBEnd( rgb2 );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size1----------------
		fx->SetSizeStart( size1s );
		fx->SetSizeEnd( size1e );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size2----------------
		fx->SetSize2Start( size2s );
		fx->SetSize2End( size2e );

		if (( flags & FX_SIZE2_PARM_MASK ) == FX_SIZE2_WAVE )
		{
			fx->SetSize2Parm( size2Parm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE2_PARM_MASK )
		{
			fx->SetSize2Parm( size2Parm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Length1---------------
		fx->SetLengthStart( length1 );
		fx->SetLengthEnd( length2 );

		if (( flags & FX_LENGTH_PARM_MASK ) == FX_LENGTH_WAVE )
		{
			fx->SetLengthParm( lengthParm * PI * 0.001f );
		}
		else if ( flags & FX_LENGTH_PARM_MASK )
		{
			fx->SetLengthParm( lengthParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetShader( shader );
		fx->SetFlags( flags );

		FX_AddPrimitive( fx, killTime );
	}

	return fx;
}

//-------------------------
//  FX_AddEmitter
//-------------------------
CEmitter *FX_AddEmitter( vec3_t org, vec3_t vel, vec3_t accel,
								float size1, float size2, float sizeParm,
								float alpha1, float alpha2, float alphaParm,
								vec3_t rgb1, vec3_t rgb2, float rgbParm,
								vec3_t angs, vec3_t deltaAngs,
								vec3_t min, vec3_t max, float elasticity,
								int deathID, int impactID, int emitterID,
								float density, float variance,
								int killTime, qhandle_t model, int flags )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding effects when the system is paused
		return 0;
	}

	CEmitter *fx = new CEmitter;

	if ( fx )
	{
		fx->SetOrigin1( org );
		fx->SetVel( vel );
		fx->SetAccel( accel );

		// RGB----------------
		fx->SetRGBStart( rgb1 );
		fx->SetRGBEnd( rgb2 );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size----------------
		fx->SetSizeStart( size1 );
		fx->SetSizeEnd( size2 );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetAngles( angs );
		fx->SetAngleDelta( deltaAngs );
		fx->SetFlags( flags );
		fx->SetModel( model );
		fx->SetElasticity( elasticity );
		fx->SetMin( min );
		fx->SetMax( max );
		fx->SetDeathFxID( deathID );
		fx->SetImpactFxID( impactID );
		fx->SetEmitterFxID( emitterID );
		fx->SetDensity( density );
		fx->SetVariance( variance );
		fx->SetOldTime( theFxHelper.mTime );

		fx->SetLastOrg( org );
		fx->SetLastVel( vel );

		FX_AddPrimitive( fx, killTime );
		// in the editor, fx may now be NULL
	}

	return fx;
}

//-------------------------
//  FX_AddLight
//-------------------------
CLight *FX_AddLight( vec3_t org, float size1, float size2, float sizeParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							int killTime, int flags )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding effects when the system is paused
		return 0;
	}

	CLight *fx = new CLight;

	if ( fx )
	{
		fx->SetOrigin1( org );

		// RGB----------------
		fx->SetRGBStart( rgb1 );
		fx->SetRGBEnd( rgb2 );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size----------------
		fx->SetSizeStart( size1 );
		fx->SetSizeEnd( size2 );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetFlags( flags );

		FX_AddPrimitive( fx, killTime );
		// in the editor, fx may now be NULL
	}

	return fx;

}


//-------------------------
//  FX_AddOrientedParticle
//-------------------------
COrientedParticle *FX_AddOrientedParticle( int clientID, vec3_t org, vec3_t norm, vec3_t vel, vec3_t accel,
						float size1, float size2, float sizeParm,
						float alpha1, float alpha2, float alphaParm,
						vec3_t rgb1, vec3_t rgb2, float rgbParm,
						float rotation, float rotationDelta,
						vec3_t min, vec3_t max, float bounce,
						int deathID, int impactID,
						int killTime, qhandle_t shader, int flags, int modelNum, int boltNum )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding effects when the system is paused
		return 0;
	}

	COrientedParticle *fx = new COrientedParticle;

	if ( fx )
	{
		if (flags&FX_RELATIVE && clientID>=0)
		{
			fx->SetOrigin1( NULL );
			fx->SetOrgOffset( org );//offset
			fx->SetNormalOffset( norm );
			fx->SetClient( clientID, modelNum, boltNum );
		}
		else
		{
			fx->SetOrigin1( org );
			fx->SetNormal( norm );
		}
		fx->SetVel( vel );
		fx->SetAccel( accel );

		// RGB----------------
		fx->SetRGBStart( rgb1 );
		fx->SetRGBEnd( rgb2 );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size----------------
		fx->SetSizeStart( size1 );
		fx->SetSizeEnd( size2 );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetFlags( flags );
		fx->SetShader( shader );
		fx->SetRotation( rotation );
		fx->SetRotationDelta( rotationDelta );
		fx->SetElasticity( bounce );
		fx->SetMin( min );
		fx->SetMax( max );
		fx->SetDeathFxID( deathID );
		fx->SetImpactFxID( impactID );

		FX_AddPrimitive( fx, killTime );
		// in the editor, fx may now be NULL
	}

	return fx;
}


//-------------------------
//  FX_AddPoly
//-------------------------
CPoly *FX_AddPoly( vec3_t *verts, vec2_t *st, int numVerts,
							vec3_t vel, vec3_t accel,
							float alpha1, float alpha2, float alphaParm,
							vec3_t rgb1, vec3_t rgb2, float rgbParm,
							vec3_t rotationDelta, float bounce, int motionDelay,
							int killTime, qhandle_t shader, int flags )
{
	if ( theFxHelper.mFrameTime < 1 || !verts )
	{ // disallow adding effects when the system is paused or the user doesn't pass in a vert array
		return 0;
	}

	CPoly *fx = new CPoly;

	if ( fx )
	{
		// Do a cheesy copy of the verts and texture coords into our own structure
		for ( int i = 0; i < numVerts; i++ )
		{
			VectorCopy( verts[i], fx->mOrg[i] );
			VectorCopy2( st[i], fx->mST[i] );
		}

		fx->SetVel( vel );
		fx->SetAccel( accel );

		// RGB----------------
		fx->SetRGBStart( rgb1 );
		fx->SetRGBEnd( rgb2 );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetFlags( flags );
		fx->SetShader( shader );
		fx->SetRot( rotationDelta );
		fx->SetElasticity( bounce );
		fx->SetMotionTimeStamp( motionDelay );
		fx->SetNumVerts( numVerts );

		// Now that we've set our data up, let's process it into a useful format
		fx->PolyInit();

		FX_AddPrimitive( fx, killTime );
		// in the editor, fx may now be NULL
	}

	return fx;
}


//-------------------------
//  FX_AddBezier
//-------------------------
CBezier *FX_AddBezier( const vec3_t start, const vec3_t end,
						const vec3_t control1, const vec3_t control1Vel,
						const vec3_t control2, const vec3_t control2Vel,
						float size1, float size2, float sizeParm,
						float alpha1, float alpha2, float alphaParm,
						const vec3_t sRGB, const vec3_t eRGB, const float rgbParm,
						int killTime, qhandle_t shader, int flags )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding new effects when the system is paused
		return 0;
	}

	CBezier *fx = new CBezier;

	if ( fx )
	{
		fx->SetOrigin1( start );
		fx->SetOrigin2( end );

		fx->SetControlPoints( control1, control2 );
		fx->SetControlVel( control1Vel, control2Vel );

		// RGB----------------
		fx->SetRGBStart( sRGB );
		fx->SetRGBEnd( eRGB );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size----------------
		fx->SetSizeStart( size1 );
		fx->SetSizeEnd( size2 );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}

		fx->SetShader( shader );
		fx->SetFlags( flags );

		fx->SetSTScale( 1.0f, 1.0f );

		FX_AddPrimitive( fx, killTime );
	}

	return fx;
}

//-------------------------
//  FX_AddFlash
//-------------------------
CFlash *FX_AddFlash( vec3_t origin, vec3_t sRGB, vec3_t eRGB, float rgbParm,
						int killTime, qhandle_t shader, int flags = 0 )
{
	if ( theFxHelper.mFrameTime < 1 )
	{ // disallow adding new effects when the system is paused
		return 0;
	}

	CFlash *fx = new CFlash;

	if ( fx )
	{
		fx->SetOrigin1( origin );

		// RGB----------------
		fx->SetRGBStart( sRGB );
		fx->SetRGBEnd( eRGB );

		if (( flags & FX_RGB_PARM_MASK ) == FX_RGB_WAVE )
		{
			fx->SetRGBParm( rgbParm * PI * 0.001f );
		}
		else if ( flags & FX_RGB_PARM_MASK )
		{
			// rgbParm should be a value from 0-100..
			fx->SetRGBParm( rgbParm * 0.01f * killTime + theFxHelper.mTime );
		}

/*		// Alpha----------------
		fx->SetAlphaStart( alpha1 );
		fx->SetAlphaEnd( alpha2 );

		if (( flags & FX_ALPHA_PARM_MASK ) == FX_ALPHA_WAVE )
		{
			fx->SetAlphaParm( alphaParm * PI * 0.001f );
		}
		else if ( flags & FX_ALPHA_PARM_MASK )
		{
			fx->SetAlphaParm( alphaParm * 0.01f * killTime + theFxHelper.mTime );
		}

		// Size----------------
		fx->SetSizeStart( size1 );
		fx->SetSizeEnd( size2 );

		if (( flags & FX_SIZE_PARM_MASK ) == FX_SIZE_WAVE )
		{
			fx->SetSizeParm( sizeParm * PI * 0.001f );
		}
		else if ( flags & FX_SIZE_PARM_MASK )
		{
			fx->SetSizeParm( sizeParm * 0.01f * killTime + theFxHelper.mTime );
		}
*/
		fx->SetShader( shader );
		fx->SetFlags( flags );

//		fx->SetSTScale( 1.0f, 1.0f );

		fx->Init();

		FX_AddPrimitive( fx, killTime );
	}

	return fx;
}

//-------------------------------------------------------
// Functions for limited backward compatibility with EF.
//	These calls can be used for simple programmatic
//	effects, temp effects or debug graphics.
// Note that this is not an all-inclusive list of
//	fx add functions from EF, nor are the calls guaranteed
//	to produce the exact same result.
//-------------------------------------------------------

//---------------------------------------------------
void FX_AddSprite( vec3_t origin, vec3_t vel, vec3_t accel,
							float scale, float dscale,
							float sAlpha, float eAlpha,
							float rotation, float bounce,
							int life, qhandle_t shader, int flags )
{
	FX_AddParticle( -1, origin, vel, accel, 0, scale, scale, 0,
							sAlpha, eAlpha, FX_ALPHA_LINEAR,
							WHITE, WHITE, 0,
							rotation, 0,
							vec3_origin, vec3_origin, bounce,
							0, 0,
							life, shader, flags );
}

//---------------------------------------------------
void FX_AddSprite( vec3_t origin, vec3_t vel, vec3_t accel,
							float scale, float dscale,
							float sAlpha, float eAlpha,
							vec3_t sRGB, vec3_t eRGB,
							float rotation, float bounce,
							int life, qhandle_t shader, int flags )
{
	FX_AddParticle( -1, origin, vel, accel, 0, scale, scale, 0,
							sAlpha, eAlpha, FX_ALPHA_LINEAR,
							sRGB, eRGB, 0,
							rotation, 0,
							vec3_origin, vec3_origin, bounce,
							0, 0,
							life, shader, flags );
}

//---------------------------------------------------
void FX_AddLine( vec3_t start, vec3_t end, float stScale,
							float width, float dwidth,
							float sAlpha, float eAlpha,
							int life, qhandle_t shader, int flags )
{
	FX_AddLine( -1, start, end, width, width, 0,
							sAlpha, eAlpha, FX_ALPHA_LINEAR,
							WHITE, WHITE, 0,
							life, shader, 0, 0 );
}

//---------------------------------------------------
void FX_AddLine( vec3_t start, vec3_t end, float stScale,
							float width, float dwidth,
							float sAlpha, float eAlpha,
							vec3_t sRGB, vec3_t eRGB,
							int life, qhandle_t shader, int flags )
{
	FX_AddLine( -1, start, end, width, width, 0,
							sAlpha, eAlpha, FX_ALPHA_LINEAR,
							sRGB, eRGB, 0,
							life, shader, 0, flags );
}

//---------------------------------------------------
void FX_AddQuad( vec3_t origin, vec3_t normal,
							vec3_t vel, vec3_t accel,
							float sradius, float eradius,
							float salpha, float ealpha,
							vec3_t sRGB, vec3_t eRGB,
							float rotation, int life, qhandle_t shader, int flags )
{
	FX_AddOrientedParticle( -1, origin, normal, vel, accel,
							sradius, eradius, 0.0f,
							salpha, ealpha, 0.0f,
							sRGB, eRGB, 0.0f,
							rotation, 0.0f,
							NULL, NULL, 0.0f, 0, 0, life,
							shader, 0 );
}
