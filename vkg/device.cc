#include <vkg/internal.hh>

#define GetInstProcAddress(inst, func)(PFN_##func)vkGetInstanceProcAddr(inst, #func);

#define GetVkProcAddress(dev, inst, func)(vkGetDeviceProcAddr(dev,#func)==NULL)?(PFN_##func)vkGetInstanceProcAddr(inst, #func):(PFN_##func)vkGetDeviceProcAddr(dev, #func)

#include "shaders.h"

#ifdef VKVG_WIRED_DEBUG
vkg_wired_debug_mode vkg_wired_debug = vkg_wired_debug_mode_normal;
#endif

PFN_vkCmdBindPipeline			CmdBindPipeline;
PFN_vkCmdBindDescriptorSets		CmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer		CmdBindIndexBuffer;
PFN_vkCmdBindVertexBuffers		CmdBindVertexBuffers;

PFN_vkCmdDrawIndexed			CmdDrawIndexed;
PFN_vkCmdDraw					CmdDraw;

PFN_vkCmdSetStencilCompareMask	CmdSetStencilCompareMask;
PFN_vkCmdSetStencilReference	CmdSetStencilReference;
PFN_vkCmdSetStencilWriteMask	CmdSetStencilWriteMask;
PFN_vkCmdBeginRenderPass		CmdBeginRenderPass;
PFN_vkCmdEndRenderPass			CmdEndRenderPass;
PFN_vkCmdSetViewport			CmdSetViewport;
PFN_vkCmdSetScissor				CmdSetScissor;

PFN_vkCmdPushConstants			CmdPushConstants;

PFN_vkWaitForFences				WaitForFences;
PFN_vkResetFences				ResetFences;
PFN_vkResetCommandBuffer		ResetCommandBuffer;

//TODO:save/reload cache in user temp directory
void _device_create_pipeline_cache(VkgDevice dev){

	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
	VK_CHECK_RESULT(vkCreatePipelineCache(dev->vke->vkdev, &pipelineCacheCreateInfo, NULL, &dev->pipelineCache));
}

VkRenderPass _device_createRenderPassNoResolve(vkg_device *dev, VkAttachmentLoadOp loadOp, VkAttachmentLoadOp stencilLoadOp)
{
	VkAttachmentDescription attColor = {
					.format = FB_COLOR_FORMAT,
					.samples = dev->samples,
					.loadOp = loadOp,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentDescription attDS = {
					.format = dev->stencilFormat,
					.samples = dev->samples,
					.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.stencilLoadOp = stencilLoadOp,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
					.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkAttachmentDescription attachments[] = {attColor,attDS};
	VkAttachmentReference colorRef	= {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference dsRef		= {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpassDescription = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
						.colorAttachmentCount	= 1,
						.pColorAttachments		= &colorRef,
						.pDepthStencilAttachment= &dsRef};

	VkSubpassDependency dependencies[] =
	{
		{ VK_SUBPASS_EXTERNAL, 0,
		  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		  VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		  VK_DEPENDENCY_BY_REGION_BIT},
		{ 0, VK_SUBPASS_EXTERNAL,
		  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
		  VK_DEPENDENCY_BY_REGION_BIT},
	};

	VkRenderPassCreateInfo renderPassInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 2,
				.pAttachments = attachments,
				.subpassCount = 1,
				.pSubpasses = &subpassDescription,
				.dependencyCount = 2,
				.pDependencies = dependencies
	};
	VkRenderPass rp;
	VK_CHECK_RESULT(vkCreateRenderPass(dev->vke->vkdev, &renderPassInfo, NULL, &rp));
	return rp;
}

/// move to vke.
VkRenderPass _device_createRenderPassMS(vkg_device *dev, VkAttachmentLoadOp loadOp, VkAttachmentLoadOp stencilLoadOp)
{
	VkAttachmentDescription attColor = {
					.format = FB_COLOR_FORMAT,
					.samples = dev->samples,
					.loadOp = loadOp,
					.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentDescription attColorResolve = {
					.format = FB_COLOR_FORMAT,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentDescription attDS = {
					.format = dev->stencilFormat,
					.samples = dev->samples,
					.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.stencilLoadOp = stencilLoadOp,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
					.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkAttachmentDescription attachments[] = {attColorResolve,attDS,attColor};
	VkAttachmentReference resolveRef= {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference dsRef		= {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	VkAttachmentReference colorRef	= {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpassDescription = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
						.colorAttachmentCount	= 1,
						.pColorAttachments		= &colorRef,
						.pResolveAttachments	= &resolveRef,
						.pDepthStencilAttachment= &dsRef};

	VkSubpassDependency dependencies[] =
	{
		{ VK_SUBPASS_EXTERNAL, 0,
		  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		  VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		  VK_DEPENDENCY_BY_REGION_BIT},
		{ 0, VK_SUBPASS_EXTERNAL,
		  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
		  VK_DEPENDENCY_BY_REGION_BIT},
	};

	VkRenderPassCreateInfo renderPassInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 3,
				.pAttachments = attachments,
				.subpassCount = 1,
				.pSubpasses = &subpassDescription,
				.dependencyCount = 2,
				.pDependencies = dependencies
	};
	VkRenderPass rp;
	VK_CHECK_RESULT(vkCreateRenderPass(dev->vke->vkdev, &renderPassInfo, NULL, &rp));
	return rp;
}

void _device_setupPipelines(vkg_device *dev)
{
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.renderPass = dev->renderPass };

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN };

	VkPipelineRasterizationStateCreateInfo rasterizationState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.polygonMode = VK_POLYGON_MODE_FILL,
				.cullMode = VK_CULL_MODE_NONE,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
				.depthClampEnable = VK_FALSE,
				.rasterizerDiscardEnable = VK_FALSE,
				.depthBiasEnable = VK_FALSE,
				.lineWidth = 1.0f };

	VkPipelineColorBlendAttachmentState blendAttachmentState =
	{ .colorWriteMask = 0x0, .blendEnable = VK_TRUE,
#ifdef VKVG_PREMULT_ALPHA
	  .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
	  .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	  .colorBlendOp = VK_BLEND_OP_ADD,
	  .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	  .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	  .alphaBlendOp = VK_BLEND_OP_ADD,
#else
	  .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
	  .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	  .colorBlendOp = VK_BLEND_OP_ADD,
	  .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	  .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
	  .alphaBlendOp = VK_BLEND_OP_ADD,
#endif
	};

	VkPipelineColorBlendStateCreateInfo colorBlendState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.attachmentCount = 1,
				.pAttachments = &blendAttachmentState };

										/*failOp,passOp,depthFailOp,compareOp, compareMask, writeMask, reference;*/
	VkStencilOpState polyFillOpState ={VK_STENCIL_OP_KEEP,VK_STENCIL_OP_INVERT,	VK_STENCIL_OP_KEEP,VK_COMPARE_OP_EQUAL,STENCIL_CLIP_BIT,STENCIL_FILL_BIT,0};
	VkStencilOpState clipingOpState = {VK_STENCIL_OP_ZERO,VK_STENCIL_OP_REPLACE,VK_STENCIL_OP_KEEP,VK_COMPARE_OP_EQUAL,STENCIL_FILL_BIT,STENCIL_ALL_BIT, 0x2};
	VkStencilOpState stencilOpState = {VK_STENCIL_OP_KEEP,VK_STENCIL_OP_ZERO,	VK_STENCIL_OP_KEEP,VK_COMPARE_OP_EQUAL,STENCIL_FILL_BIT,STENCIL_FILL_BIT,0x1};

	VkPipelineDepthStencilStateCreateInfo dsStateCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = VK_FALSE,
				.depthWriteEnable = VK_FALSE,
				.depthCompareOp = VK_COMPARE_OP_ALWAYS,
				.stencilTestEnable = VK_TRUE,
				.front = polyFillOpState,
				.back = polyFillOpState };

	VkDynamicState dynamicStateEnables[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
		VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
	};
	VkPipelineDynamicStateCreateInfo dynamicState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
				.dynamicStateCount = 2,
				.pDynamicStates = dynamicStateEnables };

	VkPipelineViewportStateCreateInfo viewportState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.viewportCount = 1, .scissorCount = 1 };

	VkPipelineMultisampleStateCreateInfo multisampleState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = dev->samples };
	/*if (dev->samples != VK_SAMPLE_COUNT_1_BIT){
		multisampleState.sampleShadingEnable = VK_TRUE;
		multisampleState.minSampleShading = 0.5f;
	}*/
	VkVertexInputBindingDescription vertexInputBinding = { .binding = 0,
				.stride = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX };

	VkVertexInputAttributeDescription vertexInputAttributs[3] = {
		{0, 0, VK_FORMAT_R32G32_SFLOAT,		0},
		{1, 0, VK_FORMAT_R8G8B8A8_UNORM,	8},
		{2, 0, VK_FORMAT_R32G32B32_SFLOAT, 12}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount	= 1,
		.pVertexBindingDescriptions		= &vertexInputBinding,
		.vertexAttributeDescriptionCount= 3,
		.pVertexAttributeDescriptions	= vertexInputAttributs };
#ifdef VKVG_WIRED_DEBUG
	VkShaderModule modVert, modFrag, modFragWired;
#else
	VkShaderModule modVert, modFrag;
#endif
	VkShaderModuleCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
											.pCode = (uint32_t*)vkg_main_vert_spv,
											.codeSize = vkg_main_vert_spv_len };
	VK_CHECK_RESULT(vkCreateShaderModule(dev->vke->vkdev, &createInfo, NULL, &modVert));
#if defined(VKVG_LCD_FONT_FILTER) && defined(FT_CONFIG_OPTION_SUBPIXEL_RENDERING)
	createInfo.pCode = (uint32_t*)vkg_main_lcd_frag_spv;
	createInfo.codeSize = vkg_main_lcd_frag_spv_len;
#else
	createInfo.pCode = (uint32_t*)vkg_main_frag_spv;
	createInfo.codeSize = vkg_main_frag_spv_len;
#endif
	VK_CHECK_RESULT(vkCreateShaderModule(dev->vke->vkdev, &createInfo, NULL, &modFrag));

	VkPipelineShaderStageCreateInfo vertStage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = modVert,
		.pName = "main",
	};
	VkPipelineShaderStageCreateInfo fragStage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = modFrag,
		.pName = "main",
	};

	// Use specialization constants to pass number of samples to the shader (used for MSAA resolve)
	/*VkSpecializationMapEntry specializationEntry = {
		.constantID = 0,
		.offset = 0,
		.size = sizeof(uint32_t)};
	uint32_t specializationData = VKVG_SAMPLES;
	VkSpecializationInfo specializationInfo = {
		.mapEntryCount = 1,
		.pMapEntries = &specializationEntry,
		.dataSize = sizeof(specializationData),
		.pData = &specializationData};*/

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage,fragStage};

	pipelineCreateInfo.stageCount = 1;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pDepthStencilState = &dsStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.layout = dev->pipelineLayout;

#ifndef __APPLE__
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vke->vkdev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipelinePolyFill));
#endif

	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	dsStateCreateInfo.back = dsStateCreateInfo.front = clipingOpState;
	dynamicState.dynamicStateCount = 5;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vke->vkdev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipelineClipping));

	dsStateCreateInfo.back = dsStateCreateInfo.front = stencilOpState;
	blendAttachmentState.colorWriteMask=0xf;
	dynamicState.dynamicStateCount = 3;
	pipelineCreateInfo.stageCount = 2;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vke->vkdev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipe_OVER));

	blendAttachmentState.alphaBlendOp = blendAttachmentState.colorBlendOp = VK_BLEND_OP_SUBTRACT;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vke->vkdev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipe_SUB));

	colorBlendState.logicOpEnable = VK_TRUE;
	blendAttachmentState.blendEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vke->vkdev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipe_CLEAR));


#ifdef VKVG_WIRED_DEBUG
	colorBlendState.logicOpEnable = VK_FALSE;
	blendAttachmentState.blendEnable = VK_TRUE;
	colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;

	createInfo.pCode = (uint32_t*)wired_frag_spv;

	createInfo.codeSize = wired_frag_spv_len;
	VK_CHECK_RESULT(vkCreateShaderModule(dev->vke->vkdev, &createInfo, NULL, &modFragWired));

	shaderStages[1].module = modFragWired;

	rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vke->vkdev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipelineLineList));

	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vke->vkdev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipelineWired));

	vkDestroyShaderModule(dev->vke->vkdev, modFragWired, NULL);
#endif

	vkDestroyShaderModule(dev->vke->vkdev, modVert, NULL);
	vkDestroyShaderModule(dev->vke->vkdev, modFrag, NULL);
}

void _device_createDescriptorSetLayout (vkg_device *dev) {

	VkDescriptorSetLayoutBinding dsLayoutBinding =
		{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
	VkDescriptorSetLayoutCreateInfo dsLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
														  .bindingCount = 1,
														  .pBindings = &dsLayoutBinding };
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev->vke->vkdev, &dsLayoutCreateInfo, NULL, &dev->dslFont));
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev->vke->vkdev, &dsLayoutCreateInfo, NULL, &dev->dslSrc));
	dsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev->vke->vkdev, &dsLayoutCreateInfo, NULL, &dev->dslGrad));

	VkPushConstantRange pushConstantRange[] = {
		{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(push_constants)},
		//{VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push_constants)}
	};
	VkDescriptorSetLayout dsls[] = {dev->dslFont,dev->dslSrc,dev->dslGrad};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
															.pushConstantRangeCount = 1,
															.pPushConstantRanges = (VkPushConstantRange*)&pushConstantRange,
															.setLayoutCount = 3,
															.pSetLayouts = dsls };
	VK_CHECK_RESULT(vkCreatePipelineLayout(dev->vke->vkdev, &pipelineLayoutCreateInfo, NULL, &dev->pipelineLayout));
}

void _device_wait_idle (vkg_device *dev) {
	vkDeviceWaitIdle (dev->vke->vkdev);
}
void _device_wait_and_reset_device_fence (vkg_device *dev) {
	vkWaitForFences (dev->vke->vkdev, 1, &dev->fence, VK_TRUE, UINT64_MAX);
	ResetFences (dev->vke->vkdev, 1, &dev->fence);
}

bool _device_try_get_cached_context (vkg_device *dev, vkg_context* pCtx) {
	io_sync(dev);

	if (dev->cachedContextCount) {
		thrd_t curThread = thrd_current ();
		_cached_ctx* prev = NULL;
		_cached_ctx* cur = dev->cachedContextLast;
		while (cur) {
			if (thrd_equal (cur->thread, curThread)) {
				if (prev)
					prev->pNext = cur->pNext;
				else
					dev->cachedContextLast = cur->pNext;

				dev->cachedContextCount--;

				vke_log(VKE_LOG_THREAD,"get cached context: %p, thd:%p cached ctx: %d\n", cur->ctx, (void*)cur->thread, dev->cachedContextCount);

				*pCtx = cur->ctx;
				free (cur);
				io_unsync(dev);
				return true;
			}
			prev = cur;
			cur = cur->pNext;
		}
	}
	*pCtx = NULL;
	io_unsync(dev);
	return false;
}

void _cached_ctx_destroy(_cached_ctx *cur) {
}

void _device_store_context (vkg_context *ctx) {
	VkgDevice dev = ctx->dev;

	mx_sync(dev);

	_cached_ctx* cur;
	mx_init(_cached_ctx, cur);

	cur->ctx	= ctx;
	cur->thread	= thrd_current ();
	cur->pNext	= dev->cachedContextLast;

	dev->cachedContextLast = cur;
	dev->cachedContextCount++;

	vke_log(VKE_LOG_THREAD,"store context: %p, thd:%p cached ctx: %d\n", cur->ctx, (void*)cur->thread, dev->cachedContextCount);
	io_unsync(dev);
}

void _device_submit_cmd (vkg_device *dev, VkCommandBuffer* cmd, VkFence fence) {
	mx_sync(dev);
	vke_cmd_submit (dev->gQueue, cmd, fence);
	io_unsync(dev);
}

bool _device_init_function_pointers (vkg_device *dev) {
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	if (vkGetInstanceProcAddr(dev->vke->app->inst, "vkSetDebugUtilsObjectNameEXT")==VK_NULL_HANDLE){
		vke_log(VKE_LOG_ERR, "vkvg create device failed: 'VK_EXT_debug_utils' has to be loaded for Debug build\n");
		return false;
	}
	vke_device_init_debug_utils (dev->vke);
#endif
	VkDevice   vkdev = dev->vke->vkdev;
	VkInstance inst  = dev->app->inst;
	CmdBindPipeline			= GetVkProcAddress(vkdev, inst, vkCmdBindPipeline);
	CmdBindDescriptorSets	= GetVkProcAddress(vkdev, inst, vkCmdBindDescriptorSets);
	CmdBindIndexBuffer		= GetVkProcAddress(vkdev, inst, vkCmdBindIndexBuffer);
	CmdBindVertexBuffers	= GetVkProcAddress(vkdev, inst, vkCmdBindVertexBuffers);
	CmdDrawIndexed			= GetVkProcAddress(vkdev, inst, vkCmdDrawIndexed);
	CmdDraw					= GetVkProcAddress(vkdev, inst, vkCmdDraw);
	CmdSetStencilCompareMask= GetVkProcAddress(vkdev, inst, vkCmdSetStencilCompareMask);
	CmdSetStencilReference	= GetVkProcAddress(vkdev, inst, vkCmdSetStencilReference);
	CmdSetStencilWriteMask	= GetVkProcAddress(vkdev, inst, vkCmdSetStencilWriteMask);
	CmdBeginRenderPass		= GetVkProcAddress(vkdev, inst, vkCmdBeginRenderPass);
	CmdEndRenderPass		= GetVkProcAddress(vkdev, inst, vkCmdEndRenderPass);
	CmdSetViewport			= GetVkProcAddress(vkdev, inst, vkCmdSetViewport);
	CmdSetScissor			= GetVkProcAddress(vkdev, inst, vkCmdSetScissor);
	CmdPushConstants		= GetVkProcAddress(vkdev, inst, vkCmdPushConstants);
	WaitForFences			= GetVkProcAddress(vkdev, inst, vkWaitForFences);
	ResetFences				= GetVkProcAddress(vkdev, inst, vkResetFences);
	ResetCommandBuffer		= GetVkProcAddress(vkdev, inst, vkResetCommandBuffer);
	return true;
}

void _device_create_empty_texture (vkg_device *dev, VkFormat format, VkImageTiling tiling) {
	//create empty image to bind to context source descriptor when not in use
	dev->emptyImg = vke_image_create(dev->vke,format,16,16,tiling,VKE_MEMORY_USAGE_GPU_ONLY,
									 VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	vke_image_create_descriptor(dev->emptyImg, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	_device_wait_and_reset_device_fence (dev);

	vke_cmd_begin (dev->cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vke_image_set_layout (dev->cmd, dev->emptyImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	vke_cmd_end (dev->cmd);
	_device_submit_cmd (dev, &dev->cmd, dev->fence);
}
void _device_check_best_image_tiling (vkg_device *dev, VkFormat format) {
	VkFlags stencilFormats[] = { VK_FORMAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };
	VkFormatProperties phyStencilProps = { 0 }, phyImgProps = { 0 };

	//check png blit format
	VkFlags pngBlitFormats[] = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM};
	dev->pngStagFormat = VK_FORMAT_UNDEFINED;
	for (int i = 0; i < 2; i++)
	{
		vkGetPhysicalDeviceFormatProperties(dev->vke->phyinfo->gpu, pngBlitFormats[i], &phyImgProps);
		if ((phyImgProps.linearTilingFeatures & VKVG_PNG_WRITE_IMG_REQUIREMENTS) == VKVG_PNG_WRITE_IMG_REQUIREMENTS) {
			dev->pngStagFormat = pngBlitFormats[i];
			dev->pngStagTiling = VK_IMAGE_TILING_LINEAR;
			break;
		} else if ((phyImgProps.optimalTilingFeatures & VKVG_PNG_WRITE_IMG_REQUIREMENTS) == VKVG_PNG_WRITE_IMG_REQUIREMENTS) {
			dev->pngStagFormat = pngBlitFormats[i];
			dev->pngStagTiling = VK_IMAGE_TILING_OPTIMAL;
			break;
		}
	}

	if (dev->pngStagFormat == VK_FORMAT_UNDEFINED)
		vke_log(VKE_LOG_DEBUG, "vkvg create device failed: no suitable image format for png write\n");

	dev->stencilFormat 		= VK_FORMAT_UNDEFINED;
	dev->stencilAspectFlag 	= VK_IMAGE_ASPECT_STENCIL_BIT;
	dev->supportedTiling 	= 0xff;
	
	vkGetPhysicalDeviceFormatProperties(dev->vke->phyinfo->gpu, format, &phyImgProps);
	
	if ((phyImgProps.optimalTilingFeatures & VKVG_SURFACE_IMGS_REQUIREMENTS) == VKVG_SURFACE_IMGS_REQUIREMENTS) {
		for (int i = 0; i < 4; i++)
		{
			vkGetPhysicalDeviceFormatProperties(dev->vke->phyinfo->gpu, stencilFormats[i], &phyStencilProps);
			if (phyStencilProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				dev->stencilFormat = stencilFormats[i];
				if (i > 0)
					dev->stencilAspectFlag |= VK_IMAGE_ASPECT_DEPTH_BIT;
				dev->supportedTiling = VK_IMAGE_TILING_OPTIMAL;
				return;
			}
		}
	}
	if ((phyImgProps.linearTilingFeatures & VKVG_SURFACE_IMGS_REQUIREMENTS) == VKVG_SURFACE_IMGS_REQUIREMENTS) {
		for (int i = 0; i < 4; i++)
		{
			vkGetPhysicalDeviceFormatProperties(dev->vke->phyinfo->gpu, stencilFormats[i], &phyStencilProps);
			if (phyStencilProps.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				dev->stencilFormat = stencilFormats[i];
				if (i > 0)
					dev->stencilAspectFlag |= VK_IMAGE_ASPECT_DEPTH_BIT;
				dev->supportedTiling = VK_IMAGE_TILING_LINEAR;
				return;
			}
		}
	}
	dev->status = VKE_STATUS_INVALID_FORMAT;
	vke_log(VKE_LOG_ERR, "vkvg create device failed: image format not supported: %d\n", format);
}

void _dump_image_format_properties (vkg_device *dev, VkFormat format) {
	/*VkImageFormatProperties imgProps;
	VK_CHECK_RESULT(vkGetPhysicalDeviceImageFormatProperties(dev->vke->phyinfo->gpu,
															 format, VK_IMAGE_TYPE_2D, VKVG_TILING,
															 VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
															 0, &imgProps));
	printf ("tiling			  = %d\n", VKVG_TILING);
	printf ("max extend		  = (%d, %d, %d)\n", imgProps.maxExtent.width, imgProps.maxExtent.height, imgProps.maxExtent.depth);
	printf ("max mip levels	  = %d\n", imgProps.maxMipLevels);
	printf ("max array layers = %d\n", imgProps.maxArrayLayers);
	printf ("sample counts	  = ");
	if (imgProps.sampleCounts & VK_SAMPLE_COUNT_1_BIT)
		printf ("1,");
	if (imgProps.sampleCounts & VK_SAMPLE_COUNT_2_BIT)
		printf ("2,");
	if (imgProps.sampleCounts & VK_SAMPLE_COUNT_4_BIT)
		printf ("4,");
	if (imgProps.sampleCounts & VK_SAMPLE_COUNT_8_BIT)
		printf ("8,");
	if (imgProps.sampleCounts & VK_SAMPLE_COUNT_16_BIT)
		printf ("16,");
	if (imgProps.sampleCounts & VK_SAMPLE_COUNT_32_BIT)
		printf ("32,");
	printf ("\n");
	printf ("max resource size= %lu\n", imgProps.maxResourceSize);
*/

}

#define TRY_LOAD_DEVICE_EXT(ext) {								\
if (vke_phyinfo_try_get_extension_properties(pi, #ext, NULL))	\
	enabledExts[enabledExtsCount++] = #ext;						\
}

void VkgDevice::free(VkgDevice dev) {
	if (dev->cachedContextCount > 0) { /// i cant wait to see what this brings
		_cached_ctx* cur = dev->cachedContextLast;
		while (cur) {
			_release_context_ressources (cur->ctx);
			_cached_ctx* prev = cur;
			cur = cur->pNext;
			free (prev);
		}
	}

	vke_log(VKE_LOG_INFO, "DESTROY Device\n");
	vkDeviceWaitIdle (dev->vke->vkdev);
	vke_image_drop				(dev->emptyImg);
	vkDestroyDescriptorSetLayout	(dev->vke->vkdev, dev->dslGrad,NULL);
	vkDestroyDescriptorSetLayout	(dev->vke->vkdev, dev->dslFont,NULL);
	vkDestroyDescriptorSetLayout	(dev->vke->vkdev, dev->dslSrc, NULL);
#ifndef __APPLE__
	vkDestroyPipeline				(dev->vke->vkdev, dev->pipelinePolyFill, NULL);
#endif
	vkDestroyPipeline				(dev->vke->vkdev, dev->pipelineClipping, NULL);

	vkDestroyPipeline				(dev->vke->vkdev, dev->pipe_OVER,	NULL);
	vkDestroyPipeline				(dev->vke->vkdev, dev->pipe_SUB,		NULL);
	vkDestroyPipeline				(dev->vke->vkdev, dev->pipe_CLEAR,	NULL);

#ifdef VKVG_WIRED_DEBUG
	vkDestroyPipeline				(dev->vke->vkdev, dev->pipelineWired, NULL);
	vkDestroyPipeline				(dev->vke->vkdev, dev->pipelineLineList, NULL);
#endif

	vkDestroyPipelineLayout			(dev->vke->vkdev, dev->pipelineLayout, NULL);
	vkDestroyPipelineCache			(dev->vke->vkdev, dev->pipelineCache, NULL);
	vkDestroyRenderPass				(dev->vke->vkdev, dev->renderPass, NULL);
	vkDestroyRenderPass				(dev->vke->vkdev, dev->renderPass_ClearStencil, NULL);
	vkDestroyRenderPass				(dev->vke->vkdev, dev->renderPass_ClearAll, NULL);

	vkWaitForFences					(dev->vke->vkdev, 1, &dev->fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence					(dev->vke->vkdev, dev->fence,NULL);

	vkFreeCommandBuffers			(dev->vke->vkdev, dev->cmdPool, 1, &dev->cmd);
	vkDestroyCommandPool			(dev->vke->vkdev, dev->cmdPool, NULL);

	vke_queue_drop(dev->gQueue);

	_font_cache_free(dev);
	
	if (dev->vke) {
		VkeApp app = vke_device_app(dev->vke);
		io_drop(dev->vke);
		io_drop(app);
	}
}

/// experiment with calling this. // todo
void VkgDevice::set_context_cache_size (VkgDevice dev, uint32_t maxCount) {
	if (maxCount == dev->cachedContextMaxCount)
		return;

	dev->cachedContextMaxCount = maxCount;

	_cached_ctx* cur = dev->cachedContextLast;
	while (cur && dev->cachedContextCount > dev->cachedContextMaxCount) {
		_release_context_ressources (cur->ctx);
		_cached_ctx* prev = cur;
		cur = cur->pNext;
		free (prev);
		dev->cachedContextCount--;
	}
	dev->cachedContextLast = cur;
}

bool _get_dev_extension_is_supported (VkExtensionProperties* pExtensionProperties, uint32_t extensionCount, const char* name) {
	for (uint32_t i=0; i<extensionCount; i++) {
		if (strcmp(name, pExtensionProperties[i].extensionName)==0)
			return true;
	}
	return false;
}
#define _CHECK_DEV_EXT(ext) {					\
	if (_get_dev_extension_is_supported(pExtensionProperties, extensionCount, #ext)){\
		if (pExtensions)							\
			pExtensions[*pExtCount] = #ext;			\
		(*pExtCount)++;								\
	}\
}

VkeStatus vkg_get_required_device_extensions (VkPhysicalDevice gpu, const char** pExtensions, uint32_t* pExtCount) {
	VkExtensionProperties* pExtensionProperties;
	uint32_t extensionCount;

	*pExtCount = 0;

	VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(gpu, NULL, &extensionCount, NULL));
	pExtensionProperties = (VkExtensionProperties*)malloc(extensionCount * sizeof(VkExtensionProperties));
	VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(gpu, NULL, &extensionCount, pExtensionProperties));

	//https://vulkan.lunarg.com/doc/view/1.2.162.0/mac/1.2-extensions/vkspec.html#VK_KHR_portability_subset
	_CHECK_DEV_EXT(VK_KHR_portability_subset);
	VkPhysicalDeviceFeatures2 phyFeat2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

#ifdef VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT
	//ensure feature is implemented by driver.
	VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutSupport = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES};
	phyFeat2.pNext = &scalarBlockLayoutSupport;
#endif

#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreSupport = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
	timelineSemaphoreSupport.pNext = phyFeat2.pNext;
	phyFeat2.pNext = &timelineSemaphoreSupport;
#endif

	vkGetPhysicalDeviceFeatures2(gpu, &phyFeat2);

#ifdef VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT
	if (!scalarBlockLayoutSupport.scalarBlockLayout) {
		vke_log(VKE_LOG_ERR, "CREATE Device failed, vkvg compiled with VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT and feature is not implemented for physical device.\n");
		return VKE_STATUS_DEVICE_ERROR;
	}
	_CHECK_DEV_EXT(VK_EXT_scalar_block_layout)
#endif
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	if (!timelineSemaphoreSupport.timelineSemaphore) {
		vke_log(VKE_LOG_ERR, "CREATE Device failed, VK_SEMAPHORE_TYPE_TIMELINE not supported.\n");
		return VKE_STATUS_DEVICE_ERROR;
	}
	_CHECK_DEV_EXT(VK_KHR_timeline_semaphore)
#endif

	return VKE_STATUS_SUCCESS;
}

//enabledFeature12 is guarantied to be the first in pNext chain
const void* vkg_get_device_requirements (VkPhysicalDeviceFeatures* pEnabledFeatures) {

	pEnabledFeatures->fillModeNonSolid	= VK_TRUE;
	pEnabledFeatures->sampleRateShading	= VK_TRUE;
	pEnabledFeatures->logicOp			= VK_TRUE;

	void* pNext = NULL;

#ifdef VK_VERSION_1_2
	static VkPhysicalDeviceVulkan12Features enabledFeatures12 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
#ifdef VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT
		,.scalarBlockLayout = VK_TRUE
#endif
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
		,.timelineSemaphore = VK_TRUE
#endif
	};
	enabledFeatures12.pNext = pNext;
	pNext = &enabledFeatures12;
#else
#ifdef VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT
	static VkPhysicalDeviceScalarBlockLayoutFeaturesEXT scalarBlockFeat = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT
		,.scalarBlockLayout = VK_TRUE
	};
	scalarBlockFeat.pNext = pNext;
	pNext = &scalarBlockFeat;
#endif
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	static VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timelineSemaFeat = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR
		,.timelineSemaphore = VK_TRUE
	};
	timelineSemaFeat.pNext = pNext;
	pNext = &timelineSemaFeat;
#endif
#endif

	return pNext;
}

VkgDevice::VkgDevice(VkeDevice vke, VkSampleCountFlagBits samples, bool defer_resolve) {
	/// construct with method and arguments; also sets ref count and mutex
	data->vke       		 = vke;
	data->samples   		 = vke_max_samples(samples);
	data->deferredResolve = (data->samples == VK_SAMPLE_COUNT_1_BIT) ? false : defer_resolve;
	data->cachedContextMaxCount = VKVG_MAX_CACHED_CONTEXT_COUNT;

	int p_index = vke_device_present_index(vke);
	vke_log(VKE_LOG_INFO, "vkg_device_with_vke: qFam = %d; qIdx = %d\n", p_index, 0);

#if VKVG_DBG_STATS
	data->debug_stats = (vkg_debug_stats) {0};
#endif
	VkFormat format = FB_COLOR_FORMAT;
	_device_check_best_image_tiling(data, format);
	if (data->status != VKE_STATUS_SUCCESS)
		return NULL;

	if (!_device_init_function_pointers (data)){
		data->status = VKE_STATUS_NULL_POINTER;
		return NULL;
	}

	data->gQueue = vke_queue_create (data->vke, p_index, 0);
	//mtx_init (&data->gQMutex, mtx_plain);

	VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice = vke->phyinfo->gpu,
		.device 		= vke->vkdev
	};
	vmaCreateAllocator(&allocatorInfo, (VmaAllocator*)&data->vke->allocator);

	data->cmdPool	= vke_cmd_pool_create		(data->vke, data->gQueue->familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	data->cmd		= vke_cmd_buff_create		(data->vke, data->cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	data->fence		= vke_fence_create_signaled (data->vke);

	_device_create_pipeline_cache		(data);
	_fonts_cache_create					(data);

	if (data->deferredResolve || data->samples == VK_SAMPLE_COUNT_1_BIT) {
		data->renderPass				= _device_createRenderPassNoResolve (data, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD);
		data->renderPass_ClearStencil	= _device_createRenderPassNoResolve (data, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR);
		data->renderPass_ClearAll		= _device_createRenderPassNoResolve (data, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);
	} else {
		data->renderPass				= _device_createRenderPassMS (data, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD);
		data->renderPass_ClearStencil	= _device_createRenderPassMS (data, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR);
		data->renderPass_ClearAll		= _device_createRenderPassMS (data, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);
	}
	_device_createDescriptorSetLayout	(data);
	_device_setupPipelines				(data);
	_device_create_empty_texture		(data, format, data->supportedTiling);

#ifdef DEBUG

	#if defined(__linux__) && defined(__GLIBC__)
		_linux_register_error_handler ();
	#endif

	#ifdef VKVG_DBG_UTILS
		vke.set_object_name(VK_OBJECT_TYPE_COMMAND_POOL, 		   (uint64_t)data->cmdPool, 						"Device Cmd Pool");
		vke.set_object_name(VK_OBJECT_TYPE_COMMAND_BUFFER, 	   	   (uint64_t)data->cmd, 							"Device Cmd Buff");
		vke.set_object_name(VK_OBJECT_TYPE_FENCE, 				   (uint64_t)data->fence, 						"Device Fence");
		vke.set_object_name(VK_OBJECT_TYPE_RENDER_PASS, 		   (uint64_t)data->renderPass, 					"RP load img/stencil");
		vke.set_object_name(VK_OBJECT_TYPE_RENDER_PASS, 		   (uint64_t)data->renderPass_ClearStencil, 		"RP clear stencil");
		vke.set_object_name(VK_OBJECT_TYPE_RENDER_PASS, 		   (uint64_t)data->renderPass_ClearAll, 			"RP clear all");
		vke.set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,  (uint64_t)data->dslSrc, 						"DSLayout SOURCE");
		vke.set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,  (uint64_t)data->dslFont, 						"DSLayout FONT");
		vke.set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,  (uint64_t)data->dslGrad, 						"DSLayout GRADIENT");
		vke.set_object_name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, 	   (uint64_t)data->pipelineLayout, 				"PLLayout data");

		#ifndef __APPLE__
			/// ??
			vke.set_object_name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)data->pipelinePolyFill, "PL Poly fill");
		#endif

		vke.set_object_name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)data->pipelineClipping, "PL Clipping");
		vke.set_object_name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)data->pipe_OVER, "PL draw Over");
		vke.set_object_name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)data->pipe_SUB, "PL draw Substract");
		vke.set_object_name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)data->pipe_CLEAR, "PL draw Clear");
		data->emptyImg.set_name("empty IMG");
		vke.set_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vke_image_get_view(data->emptyImg), "empty IMG VIEW");
		vke.set_object_name(VK_OBJECT_TYPE_SAMPLER, (uint64_t)vke_image_get_sampler(data->emptyImg), "empty IMG SAMPLER");
	#endif
#endif
	data->status = VKE_STATUS_SUCCESS;
	return data;
}

VkeStatus VkgDevice::status() {
	return data->status;
}

#if VKVG_DBG_STATS
vkg_debug_stats VkgDevice::get_stats() {
	return data->debug_stats;
}

vkg_debug_stats VkgDevice::reset_stats() {
	data->debug_stats = (vkg_debug_stats) {0};
}
#endif
