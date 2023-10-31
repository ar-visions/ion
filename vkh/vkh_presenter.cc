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
#include <vkh/vkh_presenter.h>
#include <vkh/vkh_device.h>
#include <vkh/vkh_image.h>

#ifndef MIN
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
# define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

void vkh_presenter_create_swapchain	 (VkhPresenter r);
void _swapchain_destroy (VkhPresenter r);
void _init_phy_surface	(VkhPresenter r, VkFormat preferedFormat, VkPresentModeKHR presentMode);

VkhPresenter vkh_presenter_create (VkhDevice vkh, uint32_t presentQueueFamIdx, VkSurfaceKHR surface, uint32_t width, uint32_t height,
						   VkFormat preferedFormat, VkPresentModeKHR presentMode, bool use_dpi) {
	VkhPresenter r 		= (VkhPresenter)calloc(1,sizeof(vkh_presenter_t));
	r->refs 			= 1;
	r->vkh 				= vkh_device_grab(vkh);
	r->qFam 			= presentQueueFamIdx ;
	r->surface 			= surface;
	r->scale_x			= vkh->e->vk_gpu->dpi_scale.x;
	r->scale_y			= vkh->e->vk_gpu->dpi_scale.y;
	r->width 			= width  * (use_dpi ? r->scale_x : 1.0);
	r->height 			= height * (use_dpi ? r->scale_y : 1.0);
	vkGetDeviceQueue(r->vkh->device, r->qFam, 0, &r->queue);

	r->cmdPool			= vkh_cmd_pool_create  (r->vkh, presentQueueFamIdx, 0);
	r->semaPresentEnd	= vkh_semaphore_create (r->vkh);
	r->semaDrawEnd		= vkh_semaphore_create (r->vkh);
	r->fenceDraw		= vkh_fence_create_signaled(r->vkh);

	_init_phy_surface (r, preferedFormat, presentMode);
	vkh_presenter_create_swapchain (r);
	return r;
}

VkhPresenter vkh_presenter_grab(VkhPresenter r) {
	if (r)
		r->refs++;
	return r;
}

void vkh_presenter_drop (VkhPresenter r) {
	if (r && --r->refs == 0) {
		vkDeviceWaitIdle (r->vkh->device);
		vkDestroySemaphore	(r->vkh->device, r->semaDrawEnd, NULL);
		vkDestroySemaphore	(r->vkh->device, r->semaPresentEnd, NULL);
		vkDestroyFence		(r->vkh->device, r->fenceDraw, NULL);
		
		_swapchain_destroy  (r);

		vkDestroyCommandPool(r->vkh->device, r->cmdPool, NULL);
		vkh_device_drop		(r->vkh);
		free (r);
	}
}

bool vkh_presenter_acquireNextImage (VkhPresenter r, VkFence fence, VkSemaphore semaphore) {
	// Get the index of the next available swapchain image:
	VkResult err = vkAcquireNextImageKHR
			(r->vkh->device, r->swapChain, UINT64_MAX, semaphore, fence, &r->currentScBufferIndex);
	return ((err != VK_ERROR_OUT_OF_DATE_KHR) && (err != VK_SUBOPTIMAL_KHR));
}



bool vkh_presenter_draw (VkhPresenter r) {
	if (!vkh_presenter_acquireNextImage (r, VK_NULL_HANDLE, r->semaPresentEnd)) {
		vkh_presenter_create_swapchain (r);
		return false;
	}

	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo si = { };
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &r->semaPresentEnd;
	si.pWaitDstStageMask = &dstStageMask;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &r->cmdBuffs[r->currentScBufferIndex];
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &r->semaDrawEnd;

	vkDeviceWaitIdle(r->vkh->device);
	vkWaitForFences	(r->vkh->device, 1, &r->fenceDraw, VK_TRUE, FENCE_TIMEOUT);
	vkResetFences	(r->vkh->device, 1, &r->fenceDraw);

	VK_CHECK_RESULT(vkQueueSubmit (r->queue, 1, &si, r->fenceDraw));

	/* Now present the image in the window */
	VkPresentInfoKHR present = {};
	present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = &r->semaDrawEnd;
	present.swapchainCount = 1;
	present.pSwapchains = &r->swapChain;
	present.pImageIndices = &r->currentScBufferIndex;

	/* Make sure command buffer is finished before presenting */
	vkQueuePresentKHR(r->queue, &present);
	return true;
}

void vkh_presenter_build_blit_cmd (VkhPresenter r, VkImage blitSource, uint32_t width, uint32_t height){

	uint32_t dst_w = width  * r->vkh->e->vk_gpu->dpi_scale.x;
	uint32_t dst_h = height * r->vkh->e->vk_gpu->dpi_scale.y;
	uint32_t src_w = MIN(dst_w, r->width), /// surface & presenter have the scale applied; the user does not apply the scale to args
			 src_h = MIN(dst_h, r->height);

	for (uint32_t i = 0; i < r->imgCount; ++i)
	{
		VkImage bltDstImage = r->ScBuffers[i]->image;
		VkCommandBuffer cb = r->cmdBuffs[i];

		vkh_cmd_begin(cb,0); /// we need to control this outside this function i think

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
		VkImageBlit bregion { };
		bregion.srcSubresource 	= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		bregion.srcOffsets[0] = { 0 };
		bregion.srcOffsets[1] = { ion::i32(src_w), ion::i32(src_h), 1 };

		bregion.dstSubresource	= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		bregion.dstOffsets[0] = { 0 };
		bregion.dstOffsets[1] = { ion::i32(dst_w), ion::i32(dst_h), 1 };

		vkCmdBlitImage(cb, blitSource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bregion, VK_FILTER_NEAREST);

		/*vkCmdCopyImage(cb, blitSource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1, &cregion);*/

		set_image_layout(cb, bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
						 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
						 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
		set_image_layout(cb, blitSource, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		vkh_cmd_end(cb);
	}
}
void vkh_presenter_get_size (VkhPresenter r, uint32_t* pWidth, uint32_t* pHeight, float *scale_x, float *scale_y){
	*pWidth  = r->width;
	*pHeight = r->height;
	if (scale_x) *scale_x = r->scale_x;
	if (scale_y) *scale_y = r->scale_y;
}
void _init_phy_surface(VkhPresenter r, VkFormat preferedFormat, VkPresentModeKHR presentMode){
	uint32_t count;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR (r->vkh->e->vk_gpu->phys, r->surface, &count, NULL));
	assert (count>0);
	VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)malloc(count * sizeof(VkSurfaceFormatKHR));
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR (r->vkh->e->vk_gpu->phys, r->surface, &count, formats));

	for (uint32_t i=0; i<count; i++){
		if (formats[i].format == preferedFormat) {
			r->format = formats[i].format;
			r->colorSpace = formats[i].colorSpace;
			break;
		}
	}
	assert (r->format != VK_FORMAT_UNDEFINED);

	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(r->vkh->e->vk_gpu->phys, r->surface, &count, NULL));
	assert (count>0);
	VkPresentModeKHR* presentModes = (VkPresentModeKHR*)malloc(count * sizeof(VkPresentModeKHR));
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(r->vkh->e->vk_gpu->phys, r->surface, &count, presentModes));
	r->presentMode =(VkPresentModeKHR)-1;
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

/// presenter should be made into a vk object and moved

void vkh_presenter_create_swapchain (VkhPresenter r){
	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(r->vkh->device);

	VkSurfaceCapabilitiesKHR surfCapabilities;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(r->vkh->e->vk_gpu->phys, r->surface, &surfCapabilities));
	assert (surfCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	ion::Device &vk_device = r->vkh->e->vk_device;
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
	VkSwapchainCreateInfoKHR createInfo = { };
	createInfo.sType 			= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface 			= r->surface;
	createInfo.minImageCount 	= surfCapabilities.minImageCount;
	createInfo.imageFormat 		= r->format;
	createInfo.imageColorSpace 	= r->colorSpace;
	createInfo.imageExtent 		= {r->width,r->height};
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage 		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform 	= surfCapabilities.currentTransform;
	createInfo.compositeAlpha 	= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode 		= r->presentMode;
	createInfo.clipped 			= VK_TRUE;
	createInfo.oldSwapchain 	= r->swapChain ? r->swapChain : vk_device->swapChain; ///

	VK_CHECK_RESULT(vkCreateSwapchainKHR (r->vkh->device, &createInfo, NULL, &newSwapchain)); /// we are effectively updating the swap chain on this device
	if (r->swapChain != VK_NULL_HANDLE)
		_swapchain_destroy(r);
	r->swapChain = newSwapchain;
	vk_device->swapChain = newSwapchain; /// update this data just for integrity; the two systems must merge a bit more in the presentation area; namely presenter should find a place in vk and startup there

	VK_CHECK_RESULT(vkGetSwapchainImagesKHR(r->vkh->device, r->swapChain, &r->imgCount, NULL));
	assert (r->imgCount>0);

	VkImage* images = (VkImage*)malloc(r->imgCount * sizeof(VkImage));
	VK_CHECK_RESULT(vkGetSwapchainImagesKHR(r->vkh->device, r->swapChain, &r->imgCount, images));

	r->ScBuffers = (VkhImage*)		malloc (r->imgCount * sizeof(VkhImage));
	r->cmdBuffs = (VkCommandBuffer*)malloc (r->imgCount * sizeof(VkCommandBuffer));

	for (uint32_t i=0; i<r->imgCount; i++) {

		VkhImage sci = vkh_image_import(r->vkh, images[i], r->format, r->width, r->height);
		vkh_image_create_view(sci, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
		r->ScBuffers [i] = sci;

		r->cmdBuffs [i] = vkh_cmd_buff_create(r->vkh, r->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	}
	r->currentScBufferIndex = 0;
	free (images);
}
void _swapchain_destroy (VkhPresenter r){
	for (uint32_t i = 0; i < r->imgCount; i++) {
		vkFreeCommandBuffers (r->vkh->device, r->cmdPool, 1, &r->cmdBuffs[i]);
		vkh_image_drop (r->ScBuffers [i]);
	}
	vkDestroySwapchainKHR (r->vkh->device, r->swapChain, NULL);
	r->swapChain = VK_NULL_HANDLE;
	free(r->ScBuffers);
	free(r->cmdBuffs);
}
