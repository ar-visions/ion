#pragma once
#include <mx/mx.hpp>
#include <image/image.hpp>

namespace ion {

mx inflate(mx);

/// isolating the types and then designing from there brings the isolated types together
/// i dont want these constructors implied, its a bit too much and a reduction effort should be of value
struct audio:mx {
    mx_declare(audio, mx, struct iaudio);
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

}
