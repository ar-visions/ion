
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "GLFW/glfw3.h"
#include "dawn/samples/SampleUtils.h"
#include "dawn/common/Assert.h"
#include "dawn/common/Log.h"
#include "dawn/common/Platform.h"
#include "dawn/common/SystemUtils.h"
#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"
#include "webgpu/webgpu_glfw.h"
#include <dawn/dawn.hpp>

namespace ion {

struct IDawn {

    #if defined(WIN32)
        wgpu::BackendType backend_type = wgpu::BackendType::D3D12;
    #elif defined(__APPLE__)
        wgpu::BackendType backend_type = wgpu::BackendType::Metal;
    #elif defined(__linux__)
        // android, chromeos, linux distros, all use Vulkan
        // OpenGL is not the same level of implementation in Dawn, not at the 'Surface' level
        wgpu::BackendType backend_type = wgpu::BackendType::Vulkan;
    #endif

    std::unique_ptr<dawn::native::Instance> instance;
    register(IDawn);
    // some would not like registration of types,
    // it is not made accessible at a field level, without a meta()
};

struct IWindow {
    Dawn dawn;
    str title;
    GLFWwindow* handle;
    wgpu::AdapterType adapter_type = wgpu::AdapterType::Unknown;
    lambda<void(Key, num, uint32_t)> key;
    lambda<void(vec2f, uint32_t)> mouse_move;
    lambda<void(vec2f, uint32_t)> mouse_up;
    lambda<void()> loop;
    wgpu::Device device;
    wgpu::SwapChain swap;
    vec2f sz;
    void *user_data;

    static inline int count = 0;

    void create_dawn_device();
    void close();
    ~IWindow();
    register(IWindow);
};

mx_implement(Window, mx);

mx_implement(Dawn, mx);

static void PrintDeviceError(WGPUErrorType errorType, const char* message, void*) {
    const char* errorTypeName = "";
    switch (errorType) {
        case WGPUErrorType_Validation:
            errorTypeName = "Validation";
            break;
        case WGPUErrorType_OutOfMemory:
            errorTypeName = "Out of memory";
            break;
        case WGPUErrorType_Unknown:
            errorTypeName = "Unknown";
            break;
        case WGPUErrorType_DeviceLost:
            errorTypeName = "Device lost";
            break;
        default:
            DAWN_UNREACHABLE();
            return;
    }
    dawn::ErrorLog() << errorTypeName << " error: " << message;
}

static void DeviceLostCallback(WGPUDeviceLostReason reason, const char* message, void*) {
    dawn::ErrorLog() << "Device lost: " << message;
}

static void PrintGLFWError(int code, const char* message) {
    dawn::ErrorLog() << "GLFW error: " << code << " - " << message;
}

static void DeviceLogCallback(WGPULoggingType type, const char* message, void*) {
    dawn::ErrorLog() << "Device log: " << message;
}

IWindow::~IWindow() {
    close();
}

void IWindow::close() {
    //device.release();
    glfwDestroyWindow(handle);
    if (--IWindow::count == 0)
        glfwTerminate();
}

void *Window::handle() {
    return data->handle;
}

Window Window::create(str title, vec2f sz) {
    static bool init;
    if (!init) {
        assert(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
        glfwSetErrorCallback(PrintGLFWError);
        init = true;
    }
    Window w;
    w->sz = sz;
    w->title = title;
    w->handle = glfwCreateWindow(sz.x, sz.y, title.cs(), null, null);
    IWindow::count++;
    w->create_dawn_device();
    return w;
}

wgpu::Device Window::device() {
    return data->device;
}

void Window::set_visibility(bool v) {
    if (v)
        glfwShowWindow(data->handle);
    else
        glfwHideWindow(data->handle);
}

void Window::close() {
    data->close();
}

str Window::title() {
    return data->title;
}

void Window::set_title(str title) {
    glfwSetWindowTitle(data->handle, title.cs());
    data->title = title;
}

void Window::on_key(lambda<void(Key, num, uint32_t)> key) {
    data->key = key;
}

void Window::on_mouse_move(lambda<void(vec2f, uint32_t)> mouse_move) {
    data->mouse_move = mouse_move;
}

void Window::on_mouse_up(lambda<void(vec2f, uint32_t)> mouse_up) {
    data->mouse_up = mouse_up;
}

void Window::run(lambda<void()> loop) {
    data->loop = loop;
    while (!glfwWindowShouldClose(data->handle)) {
        glfwPollEvents();
        data->loop();
    }
}

void IWindow::create_dawn_device() {
    if (!dawn->instance) {
        WGPUInstanceDescriptor instanceDescriptor{};
        instanceDescriptor.features.timedWaitAnyEnable = true;
        dawn->instance = std::make_unique<dawn::native::Instance>(&instanceDescriptor);
    }
    wgpu::RequestAdapterOptions options = {};
    options.backendType = dawn->backend_type;

    // Get an adapter for the backend to use, and create the device.
    auto adapters = dawn->instance->EnumerateAdapters(&options);
    wgpu::DawnAdapterPropertiesPowerPreference power_props{};
    wgpu::AdapterProperties adapterProperties{};
    adapterProperties.nextInChain = &power_props;
    // Find the first adapter which satisfies the adapterType requirement.
    auto isAdapterType = [&](const auto& adapter) -> bool {
        // picks the first adapter when adapterType is unknown.
        if (adapter_type == wgpu::AdapterType::Unknown) {
            return true;
        }
        adapter.GetProperties(&adapterProperties);
        return adapterProperties.adapterType == adapter_type;
    };
    auto preferredAdapter = std::find_if(adapters.begin(), adapters.end(), isAdapterType);
    if (preferredAdapter == adapters.end()) {
        fprintf(stderr, "Failed to find an adapter! Please try another adapter type.\n");
        return;
    }

    std::vector<const char*> enableToggleNames;
    std::vector<const char*> disabledToggleNames;
    WGPUDawnTogglesDescriptor toggles;
    toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
    toggles.chain.next = nullptr;
    toggles.enabledToggles = enableToggleNames.data();
    toggles.enabledToggleCount = enableToggleNames.size();
    toggles.disabledToggles = disabledToggleNames.data();
    toggles.disabledToggleCount = disabledToggleNames.size();

    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&toggles);

    WGPUDevice backendDevice = preferredAdapter->CreateDevice(&deviceDesc);
    DawnProcTable backendProcs = dawn::native::GetProcs();

    // Create the swapchain
    auto surfaceChainedDesc = wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(handle);
    WGPUSurfaceDescriptor surfaceDesc;
    surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(surfaceChainedDesc.get());
    WGPUSurface surface = backendProcs.instanceCreateSurface(dawn->instance->Get(), &surfaceDesc);

    WGPUSwapChainDescriptor swapChainDesc = {};
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.format = static_cast<WGPUTextureFormat>(wgpu::TextureFormat::BGRA8Unorm);
    swapChainDesc.width = (u32)sz.x;
    swapChainDesc.height = (u32)sz.y;
    swapChainDesc.presentMode = WGPUPresentMode_Mailbox;
    WGPUSwapChain backendSwapChain =
        backendProcs.deviceCreateSwapChain(backendDevice, surface, &swapChainDesc);

    // Choose whether to use the backend procs and devices/swapchains directly, or set up the wire.
    WGPUDevice cDevice = nullptr;
    DawnProcTable procs;

    procs = backendProcs;
    cDevice = backendDevice;
    swap = wgpu::SwapChain::Acquire(backendSwapChain);

    dawnProcSetProcs(&procs);
    procs.deviceSetUncapturedErrorCallback(cDevice, PrintDeviceError, nullptr);
    procs.deviceSetDeviceLostCallback(cDevice, DeviceLostCallback, nullptr);
    procs.deviceSetLoggingCallback(cDevice, DeviceLogCallback, nullptr);
    device = wgpu::Device::Acquire(cDevice);
}

wgpu::SwapChain Window::swap_chain() {
    return data->swap;
}

void Window::process_events() {
    data->dawn.process_events();
}

void *Window::user_data() {
    return data->user_data;
}

void Window::set_user_data(void *user_data) {
    data->user_data = user_data;
}

wgpu::TextureView Window::depth_stencil_view() {
    wgpu::TextureDescriptor descriptor;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size.width = (u32)data->sz.x;
    descriptor.size.height = (u32)data->sz.y;
    descriptor.size.depthOrArrayLayers = 1;
    descriptor.sampleCount = 1;
    descriptor.format = wgpu::TextureFormat::Depth24PlusStencil8;
    descriptor.mipLevelCount = 1;
    descriptor.usage = wgpu::TextureUsage::RenderAttachment;
    auto depthStencilTexture = data->device.CreateTexture(&descriptor);
    return depthStencilTexture.CreateView();
}

void Dawn::process_events() {
    dawn::native::InstanceProcessEvents(data->instance->Get());
}

}