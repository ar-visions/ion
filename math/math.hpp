#pragma once

#include <mx/mx.hpp>
#include <glm/glm.hpp>

namespace ion {

template <typename T> using vec2 = glm::tvec2<T>;
template <typename T> using vec3 = glm::tvec3<T>;
template <typename T> using vec4 = glm::tvec4<T>;
template <typename T> using rgba = glm::tvec4<T>;

template <typename T> using m44 = glm::tmat4x4<T>;
template <typename T> using m33 = glm::tmat3x3<T>;
template <typename T> using m22 = glm::tmat2x2<T>;

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
        vec2 &a =   a;
        vec2 &b =   b;
        vec2 &c = e.a;
        vec2 &d = e.b;
        T num   = (a.x*b.y  -  a.y*b.x) * (c.x-d.x) - (a.x-b.x) * (c.x*d.y - c.y*d.x);
        T den   = (a.x-b.x) * (c.y-d.y) - (a.y-b.y) * (c.x-d.x);
        return num / den;
    }
    
    /// returns y-value of intersection of two lines
    T y(edge e) const {
        vec2 &a =   a;
        vec2 &b =   b;
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
};

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

using recti   = rect <i32>;
using rectd   = rect <r64>;
using rectf   = rect <r32>;

}
