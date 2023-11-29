#pragma once
#include <mx/mx.hpp>
#include <media/image.hpp>
namespace ion {
struct i264e;
struct h264e:mx {
    mx_declare(h264e, mx, i264e);
    h264e(lambda<bool(mx)> output);
    void push(yuv420 frame);
    async run();
};
}