#pragma once

#include <mx/mx.hpp>
#include <async/async.hpp>
#include <net/net.hpp>
#include <math/math.hpp>
#include <media/media.hpp>

struct GLFWwindow;

namespace ion {

/// @brief  imaginary friends
struct Device;
struct Texture;

using Path    = path;
using Image   = image;
using strings = array<str>;
using Assets  = map<Texture *>;

/// never support 16bit indices for obvious reasons.  you do 1 cmclark section too many and there it goes
struct ngon {
    size_t size;
    u32   *indices;
};

using mesh = array<ngon>;

struct text_metrics:mx {
    struct tmdata {
        real        w, h;
        real      ascent,
                 descent;
        real line_height,
              cap_height;
    };
    mx_object(text_metrics, mx, tmdata);
};

using tm_t = text_metrics;

/// would be nice to handle basically everything model-wise in graphics
namespace graphics {
    enums(cap, none,
       "none, blunt, round",
        none, blunt, round);
    
    enums(join, miter,
       "miter, round, bevel",
        miter, round, bevel);

    struct shape:mx {
        using Rect    = Rect   <r64>;
        using Rounded = Rounded<r64>;
        ///
        struct sdata {
            Rectd      bounds; /// if the ops change the bounds must be re-eval'd
            doubly<mx> ops;
            vec2d      mv;
        };
        ///
        mx_object(shape, mx, sdata);

        operator rectd &() {
            if (!data->bounds)
                bounds();
            return data->bounds;
        }

        bool contains(vec2d p) {
            if (!data->bounds)
                bounds();
            return data->bounds->contains(p);
        }
        ///
        rectd &bounds() {
            real min_x = 0, max_x = 0;
            real min_y = 0, max_y = 0;
            int  index = 0;
            ///
            if (!data->bounds && data->ops) {
                /// get aabb from vec2d
                for (mx &v:data->ops) {
                    vec2d *vd = v.get<vec2d>(0); 
                    if (!vd) continue;
                    if (!index || vd->x < min_x) min_x = vd->x;
                    if (!index || vd->y < min_y) min_y = vd->y;
                    if (!index || vd->x > max_x) max_x = vd->x;
                    if (!index || vd->y > max_y) max_y = vd->y;
                    index++;
                }
            }
            return data->bounds;
        }

        shape(rectd r4, real rx = nan<real>(), real ry = nan<real>()) : shape() {
            bool use_rect = std::isnan(rx) || rx == 0 || ry == 0;
            data->bounds = use_rect ? Rect(r4) : 
                                      Rect(Rounded(r4, rx, std::isnan(ry) ? rx : ry)); /// type forwards
        }

        bool is_rect () { return data->bounds.type() == typeof(rectd)          && !data->ops; }
        bool is_round() { return data->bounds.type() == typeof(Rounded::rdata) && !data->ops; }    

        /// following ops are easier this way than having a last position which has its exceptions for arc/line primitives
        inline void line    (vec2d  l) {
            data->ops += ion::Line({ data->mv, l });
            data->mv = l;
        }
        inline void bezier  (Bezier b) { data->ops += b; }
        inline void move    (vec2d  v) {
            data->mv = v;
            data->ops += ion::Movement(v);
        }

      //void quad    (graphics::quad   q) { data->ops += q; }
        inline void arc     (Arc a) { data->ops += a; }
        operator        bool() { bounds(); return bool(data->bounds); }
        bool       operator!() { return !operator bool(); }
        shape::sdata *handle() { return data; }
    };

    struct border_data {
        real size, tl, tr, bl, br;
        rgbad color;
        inline bool operator==(const border_data &b) const { return b.tl == tl && b.tr == tr && b.bl == bl && b.br == br; }
        inline bool operator!=(const border_data &b) const { return !operator==(b); }

        /// members are always null from the memory::allocation
        border_data() { }
        ///
        border_data(str raw) : border_data() {
            str        trimmed = raw.trim();
            size_t     tlen    = trimmed.len();
            array<str> values  = raw.split();
            size_t     ncomps  = values.len();
            ///
            if (tlen > 0) {
                size = values[0].real_value<real>();
                if (ncomps == 5) {
                    tl = values[1].real_value<real>();
                    tr = values[2].real_value<real>();
                    br = values[3].real_value<real>();
                    bl = values[4].real_value<real>();
                } else if (tlen > 0 && ncomps == 2)
                    tl = tr = bl = br = values[1].real_value<real>();
                else
                    console.fault("border requires 1 (size) or 2 (size, roundness) or 5 (size, tl tr br bl) values");
            }
        }
    };
    /// graphics::border is an indication that this has information on color
    using border = sp<border_data>;
};

enums(nil, none, "none", none);

enums(xalign, undefined,
    "undefined, left, middle, right",
     undefined, left, middle, right);
    
enums(yalign, undefined,
    "undefined, top, middle, bottom, line",
     undefined, top, middle, bottom, line);

enums(blending, undefined,
    "undefined, clear, src, dst, src-over, dst-over, src-in, dst-in, src-out, dst-out, src-atop, dst-atop, xor, plus, modulate, screen, overlay, color-dodge, color-burn, hard-light, soft-light, difference, exclusion, multiply, hue, saturation, color",
     undefined, clear, src, dst, src_over, dst_over, src_in, dst_in, src_out, dst_out, src_atop, dst_atop, Xor, plus, modulate, screen, overlay, color_dodge, color_burn, hard_light, soft_light, difference, exclusion, multiply, hue, saturation, color);

enums(filter, none,
    "none, blur, brightness, contrast, drop-shadow, grayscale, hue-rotate, invert, opacity, saturate, sepia",
     none, blur, brightness, contrast, drop_shadow, grayscale, hue_rotate, invert, opacity, saturate, sepia);

enums(duration, undefined,
    "undefined, ns, ms, s",
     undefined, ns, ms, s);

/// needs more distance units.  pc = parsec
enums(distance, undefined,
    "undefined, px, m, cm, in, ft, pc, %",
     undefined, px, m, cm, in, ft, parsec, percent);

/// x20cm, t10% things like this.
template <typename P, typename S>
struct scalar:mx {
    using scalar_t = scalar<P, S>;
    struct sdata {
        P            prefix;
        real         scale;
        S            suffix;
        bool         is_percent;
    };

    mx_object(scalar, mx, sdata);
    movable(scalar);

    real operator()(real origin, real size) {
        return origin + (data->is_percent ? (data->scale / 100.0 * size) : data->scale);
    }

    using p_type = typename P::etype;
    using s_type = typename S::etype;

    inline bool operator>=(p_type p) const { return p >= data->prefix; }
    inline bool operator> (p_type p) const { return p >  data->prefix; }
    inline bool operator<=(p_type p) const { return p <= data->prefix; }
    inline bool operator< (p_type p) const { return p <  data->prefix; }
    inline bool operator>=(s_type s) const { return s >= data->suffix; }
    inline bool operator> (s_type s) const { return s >  data->suffix; }
    inline bool operator<=(s_type s) const { return s <= data->suffix; }
    inline bool operator< (s_type s) const { return s <  data->suffix; }
    inline bool operator==(p_type p) const { return p == data->prefix; }
    inline bool operator==(s_type s) const { return s == data->suffix; }
    inline bool operator!=(p_type p) const { return p != data->prefix; }
    inline bool operator!=(s_type s) const { return s != data->suffix; }

    inline operator    P() const { return p_type(data->prefix); }
    inline operator    S() const { return s_type(data->suffix); }
    inline operator real() const { return data->scale; }

    scalar(p_type prefix, real scale, s_type suffix) : scalar(sdata { prefix, scale, suffix, false }) { }

    scalar(str s) : scalar() {
        str    tr  = s.trim();
        size_t len = tr.len();
        bool in_symbol = isalpha(tr[0]) || tr[0] == '_' || tr[0] == '%'; /// str[0] is always guaranteed to be there
        array<str> sp   = tr.split([&](char &c) -> bool {
            bool split  = false;
            bool symbol = isalpha(c) || (c == '_' || c == '%'); /// todo: will require hashing the enums by - and _, but for now not exposed as issue
            ///
            if (c == 0 || in_symbol != symbol) {
                in_symbol = symbol;
                split     = true;
            }
            return split;
        });

        try {
            if constexpr (identical<P, nil>()) {
                data->scale  =   sp[0].real_value<real>();
                if (sp[1]) {
                    data->suffix = S(sp[1]);
                    if constexpr (identical<S, distance>())
                        data->is_percent = data->suffix == distance::percent;
                }
            } else if (identical<S, nil>()) {
                data->prefix = P(sp[0]);
                data->scale  =   sp[1].real_value<real>();
            } else {
                data->prefix = P(sp[0]);
                data->scale  =   sp[1].real_value<real>();
                if (sp[2]) {
                    data->suffix = S(sp[2]);
                    if constexpr (identical<S, distance>())
                        data->is_percent = data->suffix == distance::percent;
                }
                data->prefix = P(sp[0]);
                data->scale  =   sp[1].real_value<real>();
            }
        } catch (P &e) {
            console.fault("type exception in scalar construction: prefix lookup failure: {0}", { str(typeof(P)->name) });
        } catch (S &e) {
            console.fault("type exception in scalar construction: suffix lookup failure: {0}", { str(typeof(S)->name) });
        }
    }
};

///
struct alignment:mx {
    struct adata {
        scalar<xalign, distance> x; /// these are different types
        scalar<yalign, distance> y; /// ...
    };

    ///
    mx_object(alignment, mx, adata);
    movable(alignment);

    ///
    inline alignment(str  x, str  y) : alignment { adata { x, y } } { }
    inline alignment(real x, real y)
         : alignment { adata { { xalign::left, x, distance::px },
                               { yalign::top,  y, distance::px } } } { }

    inline operator vec2d() { return vec2d { real(data->x), real(data->y) }; }

    /// compute x and y coordinates given alignment cdata, and parameters for rect and offset
    vec2d plot(rectd &win) {
        vec2d rs;
        /// todo: direct enum operator does not seem to works
        /// set x coordinate
        switch (xalign::etype(xalign(data->x))) {
            case xalign::undefined: rs.x = nan<real>();                        break;
            case xalign::left:      rs.x = data->x(win.x,             win.w); break;
            case xalign::middle:    rs.x = data->x(win.x + win.w / 2, win.w); break;
            case xalign::right:     rs.x = data->x(win.x + win.w,     win.w); break;
        }
        /// set y coordinate
        switch (yalign::etype(yalign(data->y))) {
            case yalign::undefined: rs.y = nan<real>();                        break;
            case yalign::top:       rs.y = data->y(win.y,             win.h); break;
            case yalign::middle:    rs.y = data->y(win.y + win.h / 2, win.h); break;
            case yalign::bottom:    rs.y = data->y(win.y + win.h,     win.h); break;
            case yalign::line:      rs.y = data->y(win.y + win.h / 2, win.h); break;
        }
        /// return vector of this 'corner' (top-left or bottom-right)
        return vec2d(rs);
    }
};

/// good primitive for ui, implemented in basic canvas ops.
/// regions can be constructed from rects if area is static or composed in another way
struct region:mx {
    struct rdata {
        alignment tl;
        alignment br;
    };

    ///
    mx_object(region, mx, rdata);
    
    ///
    region(str s) : region() {
        array<str> a = s.trim().split();
        data->tl = alignment { a[0], a[1] };
        data->br = alignment { a[2], a[3] };
    }

    /// when given a shape, this is in-effect a window which has static bounds
    /// this is used to convert shape abstract to a region
    region(graphics::shape sh) : region() {
        rectd b = sh.bounds();
        data->tl   = alignment { b.x,  b.y };
        data->br   = alignment { b.x + b.w,
                                 b.y + b.h };
    }
    
    ///
    rectd rect(rectd &win) {
        return rectd { data->tl.plot(win), data->br.plot(win) };
    }
};

enums(keyboard, none,
    "none, caps_lock, shift, ctrl, alt, meta",
     none, caps_lock, shift, ctrl, alt, meta);

enums(mouse, none,
    "none, left, right, middle, inactive",
     none, left, right, middle, inactive);

namespace user {
    using chr = num;

    struct key:mx {
        struct kdata {
            num               unicode;
            num               scan_code;
            states<keyboard>  modifiers;
            bool              repeat;
            bool              up;
        };
        mx_object(key, mx, kdata);
    };
};

struct event:mx {
    ///
    struct edata {
        user::chr     unicode;
        user::key     key;
        vec2d          wheel_delta;
        vec2d          cursor;
        states<mouse>    buttons;
        states<keyboard> modifiers;
        size          resize;
        mouse::etype  button_id;
        bool          prevent_default;
        bool          stop_propagation;
    };

    mx_object(event, mx, edata);
    
    event(const mx &a) = delete;
    ///
    inline void prevent_default()   {         data->prevent_default = true; }
    inline bool is_default()        { return !data->prevent_default; }
    inline bool should_propagate()  { return !data->stop_propagation; }
    inline bool stop_propagation()  { return  data->stop_propagation = true; }
    inline vec2d cursor_pos()        { return data->cursor; }
    inline bool mouse_down(mouse m) { return  data->buttons[m]; }
    inline bool mouse_up  (mouse m) { return !data->buttons[m]; }
    inline num  unicode   ()        { return  data->key->unicode; }
    inline num  scan_code ()        { return  data->key->scan_code; }
    inline bool key_down  (num u)   { return  data->key->unicode   == u && !data->key->up; }
    inline bool key_up    (num u)   { return  data->key->unicode   == u &&  data->key->up; }
    inline bool scan_down (num s)   { return  data->key->scan_code == s && !data->key->up; }
    inline bool scan_up   (num s)   { return  data->key->scan_code == s &&  data->key->up; }
};


struct node;
struct style;

/// enumerables for the state bits
enums(interaction, undefined,
    "undefined, captured, focused, hover, active, cursor",
     undefined, captured, focused, hover, active, cursor);

struct   map_results:mx {   map_results():mx() { } };
struct array_results:mx { array_results():mx() { } };

struct Element:mx {
    struct edata {
        type_t           type;     /// type given
        memory*          id;       /// identifier 
        ax               args;     /// ax = better than args.  we use args as a var all the time; arguments is terrible
        array<Element>  *children; /// lol.
        node*            instance; /// node instance is 1:1
        map<node*>       mounts;   /// store instances of nodes in element data, so the cache management can go here where element turns to node
        node*            parent;
        style*           root_style; /// applied at root for multiple style trees across multiple apps
        states<interaction> istates;
    };

    /// default case when there is no render()
    array<Element> &children() { return *data->children; }

    mx_object(Element, mx, edata);
    movable(Element);

    static memory *args_id(type_t type, initial<arg> args) {
        static memory *m_id = memory::symbol("id"); /// im a token!
        for (auto &a:args)
            if (a.key.mem == m_id)
                return a.value.mem;
        
        return memory::symbol(symbol(type->name));
    }

    //inline Element(ax &a):mx(a.mem->grab()), e(defaults<data>()) { }
    Element(type_t type, initial<arg> args) : Element(
        edata { type, args_id(type, args), args } // ax = array of args; i dont want the different ones but they do syn
    ) { }

    /// inline expression of the data inside, im sure this would be common case (me 6wks later: WHERE????)
    Element(type_t type, size_t sz) : Element(
        edata { type, null, null, new array<Element>(sz) }
    ) { }

    template <typename T>
    static Element each(array<T> a, lambda<Element(T &v)> fn) {
        Element res(typeof(map_results), a.length());
        for (auto &v:a) {
            Element ve = fn(v);
            if (ve) *res.data->children += ve;
        }
        return res;
    }
    
    template <typename K, typename V>
    static Element each(map<V> m, lambda<Element(K &k, V &v)> fn) {
        Element res(typeof(array_results), m.size);
        if (res.data->children)
        for (auto &[v,k]:m) {
            Element r = fn(k, v);
            if (r) *res.data->children += r;
        }
        return res;
    }

    mx node() {
        assert(false);
        return mx();
        ///return type->ctr();
    }
};

using elements  = array<Element>;
using fn_render = lambda<Element()>;
using fn_stub   = lambda<void()>;
using callback  = lambda<void(event)>;

struct dispatch;

struct listener:mx {
    struct ldata {
        dispatch *src;
        callback  cb;
        fn_stub   detatch;
        bool      detached;
        ///
        ~ldata() { printf("listener, ...destroyed\n"); }
    };
    
    ///
    mx_object(listener, mx, ldata);

    void cancel() {
        data->detatch();
        data->detached = true;
    }

    ///
    ~listener() {
        if (!data->detached && mem->refs == 1) /// only reference is the binding reference, detect and make that ref:0, ~data() should be fired when its successfully removed from the list
            cancel();
    }
};

/// keep it simple for sending events, with a dispatch.  listen to them with listen()
struct dispatch:mx {
    struct ddata {
        doubly<listener> listeners;
    };
    ///
    mx_object(dispatch, mx, ddata);
    ///
    void operator()(event e);
    ///
    listener &listen(callback cb);
};

/// simple wavefront obj
template <typename V>
struct OBJ:mx {
    /// prefer this to typedef
    using strings = array<str>;

    struct group:mx {
        struct gdata {
            str        name;
            array<u32> ibo;
        };
        mx_object(group, mx, gdata);
    };

    struct members {
        array<V>   vbo;
        map<group> groups;
    };

    mx_object(OBJ, mx, members);

    OBJ(path p, lambda<V(group&, vec3d*, vec2d*, vec3d*)> fn) : OBJ() {
        str g;
        str contents  = str::read_file(p.exists() ? p : fmt {"models/{0}.obj", { p }});
        assert(contents.len() > 0);
        
        auto lines    = contents.split("\n");
        size_t line_c = lines.len();
        auto wlines   = array<strings>();
        auto v        = array<vec3d>(line_c); // need these
        auto vn       = array<vec3d>(line_c);
        auto vt       = array<vec2d>(line_c);
        auto indices  = std::unordered_map<str, u32>(); ///
        size_t verts  = 0;

        ///
        for (str &l:lines)
            wlines += l.split(" ");
        
        /// assert triangles as we count them
        map<int> gcount = {};
        for (strings &w: wlines) {
            if (w[0] == "g" || w[0] == "o") {
                g = w[1];
                data->groups[g].name  = g;
                data->gcount[g]       = 0;
            } else if (w[0] == "f") {
                if (!g.len() || w.len() != 4)
                    console.fault("import requires triangles"); /// f pos/uv/norm pos/uv/norm pos/uv/norm
                data->gcount[g]++;
            }
        }

        /// add vertex data
        for (auto &w: wlines) {
            if (w[0] == "g" || w[0] == "o") {
                g = w[1];
                if (!data->groups[g].ibo)
                     data->groups[g].ibo = array<u32>(data->gcount[g] * 3);
            }
            else if (w[0] == "v")  v  += vec3d { w[1].real_value<real>(), w[2].real_value<real>(), w[3].real_value<real>() };
            else if (w[0] == "vt") vt += vec2d { w[1].real_value<real>(), w[2].real_value<real>() };
            else if (w[0] == "vn") vn += vec3d { w[1].real_value<real>(), w[2].real_value<real>(), w[3].real_value<real>() };
            else if (w[0] == "f") {
                assert(g.len());
                for (size_t i = 1; i < 4; i++) {
                    auto key = w[i];
                    if (indices.count(key) == 0) {
                        indices[key] = u32(verts++);
                        auto      sp =  w[i].split("/");
                        int      iv  = sp[0].integer_value();
                        int      ivt = sp[1].integer_value();
                        int      ivn = sp[2].integer_value();
                        data->vbo       += fn(data->groups[g],
                                                iv  ? & v[ iv-1] : null,
                                                ivt ? &vt[ivt-1] : null,
                                                ivn ? &vn[ivn-1] : null);
                    }
                    data->groups[g].ibo += indices[key];
                }
            }
        }
    }
};

enums(mode, regular,
     "regular, headless",
      regular, headless);

struct window_data;
struct window:mx {
    mx_declare(window, mx, window_data);
    
    window(ion::size sz, mode::etype m, memory *control);

    void        loop(lambda<void()> fn);
    bool        key(int key);
    vec2d        cursor();
    str         clipboard();
    void        set_clipboard(str text);
    void        set_title(str s);
    void        show();
    void        hide();
    void        start();
    ion::size   size();
    void        repaint();
    Device     *device();
    Texture    *texture();
    Texture    *texture(ion::size sz);

    operator bool();
};

enums(Asset, Undefined,
    "Undefined, Color, Normal, Specular, Displace",
     Undefined, Color, Normal, Specular, Displace);

enums(VA, Position,
    "Position, Normal, UV, Color, Tangent, BiTangent",
     Position, Normal, UV, Color, Tangent, BiTangent);

using VAttribs = states<VA>;

struct TextureMemory;
struct StageData;
///

/// generic vertex model; uses spec map, normal map by tangent/bi-tangent v3 unit vectors
struct Vertex {
    ///
    vec3f pos;  // position
    vec3f norm; // normal position
    vec2f uv;   // texture uv coordinate
    vec4f clr;  // color
    vec3f ta;   // tangent
    vec3f bt;   // bi-tangent
    
    /// VkAttribs (array of VkAttribute) [data] from VAttribs (state flags) [model]
    static void attribs(VAttribs attr, void *vk_attr_res);
    Vertex() { }
    Vertex(vec3f pos, vec3f norm, vec2f uv, vec4f clr, vec3f ta = {}, vec3f bt = {}):
           pos  (pos), norm (norm), uv   (uv),
           clr  (clr), ta   (ta),   bt   (bt) { }
    Vertex(vec3f &pos, vec3f &norm, vec2f &uv, vec4f &clr, vec3f &ta, vec3f &bt):
           pos  (pos), norm (norm), uv   (uv),
           clr  (clr), ta   (ta),   bt   (bt) { }
    
    /// hip to consider this data
    static array<Vertex> square(vec4f v_clr = {1.0, 1.0, 1.0, 1.0}) {
        return array<Vertex> {
            Vertex {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, v_clr},
            Vertex {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, v_clr},
            Vertex {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, v_clr},
            Vertex {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, v_clr}
        };
    }
    
    void calculate_tangents(array<Vertex> &verts, array<ngon> &faces);
};

/// use this for building up data, and then it is handed to VertexBuffer
using Vertices = array<Vertex>;

mesh subdiv(const mesh& input_mesh, const array<vec3f>& verts);

/// this one would be nice to use gcc's static [] or () overload in c++ 23
struct Shaders {
    ion::map<str> m;
    /// default construction
    Shaders(std::nullptr_t n = null) { m["*"] = "main"; }
    
    bool operator==(Shaders &ref) { return m == ref.m; }
    operator  bool() { return  m.count(); }
    bool operator!() { return !m.count(); }
    
    /// group=shader
    Shaders(str v) { /// str is the only interface in it.  everything else is just too messy for how simple the map is
        strings sp = v.split(",");
        for (str &v: sp) {
            auto a = v.split("=");
            assert(a.length() == 2);
            str key   = a[0];
            str value = a[1];
            m[key] = value;
        }
    }

    str operator()(str  &group) {
        return m->count(group) ? m[group] : m["*"];
    }

    str &operator[](str n) {
        return m[n];
    }

    size_t count(str n) {
        return m->count(n);
    }
};

struct Composer;

extern Assets cache_assets(str model, str skin, states<Asset> &atypes);

struct font:mx {
    struct fdata {
        str  alias;
        real sz;
        path res;
    };

    mx_object(font, mx, fdata);
    movable(font);

    font(str alias, real sz, path res) : font(fdata { alias, sz, res }) { }

    /// if there is no res, it can use font-config; there must always be an alias set, just so any cache has identity
    static font default_font() {
        static font def; ///
        if        (!def) ///
                    def = font("monaco", 16, "fonts/monaco.ttf");
        return      def; ///
    }
            operator bool()    { return  data->alias; }
    bool    operator!()        { return !data->alias; }
};

///
struct glyph:mx {
    struct members {
        int        border;
        str        chr;
        rgba8      bg;
        rgba8      fg;
    };
    mx_object(glyph, mx, members);

    str ansi();
    bool operator==(glyph &lhs) {
        return   (data->border == lhs->border) && (data->chr == lhs->chr) &&
                 (data->bg     == lhs->bg)     && (data->fg  == lhs->fg);
    }
    bool operator!=(glyph &lhs) { return !operator==(lhs); }
};

struct cbase:mx {
    struct draw_state {
        real             outline_sz;
        real             font_sz;
        real             line_height;
        real             opacity;
        graphics::shape  clip;
        str              font;
        m44d             mat44;
        rgba8            color;
        vec2d            blur;
        graphics::cap    cap;
        graphics::join   join;
    };

    ///
    enums(state_change, defs,
       "defs, push, pop",
        defs, push, pop);
    
    struct cdata {
        ion::size          size; /// size in integer units; certainly not part of the stack lol.
        type_t             pixel_t;
        doubly<draw_state> stack; /// ds = states.last()
        draw_state*        state; /// todo: update w push and pop
    };

    mx_object(cbase, mx, cdata);

    protected:
    virtual void init() { }
    
    draw_state &cur() {
        if (data->stack->length() == 0)
            data->stack += draw_state(); /// this errors in graphics::cap initialize (type construction, Deallocate error indicates stack corruption?)
        
        return data->stack->last();
    }

    public:
    ion::size &size() { return data->size; }

    virtual void    outline(graphics::shape) { }
    virtual void       fill(graphics::shape) { }
    virtual void       clip(graphics::shape cl) {
        draw_state  &ds = cur();
        ds.clip = cl;
    }

    virtual void      flush() { }
    virtual void       text(str, graphics::shape, vec2d, vec2d, bool)  { }
    virtual void      image(ion::image, graphics::shape, vec2d, vec2d) { }
    virtual void    texture(ion::image)        { }
    virtual void      clear()                  { }
    virtual void      clear(rgba8)             { }
    virtual tm_t    measure(str s)               { assert(false); return {}; }
    virtual void        cap(graphics::cap   cap) { cur().cap     = cap;      }
    virtual void       join(graphics::join join) { cur().join    = join;     }
    virtual void    opacity(real        opacity) { cur().opacity = opacity;  }
    virtual void       font(font f)            { }
    virtual void      color(rgba8)             { }
    virtual void   gaussian(vec2d, graphics::shape) { }
    virtual void      scale(vec2d)             { }
    virtual void     rotate(real)              { }
    virtual void  translate(vec2d)             { }
    virtual void   set_char(int, int, glyph)   { } /// this needs to be tied to pixel unit (cbase 2nd T arg)
    virtual str    get_char(int, int)          { return "";    }
    virtual str  ansi_color(rgba8 &c, bool text) { return null;  }
    virtual ion::image  resample(ion::size sz, real deg = 0.0f, rectd view = {}, vec2d rc = {}) {
        return null;
    }

    virtual void draw_state_change(draw_state *ds, state_change sc) { }

    /// these two arent virtual but draw_state_changed_is; do all of the things there.
    void push() {
        data->stack +=  cur();
        data->state  = &cur();
        /// first time its pushed in more hacky dependency chain ways
        draw_state_change(data->state, state_change::push);
    }

    void  pop() {
        data->stack->pop();
        data->state = &cur();
        ///
        draw_state_change(data->state, state_change::pop);
    }

    void    outline_sz(real sz)  { cur().outline_sz  = sz;  }
    void    font_sz   (real sz)  { cur().font_sz     = sz;  }
    void    line_height(real lh) { cur().line_height = lh;  }
    m44d    get_matrix()         { return cur().mat44;      }
    void    set_matrix(m44d m)   {        cur().mat44 = m;  }
    void      defaults()         {
        draw_state &ds = cur();
        ds.line_height = 1; /// its interesting to base units on a font-derrived line-height
                            /// in the case of console context, this allows for integral rows and columns for more translatable views to context2d
    }

    /// boolean operator
    inline operator bool() { return bool(data->size); }
};

struct gfx_memory;
template <> struct is_opaque<gfx_memory> : true_type { };

/// gfx: a frontend on skia
struct gfx:cbase {
    gfx_memory* g;
    
    /// create with a window (indicated by a name given first)
    gfx(ion::window &w);

    /// data is single instanced on this cbase, and the draw_state is passed in as type for the cbase, which atleast makes it type-unique
    mx_declare(gfx, cbase, gfx_memory);

    ion::window    &window();
    Device         *device();
    void draw_state_change(draw_state *ds, cbase::state_change type);
    text_metrics   measure(str text);
    str    format_ellipsis(str text, real w, text_metrics &tm_result);
    void     draw_ellipsis(str text, real w);
    void             image(ion::image img, graphics::shape sh, vec2d align, vec2d offset, vec2d source);
    void              push();
    void               pop();
    void              text(str text, rectd rect, alignment align, vec2d offset, bool ellip);
    void              clip(graphics::shape cl);
    Texture        texture();
    void             flush();
    void             clear(rgba8 c);
    void              font(ion::font f);
    void               cap(graphics::cap   c);
    void              join(graphics::join  j);
    void         translate(vec2d       tr);
    void             scale(vec2d       sc);
    void             scale(real       sc);
    void            rotate(real     degs);
    void              fill(graphics::shape  p);
    void          gaussian(vec2d sz, graphics::shape c);
    void           outline(graphics::shape p);
    str           get_char(int x, int y);
    str         ansi_color(rgba8 &c, bool text);
    ion::image    resample(ion::size sz, real deg, graphics::shape view, vec2d axis);
};

/// should call it text_canvas, display_buffer, tcanvas; terminal should be implementation of escape sequence/display in gfx context
struct terminal:cbase {
    static inline symbol reset_chr = "\u001b[0m";
    struct tdata {
        array<glyph> glyphs;
        draw_state  *ds;
    };
    terminal(ion::size sz);
    mx_object(terminal, cbase, tdata);

    void draw_state_change(draw_state &ds, cbase::state_change type);
    str         ansi_color(rgba8 &c, bool text);
    void          set_char(int x, int y, glyph gl);
    str           get_char(int x, int y);
    void              text(str s, graphics::shape vrect, alignment::adata &align, vec2d voffset, bool ellip);
    void           outline(graphics::shape sh);
    void              fill(graphics::shape sh);
};

enums(operation, fill,
    "fill, image, outline, text, child",
     fill, image, outline, text, child);

struct node:Element {
    /// standard props
    struct props {
        struct events {
            dispatch            hover;
            dispatch            out;
            dispatch            down;
            dispatch            up;
            dispatch            key;
            dispatch            focus;
            dispatch            blur;
            dispatch            cursor;
        } ev;

        struct drawing {
            rgba8               color; 
            real                offset;
            alignment           align; 
            image               img;   
            region              area;  
            graphics::shape     shape; 
            graphics::cap       cap;   
            graphics::join      join;
            graphics::border    border;
            inline rectd rect() { return shape.bounds(); }
        };

        /// this is simply bounds, not rounded
        inline graphics::shape shape() {
            return drawings[operation::child].shape;
        }

        inline rectd bounds() {
            return rectd((rectd&)shape());
        }

        ///
        str                 id;         ///
        drawing             drawings[operation::count];
        rectd               container;  /// the parent child rectangle
        int                 tab_index;  /// if 0, this is default; its all relative numbers we dont play absolutes.
        states<interaction> istates;    /// interaction states; these are referenced in qualifiers
        vec2d                cursor;     /// set relative to the area origin
        vec2d                scroll = {0,0};
        std::queue<fn_t>    queue;      /// this is an animation queue
        bool                active;
        bool                focused;
        type_t              type;       /// this is the type of node, as stored by the initializer of this data
        mx                  content;
        mx                  bind;       /// bind is useful to be in mx form, as any object can be key then.

        doubly<prop> meta() const {
            return {
                prop { "id",        id        },
                prop { "on-hover",  ev.hover  }, /// must give member-parent-origin (std) to be able to 're-apply' somewhere else
                prop { "on-out",    ev.out    },
                prop { "on-down",   ev.down   },
                prop { "on-up",     ev.up     },
                prop { "on-key",    ev.key    },
                prop { "on-focus",  ev.focus  },
                prop { "on-blur",   ev.blur   },
                prop { "on-cursor", ev.cursor },
                prop { "on-hover",  ev.hover  },
                prop { "content",   content   }
            };
        }
    };

    /// type and prop name lookup in tree
    /// there can also be an mx-based context.  it must assert that the type looked up in meta_map inherits from mx.  from there it can be ref memory,
    template <typename T>
    T &context(memory *sym) {
        node  *cur = (node*)this;
        while (cur) {
            type_t ctx = cur->mem->type;
            if (ctx) {
                prop *def = ctx->schema->meta_map.lookup((symbol&)*sym); /// will always need schema
                if (def) {
                    T &ref = def->member_ref<T>(cur->data); // require inheritance here
                    return ref;
                }
            }
            cur = cur->Element::data->parent;
        }
        assert(false);
        T *n = null;
        return *n;
    }

    /// base drawing routine, easiest to understand and i think favoured over a non-virtual that purely uses the meta tables in polymorphic back tracking fashion
    /// one could lazy-cache that but i dunno.  why make drawing things complicated and covered in lambdas.  during iteration in debug you thank thy-self
    virtual void draw(gfx& canvas) {
        props::drawing &fill    = data->drawings[operation::fill];
        props::drawing &image   = data->drawings[operation::image];
        props::drawing &text    = data->drawings[operation::text];
        props::drawing &outline = data->drawings[operation::outline]; /// outline is more AR than border.  and border is a bad idea, badly defined and badly understood. outline is on the 0 pt.  just offset it if you want.
        
        /// if there is a fill color
        if (!!fill.color) { /// free'd prematurely during style change (not a transition)
            canvas.color(fill.color);
            canvas.fill(fill.shape);
        }

        /// if there is fill image
        if (image.img)
            canvas.image(image.img, image.shape, image.align, {0,0}, {0,0});
        
        /// if there is text (its not alpha 0, and there is text)
        if (data->content && ((data->content.type() == typeof(char)) ||
                            (data->content.type() == typeof(str)))) {
            canvas.color(text.color);
            canvas.text(
                data->content.grab(), text.shape.bounds(),
                text.align, {0.0, 0.0}, true);
        }

        /// if there is an effective border to draw
        if (!!outline.color && outline.border->size > 0.0) {
            canvas.color(outline.border->color);
            canvas.outline_sz(outline.border->size);
            canvas.outline(outline.shape); /// this needs to work with vshape, or border
        }
    }

    vec2d offset() {
        node *n = Element::data->parent;
        vec2d  o = { 0, 0 };
        while (n) {
            props::drawing &draw = n->data->drawings[operation::child];
            rectd &rect = draw.shape.bounds();
            o  +=  rect.xy();
            o  -= n->node   ::data->scroll;
            n   = n->Element::data->parent;
        }
        return o;
    }

    array<node*> select(lambda<node*(node*)> fn) {
        array<node*> result;
        lambda<node *(node *)> recur;
        recur = [&](node* n) -> node* {
            node* r = fn(n);
            if   (r) result += r;
            /// go through mount fields mx(symbol) -> node*
            for (field<node*> &f:n->Element::data->mounts)
                if (f.value) recur(f.value);
            return null;
        };
        recur(this);
        return result;
    }

    // node(Element::data&); /// constructs default members.  i dont see an import use-case for members just yet
    mx_object(node, Element, props);

    /// does not resolve at this point, this is purely storage in element with a static default data pointer
    /// we do not set anything in data
    node(type_t type, initial<arg> args) : Element(type, args), data(defaults<props>()) { }

    node *root() const {
        node  *pe = (node*)this;
        while (pe) {
            if (!pe->Element::data->parent) return pe;
            pe = pe->Element::data->parent;
        }
        return null;
    }

    style *fetch_style() const { return  root()->Element::data->root_style; } /// fetch data from Element's accessor
    
    //Device *dev() const {
    //    return null;
    //}

    inline size_t count(memory *symbol) {
        for (Element &c: *Element::data->children)
            if (c.data->id == symbol)
                return 1;
        ///
        return 0;
    }

    node *select_first(lambda<node*(node*)> fn) {
        lambda<node*(node*)> recur;
        recur = [&](node* n) -> node* {
            node* r = fn(n);
            if   (r) return r;
            for (field<node*> &f: n->Element::data->mounts) {
                if (!f.value) continue;
                node* r = recur(f.value);
                if   (r) return r;
            }
            return null;
        };
        return recur(this);
    }

    void exec(lambda<void(node*)> fn) {
        lambda<node*(node*)> recur;
        recur = [&](node* n) -> node* {
            fn(n);
            for (field<node*> &f: n->Element::data->mounts)
                if (f.value) recur(f.value);
            return null;
        };
        recur(this);
    }

    /// test this path in compositor
    virtual Element update() {
        return *this;
    }
};

struct style:mx {
    /// construct with code
    style(str  code);
    style(path css) : style(css.read<str>()) { }

    struct qualifier:mx {
        struct members {
            str       type;
            str       id;
            str       state; /// member to perform operation on (boolean, if not specified)
            str       oper;  /// if specified, use non-boolean operator
            str       value;
        };
        mx_object(qualifier, mx, members);
    };

    ///
    struct transition:mx {
        enums(curve, none, 
            "none, linear, ease-in, ease-out, cubic-in, cubic-out",
             none, linear, ease_in, ease_out, cubic_in, cubic_out);
        
        struct members {
            curve ct; /// curve-type, or counter-terrorist.
            scalar<nil, duration> dur;
        };

        mx_object(transition, mx, members);

        inline real pos(real tf) const {
            real x = math::clamp<real>(tf, 0.0, 1.0);
            switch (style::transition::curve::etype(data->ct)) {
                case curve::none:      x = 1.0;                    break;
                case curve::linear:                                break; // /*x = x*/  
                case curve::ease_in:   x = x * x * x;              break;
                case curve::ease_out:  x = x * x * x;              break;
                case curve::cubic_in:  x = x * x * x;              break;
                case curve::cubic_out: x = 1 - std::pow((1-x), 3); break;
            };
            return x;
        }

        template <typename T>
        inline T operator()(T &fr, T &to, real value) const {
            constexpr bool transitions = transitionable<T>();
            if constexpr (transitions) {
                real x = pos(value);
                real i = 1.0 - x;
                return (fr * i) + (to * x);
            } else
                return to;
        }

        transition(str s) : transition() {
            if (s) {
                array<str> sp = s.split();
                size_t    len = sp.length();
                ///
                if (len == 2) {
                    /// 0.5s ease-out [things like that]
                    data->dur = sp[0];
                    data->ct  = curve(sp[1]).value;
                } else if (len == 1) {
                    doubly<memory*> &sym = curve::symbols();
                    if (s == str(sym[0])) {
                        /// none
                        data->ct  = curve::none;
                    } else {
                        /// 0.2s
                        data->dur = sp[0];
                        data->ct  = curve::linear; /// linear is set if any duration is set; none = no transition
                    }
                } else
                    console.fault("transition values: none, count type, count (seconds default)");
            }
        }
        ///
        operator  bool() { return data->ct != curve::none; }
        bool operator!() { return data->ct == curve::none; }
    };

    /// Element or style block entry
    struct entry:mx {
        struct edata {
            mx              member;
            str             value;
            transition      trans;
        };
        mx_object(entry, mx, edata);
    };

    ///
    struct block:mx {
        struct bdata {
            mx                       parent; /// pattern: reduce type rather than create pointer to same type in delegation
            doubly<style::qualifier> quals;  /// an array of qualifiers it > could > be > this:state, or > just > that [it picks the best score, moves up for a given node to match style in]
            doubly<style::entry>     entries;
            doubly<style::block>     blocks;
        };
        
        ///
        mx_object(block, mx, bdata);
        
        ///
        inline size_t count(str s) {
            for (entry &p:data->entries)
                if (p->member == s)
                    return 1;
            return 0;
        }

        ///
        inline entry *b_entry(mx member_name) {
            for (entry &p:data->entries)
                if (p->member == member_name)
                    return (entry*)&(mx&)p; /// this is how you get the pointer to the thing
            return null;
        }

        size_t score(node *n) {
            double best_sc = 0;
            for (qualifier &q:data->quals) {
                qualifier::members &qd = *q;
                bool    id_match  = qd.id    &&  qd.id == n->data->id;
                bool   id_reject  = qd.id    && !id_match;
                bool  type_match  = qd.type  &&  strcmp((symbol)qd.type.cs(), (symbol)n->mem->type->name) == 0; /// class names are actual type names
                bool type_reject  = qd.type  && !type_match;
                bool state_match  = qd.state &&  n->data->istates[qd.state];
                bool state_reject = qd.state && !state_match;
                ///
                if (!id_reject && !type_reject && !state_reject) {
                    double sc = size_t(   id_match) << 1 |
                                size_t( type_match) << 0 |
                                size_t(state_match) << 2;
                    best_sc = math::max(sc, best_sc);
                }
            }
            return best_sc;
        };

        /// each qualifier is defined as it, and all of the blocked qualifiers below.
        /// there are more optimal data structures to use for this matching routine
        /// state > state2 would be a nifty one, no operation indicates bool, as-is current normal syntax
        double match(node *from) {
            block          &bl = *this;
            double total_score = 0;
            int            div = 0;
            node            *n = from;
            ///
            while (bl && n) {
                bool   is_root   = !bl->parent;
                double score     = 0;
                ///
                while (n) { ///
                    auto sc = bl.score(n);
                    n       = n->Element::data->parent;
                    if (sc == 0 && is_root) {
                        score = 0;
                        break;
                    } else if (sc > 0) {
                        score += double(sc) / ++div;
                        break;
                    }
                }
                total_score += score;
                ///
                if (score > 0) {
                    /// proceed...
                    bl = block(bl->parent);
                } else {
                    /// style not matched
                    bl = null;
                    total_score = 0;
                }
            }
            return total_score;
        }

        ///
        inline operator bool() { return data->quals || data->entries || data->blocks; }
    };

    static inline map<style> cache;

    struct sdata {
        array<block>      root;
        map<array<block>> members;
    };

    mx_object(style, mx, sdata);

    array<block> &members(mx &s_member) {
        return data->members[s_member];
    }

    ///
    void cache_members();

    /// load .css in style res
    static void init(path res = "style") {
        res.resources({".css"}, {},
            [&](path css_file) -> void {
                for_class(css_file.cs());
            });
    }

    static style load(path p);
    static style for_class(symbol);
};

/// defines a component with a restricted subset and props initializer_list.
/// this macro is used for template purposes and bypasses memory allocation.
/// the actual instances of the component are managed by the composer.
/// -------------------------------------------
#define component(C, B, D) \
    using parent_class   = B;\
    using context_class  = C;\
    using intern         = D;\
    intern* state;\
    C(memory*         mem) : B(mem), state(mx::data<D>()) { }\
    C(initial<arg>  props) : B(typeof(C), props) { }\
    C(mx                o) : C(o.mem->grab())  { }\
    C()                    : C(mx::alloc<C>()) { }\
    intern    &operator *() { return *state; }\
    intern    *operator->() { return  state; }\
    intern    *operator &() { return  state; }\
    operator     intern *() { return  state; }\
    operator     intern &() { return *state; }

style::entry *prop_style(node &n, prop *member);

//typedef node* (*FnFactory)();
using FnFactory = node*(*)();

/// a type registered enum, with a default value
enums(rendition, none,
    "none, shader, wireframe",
     none, shader, wireframe);

using AssetUtil    = array<Asset>;

//void push_pipeline(Device *dev, PipelineData &pipe);

template <typename V>
struct object:node {
    /// our members
    struct members {
      //construction    plumbing;
        str             model     = "";
        str             skin      = "";
        states<Asset>   assets    = { Asset::Color };
        Shaders         shaders   = { "*=main" };
      //UniformData     ubo;
      //VAttribs        attr      = { VA::Position, VA::UV, VA::Normal };
        rendition       render    = { rendition::shader };

        /// our interface
        doubly<prop> meta() {
            return {
                prop { "model",     model   },
                prop { "skin",      skin    },
                prop { "assets",    assets  },
                prop { "shaders",   shaders },
              //prop { "ubo",       ubo     },
              //prop { "attr",      attr    },
                prop { "render",    render  }
            };
        }
    };

    /// make a node_constructors
    mx_object(object, node, members)
    
    /// change management, we probably have to give prev values in a map.
    void changed(doubly<prop> list) {
        // needs new model interface through vkengine
        //data->plumbing = Model<Vertex>(data->model, data->skin, data->ubo, data->attr, data->assets, data->shaders);
    }

    /// rendition of pipes
    Element update() {
        //if (data->plumbing)
        //    for (auto &[pipe, name]: data->plumbing.map())
        //        push_pipeline(dev(), pipe);
        return node::update();
    }
};

///
struct button:node {
    enums(behavior, push,
        "push, label, toggle, radio",
         push, label, toggle, radio); /// you must make aliases anyway, friend
    
    struct props {
        button::behavior behavior;
    };
    component(button, node, props);
};

#if 0

/// bind-compatible List component for displaying data from models or static (type cases: array)
struct list_view:node {
    struct Column:mx {
        struct cdata {
            str     id     = null;
            real    final  = 0;
            real    value  = 1.0;
            bool    scale  = true;
            xalign  align  = xalign::left;
        };
        
        mx_object(Column, mx, cdata);

        Column(str id, real scale = 1.0, xalign ax = xalign::left) : Column() {
            data->id    = id;
            data->value = scale;
            data->scale = true;
            data->align = ax;
        }
        
        Column(str id, int size, xalign ax = xalign::left) : Column() {
            data->id    = id;
            data->value = size;
            data->scale = false;
            data->align = ax;
        }
    };
    
    array<Column> columns;
    using Columns = array<Column>;
    
    struct Members {
        ///
        struct Cell {
            mx        id;
            str       text;
            rgba8     fg;
            rgba8     bg;
            alignment va;
        } cell;
        
        struct ColumnConf:Cell {
            Columns   ids;
        } column;

        rgba8 odd_bg;

        doubly<prop> meta() {
            return {
                prop { *this, "cell-fg",    cell.fg    }, /// designer gets to set app-defaults in css; designer gets to know entire component model; designer almost immediately regrets
                prop { *this, "cell-fg",    cell.fg    },
                prop { *this, "cell-bg",    cell.bg    },
                prop { *this, "odd-bg",     odd_bg     },
                prop { *this, "column-fg",  column.fg  },
                prop { *this, "column-bg",  column.bg  },
                prop { *this, "column-ids", column.ids }
            };
        }
    } m;
    
    void update_columns() {
        columns       = array<Column>(m.column.ids);
        double  tsz   = 0;
        double  warea = node::std.drawings[operation::fill].shape.w();
        double  t     = 0;
        
        for (Column::data &c: columns)
            if (c.scale)
                tsz += c.value;
        ///
        for (Column::data &c: columns)
            if (c.scale) {
                c.value /= tsz;
                t += c.value;
            } else
                warea = math::max(0.0, warea - c.value);
        ///
        for (Column::data &c: columns)
            if (c.scale)
                c.final = math::round(warea * (c.value / t));
            else
                c.final = c.value;
    }
    
    void draw(gfx &canvas) {
        update_columns();
        node::draw(canvas);

        members::drawing &fill    = std.drawings[operation::fill];
        members::drawing &image   = std.drawings[operation::image];
        members::drawing &text    = std.drawings[operation::text];
        members::drawing &outline = std.drawings[operation::outline]; /// outline is more AR than border.  and border is a bad idea, badly defined and badly understood. outline is on the 0 pt.  just offset it if you want.
        

        /// standard bind (mx) is just a context lookup, key to value in app controller; filters applied during render
        mx data = context(node::std.bind);
        if (!data) return; /// use any smooth bool operator

        ///
        rectd &rect   = node::std.drawings[operation::fill].rect();
        rectd  h_line = { rect.x, rect.y, rect.w, 1.0 };

        /// would be nice to just draw 1 outline here, canvas should work with that.
        auto draw_rect = [&](rgba8 &c, rectd &h_line) {
            rectr  r   = h_line;
            shape vs  = r; /// todo: decide on final xy scaling for text and gfx; we need a the same unit scales
            canvas.push();
            canvas.color(c);
            canvas.outline(vs);
            canvas.pop();
        };
        Column  nc = null;
        auto cells = [&] (lambda<void(Column::data &, rectd &)> fn) {
            if (columns) {
                v2d p = h_line.xy();
                for (Column::data &c: columns) {
                    double w = c.final;
                    rectd cell = { p.x, p.y, w - 1.0, 1.0 };
                    fn(c, cell);
                    p.x += w;
                }
            } else
                fn(nc, h_line);
        };

        ///
        auto pad_rect = [&](rectd r) -> rectd {
            return {r.x + 1.0, r.y, r.w - 2.0, r.h};
        };
        
        /// paint column text, fills and outlines
        if (columns) {
            bool prev = false;
            cells([&](Column::data &c, rectd &cell) {
                //var d_id = c.id;
                str label = "n/a";//context("resolve")(d_id); -- c
                canvas.color(text.color);
                canvas.text(label, pad_rect(cell), { xalign::middle, yalign::middle }, { 0, 0 }, true);
                if (prev) {
                    rectd cr = cr;
                    draw_rect(outline.border.color(), cr);
                } else
                    prev = true;
            });
            rectd rr = { h_line.x - 1.0, h_line.y + 1.0, h_line.w + 2.0, 0.0 }; /// needs to be in props
            draw_rect(outline.border.color(), rr);
            h_line.y += 2.0; /// this too..
        }
        
        /// paint visible rows
        array<mx> d_array(data.grab());
        double  sy = std.scroll.data.y;
        int  limit = sy + fill.h() - (columns ? 2.0 : 0.0);
        for (int i = sy; i < math::min(limit, int(d_array.length())); i++) {
            mx &d = d_array[i];
            cells([&](Column::data &c, rectd &cell) {
                str s = c ? str(d[c.id]) : str(d);
                rgba8 &clr = (i & 1) ? m.odd_bg : m.cell.bg;
                canvas.color(clr);
                canvas.fill(cell);
                canvas.color(text.color);
                canvas.text(s, pad_rect(cell), { xalign::left, yalign::middle }, {0,0});
            });
            r.y += r.h;
        }
    }
};
#endif



#pragma once

struct composer:mx {
    ///
    struct cdata {
        node         *root;
        struct vk_interface *vk;
        //fn_render     render;
        lambda<Element()> render;
        map<mx>       args;
    };
    ///
    mx_object(composer, mx, cdata);
    ///
    array<node *> select_at(vec2d cur, bool active = true) {
        array<node*> inside = data->root->select([&](node *n) {
            real           x = cur.x, y = cur.y;
            vec2d          o = n->offset();
            vec2d        rel = cur + o;
            node::props &std = **n;
            /// it could be Element::drawing
            node::props::drawing &draw = std.drawings[operation::fill];

            bool in = draw.shape.contains(rel);
            n->node::data->cursor = in ? vec2d(x, y) : vec2d(-1, -1);
            return (in && (!active || !std.active)) ? n : null;
        });

        array<node*> actives = data->root->select([&](node *n) -> node* {
             node::props &std = **n;
            return (active && std.active) ? n : null;
        });

        array<node*> result = array<node *>(size_t(inside.length()) + size_t(actives.length()));

        for (node *i: inside)
            result += i;
        for (node *i: actives)
            result += i;
        
        return result;
    }
    ///
};

struct app:composer {
    struct adata {
        window win;
        gfx    canvas;
        lambda<Element(app&)> app_fn;
    };

    mx_object(app, composer, adata);

    app(lambda<Element(app&)> app_fn) : app() {
        data->app_fn = app_fn;
    }
    ///
    int run();

    operator int() { return run(); }
};

using AppFn = lambda<Element(app&)>;
}
