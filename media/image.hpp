#pragma once
#include <mx/mx.hpp>
#include <math/math.hpp>

namespace ion {

using rgb8    = vec3u8;
using rgbd    = vec3d;
using rgbf    = vec3f;

using rgba8   = vec4u8;
using rgba    = vec4d;
using rgbaf   = vec4f;

struct Rect:mx {
    mx_object(Rect, mx, rect);
};

/// shortening methods
static inline vec2d xy  (const vec4d &v) { return { v.x, v.y }; }
static inline vec2d xy  (const vec3d &v) { return { v.x, v.y }; }
static inline vec3d xyz (const vec4d &v) { return { v.x, v.y, v.z }; }
static inline vec4d xyxy(const vec4d &v) { return { v.x, v.y, v.x, v.y }; }
static inline vec4d xyxy(const vec3d &v) { return { v.x, v.y, v.x, v.y }; }

/// 'rect' has rounded features; so implement the bezier output into that one
struct Rounded:Rect {
    using vec2 = ion::vec2d;
    using vec4 = ion::vec4d;
    using rect = ion::rect;
    using T    = double;

    struct rdata {
        vec2  p_tl, v_tl, d_tl;
        T     l_tl, l_tr, l_br, l_bl;
        vec2  p_tr, v_tr, d_tr;
        vec2  p_br, v_br, d_br;
        vec2  p_bl, v_bl, d_bl;
        vec2  r_tl, r_tr, r_br, r_bl;

        /// pos +/- [ dir * scale ]
        vec2 tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y;
        vec2 c0, c1, p1, c0b, c1b, c0c, c1c, c0d, c1d;

        rdata();
        rdata(vec4 tl, vec4 tr, vec4 br, vec4 bl);

        rdata(rect &r, T rx, T ry);
    };
    mx_object(Rounded, Rect, rdata);

    /// needs routines for all prims
    bool contains(vec2 v);
    ///
    T           w();
    T           h();
    vec2       xy();
    operator bool();

    /// set og rect (rectd) and compute the bezier
    Rounded(rect &r, T rx, T ry);
};

struct Arc:mx {
    struct adata {
        vec2d cen;
        real radius;
        vec2d degs; /// x:from [->y:distance]
        vec2d origin;
    };
    mx_object(Arc, mx, adata);
};

struct Line:mx {
    struct ldata {
        vec2d to;
        vec2d origin;
    };
    mx_object(Line, mx, ldata);
    //movable(Line);
};

struct Movement:mx {
    mx_object(Movement, mx, vec2d);
    //movable(Movement); /// makes a certain amount of sense
};

struct Bezier:mx {
    struct bdata {
        vec2d cp0;
        vec2d cp1;
        vec2d b;
        vec2d origin;
    };
    mx_object(Bezier, mx, bdata);
};

struct Vec2:mx {
    mx_object(Vec2, mx, vec2d);
};

/// modules && classes && functions
struct image:array {
    mx_object(image, array, rgba8);

    image(size  sz);
    image(null_t n);
    image(path   p);
    image(cstr   s);
    image(ion::size sz, rgba8 *px, int scanline = 0);

    ///
    bool    save(path p) const;
    rgba8      *pixels() const;
    size_t       width() const;
    size_t      height() const;
    size_t      stride() const;
    ion::rect     rect() const;
    vec2i           sz() const;
    ///
    rgba8 &operator[](ion::size pos) const;
};

struct yuv420:array {
    size_t w = 0, h = 0;
    size_t sz = 0;
    u8 *y = null, *u = null, *v = null;

    operator bool();
    bool operator!();

    yuv420();

    yuv420(image img);
    size_t       width() const;
    size_t      height() const;
};

}
