#pragma once

/// glfw (native) and dawn would be a webgpu minimal interface to us

#include <GLFW/glfw3.h>

#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
/// X11 globals that conflict with Dawn
#undef None
#undef Success
#undef Always
#undef Bool
#endif

#include "dawn/samples/SampleUtils.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/SystemUtils.h"
#include "dawn/utils/WGPUHelpers.h"

#include <dawn/webgpu_cpp.h>
#include "dawn/common/Assert.h"
#include "dawn/common/Log.h"
#include "dawn/common/Platform.h"
#include "dawn/common/SystemUtils.h"
#include "dawn/common/Constants.h"
#include "dawn/dawn_proc.h"
#include "webgpu/webgpu_glfw.h"
#include <dawn/native/DawnNative.h>

#include "include/gpu/graphite/dawn/DawnUtils.h"
#include "include/gpu/graphite/dawn/DawnTypes.h"