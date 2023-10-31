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
#include <vkh/vkh_image.h>
#include <vkh/vkh_device.h>

VkhImage _vkh_image_create (VkhDevice vkh, VkImageType imageType,
				  VkFormat format, uint32_t width, uint32_t height,
				  VkhMemoryUsage memprops, VkImageUsageFlags usage,
				  VkSampleCountFlagBits samples, VkImageTiling tiling,
				  uint32_t mipLevels, uint32_t arrayLayers, void *import = nullptr){

	VkhImage img = (VkhImage)calloc(1,sizeof(vkh_image_t));

	img->vkh = vkh_device_grab(vkh);
	img->width = width;
	img->height = height;

	VkImageCreateInfo* pInfo = &img->infos;
	pInfo->sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	pInfo->imageType		= imageType;
	pInfo->tiling			= tiling;
	pInfo->initialLayout	= (tiling == VK_IMAGE_TILING_OPTIMAL) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED;
	pInfo->sharingMode		= VK_SHARING_MODE_EXCLUSIVE;
	pInfo->usage			= usage;
	pInfo->format			= format;
	pInfo->extent.width		= width;
	pInfo->extent.height	= height;
	pInfo->extent.depth		= 1;
	pInfo->mipLevels		= mipLevels;
	pInfo->arrayLayers		= arrayLayers;
	pInfo->samples			= samples;

	/// support different forms of platform imports (dx on windows, metal on macos, hopefully nothing on linux)
    if (import) {
#ifdef __APPLE__
        struct MTLTexture;
        VkImportMetalTextureInfoEXT metal_import = {
            .sType 		= VK_STRUCTURE_TYPE_IMPORT_METAL_TEXTURE_INFO_EXT,
            .plane 		= VK_IMAGE_ASPECT_COLOR_BIT,
            .mtlTexture = (MTLTexture*)import
        };
        pInfo->pNext = &metal_import;
#endif
    }
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = (VmaMemoryUsage)memprops;
	VK_CHECK_RESULT(vmaCreateImage (vkh->e->allocator, pInfo, &allocInfo, &img->image, &img->alloc, &img->allocInfo));
	mtx_init(&img->mutex, mtx_plain);
	img->refs = 1;
	return img;
}
void vkh_image_drop(VkhImage img)
{
	if (img==NULL)
		return;

	mtx_lock (&img->mutex);
	img->refs--;
	if (img->refs > 0) {
		mtx_unlock (&img->mutex);
		return;
	}

	mtx_unlock (&img->mutex);
	mtx_destroy (&img->mutex);

	vkh_device_drop(img->vkh);

	if(img->view != VK_NULL_HANDLE)
		vkDestroyImageView (img->vkh->device,img->view, NULL);
	if(img->sampler != VK_NULL_HANDLE)
		vkDestroySampler (img->vkh->device,img->sampler, NULL);

	if (!img->imported) {
		vmaDestroyImage	(img->vkh->e->allocator, img->image, img->alloc);
	}

	free(img);
	img = NULL;
}

VkhImage vkh_image_grab (VkhImage img) {
	mtx_lock	(&img->mutex);
	img->refs++;
	mtx_unlock	(&img->mutex);
	return img;
}

VkhImage vkh_tex2d_array_create (VkhDevice vkh,
							 VkFormat format, uint32_t width, uint32_t height, uint32_t layers,
							 VkhMemoryUsage memprops, VkImageUsageFlags usage){
	return _vkh_image_create (vkh, VK_IMAGE_TYPE_2D, format, width, height, memprops,usage,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 1, layers);
}

/// Vkh Device needs a command pool.  its just not complete as-is

VkhImage vkh_image_create (VkhDevice vkh,
						   VkFormat format, uint32_t width, uint32_t height, VkImageTiling tiling,
						   VkhMemoryUsage memprops,
						   VkImageUsageFlags usage, void *import)
{
	return _vkh_image_create (vkh, VK_IMAGE_TYPE_2D, format, width, height, memprops,usage,
					  VK_SAMPLE_COUNT_1_BIT, tiling, 1, 1, import);
}

//create vkhImage from existing VkImage
VkhImage vkh_image_import (VkhDevice vkh, VkImage vkImg, VkFormat format, uint32_t width, uint32_t height) {
	VkhImage img = (VkhImage)calloc(1,sizeof(vkh_image_t));
	img->vkh		= vkh_device_grab(vkh);
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
	img->refs   			= 1;

	mtx_init (&img->mutex, mtx_plain);

	return img;
}
VkhImage vkh_image_ms_create(VkhDevice vkh,
						   VkFormat format, VkSampleCountFlagBits num_samples, uint32_t width, uint32_t height,
						   VkhMemoryUsage memprops,
						   VkImageUsageFlags usage){
   return  _vkh_image_create (vkh, VK_IMAGE_TYPE_2D, format, width, height, memprops,usage,
					  num_samples, VK_IMAGE_TILING_OPTIMAL, 1, 1);
}

void vkh_image_create_view (VkhImage img, VkImageViewType viewType, VkImageAspectFlags aspectFlags)
{
	if(img->view != VK_NULL_HANDLE)
		vkDestroyImageView	(img->vkh->device,img->view,NULL);

	VkImageViewCreateInfo viewInfo = { };
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = img->image;
	viewInfo.viewType = viewType;
	viewInfo.format = img->infos.format;
	viewInfo.components = {VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B,VK_COMPONENT_SWIZZLE_A};
	viewInfo.subresourceRange = {aspectFlags,0,1,0,img->infos.arrayLayers};
	VK_CHECK_RESULT(vkCreateImageView(img->vkh->device, &viewInfo, NULL, &img->view));
}

void vkh_image_create_sampler (VkhImage img, VkFilter magFilter, VkFilter minFilter,
							   VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode){
	if(img->sampler != VK_NULL_HANDLE)
		vkDestroySampler	(img->vkh->device,img->sampler,NULL);
	VkSamplerCreateInfo samplerCreateInfo = { };

	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter	= magFilter;
	samplerCreateInfo.minFilter	= minFilter;
	samplerCreateInfo.mipmapMode	= mipmapMode;
	samplerCreateInfo.addressModeU = addressMode;
	samplerCreateInfo.addressModeV = addressMode;
	samplerCreateInfo.addressModeW = addressMode;
	samplerCreateInfo.maxAnisotropy= 1.0;

	VK_CHECK_RESULT(vkCreateSampler(img->vkh->device, &samplerCreateInfo, NULL, &img->sampler));
}
void vkh_image_set_sampler (VkhImage img, VkSampler sampler){
	img->sampler = sampler;
}
void vkh_image_create_descriptor(VkhImage img, VkImageViewType viewType, VkImageAspectFlags aspectFlags, VkFilter magFilter,
								 VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode)
{
	vkh_image_create_view		(img, viewType, aspectFlags);
	vkh_image_create_sampler	(img, magFilter, minFilter, mipmapMode, addressMode);
}
VkImage vkh_image_get_vkimage (VkhImage img){
	return img->image;
}
VkSampler vkh_image_get_sampler (VkhImage img){
	if (img == NULL)
		return NULL;
	return img->sampler;
}
VkImageView vkh_image_get_view (VkhImage img){
	if (img == NULL)
		return NULL;
	return img->view;
}
VkImageLayout vkh_image_get_layout (VkhImage img){
	if (img == NULL)
		return VK_IMAGE_LAYOUT_UNDEFINED;
	return img->layout;
}
VkDescriptorImageInfo vkh_image_get_descriptor (VkhImage img, VkImageLayout imageLayout){
	VkDescriptorImageInfo desc = { };
	desc.sampler = img->sampler;
	desc.imageView = img->view;
	desc.imageLayout = imageLayout;
	return desc;
}
/*
/// dont need args for the other things until i need them
void vkh_image_set_layout_sync(VkhImage image, VkImageLayout layout) {
	
	VkCommandBuffer commandBuffer = image->vkh->e->vk_device->command_begin();
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = image->layout;
	barrier.newLayout = layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image->image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	switch (image->layout) {
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

	switch (layout) {
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

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	image->vkh->e->vk_device->command_submit(commandBuffer);
	image->layout = layout;
}
*/

void vkh_image_set_layout(VkCommandBuffer cmdBuff, VkhImage image, VkImageAspectFlags aspectMask,
						  VkImageLayout old_image_layout, VkImageLayout new_image_layout,
					  VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
	VkImageSubresourceRange subres = {aspectMask,0,1,0,1};
	vkh_image_set_layout_subres(cmdBuff, image, subres, old_image_layout, new_image_layout, src_stages, dest_stages);
}

void vkh_image_set_layout_subres(VkCommandBuffer cmdBuff, VkhImage image, VkImageSubresourceRange subresourceRange,
							 VkImageLayout old_image_layout, VkImageLayout new_image_layout,
							 VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
	VkImageMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = image->layout;
	barrier.newLayout = new_image_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image->image;
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
	image->layout = new_image_layout;
}
void vkh_image_destroy_sampler (VkhImage img) {
	if (img==NULL)
		return;
	if(img->sampler != VK_NULL_HANDLE)
		vkDestroySampler	(img->vkh->device,img->sampler,NULL);
	img->sampler = VK_NULL_HANDLE;
}

void* vkh_image_map (VkhImage img) {
	void* data;
	vmaMapMemory(img->vkh->e->allocator, img->alloc, &data);
	return data;
}

void vkh_image_unmap (VkhImage img) {
	vmaUnmapMemory(img->vkh->e->allocator, img->alloc);
}

void vkh_image_set_name (VkhImage img, const char* name){
	if (img==NULL)
		return;
	vkh_device_set_object_name(img->vkh, VK_OBJECT_TYPE_IMAGE, (uint64_t)img->image, name);
}

uint64_t vkh_image_get_stride (VkhImage img) {
	VkImageSubresource subres = {VK_IMAGE_ASPECT_COLOR_BIT,0,0};
	VkSubresourceLayout layout = {0};
	vkGetImageSubresourceLayout(img->vkh->device, img->image, &subres, &layout);
	return (uint64_t) layout.rowPitch;
}
