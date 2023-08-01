#pragma once

#include <async/async.hpp>
#include <net/net.hpp>
#include <image/image.hpp>
#include <media/media.hpp>
#include <vk/vk.hpp>
#include <vkh/vkh.h>

struct GLFWwindow;

namespace ion {

struct Device;
struct Texture;

using Path    = path;
using Image   = image;
using strings = array<str>;
using Assets  = map<Texture *>;

struct text_metrics:mx {
    struct tmdata {
        real        w, h;
        real        ascent, descent;
        real        line_height, cap_height;
        type_register(tmdata);
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
            Rectd       bounds; /// if the ops change the bounds must be re-eval'd
            doubly<mx>  ops;
            vec2d       mv;
            type_register(sdata);
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
        inline void arc(Arc a) { data->ops += a; }
        operator        bool() { bounds(); return bool(data->bounds); }
        bool       operator!() { return !operator bool(); }
        shape::sdata *handle() { return data; }
    };

    struct border_data {
        real size, tl, tr, bl, br;
        rgba8 color;
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
        type_register(border_data);
    };
    /// graphics::border is an indication that this has information on color
    using border = sp<border_data>;
};

enums(nil, none, "none", none);

enums(xalign, undefined,
    "undefined, left, middle, right, width, x",
     undefined, left, middle, right, width, x);
    
enums(yalign, undefined,
    "undefined, top, middle, bottom, line, height, y",
     undefined, top, middle, bottom, line, height, y);

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
        type_register(sdata);
    };

    mx_object(scalar, mx, sdata);
    
    scalar(P p, S s) : scalar() {
        data->prefix = p;
        data->suffix = s;
    }

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
        str tr = s.trim();
        size_t len = tr.len();
        bool in_symbol = isalpha(tr[0]) || tr[0] == '_' || tr[0] == '%';
        array<str> sp   = tr.split([&](char &c) -> int {
            int split   = 0;
            bool symbol = isalpha(c) || (c == '_' || c == '%'); /// todo: will require hashing the enums by - and _, but for now not exposed as issue
            ///
            if (c == 0 || in_symbol != symbol) {
                in_symbol = symbol;
                split     = c == ' ' ? 1 : 2; // 2 == keep char
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
            } else if (sp.len() == 1) {
                data->prefix = P(sp[0]);
                data->scale  = 0.0;
            } else {
                data->prefix = P(sp[0]);
                data->scale  =   sp[1].real_value<real>();
                if (sp.len() >= 3 && sp[2]) {
                    data->suffix = S(sp[2]);
                    if constexpr (identical<S, distance>())
                        data->is_percent = data->suffix == distance::percent;
                }
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
        type_register(adata);
    };

    ///
    mx_object(alignment, mx, adata);

    alignment(scalar<xalign, distance> sx, scalar<yalign, distance> sy) : alignment() {
        data->x = sx;
        data->y = sy;
    }

    alignment(cstr cs) : alignment() {
        array<str> sp = str(cs).split();
        assert(sp.len() == 2);
        data->x = sp[0];
        data->y = sp[1];
    }
    ///
    inline alignment(str  x, str  y) : alignment() {
        data->x = x;
        data->y = y;
    }

    inline alignment(real x, real y) : alignment() {
        data->x = { xalign::left, x, distance::px };
        data->y = { yalign::top,  y, distance::px };
    }

    inline operator vec2d() { return vec2d { real(data->x), real(data->y) }; }

    /// compute x and y coordinates given alignment cdata, and parameters for rect and offset
    vec2d plot(rectd &win) {
        vec2d rs;
        /// todo: direct enum operator does not seem to works
        /// set x coordinate
        switch (xalign::etype(xalign(data->x))) {
            case xalign::undefined:
            case xalign::x:
            case xalign::width:
            case xalign::left:      rs.x = data->x(win.x,             win.w); break;
            case xalign::middle:    rs.x = data->x(win.x + win.w / 2, win.w); break;
            case xalign::right:     rs.x = data->x(win.x + win.w,     win.w); break;
        }
        /// set y coordinate
        switch (yalign::etype(yalign(data->y))) {
            case yalign::undefined:
            case yalign::y:
            case yalign::height:
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
        alignment tl = alignment { scalar<xalign, distance> { xalign::left,   distance::px },
                                   scalar<yalign, distance> { yalign::top,    distance::px }};
        alignment br = alignment { scalar<xalign, distance> { xalign::right,  distance::px },
                                   scalar<yalign, distance> { yalign::bottom, distance::px }};
        type_register(rdata);
    };

    ///
    mx_object(region, mx, rdata);
    
    /// just two types supported, 4 and 1 component
    region(str s) : region() {
        array<str> a = s.trim().split();
        if (a.len() >= 4) {
            data->tl = alignment { a[0], a[1] };
            data->br = alignment { a[2], a[3] };
        } else if (a.len() >= 1) {
            data->tl = alignment { a[0], a[0] };
            data->br = alignment { a[0], a[0] };
        }
    }
    region(cstr s):region(str(s)) { }

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
            num                 unicode;
            num                 scan_code;
            states<keyboard>    modifiers;
            bool                repeat;
            bool                up;
            type_register(kdata);
        };
        mx_object(key, mx, kdata);
    };
};

struct event:mx {
    ///
    struct edata {
        user::chr               unicode;
        user::key               key;
        vec2d                   wheel_delta;
        vec2d                   cursor;
        states<mouse>           buttons;
        states<keyboard>        modifiers;
        size                    resize;
        mouse::etype            button_id;
        bool                    prevent_default;
        bool                    stop_propagation;
        type_register(edata);
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

struct Element:mx {
    struct edata {
        type_t                  type;     /// type given
        memory*                 id;       /// identifier 
        ax                      args;     /// arguments
        array<Element>          children; /// children elements (if provided in children { } pair<mx,mx> inherited data struct; sets key='children')
        node*                   instance; /// node instance is 1:1
        map<node*>              mounts;   /// store instances of nodes in element data, so the cache management can go here where element turns to node
        node*                   parent;
        style*                  root_style; /// applied at root for multiple style trees across multiple apps

        bool                    captured;
        bool                    focused;
        bool                    hover;
        bool                    active;
        vec2d                   cursor;

        doubly<prop> meta() {
            return {
                prop { "captured", captured },
                prop { "focused",  focused  },
                prop { "hover",    hover    },
                prop { "active",   active   },
                prop { "children", children }
            };
        }

        operator bool() {
            return (type || children.len() > 0);
        }

        type_register(edata);
    };

    mx_object(Element, mx, edata);

    static memory *args_id(type_t type, initial<arg> args) {
        static memory *m_id = memory::symbol("id"); /// im a token!
        for (auto &a:args)
            if (a.key.mem == m_id)
                return a.value.mem;
        
        return memory::symbol(symbol(type->name));
    }

    Element(array<Element> ch) : Element() {
        data->children = ch;
    }

    Element(type_t type, initial<arg> args) : Element(
        edata { type, args_id(type, args), args }
    ) { }

    /// used below in each(); a simple allocation of Elements
    Element(type_t type, size_t sz) : Element(
        edata { type, null, null, array<Element>(sz) }
    ) { }

    template <typename T>
    static Element each(array<T> a, lambda<Element(T &v)> fn) {
        Element res(typeof(array<Element>), a.length());
        for (auto &v:a) {
            Element ve = fn(v);
            if (ve) res.data->children += ve;
        }
        return res;
    }
    
    template <typename K, typename V>
    static Element each(map<V> m, lambda<Element(K &k, V &v)> fn) {
        Element res(typeof(map<Element>), m.size);
        if (res.data->children)
        for (auto &[v,k]:m) {
            Element r = fn(k, v);
            if (r) res.data->children += r;
        }
        return res;
    }

    /// the element can create its instance.. that instance is a sub-class of Element too so we use a forward
    struct node *new_instance() {
        return (struct node*)data->type->functions->alloc_new((struct node*)null, (struct node*)null);
    }
};

struct ch:pair<mx,mx> {
    ch(array<Element> a) {
        key   = "children";
        value = a;
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
        type_register(ldata);
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
/// need to implement an assign with a singular callback.  these can support more than one, though.  in a component tree you can use multiple with use of context
struct dispatch:mx {
    struct ddata {
        doubly<listener> listeners;
        type_register(ddata);
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
            type_register(gdata);
        };
        mx_object(group, mx, gdata);
    };

    struct members {
        array<V>   vbo;
        map<group> groups;
        type_register(members);
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

struct font:mx {
    struct fdata {
        real sz = 16;
        str  name = "Avenir-Bold";
        bool loaded = false;
        type_register(fdata);
    };

    mx_object(font, mx, fdata);

    font(real sz, str name) : font(fdata { sz, name }) { }

    font(str s):font() {
        array<str> sp = s.split();
        assert(sp.len() == 2);
        data->sz  = sp[0].real_value<real>();
        data->name = sp[1];
    }

    path get_path() {
        assert(data->name);
        return fmt { "fonts/{0}.ttf", { data->name } };
    }

    font update_sz(real sz) {
        return fdata { sz, data->name };
    }

            operator bool()    { return  data->sz; }
    bool    operator!()        { return !data->sz; }
};

struct gfx_memory;

/// gfx: graphics in either vkvg or text buffer
struct gfx:mx {
    /// data is single instanced on this cbase, and the draw_state is passed in as type for the cbase, which atleast makes it type-unique
    mx_declare(gfx, mx, gfx_memory);

    /// create with a window (indicated by a name given first)
    gfx(VkEngine e, VkhPresenter vkh_renderer);

    VkEngine        engine();
    Device          device();
    void           resized();
    void          defaults();
    text_metrics   measure(str text);
    str    format_ellipsis(str text, real w, text_metrics &tm_result);
    void     draw_ellipsis(str text, real w);
    void             image(ion::image img, graphics::shape sh, alignment align, vec2d offset, vec2d source);
    void              push();
    void               pop();
    void              text(str text, Rect<double> rect, alignment align, vec2d offset, bool ellip);
    void              clip(graphics::shape cl);
    VkhImage       texture();
    void             flush();
    void             clear(rgba8 c);
    void              font(ion::font &f);
    void               cap(graphics::cap  c);
    void              join(graphics::join j);
    void         translate(vec2d    tr);
    void             scale(vec2d    sc);
    void             scale(real     sc);
    void            rotate(real   degs);
    void             color(rgba8 &color);
    void              fill(rectd    &p);
    void              fill(graphics::shape  p);
    void          gaussian(vec2d sz, graphics::shape c);
    void           outline(graphics::shape p);
    str           get_char(int x, int y);
    str         ansi_color(rgba8 &c, bool text);
    ion::image    resample(ion::size sz, real deg, graphics::shape view, vec2d axis);
};

struct style:mx {
    /// qualifier for style block
    struct qualifier {
        type_t    ty; /// its useful to look this up (which we cant by string)
        str       type;
        str       id;
        str       state; /// member to perform operation on (boolean, if not specified)
        str       oper;  /// if specified, use non-boolean operator
        str       value;
        type_register(qualifier);
    };

    ///
    struct transition {
        enums(curve, none, 
            "none, linear, ease-in, ease-out, cubic-in, cubic-out",
             none, linear, ease_in, ease_out, cubic_in, cubic_out);
        
        curve ct; /// curve-type, or counter-terrorist.
        scalar<nil, duration> dur;
        type_register(transition);

        transition(null_t n = null) { }

        inline real pos(real tf) const {
            real x = math::clamp<real>(tf, 0.0, 1.0);
            switch (ct.value) {
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
                    dur = sp[0];
                    ct  = curve(sp[1]).value;
                } else if (len == 1) {
                    doubly<memory*> &sym = curve::symbols();
                    if (s == str(sym[0])) {
                        /// none
                        ct  = curve::none;
                    } else {
                        /// 0.2s
                        dur = sp[0];
                        ct  = curve::linear; /// linear is set if any duration is set; none = no transition
                    }
                } else
                    console.fault("transition values: none, count type, count (seconds default)");
            }
        }
        ///
        operator  bool() { return ct != curve::none; }
        bool operator!() { return ct == curve::none; }
    };

    struct block;

    /// Element or style block entry
    struct entry {
        mx              member;
        str             value;
        transition      trans; 
        block          *bl; /// block location would be nice to have; you compute a list of entries and props once and apply on update
        mx             *mx_instance;  ///
        void           *raw_instance; /// we run type->from_string(null, value.cs()), on style load.  we need not do this every time
    };

    ///
    struct block {
        block*             parent; /// pattern: reduce type rather than create pointer to same type in delegation
        doubly<qualifier*> quals;  /// an array of qualifiers it > could > be > this:state, or > just > that [it picks the best score, moves up for a given node to match style in]
        map<entry*>        entries;
        doubly<block*>     blocks;
        array<type_t>      types; // if !types then its all types.

        size_t score(node *n);

        /// each qualifier is defined as it, and all of the blocked qualifiers below.
        /// there are more optimal data structures to use for this matching routine
        /// state > state2 would be a nifty one, no operation indicates bool, as-is current normal syntax
        double match(node *from);

        ///
        inline operator bool() { return quals || entries || blocks; }
    };

    using style_map = map<array<entry*>>;

    struct impl {
        array<block*>       root;
        map<array<block*>>  members;
        bool                loaded;
        style_map        compute(node *dst);
        entry        *best_match(node *n, prop *member, array<entry*> &entries);
        bool          applicable(node *n, prop *member, array<entry*> &all);
        void                load(str code);

        /// optimize member access by caching by member name, and type
        void cache_members();

        type_register(impl);
    };

    /// load all style sheets in resources
    static style init();
    mx_object(style, mx, impl);
};

/// no reason to have style separated in a single app
/// if we have multiple styles, just reload
template <> struct is_singleton<style> : true_type { };

enums(operation, fill,
    "fill, image, outline, text, child",
     fill, image, outline, text, child);

struct node:Element {
    /// standard props
    struct props {
        style::style_map style_avail; /// properties outside of meta are effectively internal state only; it can be labelled as such
        
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

        /// padding does not seem needed with this set of members and enumerable drawing types
        /// not ALL are used for all types.  However its useful to have a generic data structure for binding and function
        /// not sure if overflow should be used for all of these.  that might be much to deal with
        struct drawing {
            rgba8               color;
            alignment           align;
            image               img;
            region              area;
            vec4d               radius;
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

        inline rectd shape_bounds() {
            return rectd((rectd&)shape());
        }

        ///
        str                 id;         ///
        drawing             drawings[operation::count];
        int                 tab_index;  /// if 0, this is default; its all relative numbers we dont play absolutes.
        vec2d               cursor;     /// set relative to the area origin
        vec2d               scroll = {0,0};
        std::queue<fn_t>    queue;      /// this is an animation queue
        mx                  content;
        double              opacity = 1.0;
        rectd               bounds;     /// local coordinates of this control, so x and y are 0 based
        ion::font           font;

        doubly<prop> meta() const {
            return {
                prop { "id",        id        }, /// move to Element?
                prop { "on-hover",  ev.hover  }, /// must give member-parent-origin (std) to be able to 're-apply' somewhere else
                prop { "on-out",    ev.out    },
                prop { "on-down",   ev.down   },
                prop { "on-up",     ev.up     },
                prop { "on-key",    ev.key    },
                prop { "on-focus",  ev.focus  },
                prop { "on-blur",   ev.blur   },
                prop { "on-cursor", ev.cursor },
                prop { "on-hover",  ev.hover  },
                prop { "content",   content   },
                prop { "opacity",   opacity   },
                prop { "font",      font },

                prop { "image-src",      drawings[operation::image]  .img  },

                prop { "fill-area",      drawings[operation::fill]   .area },
                prop { "image-area",     drawings[operation::image]  .area },
                prop { "outline-area",   drawings[operation::outline].area },
                prop { "text-area",      drawings[operation::text]   .area },
                prop { "child-area",     drawings[operation::child]  .area },

                prop { "fill-color",     drawings[operation::fill]   .color },
                prop { "image-color",    drawings[operation::image]  .color },
                prop { "outline-color",  drawings[operation::outline].color },
                prop { "text-color",     drawings[operation::text]   .color },

                prop { "fill-radius",    drawings[operation::fill]   .radius },
                prop { "image-radius",   drawings[operation::image]  .radius },
                prop { "outline-radius", drawings[operation::outline].radius },

                prop { "fill-border",    drawings[operation::fill]   .border },
                prop { "image-border",   drawings[operation::image]  .border },
                prop { "outline-border", drawings[operation::outline].border },
                prop { "text-border",    drawings[operation::text]   .border },

                prop { "image-align",    drawings[operation::image]  .align },
                prop { "child-align",    drawings[operation::child]  .align },
                prop { "text-align",     drawings[operation::text]   .align },
            };
        }
        type_register(props);
    };

    /// context works by looking at meta() on the context types polymorphically, and up the parent nodes to the root context (your app)
    template <typename T>
    T *context(str id) {
        node *n = (node*)this;
        while (n) {
            for (type_t type = n->mem->type; type; type = type->parent) {
                if (!type->meta)
                    continue;
                prop* member = null;
                u8*   addr   = get_member_address(n->mem->type, n, id, member);
                if (addr) {
                    assert(member->member_type == typeof(T));
                    return (T*)addr;
                }
            }
            n = n->Element::data->parent;
        }
        return null;
    }

    rgba8 color_with_opacity(rgba8 &input) {
        rgba8 result = input;
        node  *n = Element::data->parent;
        double o = 1.0;
        while (n) {
            o *= n->data->opacity;
            n  = n->Element::data->parent;
        }
        result.a = math::round(result.a * o);
        return result;
    }

    virtual void draw(gfx& canvas);

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

    mx_object(node, Element, props);

    /// does not resolve at this point, this is purely storage in element with a static default data pointer
    /// we do not set anything in data
    node(type_t type, initial<arg> args) : Element(type, args), data(defaults<props>()) { }

    node(symbol id, array<Element> ch):node() {
        data->id = id;
        Element::data->children = ch;
    }

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
        for (Element &c: Element::data->children)
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

    /// test this path in composer
    virtual Element update() {
        return *this;
    }
};

/// defines a component with a restricted subset and props initializer_list.
/// this macro is used for template purposes and bypasses memory allocation.
/// the actual instances of the component are managed by the composer.
/// -------------------------------------------
#define component(C, B, D) \
    using parent_class   = B;\
    using context_class  = C;\
    using intern         = D;\
    static const inline type_t ctx_t  = typeof(C);\
    static const inline type_t data_t = typeof(D);\
    intern* state;\
    C(memory*         mem) : B(mem), state(mx::data<D>()) { }\
    C(type_t ty, initial<arg>  props) : B(ty,        props), state(defaults<intern>()) { }\
    C(initial<arg>  props) :            B(typeof(C), props), state(defaults<intern>()) { }\
    C(mx                o) : C(o.mem->grab())  { }\
    C()                    : C(mx::alloc<C>()) { }\
    intern    &operator *() { return *state; }\
    intern    *operator->() { return  state; }\
    intern    *operator &() { return  state; }\
    operator     intern *() { return  state; }\
    operator     intern &() { return *state; }\
    type_register(C);

//typedef node* (*FnFactory)();
using FnFactory = node*(*)();

using AssetUtil    = array<Asset>;

//void push_pipeline(Device *dev, Pipeline &pipe);

template <typename V>
struct object:node {
    /// our members
    struct members {
      //construction    plumbing;
        str             model     = "";
        str             skin      = "";
        states<Asset>   assets    = { }; /// if this is blank, it should load all; likewise with shader it should use default
      //Shaders         shaders   = { "*=main" };
      //UniformData     ubo;
      //VAttribs        attr      = { VA::Position, VA::UV, VA::Normal };
        Rendition       render    = { Rendition::shader };

        /// our interface
        doubly<prop> meta() {
            return {
                prop { "model",     model   },
                prop { "skin",      skin    },
                prop { "assets",    assets  },
              //prop { "shaders",   shaders },
              //prop { "ubo",       ubo     },
              //prop { "attr",      attr    },
                prop { "render",    render  }
            };
        }
        type_register(members);
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
struct Button:node {
    enums(Behavior, push,
        "push, label, toggle, radio",
         push, label, toggle, radio);
    
    struct props {
        Button::Behavior behavior;
        type_register(props);
    };
    component(Button, node, props);

    Element update() {
        return node::update();
    }
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
            type_register(cdata);
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
                prop { "cell-fg",    cell.fg    }, /// designer gets to set app-defaults in css; designer gets to know entire component model; designer almost immediately regrets
                prop { "cell-fg",    cell.fg    },
                prop { "cell-bg",    cell.bg    },
                prop { "odd-bg",     odd_bg     },
                prop { "column-fg",  column.fg  },
                prop { "column-bg",  column.bg  },
                prop { "column-ids", column.ids }
            };
        }
        type_register(Members);
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

struct composer:mx {
    ///
    struct cdata {
        node         *root_instance;
        struct vk_interface *vk;
        //fn_render     render;
        lambda<Element()> render;
        map<mx>       args;
        ion::style    style;
        type_register(cdata);
    };
    
    ///
    mx_object(composer, mx, cdata);
    
    /// called from app
    void update_all(Element e);

    /// called recursively from update_all -> update(), also called from node generic render (node will need a composer data member)
    static void update(composer::cdata *composer, node *parent, node *&instance, Element &e);
    
    ///
    array<node *> select_at(vec2d cur, bool active = true) {
        array<node*> inside = data->root_instance->select([&](node *n) {
            real           x = cur.x, y = cur.y;
            vec2d          o = n->offset();
            vec2d        rel = cur + o;
            Element::edata *edata = n->Element::data;
            
            node::props::drawing &draw = n->data->drawings[operation::fill];

            bool in = draw.shape.contains(rel);
            edata->cursor = in ? vec2d(x, y) : vec2d(-1, -1);
            return (in && (!active || !edata->active)) ? n : null;
        });

        array<node*> actives = data->root_instance->select([&](node *n) -> node* {
            return (active && n->Element::data->active) ? n : null;
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

struct App:composer {
    struct adata {
        GPU    win;
        gfx    canvas;
        VkEngine e;
        lambda<Element(App&)> app_fn;
        type_register(adata);
    };

    mx_object(App, composer, adata);

    App(lambda<Element(App&)> app_fn) : App() {
        data->app_fn = app_fn;
    }
    ///
    int run();
    static void resize(vec2i &sz, App *app);

    operator int() { return run(); }
};

using AppFn = lambda<Element(App&)>;
}
