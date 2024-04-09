#pragma once
#include <mx/mx.hpp>
#include <media/image.hpp>
#include <async/async.hpp>

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
    };
    mx_basic(MediaBuffer);

    MediaBuffer(Media type, const mx &buf, int id);

    MediaBuffer(PCMInfo &pcm, const mx &buf, int id);

    /// hand-off constructor
    MediaBuffer(PCMInfo &pcm);

    /// for audio; we could have others with different arguments for video
    /// or we can name it convert_pcm
    MediaBuffer convert_pcm(PCMInfo &pcm_to, int id);
};

struct PCMu32 : MediaBuffer { PCMu32(const array &buf); };
struct PCMf32 : MediaBuffer { PCMf32(const array &buf); };
struct PCM    : MediaBuffer { PCM   (const array &buf); };
struct YUY2   : MediaBuffer { YUY2  (const array &buf); };
struct NV12   : MediaBuffer { NV12  (const array &buf); };
struct MJPEG  : MediaBuffer { MJPEG (const array &buf); };
struct H264   : MediaBuffer { H264  (const array &buf); };

struct iaudio;

struct audio:mx {
    
    mx_declare(audio, mx, iaudio)

    ///
    audio(path res, bool force_mono = false);
    audio(MediaBuffer buf);

    void       convert_mono();
    array          pcm_data();
    int            channels();
    bool               save(ion::path dest, i64 bitrate = 128000);
    ion::image    fft_image(ion::size);
    operator           bool();
    bool          operator!();
    sz_t               size();
    sz_t      total_samples();
    sz_t        sample_rate();
    sz_t          mono_size();
    i16*            samples();
    static audio random_noise(i64 millis, int channels, int sample_rate);
protected:
    bool           save_m4a(path dest, i64 bitrate);
};

}
