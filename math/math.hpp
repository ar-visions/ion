#pragma once

#include <mx/mx.hpp>
#include <glm/glm.hpp>

namespace ion {

template <typename T> using vec2 = glm::tvec2<T>;
template <typename T> using vec3 = glm::tvec3<T>;
template <typename T> using vec4 = glm::tvec4<T>;
template <typename T> using rgba = glm::tvec4<T>;

template <typename T> using m44  = glm::tmat4x4<T>;
template <typename T> using m33  = glm::tmat3x3<T>;
template <typename T> using m22  = glm::tmat2x2<T>;


template <typename T>
constexpr bool operator==(const rgba<T>& a, const rgba<T>& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

template <typename T>
constexpr bool operator!=(const rgba<T>& a, const rgba<T>& b) {
    return !(a == b);
}

template <typename T>
constexpr bool operator!(const rgba<T>& a) {
    return !(a.r || a.g || a.b || a.a);
}


/// vkg must use doubles!
using m44d    = m44 <r64>;
using m44f    = m44 <r32>;

using m33d    = m33 <r64>;
using m33f    = m33 <r32>;

using m22d    = m22 <r64>;
using m22f    = m22 <r32>;

using vec2i   = vec2 <i32>;
using vec2d   = vec2 <r64>;
using vec2f   = vec2 <r32>;

using vec3i   = vec3 <i32>;
using vec3d   = vec3 <r64>;
using vec3f   = vec3 <r32>;

using vec4i   = vec4 <i32>;
using vec4d   = vec4 <r64>;
using vec4f   = vec4 <r32>;

using rgb8    = vec3 <u8>;
using rgbd    = vec3 <r64>;
using rgbf    = vec3 <r32>;

using rgba8   = vec4 <u8>;
using rgbad   = vec4 <r64>;
using rgbaf   = vec4 <r32>;

/// would be nice to have bezier/composite-offsets
template <typename T>
struct edge {
    using vec2 = ion::vec2<T>;
    using vec4 = ion::vec4<T>;
    vec2 a, b;

    /// returns x-value of intersection of two lines
    T x(vec2 c, vec2 d) const {
        T num = (a.x*b.y  -  a.y*b.x) * (c.x-d.x) - (a.x-b.x) * (c.x*d.y - c.y*d.x);
        T den = (a.x-b.x) * (c.y-d.y) - (a.y-b.y) * (c.x-d.x);
        return num / den;
    }
    
    /// returns y-value of intersection of two lines
    T y(vec2 c, vec2 d) const {
        T num = (a.x*b.y  -  a.y*b.x) * (c.y-d.y) - (a.y-b.y) * (c.x*d.y - c.y*d.x);
        T den = (a.x-b.x) * (c.y-d.y) - (a.y-b.y) * (c.x-d.x);
        return num / den;
    }
    
    /// returns xy-value of intersection of two lines
    inline vec2 xy(vec2 c, vec2 d) const { return { x(c, d), y(c, d) }; }

    /// returns x-value of intersection of two lines
    T x(edge e) const {
        vec2 &c = e.a;
        vec2 &d = e.b;
        T num   = (a.x*b.y  -  a.y*b.x) * (c.x-d.x) - (a.x-b.x) * (c.x*d.y - c.y*d.x);
        T den   = (a.x-b.x) * (c.y-d.y) - (a.y-b.y) * (c.x-d.x);
        return num / den;
    }
    
    /// returns y-value of intersection of two lines
    T y(edge e) const {
        vec2 &c = e.a;
        vec2 &d = e.b;
        T num = (a.x*b.y  -  a.y*b.x) * (c.y-d.y) - (a.y-b.y) * (c.x*d.y - c.y*d.x);
        T den = (a.x-b.x) * (c.y-d.y) - (a.y-b.y) * (c.x-d.x);
        return num / den;
    }

    /// returns xy-value of intersection of two lines (via edge0 and edge1)
    inline vec2 xy(edge e) const { return { x(e), y(e) }; }
};

template <typename T>
struct rect {
    using vec2 = ion::vec2<T>;
    T x, y, w, h;
    
    inline rect(T x = 0, T y = 0, T w = 0, T h = 0) : x(x), y(y), w(w), h(h) { }
    inline rect(vec2 p0, vec2 p1) : x(p0.x), y(p0.y), w(p1.x - p0.x), h(p1.y - p0.y) { }

    inline rect  offset(T a)                const { return { x - a, y - a, w + (a * 2), h + (a * 2) }; }
    inline vec2  size()                     const { return { w, h }; }
    inline vec2  xy()                       const { return { x, y }; }
    inline vec2  center()                   const { return { x + w / 2.0, y + h / 2.0 }; }
    inline bool  contains(vec2 p)           const { return p.x >= x && p.x <= (x + w) && p.y >= y && p.y <= (y + h); }
    inline bool  operator== (const rect &r) const { return x == r.x && y == r.y && w == r.w && h == r.h; }
    inline bool  operator!= (const rect &r) const { return !operator==(r); }
    inline rect  operator + (rect     r)    const { return { x + r.x, y + r.y, w + r.w, h + r.h }; }
    inline rect  operator - (rect     r)    const { return { x - r.x, y - r.y, w - r.w, h - r.h }; }
    inline rect  operator + (vec2     v)    const { return { x + v.x, y + v.y, w, h }; }
    inline rect  operator - (vec2     v)    const { return { x - v.x, y - v.y, w, h }; }
    inline rect  operator * (T        r)    const { return { x * r, y * r, w * r, h * r }; }
    inline rect  operator / (T        r)    const { return { x / r, y / r, w / r, h / r }; }
    inline       operator rect<int>   ()    const { return {    int(x),    int(y),    int(w),    int(h) }; }
    inline       operator rect<float> ()    const { return {  float(x),  float(y),  float(w),  float(h) }; }
    inline       operator rect<double>()    const { return { double(x), double(y), double(w), double(h) }; }
    inline       operator bool()            const { return w > 0 && h > 0; }

    rect clip(rect &input) const {
        return rect {
            math::max(x, input.x),
            math::max(y, input.y),
            math::min(x + w, input.x + input.w) - math::max(x, input.x),
            math::min(y + h, input.y + input.h) - math::max(y, input.y),
        };
    }
};



/// high level class begins with Capitalisation, spelling words properly
/// try to express in 1 word when succinct
template <typename T>
struct Rect:mx {
    ptr(Rect, mx, rect<T>);
    movable(Rect);
};

using recti   = rect <i32>;
using rectd   = rect <r64>;
using rectf   = rect <r32>;

using Recti   = Rect <i32>;
using Rectd   = Rect <r64>;
using Rectf   = Rect <r32>;

/// shortening methods
template <typename T> inline vec2<T> xy  (vec4<T> v) { return { v.x, v.y }; }
template <typename T> inline vec2<T> xy  (vec3<T> v) { return { v.x, v.y }; }
template <typename T> inline vec3<T> xyz (vec4<T> v) { return { v.x, v.y, v.z }; }
template <typename T> inline vec4<T> xyxy(vec4<T> v) { return { v.x, v.y, v.x, v.y }; }
template <typename T> inline vec4<T> xyxy(vec3<T> v) { return { v.x, v.y, v.x, v.y }; }

template <typename T>
struct Rounded:Rect<T> {
    using vec2 = ion::vec2<T>;
    using vec4 = ion::vec4<T>;
    using rect = ion::rect<T>;

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

        inline rdata() { } /// this is implicit zero fill
        inline rdata(vec4 tl, vec4 tr, vec4 br, vec4 bl) {
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

        inline rdata(rect &r, T rx, T ry)
             : rdata(vec4 {r.x, r.y, rx, ry},             vec4 {r.x + r.w, r.y, rx, ry},
                     vec4 {r.x + r.w, r.y + r.h, rx, ry}, vec4 {r.x, r.y + r.h, rx, ry}) { }
        
    };
    ptr(Rounded, Rect<T>, rdata);
    movable(Rounded);

    /// needs routines for all prims
    inline bool contains(vec2 v) { return (v >= data->p_tl && v < data->p_br); }
    ///
    T           w() { return data->p_br.x - data->p_tl.x; }
    T           h() { return data->p_br.y - data->p_tl.y; }
    vec2       xy() { return data->p_tl; }
    operator bool() { return data->l_tl <= 0; }

    /// set og rect (r4r) and compute the bezier
    Rounded(rect &r, T rx, T ry) : Rounded(rdata(r, rx, ry)) { *Rect<T>::data = r; }
};

struct Arc:mx {
    struct adata {
        vec2d cen;
        real radius;
        vec2d degs; /// x:from [->y:distance]
        vec2d origin;
    };
    ptr(Arc, mx, adata);
};

struct Line:mx {
    struct ldata {
        vec2d to;
        vec2d origin;
    };
    ptr(Line, mx, ldata);
    movable(Line);
};

struct Movement:mx {
    ptr(Movement, mx, vec2d);
    movable(Movement); /// makes a certain amount of sense
};

struct Bezier:mx {
    struct bdata {
        vec2d cp0;
        vec2d cp1;
        vec2d b;
        vec2d origin;
    };
    ptr(Bezier, mx, bdata);
};

template <typename T>
struct Vec2:mx {
    ptr(Vec2<T>, mx, vec2<T>);
};

}
