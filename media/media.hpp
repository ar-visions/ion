#pragma once
#include <mx/mx.hpp>
#include <image/image.hpp>
#include <async/async.hpp>

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
    bool              save(ion::path dest, i64 bitrate = 64000);
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

    /// the call is async if there is no input, because that means frames are pushed independent of h264e, it waits for new frames as a result
    h264e(lambda<bool(mx)> output, lambda<yuv420(i64)> input = {}); /// the stream ends on null image pushed or given in this input
    void push(yuv420 frame);
    async run();
};

}
