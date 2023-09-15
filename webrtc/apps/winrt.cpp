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

static DispatcherQueueController dispatcher { nullptr };

#define errorf(...) do { fprintf(stderr, __VA_ARGS__); exit(1); } while (0);

extern "C"
{
    HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(::IDXGIDevice* dxgiDevice,
        ::IInspectable** graphicsDevice);

    HRESULT __stdcall CreateDirect3D11SurfaceFromDXGISurface(::IDXGISurface* dgxiSurface,
        ::IInspectable** graphicsSurface);
}

auto CreateDXGISwapChain(winrt::com_ptr<ID3D11Device> const& device, const DXGI_SWAP_CHAIN_DESC1* desc)
{
    auto dxgiDevice = device.as<IDXGIDevice2>();
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(dxgiDevice->GetParent(winrt::guid_of<IDXGIAdapter>(), adapter.put_void()));
    winrt::com_ptr<IDXGIFactory2> factory;
    winrt::check_hresult(adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));

    winrt::com_ptr<IDXGISwapChain1> swapchain;
    winrt::check_hresult(factory->CreateSwapChainForComposition(device.get(), desc, nullptr, swapchain.put()));
    return swapchain;
}

auto CreateDXGISwapChain(com_ptr<ID3D11Device> const& device,
    uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t bufferCount)
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferCount = bufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    return CreateDXGISwapChain(device, &desc);
}

template <typename T>
auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
    auto access = object.as<IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<T> result;
    winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
    return result;
}


struct Capture {
    protected:
    struct NVenc {
        void* enc;
        NV_ENC_REGISTER_RESOURCE       res       { NV_ENC_REGISTER_RESOURCE_VER };
        NV_ENC_MAP_INPUT_RESOURCE      input     { NV_ENC_MAP_INPUT_RESOURCE_VER };
        NV_ENCODE_API_FUNCTION_LIST    fn        { NV_ENCODE_API_FUNCTION_LIST_VER };
        NV_ENC_CREATE_BITSTREAM_BUFFER bitstream { NV_ENC_CREATE_BITSTREAM_BUFFER_VER };
    } nv;

    public:
    GraphicsCaptureItem                 m_item          { nullptr };
    IDirect3DDevice                     m_device        { nullptr };
    com_ptr<ID3D11DeviceContext>        m_d3dContext    { nullptr };
    Direct3D11CaptureFramePool          m_framePool     { nullptr };
    GraphicsCaptureSession              m_session       { nullptr };
    SizeInt32                           m_lastSize;
    com_ptr<IDXGISwapChain1>            m_swapChain     { nullptr };
    DirectXPixelFormat                  m_pixelFormat;

    std::atomic<std::optional<DirectXPixelFormat>> m_pixelFormatUpdate = std::nullopt;
    std::atomic<bool> m_closed           = false;
    std::atomic<bool> m_captureNextImage = false;

    ID3D11Device* d3d11;
    ID3D11DeviceContext* d3d11_context;
    ComPtr<ID3D11Texture2D> pTexSysMem;

    /// we are dependent on nvenc/samples/d3d11 and its nvenc generic

    NvEncoderD3D11 *nvenc;

    void OnFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const&) {
        auto swapChainResizedToFrame = false;
        Direct3D11CaptureFrame frame = sender.TryGetNextFrame();
        swapChainResizedToFrame      = TryResizeSwapChain(frame);

        winrt::com_ptr<ID3D11Texture2D> backBuffer;
        winrt::check_hresult(m_swapChain->GetBuffer(0, winrt::guid_of<ID3D11Texture2D>(), backBuffer.put_void()));
        auto surfaceTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

        // copy surfaceTexture to backBuffer
        m_d3dContext->CopyResource(backBuffer.get(), surfaceTexture.get());
        ID3D11Texture2D* tx_back_buffer = backBuffer.get();
        

        /*
        DXGI_PRESENT_PARAMETERS presentParameters{};
        m_swapChain->Present1(1, 0, &presentParameters);

        swapChainResizedToFrame = swapChainResizedToFrame || TryUpdatePixelFormat();

        if (swapChainResizedToFrame)
        {
            m_framePool.Recreate(m_device, m_pixelFormat, 2, m_lastSize);
        }
        */
    }

    Capture(ID3D11Device* d3d11, ID3D11Device* d3d11_context, IDirect3DDevice const& device, GraphicsCaptureItem const& item, DirectXPixelFormat pixelFormat)
    {
        this->d3d11 = d3d11;
        this->d3d11_context = d3d11_context;

        assert(NV_ENC_SUCCESS == NvEncodeAPICreateInstance(&nv.fn));

        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS startup { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
        startup.device     = (void*)d3d11;
        startup.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
        startup.apiVersion = NVENCAPI_VERSION;

        assert(NV_ENC_SUCCESS == nv.fn.nvEncOpenEncodeSessionEx(&startup, &nv.enc));

        m_item = item;
        m_device = device;
        m_pixelFormat = pixelFormat;

        auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
        d3dDevice->GetImmediateContext(m_d3dContext.put());

        uint32_t width  = m_item.Size().Width;
        uint32_t height = m_item.Size().Height;
        m_swapChain = CreateDXGISwapChain(
            d3dDevice, width, height, DXGI_FORMAT(m_pixelFormat), 2);

        // Creating our frame pool with 'Create' instead of 'CreateFreeThreaded'
        // means that the frame pool's FrameArrived event is called on the thread
        // the frame pool was created on. This also means that the creating thread
        // must have a DispatcherQueue. If you use this method, it's best not to do
        // it on the UI thread. 
        m_framePool = Direct3D11CaptureFramePool::Create(m_device, m_pixelFormat, 2, m_item.Size());
        m_session   = m_framePool.CreateCaptureSession(m_item);
        m_lastSize  = m_item.Size();
        m_framePool.FrameArrived({ this, &Capture::OnFrameArrived });
        NvEncoderInitParam cli;
        nvenc = new NvEncoderD3D11(
            d3d11, item.Size().Width, item.Size().Height,
            NV_ENC_BUFFER_FORMAT_ARGB, 0, false, false);
        
        NV_ENC_INITIALIZE_PARAMS initializeParams { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG            encodeConfig     { NV_ENC_CONFIG_VER };
        initializeParams.encodeConfig = &encodeConfig;
        nvenc->CreateDefaultEncoderParams(
            &initializeParams,
             NV_ENC_CODEC_H264_GUID,
             NV_ENC_PRESET_P3_GUID,
             NV_ENC_TUNING_INFO_HIGH_QUALITY);

        cli.SetInitParams(&initializeParams, NV_ENC_BUFFER_FORMAT_ARGB);
        nvenc->CreateEncoder(&initializeParams);















     DXGI_ADAPTER_DESC adapterDesc;
        pAdapter->GetDesc(&adapterDesc);
        char szDesc[80];
        wcstombs(szDesc, adapterDesc.Description, sizeof(szDesc));
        std::cout << "GPU in use: " << szDesc << std::endl;

        D3D11_TEXTURE2D_DESC desc;
        memset(&desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ck(d3d11->CreateTexture2D(&desc, NULL, pTexSysMem.GetAddressOf()));
        
        std::unique_ptr<RGBToNV12ConverterD3D11> pConverter;

        Encode(d3d11, m_d3dContext.get(), pConverter.get(), width, height, encodeCLIOptions, bForceNv12,
                pTexSysMem.Get(), fpBgra, fpOut);













        m_session.StartCapture();
    }


    void Encode(ID3D11DeviceContext *pContext, int nWidth, int nHeight,
                NvEncoderInitParam encodeCLIOptions, bool bForceNv12, ID3D11Texture2D *pTexSysMem, 
                std::ifstream &fpBgra, std::ofstream &fpOut)
    {
        int nSize = nWidth * nHeight * 4;
        std::unique_ptr<uint8_t[]> pHostFrame(new uint8_t[nSize]);
        int nFrame = 0;
        NvEncoderD3D11 &enc = *nvenc;
        
        while (true) 
        {
            std::vector<std::vector<uint8_t>> vPacket;
            const NvEncInputFrame* encoderInputFrame = enc.GetNextInputFrame();

            ID3D11Texture2D *pTexBgra = reinterpret_cast<ID3D11Texture2D*>(encoderInputFrame->inputPtr);
            pContext->CopyResource(pTexBgra, pTexSysMem);

            std::streamsize nRead = ReadInputFrame(encoderInputFrame, fpBgra, reinterpret_cast<char*>(pHostFrame.get()), pContext, pTexSysMem,
                                                nSize, nHeight, nWidth, bForceNv12);

            if (nRead == nSize)
            {
                enc.EncodeFrame(vPacket);
            }
            else
            {
                enc.EndEncode(vPacket);
            }
            nFrame += (int)vPacket.size();
            for (std::vector<uint8_t> &packet : vPacket)
            {
                fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
            }
            if (nRead != nSize) {
                break;
            }
        }

        enc.DestroyEncoder();

        fpOut.close();
        fpBgra.close();

        std::cout << "Total frames encoded: " << nFrame << std::endl;
    }
    

    void Close()
    {
        auto expected = false;
        if (m_closed.compare_exchange_strong(expected, true))
        {
            m_session.Close();
            m_framePool.Close();

            m_swapChain = nullptr;
            m_framePool = nullptr;
            m_session   = nullptr;
            m_item      = nullptr;
        }
    }

    void ResizeSwapChain()
    {
        winrt::check_hresult(
            m_swapChain->ResizeBuffers(2,
                static_cast<uint32_t>(m_lastSize.Width),
                static_cast<uint32_t>(m_lastSize.Height),
                static_cast<DXGI_FORMAT>(m_pixelFormat), 0));
    }

    bool TryResizeSwapChain(Direct3D11CaptureFrame const& frame)
    {
        auto const contentSize   = frame.ContentSize();
        if ((contentSize.Width  != m_lastSize.Width) ||
            (contentSize.Height != m_lastSize.Height))
        {
            // The thing we have been capturing has changed size, resize the swap chain to match.
            m_lastSize = contentSize;
            ResizeSwapChain();
            return true;
        }
        return false;
    }

    bool TryUpdatePixelFormat()
    {
        auto newFormat = m_pixelFormatUpdate.exchange(std::nullopt);
        if (newFormat.has_value())
        {
            auto pixelFormat = newFormat.value();
            if (pixelFormat != m_pixelFormat)
            {
                m_pixelFormat = pixelFormat;
                ResizeSwapChain();
                return true;
            }
        }
        return false;
    }
};


int main(int argc, char *argv[]) {
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    dispatcher = DispatcherQueueController::CreateOnDedicatedThread();

    // Check to see that capture is supported
    auto isCaptureSupported = GraphicsCaptureSession::IsSupported();
    if (!isCaptureSupported) {
        errorf("Screen capture is not supported on this device for this release of Windows!\n");
    } else {
        printf("screen capture should work\n");
    }

    DirectXPixelFormat           format = DirectXPixelFormat::B8G8R8A8UIntNormalized;
    winrt::com_ptr<ID3D11Device> d3d_device;

    winrt::check_hresult(
        D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE,   nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,    nullptr, 0,
            D3D11_SDK_VERSION, d3d_device.put(), nullptr, nullptr));
    ID3D11Device *d3d11 = d3d_device.get();
    
    auto dxgi_device = d3d_device.as<IDXGIDevice>();

    com_ptr<::IInspectable> d3d_device_i;
    winrt::check_hresult(
        CreateDirect3D11DeviceFromDXGIDevice(
            dxgi_device.get(), d3d_device_i.put()));
    
    IDirect3DDevice m_device = d3d_device_i.as<IDirect3DDevice>();

    HWND hwnd = FindWindowA(NULL, "brianwd");

    auto interop_factory = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    GraphicsCaptureItem item = { nullptr };
    winrt::check_hresult(
        interop_factory->CreateForWindow(
            hwnd, winrt::guid_of<IGraphicsCaptureItem>(), winrt::put_abi(item)));

    Capture capture { d3d11, m_device, item, format };

    MSG msg = { 0 };
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    int test = 0;
    test++;
}