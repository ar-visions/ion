#pragma once
#include <mx/mx.hpp>
#include <math/math.hpp>

namespace ion {

template <typename T>
struct color:rgba<T> {
    using  base = ion::vec<T,4>;
    using  data = ion::rgba<T>;

    color(cstr h) {
        size_t sz = strlen(h);
        i32    ir = 0, ig = 0,
               ib = 0, ia = 255;
        if (sz && h[0] == '#') {
            auto nib = [&](char const n) -> i32 const {
                return (n >= '0' && n <= '9') ?       (n - '0')  :
                       (n >= 'a' && n <= 'f') ? (10 + (n - 'a')) :
                       (n >= 'A' && n <= 'F') ? (10 + (n - 'A')) : 0;
            };
            switch (sz) {
                case 5:
                    ia  = nib(h[4]) << 4 | nib(h[4]);
                    [[fallthrough]];
                case 4:
                    ir  = nib(h[1]) << 4 | nib(h[1]);
                    ig  = nib(h[2]) << 4 | nib(h[2]);
                    ib  = nib(h[3]) << 4 | nib(h[3]);
                    break;
                case 9:
                    ia  = nib(h[7]) << 4 | nib(h[8]);
                    [[fallthrough]];
                case 7:
                    ir  = nib(h[1]) << 4 | nib(h[2]);
                    ig  = nib(h[3]) << 4 | nib(h[4]);
                    ib  = nib(h[5]) << 4 | nib(h[6]);
                    break;
            }
        }
        if constexpr (sizeof(T) == 1) {
            base::r = T(ir);
            base::g = T(ig);
            base::b = T(ib);
            base::a = T(ia);
        } else {
            base::r = T(math::round(T(ir) / T(255.0)));
            base::g = T(math::round(T(ig) / T(255.0)));
            base::b = T(math::round(T(ib) / T(255.0)));
            base::a = T(math::round(T(ia) / T(255.0)));
        }
    }

    color(str s) : color(s.data) { }
    
    /// #hexadecimal formatted str
    operator str() {
        char    res[64];
        u8      arr[4];
        ///
        real sc;
        ///
        if constexpr (identical<T, uint8_t>())
            sc = 1.0;
        else
            sc = 255.0;
        ///
        arr[0] = uint8_t(math::round(base::r * sc));
        arr[1] = uint8_t(math::round(base::g * sc));
        arr[2] = uint8_t(math::round(base::b * sc));
        arr[3] = uint8_t(math::round(base::a * sc));
        ///
        res[0]  = '#';
        ///
        for (size_t i = 0, len = sizeof(arr) - (arr[3] == 255); i < len; i++) {
            size_t index = 1 + i * 2;
            snprintf(&res[index], sizeof(res) - index, "%02x", arr[i]); /// secure.
        }
        ///
        return (symbol)res;
    }
};

struct image:array<rgba8> {
    image(memory *m)       : array<rgba8>(m)  { }
    image(size   sz)       : array<rgba8>(sz) { }
    image(null_t n = null) : image(size { 1, 1 })  { }
    image(path p);
    image(size sz, rgba8 *px, int scanline = 0);
    bool save(path p) const;
    rgba8 *pixels() const { return elements; }
    size_t       width() const { return (*mem->shape)[1]; }
    size_t      height() const { return (*mem->shape)[0]; }
    size_t      stride() const { return (*mem->shape)[1]; }
    recti         rect() const { return { 0, 0, int(width()), int(height()) }; }
    rgba8 &operator[](size pos) const {
        size_t index = mem->shape->index_value(pos);
        return elements[index];
    }
};

mx inflate(mx);

/// isolating the types and then designing from there brings the isolated types together
/// i dont want these constructors implied, its a bit too much and a reduction effort should be of value

struct audio:mx {
    using intern = struct iaudio;
    using parent = mx;
    ///
    ptr_declare(audio);
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
