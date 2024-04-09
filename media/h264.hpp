#pragma once
#include <mx/mx.hpp>
#include <media/image.hpp>
namespace ion {
struct i264;
struct h264:mx {
    mx_declare(h264, mx, i264);
    array encode(yuv420 frame);
};
}