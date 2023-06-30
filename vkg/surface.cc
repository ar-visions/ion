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



void VkgSurface::explicit_ms_resolve () {//should init cmd before calling this (unused, using automatic resolve by renderpass)
	vke_image_set_layout (data->cmd, data->pSurf->imgMS, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vke_image_set_layout (data->cmd, data->pSurf->img, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageResolve re = {
		.extent = {data->pSurf->width, data->pSurf->height,1},
		.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
		.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}
	};

	vkCmdResolveImage(data->cmd,
					  vke_image_get_vkimage (data->pSurf->imgMS), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					  vke_image_get_vkimage (data->pSurf->img) ,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					  1,&re);
	vke_image_set_layout (data->cmd, data->pSurf->imgMS, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ,
						  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VkgSurface::clear_surface (VkImageAspectFlags aspect)
{
	io_sync(data);

	VkCommandBuffer cmd = data->cmd;

	cmd.begin(VkeCommandBufferUsage::one_time_submit);

	if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
		VkClearColorValue cclr = {{0,0,0,0}};
		VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};

		VkeImage img = data->imgMS;
		if (data->dev->samples == VK_SAMPLE_COUNT_1_BIT)
			img = data->img;

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

		vke_image_set_layout (cmd, data->stencil, VK_IMAGE_ASPECT_STENCIL_BIT,
							  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		vkCmdClearDepthStencilImage (cmd, vke_image_get_vkimage (data->stencil),
									 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clr,1,&range);

		vke_image_set_layout (cmd, data->stencil, VK_IMAGE_ASPECT_STENCIL_BIT,
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
							  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	}
	cmd.finish();

	_surface_submit_cmd (data);

	io_unsync(data);
}

void VkgSurface::create_surface_main_image (){
	data->img = vke_image_create_basic(
		data->dev->vke,data->format,data->width,data->height,data->dev->supportedTiling,VKE_MEMORY_USAGE_GPU_ONLY,
		VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	
	vke_image_create_descriptor(data->img, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_image_set_name(data->img, "SURF main color");
	vke_device_set_object_name(data->dev->vke, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vke_image_get_view(data->img), "SURF main color VIEW");
	vke_device_set_object_name(data->dev->vke, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vke_image_get_sampler(data->img), "SURF main color SAMPLER");
#endif
}
//create multisample color img if sample count > 1 and the stencil buffer multisampled or not
void _create_surface_secondary_images (VkgSurface data) {
	VkeDevice vke = data->dev->vke;
	if (data->dev->samples > VK_SAMPLE_COUNT_1_BIT){
		data->imgMS = vke_image_ms_create(vke,data->format,data->dev->samples,data->width,data->height,VKE_MEMORY_USAGE_GPU_ONLY,
										  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		vke_image_create_descriptor(data->imgMS, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST,
									VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
		vke_image_set_name(data->imgMS, "SURF MS color IMG");
		vke_device_set_object_name(vke, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vke_image_get_view(data->imgMS), "SURF MS color VIEW");
		vke_device_set_object_name(vke, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vke_image_get_sampler(data->imgMS), "SURF MS color SAMPLER");
#endif
	}
	data->stencil = vke_image_ms_create(vke,data->dev->stencilFormat,data->dev->samples,data->width,data->height,VKE_MEMORY_USAGE_GPU_ONLY,									 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	vke_image_create_descriptor(data->stencil, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_STENCIL_BIT, VK_FILTER_NEAREST,
								VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_image_set_name(data->stencil, "SURF stencil");
	vke_device_set_object_name(vke, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vke_image_get_view(data->stencil), "SURF stencil VIEW");
	vke_device_set_object_name(vke, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vke_image_get_sampler(data->stencil), "SURF stencil SAMPLER");
#endif
}
void _create_framebuffer (VkgSurface data) {
	VkImageView attachments[] = {
		vke_image_get_view (data->img),
		vke_image_get_view (data->stencil),
		vke_image_get_view (data->imgMS),
	};
	VkFramebufferCreateInfo frameBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
													  .renderPass = data->dev->renderPass,
													  .attachmentCount = 3,
													  .pAttachments = attachments,
													  .width = data->width,
													  .height = data->height,
													  .layers = 1 };
	if (data->dev->samples == VK_SAMPLE_COUNT_1_BIT)
		frameBufferCreateInfo.attachmentCount = 2;
	else if (data->dev->deferredResolve) {
		attachments[0] = attachments[2];
		frameBufferCreateInfo.attachmentCount = 2;
	}
	VK_CHECK_RESULT(vkCreateFramebuffer(data->dev->vke->vkdev, &frameBufferCreateInfo, NULL, &data->fb));
#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vke_device_set_object_name(data->dev->vke, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)data->fb, "SURF FB");
#endif
}
void VkgSurface::create_surface_images (VkgSurface data) {
	_create_surface_main_image		(data);
	_create_surface_secondary_images(data);
	_create_framebuffer				(data);
#if defined(DEBUG) && defined(ENABLE_VALIDATION)
	vke_image_set_name(data->img,     "surfImg");
	vke_image_set_name(data->imgMS,   "surfImgMS");
	vke_image_set_name(data->stencil, "surfStencil");
#endif
}

VkgSurface::VkgSurface(VkgDevice dev, VkFormat format) : mx(&data) {
	if (dev->status != VKE_STATUS_SUCCESS) {
		data->status = VKE_STATUS_DEVICE_ERROR;
		return data;
	}
	data->dev     = dev;
	data->format  = format;
	data->cmdPool = vke_cmd_pool_create (dev->vke, dev->gQueue->familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	vke_cmd_buffs_create(dev->vke, data->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, &data->cmd);

#if VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	data->timeline = vke_timeline_create (dev->vke, 0);
#else
	data->flushFence = vke_fence_create (dev->vke);
#endif

#if defined(DEBUG) && defined(VKVG_DBG_UTILS)
	vke_device_set_object_name(dev->vke, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)data->cmd, "vkvgSurfCmd");
#endif
	return data;
}

//if fence sync, data mutex must be locked.
/*bool _surface_wait_cmd (VkgSurface data) {
	vke_log(VKE_LOG_INFO, "SURF: _surface__wait_flush_fence\n");
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	if (vke_timeline_wait (data->dev->vke, data->timeline, data->timelineStep) == VK_SUCCESS)
		return true;
#else
	if (WaitForFences (data->dev->vke->vkdev, 1, &data->flushFence, VK_TRUE, VKVG_FENCE_TIMEOUT) == VK_SUCCESS) {
		ResetFences (data->dev->vke->vkdev, 1, &data->flushFence);
		return true;
	}
#endif
	vke_log(VKE_LOG_DEBUG, "CTX: _wait_flush_fence timeout\n");
	data->status = VKE_STATUS_TIMEOUT;
	return false;
}*/
//surface mutex must be locked to call this method, locking to guard also the data->cmd local buffer usage.
void VkgSurface::surface_submit_cmd() {
	VkgDevice dev = data->dev;
#ifdef VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	io_sync(dev);
	vke_cmd_submit_timelined (dev->gQueue, &data->cmd, data->timeline, data->timelineStep, data->timelineStep+1);
	data->timelineStep++;
	io_unsync(dev);
	vke_timeline_wait (dev->vke, data->timeline, data->timelineStep);
#else
	io_sync(dev);
	vke_cmd_submit (data->dev->gQueue, &data->cmd, data->flushFence);
	io_unsync(dev);
	WaitForFences (data->dev->vke->vkdev, 1, &data->flushFence, VK_TRUE, VKVG_FENCE_TIMEOUT);
	ResetFences (data->dev->vke->vkdev, 1, &data->flushFence);
#endif
}


#define max(x,y)
void VkgSurface::transition_surf_images () {
	VkgDevice dev = data->dev;

	//_surface_wait_cmd (data);

	data->cmd.begin(VkeCommandBufferUsage::one_time_submit);
	VkeImage imgMs = data->imgMS;
	if (imgMs != NULL)
		vke_image_set_layout(data->cmd, imgMs, VK_IMAGE_ASPECT_COLOR_BIT,
							 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	vke_image_set_layout(data->cmd, data->img, VK_IMAGE_ASPECT_COLOR_BIT,
					 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	vke_image_set_layout (data->cmd, data->stencil, dev->stencilAspectFlag,
						  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	data->cmd.finish();

	_surface_submit_cmd (data);
}

void VkgSurface::clear (VkgSurface data) {
	if (data->status)
		return;
	_clear_surface(data, VK_IMAGE_ASPECT_STENCIL_BIT|VK_IMAGE_ASPECT_COLOR_BIT);
}

VkgSurface::VkgSurface(VkgDevice dev, uint32_t width, uint32_t height){
	VkgSurface data = _create_surface(dev, FB_COLOR_FORMAT);
	if (data->status)
		return data;

	data->width = MAX(1, width);
	data->height = MAX(1, height);
	data->newSurf = true;//used to clear all attacments on first render pass

	_create_surface_images (data);

	_transition_surf_images (data);

	data->status = VKE_STATUS_SUCCESS;
	vkg_device_reference (data->dev);
	return data;
}

VkgSurface::VkgSurface(VkgDevice dev, void* vkhImg) {
	VkgSurface data = _create_surface(dev, FB_COLOR_FORMAT);
	if (data->status)
		return data;

	if (!vkhImg) {
		data->status = VKE_STATUS_INVALID_IMAGE;
		return data;
	}

	VkeImage img = (VkeImage)vkhImg;
	data->width = img->infos.extent.width;
	data->height= img->infos.extent.height;

	data->img = img;

	vke_image_create_sampler(img, VK_FILTER_NEAREST, VK_FILTER_NEAREST,
							 VK_SAMPLER_MIPMAP_MODE_NEAREST,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	_create_surface_secondary_images	(data);
	_create_framebuffer					(data);

	_transition_surf_images (data);
	//_clear_surface						(data, VK_IMAGE_ASPECT_STENCIL_BIT);

	data->status = VKE_STATUS_SUCCESS;
	vkg_device_reference (data->dev);
	return data;
}

VkgSurface::VkgSurface(VkgDevice dev, unsigned char* img, uint32_t width, uint32_t height) {
	VkgSurface data = _create_surface(dev, FB_COLOR_FORMAT);
	if (data->status)
		return data;
	if (!img || width <= 0 || height <= 0) {
		data->status = VKE_STATUS_INVALID_IMAGE;
		return data;
	}

	data->width = MAX(1, width);
	data->height = MAX(1, height);

	_create_surface_images (data);

	uint32_t imgSize = width * height * 4;
	VkImageSubresourceLayers imgSubResLayers = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
	//original format image
	VkeImage stagImg= vke_image_create ((VkeDevice)data->dev,VK_FORMAT_R8G8B8A8_UNORM,data->width,data->height,VK_IMAGE_TILING_LINEAR,
										 VKE_MEMORY_USAGE_GPU_ONLY,
										 VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	//bgra bliting target
	VkeImage tmpImg = vke_image_create ((VkeDevice)data->dev,data->format,data->width,data->height,VK_IMAGE_TILING_LINEAR,
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

	VkCommandBuffer cmd = data->cmd;

	cmd.begin(VkeCommandBufferUsage::one_time_submit);
	vke_image_set_layout (cmd, stagImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);


	VkBufferImageCopy bufferCopyRegion = { .imageSubresource = imgSubResLayers,
										   .imageExtent = {data->width,data->height,1}};

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
		.srcOffsets[1] = {(int32_t)data->width, (int32_t)data->height, 1},
		.dstSubresource = imgSubResLayers,
		.dstOffsets[1] = {(int32_t)data->width, (int32_t)data->height, 1},
	};
	vkCmdBlitImage	(cmd,
					 vke_image_get_vkimage (stagImg), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					 vke_image_get_vkimage (tmpImg),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

	vke_image_set_layout (cmd, tmpImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	cmd.finish();

	_surface_submit_cmd (data);//lock surface?

	vke_buffer_reset	(&buff);
	vke_image_drop	(stagImg);

	data->newSurf = false;

	//create tmp context with rendering pipeline to create the multisample img
	VkgContext ctx = vkg_create (data);

/*	  VkClearAttachment ca = {VK_IMAGE_ASPECT_COLOR_BIT,0, { 0.0f, 0.0f, 0.0f, 0.0f }};
	VkClearRect cr = {{{0,0},{data->width,data->height}},0,1};
	vkCmdClearAttachments(ctx->cmd, 1, &ca, 1, &cr);*/

	vec4f srcRect = {.x=0,.y=0,.width=(float)data->width,.height=(float)data->height};
	ctx->pushConsts.source = srcRect;
	ctx->pushConsts.fsq_patternType = (ctx->pushConsts.fsq_patternType & FULLSCREEN_BIT) + VKVG_PATTERN_TYPE_SURFACE;

	//update_push_constants (ctx);
	update_descriptor_set (ctx, tmpImg, ctx->dsSrc);
	ensure_renderpass_is_started  (ctx);

	vkg_paint			(ctx);
	vkg_drop		(ctx);

	vke_image_drop	(tmpImg);

	data->status = VKE_STATUS_SUCCESS;
	vkg_device_reference (data->dev);
	return data;
}

VkgSurface::VkgSurface(VkgDevice dev, const char* filePath) {
	int w = 0,
		h = 0,
		channels = 0;
	unsigned char *img = stbi_load(filePath, &w, &h, &channels, 4);//force 4 components per pixel
	if (!img){
		vke_log(VKE_LOG_ERR, "Could not load texture from %s, %s\n", filePath, stbi_failure_reason());
		return (VkgSurface)&_no_mem_status;
	}
	VkgSurface data = vkg_surface_create_from_bitmap(dev, img, (uint32_t)w, (uint32_t)h);
	stbi_image_free (img);
}

#if VKVG_ENABLE_VK_TIMELINE_SEMAPHORE
	vkDestroySemaphore (data->dev->vke->vkdev, data->timeline, NULL);
#else
	vkDestroyFence (data->dev->vke->vkdev, data->flushFence, NULL);
#endif

	vkg_device_drop(data->dev);
}

VkImage VkgSurface::get_vk_image(VkgSurface data) {
	if (data->status)
		return NULL;
	if (data->dev->deferredResolve)
		_explicit_ms_resolve(data);
	return vke_image_get_vkimage (data->img);
}

void vkg_surface_resolve (VkgSurface data){
	if (data->status || !data->dev->deferredResolve)
		return;
	_explicit_ms_resolve(data);
}
VkFormat VkgSurface::get_vk_format(VkgSurface data)
{
	if (data->status)
		return VK_FORMAT_UNDEFINED;
	return data->format;
}
uint32_t VkgSurface::get_width (VkgSurface data) {
	if (data->status)
		return 0;
	return data->width;
}
uint32_t VkgSurface::get_height (VkgSurface data) {
	if (data->status)
		return 0;
	return data->height;
}

VkeStatus VkgSurface::write_to_png(const char* path) {
	if (data->status) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_png failed, invalid status: %d\n", data->status);
		return VKE_STATUS_INVALID_STATUS;
	}
	if (data->dev->status) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_png failed, invalid device status: %d\n", data->dev->status);
		return VKE_STATUS_INVALID_STATUS;
	}
	if (data->dev->pngStagFormat == VK_FORMAT_UNDEFINED) {
		vke_log(VKE_LOG_ERR, "no suitable image format for png write\n");
		return VKE_STATUS_INVALID_FORMAT;
	}
	if (!path) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_png failed, null path\n");
		return VKE_STATUS_WRITE_ERROR;
	}
	io_sync(data);
	VkImageSubresourceLayers imgSubResLayers = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
	VkgDevice dev = data->dev;

	//RGBA to blit to, data img is bgra
	VkeImage stagImg;

	if (dev->pngStagTiling == VK_IMAGE_TILING_LINEAR)
		stagImg = vke_image_create ((VkeDevice)data->dev, dev->pngStagFormat, data->width, data->height, dev->pngStagTiling,
										 VKE_MEMORY_USAGE_GPU_TO_CPU,
										 VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	else
		stagImg = vke_image_create ((VkeDevice)data->dev, dev->pngStagFormat, data->width,data->height, dev->pngStagTiling,
										 VKE_MEMORY_USAGE_GPU_ONLY,
										 VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	VkCommandBuffer cmd = data->cmd;
	cmd.begin(VkeCommandBufferUsage::one_time_submit);
	vke_image_set_layout (cmd, stagImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vke_image_set_layout (cmd, data->img, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageBlit blit = {
		.srcSubresource = imgSubResLayers,
		.srcOffsets[1] = {(int32_t)data->width, (int32_t)data->height, 1},
		.dstSubresource = imgSubResLayers,
		.dstOffsets[1] = {(int32_t)data->width, (int32_t)data->height, 1},
	};
	vkCmdBlitImage	(cmd,
					 vke_image_get_vkimage (data->img), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					 vke_image_get_vkimage (stagImg),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

	cmd.finish();

	_surface_submit_cmd (data);

	VkeImage stagImgLinear = stagImg;

	if (dev->pngStagTiling == VK_IMAGE_TILING_OPTIMAL) {
		stagImgLinear = vke_image_create ((VkeDevice)data->dev, dev->pngStagFormat, data->width, data->height, VK_IMAGE_TILING_LINEAR,
										  VKE_MEMORY_USAGE_GPU_TO_CPU,
										  VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		VkImageCopy cpy = {
			.srcSubresource = imgSubResLayers,
			.srcOffset = {0},
			.dstSubresource = imgSubResLayers,
			.dstOffset = {0},
			.extent = {(int32_t)data->width, (int32_t)data->height, 1}
		};

		cmd.begin(VkeCommandBufferUsage::one_time_submit);
		vke_image_set_layout (cmd, stagImgLinear, VK_IMAGE_ASPECT_COLOR_BIT,
							  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		vke_image_set_layout (cmd, stagImg, VK_IMAGE_ASPECT_COLOR_BIT,
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		vkCmdCopyImage(cmd,
					   vke_image_get_vkimage (stagImg), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   vke_image_get_vkimage (stagImgLinear),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);

		cmd.finish();

		_surface_submit_cmd (data);

		vke_image_drop(stagImg);
	}

	void*    img    = vke_image_map       (stagImgLinear);
	uint64_t stride = vke_image_get_stride(stagImgLinear);

	stbi_write_png (path, (int32_t)data->width, (int32_t)data->height, 4, img, (int32_t)stride);

	vke_image_unmap (stagImgLinear);
	vke_image_drop(stagImgLinear);

	io_unsync(data);
	return VKE_STATUS_SUCCESS;
}

VkeStatus vkg_surface_write_to_memory (unsigned char* const bitmap){
	if (data->status) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_memory failed, invalid status: %d\n", data->status);
		return VKE_STATUS_INVALID_STATUS;
	}
	if (!bitmap) {
		vke_log(VKE_LOG_ERR, "vkg_surface_write_to_memory failed, null path\n");
		return VKE_STATUS_INVALID_IMAGE;
	}

	io_sync(data);

	VkImageSubresourceLayers imgSubResLayers = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
	VkgDevice dev = data->dev;

	//RGBA to blit to, data img is bgra
	VkeImage stagImg= vke_image_create ((VkeDevice)data->dev,VK_FORMAT_B8G8R8A8_UNORM ,data->width,data->height,VK_IMAGE_TILING_LINEAR,
										 VKE_MEMORY_USAGE_GPU_TO_CPU,
										 VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	VkCommandBuffer cmd = data->cmd;

	cmd.begin(VkeCommandBufferUsage::one_time_submit);
	vke_image_set_layout (cmd, stagImg, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vke_image_set_layout (cmd, data->img, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageBlit blit = {
		.srcSubresource = imgSubResLayers,
		.srcOffsets[1] = {(int32_t)data->width, (int32_t)data->height, 1},
		.dstSubresource = imgSubResLayers,
		.dstOffsets[1] = {(int32_t)data->width, (int32_t)data->height, 1},
	};
	vkCmdBlitImage	(cmd,
					 vke_image_get_vkimage (data->img), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					 vke_image_get_vkimage (stagImg),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

	cmd.finish();

	_surface_submit_cmd (data);

	uint64_t stride = vke_image_get_stride(stagImg);
	uint32_t dest_stride = data->width * 4;

	char* img = vke_image_map (stagImg);
	char* row = (char*)bitmap;
	for (uint32_t y = 0; y < data->height; y++) {
		memcpy(row, img, dest_stride);
		row += dest_stride;
		img += stride;
	}

	vke_image_unmap (stagImg);
	vke_image_drop(stagImg);

	io_unsync(data);

	return VKE_STATUS_SUCCESS;
}
