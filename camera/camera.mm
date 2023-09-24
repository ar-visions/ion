#include <camera/camera.hpp>
#ifdef __APPLE__
#import <camera/apple.h>
#endif
#include <memory>
//#include <vk/vk.hpp>
//#include <vkh/vkh_device.h>
//#include <vkh/vkh_image.h>

using namespace ion;

namespace ion {

Camera::Camera(void *user, CaptureCallback callback) : user(user), callback(callback) { }

#ifdef __APPLE__

/*
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


void Camera::on_data(void *user, void *data, int len) {
    printf("got data: %p, %d\n", user, len);
}

/// Camera will resolve the vulkan texture in this module (not doing this in ux/app lol)
void Camera::start_capture() {
    //capture->capture = [[metal_capture alloc] initWithCallback:global_callback context:(void*)this camera_index:camera_index];
    AppleCapture *cap = [AppleCapture new];
    [cap start:0 width:1920 height:1080 user:(void*)this callback:&Camera::on_data];

    capture = (AppleCapture_*)CFBridgingRetain(cap);

}

void Camera::stop_capture() {
    [((__bridge AppleCapture*)capture) stop];
    capture = null;
}

#else

void Camera::start_capture() {
}

void Camera::stop_capture() {
}

#endif

}