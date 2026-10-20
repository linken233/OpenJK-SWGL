/*
===========================================================================
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

#ifdef USE_VK_PBR

/*
Resources of the PBR shading of the raster renderer (pbr.glsl), ported from JKSunny/EternalJK.

- tr.brdfLutImage: the split-sum environment BRDF (Karis 2013), x = N.V, y = 1 - roughness.
  The CPU computes it once; the image is created again with the other images of each map.
- vk.pbr.empty_cube: a black cubemap for the stages without an environment cubemap. Its
  descriptor set is allocated again after each descriptor pool reset (vk_init_descriptors).
*/

#define BRDF_LUT_SIZE		64
#define BRDF_LUT_SAMPLES	512

static byte brdfLut[BRDF_LUT_SIZE][BRDF_LUT_SIZE][4];
static qboolean brdfLutValid = qfalse;

static float RadicalInverse( uint32_t bits )
{
	bits = ( bits << 16u ) | ( bits >> 16u );
	bits = ( ( bits & 0x55555555u ) << 1u ) | ( ( bits & 0xAAAAAAAAu ) >> 1u );
	bits = ( ( bits & 0x33333333u ) << 2u ) | ( ( bits & 0xCCCCCCCCu ) >> 2u );
	bits = ( ( bits & 0x0F0F0F0Fu ) << 4u ) | ( ( bits & 0xF0F0F0F0u ) >> 4u );
	bits = ( ( bits & 0x00FF00FFu ) << 8u ) | ( ( bits & 0xFF00FF00u ) >> 8u );
	return (float)bits * 2.3283064365386963e-10f;
}

// The scale and the bias of F0 in the environment BRDF, for the normal along z.
static void IntegrateBRDF( float NoV, float roughness, float *scale, float *bias )
{
	const float alpha = roughness * roughness;
	const float k = alpha / 2.0f;
	vec3_t V, H, L;
	uint32_t i;

	V[0] = sqrtf( 1.0f - NoV * NoV );
	V[1] = 0.0f;
	V[2] = NoV;

	*scale = *bias = 0.0f;

	for ( i = 0; i < BRDF_LUT_SAMPLES; i++ )
	{
		// GGX importance sample of the half vector
		const float Xi0 = (float)i / BRDF_LUT_SAMPLES;
		const float Xi1 = RadicalInverse( i );
		const float phi = 2.0f * M_PI * Xi0;
		const float cosTheta = sqrtf( ( 1.0f - Xi1 ) / ( 1.0f + ( alpha * alpha - 1.0f ) * Xi1 ) );
		const float sinTheta = sqrtf( 1.0f - cosTheta * cosTheta );

		H[0] = sinTheta * cosf( phi );
		H[1] = sinTheta * sinf( phi );
		H[2] = cosTheta;

		const float VoH = DotProduct( V, H );
		VectorScale( H, 2.0f * VoH, L );
		VectorSubtract( L, V, L );

		const float NoL = L[2];
		const float NoH = H[2];

		if ( NoL > 0.0f && VoH > 0.0f )
		{
			const float G = ( NoL / ( NoL * ( 1.0f - k ) + k ) ) * ( NoV / ( NoV * ( 1.0f - k ) + k ) );
			const float G_Vis = G * VoH / ( NoH * NoV );
			const float Fc = powf( 1.0f - VoH, 5.0f );

			*scale += ( 1.0f - Fc ) * G_Vis;
			*bias += Fc * G_Vis;
		}
	}

	*scale /= BRDF_LUT_SAMPLES;
	*bias /= BRDF_LUT_SAMPLES;
}

void vk_create_brdf_lut( void )
{
	if ( !vk.pbrActive )
		return;

	if ( !brdfLutValid )
	{
		int x, y;

		for ( y = 0; y < BRDF_LUT_SIZE; y++ )
		{
			const float roughness = 1.0f - ( y + 0.5f ) / BRDF_LUT_SIZE;

			for ( x = 0; x < BRDF_LUT_SIZE; x++ )
			{
				const float NoV = ( x + 0.5f ) / BRDF_LUT_SIZE;
				float scale, bias;

				IntegrateBRDF( NoV, roughness, &scale, &bias );

				brdfLut[y][x][0] = (byte)Com_Clamp( 0.0f, 255.0f, scale * 255.0f + 0.5f );
				brdfLut[y][x][1] = (byte)Com_Clamp( 0.0f, 255.0f, bias * 255.0f + 0.5f );
				brdfLut[y][x][2] = 0;
				brdfLut[y][x][3] = 255;
			}
		}

		brdfLutValid = qtrue;
	}

	tr.brdfLutImage = R_CreateImage( "*brdflut", (byte *)brdfLut, BRDF_LUT_SIZE, BRDF_LUT_SIZE,
		IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_NOSCALE );
}

void vk_create_pbr_resources( void )
{
	VkImageCreateInfo desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkImageViewCreateInfo view;
	VkClearColorValue black;
	VkImageSubresourceRange range;

	if ( !vk.pbrActive )
		return;

	// black 1x1 cubemap
	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	desc.extent.width = 1;
	desc.extent.height = 1;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 6;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &vk.pbr.empty_cube ) );

	qvkGetImageMemoryRequirements( vk.device, vk.pbr.empty_cube, &memory_requirements );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.pbr.empty_cube_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.pbr.empty_cube, vk.pbr.empty_cube_memory, 0 ) );

	Com_Memset( &view, 0, sizeof( view ) );
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.image = vk.pbr.empty_cube;
	view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	view.format = desc.format;
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.levelCount = 1;
	view.subresourceRange.layerCount = 6;
	VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &vk.pbr.empty_cube_view ) );

	VK_SET_OBJECT_NAME( vk.pbr.empty_cube, "PBR empty cubemap", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

	Com_Memset( &black, 0, sizeof( black ) );
	range = view.subresourceRange;

	VkCommandBuffer command_buffer = vk_begin_command_buffer();
	vk_record_image_layout_transition( command_buffer, vk.pbr.empty_cube, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	qvkCmdClearColorImage( command_buffer, vk.pbr.empty_cube, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range );
	vk_record_image_layout_transition( command_buffer, vk.pbr.empty_cube, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	vk_end_command_buffer( command_buffer, __func__ );
}

void vk_destroy_pbr_resources( void )
{
	if ( vk.pbr.empty_cube_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.pbr.empty_cube_view, NULL );
		vk.pbr.empty_cube_view = VK_NULL_HANDLE;
	}

	if ( vk.pbr.empty_cube != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.pbr.empty_cube, NULL );
		vk.pbr.empty_cube = VK_NULL_HANDLE;
	}

	if ( vk.pbr.empty_cube_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.pbr.empty_cube_memory, NULL );
		vk.pbr.empty_cube_memory = VK_NULL_HANDLE;
	}

	vk.pbr.empty_cube_descriptor = VK_NULL_HANDLE;
}

// After each descriptor pool reset.
void vk_init_pbr_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet desc;
	Vk_Sampler_Def sd;

	if ( !vk.pbrActive || vk.pbr.empty_cube_view == VK_NULL_HANDLE )
		return;

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_sampler;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.pbr.empty_cube_descriptor ) );

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;

	info.sampler = vk_find_sampler( &sd );
	info.imageView = vk.pbr.empty_cube_view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.pbr.empty_cube_descriptor;
	desc.dstBinding = 0;
	desc.descriptorCount = 1;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	desc.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

#ifdef VK_CUBEMAP
	vk_init_cubemap_descriptors();
#endif
}

/*
The light type of a lit stage (LIGHTDEF_*), or 0. FinishShader gives the world types to the
pipeline definition; the draw gives LIGHTDEF_USE_LIGHT_VECTOR to the model VBO stages.
*/
uint32_t vk_stage_light_flags( const shaderStage_t *pStage, Vk_Shader_Type type )
{
	const colorGen_t rgbGen = pStage->bundle[0].rgbGen;

	if ( pStage->bundle[0].isLightmap || pStage->bundle[0].image[0] == NULL )
		return 0;

	// texture * lightmap
	if ( pStage->numTexBundles > 1 && pStage->bundle[1].isLightmap && pStage->mtEnv == GL_MODULATE && !pStage->mtEnv3 )
	{
		switch ( type ) {
			case TYPE_MULTI_TEXTURE_MUL2:
			case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
				return LIGHTDEF_USE_LIGHTMAP;
			default:
				return 0;
		}
	}

	if ( pStage->mtEnv || type != TYPE_SINGLE_TEXTURE )
		return 0;

	if ( rgbGen == CGEN_VERTEX || rgbGen == CGEN_EXACT_VERTEX )
		return LIGHTDEF_USE_LIGHT_VERTEX;

	if ( rgbGen == CGEN_LIGHTING_DIFFUSE || rgbGen == CGEN_LIGHTING_DIFFUSE_ENTITY )
		return LIGHTDEF_USE_LIGHT_VECTOR;

	return 0;
}

#endif // USE_VK_PBR
