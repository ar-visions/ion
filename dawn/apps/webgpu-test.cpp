
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
#include "dawn/samples/SampleUtils.h"

#include "GLFW/glfw3.h"
#include "dawn/common/Assert.h"
#include "dawn/common/Log.h"
#include "dawn/common/Platform.h"
#include "dawn/common/SystemUtils.h"
#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"
#include "webgpu/webgpu_glfw.h"


#include "include/gpu/graphite/Context.h"
#define SK_DAWN
#include "include/gpu/graphite/Surface.h"
#include "src/gpu/graphite/Surface_Graphite.h"
#include "include/gpu/graphite/TextureInfo.h"
#include "include/gpu/graphite/BackendTexture.h"
#include "include/gpu/graphite/ContextOptions.h"
#include "include/gpu/graphite/dawn/DawnTypes.h"
#include "include/gpu/graphite/dawn/DawnUtils.h"
#include "include/private/base/SkOnce.h"
#include "include/private/gpu/graphite/ContextOptionsPriv.h"
#include "tools/gpu/ContextType.h"
#include "tools/graphite/TestOptions.h"

#include "dawn/dawn_proc.h"




#include "include/gpu/dawn/GrDawnTypes.h"

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#define SK_DAWN
#include "include/gpu/GrBackendSurface.h"
#include "include/core/SkCanvas.h"
#include "include/gpu/graphite/Context.h"
#include "include/gpu/graphite/Recorder.h"
#include "include/gpu/graphite/TextureInfo.h"
#include "include/gpu/graphite/dawn/DawnBackendContext.h"
#include "src/gpu/graphite/ContextUtils.h"


wgpu::Device            device;
wgpu::Buffer            indexBuffer;
wgpu::Buffer            vertexBuffer;
wgpu::Texture           texture;
wgpu::Sampler           sampler;
wgpu::Queue             queue;
wgpu::SwapChain         swapchain;
wgpu::TextureView       depthStencilView;
wgpu::RenderPipeline    pipeline;
wgpu::BindGroup         bindGroup;

sk_sp<SkSurface>                           sk_surface;
std::unique_ptr<skgpu::graphite::Recorder> sk_recorder;
std::unique_ptr<skgpu::graphite::Context>  sk_dawn;
sk_sp<SkColorSpace>                        color_space;
skgpu::graphite::BackendTexture *          sk_backend_tx;

void PrintDeviceError(WGPUErrorType errorType, const char* message, void*) {
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

void DeviceLostCallback(WGPUDeviceLostReason reason, const char* message, void*) {
    dawn::ErrorLog() << "Device lost: " << message;
}

void PrintGLFWError(int code, const char* message) {
    dawn::ErrorLog() << "GLFW error: " << code << " - " << message;
}

void DeviceLogCallback(WGPULoggingType type, const char* message, void*) {
    dawn::ErrorLog() << "Device log: " << message;
}

#define DAWN_ENABLE_BACKEND_METAL

// Default to D3D12, Metal, Vulkan, OpenGL in that order as D3D12 and Metal are the preferred on
// their respective platforms, and Vulkan is preferred to OpenGL
#if defined(DAWN_ENABLE_BACKEND_D3D12)
static wgpu::BackendType backendType = wgpu::BackendType::D3D12;
#elif defined(DAWN_ENABLE_BACKEND_D3D11)
static wgpu::BackendType backendType = wgpu::BackendType::D3D11;
#elif defined(DAWN_ENABLE_BACKEND_METAL)
static wgpu::BackendType backendType = wgpu::BackendType::Metal;
#elif defined(DAWN_ENABLE_BACKEND_VULKAN)
static wgpu::BackendType backendType = wgpu::BackendType::Vulkan;
#elif defined(DAWN_ENABLE_BACKEND_OPENGLES)
static wgpu::BackendType backendType = wgpu::BackendType::OpenGLES;
#elif defined(DAWN_ENABLE_BACKEND_DESKTOP_GL)
static wgpu::BackendType backendType = wgpu::BackendType::OpenGL;
#else
#error
#endif

static wgpu::AdapterType adapterType = wgpu::AdapterType::Unknown;
static std::unique_ptr<dawn::native::Instance> instance;
static wgpu::SwapChain swapChain;
static GLFWwindow* glfw_window = nullptr;
static ion::Window window;
static constexpr uint32_t kWidth = 1024;
static constexpr uint32_t kHeight = 1024;

wgpu::Device CreateCppDawnDevice() {
    dawn::ScopedEnvironmentVar angleDefaultPlatform;
    if (dawn::GetEnvironmentVar("ANGLE_DEFAULT_PLATFORM").first.empty()) {
        angleDefaultPlatform.Set("ANGLE_DEFAULT_PLATFORM", "swiftshader");
    }

    glfwSetErrorCallback(PrintGLFWError);
    if (!glfwInit()) {
        return wgpu::Device();
    }

    // Create the test window with no client API.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
    glfw_window = glfwCreateWindow(kWidth, kHeight, "Dawn window", nullptr, nullptr);
    if (!glfw_window) {
        return wgpu::Device();
    }

    WGPUInstanceDescriptor instanceDescriptor{};
    instanceDescriptor.features.timedWaitAnyEnable = true;
    instance = std::make_unique<dawn::native::Instance>(&instanceDescriptor);

    wgpu::RequestAdapterOptions options = {};
    options.backendType = backendType;

    // Get an adapter for the backend to use, and create the device.
    auto adapters = instance->EnumerateAdapters(&options);
    wgpu::DawnAdapterPropertiesPowerPreference power_props{};
    wgpu::AdapterProperties adapterProperties{};
    adapterProperties.nextInChain = &power_props;
    // Find the first adapter which satisfies the adapterType requirement.
    auto isAdapterType = [&adapterProperties](const auto& adapter) -> bool {
        // picks the first adapter when adapterType is unknown.
        if (adapterType == wgpu::AdapterType::Unknown) {
            return true;
        }
        adapter.GetProperties(&adapterProperties);
        return adapterProperties.adapterType == adapterType;
    };
    auto preferredAdapter = std::find_if(adapters.begin(), adapters.end(), isAdapterType);
    if (preferredAdapter == adapters.end()) {
        fprintf(stderr, "Failed to find an adapter! Please try another adapter type.\n");
        return wgpu::Device();
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
    auto surfaceChainedDesc = wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(glfw_window);
    WGPUSurfaceDescriptor surfaceDesc;
    surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(surfaceChainedDesc.get());
    WGPUSurface surface = backendProcs.instanceCreateSurface(instance->Get(), &surfaceDesc);

    WGPUSwapChainDescriptor swapChainDesc = {};
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.format = static_cast<WGPUTextureFormat>(GetPreferredSwapChainTextureFormat());
    swapChainDesc.width = kWidth;
    swapChainDesc.height = kHeight;
    swapChainDesc.presentMode = WGPUPresentMode_Mailbox;
    WGPUSwapChain backendSwapChain =
        backendProcs.deviceCreateSwapChain(backendDevice, surface, &swapChainDesc);

    // Choose whether to use the backend procs and devices/swapchains directly, or set up the wire.
    WGPUDevice cDevice = nullptr;
    DawnProcTable procs;

    procs = backendProcs;
    cDevice = backendDevice;
    swapChain = wgpu::SwapChain::Acquire(backendSwapChain);

    dawnProcSetProcs(&procs);
    procs.deviceSetUncapturedErrorCallback(cDevice, PrintDeviceError, nullptr);
    procs.deviceSetDeviceLostCallback(cDevice, DeviceLostCallback, nullptr);
    procs.deviceSetLoggingCallback(cDevice, DeviceLogCallback, nullptr);
    return wgpu::Device::Acquire(cDevice);
}

wgpu::TextureFormat GetPreferredSwapChainTextureFormat() {
    // TODO(dawn:1362): Return the adapter's preferred format when implemented.
    return wgpu::TextureFormat::BGRA8Unorm;
}

wgpu::SwapChain GetSwapChain() {
    return swapChain;
}

bool ShouldQuit() {
    return glfwWindowShouldClose(glfw_window);
}

GLFWwindow* GetGLFWWindow() {
    return glfw_window;
}

void ProcessEvents() {
    dawn::native::InstanceProcessEvents(instance->Get());
}

void initBuffers() {
    static const uint32_t indexData[3] = {
        0,
        1,
        2,
    };
    indexBuffer = dawn::utils::CreateBufferFromData(device, indexData, sizeof(indexData),
                                                    wgpu::BufferUsage::Index);

    static const float vertexData[12] = {
        0.0f, 0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, 0.0f, 1.0f,
    };
    vertexBuffer = dawn::utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                     wgpu::BufferUsage::Vertex);
}

void update_tx();

void initTextures() {
    wgpu::TextureDescriptor descriptor;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size.width = kWidth;
    descriptor.size.height = kHeight;
    descriptor.size.depthOrArrayLayers = 1;
    descriptor.sampleCount = 1;
    descriptor.format = wgpu::TextureFormat::BGRA8Unorm;
    descriptor.mipLevelCount = 1;
    descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;

    texture = device.CreateTexture(&descriptor);
    sampler = device.CreateSampler();

    /// this works; the data is random
    update_tx();
}

void init() {
    window = ion::Window::create("dawn", {kWidth, kHeight});
    device = window.device();//CreateCppDawnDevice();

    queue = device.GetQueue();
    swapchain = window.swap_chain();//GetSwapChain();

    initBuffers();
    initTextures();

    wgpu::ShaderModule vsModule = dawn::utils::CreateShaderModule(device, R"(
        @vertex fn main(@location(0) pos : vec4f)
                            -> @builtin(position) vec4f {
            return pos;
        })");

    wgpu::ShaderModule fsModule = dawn::utils::CreateShaderModule(device, R"(
        @group(0) @binding(0) var mySampler: sampler;
        @group(0) @binding(1) var myTexture : texture_2d<f32>;

        @fragment fn main(@builtin(position) FragCoord : vec4f)
                              -> @location(0) vec4f {
            return textureSample(myTexture, mySampler, FragCoord.xy / vec2f(640.0, 480.0));
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
    descriptor.vertex.module = vsModule;
    descriptor.vertex.bufferCount = 1;
    descriptor.cBuffers[0].arrayStride = 4 * sizeof(float);
    descriptor.cBuffers[0].attributeCount = 1;
    descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
    descriptor.cFragment.module = fsModule;
    descriptor.cTargets[0].format = GetPreferredSwapChainTextureFormat();
    descriptor.EnableDepthStencil(wgpu::TextureFormat::Depth24PlusStencil8);

    pipeline = device.CreateRenderPipeline(&descriptor);

    wgpu::TextureView view = texture.CreateView();

    bindGroup = dawn::utils::MakeBindGroup(device, bgl, {{0, sampler}, {1, view}});
}

void update_tx() {

    // Initialize the texture with arbitrary data until we can load images
    std::vector<uint8_t> data(4 * kWidth * kHeight, 0);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(rand() & 127);
    }

    wgpu::Buffer stagingBuffer = dawn::utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
    wgpu::ImageCopyBuffer imageCopyBuffer =
        dawn::utils::CreateImageCopyBuffer(stagingBuffer, 0, 4 * kWidth);
    wgpu::ImageCopyTexture imageCopyTexture =
        dawn::utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {kWidth, kHeight, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);

    // Finish and submit command
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
}

void frame() {
    // this does not update the texture, even though the texture is skia's backend render target
    // it is not updated from the test
    SkCanvas* sk_canvas = sk_surface->getCanvas();
    SkPaint   paint;
    paint.setColor(SK_ColorBLUE);
    sk_canvas->drawRect({0.0f, 0.0f, float(kWidth), float(kHeight)}, paint);
    skgpu::graphite::Flush(sk_surface);
    sk_dawn->submit();

    //update_tx();

    wgpu::TextureView backbufferView = swapchain.GetCurrentTextureView();
    dawn::utils::ComboRenderPassDescriptor renderPass({backbufferView}, depthStencilView);
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.DrawIndexed(3);
        pass.End();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
    swapchain.Present();
    glfwPollEvents();
}


void init_skia() {
    skgpu::graphite::DawnBackendContext backendContext = { device, queue };
    skgpu::graphite::ContextOptions opts;
    sk_dawn =
        skgpu::graphite::ContextFactory::MakeDawn(backendContext, opts);
    
    skgpu::graphite::RecorderOptions recorder_opts;
    sk_recorder     = sk_dawn->makeRecorder(recorder_opts);
    sk_backend_tx   = new skgpu::graphite::BackendTexture(texture.Get());
    color_space     = SkColorSpace::MakeSRGB();
    sk_surface      = SkSurfaces::WrapBackendTexture(
        sk_recorder.get(),
        *sk_backend_tx,
        kBGRA_8888_SkColorType,
        color_space,
        nullptr);

}

int main(int argc, const char* argv[]) {
    init();
    init_skia();
    while (true) {
        window.process_events();
        frame();
        dawn::utils::USleep(16000);
    }
}
