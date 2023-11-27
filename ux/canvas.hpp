#pragma once
#include <ux/ux.hpp>

struct ICanvas;
struct iSVG;
namespace ion {

struct Canvas:mx {
    mx_declare(Canvas, mx, ICanvas);

    Canvas(VkhImage image);
    Canvas(VkhPresenter renderer);

    u32 get_virtual_width();
    u32 get_virtual_height();
    void canvas_resize(VkhImage image, int width, int height);
    void app_resize();
    void font(ion::font f);
    void save();
    void clear();
    void clear(rgbad c);
    void flush();
    void restore();
    void color(rgbad c);
    void opacity(double o);
    vec2i size();
    text_metrics measure(str text);
    double measure_advance(char *text, size_t len);
    str ellipsis(str text, rectd rect, text_metrics &tm);
    void image(image img, rectd rect, alignment align, vec2d offset, bool attach_tx = false);
    void text(str text, rectd rect, alignment align, vec2d offset, bool ellip, rectd *placement = null);
    void clip(rectd path);
    void projection(glm::mat4 &m, glm::mat4 &v, glm::mat4 &p);
    void outline(array<glm::vec3> v3);
    void outline(array<glm::vec2> v2);
    void line(glm::vec3 &a, glm::vec3 &b);
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
    void arc(glm::vec3 pos, real radius, real startAngle, real endAngle, bool is_fill = false);
};

struct SVG:mx {
    mx_declare(SVG, mx, iSVG);

    SVG(path p);
    void render(Canvas &canvas, int w, int h);
};

}
