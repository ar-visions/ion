#include <camera/camera.hpp>
#import <camera/apple.h>
#include <memory>
#include <vk/vk.hpp>
#include <vkh/vkh_image.h>

using namespace ion;


struct opaque_capture {
    metal_capture *capture;
};

/// we need to retain this texture until the user is done with it; thats when the user is done converting to a normal vulkan texture
/// through the metal extension
void global_callback(void *metal_texture, void *metal_layer, void *context) {
    Camera *camera = (Camera*)context;
    camera->e->vk_device->mtx.lock();

    if (camera->image)
        vkh_image_drop(camera->image);

    camera->image = vkh_image_create(
        camera->e->vkh, VK_FORMAT_B8G8R8A8_UNORM, 1920, 1080, VK_IMAGE_TILING_OPTIMAL,
        VKH_MEMORY_USAGE_CPU_TO_GPU, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, null);



    // Assuming you have a Vulkan device, VkImage, VkImageView, and VkCommandBuffer set up.

    // Step 1: Transition VkImage layout to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    // (You need to do this if the image is not already in TRANSFER_SRC_OPTIMAL layout)

    u32 width = 1920;
    u32 height = 1080;
    u32 imageSize = width * height * 4;
    VkDevice device = camera->e->vk_device;
    VkImage image = camera->image->image;

    // Step 2: Create a staging buffer
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = imageSize; // The size of the image data
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VkBuffer stagingBuffer;
    vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer);

    // Allocate memory for the staging buffer
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = camera->e->vk_gpu->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory stagingBufferMemory;
    vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory);


    


    void* mappedData;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &mappedData);

    // Write test color data to the staging buffer
    // Assuming you have a 32-bit RGBA color (e.g., R: 255, G: 0, B: 0, A: 255)
    uint32_t* pData = static_cast<uint32_t*>(mappedData);
    for (size_t i = 0; i < imageSize / 4; i++)
        pData[i] = 0xFF0000FF; // RGBA color

    // Unmap staging buffer memory
    vkUnmapMemory(device, stagingBufferMemory);

    VkCommandBuffer commandBuffer = camera->e->vk_device->beginSingleTimeCommands();

    VkImageMemoryBarrier imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.srcAccessMask = 0; // No access flags required as the image is in an undefined layout
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // Allow transfer write after the barrier
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = image; // Your VkImage object
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Assuming the image is a color image
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = 1;

    // Transition VkImage layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    // Assuming you have a valid command buffer (cmdBuffer) and a valid image memory barrier (imageBarrier) set up for this transition.
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    // Copy data from staging buffer to VkImage
    VkBufferImageCopy bufferImageCopyRegion = {};
    bufferImageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopyRegion.imageSubresource.layerCount = 1;
    bufferImageCopyRegion.imageExtent.width = width;
    bufferImageCopyRegion.imageExtent.height = height;
    bufferImageCopyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopyRegion);


    camera->e->vk_device->endSingleTimeCommands(commandBuffer);


    camera->e->vk_device->mtx.unlock();
}

Camera::Camera() : e((VkEngine)null), camera_index(0), image((VkhImage)null), capture((opaque_capture*)null) { }

Camera::Camera(VkEngine e, int camera_index) : e(e), camera_index(camera_index), image((VkhImage)null), capture((opaque_capture*)null) { }

/// Camera will resolve the vulkan texture in this module (not doing this in ux/app lol)
void Camera::start_capture() {
    capture          = new opaque_capture();
    capture->capture = [[metal_capture alloc] initWithCallback:global_callback context:(void*)this camera_index:camera_index];
    [capture->capture startCapture];
}

void Camera::stop_capture() {
    [capture->capture stopCapture];
    delete capture;
    capture = null;
}