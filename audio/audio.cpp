#define  MINIMP3_ONLY_MP3
#define  MINIMP3_ONLY_SIMD
//#define  MINIMP3_FLOAT_OUTPUT
#define  MINIMP3_IMPLEMENTATION

#include "minimp3_ex.h"

#include <core/core.hpp>
#include <audio/audio.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gsl/gsl_fft_real.h>
#include <opus/opus.h>
#include <ogg/ogg.h>

namespace ion {
struct iaudio {
    bool             init;
    mp3dec_t         dec;
    mp3dec_ex_t      api;
    i16             *samples;
    u64              sz;
    u64              sz_alloc;
    ogg_sync_state   oy;
    ogg_stream_state os;
    ogg_page         og;
    ogg_packet       op;
    OpusDecoder     *decoder;
    int              version;
    int              channels;
    int              total_samples;

    ~iaudio() { delete[] buf; }
};

/// use this to hide away the data and isolate its dependency
ptr_impl(audio, mx, iaudio, p);

audio::audio(path res, bool force_mono) : audio() {
    str ext = res.ext();

    if (!p->init) {
        mp3dec_init(&p->dec); /// see how often this needs to be done?
        p->init = true;
    }
    int channels = p->api.info.channels;
    cstr  s_path = res.cs();

    if (ext == ".opus") {
        
        /// read packets from pages from ogg frame
        FILE     *f                = fopen(s_path, "rb");
        bool      first            = true;
        int       version          = 0;
        int       channels         = 0;
        const int buffer_size      = 1024 * 16; /// should handle a 60ms pcm as well as the encoded buffer, prior case, multiplied by channels read
        int       pcm_max_samples  = 1024 * 32;
        int       alloc_samples    = pcm_max_samples * channels;
        int       total_samples    = 0;
        i16      *pcm              = new i16[pcm_max_samples * channels];
        i16      *result           = new i16[alloc_samples]; /// start it the same and it will adjust 

        // main loop
        while (!feof(f)) {
            // read data from file and add to sync state
            i8    *buffer = ogg_sync_buffer(&oy, buffer_size);
            size_t bytes  = fread(buffer, 1, buffer_size, f);
            ogg_sync_wrote(&oy, bytes);

            // extract pages and add to stream state
            while (ogg_sync_pageout(&oy, &og) == 1) {
                ogg_stream_pagein(&os, &og);
                // extract packets and decode
                while (ogg_stream_packetout(&os, &op) == 1) {
                    if (first) {
                        /// find the head of opus
                        assert(op.bytes >= 9 && memcmp(op.packet, "OpusHead", 8) == 0);
                        version         = ((u8 *)op.packet)[8];
                        channels        = ((u8 *)op.packet)[9];
                        first           = false;
                    }
                    // samples doesnt take into account channels, stride by channel in copying
                    int samples = opus_decode(decoder, op.packet, op.bytes, pcm, pcm_max_samples, 0);

                    if ((total_samples + samples) > alloc_samples) {
                        while (total_samples + samples > alloc_samples)
                            alloc_samples *= 2;
                        i16    *n_result = new i16[alloc_samples];
                        size_t   n_bytes = total_samples * channels * sizeof(i16);
                        memcpy(n_result, result, n_bytes);
                        delete[] result;
                        result = n_result;
                    }

                    size_t n_bytes = samples * channels * sizeof(i16);
                    memcpy(&result[total_samples * channels], result, n_bytes);
                    total_samples += samples;
                }
            }
        }
        fclose(f);

        p->total_samples = total_samples;
        p->samples = result;

    } else if (ext == ".mp3") {
        assert(!mp3dec_ex_open(&p->api, s_path, MP3D_SEEK_TO_SAMPLE));
        p->total_samples = p->api.samples;
        assert(p->total_samples > 0);
        p->samples = new short[p->total_samples];
        assert(mp3dec_ex_read(&p->api, p->samples, p->api.samples) == p->total_samples);
    }

    /// convert to mono if we feel like it.
    const int   channels = p->api.info.channels;
    if (channels > 1 && force_mono) {
        size_t      n_sz = p->sz / channels;
        short     *n_buf = new short[n_sz];
        for (int       i = 0; i < p->sz; i += channels) {
            float    sum = 0;
            for (int   c = 0; c < channels; c++)
                    sum += p->buf[i + c];
            n_buf[i / channels] = sum / channels;
        }
        delete[] p->buf; /// delete previous data
        p->buf = n_buf; /// set new data 
        p->sz  = n_sz; /// set new size
        p->api.info.channels = 1; /// set to 1 channel
    }
}

/// obtain short waves from skinny aliens
array<short> audio::pcm_data() {
    array<short> res(size_t(p->sz));
    for (int i = 0; i < p->sz; i++)
        res += p->buf[i];
    return res;
}

int           audio::channels() { return  p->api.info.channels; }
audio::operator          bool() { return  p->sz; }
bool         audio::operator!() { return !p->sz; }
size_t            audio::size() { return  p->sz; }
size_t       audio::mono_size() { return  p->sz / p->api.info.channels; }


#define FRAME_SIZE 960
#define SAMPLE_RATE 48000
#define CHANNELS 2

int opus_fft(path p) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s input.opus\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int err;
    OpusDecoder *decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (err < 0) {
        fprintf(stderr, "Failed to create decoder: %s\n", opus_strerror(err));
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int16_t pcm[FRAME_SIZE * CHANNELS];
    float fft_in[FRAME_SIZE];
    double fft_out[FRAME_SIZE / 2 + 1];

    gsl_fft_real_wavetable *real = gsl_fft_real_wavetable_alloc(FRAME_SIZE);
    gsl_fft_real_workspace *work = gsl_fft_real_workspace_alloc(FRAME_SIZE);

    while (1) {
        unsigned char packet[1275];
        int packet_size = fread(packet, 1, sizeof(packet), fp);
        if (packet_size == 0) {
            break;
        }

        int frames_decoded = opus_decode(decoder, packet, packet_size, pcm, FRAME_SIZE, 0);
        if (frames_decoded < 0) {
            fprintf(stderr, "Failed to decode frame: %s\n", opus_strerror(frames_decoded));
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < FRAME_SIZE; i++) {
            fft_in[i] = pcm[i * CHANNELS] / 32768.0;
        }

        gsl_fft_real_transform(fft_in, 1, FRAME_SIZE, real, work);
        for (int i = 0; i < FRAME_SIZE / 2 + 1; i++) {
            fft_out[i] = sqrt(fft_in[i * 2] * fft_in[i * 2] + fft_in[i * 2 + 1] * fft_in[i * 2 + 1]);
        }

        // Do something with the FFT output...
    }

    fclose(fp);
    opus_decoder_destroy(decoder);
    gsl_fft_real_wavetable_free(real);
    gsl_fft_real_workspace_free(work);

    return 0;
}

}
