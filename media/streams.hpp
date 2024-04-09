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
    ion::image  image;
    mx          video;
    mx          audio;
    mutex       mtx;
    void lock() {
        mtx.lock();
    }
    void unlock() {
        mtx.unlock();
    }
};

struct Remote:mx {
    struct M {
        raw_t sdata;
        lambda<void(Frame&)> callback;
    };
    void close();
    mx_basic(Remote);
};

struct MStream:mx {
    struct M {
        runtime*             rt;
        mutex                mtx_;
        Array<StreamType>    stream_types;
        Array<Media>         media;
        Media                video_format;
        doubly               listeners; // Remote
        u64                  frames;
        int                  channels; /// number of audio channels contained (float32 format!)
        bool                 ready;
        bool                 stop;
        bool                 error;
        int                  w, h, hz;
        doubly               audio_queue; /// mx: a user need only know frame for the video, but then access audio from here; storage is not kept but its far easier to keep this integral
        mx                   video;
        i64                  start_time;
        bool                 use_audio;
        bool                 use_video;
        bool                 resolve_image;
        ion::image           image;
        PCMInfo              pcm_input;
        PCMInfo              pcm_output;
        int                  video_next_id;
        int                  audio_next_id;
    };

    MStream(Array<StreamType> stream_types, Array<Media> media, lambda<void(MStream)> fn):MStream() {
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

    void set_video_format(Media format);

    void start();

    /// Streams SHOULD offer a frame of reference
    Remote listen(lambda<void(Frame&)> callback);

    void set_info(int w, int h, int hz, int channels);

    bool push_audio(mx buffer);
    bool push_video(mx buffer);
    void dispatch();

    void ready()  { data->ready = true; start(); } /// we start dispatching at the ready
    void stop()   { data->stop  = true; data->rt->stop = true; }
    void cancel() { data->error = true; data->rt->stop = true; }

    MStream &await_ready() {
        while (!data->ready)
            yield();
        return *this;
    }

    void init_input_pcm (Media format, int channels, int samples);
    void init_output_pcm(Media format, int channels, int samples);

    //Array<MediaBuffer> audio_packets(u8 *buffer, int len);
        /// frameless pcm, MStreams will reframe and resample for the user

    bool contains(StreamType type) { return data->stream_types.index_of(type) >= 0; }
    
    mx_basic(MStream);
};

}