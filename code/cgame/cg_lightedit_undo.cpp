// Light edit mode: undo and redo stacks of grouped entries.
// A group holds the sub-entries of one user action. Undo applies them in reverse order,
// redo in order.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>

#define LEDIT_UNDO_DEPTH		256

typedef struct {
	int				kind;
	int				id;
	rtxLightDesc_t	before;
	rtxLightDesc_t	after;
	float			scaleBefore;	// LEDU_EMISSIVE
	float			scaleAfter;
	rtxSkyDesc_t	skyBefore;		// LEDU_SKY
	rtxSkyDesc_t	skyAfter;
} ledUndoSub_t;

#define LEDIT_MERGE_MS			500

struct ledUndoGroup_t {
	char						label[48];
	std::vector<ledUndoSub_t>	subs;
	unsigned					mergeKey;		// 0 for a group that does not merge
	int							mergeTime;		// cg.time of the last change

	ledUndoGroup_t() : mergeKey( 0 ), mergeTime( 0 ) { label[0] = 0; }
};

static std::vector<ledUndoGroup_t>	s_undo;
static std::vector<ledUndoGroup_t>	s_redo;
static ledUndoGroup_t				s_open;
static qboolean						s_groupOpen = qfalse;
static qboolean						s_merging = qfalse;		// the open group extends the top of the undo stack

void LE_UndoClear( void )
{
	s_undo.clear();
	s_redo.clear();
	s_open.subs.clear();
	s_groupOpen = qfalse;
	s_merging = qfalse;
}

int LE_UndoDepth( void )
{
	return (int)s_undo.size();
}

int LE_RedoDepth( void )
{
	return (int)s_redo.size();
}

static void LE_UndoPushGroup( const ledUndoGroup_t &g )
{
	if ( (int)s_undo.size() == LEDIT_UNDO_DEPTH )
	{
		s_undo.erase( s_undo.begin() );
	}
	s_undo.push_back( g );
	s_redo.clear();
}

void LE_UndoBegin( const char *label )
{
	s_open.subs.clear();
	s_open.mergeKey = 0;
	Q_strncpyz( s_open.label, label, sizeof( s_open.label ) );
	s_groupOpen = qtrue;
	s_merging = qfalse;
}

// A group with a key joins the top group when that one has the same key, is under 500 ms old and no undo happened since.
void LE_UndoBeginMerge( const char *label, unsigned key )
{
	LE_UndoBegin( label );
	s_open.mergeKey = key;
	s_merging = (qboolean)( key && s_redo.empty() && !s_undo.empty() && s_undo.back().mergeKey == key
		&& cg.time - s_undo.back().mergeTime <= LEDIT_MERGE_MS );
}

void LE_UndoEnd( void )
{
	// A merging group writes into the top group directly: there is nothing to push.
	if ( s_groupOpen && !s_merging && !s_open.subs.empty() )
	{
		s_open.mergeTime = cg.time;
		LE_UndoPushGroup( s_open );
	}
	s_open.subs.clear();
	s_groupOpen = qfalse;
	s_merging = qfalse;
}

static void LE_UndoPushSub( const ledUndoSub_t &sub, const char *label );

// Without an open group, the entry makes a group of its own.
void LE_UndoPush( int kind, int id, const rtxLightDesc_t *before, const rtxLightDesc_t *after, const char *label )
{
	ledUndoSub_t	sub;

	memset( &sub, 0, sizeof( sub ) );
	sub.kind = kind;
	sub.id = id;
	if ( before )
	{
		sub.before = *before;
	}
	if ( after )
	{
		sub.after = *after;
	}
	LE_UndoPushSub( sub, label );
}

// Entry for the emissive scale of a shader.
void LE_UndoPushScale( int shader, float before, float after, const char *label )
{
	ledUndoSub_t	sub;

	memset( &sub, 0, sizeof( sub ) );
	sub.kind = LEDU_EMISSIVE;
	sub.id = shader;
	sub.scaleBefore = before;
	sub.scaleAfter = after;
	LE_UndoPushSub( sub, label );
}

// Entry for the sky and sun of the map.
void LE_UndoPushSky( const rtxSkyDesc_t *before, const rtxSkyDesc_t *after, const char *label )
{
	ledUndoSub_t	sub;

	memset( &sub, 0, sizeof( sub ) );
	sub.kind = LEDU_SKY;
	sub.skyBefore = *before;
	sub.skyAfter = *after;
	LE_UndoPushSub( sub, label );
}

static void LE_UndoPushSub( const ledUndoSub_t &sub, const char *label )
{
	const int	kind = sub.kind;
	const int	id = sub.id;

	if ( s_groupOpen && s_merging )
	{
		std::vector<ledUndoSub_t>	&top = s_undo.back().subs;
		size_t						i;

		for ( i = 0; i < top.size(); i++ )
		{
			if ( top[i].id == id && top[i].kind == kind )
			{
				top[i].after = sub.after;
				top[i].scaleAfter = sub.scaleAfter;
				top[i].skyAfter = sub.skyAfter;
				break;
			}
		}
		if ( i == top.size() )
		{
			top.push_back( sub );
		}
		s_undo.back().mergeTime = cg.time;
		return;
	}
	if ( s_groupOpen )
	{
		s_open.subs.push_back( sub );
		return;
	}

	ledUndoGroup_t	g;

	Q_strncpyz( g.label, label, sizeof( g.label ) );
	g.subs.push_back( sub );
	LE_UndoPushGroup( g );
}

static qboolean LE_UndoApply( const ledUndoSub_t &e, qboolean undo )
{
	switch ( e.kind )
	{
	case LEDU_SET:
		return s_api->Set( e.id, undo ? &e.before : &e.after );
	case LEDU_ADD:
		return undo ? s_api->Remove( e.id ) : s_api->Restore( e.id );
	case LEDU_REMOVE:
		return undo ? s_api->Restore( e.id ) : s_api->Remove( e.id );
	case LEDU_RESTORE:
		return undo ? s_api->Remove( e.id ) : s_api->Restore( e.id );
	case LEDU_EMISSIVE:
		return s_api->SetEmissiveScale( e.id, undo ? e.scaleBefore : e.scaleAfter );
	case LEDU_SKY:
		return LE_SkyApply( undo ? &e.skyBefore : &e.skyAfter );
	}
	return qfalse;
}

// Applies a group, then selects the lights that are still there.
static int LE_ApplyGroup( const ledUndoGroup_t &g, qboolean undo )
{
	const int	n = (int)g.subs.size();
	int			failed = 0;

	{
		ledBatch	batch;

		for ( int i = 0; i < n; i++ )
		{
			const ledUndoSub_t	&e = g.subs[undo ? n - 1 - i : i];

			if ( !LE_UndoApply( e, undo ) )
			{
				failed++;
			}
		}
	}

	bool	hasLights = false;

	for ( int i = 0; i < n; i++ )
	{
		hasLights = hasLights || g.subs[i].kind < LEDU_EMISSIVE;
	}
	if ( !hasLights )
	{
		return failed;
	}
	LE_SelClear();
	for ( int i = 0; i < n; i++ )
	{
		const ledUndoSub_t	&e = g.subs[i];

		if ( e.kind >= LEDU_EMISSIVE )
		{
			continue;
		}
		const bool			gone = undo ? ( e.kind == LEDU_ADD || e.kind == LEDU_RESTORE ) : ( e.kind == LEDU_REMOVE );

		if ( !gone && !LE_SelHas( e.id ) )
		{
			LE_SelAdd( e.id );
		}
	}
	return failed;
}

static const char *LE_GroupDesc( const ledUndoGroup_t &g )
{
	if ( g.subs.size() == 1 && g.subs[0].kind == LEDU_EMISSIVE )
	{
		return va( "%s (shader %d)", g.label, g.subs[0].id );
	}
	if ( g.subs.size() == 1 && g.subs[0].kind == LEDU_SKY )
	{
		return g.label;
	}
	if ( g.subs.size() == 1 )
	{
		return va( "%s (light %d)", g.label, g.subs[0].id );
	}
	return va( "%s (%d lights)", g.label, (int)g.subs.size() );
}

void LE_Undo( void )
{
	if ( LE_GrabActive() )
	{
		LE_Msg( "light edit: release the light first" );
		return;
	}
	if ( s_undo.empty() )
	{
		LE_Msg( "light edit: nothing to undo" );
		return;
	}

	const ledUndoGroup_t	g = s_undo.back();
	const int				failed = LE_ApplyGroup( g, qtrue );

	s_undo.pop_back();
	s_redo.push_back( g );
	if ( failed )
	{
		LE_Msg( "light edit: undo %s: %d of %d failed (%s)", LE_GroupDesc( g ), failed, (int)g.subs.size(), s_api->LastError() );
		return;
	}
	LE_Msg( "light edit: undo %s", LE_GroupDesc( g ) );
}

void LE_Redo( void )
{
	if ( LE_GrabActive() )
	{
		LE_Msg( "light edit: release the light first" );
		return;
	}
	if ( s_redo.empty() )
	{
		LE_Msg( "light edit: nothing to redo" );
		return;
	}

	const ledUndoGroup_t	g = s_redo.back();
	const int				failed = LE_ApplyGroup( g, qfalse );

	s_redo.pop_back();
	s_undo.push_back( g );
	if ( failed )
	{
		LE_Msg( "light edit: redo %s: %d of %d failed (%s)", LE_GroupDesc( g ), failed, (int)g.subs.size(), s_api->LastError() );
		return;
	}
	LE_Msg( "light edit: redo %s", LE_GroupDesc( g ) );
}

// One line per group, most recent first.
void LE_UndoHistory( void )
{
	CG_Printf( "light edit: %d undo, %d redo\n", (int)s_undo.size(), (int)s_redo.size() );
	for ( int i = (int)s_undo.size() - 1; i >= 0; i-- )
	{
		CG_Printf( "  undo %3d: %s, %d sub-entries\n", (int)s_undo.size() - i, LE_GroupDesc( s_undo[i] ), (int)s_undo[i].subs.size() );
	}
	for ( int i = (int)s_redo.size() - 1; i >= 0; i-- )
	{
		CG_Printf( "  redo %3d: %s, %d sub-entries\n", (int)s_redo.size() - i, LE_GroupDesc( s_redo[i] ), (int)s_redo[i].subs.size() );
	}
}
