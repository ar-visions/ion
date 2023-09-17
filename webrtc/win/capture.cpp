/// include spec for Capture class
#include <webrtc/win/capture.h>

#include <vk/vk.hpp>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

//#include <webrtc/apple/apple_capture.h>
//#define GLFW_EXPOSE_NATIVE_COCOA

#include <async/async.hpp>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <Inspectable.h>
#include <winrt/base.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.ui.xaml.media.dxinterop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <nvenc/nvEncodeAPI.h>
#include <webrtc/nvenc/NvCodecUtils.h>
#include <webrtc/nvenc/NvEncoderD3D11.h>
#include <webrtc/nvenc/NvEncoderCLIOptions.h>

using GraphicsCaptureItem           = winrt::Windows::Graphics::Capture::GraphicsCaptureItem;
using GraphicsCaptureSession        = winrt::Windows::Graphics::Capture::GraphicsCaptureSession;
using IDirect3DDxgiInterfaceAccess  = Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess;
using DirectXPixelFormat            = winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
using IDirect3DDevice               = winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;
template <typename T>
using com_ptr                       = winrt::com_ptr<T>;
using Direct3D11CaptureFrame        = winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame;
using Direct3D11CaptureFramePool    = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool;
using SizeInt32                     = winrt::Windows::Graphics::SizeInt32;
using IGraphicsCaptureItem          = ABI::Windows::Graphics::Capture::IGraphicsCaptureItem;
using DispatcherQueueController     = winrt::Windows::System::DispatcherQueueController;

using namespace std;
using namespace ion;

extern "C"
{
    HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(::IDXGIDevice* dxgiDevice,
        ::IInspectable** graphicsDevice);

    HRESULT __stdcall CreateDirect3D11SurfaceFromDXGISurface(::IDXGISurface* dgxiSurface,
        ::IInspectable** graphicsSurface);
}

template <typename T>
auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
    auto access = object.as<IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<T> result;
    winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
    return result;
}

namespace ion {
struct iCapture {
    protected:
    struct NVenc {
        void* enc;
        NV_ENC_REGISTER_RESOURCE       res       { NV_ENC_REGISTER_RESOURCE_VER };
        NV_ENC_MAP_INPUT_RESOURCE      input     { NV_ENC_MAP_INPUT_RESOURCE_VER };
        NV_ENCODE_API_FUNCTION_LIST    fn        { NV_ENCODE_API_FUNCTION_LIST_VER };
        NV_ENC_CREATE_BITSTREAM_BUFFER bitstream { NV_ENC_CREATE_BITSTREAM_BUFFER_VER };
    } nv;

    public:

    inline static DispatcherQueueController dispatcher { null };
    inline static com_ptr<  ID3D11Device>   d3d_device;
    inline static com_ptr<::IInspectable>   d3d_device_i;

    bool async_start = false;
    std::mutex mtx;
    
    GraphicsCaptureItem                 m_item          { null };
    IDirect3DDevice                     m_device        { null };
    com_ptr<ID3D11DeviceContext>        m_d3dContext    { null };
    Direct3D11CaptureFramePool          m_framePool     { null };
    GraphicsCaptureSession              m_session       { null };
    SizeInt32                           m_lastSize;
    com_ptr<IDXGISwapChain1>            m_swapChain     { null };
    DirectXPixelFormat                  m_pixelFormat;
    uint32_t                            width = 0, height = 0;
    Capture::OnData                     m_output;
    atomic<optional<DirectXPixelFormat>> m_pixelFormatUpdate = std::nullopt;
    atomic<bool>                        m_closed           = false;
    atomic<bool>                        m_captureNextImage = false;
    ID3D11DeviceContext*                d3d11_context;
    com_ptr<ID3D11Texture2D>            m_TexSysMem;
    NvEncoderD3D11 *                    nvenc;

    type_register(iCapture);

    void OnFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const&) {
        u64      frame_time = microseconds();
        Direct3D11CaptureFrame frame = sender.TryGetNextFrame();
        auto surfaceTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        array<mx>   packets = Encode(surfaceTexture.get());
        SizeInt32        sz = frame.ContentSize();
        for (mx &bytes: packets) {
            if (!m_output(frame_time, bytes))
                Close();
        }

        if (sz != m_lastSize) {
            m_lastSize = sz; 
            m_framePool.Recreate(m_device, m_pixelFormat, 2, m_lastSize);
        }
    }

    void init(HWND hwnd, Capture::OnData on_encoded) {
        static bool init = false;
        static std::mutex mtx;

        /// lets protect during init
        mtx.lock();
        if (!init) {
            init = true;
            winrt::init_apartment(winrt::apartment_type::single_threaded);
            dispatcher = DispatcherQueueController::CreateOnDedicatedThread();

            if (!GraphicsCaptureSession::IsSupported())
                errorf("Screen capture is not supported on this device for this release of Windows!\n");
            
            winrt::check_hresult(
                D3D11CreateDevice(
                    null, D3D_DRIVER_TYPE_HARDWARE,   null,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT,    null, 0,
                    D3D11_SDK_VERSION, d3d_device.put(), null, null));
            
            auto dxgi_device = d3d_device.as<IDXGIDevice>();
            winrt::check_hresult(
                CreateDirect3D11DeviceFromDXGIDevice(
                    dxgi_device.get(), d3d_device_i.put()));
        }
        mtx.unlock();

        /// get context info
        ID3D11Device *d3d11 = d3d_device.get();
        m_device = d3d_device_i.as<IDirect3DDevice>();
        auto d3dDevice     = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
        d3dDevice->GetImmediateContext(m_d3dContext.put());
        d3d11_context      = m_d3dContext.get();

        /// object has a falsey state if a window is not found

        
        /// get window item
        auto interop_factory = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        interop_factory->CreateForWindow(
            hwnd, winrt::guid_of<IGraphicsCaptureItem>(), winrt::put_abi(m_item));
        if (!m_item) /// not a fault
            return;
        
        /// instance nvenc
        assert(NV_ENC_SUCCESS == NvEncodeAPICreateInstance(&nv.fn));
        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS startup { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
        startup.device     = (void*)d3d11;
        startup.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
        startup.apiVersion = NVENCAPI_VERSION;

        /// open encode session in nvenc
        assert(NV_ENC_SUCCESS == nv.fn.nvEncOpenEncodeSessionEx(&startup, &nv.enc));

        /// set data members
        m_pixelFormat      = DirectXPixelFormat::B8G8R8A8UIntNormalized;
        m_lastSize         = m_item.Size();
        width              = m_lastSize.Width;
        height             = m_lastSize.Height;
        m_output           = on_encoded;
        m_framePool        = Direct3D11CaptureFramePool::Create(m_device, m_pixelFormat, 2, m_item.Size());
        m_session          = m_framePool.CreateCaptureSession(m_item);
        
        /// set frame processor
        m_framePool.FrameArrived({ this, &iCapture::OnFrameArrived });

        /// allocate nvenc and set initial config
        nvenc = new NvEncoderD3D11(
            d3d11, width, height, NV_ENC_BUFFER_FORMAT_ARGB, 0, false, false);
        
        /// supplement configuration
        NV_ENC_INITIALIZE_PARAMS initializeParams { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG            encodeConfig     { NV_ENC_CONFIG_VER };
        initializeParams.encodeConfig = &encodeConfig;
        nvenc->CreateDefaultEncoderParams(
            &initializeParams,
             NV_ENC_CODEC_H264_GUID,
             NV_ENC_PRESET_P3_GUID,
             NV_ENC_TUNING_INFO_HIGH_QUALITY);

        NvEncoderInitParam cli;
        cli.SetInitParams(&initializeParams, NV_ENC_BUFFER_FORMAT_ARGB);

        /// create nvencoder
        nvenc->CreateEncoder(&initializeParams);

        /// start capturing window item
        m_session.StartCapture();
    }

    /// control for how many frames it encodes
    array<mx> Encode(ID3D11Texture2D *texture)
    {
        array<mx> res;
        std::vector<std::vector<uint8_t>> vPacket;
        const NvEncInputFrame* input_frame = nvenc->GetNextInputFrame();
        ID3D11Texture2D *pTexInput = reinterpret_cast<ID3D11Texture2D*>(input_frame->inputPtr);
        d3d11_context->CopyResource(pTexInput, texture);
        nvenc->EncodeFrame(vPacket);
        if (vPacket.size()) {
            u8    *src = vPacket[0].data();
            size_t sz  = vPacket[0].size();
            int b0 = src[0];
            int b1 = src[1];
            int b2 = src[2];
            int b3 = src[3];
            
            /// must be NALU header with this type of prefix
            assert(b0 == 0 && b1 == 0 && b2 == 0 && b3 == 1);

            int header = 0;
            int start  = 0;
            for (int i = 0; i < sz; i++) {
                int b0 = src[i+0];
                int b1 = src[i+1];
                int b2 = src[i+2];
                int b3 = src[i+3];
                
            /// must be NALU header with this type of prefix
                if (b0 == 0 && b1 == 0 && b2 == 0 && b3 == 1) {
                    u_long len = i - start + 4;
                    header++;

                    uint8_t *p = (uint8_t*)calloc64(1, len + 4);
                    memcpy(p, &src[start], len + 4);
                    *(uint32_t*)p = htonl(len);
                    res += mx { p, size_t(len + 4) };

                    start = i;
                }
            }

            printf("n headers: %d\n", header);

            uint32_t len = ntohl(*(uint32_t*)&src[4]);

            assert(vPacket.size() == 1);
            return res;
        }
        return res;
    }

    void Close()
    {
        nvenc->DestroyEncoder();
        delete nvenc;

        m_session.Close();
        m_framePool.Close();
        m_framePool = null;
        m_session   = null;
        m_item      = null;
    }
};

mx_implement(Capture, mx);

/// async makes case for context-data
Capture::Capture(GLFWwindow *glfw, OnData encoded, OnClosed closed):Capture() {
    async { 1, [glfw, encoded, closed, data=data](runtime *rt, int idx) -> mx {
        data->mtx.lock();
        data->async_start = true;
        HWND hwnd = glfwGetWin32Window((GLFWwindow*)glfw);
        data->init(hwnd, encoded);
        data->mtx.unlock();
        ///
        MSG     msg { 0 };
        while (GetMessage(&msg, null, 0, 0)) {
            if (!data->nvenc)
                break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        closed(data);
        return int(0);
    }};
}

void Capture::stop() {
    while (!data->async_start) {
        usleep(100);
    }
    data->Close();
}
}