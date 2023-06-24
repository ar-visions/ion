#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#ifdef __APPLE__
	/// never do this again!
    #include <MoltenVK/vk_mvk_moltenvk.h>
#else
	/// or this!
    #include <vulkan/vulkan.h>
#endif

/// and definitely not this!
#include <GLFW/glfw3.h>

/// never include this again either (VERIFY)
#include <vk_mem_alloc.h>

#include <mx/mx.hpp>
#include <vke/vke.hh>
#include <math/math.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++11-narrowing"

namespace ion {

/// needs organization into what is instance/device/feature extension
static bool validation;
static bool mesa_overlay;
static bool render_doc;
static bool scalar_block_layout;
static bool timeline_semaphore;

/// additional extensions would be nice, then it can look them up case by case
/// then it supports a broader interface
void vke_configure(bool _validation, bool _mesa_overlay, bool _render_doc) {
	if (!vke_out) vke_out = stdout;
	validation 	   = _validation;
	mesa_overlay   = _mesa_overlay;
	render_doc     = _render_doc;
}

#define VKE_LOG_ERR		    0x00000001
#define VKE_LOG_DEBUG		0x00000002
#define VKE_LOG_INFO_PTS	0x00000004
#define VKE_LOG_INFO_PATH	0x00000008
#define VKE_LOG_INFO_CMD	0x00000010
#define VKE_LOG_INFO_VBO	0x00000020
#define VKE_LOG_INFO_IBO	0x00000040
#define VKE_LOG_INFO_VAO	(VKE_LOG_INFO_VBO|VKE_LOG_INFO_IBO)
#define VKE_LOG_THREAD		0x00000080
#define VKE_LOG_DBG_ARRAYS	0x00001000
#define VKE_LOG_STROKE		0x00010000
#define VKE_LOG_FULL		0xffffffff

#define VKE_LOG_INFO		0x00008000//(VKE_LOG_INFO_PTS|VKE_LOG_INFO_PATH|VKE_LOG_INFO_CMD|VKE_LOG_INFO_VAO)
#ifdef DEBUG
	extern uint32_t vke_log_level;
	#ifdef VKVG_WIRED_DEBUG
		enum vkvg_wired_debug_mode {
			vkvg_wired_debug_mode_normal	= 0x01,
			vkvg_wired_debug_mode_points	= 0x02,
			vkvg_wired_debug_mode_lines		= 0x04,
			vkvg_wired_debug_mode_both		= vkvg_wired_debug_mode_points|vkvg_wired_debug_mode_lines,
			vkvg_wired_debug_mode_all		= 0xFFFFFFFF
		};
		extern vkvg_wired_debug_mode vkvg_wired_debug;
	#endif
#endif

uint32_t vke_log_level = VKE_LOG_DEBUG;
FILE    *vke_out;

#define ENGINE_NAME		"vkhelpers"
#define ENGINE_VERSION	1
#define VKE_KO      	0x00000400
#define VKE_MO      	0x00100000
#define VKE_GO      	0x40000000

#define vk_assert(f)                                                                      		\
{                                                                                               \
    VkResult res = (f);                                                                         \
    if (res != VK_SUCCESS)                                                                      \
    {                                                                                           \
        fprintf(stderr, "Fatal : VkResult is %d in %s at line %d\n", res,  __FILE__, __LINE__); \
        assert(res == VK_SUCCESS);                                                              \
    }                                                                                           \
}

ptr_implement(VkeGPU, 		 mx);
ptr_implement(VkeSurfaceKHR, mx);
ptr_implement(VkeApp, 		 mx);
ptr_implement(VkeWindow, 	 mx);
ptr_implement(VkeMonitor, 	 mx);
ptr_implement(VkePresenter,  mx);

struct vke_window {
    VkeDevice          dev;
    int                w, h;
    VkeKeyEvent        key_event;
    VkeCharEvent       char_event;
    VkeButtonEvent     button_event;
    VkeCursorEvent     cursor_event;
    VkeResizeEvent     resize_event;
    VkePresenter       renderer;
    struct GLFWwindow *glfw;
	VkeSurfaceKHR      surface;
    void              *user;
	///
	~vke_window() {
		glfwDestroyWindow(glfw);
	}
};



struct vke_device;
struct vke_queue {
	vke_device	   *dev;
	uint32_t		familyIndex;
	VkQueue			queue;
	VkQueueFlags	flags;

	///
	static const size_t valloc = 32;
	struct entry {
		array<VkCommandBuffer> 	   	cmd_buffers;
		array<VkPipelineStageFlags> wait_stages;
		array<VkSemaphore>			wait_semaphores;
		array<VkSemaphore>			signal_semaphores;
		array<u64>				 	wait_values;
		array<u64>				 	signal_values;
		///
		entry() :
			cmd_buffers			(valloc),
			wait_stages			(valloc),
			wait_semaphores		(valloc),
			signal_semaphores	(valloc),
			wait_values			(valloc),
			signal_values		(valloc) { }
		///
		entry &reset() {
			cmd_buffers.clear();
			wait_stages.clear();
			wait_semaphores.clear();
			signal_semaphores.clear();
			wait_values.clear();
			signal_values.clear();
			return *this;
		}
	};

	static const size_t max_transit = 32;
	static inline entry *transit = new entry[max_transit];
	///
	static entry &next_entry() {
		static size_t index = max_transit;
		if (++index >= max_transit)
			  index = 0;
		return transit[index].reset();
	}

	///
	operator VkQueue() { return queue; }

	void submit(
		array<VkCommandBuffer> 		cmd_buf,
		array<VkeSemaphore> 		wait_semaphores,
		array<VkeSemaphore> 		signal_semaphores,
		array<u64> 					waits,
		array<u64> 					signals,
		array<VkPipelineStageFlags> wait_stages);

};

struct vke_buffer {
	vke_device	   		   *dev;
	VkBufferCreateInfo		infos;
	VkBuffer				buffer;
	VmaAllocation			alloc;
	VmaAllocationInfo		allocInfo;
	VmaAllocationCreateInfo allocCreateInfo;
	VkDescriptorBufferInfo	descriptor;
	VkDeviceSize			alignment;
	void*					mapped;
	///
	operator VkBuffer() { return buffer; }
	///
	~vke_buffer();
};

static void glfw_error(int code, const char *cs) {
	printf("glfw error: code: %d (%s)", code, cs);
}

static void glfw_key(GLFWwindow *h, int unicode, int scan_code, int action, int mods) {
	vke_window *win = (vke_window *)glfwGetWindowUserPointer(h);
    win->key_event(win->user, unicode, scan_code, action, mods); /// i think its a good idea to pass handle to your user data not the window.  afterall you are going to look that up anyway, and guess waht it can contain too
}

static void glfw_char(GLFWwindow *h, unsigned int unicode) {
	vke_window *win =(vke_window *)glfwGetWindowUserPointer(h);
	win->char_event(win->user, unicode);
}

static void glfw_button(GLFWwindow *h, int button, int action, int mods) {
    vke_window *win = (vke_window *)glfwGetWindowUserPointer(h);
    win->button_event(win->user, button, action, mods);
}

static void glfw_cursor(GLFWwindow *h, double x, double y) {
    vke_window *win = (vke_window *)glfwGetWindowUserPointer(h);
    win->cursor_event(win->user, x, y, (bool)glfwGetWindowAttrib(h, GLFW_HOVERED));
}

static void glfw_resize(GLFWwindow *handle, int w, int h) {
	vke_window *win = (vke_window *)glfwGetWindowUserPointer(handle);
	win->resize_event(win->user, w, h);
}

struct vke_app {
    struct ivke_app *data;

	static inline array<vke_app*>				apps;

	/// these are set prior to bootstrap() call in vke-app
	static inline array<symbol> 				config_layers   {"VK_LAYER_KHRONOS_validation"};
	static inline array<symbol> 				config_instance {"VK_EXT_debug_utils", "VK_KHR_get_physical_device_properties2"};
	static inline array<symbol> 				config_device;

    /// the effectively used instance extensions (instance) and instance layers (layer)
    static inline array<VkExtensionProperties> instance_props;
	static inline array<VkLayerProperties> 	   layer_props;

	static inline array<symbol> 		 	   enabled_layers;
	static inline array<symbol> 		 	   enabled_exts;

	VkInstance  		 				 inst;
	VkDebugUtilsMessengerEXT 			 debugMessenger;

	operator VkInstance() { return inst; }

	static bool instance_supports(symbol iext) {
		for (VkExtensionProperties &e: instance_props)
			if (strcmp(e.extensionName, iext) == 0)
				return true;
		return false;
	}

	static bool layer_supported(symbol layerName) {
		for (VkLayerProperties &lp: layer_props) {
			if (strcmp(lp.layerName, layerName) == 0)
				return true;
		}
		return false;
	}

	static void config(array<symbol> config_layers, array<symbol> config_instance, array<symbol> config_device) {
		vke_app::config_layers   = config_layers;
		vke_app::config_instance = config_instance;
		vke_app::config_device   = config_device;
	}

		//if (instance_supports ("VK_EXT_debug_utils")) 					  enabled_exts += "VK_EXT_debug_utils";
		//if (instance_supports ("VK_KHR_get_physical_device_properties2")) enabled_exts += "VK_KHR_get_physical_device_properties2";
	static void bootstrap(array<symbol> &layers, array<symbol> &instance) {
		static bool init = false;
		if (init) return;
		init = true;
		
		assert(glfwInit());
		assert(glfwVulkanSupported());

		u32 icount = 0;
		vk_assert(vkEnumerateInstanceExtensionProperties(NULL, &icount, NULL));
		instance_props = array<VkExtensionProperties>(icount);
		vk_assert(vkEnumerateInstanceExtensionProperties(NULL, &icount, instance_props.data));
		instance_props.set_size(icount);

		if (!vke_out) vke_out = stdout;
		glfwSetErrorCallback(glfw_error);

		const char* enabledLayers[10];
		const char* enabledExts  [10];
		uint32_t 	enabledExtsCount   = 0,
					enabledLayersCount = 0,
					phyCount           = 0;

		u32 layer_count;
		vk_assert(vkEnumerateInstanceLayerProperties(&layer_count, NULL));
		layer_props.set_size(layer_count);

		vk_assert(vkEnumerateInstanceLayerProperties(&layer_count, layer_props.data));

		/// enable layers from configuration
		for (symbol cfg: config_layers)
			if (layer_supported(cfg)) enabled_layers += cfg;

		/// get required extensions
		uint32_t     glfwReqExtsCount = 0;
		const char** gflwExts = glfwGetRequiredInstanceExtensions (&glfwReqExtsCount);

		for (symbol cfg: config_instance)
			if (instance_supports(cfg)) enabled_exts += cfg;
		
		for (uint32_t i = 0; i < glfwReqExtsCount; i++)
			enabled_exts += gflwExts[i];
	}

	static vke_app *main(array<symbol> &layers, array<symbol> &instance) {
		bootstrap(layers, instance);
		for (int i = 0; i < apps.length(); i++)
			if (apps[i]) return apps[i];
		return null;
	}

   ~vke_app();
};

struct vke_device {
	VkDevice     	 vk;
	VmaAllocator	 allocator;
	VkePhyInfo		 phyinfo;
	VkeApp			 app; 		/// contains instance

    static VkeDevice create (VkeApp app, VkePhyInfo phyInfo, VkDeviceCreateInfo* pDevice_info);
    static VkeDevice import (VkInstance inst, VkPhysicalDevice phy, VkDevice dev);

    void             init_debug_utils ();
    VkPhysicalDevice gpu              ();
    void             set_object_name  (VkObjectType objectType, uint64_t handle, const char *name);
    VkSampler        sampler          (VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mipmap, VkSamplerAddressMode sampler_amode);
    void             destroy_sampler  (VkSampler sampler);
    const void*      get_device_requirements (VkPhysicalDeviceFeatures* pEnabledFeatures);
    int              present_index    ();
    int              transfer_index   ();
    int              graphics_index   ();
    int              compute_index    ();

	~vke_device() {
		vmaDestroyAllocator(allocator);
		vkDestroyDevice(vk, NULL);
	}
};

VkeDevice::operator VkDevice &() { return data->vk; }

struct vke_sampler {
	VkeDevice 		dev;
	VkSampler 		sampler;
	///
	~vke_sampler() {
		vkDestroySampler(dev, sampler, NULL);
	}
};

VkeSampler::operator VkSampler &() {
	return data->sampler;
}


vke_buffer::~vke_buffer() {
	if (buffer)
		vmaDestroyBuffer(dev->allocator, buffer, alloc);
}

#define _CHECK_DEV_EXT(ext) {					\
	if (_get_dev_extension_is_supported(pExtensionProperties, extensionCount, #ext)){\
		if (pExtensions)							\
			pExtensions[*pExtCount] = #ext;			\
		(*pExtCount)++;								\
	}\
}


struct vke_image {
	VkeDevice				dev;
	VkImageCreateInfo		infos;
	VkImage					image;
	VmaAllocation			alloc;
	VmaAllocationInfo		allocInfo;
	VkeSampler				sampler;
	VkImageView				view;
	VkImageLayout			layout; //current layout
	bool					imported;//dont destroy vkimage at end
	~vke_image() {
		if (view    != VK_NULL_HANDLE) vkDestroyImageView(dev, view, NULL);
		if (sampler != VK_NULL_HANDLE) vkDestroySampler  (dev, sampler, NULL);
		if (!imported) 			       vmaDestroyImage   (dev->allocator, image, alloc);
	}
};

struct vke_phyinfo {
	VkPhysicalDevice					gpu;
	VkPhysicalDeviceMemoryProperties	mem_props;
	vec2i 								dpi;
	VkPhysicalDeviceProperties			gpu_props;
	VkQueueFamilyProperties*			queues;
	uint32_t							queueCount;
	int									cQueue; // compute index
	int									gQueue; // graphic index
	int									tQueue; // transfer index
	int									pQueue; // presentation index
	array<VkExtensionProperties>		ext_props;
};

VkExtensionProperties *VkePhyInfo::lookup_extension(symbol input) {
	if (!data->ext_props) {
		uint32_t dev_ext_count = 0;
		vk_assert(vkEnumerateDeviceExtensionProperties(data->gpu, NULL, &dev_ext_count, NULL));
		data->ext_props = array<VkExtensionProperties> { dev_ext_count };
		vk_assert(vkEnumerateDeviceExtensionProperties(data->gpu, NULL,
			&dev_ext_count, &data->ext_props));
	}
	if (input)
		for (VkExtensionProperties &prop: data->ext_props)
			if (strcmp(prop.extensionName, input) == 0)
				return &prop;
	return null;
}

array<symbol> VkePhyInfo::usable_extensions(array<symbol> input) { /// input: VK_KHR_portability_subset, VK_EXT_scalar_block_layout
	array<symbol> result(input.len());
	///
	lookup_extension(null);
	///
	for (symbol &ext: input)
		if (lookup_extension(ext))
			result += ext;
	
	return result;
}


struct vke_presenter {
	VkQueue 		        queue;
	VkCommandPool        	cmdPool;
	uint32_t		        qFam;
	VkeDevice		        dev;
	VkSurfaceKHR        	surface;
	VkSemaphore		    	semaPresentEnd;
	VkSemaphore		    	semaDrawEnd;
	VkeFence			    fenceDraw;
	VkFormat		    	format;
	VkColorSpaceKHR     	colorSpace;
	VkPresentModeKHR    	presentMode;
	uint32_t		        width;
	uint32_t		        height;
	uint32_t		        imgCount;
	uint32_t		        currentScBufferIndex;
	VkRenderPass 	    	renderPass;
	VkSwapchainKHR 	    	swapChain;
	array<VkeImage>	        ScBuffers;
	array<VkeCommandBuffer> cmdBuffs; // neither of these need to be in compact vector for vulkan api usage.
	VkFramebuffer*	        frameBuffs;
	///
	~vke_presenter() {
		vkDeviceWaitIdle    (dev);
		for (uint32_t i = 0; i < imgCount; i++)
			vkFreeCommandBuffers (dev, cmdPool, 1, &cmdBuffs[i]);
		
		vkDestroySwapchainKHR (dev, swapChain, NULL);
		swapChain = VK_NULL_HANDLE;

		free(cmdBuffs);

		ScBuffers.clear();
		vkDestroySemaphore	(dev, semaDrawEnd,    NULL);
		vkDestroySemaphore	(dev, semaPresentEnd, NULL);
		vkDestroyFence		(dev, fenceDraw,      NULL);
		vkDestroyCommandPool(dev, cmdPool,        NULL);
	}
};

VkeImage::VkeImage (
		VkeDevice dev, VkeImageType imageType, VkeFormat format, uint32_t width, uint32_t height,
		VkeMemoryUsage memprops, VkeImageUsageFlags usage, VkeSampleCountFlagBits samples, VkeImageTiling tiling,
		uint32_t mipLevels, uint32_t arrayLayers) : mx(&data) {
	///
	data->dev 				 = dev;
	VkImageCreateInfo* pInfo = &data->infos;
	pInfo->sType			 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	pInfo->imageType		 = imageType;
	pInfo->tiling			 = tiling;
	pInfo->initialLayout	 = (tiling == VK_IMAGE_TILING_OPTIMAL) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED;
	pInfo->sharingMode		 = VK_SHARING_MODE_EXCLUSIVE;
	pInfo->usage			 = usage;
	pInfo->format			 = format;
	pInfo->extent.width		 = width;
	pInfo->extent.height	 = height;
	pInfo->extent.depth		 = 1;
	pInfo->mipLevels		 = mipLevels;
	pInfo->arrayLayers		 = arrayLayers;
	pInfo->samples			 = samples;
	///
	VmaAllocationCreateInfo allocInfo = { .usage = (VmaMemoryUsage)memprops };
	vk_assert(vmaCreateImage (dev->allocator, pInfo, &allocInfo, &data->image, &data->alloc, &data->allocInfo));
}

VkeImage VkeImage::tex2d_array_create(
		VkeDevice dev, VkeFormat format, uint32_t width, uint32_t height, uint32_t layers,
		VkeMemoryUsage memprops, VkeImageUsageFlags usage)
{
	VkeImageType it = VK_IMAGE_TYPE_2D;
	return VkeImage(dev, it, format, width, height, memprops, usage,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 1, layers);
}

// 1x1 basic sample
VkeImage::VkeImage(VkeDevice dev, VkeFormat format, uint32_t width, uint32_t height, VkeImageTiling tiling,
				   VkeMemoryUsage memprops, VkeImageUsageFlags usage) :
	VkeImage(dev, VK_IMAGE_TYPE_2D, format, width, height, memprops, usage, VK_SAMPLE_COUNT_1_BIT, tiling, 1, 1) { }
}

using namespace ion;
//create vkhImage from existing VkImage
VkeImage::VkeImage(VkeDevice dev, VkeImage vkImg, VkeFormat format, uint32_t width, uint32_t height) : mx(&data) {
	data->dev 		= dev;
	data->image		= vkImg->image;
	data->imported	= true;

	VkImageCreateInfo* pInfo = &data->infos;
	pInfo->imageType		= VK_IMAGE_TYPE_2D;
	pInfo->format			= format;
	pInfo->extent.width		= width;
	pInfo->extent.height	= height;
	pInfo->extent.depth		= 1;
	pInfo->mipLevels		= 1;
	pInfo->arrayLayers		= 1;
	//pInfo->samples		= samples;
}

VkeImage::VkeImage(VkeDevice dev, VkeFormat format, VkeSampleCountFlagBits num_samples, uint32_t width, uint32_t height,
			       VkeMemoryUsage memprops, VkeImageUsageFlags usage) : VkeImage(
						dev, VK_IMAGE_TYPE_2D, format, width, height,
						memprops, usage, num_samples, VK_IMAGE_TILING_OPTIMAL, 1, 1) { }

/// instance function
void VkeImage::create_view(VkeImageViewType viewType, VkeImageAspectFlags aspectFlags) {
	if (data->view != VK_NULL_HANDLE)
		vkDestroyImageView	(data->dev, data->view, NULL);
	VkImageViewCreateInfo viewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = data->image,
		.viewType = viewType,
		.format = data->infos.format,
		.components = {VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B,VK_COMPONENT_SWIZZLE_A},
		.subresourceRange = {aspectFlags,0,1,0,data->infos.arrayLayers}
	};
	vk_assert(vkCreateImageView(data->dev, &viewInfo, NULL, &data->view));
}

void VkeImage::create_sampler(VkeFilter magFilter, VkeFilter minFilter,
							  VkeSamplerMipmapMode mipmapMode, VkeSamplerAddressMode addressMode){
	if(data->sampler != VK_NULL_HANDLE)
		vkDestroySampler	(data->dev, data->sampler ,NULL);
	VkSamplerCreateInfo samplerCreateInfo = { 
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.maxAnisotropy= 1.0,
		.addressModeU = addressMode,
		.addressModeV = addressMode,
		.addressModeW = addressMode,
		.magFilter	= magFilter,
		.minFilter	= minFilter,
		.mipmapMode	= mipmapMode
	};
	vk_assert(vkCreateSampler(data->dev, &samplerCreateInfo, NULL, &data->sampler->sampler));
}

void VkeImage::set_sampler(VkeSampler sampler) {
	data->sampler = sampler;
}

void VkeImage::create_descriptor(VkeImageViewType viewType, VkeImageAspectFlags aspectFlags, VkeFilter magFilter,
								 VkeFilter minFilter, VkeSamplerMipmapMode mipmapMode, VkeSamplerAddressMode addressMode) {
	create_view		(viewType, aspectFlags);
	create_sampler	(magFilter, minFilter, mipmapMode, addressMode);
}

VkeDescriptorImageInfo VkeImage::get_descriptor(
		VkeImageLayout imageLayout) {
	VkDescriptorImageInfo desc = {
		.imageView   = data->view,
		.imageLayout = imageLayout,
		.sampler     = data->sampler
	};
	return desc; 
}

void VkeImage::set_layout(
		VkeCommandBuffer cmdBuff, VkeImageAspectFlags aspectMask,
		VkeImageLayout old_image_layout, VkeImageLayout new_image_layout,
		VkePipelineStageFlags src_stages, VkePipelineStageFlags dest_stages) {
	VkImageSubresourceRange subres = { aspectMask, 0, 1, 0, 1 };
	set_layout_subres(cmdBuff, (VkImageSubresourceRange&)subres, old_image_layout, new_image_layout, src_stages, dest_stages);
}

void VkeImage::set_layout_subres(
		VkeCommandBuffer cmdBuff, VkeImageSubresourceRange subresourceRange,
		VkeImageLayout old_image_layout, VkeImageLayout new_image_layout,
		VkePipelineStageFlags src_stages, VkePipelineStageFlags dest_stages) {
	VkImageMemoryBarrier image_memory_barrier = {
		.sType 					= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.oldLayout 				= data->layout,
		.newLayout 				= new_image_layout,
		.srcQueueFamilyIndex 	= VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex 	= VK_QUEUE_FAMILY_IGNORED,
		.image 					= data->image,
		.subresourceRange 		= subresourceRange
	};
	switch (old_image_layout) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			break;
		default:
			break;
	}
	switch (new_image_layout) {
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		default:
			break;
	}
	vkCmdPipelineBarrier(cmdBuff, src_stages, dest_stages, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier);
	data->layout = new_image_layout;
}

void VkeImage::destroy_sampler() {
	if(data->sampler != VK_NULL_HANDLE)
		vkDestroySampler(data->dev,data->sampler,NULL);
	data->sampler = VK_NULL_HANDLE;
}

void* VkeImage::map() {
	void* res;
	vmaMapMemory(data->dev->allocator, data->alloc, &res);
	return res;
}

void VkeImage::unmap() {
	vmaUnmapMemory(data->dev->allocator, data->alloc);
}

void VkeImage::set_name(const char* name) {
	data->dev->set_object_name(VK_OBJECT_TYPE_IMAGE, (uint64_t)data->image, name);
}

uint64_t VkeImage::get_stride() {
	VkImageSubresource  subres = {VK_IMAGE_ASPECT_COLOR_BIT,0,0};
	VkSubresourceLayout layout = {0};
	vkGetImageSubresourceLayout(data->dev, data->image, &subres, &layout);
	return (uint64_t) layout.rowPitch;
}

// using the vke.  im not sure if this matters
//#define FENCE_TIMEOUT UINT16_MAX

void VkePresenter::swapchain_destroy() {
	for (uint32_t i = 0; i < data->imgCount; i++)
		vkFreeCommandBuffers (data->dev, data->cmdPool, 1, &data->cmdBuffs[i]);
	
	vkDestroySwapchainKHR (data->dev, data->swapChain, NULL);
	data->swapChain = VK_NULL_HANDLE;
	free(data->cmdBuffs);
	data->ScBuffers.clear();
}

void _init_phy_surface(VkePresenter r, VkFormat preferedFormat, VkPresentModeKHR presentMode) {
	vke_presenter  *data = &r; /// get the data address
	VkPhysicalDevice gpu = data->dev->phyinfo->gpu;

	uint32_t count;
	vk_assert(vkGetPhysicalDeviceSurfaceFormatsKHR (gpu, data->surface, &count, NULL));
	assert (count>0);
	VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)malloc(count * sizeof(VkSurfaceFormatKHR));
	vk_assert(vkGetPhysicalDeviceSurfaceFormatsKHR (gpu, data->surface, &count, formats));

	for (uint32_t i=0; i<count; i++){
		if (formats[i].format == preferedFormat) {
			data->format = formats[i].format;
			data->colorSpace = formats[i].colorSpace;
			break;
		}
	}
	assert (data->format != VK_FORMAT_UNDEFINED);

	vk_assert(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, data->surface, &count, NULL));
	assert (count>0);
	VkPresentModeKHR* presentModes = (VkPresentModeKHR*)malloc(count * sizeof(VkPresentModeKHR));
	vk_assert(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, data->surface, &count, presentModes));
	data->presentMode = (VkPresentModeKHR)-1;
	for (uint32_t i=0; i<count; i++){
		if (presentModes[i] == presentMode) {
			data->presentMode = presentModes[i];
			break;
		}
	}
	if ((int32_t)data->presentMode < 0)
		data->presentMode = presentModes[0];//take first as fallback

	free(formats);
	free(presentModes);
}

/// there is so much repeating on this stuff; this presentQueueFamIdx you can get from Device. and, never would you get anything else from deive there arent Multiple presents
VkePresenter::VkePresenter(VkeDevice dev, uint32_t presentQueueFamIdx, VkeSurfaceKHR surface, uint32_t width, uint32_t height,
						   VkeFormat preferedFormat, VkePresentModeKHR presentMode) : mx(&data) {
	data->dev     = dev;
	data->qFam    = presentQueueFamIdx;
	data->surface = surface;
	data->width   = width;
	data->height  = height;

	vkGetDeviceQueue(data->dev, data->qFam, 0, &data->queue);

	data->cmdPool		  = VkeCommandPool(data->dev, presentQueueFamIdx, VkCommandPoolCreateFlags(0));
	data->semaPresentEnd  = VkeSemaphore(data->dev);
	data->semaDrawEnd	  = VkeSemaphore(data->dev);
	data->fenceDraw	      = VkeFence(data->dev, true);

	_init_phy_surface(data, preferedFormat, presentMode);
	create_swapchain();
}

bool VkePresenter::acquireNextImage(VkeFence fence, VkeSemaphore semaphore) {
	// Get the index of the next available swapchain image:
	VkResult err = vkAcquireNextImageKHR
			(data->dev, data->swapChain, UINT64_MAX, semaphore, fence, &data->currentScBufferIndex);
	return ((err != VK_ERROR_OUT_OF_DATE_KHR) && (err != VK_SUBOPTIMAL_KHR));
}


bool VkePresenter::draw() {
	if (!acquireNextImage(VkFence(VK_NULL_HANDLE), data->semaPresentEnd)){
		create_swapchain();
		return false;
	}

	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 .commandBufferCount = 1,
								 .signalSemaphoreCount = 1,
								 .pSignalSemaphores = &data->semaDrawEnd,
								 .waitSemaphoreCount = 1,
								 .pWaitSemaphores = &data->semaPresentEnd,
								 .pWaitDstStageMask = &dstStageMask,
								 .pCommandBuffers = &data->cmdBuffs[data->currentScBufferIndex]};

	vkWaitForFences	(data->dev, 1, &data->fenceDraw, VK_TRUE, FENCE_TIMEOUT);
	vkResetFences	(data->dev, 1, &data->fenceDraw);

	vk_assert(vkQueueSubmit (data->queue, 1, &submit_info, data->fenceDraw));

	/* Now present the image in the window */
	VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
								 .swapchainCount = 1,
								 .pSwapchains = &data->swapChain,
								 .waitSemaphoreCount = 1,
								 .pWaitSemaphores = &data->semaDrawEnd,
								 .pImageIndices = &data->currentScBufferIndex };

	/* Make sure command buffer is finished before presenting */
	vkQueuePresentKHR(data->queue, &present);
	return true;
}

void VkePresenter::build_blit_cmd (VkeImage blitSource, uint32_t width, uint32_t height) {

	uint32_t w = MIN(width, data->width), h = MIN(height, data->height);
	///
	for (uint32_t i = 0; i < data->imgCount; ++i) {
		VkeImage bltDstImage = data->ScBuffers[i];
		VkeCommandBuffer  cb = data->cmdBuffs [i];

		cb.cmd_begin(VkCommandBufferUsageFlags(0)); /// no implicit literal, probably for nullptr_t ambiguity; the cast is fine

		bltDstImage.set_layout(cb, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		blitSource.set_layout(cb, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		/*VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
								.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
								.srcOffset = {0},
								.dstOffset = {0,0,0},
								.extent = {MIN(w,data->width), MIN(h,data->height),1}};*/
		VkImageBlit bregion = {
			.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
			.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
			.srcOffsets[0] = { 0 },
			.srcOffsets[1] = { w, h, 1 },
			.dstOffsets[0] = { 0 },
			.dstOffsets[1] = { width, height, 1 }
		};

		vkCmdBlitImage(*cb, blitSource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bregion, VK_FILTER_NEAREST);

		/*vkCmdCopyImage(cb, blitSource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1, &cregion);*/

		bltDstImage.set_layout(cb, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
		blitSource.set_layout(cb, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		cb.cmd_end();
	}
}

void VkePresenter::get_size(uint32_t* pWidth, uint32_t* pHeight){
	*pWidth = data->width;
	*pHeight = data->height;
}

void VkePresenter::create_swapchain () {
	VkPhysicalDevice gpu = data->dev->phyinfo->gpu;
	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(data->dev);

	VkSurfaceCapabilitiesKHR surfCapabilities;
	vk_assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, data->surface, &surfCapabilities));
	assert (surfCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	// width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
	if (surfCapabilities.currentExtent.width == 0xFFFFFFFF) {
		// If the surface size is undefined, the size is set to
		// the size of the images requested
		if (data->width < surfCapabilities.minImageExtent.width)
			data->width = surfCapabilities.minImageExtent.width;
		else if (data->width > surfCapabilities.maxImageExtent.width)
			data->width = surfCapabilities.maxImageExtent.width;
		if (data->height < surfCapabilities.minImageExtent.height)
			data->height = surfCapabilities.minImageExtent.height;
		else if (data->height > surfCapabilities.maxImageExtent.height)
			data->height = surfCapabilities.maxImageExtent.height;
	} else {
		// If the surface size is defined, the swap chain size must match
		data->width = surfCapabilities.currentExtent.width;
		data->height= surfCapabilities.currentExtent.height;
	}

	VkSwapchainKHR newSwapchain;
	VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = data->surface,
		.minImageCount = surfCapabilities.minImageCount,
		.imageFormat = data->format,
		.imageColorSpace = data->colorSpace,
		.imageExtent = {data->width,data->height},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = surfCapabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = data->presentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = data->swapChain
	};

	vk_assert(vkCreateSwapchainKHR (data->dev, &createInfo, NULL, &newSwapchain));
	if (data->swapChain != VK_NULL_HANDLE)
		swapchain_destroy();
	data->swapChain = newSwapchain;

	vk_assert(vkGetSwapchainImagesKHR(data->dev, data->swapChain, &data->imgCount, NULL));
	assert(data->imgCount > 0);

	array<VkImage> images(data->imgCount);
	vk_assert(vkGetSwapchainImagesKHR(data->dev, data->swapChain, &data->imgCount, images.data));
	images.set_size(data->imgCount);
	
	data->ScBuffers  = (VkeImage*)malloc (data->imgCount * sizeof(VkeImage));
	data->cmdBuffs   = array<VkeCommandBuffer>(data->imgCount);

	for (uint32_t  i = 0; i < data->imgCount; i++) {
		VkeImage sci = VkeImage(data->dev, VkeImage(images[i]), data->format, data->width, data->height);
		sci.create_view(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT); /// this modifies field on the VkeImage
		
		data->ScBuffers  += sci; /// VkeImage wont be deleting the VkImage as read form vkGetSwapchainImagesKHR
		data->cmdBuffs   += VkeCommandBuffer(data->dev, data->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	}
	data->currentScBufferIndex = 0;
}


VkBool32 _messenger_callback (
	VkDebugUtilsMessageSeverityFlagBitsEXT			 messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT					 messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT*		 pCallbackData,
	void*											 pUserData) {
	printf("[vke-dbg]: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

VkeApp VkeApp::main(array<symbol> layers, array<symbol> instance) {
	return vke_app::main(layers, instance);
}

VkeEvent::VkeEvent(VkeDevice vke) : mx(&data) {
	VkEventCreateInfo evtInfo = { .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
	vk_assert(vkCreateEvent(vke->vk, &evtInfo, NULL, data));
};

/// lower case is the implemention and internal access, used in module
/// PascalCase is user-level; access to methods (pascal!)
/// the mx case is a bit nicer because you have data described internal, protected
/// in this situation it needs to all be handled member by member and it 
/// might restrict its operation if its not updating the states optimally

vke_app::~vke_app() {
	if (debugMessenger) {
		PFN_vkDestroyDebugUtilsMessengerEXT	 DestroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(inst, "vkDestroyDebugUtilsMessengerEXT");
		DestroyDebugUtilsMessenger(inst, debugMessenger, VK_NULL_HANDLE);
	}
	vkDestroyInstance(inst, NULL);
	for (int i = 0; i < apps.len(); i++)
		if (this == apps[i])
			apps[i] = NULL;
}

/// to not have constructors, ever, is practically good because you get trivial construction in fields
/// your constructors have names. thats clear and works.
/// if node api can work without inheritence that would be nice.  the mx is about passing the common memory through
///

VkeApp::VkeApp(
		uint32_t v_major, uint32_t v_minor, const char* app_name,
		array<symbol> layers, array<symbol> extensions) {
	///
	VkApplicationInfo infos 		= {
		.sType 						= VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName 			= app_name,
		.applicationVersion 		= 1,
		.pEngineName 				= ENGINE_NAME,
		.engineVersion 				= ENGINE_VERSION,
		.apiVersion 				= VK_MAKE_API_VERSION (0, v_major, v_minor, 0)
	};
	VkInstanceCreateInfo inst_info 	= {
		.sType 						= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo 			= &infos,
		.enabledExtensionCount 		= extensions.len(),
		.ppEnabledExtensionNames 	= extensions,
		.enabledLayerCount 			= layers.len(),
		.ppEnabledLayerNames 		= layers
	};
	vk_assert(vkCreateInstance (&inst_info, NULL, &data->inst));
	data->debugMessenger = VK_NULL_HANDLE;
	///
	#ifdef DEBUG
		VkDebugUtilsMessageTypeFlagsEXT typeFlags         = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		VkDebugUtilsMessageSeverityFlagsEXT severityFlags = 
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

		VkDebugUtilsMessengerCreateInfoEXT info = {
			.sType 				= VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext 				= VK_NULL_HANDLE,
			.flags 				= 0,
			.messageSeverity 	= severityFlags,
			.messageType 		= typeFlags,
			.pUserData 			= NULL
		};
		info.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)_messenger_callback;

		PFN_vkCreateDebugUtilsMessengerEXT	CreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)
				vkGetInstanceProcAddr(data->inst, "vkCreateDebugUtilsMessengerEXT");
		CreateDebugUtilsMessenger(data->inst, &info, VK_NULL_HANDLE, &data->debugMessenger);
	#endif
	///
	vke_app::apps.push(data);
}

array<VkePhyInfo> VkeApp::hardware(VkeSurfaceKHR surface) {
	uint32_t count = 0;
	vk_assert(vkEnumeratePhysicalDevices (data->inst, &count, NULL));
	VkPhysicalDevice* phyDevices = (VkPhysicalDevice*)malloc((count) * sizeof(VkPhysicalDevice));
	vk_assert(vkEnumeratePhysicalDevices (data->inst, &count, phyDevices));
	array<VkePhyInfo> result(count);
	for (uint32_t i=0; i<count; i++) {
		result += VkePhyInfo(VkeGPU(phyDevices[i]), surface);
	}
	free (phyDevices);
	return result;
}

static PFN_vkSetDebugUtilsObjectNameEXT		SetDebugUtilsObjectNameEXT;
static PFN_vkQueueBeginDebugUtilsLabelEXT	QueueBeginDebugUtilsLabelEXT;
static PFN_vkQueueEndDebugUtilsLabelEXT		QueueEndDebugUtilsLabelEXT;
static PFN_vkCmdBeginDebugUtilsLabelEXT		CmdBeginDebugUtilsLabelEXT;
static PFN_vkCmdEndDebugUtilsLabelEXT		CmdEndDebugUtilsLabelEXT;
static PFN_vkCmdInsertDebugUtilsLabelEXT	CmdInsertDebugUtilsLabelEXT;

/// 
VkeDevice::VkeDevice(VkeApp app, VkePhyInfo phyInfo, VkeDeviceCreateInfo pDevice_info) {
	assert(false);
}

int VkeDevice::present_index() {
	return data->phyinfo->pQueue;
}

int VkeDevice::transfer_index() {
	return data->phyinfo->tQueue;
}

int VkeDevice::graphics_index() {
	return data->phyinfo->gQueue;
}

int VkeDevice::compute_index() {
	return data->phyinfo->cQueue;
}

/**
 * @brief get instance proc addresses for debug utils (name, label,...)
 * @param vkh device
 */
void VkeDevice::init_debug_utils () {
	SetDebugUtilsObjectNameEXT		= (PFN_vkSetDebugUtilsObjectNameEXT)	vkGetInstanceProcAddr(data->app->inst, "vkSetDebugUtilsObjectNameEXT");
	QueueBeginDebugUtilsLabelEXT	= (PFN_vkQueueBeginDebugUtilsLabelEXT)	vkGetInstanceProcAddr(data->app->inst, "vkQueueBeginDebugUtilsLabelEXT");
	QueueEndDebugUtilsLabelEXT		= (PFN_vkQueueEndDebugUtilsLabelEXT)	vkGetInstanceProcAddr(data->app->inst, "vkQueueEndDebugUtilsLabelEXT");
	CmdBeginDebugUtilsLabelEXT		= (PFN_vkCmdBeginDebugUtilsLabelEXT)	vkGetInstanceProcAddr(data->app->inst, "vkCmdBeginDebugUtilsLabelEXT");
	CmdEndDebugUtilsLabelEXT		= (PFN_vkCmdEndDebugUtilsLabelEXT)		vkGetInstanceProcAddr(data->app->inst, "vkCmdEndDebugUtilsLabelEXT");
	CmdInsertDebugUtilsLabelEXT		= (PFN_vkCmdInsertDebugUtilsLabelEXT)	vkGetInstanceProcAddr(data->app->inst, "vkCmdInsertDebugUtilsLabelEXT");
}

/* this one may not be used
VkeSampler::VkeSampler(VkeDevice vke, VkeFilter magFilter, VkeFilter minFilter, VkeSamplerMipmapMode mipmapMode, VkeSamplerAddressMode addressMode) {
	VkSampler sampler = VK_NULL_HANDLE;
	VkSamplerCreateInfo samplerCreateInfo = {
		.sType 			= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.maxAnisotropy	= 1.0,
		.addressModeU 	= addressMode,
		.addressModeV 	= addressMode,
		.addressModeW 	= addressMode,
		.magFilter		= magFilter,
		.minFilter		= minFilter,
		.mipmapMode		= mipmapMode};
	vk_assert(vkCreateSampler(vke->vk, &samplerCreateInfo, NULL, *data));
}*/

void vke_device_set_object_name (VkeDevice vke, VkObjectType objectType, uint64_t handle, const char* name){
	const VkDebugUtilsObjectNameInfoEXT info = {
		.sType		 = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext		 = 0,
		.objectType	 = objectType,
		.objectHandle= handle,
		.pObjectName = name
	};
	SetDebugUtilsObjectNameEXT (vke->vk, &info);
}

void VkeCommandBuffer::label_start(const char* name, const float color[4]) {
	const VkDebugUtilsLabelEXT info = {
		.sType		= VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext		= 0,
		.pLabelName= name
	};
	memcpy ((void*)info.color, (void*)color, 4 * sizeof(float));
	CmdBeginDebugUtilsLabelEXT(*data, &info);
}

void VkeCommandBuffer::label_insert(const char* name, const float color[4]) {
	const VkDebugUtilsLabelEXT info = {
		.sType		= VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext		= 0,
		.pLabelName= name
	};
	memcpy ((void*)info.color, (void*)color, 4 * sizeof(float));
	CmdInsertDebugUtilsLabelEXT(*data, &info);
}

void VkeCommandBuffer::label_end () {
	CmdEndDebugUtilsLabelEXT (*data);
}


static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}




array<VkeMonitor> VkeMonitor::enumerate() {
	int count;
	GLFWmonitor **glfwm = glfwGetMonitors(&count);
	static array<VkeMonitor> *cache;
	if (!cache) cache = new array<VkeMonitor>(count);
	for (int i = 0; i < count; i++)
		(*cache)[i] = VkeMonitor(glfwm[i]);
	return *cache;
}


void vke_poll_events() {
	glfwPollEvents();
}

/// imported from vkvg (it needs more use of vke in vkvg_device)
/// too many compile-time switches where you dont need them there
/// and that prevents the code from running elsewhere (no actual runtime benefit)



/// this creates the main app if needed
/// its less dimensional interface but more intuitive
/// VkeApp is 1:1 with VkeDevice
VkeDevice VkeDevice::select_device(VkeSelection selection, array<symbol> device_ext) {
	VkeApp        app = VkeApp::main(); /// its a bit toolbox to check support against 2 lists again via main? -- we could just assume and make the ext loading standard throughout.
	if     (!app) app = VkeApp(1, 2, "vkvg");
	
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	e->app.enable_debug(e->app);
#endif

	GLFWwindow *win_temp = glfwCreateWindow(128, 128, "glfw_temp", NULL, NULL);
	VkeSurfaceKHR surface;
	assert(surface.data);
	vk_assert (glfwCreateWindowSurface(app->inst, win_temp, NULL, &surface))
	array<VkePhyInfo> phys = app.hardware(surface);
	assert(phys.length());
	
	static auto query = [](array<VkePhyInfo> hardware, VkPhysicalDeviceType gpuType) -> VkePhyInfo {
		for (size_t i = 0; i < hardware.length(); i++)
			if (hardware[i]->gpu_props.deviceType == gpuType)
				return hardware[i];
		///
		return (vke_phyinfo*)null;
	};

	VkPhysicalDeviceType preferredGPU = (VkPhysicalDeviceType)(int)selection; /// these line up
	VkePhyInfo   pi = query(phys, preferredGPU);
	if (!pi)     pi = query(phys, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
	if (!pi)     pi = query(phys, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
	if (!pi)     return (vke_device*)null;
	uint32_t qCount = 0;
	float    qPriorities[] = {0.0};

	VkDeviceQueueCreateInfo pQueueInfos[] = {};
	if (pi.create_presentable_queues(1, qPriorities, &pQueueInfos[qCount]))
		qCount++;
	/*if (vke_phyinfo_create_compute_queues		(pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;
	if (vke_phyinfo_create_transfer_queues		(pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;*/

	array<symbol> exts;
	if (!device_ext) {
		/// default behavior is to load these two
		exts += "VK_KHR_swapchain";
		exts += "VK_KHR_portability_subset";
	} else
		exts = device_ext; /// 
	
	array<symbol> usable = pi.usable_extensions(exts);
	assert(usable.len() == 2);

	VkPhysicalDeviceFeatures enabledFeatures = {
		.fillModeNonSolid				= VK_TRUE,
		.sampleRateShading				= VK_TRUE,
		.logicOp						= VK_TRUE
	};
	/// supporting just 1.2 and up but may merge in things when needed
	VkPhysicalDeviceVulkan12Features enabledFeatures12 {
		.sType 							= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.scalarBlockLayout 				= VK_FALSE, //scalar_block_layout ? VK_TRUE : VK_FALSE,
		.timelineSemaphore 				= VK_TRUE, //timeline_semaphore  ? VK_TRUE : VK_FALSE,
		.pNext             				= NULL
	};
	VkDeviceCreateInfo device_info {
		.sType                   		= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount    		= qCount,
		.pQueueCreateInfos       		= (VkDeviceQueueCreateInfo*)&pQueueInfos,
		.enabledExtensionCount   		= usable.len(),
		.ppEnabledExtensionNames 		= usable.data,
		.pEnabledFeatures        		= &enabledFeatures,
		.pNext 					 		= &enabledFeatures12
	};
	return VkeDevice(app, pi, device_info);
}

const char *VkeWindow::clipboard() 					 { return glfwGetClipboardString(data->glfw);     }
void        VkeWindow::set_title(const char *cs)     {        glfwSetWindowTitle    (data->glfw, cs); }
void        VkeWindow::set_clipboard(const char* cs) {        glfwSetWindowTitle    (data->glfw, cs); }
void        VkeWindow::show() 						 {        glfwShowWindow        (data->glfw);     }
void        VkeWindow::hide() 						 {        glfwHideWindow        (data->glfw);     }
void       *VkeWindow::user_data() 					 { return data->user; 							  }


VkeBuffer::VkeBuffer(VkeDevice dev, VkeBufferUsageFlags usage, VkeMemoryUsage memprops, VkeDeviceSize size, bool mapped) : mx(&data) {
	data->dev			        = dev;
	VkBufferCreateInfo*   pInfo = &data->infos;
	pInfo->sType				= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	pInfo->usage				= usage;
	pInfo->size					= size;
	pInfo->sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	data->allocCreateInfo.usage	= (VmaMemoryUsage)memprops;
	data->allocCreateInfo.flags = mapped ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0L;
	vk_assert(vmaCreateBuffer(dev->allocator, pInfo, &data->allocCreateInfo, &data->buffer, &data->alloc, &data->allocInfo));
}

void VkeBuffer::reset() {
	if (data->buffer)
		vmaDestroyBuffer(data->dev->allocator, data->buffer, data->alloc);
}

void VkeBuffer::resize(VkeDeviceSize newSize, bool mapped) {
	reset();
	data->infos.size = newSize;
	vk_assert(vmaCreateBuffer(
		data->dev->allocator, &data->infos, &data->allocCreateInfo, &data->buffer, &data->alloc, &data->allocInfo));
}

VkeDescriptorBufferInfo VkeBuffer::get_descriptor() {
	VkDescriptorBufferInfo desc = {
		.buffer = data->buffer,
		.offset = 0,
		.range	= VK_WHOLE_SIZE};
	return desc;
}

VkeResult VkeBuffer::map  			   () { return vmaMapMemory(data->dev->allocator, data->alloc, &data->mapped); }
void      VkeBuffer::unmap		       () { vmaUnmapMemory(data->dev->allocator, data->alloc); }
void*     VkeBuffer::get_mapped_pointer() { return data->allocInfo.pMappedData; }
void      VkeBuffer::flush 			   () { vmaFlushAllocation (data->dev->allocator, data->alloc, data->allocInfo.offset, data->allocInfo.size); }

/// create a window, register callbacks in one shot
VkeWindow::VkeWindow(
		VkeDevice &dev, VkeMonitor mon, void* user,
		int w, int h, const char *title,
		VkeResizeEvent resize_event,
		VkeKeyEvent       key_event,
		VkeCursorEvent cursor_event,
		VkeCharEvent     char_event,
		VkeButtonEvent button_event) : mx(&data)
{
	data->dev        = dev;
	data->w          = w;
	data->h          = h;
	data->glfw       = glfwCreateWindow(w, h, title, *mon, null);
	data->surface    = (VkSurfaceKHR*)calloc(1, sizeof(VkSurfaceKHR));
	VkInstance inst  = dev->app->inst; // todo: import device by creating app, returning its device
	///
	vk_assert (glfwCreateWindowSurface(inst, data->glfw, NULL, (VkSurfaceKHR *)data->surface));
	///
	data->renderer   = VkePresenter(
		dev, (uint32_t) dev->phyinfo->pQueue, data->surface, w, h,
		VK_FORMAT_B8G8R8A8_UNORM, VK_PRESENT_MODE_FIFO_KHR); // should be a kind of configuration call
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);
	glfwWindowHint(GLFW_FLOATING,   GLFW_FALSE);
	glfwWindowHint(GLFW_DECORATED,  GLFW_TRUE);
	///
	glfwSetFramebufferSizeCallback(data->glfw, glfw_resize);
	glfwSetKeyCallback            (data->glfw, glfw_key);
	glfwSetCursorPosCallback      (data->glfw, glfw_cursor);
	glfwSetCharCallback           (data->glfw, glfw_char);
	glfwSetMouseButtonCallback    (data->glfw, glfw_button);
}

#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

VkeFence::VkeFence(VkeDevice vke, bool signalled) : mx(&data) {
	VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
									.pNext = NULL,
									.flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0 };
	vk_assert(vkCreateFence(vke->vk, &fenceInfo, NULL, data));
}

///
VkeSemaphore::VkeSemaphore(VkeDevice &vke, uint64_t initialValue) {
	/// we register a timeline-based Semaphore if initialValue != 0
	VkSemaphoreTypeCreateInfo t_info = {
		.sType 			= VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext 			= NULL,
		.semaphoreType 	= VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue 	= initialValue
	};
	VkSemaphoreCreateInfo info {
		.sType 			= VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext 			= initialValue ? &t_info : null,
		.flags 			= 0
	};
	/// .semaphoreType 	= initialValue ? VK_SEMAPHORE_TYPE_TIMELINE : VK_SEMAPHORE_TYPE_BINARY,
	vk_assert (vkCreateSemaphore (vke->vk, &info, NULL, data));
}

VkeCommandPool::VkeCommandPool(VkeDevice &vke, uint32_t qFamIndex, VkeCommandPoolCreateFlags flags) : mx(&data) {
	VkCommandPool 			cmd_pool;
	VkCommandPoolCreateInfo cmd_pool_info {
		.sType 				= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext 				= NULL,
		.queueFamilyIndex 	= qFamIndex,
		.flags 				= flags
	};
	vk_assert (vkCreateCommandPool (vke->vk, &cmd_pool_info, NULL, &cmd_pool));
}

VkeCommandBuffer::VkeCommandBuffer(VkeDevice &vke, VkeCommandPool cmd_pool, VkeCommandBufferLevel level, uint32_t count) : mx(&data) {
	VkCommandBufferAllocateInfo cmd {
		.sType 				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext 				= NULL,
		.commandPool 		= cmd_pool,
		.level 				= level,
		.commandBufferCount = count
	};
	vk_assert (vkAllocateCommandBuffers (vke, &cmd, data));
}

void VkeCommandBuffer::cmd_begin(VkeCommandBufferUsageFlags flags) {
	VkCommandBufferBeginInfo cmd_buf_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = NULL,
		.flags = flags,
		.pInheritanceInfo = NULL
	};
	vk_assert (vkBeginCommandBuffer (*data, &cmd_buf_info));
}

void VkeCommandBuffer::cmd_end(){
	vk_assert (vkEndCommandBuffer (*data));
}

void VkeQueue::submit(
		VkeFence 					 fence,
		array<VkeCommandBuffer>  	 cmd_buffers,
		array<VkePipelineStageFlags> wait_stages, 	  /// --> must be size of wait_semaphores len() <--
		array<VkeSemaphore> 		 wait_semaphores, array<VkeSemaphore> signal_semaphores,
		array<u64> 					 wait_values, 	  array<u64> 		  signal_values)
{
	auto e = vke_queue::next_entry();
	/// copy wrapped data into contiguous vector (max of 2 in vkvg)
	for (auto &v:       wait_values)       wait_values += v;
	for (auto &v:     signal_values)     signal_values += v;
	for (auto &v:       cmd_buffers)       cmd_buffers += v;
	for (auto &v:       wait_stages)       wait_stages += v;
	for (auto &v:   wait_semaphores)   wait_semaphores += v;
	for (auto &v: signal_semaphores) signal_semaphores += v;
	for (auto &v:       wait_values)       wait_values += v;
	for (auto &v:     signal_values)     signal_values += v;
	///
	VkTimelineSemaphoreSubmitInfo timeline_info;
	timeline_info.sType 					= VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	timeline_info.pNext 					= NULL;
	timeline_info.waitSemaphoreValueCount 	= e.wait_values.len();
	timeline_info.pWaitSemaphoreValues 		= e.wait_values.data;
	timeline_info.signalSemaphoreValueCount	= e.signal_values.len();
	timeline_info.pSignalSemaphoreValues 	= e.signal_values.data;
	/// --------------------------------------------
	assert (wait_semaphores.len() == wait_stages.len());
	/// --------------------------------------------
	VkSubmitInfo submit_info;
	submit_info.sType 				 = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext 				 = &timeline_info;
	submit_info.pWaitDstStageMask	 = e.wait_stages.data;
	submit_info.waitSemaphoreCount 	 = e.wait_semaphores.len();
	submit_info.pWaitSemaphores 	 = e.wait_semaphores.data;
	submit_info.signalSemaphoreCount = e.signal_semaphores.len();
	submit_info.pSignalSemaphores 	 = e.signal_semaphores.data;
	submit_info.commandBufferCount 	 = e.cmd_buffers.len();
	submit_info.pCommandBuffers 	 = e.cmd_buffers.data;
	/// --------------------------------------------
	vk_assert (vkQueueSubmit(data->queue, 1, &submit_info, fence));
}

///
bool VkeSemaphore::wait(VkeDevice &vke, VkeSemaphore timeline, const uint64_t wait) {
	VkSemaphoreWaitInfo w;
	///
	w.sType			= VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	w.pNext			= NULL;
	w.flags			= 0;
	w.semaphoreCount = 1;
	w.pSemaphores	= &timeline;
	w.pValues		= &wait;
	///
	return vkWaitSemaphores(vke->vk, &w, UINT64_MAX) == VK_SUCCESS;
}

/**
bool vke_memory_type_from_properties(VkePhysicalDeviceMemoryProperties* memory_properties, uint32_t typeBits, VkeMemoryUsage memUsage, uint32_t *typeIndex) {
	VkMemoryPropertyFlagBits memFlags = 0;
	///
	switch(memUsage) {
		case VKE_MEMORY_USAGE_GPU_ONLY:				memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;     break;
		case VKE_MEMORY_USAGE_CPU_ONLY: 			memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; break;
		case VKE_MEMORY_USAGE_CPU_TO_GPU: 			memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; break;
		case VKE_MEMORY_USAGE_GPU_TO_CPU: 		    memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;   break;
		case VKE_MEMORY_USAGE_CPU_COPY: 		    memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;     break;
		case VKE_MEMORY_USAGE_GPU_LAZILY_ALLOCATED: memFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT; break;
		default: break;
	}

	// Search memtypes to find first index with those properties
	for (uint32_t i = 0; i < memory_properties->memoryTypeCount; i++) {
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
*/

VkeSampleCountFlagBits VkeSampler::max_samples(VkeSampleCountFlagBits counts) {
	VkSampleCountFlagBits v = *counts;
	if (v & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (v & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (v & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (v & VK_SAMPLE_COUNT_8_BIT)  { return VK_SAMPLE_COUNT_8_BIT;  }
	if (v & VK_SAMPLE_COUNT_4_BIT)  { return VK_SAMPLE_COUNT_4_BIT;  }
	if (v & VK_SAMPLE_COUNT_2_BIT)  { return VK_SAMPLE_COUNT_2_BIT;  }
	return  VK_SAMPLE_COUNT_1_BIT;
}

VkeShaderModule VkeShaderModule::load(VkeDevice vke, const char* path){
	VkeShaderModule module;
	size_t filelength;
	uint32_t* pCode = (uint32_t*)read_spv(path, &filelength);
	VkShaderModuleCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
											.pCode = pCode,
											.codeSize = filelength };
	vk_assert(vkCreateShaderModule(vke->vk, &createInfo, NULL, &module));
	free (pCode);
	//assert(module != VK_NULL_HANDLE);
	return module;
}

char *read_spv(const char *filename, size_t *psize) {
	size_t size;
	size_t retval;
	char *shader_code;
// todo investigate:
//#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
//	filename =[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent: @(filename)].UTF8String;
//#endif
	FILE *fp = fopen(filename, "rb");
	if  (!fp) return NULL;
	///
	fseek(fp, 0L, SEEK_END);
	size = (size_t)ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	shader_code = (char*)malloc(size);
	retval = fread(shader_code, size, 1, fp);
	assert(retval == 1);
	*psize = size;
	fclose(fp);
	///
	return shader_code;
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
	vk_assert (vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL));
	if (instance_layer_count == 0)
		return;
	VkLayerProperties* vk_props = (VkLayerProperties*)malloc (instance_layer_count * sizeof(VkLayerProperties));
	vk_assert (vkEnumerateInstanceLayerProperties(&instance_layer_count, vk_props));

	for (uint32_t i = 0; i < instance_layer_count; i++) {
		printf ("\t%s, %s\n", vk_props[i].layerName, vk_props[i].description);
		/*		  res = init_global_extension_properties(layer_props);
		if (res) return res;
		info.instance_layer_properties.push_back(layer_props);*/
	}
	free(vk_props);
}

VkeQueue::VkeQueue(VkeDevice dev, uint32_t familyIndex, uint32_t qIndex) : mx(&data) {
	data->dev = dev;
	data->familyIndex = familyIndex;
	vkGetDeviceQueue (dev, familyIndex, qIndex, &data->queue);
}

VkePhyInfo::VkePhyInfo(VkeGPU gpu, VkeSurfaceKHR surface) : mx(&data) {
	data->gpu = gpu;
	vkGetPhysicalDeviceProperties (gpu, &data->gpu_props);
	vkGetPhysicalDeviceMemoryProperties (gpu, &data->mem_props);
	vkGetPhysicalDeviceQueueFamilyProperties (gpu, &data->queueCount, NULL);
	data->queues = (VkQueueFamilyProperties*)malloc(data->queueCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties (gpu, &data->queueCount, data->queues);

	//identify dedicated queues
	data->cQueue = -1;
	data->gQueue = -1;
	data->tQueue = -1;
	data->pQueue = -1;

	//try to find dedicated queues first
	for (uint32_t j=0; j<data->queueCount; j++){
		VkBool32 present = VK_FALSE;
		switch (data->queues[j].queueFlags) {
		case VK_QUEUE_GRAPHICS_BIT:
			if (*surface)
				vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, surface, &present);
			if (present){
				if (data->pQueue<0)
					data->pQueue = j;
			}else if (data->gQueue<0)
				data->gQueue = j;
			break;
		case VK_QUEUE_COMPUTE_BIT:
			if (data->cQueue<0)
				data->cQueue = j;
			break;
		case VK_QUEUE_TRANSFER_BIT:
			if (data->tQueue<0)
				data->tQueue = j;
			break;
		}
	}
	//try to find suitable queue if no dedicated one found
	for (uint32_t j=0; j<data->queueCount; j++){
		if (data->queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			VkBool32 present = 0;
			if (*surface)
				vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, surface, &present);
			//printf ("surf=%d, q=%d, present=%d\n",surface,j,present);
			if (present){
				if (data->pQueue<0)
					data->pQueue = j;
			} else if (data->gQueue<0)
				data->gQueue = j;
		}
		if ((data->queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (data->gQueue < 0))
			data->gQueue = j;
		if ((data->queues[j].queueFlags & VK_QUEUE_COMPUTE_BIT) && (data->cQueue < 0))
			data->cQueue = j;
		if ((data->queues[j].queueFlags & VK_QUEUE_TRANSFER_BIT) && (data->tQueue < 0))
			data->tQueue = j;
	}
}

/// bring in gpu boot strapping from pascal project
/*
int vke_phyinfo_enumerate(VkeMonitor mon, VkePhyInfo *info) {
	VkeApp     app  = vke_app_main();
	if       (!app) = vke_app_create(1, 2, "vke", 0, NULL, 0, NULL);
    VkInstance inst = vke_app_vkinst(app);
    u32        gpu_count;

    /// gpu count
    vkEnumeratePhysicalDevices(inst, &gpu_count, null);

    /// facilitate vk
    static VkPhysicalDevice *gpus = (VkPhysicalDevice*)calloc(gpu_count, sizeof(VkPhysicalDevice));

	/// facilitate user
	static VkPhysicalDevice *info = (VkPhysicalDevice*)calloc(gpu_count, sizeof(VkePhyInfo));

	VkSurfaceKHR surf;
	GLFWwindow  *class2_probe = glfwCreateWindow(128, 128, "", mon ? mon->data : NULL, NULL);
	glfwCreateWindowSurface(inst)
	for (int i = 0; i < gpu_count; i++) {
		info[i] = vke_phyinfo_create(gpus[i], surf);
	}
	///
    static bool init;
    if (!init) {
        init = true;
        vkEnumeratePhysicalDevices(inst, &gpu_count, (VkPhysicalDevice*)hw.data());
    }
    return hw;
}
*/

void VkePhyInfo::set_dpi(vec2i dpi) {
	data->dpi = dpi;
}

vec2i VkePhyInfo::get_dpi() {
	return data->dpi;
}

bool VkePhyInfo::create_queues(int qFam, uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (data->queues[qFam].queueCount < queueCount)
		fprintf(stderr, "Request %d queues of family %d, but only %d available\n", queueCount, qFam, data->queues[qFam].queueCount);
	else {
		qInfo->queueCount = queueCount,
		qInfo->queueFamilyIndex = qFam,
		qInfo->pQueuePriorities = queue_priorities;
		data->queues[qFam].queueCount -= queueCount;
		return true;
	}
	return false;
}

bool VkePhyInfo::create_presentable_queues(uint32_t queueCount, const float* queue_priorities, VkeDeviceQueueCreateInfo qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (data->pQueue < 0)
		perror("No queue with presentable support found");
	else if (data->queues[data->pQueue].queueCount < queueCount)
		fprintf(stderr, "Request %d queues of family %d, but only %d available\n",
			queueCount, data->pQueue, data->queues[data->pQueue].queueCount);
	else {
		qInfo->queueCount = queueCount,
		qInfo->queueFamilyIndex = data->pQueue,
		qInfo->pQueuePriorities = queue_priorities;
		data->queues[data->pQueue].queueCount -= queueCount;
		return true;
	}
	return false;
}

bool VkePhyInfo::create_transfer_queues(uint32_t queueCount, const float* queue_priorities, VkeDeviceQueueCreateInfo qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (data->tQueue < 0)
		perror("No transfer queue found");
	else if (data->queues[data->tQueue].queueCount < queueCount)
		fprintf(stderr, "Request %d transfer queues of family %d, but only %d available\n", queueCount, data->tQueue, data->queues[data->tQueue].queueCount);
	else {
		qInfo->queueCount = queueCount;
		qInfo->queueFamilyIndex = data->tQueue;
		qInfo->pQueuePriorities = queue_priorities;
		data->queues[data->tQueue].queueCount -= queueCount;
		return true;
	}
	return false;
}

bool VkePhyInfo::create_compute_queues(uint32_t queueCount, const float* queue_priorities, VkeDeviceQueueCreateInfo qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (data->cQueue < 0)
		perror("No compute queue found");
	else if (data->queues[data->cQueue].queueCount < queueCount)
		fprintf(stderr, "Request %d compute queues of family %d, but only %d available\n", queueCount, data->cQueue, data->queues[data->cQueue].queueCount);
	else {
		qInfo->queueCount = queueCount,
		qInfo->queueFamilyIndex = data->cQueue,
		qInfo->pQueuePriorities = queue_priorities;
		data->queues[data->cQueue].queueCount -= queueCount;
		return true;
	}
	return false;
}

bool VkePhyInfo::create_graphic_queues (uint32_t queueCount, const float* queue_priorities, VkeDeviceQueueCreateInfo qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (data->gQueue < 0)
		perror("No graphic queue found");
	else if (data->queues[data->gQueue].queueCount < queueCount)
		fprintf(stderr, "Request %d graphic queues of family %d, but only %d available\n", queueCount, data->gQueue, data->queues[data->gQueue].queueCount);
	else {
		qInfo->queueCount = queueCount,
		qInfo->queueFamilyIndex = data->gQueue,
		qInfo->pQueuePriorities = queue_priorities;
		data->queues[data->gQueue].queueCount -= queueCount;
		return true;
	}
	return false;
}

#pragma GCC diagnostic pop