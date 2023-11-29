#pragma once
#include <media/image.hpp>
#include <async/async.hpp>

namespace ion {

/// some users only need access to video frames (as Image)
/// and others want to do something with the encoded Video data
enums(StreamType, undefined,
      undefined, Audio, Image, Video);

enums(Media, undefined,
      undefined, PCM, PCMf32, YUY2, NV12, MJPEG, H264);

struct MediaBuffer:mx {
    struct M {
        Media    type;
        u8      *bytes;
        sz_t     sz;
        ~M() {
            free(bytes);
        }
        register(M)
    };
    mx_basic(MediaBuffer);

    MediaBuffer(Media type, u8 *bytes, sz_t sz):MediaBuffer() {
        data->type  = type;
        data->bytes = bytes;
        data->sz    = sz;
    }
    operator bool() { return data->sz; }
};

struct PCMf32 : MediaBuffer { PCMf32(u8 *bytes, sz_t sz) : MediaBuffer(Media::PCMf32, bytes, sz) { } };
struct PCM    : MediaBuffer { PCM   (u8 *bytes, sz_t sz) : MediaBuffer(Media::PCM,    bytes, sz) { } };
struct YUY2   : MediaBuffer { YUY2  (u8 *bytes, sz_t sz) : MediaBuffer(Media::YUY2,   bytes, sz) { } };
struct NV12   : MediaBuffer { NV12  (u8 *bytes, sz_t sz) : MediaBuffer(Media::NV12,   bytes, sz) { } };
struct MJPEG  : MediaBuffer { MJPEG (u8 *bytes, sz_t sz) : MediaBuffer(Media::MJPEG,  bytes, sz) { } };
struct H264   : MediaBuffer { H264  (u8 *bytes, sz_t sz) : MediaBuffer(Media::H264,   bytes, sz) { } };

struct Frame {
    u64         from, to;
    MediaBuffer video;
    MediaBuffer audio;
    ion::image  image;
};

struct Remote:mx {
    struct M {
        raw_t sdata;
        lambda<void(Frame&)> callback;
        register(M)
    };
    void close();
    mx_basic(Remote);
};

struct MStream:mx {
    struct M {
        runtime*             rt;
        mutex                mtx;
        Frame                swap[2];
        array<StreamType>    stream_types;
        array<Media>         media;
        doubly<Remote>       listeners;
        u64                  frames;
        int                  channels; /// number of audio channels contained (float32 format!)
        bool                 ready;
        bool                 error;
        int                  w, h, hz;
        bool                 use_audio;
        bool                 use_video;
        bool                 resolve_image;
        bool                 audio_queued;
        bool                 video_queued;
        register(M)
    };

    MStream(array<StreamType> stream_types, array<Media> media, lambda<void(MStream)> fn):MStream() {
        data->stream_types   = stream_types;
        data->media          = media;
        StreamType audio_t   = StreamType::Audio;
        StreamType video_t   = StreamType::Video;
        StreamType image_t   = StreamType::Image;
        data->use_audio      = stream_types.index_of(audio_t) >= 0; /// get audio working; render an audiogram strip at the bottom
        data->use_video      = stream_types.index_of(video_t) >= 0;
        data->resolve_image  = stream_types.index_of(image_t) >= 0;

        async([&](runtime *rt, int i) -> mx {
            data->rt = rt;
            fn(*this);
            return true;
        });
        await_ready();
    }

    Remote listen(lambda<void(Frame&)> callback) {
        data->mtx.lock();
        Remote remote = Remote::M { (raw_t)data, callback };
        data->listeners += remote;
        data->mtx.unlock();
        return remote;
    }

    void set_info(int w, int h, int hz, int channels) {
        data->w  = w;
        data->h  = h;
        data->hz = hz;
        data->channels = channels;
        if (data->resolve_image)
            for (num i = 0; i < 2; i++) {
                size sz { h, w };
                rgba8 *bytes = (rgba8*)calloc(sizeof(rgba8), sz);
                data->swap[i].image = image(sz, bytes, 0);
            }
    }

    void push(MediaBuffer buffer);
    void dispatch() {
        Frame &frame = data->swap[data->frames++ % 2];
        for (Remote &listener: data->listeners)
            listener->callback(frame);
    }

    void ready()  { data->ready = true; }
    void cancel() { data->error = true; }

    MStream &await_ready() {
        while (!data->ready)
            yield();
        return *this;
    }

    bool contains(StreamType type) { return data->stream_types.index_of(type) >= 0; }
    
    mx_basic(MStream);
};

}