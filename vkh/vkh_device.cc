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
#include <vkh/vkh_device.h>
#include <vkh/vkh_phyinfo.h>
#include <vkh/vkh.h>
#include "string.h"


static PFN_vkSetDebugUtilsObjectNameEXT		SetDebugUtilsObjectNameEXT;
static PFN_vkQueueBeginDebugUtilsLabelEXT	QueueBeginDebugUtilsLabelEXT;
static PFN_vkQueueEndDebugUtilsLabelEXT		QueueEndDebugUtilsLabelEXT;
static PFN_vkCmdBeginDebugUtilsLabelEXT		CmdBeginDebugUtilsLabelEXT;
static PFN_vkCmdEndDebugUtilsLabelEXT		CmdEndDebugUtilsLabelEXT;
static PFN_vkCmdInsertDebugUtilsLabelEXT	CmdInsertDebugUtilsLabelEXT;


#define _CHECK_INST_EXT(ext)\
if (vkh_instance_extension_supported(#ext)) {	\
	if (pExtensions)							\
	   pExtensions[*pExtCount] = #ext;			\
	(*pExtCount)++;								\
}
void vkh_get_required_instance_extensions (const char** pExtensions, uint32_t* pExtCount) {
	*pExtCount = 0;

	vkh_instance_extensions_check_init ();

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	_CHECK_INST_EXT(VK_EXT_debug_utils)
#endif
	_CHECK_INST_EXT(VK_KHR_get_physical_device_properties2)

	vkh_instance_extensions_check_release();
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

bool vkh_get_required_device_extensions (VkPhysicalDevice phy, const char** pExtensions, uint32_t* pExtCount) {
	VkExtensionProperties* pExtensionProperties;
	uint32_t extensionCount;

	*pExtCount = 0;

	VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(phy, NULL, &extensionCount, NULL));
	pExtensionProperties = (VkExtensionProperties*)malloc(extensionCount * sizeof(VkExtensionProperties));
	VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(phy, NULL, &extensionCount, pExtensionProperties));

	//https://vulkan.lunarg.com/doc/view/1.2.162.0/mac/1.2-extensions/vkspec.html#VK_KHR_portability_subset
	_CHECK_DEV_EXT(VK_KHR_portability_subset);
	VkPhysicalDeviceFeatures2 phyFeat2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

	vkGetPhysicalDeviceFeatures2(phy, &phyFeat2);
	return true;
}

//enabledFeature12 is guaranteed to be the first in pNext chain
const void* vkh_get_device_requirements (VkPhysicalDevice phy, VkPhysicalDeviceFeatures* pEnabledFeatures) {

	VkPhysicalDeviceFeatures supported;
	vkGetPhysicalDeviceFeatures(phy, &supported);

	pEnabledFeatures->fillModeNonSolid	= supported.fillModeNonSolid;  // VK_TRUE;
	pEnabledFeatures->sampleRateShading	= supported.sampleRateShading; // VK_TRUE;
	pEnabledFeatures->logicOp			= supported.logicOp; 		   // VK_TRUE;

	void* pNext = NULL;

#ifdef VK_VERSION_1_2
	static VkPhysicalDeviceVulkan12Features enabledFeatures12 = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
	};
	enabledFeatures12.pNext = pNext;
	pNext = &enabledFeatures12;
#endif

	return pNext;
}

VkhDevice vkh_device_grab(VkhDevice vkh) {
	if (vkh)
		vkh->refs++;
	return vkh;
}

VkhDevice vkh_device_import (VkEngine e) {
	VkhDevice vkh = (vkh_device_t*)calloc(1,sizeof(vkh_device_t));
	vkh->refs = 1;
	vkh->device = e->vk_device->device;
	vkh->e = e;

//vkGetPhysicalDeviceMemoryProperties (phy, &vkh->e->memory_properties);
//#ifdef VKH_USE_VMA
//	VmaAllocatorCreateInfo allocatorInfo = {
//		.physicalDevice = phy,
//		.device = vkDev
//	};
//	vmaCreateAllocator(&allocatorInfo, &vkh->allocator);
//#else
//#endif

	return vkh;
}

VkDevice vkh_device_get_vkdev (VkhDevice vkh) {
	return vkh->e->vk_device->device;
}

VkPhysicalDevice vkh_device_get_phy (VkhDevice vkh) {
	return vkh->e->vk_gpu->phys;
}

VkEngine vkh_device_get_engine (VkhDevice vkh) {
	return vkh->e;
}
/**
 * @brief get instance proc addresses for debug utils (name, label,...)
 * @param vkh device
 */
void vkh_device_init_debug_utils (VkhDevice vkh) {
	SetDebugUtilsObjectNameEXT		= (PFN_vkSetDebugUtilsObjectNameEXT)	vkGetInstanceProcAddr(vkh->e->instance, "vkSetDebugUtilsObjectNameEXT");
	QueueBeginDebugUtilsLabelEXT	= (PFN_vkQueueBeginDebugUtilsLabelEXT)	vkGetInstanceProcAddr(vkh->e->instance, "vkQueueBeginDebugUtilsLabelEXT");
	QueueEndDebugUtilsLabelEXT		= (PFN_vkQueueEndDebugUtilsLabelEXT)	vkGetInstanceProcAddr(vkh->e->instance, "vkQueueEndDebugUtilsLabelEXT");
	CmdBeginDebugUtilsLabelEXT		= (PFN_vkCmdBeginDebugUtilsLabelEXT)	vkGetInstanceProcAddr(vkh->e->instance, "vkCmdBeginDebugUtilsLabelEXT");
	CmdEndDebugUtilsLabelEXT		= (PFN_vkCmdEndDebugUtilsLabelEXT)		vkGetInstanceProcAddr(vkh->e->instance, "vkCmdEndDebugUtilsLabelEXT");
	CmdInsertDebugUtilsLabelEXT		= (PFN_vkCmdInsertDebugUtilsLabelEXT)	vkGetInstanceProcAddr(vkh->e->instance, "vkCmdInsertDebugUtilsLabelEXT");
}
VkSampler vkh_device_create_sampler (VkhDevice vkh, VkFilter magFilter, VkFilter minFilter,
							   VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode){
	VkSampler 		    sampler 		  = VK_NULL_HANDLE;
	VkSamplerCreateInfo samplerCreateInfo = { };

	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter	= magFilter;
	samplerCreateInfo.minFilter	= minFilter;
	samplerCreateInfo.mipmapMode   = mipmapMode;
	samplerCreateInfo.addressModeU = addressMode;
	samplerCreateInfo.addressModeV = addressMode;
	samplerCreateInfo.addressModeW = addressMode;
	samplerCreateInfo.maxAnisotropy= 1.0;

	VK_CHECK_RESULT(vkCreateSampler(vkh->device, &samplerCreateInfo, NULL, &sampler));
	return sampler;
}
void vkh_device_destroy_sampler (VkhDevice vkh, VkSampler sampler) {
	vkDestroySampler (vkh->device, sampler, NULL);
}
void vkh_device_drop (VkhDevice vkh) {
	if (vkh && --vkh->refs == 0) {
// VkEngine does this:
//		vmaDestroyAllocator (vkh->e->allocator);
		free (vkh);
	}
}

void vkh_device_set_object_name (VkhDevice vkh, VkObjectType objectType, uint64_t handle, const char* name){
	const VkDebugUtilsObjectNameInfoEXT info = {
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, 0, objectType, handle, name
	};
	SetDebugUtilsObjectNameEXT (vkh->device, &info);
}
void vkh_cmd_label_start (VkCommandBuffer cmd, const char* name, const float color[4]) {
	const VkDebugUtilsLabelEXT info = {
		VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, 0, name
	};
	memcpy ((void*)info.color, (void*)color, 4 * sizeof(float));
	CmdBeginDebugUtilsLabelEXT (cmd, &info);
}
void vkh_cmd_label_insert (VkCommandBuffer cmd, const char* name, const float color[4]) {
	const VkDebugUtilsLabelEXT info = {
		VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		0,
		name
	};
	memcpy ((void*)info.color, (void*)color, 4 * sizeof(float));
	CmdInsertDebugUtilsLabelEXT (cmd, &info);
}
void vkh_cmd_label_end (VkCommandBuffer cmd) {
	CmdEndDebugUtilsLabelEXT (cmd);
}
