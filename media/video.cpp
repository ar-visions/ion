#define MP4V2_USE_STATIC_LIB
#include <mp4v2/mp4v2.h>
#include <fdk-aac/aacenc_lib.h>

#include <mx/mx.hpp>
#include <media/h264.hpp>
#include <media/video.hpp>

#include <iostream>
#include <random>
#include <vector>

using namespace std;

#define SAMPLE_RATE 48000
#define CHANNELS 1
#define DURATION 10  // 10 seconds
#define FRAME_SIZE 1024  // AAC frame size for many profiles

int mp4_test() {
    // Initialize random number generator
    srand(time(NULL));

    // Allocate buffer for 10 seconds of random mono audio at 48kHz
    int16_t buffer[SAMPLE_RATE * DURATION];
    for (int i = 0; i < SAMPLE_RATE * DURATION; ++i) {
        buffer[i] = rand() % 65536 - 32768;  // Generate random noise
    }

    // AAC encoder initialization (you need to add error checks and configuration)
    HANDLE_AACENCODER encoder;
    AACENC_InfoStruct encInfo;
    aacEncOpen(&encoder, 0, CHANNELS);
    aacEncoder_SetParam(encoder, AACENC_SAMPLERATE, SAMPLE_RATE);
    aacEncoder_SetParam(encoder, AACENC_CHANNELMODE, MODE_1);
    aacEncoder_SetParam(encoder, AACENC_BITRATE, 64000);
    aacEncoder_SetParam(encoder, AACENC_TRANSMUX, TT_MP4_RAW);

    aacEncEncode(encoder, NULL, NULL, NULL, NULL); // Initialize encoder
    aacEncInfo(encoder, &encInfo);

    // Encoding loop
    AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
    AACENC_InArgs in_args = { 0 };
    AACENC_OutArgs out_args = { 0 };
    int in_identifier = IN_AUDIO_DATA, out_identifier = OUT_BITSTREAM_DATA;
    void *in_ptr, *out_ptr;
    INT in_size, in_elem_size, out_size, out_elem_size;
    UCHAR outbuf[20480]; // 20k should be more than enough
    INT_PCM *convert_buf = (INT_PCM*)buffer;

    for (int i = 0; i < SAMPLE_RATE * DURATION; i += FRAME_SIZE) {
        // Set up input buffer
        in_ptr = &convert_buf[i];
        in_size = FRAME_SIZE * sizeof(INT_PCM);
        in_elem_size = sizeof(INT_PCM);
        in_buf.numBufs = 1;
        in_buf.bufs = &in_ptr;
        in_buf.bufferIdentifiers = &in_identifier;
        in_buf.bufSizes = &in_size;
        in_buf.bufElSizes = &in_elem_size;

        // Set up output buffer
        out_ptr = outbuf;
        out_size = sizeof(outbuf);
        out_elem_size = 1;
        out_buf.numBufs = 1;
        out_buf.bufs = &out_ptr;
        out_buf.bufferIdentifiers = &out_identifier;
        out_buf.bufSizes = &out_size;
        out_buf.bufElSizes = &out_elem_size;

        // Set up in_args
        in_args.numInSamples = FRAME_SIZE <= SAMPLE_RATE * DURATION - i ? FRAME_SIZE : SAMPLE_RATE * DURATION - i;

        // Encode frame
        if (aacEncEncode(encoder, &in_buf, &out_buf, &in_args, &out_args) != AACENC_OK) {
            // Handle error
            break;
        }

        if (out_args.numOutBytes == 0) {
            continue;
        }

        // Here, outbuf contains the encoded AAC frame of length out_args.numOutBytes
        // You should add this frame to your MP4 container here
    }

    // Finalize encoding
    aacEncEncode(encoder, NULL, NULL, NULL, NULL);

    // Create MP4 file and add AAC track (use mp4v2 library)
    MP4FileHandle mp4file = MP4Create("/home/kalen/output.mp4", 0);
    MP4TrackId track = MP4AddAudioTrack(mp4file, SAMPLE_RATE, MP4_INVALID_DURATION, MP4_MPEG4_AUDIO_TYPE);
    MP4SetAudioProfileLevel(mp4file, 2);
    // Add encoded AAC frames to the MP4 track

    // Finalize file
    MP4Close(mp4file, 0);

    // Clean up
    aacEncClose(&encoder);

    return 0;
}



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
    AACENC_InArgs      args { 0, 0 };
    AACENC_OutArgs     out_args { };

    u8*                buffer_aac;
    int                buffer_aac_id = OUT_BITSTREAM_DATA;
    int                buffer_aac_sz;
    int                buffer_aac_el = sizeof(u8);
    int                buffer_pcm_id = IN_AUDIO_DATA;
    int                buffer_pcm_sz;
    int                buffer_pcm_el = sizeof(short);
    Frame*             current = null;
    mutex              mtx_write;

    register(iVideo);

    void stop() {
        if (!stopped) {
            mtx_write.lock();
            stopped = true;
            aacEncClose(&aac_encoder);
            MP4Close(mp4);
            mtx_write.unlock();
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
        mp4_test();
        exit(0);

        assert(audio_channels == 1 || audio_channels == 2);

        buffer_aac_sz = 128000 / 8 / hz * audio_channels * 10; /// adequate size for encoder
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
        // Set the number of channels to 1 for mono audio
        if (!MP4SetTrackIntegerProperty(mp4, audio_track, "mdia.minf.stbl.stsd.*.channels", 1)) {
            // Handle error
            assert(false);
        }

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
                Frame *f = current;
                auto wait_for_media = [&]() {
                    /// critical section: must wait until they are both set! 
                    for (;;) {
                        f->mtx.lock();

                        /// wait for both resources on this
                        if (f->audio && f->video) // todo: needs to be a resource array
                            break;
                        
                        f->mtx.unlock();
                        usleep(10);
                    }
                };
                auto write_encoded_audio = [&]() {
                    if (f->audio) {
                        assert(f->audio->type == Media::PCM);
                        int n_samples = f->audio->buf.count();
                        assert(n_samples == buffer_pcm_sz / sizeof(short)); // buf == 1600, buffer_pcm_sz == 3200 (extra channel?)
                        /// set input pointer for this operation
                        u8 *mbuf = (u8*)f->audio->buf.mem->origin;
                        int buf_id = f->audio->id;
                        /// sequential id on audio buffers; output here to verify identity.

                        input_pcm.bufs = (void**)&mbuf;
                        for (int i = 0; i < buffer_pcm_sz; i++)
                            mbuf[i] = (u8)(rand::uniform(0, 255));
                        
                        args.numInSamples = n_samples;
                        assert(aacEncEncode(aac_encoder, &input_pcm, &output_aac, &args, &out_args) == AACENC_OK);
                        input_pcm.bufs = (void**)0;

                        if (out_args.numOutBytes) {
                            MP4WriteSample(mp4, audio_track, buffer_aac, buffer_aac_sz);
                        }
                    }
                };
                auto write_encoded_video = [&]() {
                    ion::array<u8> nalus = h264_encoder.encode(f->image); /// needs a quality setting; this default is very high quality and constant quality too; meant for data science work
                    MP4WriteSample(mp4, video_track, nalus.data, nalus.len());
                    printf("writing video packet of %d bytes\n", (int)nalus.len());
                    f->audio = {};
                    f->video = {};
                    assert(!f->audio);
                    assert(!f->video);
                };
                auto unlock_frame = [&]() {
                    frames++;
                    f->mtx.unlock();
                };
                wait_for_media();
                if (!stopped) {
                    mtx_write.lock();
                    printf("writing frame id %d\n", (int)frames);
                    write_encoded_audio();
                    write_encoded_video();
                    mtx_write.unlock();
                }
                unlock_frame();
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