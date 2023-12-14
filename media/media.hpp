#pragma once
#include <mx/mx.hpp>
#include <media/image.hpp>
#include <async/async.hpp>

struct iaudio;
namespace ion {

mx inflate(mx);


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
        operator bool() { return samples > 0; }
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

    MediaBuffer(Media type, mx &buf, int id):MediaBuffer() {
        data->type = type;
        if (buf.type() == typeof(float)) {
            assert(type == Media::PCMf32);
        } else if (buf.type() == typeof(short)) {
            assert(type == Media::PCM);
        } else if (buf.type() == typeof(u8)) {
            assert(type == Media::YUY2);
        } else
            assert(false);
        data->buf  = buf;
        data->id   = id;
    }

    MediaBuffer(PCMInfo &pcm, mx buf, int id) : MediaBuffer() {
        if (buf.type() == typeof(u8)) {
            assert(false);
        }
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

struct iaudio {
    ion::i32    sample_rate; /// hz
    ion::i16   *samples;
    ion::i8     channels;
    ion::i64    total_samples;
    ion::str    artist;
    ion::str    title;
    
    static i16 *alloc_pcm(int samples, int channels) {
        const int pad = 2048; /// mp3/aac never goes over this size
        sz_t sz = samples * channels + pad;
        i16 *pcm = new i16[sz];
        memset(pcm, 0, sz * sizeof(i16));
        return pcm;
    }

    ~iaudio()       { delete[] samples; }
    operator bool() { return samples; }

    register(iaudio);
};

struct audio:mx {
    mx_declare(audio, mx, iaudio);
    ///
    audio(path res, bool force_mono = false);
    audio(MediaBuffer buf);

    void       convert_mono();
    array<short>   pcm_data();
    int            channels();
    bool               save(ion::path dest, i64 bitrate = 128000);
    ion::image    fft_image(ion::size);
    operator           bool();
    bool          operator!();
    size_t             size();
    size_t        mono_size();
    static audio random_noise(i64 millis, int channels, int sample_rate);
protected:
    bool           save_m4a(path dest, i64 bitrate);
};

}
