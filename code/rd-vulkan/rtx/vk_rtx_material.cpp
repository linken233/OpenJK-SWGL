/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

#include "tr_local.h"
#include "conversion.h"
#include "rtx_light_emissive.h"

static rtx_material_t rtx_materials[MAX_SHADERS];

static void MAT_SetIndex( rtx_material_t *mat )
{
	mat->flags = (mat->flags & ~MATERIAL_INDEX_MASK) | (mat->index & MATERIAL_INDEX_MASK);
}

uint32_t MAT_SetKind(uint32_t material, uint32_t kind)
{
	return (material & ~MATERIAL_KIND_MASK) | kind;
}

bool MAT_IsKind(uint32_t material, uint32_t kind)
{
	return (material & MATERIAL_KIND_MASK) == kind;
}

// Frame materials are taken from the top of the table, clear of the shaders.
static uint32_t rtx_next_frame_material = MATERIAL_INDEX_MASK;

void vk_rtx_clear_material_list( void ) 
{
	Com_Memset( &rtx_materials, 0, sizeof(rtx_materials) );
	rtx_next_frame_material = MATERIAL_INDEX_MASK;
}

void vk_rtx_clear_material( uint32_t index ) 
{
	Com_Memset( rtx_materials + index, 0, sizeof(rtx_material_t) );
}

 rtx_material_t *vk_rtx_get_material( uint32_t index ) 
{
	if ( index >= MAX_SHADERS )
		return NULL;

	if ( !rtx_materials[index].active )
		vk_rtx_clear_material( index );

	return &rtx_materials[index];
}

qboolean RB_IsTransparent( shader_t *shader ) 
{
	// skip certain objects that are transparent but should be handled like opaque objects
	if ( strstr(shader->name, "glass" ) )
		return qfalse;

	if ( ( shader->contentFlags & CONTENTS_TRANSLUCENT ) == CONTENTS_TRANSLUCENT && shader->sort == SS_OPAQUE ) 
		return qfalse;

	if ( ( shader->contentFlags & CONTENTS_TRANSLUCENT ) == CONTENTS_TRANSLUCENT || shader->sort > SS_OPAQUE ) 
		return qtrue;

	return qfalse;
}

qboolean RB_IsMasked( shader_t *shader ) 
{
	rtx_material_t *mat;

	mat = vk_rtx_get_material( (uint32_t)shader->index );

	if ( !mat )
		return qfalse;

	if ( (mat->flags & MATERIAL_FLAG_MASKED) == MATERIAL_FLAG_MASKED )
		return qtrue;

    return qfalse;
}

qboolean RB_IsSky(shader_t* shader)
{
	return (qboolean)(shader->isSky || shader->sun || (shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY)));
}

qboolean RB_IsDynamicGeometry( shader_t *shader ) 
{
	return (qboolean)((shader->numDeforms > 0) || (backEnd.currentEntity->e.frame > 0 || backEnd.currentEntity->e.oldframe > 0));
}

qboolean RB_IsDynamicMaterial( shader_t *shader ) {
	uint32_t i, j;
	qboolean changes = qfalse;

	for ( i = 0; i < MAX_SHADER_STAGES; i++ ) 
	{
		if ( shader->stages[i] != NULL && shader->stages[i]->active ) 
		{
			for ( j = 0; j < shader->stages[i]->numTexBundles; j++ ) 
			{

				if ( shader->stages[i]->bundle[j].numImageAnimations > 0 ) 
					return qtrue;

				if ( (shader->stages[i]->bundle[j].tcGen != TCGEN_BAD) && (shader->stages[i]->bundle[j].numTexMods > 0 ) ) 
					return qtrue;

				if ( shader->stages[i]->bundle[0].rgbGen == CGEN_WAVEFORM )
					return qtrue;
			}
		}
	}
	return changes;
}

static qboolean RB_NeedsColor() {

	for (int i = 0; i < MAX_SHADER_STAGES; i++) {
		if (tess.shader->stages[i] != NULL && tess.shader->stages[i]->active) {
			if (tess.shader->stages[i]->bundle[0].rgbGen == CGEN_WAVEFORM) {
				return qtrue;
			}
		}
	}
	return qfalse;
}

qboolean RB_StageNeedsColor( shaderStage_t *stage ) 
{
	//if ( strstr( tess.shader->name, "fog" ) )
		//return qtrue;

	if ( stage == NULL || !stage->active ) 
		return qfalse;

	if ( stage->bundle[0].rgbGen == CGEN_WAVEFORM || stage->bundle[0].rgbGen == CGEN_CONST ) {
		return qtrue;
	}

	return qfalse;
}

qboolean RB_SkipObject(shader_t* shader) {
	
	if ( strstr( shader->name, "glass" ) )
		return qfalse;

	if ( RB_IsSky(shader) )
		return qfalse;

	if ( strstr( shader->name, "Shadow" )
		|| shader->surfaceFlags == SURF_NODRAW /*|| shader->surfaceFlags == SURF_NONE*///SURF_SKIP
		|| shader->stages[0] == NULL 
		|| !shader->stages[0]->active )
		return qtrue;

	return qfalse;
}

// refactor me
uint32_t vk_rtx_find_emissive_texture( const shader_t *shader, rtx_material_t *material )
{
	uint32_t i, j;

	uint32_t lastValidStage = 0;
	uint32_t numStages = 0;
	shaderStage_t *pStage;

	for ( i = 0; i < MAX_RTX_STAGES; i++ ) 
	{
		pStage = shader->stages[i];

		if ( !pStage || !pStage->active || ( pStage->ss && pStage->ss->type ) )
			continue;

		lastValidStage = i;
		numStages++;

		if ( !pStage->glow )
			continue;

		for ( j = 0; j < pStage->numTexBundles; ++j ) 
		{
			if ( pStage->bundle[j].glow && pStage->bundle[j].image[0] ) 
			{
				if ( material != NULL )
					material->glow_emissive = qtrue;

				//Com_Printf("found glow texture: %d = %s", pStage->bundle[j].image[0]->index, pStage->bundle[j].image[0]->imgName );
				
				return pStage->bundle[j].image[0]->index;
			}
		}

	}

	// no glow texture found, try surfacelight fallback type
	if ( !shader->surfacelight )
		return 0;

	// masked light texture is usualy in the last stage. 
	uint32_t stage = (numStages == 0) ? 0 : lastValidStage;
	pStage = shader->stages[stage];

	if ( !pStage ) // check if stage 0 exists
		return 0;

	const int bundle = (pStage->numTexBundles == 0) ? 0 : pStage->numTexBundles-1;
	image_t *fallback = pStage->bundle[bundle].image[0];

	if ( fallback ) 
	{
		if ( material != NULL )
			material->surface_light = shader->surfacelight;

		return fallback->index;
	}
	return 0;
}

uint32_t RB_GetNextTex( shader_t *shader, int stage ) 
{
	int indexAnim = 0;
	if (shader->stages[stage]->bundle[0].numImageAnimations > 1) 
	{
		// tess.shadertime is wrong if shader != tess.shader
		// sunny
		indexAnim = (int)(tess.shaderTime * shader->stages[stage]->bundle[0].imageAnimationSpeed * FUNCTABLE_SIZE);
		indexAnim >>= FUNCTABLE_SIZE2;
		if (indexAnim < 0) {
			indexAnim = 0;	// may happen with shader time offsets
		}
		indexAnim %= shader->stages[stage]->bundle[0].numImageAnimations;
	}
	return indexAnim;
}

uint32_t RB_GetNextTexEncoded( shader_t *shader, int stage ) 
{
	if (shader->stages[stage] != NULL && shader->stages[stage]->active) 
	{
		int indexAnim = RB_GetNextTex( shader, stage );

		uint32_t blend = 0;
		uint32_t stateBits = shader->stages[stage]->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
		
		if ((stateBits & GLS_SRCBLEND_BITS) > GLS_SRCBLEND_ONE && (stateBits & GLS_DSTBLEND_BITS) > GLS_DSTBLEND_ONE) 
			blend = TEX0_NORMAL_BLEND_MASK;

		if (stateBits == 19) 
			blend = TEX0_MUL_BLEND_MASK;

		if (stateBits == 34 || stateBits == 1073742080) 
			blend = TEX0_ADD_BLEND_MASK;

		if (stateBits == 101) 
			blend = TEX0_NORMAL_BLEND_MASK;

		qboolean color = RB_StageNeedsColor( shader->stages[stage] );

		uint32_t nextidx = (uint32_t)indexAnim;
		uint32_t idx = shader->stages[stage]->bundle[0].image[nextidx]->index;

		shader->stages[stage]->bundle[0].image[nextidx]->frameUsed = tr.frameCount;
		return (idx) ;//| (blend) | (color ? TEX0_COLOR_MASK : 0);
	}
	return TEX0_IDX_MASK;
}

void vk_rtx_update_shader_material( shader_t *shader, shader_t *updatedShader )
{
	if ( !vk.rtxActive )
		return;

	// clear material first so mat->uploaded[idx] is reset reupload in the next frame in
	// vk_rtx_upload_materials() 

	if ( updatedShader )
	{
		vk_rtx_clear_material( (uint32_t)updatedShader->index );
		vk_rtx_shader_to_material( updatedShader );
	}

	if ( shader )
	{
		vk_rtx_clear_material( (uint32_t)shader->index );
		vk_rtx_shader_to_material( shader );
	}
}

uint32_t vk_get_rtx_material_stage_tex_mode( Vk_Pipeline_Def *def )
{
    switch ( def->shader_type ) {
        case TYPE_MULTI_TEXTURE_MUL2:
        case TYPE_MULTI_TEXTURE_MUL2_ENV:
        case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
        case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
        case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
        case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
        case TYPE_MULTI_TEXTURE_MUL3:
        case TYPE_MULTI_TEXTURE_MUL3_ENV:
        case TYPE_BLEND2_MUL:
        case TYPE_BLEND2_MUL_ENV:
        case TYPE_BLEND3_MUL:
        case TYPE_BLEND3_MUL_ENV:
            return 0;
            break;

        case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
        case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
        case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
        case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
        case TYPE_MULTI_TEXTURE_ADD2_1_1:
        case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
        case TYPE_MULTI_TEXTURE_ADD3_1_1:
        case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
            return 1;
            break;

        case TYPE_MULTI_TEXTURE_ADD2:
        case TYPE_MULTI_TEXTURE_ADD2_ENV:
        case TYPE_MULTI_TEXTURE_ADD3:
        case TYPE_MULTI_TEXTURE_ADD3_ENV:
        case TYPE_BLEND2_ADD:
        case TYPE_BLEND2_ADD_ENV:
        case TYPE_BLEND3_ADD:
        case TYPE_BLEND3_ADD_ENV:
            return 2;
            break;

        case TYPE_BLEND2_ALPHA:
        case TYPE_BLEND2_ALPHA_ENV:
        case TYPE_BLEND3_ALPHA:
        case TYPE_BLEND3_ALPHA_ENV:
            return 3;
            break;

        case TYPE_BLEND2_ONE_MINUS_ALPHA:
        case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
        case TYPE_BLEND3_ONE_MINUS_ALPHA:
        case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
            return 4;
            break;

        case TYPE_BLEND2_MIX_ALPHA:
        case TYPE_BLEND2_MIX_ALPHA_ENV:
        case TYPE_BLEND3_MIX_ALPHA:
        case TYPE_BLEND3_MIX_ALPHA_ENV:
            return 5;
            break;

        case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
        case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
        case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
        case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
            return 6;
            break;

        case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
        case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
        case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
        case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
            return 7;
            break;

        default:
			return 100;
            break;
    }
}

uint32_t vk_get_rtx_material_stage_tex_count( const Vk_Pipeline_Def *def )
{
    switch ( def->shader_type ) {

        case TYPE_MULTI_TEXTURE_MUL2:
        case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
        case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
        case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
        case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
        case TYPE_MULTI_TEXTURE_ADD2_1_1:
        case TYPE_MULTI_TEXTURE_ADD2:
        case TYPE_MULTI_TEXTURE_MUL2_ENV:
        case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
        case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
        case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
        case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
        case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
        case TYPE_MULTI_TEXTURE_ADD2_ENV:

        case TYPE_BLEND2_MUL:
        case TYPE_BLEND2_ADD:
        case TYPE_BLEND2_ALPHA:
        case TYPE_BLEND2_ONE_MINUS_ALPHA:
        case TYPE_BLEND2_MIX_ALPHA:
        case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
        case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:

        case TYPE_BLEND2_MUL_ENV:
        case TYPE_BLEND2_ADD_ENV:
        case TYPE_BLEND2_ALPHA_ENV:
        case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
        case TYPE_BLEND2_MIX_ALPHA_ENV:
        case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
        case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
            return 1; // 2 textures

        case TYPE_MULTI_TEXTURE_MUL3:
        case TYPE_MULTI_TEXTURE_ADD3_1_1:
        case TYPE_MULTI_TEXTURE_ADD3:
        case TYPE_MULTI_TEXTURE_MUL3_ENV:
        case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
        case TYPE_MULTI_TEXTURE_ADD3_ENV:

        case TYPE_BLEND3_MUL:
        case TYPE_BLEND3_ADD:
        case TYPE_BLEND3_ALPHA:
        case TYPE_BLEND3_ONE_MINUS_ALPHA:
        case TYPE_BLEND3_MIX_ALPHA:
        case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
        case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:

        case TYPE_BLEND3_MUL_ENV:
        case TYPE_BLEND3_ADD_ENV:
        case TYPE_BLEND3_ALPHA_ENV:
        case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
        case TYPE_BLEND3_MIX_ALPHA_ENV:
        case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
        case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
            return 2; // 3 textures

        default:
            return 0; // 1 texture
    }
}

// Which of the tracer's four composites a stage's blendFunc maps onto. Only two pairs
// used to be recognised and everything else fell through to OPAQUE, which the sprite
// path then drew at full coverage - a blended effect came out as a solid disc of its own
// texture, reading as a shadow wherever that texture was dark.
static uint32_t vk_get_rtx_material_stage_blend_mode( uint32_t state_bits )
{
	const uint32_t src = state_bits & GLS_SRCBLEND_BITS;
	const uint32_t dst = state_bits & GLS_DSTBLEND_BITS;

	// No blendFunc at all. ParseStage folds GL_ONE/GL_ZERO into this and sets the depth
	// mask, so either test on its own would do; take both.
	if ( ( src == 0 && dst == 0 ) || ( src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ZERO ) )
		return RTX_BLEND_OPAQUE;

	if ( ( state_bits & GLS_DEPTHMASK_TRUE ) != 0 )
		return RTX_BLEND_OPAQUE;	// allow_discard = 0

	// Everything added onto the framebuffer. The source factor only scales how much of
	// the texture goes in, which the sprite path already folds into the colour.
	if ( dst == GLS_DSTBLEND_ONE )
		return RTX_BLEND_ADDITIVE;

	if ( dst == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
		return ( src == GLS_SRCBLEND_ONE ) ? RTX_BLEND_ALPHA_PREMUL : RTX_BLEND_ALPHA;

	// A filter: dst * src, written either way round. Common on scorch marks and the
	// smoke that is meant to darken what is behind it.
	if ( dst == GLS_DSTBLEND_ZERO && ( src == GLS_SRCBLEND_DST_COLOR || src == GLS_SRCBLEND_ONE_MINUS_DST_COLOR ) )
		return RTX_BLEND_MODULATE;

	if ( src == GLS_SRCBLEND_ZERO && ( dst == GLS_DSTBLEND_SRC_COLOR || dst == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) )
		return RTX_BLEND_MODULATE;

	// GL_DST_COLOR/GL_SRC_COLOR (blend2x) and the rest of the long tail. Alpha is the
	// safe landing: it never covers more than the texture's own alpha says it does.
	return RTX_BLEND_ALPHA;
}

static void vk_rtx_build_frame_materials( const shader_t *shader, rtx_material_t *mat );

rtx_material_t *vk_rtx_shader_to_material( shader_t *shader )
{
	shader_t			*state;
	const shaderStage_t *pStage;
	rtx_material_t		*mat;
	uint32_t			i, j;
	Vk_Pipeline_Def			def;

	state = (shader->remappedShader) ? shader->remappedShader : NULL;

	if ( shader->updatedShader )
		state = shader->updatedShader;

	mat = vk_rtx_get_material( (uint32_t)shader->index );

	if ( !mat )
		return NULL;

	if ( mat->active )
		return mat;

	// build material
	mat->index	= (uint32_t)shader->index;	// shared index
	mat->flags	= MATERIAL_KIND_REGULAR;	// ~sunny, this should be shader or remapped no?
	
	mat->remappedIndex	= (state) ? (uint32_t)state->index : 0U;
	mat->active			= qtrue;
	//mat->albedo			= RB_GetNextTexEncoded( shader, 0 );
	mat->albedo			= 0u;
	mat->emissive		= vk_rtx_find_emissive_texture( shader, mat );

	if ( mat->emissive ) {
		mat->emissive_factor = 1.0f;
		mat->flags |= MATERIAL_FLAG_LIGHT;
	}

	if ( mat->index >= (int)MATERIAL_INDEX_MASK || mat->remappedIndex >= (int)MATERIAL_INDEX_MASK  )
		ri.Error( ERR_DROP, "%s() - MATERIAL_INDEX_MASK(4095) hit. Need to finaly seperate material_index from material_flags", __func__ );

	// The frame materials sit at the top of the table (vk_rtx_build_frame_materials).
	if ( mat->index > rtx_next_frame_material )
		ri.Error( ERR_DROP, "%s() - shader %s (%u) reaches the frame materials (from %u)", __func__, shader->name, mat->index, rtx_next_frame_material + 1 );

	uint32_t alphaBlend = 0;

	memset(mat->stage, 0, sizeof(MaterialStage) * MAX_RTX_STAGES);
	for ( i = 0; i < MAX_RTX_STAGES; i++ ) 
	{
		pStage = shader->stages[i];

		if ( !pStage || !pStage->active )
			break;

		// A surface sprite stage draws the sprites, not the surface. It is last, as in the raster.
		if ( pStage->ss && pStage->ss->type )
			break;

		Com_Memset( &def, 0, sizeof(Vk_Pipeline_Def) );
		vk_get_pipeline_def(pStage->vk_pipeline[0], &def);

		// stage/bundle
		mat->num_stages++;
		mat->stage[i].tex_mode = vk_get_rtx_material_stage_tex_mode( &def );
		mat->stage[i].tex_count = vk_get_rtx_material_stage_tex_count( &def );

		// The tracer composes the stages as the raster blends them (see the shader).
		mat->stage[i].blend = STAGE_BLEND_ACTIVE | ( pStage->stateBits & GLS_BLEND_BITS );
		mat->stage[i].portal_range_r = shader->portalRangeR;

		// vk_rtx_animate_materials sets the tcMods and the colors.
		for ( j = 0; j < NUM_TEXTURE_BUNDLES; j++ ) {
			mat->stage[i].bundle[j].tc_matrix[0] = 1.0f;
			mat->stage[i].bundle[j].tc_matrix[3] = 1.0f;
			mat->stage[i].bundle[j].color = 0xffffffffu;
		}

		// The tracer lights the surface itself, so a lightmap bundle is left out of the albedo.
		uint32_t kept = 0;
		for ( j = 0; j <= mat->stage[i].tex_count; j++ ) {
			if ( pStage->bundle[j].isLightmap )
				mat->stage[i].bundle[j].alphaGen = BUNDLE_SKIP;
			else
				kept++;

			// A collapsed stage is a glow stage if one of its bundles glows; the glow pass takes
			// that bundle only.
			if ( pStage->bundle[j].glow )
				mat->stage[i].bundle[j].alphaGen |= BUNDLE_GLOW;

			// The tracer computes the texture coordinates of each bundle from its tcGen, as the
			// raster does (bundle_uv). rd-vulkan moves a tcGen environment of bundle 0 to TESS_ENV.
			texCoordGen_t tcGen = pStage->bundle[j].tcGen;
			if ( j == 0 && ( pStage->tessFlags & TESS_ENV ) )
				tcGen = TCGEN_ENVIRONMENT_MAPPED;

			mat->stage[i].bundle[j].tc_gen_s[3] = (float)tcGen;
			if ( tcGen == TCGEN_VECTOR && pStage->bundle[j].tcGenVectors )
			{
				VectorCopy( pStage->bundle[j].tcGenVectors[0], mat->stage[i].bundle[j].tc_gen_s );
				VectorCopy( pStage->bundle[j].tcGenVectors[1], mat->stage[i].bundle[j].tc_gen_t );
			}

			if ( pStage->bundle[j].image[0] == NULL )
				continue;

			mat->stage[i].bundle[j].image = pStage->bundle[j].isLightmap ? 0 : pStage->bundle[j].image[0]->index;
			mat->stage[i].bundle[j].rgbGen = (uint32_t)pStage->bundle[j].rgbGen;
			mat->stage[i].bundle[j].alphaGen |= (uint32_t)pStage->bundle[j].alphaGen;
		}

		if ( kept == 0 )
			mat->stage[i].blend |= STAGE_BLEND_LIGHTMAP;

		// physical
		if ( pStage->vk_pbr_flags ) 
		{
			if ( pStage->vk_pbr_flags & PBR_HAS_NORMALMAP )
				mat->normals = pStage->normalMap->index;

			if ( pStage->vk_pbr_flags & PBR_HAS_PHYSICALMAP || pStage->vk_pbr_flags & PBR_HAS_SPECULARMAP )
				mat->phyiscal = pStage->physicalMap->index;

			Com_Memcpy( mat->specular_scale, pStage->specularScale, sizeof(vec4_t));
		}
	}

	// blend
	if (shader->stages[0] != NULL && shader->stages[0]->active) 
	{
		uint32_t state_bits = shader->stages[0]->stateBits;

		if (state_bits & GLS_ATEST_BITS)
			mat->flags |= MATERIAL_FLAG_MASKED;

		uint32_t atest_bits = state_bits & GLS_ATEST_BITS;

		switch (atest_bits)
		{
			case GLS_ATEST_GT_0:
				mat->alpha_test_func = 1;
				mat->alpha_test_value = 0.0f;
				break;
			case GLS_ATEST_LT_80:
				mat->alpha_test_func = 2;
				mat->alpha_test_value = 0.5f;
				break;
			case GLS_ATEST_GE_80:
				mat->alpha_test_func = 3;
				mat->alpha_test_value = 0.5f;
				break;
			case GLS_ATEST_GE_C0:
				mat->alpha_test_func = 3;
				mat->alpha_test_value = 0.75f;
				break;
			default:
				mat->alpha_test_func = 0;
				mat->alpha_test_value = 0.0f;
				break;
		}

		mat->blend_mode = vk_get_rtx_material_stage_blend_mode( state_bits ); // discard mode
	}else{
		mat->blend_mode = 0;
		mat->alpha_test_func = 0;
		mat->alpha_test_value = 0.0f;
	}

	vk_rtx_build_frame_materials( shader, mat );

	MAT_SetIndex( mat );

	if ( pt_verbose->integer )
	{
		static const char * const blend_names[] = {
			"opaque", "alpha", "additive", "modulate", "premul", "?", "?", "?"
		};

		const image_t *img0 = ( shader->stages[0] && shader->stages[0]->active ) ? shader->stages[0]->bundle[0].image[0] : NULL;

		ri.Printf( PRINT_ALL, "rtx material %-4u stages %u  s0 mode %u count %u  blend %-8s  tex %u/%u/%u  %-30s  %s\n",
			mat->index, mat->num_stages,
			mat->stage[0].tex_mode, mat->stage[0].tex_count,
			blend_names[mat->blend_mode & RTX_BLEND_MASK],
			mat->stage[0].bundle[0].image, mat->stage[0].bundle[1].image, mat->stage[0].bundle[2].image,
			img0 ? img0->imgName : "<none>",
			shader->name );
	}

	return mat;
}

// What the rasterizer draws without light (glow and additive stages, fullbright stages) is
// emitted in screen units (see get_material): 1 is the brightness of the rasterizer.
static cvar_t *vk_rtx_glow_scale( void )
{
	static cvar_t *pt_glow_scale;

	if ( !pt_glow_scale )
		pt_glow_scale = ri.Cvar_Get( "pt_glow_scale", "1.0", CVAR_ARCHIVE_ND );

	return pt_glow_scale;
}

VkResult vk_rtx_upload_materials( LightBuffer *lbo )
{
	uint32_t i;
	rtx_material_t *mat;

	for ( i = 0; i < MAX_SHADERS; i++ ) 
	{
		mat = vk_rtx_get_material( i );

		if ( !mat || !mat->active || mat->uploaded[vk.current_frame_index] ) 
			continue;

		uint32_t *data = lbo->material_table + i * MATERIAL_UINTS;
		memset(data, 0, sizeof(uint32_t) * MATERIAL_UINTS);
		if ( mat->albedo )		data[0] |= 0u;	// deprecated
		if ( mat->emissive )	data[0] |= mat->emissive << 16;
		// The tracer perturbs the normal with any normal map it gets; r_normalMapping 0 means none.
		if ( mat->normals && r_normalMapping->integer )	data[1] |= mat->normals;
		if ( mat->phyiscal && ( r_specularMapping->integer || r_normalMapping->integer ) )	data[1] |= mat->phyiscal << 16;

		data[2] =	floatToHalf( mat->specular_scale[0] );
		data[2] |=	floatToHalf( mat->specular_scale[1] ) << 16;
		data[3] =	floatToHalf( mat->specular_scale[2] );
		data[3] |=	floatToHalf( mat->specular_scale[3] ) << 16;

		// Bits 0-11: remapped material. Bits 12-23: material of the first or next animMap frame,
		// bits 24-30: frame count, bit 31: oneshotanimMap. See animate_material.
		data[4] =	( mat->remappedIndex & MATERIAL_INDEX_MASK )
					| ( ( mat->anim_first & MATERIAL_INDEX_MASK ) << 12 )
					| ( MIN( mat->anim_frames, 127u ) << 24 )
					| ( mat->anim_oneshot ? 0x80000000u : 0u );

		data[5] |= (mat->alpha_test_func & 0x3);	// bits 0-1
		data[5] |= (mat->blend_mode & RTX_BLEND_MASK) << 2;	// bits 2-4
		data[5] |= floatToHalf(mat->alpha_test_value) << 16;

		// Bits 8-15: pt_glow_scale times the light edit factor of the shader as e, 2^((e - 128) / 16).
		// Bit 5: a glow material, or a shader with a factor. See get_material_info.
		{
			const float shaderScale = ( i < MAX_SHADERS ) ? g_rtxEmissiveScale[i] : 1.0f;
			const float scale = vk_rtx_glow_scale()->value * shaderScale;
			const int e = ( scale > 0.f ) ? (int)floorf( log2f( scale ) * 16.f + 128.5f ) : 1;
			data[5] |= (uint32_t)Com_Clampi( 1, 255, e ) << 8;

			if ( mat->emissive && ( mat->glow_emissive || shaderScale != 1.0f ) )
				data[5] |= 0x20u;
		}

		mat->uploaded[vk.current_frame_index] = qtrue;

		// stages
		MaterialStage *stage = lbo->material_stages + i * MAX_RTX_STAGES;
		Com_Memcpy( stage, mat->stage, sizeof( MaterialStage ) * MAX_RTX_STAGES );	// unused stages clear STAGE_BLEND_ACTIVE
	}

	return VK_SUCCESS;
}
/*
==============================================================================

ANIMATED IMAGES

A material holds one image per bundle. The image of every animMap stage follows the time
here, the same way R_BindAnimatedImage does, and the material is uploaded again when it
changes. A brush model instance can also ask for one frame of its own (a door shows its
lock state this way): each frame then has a material of its own, see
vk_rtx_build_frame_materials and animate_material in the shaders.

==============================================================================
*/

static byte		rtx_anim_reported[MAX_SHADERS];	// pt_verbose: one line per shader

static int vk_rtx_anim_frame( const shader_t *shader, const textureBundle_t *bundle, double time )
{
	int index;

	if ( bundle->numImageAnimations <= 1 )
		return 0;

	// Same calculation as R_BindAnimatedImage, so the frames line up with waveforms.
	index = Q_ftol( ( time - shader->timeOffset ) * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
	index >>= FUNCTABLE_SIZE2;
	if ( index < 0 )
		index = 0;

	if ( bundle->oneShotAnimMap )
		return MIN( index, bundle->numImageAnimations - 1 );

	return index % bundle->numImageAnimations;
}

// Frame k of a bundle, the way R_BindAnimatedImage treats an index set by the entity.
static int vk_rtx_bundle_frame( const textureBundle_t *bundle, int k )
{
	if ( bundle->oneShotAnimMap )
		return MIN( k, bundle->numImageAnimations - 1 );

	return k % bundle->numImageAnimations;
}

// Same stage and bundle choice as vk_rtx_find_emissive_texture.
static const textureBundle_t *vk_rtx_glow_bundle( const shader_t *shader )
{
	uint32_t i, j;

	for ( i = 0; i < MAX_RTX_STAGES; i++ )
	{
		const shaderStage_t *pStage = shader->stages[i];

		if ( !pStage || !pStage->active || !pStage->glow )
			continue;

		for ( j = 0; j < (uint32_t)pStage->numTexBundles; j++ )
		{
			if ( pStage->bundle[j].glow && pStage->bundle[j].image[0] )
				return &pStage->bundle[j];
		}
	}

	return NULL;
}

// The color of rgbGen and alphaGen that do not come from the entity or the vertex, as rgba8.
// The tracer lights the surface itself, so the other gens count as white.
uint32_t vk_rtx_bundle_color( const textureBundle_t *bundle )
{
	byte color[4] = { 255, 255, 255, 255 };
	const int numVertexes = tess.numVertexes;

	tess.numVertexes = 1;

	switch ( bundle->rgbGen )
	{
	case CGEN_CONST:
		color[0] = bundle->constantColor[0];
		color[1] = bundle->constantColor[1];
		color[2] = bundle->constantColor[2];
		break;
	case CGEN_WAVEFORM:
		RB_CalcWaveColor( &bundle->rgbWave, color );
		break;
	case CGEN_IDENTITY_LIGHTING:
		color[0] = color[1] = color[2] = (byte)Q_ftol( 255.0f * tr.identityLight );
		break;
	default:
		break;
	}

	switch ( bundle->alphaGen )
	{
	case AGEN_CONST:
		color[3] = bundle->constantColor[3];
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &bundle->alphaWave, color );
		break;
	default:
		color[3] = 255;
		break;
	}

	tess.numVertexes = numVertexes;

	return color[0] | ( color[1] << 8 ) | ( color[2] << 16 ) | ( (uint32_t)color[3] << 24 );
}

// The tcMods and the color of each bundle at the time of this frame. The frame materials get
// the same values. Turbulence is per vertex and is not applied.
static qboolean vk_rtx_bundle_gens( const shader_t *shader, rtx_material_t *mat, const trRefdef_t *refdef )
{
	float matrix[4], offTurb[4];
	qboolean changed = qfalse;
	uint32_t s, j, k, color;

	const double shaderTime = tess.shaderTime;
	trRefEntity_t *entity = backEnd.currentEntity;
	tess.shaderTime = refdef->floatTime - shader->timeOffset;
	backEnd.currentEntity = &tr.worldEntity;

	for ( s = 0; s < mat->num_stages && s < MAX_RTX_STAGES; s++ )
	{
		const shaderStage_t *pStage = shader->stages[s];

		if ( !pStage || !pStage->active )
			break;

		for ( j = 0; j < NUM_TEXTURE_BUNDLES; j++ )
		{
			const textureBundle_t *bundle = &pStage->bundle[j];
			MaterialBundle *out = &mat->stage[s].bundle[j];

			vk_compute_tex_mods( bundle, matrix, offTurb );
			offTurb[3] -= floorf( offTurb[3] );	// the phase is periodic
			color = vk_rtx_bundle_color( bundle );

			if ( !memcmp( out->tc_matrix, matrix, sizeof( matrix ) ) && !memcmp( out->tc_offset, offTurb, sizeof( offTurb ) ) && out->color == color )
				continue;

			Com_Memcpy( out->tc_matrix, matrix, sizeof( matrix ) );
			Com_Memcpy( out->tc_offset, offTurb, sizeof( offTurb ) );	// zw: turbulence amplitude and phase
			out->color = color;
			changed = qtrue;

			for ( k = mat->anim_first; k != 0 && k <= MATERIAL_INDEX_MASK; k = rtx_materials[k].anim_first )
			{
				MaterialBundle *frame = &rtx_materials[k].stage[s].bundle[j];

				Com_Memcpy( frame->tc_matrix, out->tc_matrix, sizeof( out->tc_matrix ) );
				Com_Memcpy( frame->tc_offset, out->tc_offset, sizeof( out->tc_offset ) );
				frame->color = out->color;
				Com_Memset( rtx_materials[k].uploaded, 0, sizeof( rtx_materials[k].uploaded ) );
			}
		}
	}

	tess.shaderTime = shaderTime;
	backEnd.currentEntity = entity;

	return changed;
}

void vk_rtx_animate_materials( const trRefdef_t *refdef )
{
	int i;
	uint32_t s, j, f;

	// A new pt_glow_scale or light edit factor is in every material.
	static int emissiveGeneration = -1;
	const qboolean rescale = (qboolean)( vk_rtx_glow_scale()->modified || emissiveGeneration != RTX_LightEmissive_Generation() );
	vk_rtx_glow_scale()->modified = qfalse;
	emissiveGeneration = RTX_LightEmissive_Generation();

	for ( i = 0; i < tr.numShaders && i < MAX_SHADERS; i++ )
	{
		const shader_t *shader = tr.shaders[i];
		rtx_material_t *mat = vk_rtx_get_material( (uint32_t)i );
		qboolean changed = qfalse;

		if ( !shader || !mat || !mat->active )
			continue;

		for ( s = 0; s < mat->num_stages && s < MAX_RTX_STAGES; s++ )
		{
			const shaderStage_t *pStage = shader->stages[s];

			if ( !pStage || !pStage->active )
				break;

			for ( j = 0; j <= mat->stage[s].tex_count && j < NUM_TEXTURE_BUNDLES; j++ )
			{
				const textureBundle_t *bundle = &pStage->bundle[j];

				if ( bundle->numImageAnimations <= 1 || !bundle->image[0] )
					continue;

				f = (uint32_t)vk_rtx_anim_frame( shader, bundle, refdef->floatTime );
				if ( bundle->image[f] && mat->stage[s].bundle[j].image != (uint32_t)bundle->image[f]->index )
				{
					mat->stage[s].bundle[j].image = bundle->image[f]->index;
					if ( pt_verbose->integer && !( rtx_anim_reported[i] & 2 ) )
					{
						rtx_anim_reported[i] |= 2;
						ri.Printf( PRINT_ALL, "rtx anim: %s stage %u bundle %u -> frame %u (%s)\n", shader->name, s, j, f, bundle->image[f]->imgName );
					}
					changed = qtrue;
				}
			}
		}

		const textureBundle_t *glow = vk_rtx_glow_bundle( shader );
		if ( glow && glow->numImageAnimations > 1 )
		{
			f = (uint32_t)vk_rtx_anim_frame( shader, glow, refdef->floatTime );
			if ( glow->image[f] && mat->emissive != (uint32_t)glow->image[f]->index )
			{
				mat->emissive = glow->image[f]->index;
				changed = qtrue;
			}
		}

		if ( vk_rtx_bundle_gens( shader, mat, refdef ) )
			changed = qtrue;

		if ( rescale )
			changed = qtrue;

		if ( changed )
			Com_Memset( mat->uploaded, 0, sizeof( mat->uploaded ) );
	}
}

// A brush model instance can ask for one frame of its animMaps, which the shared material
// cannot show. Each frame gets its own material, chained from the shader's material; the
// instance frame picks one in animate_material.
static void vk_rtx_build_frame_materials( const shader_t *shader, rtx_material_t *mat )
{
	uint32_t frames = 0, s, j, k;
	qboolean oneshot = qfalse;

	for ( s = 0; s < mat->num_stages && s < MAX_RTX_STAGES; s++ )
	{
		const shaderStage_t *pStage = shader->stages[s];

		for ( j = 0; pStage && j <= mat->stage[s].tex_count && j < NUM_TEXTURE_BUNDLES; j++ )
		{
			if ( pStage->bundle[j].numImageAnimations > (int)frames )
			{
				frames = (uint32_t)pStage->bundle[j].numImageAnimations;
				oneshot = (qboolean)pStage->bundle[j].oneShotAnimMap;
			}
		}
	}

	if ( frames <= 1 )
		return;

	frames = MIN( frames, 127u );

	// The frame materials must stay above every shader index.
	if ( rtx_next_frame_material < (uint32_t)tr.numShaders + frames + 64 )
	{
		if ( pt_verbose->integer )
			ri.Printf( PRINT_WARNING, "rtx: no room for the %u frame materials of %s\n", frames, shader->name );
		return;
	}

	const uint32_t first = rtx_next_frame_material - frames + 1;
	rtx_next_frame_material -= frames;

	const textureBundle_t *glow = vk_rtx_glow_bundle( shader );

	for ( k = 0; k < frames; k++ )
	{
		rtx_material_t *frame = &rtx_materials[first + k];

		*frame = *mat;
		frame->index = first + k;
		MAT_SetIndex( frame );
		Com_Memset( frame->uploaded, 0, sizeof( frame->uploaded ) );
		frame->anim_first = ( k + 1 < frames ) ? first + k + 1 : 0;
		frame->anim_frames = 0;
		frame->anim_oneshot = qfalse;

		for ( s = 0; s < mat->num_stages && s < MAX_RTX_STAGES; s++ )
		{
			const shaderStage_t *pStage = shader->stages[s];

			for ( j = 0; pStage && j <= mat->stage[s].tex_count && j < NUM_TEXTURE_BUNDLES; j++ )
			{
				const textureBundle_t *bundle = &pStage->bundle[j];

				if ( bundle->numImageAnimations > 1 && bundle->image[0] )
					frame->stage[s].bundle[j].image = bundle->image[vk_rtx_bundle_frame( bundle, (int)k )]->index;
			}
		}

		if ( glow && glow->numImageAnimations > 1 )
			frame->emissive = glow->image[vk_rtx_bundle_frame( glow, (int)k )]->index;
	}

	mat->anim_first = first;
	mat->anim_frames = frames;
	mat->anim_oneshot = oneshot;
}
