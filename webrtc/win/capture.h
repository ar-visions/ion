#pragma once
#include <mx/mx.hpp>

namespace ion {

struct iCapture;
struct  Capture: mx {
    Capture(HWND hwnd, lambda<bool(mx)> on_data, lambda<void(iCapture*)> closed);
    void stop();
    mx_declare(Capture, mx, iCapture);
};

}
