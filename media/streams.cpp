#include <mx/mx.hpp>
#include <media/image.hpp>
#include <media/streams.hpp>

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

void Streams::push(MediaBuffer buffer) {
    Frame  &frame = data->swap[data->frames % 2];
    bool is_video = false;

    if (buffer->type == Media::PCM || buffer->type == Media::PCMf32) {
        frame.audio = buffer;
        data->audio_queued = true;
    } else {
        frame.video = buffer;
        is_video = true;
        data->video_queued = true;
    }
    
    if (is_video && data->resolve_image) {
        u8 *src =      frame.video->bytes;
        u8 *dst = (u8*)frame.image.data;
        frame.image.mem->count   = data->w * data->h;
        frame.image.mem->reserve = frame.image.mem->count;
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
    }
    
    if ((!data->use_video || data->video_queued) && (!data->use_audio || data->audio_queued)) {
        dispatch();
        data->video_queued = false;
        data->audio_queued = false;
    }
}

void Remote::close() {
    Streams::M *streams = (Streams::M*)data->sdata;
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