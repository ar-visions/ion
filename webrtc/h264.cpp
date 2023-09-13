#include <webrtc/webrtc.hpp>
#include <vk/vk.hpp>
#include <media/media.hpp>
#include <async/async.hpp>

#ifndef __APPLE__

    #include <nvenc/nvEncodeAPI.h>

#endif

#if defined(_WIN32_)

    #include <d3d11.h>
    #include <dxgi.h>
    #include <windows.h>
    #define GLFW_EXPOSE_NATIVE_WIN32

#elif defined(__APPLE__)

    #define GLFW_EXPOSE_NATIVE_COCOA

#elif defined(__linux__)
    #include <GL/freeglut.h>
    #include <GL/glew.h>
    #include <GL/gl.h>
    #include <GL/glx.h>
    #include <GL/glxext.h>
    #include <X11/Xlib.h>
    #include <X11/extensions/Xcomposite.h>

    #define GLFW_EXPOSE_NATIVE_X11
    #define GLFW_EXPOSE_NATIVE_GLX
    
    static Display* dpy;
    static int composite_event_base, composite_error_base;

    using PFNGlXBindTexImageEXTPROC     = void (*)(Display *, GLXDrawable, int, const int *);
    using PFNGlXReleaseTexImageEXTPROC  = void (*)(Display *, GLXDrawable, int);
        PFNGlXBindTexImageEXTPROC       glXBindTexImageEXT    = ion::null;
        PFNGlXReleaseTexImageEXTPROC    glXReleaseTexImageEXT = ion::null;
        
    static int
    Xwindows_error (Display *xdisplay, XErrorEvent *error) {
        int test = 0;
        test++;
        return 0;
    }
#endif

    #include <GLFW/glfw3native.h>

#ifdef USE_H264E

///
#define MINIH264_IMPLEMENTATION
#define H264E_ENABLE_PLAIN_C    1
#include <webrtc/minih264e.h>

    #if H264E_MAX_THREADS

#include "system.h"
typedef struct {
    void *event_start;
    void *event_done;
    void (*callback)(void*);
    void *job;
    void *thread;
    int terminated;
} h264e_thread_t;

static THREAD_RET THRAPI minih264_thread_func(void *arg)
{
    h264e_thread_t *t = (h264e_thread_t *)arg;
    thread_name("h264");
    for (;;)
    {
        event_wait(t->event_start, INFINITE);
        if (t->terminated)
            break;
        t->callback(t->job);
        event_set(t->event_done);
    }
    return 0;
}

void *h264e_thread_pool_init(int max_threads)
{
    int i;
    h264e_thread_t *threads = (h264e_thread_t *)calloc64(sizeof(h264e_thread_t), max_threads);
    if (!threads)
        return 0;
    for (i = 0; i < max_threads; i++)
    {
        h264e_thread_t *t = threads + i;
        t->event_start = event_create(0, 0);
        t->event_done  = event_create(0, 0);
        t->thread = thread_create(minih264_thread_func, t);
    }
    return threads;
}

void h264e_thread_pool_close(void *pool, int max_threads)
{
    int i;
    h264e_thread_t *threads = (h264e_thread_t *)pool;
    for (i = 0; i < max_threads; i++)
    {
        h264e_thread_t *t = threads + i;
        t->terminated = 1;
        event_set(t->event_start);
        thread_wait(t->thread);
        thread_close(t->thread);
        event_destroy(t->event_start);
        event_destroy(t->event_done);
    }
    free64(pool);
}

void h264e_thread_pool_run(void *pool, void (*callback)(void*), void *callback_job[], int njobs)
{
    h264e_thread_t *threads = (h264e_thread_t*)pool;
    int i;
    for (i = 0; i < njobs; i++)
    {
        h264e_thread_t *t = threads + i;
        t->callback = (void (*)(void *))callback;
        t->job = callback_job[i];
        event_set(t->event_start);
    }
    for (i = 0; i < njobs; i++)
    {
        h264e_thread_t *t = threads + i;
        event_wait(t->event_done, INFINITE);
    }
}
    #endif

#endif

namespace ion {

struct i264e {
public:
    bool            valid;     /// while waiting for first frame it should be valid
    size_t          frame_size, width, height;

#ifdef USE_H264E
protected:
    H264E_create_param_t    create_param;
    H264E_run_param_t       run_param;
    H264E_io_yuv_t          yuv;
    uint8_t                *buf_in, *buf_save;
    uint8_t                *coded_data;

    int gop;        /// key frame period
    int qp;         /// QP = 10..51
    int kbps;       /// 0 = default (constant qp)
    int fps;        /// 30 is default
    int threads;    /// 4 = default
    int speed;      /// 0 = best quality, 10 = fastest
    bool denoise;

    mutex  encode_mtx;
    yuv420 encode_input;

    /// called from outside thread; we must maintain sync with encoder
    void push(yuv420 &frame) {
        /// if it has not processed last frame yet, it must wait!
        encode_mtx.lock();
        encode_input = frame;
        if (!frame) {
            valid = false; /// we can only know validity on incoming frame
        }
        encode_mtx.unlock();
    }

    yuv420 fetch() {
        yuv420 result;
        for (;;) {
            encode_mtx.lock();
            if (!encode_input) {
                encode_mtx.unlock();
                usleep(1000);
            } else {
                result = encode_input;
                encode_input = {};
                encode_mtx.unlock();
                break;
            }
        }
        return result;
    }
    lambda<yuv420(i64)> input;

    async run() {
        return async {1, [this](runtime *rt, int index) -> mx {
            int   iframes = 0;
            yuv420 frame = fetch(); /// must be yuv420; this will not convert
            if (!frame) {
                printf("WARNING: first frame null; unknown dimensions and end of stream\n");
                return null;
            }
            valid               = true; /// valid frame received for first; this is an indicator of eof, eos, something.
            width               = frame.width();
            height              = frame.height();
            frame_size          = width*height*3/2; /// yuv 420

            /// this will be nice to use!
        #if H264E_SVC_API
            create_param.num_layers = 1;
            create_param.inter_layer_pred_flag = 0;
        #endif

        #if H264E_MAX_THREADS
            void *thread_pool = NULL;
            create_param.max_threads = threads;
            if (threads)
            {
                thread_pool = h264e_thread_pool_init(threads);
                create_param.token = thread_pool;
                create_param.run_func_in_thread = h264e_thread_pool_run;
            }
        #endif

            create_param.gop    = gop;
            create_param.max_long_term_reference_frames = 0;
            create_param.fine_rate_control_flag = 0;
            create_param.const_input_flag = 1;
            //create_param.vbv_overflow_empty_frame_flag = 1;
            //create_param.vbv_underflow_stuffing_flag = 1;
            create_param.vbv_size_bytes = kbps*1000/8*2; // 2 seconds vbv buffer for quality, so rate control can allocate more bits for intra frame
            create_param.temporal_denoise_flag = denoise;
            create_param.height = height;
            create_param.width  = width;
            create_param.enableNEON = 1;

            int sizeof_enc = 0, sizeof_scratch = 0, error;
            H264E_persist_t *enc = NULL;
            H264E_scratch_t *scratch = NULL;

            error = H264E_sizeof(&create_param, &sizeof_enc, &sizeof_scratch);
            if (error) {
                printf("H264E_init error = %d\n", error);
                return null;
            }

            printf("sizeof_enc = %d sizeof_scratch = %d\n", sizeof_enc, sizeof_scratch);

            enc     = (H264E_persist_t *)calloc64(sizeof_enc, 1);
            scratch = (H264E_scratch_t *)calloc64(sizeof_scratch, 1);
            error   = H264E_init(enc, &create_param);

            for (;valid;) {
                /// verify frame is correct count of bytes, yuv 420
                u8* buf_in = (u8*)frame.mem->origin;
                assert(frame.mem->count * frame.mem->type->base_sz == frame_size);

                yuv.yuv[0]    = buf_in; 
                yuv.yuv[1]    = buf_in + width*height; 
                yuv.yuv[2]    = buf_in + width*height*5/4; 
                yuv.stride[0] = width;
                yuv.stride[1] = width/2;
                yuv.stride[2] = width/2;
                
                run_param.frame_type = 0;
                run_param.encode_speed = speed;
                //run_param.desired_nalu_bytes = 100;

                /// constant bitrate or constant quality
                if (kbps) {
                    run_param.desired_frame_bytes = kbps*1000/8/fps;
                    run_param.qp_min = 10;
                    run_param.qp_max = 50;
                } else {
                    run_param.qp_min = run_param.qp_max = qp;
                }
                
                /// allocate encoded header (H264E maintains or allocates its source buffer)
                mx encoded = memory::raw_alloc(typeof(int8_t), 0, 0, 0);

                /// encode
                int encoded_len = 0;
                assert(!H264E_encode(enc, scratch, &run_param, &yuv, (u8**)&encoded.mem->origin, &encoded_len));
                encoded.mem->count   = size_t(encoded_len);
                encoded.mem->reserve = encoded.mem->count;

                //u8 *nalu = (u8*)encoded.mem->origin;
                //u8 &b0 = nalu[0];
                //u8 &b1 = nalu[1];
                //u8 &b2 = nalu[2];
                //u8 &b3 = nalu[3];
                /// output encoded
                if (!output(encoded))
                    break;

                /// maintained by H264E (if we referenced this, we may update it in place)
                /// better to have context in the caching mechanism here, just make it ion oriented
                encoded.mem->origin = null;

                /// get next frame
                frame = fetch();
                iframes++;
            }

            if (enc)
                free64(enc);
            
            if (scratch)
                free64(scratch);

    #if H264E_MAX_THREADS
            if (thread_pool)
                h264e_thread_pool_close(thread_pool, threads);
    #endif
            return iframes;
        }};

#else

    struct Session {

#if defined(__linux__) || defined(_WIN32)
        struct NVenc {
            void* enc;
            NV_ENC_REGISTER_RESOURCE       res       { NV_ENC_REGISTER_RESOURCE_VER };
            NV_ENC_MAP_INPUT_RESOURCE      input     { NV_ENC_MAP_INPUT_RESOURCE_VER };
            NV_ENCODE_API_FUNCTION_LIST    fn        { NV_ENCODE_API_FUNCTION_LIST_VER };
            NV_ENC_CREATE_BITSTREAM_BUFFER bitstream { NV_ENC_CREATE_BITSTREAM_BUFFER_VER };
        } nv;
#endif
        ~Session() {
            release();
        }

        GLFWwindow* glfw;
        int sx = -1, sy = -1;

        void update() {
            int x, y;
            glfwGetWindowSize(glfw, &x, &y);
            if (sx == -1 || x != sx || y != sy) {
                release();
                ///
                sx = x;
                sy = y;
                update_window();
            }
        }

#if defined(__linux)

        int         screen;
        ::Window    window;
        Pixmap      pixmap; /// this is active texture management in OS
        GLXDrawable glx_pixmap;
        GLuint      texture_id;
        GLint       attributes[5] = {
            GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
            GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
            None
        };
        int         visual_attribs[(22 * 2) + 1] = {
            GLX_X_RENDERABLE,   True,
            GLX_DRAWABLE_TYPE,  GLX_PIXMAP_BIT,
            GLX_RENDER_TYPE,    GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
            GLX_RED_SIZE,       8,
            GLX_GREEN_SIZE,     8,
            GLX_BLUE_SIZE,      8,
            GLX_ALPHA_SIZE,     8,
            GLX_DEPTH_SIZE,     24,
            GLX_STENCIL_SIZE,   8,
            GLX_DOUBLEBUFFER,   True,
            // Add any other attributes you need
            None
        };

        bool SetupGLXResources()
        {
            int argc = 1;
            char *argv[1] = {(char*)"dummy"};

            // Use glx context/surface
            glutInit(&argc, argv);
            glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
            glutInitWindowSize(16, 16);

            int window = glutCreateWindow("h264");
            if (!window)
            {
                std::cout << "\nUnable to create GLUT window.\n" << std::endl;
                return false;
            }

            glutHideWindow();
            return true;
        }

        bool encode(lambda<bool(mx)> &output) {
            void *bytes = null;

            // Now you can encode using the mapped resource.
            NV_ENC_PIC_PARAMS picParams { NV_ENC_PIC_PARAMS_VER };
            picParams.inputBuffer         = nv.input.mappedResource;
            picParams.inputWidth          = sx;
            picParams.inputHeight         = sy;
            picParams.outputBitstream     = nv.bitstream.bitstreamBuffer;
            picParams.bufferFmt           = NV_ENC_BUFFER_FORMAT_ARGB;
            NVENCSTATUS s0 = nv.fn.nvEncEncodePicture(nv.enc, &picParams);

            NV_ENC_LOCK_BITSTREAM lock { NV_ENC_LOCK_BITSTREAM_VER };
            lock.outputBitstream = nv.bitstream.bitstreamBuffer; // The output buffer associated with the input frame
            lock.doNotWait       = false; // Set to true for non-blocking mode
            NVENCSTATUS s1 = nv.fn.nvEncLockBitstream(nv.enc, &lock);

            bool result = false;
            if (s1 == NV_ENC_SUCCESS) {
                mx encoded = memory::raw_alloc(typeof(int8_t), 0, 0, 0);
                encoded.mem->origin  = lock.bitstreamBufferPtr;
                encoded.mem->count   = lock.bitstreamSizeInBytes;
                encoded.mem->reserve = lock.bitstreamSizeInBytes;
                result = output(encoded);
                encoded.mem->origin = null;
                nv.fn.nvEncUnlockBitstream(nv.enc, nv.bitstream.bitstreamBuffer);
            }
            return result;
        }

        void release() {
            if (glx_pixmap) {
                glXReleaseTexImageEXT(dpy, glx_pixmap, GLX_FRONT_LEFT_EXT);
                glXDestroyPixmap(dpy, glx_pixmap);
                glDeleteTextures(1, &texture_id);
                XFreePixmap(dpy, pixmap);

                nv.fn.nvEncUnmapInputResource(nv.enc, nv.input.mappedResource);
                nv.fn.nvEncDestroyBitstreamBuffer(nv.enc, &nv.bitstream);
                nv.fn.nvEncUnregisterResource(nv.enc, nv.res.registeredResource);
                nv.fn.nvEncDestroyEncoder((void*)nv.enc);
            }
        }

        void init(GLFWwindow* glfw) {
            static bool glx_init = false;
            /// perform this first; start a GL instance
            if (!glx_init) {
                SetupGLXResources();
                glx_init = true;
            }

            XSetErrorHandler (Xwindows_error);
            //glfwMakeContextCurrent(window);
            
            assert(NV_ENC_SUCCESS == NvEncodeAPICreateInstance(&nv.fn));

            window = glfwGetX11Window(glfw);
            this->glfw = glfw;

            NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS startup = { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
            startup.device     = null;
            startup.deviceType = NV_ENC_DEVICE_TYPE_OPENGL; // NV_ENC_DEVICE_TYPE_OPENGL
            startup.apiVersion = NVENCAPI_VERSION;

            NVENCSTATUS nvStatus = nv.fn.nvEncOpenEncodeSessionEx(&startup, &nv.enc);

            /// lookup a session by map id here..
            // Handle the error: The extension function isn't available.
            glXBindTexImageEXT = (PFNGlXBindTexImageEXTPROC) glXGetProcAddressARB((const GLubyte*) "glXBindTexImageEXT");
            if (!glXBindTexImageEXT) {
                console.fault("glXBindTexImageEXT not found");
            }
            // Get the X11 Pixmap
            // GLX setup to bind pixmap to a texture
            pixmap = XCompositeNameWindowPixmap(dpy, window);
            screen = DefaultScreen(dpy);
        }

        void update_window() {
            int          fbcount;
            GLXFBConfig* fbconfigs = glXChooseFBConfig(dpy, screen, visual_attribs, &fbcount);
            if (!fbconfigs) console.fault("glXChooseFBConfig no configurations");

            glx_pixmap = glXCreatePixmap(dpy, fbconfigs[0], pixmap, attributes);
            
            // Bind to an OpenGL texture
            glGenTextures(1, &texture_id);
            glBindTexture(GL_TEXTURE_2D, texture_id);
            glXBindTexImageEXT(dpy, glx_pixmap, GLX_FRONT_LEFT_EXT, NULL);

            // Register the texture with NVENC
            nv.res.resourceType         = NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX;

            NV_ENC_INPUT_RESOURCE_OPENGL_TEX tx = {
                texture_id, GL_TEXTURE_2D
            };

            nv.res.resourceToRegister   = &tx;
            nv.res.bufferFormat         = NV_ENC_BUFFER_FORMAT_ARGB;
            nv.res.width                = sx;
            nv.res.height               = sy;
            nv.res.pitch                = sx * 4;
            NVENCSTATUS s0 = nv.fn.nvEncRegisterResource(nv.enc, &nv.res);

            nv.input.registeredResource = nv.res.registeredResource;
            NVENCSTATUS s1 = nv.fn.nvEncMapInputResource(nv.enc, &nv.input);
            NVENCSTATUS s2 = nv.fn.nvEncCreateBitstreamBuffer(nv.enc, &nv.bitstream);

            int test = 0;
            test++;
        }

    #elif defined(_WIN32)
            IDXGIFactory1* pFactory;
            IDXGIAdapter1* pAdapter;
            IDXGIOutput* pOutput;
            DXGI_OUTPUT_DESC desc;
            ID3D11Texture2D* prev;
            HWND hWnd;

        void release() {
            pOutput->Release();
            pAdapter->Release();
            pFactory->Release();
        }

        void init(GLFWwindow *window) {
            hWnd = glfwGetWin32Window(window);

                // Obtain the DXGI factory
            if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory))) {
                assert("create dxgi failure");
                return;
            }

            if (FAILED(pFactory->EnumAdapters1(0, &pAdapter))) {
                pFactory->Release();
                assert("enum adapters failure");
                return;
            }

            if (FAILED(pAdapter->EnumOutputs(0, &pOutput))) {
                pAdapter->Release();
                pFactory->Release();
                assert("enum outputs failure");
                return;
            }

            pOutput->GetDesc(&desc);
        }

        ID3D11Texture2D* capture() {
            ID3D11Texture2D* pTexture = nullptr;

            if (desc.AttachedToDesktop && desc.DesktopCoordinates.right > 0) {
                IDXGIOutput1* pOutput1;
                if (SUCCEEDED(pOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&pOutput1))) {
                    ID3D11Device* pDevice;
                    D3D_FEATURE_LEVEL featureLevel;
                    if (SUCCEEDED(D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &pDevice, &featureLevel, nullptr))) {
                        ID3D11DeviceContext* pContext;
                        pDevice->GetImmediateContext(&pContext);

                        if (SUCCEEDED(pOutput1->DuplicateOutput(pDevice, &pOutput1))) {
                            DXGI_OUTDUPL_FRAME_INFO frameInfo;
                            IDXGIResource* pDesktopResource = nullptr;
                            if (SUCCEEDED(pOutput1->AcquireNextFrame(1000, &frameInfo, &pDesktopResource))) {
                                pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pTexture);
                                pDesktopResource->Release();
                                pOutput1->ReleaseFrame();
                            }
                            pOutput1->Release();
                        }
                        pContext->Release();
                        pDevice->Release();
                    }
                }
            }
            return pTexture;
        }

        void encode(lambda<bool(mx)> &output) {
            ID3D11Texture2D* tx = capture(); // acquired from DXGI Duplication
            if (!tx || tx != prev) {
                if (prev) {
                    // Unregister prevTexture with NVENC
                    nv.fn.nvEncUnregisterResource(nv.enc, nv.res.registeredResource);
                }
                // Register tx with NVENC
                nv.res.resourceType       = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
                nv.res.resourceToRegister = (void*)tx;
                nv.res.bufferFormat       = NV_ENC_BUFFER_FORMAT_ARGB;  // or appropriate format
                NVENCSTATUS nvStatus      = nv.fn.nvEncRegisterResource(nv.enc, &nv.res);
                prev = tx;
            }
        }

        void update_window() {
        }

    #elif defined(__APPLE__)

        void init(GLFWwindow* glfw) {
            this->glfw = glfw;
            /// on mac-os the AVFoundation and capture at window pos need to be instanced
        }

        void encode(lambda<bool(mx)> &output) {
            
        }

        void release() {
        }

        void update_window() {
        }

    #endif

    };

    async run() {
        return async {1, [this](runtime *rt, int index) -> mx {
            return true;
        }};
    }
    doubly<Session>  windows;

#endif

    lambda<bool(mx)> output;
    type_register(i264e);
};

mx_implement(h264e, mx);

h264e::h264e(lambda<bool(mx)> output) : h264e() {

#ifdef __linux__
    if (!dpy) {
        dpy = XOpenDisplay(NULL);
        int screen = DefaultScreen(dpy);
        if (!XCompositeQueryExtension(dpy, &composite_event_base, &composite_error_base)) {
            console.fault("X Composite extension required in X11 for accelerated encoding");
        }
        XCompositeRedirectSubwindows(dpy, RootWindow(dpy, screen), CompositeRedirectAutomatic);
    }
#endif

#ifdef USE_H264E
    data->gop        = 16;
    data->qp         = 33;
    data->kbps       = 0; /// not sure whats normal here, but default shouldnt set it
    data->fps        = 30;
    data->threads    = 4;
    data->speed      = 5;
    data->denoise    = false;
#endif

    data->output     = output;
}

void h264e::push(GLFWwindow *glfw) {
    auto fetch_window = [data=data](GLFWwindow* glfw) -> i264e::Session& {
        for (i264e::Session &c: data->windows)
            if (c.glfw == glfw)
                return c;

        i264e::Session &c = data->windows->push();
        c.init(glfw);
        return c;
    };
    i264e::Session &window = fetch_window(glfw);
    ///
    window.update();
    window.encode(data->output); // this calls output()
    /// needs a true/false return
}

void h264e::push(yuv420 image) {

#ifdef USE_H264E
    data->push(image);
#else
    assert(false);
#endif

}

async h264e::run() {
    return data->run();
}

};
