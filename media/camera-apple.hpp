#pragma once
#include <mx/mx.hpp>
//#include <media/media.hpp>
//#include <vk/vkh.h>
//#include <vkg/vkg.hpp>

struct AppleCapture_;

namespace ion {

typedef void (*CaptureCallback)(void*, void*, int);

struct Camera {
    //VkEngine        e            = null;
    CaptureCallback callback;
    void*           user;
    int             camera_index = 0;
    int             width        = 1920;
    int             height       = 1080;
    int             rate         = 30;
    //VkhImage        image        = null;
    AppleCapture_ *capture;

    Camera(void *user, CaptureCallback callback);
    //Camera(VkEngine e, int camera_index, int width, int height, int max_rate);

    static void on_data(void *user, void *buf, int len);
    void start_capture(); /// rate and resolution should be provided as well as a callback
    void stop_capture();
};

}