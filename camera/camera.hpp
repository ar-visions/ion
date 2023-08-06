#pragma once
#include <media/media.hpp>

struct opaque_capture;

namespace ion {

struct Camera {
    lambda<void(void*, void*)> on_frame;

    void start_capture(lambda<void(void*, void*, void*)>, void *user_data); /// rate and resolution should be provided as well as a callback
    void stop_capture();

    opaque_capture *capture;
};

}