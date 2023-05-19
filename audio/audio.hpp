#pragma once
#include <core/core.hpp>
#ifndef MINIMP3_IMPLEMENTATION
struct mp3dec_ex_t;
#endif

namespace ion {
struct audio:mx {
protected:
    struct iaudio *p;
public:
    ptr_decl(audio, mx, iaudio, p);
    audio(path res, bool force_mono = false);
    void      convert_mono();
    array<short>  pcm_data();
    int           channels();
    bool              save(path dest, i64 bitrate = 64000);
    operator          bool();
    bool         operator!();
    size_t            size();
    size_t       mono_size();
};
}
