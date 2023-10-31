/*
 * Copyright (c) 2018-2021 Jean-Philippe Bruyère <jp_bruyere@hotmail.com>
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

#include <vk/vk.hpp>
#include "vkh/vkh.h"
#include "vkh/vkengine.h"
#include "vkh/vkh_phyinfo.h"
#include "vkh/vkh_presenter.h"
#include "vkh/vkh_image.h"
#include "vkh/vkh_device.h"

/// will merge vkengine & vk next
#include <GLFW/glfw3.h>

//#include "vkg/vkvg.h"

VkBool32 debugUtilsMessengerCallback (
	VkDebugUtilsMessageSeverityFlagBitsEXT			 messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT					 messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT*		 pCallbackData,
	void*											 pUserData) {

	switch (messageSeverity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		printf (KYEL);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		printf (KRED);
		break;
	default:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		printf (KGRN);
		break;
	}
	switch (messageTypes) {
	case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
		printf ("GEN: ");
		break;
	case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
		printf ("VAL: ");
		break;
	case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
		printf ("PRF: ");
		break;
	}

	printf (KNRM);
	printf ("%s\n", pCallbackData->pMessage);


	fflush(stdout);
	return VK_FALSE;
}

VkInstance vkengine_get_inst (VkEngine e) {
	return e->vk->inst();
}

VkhPhyInfo* vkengine_get_phyinfos (VkEngine e, uint32_t* count, VkSurfaceKHR surface) {
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices (e->vk->inst(), count, NULL));
	VkPhysicalDevice* phyDevices = (VkPhysicalDevice*)malloc((*count) * sizeof(VkPhysicalDevice));
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices (e->vk->inst(), count, phyDevices));
	VkhPhyInfo* infos = (VkhPhyInfo*)malloc((*count) * sizeof(VkhPhyInfo));

	for (uint32_t i=0; i<(*count); i++)
		infos[i] = vkh_phyinfo_create (phyDevices[i], surface);

	free (phyDevices);
	return infos;
}

void vkengine_free_phyinfos (uint32_t count, VkhPhyInfo* infos) {
	for (uint32_t i=0; i<count; i++)
		vkh_phyinfo_drop (infos[i]);
	free (infos);
}

static void glfw_error_callback(int error, const char *description) {
	fprintf(stderr, "vkengine: GLFW error %d: %s\n", error, description);
}

VkSampleCountFlagBits getMaxUsableSampleCount(VkSampleCountFlags counts)
{
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
	return VK_SAMPLE_COUNT_1_BIT;
}

void vkengine_dump_Infos (VkEngine e){
	printf("max samples = %d\n", getMaxUsableSampleCount(e->gpu_props.limits.framebufferColorSampleCounts));
	printf("max tex2d size = %d\n", e->gpu_props.limits.maxImageDimension2D);
	printf("max tex array layers = %d\n", e->gpu_props.limits.maxImageArrayLayers);
	printf("max mem alloc count = %d\n", e->gpu_props.limits.maxMemoryAllocationCount);

	for (uint32_t i = 0; i < e->memory_properties.memoryHeapCount; i++) {
		printf("Mem Heap %d\n", i);
		printf("\tflags= %d\n", e->memory_properties.memoryHeaps[i].flags);
		printf("\tsize = %lu Mo\n", (unsigned long)e->memory_properties.memoryHeaps[i].size/ (uint32_t)(1024*1024));
	}
	for (uint32_t i = 0; i < e->memory_properties.memoryTypeCount; i++) {
		printf("Mem type %d\n", i);
		printf("\theap %d: ", e->memory_properties.memoryTypes[i].heapIndex);
		if (e->memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			printf("VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|");
		if (e->memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			printf("VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|");
		if (e->memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			printf("VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|");
		if (e->memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
			printf("VK_MEMORY_PROPERTY_HOST_CACHED_BIT|");
		if (e->memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
			printf("VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT|");
		printf("\n");
	}
}

void vkengine_dump_available_layers () {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);

	VkLayerProperties* availableLayers = (VkLayerProperties*)malloc(layerCount*sizeof(VkLayerProperties));
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

	printf("Available Layers:\n");
	printf("-----------------\n");
	for (uint32_t i=0; i<layerCount; i++) {
		 printf ("\t - %s\n", availableLayers[i].layerName);
	}
	printf("-----------------\n\n");
	free (availableLayers);
}

bool vkengine_try_get_phyinfo (VkhPhyInfo* phys, uint32_t phyCount, VkPhysicalDeviceType gpuType, VkhPhyInfo* phy) {
	for (uint32_t i=0; i<phyCount; i++){
		if (phys[i]->properties.deviceType == gpuType) {
			 *phy = phys[i];
			 return true;
		}
	}
	return false;
}

bool instance_extension_supported (VkExtensionProperties* instanceExtProps, uint32_t extCount, const char* instanceName) {
	for (uint32_t i=0; i<extCount; i++) {
		if (!strcmp(instanceExtProps[i].extensionName, instanceName))
			return true;
	}
	return false;
}

static VkEngine singleton;

void glfw_key(GLFWwindow* window, int key, int scancode, int action, int mods) {
	vk_engine_t *e = (vk_engine_t*)glfwGetWindowUserPointer(window);
	e->fn_key(e->user, key, scancode, action, mods);
}

void glfw_button(GLFWwindow* window, int button, int action, int mods) {
	vk_engine_t *e = (vk_engine_t*)glfwGetWindowUserPointer(window);
	e->fn_button(e->user, button, action, mods);
}

void glfw_move(GLFWwindow* window, double x, double y) {
	vk_engine_t *e = (vk_engine_t*)glfwGetWindowUserPointer(window);
	e->fn_move(e->user, x, y);
}

void glfw_scroll(GLFWwindow* window, double x, double y) {
	vk_engine_t *e = (vk_engine_t*)glfwGetWindowUserPointer(window);
	e->fn_scroll(e->user, x, y);
}

void glfw_char(GLFWwindow* window, uint32_t code) {
	vk_engine_t *e = (vk_engine_t*)glfwGetWindowUserPointer(window);
	e->fn_char(e->user, code);
}

VkEngine vkengine_create (
	uint32_t version_major, uint32_t version_minor, const char* app_name,
	VkPhysicalDeviceType preferedGPU, VkPresentModeKHR presentMode, VkSampleCountFlagBits max_samples,
	uint32_t width, uint32_t height, int dpi_index, ion::mx &user)
{

	if (singleton)
		return singleton;
	
	glfwSetErrorCallback(glfw_error_callback);

	if (!glfwInit()) {
		perror ("glfwInit failed");
		exit(-1);
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	
	if (!glfwVulkanSupported()) {
		perror ("glfwVulkanSupported return false.");
		exit(-1);
	}

	VkEngine e     = (VkEngine)calloc(1, sizeof(vk_engine_t));
	e->refs        = 1;
	e->vk_gpu      = ion::Window::select(ion::vec2i(width, height), ion::ResizeFn(nullptr), (void*)ion::null);
	e->vk_device   = ion::Device::create(e->vk_gpu); /// extensions should be application defined; they are loaded only when available anyway. we dont NEED anything more complex
	e->max_samples = max_samples;
	e->window 	   = e->vk_gpu->window;
	e->pi 	       = vkh_phyinfo_create(e->vk_gpu->phys, e->vk_gpu->surface);
	e->memory_properties = e->pi->memProps; // redundant
	e->gpu_props   = e->pi->properties;
	e->user		   = user;

	glfwSetKeyCallback (e->window, glfw_key);
	glfwSetMouseButtonCallback(e->window, glfw_button);
	glfwSetCursorPosCallback(e->window, glfw_move);
	glfwSetScrollCallback(e->window, glfw_scroll);
	glfwSetCharCallback(e->window, glfw_char);
	glfwSetWindowUserPointer(e->window, (void*)e);

	ion::Vulkan vk;
	VmaVulkanFunctions m_VulkanFunctions = {};

// Vulkan 1.0
    m_VulkanFunctions.vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr;
    m_VulkanFunctions.vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetDeviceProcAddr;
    m_VulkanFunctions.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vkGetPhysicalDeviceProperties;
    m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)vkGetPhysicalDeviceMemoryProperties;
    m_VulkanFunctions.vkAllocateMemory = (PFN_vkAllocateMemory)vkAllocateMemory;
    m_VulkanFunctions.vkFreeMemory = (PFN_vkFreeMemory)vkFreeMemory;
    m_VulkanFunctions.vkMapMemory = (PFN_vkMapMemory)vkMapMemory;
    m_VulkanFunctions.vkUnmapMemory = (PFN_vkUnmapMemory)vkUnmapMemory;
    m_VulkanFunctions.vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)vkFlushMappedMemoryRanges;
    m_VulkanFunctions.vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)vkInvalidateMappedMemoryRanges;
    m_VulkanFunctions.vkBindBufferMemory = (PFN_vkBindBufferMemory)vkBindBufferMemory;
    m_VulkanFunctions.vkBindImageMemory = (PFN_vkBindImageMemory)vkBindImageMemory;
    m_VulkanFunctions.vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)vkGetBufferMemoryRequirements;
    m_VulkanFunctions.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)vkGetImageMemoryRequirements;
    m_VulkanFunctions.vkCreateBuffer = (PFN_vkCreateBuffer)vkCreateBuffer;
    m_VulkanFunctions.vkDestroyBuffer = (PFN_vkDestroyBuffer)vkDestroyBuffer;
    m_VulkanFunctions.vkCreateImage = (PFN_vkCreateImage)vkCreateImage;
    m_VulkanFunctions.vkDestroyImage = (PFN_vkDestroyImage)vkDestroyImage;
    m_VulkanFunctions.vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)vkCmdCopyBuffer;

    // Vulkan 1.1
#if VMA_VULKAN_VERSION >= 1001000
    m_VulkanFunctions.vkGetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2)vkGetBufferMemoryRequirements2;
    m_VulkanFunctions.vkGetImageMemoryRequirements2KHR = (PFN_vkGetImageMemoryRequirements2)vkGetImageMemoryRequirements2;
    m_VulkanFunctions.vkBindBufferMemory2KHR = (PFN_vkBindBufferMemory2)vkBindBufferMemory2;
    m_VulkanFunctions.vkBindImageMemory2KHR = (PFN_vkBindImageMemory2)vkBindImageMemory2;
    m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = (PFN_vkGetPhysicalDeviceMemoryProperties2)vkGetPhysicalDeviceMemoryProperties2;
#endif

#if VMA_VULKAN_VERSION >= 1003000
    m_VulkanFunctions.vkGetDeviceBufferMemoryRequirements = (PFN_vkGetDeviceBufferMemoryRequirements)vkGetDeviceBufferMemoryRequirements;
    m_VulkanFunctions.vkGetDeviceImageMemoryRequirements = (PFN_vkGetDeviceImageMemoryRequirements)vkGetDeviceImageMemoryRequirements;
#endif


	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
	allocatorCreateInfo.physicalDevice = e->vk_gpu->phys;
	allocatorCreateInfo.device = e->vk_device->device;
	allocatorCreateInfo.instance = vk->inst();
	allocatorCreateInfo.pVulkanFunctions = &m_VulkanFunctions;

	VmaAllocator allocator;
	vmaCreateAllocator(&allocatorCreateInfo, &e->allocator);

	uint32_t qCount = 0;
	float qPriorities[] = {0.0};

	VkDeviceQueueCreateInfo pQueueInfos[4] = { };
	if (vkh_phyinfo_create_presentable_queues	(e->pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;
	/*if (vkh_phyinfo_create_compute_queues		(e->pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;
	if (vkh_phyinfo_create_transfer_queues		(e->pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;*/

	/// copy from vk
	e->pi->supportedFeatures = e->vk_gpu->support;

	/// this was vkh_device_create; importing from vk now; the extensions should match
	e->vkh = vkh_device_import(e); /// round-about device

	/// create presenter associated to engine/window
	e->renderer = vkh_presenter_create(
		e->vkh, (uint32_t) e->pi->pQueue, e->vk_gpu->surface,
		width, height, VK_FORMAT_B8G8R8A8_UNORM, presentMode, true);

	singleton = e;
	return e;
}

VkEngine vkengine_grab (VkEngine e) {
	if (e)
		e->refs++;
	return e;
}

void vkengine_drop (VkEngine e) {
	if (e && --e->refs == 0) {
		vkDeviceWaitIdle(e->vkh->device);
		VkSurfaceKHR surf = e->renderer->surface;
		vkh_presenter_drop(e->renderer);
		vkh_device_drop(e->vkh);
		e->vk_device.drop();
		e->vk_gpu.drop();
		free(e);
	}
}

/// i think vkengine should support multiple windows
void vkengine_close (VkEngine e) {
	glfwSetWindowShouldClose(e->window, GLFW_TRUE);
}

void vkengine_blitter_run (VkEngine e, VkImage img, uint32_t width, uint32_t height) {
	VkhPresenter p = e->renderer;
	vkh_presenter_build_blit_cmd (p, img, width, height);

	while (!vkengine_should_close (e)) {
		glfwPollEvents();
		if (!vkh_presenter_draw (p))
			vkh_presenter_build_blit_cmd (p, img, width, height);
	}
}

bool vkengine_should_close(VkEngine e) {
	return glfwWindowShouldClose (e->window);
}

bool vkengine_poll_events(VkEngine e) {
	glfwPollEvents();
	return true;
}

void vkengine_set_title(VkEngine e, const char* title) {
	glfwSetWindowTitle(e->window, title);
}

VkInstance vkengine_get_instance(VkEngine e) {
	return e->vk->inst();
}

VkDevice vkengine_get_device(VkEngine e) {
	return e->vk_device->device;
}

VkPhysicalDevice vkengine_get_physical_device(VkEngine e) {
	return e->vk_gpu->phys;
}

VkQueue vkengine_get_queue(VkEngine e) {
	return e->renderer->queue;
}

uint32_t vkengine_get_queue_fam_idx(VkEngine e) {
	return e->renderer->qFam;
}

void vkengine_wait_idle (VkEngine e) {
	vkDeviceWaitIdle(e->vk_device->device);
}

void vkengine_key_callback 		(VkEngine e, FnKey fn_key) 	  { e->fn_key    = fn_key; }
void vkengine_button_callback 	(VkEngine e, FnButton button) { e->fn_button = button; }
void vkengine_move_callback 	(VkEngine e, FnMove move)	  { e->fn_move   = move;   }

void vkengine_scroll_callback   (VkEngine e, FnScroll scroll) {
	e->fn_scroll = scroll;
}

void vkengine_char_callback (VkEngine e, FnChar chr){
	e->fn_char = chr;
}
