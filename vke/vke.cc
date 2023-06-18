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

namespace ion {

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

ptr_implement(VkeGPU, 		mx);
ptr_implement(VkeSurface, 	mx);
ptr_implement(VkeApp, 		mx);
ptr_implement(VkeWindow, 	mx);
ptr_implement(VkeMonitor, 	mx);

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
	VkSurfaceKHR      *surf;
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
};

struct vke_app {
    struct ivke_app *data;
    
	VkInstance  		 		inst;
	VkDebugUtilsMessengerEXT 	debugMessenger;

   ~vke_app();
};

/// just 1 structure, thats it. one.  no inheritence, and smart pointers
struct vke_device {
	VkDevice     	    vk;
	VmaAllocator		allocator;
	VkePhyInfo			phyinfo;
	VkeApp				app; 		/// contains instance

    static VkeDevice create (VkeApp app, VkePhyInfo phyInfo, VkDeviceCreateInfo* pDevice_info);
    static VkeDevice import (VkInstance inst, VkPhysicalDevice phy, VkDevice dev);

    void             init_debug_utils ();
    VkDevice         vk               ();
    VkPhysicalDevice gpu              ();
    VkePhyInfo       phyinfo          ();
    VkeApp	         app              ();
    void             set_object_name  (VkObjectType objectType, uint64_t handle, const char *name);
    VkSampler        sampler          (VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mipmap, VkSamplerAddressMode sampler_amode);
    void             destroy_sampler  (VkSampler sampler);
    const void*      get_device_requirements (VkPhysicalDeviceFeatures* pEnabledFeatures);
    int              present_index    ();
    int              transfer_index   ();
    int              graphics_index   ();
    int              compute_index    ();
};

struct vke_image {
	VkDevice				dev;
	VkImageCreateInfo		infos;
	VkImage					image;
	VmaAllocation			alloc;
	VmaAllocationInfo		allocInfo;
	VkSampler				sampler;
	VkImageView				view;
	VkImageLayout			layout; //current layout
	bool					imported;//dont destroy vkimage at end
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
	uint32_t							qCreateInfosCount;
	VkDeviceQueueCreateInfo*			qCreateInfos;
	VkExtensionProperties*				pExtensionProperties;
	uint32_t							extensionCount;
};

struct vke_presenter {
	VkQueue>		        queue;
	VkCommandPool>       	cmdPool;
	uint32_t		        qFam;
	VkeDevice		        dev;
	VkSurfaceKHR        	surface;
	VkSemaphore		    	semaPresentEnd;
	VkSemaphore		    	semaDrawEnd;
	VkFence			    	fenceDraw;
	VkFormat		    	format;
	VkColorSpaceKHR     	colorSpace;
	VkPresentModeKHR    	presentMode;
	uint32_t		        width;
	uint32_t		        height;
	uint32_t		        imgCount;
	uint32_t		        currentScBufferIndex;
	VkRenderPass>	    	renderPass;
	VkSwapchainKHR>	    	swapChain;
	VkImage*		        ScBuffers;
	VkCommandBuffer*        cmdBuffs;
	VkFramebuffer*	        frameBuffs;
};

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

/// abstract away these args its only usage and memprops we need isolated
void vke_buffer_init(VkeDevice dev, VkBufferUsageFlags usage, VkeMemoryUsage memprops, VkDeviceSize size, VkeBuffer buff, bool mapped){
	buff->dev			= dev;
	VkBufferCreateInfo* pInfo = &buff->infos;
	pInfo->sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	pInfo->usage		= usage;
	pInfo->size			= size;
	pInfo->sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	buff->allocCreateInfo.usage	= (VmaMemoryUsage)memprops;
	if (mapped)
		buff->allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	vk_assert(vmaCreateBuffer(dev->allocator, pInfo, &buff->allocCreateInfo, &buff->buffer, &buff->alloc, &buff->allocInfo));
}

void 	   vke_buffer_free(VkeBuffer buff) {
	if (buff->buffer)
		vmaDestroyBuffer(buff->dev->allocator, buff->buffer, buff->alloc);
	free(buff);
	buff = NULL;
}

void vke_buffer_resize(VkeBuffer buff, VkDeviceSize newSize, bool mapped){
	vke_buffer_reset(buff);
	buff->infos.size = newSize;
	vk_assert(vmaCreateBuffer(buff->dev->allocator, &buff->infos, &buff->allocCreateInfo, &buff->buffer, &buff->alloc, &buff->allocInfo));
}

VkeBuffer vke_buffer_create(VkeDevice dev, VkBufferUsageFlags usage, VkeMemoryUsage memprops, VkDeviceSize size) {
	VkeBuffer buff = io_new(vke_buffer);
	vke_buffer_init(dev, usage, memprops, size, buff, false);
	return buff;
}

void vke_buffer_reset(VkeBuffer buff) {
	if (buff->buffer)
		vmaDestroyBuffer(buff->dev->allocator, buff->buffer, buff->alloc);
}

VkDescriptorBufferInfo vke_buffer_get_descriptor (VkeBuffer buff){
	VkDescriptorBufferInfo desc = {
		.buffer = buff->buffer,
		.offset = 0,
		.range	= VK_WHOLE_SIZE};
	return desc;
}

VkResult vke_buffer_map  			  (VkeBuffer buff) { return vmaMapMemory(buff->dev->allocator, buff->alloc, &buff->mapped); }
void     vke_buffer_unmap		      (VkeBuffer buff) { vmaUnmapMemory(buff->dev->allocator, buff->alloc); }
VkBuffer vke_buffer_get_vkbuffer 	  (VkeBuffer buff) { return buff->buffer; }
void*    vke_buffer_get_mapped_pointer(VkeBuffer buff) { return buff->allocInfo.pMappedData; }
void     vke_buffer_flush 			  (VkeBuffer buff) { vmaFlushAllocation (buff->dev->allocator, buff->alloc, buff->allocInfo.offset, buff->allocInfo.size); }
void 	   vke_image_free			  (VkeImage   img) { 
	if ( img->view    != VK_NULL_HANDLE) vkDestroyImageView(img->dev->vk,img->view, NULL);
	if ( img->sampler != VK_NULL_HANDLE) vkDestroySampler  (img->dev->vk,img->sampler, NULL);
	if (!img->imported) 			     vmaDestroyImage   (img->dev->allocator, img->image, img->alloc);
}

VkeImage vke_image_create (
		VkeDevice dev, VkImageType imageType, VkFormat format, uint32_t width, uint32_t height,
		VkeMemoryUsage memprops, VkImageUsageFlags usage, VkSampleCountFlagBits samples, VkImageTiling tiling,
		uint32_t mipLevels, uint32_t arrayLayers) {
	///
	VkeImage img = io_new(vke_image);
	///
	img->dev 				 = dev;
	VkImageCreateInfo* pInfo = &img->infos;
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
	vk_assert(vmaCreateImage (dev->allocator, pInfo, &allocInfo, &img->image, &img->alloc, &img->allocInfo));
	return img;
}

void vke_image_reference (VkeImage img) {
	io_grab(img);
}

VkeImage vke_tex2d_array_create (VkeDevice dev,
							 VkFormat format, uint32_t width, uint32_t height, uint32_t layers,
							 VkeMemoryUsage memprops, VkImageUsageFlags usage){
	return vke_image_create (dev, VK_IMAGE_TYPE_2D, format, width, height, memprops,usage,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 1, layers);
}

// 1x1 basic sample
VkeImage vke_image_create_basic(VkeDevice dev,
						   VkFormat format, uint32_t width, uint32_t height, VkImageTiling tiling,
						   VkeMemoryUsage memprops,
						   VkImageUsageFlags usage) {
	return vke_image_create (dev, VK_IMAGE_TYPE_2D, format, width, height, memprops,usage,
					  VK_SAMPLE_COUNT_1_BIT, tiling, 1, 1);
}
//create vkhImage from existing VkImage
VkeImage vke_image_import (VkeDevice dev, VkImage vkImg, VkFormat format, uint32_t width, uint32_t height) {
	VkeImage    img = io_new(vke_image); /// new creates with sync, alloc does not.  alloc = less overhead.. its actually allocating the mtx_t space but not initializing it
	img->dev 		= dev;
	img->image		= vkImg;
	img->imported	= true;

	VkImageCreateInfo* pInfo = &img->infos;
	pInfo->imageType		= VK_IMAGE_TYPE_2D;
	pInfo->format			= format;
	pInfo->extent.width		= width;
	pInfo->extent.height	= height;
	pInfo->extent.depth		= 1;
	pInfo->mipLevels		= 1;
	pInfo->arrayLayers		= 1;
	//pInfo->samples		= samples;
	return img;
}

VkeImage vke_image_ms_create(VkeDevice dev,
						   VkFormat format, VkSampleCountFlagBits num_samples, uint32_t width, uint32_t height,
						   VkeMemoryUsage memprops,
						   VkImageUsageFlags usage){
   return  vke_image_create (dev, VK_IMAGE_TYPE_2D, format, width, height, memprops,usage,
					  num_samples, VK_IMAGE_TILING_OPTIMAL, 1, 1);
}
void vke_image_create_view (VkeImage img, VkImageViewType viewType, VkImageAspectFlags aspectFlags){
	if(img->view != VK_NULL_HANDLE)
		vkDestroyImageView	(img->dev->vk,img->view,NULL);

	VkImageViewCreateInfo viewInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
										 .image = img->image,
										 .viewType = viewType,
										 .format = img->infos.format,
										 .components = {VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B,VK_COMPONENT_SWIZZLE_A},
										 .subresourceRange = {aspectFlags,0,1,0,img->infos.arrayLayers}};
	vk_assert(vkCreateImageView(img->dev->vk, &viewInfo, NULL, &img->view));
}
void vke_image_create_sampler (VkeImage img, VkFilter magFilter, VkFilter minFilter,
							   VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode){
	if(img->sampler != VK_NULL_HANDLE)
		vkDestroySampler	(img->dev->vk,img->sampler,NULL);
	VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
											  .maxAnisotropy= 1.0,
											  .addressModeU = addressMode,
											  .addressModeV = addressMode,
											  .addressModeW = addressMode,
											  .magFilter	= magFilter,
											  .minFilter	= minFilter,
											  .mipmapMode	= mipmapMode};
	vk_assert(vkCreateSampler(img->dev->vk, &samplerCreateInfo, NULL, &img->sampler));
}
void vke_image_set_sampler (VkeImage img, VkSampler sampler){
	img->sampler = sampler;
}
void vke_image_create_descriptor(VkeImage img, VkImageViewType viewType, VkImageAspectFlags aspectFlags, VkFilter magFilter,
								 VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode)
{
	vke_image_create_view		(img, viewType, aspectFlags);
	vke_image_create_sampler	(img, magFilter, minFilter, mipmapMode, addressMode);
}
VkImage vke_image_get_vkimage (VkeImage img){
	return img->image;
}
VkSampler vke_image_get_sampler (VkeImage img){
	if (img == NULL)
		return NULL;
	return img->sampler;
}
VkImageView vke_image_get_view (VkeImage img){
	if (img == NULL)
		return NULL;
	return img->view;
}
VkImageLayout vke_image_get_layout (VkeImage img){
	if (img == NULL)
		return VK_IMAGE_LAYOUT_UNDEFINED;
	return img->layout;
}
VkDescriptorImageInfo vke_image_get_descriptor (VkeImage img, VkImageLayout imageLayout){
	VkDescriptorImageInfo desc = { .imageView = img->view,
								   .imageLayout = imageLayout,
								   .sampler = img->sampler };
	return desc;
}

void vke_image_set_layout(VkCommandBuffer cmdBuff, VkeImage image, VkImageAspectFlags aspectMask,
						  VkImageLayout old_image_layout, VkImageLayout new_image_layout,
					  VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
	VkImageSubresourceRange subres = {aspectMask,0,1,0,1};
	vke_image_set_layout_subres(cmdBuff, image, subres, old_image_layout, new_image_layout, src_stages, dest_stages);
}

void vke_image_set_layout_subres(VkCommandBuffer cmdBuff, VkeImage image, VkImageSubresourceRange subresourceRange,
							 VkImageLayout old_image_layout, VkImageLayout new_image_layout,
							 VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
	VkImageMemoryBarrier image_memory_barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
												  .oldLayout = image->layout,
												  .newLayout = new_image_layout,
												  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
												  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
												  .image = image->image,
												  .subresourceRange = subresourceRange};

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
	image->layout = new_image_layout;
}
void vke_image_destroy_sampler (VkeImage img) {
	if (img==NULL)
		return;
	if(img->sampler != VK_NULL_HANDLE)
		vkDestroySampler	(img->dev->vk,img->sampler,NULL);
	img->sampler = VK_NULL_HANDLE;
}

void* vke_image_map (VkeImage img) {
	void* data;
	vmaMapMemory(img->dev->allocator, img->alloc, &data);
	return data;
}
void vke_image_unmap (VkeImage img) {
	vmaUnmapMemory(img->dev->allocator, img->alloc);
}
void vke_image_set_name (VkeImage img, const char* name){
	if (img==NULL)
		return;
	vke_device_set_object_name(img->dev, VK_OBJECT_TYPE_IMAGE, (uint64_t)img->image, name);
}
uint64_t vke_image_get_stride (VkeImage img) {
	VkImageSubresource subres = {VK_IMAGE_ASPECT_COLOR_BIT,0,0};
	VkSubresourceLayout layout = {0};
	vkGetImageSubresourceLayout(img->dev->vk, img->image, &subres, &layout);
	return (uint64_t) layout.rowPitch;
}

// using the vke.  im not sure if this matters
//#define FENCE_TIMEOUT UINT16_MAX

void vke_presenter_create_swapchain(VkePresenter r);

void vke_presenter_swapchain_destroy (VkePresenter r) {
	for (uint32_t i = 0; i < r->imgCount; i++) {
		io_drop(vke_image, r->ScBuffers[i]); // should call them images?
		vkFreeCommandBuffers (r->dev->vk, r->cmdPool, 1, &r->cmdBuffs[i]);
	}
	vkDestroySwapchainKHR (r->dev->vk, r->swapChain, NULL);
	r->swapChain = VK_NULL_HANDLE;
	free(r->ScBuffers);
	free(r->cmdBuffs);
}

void 		  vke_presenter_free(VkePresenter r) {
	vkDeviceWaitIdle (r->dev->vk);
	vke_presenter_swapchain_destroy(r);
	vkDestroySemaphore	(r->dev->vk, r->semaDrawEnd,    NULL);
	vkDestroySemaphore	(r->dev->vk, r->semaPresentEnd, NULL);
	vkDestroyFence		(r->dev->vk, r->fenceDraw,      NULL);
	vkDestroyCommandPool(r->dev->vk, r->cmdPool,        NULL);
}

void _init_phy_surface	(VkePresenter r, VkFormat preferedFormat, VkPresentModeKHR presentMode);

/// there is so much repeating on this stuff; this presentQueueFamIdx you can get from Device. and, never would you get anything else from deive there arent Multiple presents
VkePresenter vke_presenter_create (VkeDevice dev, uint32_t presentQueueFamIdx, VkSurfaceKHR surface, uint32_t width, uint32_t height,
						   VkFormat preferedFormat, VkPresentModeKHR presentMode) {
	
	VkePresenter r = io_new(vke_presenter);
	r->dev     = dev;
	r->qFam    = presentQueueFamIdx ;
	r->surface = surface;
	r->width   = width;
	r->height  = height;
	vkGetDeviceQueue(r->dev->vk, r->qFam, 0, &r->queue);

	r->cmdPool			= vke_cmd_pool_create  (r->dev, presentQueueFamIdx, 0);
	r->semaPresentEnd	= vke_semaphore_create (r->dev);
	r->semaDrawEnd		= vke_semaphore_create (r->dev);
	r->fenceDraw		= vke_fence_create_signaled(r->dev);

	_init_phy_surface (r, preferedFormat, presentMode);
	vke_presenter_create_swapchain (r);
	return r;
}

bool vke_presenter_acquireNextImage (VkePresenter r, VkFence fence, VkSemaphore semaphore) {
	// Get the index of the next available swapchain image:
	VkResult err = vkAcquireNextImageKHR
			(r->dev->vk, r->swapChain, UINT64_MAX, semaphore, fence, &r->currentScBufferIndex);
	return ((err != VK_ERROR_OUT_OF_DATE_KHR) && (err != VK_SUBOPTIMAL_KHR));
}


bool vke_presenter_draw (VkePresenter r) {
	if (!vke_presenter_acquireNextImage (r, VK_NULL_HANDLE, r->semaPresentEnd)){
		vke_presenter_create_swapchain (r);
		return false;
	}

	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 .commandBufferCount = 1,
								 .signalSemaphoreCount = 1,
								 .pSignalSemaphores = &r->semaDrawEnd,
								 .waitSemaphoreCount = 1,
								 .pWaitSemaphores = &r->semaPresentEnd,
								 .pWaitDstStageMask = &dstStageMask,
								 .pCommandBuffers = &r->cmdBuffs[r->currentScBufferIndex]};

	vkWaitForFences	(r->dev->vk, 1, &r->fenceDraw, VK_TRUE, FENCE_TIMEOUT);
	vkResetFences	(r->dev->vk, 1, &r->fenceDraw);

	vk_assert(vkQueueSubmit (r->queue, 1, &submit_info, r->fenceDraw));

	/* Now present the image in the window */
	VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
								 .swapchainCount = 1,
								 .pSwapchains = &r->swapChain,
								 .waitSemaphoreCount = 1,
								 .pWaitSemaphores = &r->semaDrawEnd,
								 .pImageIndices = &r->currentScBufferIndex };

	/* Make sure command buffer is finished before presenting */
	vkQueuePresentKHR(r->queue, &present);
	return true;
}

void vke_presenter_build_blit_cmd (VkePresenter r, VkImage blitSource, uint32_t width, uint32_t height){

	uint32_t w = MIN(width, r->width), h = MIN(height, r->height);

	for (uint32_t i = 0; i < r->imgCount; ++i)
	{
		VkImage bltDstImage = r->ScBuffers[i]->image;
		VkCommandBuffer cb = r->cmdBuffs[i];

		vke_cmd_begin(cb,0);

		set_image_layout(cb, bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		set_image_layout(cb, blitSource, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		/*VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
								.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
								.srcOffset = {0},
								.dstOffset = {0,0,0},
								.extent = {MIN(w,r->width), MIN(h,r->height),1}};*/
		VkImageBlit bregion = { .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
								.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
								.srcOffsets[0] = {0},
								.srcOffsets[1] = {w, h, 1},
								.dstOffsets[0] = {0},
								.dstOffsets[1] = {width, height, 1}
							  };

		vkCmdBlitImage(cb, blitSource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bregion, VK_FILTER_NEAREST);

		/*vkCmdCopyImage(cb, blitSource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1, &cregion);*/

		set_image_layout(cb, bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
						 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
						 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
		set_image_layout(cb, blitSource, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		vke_cmd_end(cb);
	}
}
void vke_presenter_get_size (VkePresenter r, uint32_t* pWidth, uint32_t* pHeight){
	*pWidth = r->width;
	*pHeight = r->height;
}
void _init_phy_surface(VkePresenter r, VkFormat preferedFormat, VkPresentModeKHR presentMode){
	VkPhysicalDevice gpu = r->dev->phyinfo->gpu;

	uint32_t count;
	vk_assert(vkGetPhysicalDeviceSurfaceFormatsKHR (gpu, r->surface, &count, NULL));
	assert (count>0);
	VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)malloc(count * sizeof(VkSurfaceFormatKHR));
	vk_assert(vkGetPhysicalDeviceSurfaceFormatsKHR (gpu, r->surface, &count, formats));

	for (uint32_t i=0; i<count; i++){
		if (formats[i].format == preferedFormat) {
			r->format = formats[i].format;
			r->colorSpace = formats[i].colorSpace;
			break;
		}
	}
	assert (r->format != VK_FORMAT_UNDEFINED);

	vk_assert(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, r->surface, &count, NULL));
	assert (count>0);
	VkPresentModeKHR* presentModes = (VkPresentModeKHR*)malloc(count * sizeof(VkPresentModeKHR));
	vk_assert(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, r->surface, &count, presentModes));
	r->presentMode = -1;
	for (uint32_t i=0; i<count; i++){
		if (presentModes[i] == presentMode) {
			r->presentMode = presentModes[i];
			break;
		}
	}
	if ((int32_t)r->presentMode < 0)
		r->presentMode = presentModes[0];//take first as fallback

	free(formats);
	free(presentModes);
}

void vke_presenter_create_swapchain (VkePresenter r) {
	VkPhysicalDevice gpu = r->dev->phyinfo->gpu;
	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(r->dev->vk);

	VkSurfaceCapabilitiesKHR surfCapabilities;
	vk_assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, r->surface, &surfCapabilities));
	assert (surfCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	// width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
	if (surfCapabilities.currentExtent.width == 0xFFFFFFFF) {
		// If the surface size is undefined, the size is set to
		// the size of the images requested
		if (r->width < surfCapabilities.minImageExtent.width)
			r->width = surfCapabilities.minImageExtent.width;
		else if (r->width > surfCapabilities.maxImageExtent.width)
			r->width = surfCapabilities.maxImageExtent.width;
		if (r->height < surfCapabilities.minImageExtent.height)
			r->height = surfCapabilities.minImageExtent.height;
		else if (r->height > surfCapabilities.maxImageExtent.height)
			r->height = surfCapabilities.maxImageExtent.height;
	} else {
		// If the surface size is defined, the swap chain size must match
		r->width = surfCapabilities.currentExtent.width;
		r->height= surfCapabilities.currentExtent.height;
	}

	VkSwapchainKHR newSwapchain;
	VkSwapchainCreateInfoKHR createInfo = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
											.surface = r->surface,
											.minImageCount = surfCapabilities.minImageCount,
											.imageFormat = r->format,
											.imageColorSpace = r->colorSpace,
											.imageExtent = {r->width,r->height},
											.imageArrayLayers = 1,
											.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
											.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
											.preTransform = surfCapabilities.currentTransform,
											.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
											.presentMode = r->presentMode,
											.clipped = VK_TRUE,
											.oldSwapchain = r->swapChain};

	vk_assert(vkCreateSwapchainKHR (r->dev->vk, &createInfo, NULL, &newSwapchain));
	if (r->swapChain != VK_NULL_HANDLE)
		vke_presenter_swapchain_destroy(r);
	r->swapChain = newSwapchain;

	vk_assert(vkGetSwapchainImagesKHR(r->dev->vk, r->swapChain, &r->imgCount, NULL));
	assert (r->imgCount>0);

	VkImage* images = (VkImage*)malloc(r->imgCount * sizeof(VkImage));
	vk_assert(vkGetSwapchainImagesKHR(r->dev->vk, r->swapChain, &r->imgCount, images));

	r->ScBuffers = (VkeImage*)		malloc (r->imgCount * sizeof(VkeImage));
	r->cmdBuffs = (VkCommandBuffer*)malloc (r->imgCount * sizeof(VkCommandBuffer));

	for (uint32_t i=0; i<r->imgCount; i++) {

		VkeImage sci = vke_image_import(r->dev, images[i], r->format, r->width, r->height);
		vke_image_create_view(sci, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
		r->ScBuffers [i] = sci;

		r->cmdBuffs [i] = vke_cmd_buff_create(r->dev, r->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	}
	r->currentScBufferIndex = 0;
	free (images);
}


VkBool32 _messenger_callback (
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

static array<VkeApp> apps;

VkeApp vke_app::main() {
	for (int i = 0; i < apps->length(); i++)
		if (apps[i])
			return apps[i];
	return NULL;
}

/// lower case is the implemention and internal access, used in module
/// PascalCase is user-level; access to methods (pascal!)
/// the mx case is a bit nicer because you have data described internal, protected
/// in this situation it needs to all be handled member by member and it 
/// might restrict its operation if its not updating the states optimally

void vke_app::~vke_app() {
	if (debugMessenger) {
		PFN_vkDestroyDebugUtilsMessengerEXT	 DestroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)
				vkGetInstanceProcAddr(inst, "vkDestroyDebugUtilsMessengerEXT");
		DestroyDebugUtilsMessenger(inst, debugMessenger, VK_NULL_HANDLE);
	}
	vkDestroyInstance(inst, NULL);
	for (int i = 0; i < app_count; i++)
		if (this == apps[i])
			apps[i] = NULL;
}

/// to not have constructors, ever, is practically good because you get trivial construction in fields
/// your constructors have names. thats clear and works.
/// if node api can work without inheritence that would be nice.  the mx is about passing the common memory through
/// 

VkeApp vke_app::create() {
}

/// not using this but need to implement
mx::describe(vke_app,
	[](vke_app &m) -> props {
		return {
			prop { m, "abc", m.abc }
		};
});

VkeApp vke_app::create(
		uint32_t version_major,		 uint32_t     version_minor, const char* app_name,
		uint32_t enabledLayersCount, const char** enabledLayers,
		uint32_t ext_count, 		 const char*  extentions[]) {
	vke_app *app = new vke_app();
	VkApplicationInfo infos = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = app_name,
		.applicationVersion = 1,
		.pEngineName = ENGINE_NAME,
		.engineVersion = ENGINE_VERSION,
		.apiVersion = VK_MAKE_API_VERSION (0, version_major, version_minor, 0)
	};
	VkInstanceCreateInfo inst_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &infos,
		.enabledExtensionCount = ext_count,
		.ppEnabledExtensionNames = extentions,
		.enabledLayerCount = enabledLayersCount,
		.ppEnabledLayerNames = enabledLayers
	};
	vk_assert(vkCreateInstance (&inst_info, NULL, &app->inst));
	app->debugMessenger = VK_NULL_HANDLE;
	apps->push(app);
	return app;
}

VkInstance vke_app_vkinst (VkeApp app) {
	return app->inst;
}

VkePhyInfo* VkeApp::phyinfos(VkSurfaceKHR surface) {
	uint32_t count = 0;
	vk_assert(vkEnumeratePhysicalDevices (data->inst, &count, NULL));
	VkPhysicalDevice* phyDevices = (VkPhysicalDevice*)malloc((count) * sizeof(VkPhysicalDevice));
	vk_assert(vkEnumeratePhysicalDevices (data->inst, &count, phyDevices));
	VkePhyInfo* infos = (VkePhyInfo*)malloc(count * sizeof(VkePhyInfo));
	for (uint32_t i=0; i<count; i++)
		infos[i] = vke_phyinfo_create (phyDevices[i], surface);
	free (phyDevices);
	return infos;
}

void vke_app_free_phyinfos(uint32_t count, VkePhyInfo* infos) {
	for (uint32_t i=0; i<count; i++)
		io_drop(vke_phyinfo, infos[i]);
	free(infos);
}

static PFN_vkSetDebugUtilsObjectNameEXT		SetDebugUtilsObjectNameEXT;
static PFN_vkQueueBeginDebugUtilsLabelEXT	QueueBeginDebugUtilsLabelEXT;
static PFN_vkQueueEndDebugUtilsLabelEXT		QueueEndDebugUtilsLabelEXT;
static PFN_vkCmdBeginDebugUtilsLabelEXT		CmdBeginDebugUtilsLabelEXT;
static PFN_vkCmdEndDebugUtilsLabelEXT		CmdEndDebugUtilsLabelEXT;
static PFN_vkCmdInsertDebugUtilsLabelEXT	CmdInsertDebugUtilsLabelEXT;

VkeDevice vke_device_create(VkeApp app, VkePhyInfo phyInfo, VkDeviceCreateInfo* pDevice_info) {
	assert(false);
	return NULL;
}

VkeDevice vke_device_import(VkInstance inst, VkPhysicalDevice phy, VkDevice vk) {
	assert(false);
	return NULL;
}

VkDevice vke_device_vk(VkeDevice dev) {
	return dev->vk;
}

VkPhysicalDevice vke_device_gpu(VkeDevice dev) {
	return dev->phyinfo->gpu;
}

VkePhyInfo vke_device_phyinfo(VkeDevice dev) {
	return dev->phyinfo;
}

VkeApp vke_device_app (VkeDevice dev) {
	return dev->app;
}

int vke_device_present_index(VkeDevice dev) {
	return dev->phyinfo->pQueue;
}

int vke_device_transfer_index(VkeDevice dev) {
	return dev->phyinfo->tQueue;
}

int vke_device_graphics_index(VkeDevice dev) {
	return dev->phyinfo->gQueue;
}

int vke_device_compute_index(VkeDevice dev) {
	return dev->phyinfo->cQueue;
}

/**
 * @brief get instance proc addresses for debug utils (name, label,...)
 * @param vkh device
 */
void vke_device_init_debug_utils (VkeDevice dev) {
	SetDebugUtilsObjectNameEXT		= (PFN_vkSetDebugUtilsObjectNameEXT)	vkGetInstanceProcAddr(dev->app->inst, "vkSetDebugUtilsObjectNameEXT");
	QueueBeginDebugUtilsLabelEXT	= (PFN_vkQueueBeginDebugUtilsLabelEXT)	vkGetInstanceProcAddr(dev->app->inst, "vkQueueBeginDebugUtilsLabelEXT");
	QueueEndDebugUtilsLabelEXT		= (PFN_vkQueueEndDebugUtilsLabelEXT)	vkGetInstanceProcAddr(dev->app->inst, "vkQueueEndDebugUtilsLabelEXT");
	CmdBeginDebugUtilsLabelEXT		= (PFN_vkCmdBeginDebugUtilsLabelEXT)	vkGetInstanceProcAddr(dev->app->inst, "vkCmdBeginDebugUtilsLabelEXT");
	CmdEndDebugUtilsLabelEXT		= (PFN_vkCmdEndDebugUtilsLabelEXT)		vkGetInstanceProcAddr(dev->app->inst, "vkCmdEndDebugUtilsLabelEXT");
	CmdInsertDebugUtilsLabelEXT		= (PFN_vkCmdInsertDebugUtilsLabelEXT)	vkGetInstanceProcAddr(dev->app->inst, "vkCmdInsertDebugUtilsLabelEXT");
}
VkeSampler VkeDevice::create_sampler (VkeDevice vke, VkFilter magFilter, VkFilter minFilter,
							   VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode){
	VkSampler sampler = VK_NULL_HANDLE;
	VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
											  .maxAnisotropy= 1.0,
											  .addressModeU = addressMode,
											  .addressModeV = addressMode,
											  .addressModeW = addressMode,
											  .magFilter	= magFilter,
											  .minFilter	= minFilter,
											  .mipmapMode	= mipmapMode};
	vk_assert(vkCreateSampler(vke->vk, &samplerCreateInfo, NULL, &sampler));
	return sampler;
}

/// this is better than having 'attachments' in general
void VkeSampler::destructor() {
	vkDestroySampler (vke->vk, sampler, NULL);
}

void vke_device_destroy(VkeDevice vke) {
	vmaDestroyAllocator (vke->allocator);
	vkDestroyDevice (vke->vk, NULL);
	free (vke);
}

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
void vke_cmd_label_start (VkCommandBuffer cmd, const char* name, const float color[4]) {
	const VkDebugUtilsLabelEXT info = {
		.sType		= VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext		= 0,
		.pLabelName= name
	};
	memcpy ((void*)info.color, (void*)color, 4 * sizeof(float));
	CmdBeginDebugUtilsLabelEXT (cmd, &info);
}
void vke_cmd_label_insert (VkCommandBuffer cmd, const char* name, const float color[4]) {
	const VkDebugUtilsLabelEXT info = {
		.sType		= VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext		= 0,
		.pLabelName= name
	};
	memcpy ((void*)info.color, (void*)color, 4 * sizeof(float));
	CmdInsertDebugUtilsLabelEXT (cmd, &info);
}
void vke_cmd_label_end (VkCommandBuffer cmd) {
	CmdEndDebugUtilsLabelEXT (cmd);
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




uint32_t vke_log_level = VKE_LOG_DEBUG;
FILE    *vke_out;

const char *vke_clipboard(VkeWindow win) {
	return glfwGetClipboardString(win->glfw);
}

void vke_set_window_title(VkeWindow win, const char *cs) {
	glfwSetWindowTitle(win->glfw, cs);
}

void vke_set_clipboard_string(VkeWindow win, const char* cs) {
	glfwSetWindowTitle(win->glfw, cs);
}

void vke_show_window(VkeWindow win) {
	glfwShowWindow(win->glfw);
}

void vke_hide_window(VkeWindow win) {
	glfwHideWindow(win->glfw);
}

void *vke_window_user_data(VkeWindow win) {
	return win->user;
}

void vke_poll_events() {
	glfwPollEvents();
}

static void glfw_error(int code, const char *cs) {
	printf("glfw error: code: %d (%s)", code, cs);
}

static void glfw_key    (GLFWwindow *h, int unicode, int scan_code, int action, int mods) {
	VkeWindow win = glfwGetWindowUserPointer(h);
    win->key_event(win, unicode, scan_code, action, mods);
}

static void glfw_char(GLFWwindow *h, unsigned int unicode) {
	VkeWindow win = glfwGetWindowUserPointer(h);
	win->char_event(win, unicode);
}

static void glfw_button(GLFWwindow *h, int button, int action, int mods) {
    VkeWindow win = glfwGetWindowUserPointer(h);
    win->button_event(win, button, action, mods);
}

static void glfw_cursor (GLFWwindow *h, double x, double y) {
    VkeWindow win = glfwGetWindowUserPointer(h);
    win->cursor_event(win, x, y, (bool)glfwGetWindowAttrib(h, GLFW_HOVERED));
}

static void glfw_resize (GLFWwindow *handle, int w, int h) {
	VkeWindow win = glfwGetWindowUserPointer(handle);
	win->resize_event(win, w, h);
}


/// imported from vkvg (it needs more use of vke in vkvg_device)
/// too many compile-time switches where you dont need them there
/// and that prevents the code from running elsewhere (no actual runtime benefit)

#define _CHECK_INST_EXT(ext)\
if (vke_instance_extension_supported(#ext)) {	\
	if (pExtensions)							\
	   pExtensions[*pExtCount] = #ext;			\
	(*pExtCount)++;								\
}
void vke_get_required_instance_extensions (const char** pExtensions, uint32_t* pExtCount) {
	*pExtCount = 0;

	vke_instance_extensions_check_init ();

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	_CHECK_INST_EXT(VK_EXT_debug_utils)
#endif
	_CHECK_INST_EXT(VK_KHR_get_physical_device_properties2)

	vke_instance_extensions_check_release();
}

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

//enabledFeature12 is guaranteed to be the first in pNext chain
const void* vke_get_device_requirements (VkPhysicalDeviceFeatures* pEnabledFeatures) {
	pEnabledFeatures->fillModeNonSolid	= VK_TRUE;
	pEnabledFeatures->sampleRateShading	= VK_TRUE;
	pEnabledFeatures->logicOp			= VK_TRUE;
	void* pNext                         = NULL;

	/// supporting just 1.2 and up but may merge in things when needed
	static VkPhysicalDeviceVulkan12Features enabledFeatures12 = { };
	enabledFeatures12.sType 			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	enabledFeatures12.scalarBlockLayout = scalar_block_layout ? VK_TRUE : VK_FALSE;
	enabledFeatures12.timelineSemaphore = timeline_semaphore  ? VK_TRUE : VK_FALSE;
	enabledFeatures12.pNext             = pNext;
	pNext                               = &enabledFeatures12;
	return pNext;
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

VkeStatus vke_get_required_device_extensions (VkPhysicalDevice phy, const char** pExtensions, uint32_t* pExtCount) {
	VkExtensionProperties* pExtensionProperties;
	uint32_t extensionCount;

	*pExtCount = 0;

	vk_assert(vkEnumerateDeviceExtensionProperties(phy, NULL, &extensionCount, NULL));
	pExtensionProperties = (VkExtensionProperties*)malloc(extensionCount * sizeof(VkExtensionProperties));
	vk_assert(vkEnumerateDeviceExtensionProperties(phy, NULL, &extensionCount, pExtensionProperties));

	//https://vulkan.lunarg.com/doc/view/1.2.162.0/mac/1.2-extensions/vkspec.html#VK_KHR_portability_subset
	_CHECK_DEV_EXT(VK_KHR_portability_subset);
	VkPhysicalDeviceFeatures2 phyFeat2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

	VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutSupport = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES};
	VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreSupport = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};

	if (scalar_block_layout)
		phyFeat2.pNext = &scalarBlockLayoutSupport;

	if (timeline_semaphore) {
		timelineSemaphoreSupport.pNext = phyFeat2.pNext;
		phyFeat2.pNext = &timelineSemaphoreSupport;
	}
	vkGetPhysicalDeviceFeatures2(phy, &phyFeat2);

	if (scalar_block_layout) {
		if (!scalarBlockLayoutSupport.scalarBlockLayout) {
			perror("CREATE Device failed, vkvg compiled with VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT and feature is not implemented for physical device.\n");
			return VKE_STATUS_DEVICE_ERROR;
		}
		_CHECK_DEV_EXT(VK_EXT_scalar_block_layout)
	}
	if (timeline_semaphore) {
		if (!timelineSemaphoreSupport.timelineSemaphore) {
			perror("CREATE Device failed, VK_SEMAPHORE_TYPE_TIMELINE not supported.\n");
			return VKE_STATUS_DEVICE_ERROR;
		}
		_CHECK_DEV_EXT(VK_KHR_timeline_semaphore)
	}
	return VKE_STATUS_SUCCESS;
}

/// push extension if supported (no return of that so it should)
#define TRY_LOAD_DEVICE_EXT(ext) {								\
if (vke_phyinfo_try_get_extension_properties(pi, #ext, NULL))	\
	enabledExts[enabledExtsCount++] = #ext;						\
}

void VkeApp::enable_debug() {
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
			vkGetInstanceProcAddr(app->inst, "vkCreateDebugUtilsMessengerEXT");
	CreateDebugUtilsMessenger(app->inst, &info, VK_NULL_HANDLE, &app->debugMessenger);
}


/// this creates the main app if needed
/// its less dimensional interface but more intuitive
/// VkeApp is 1:1 with VkeDevice
VkeDevice VkeDevice::select_device(VkeSelection selection) {
	if (!vke_out) vke_out = stdout;
	glfwSetErrorCallback(glfw_error);

	if (!glfwInit())            return (VkeDevice)NULL;
	if (!glfwVulkanSupported()) return (VkeDevice)NULL;

	const char* enabledLayers[10];
	const char* enabledExts  [10];
	uint32_t enabledExtsCount   = 0,
	         enabledLayersCount = 0,
			 phyCount           = 0;

	vke_layers_check_init();

	if (validation   &&  vke_layer_is_present("VK_LAYER_KHRONOS_validation"))
		enabledLayers[enabledLayersCount++] = "VK_LAYER_KHRONOS_validation";

	if (mesa_overlay &&  vke_layer_is_present("VK_LAYER_MESA_overlay"))
		enabledLayers[enabledLayersCount++] = "VK_LAYER_MESA_overlay";

	if (render_doc   &&  vke_layer_is_present("VK_LAYER_RENDERDOC_Capture"))
		enabledLayers[enabledLayersCount++] = "VK_LAYER_RENDERDOC_Capture";

	vke_layers_check_release();

	uint32_t     glfwReqExtsCount = 0;
	const char** gflwExts = glfwGetRequiredInstanceExtensions (&glfwReqExtsCount);

	vke_get_required_instance_extensions (enabledExts, &enabledExtsCount);

	for (uint32_t i = 0; i < glfwReqExtsCount; i++)
		enabledExts[i + enabledExtsCount] = gflwExts[i];

	enabledExtsCount += glfwReqExtsCount;
	VkeApp        app = vke_app::main();
	if     (!app) app = vke_app::create(1, 2, "vkvg", enabledLayersCount, enabledLayers, enabledExtsCount, enabledExts);
	
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)

	e->app.enable_debug(e->app);
	
#endif

	GLFWwindow *win_temp = glfwCreateWindow(128, 128, "glfw_temp", NULL, NULL);
	
	VkSurfaceKHR surf;
	vk_assert (glfwCreateWindowSurface(app->inst, win_temp, NULL, &surf))

	/// if you are only doing it once, dont
	array<VkePhyInfo> phys = app->phyinfos(surf);
	assert(phys.length());
	
	static auto query = [](array<VkePhyInfo> hw, VkPhysicalDeviceType gpuType) -> VkePhyInfo {
		for (size_t i = 0; i < hw.length(); i++)
			if (hw[i]->gpu_props.deviceType == gpuType) return hw[i];
		return null;
	};

	VkPhysicalDeviceType preferredGPU = (int)selection; /// these line up
	VkePhyInfo   pi = query(phys, preferedGPU);
	if (!pi)     pi = query(phys, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
	if (!pi)     pi = query(phys, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
	if (!pi)     return null;

	uint32_t qCount        = 0;
	float    qPriorities[] = {0.0};

	VkDeviceQueueCreateInfo pQueueInfos[] = { {0},{0},{0} };
	if (vke_phyinfo_create_presentable_queues	(pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;
	/*if (vke_phyinfo_create_compute_queues		(pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;
	if (vke_phyinfo_create_transfer_queues		(pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;*/

	enabledExtsCount=0;

	if (vke_get_required_device_extensions (pi->gpu, enabledExts, &enabledExtsCount)) {
		perror ("vkvg_get_required_device_extensions failed, enable log for details.\n");
		return (VkeDevice)NULL;
	}
	TRY_LOAD_DEVICE_EXT (VK_KHR_swapchain)

	VkPhysicalDeviceFeatures enabledFeatures = {0};
	const void* pNext = vke_get_device_requirements (&enabledFeatures);

	VkDeviceCreateInfo device_info = {
		.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount    = qCount,
		.pQueueCreateInfos       = (VkDeviceQueueCreateInfo*)&pQueueInfos,
		.enabledExtensionCount   = enabledExtsCount,
		.ppEnabledExtensionNames = enabledExts,
		.pEnabledFeatures        = &enabledFeatures,
		.pNext 					 = pNext
	};

	return vke_device_create(app, pi, &device_info);
}

/// create a window, register callbacks in one shot
VkeWindow vke_create_window(
		VkeDevice dev, VkeMonitor mon,
		int w, int h, const char *title,
		VkeResizeEvent resize_event,
		VkeKeyEvent       key_event,
		VkeCursorEvent cursor_event,
		VkeCharEvent     char_event,
		VkeButtonEvent button_event) {
	
	VkeWindow   win = io_new(vke_window);
	win->dev        = dev;
	win->w          = w;
	win->h          = h;
	win->glfw       = glfwCreateWindow(w, h, title, mon ? mon->data : NULL, NULL);
	win->surf       = (VkSurfaceKHR*)calloc(1, sizeof(VkSurfaceKHR));
	VkeApp app      = vke_device_app(dev);
	VkInstance inst = vke_app_vkinst(app); // todo: import device by creating app, returning its device
	vk_assert (glfwCreateWindowSurface(inst, win->glfw, NULL, (VkSurfaceKHR *)win->surf));

	win->renderer   = vke_presenter_create(
		dev, (uint32_t) dev->phyinfo->pQueue,
		*(VkSurfaceKHR*)win->surf, w, h,
		VK_FORMAT_B8G8R8A8_UNORM, VK_PRESENT_MODE_FIFO_KHR); // should be a kind of configuration call
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);
	glfwWindowHint(GLFW_FLOATING,   GLFW_FALSE);
	glfwWindowHint(GLFW_DECORATED,  GLFW_TRUE);
	///
	glfwSetFramebufferSizeCallback(win->glfw, glfw_resize);
	glfwSetKeyCallback            (win->glfw, glfw_key);
	glfwSetCursorPosCallback      (win->glfw, glfw_cursor);
	glfwSetCharCallback           (win->glfw, glfw_char);
	glfwSetMouseButtonCallback    (win->glfw, glfw_button);
	///
	return win;
}

#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

VkFence vke_fence_create (VkeDevice vke) {
	VkFence fence;
	VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
									.pNext = NULL,
									.flags = 0 };
	vk_assert(vkCreateFence(vke->vk, &fenceInfo, NULL, &fence));
	return fence;
}

VkFence vke_fence_create_signaled (VkeDevice vke) {
	VkFence fence;
	VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
									.pNext = NULL,
									.flags = VK_FENCE_CREATE_SIGNALED_BIT };
	vk_assert(vkCreateFence(vke->vk, &fenceInfo, NULL, &fence));
	return fence;
}
VkSemaphore vke_semaphore_create (VkeDevice vke) {
	VkSemaphore semaphore;
	VkSemaphoreCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
								   .pNext = NULL,
								   .flags = 0};
	vk_assert(vkCreateSemaphore(vke->vk, &info, NULL, &semaphore));
	return semaphore;
}
VkSemaphore vke_timeline_create (VkeDevice vke, uint64_t initialValue) {
	VkSemaphore semaphore;
	VkSemaphoreTypeCreateInfo timelineInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .pNext = NULL,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = initialValue};
	VkSemaphoreCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
								   .pNext = &timelineInfo,
								   .flags = 0};
	vk_assert(vkCreateSemaphore(vke->vk, &info, NULL, &semaphore));
	return semaphore;
}

VkResult vke_timeline_wait (VkeDevice vke, VkSemaphore timeline, const uint64_t wait) {
	VkSemaphoreWaitInfo waitInfo;
	waitInfo.sType			= VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	waitInfo.pNext			= NULL;
	waitInfo.flags			= 0;
	waitInfo.semaphoreCount = 1;
	waitInfo.pSemaphores	= &timeline;
	waitInfo.pValues		= &wait;

	return vkWaitSemaphores(vke->vk, &waitInfo, UINT64_MAX);
}
void vke_cmd_submit_timelined (VkeQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore timeline, const uint64_t wait, const uint64_t signal) {
	static VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkTimelineSemaphoreSubmitInfo timelineInfo;
	timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	timelineInfo.pNext = NULL;
	timelineInfo.waitSemaphoreValueCount = 1;
	timelineInfo.pWaitSemaphoreValues = &wait;
	timelineInfo.signalSemaphoreValueCount = 1;
	timelineInfo.pSignalSemaphoreValues = &signal;

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = &timelineInfo;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &timeline;
	submitInfo.signalSemaphoreCount  = 1;
	submitInfo.pSignalSemaphores = &timeline;
	submitInfo.pWaitDstStageMask = &stageFlags,
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = pCmdBuff;

	vk_assert(vkQueueSubmit(queue->queue, 1, &submitInfo, VK_NULL_HANDLE));
}
void vke_cmd_submit_timelined2 (VkeQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore timelines[2], const uint64_t waits[2], const uint64_t signals[2]) {
	static VkPipelineStageFlags stageFlags[2] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
	VkTimelineSemaphoreSubmitInfo timelineInfo;
	timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	timelineInfo.pNext = NULL;
	timelineInfo.waitSemaphoreValueCount = 2;
	timelineInfo.pWaitSemaphoreValues = waits;
	timelineInfo.signalSemaphoreValueCount = 2;
	timelineInfo.pSignalSemaphoreValues = signals;

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = &timelineInfo;
	submitInfo.waitSemaphoreCount = 2;
	submitInfo.pWaitSemaphores = timelines;
	submitInfo.signalSemaphoreCount  = 2;
	submitInfo.pSignalSemaphores = timelines;
	submitInfo.pWaitDstStageMask = stageFlags,
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = pCmdBuff;

	vk_assert(vkQueueSubmit(queue->queue, 1, &submitInfo, VK_NULL_HANDLE));
}
VkEvent vke_event_create (VkeDevice vke) {
	VkEvent evt;
	VkEventCreateInfo evtInfo = {.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
	vk_assert(vkCreateEvent (vke->vk, &evtInfo, NULL, &evt));
	return evt;
}
VkCommandPool vke_cmd_pool_create (VkeDevice vke, uint32_t qFamIndex, VkCommandPoolCreateFlags flags){
	VkCommandPool cmdPool;
	VkCommandPoolCreateInfo cmd_pool_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
											  .pNext = NULL,
											  .queueFamilyIndex = qFamIndex,
											  .flags = flags };
	vk_assert (vkCreateCommandPool(vke->vk, &cmd_pool_info, NULL, &cmdPool));
	return cmdPool;
}
VkCommandBuffer vke_cmd_buff_create (VkeDevice vke, VkCommandPool cmdPool, VkCommandBufferLevel level){
	VkCommandBuffer cmdBuff;
	VkCommandBufferAllocateInfo cmd = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
										.pNext = NULL,
										.commandPool = cmdPool,
										.level = level,
										.commandBufferCount = 1 };
	vk_assert (vkAllocateCommandBuffers (vke->vk, &cmd, &cmdBuff));
	return cmdBuff;
}
void vke_cmd_buffs_create (VkeDevice vke, VkCommandPool cmdPool, VkCommandBufferLevel level, uint32_t count, VkCommandBuffer* cmdBuffs){
	VkCommandBufferAllocateInfo cmd = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
										.pNext = NULL,
										.commandPool = cmdPool,
										.level = level,
										.commandBufferCount = count };
	vk_assert (vkAllocateCommandBuffers (vke->vk, &cmd, cmdBuffs));
}
void vke_cmd_begin(VkCommandBuffer cmdBuff, VkCommandBufferUsageFlags flags) {
	VkCommandBufferBeginInfo cmd_buf_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
											  .pNext = NULL,
											  .flags = flags,
											  .pInheritanceInfo = NULL };

	vk_assert (vkBeginCommandBuffer (cmdBuff, &cmd_buf_info));
}
void vke_cmd_end(VkCommandBuffer cmdBuff){
	vk_assert (vkEndCommandBuffer (cmdBuff));
}
void vke_cmd_submit(VkeQueue queue, VkCommandBuffer *pCmdBuff, VkFence fence){
	VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 .pWaitDstStageMask = &stageFlags,
								 .commandBufferCount = 1,
								 .pCommandBuffers = pCmdBuff};
	vk_assert(vkQueueSubmit(queue->queue, 1, &submit_info, fence));
}
void vke_cmd_submit_with_semaphores(VkeQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore waitSemaphore,
									VkSemaphore signalSemaphore, VkFence fence){

	VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 .pWaitDstStageMask = &stageFlags,
								 .commandBufferCount = 1,
								 .pCommandBuffers = pCmdBuff};

	if (waitSemaphore != VK_NULL_HANDLE){
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waitSemaphore;
	}
	if (signalSemaphore != VK_NULL_HANDLE){
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores= &signalSemaphore;
	}

	vk_assert(vkQueueSubmit(queue->queue, 1, &submit_info, fence));
}


void set_image_layout(VkCommandBuffer cmdBuff, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout,
					  VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
	VkImageSubresourceRange subres = {aspectMask,0,1,0,1};
	set_image_layout_subres(cmdBuff, image, subres, old_image_layout, new_image_layout, src_stages, dest_stages);
}

void set_image_layout_subres(VkCommandBuffer cmdBuff, VkImage image, VkImageSubresourceRange subresourceRange,
							 VkImageLayout old_image_layout, VkImageLayout new_image_layout,
							 VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
	VkImageMemoryBarrier image_memory_barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
												  .oldLayout = old_image_layout,
												  .newLayout = new_image_layout,
												  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
												  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
												  .image = image,
												  .subresourceRange = subresourceRange};

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
}

bool vke_memory_type_from_properties(VkPhysicalDeviceMemoryProperties* memory_properties, uint32_t typeBits, VkeMemoryUsage memUsage, uint32_t *typeIndex) {
	VkMemoryPropertyFlagBits memFlags = 0;
	switch(memUsage) {
	case VKE_MEMORY_USAGE_GPU_ONLY:
		memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		break;
	case VKE_MEMORY_USAGE_CPU_ONLY:
		memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		break;
	case VKE_MEMORY_USAGE_CPU_TO_GPU:
		memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		break;
	case VKE_MEMORY_USAGE_GPU_TO_CPU:
		memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		break;
	case VKE_MEMORY_USAGE_CPU_COPY:
		memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		break;
	case VKE_MEMORY_USAGE_GPU_LAZILY_ALLOCATED:
		memFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
		break;
	default:
		break;
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

VkSampleCountFlagBits vke_max_samples(VkSampleCountFlagBits counts) {
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT)  { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT)  { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT)  { return VK_SAMPLE_COUNT_2_BIT; }
	return VK_SAMPLE_COUNT_1_BIT;
}

VkShaderModule vke_load_module(VkeDevice vke, const char* path){
	VkShaderModule module;
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
	void *shader_code;

//#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
//	filename =[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent: @(filename)].UTF8String;
//#endif

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

static VkExtensionProperties* instExtProps;
static uint32_t instExtCount;
bool vke_instance_extension_supported (const char* instanceName) {
    for (uint32_t i=0; i<instExtCount; i++) {
        if (!strcmp(instExtProps[i].extensionName, instanceName))
            return true;
    }
    return false;
}
void vke_instance_extensions_check_init () {
    vk_assert(vkEnumerateInstanceExtensionProperties(NULL, &instExtCount, NULL));
    instExtProps =(VkExtensionProperties*)malloc(instExtCount * sizeof(VkExtensionProperties));
    vk_assert(vkEnumerateInstanceExtensionProperties(NULL, &instExtCount, instExtProps));
}
void vke_instance_extensions_check_release () {
    free (instExtProps);
}

static VkLayerProperties* instLayerProps;
static uint32_t instance_layer_count;
bool vke_layer_is_present (const char* layerName) {
    for (uint32_t i=0; i<instance_layer_count; i++) {
        if (!strcmp(instLayerProps[i].layerName, layerName))
            return true;
    }
    return false;
}
void vke_layers_check_init () {
    vk_assert(vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL));
    instLayerProps =(VkLayerProperties*)malloc(instance_layer_count * sizeof(VkLayerProperties));
    vk_assert(vkEnumerateInstanceLayerProperties(&instance_layer_count, instLayerProps));
}
void vke_layers_check_release () {
    free (instLayerProps);
}












void     vke_queue_free  (VkeQueue q) { }

VkeQueue vke_queue_create(VkeDevice dev, uint32_t familyIndex, uint32_t qIndex) {
	VkeQueue q = io_new(vke_queue); /// having it work with assign would be best but thats not msvc compatible without compiling as c++17
	q->dev          = dev;
	q->familyIndex	= familyIndex;
	vkGetDeviceQueue (dev->vk, familyIndex, qIndex, &q->queue);
	return q;
}













/// this is a static method here, and thus means user would need to
/// implement their own, and thus we allocate here and cant mistakenly allocate somewhere else
vke_phyinfo::~vke_phyinfo(VkePhyInfo phy) {
	if (phy->pExtensionProperties != NULL)
		free(phy->pExtensionProperties);
	free(phy->queues);
}

VkePhyInfo vke_phyinfo::create(VkPhysicalDevice phy, VkSurfaceKHR surface) {
	VkePhyInfo pi = new vke_phyinfo();

	pi->gpu = phy;
	vkGetPhysicalDeviceProperties (phy, &pi->gpu_props);
	vkGetPhysicalDeviceMemoryProperties (phy, &pi->mem_props);
	vkGetPhysicalDeviceQueueFamilyProperties (phy, &pi->queueCount, NULL);
	pi->queues = (VkQueueFamilyProperties*)malloc(pi->queueCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties (phy, &pi->queueCount, pi->queues);

	//identify dedicated queues
	pi->cQueue = -1;
	pi->gQueue = -1;
	pi->tQueue = -1;
	pi->pQueue = -1;

	//try to find dedicated queues first
	for (uint32_t j=0; j<pi->queueCount; j++){
		VkBool32 present = VK_FALSE;
		switch (pi->queues[j].queueFlags) {
		case VK_QUEUE_GRAPHICS_BIT:
			if (surface)
				vkGetPhysicalDeviceSurfaceSupportKHR(phy, j, surface, &present);
			if (present){
				if (pi->pQueue<0)
					pi->pQueue = j;
			}else if (pi->gQueue<0)
				pi->gQueue = j;
			break;
		case VK_QUEUE_COMPUTE_BIT:
			if (pi->cQueue<0)
				pi->cQueue = j;
			break;
		case VK_QUEUE_TRANSFER_BIT:
			if (pi->tQueue<0)
				pi->tQueue = j;
			break;
		}
	}
	//try to find suitable queue if no dedicated one found
	for (uint32_t j=0; j<pi->queueCount; j++){
		if (pi->queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			VkBool32 present = 0;
			if (surface)
				vkGetPhysicalDeviceSurfaceSupportKHR(phy, j, surface, &present);
			//printf ("surf=%d, q=%d, present=%d\n",surface,j,present);
			if (present){
				if (pi->pQueue<0)
					pi->pQueue = j;
			}else if (pi->gQueue<0)
				pi->gQueue = j;
		}
		if ((pi->queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (pi->gQueue < 0))
			pi->gQueue = j;
		if ((pi->queues[j].queueFlags & VK_QUEUE_COMPUTE_BIT) && (pi->cQueue < 0))
			pi->cQueue = j;
		if ((pi->queues[j].queueFlags & VK_QUEUE_TRANSFER_BIT) && (pi->tQueue < 0))
			pi->tQueue = j;
	}

	return pi;
}

VkPhysicalDeviceProperties vke_phyinfo_get_properties (VkePhyInfo phy) {
	return phy->gpu_props;
}
VkPhysicalDeviceMemoryProperties vke_phyinfo_get_memory_properties (VkePhyInfo phy) {
	return phy->mem_props;
}

void vke_phyinfo_get_queue_fam_indices (VkePhyInfo phy, int* pQueue, int* gQueue, int* tQueue, int* cQueue) {
	if (pQueue)	*pQueue = phy->pQueue;
	if (gQueue)	*gQueue = phy->gQueue;
	if (tQueue)	*tQueue = phy->tQueue;
	if (cQueue)	*cQueue = phy->cQueue;
}

VkQueueFamilyProperties* vke_phyinfo_get_queues_props(VkePhyInfo phy, uint32_t* qCount) {
	*qCount = phy->queueCount;
	return phy->queues;
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

void VkePhyInfo::set_dpi(VkePhyInfo phy, vec2 dpi) {
	phy->dpi = dpi;
}

vec2i VkePhyInfo::get_dpi(VkePhyInfo phy) {
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

bool VkePhyInfo::create_presentable_queues(uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (phy->pQueue < 0)
		perror("No queue with presentable support found");
	else if (phy->queues[phy->pQueue].queueCount < queueCount)
		fprintf(stderr, "Request %d queues of family %d, but only %d available\n",
			queueCount, phy->pQueue, phy->queues[phy->pQueue].queueCount);
	else {
		qInfo->queueCount = queueCount,
		qInfo->queueFamilyIndex = phy->pQueue,
		qInfo->pQueuePriorities = queue_priorities;
		phy->queues[phy->pQueue].queueCount -= queueCount;
		return true;
	}
	return false;
}

bool VkePhyInfo::create_transfer_queues(uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (phy->tQueue < 0)
		perror("No transfer queue found");
	else if (phy->queues[phy->tQueue].queueCount < queueCount)
		fprintf(stderr, "Request %d transfer queues of family %d, but only %d available\n", queueCount, phy->tQueue, phy->queues[phy->tQueue].queueCount);
	else {
		qInfo->queueCount = queueCount;
		qInfo->queueFamilyIndex = phy->tQueue;
		qInfo->pQueuePriorities = queue_priorities;
		phy->queues[phy->tQueue].queueCount -= queueCount;
		return true;
	}
	return false;
}

bool VkePhyInfo::create_compute_queues(VkePhyInfo phy, uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (phy->cQueue < 0)
		perror("No compute queue found");
	else if (phy->queues[phy->cQueue].queueCount < queueCount)
		fprintf(stderr, "Request %d compute queues of family %d, but only %d available\n", queueCount, phy->cQueue, phy->queues[phy->cQueue].queueCount);
	else {
		qInfo->queueCount = queueCount,
		qInfo->queueFamilyIndex = phy->cQueue,
		qInfo->pQueuePriorities = queue_priorities;
		phy->queues[phy->cQueue].queueCount -= queueCount;
		return true;
	}
	return false;
}

bool VkePhyInfo::create_graphic_queues (VkePhyInfo phy, uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (phy->gQueue < 0)
		perror("No graphic queue found");
	else if (phy->queues[phy->gQueue].queueCount < queueCount)
		fprintf(stderr, "Request %d graphic queues of family %d, but only %d available\n", queueCount, phy->gQueue, phy->queues[phy->gQueue].queueCount);
	else {
		qInfo->queueCount = queueCount,
		qInfo->queueFamilyIndex = phy->gQueue,
		qInfo->pQueuePriorities = queue_priorities;
		phy->queues[phy->gQueue].queueCount -= queueCount;
		return true;
	}
	return false;
}

bool VkePhyInfo::try_get_extension_properties (VkePhyInfo phy, const char* name, const VkExtensionProperties* properties) {
	if (phy->pExtensionProperties == NULL) {
		vk_assert(vkEnumerateDeviceExtensionProperties(phy->gpu, NULL, &phy->extensionCount, NULL));
		phy->pExtensionProperties = (VkExtensionProperties*)malloc(phy->extensionCount * sizeof(VkExtensionProperties));
		vk_assert(vkEnumerateDeviceExtensionProperties(phy->gpu, NULL, &phy->extensionCount, phy->pExtensionProperties));
	}
	for (uint32_t i=0; i<phy->extensionCount; i++) {
		if (strcmp(name, phy->pExtensionProperties[i].extensionName)==0) {
			if (properties)
				properties = &phy->pExtensionProperties[i];
			return true;
		}
	}
	properties = NULL;
	return false;
}

}