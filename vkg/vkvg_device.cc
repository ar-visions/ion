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

#include "vkvg_device_internal.h"
#include "vkvg_surface_internal.h"
#include "vkvg_context_internal.h"
#include <vkh/vkh_image.h>
#include <vkh/vkh_queue.h>
#include <vkh/vkh_phyinfo.h>

#define TRY_LOAD_DEVICE_EXT(ext) {								\
if (vkh_phyinfo_try_get_extension_properties(pi, #ext, NULL))	\
	enabledExts[enabledExtsCount++] = #ext;						\
}

void vkvg_image_blt(VkvgDevice dev, VkhImage dst, VkhImage src, VkOffset3D dst_extent, VkOffset3D src_extent, VkFilter filter) {
	VkCommandBuffer commandBuffer = vkh_cmd_buff_create((VkhDevice)dev, dev->cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkImageBlit blitRegion = {};
    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcOffsets[1] = src_extent;//{ extent.width, extent.height, 1 };

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.mipLevel = 0;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstOffsets[1] = dst_extent;//{ extent.width, extent.height, 1 };

    vkCmdBlitImage(
        commandBuffer,
        src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blitRegion,
        filter
    );

	vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submit {
		.sType 				= VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers 	= &commandBuffer
	};
    vkQueueSubmit(dev->gQueue->queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(dev->gQueue->queue);
	vkFreeCommandBuffers(dev->device, dev->cmdPool, 1, &commandBuffer);
}

void vkvg_device_set_context_cache_size (VkvgDevice dev, uint32_t maxCount) {
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

//VkPhysicalDeviceType preferedGPU, VkPresentModeKHR presentMode, uint32_t width, uint32_t height, 0
VkvgDevice vkvg_device_create (VkEngine e, VkSampleCountFlags samples, bool deferredResolve) {
	LOG(VKVG_LOG_INFO, "CREATE Device\n");
	VkvgDevice dev  		= (vkvg_device*)calloc(1,sizeof(vkvg_device));
	dev->refs 				= 1;
	dev->e 					= e;
	dev->device 			= e->vk_device->device;
	dev->hdpi				= 72; /// these dont match other dpis of 96 else-where; it should be looked up from phys and dpi
	dev->vdpi				= 72;
	dev->samples			= (VkSampleCountFlags)e->vk_gpu->getUsableSampling((VkSampleCountFlagBits)samples); /// important to filter it down because otehrwise it will break if it isnt supported
	dev->deferredResolve	= (dev->samples == VK_SAMPLE_COUNT_1_BIT) ? false : deferredResolve;

	dev->cachedContextMaxCount 	= VKVG_MAX_CACHED_CONTEXT_COUNT;

#if VKVG_DBG_STATS
	dev->debug_stats = (vkvg_debug_stats_t) {0};
#endif

	VkFormat format = FB_COLOR_FORMAT;

	_device_check_best_image_tiling(dev, format);
	if (dev->status != VKVG_STATUS_SUCCESS)
		return nullptr;

	if (!_device_init_function_pointers (dev)){
		dev->status = VKVG_STATUS_NULL_POINTER;
		return nullptr;
	}

	dev->gQueue = vkh_queue_create ((VkhDevice)dev,
		e->vk_gpu->indices.graphicsFamily.value(),
		e->vk_gpu->indices.presentFamily.value());
	//mtx_init (&dev->gQMutex, mtx_plain);

#ifdef VKH_USE_VMA
	VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice = phy,
		.device = vkdev
	};
	vmaCreateAllocator(&allocatorInfo, (VmaAllocator*)&dev->allocator);
#endif

	dev->cmdPool= vkh_cmd_pool_create		((VkhDevice)dev, dev->gQueue->familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	dev->cmd	= vkh_cmd_buff_create		((VkhDevice)dev, dev->cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	dev->fence	= vkh_fence_create_signaled ((VkhDevice)dev);

	_device_create_pipeline_cache		(dev);
	_fonts_cache_create					(dev);
	if (dev->deferredResolve || dev->samples == VK_SAMPLE_COUNT_1_BIT){
		dev->renderPass					= _device_createRenderPassNoResolve (dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD);
		dev->renderPass_ClearStencil	= _device_createRenderPassNoResolve (dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR);
		dev->renderPass_ClearAll		= _device_createRenderPassNoResolve (dev, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);
	}else{
		dev->renderPass					= _device_createRenderPassMS (dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD);
		dev->renderPass_ClearStencil	= _device_createRenderPassMS (dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR);
		dev->renderPass_ClearAll		= _device_createRenderPassMS (dev, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);
	}
	_device_createDescriptorSetLayout	(dev);
	_device_setupPipelines				(dev);
	_device_create_empty_texture		(dev, format, dev->supportedTiling);

#ifdef DEBUG
	#if defined(__linux__) && defined(__GLIBC__)
		_linux_register_error_handler ();
	#endif
	#ifdef VKVG_DBG_UTILS
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)dev->cmdPool, "Device Cmd Pool");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)dev->cmd, "Device Cmd Buff");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_FENCE, (uint64_t)dev->fence, "Device Fence");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)dev->renderPass, "RP load img/stencil");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)dev->renderPass_ClearStencil, "RP clear stencil");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)dev->renderPass_ClearAll, "RP clear all");

		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)dev->dslSrc, "DSLayout SOURCE");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)dev->dslFont, "DSLayout FONT");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)dev->dslGrad, "DSLayout GRADIENT");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)dev->pipelineLayout, "PLLayout dev");

		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipelinePolyFill, "PL Poly fill");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipelineClipping, "PL Clipping");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipe_OVER, "PL draw Over");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipe_SUB, "PL draw Substract");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipe_CLEAR, "PL draw Clear");

		vkh_image_set_name(dev->emptyImg, "empty IMG");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkh_image_get_view(dev->emptyImg), "empty IMG VIEW");
		vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vkh_image_get_sampler(dev->emptyImg), "empty IMG SAMPLER");
	#endif
#endif

	dev->status = VKVG_STATUS_SUCCESS;
	dev->vkh = vkh_device_grab(e->vkh); /// e->vkh is VkhDevice
	return dev;
}

void vkvg_device_drop (VkvgDevice dev)
{
	LOCK_DEVICE
	dev->refs--;
	if (dev->refs > 0) {
		UNLOCK_DEVICE
		return;
	}
	UNLOCK_DEVICE


	if (dev->cachedContextCount > 0) {
		_cached_ctx* cur = dev->cachedContextLast;
		while (cur) {
			_release_context_ressources (cur->ctx);
			_cached_ctx* prev = cur;
			cur = cur->pNext;
			free (prev);
		}
	}


	LOG(VKVG_LOG_INFO, "DESTROY Device\n");

	vkDeviceWaitIdle (dev->device);

	vkh_image_drop				(dev->emptyImg);

	vkDestroyDescriptorSetLayout	(dev->device, dev->dslGrad,NULL);
	vkDestroyDescriptorSetLayout	(dev->device, dev->dslFont,NULL);
	vkDestroyDescriptorSetLayout	(dev->device, dev->dslSrc, NULL);

	vkDestroyPipeline				(dev->device, dev->pipelinePolyFill, NULL);
	vkDestroyPipeline				(dev->device, dev->pipelineClipping, NULL);

	vkDestroyPipeline				(dev->device, dev->pipe_OVER,	NULL);
	vkDestroyPipeline				(dev->device, dev->pipe_SUB,		NULL);
	vkDestroyPipeline				(dev->device, dev->pipe_CLEAR,	NULL);

#ifdef VKVG_WIRED_DEBUG
	vkDestroyPipeline				(dev->device, dev->pipelineWired, NULL);
	vkDestroyPipeline				(dev->device, dev->pipelineLineList, NULL);
#endif

	vkDestroyPipelineLayout			(dev->device, dev->pipelineLayout, NULL);
	vkDestroyPipelineCache			(dev->device, dev->pipelineCache, NULL);
	vkDestroyRenderPass				(dev->device, dev->renderPass, NULL);
	vkDestroyRenderPass				(dev->device, dev->renderPass_ClearStencil, NULL);
	vkDestroyRenderPass				(dev->device, dev->renderPass_ClearAll, NULL);

	vkWaitForFences					(dev->device, 1, &dev->fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence					(dev->device, dev->fence,NULL);

	vkFreeCommandBuffers			(dev->device, dev->cmdPool, 1, &dev->cmd);
	vkDestroyCommandPool			(dev->device, dev->cmdPool, NULL);

	vkh_queue_destroy(dev->gQueue);

	_font_cache_destroy(dev);

#ifdef VKH_USE_VMA
	vmaDestroyAllocator (dev->allocator);
#endif

	if (dev->threadAware)
		mtx_destroy (&dev->mutex);

	if (dev->vkh) {
		VkEngine e = vkh_device_get_engine(dev->vkh);
		vkh_device_drop (dev->vkh);
		vkengine_drop (e);
	}

	free(dev);
}

vkvg_status_t vkvg_device_status (VkvgDevice dev) {
	return dev->status;
}
VkvgDevice vkvg_device_grab (VkvgDevice dev) {
	LOCK_DEVICE
	dev->refs++;
	UNLOCK_DEVICE
	return dev;
}
uint32_t vkvg_device_get_reference_count (VkvgDevice dev) {
	return dev->refs;
}
void vkvg_device_set_dpy (VkvgDevice dev, int hdpy, int vdpy) {
	dev->hdpi = hdpy;
	dev->vdpi = vdpy;

	//TODO: reset font cache
}
void vkvg_device_get_dpy (VkvgDevice dev, int* hdpy, int* vdpy) {
	*hdpy = dev->hdpi;
	*vdpy = dev->vdpi;
}
void vkvg_device_set_thread_aware (VkvgDevice dev, uint32_t thread_aware) {
	if (thread_aware) {
		if (dev->threadAware)
			return;
		mtx_init (&dev->mutex, mtx_plain);
		mtx_init (&dev->fontCache->mutex, mtx_plain);
		dev->threadAware = true;
	} else if (dev->threadAware) {
		mtx_destroy (&dev->mutex);
		mtx_destroy (&dev->fontCache->mutex);
		dev->threadAware = false;
	}
}
#if VKVG_DBG_STATS
vkvg_debug_stats_t vkvg_device_get_stats (VkvgDevice dev) {
	return dev->debug_stats;
}
vkvg_debug_stats_t vkvg_device_reset_stats (VkvgDevice dev) {
	dev->debug_stats = (vkvg_debug_stats_t) {0};
}
#endif
