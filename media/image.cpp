#include <media/image.hpp>
#include <png.h>
#include <yuv_rgb.h>

namespace ion {

yuv420::yuv420(image img) : 
        array(typeof(u8), size_t(img.width() * img.height() +
                         img.width() * img.height() / 4 +
                         img.width() * img.height() / 4),
                  size_t(img.width() * img.height() +
                         img.width() * img.height() / 4 +
                         img.width() * img.height() / 4)) {
    u8 *data = &array::get<u8>(0);
    w  = img.width();
    h  = img.height();
    y  = data;
    u  = &data[w * h];
    v  = &data[w * h + (w/2 * h/2)];
    sz = w * h * 3 / 2;
    rgb32_yuv420(
        w, h, (u8*)img.data, 4 * w, y, u, v, w, (w+1)/2, YCBCR_JPEG);
}

yuv420::operator bool() {
    return mem && mem->count;
}

bool yuv420::operator!() {
    return !mem || !mem->count;
}

yuv420::yuv420() : array(typeof(u8), 0, 1) { }
size_t      yuv420:: width() const { return w; }
size_t      yuv420::height() const { return h; }


image::image(size  sz) : array(typeof(rgba8), sz) {
    mem->count = mem->reserve;
}
image::image(null_t n) : image() { }
image::image(cstr   s) : image(path(s)) { }

///
rgba8  *image::    pixels() const { return array::data<rgba8>(); }
size_t  image::     width() const { return (mem && mem->shape) ? (*mem->shape)[1] : 0; }
size_t  image::    height() const { return (mem && mem->shape) ? (*mem->shape)[0] : 0; }
size_t  image::    stride() const { return (mem && mem->shape) ? (*mem->shape)[1] : 0; }
rect    image::      rect() const { return { 0.0, 0.0, double(width()), double(height()) }; }
vec2i   image::        sz() const { return { int(width()), int(height()) }; }
///
rgba8 &image::operator[](ion::size pos) const {
    size_t index = mem->shape->index_value(pos);
    return array::data<rgba8>()[index];
}

/// load an image into 32bit rgba format
image::image(ion::size sz, rgba8 *px, int scanline) : array() { // scanline not stored atm; if we make use of this we need to store it and use it
    mem->shape  = new ion::size(sz);
    mem->origin = px ? px : (rgba8*)calloc(sizeof(rgba8), sz.area());
    data        = (rgba8*)mem->origin;
    assert(scanline == 0 || scanline == sz[1]);
}

/// save image, just png out but it could look at extensions too
bool image::save(path p) const {
    assert(mem->shape && mem->shape->dims() == 2);
    int w = int(width());
    int h = int(height());
    FILE* file = fopen(p.data->p.string().c_str(), "wb");
    if (!file) {
        return false; // Failed to open the file
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(file);
        return false; // Failed to create PNG write structure
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(file);
        return false; // Failed to create PNG info structure
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(file);
        return false; // Error occurred during PNG write
    }

    png_init_io(png, file);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    const u8** rows = new const u8*[h];
    for (int y = 0; y < h; ++y) {
        rows[y] = (u8*)&data[y * w];
    }

    png_write_image(png, const_cast<u8**>(rows));
    png_write_end(png, NULL);

    delete[] rows;
    png_destroy_write_struct(&png, &info);
    fclose(file);
    return true;
}

image::image(path p) : array() {
    if (!p.exists())
        return;
    
    int w = 0, h = 0;
    png_bytep* rows = nullptr;
        
    FILE* file = fopen(p.cs(), "rb");
    if (!file)
        return;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        /// Handle libpng read struct creation error
        fclose(file);
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        /// Handle png info struct creation error
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(file);
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        /// Handle libpng error during read
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return;
    }

    png_init_io(png, file);
    png_read_info(png, info);

    w = png_get_image_width(png, info);
    h = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    /// Convert to 32-bit RGBA if necessary
    if (bit_depth == 16)
        png_set_strip_16(png);
    
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    
    if (color_type == PNG_COLOR_TYPE_RGB  ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    /// Allocate memory for image data
    rows = new png_bytep[h];
    for (int y = 0; y < h; ++y) {
        rows[y] = new png_byte[png_get_rowbytes(png, info)];
    }

    /// Read the image rows
    png_read_image(png, rows);

    /// Create array to hold image data
    mem->count = h * w;
    mem->shape = new ion::size { h, w };
    data = new rgba8[mem->count];

    /// Copy image data to elements array
    for (int y = 0; y < h; ++y) {
        png_bytep row = rows[y];
        for (int x = 0; x < w; ++x) {
            png_bytep px = &(row[x * 4]);
            data[y * w + x] = rgba8 { px[0], px[1], px[2], px[3] };
        }
    }
    /// Clean up and free resources
    for (int y = 0; y < h; ++y) {
        delete[] rows[y];
    }
    delete[] rows;
    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);
    mem->origin = data;
}


Rounded::rdata::rdata() { } /// this is implicit zero fill
Rounded::rdata::rdata(vec4 tl, vec4 tr, vec4 br, vec4 bl) {
    /// top-left
    p_tl  = ion::xy(tl);
    v_tl  = ion::xy(tr) - p_tl;
    l_tl  = v_tl.length();
    d_tl  = v_tl / l_tl;

    /// top-right
    p_tr  = ion::xy(tr);
    v_tr  = ion::xy(br) - p_tr;
    l_tr  = v_tr.length();
    d_tr  = v_tr / l_tr;

    // bottom-right
    p_br  = ion::xy(br);
    v_br  = ion::xy(bl) - p_br;
    l_br  = v_br.length();
    d_br  = v_br / l_br;

    /// bottom-left
    p_bl  = ion::xy(bl);
    v_bl  = ion::xy(tl) - p_bl;
    l_bl  = v_bl.length();
    d_bl  = v_bl / l_bl;

    /// set-radius
    r_tl  = { math::min(tl.w, l_tl / 2), math::min(tl.z, l_bl / 2) };
    r_tr  = { math::min(tr.w, l_tr / 2), math::min(tr.z, l_br / 2) };
    r_br  = { math::min(br.w, l_br / 2), math::min(br.z, l_tr / 2) };
    r_bl  = { math::min(bl.w, l_bl / 2), math::min(bl.z, l_tl / 2) };
    
    /// pos +/- [ dir * radius ]
    tl_x = p_tl + d_tl * r_tl;
    tl_y = p_tl - d_bl * r_tl;
    tr_x = p_tr - d_tl * r_tr;
    tr_y = p_tr + d_tr * r_tr;
    br_x = p_br + d_br * r_br;
    br_y = p_br + d_bl * r_br;
    bl_x = p_bl - d_br * r_bl;
    bl_y = p_bl - d_tr * r_bl;

    c0   = (p_tr + tr_x) / T(2);
    c1   = (p_tr + tr_y) / T(2);
    p1   =  tr_y;
    c0b  = (p_br + br_y) / T(2);
    c1b  = (p_br + br_x) / T(2);
    c0c  = (p_bl + bl_x) / T(2);
    c1c  = (p_bl + bl_y) / T(2);
    c0d  = (p_tl + bl_x) / T(2);
    c1d  = (p_bl + bl_y) / T(2);
}

Rounded::rdata::rdata(rect &r, T rx, T ry) : rdata(vec4 {r.x, r.y, rx, ry}, vec4 {r.x + r.w, r.y, rx, ry},
                                                    vec4 {r.x + r.w, r.y + r.h, rx, ry}, vec4 {r.x, r.y + r.h, rx, ry}) { }


/// needs routines for all prims
bool Rounded::contains(vec2 v) { return (v >= data->p_tl && v < data->p_br); }
///
double     Rounded:: w() { return data->p_br.x - data->p_tl.x; }
double     Rounded:: h() { return data->p_br.y - data->p_tl.y; }
vec2d      Rounded::xy() { return data->p_tl; }
Rounded::operator bool() { return data->l_tl <= 0; }

/// set og rect (rectd) and compute the bezier
Rounded::Rounded(rect &r, T rx, T ry) : Rounded() {
    *Rect::data = r;
    *data = rdata { r, rx, ry };
}

}
