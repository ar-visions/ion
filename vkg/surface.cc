/*
 * Copyright (c) 2018-2020 Jean-Philippe Bruyère <jp_bruyere@hotmail.com>
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

#include <vkg/internal.hh>
#include <vke/image.h>
#include <vke/queue.h>
#include <vke/device.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"



void _explicit_ms_resolve (VkgSurface surf){
	io_sync(surf);

	VkCommandBuffer cmd = surf->cmd;

	vke_cmd_begin (cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vke_image_set_layout (cmd, surf->imgMS, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vke_image_set_layout (cmd, surf->img, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageResolve re = {
		.extent = {surf->width, surf->height,1},
		.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
		.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}
	};

	vkCmdResolveImage(cmd,
					  vke_image_get_vkimage (surf->imgMS), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					  vke_image_get_vkimage (surf->img) ,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					  1,&re);
	vke_image_set_layout (cmd, surf->imgMS, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	vke_cmd_end (cmd);

	_surface_submit_cmd (surf);

	io_unsync(surf);
}

void _clear_surface (VkgSurface surf, VkImageAspectFlags aspect)
{
	io_sync(surf);

	VkCommandBuffer cmd = surf->cmd;

	vke_cmd_begin (cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
		VkClearColorValue cclr = {{0,0,0,0}};
		VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};

		VkeImage img = surf->imgMS;
		if (surf->dev->samples == VK_SAMPLE_COUNT_1_BIT)
			img = surf->img;

		vke_image_set_layout (cmd, img, VK_IMAGE_ASPECT_COLOR_BIT,
							  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		vkCmdClearColorImage(cmd, vke_image_get_vkimage (img),
									 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cclr, 1, &range);

		vke_image_set_layout (cmd, img, VK_IMAGE_ASPECT_COLOR_BIT,
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	if (aspect & VK_IMAGE_ASPECT_STENCIL_BIT) {
		VkClearDepthStencilValue clr = {0,0};
		VkImageSubresourceRange range = {VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};

		vke_image_set_layout (cmd, surf->stencil, VK_IMAGE_ASPECT_STENCIL_BIT,
							  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		vkCmdClearDepthStencilImage (cmd, vke_image_get_vkimage (surf->stencil),
									 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clr,1,&range);

		vke_image_set_layout (cmd, surf->stencil, VK_IMAGE_ASPECT_STENCIL_BIT,
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
							  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	}
	vke_cmd_end (cmd);

	_surface_submit_cmd (surf);

	io_unsync(surf);
}

void _create_surface_main_image (VkgSurface surf){
	surf->img = vke_image_create_basic(
		surf->dev->vke,surf->format,surf->width,surf->height,surf->dev->supportedTiling,VKE_MEMORY_USAGE_GPU_ONLY,
		VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	
	vke_image_create_descriptor(surf->img, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_image_set_name(surf->img, "SURF main color");
	vke_device_set_object_name(surf->dev->vke, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vke_image_get_view(surf->img), "SURF main color VIEW");
	vke_device_set_object_name(surf->dev->vke, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vke_image_get_sampler(surf->img), "SURF main color SAMPLER");
#endif
}
//create multisample color img if sample count > 1 and the stencil buffer multisampled or not
void _create_surface_secondary_images (VkgSurface surf) {
	VkeDevice vke = surf->dev->vke;
	if (surf->dev->samples > VK_SAMPLE_COUNT_1_BIT){
		surf->imgMS = vke_image_ms_create(vke,surf->format,surf->dev->samples,surf->width,surf->height,VKE_MEMORY_USAGE_GPU_ONLY,
										  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		vke_image_create_descriptor(surf->imgMS, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST,
									VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
		vke_image_set_name(surf->imgMS, "SURF MS color IMG");
		vke_device_set_object_name(vke, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vke_image_get_view(surf->imgMS), "SURF MS color VIEW");
		vke_device_set_object_name(vke, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vke_image_get_sampler(surf->imgMS), "SURF MS color SAMPLER");
#endif
	}
	surf->stencil = vke_image_ms_create(vke,surf->dev->stencilFormat,surf->dev->samples,surf->width,surf->height,VKE_MEMORY_USAGE_GPU_ONLY,									 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	vke_image_create_descriptor(surf->stencil, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_STENCIL_BIT, VK_FILTER_NEAREST,
								VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_image_set_name(surf->stencil, "SURF stencil");
	vke_device_set_object_name(vke, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vke_image_get_view(surf->stencil), "SURF stencil VIEW");
	vke_device_set_object_name(vke, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vke_image_get_sampler(surf->stencil), "SURF stencil SAMPLER");
#endif
}
void _create_framebuffer (VkgSurface surf) {
	VkImageView attachments[] = {
		vke_image_get_view (surf->img),
		vke_image_get_view (surf->stencil),
		vke_image_get_view (surf->imgMS),
	};
	VkFramebufferCreateInfo frameBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
													  .renderPass = surf->dev->renderPass,
													  .attachmentCount = 3,
													  .pAttachments = attachments,
													  .width = surf->width,
													  .height = surf->height,
													  .layers = 1 };
	if (surf->dev->samples == VK_SAMPLE_COUNT_1_BIT)
		frameBufferCreateInfo.attachmentCount = 2;
	else if (surf->dev->deferredResolve) {
		attachments[0] = attachments[2];
		frameBufferCreateInfo.attachmentCount = 2;
	}
	VK_CHECK_RESULT(vkCreateFramebuffer(surf->dev->vke->vkdev, &frameBufferCreateInfo, NULL, &surf->fb));
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_device_set_object_name(surf->dev->vke, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)surf->fb, "SURF FB");
#endif
}
void _create_surface_images (VkgSurface surf) {
	_create_surface_main_image		(surf);
	_create_surface_secondary_images(surf);
	_create_framebuffer				(surf);
#if defined(DEBUG) && defined(ENABLE_VALIDATION)
	vke_image_set_name(surf->img,     "surfImg");
	vke_image_set_name(surf->imgMS,   "surfImgMS");
	vke_image_set_name(surf->stencil, "surfStencil");
#endif
}

VkgSurface _create_surface(VkgDevice dev, VkFormat format) {	
	VkgSurface surf = io_new(vkg_surface); // todo: create with sync if device has sync

	if (dev->status != VKE_STATUS_SUCCESS) {
		surf->status = VKE_STATUS_DEVICE_ERROR;
		return surf;
	}
	surf->dev     = dev;
	surf->format  = format;
	surf->cmdPool = vke_cmd_pool_create (dev->vke, dev->gQueue->familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	vke_cmd_buffs_create(dev->vke, surf->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, &surf->cmd);

#if VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	surf->timeline = vke_timeline_create (dev->vke, 0);
#else
	surf->flushFence = vke_fence_create (dev->vke);
#endif

#if defined(DEBUG) && defined(VKVG_DBG_UTILS)
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)surf->cmd, "vkvgSurfCmd");
#endif
	return surf;
}

//if fence sync, surf mutex must be locked.
/*bool _surface_wait_cmd (VkgSurface surf) {
	vke_log(VKE_LOG_INFO, "SURF: _surface__wait_flush_fence\n");
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	if (vke_timeline_wait (surf->dev->vke, surf->timeline, surf->timelineStep) == VK_SUCCESS)
		return true;
#else
	if (WaitForFences (surf->dev->vke->vkdev, 1, &surf->flushFence, VK_TRUE, VKVG_FENCE_TIMEOUT) == VK_SUCCESS) {
		ResetFences (surf->dev->vke->vkdev, 1, &surf->flushFence);
		return true;
	}
#endif
	vke_log(VKE_LOG_DEBUG, "CTX: _wait_flush_fence timeout\n");
	surf->status = VKE_STATUS_TIMEOUT;
	return false;
}*/
//surface mutex must be locked to call this method, locking to guard also the surf->cmd local buffer usage.
void _surface_submit_cmd (VkgSurface surf) {
	VkgDevice dev = surf->dev;
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	io_sync(dev);
	vke_cmd_submit_timelined (dev->gQueue, &surf->cmd, surf->timeline, surf->timelineStep, surf->timelineStep+1);
	surf->timelineStep++;
	io_unsync(dev);
	vke_timeline_wait (dev->vke, surf->timeline, surf->timelineStep);
#else
	io_sync(dev);
	vke_cmd_submit (surf->dev->gQueue, &surf->cmd, surf->flushFence);
	io_unsync(dev);
	WaitForFences (surf->dev->vke->vkdev, 1, &surf->flushFence, VK_TRUE, VKVG_FENCE_TIMEOUT);
	ResetFences (surf->dev->vke->vkdev, 1, &surf->flushFence);
#endif
}


#define max(x,y)
void _transition_surf_images (VkgSurface surf) {
	io_sync(surf);
	VkgDevice dev = surf->dev;

	//_surface_wait_cmd (surf);

	vke_cmd_begin (surf->cmd,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VkeImage imgMs = surf->imgMS;
	if (imgMs != NULL)
		vke_image_set_layout(surf->cmd, imgMs, VK_IMAGE_ASPECT_COLOR_BIT,
							 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	vke_image_set_layout(surf->cmd, surf->img, VK_IMAGE_ASPECT_COLOR_BIT,
					 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	vke_image_set_layout (surf->cmd, surf->stencil, dev->stencilAspectFlag,
						  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	vke_cmd_end (surf->cmd);

	_surface_submit_cmd (surf);

	io_unsync(surf);
}

void vkg_surface_clear (VkgSurface surf) {
	if (surf->status)
		return;
	_clear_surface(surf, VK_IMAGE_ASPECT_STENCIL_BIT|VK_IMAGE_ASPECT_COLOR_BIT);
}

VkgSurface vkg_surface_create (VkgDevice dev, uint32_t width, uint32_t height){
	VkgSurface surf = _create_surface(dev, FB_COLOR_FORMAT);
	if (surf->status)
		return surf;

	surf->width = MAX(1, width);
	surf->height = MAX(1, height);
	surf->newSurf = true;//used to clear all attacments on first render pass

	_create_surface_images (surf);

	_transition_surf_images (surf);

	surf->status = VKE_STATUS_SUCCESS;
	vkg_device_reference (surf->dev);
	return surf;
}

VkgSurface vkg_surface_create_for_VkhImage (VkgDevice dev, void* vkhImg) {
	VkgSurface surf = _create_surface(dev, FB_COLOR_FORMAT);
	if (surf->status)
		return surf;

	if (!vkhImg) {
		surf->status = VKE_STATUS_INVALID_IMAGE;
		return surf;
	}

	VkeImage img = (VkeImage)vkhImg;
	surf->width = img->infos.extent.width;
	surf->height= img->infos.extent.height;

	surf->img = img;

	vke_image_create_sampler(img, VK_FILTER_NEAREST, VK_FILTER_NEAREST,
							 VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	_create_surface_secondary_images	(surf);
	_create_framebuffer					(surf);

	_transition_surf_images (surf);
	//_clear_surface						(surf, VK_IMAGE_ASPECT_STENCIL_BIT);

	surf->status = VKE_STATUS_SUCCESS;
	vkg_device_reference (surf->dev);
	return surf;
}

//TODO: it would be better to blit in original size and create ms final image with dest surf dims
VkgSurface vkg_surface_create_from_bitmap (VkgDevice dev, unsigned char* img, uint32_t width, uint32_t height) {
	VkgSurface surf = _create_surface(dev, FB_COLOR_FORMAT);
	if (surf->status)
		return surf;
	if (!img || width <= 0 || height <= 0) {
		surf->status = VKE_STATUS_INVALID_IMAGE;
		return surf;
	}

	surf->width = MAX(1, width);
	surf->height = MAX(1, height);

	_create_surface_images (surf);

	uint32_t imgSize = width * height * 4;
	VkImageSubresourceLayers imgSubResLayers = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
	//original format image
	VkeImage stagImg= vke_image_create ((VkeDevice)surf->dev,VK_FORMAT_R8G8B8A8_UNORM,surf->width,surf->height,VK_IMAGE_TILING_LINEAR,
										 VKE_MEMORY_USAGE_GPU_ONLY,
										 VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	//bgra bliting target
	VkeImage tmpImg = vke_image_create ((VkeDevice)surf->dev,surf->format,surf->width,surf->height,VK_IMAGE_TILING_LINEAR,
										 VKE_MEMORY_USAGE_GPU_ONLY,
										 VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	vke_image_create_descriptor (tmpImg, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT,
								 VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
	//staging buffer
	vke_buffer buff = {0};
	vke_buffer_init((VkeDevice)dev,
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VKE_MEMORY_USAGE_CPU_TO_GPU,
					imgSize, &buff, true);

	memcpy (vke_buffer_get_mapped_pointer (&buff), img, imgSize);

	VkCommandBuffer cmd = surf->cmd;

	vke_cmd_begin (cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vke_image_set_layout (cmd, stagImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);


	VkBufferImageCopy bufferCopyRegion = { .imageSubresource = imgSubResLayers,
										   .imageExtent = {surf->width,surf->height,1}};

	vkCmdCopyBufferToImage(cmd, buff.buffer,
		vke_image_get_vkimage (stagImg), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

	vke_image_set_layout (cmd, stagImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vke_image_set_layout (cmd, tmpImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageBlit blit = {
		.srcSubresource = imgSubResLayers,
		.srcOffsets[1] = {(int32_t)surf->width, (int32_t)surf->height, 1},
		.dstSubresource = imgSubResLayers,
		.dstOffsets[1] = {(int32_t)surf->width, (int32_t)surf->height, 1},
	};
	vkCmdBlitImage	(cmd,
					 vke_image_get_vkimage (stagImg), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					 vke_image_get_vkimage (tmpImg),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

	vke_image_set_layout (cmd, tmpImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	vke_cmd_end		(cmd);

	_surface_submit_cmd (surf);//lock surface?

	vke_buffer_reset	(&buff);
	vke_image_drop	(stagImg);

	surf->newSurf = false;

	//create tmp context with rendering pipeline to create the multisample img
	VkgContext ctx = vkg_create (surf);

/*	  VkClearAttachment ca = {VK_IMAGE_ASPECT_COLOR_BIT,0, { 0.0f, 0.0f, 0.0f, 0.0f }};
	VkClearRect cr = {{{0,0},{surf->width,surf->height}},0,1};
	vkCmdClearAttachments(ctx->cmd, 1, &ca, 1, &cr);*/

	vec4f srcRect = {.x=0,.y=0,.width=(float)surf->width,.height=(float)surf->height};
	ctx->pushConsts.source = srcRect;
	ctx->pushConsts.fsq_patternType = (ctx->pushConsts.fsq_patternType & FULLSCREEN_BIT) + VKVG_PATTERN_TYPE_SURFACE;

	//_update_push_constants (ctx);
	_update_descriptor_set (ctx, tmpImg, ctx->dsSrc);
	_ensure_renderpass_is_started  (ctx);

	vkg_paint			(ctx);
	vkg_drop		(ctx);

	vke_image_drop	(tmpImg);

	surf->status = VKE_STATUS_SUCCESS;
	vkg_device_reference (surf->dev);
	return surf;
}

VkgSurface vkg_surface_create_from_image (VkgDevice dev, const char* filePath) {
	int w = 0,
		h = 0,
		channels = 0;
	unsigned char *img = stbi_load(filePath, &w, &h, &channels, 4);//force 4 components per pixel
	if (!img){
		vke_log(VKE_LOG_ERR, "Could not load texture from %s, %s\n", filePath, stbi_failure_reason());
		return (VkgSurface)&_no_mem_status;
	}

	VkgSurface surf = vkg_surface_create_from_bitmap(dev, img, (uint32_t)w, (uint32_t)h);

	stbi_image_free (img);

	return surf;
}

void vkg_surface_drop(VkgSurface surf)
{
	vkDestroyCommandPool(surf->dev->vke->vkdev, surf->cmdPool, NULL);
	vkDestroyFramebuffer(surf->dev->vke->vkdev, surf->fb, NULL);

	if (!surf->img->imported)
		vke_image_drop(surf->img);

	vke_image_drop(surf->imgMS);
	vke_image_drop(surf->stencil);

	if (surf->dev->threadAware)
		mtx_destroy(&surf->mutex);

#if VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	vkDestroySemaphore (surf->dev->vke->vkdev, surf->timeline, NULL);
#else
	vkDestroyFence (surf->dev->vke->vkdev, surf->flushFence, NULL);
#endif

	vkg_device_drop(surf->dev);
}

VkgSurface vkg_surface_reference (VkgSurface surf) {
	return (VkgSurface)io_grab(surf);
}

uint32_t vkg_surface_get_reference_count (VkgSurface surf) {
	return mx_refs(surf);
}

VkImage vkg_surface_get_vk_image(VkgSurface surf)
{
	if (surf->status)
		return NULL;
	if (surf->dev->deferredResolve)
		_explicit_ms_resolve(surf);
	return vke_image_get_vkimage (surf->img);
}
void vkg_surface_resolve (VkgSurface surf){
	if (surf->status || !surf->dev->deferredResolve)
		return;
	_explicit_ms_resolve(surf);
}
VkFormat vkg_surface_get_vk_format(VkgSurface surf)
{
	if (surf->status)
		return VK_FORMAT_UNDEFINED;
	return surf->format;
}
uint32_t vkg_surface_get_width (VkgSurface surf) {
	if (surf->status)
		return 0;
	return surf->width;
}
uint32_t vkg_surface_get_height (VkgSurface surf) {
	if (surf->status)
		return 0;
	return surf->height;
}

VkeStatus vkg_surface_write_to_png (VkgSurface surf, const char* path){
	if (surf->status) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_png failed, invalid status: %d\n", surf->status);
		return VKE_STATUS_INVALID_STATUS;
	}
	if (surf->dev->status) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_png failed, invalid device status: %d\n", surf->dev->status);
		return VKE_STATUS_INVALID_STATUS;
	}
	if (surf->dev->pngStagFormat == VK_FORMAT_UNDEFINED) {
		vke_log(VKE_LOG_ERR, "no suitable image format for png write\n");
		return VKE_STATUS_INVALID_FORMAT;
	}
	if (!path) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_png failed, null path\n");
		return VKE_STATUS_WRITE_ERROR;
	}
	io_sync(surf);
	VkImageSubresourceLayers imgSubResLayers = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
	VkgDevice dev = surf->dev;

	//RGBA to blit to, surf img is bgra
	VkeImage stagImg;

	if (dev->pngStagTiling == VK_IMAGE_TILING_LINEAR)
		stagImg = vke_image_create ((VkeDevice)surf->dev, dev->pngStagFormat, surf->width, surf->height, dev->pngStagTiling,
										 VKE_MEMORY_USAGE_GPU_TO_CPU,
										 VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	else
		stagImg = vke_image_create ((VkeDevice)surf->dev, dev->pngStagFormat, surf->width,surf->height, dev->pngStagTiling,
										 VKE_MEMORY_USAGE_GPU_ONLY,
										 VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	VkCommandBuffer cmd = surf->cmd;
	vke_cmd_begin (cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vke_image_set_layout (cmd, stagImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vke_image_set_layout (cmd, surf->img, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageBlit blit = {
		.srcSubresource = imgSubResLayers,
		.srcOffsets[1] = {(int32_t)surf->width, (int32_t)surf->height, 1},
		.dstSubresource = imgSubResLayers,
		.dstOffsets[1] = {(int32_t)surf->width, (int32_t)surf->height, 1},
	};
	vkCmdBlitImage	(cmd,
					 vke_image_get_vkimage (surf->img), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					 vke_image_get_vkimage (stagImg),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

	vke_cmd_end		(cmd);

	_surface_submit_cmd (surf);

	VkeImage stagImgLinear = stagImg;

	if (dev->pngStagTiling == VK_IMAGE_TILING_OPTIMAL) {
		stagImgLinear = vke_image_create ((VkeDevice)surf->dev, dev->pngStagFormat, surf->width, surf->height, VK_IMAGE_TILING_LINEAR,
										  VKE_MEMORY_USAGE_GPU_TO_CPU,
										  VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		VkImageCopy cpy = {
			.srcSubresource = imgSubResLayers,
			.srcOffset = {0},
			.dstSubresource = imgSubResLayers,
			.dstOffset = {0},
			.extent = {(int32_t)surf->width, (int32_t)surf->height, 1}
		};

		vke_cmd_begin (cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		vke_image_set_layout (cmd, stagImgLinear, VK_IMAGE_ASPECT_COLOR_BIT,
							  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		vke_image_set_layout (cmd, stagImg, VK_IMAGE_ASPECT_COLOR_BIT,
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		vkCmdCopyImage(cmd,
					   vke_image_get_vkimage (stagImg), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   vke_image_get_vkimage (stagImgLinear),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);

		vke_cmd_end		(cmd);

		_surface_submit_cmd (surf);

		vke_image_drop(stagImg);
	}

	void*    img    = vke_image_map       (stagImgLinear);
	uint64_t stride = vke_image_get_stride(stagImgLinear);

	stbi_write_png (path, (int32_t)surf->width, (int32_t)surf->height, 4, img, (int32_t)stride);

	vke_image_unmap (stagImgLinear);
	vke_image_drop(stagImgLinear);

	io_unsync(surf);
	return VKE_STATUS_SUCCESS;
}

VkeStatus vkg_surface_write_to_memory (VkgSurface surf, unsigned char* const bitmap){
	if (surf->status) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_memory failed, invalid status: %d\n", surf->status);
		return VKE_STATUS_INVALID_STATUS;
	}
	if (!bitmap) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_memory failed, null path\n");
		return VKE_STATUS_INVALID_IMAGE;
	}

	io_sync(surf);

	VkImageSubresourceLayers imgSubResLayers = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
	VkgDevice dev = surf->dev;

	//RGBA to blit to, surf img is bgra
	VkeImage stagImg= vke_image_create ((VkeDevice)surf->dev,VK_FORMAT_B8G8R8A8_UNORM ,surf->width,surf->height,VK_IMAGE_TILING_LINEAR,
										 VKE_MEMORY_USAGE_GPU_TO_CPU,
										 VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	VkCommandBuffer cmd = surf->cmd;

	vke_cmd_begin (cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vke_image_set_layout (cmd, stagImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vke_image_set_layout (cmd, surf->img, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageBlit blit = {
		.srcSubresource = imgSubResLayers,
		.srcOffsets[1] = {(int32_t)surf->width, (int32_t)surf->height, 1},
		.dstSubresource = imgSubResLayers,
		.dstOffsets[1] = {(int32_t)surf->width, (int32_t)surf->height, 1},
	};
	vkCmdBlitImage	(cmd,
					 vke_image_get_vkimage (surf->img), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					 vke_image_get_vkimage (stagImg),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

	vke_cmd_end		(cmd);

	_surface_submit_cmd (surf);

	uint64_t stride = vke_image_get_stride(stagImg);
	uint32_t dest_stride = surf->width * 4;

	char* img = vke_image_map (stagImg);
	char* row = (char*)bitmap;
	for (uint32_t y = 0; y < surf->height; y++) {
		memcpy(row, img, dest_stride);
		row += dest_stride;
		img += stride;
	}

	vke_image_unmap (stagImg);
	vke_image_drop(stagImg);

	io_unsync(surf);

	return VKE_STATUS_SUCCESS;
}
