#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

/// X11 global pollution
#undef None
#undef Success
#undef Always
#undef Bool

#include <vector>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

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
#include <tools/window/unix/WindowContextFactory_unix.h>
#include <tools/window/WindowContext.h>
#include <gpu/graphite/BackendTexture.h>
#include <gpu/graphite/dawn/DawnBackendContext.h>
#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Surface.h>
#include <gpu/graphite/Surface_Graphite.h>
#include <tools/GpuToolUtils.h>

#include <async/async.hpp>
#include <math/math.hpp>

using namespace ion;

namespace ion {
struct IDawn;
struct Dawn:mx {
    void process_events();
    static float get_dpi();
    mx_declare(Dawn, mx, IDawn);
};

template <> struct is_singleton<IDawn> : true_type { };
template <> struct is_singleton<Dawn> : true_type { };
}

namespace ion {

struct IDawn {
    #if defined(WIN32)
        wgpu::BackendType backend_type = wgpu::BackendType::D3D12;
    #elif defined(__APPLE__)
        wgpu::BackendType backend_type = wgpu::BackendType::Metal;
    #elif defined(__linux__)
        wgpu::BackendType backend_type = wgpu::BackendType::Vulkan;
    #endif
    std::unique_ptr<dawn::native::Instance> instance;
    register(IDawn);
};

enums(Key, undefined,
    undefined=0,
    Space=32,
    Apostrophe=39,
    Comma=44,
    Minus=45,
    Period=46,
    Slash=47,
    k_0=48,
    k_1=49,
    k_2=50,
    k_3=51,
    k_4=52,
    k_5=53,
    k_6=54,
    k_7=55,
    k_8=56,
    k_9=57,
    SemiColon=59,
    Equal=61,
    A=65,
    B=66,
    C=67,
    D=68,
    E=69,
    F=70,
    G=71,
    H=72,
    I=73,
    J=74,
    K=75,
    L=76,
    M=77,
    N=78,
    O=79,
    P=80,
    Q=81,
    R=82,
    S=83,
    T=84,
    U=85,
    V=86,
    W=87,
    X=88,
    Y=89,
    Z=90,
    LeftBracket=91,
    BackSlash=92,
    RightBracket=93,
    GraveAccent=96,
    World1 = 161,
    World2 = 162,
    Escape=256,
    ENTER=257,
    TAB=258,
    BACKSPACE=259,
    INSERT=260,
    DELETE=261,
    RIGHT=262,
    LEFT=263,
    DOWN=264,
    UP=265,
    PAGE_UP=266,
    PAGE_DOWN=267,
    HOME=268,
    END=269,
    CAPS_LOCK=280,
    SCROLL_LOCK=281,
    NUM_LOCK=282,
    PRINT_SCREEN=283,
    PAUSE=284,
    F1=290,
    F2=291,
    F3=292,
    F4=293,
    F5=294,
    F6=295,
    F7=296,
    F8=297,
    F9=298,
    F10=299,
    F11=300,
    F12=301,
    F13=302,
    F14=303,
    F15=304,
    F16=305,
    F17=306,
    F18=307,
    F19=308,
    F20=309,
    F21=310,
    F22=311,
    F23=312,
    F24=313,
    F25=314,
    KP_0=320,
    KP_1=321,
    KP_2=322,
    KP_3=323,
    KP_4=324,
    KP_5=325,
    KP_6=326,
    KP_7=327,
    KP_8=328,
    KP_9=329,
    KP_DECIMAL=330,
    KP_DIVIDE=331,
    KP_MULTIPLY=332,
    KP_SUBTRACT=333,
    KP_ADD=334,
    KP_ENTER=335,
    KP_EQUAL=336,
    LEFT_SHIFT=340,
    LEFT_CONTROL=341,
    LEFT_ALT=342,
    LEFT_SUPER=343,
    RIGHT_SHIFT=344,
    RIGHT_CONTROL=345,
    RIGHT_ALT=346,
    RIGHT_SUPER=347,
    MENU=348
)

struct IWindow;
struct Window:mx {
    static Window create(str title, vec2i sz);
    void set_visibility(bool v);
    void close();
    str  title();
    void set_title(str title);
    void on_key(lambda<void(Key, num, uint32_t)> key);
    void on_mouse_move(lambda<void(vec2f, uint32_t)> mouse_move);
    void on_mouse_up(lambda<void(vec2f, uint32_t)> mouse_up);
    void run(lambda<void()> loop);
    void *handle();
    void process_events();
    void *user_data();
    void set_user_data(void *);
    vec2i size();

    operator bool();

    wgpu::Device device();
    wgpu::SwapChain swap_chain();
    wgpu::TextureView depth_stencil_view();

    mx_declare(Window, mx, IWindow)
};

struct IWindow {
    Dawn dawn;
    ion::str title;
    GLFWwindow* handle;
    wgpu::AdapterType adapter_type = wgpu::AdapterType::Unknown;
    ion::lambda<void(Key, num, uint32_t)> key;
    ion::lambda<void(vec2f, uint32_t)> mouse_move;
    ion::lambda<void(vec2f, uint32_t)> mouse_up;
    ion::lambda<void()> loop;
    wgpu::Device device;
    wgpu::SwapChain swap;
    vec2i sz;
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

Window Window::create(str title, vec2i sz) {
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

Window::operator bool() {
    return bool(data->device);
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

    std::vector<const char*> enableToggleNames = {
        "allow_unsafe_apis",  // Needed for dual-source blending, BufferMapExtendedUsages.
        "use_user_defined_labels_in_backend"
    };
    std::vector<const char*> disabledToggleNames;
    WGPUDawnTogglesDescriptor toggles;
    toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
    toggles.chain.next = nullptr;
    toggles.enabledToggles = enableToggleNames.data();
    toggles.enabledToggleCount = enableToggleNames.size();
    toggles.disabledToggles = disabledToggleNames.data();
    toggles.disabledToggleCount = disabledToggleNames.size();

    WGPUDeviceDescriptor deviceDesc = {};

    std::vector<wgpu::FeatureName> requiredFeatures;
    requiredFeatures.push_back(wgpu::FeatureName::SurfaceCapabilities);
    //if (adapter.HasFeature(wgpu::FeatureName::BufferMapExtendedUsages)) {
    //    requiredFeatures.push_back(wgpu::FeatureName::BufferMapExtendedUsages);
    //}

    deviceDesc.requiredFeatures = (WGPUFeatureName*)requiredFeatures.data();
    deviceDesc.requiredFeatureCount = requiredFeatures.size();



    deviceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&toggles);

    WGPUDevice backendDevice = preferredAdapter->CreateDevice(&deviceDesc);
    DawnProcTable backendProcs = dawn::native::GetProcs();

    // Create the swapchain
    static std::unique_ptr<wgpu::ChainedStruct> surfaceChainedDesc;
    surfaceChainedDesc = wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(handle); // this memory is not referenced properly by skia

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

ion::vec2i Window::size() {
    return data->sz;
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

float Dawn::get_dpi() {
    glfwInit();
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    float xscale, yscale;
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    return xscale;
}

}

static wgpu::Device            device;
static wgpu::Buffer            indexBuffer;
static wgpu::Buffer            vertexBuffer;
static wgpu::Texture           texture;
static wgpu::Sampler           sampler;
static wgpu::Queue             queue;
static wgpu::SwapChain         swapchain;
static wgpu::TextureView       depthStencilView;
static wgpu::RenderPipeline    pipeline;
static wgpu::BindGroup         bindGroup;
static ion::Window             window;

std::unique_ptr<skgpu::graphite::Context>  fGraphiteContext;
std::unique_ptr<skgpu::graphite::Recorder> fGraphiteRecorder;
skwindow::DisplayParams params = { };

static constexpr uint32_t kWidth = 1024;
static constexpr uint32_t kHeight = 1024;

wgpu::TextureFormat GetPreferredSwapChainTextureFormat() {
    return wgpu::TextureFormat::BGRA8Unorm;
}

struct Attribs {
    glm::vec4 pos;
    glm::vec2 uv;
};

void init() {
    window    = ion::Window::create("dawn", {kWidth, kHeight});
    device    = window.device();
    queue     = device.GetQueue();
    swapchain = window.swap_chain();

    static const uint32_t indexData[6] = {
        0, 1, 2,
        2, 1, 3
    };
    indexBuffer = dawn::utils::CreateBufferFromData(
        device, indexData, sizeof(indexData),
        wgpu::BufferUsage::Index);

    static const Attribs vertexData[4] = {
        {{ -1.0f, -1.0f, 0.0f, 1.0f }, {  0.0f, 0.0f }},
        {{  1.0f, -1.0f, 0.0f, 1.0f }, {  1.0f, 0.0f }},
        {{ -1.0f,  1.0f, 0.0f, 1.0f }, { -1.0f, 1.0f }},
        {{  1.0f,  1.0f, 0.0f, 1.0f }, {  1.0f, 1.0f }}
    };
    vertexBuffer = dawn::utils::CreateBufferFromData(
        device, vertexData, sizeof(vertexData),
        wgpu::BufferUsage::Vertex);

    wgpu::TextureDescriptor tx_desc;
    tx_desc.dimension = wgpu::TextureDimension::e2D;
    tx_desc.size.width = kWidth;
    tx_desc.size.height = kHeight;
    tx_desc.size.depthOrArrayLayers = 1;
    tx_desc.sampleCount = 1;
    tx_desc.format = wgpu::TextureFormat::BGRA8Unorm;
    tx_desc.mipLevelCount = 1;
    tx_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;

    texture = device.CreateTexture(&tx_desc);
    sampler = device.CreateSampler();

    wgpu::ShaderModule mod = dawn::utils::CreateShaderModule(device, R"(
        struct args {
            @builtin(position) pos : vec4f,
            @location(0)       uv  : vec2f
        };
        
        @vertex
        fn vertex_main(
                @location(0) pos1 : vec4f, 
                @location(1) uv  : vec2f) -> args {
            var a : args;
            a.pos = pos1;
            a.uv  = uv;
            return a;
        }

        @group(0) @binding(0) var mySampler : sampler;
        @group(0) @binding(1) var myTexture : texture_2d<f32>;

        @fragment
        fn fragment_main(a : args) -> @location(0) vec4f {
            return textureSample(myTexture, mySampler, a.uv);
        })");

    auto bgl = dawn::utils::MakeBindGroupLayout(
        device, {
            {0, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering},
            {1, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Float},
        });

    wgpu::PipelineLayout pl = dawn::utils::MakeBasicPipelineLayout(device, &bgl);

    depthStencilView = window.depth_stencil_view();

    dawn::utils::ComboRenderPipelineDescriptor descriptor;
    descriptor.layout = dawn::utils::MakeBasicPipelineLayout(device, &bgl);
    descriptor.vertex.module = mod;
    descriptor.vertex.bufferCount = 1;
    descriptor.cBuffers[0].arrayStride = sizeof(Attribs);


    /// attributes here (use simple meta iteration for this)

    descriptor.cBuffers[0].attributeCount = 2;
    descriptor.cAttributes = std::array<wgpu::VertexAttribute, dawn::kMaxVertexAttributes> {
        // Position attribute
        wgpu::VertexAttribute {
            wgpu::VertexFormat::Float32x4, offsetof(Attribs, pos), 0
        },
        // UV attribute
        wgpu::VertexAttribute {
            wgpu::VertexFormat::Float32x2, offsetof(Attribs, uv), 1
        }
    };
    descriptor.cFragment.module = mod;
    descriptor.cTargets[0].format = GetPreferredSwapChainTextureFormat();
    descriptor.EnableDepthStencil(wgpu::TextureFormat::Depth24PlusStencil8);

    pipeline = device.CreateRenderPipeline(&descriptor);
    wgpu::TextureView view = texture.CreateView();
    bindGroup = dawn::utils::MakeBindGroup(device, bgl, {{0, sampler}, {1, view}});

    skgpu::graphite::DawnBackendContext backendContext;
    backendContext.fDevice = device;
    backendContext.fQueue = device.GetQueue();

    params.fColorType = kRGBA_8888_SkColorType;
    params.fColorSpace = SkColorSpace::MakeSRGB();
    params.fMSAASampleCount = 0;
    params.fGrContextOptions = GrContextOptions(); // Set any desired context options
    params.fSurfaceProps = SkSurfaceProps(0, kBGR_H_SkPixelGeometry); // Set any desired surface properties
    params.fDisableVsync = false;
    params.fDelayDrawableAcquisition = false;
    params.fEnableBinaryArchive = false;
    params.fCreateProtectedNativeBackend = false;

    // Needed to make synchronous readPixels work:
    params.fGraphiteContextOptions.fPriv.fStoreContextRefInRecorder = true;

    fGraphiteContext = skgpu::graphite::ContextFactory::MakeDawn(
            backendContext, params.fGraphiteContextOptions.fOptions);
    fGraphiteRecorder = fGraphiteContext->makeRecorder(ToolUtils::CreateTestingRecorderOptions());
}

sk_sp<SkSurface> skia_surface() {
    //auto texture = swapchain.GetCurrentTexture();
    skgpu::graphite::BackendTexture backend_texture(texture.Get());
    assert(fGraphiteRecorder);

    auto surface = SkSurfaces::WrapBackendTexture(
        fGraphiteRecorder.get(),
        backend_texture,
        kBGRA_8888_SkColorType,
        params.fColorSpace,
        &params.fSurfaceProps);
    assert(surface);

    return surface;
}

/// general app-frame will manage the surface resources, 
void frame() {
    
    sk_sp<SkSurface> sk_surface = skia_surface();
    SkCanvas*        sk_canvas  = sk_surface->getCanvas();
    SkPaint          sk_paint;
    sk_paint.setColor(SK_ColorBLUE);
    sk_canvas->drawRect({0.0f, 0.0f, float(1024), float(1024)}, sk_paint);

    sk_paint.setColor(SK_ColorYELLOW);
    sk_canvas->drawRect({0.0f, 512.0f, float(1024), float(512)}, sk_paint);
    
    sk_paint.setColor(SK_ColorYELLOW);
    sk_canvas->drawRect({0.0f, 0.0f, float(1024), float(512)}, sk_paint);

    /// this is what i wasnt doing:
    std::unique_ptr<skgpu::graphite::Recording> recording = fGraphiteRecorder->snap();
    if (recording) {
        skgpu::graphite::InsertRecordingInfo info;
        info.fRecording = recording.get();
        fGraphiteContext->insertRecording(info);
        fGraphiteContext->submit(skgpu::graphite::SyncToCpu::kNo);
    }

    
    wgpu::TextureView backbufferView = swapchain.GetCurrentTextureView();
    dawn::utils::ComboRenderPassDescriptor renderPass({backbufferView}, depthStencilView);
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.DrawIndexed(6);
        pass.End();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
    swapchain.Present();
    glfwPollEvents();
}

int main(int argc, const char* argv[]) {
    init();

    i64 s_last = millis() / 1000;
    i64 frames_drawn = 0;

    while (true) {
        window.process_events();
        frame();
        frames_drawn++;
        dawn::utils::USleep(1);
        i64 s_cur = millis() / 1000;
        if (s_cur != s_last) {
            console.log("frames_drawn: {0}", { frames_drawn });
            s_last = s_cur;
            frames_drawn = 0;
        }
    }
}
