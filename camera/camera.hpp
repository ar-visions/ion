#pragma once
#include <media/media.hpp>
#include <vkh/vkh.h>
//#include <vkg/vkg.hpp>

struct opaque_capture;

namespace ion {

struct Camera {
    VkEngine        e            = null;
    int             camera_index = 0;
    int             width        = 1920;
    int             height       = 1080;
    int             rate         = 30;
    VkhImage        image        = null;
    opaque_capture *capture;

    Camera();
    Camera(VkEngine e, int camera_index, int width, int height, int max_rate);

    void start_capture(); /// rate and resolution should be provided as well as a callback
    void stop_capture();
};

}