/// our dawn interface is glfw, webgpu api and dawn-native
/// they cast in like rays

#pragma once

#include <dawn/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>
#include <async/async.hpp>
#include <dawn/window.hpp>

int dawn_test();

namespace ion {
struct IDawn;
struct Dawn:mx {
    void process_events();
    mx_declare(Dawn, mx, IDawn);
};

template <> struct is_singleton<IDawn> : true_type { };
template <> struct is_singleton<Dawn> : true_type { };
}