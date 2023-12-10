#pragma once
#include <media/image.hpp>
#include <async/async.hpp>

namespace ion {

/// some users only need access to video frames (as Image)
/// and others want to do something with the encoded Video data
enums(StreamType, undefined,
      undefined, Audio, Image, Video);

/// todo: convert to types, removing enum
/// h264 is nothing more than a nalu buffer sequence
/// mjpeg should remain the bytes encoded
/// it just seems a better more open system, using registered types
enums(Media, undefined,
      undefined, PCM, PCMf32, PCMu32, YUY2, NV12, MJPEG, H264);

struct PCMInfo:mx {
    struct M {
        Media               format;
        sz_t                frame_samples;
        int                 bytes_per_frame;
        u32                 samples;
        u32                 channels;
        mx                  audio_buffer;
        register(M);
    };
    mx_basic(PCMInfo);
};

struct MediaBuffer:mx {
    struct M {
        PCMInfo  pcm;
        Media    type;
        mx       buf;
        int      id;
        operator bool() { return buf.mem && bool(buf); }
        register(M)
    };
    mx_basic(MediaBuffer);

    MediaBuffer(Media type, array<u8> buf, int id):MediaBuffer() {
        data->type = type;
        data->buf  = buf;
        data->id   = id;
    }

    MediaBuffer(PCMInfo &pcm, mx buf, int id) : MediaBuffer() {
        data->pcm  = pcm;
        data->type = pcm->format;
        data->buf  = buf;
        data->id   = id;
    }

    /// hand-off constructor
    MediaBuffer(PCMInfo &pcm) : MediaBuffer() 
    {
        data->pcm  = pcm;
        data->type = pcm->format;
        data->buf  = pcm->audio_buffer; /// no copy
        pcm->audio_buffer = array<u8>(0, data->buf.reserve()); /// recycle potential
    }

    /// for audio; we could have others with different arguments for video
    /// or we can name it convert_pcm
    MediaBuffer convert_pcm(PCMInfo &pcm_to, int id);
};



struct PCMu32 : MediaBuffer { PCMu32(array<u8> buf) : MediaBuffer(Media::PCMu32, buf, 0) { } };
struct PCMf32 : MediaBuffer { PCMf32(array<u8> buf) : MediaBuffer(Media::PCMf32, buf, 0) { } };
struct PCM    : MediaBuffer { PCM   (array<u8> buf) : MediaBuffer(Media::PCM,    buf, 0) { } };
struct YUY2   : MediaBuffer { YUY2  (array<u8> buf) : MediaBuffer(Media::YUY2,   buf, 0) { } };
struct NV12   : MediaBuffer { NV12  (array<u8> buf) : MediaBuffer(Media::NV12,   buf, 0) { } };
struct MJPEG  : MediaBuffer { MJPEG (array<u8> buf) : MediaBuffer(Media::MJPEG,  buf, 0) { } };
struct H264   : MediaBuffer { H264  (array<u8> buf) : MediaBuffer(Media::H264,   buf, 0) { } };

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