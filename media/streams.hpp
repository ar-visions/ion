#pragma once
#include <mx/mx.hpp>
#include <media/media.hpp>
#include <media/image.hpp>
#include <async/async.hpp>
#include <cassert>

namespace ion {

/// some users only need access to video frames (as Image)
/// and others want to do something with the encoded Video data
enums(StreamType, undefined,
      undefined, Audio, Image, Video);

struct Frame {
    u64         from, to;
    MediaBuffer video;
    MediaBuffer audio;
    ion::image  image;
    mutex       mtx;
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
        Frame                main_frame;
        array<StreamType>    stream_types;
        array<Media>         media;
        doubly<Remote>       listeners;
        u64                  frames;
        int                  channels; /// number of audio channels contained (float32 format!)
        bool                 ready;
        bool                 stop;
        bool                 error;
        int                  w, h, hz;
        bool                 use_audio;
        bool                 use_video;
        bool                 resolve_image;
        bool                 audio_queued;
        bool                 video_queued;
        PCMInfo              pcm_input;
        PCMInfo              pcm_output;
        int                  video_next_id;
        int                  audio_next_id;
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
        if (data->resolve_image) {
            size sz { h, w };
            rgba8 *bytes = (rgba8*)calloc(sizeof(rgba8), sz);
            data->main_frame.image = image(sz, bytes, 0);
        }
    }

    bool push(MediaBuffer buffer);
    void dispatch() {
        Frame &frame = data->main_frame;
        for (Remote &listener: data->listeners)
            listener->callback(frame);
    }

    void ready()  { data->ready = true; }
    void stop()   { data->stop  = true; data->rt->stop = true; }
    void cancel() { data->error = true; data->rt->stop = true; }

    MStream &await_ready() {
        while (!data->ready)
            yield();
        return *this;
    }

    void init_input_pcm (Media format, int channels, int samples);
    void init_output_pcm(Media format, int channels, int samples);

    array<MediaBuffer> audio_packets(u8 *buffer, int len);
        /// frameless pcm, MStreams will reframe and resample for the user

    bool contains(StreamType type) { return data->stream_types.index_of(type) >= 0; }
    
    mx_basic(MStream);
};

}