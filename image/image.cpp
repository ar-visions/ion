#define _CRT_SECURE_NO_WARNINGS
#include <zlib.h>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <image/stb-image-read.h>
#include <image/stb-image-write.h>
#include <core/core.hpp>
#include <math/math.hpp>
#include <image/image.hpp>

namespace ion {

/// load an image into 32bit rgba format
image::image(size sz, rgba::data *px, int scanline) : array() {
    mem->shape  = new size(sz);
    mem->origin = px;
    elements    = px;
    assert(scanline == 0 || scanline == sz[1]);
}

/// save image, just png out but it could look at extensions too
bool image::save(path p) const {
    assert(mem->shape && mem->shape->dims() == 2);
    int w = int(width()),
    h = int(height());
    return stbi_write_png(p.cs(), w, h, 4, elements, w * 4);
}

/// load an image into 32bit rgba format
image::image(path p) : array() {
    int w = 0, h = 0, c = 0;
    /// if path exists and we can read read an image, set the size
    rgba::data *data = null;
    if (p.exists() && (data = (rgba::data *)stbi_load(p.cs(), &w, &h, &c, 4))) {
        mem->count  = h * w;
        mem->shape  = new size { h, w };
        mem->origin = data; /// you can set whatever you want here; its freed at end of life-cycle for memory
        elements    = data;
    }
}

/// inflate
mx inflate(mx input) {
    size_t sz = input.byte_len();
    array<char> out(sz * 4);
    
    // setup zlib inflate stream
    z_stream stream {
        .zalloc   = Z_NULL,
        .zfree    = Z_NULL,
        .opaque   = Z_NULL,
        .avail_in = 0,
        .next_in  = Z_NULL
    };

    if (::inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        console.fault("inflate failure: init");
        return {};
    }

    u8 *src = input.data<u8>();
    stream.avail_in = u32(sz);
    stream.next_in  = src;
    const size_t CHUNK_SIZE = 1024;
    char buf[CHUNK_SIZE];
    do {
        stream.avail_out = CHUNK_SIZE;
        stream.next_out  = (Bytef*)buf;
        int res = ::inflate(&stream, Z_NO_FLUSH);  // inflate the data
        ///
        if (res == Z_NEED_DICT  ||
            res == Z_DATA_ERROR ||
            res == Z_MEM_ERROR) {
            ::inflateEnd(&stream);
            console.fault("inflate failure: {0}", int(res));
            return {};
        }
        ///
        size_t inflated_sz = CHUNK_SIZE - stream.avail_out;
        out.push(buf, inflated_sz);
        ///
    } while (stream.avail_out == 0);
    ///
    ::inflateEnd(&stream);
    return out;
}

}
