#pragma once
#include <media/media.hpp>
#include <vkh/vkh.h>

struct opaque_capture;

namespace ion {

struct Camera {
    VkEngine        e;
    int             camera_index;
    //VkvgSurface     surface;
    VkhImage        image; // -- todo: vulkan extension needs to work to convert the metal texture to vk.  the documented one does not seem to work. it should be traced
    opaque_capture *capture;

    Camera();
    Camera(VkEngine e, int camera_index);

    void start_capture(); /// rate and resolution should be provided as well as a callback
    void stop_capture();
};

}