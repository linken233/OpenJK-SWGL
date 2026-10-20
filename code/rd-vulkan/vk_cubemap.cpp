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

#ifdef VK_CUBEMAP

/*
Environment cubemaps of the map probes (r_cubeMapping), ported from JKSunny/EternalJK.

- Capture: R_RenderCubemaps adds six 90 degree views of the world per probe. RB_DrawSurfs
  draws each view into one face of vk.cubemap.color_image (RENDER_PASS_CUBEMAP).
- Prefilter: RC_CONVOLVECUBEMAP makes the mips of the capture, then writes GGX prefiltered
  mips into the cubemap of the probe. Mip j holds roughness j / (REF_CUBEMAP_MIPS - 1).
- pbr.glsl samples the cubemap with the direction ( x, -y, z ): the capture axes in
  R_RenderCubemaps and the face directions in prefilterenvmap.frag agree with it.

Differences from sunny: no geometry shader (one draw per face), a mip chain on the capture
for the filtered source reads, no irradiance cubemap (pbr.glsl does not read it).
*/

#define CUBEMAP_CAPTURE_MIPS	9		// log2( REF_CUBEMAP_SIZE ) + 1
#define CUBEMAP_PROBE_FORMAT	VK_FORMAT_R16G16B16A16_SFLOAT

static void create_image( VkImage *image, VkDeviceMemory *memory, VkFormat format, uint32_t size,
	uint32_t mipLevels, uint32_t layers, VkImageUsageFlags usage, VkImageCreateFlags flags, const char *name )
{
	VkImageCreateInfo desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.flags = flags;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = format;
	desc.extent.width = size;
	desc.extent.height = size;
	desc.extent.depth = 1;
	desc.mipLevels = mipLevels;
	desc.arrayLayers = layers;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	desc.usage = usage;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, image ) );

	qvkGetImageMemoryRequirements( vk.device, *image, &memory_requirements );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, *image, *memory, 0 ) );

	VK_SET_OBJECT_NAME( *image, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
}

static VkImageView create_view( VkImage image, VkFormat format, VkImageViewType type, VkImageAspectFlags aspect,
	uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount )
{
	VkImageViewCreateInfo desc;
	VkImageView view;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	desc.image = image;
	desc.viewType = type;
	desc.format = format;
	desc.subresourceRange.aspectMask = aspect;
	desc.subresourceRange.baseMipLevel = baseMip;
	desc.subresourceRange.levelCount = mipCount;
	desc.subresourceRange.baseArrayLayer = baseLayer;
	desc.subresourceRange.layerCount = layerCount;
	VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &view ) );

	return view;
}

// A layout transition of mips [baseMip, baseMip + mipCount) of all six faces.
static void image_barrier( VkCommandBuffer cmd, VkImage image, uint32_t baseMip, uint32_t mipCount,
	VkImageLayout oldLayout, VkImageLayout newLayout,
	VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess )
{
	VkImageMemoryBarrier barrier;

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = baseMip;
	barrier.subresourceRange.levelCount = mipCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 6;

	qvkCmdPipelineBarrier( cmd, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &barrier );
}

static VkSampler cubemap_sampler( void )
{
	Vk_Sampler_Def sd;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = GL_LINEAR;
	sd.gl_min_filter = GL_LINEAR_MIPMAP_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;

	return vk_find_sampler( &sd );
}

static VkDescriptorSet alloc_cube_descriptor( VkImageView view )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet desc;
	VkDescriptorSet set;

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_sampler;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &set ) );

	info.sampler = cubemap_sampler();
	info.imageView = view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = set;
	desc.dstBinding = 0;
	desc.descriptorCount = 1;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	desc.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	return set;
}

static void create_capture_render_pass( void )
{
	VkAttachmentDescription attachments[2];
	VkAttachmentReference color_ref, depth_ref;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[2];
	VkRenderPassCreateInfo desc;

	Com_Memset( attachments, 0, sizeof( attachments ) );

	// one face of mip 0, read by the mip blits after the pass
	attachments[0].format = vk.color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	attachments[1].format = vk.depth_format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	color_ref.attachment = 0;
	color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	depth_ref.attachment = 1;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

	// after the previous face (depth) and the previous prefilter (blits, sampling)
	Com_Memset( deps, 0, sizeof( deps ) );
	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	deps[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	// before the mip blits
	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.attachmentCount = 2;
	desc.pAttachments = attachments;
	desc.subpassCount = 1;
	desc.pSubpasses = &subpass;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;
	VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &vk.cubemap.render_pass ) );
	VK_SET_OBJECT_NAME( vk.cubemap.render_pass, "render pass - cubemap capture", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
}

static void create_prefilter_render_pass( void )
{
	VkAttachmentDescription attachment;
	VkAttachmentReference color_ref;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[2];
	VkRenderPassCreateInfo desc;

	// one face of the scratch image, copied into the probe after the pass
	Com_Memset( &attachment, 0, sizeof( attachment ) );
	attachment.format = CUBEMAP_PROBE_FORMAT;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	color_ref.attachment = 0;
	color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;

	// after the copy of the previous mip
	Com_Memset( deps, 0, sizeof( deps ) );
	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[0].srcAccessMask = 0;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// before the copy
	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.attachmentCount = 1;
	desc.pAttachments = &attachment;
	desc.subpassCount = 1;
	desc.pSubpasses = &subpass;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;
	VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &vk.cubemap.prefilter_render_pass ) );
	VK_SET_OBJECT_NAME( vk.cubemap.prefilter_render_pass, "render pass - cubemap prefilter", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
}

static void create_prefilter_pipeline( void )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkGraphicsPipelineCreateInfo create_info;
	VkPipelineLayoutCreateInfo layout;
	VkPushConstantRange push_range;
	int i;

	// roughness, face
	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( float ) + sizeof( int32_t );

	Com_Memset( &layout, 0, sizeof( layout ) );
	layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout.setLayoutCount = 1;
	layout.pSetLayouts = &vk.set_layout_sampler;
	layout.pushConstantRangeCount = 1;
	layout.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &layout, NULL, &vk.cubemap.prefilter_layout ) );

	Com_Memset( shader_stages, 0, sizeof( shader_stages ) );
	for ( i = 0; i < 2; i++ ) {
		shader_stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[i].pName = "main";
	}
	shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[0].module = vk.shaders.filtercube_vs;
	shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[1].module = vk.shaders.prefilterenvmap_fs;

	Com_Memset( &vertex_input_state, 0, sizeof( vertex_input_state ) );
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	Com_Memset( &input_assembly_state, 0, sizeof( input_assembly_state ) );
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	Com_Memset( &rasterization_state, 0, sizeof( rasterization_state ) );
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization_state.lineWidth = 1.0f;

	Com_Memset( &multisample_state, 0, sizeof( multisample_state ) );
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	Com_Memset( &attachment_blend_state, 0, sizeof( attachment_blend_state ) );
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &blend_state, 0, sizeof( blend_state ) );
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;

	Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_state_array );
	dynamic_state.pDynamicStates = dynamic_state_array;

	Com_Memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = ARRAY_LEN( shader_stages );
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = vk.cubemap.prefilter_layout;
	create_info.renderPass = vk.cubemap.prefilter_render_pass;
	create_info.basePipelineIndex = -1;
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &vk.cubemap.prefilter_pipeline ) );
	VK_SET_OBJECT_NAME( vk.cubemap.prefilter_pipeline, "pipeline - cubemap prefilter", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}

void vk_create_cubemap_resources( void )
{
	VkFramebufferCreateInfo desc;
	VkImageView attachments[2];
	VkImageAspectFlags depth_aspect;
	VkCommandBuffer cmd;
	int i;

	if ( !vk.cubemapActive )
		return;

	// capture target
	create_image( &vk.cubemap.color_image, &vk.cubemap.color_memory, vk.color_format, REF_CUBEMAP_SIZE,
		CUBEMAP_CAPTURE_MIPS, 6,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, "cubemap capture" );

	for ( i = 0; i < 6; i++ )
		vk.cubemap.color_face_view[i] = create_view( vk.cubemap.color_image, vk.color_format, VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1 );
	vk.cubemap.color_cube_view = create_view( vk.cubemap.color_image, vk.color_format, VK_IMAGE_VIEW_TYPE_CUBE,
		VK_IMAGE_ASPECT_COLOR_BIT, 0, CUBEMAP_CAPTURE_MIPS, 0, 6 );

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

	create_image( &vk.cubemap.depth_image, &vk.cubemap.depth_memory, vk.depth_format, REF_CUBEMAP_SIZE, 1, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, "cubemap capture depth" );
	vk.cubemap.depth_view = create_view( vk.cubemap.depth_image, vk.depth_format, VK_IMAGE_VIEW_TYPE_2D,
		depth_aspect, 0, 1, 0, 1 );

	// prefilter target
	create_image( &vk.cubemap.scratch_image, &vk.cubemap.scratch_memory, CUBEMAP_PROBE_FORMAT, REF_CUBEMAP_SIZE, 1, 6,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, "cubemap prefilter scratch" );

	for ( i = 0; i < 6; i++ )
		vk.cubemap.scratch_face_view[i] = create_view( vk.cubemap.scratch_image, CUBEMAP_PROBE_FORMAT, VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1 );

	create_capture_render_pass();
	create_prefilter_render_pass();
	create_prefilter_pipeline();

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.pAttachments = attachments;
	desc.width = REF_CUBEMAP_SIZE;
	desc.height = REF_CUBEMAP_SIZE;
	desc.layers = 1;

	for ( i = 0; i < 6; i++ )
	{
		desc.renderPass = vk.cubemap.render_pass;
		desc.attachmentCount = 2;
		attachments[0] = vk.cubemap.color_face_view[i];
		attachments[1] = vk.cubemap.depth_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.cubemap.framebuffer[i] ) );

		desc.renderPass = vk.cubemap.prefilter_render_pass;
		desc.attachmentCount = 1;
		attachments[0] = vk.cubemap.scratch_face_view[i];
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.cubemap.scratch_framebuffer[i] ) );
	}

	// the capture descriptor is valid before the first capture
	cmd = vk_begin_command_buffer();
	image_barrier( cmd, vk.cubemap.color_image, 0, CUBEMAP_CAPTURE_MIPS,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT );
	vk_end_command_buffer( cmd, __func__ );
}

// The prefiltered cubemaps of the map probes. Their descriptor sets go with the pool reset.
void vk_release_cubemaps( void )
{
	int i;

	for ( i = 0; i < MAX_CUBEMAPS; i++ )
	{
		if ( vk.cubemap.probe[i].view != VK_NULL_HANDLE )
			qvkDestroyImageView( vk.device, vk.cubemap.probe[i].view, NULL );
		if ( vk.cubemap.probe[i].image != VK_NULL_HANDLE )
			qvkDestroyImage( vk.device, vk.cubemap.probe[i].image, NULL );
		if ( vk.cubemap.probe[i].memory != VK_NULL_HANDLE )
			qvkFreeMemory( vk.device, vk.cubemap.probe[i].memory, NULL );
	}

	Com_Memset( vk.cubemap.probe, 0, sizeof( vk.cubemap.probe ) );
}

void vk_destroy_cubemap_resources( void )
{
	int i;

	vk_release_cubemaps();

	for ( i = 0; i < 6; i++ )
	{
		if ( vk.cubemap.framebuffer[i] != VK_NULL_HANDLE )
			qvkDestroyFramebuffer( vk.device, vk.cubemap.framebuffer[i], NULL );
		if ( vk.cubemap.scratch_framebuffer[i] != VK_NULL_HANDLE )
			qvkDestroyFramebuffer( vk.device, vk.cubemap.scratch_framebuffer[i], NULL );
		if ( vk.cubemap.color_face_view[i] != VK_NULL_HANDLE )
			qvkDestroyImageView( vk.device, vk.cubemap.color_face_view[i], NULL );
		if ( vk.cubemap.scratch_face_view[i] != VK_NULL_HANDLE )
			qvkDestroyImageView( vk.device, vk.cubemap.scratch_face_view[i], NULL );
	}

	if ( vk.cubemap.prefilter_pipeline != VK_NULL_HANDLE )
		qvkDestroyPipeline( vk.device, vk.cubemap.prefilter_pipeline, NULL );
	if ( vk.cubemap.prefilter_layout != VK_NULL_HANDLE )
		qvkDestroyPipelineLayout( vk.device, vk.cubemap.prefilter_layout, NULL );
	if ( vk.cubemap.prefilter_render_pass != VK_NULL_HANDLE )
		qvkDestroyRenderPass( vk.device, vk.cubemap.prefilter_render_pass, NULL );
	if ( vk.cubemap.render_pass != VK_NULL_HANDLE )
		qvkDestroyRenderPass( vk.device, vk.cubemap.render_pass, NULL );

	if ( vk.cubemap.color_cube_view != VK_NULL_HANDLE )
		qvkDestroyImageView( vk.device, vk.cubemap.color_cube_view, NULL );
	if ( vk.cubemap.depth_view != VK_NULL_HANDLE )
		qvkDestroyImageView( vk.device, vk.cubemap.depth_view, NULL );

	if ( vk.cubemap.color_image != VK_NULL_HANDLE )
		qvkDestroyImage( vk.device, vk.cubemap.color_image, NULL );
	if ( vk.cubemap.depth_image != VK_NULL_HANDLE )
		qvkDestroyImage( vk.device, vk.cubemap.depth_image, NULL );
	if ( vk.cubemap.scratch_image != VK_NULL_HANDLE )
		qvkDestroyImage( vk.device, vk.cubemap.scratch_image, NULL );

	if ( vk.cubemap.color_memory != VK_NULL_HANDLE )
		qvkFreeMemory( vk.device, vk.cubemap.color_memory, NULL );
	if ( vk.cubemap.depth_memory != VK_NULL_HANDLE )
		qvkFreeMemory( vk.device, vk.cubemap.depth_memory, NULL );
	if ( vk.cubemap.scratch_memory != VK_NULL_HANDLE )
		qvkFreeMemory( vk.device, vk.cubemap.scratch_memory, NULL );

	Com_Memset( &vk.cubemap, 0, sizeof( vk.cubemap ) );
}

// After each descriptor pool reset.
void vk_init_cubemap_descriptors( void )
{
	if ( !vk.cubemapActive || vk.cubemap.color_cube_view == VK_NULL_HANDLE )
		return;

	vk.cubemap.color_descriptor = alloc_cube_descriptor( vk.cubemap.color_cube_view );
}

// The environment of a draw: the probe cubemap once it is prefiltered, else black.
VkDescriptorSet vk_cubemap_descriptor( int cubemapIndex )
{
	if ( cubemapIndex > 0 && cubemapIndex <= MAX_CUBEMAPS && cubemapIndex <= tr.numCubemaps
		&& vk.cubemap.probe[cubemapIndex - 1].descriptor != VK_NULL_HANDLE )
		return vk.cubemap.probe[cubemapIndex - 1].descriptor;

	return vk.pbr.empty_cube_descriptor;
}

// The capture makes its mips: the prefilter reads the lower mips for the wide samples.
static void generate_capture_mips( VkCommandBuffer cmd )
{
	VkImageBlit blit;
	int32_t size = REF_CUBEMAP_SIZE;
	uint32_t i;

	// mip 0 is in TRANSFER_SRC_OPTIMAL after the capture pass
	for ( i = 1; i < CUBEMAP_CAPTURE_MIPS; i++ )
	{
		image_barrier( cmd, vk.cubemap.color_image, i, 1,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );

		Com_Memset( &blit, 0, sizeof( blit ) );
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.layerCount = 6;
		blit.srcOffsets[1].x = size;
		blit.srcOffsets[1].y = size;
		blit.srcOffsets[1].z = 1;
		size >>= 1;
		blit.dstSubresource = blit.srcSubresource;
		blit.dstSubresource.mipLevel = i;
		blit.dstOffsets[1].x = size;
		blit.dstOffsets[1].y = size;
		blit.dstOffsets[1].z = 1;

		qvkCmdBlitImage( cmd, vk.cubemap.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.cubemap.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR );

		image_barrier( cmd, vk.cubemap.color_image, i, 1,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	}

	image_barrier( cmd, vk.cubemap.color_image, 0, CUBEMAP_CAPTURE_MIPS,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT );
}

static void create_probe( int index )
{
	create_image( &vk.cubemap.probe[index].image, &vk.cubemap.probe[index].memory, CUBEMAP_PROBE_FORMAT,
		REF_CUBEMAP_SIZE, REF_CUBEMAP_MIPS, 6, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, va( "cubemap %i", index ) );
	vk.cubemap.probe[index].view = create_view( vk.cubemap.probe[index].image, CUBEMAP_PROBE_FORMAT,
		VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_ASPECT_COLOR_BIT, 0, REF_CUBEMAP_MIPS, 0, 6 );
}

/*
Called by the back end after the six faces of the probe. It interrupts the current
render pass and resumes it at the end.
*/
void vk_prefilter_cubemap( int cubemapIndex )
{
	const renderPass_t resume = vk.renderPassIndex;
	VkCommandBuffer cmd = vk.cmd->command_buffer;
	VkRenderPassBeginInfo begin_info;
	VkViewport viewport;
	VkRect2D scissor;
	VkImageCopy region;
	struct {
		float	roughness;
		int32_t	face;
	} push;
	uint32_t size, mip;
	int face, i;

	if ( cubemapIndex < 0 || cubemapIndex >= MAX_CUBEMAPS )
		return;

	vk_end_render_pass();

	if ( vk.cubemap.probe[cubemapIndex].image == VK_NULL_HANDLE )
		create_probe( cubemapIndex );

	generate_capture_mips( cmd );

	image_barrier( cmd, vk.cubemap.probe[cubemapIndex].image, 0, REF_CUBEMAP_MIPS,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );

	Com_Memset( &begin_info, 0, sizeof( begin_info ) );
	begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	begin_info.renderPass = vk.cubemap.prefilter_render_pass;

	Com_Memset( &region, 0, sizeof( region ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 6;
	region.dstSubresource = region.srcSubresource;
	region.extent.depth = 1;

	for ( mip = 0, size = REF_CUBEMAP_SIZE; mip < REF_CUBEMAP_MIPS; mip++, size >>= 1 )
	{
		push.roughness = (float)mip / (float)( REF_CUBEMAP_MIPS - 1 );

		viewport.x = viewport.y = 0.0f;
		viewport.width = viewport.height = (float)size;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		scissor.offset.x = scissor.offset.y = 0;
		scissor.extent.width = scissor.extent.height = size;

		begin_info.renderArea = scissor;

		for ( face = 0; face < 6; face++ )
		{
			push.face = face;
			begin_info.framebuffer = vk.cubemap.scratch_framebuffer[face];

			qvkCmdBeginRenderPass( cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE );
			qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.cubemap.prefilter_pipeline );
			qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.cubemap.prefilter_layout,
				0, 1, &vk.cubemap.color_descriptor, 0, NULL );
			qvkCmdPushConstants( cmd, vk.cubemap.prefilter_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
			qvkCmdSetViewport( cmd, 0, 1, &viewport );
			qvkCmdSetScissor( cmd, 0, 1, &scissor );
			qvkCmdDraw( cmd, 3, 1, 0, 0 );
			qvkCmdEndRenderPass( cmd );
		}

		region.dstSubresource.mipLevel = mip;
		region.extent.width = region.extent.height = size;
		qvkCmdCopyImage( cmd, vk.cubemap.scratch_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.cubemap.probe[cubemapIndex].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	image_barrier( cmd, vk.cubemap.probe[cubemapIndex].image, 0, REF_CUBEMAP_MIPS,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT );

	if ( vk.cubemap.probe[cubemapIndex].descriptor == VK_NULL_HANDLE )
		vk.cubemap.probe[cubemapIndex].descriptor = alloc_cube_descriptor( vk.cubemap.probe[cubemapIndex].view );

	vk_resume_render_pass( resume );

	// the prefilter layout is not compatible with vk.pipeline_layout: bind all again
	for ( i = 0; i < VK_DESC_COUNT; i++ )
		vk_reset_descriptor( i );
	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
}

#endif // VK_CUBEMAP
