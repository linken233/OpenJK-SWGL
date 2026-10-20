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

#ifdef VK_COMPUTE_NORMALMAP

/*
Normal maps computed from the diffuse texture (r_genNormalMaps), from JKSunny/EternalJK.

1. FinishShader gives vk_add_compute_normalmap() the diffuse texture of a lit stage without a
   normal map. It creates the storage image of the normal map and a descriptor set with the
   two images, and puts them in a batch.
2. vk_begin_frame() runs the batch: vk_dispatch_compute_normalmaps() dispatches the compute
   shader (normalmap.comp) for each normal map, blits its mip chain, then frees its
   descriptor set and its storage view.
3. A map change drops the batch (vk_clear_compute_normalmaps): its images are deleted and its
   descriptor sets go with the pool reset.

The pipeline exists only when r_genNormalMaps is set at start (CVAR_LATCH).
*/

void vk_create_compute_normalmap_pipelines( void )
{
	if ( !r_genNormalMaps->integer )
		return;

	// descriptor layout: the diffuse texture, the normal map
	VkDescriptorSetLayoutBinding bind[2];

	bind[0].binding = 0;
	bind[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bind[0].descriptorCount = 1;
	bind[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bind[0].pImmutableSamplers = NULL;

	bind[1].binding = 1;
	bind[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bind[1].descriptorCount = 1;
	bind[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bind[1].pImmutableSamplers = NULL;

	VkDescriptorSetLayoutCreateInfo desc;
	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.bindingCount = 2;
	desc.pBindings = bind;

	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.set_layout_compute_normalmap ) );

	// pipeline layout
	VkPipelineLayoutCreateInfo pipeline_layout;
	Com_Memset( &pipeline_layout, 0, sizeof( pipeline_layout ) );
	pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout.setLayoutCount = 1;
	pipeline_layout.pSetLayouts = &vk.set_layout_compute_normalmap;

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeline_layout, NULL, &vk.pipeline_layout_compute_normalmap ) );

	// pipeline
	VkPipelineShaderStageCreateInfo stage;
	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.shaders.normalmap;
	stage.pName = "main";

	VkComputePipelineCreateInfo info;
	Com_Memset( &info, 0, sizeof( info ) );
	info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	info.stage = stage;
	info.layout = vk.pipeline_layout_compute_normalmap;

	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &info, NULL, &vk.compute_normalmap_pipeline ) );
}

static void vk_destroy_normalmap_storage_views( void )
{
	uint32_t i;

	for ( i = 0; i < tr.compute_normalmaps_batch_num; i++ ) {
		if ( tr.compute_normalmaps[i].storage_view != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, tr.compute_normalmaps[i].storage_view, NULL );
			tr.compute_normalmaps[i].storage_view = VK_NULL_HANDLE;
		}
	}
}

void vk_destroy_compute_normalmap_pipelines( void )
{
	if ( vk.compute_normalmap_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.compute_normalmap_pipeline, NULL );
		vk.compute_normalmap_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.pipeline_layout_compute_normalmap != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_compute_normalmap, NULL );
		vk.pipeline_layout_compute_normalmap = VK_NULL_HANDLE;
	}

	if ( vk.set_layout_compute_normalmap != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.set_layout_compute_normalmap, NULL );
		vk.set_layout_compute_normalmap = VK_NULL_HANDLE;
	}

	vk_destroy_normalmap_storage_views();
	tr.compute_normalmaps_batch_num = 0;
}

// The images of the batch are deleted with the textures, its descriptor sets with the pool.
void vk_clear_compute_normalmaps( void )
{
	vk_destroy_normalmap_storage_views();
	tr.compute_normalmaps_batch_num = 0;
}

// The normal map of the stage, computed from its diffuse texture at the next frame.
void vk_add_compute_normalmap( shaderStage_t *stage, image_t *albedo, imgFlags_t flags )
{
	if ( !r_genNormalMaps->integer || vk.compute_normalmap_pipeline == VK_NULL_HANDLE )
		return;

	if ( stage->normalMap || !albedo )
		return;

	// the world textures only
	if ( !( albedo->flags & IMGFLAG_PICMIP ) || !( albedo->flags & IMGFLAG_MIPMAP ) )
		return;

	if ( albedo->width < 64 && albedo->height < 64 )
		return;

	if ( tr.compute_normalmaps_batch_num == MAX_BATCH_COMPUTE_NORMALMAPS ) {
		ri.Printf( PRINT_WARNING, "%s: MAX_BATCH_COMPUTE_NORMALMAPS hit, %s keeps no normal map\n", __func__, albedo->imgName );
		return;
	}

	char imageName[MAX_QPATH];
	COM_StripExtension( albedo->imgName, imageName, MAX_QPATH );
	Q_strcat( imageName, MAX_QPATH, "_gen_n" );

	// The diffuse texture of another stage may have its normal map already.
	image_t *normal = R_GetLoadedImage( imageName, flags | IMGFLAG_STORAGE );
	if ( normal == NULL )
	{
		Vk_Sampler_Def sampler_def;
		VkDescriptorImageInfo image_info, normal_info;
		VkWriteDescriptorSet writes[2];

		normal = R_CreateImage( imageName, NULL, albedo->width, albedo->height, flags | IMGFLAG_STORAGE );

		Com_Memset( &sampler_def, 0, sizeof( sampler_def ) );
		sampler_def.address_mode = albedo->wrapClampMode;
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
		sampler_def.noAnisotropy = qtrue;	// no anisotropy without mipmaps

		image_info.sampler = vk_find_sampler( &sampler_def );
		image_info.imageView = albedo->view;
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// A storage descriptor takes a view of one level.
		VkImageViewCreateInfo view_desc;
		VkImageView storage_view;
		Com_Memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = normal->handle;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = (VkFormat)normal->internalFormat;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &storage_view ) );

		normal_info.sampler = VK_NULL_HANDLE;
		normal_info.imageView = storage_view;
		normal_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkDescriptorSetAllocateInfo alloc;
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_compute_normalmap;

		VkDescriptorSet descriptor_set;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &descriptor_set ) );

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = descriptor_set;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &image_info;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptor_set;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[1].pImageInfo = &normal_info;

		qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

		tr.compute_normalmaps[tr.compute_normalmaps_batch_num].normal = normal;
		tr.compute_normalmaps[tr.compute_normalmaps_batch_num].storage_view = storage_view;
		tr.compute_normalmaps[tr.compute_normalmaps_batch_num].descriptor_set = descriptor_set;
		tr.compute_normalmaps_batch_num++;
	}

	stage->normalMap = normal;
	stage->normalMapType = PHYS_NORMAL;
	stage->vk_pbr_flags |= PBR_HAS_NORMALMAP;
	VectorSet4( stage->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value );
}

static void vk_normalmap_level_barrier( VkCommandBuffer command_buffer, VkImage image, uint32_t level,
	VkImageLayout old_layout, VkImageLayout new_layout,
	VkPipelineStageFlags src_stage, VkAccessFlags src_access, VkAccessFlags dst_access )
{
	VkImageMemoryBarrier barrier;

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = level;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier( command_buffer, src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier );
}

// Level 0 goes down the mip chain. Without the mips a distant surface samples
// level 0 and its normals alias into noise.
static void vk_blit_normalmap_mips( VkCommandBuffer command_buffer, const image_t *normal )
{
	int32_t w = normal->uploadWidth;
	int32_t h = normal->uploadHeight;
	uint32_t level;

	vk_normalmap_level_barrier( command_buffer, normal->handle, 0, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );

	for ( level = 1; w > 1 || h > 1; level++ )
	{
		VkImageBlit region;
		const int32_t mw = ( w > 1 ) ? ( w >> 1 ) : 1;
		const int32_t mh = ( h > 1 ) ? ( h >> 1 ) : 1;

		vk_normalmap_level_barrier( command_buffer, normal->handle, level, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT );

		Com_Memset( &region, 0, sizeof( region ) );
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = level - 1;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[1].x = w;
		region.srcOffsets[1].y = h;
		region.srcOffsets[1].z = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.dstSubresource.mipLevel = level;
		region.dstSubresource.layerCount = 1;
		region.dstOffsets[1].x = mw;
		region.dstOffsets[1].y = mh;
		region.dstOffsets[1].z = 1;

		qvkCmdBlitImage( command_buffer, normal->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			normal->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR );

		vk_normalmap_level_barrier( command_buffer, normal->handle, level, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );

		w = mw;
		h = mh;
	}

	vk_record_image_layout_transition( command_buffer, normal->handle, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
}

// The normal maps of the batch: the compute shader writes their level 0, a blit chain the mips.
void vk_dispatch_compute_normalmaps( void )
{
	uint32_t i;

	if ( tr.compute_normalmaps_batch_num == 0 || vk.compute_normalmap_pipeline == VK_NULL_HANDLE )
		return;

	VkCommandBuffer command_buffer = vk_begin_command_buffer();

	qvkCmdBindPipeline( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.compute_normalmap_pipeline );

	for ( i = 0; i < tr.compute_normalmaps_batch_num; i++ )
	{
		image_t *normal = tr.compute_normalmaps[i].normal;

		vk_record_image_layout_transition( command_buffer, normal->handle, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );

		qvkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.pipeline_layout_compute_normalmap, 0, 1, &tr.compute_normalmaps[i].descriptor_set, 0, NULL );

		qvkCmdDispatch( command_buffer, ( normal->uploadWidth + 7 ) / 8, ( normal->uploadHeight + 7 ) / 8, 1 );

		if ( normal->flags & IMGFLAG_MIPMAP )
			vk_blit_normalmap_mips( command_buffer, normal );
		else
			vk_record_image_layout_transition( command_buffer, normal->handle, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	}

	// waits for the queue
	vk_end_command_buffer( command_buffer, __func__ );

	for ( i = 0; i < tr.compute_normalmaps_batch_num; i++ )
		qvkFreeDescriptorSets( vk.device, vk.descriptor_pool, 1, &tr.compute_normalmaps[i].descriptor_set );

	vk_destroy_normalmap_storage_views();
	tr.compute_normalmaps_batch_num = 0;
}

#endif // VK_COMPUTE_NORMALMAP
