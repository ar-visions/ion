#pragma once
#include <mx/mx.hpp>

struct GLFWwindow;

namespace ion {

struct iCapture;
struct  Capture: mx {
    using OnData   = lambda<bool(u64, mx)>;
    using OnClosed = lambda<void(iCapture*)>;
    ///
    Capture(GLFWwindow *glfw, OnData on_data, OnClosed closed);
    void stop();
    ///
    mx_declare(Capture, mx, iCapture);
};

}
