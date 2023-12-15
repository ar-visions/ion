#define MP4V2_USE_STATIC_LIB
#include <mp4v2/mp4v2.h>
#include <fdk-aac/aacenc_lib.h>

#include <media/media.hpp>
#include <media/h264.hpp>
#include <media/video.hpp>

#include <iostream>
#include <random>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdint.h>

extern "C" {
    #include <shine/layer3.h>
};

using namespace std;

#define SAMPLE_RATE 48000
#define CHANNELS    2
#define DURATION    10  // 10 seconds
#define FRAME_SIZE  1600  // 48000 (let 1 sample = 1 or 2 channels) / 30 (for video; the frame is the amount of audio in one frame)

namespace ion {

#if 1
/// fdk aac requires us to use their frame size; not exactly sure how to increase it either.
/// we pad our allocations to allow for this non divisible number 1024 to overlap our total samples
/// it seems to have multiple arguments and allow for it as long as the numberi s larger; however it doesnt work properly.
bool audio::save_m4a(path dest, i64 bitrate) {
    // AAC encoder initialization (you need to add error checks and configuration)
    HANDLE_AACENCODER encoder;
    aacEncOpen(&encoder, 0, data->channels);

    aacEncoder_SetParam(encoder, AACENC_SAMPLERATE,     48000);
    aacEncoder_SetParam(encoder, AACENC_AOT,            2);
    aacEncoder_SetParam(encoder, AACENC_CHANNELORDER,   0);
    aacEncoder_SetParam(encoder, AACENC_CHANNELMODE,    data->channels == 1 ? MODE_1 : MODE_2);
    aacEncoder_SetParam(encoder, AACENC_BITRATE,        bitrate);
    aacEncoder_SetParam(encoder, AACENC_SIGNALING_MODE, 0);
    aacEncoder_SetParam(encoder, AACENC_TRANSMUX,       2); /// ADTS mode; this may have been the issue?

    AACENC_BufDesc in_buf        = { 0 }, out_buf = { 0 };
    AACENC_InArgs  in_args       = { 0 };
    AACENC_OutArgs out_args      = { 0 };
    
    aacEncEncode(encoder, NULL, NULL, NULL, NULL); // Initialize encoder
    AACENC_InfoStruct info = {0}; 
    assert(aacEncInfo(encoder, &info) == AACENC_OK);

    int frame_size = info.frameLength; /// this is before we * (PCM short) (2) * channels
    int n_frames   = (int)std::ceil((double)data->total_samples / (double)frame_size);

    // Encoding loop
    int            in_identifier = IN_AUDIO_DATA, out_identifier = OUT_BITSTREAM_DATA;
    void          *in_ptr, *out_ptr;
    INT            in_size, in_elem_size, out_size, out_elem_size;
    UCHAR outbuf[20480]; // 20k should be more than enough
    MP4FileHandle mp4file = MP4Create((symbol)dest.cs(), 0);

    MP4TrackId track = MP4AddAudioTrack(
        mp4file, data->sample_rate, frame_size, MP4_MPEG4_AAC_LC_AUDIO_TYPE);

    MP4SetTrackIntegerProperty(mp4file, track, "mdia.minf.stbl.stsd.*[0].channels", data->channels);
    MP4SetAudioProfileLevel(mp4file, 1);

    for (int i = 0; i < n_frames; i++) {
        // Set up input buffer
        in_ptr                     = &data->samples[i * frame_size * data->channels];
        in_size                    = frame_size * data->channels * sizeof(short);
        in_elem_size               = sizeof(short);
        in_buf.numBufs             = 1;
        in_buf.bufs                = &in_ptr;
        in_buf.bufferIdentifiers   = &in_identifier;
        in_buf.bufSizes            = &in_size;
        in_buf.bufElSizes          = &in_elem_size;

        // Set up output buffer
        out_ptr                    = outbuf;
        out_size                   = sizeof(outbuf);
        out_elem_size              = 1;
        out_buf.numBufs            = 1;
        out_buf.bufs               = &out_ptr;
        out_buf.bufferIdentifiers  = &out_identifier;
        out_buf.bufSizes           = &out_size;
        out_buf.bufElSizes         = &out_elem_size;

        // Set up in_args
        in_args.numInSamples       = frame_size * data->channels;
        
        // Encode frame
        if (aacEncEncode(encoder, &in_buf, &out_buf, &in_args, &out_args) != AACENC_OK)
            break;

        if (out_args.numOutBytes == 0)
            continue;

        MP4WriteSample(mp4file, track, (const uint8_t*)out_ptr, out_args.numOutBytes, MP4_INVALID_DURATION, 0, false);
    }

    MP4Close(mp4file, 0);
    aacEncClose(&encoder);
    return true;
}

#else 

/// writing mp3 without incident; sounds exactly the same at 256k
bool audio::save_m4a(path dest, i64 bitrate) {
    MP4FileHandle mp4file = MP4Create((symbol)dest.cs(), 0);
    const size_t samples_per_frame = 1152;
    MP4TrackId track = MP4AddAudioTrack(
        mp4file, data->sample_rate, samples_per_frame, MP4_MP3_AUDIO_TYPE);

    MP4SetTrackIntegerProperty(mp4file, track, "mdia.minf.stbl.stsd.*[0].channels", data->channels);
    MP4SetAudioProfileLevel(mp4file, 1);

    shine_config_t config { };
    shine_t        s;

    /// set defaults
    shine_set_config_mpeg_defaults(&config.mpeg);
    config.wave.channels   = (enum channels)data->channels;
    config.wave.samplerate = data->sample_rate;
    config.mpeg.bitr       = int(bitrate / 1000);
    config.mpeg.emph       = NONE; /// no emphasis
    config.mpeg.original   = 1;    /// original
    config.mpeg.mode       = data->channels == 2 ? STEREO : MONO;

    /// init shine
    if (!(s = shine_initialise(&config))) console.fault("cannot init shine");

    /// read pcm into shine, encode and write to output
    for (i64 i = 0; i < data->total_samples; i += samples_per_frame) {
        int n_bytes = 0;
        u8 *encoded = shine_encode_buffer_interleaved(
            s, &data->samples[i * data->channels], &n_bytes);
        MP4WriteSample(mp4file, track, (const uint8_t*)encoded, n_bytes, MP4_INVALID_DURATION, 0, false);
    }
    /// cleanup
    shine_close(s);
    
    // Finalize encoding
    MP4Close(mp4file, 0);
    return true;
}

#endif

struct iVideo {
    MP4FileHandle      mp4;
    MP4TrackId         video_track;
    MP4TrackId         audio_track;
    HANDLE_AACENCODER  aac_encoder;
    h264               h264_encoder;
    
    int                width, height, hz = 30;
    int                audio_rate        = 48000;
    int                audio_channels    = 1;
    int                audio_frame_size;
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
    short             *pcm_buffer;
    int                pcm_buffer_len;

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
        aacEncoder_SetParam(aac_encoder, AACENC_SAMPLERATE,     48000);
        aacEncoder_SetParam(aac_encoder, AACENC_AOT,            2);
        aacEncoder_SetParam(aac_encoder, AACENC_CHANNELORDER,   0);
        aacEncoder_SetParam(aac_encoder, AACENC_CHANNELMODE,    audio_channels == 1 ? MODE_1 : MODE_2);
        aacEncoder_SetParam(aac_encoder, AACENC_BITRATE,        128000);
        aacEncoder_SetParam(aac_encoder, AACENC_SIGNALING_MODE, 0);
        aacEncoder_SetParam(aac_encoder, AACENC_TRANSMUX,       2); /// ADTS mode; this may have been the issue?
        assert(aacEncEncode(aac_encoder, NULL, NULL, NULL, NULL) == AACENC_OK);
        
        AACENC_InfoStruct aac_info = { 0 };
        assert(aacEncInfo(aac_encoder, &aac_info) == AACENC_OK);
        audio_frame_size = aac_info.frameLength;

        pcm_buffer = (short*)calloc(sizeof(short), audio_frame_size * audio_channels * 64);

        mp4 = MP4Create((symbol)output.cs());
        assert(mp4 != MP4_INVALID_FILE_HANDLE);
        MP4SetAudioProfileLevel(mp4, 1);
        
        video_track = MP4AddH264VideoTrack(mp4,
            hz, 1, width, height, // time base is hz, duration is 1, 1/hz
            0x42, // Baseline Profile (BP)
            0x80, // no-constraints
            0x1F, // level 3.1, commonly used for standard high-definition video, can be indicated with the value 0x1F.
            3);   // 4-1. This value is used in the encoding of the length of the NAL units in the video stream. Typically, this is set to 3, which means the length field size is 4 bytes (since it's zero-based counting).

        assert(audio_rate % hz == 0);
        audio_track = MP4AddAudioTrack(
            mp4, audio_rate, audio_frame_size, MP4_MPEG2_AAC_LC_AUDIO_TYPE);

        MP4SetTrackIntegerProperty(mp4, audio_track, "mdia.minf.stbl.stsd.*[0].channels", audio_channels);

        ion::async([this](runtime *rt, int i) -> mx {
            while (!stopped && !current)
                yield();
            
            i64 start_write = millis();
            i64 frames = 0;
            Frame *f;

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

                    memcpy(&pcm_buffer[pcm_buffer_len], mbuf, n_samples * sizeof(short)); /// bytes to copy
                    pcm_buffer_len += n_samples;

                    for (int i = 0; i < pcm_buffer_len; i += audio_frame_size * audio_channels) {
                        int amount = pcm_buffer_len - i;
                        if (amount < audio_frame_size * audio_channels) {
                            short *cp = (short*)calloc(sizeof(short), audio_frame_size * audio_channels * 64);
                            memcpy(cp, &pcm_buffer[i], amount * sizeof(short));
                            pcm_buffer_len = amount;
                            free(pcm_buffer);
                            pcm_buffer = cp;
                            break;
                        } else {
                            short *stack_origin = &pcm_buffer[i];
                            input_pcm.bufs = (void**)&stack_origin;
                            args.numInSamples = audio_frame_size * audio_channels;
                            assert(aacEncEncode(aac_encoder, &input_pcm, &output_aac, &args, &out_args) == AACENC_OK);
                            input_pcm.bufs = (void**)0;

                            if (out_args.numOutBytes) {
                                MP4WriteSample(mp4, audio_track, buffer_aac, out_args.numOutBytes, audio_frame_size, 0, true);
                            }
                        }
                    }
                }
            };
            auto write_encoded_video = [&]() {
                ion::array<u8> nalus = h264_encoder.encode(f->image); /// needs a quality setting; this default is very high quality and constant quality too; meant for data science work
                MP4WriteSample(mp4, video_track, nalus.data, nalus.len());
                //printf("writing video packet of %d bytes\n", (int)nalus.len());
                f->audio = {};
                f->video = {};
                assert(!f->audio);
                assert(!f->video);
            };
            auto unlock_frame = [&]() {
                frames++;
                f->mtx.unlock();
            };

            while (!stopped) {
                i64 start = start_write + frames * (1000 / hz);
                i64 t = millis();
                if (start < t) {
                    //printf("warning: took too long to write frames; origin is offset\n");
                } else {
                    i64 w = (start - t);
                    usleep(w * 1000);
                }
                f = current;
                
                wait_for_media();
                if (!stopped) {
                    mtx_write.lock();
                    //printf("writing frame id %d\n", (int)frames);
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
}