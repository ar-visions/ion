#pragma once
#include <ux/ux.hpp>

struct ICanvas;
namespace ion {

struct Canvas:mx {
    mx_declare(Canvas, mx, ICanvas);

    Canvas(VkEngine e, VkhImage image, vec2i sz);
    u32 get_width();
    u32 get_height();
    void resize(VkhImage image, int width, int height);
    void font(ion::font &f);
    void save();
    void clear();
    void clear(rgbad c);
    void flush();
    void restore();
    void color(rgbad c);
    void opacity(double o);
    vec2i size();
    text_metrics measure(str text);
    str ellipsis(str &text, rectd rect, text_metrics &tm);
    void image(image img, rectd rect, alignment align, vec2d offset);
    void text(str text, rectd rect, alignment align, vec2d offset, bool ellip);
    void clip(rectd path);
    void outline(rectd rect);
    void outline_sz(double sz);
    void cap(graphics::cap c);
    void join(graphics::join j);
    void translate(vec2d tr);
    void scale(vec2d sc);
    void rotate(double degs);
    void fill(rectd rect);
    void fill(graphics::shape path);
    void clip(graphics::shape path);
    void gaussian(vec2d sz, rectd crop);
};
}
