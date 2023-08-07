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
        camera->e->vkh, VK_FORMAT_B8G8R8A8_UNORM, 1920, 1080, VK_IMAGE_TILING_OPTIMAL, VKH_MEMORY_USAGE_GPU_ONLY,
        VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT, metal_texture);

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