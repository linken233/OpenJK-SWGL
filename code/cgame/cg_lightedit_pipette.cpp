// Light edit mode: tool 6 (pipette). The primary fire copies the aimed light into a clipboard, which also
// becomes the preset of tool 2. The secondary fire pastes it, with a filter that the wheel cycles.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>

typedef enum {
	LEF_ALL = 0,
	LEF_COLOR,
	LEF_INTENSITY,
	LEF_SHAPE,
	LEF_NUM
} ledPasteFilter_t;

static const char	*s_filterNames[LEF_NUM] = { "all", "colour", "intensity", "shape" };

static qboolean			s_clipValid = qfalse;
static rtxLightDesc_t	s_clip;				// intensity in the units of an added light
static int				s_filter = LEF_ALL;

void LE_PipetteInit( void )
{
	s_clipValid = qfalse;
	memset( &s_clip, 0, sizeof( s_clip ) );
	s_filter = LEF_ALL;
}

// The clipboard as a preset for a new light: type, colour, intensity, radius, direction and cones.
// An emissive surface is not a preset: its factor is not an intensity.
qboolean LE_PipettePreset( rtxLightDesc_t *d )
{
	if ( !s_clipValid || s_clip.type == RTX_LTYPE_SURFACE )
	{
		return qfalse;
	}
	d->type = s_clip.type;
	VectorCopy( s_clip.color, d->color );
	d->intensity = s_clip.intensity;
	d->radius = s_clip.radius;
	VectorCopy( s_clip.dir, d->dir );
	d->coneOuter = s_clip.coneOuter;
	d->coneInner = s_clip.coneInner;
	d->width = s_clip.width;
	d->height = s_clip.height;
	d->roll = s_clip.roll;
	d->twoSided = s_clip.twoSided;
	d->style = s_clip.style;
	return qtrue;
}

// Shift + wheel in tool 2 edits the preset: with a clipboard, the clipboard is the preset.
void LE_PipettePresetSet( const rtxLightDesc_t *d )
{
	VectorCopy( d->color, s_clip.color );
	s_clip.intensity = d->intensity;
	s_clip.radius = d->radius;
	s_clip.coneOuter = d->coneOuter;
	s_clip.coneInner = d->coneInner;
	s_clip.width = d->width;
	s_clip.height = d->height;
	s_clip.roll = d->roll;
	s_clip.twoSided = d->twoSided;
	s_clip.style = d->style;
}

static void LE_PipetteCopy( void )
{
	rtxLightDesc_t	src;
	float			scaleSrc, scaleNew;

	if ( s_pickId < 0 || !LE_GetDesc( s_pickId, &src ) || ( src.flags & RTX_LFLAG_DELETED ) )
	{
		LE_Msg( "light edit: aim at the light to copy" );
		return;
	}
	scaleSrc = s_api->IntensityScale( src.id );
	scaleNew = s_api->IntensityScale( -1 );
	s_clip = src;
	s_clip.id = -1;
	s_clip.flags = 0;
	VectorClear( s_clip.origin );
	s_clip.name[0] = 0;
	s_clipValid = qtrue;
	if ( src.type == RTX_LTYPE_SURFACE )
	{
		LE_Msg( "light edit: copied emissive surface %d (factor %.3g, tint %.2f %.2f %.2f, style %d)", src.id, src.intensity,
			src.color[0], src.color[1], src.color[2], src.style );
		return;
	}
	if ( scaleSrc > 0.0f && scaleNew > 0.0f )
	{
		s_clip.intensity = src.intensity * scaleSrc / scaleNew;
	}
	LE_CreateSetType( src.type );
	LE_Msg( "light edit: copied light %d (%s, intensity %.1f); tool 2 uses it as its preset", src.id,
		LE_TypeName( src.type ), s_clip.intensity );
}

// Gives d the clipboard values that the filter selects. The intensity goes back to the units of the target.
// An emissive surface and a light share the colour and the style only; two surfaces share the factor too.
// Returns qfalse when nothing applies.
static qboolean LE_PipetteApply( rtxLightDesc_t *d )
{
	const qboolean	toSurface = (qboolean)( d->type == RTX_LTYPE_SURFACE );
	const qboolean	fromSurface = (qboolean)( s_clip.type == RTX_LTYPE_SURFACE );
	qboolean		applied = qfalse;

	if ( s_filter == LEF_ALL || s_filter == LEF_COLOR )
	{
		VectorCopy( s_clip.color, d->color );
		applied = qtrue;
	}
	if ( ( s_filter == LEF_ALL || s_filter == LEF_INTENSITY ) && toSurface && fromSurface )
	{
		d->intensity = s_clip.intensity;
		applied = qtrue;
	}
	else if ( ( s_filter == LEF_ALL || s_filter == LEF_INTENSITY ) && !toSurface && !fromSurface )
	{
		const float	scaleNew = s_api->IntensityScale( -1 );
		const float	scaleDst = s_api->IntensityScale( d->id );

		d->intensity = ( scaleNew > 0.0f && scaleDst > 0.0f ) ? s_clip.intensity * scaleNew / scaleDst : s_clip.intensity;
		applied = qtrue;
	}
	if ( ( s_filter == LEF_ALL || s_filter == LEF_SHAPE ) && !toSurface && !fromSurface )
	{
		applied = qtrue;
		if ( s_clip.type != d->type )
		{
			if ( s_clip.type != RTX_LTYPE_SPHERE )
			{
				VectorCopy( s_clip.dir, d->dir );
			}
			LE_ConvertType( d, s_clip.type );
		}
		if ( s_clip.type == RTX_LTYPE_SPOT )
		{
			d->coneOuter = s_clip.coneOuter;
			d->coneInner = s_clip.coneInner;
		}
		else if ( s_clip.type == RTX_LTYPE_RECT )
		{
			d->width = s_clip.width;
			d->height = s_clip.height;
			d->roll = s_clip.roll;
			d->twoSided = s_clip.twoSided;
		}
		d->radius = s_clip.radius;
	}
	if ( s_filter == LEF_ALL )
	{
		d->style = s_clip.style;
		applied = qtrue;
	}
	return applied;
}

static void LE_PipettePaste( void )
{
	std::vector<int>	ids;
	int					done = 0, refused = 0;

	if ( !s_clipValid )
	{
		LE_Msg( "light edit: the clipboard is empty (fire copies the aimed light)" );
		return;
	}
	if ( s_pickId < 0 )
	{
		LE_Msg( "light edit: aim at the light to paste on" );
		return;
	}
	LE_ActionTargets( ids );
	{
		ledBatch	batch;

		LE_UndoBegin( "paste" );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d;

			if ( !LE_GetDesc( ids[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) )
			{
				continue;
			}
			if ( !LE_PipetteApply( &d ) )
			{
				refused++;
				continue;
			}
			if ( LE_DoSet( ids[i], &d, "paste" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: pasted %s on %d light%s", s_filterNames[s_filter], done, done > 1 ? "s" : "" );
	}
	if ( refused )
	{
		LE_Msg( "light edit: %d target%s took nothing: an emissive surface and a light share the colour and the style only",
			refused, refused > 1 ? "s" : "" );
	}
}

void LE_ToolPipette( qboolean priDown, qboolean secDown, int wheel )
{
	if ( wheel )
	{
		s_filter = ( ( s_filter + wheel ) % LEF_NUM + LEF_NUM ) % LEF_NUM;
	}
	if ( priDown )
	{
		LE_PipetteCopy();
	}
	if ( secDown )
	{
		LE_PipettePaste();
	}
}

void LE_PipetteHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize )
{
	static char	altBuf[96];

	*name = "6 Pipette";
	*fire = "copy the aimed light (it becomes the preset of tool 2) or emissive surface (factor, tint, style)";
	if ( s_clipValid )
	{
		Com_sprintf( altBuf, sizeof( altBuf ), "paste %s on the aimed light (the selection if it is a member)", s_filterNames[s_filter] );
	}
	else
	{
		Com_sprintf( altBuf, sizeof( altBuf ), "paste on the aimed light (the clipboard is empty)" );
	}
	*alt = altBuf;
	Com_sprintf( wheelBuf, wheelSize, "paste filter: all / colour / intensity / shape (now %s)", s_filterNames[s_filter] );
}
