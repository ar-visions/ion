#define MP4V2_USE_STATIC_LIB
#include <mp4v2/mp4v2.h>
#include <fdk-aac/aacenc_lib.h>

#include <mx/mx.hpp>
#include <media/h264.hpp>
#include <media/video.hpp>

using namespace ion;

struct iVideo {
    MP4FileHandle      mp4;
    MP4TrackId         video_track;
    MP4TrackId         audio_track;
    HANDLE_AACENCODER  aac_encoder;
    h264               h264_encoder;
    
    int                width, height, hz = 30;
    int                audio_rate        = 48000;
    int                audio_channels    = 1;
    bool               stopped;
    int                pts;

    AACENC_BufDesc     output_aac;
    AACENC_BufDesc     input_pcm;
    AACENC_InArgs      args { 1, 0 };
    AACENC_OutArgs     out_args { };

    u8*                buffer_aac;
    int                buffer_aac_id = 1;
    int                buffer_aac_sz;
    int                buffer_aac_el = sizeof(u8);
    int                buffer_pcm_id = 2;
    int                buffer_pcm_sz;
    int                buffer_pcm_el = sizeof(short);
    Frame*             current = null;

    register(iVideo);

    void stop() {
        if (!stopped) {
            stopped = true;
            aacEncClose(&aac_encoder);
            MP4Close(mp4);
        }
    }

    int write_frame(Frame &f) {
        current = &f; /// no need to sync this with mutex
        return 0;
    }

    ~iVideo() {
        stop();
    }

    void start(path &output) {
        assert(audio_channels == 1 || audio_channels == 2);

        buffer_aac_sz = 128000 / 8 / hz * audio_channels;
        buffer_pcm_sz = sizeof(short) * audio_channels * audio_rate / hz;
        buffer_aac = (u8*)   calloc(1, buffer_aac_sz);

        input_pcm.numBufs            = 1;
        input_pcm.bufs               = (void**)0;
        input_pcm.bufSizes           = &buffer_pcm_sz;
        input_pcm.bufferIdentifiers  = &buffer_pcm_id;
        input_pcm.bufElSizes         = &buffer_pcm_el;
        output_aac.numBufs           = 1;
        output_aac.bufs              = (void**)&buffer_aac;
        output_aac.bufSizes          = &buffer_aac_sz;
        output_aac.bufferIdentifiers = &buffer_aac_id;
        output_aac.bufElSizes        = &buffer_aac_el;
        
        // init aac encoder
        assert(aacEncOpen(&aac_encoder, 0, 1) == AACENC_OK);
        
        AACENC_InfoStruct info = { 0 };
        assert(aacEncoder_SetParam(aac_encoder, AACENC_AOT,         AOT_AAC_LC) == AACENC_OK);
        assert(aacEncoder_SetParam(aac_encoder, AACENC_BITRATE,     128000)     == AACENC_OK);
        assert(aacEncoder_SetParam(aac_encoder, AACENC_SAMPLERATE,  audio_rate) == AACENC_OK);
        assert(aacEncoder_SetParam(aac_encoder, AACENC_CHANNELMODE, audio_channels == 1? MODE_1 : MODE_2) == AACENC_OK);
        
        assert(aacEncEncode(aac_encoder, NULL, NULL, NULL, NULL) == AACENC_OK);

        mp4 = MP4Create((symbol)output.cs());
        assert(mp4 != MP4_INVALID_FILE_HANDLE);
        
        video_track = MP4AddH264VideoTrack(mp4,
            hz, 1, width, height, // time base is hz, duration is 1, 1/hz
            0x42, // Baseline Profile (BP)
            0x80, // no-constraints
            0x1F, // level 3.1, commonly used for standard high-definition video, can be indicated with the value 0x1F.
            3);   // 4-1. This value is used in the encoding of the length of the NAL units in the video stream. Typically, this is set to 3, which means the length field size is 4 bytes (since it's zero-based counting).

        assert(audio_rate % hz == 0);
        audio_track = MP4AddAudioTrack(
            mp4, audio_rate, audio_rate / hz, MP4_MPEG2_AAC_LC_AUDIO_TYPE);

        ion::async([this](runtime *rt, int i) -> mx {
            while (!stopped && !current)
                yield();
            
            i64 start_write = millis();
            i64 frames = 0;

            while (!stopped) {
                i64 start = start_write + frames * (1000 / hz);
                i64 t = millis();

                if (start < t) {
                    printf("warning: took too long to write frames; origin is offset\n");
                } else {
                    i64 w = (start - t);
                    usleep(w * 1000);
                }
                
                if (stopped)
                    break;
                
                Frame *f = current;
                f->mtx.lock();
                if (f->audio) {
                    assert(f->audio->type == Media::PCM);
                    assert(f->audio->buf.count() == buffer_pcm_sz / sizeof(short)); // buf == 1600, buffer_pcm_sz == 3200 (extra channel?)
                    /// set input pointer for this operation
                    input_pcm.bufs = &f->audio->buf.mem->origin;
                    assert(aacEncEncode(aac_encoder, &input_pcm, &output_aac, &args, &out_args) == AACENC_OK);
                    MP4WriteSample(mp4, audio_track, buffer_aac, buffer_aac_sz);
                    input_pcm.bufs = (void**)0;
                }

                array<u8> nalus = h264_encoder.encode(f->image); /// needs a quality setting; this default is very high quality and constant quality too; meant for data science work
                MP4WriteSample(mp4, video_track, nalus.data, nalus.len());
                f->mtx.unlock();
                frames++;
            }
            return true;
        });
    }
};

mx_implement(Video, mx);

Video::Video(int width, int height, int hz, int audio_rate, path output) : Video() {
    data->width      = width;
    data->height     = height;
    data->hz         = hz;
    data->audio_rate = audio_rate;
    data->start(output);
}

int Video::write_frame(Frame &f) {
    return data->write_frame(f);
}

void Video::stop() {
    data->stop();
}