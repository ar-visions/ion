#include <camera/camera.hpp>
#import <camera/apple.h>
#include <memory>

using namespace ion;


struct opaque_capture {
    metal_capture *capture;
};

/// we need to retain this texture until the user is done with it; thats when the user is done converting to a normal vulkan texture
/// through the metal extension
void global_callback(void *metal_texture, void *metal_layer, void *context) {
    ((Camera*)context)->on_frame(metal_texture, metal_layer, ((Camera*)context)->user_data); /// make sure this is a synchronous function
}

void Camera::start_capture(lambda<void(void*,void*)> _on_frame, void *user) {
    on_frame         = _on_frame;
    user_data        = user;
    capture          = new opaque_capture();
    capture->capture = [[metal_capture alloc] initWithCallback:global_callback context:(void*)this];
    [capture->capture startCapture];
}

void Camera::stop_capture() {
    [capture->capture stopCapture];
    delete capture;
    capture = null;
}