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
#include <vkh/vkh_buffer.h>
#include <vkh/vkh_device.h>

void vkh_buffer_init(VkhDevice vkh, VkBufferUsageFlags usage, VkhMemoryUsage memprops, VkDeviceSize size, VkhBuffer buff, bool mapped){
	buff->vkh = vkh_device_grab(vkh);
	
	VkBufferCreateInfo* pInfo = &buff->infos;
	pInfo->sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	pInfo->usage		= usage;
	pInfo->size			= size;
	pInfo->sharingMode	= VK_SHARING_MODE_EXCLUSIVE;
	buff->allocCreateInfo.usage	= (VmaMemoryUsage)memprops;
	if (mapped)
		buff->allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	VK_CHECK_RESULT(vmaCreateBuffer(vkh->e->allocator, pInfo, &buff->allocCreateInfo, &buff->buffer, &buff->alloc, &buff->allocInfo));
}

VkhBuffer vkh_buffer_create(VkhDevice vkh, VkBufferUsageFlags usage, VkhMemoryUsage memprops, VkDeviceSize size){
	VkhBuffer buff = (VkhBuffer)calloc(1, sizeof(vkh_buffer_t));
	vkh_buffer_init(vkh, usage, memprops, size, buff, false);
	return buff;
}

void vkh_buffer_reset(VkhBuffer buff){
	if (buff->buffer)
		vmaDestroyBuffer(buff->vkh->e->allocator, buff->buffer, buff->alloc);
}

void vkh_buffer_destroy(VkhBuffer buff){
	if (buff->buffer)
		vmaDestroyBuffer(buff->vkh->e->allocator, buff->buffer, buff->alloc);
	free(buff);
	buff = NULL;
}

void vkh_buffer_resize(VkhBuffer buff, VkDeviceSize newSize, bool mapped){
	vkh_buffer_reset(buff);
	buff->infos.size = newSize;
	VK_CHECK_RESULT(vmaCreateBuffer(buff->vkh->e->allocator, &buff->infos, &buff->allocCreateInfo, &buff->buffer, &buff->alloc, &buff->allocInfo));
}

VkDescriptorBufferInfo vkh_buffer_get_descriptor (VkhBuffer buff){
	VkDescriptorBufferInfo desc = { };
	desc.buffer = buff->buffer;
	desc.offset = 0;
	desc.range	= VK_WHOLE_SIZE;
	return desc;
}

VkResult vkh_buffer_map(VkhBuffer buff){
	return vmaMapMemory(buff->vkh->e->allocator, buff->alloc, &buff->mapped);
}

void vkh_buffer_unmap(VkhBuffer buff){
	vmaUnmapMemory(buff->vkh->e->allocator, buff->alloc);
}

VkBuffer vkh_buffer_get_vkbuffer (VkhBuffer buff){
	return buff->buffer;
}

void* vkh_buffer_get_mapped_pointer (VkhBuffer buff){
	//vmaFlushAllocation (buff->vkh->allocator, buff->alloc, buff->allocInfo.offset, buff->allocInfo.size);
	return buff->allocInfo.pMappedData;
}

void vkh_buffer_flush (VkhBuffer buff){
	vmaFlushAllocation (buff->vkh->e->allocator, buff->alloc, buff->allocInfo.offset, buff->allocInfo.size);
}
