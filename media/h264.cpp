#include <media/media.hpp>
#include <media/h264.hpp>
#include <async/async.hpp>

#define MINIH264_IMPLEMENTATION
#define H264E_ENABLE_PLAIN_C    1
#include <media/minih264e.h>

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

namespace ion {

struct i264e {
public:
    bool                    valid;     /// while waiting for first frame it should be valid
    size_t                  frame_size, width, height;

    H264E_create_param_t    create_param;
    H264E_run_param_t       run_param;
    H264E_io_yuv_t          yuv;
    uint8_t                *buf_in, *buf_save;
    uint8_t                *coded_data;

    int                     gop;        /// key frame period
    int                     qp;         /// QP = 10..51
    int                     kbps;       /// 0 = default (constant qp)
    int                     fps;        /// 30 is default
    int                     threads;    /// 4 = default
    int                     speed;      /// 0 = best quality, 10 = fastest
    bool                    denoise;

    mutex                   encode_mtx;
    yuv420                  encode_input;

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

    lambda<bool(mx)> output;
    type_register(i264e);
};

mx_implement(h264e, mx);

h264e::h264e(lambda<bool(mx)> output) : h264e() {
    data->gop        = 16;
    data->qp         = 33;
    data->kbps       = 0; /// not sure whats normal here, but default shouldnt set it
    data->fps        = 30;
    data->threads    = 4;
    data->speed      = 5;
    data->denoise    = false;
    data->output     = output;
}

void h264e::push(yuv420 image) {
    data->push(image);
}

async h264e::run() {
    return data->run();
}

}