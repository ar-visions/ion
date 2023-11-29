#pragma once
#include <mx/mx.hpp>
#include <media/image.hpp>
namespace ion {
struct i264;
struct h264:mx {
    mx_declare(h264, mx, i264);
    h264(lambda<bool(mx)> output);
    void push(yuv420 frame);
    async run();
    yuv420 decode(array<u8> nalu);
};
}