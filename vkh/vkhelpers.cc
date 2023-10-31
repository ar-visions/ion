/*
 * Copyright (c) 2018-2022 Jean-Philippe Bruyère <jp_bruyere@hotmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <vkh/vkh_queue.h>
#include <vkh/vkh_device.h>

#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

VkFence vkh_fence_create (VkhDevice vkh) {
	VkFence fence;
	VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceInfo.pNext = NULL;
	fenceInfo.flags = 0;
	VK_CHECK_RESULT(vkCreateFence(vkh->device, &fenceInfo, NULL, &fence));
	return fence;
}
VkFence vkh_fence_create_signaled (VkhDevice vkh) {
	VkFence fence;
	VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceInfo.pNext = NULL;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VK_CHECK_RESULT(vkCreateFence(vkh->device, &fenceInfo, NULL, &fence));
	return fence;
}
VkSemaphore vkh_semaphore_create (VkhDevice vkh) {
	VkSemaphore semaphore;
	VkSemaphoreCreateInfo info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VK_CHECK_RESULT(vkCreateSemaphore(vkh->device, &info, NULL, &semaphore));
	return semaphore;
}
VkSemaphore vkh_timeline_create (VkhDevice vkh, uint64_t initialValue) {
	VkSemaphore semaphore;
	VkSemaphoreTypeCreateInfo timelineInfo { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
	timelineInfo.pNext = NULL;
	timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timelineInfo.initialValue = initialValue;

	VkSemaphoreCreateInfo info = { };
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = &timelineInfo;
	info.flags = 0;
	VK_CHECK_RESULT(vkCreateSemaphore(vkh->device, &info, NULL, &semaphore));
	return semaphore;
}

VkResult vkh_timeline_wait (VkhDevice vkh, VkSemaphore timeline, const uint64_t wait) {
	VkSemaphoreWaitInfo waitInfo { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	waitInfo.semaphoreCount = 1;
	waitInfo.pSemaphores	= &timeline;
	waitInfo.pValues		= &wait;

	return vkWaitSemaphores(vkh->device, &waitInfo, UINT64_MAX);
}
void vkh_cmd_submit_timelined (VkhQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore timeline, const uint64_t wait, const uint64_t signal) {
	static VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkTimelineSemaphoreSubmitInfo timelineInfo { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	timelineInfo.waitSemaphoreValueCount = 1;
	timelineInfo.pWaitSemaphoreValues = &wait;
	timelineInfo.signalSemaphoreValueCount = 1;
	timelineInfo.pSignalSemaphoreValues = &signal;

	VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.pNext = &timelineInfo;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &timeline;
	submitInfo.signalSemaphoreCount  = 1;
	submitInfo.pSignalSemaphores = &timeline;
	submitInfo.pWaitDstStageMask = &stageFlags,
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = pCmdBuff;

	VK_CHECK_RESULT(vkQueueSubmit(queue->queue, 1, &submitInfo, VK_NULL_HANDLE));
}
void vkh_cmd_submit_timelined2 (VkhQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore timelines[2], const uint64_t waits[2], const uint64_t signals[2]) {
	static VkPipelineStageFlags stageFlags[2] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
	VkTimelineSemaphoreSubmitInfo timelineInfo { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	timelineInfo.pNext = NULL;
	timelineInfo.waitSemaphoreValueCount = 2;
	timelineInfo.pWaitSemaphoreValues = waits;
	timelineInfo.signalSemaphoreValueCount = 2;
	timelineInfo.pSignalSemaphoreValues = signals;

	VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.pNext = &timelineInfo;
	submitInfo.waitSemaphoreCount = 2;
	submitInfo.pWaitSemaphores = timelines;
	submitInfo.signalSemaphoreCount  = 2;
	submitInfo.pSignalSemaphores = timelines;
	submitInfo.pWaitDstStageMask = stageFlags,
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = pCmdBuff;

	VK_CHECK_RESULT(vkQueueSubmit(queue->queue, 1, &submitInfo, VK_NULL_HANDLE));
}
VkEvent vkh_event_create (VkhDevice vkh) {
	VkEvent evt;
	VkEventCreateInfo evtInfo { VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
	VK_CHECK_RESULT(vkCreateEvent (vkh->device, &evtInfo, NULL, &evt));
	return evt;
}
VkCommandPool vkh_cmd_pool_create (VkhDevice vkh, uint32_t qFamIndex, VkCommandPoolCreateFlags flags){
	VkCommandPool cmdPool;
	VkCommandPoolCreateInfo cmd_pool_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cmd_pool_info.flags = flags;
	cmd_pool_info.queueFamilyIndex = qFamIndex;
	VK_CHECK_RESULT (vkCreateCommandPool(vkh->device, &cmd_pool_info, NULL, &cmdPool));
	return cmdPool;
}
VkCommandBuffer vkh_cmd_buff_create (VkhDevice vkh, VkCommandPool cmdPool, VkCommandBufferLevel level){
	VkCommandBuffer cmdBuff;
	VkCommandBufferAllocateInfo cmd { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmd.commandPool = cmdPool;
	cmd.level = level;
	cmd.commandBufferCount = 1;
	VK_CHECK_RESULT (vkAllocateCommandBuffers (vkh->device, &cmd, &cmdBuff));
	return cmdBuff;
}
void vkh_cmd_buffs_create (VkhDevice vkh, VkCommandPool cmdPool, VkCommandBufferLevel level, uint32_t count, VkCommandBuffer* cmdBuffs){
	VkCommandBufferAllocateInfo cmd { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmd.commandPool = cmdPool;
	cmd.level = level;
	cmd.commandBufferCount = count;
	VK_CHECK_RESULT (vkAllocateCommandBuffers (vkh->device, &cmd, cmdBuffs));
}
void vkh_cmd_begin(VkCommandBuffer cmdBuff, VkCommandBufferUsageFlags flags) {
	VkCommandBufferBeginInfo cbinfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	cbinfo.flags = flags;

	VK_CHECK_RESULT (vkBeginCommandBuffer (cmdBuff, &cbinfo));
}
void vkh_cmd_end(VkCommandBuffer cmdBuff){
	VK_CHECK_RESULT (vkEndCommandBuffer (cmdBuff));
}
void vkh_cmd_submit(VkhQueue queue, VkCommandBuffer *pCmdBuff, VkFence fence){
	VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.pWaitDstStageMask = &stageFlags;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = pCmdBuff;
	VK_CHECK_RESULT(vkQueueSubmit(queue->queue, 1, &submit_info, fence));
}
void vkh_cmd_submit_with_semaphores(VkhQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore waitSemaphore,
									VkSemaphore signalSemaphore, VkFence fence){

	VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.pWaitDstStageMask = &stageFlags;
	si.commandBufferCount = 1;
	si.pCommandBuffers = pCmdBuff;

	if (waitSemaphore != VK_NULL_HANDLE){
		si.waitSemaphoreCount = 1;
		si.pWaitSemaphores = &waitSemaphore;
	}
	if (signalSemaphore != VK_NULL_HANDLE){
		si.signalSemaphoreCount = 1;
		si.pSignalSemaphores= &signalSemaphore;
	}

	VK_CHECK_RESULT(vkQueueSubmit(queue->queue, 1, &si, fence));
}


void set_image_layout(VkCommandBuffer cmdBuff, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout,
					  VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
	VkImageSubresourceRange subres = {aspectMask,0,1,0,1};
	set_image_layout_subres(cmdBuff, image, subres, old_image_layout, new_image_layout, src_stages, dest_stages);
}

void set_image_layout_subres(VkCommandBuffer cmdBuff, VkImage image, VkImageSubresourceRange subresourceRange,
							 VkImageLayout old_image_layout, VkImageLayout new_image_layout,
							 VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
	VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.oldLayout = old_image_layout;
	barrier.newLayout = new_image_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = subresourceRange;

	switch (old_image_layout) {
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;

	default:
		break;
	}

	switch (new_image_layout) {
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	default:
		break;
	}

	vkCmdPipelineBarrier(cmdBuff, src_stages, dest_stages, 0, 0, NULL, 0, NULL, 1, &barrier);
}

bool vkh_memory_type_from_properties(VkPhysicalDeviceMemoryProperties* memory_properties, uint32_t typeBits, VkhMemoryUsage memUsage, uint32_t *typeIndex) {
	VkMemoryPropertyFlagBits memFlags = (VkMemoryPropertyFlagBits)0;
	switch(memUsage) {
	case VKH_MEMORY_USAGE_GPU_ONLY:
		memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		break;
	case VKH_MEMORY_USAGE_CPU_ONLY:
		memFlags = (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		break;
	case VKH_MEMORY_USAGE_CPU_TO_GPU:
		memFlags = (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		break;
	case VKH_MEMORY_USAGE_GPU_TO_CPU:
		memFlags = (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		break;
	case VKH_MEMORY_USAGE_CPU_COPY:
		memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		break;
	case VKH_MEMORY_USAGE_GPU_LAZILY_ALLOCATED:
		memFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
		break;
	}

	// Search memtypes to find first index with those properties
	for (uint32_t i = 0; i < memory_properties->memoryTypeCount; i++) { /// type count is 0. fix this.
		if (CHECK_BIT(typeBits, i)) {
			// Type is available, does it match user properties?
			if ((memory_properties->memoryTypes[i].propertyFlags & memFlags) == memFlags) {
				*typeIndex = i;
				return true;
			}
		}
		typeBits >>= 1;
	}
	// No memory types matched, return failure
	return false;
}

VkShaderModule vkh_load_module(VkDevice dev, const char* path){
	VkShaderModule module;
	size_t filelength;
	uint32_t* pCode = (uint32_t*)read_spv(path, &filelength);
	VkShaderModuleCreateInfo createInfo { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	createInfo.codeSize = filelength;
	createInfo.pCode = pCode;

	VK_CHECK_RESULT(vkCreateShaderModule(dev, &createInfo, NULL, &module));
	free (pCode);
	//assert(module != VK_NULL_HANDLE);
	return module;
}

char *read_spv(const char *filename, size_t *psize) {
	size_t size;
	size_t retval;
	void *shader_code;

#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
	filename =[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent: @(filename)].UTF8String;
#endif

	FILE *fp = fopen(filename, "rb");
	if (!fp)
		return NULL;

	fseek(fp, 0L, SEEK_END);
	size = (size_t)ftell(fp);

	fseek(fp, 0L, SEEK_SET);

	shader_code = malloc(size);
	retval = fread(shader_code, size, 1, fp);
	assert(retval == 1);

	*psize = size;

	fclose(fp);
	return (char*)shader_code;
}

// Read file into array of bytes, and cast to uint32_t*, then return.
// The data has been padded, so that it fits into an array uint32_t.
uint32_t* readFile(uint32_t* length, const char* filename) {

	FILE* fp = fopen(filename, "rb");
	if (fp == 0) {
		printf("Could not find or open file: %s\n", filename);
	}

	// get file size.
	fseek(fp, 0, SEEK_END);
	long filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	long filesizepadded = (long)(ceil(filesize / 4.0)) * 4;

	// read file contents.
	char *str = (char*)malloc(filesizepadded*sizeof(char));
	fread(str, filesize, sizeof(char), fp);
	fclose(fp);

	// data padding.
	for (int i = filesize; i < filesizepadded; i++)
		str[i] = 0;

	*length = filesizepadded;
	return (uint32_t *)str;
}

void dumpLayerExts () {
	printf ("Layers:\n");
	uint32_t instance_layer_count;
	VK_CHECK_RESULT (vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL));
	if (instance_layer_count == 0)
		return;
	VkLayerProperties* vk_props = (VkLayerProperties*)malloc (instance_layer_count * sizeof(VkLayerProperties));
	VK_CHECK_RESULT (vkEnumerateInstanceLayerProperties(&instance_layer_count, vk_props));

	for (uint32_t i = 0; i < instance_layer_count; i++) {
		printf ("\t%s, %s\n", vk_props[i].layerName, vk_props[i].description);
		/*		  res = init_global_extension_properties(layer_props);
		if (res) return res;
		info.instance_layer_properties.push_back(layer_props);*/
	}
	free(vk_props);
}

static VkExtensionProperties* instExtProps;
static uint32_t instExtCount;
bool vkh_instance_extension_supported (const char* instanceName) {
    for (uint32_t i=0; i<instExtCount; i++) {
        if (!strcmp(instExtProps[i].extensionName, instanceName))
            return true;
    }
    return false;
}
void vkh_instance_extensions_check_init () {
    VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(NULL, &instExtCount, NULL));
    instExtProps =(VkExtensionProperties*)malloc(instExtCount * sizeof(VkExtensionProperties));
    VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(NULL, &instExtCount, instExtProps));
}
void vkh_instance_extensions_check_release () {
    free (instExtProps);
}

static VkLayerProperties* instLayerProps;
static uint32_t instance_layer_count;
bool vkh_layer_is_present (const char* layerName) {
    for (uint32_t i=0; i<instance_layer_count; i++) {
        if (!strcmp(instLayerProps[i].layerName, layerName))
            return true;
    }
    return false;
}
void vkh_layers_check_init () {
    VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL));
    instLayerProps =(VkLayerProperties*)malloc(instance_layer_count * sizeof(VkLayerProperties));
    VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&instance_layer_count, instLayerProps));
}
void vkh_layers_check_release () {
    free (instLayerProps);
}
