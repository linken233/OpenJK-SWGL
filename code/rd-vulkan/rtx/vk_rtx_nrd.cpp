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

// This software contains source code provided by NVIDIA Corporation.

// NVIDIA NRD denoiser, RELAX diffuse and specular, as the denoiser DENOISER_NRD_RELAX (pt_denoiser 2).
// The Vulkan side follows nrd/Integration/NRDIntegration.hpp, without NRI:
//  - one compute pipeline for each pipeline of the NRD instance, with a tight descriptor set layout
//  - the pool textures of NRD, the constant buffer ring, and a descriptor pool for each frame in flight
//  - two shaders of the renderer around NRD: nrd_prepare.comp makes the inputs, nrd_composite.comp composites the outputs
// All images are in the layout GENERAL.

#include "tr_local.h"
#include "NRD.h"

// Runs of NRD that one frame index can hold. The pools and the constant ring have room for all of them.
#define NRD_RUNS_PER_FRAME	4

// The Vulkan format of each nrd::Format, in the order of the enum.
static const VkFormat nrd_vk_formats[(size_t)nrd::Format::MAX_NUM] = {
	VK_FORMAT_R8_UNORM, VK_FORMAT_R8_SNORM, VK_FORMAT_R8_UINT, VK_FORMAT_R8_SINT,
	VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8_SINT,
	VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_SINT, VK_FORMAT_R8G8B8A8_SRGB,
	VK_FORMAT_R16_UNORM, VK_FORMAT_R16_SNORM, VK_FORMAT_R16_UINT, VK_FORMAT_R16_SINT, VK_FORMAT_R16_SFLOAT,
	VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16_SFLOAT,
	VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_SNORM, VK_FORMAT_R16G16B16A16_UINT, VK_FORMAT_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SFLOAT,
	VK_FORMAT_R32_UINT, VK_FORMAT_R32_SINT, VK_FORMAT_R32_SFLOAT,
	VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32_SFLOAT,
	VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32_SFLOAT,
	VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_SFLOAT,
	VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2B10G10R10_UINT_PACK32,
	VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32
};

typedef struct {
	VkShaderModule			module;
	VkDescriptorSetLayout	set_layout;		// the resources: space 0
	VkPipelineLayout		layout;
	VkPipeline				pipeline;
	uint32_t				num_textures;
	uint32_t				num_storages;
} nrd_pipeline_t;

typedef struct {
	nrd::Instance			*instance;
	qboolean				ready;				// every Vulkan object exists

	uint32_t				num_pipelines;
	nrd_pipeline_t			*pipelines;

	uint32_t				num_samplers;
	VkSampler				samplers[(size_t)nrd::Sampler::MAX_NUM];
	VkDescriptorSetLayout	constants_layout;	// the samplers and the constant buffer: space 1
	VkDescriptorPool		constants_pool;
	VkDescriptorSet			constants_set;

	VkDescriptorPool		pools[NUM_COMMAND_BUFFERS];

	vkbuffer_t				constant_buffer;
	byte					*constants;			// the mapped memory of the constant buffer
	VkDeviceSize			constants_stride;
	uint32_t				slices_per_run;

	uint32_t				num_pool_images;	// the permanent pool, then the transient pool
	vkimage_t				*pool_images;
	qboolean				pool_images_ready;	// the layout of the pool images is GENERAL

	vkpipeline_t			prepare_pipeline;
	vkpipeline_t			composite_pipeline;

	uint32_t				resource_width, resource_height;

	// the state of the frames
	qboolean				history_valid[2];	// the previous frame ran the denoiser: 0 ReLAX, 1 ReBLUR
	uint32_t				frame_index;
	uint32_t				last_frame_counter;
	uint32_t				runs_this_frame;
	float					jitter_prev[2];
	uint16_t				rect_prev[2];
} nrd_state_t;

static nrd_state_t g_nrd = { };

// The values of the pt_nrd_* cvars.
typedef struct {
	int		max_accum;
	int		max_fast_accum;
	float	prepass_blur;
	int		antifirefly;		// -1: the default of the denoiser (ReBLUR on, ReLAX off)
	int		hitdist_recon;
	int		direct;
} nrd_tuning_t;

// The push constants of nrd_prepare.comp and nrd_composite.comp.
typedef struct {
	float		hit_dist_params[4];		// ReBLUR: A, B, C of the normalized hit distance
	uint32_t	mode;					// 0 ReLAX, 1 ReBLUR
	uint32_t	direct;					// 1: the direct diffuse goes through NRD
	uint32_t	validation;				// 1: show OUT_VALIDATION
	uint32_t	pad;
} nrd_push_t;

// Units of the world in one meter. The hit distance parameters of NRD are in meters.
#define NRD_UNITS_PER_METER	40.0f

static void nrd_read_tuning( nrd_tuning_t *t )
{
	static nrd_tuning_t last;
	static qboolean printed;

	t->max_accum = (int)Com_Clamp( 0.f, 63.f, pt_nrd_max_accum->value );
	t->max_fast_accum = (int)Com_Clamp( 0.f, 63.f, pt_nrd_max_fast_accum->value );
	t->prepass_blur = Com_Clamp( 0.f, 100.f, pt_nrd_prepass_blur->value );
	t->antifirefly = pt_nrd_antifirefly->integer < 0 ? -1 : ( pt_nrd_antifirefly->integer ? 1 : 0 );
	t->hitdist_recon = (int)Com_Clamp( 0.f, 2.f, (float)pt_nrd_hitdist_recon->integer );
	t->direct = pt_nrd_direct->integer ? 1 : 0;

	if ( pt_verbose && pt_verbose->integer && ( !printed || memcmp( &last, t, sizeof(*t) ) ) )
	{
		ri.Printf( PRINT_ALL, "NRD settings: max accum %d, fast %d, prepass blur %.1f, anti firefly %d, hit distance reconstruction %d, direct through NRD %d\n",
			t->max_accum, t->max_fast_accum, t->prepass_blur, t->antifirefly, t->hitdist_recon, t->direct );
		last = *t;
		printed = qtrue;
	}
}

static uint32_t nrd_divide_up( uint32_t x, uint32_t y )
{
	return ( x + y - 1 ) / y;
}

static VkDeviceSize nrd_align( VkDeviceSize size, VkDeviceSize alignment )
{
	return ( size + alignment - 1 ) / alignment * alignment;
}

static qboolean nrd_format_supported( VkFormat format )
{
	VkFormatProperties props;
	const VkFormatFeatureFlags need = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

	qvkGetPhysicalDeviceFormatProperties( vk.physical_device, format, &props );

	return (qboolean)( ( props.optimalTilingFeatures & need ) == need );
}

static qboolean nrd_create_pool_image( vkimage_t *image, const char *name, uint32_t width, uint32_t height, VkFormat format )
{
	VkImageCreateInfo desc;
	VkImageViewCreateInfo view;

	Com_Memset( image, 0, sizeof(vkimage_t) );
	image->extent.width = width;
	image->extent.height = height;
	image->extent.depth = 1;
	image->mipLevels = 1;
	image->arrayLayers = 1;

	Com_Memset( &desc, 0, sizeof(desc) );
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = format;
	desc.extent = image->extent;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	desc.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );
	VK_CreateImageMemory( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image->handle, &image->memory );

	Com_Memset( &view, 0, sizeof(view) );
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.image = image->handle;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = format;
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.levelCount = 1;
	view.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &image->view ) );

	VK_SET_OBJECT_NAME( image->handle, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	VK_SET_OBJECT_NAME( image->view, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	return qtrue;
}

static void nrd_destroy_vulkan_objects( void )
{
	uint32_t i;

	if ( g_nrd.pipelines )
	{
		for ( i = 0; i < g_nrd.num_pipelines; i++ )
		{
			nrd_pipeline_t *p = &g_nrd.pipelines[i];

			if ( p->pipeline )
				qvkDestroyPipeline( vk.device, p->pipeline, NULL );
			if ( p->layout )
				qvkDestroyPipelineLayout( vk.device, p->layout, NULL );
			if ( p->set_layout )
				qvkDestroyDescriptorSetLayout( vk.device, p->set_layout, NULL );
			if ( p->module )
				qvkDestroyShaderModule( vk.device, p->module, NULL );
		}

		free( g_nrd.pipelines );
		g_nrd.pipelines = NULL;
	}
	g_nrd.num_pipelines = 0;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		if ( g_nrd.pools[i] )
			qvkDestroyDescriptorPool( vk.device, g_nrd.pools[i], NULL );
		g_nrd.pools[i] = VK_NULL_HANDLE;
	}

	if ( g_nrd.constants_pool )
		qvkDestroyDescriptorPool( vk.device, g_nrd.constants_pool, NULL );
	g_nrd.constants_pool = VK_NULL_HANDLE;
	g_nrd.constants_set = VK_NULL_HANDLE;

	if ( g_nrd.constants_layout )
		qvkDestroyDescriptorSetLayout( vk.device, g_nrd.constants_layout, NULL );
	g_nrd.constants_layout = VK_NULL_HANDLE;

	for ( i = 0; i < g_nrd.num_samplers; i++ )
	{
		if ( g_nrd.samplers[i] )
			qvkDestroySampler( vk.device, g_nrd.samplers[i], NULL );
		g_nrd.samplers[i] = VK_NULL_HANDLE;
	}
	g_nrd.num_samplers = 0;

	if ( g_nrd.constants )
		qvkUnmapMemory( vk.device, g_nrd.constant_buffer.memory );
	g_nrd.constants = NULL;

	if ( g_nrd.constant_buffer.buffer )
		vk_rtx_buffer_destroy( &g_nrd.constant_buffer );

	if ( g_nrd.pool_images )
	{
		for ( i = 0; i < g_nrd.num_pool_images; i++ )
			vk_rtx_destroy_image( &g_nrd.pool_images[i] );

		free( g_nrd.pool_images );
		g_nrd.pool_images = NULL;
	}
	g_nrd.num_pool_images = 0;
	g_nrd.pool_images_ready = qfalse;

	vk_rtx_destroy_pipeline( &g_nrd.prepare_pipeline );
	vk_rtx_destroy_pipeline( &g_nrd.composite_pipeline );

	g_nrd.ready = qfalse;
}

// Makes the Vulkan objects for the instance. False when a format of the pools is not usable.
static qboolean nrd_create_vulkan_objects( const nrd::InstanceDesc *info, const nrd::LibraryDesc *lib )
{
	uint32_t i, j;
	VkPhysicalDeviceProperties props;
	uint32_t total_textures = info->descriptorPoolDesc.perSetTexturesMaxNum * info->descriptorPoolDesc.setsMaxNum;
	uint32_t total_storages = info->descriptorPoolDesc.perSetStorageTexturesMaxNum * info->descriptorPoolDesc.setsMaxNum;
	const uint32_t sets_per_pool = MAX( 1u, info->descriptorPoolDesc.setsMaxNum ) * NRD_RUNS_PER_FRAME;

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	g_nrd.resource_width = vk.extent_screen_images.width;
	g_nrd.resource_height = vk.extent_screen_images.height;

	// samplers: immutable, in the space of the constant buffer
	g_nrd.num_samplers = MIN( info->samplersNum, (uint32_t)ARRAY_LEN( g_nrd.samplers ) );
	for ( i = 0; i < g_nrd.num_samplers; i++ )
	{
		VkSamplerCreateInfo desc;
		const VkFilter filter = info->samplers[i] == nrd::Sampler::NEAREST_CLAMP ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

		Com_Memset( &desc, 0, sizeof(desc) );
		desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		desc.magFilter = filter;
		desc.minFilter = filter;
		desc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		desc.maxLod = 0.0f;
		VK_CHECK( qvkCreateSampler( vk.device, &desc, NULL, &g_nrd.samplers[i] ) );
	}

	{
		VkDescriptorSetLayoutBinding bindings[(size_t)nrd::Sampler::MAX_NUM + 1];
		VkDescriptorSetLayoutCreateInfo layout_info;

		Com_Memset( bindings, 0, sizeof(bindings) );
		for ( i = 0; i < g_nrd.num_samplers; i++ )
		{
			bindings[i].binding = lib->spirvBindingOffsets.samplerOffset + info->samplersBaseRegisterIndex + i;
			bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			bindings[i].descriptorCount = 1;
			bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			bindings[i].pImmutableSamplers = &g_nrd.samplers[i];
		}
		bindings[i].binding = lib->spirvBindingOffsets.constantBufferOffset + info->constantBufferRegisterIndex;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		Com_Memset( &layout_info, 0, sizeof(layout_info) );
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = g_nrd.num_samplers + 1;
		layout_info.pBindings = bindings;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_info, NULL, &g_nrd.constants_layout ) );
	}

	// the constant buffer: one slice for each dispatch of each run of each frame index
	g_nrd.constants_stride = nrd_align( info->constantBufferMaxDataSize, props.limits.minUniformBufferOffsetAlignment );
	g_nrd.slices_per_run = MAX( 1u, info->descriptorPoolDesc.setsMaxNum );
	{
		const VkDeviceSize size = g_nrd.constants_stride * g_nrd.slices_per_run * NRD_RUNS_PER_FRAME * NUM_COMMAND_BUFFERS;
		void *mapped = NULL;

		VK_CHECK( vk_rtx_buffer_create( &g_nrd.constant_buffer, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) );
		VK_CHECK( qvkMapMemory( vk.device, g_nrd.constant_buffer.memory, 0, size, 0, &mapped ) );
		g_nrd.constants = (byte *)mapped;
		Com_Memset( g_nrd.constants, 0, (size_t)size );
	}

	{
		VkDescriptorPoolSize sizes[2];
		VkDescriptorPoolCreateInfo pool_info;
		VkDescriptorSetAllocateInfo alloc;
		VkDescriptorBufferInfo buffer_info;
		VkWriteDescriptorSet write;

		sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		sizes[0].descriptorCount = 1;
		sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
		sizes[1].descriptorCount = MAX( 1u, g_nrd.num_samplers );

		Com_Memset( &pool_info, 0, sizeof(pool_info) );
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.maxSets = 1;
		pool_info.poolSizeCount = ARRAY_LEN( sizes );
		pool_info.pPoolSizes = sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_info, NULL, &g_nrd.constants_pool ) );

		Com_Memset( &alloc, 0, sizeof(alloc) );
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = g_nrd.constants_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &g_nrd.constants_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &g_nrd.constants_set ) );

		buffer_info.buffer = g_nrd.constant_buffer.buffer;
		buffer_info.offset = 0;
		buffer_info.range = info->constantBufferMaxDataSize;

		Com_Memset( &write, 0, sizeof(write) );
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = g_nrd.constants_set;
		write.dstBinding = lib->spirvBindingOffsets.constantBufferOffset + info->constantBufferRegisterIndex;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write.pBufferInfo = &buffer_info;
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}

	// the pipelines
	g_nrd.num_pipelines = info->pipelinesNum;
	g_nrd.pipelines = (nrd_pipeline_t *)calloc( MAX( 1u, g_nrd.num_pipelines ), sizeof(nrd_pipeline_t) );

	for ( i = 0; i < g_nrd.num_pipelines; i++ )
	{
		const nrd::PipelineDesc *desc = &info->pipelines[i];
		nrd_pipeline_t *p = &g_nrd.pipelines[i];
		VkDescriptorSetLayoutBinding bindings[48];
		VkDescriptorSetLayoutCreateInfo layout_info;
		VkDescriptorSetLayout set_layouts[2];
		VkPipelineLayoutCreateInfo pipeline_layout_info;
		VkShaderModuleCreateInfo module_info;
		VkComputePipelineCreateInfo pipeline_info;
		uint32_t num_bindings = 0;
		char name[64];

		// The textures and the storage textures have their own registers: textures from textureOffset, storages from storageOffset.
		for ( j = 0; j < desc->resourceRangesNum; j++ )
		{
			const nrd::ResourceRangeDesc *range = &desc->resourceRanges[j];
			const qboolean storage = (qboolean)( range->descriptorType == nrd::DescriptorType::STORAGE_TEXTURE );
			uint32_t k;

			for ( k = 0; k < range->descriptorsNum; k++ )
			{
				const uint32_t base = storage ? lib->spirvBindingOffsets.storageTextureAndBufferOffset : lib->spirvBindingOffsets.textureOffset;
				const uint32_t index = storage ? p->num_storages++ : p->num_textures++;

				if ( num_bindings >= ARRAY_LEN( bindings ) )
				{
					ri.Printf( PRINT_WARNING, "NRD: pipeline %u has too many resources\n", i );
					return qfalse;
				}

				Com_Memset( &bindings[num_bindings], 0, sizeof(bindings[0]) );
				bindings[num_bindings].binding = base + info->resourcesBaseRegisterIndex + index;
				bindings[num_bindings].descriptorType = storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				bindings[num_bindings].descriptorCount = 1;
				bindings[num_bindings].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
				num_bindings++;
			}
		}

		Com_Memset( &layout_info, 0, sizeof(layout_info) );
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = num_bindings;
		layout_info.pBindings = bindings;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_info, NULL, &p->set_layout ) );

		// The set index is the register space: resources 0, constants and samplers 1.
		if ( info->resourcesSpaceIndex != 0 || info->constantBufferAndSamplersSpaceIndex != 1 )
		{
			ri.Printf( PRINT_WARNING, "NRD: unexpected register spaces\n" );
			return qfalse;
		}

		set_layouts[info->resourcesSpaceIndex] = p->set_layout;
		set_layouts[info->constantBufferAndSamplersSpaceIndex] = g_nrd.constants_layout;

		Com_Memset( &pipeline_layout_info, 0, sizeof(pipeline_layout_info) );
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.setLayoutCount = 2;
		pipeline_layout_info.pSetLayouts = set_layouts;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeline_layout_info, NULL, &p->layout ) );

		Com_Memset( &module_info, 0, sizeof(module_info) );
		module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		module_info.codeSize = (size_t)desc->computeShaderSPIRV.size;
		module_info.pCode = (const uint32_t *)desc->computeShaderSPIRV.bytecode;
		VK_CHECK( qvkCreateShaderModule( vk.device, &module_info, NULL, &p->module ) );

		Com_Memset( &pipeline_info, 0, sizeof(pipeline_info) );
		pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipeline_info.stage.module = p->module;
		pipeline_info.stage.pName = info->shaderEntryPoint;
		pipeline_info.layout = p->layout;
		VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &p->pipeline ) );

		// The identifier is "file|macro=value...": the name is the file.
		Q_strncpyz( name, desc->shaderIdentifier, sizeof(name) );
		if ( strchr( name, '|' ) )
			*strchr( name, '|' ) = '\0';
		VK_SET_OBJECT_NAME( p->pipeline, name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	}

	// One descriptor pool for each frame index. A run takes at most one set for each dispatch.
	{
		VkDescriptorPoolSize sizes[2];
		VkDescriptorPoolCreateInfo pool_info;
		uint32_t max_textures = 0, max_storages = 0;

		for ( i = 0; i < g_nrd.num_pipelines; i++ )
		{
			max_textures = MAX( max_textures, g_nrd.pipelines[i].num_textures );
			max_storages = MAX( max_storages, g_nrd.pipelines[i].num_storages );
		}

		// The limits of the instance sum the resources of all dispatches. The product with the largest pipeline is a bound too.
		total_textures = MAX( MAX( total_textures, info->descriptorPoolDesc.totalTexturesNum ), max_textures * sets_per_pool / NRD_RUNS_PER_FRAME );
		total_storages = MAX( MAX( total_storages, info->descriptorPoolDesc.totalStorageTexturesNum ), max_storages * sets_per_pool / NRD_RUNS_PER_FRAME );

		sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		sizes[0].descriptorCount = MAX( 1u, total_textures * NRD_RUNS_PER_FRAME );
		sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		sizes[1].descriptorCount = MAX( 1u, total_storages * NRD_RUNS_PER_FRAME );

		Com_Memset( &pool_info, 0, sizeof(pool_info) );
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.maxSets = sets_per_pool;
		pool_info.poolSizeCount = ARRAY_LEN( sizes );
		pool_info.pPoolSizes = sizes;

		for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
			VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_info, NULL, &g_nrd.pools[i] ) );
	}

	// the pool textures
	g_nrd.num_pool_images = info->permanentPoolSize + info->transientPoolSize;
	g_nrd.pool_images = (vkimage_t *)calloc( MAX( 1u, g_nrd.num_pool_images ), sizeof(vkimage_t) );

	for ( i = 0; i < g_nrd.num_pool_images; i++ )
	{
		const qboolean permanent = (qboolean)( i < info->permanentPoolSize );
		const nrd::TextureDesc *desc = permanent ? &info->permanentPool[i] : &info->transientPool[i - info->permanentPoolSize];
		const VkFormat format = nrd_vk_formats[(size_t)desc->format];
		char name[64];

		if ( !nrd_format_supported( format ) )
		{
			ri.Printf( PRINT_WARNING, "NRD: the format %d of a pool texture is not usable as a storage image\n", (int)format );
			g_nrd.num_pool_images = i;
			return qfalse;
		}

		Com_sprintf( name, sizeof(name), "NRD %s %u", permanent ? "permanent" : "transient", permanent ? i : i - info->permanentPoolSize );
		nrd_create_pool_image( &g_nrd.pool_images[i], name,
			nrd_divide_up( g_nrd.resource_width, desc->downsampleFactor ), nrd_divide_up( g_nrd.resource_height, desc->downsampleFactor ), format );
	}

	// the shaders around NRD
	vk_rtx_create_standard_compute_pipeline( &g_nrd.prepare_pipeline, vk.compute_shader[SHADER_NRD_PREPARE_COMP], NULL, sizeof(nrd_push_t) );
	vk_rtx_create_standard_compute_pipeline( &g_nrd.composite_pipeline, vk.compute_shader[SHADER_NRD_COMPOSITE_COMP], NULL, sizeof(nrd_push_t) );

	return qtrue;
}

/*
=============
vk_rtx_nrd_init

Creates an instance with a RELAX (identifier 0) and a REBLUR (identifier 1) diffuse/specular denoiser,
and its Vulkan objects.
=============
*/
void vk_rtx_nrd_init( void )
{
	// One instance for both denoisers: the identifier is the argument of GetComputeDispatches.
	const nrd::DenoiserDesc denoisers[2] = {
		{ 0, nrd::Denoiser::RELAX_DIFFUSE_SPECULAR },
		{ 1, nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR }
	};
	nrd::InstanceCreationDesc desc = {};
	nrd::Result result;
	const nrd::InstanceDesc *info;
	const nrd::LibraryDesc *lib;

	// Both denoisers of NRD call this: the first one makes the objects.
	if ( g_nrd.ready )
		return;

	if ( g_nrd.instance || g_nrd.pipelines )
		vk_rtx_nrd_shutdown();

	// Null callbacks select the default allocator.
	desc.denoisers = denoisers;
	desc.denoisersNum = 2;

	result = nrd::CreateInstance( desc, g_nrd.instance );
	if ( result != nrd::Result::SUCCESS || !g_nrd.instance ) {
		ri.Printf( PRINT_WARNING, "NRD: CreateInstance failed (result %d)\n", (int)result );
		g_nrd.instance = NULL;
		return;
	}

	lib = nrd::GetLibraryDesc();
	info = nrd::GetInstanceDesc( *g_nrd.instance );

	if ( pt_verbose && pt_verbose->integer )
	{
		ri.Printf( PRINT_ALL, "NRD v%d.%d.%d: RELAX and REBLUR (diffuse and specular) instance created\n",
			lib->versionMajor, lib->versionMinor, lib->versionBuild );
		ri.Printf( PRINT_ALL, "NRD: %u pipelines, permanent pool %u, transient pool %u\n",
			info->pipelinesNum, info->permanentPoolSize, info->transientPoolSize );
		ri.Printf( PRINT_ALL, "NRD: constant buffer max %u bytes, %u samplers, entry point %s\n",
			info->constantBufferMaxDataSize, info->samplersNum, info->shaderEntryPoint );
		ri.Printf( PRINT_ALL, "NRD: spaces cb/samplers %u, resources %u; registers cb %u, samplers %u, resources %u\n",
			info->constantBufferAndSamplersSpaceIndex, info->resourcesSpaceIndex,
			info->constantBufferRegisterIndex, info->samplersBaseRegisterIndex, info->resourcesBaseRegisterIndex );
		ri.Printf( PRINT_ALL, "NRD: SPIR-V offsets s %u, b %u, u %u, t %u\n",
			lib->spirvBindingOffsets.samplerOffset, lib->spirvBindingOffsets.constantBufferOffset,
			lib->spirvBindingOffsets.storageTextureAndBufferOffset, lib->spirvBindingOffsets.textureOffset );
		ri.Printf( PRINT_ALL, "NRD: descriptor sets max %u, per set textures %u storages %u, totals %u %u\n",
			info->descriptorPoolDesc.setsMaxNum, info->descriptorPoolDesc.perSetTexturesMaxNum, info->descriptorPoolDesc.perSetStorageTexturesMaxNum,
			info->descriptorPoolDesc.totalTexturesNum, info->descriptorPoolDesc.totalStorageTexturesNum );
	}

	if ( !nrd_create_vulkan_objects( info, lib ) )
	{
		ri.Printf( PRINT_WARNING, "NRD: the Vulkan objects are not available, pt_denoiser 2 and 3 are off\n" );
		vk_rtx_nrd_shutdown();
		return;
	}

	g_nrd.history_valid[0] = g_nrd.history_valid[1] = qfalse;
	g_nrd.frame_index = 0;
	g_nrd.last_frame_counter = (uint32_t)-1;
	g_nrd.runs_this_frame = 0;
	g_nrd.ready = qtrue;
}

/*
=============
vk_rtx_nrd_shutdown
=============
*/
void vk_rtx_nrd_shutdown( void )
{
	nrd_destroy_vulkan_objects();

	if ( !g_nrd.instance )
		return;

	nrd::DestroyInstance( *g_nrd.instance );
	g_nrd.instance = NULL;
}

qboolean vk_rtx_nrd_available( void )
{
	return g_nrd.ready;
}

// The NRD projection: the Y axis of the image goes down, the Z row is the one of a Direct3D left-handed matrix.
static void nrd_projection( const float *P, float znear, float zfar, float out[16] )
{
	Com_Memcpy( out, P, sizeof(float) * 16 );

	// The projection of the tracer makes a screen Y that goes down with the view Y up: the row Y is negated.
	out[1] = -out[1];
	out[5] = -out[5];
	out[9] = -out[9];
	out[13] = -out[13];

	out[10] = zfar / ( zfar - znear );
	out[14] = -znear * zfar / ( zfar - znear );
}

static VkImageView nrd_resource_view( const nrd::ResourceDesc *r, const nrd::InstanceDesc *info )
{
	switch ( r->type )
	{
	case nrd::ResourceType::IN_MV:						return vk.img_rtx[RTX_IMG_NRD_IN_MV].view;
	case nrd::ResourceType::IN_NORMAL_ROUGHNESS:		return vk.img_rtx[RTX_IMG_NRD_IN_NORMAL_ROUGHNESS].view;
	case nrd::ResourceType::IN_VIEWZ:					return vk.img_rtx[RTX_IMG_NRD_IN_VIEWZ].view;
	case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:	return vk.img_rtx[RTX_IMG_NRD_IN_DIFF].view;
	case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:	return vk.img_rtx[RTX_IMG_NRD_IN_SPEC].view;
	case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:	return vk.img_rtx[RTX_IMG_NRD_OUT_DIFF].view;
	case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:	return vk.img_rtx[RTX_IMG_NRD_OUT_SPEC].view;
	case nrd::ResourceType::OUT_VALIDATION:				return vk.img_rtx[RTX_IMG_NRD_OUT_VALIDATION].view;
	case nrd::ResourceType::PERMANENT_POOL:				return g_nrd.pool_images[r->indexInPool].view;
	case nrd::ResourceType::TRANSIENT_POOL:				return g_nrd.pool_images[info->permanentPoolSize + r->indexInPool].view;
	default:											return VK_NULL_HANDLE;
	}
}

static void nrd_memory_barrier( VkCommandBuffer cmd_buf )
{
	VkMemoryBarrier barrier;

	Com_Memset( &barrier, 0, sizeof(barrier) );
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

	qvkCmdPipelineBarrier( cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &barrier, 0, NULL, 0, NULL );
}

// The pool textures start in the layout UNDEFINED.
static void nrd_transition_pool_images( VkCommandBuffer cmd_buf )
{
	uint32_t i;

	if ( g_nrd.pool_images_ready )
		return;

	for ( i = 0; i < g_nrd.num_pool_images; i++ )
	{
		VkImageMemoryBarrier barrier;

		Com_Memset( &barrier, 0, sizeof(barrier) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = g_nrd.pool_images[i].handle;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

		qvkCmdPipelineBarrier( cmd_buf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );
	}

	g_nrd.pool_images_ready = qtrue;
}

// Sets the settings of the frame and records the dispatches of NRD.
static void nrd_denoise( VkCommandBuffer cmd_buf, int which, const nrd_tuning_t *tuning )
{
	const vkUniformRTX_t *ubo = &vk.uniform_buffer;
	const nrd::InstanceDesc *info = nrd::GetInstanceDesc( *g_nrd.instance );
	const nrd::LibraryDesc *lib = nrd::GetLibraryDesc();
	const uint32_t slot = vk.current_frame_index;
	const nrd::Identifier identifier = (nrd::Identifier)which;
	nrd::CommonSettings common;
	nrd::RelaxSettings relax;
	nrd::ReblurSettings reblur;
	const nrd::DispatchDesc *dispatches = NULL;
	uint32_t num_dispatches = 0;
	uint32_t i, next_slice = 0;
	VkDeviceSize prev_offset = 0;

	// A new frame index: its pool and constant slices are free.
	if ( vk.frame_counter != g_nrd.last_frame_counter )
	{
		g_nrd.last_frame_counter = vk.frame_counter;
		g_nrd.runs_this_frame = 0;
		VK_CHECK( qvkResetDescriptorPool( vk.device, g_nrd.pools[slot], 0 ) );
	}
	else if ( ++g_nrd.runs_this_frame >= NRD_RUNS_PER_FRAME )
	{
		ri.Printf( PRINT_WARNING, "NRD: too many runs in a frame\n" );
		return;
	}

	{
		const float znear = backEnd.viewParms.zNear;
		const float zfar = backEnd.viewParms.zFar;
		const qboolean random_offset = (qboolean)( ubo->flt_taa == AA_MODE_TAA || ubo->temporal_blend_factor > 0.f );

		// The default members are the defaults of NRD.
		common.accumulationMode = g_nrd.history_valid[which] ? nrd::AccumulationMode::CONTINUE : nrd::AccumulationMode::CLEAR_AND_RESTART;

		nrd_projection( ubo->P, znear, zfar, common.viewToClipMatrix );
		nrd_projection( ubo->P_prev, znear, zfar, common.viewToClipMatrixPrev );
		Com_Memcpy( common.worldToViewMatrix, ubo->V, sizeof(float) * 16 );
		Com_Memcpy( common.worldToViewMatrixPrev, ubo->V_prev, sizeof(float) * 16 );

		// 2.5D motion: previous minus current screen position, in the units of the screen (0 to 1),
		// and .z = viewZprev - viewZ (nrd_prepare.comp).
		common.motionVectorScale[0] = 1.0f;
		common.motionVectorScale[1] = 1.0f;
		common.motionVectorScale[2] = 1.0f;
		common.isMotionVectorInWorldSpace = false;

		// The sub-pixel jitter of the tracer, in pixels. Random offsets (TAA mode, accumulation) are not known to NRD.
		common.cameraJitter[0] = random_offset ? 0.f : ubo->sub_pixel_jitter[0];
		common.cameraJitter[1] = random_offset ? 0.f : ubo->sub_pixel_jitter[1];
		common.cameraJitterPrev[0] = g_nrd.jitter_prev[0];
		common.cameraJitterPrev[1] = g_nrd.jitter_prev[1];
		g_nrd.jitter_prev[0] = common.cameraJitter[0];
		g_nrd.jitter_prev[1] = common.cameraJitter[1];

		common.resourceSize[0] = common.resourceSizePrev[0] = (uint16_t)g_nrd.resource_width;
		common.resourceSize[1] = common.resourceSizePrev[1] = (uint16_t)g_nrd.resource_height;
		common.rectSize[0] = (uint16_t)vk.extent_render.width;
		common.rectSize[1] = (uint16_t)vk.extent_render.height;
		common.rectSizePrev[0] = g_nrd.rect_prev[0] ? g_nrd.rect_prev[0] : common.rectSize[0];
		common.rectSizePrev[1] = g_nrd.rect_prev[1] ? g_nrd.rect_prev[1] : common.rectSize[1];
		g_nrd.rect_prev[0] = common.rectSize[0];
		g_nrd.rect_prev[1] = common.rectSize[1];

		// A pixel with a view Z of PRIMARY_RAY_T_MAX or more is the sky.
		common.denoisingRange = PRIMARY_RAY_T_MAX;
		common.enableValidation = pt_nrd_validation->integer != 0;
		common.frameIndex = g_nrd.frame_index++;
	}

	if ( nrd::SetCommonSettings( *g_nrd.instance, common ) != nrd::Result::SUCCESS )
		ri.Printf( PRINT_WARNING, "NRD: SetCommonSettings failed\n" );

	// The tracer picks the diffuse or the specular ray of the first bounce for a pixel: the other lobe has no hit distance.
	// The modes of the hit distance reconstruction are in the order of the cvar.
	if ( which == 0 )
	{
		relax.diffuseMaxAccumulatedFrameNum = relax.specularMaxAccumulatedFrameNum = (uint32_t)tuning->max_accum;
		relax.diffuseMaxFastAccumulatedFrameNum = relax.specularMaxFastAccumulatedFrameNum = (uint32_t)tuning->max_fast_accum;
		relax.diffusePrepassBlurRadius = tuning->prepass_blur;
		relax.specularPrepassBlurRadius = tuning->prepass_blur * ( 50.0f / 30.0f );
		if ( tuning->antifirefly >= 0 )
			relax.enableAntiFirefly = tuning->antifirefly != 0;
		relax.hitDistanceReconstructionMode = (nrd::HitDistanceReconstructionMode)tuning->hitdist_recon;
	}
	else
	{
		reblur.maxAccumulatedFrameNum = (uint32_t)tuning->max_accum;
		reblur.maxFastAccumulatedFrameNum = (uint32_t)tuning->max_fast_accum;
		reblur.maxStabilizedFrameNum = MIN( reblur.maxStabilizedFrameNum, reblur.maxAccumulatedFrameNum );
		reblur.diffusePrepassBlurRadius = tuning->prepass_blur;
		reblur.specularPrepassBlurRadius = tuning->prepass_blur * ( 50.0f / 30.0f );
		if ( tuning->antifirefly >= 0 )
			reblur.enableAntiFirefly = tuning->antifirefly != 0;
		reblur.hitDistanceReconstructionMode = (nrd::HitDistanceReconstructionMode)tuning->hitdist_recon;
		reblur.hitDistanceParameters.A *= NRD_UNITS_PER_METER;
	}

	if ( nrd::SetDenoiserSettings( *g_nrd.instance, identifier, which == 0 ? (const void *)&relax : (const void *)&reblur ) != nrd::Result::SUCCESS )
		ri.Printf( PRINT_WARNING, "NRD: SetDenoiserSettings failed\n" );

	if ( nrd::GetComputeDispatches( *g_nrd.instance, &identifier, 1, dispatches, num_dispatches ) != nrd::Result::SUCCESS )
	{
		ri.Printf( PRINT_WARNING, "NRD: GetComputeDispatches failed\n" );
		return;
	}

	for ( i = 0; i < num_dispatches; i++ )
	{
		const nrd::DispatchDesc *dispatch = &dispatches[i];
		const nrd_pipeline_t *p = &g_nrd.pipelines[dispatch->pipelineIndex];
		VkDescriptorImageInfo images[48];
		VkWriteDescriptorSet writes[48];
		VkDescriptorSetAllocateInfo alloc;
		VkDescriptorSet set;
		uint32_t n, num_textures = 0, num_storages = 0, dynamic_offset;
		qboolean valid = qtrue;
		VkResult res;

		if ( dispatch->resourcesNum > ARRAY_LEN( writes ) )
			continue;

		Com_Memset( &alloc, 0, sizeof(alloc) );
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = g_nrd.pools[slot];
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &p->set_layout;
		res = qvkAllocateDescriptorSets( vk.device, &alloc, &set );
		if ( res != VK_SUCCESS )
		{
			ri.Printf( PRINT_WARNING, "NRD: no descriptor set left (%d)\n", (int)res );
			return;
		}

		for ( n = 0; n < dispatch->resourcesNum; n++ )
		{
			const nrd::ResourceDesc *r = &dispatch->resources[n];
			const qboolean storage = (qboolean)( r->descriptorType == nrd::DescriptorType::STORAGE_TEXTURE );
			const uint32_t base = storage ? lib->spirvBindingOffsets.storageTextureAndBufferOffset : lib->spirvBindingOffsets.textureOffset;
			const uint32_t index = storage ? num_storages++ : num_textures++;

			images[n].sampler = VK_NULL_HANDLE;
			images[n].imageView = nrd_resource_view( r, info );
			images[n].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			valid = (qboolean)( valid && images[n].imageView != VK_NULL_HANDLE );

			Com_Memset( &writes[n], 0, sizeof(writes[n]) );
			writes[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[n].dstSet = set;
			writes[n].dstBinding = base + info->resourcesBaseRegisterIndex + index;
			writes[n].descriptorCount = 1;
			writes[n].descriptorType = storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			writes[n].pImageInfo = &images[n];
		}

		if ( !valid || num_textures != p->num_textures || num_storages != p->num_storages )
		{
			ri.Printf( PRINT_WARNING, "NRD: the resources of the dispatch %u do not match its pipeline\n", i );
			return;
		}

		qvkUpdateDescriptorSets( vk.device, dispatch->resourcesNum, writes, 0, NULL );

		// The constants: a slice of the ring for each dispatch, unless the data is the one of the previous dispatch.
		if ( dispatch->constantBufferDataSize && !dispatch->constantBufferDataMatchesPreviousDispatch )
		{
			const uint32_t slice = ( ( slot * NRD_RUNS_PER_FRAME + g_nrd.runs_this_frame ) * g_nrd.slices_per_run ) + next_slice++;

			if ( next_slice > g_nrd.slices_per_run || dispatch->constantBufferDataSize > info->constantBufferMaxDataSize )
			{
				ri.Printf( PRINT_WARNING, "NRD: no constant slice left\n" );
				return;
			}

			prev_offset = (VkDeviceSize)slice * g_nrd.constants_stride;
			Com_Memcpy( g_nrd.constants + prev_offset, dispatch->constantBufferData, dispatch->constantBufferDataSize );
		}
		dynamic_offset = (uint32_t)prev_offset;

		{
			VkDescriptorSet sets[2];
			sets[info->resourcesSpaceIndex] = set;
			sets[info->constantBufferAndSamplersSpaceIndex] = g_nrd.constants_set;

			qvkCmdBindPipeline( cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, p->pipeline );
			qvkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, p->layout, 0, 2, sets, 1, &dynamic_offset );
		}

		qvkCmdDispatch( cmd_buf, dispatch->gridWidth, dispatch->gridHeight, 1 );
		nrd_memory_barrier( cmd_buf );

	}
}

static void nrd_filter( VkCommandBuffer cmd_buf, int which )
{
	nrd_tuning_t tuning;
	nrd_push_t push;

	if ( !g_nrd.ready )
		return;

	nrd_read_tuning( &tuning );

	Com_Memset( &push, 0, sizeof(push) );
	{
		nrd::ReblurSettings reblur;

		push.hit_dist_params[0] = reblur.hitDistanceParameters.A * NRD_UNITS_PER_METER;
		push.hit_dist_params[1] = reblur.hitDistanceParameters.B;
		push.hit_dist_params[2] = reblur.hitDistanceParameters.C;
	}
	push.mode = (uint32_t)which;
	push.direct = (uint32_t)tuning.direct;
	push.validation = pt_nrd_validation->integer != 0 ? 1u : 0u;

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_SH] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_LF_COCG] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_HF] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_COLOR_SPEC] );
	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_PT_HIT_DIST] );

	nrd_transition_pool_images( cmd_buf );

	// the inputs
	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &g_nrd.prepare_pipeline );
	qvkCmdPushConstants( cmd_buf, g_nrd.prepare_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push );
	qvkCmdDispatch( cmd_buf, ( vk.extent_render.width + 15 ) / 16, ( vk.extent_render.height + 15 ) / 16, 1 );
	nrd_memory_barrier( cmd_buf );

	nrd_denoise( cmd_buf, which, &tuning );

	// the composition, in the layout of the tracer images
	vk_rtx_bind_standard_compute_pipeline( cmd_buf, &g_nrd.composite_pipeline );
	qvkCmdPushConstants( cmd_buf, g_nrd.composite_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push );
	qvkCmdDispatch( cmd_buf, ( vk.gpu_slice_width + 15 ) / 16, ( vk.extent_render.height + 15 ) / 16, 1 );

	BARRIER_COMPUTE( cmd_buf, vk.img_rtx[RTX_IMG_DENOISED_COLOR] );
}

static void vk_rtx_nrd_relax_filter( VkCommandBuffer cmd_buf )		{ nrd_filter( cmd_buf, 0 ); }
static void vk_rtx_nrd_reblur_filter( VkCommandBuffer cmd_buf )		{ nrd_filter( cmd_buf, 1 ); }

static void vk_rtx_nrd_relax_invalidate_history( void )				{ g_nrd.history_valid[0] = qfalse; }
static void vk_rtx_nrd_reblur_invalidate_history( void )			{ g_nrd.history_valid[1] = qfalse; }

static void vk_rtx_nrd_relax_end_frame( qboolean active )			{ g_nrd.history_valid[0] = active; }
static void vk_rtx_nrd_reblur_end_frame( qboolean active )			{ g_nrd.history_valid[1] = active; }

// The Vulkan objects are shared: the ReLAX entry makes and destroys them, the ReBLUR entry has nothing to do.
static void vk_rtx_nrd_reblur_none( void )
{
}

#define NRD_DENOISER_FLAGS	( DENOISER_FLAG_ACTIVE | DENOISER_FLAG_SPEC_DEMODULATE | DENOISER_FLAG_HIT_DISTANCE | DENOISER_FLAG_NRD_GUIDES )

const denoiser_t vk_rtx_denoiser_nrd_relax = {
	"nrd relax",
	NRD_DENOISER_FLAGS,
	vk_rtx_nrd_init,
	vk_rtx_nrd_shutdown,
	vk_rtx_nrd_relax_invalidate_history,
	NULL,
	NULL,
	vk_rtx_nrd_relax_filter,
	vk_rtx_nrd_relax_end_frame
};

const denoiser_t vk_rtx_denoiser_nrd_reblur = {
	"nrd reblur",
	NRD_DENOISER_FLAGS,
	vk_rtx_nrd_reblur_none,
	vk_rtx_nrd_reblur_none,
	vk_rtx_nrd_reblur_invalidate_history,
	NULL,
	NULL,
	vk_rtx_nrd_reblur_filter,
	vk_rtx_nrd_reblur_end_frame
};
