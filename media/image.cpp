#include <media/image.hpp>
#include <png.h>
#include <yuv_rgb.h>

namespace ion {

yuv420::yuv420(image img) : 
        array<u8>(size_t(img.width() * img.height() +
                         img.width() * img.height() / 4 +
                         img.width() * img.height() / 4),
                  size_t(img.width() * img.height() +
                         img.width() * img.height() / 4 +
                         img.width() * img.height() / 4)) {
    w  = img.width();
    h  = img.height();
    y  =  data;
    u  = &data[w * h];
    v  = &data[w * h + (w/2 * h/2)];
    sz = w * h * 3 / 2;
    rgb32_yuv420(
        w, h, (u8*)img.data, 4 * w, y, u, v, w, (w+1)/2, YCBCR_JPEG);
}

/// load an image into 32bit rgba format
image::image(ion::size sz, rgba8 *px, int scanline) : array() {
    mem->shape  = new ion::size(sz);
    mem->origin = px;
    data        = px;
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

}
