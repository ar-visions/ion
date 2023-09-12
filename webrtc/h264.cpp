#include <webrtc/webrtc.hpp>
#include <vk/vk.hpp>
#include <media/media.hpp>
#include <async/async.hpp>
#include <nvenc/nvEncodeAPI.h>

///
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#if defined(__linux__)
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
static Display* dpy;
static int composite_event_base, composite_error_base;

#elif defined(_WIN32_)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include <GLFW/glfw3native.h>


namespace ion {

using PFNGlXBindTexImageEXTPROC = void (*)(Display *, GLXDrawable, int, const int *);
using PFNGlXReleaseTexImageEXTPROC = void (*)(Display *, GLXDrawable, int);

PFNGlXBindTexImageEXTPROC    glXBindTexImageEXT    = null;
PFNGlXReleaseTexImageEXTPROC glXReleaseTexImageEXT = null;

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


using namespace ion;

struct i264e {
protected:
    H264E_create_param_t    create_param;
    H264E_run_param_t       run_param;
    H264E_io_yuv_t          yuv;
    uint8_t                *buf_in, *buf_save;
    uint8_t                *coded_data;
    size_t                  frame_size, width, height;
public:
    int gop;        /// key frame period
    int qp;         /// QP = 10..51
    int kbps;       /// 0 = default (constant qp)
    int fps;        /// 30 is default
    int threads;    /// 4 = default
    int speed;      /// 0 = best quality, 10 = fastest
    bool denoise;
    bool valid;     /// while waiting for first frame it should be valid

#if defined(__linux__)

    struct glxCache {
        
        
        
        struct NVenc {
            void* enc;
            NV_ENC_REGISTER_RESOURCE       res       { NV_ENC_REGISTER_RESOURCE_VER };
            NV_ENC_MAP_INPUT_RESOURCE      input     { NV_ENC_MAP_INPUT_RESOURCE_VER };
            NV_ENCODE_API_FUNCTION_LIST    fn        { NV_ENCODE_API_FUNCTION_LIST_VER };
            NV_ENC_CREATE_BITSTREAM_BUFFER bitstream { NV_ENC_CREATE_BITSTREAM_BUFFER_VER };

        } nv;

        int         screen;
        int         sx = -1, sy = -1;
        GLFWwindow* glfw;
        ::Window    window;
        Pixmap      pixmap; /// this is active texture management in OS
        GLXDrawable glx_pixmap;
        GLuint      texture_id;
        GLint       attributes[5] = {
            GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
            GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
            None
        };
        int         visual_attribs[24] = {
            GLX_X_RENDERABLE,   True,
            GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
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
            None,               0
        };

        ~glxCache() {
            release();
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
            glXReleaseTexImageEXT(dpy, glx_pixmap, GLX_FRONT_LEFT_EXT);
            glXDestroyPixmap(dpy, glx_pixmap);
            glDeleteTextures(1, &texture_id);
            XFreePixmap(dpy, pixmap);

            // After you're done, unmap and unregister the resource
            nv.fn.nvEncUnmapInputResource(nv.enc, nv.input.mappedResource);
            
            nv.fn.nvEncDestroyBitstreamBuffer(nv.enc, &nv.bitstream);

            nv.fn.nvEncUnregisterResource(nv.enc, nv.res.registeredResource);
            nv.fn.nvEncDestroyEncoder((void*)nv.enc);
        }

        void init(GLFWwindow* glfw) {
            nv.fn.version = NV_ENCODE_API_FUNCTION_LIST_VER;
            assert(NV_ENC_SUCCESS == NvEncodeAPICreateInstance(&nv.fn));
            NVENCSTATUS nvStatus = nv.fn.nvEncOpenEncodeSession(
                glfw, NV_ENC_DEVICE_TYPE_OPENGL, &nv.enc); /// handle is opaque for GL

            this->glfw = glfw;
            window = glfwGetX11Window(glfw);
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

        void update() {
            int x, y;
            glfwGetWindowSize(glfw, &x, &y);
            if (sx == -1 || x != sx || y != sy) {
                release();
                ///
                sx = x;
                sy = y;
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
                nv.res.resourceToRegister   = &texture_id;
                nv.res.bufferFormat         = NV_ENC_BUFFER_FORMAT_ARGB;
                nv.fn.nvEncRegisterResource(nv.enc, &nv.res);

                nv.input.registeredResource = nv.res.registeredResource;
                nv.fn.nvEncMapInputResource(nv.enc, &nv.input);

                nv.fn.nvEncCreateBitstreamBuffer(nv.enc, &nv.bitstream);
            }
        }
    };

    doubly<glxCache> windows;

#endif

    mutex  encode_mtx;
    yuv420 encode_input;

    lambda<yuv420(i64)> input;
    lambda<bool(mx)>   output;

    type_register(i264e);

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
    }
};

mx_implement(h264e, mx);

h264e::h264e(lambda<bool(mx)> output, lambda<yuv420(i64)> input) : h264e() {

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

    data->input      = input;
    data->output     = output;
    data->gop        = 16;
    data->qp         = 33;
    data->kbps       = 0; /// not sure whats normal here, but default shouldnt set it
    data->fps        = 30;
    data->threads    = 4;
    data->speed      = 5;
    data->denoise    = false;

    data->run();
    /// if input then call at fps rate
}

void h264e::push(GLFWwindow *glfw) {
    auto fetch_window = [data=data](GLFWwindow* glfw) -> i264e::glxCache& {
        for (i264e::glxCache &c: data->windows)
            if (c.glfw == glfw)
                return c;

        i264e::glxCache &c = data->windows->push();
        c.init(glfw);
        return c;
    };
    i264e::glxCache &window = fetch_window(glfw);
    ///
    window.update();
    window.encode(data->output); // this calls output()
    /// needs a true/false return
}

void h264e::push(yuv420 image) {
    data->push(image);
}

async h264e::run() {
    return data->run();
}

};
