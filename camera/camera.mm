#include <camera/camera.hpp>
#ifdef __APPLE__
#import <camera/apple.h>
#endif
#include <memory>
//#include <vk/vk.hpp>
//#include <vkh/vkh_device.h>
//#include <vkh/vkh_image.h>

using namespace ion;

Camera::Camera() { }

//Camera::Camera(VkEngine e, int camera_index, int width, int height, int rate) : 
//    e(e), camera_index(camera_index), width(width), height(height), rate(rate) { }

#ifdef __APPLE__

struct opaque_capture {
    metal_capture *capture;
    opaque_capture() { }
};

/*
/// this 'camera' capture method is not ideal due to its texture-based model
/// universally we need h264 data only.  we stream this, not images and we would never, Ever want to convert from image to h264 because it likely already has been compressed
/// this delay is not ideal for performant runtime

void global_callback(void *metal_texture, void *metal_layer, void *context) {
    Camera *camera = (Camera*)context;
    camera->e->vk_device->mtx.lock();
    camera->image = vkh_image_create(
        camera->e->vkh, VK_FORMAT_B8G8R8A8_UNORM, u32(camera->width), u32(camera->height),
        VK_IMAGE_TILING_OPTIMAL, VKH_MEMORY_USAGE_GPU_ONLY,
        VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT, metal_texture);
    camera->e->vk_device->mtx.unlock();
}

todo:
convert metal_texture to h264 nalu packet src
*/

/// Camera will resolve the vulkan texture in this module (not doing this in ux/app lol)
void Camera::start_capture() {
    capture          = new opaque_capture();
    //capture->capture = [[metal_capture alloc] initWithCallback:global_callback context:(void*)this camera_index:camera_index];
    [capture->capture startCapture];
}

void Camera::stop_capture() {
    [capture->capture stopCapture];
    delete capture;
    capture = null;
}

#else

void Camera::start_capture() {
}

void Camera::stop_capture() {
}

#endif