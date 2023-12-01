#include <media/media.hpp>
#include <media/h264.hpp>
#include <async/async.hpp>

#define MINIH264_IMPLEMENTATION
#define H264E_ENABLE_PLAIN_C    1
#include <media/minih264e.h>

#include <h264bsd_decoder.h>

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

struct i264 {
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

    void                   *thread_pool;
    H264E_persist_t        *enc;
    H264E_scratch_t        *scratch;
    mutex                   encode_mtx;
    yuv420                  encode_input;
    bool                    started;
    int                     iframes;

    ~i264() {
        if (enc)     free64(enc);
        if (scratch) free64(scratch);
#if H264E_MAX_THREADS
        if (thread_pool) h264e_thread_pool_close(thread_pool, threads);
#endif
    }

    void start(yuv420 &ref) {
        gop        = 5; //16 = low
        qp         = 11; //33 = low
        kbps       = 0; /// not sure whats normal here, but default shouldnt set it
        fps        = 30;
        threads    = 4;
        speed      = 5;
        denoise    = false;
        width      = ref.width();
        height     = ref.height();
        frame_size = width * height * 3 / 2; /// yuv 420
        started    = true;
        
        /// this will be nice to use!
    #if H264E_SVC_API
        create_param.num_layers = 1;
        create_param.inter_layer_pred_flag = 0;
    #endif

    #if H264E_MAX_THREADS
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
        error = H264E_sizeof(&create_param, &sizeof_enc, &sizeof_scratch);
        if (error) {
            printf("H264E_init error = %d\n", error);
            return;
        }
        printf("sizeof_enc = %d sizeof_scratch = %d\n", sizeof_enc, sizeof_scratch);
        enc     = (H264E_persist_t *)calloc64(sizeof_enc, 1);
        scratch = (H264E_scratch_t *)calloc64(sizeof_scratch, 1);
        error   = H264E_init(enc, &create_param);
    }

    /// called from outside thread; we must maintain sync with encoder
    array<u8> encode(yuv420 &frame) {
        num w = frame.width();
        num h = frame.height();
        num w_prev = width;
        num h_prev = height;
        array<u8> res(size(w * h * 4 * 2));
        int w_cursor = 0;

        if (!started)
            start(frame);

        /// verify frame is correct count of bytes, yuv 420
        u8* buf_in = (u8*)frame.mem->origin;
        assert(frame.mem->count * frame.mem->type->base_sz == frame_size);

        yuv.yuv[0]    = buf_in; 
        yuv.yuv[1]    = buf_in + w*h; 
        yuv.yuv[2]    = buf_in + w*h*5/4; 
        yuv.stride[0] = w;
        yuv.stride[1] = w/2;
        yuv.stride[2] = w/2;
        
        run_param.frame_type = 0;
        run_param.encode_speed = speed;
        //run_param.desired_nalu_bytes = 100;

        /// constant bitrate or constant quality
        if (kbps) {
            run_param.desired_frame_bytes = kbps*1000/8/fps;
            run_param.qp_min = 10;
            run_param.qp_max = 50;
        } else
            run_param.qp_min = run_param.qp_max = qp;
 
        /// encode
        u8* encoded_raw = null;
        int encoded_len = 0;
        assert(!H264E_encode(enc, scratch, &run_param, &yuv, (u8**)&encoded_raw, &encoded_len));
        iframes++;

        memcpy(&res.data[w_cursor], (u8*)encoded_raw, (size_t)encoded_len);
        res.set_size((size_t)encoded_len);
        return res;
    }

    register(i264);
};

/*

MP4FileHandle mp4File;
    MP4TrackId    trackId;
    
    // Open the MP4 file
    mp4File = MP4Read("mp4/output.mp4");
    if (mp4File == MP4_INVALID_FILE_HANDLE) {
        // Handle error
        return 1;
    }

    // Get the number of tracks in the MP4 file
    uint32_t numTracks = MP4GetNumberOfTracks(mp4File);
    
    for (uint32_t i = 0; i < numTracks; i++) {
        trackId = MP4FindTrackId(mp4File, i);
        
        // Get the track type (video, audio, etc.)
        str trackType = MP4GetTrackType(mp4File, trackId);
        
        // Get the codec name of the track
        str codecName = MP4GetTrackMediaDataName(mp4File, trackId);
        
        // Print track information
        printf("Track ID: %u\n", trackId);
        printf("Track Type: %s\n", (trackType == "h264") ? "Video" : "Audio");
        printf("Codec Name: %s\n", codecName.data);
    }
    fflush(stdout);

MStream h264bsd_test() {
    // Initialize decoder
    storage_t* storage = h264bsdAlloc();
    h264bsdInit(storage, 0);

    // Variables for NALU data
    u8* naluData;
    u32 naluSize;

    // Variables for picture size
    u32 picWidth, picHeight, picSize;
    u32 cropParamsAvailable;

    auto readNextNalu = lambda<bool(u8**, u32*)> [&](u8** pdata, u32* plen) -> bool {

    };

    // Main loop to read NALUs and decode
    while (readNextNalu(&naluData, &naluSize)) { // Implement this function
        // Feed NAL unit to the decoder
        u32 usedBytes = h264bsdDecode(storage, naluData, naluSize, 0, &readBytes);

        if (usedBytes == H264BSD_RDY) {
            // Check if sequence parameters are available
            if (h264bsdIsSequenceHeaderReady(storage)) {
                // Get picture size information
                picWidth = h264bsdPicWidth(storage);
                picHeight = h264bsdPicHeight(storage);
                picSize = h264bsdPicSizeInMbs(storage) * 256;

                // Check for cropping parameters (if the video is cropped)
                u32 l, w, h, o;
                h264bsdCroppingParams(storage, &cropParamsAvailable, &l, &w, &t, &h);
                if (cropParamsAvailable) {
                    // Adjust width and height based on cropping parameters
                    picWidth  = w;
                    picHeight = h;
                }

                printf("Video dimensions: %ux%u\n", picWidth, picHeight);
            }

            // Retrieve and process the decoded picture
            u32 picId;
            u8* picData;
            h264bsdNextOutputPicture(storage, &picData, &picId, &error);
            processDecodedPicture(picData, picSize); // Implement this function
        } else if (usedBytes == H264BSD_ERROR) {
            printf("Decoding error\n");
            break;
        }
    }

    // Release resources
    h264bsdShutdown(storage);
    h264bsdFree(storage);

    return 0;
}
*/

mx_implement(h264, mx);

array<u8> h264::encode(yuv420 image) { /// its not a ref, so we can pass image and have it auto convert to yuv420
    return data->encode(image); /// handles resize with the proper nalu packet here, so the controller doesnt need to
}

}