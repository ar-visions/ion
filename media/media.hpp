#pragma once
#include <mx/mx.hpp>
#include <image/image.hpp>

namespace ion {

mx inflate(mx);

struct iaudio {
    ion::i32    sample_rate; /// hz
    ion::i16   *samples;
    ion::i8     channels;
    ion::i64    total_samples;
    ion::str    artist;
    ion::str    title;
    ~iaudio() { delete[] samples; }
};

struct audio:mx {
    mx_declare(audio, mx, iaudio);
    ///
    audio(path res, bool force_mono = false);
    void      convert_mono();
    array<short>  pcm_data();
    int           channels();
    bool              save(path dest, i64 bitrate = 64000);
    ion::image    fft_image(ion::size);
    operator          bool();
    bool         operator!();
    size_t            size();
    size_t       mono_size();
};

/// interface for minih264e
struct i264e;
struct h264e:mx {
    mx_declare(h264e, mx, i264e);
    ///
    h264e(lambda<image(i64)> fetch, lambda<void(mx)> data); /// the stream ends on null image
};

}
