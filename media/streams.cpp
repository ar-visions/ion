#include <mx/mx.hpp>
#include <media/image.hpp>
#include <media/streams.hpp>
#include <speex/speex_resampler.h>

#include <assert.h>

namespace ion {

int clamp(int v) {
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

/// resolve rgba from yuy2
void yuy2_rgba(u8* yuy2, u8* rgba, int width, int height) {
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x += 2) {
            // Extract YUY2 components
            u8 Y0 = yuy2[0];
            u8 U  = yuy2[1];
            u8 Y1 = yuy2[2];
            u8 V  = yuy2[3];

            // Convert YUV to RGB
            int C = Y0 - 16;
            int D = U - 128;
            int E = V - 128;

            rgba[0] = (u8)clamp(( 298 * C + 409 * E + 128) >> 8); // R0
            rgba[1] = (u8)clamp(( 298 * C - 100 * D - 208 * E + 128) >> 8); // G0
            rgba[2] = (u8)clamp(( 298 * C + 516 * D + 128) >> 8); // B0
            rgba[3] = 255; // A0

            C = Y1 - 16;

            rgba[4] = (u8)clamp(( 298 * C + 409 * E + 128) >> 8); // R1
            rgba[5] = (u8)clamp(( 298 * C - 100 * D - 208 * E + 128) >> 8); // G1
            rgba[6] = (u8)clamp(( 298 * C + 516 * D + 128) >> 8); // B1
            rgba[7] = 255; // A1

            yuy2 += 4;
            rgba += 8;
        }
}

/// resolve rgba from nv12
void nv12_rgba(const uint8_t* nv12, uint8_t* rgba, int width, int height) {
    const uint8_t* Y = nv12;
    const uint8_t* UV = nv12 + (width * height);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            int Yindex = y * width + x;
            int UVindex = (y / 2) * width + (x & ~1);

            // Y component
            int Yvalue = Y[Yindex] - 16;

            // UV components
            int U = UV[UVindex] - 128;
            int V = UV[UVindex + 1] - 128;

            // Set RGBA values
            rgba8 *dst = (rgba8*)(rgba + 4 * Yindex);
            dst->r = clamp((298 * Yvalue + 409 * V + 128) >> 8);
            dst->g = clamp((298 * Yvalue - 100 * U - 208 * V + 128) >> 8);
            dst->b = clamp((298 * Yvalue + 516 * U + 128) >> 8);
            dst->a = 255;
        }
}

MediaBuffer MediaBuffer::convert_pcm(PCMInfo &pcm_to, int id) {
    PCMInfo &pcm = data->pcm;
    if (pcm_to->format   == pcm->format   &&
        pcm_to->channels == pcm->channels &&
        pcm_to->samples  == pcm->samples)
        return MediaBuffer(pcm, data->buf, id);
    
    array<float> combined(sz_t(pcm->frame_samples * 2)); /// must be reduced to float32 so resampling can take place after
    sz_t         combined_samples = 0;

    if (pcm->format == Media::PCMf32) {
        float *input = data->buf.origin<float>();
        if (pcm_to->channels == pcm->channels) {
            combined_samples = pcm->frame_samples;
            for (int i = 0; i < pcm->frame_samples; i++)
                combined[i] = input[i];
        } else if (pcm->channels == 1 && pcm_to->channels == 2) {
            combined_samples = pcm->frame_samples * 2;
            for (int i = 0; i < pcm->frame_samples; i++) {
                float cv = input[i];
                combined[i * 2 + 0] = cv;
                combined[i * 2 + 1] = cv;
            }
        } else if (pcm_to->channels == 1) {
            combined_samples = pcm->frame_samples / 2;
            for (sz_t i = 0; i < combined_samples; i++) {
                combined[i] =
                    (float)((input[i * 2 + 0] + 
                             input[i * 2 + 1]) / 2.0f);
            }
        } else
            assert(false);
        
    } else {
        short *input = data->buf.origin<short>();
        if (pcm_to->channels == pcm->channels) {
            combined_samples = pcm->frame_samples;
            for (int i = 0; i < pcm->frame_samples; i++)
                combined[i] = std::max(input[i] / 32767.0f, -1.0f);
        } else if (pcm->channels == 1 && pcm_to->channels == 2) {
            combined_samples = pcm->frame_samples * 2;
            for (int i = 0; i < pcm->frame_samples; i++) {
                float cv = std::max(input[i] / 32767.0f, -1.0f);
                combined[i * 2 + 0] = cv;
                combined[i * 2 + 1] = cv;
            }
        } else if (pcm_to->channels == 1) {
            combined_samples = pcm->frame_samples / 2;
            for (sz_t i = 0; i < combined_samples; i++) {
                /// frame size is channel-2 based on this struct
                combined[i] = std::max(
                    (float)((input[i * 2 + 0] / 32767.0f + 
                             input[i * 2 + 1] / 32767.0f) / 2.0f), -1.0f);
            }
        } else
            assert(false);
    }
    combined.set_size(combined_samples);

    /// if we are requesting f32 and the sample rate does not change, we return this as a MediaBuffer
    if (pcm_to->format == Media::PCMf32 && pcm_to->samples == pcm->samples)
        return MediaBuffer(pcm_to, combined, id);

    /// channels are pcm_to, but rate is different
    array<float> res;
    if (pcm_to->samples != pcm->samples) {
        array<float> resampled = array<float>(pcm_to->samples);
        int err;
        SpeexResamplerState *resampler = speex_resampler_init(
            pcm_to->channels, pcm->samples, pcm_to->samples, SPEEX_RESAMPLER_QUALITY_DEFAULT, &err);
        assert(err == RESAMPLER_ERR_SUCCESS);
        u32    in_len  = combined.len()      / pcm_to->channels;
        u32    out_len = resampled.reserve() / pcm_to->channels;
        speex_resampler_process_interleaved_float(resampler, combined.data, &in_len, resampled.data, &out_len);
        speex_resampler_destroy(resampler);
        res = resampled;
        res.set_size(out_len);
    } else {
        res = combined;
    }

    /// going to float, no need to convert
    if (pcm_to->format == Media::PCMf32)
        return MediaBuffer(pcm_to, res, id);

    /// convert final
    sz_t sz = res.len();
    array<short> conv(sz);
    for (sz_t i = 0; i < sz; i++)
        conv[i] = (short)(res[i] * 32767.0f);
    
    conv.set_size(sz);
    ///
    return MediaBuffer(pcm_to, conv, id);
}

static void set_pcminfo(PCMInfo &pcm, Media format, int channels, int samples) {
    int bits_per_sample = format == Media::PCM ? 16 : 32;
    int nb = bits_per_sample / 8;

    pcm->format          = format;
    pcm->samples         = samples;
    pcm->channels        = channels;
    pcm->frame_samples   = samples * channels;
    pcm->bytes_per_frame = pcm->frame_samples * nb;
    pcm->audio_buffer    = array<u8>(size_t(pcm->bytes_per_frame) * 16);
}

void MStream::init_input_pcm(Media format, int channels, int samples) {
    set_pcminfo(data->pcm_input, format, channels, samples);
}

void MStream::init_output_pcm(Media format, int channels, int samples) {
    set_pcminfo(data->pcm_output, format, channels, samples);
}

array<MediaBuffer> MStream::audio_packets(u8 *pData, int currentLength) {
    PCMInfo &pcm_o = data->pcm_output;
    PCMInfo &pcm_i = data->pcm_input;
    /// audio method is to perform some flow control and conversion (this code should be in PCMState or so)
    int reserve    = pcm_i->audio_buffer.reserve();
    int cur_sz     = pcm_i->audio_buffer.count();
    printf("cur sz = %d\n", cur_sz);
    u8*        src = (u8*)pcm_i->audio_buffer.origin<u8>(); 
    array<MediaBuffer> res;

    memcpy(&src[cur_sz], pData, currentLength);
    cur_sz += currentLength;
    pcm_i->audio_buffer.set_size(cur_sz);
    
    int rpos = 0;
    while (cur_sz >= pcm_i->bytes_per_frame) {
        mx buf;
        if (pcm_i->format == Media::PCM) {
            array<short> sbuf(pcm_i->bytes_per_frame / sizeof(short));
            memcpy(sbuf.data, &src[rpos], pcm_i->bytes_per_frame);
            buf = sbuf;
        } else {
            array<float> fbuf(pcm_i->bytes_per_frame / sizeof(float)); /// 16000 * 2 * 4 = 128,0000 samples / 30 = 4266
            memcpy(fbuf.data, &src[rpos], pcm_i->bytes_per_frame);
            buf = fbuf;
        }
        MediaBuffer input_unconv = MediaBuffer(pcm_i, buf, 0);
        MediaBuffer conv = input_unconv.convert_pcm(pcm_o, 0);
        static int s_id = 0;
        conv->id = s_id++; // test code to verify audio is coming in, in sequence; never left there, etc
        res += conv;
        cur_sz     -= pcm_i->bytes_per_frame;
        rpos       += pcm_i->bytes_per_frame;
    }

    array<u8> remaining(pcm_i->audio_buffer.reserve());
    memcpy(remaining.data, &src[rpos], cur_sz);
    remaining.set_size(cur_sz);
    pcm_i->audio_buffer = remaining;
    return res; /// may be default state, a falsey object
}

bool MStream::push(MediaBuffer buffer) {
    if (data->stop || data->error || !data->ready)
        return false;
    Frame  &frame = data->main_frame; //data->swap[data->frames % 4];
    frame.mtx.lock();
    bool is_video = false;
    if (buffer->type == Media::PCM || buffer->type == Media::PCMf32) {
        /// convert if this PCM format doesnt equal the priority #1
        for (;;) {
            if (!frame.audio)
                break;
            frame.mtx.unlock();
            usleep(10);
            frame.mtx.lock();
        }

        assert(buffer->id == data->audio_next_id);

        if (buffer->type == Media::PCMf32) {
            int  n_floats = buffer->buf.total_size() / sizeof(float);
            float *floats = buffer->buf.origin<float>();
            array<short> shorts(n_floats);
            for (int i = 0; i < n_floats; i++) {
                shorts[i] = floats[i] * 32767.0;
            }
            shorts.set_size(n_floats);
            frame.audio = MediaBuffer(Media::PCM, shorts, buffer->id);
        } else
            frame.audio = buffer;
        
        data->audio_next_id = buffer->id + 1;
        data->audio_queued = true;
    } else {
        for (;;) {
            if (!frame.video) /// not operator was not working because the bool was not implemented in the data struct on MediaBuffer.
                break;
            frame.mtx.unlock();
            usleep(10);
            frame.mtx.lock();
        }
        frame.video = buffer;
        assert(buffer->id == data->video_next_id);
        data->video_next_id = buffer->id + 1;
        bool is_null    = frame.video->buf.type() == typeof(null_t);
        bool has_buffer = !is_null && frame.video->buf.count() > 0;
        assert (has_buffer);
        is_video = true;
        data->video_queued = true;
    }
    if (is_video && data->resolve_image) {
        u8 *src = (u8*)frame.video->buf.mem->origin;
        u8 *dst = (u8*)frame.image.data;
        frame.image.mem->count   = data->w * data->h;
        frame.image.mem->reserve = frame.image.mem->count;
        i64 a = millis();
        switch (frame.video->type.value) {
            case Media::YUY2:
                yuy2_rgba(src, dst, data->w, data->h);
                break;
            case Media::NV12:
                nv12_rgba(src, dst, data->w, data->h);
                break;
            case Media::MJPEG:
                assert(false);
                break;
            default:
                assert(false);
                break; 
        }
        i64 b = millis();
        i64 c = b - a;
        //printf("[streams] conversion took %d millis\n", (int)c);
    }
    bool run_dispatch = false;
    if ((!data->use_video || data->video_queued) && (!data->use_audio || data->audio_queued)) {
        run_dispatch = true;
        data->video_queued = false;
        data->audio_queued = false;
    }
    printf("[streams] pushed to stream\n");
    frame.mtx.unlock();
    if (run_dispatch)
        dispatch();
    return true;
}

void Remote::close() {
    MStream::M *streams = (MStream::M*)data->sdata;
    raw_t       key     = mem->origin;

    streams->mtx.lock();
    num i = 0, index = -1;
    for (mx &r: streams->listeners) {
        if (r.mem->origin == key) {
            index = i;
            break;
        }
        i++;
    }
    assert(index >= 0);
    if (index >= 0)
        streams->listeners->remove(index);
    streams->mtx.unlock();
}

}