#pragma once

#include <async/async.hpp>
#include <net/net.hpp>
#include <image/image.hpp>
#include <media/media.hpp>
#include <vk/vk.hpp>
#include <vkh/vkh.h>
#include <camera/camera.hpp>
#include <math.h>
#include <composer/composer.hpp>

struct GLFWwindow;

namespace ion {

struct Device;
struct Texture;

using Path    = path;
using Image   = image;
using strings = array<str>;
using Assets  = map<Texture *>;

struct text_metrics {
    real            w = 0,
                    h = 0;
    real       ascent = 0,
              descent = 0;
    real  line_height = 0,
           cap_height = 0;
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
            type_t      type;           /// type of shape (null for operations)
            Rectd       bounds;         /// boundaries, or identity of shape primitive
            doubly<mx>  ops;            /// operation list (or verbs)
            vec2d       mv;             /// movement cursor
            void*       sk_path;        /// saved sk_path; (invalidated when changed)
            void*       sk_offset;      /// applied offset; this only works for positive offset
            real        cache_offset;   /// when sk_path/sk_offset made, this is set
            real        offset;         /// current state for offset; an sk_path is not made because it may not be completed by the user
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
            data->type   = use_rect ? typeof(Rectd) : typeof(Rounded);
            data->bounds = use_rect ? Rectd(r4) : 
                                      Rectd(Rounded(r4, rx, std::isnan(ry) ? rx : ry)); /// type forwards
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

    struct border {
        real size, tl, tr, bl, br;
        rgbad color;
        inline bool operator==(const border &b) const { return b.tl == tl && b.tr == tr && b.bl == bl && b.br == br; }
        inline bool operator!=(const border &b) const { return !operator==(b); }

        /// members are always null from the memory::allocation
        border() { }
        ///
        border(str raw) : border() {
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
        type_register(border);
    };
};

enums(nil, none, "none", none);

enums(xalign, left,
    "left, middle, right, width",
     left, middle, right, width);
    
enums(yalign, top,
    "top, middle, bottom, height",
     top, middle, bottom, height);

enums(blending, undefined,
    "undefined, clear, src, dst, src-over, dst-over, src-in, dst-in, src-out, dst-out, src-atop, dst-atop, xor, plus, modulate, screen, overlay, color-dodge, color-burn, hard-light, soft-light, difference, exclusion, multiply, hue, saturation, color",
     undefined, clear, src, dst, src_over, dst_over, src_in, dst_in, src_out, dst_out, src_atop, dst_atop, Xor, plus, modulate, screen, overlay, color_dodge, color_burn, hard_light, soft_light, difference, exclusion, multiply, hue, saturation, color);

enums(filter, none,
    "none, blur, brightness, contrast, drop-shadow, grayscale, hue-rotate, invert, opacity, saturate, sepia",
     none, blur, brightness, contrast, drop_shadow, grayscale, hue_rotate, invert, opacity, saturate, sepia);

/// need interpolation with enumerable type.  the alignment itsself will represent an axis
/// alignment doesnt use the full enumerate set in xalign/yalign because its use-case is inside of the scope of that
struct alignment {
    real x, y;

    alignment(real x = 0, real y = 0) : x(x), y(y) { }

    alignment(str s) {
        array<str> a = s.split();
        str s0 = a[0];
        str s1 = a.len() > 1 ? a[1] : a[0];

        if (s0.numeric()) {
            x = s0.real_value<real>();
        } else {
            xalign xa { s0 };
            switch (xa.value) {
                case xalign::left:   x = 0.0; break;
                case xalign::middle: x = 0.5; break;
                case xalign::right:  x = 1.0; break;
                default: assert(false);       break;
            }
        }

        if (s1.numeric()) {
            y = s1.real_value<real>();
        } else {
            yalign ya { s1 };
            switch (ya.value) {
                case yalign::top:    y = 0.0; break;
                case yalign::middle: y = 0.5; break;
                case yalign::bottom: y = 1.0; break;
                default: assert(false);       break;
            }
        }
    }

    alignment(cstr cs) : alignment(str(cs)) { }

    alignment mix(alignment &b, double f) {
        return { x * (1.0 - f) + b.x * f,
                 y * (1.0 - f) + b.y * f };
    }

    type_register(alignment);
};

struct coord {
    alignment   align;
    vec2d       offset;
    xalign      x_type;
    yalign      y_type;

    coord(alignment &al, vec2d &offset, xalign &xt, yalign &yt) :
        align(al), offset(offset), x_type(xt), y_type(yt) { }

    coord(str s) {
        array<str> sp = s.split();
        str       &sx = sp[0];
        str       &sy = sp[1];
        assert(sp.len() == 2);

        x_type = str(sx[0]);
        y_type = str(sy[0]);

        str sx_offset = &sx[1];
        str sy_offset = &sy[1];

        offset = { sx_offset.real_value<real>(), sy_offset.real_value<real>() };
        
        switch (x_type.value) {
            case xalign::left:   align.x = 0.0; break;
            case xalign::middle: align.x = 0.5; break;
            case xalign::right:  align.x = 1.0; break;
            case xalign::width:  align.x = 0.0; break;
            default: break;
        }

        switch (y_type.value) {
            case yalign::top:    align.y = 0.0; break;
            case yalign::middle: align.y = 0.5; break;
            case yalign::bottom: align.y = 1.0; break;
            case yalign::height: align.y = 0.0; break;
            default: break;
        }
    }

    coord mix(coord &b, double a) {
        alignment   align  = this->align.mix(b.align, a);
        vec2d       offset = this->offset  * (1.0 - a) + b.offset  * a;
        xalign      x_type = b.x_type;
        yalign      y_type = b.y_type;
        return coord {
            align, offset, x_type, y_type
        };
    }

    vec2d plot(rectd &rect, vec2d &rel) {
        double ws = (x_type.value == xalign::width)  ? (rel.x / rect.w) : align.x;
        double hs = (y_type.value == yalign::height) ? (rel.y / rect.h) : align.y;
        return {
            rect.x + rect.w * ws + offset.x,
            rect.y + rect.h * hs + offset.y
        };
    }

    coord(cstr cs) : coord(str(cs)) { }

    type_register(coord);
};

/// good primitive for ui, implemented in basic canvas ops.
/// regions can be constructed from rects if area is static or composed in another way
struct region {
    coord tl = (cstr)"l0 t0";
    coord br = (cstr)"r0 b0";
    bool set = false;

    region() { }

    region(coord &tl, coord &br) : tl(tl), br(br), set(true) { }

    region(str s) {
        array<str> a = s.split();
        assert(a.length() == 4);
        str s0  = a[0] + " " + a[1];
        str s1  = a[2] + " " + a[3];
        tl      = s0;
        br      = s1;
        set     = true;
    }

    region(cstr s):region(str(s)) { }

    operator bool() { return set; }
    
    ///
    rectd rect(rectd &win) {
        vec2d rel = win.xy();
        return rectd { tl.plot(win, rel), br.plot(win, rel) };
    }

    region mix(region &b, double a) {
        coord m_tl = tl.mix(b.tl, a);
        coord m_tr = br.mix(b.br, a);
        return region {
            m_tl, m_tr
        };
    }

    type_register(region);
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
    inline vec2d cursor_pos()       { return  data->cursor; }
    inline bool mouse_down(mouse m) { return  data->buttons[m]; }
    inline bool mouse_up  (mouse m) { return !data->buttons[m]; }
    inline num  unicode   ()        { return  data->key->unicode; }
    inline num  scan_code ()        { return  data->key->scan_code; }
    inline bool key_down  (num u)   { return  data->key->unicode   == u && !data->key->up; }
    inline bool key_up    (num u)   { return  data->key->unicode   == u &&  data->key->up; }
    inline bool scan_down (num s)   { return  data->key->scan_code == s && !data->key->up; }
    inline bool scan_up   (num s)   { return  data->key->scan_code == s &&  data->key->up; }
};

struct Element;

struct ch:pair<mx,mx> {
    ch(array<node> a) {
        key   = "children";
        value = a;
    }
};

using fn_render = lambda<node()>;
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

struct Canvas;

struct font:mx {
    struct fdata {
        real sz = 22;
        str  name = "Sofia-Sans-Condensed";
        bool loaded = false;
        void *sk_font = null;
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

    array<double> advances(Canvas& canvas);
    array<double> advances(Canvas& canvas, str line);

            operator bool()    { return  data->sz; }
    bool    operator!()        { return !data->sz; }
};

enums(operation, fill,
    "fill, image, outline, text, child",
     fill, image, outline, text, child);

template <typename> struct simple_content : true_type { };

struct LineInfo {
    str            data;
    num            len;
    array<double>  adv;
    rectd          bounds;     /// bounds of area of the text line
    rectd          placement;  /// effective bounds of the aligned text, with y and h same as bounds
};

struct TextSel:mx {
    struct Data {
        num     column;
        num     row;
        ///
        bool operator<= (const Data &b) { return column <= b.column || row <= b.row; }
        bool operator>  (const Data &b) { return column >  b.column || row >  b.row; }

        type_register(Data);
    };
    ///
    mx_object(TextSel, mx, Data);
    ///
    bool operator<= (const TextSel &b) { return *data <= *b.data; }
    bool operator>  (const TextSel &b) { return *data >  *b.data; }
};

struct Canvas;
struct Element:node {
    /// standard props
    struct props {

        style*                  root_style; /// applied at root for multiple style trees across multiple apps
        Element*                focused;
        Element*                capt;
        bool                    capture;
        bool                    hover;
        bool                    active;
        bool                    focus;
        int                     tab_index;
        vec2d                   cursor;

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
            rgbad               color;
            rgbad               secondary;
            real                opacity;
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
        drawing             drawings[operation::count];
        rgbad               sel_color;
        rgbad               sel_background;
        vec2d               scroll = {0,0};
        std::queue<fn_t>    queue;      /// this is an animation queue
        vec2d               text_spacing = { 1.0, 1.0 }; /// kerning & line-height scales here
        mx                  content;
        double              opacity = 1.0;
        rectd               bounds;     /// local coordinates of this control, so x and y are 0 based
        rectd               fill_bounds;
        rectd               text_bounds;
        ion::font           font;
        mx                  cache_source;   /// cache of content when lines are made
        array<str>          cache_split;    
        array<LineInfo>     lines;          /// in sync with cache
        bool                editable   = false;
        bool                selectable = true;
        bool                multiline  = false;
        TextSel             sel_start, sel_end, sel_prev;

        doubly<prop> meta() const {
            return {
                prop { "on-hover",       ev.hover  },
                prop { "on-out",         ev.out    },
                prop { "on-down",        ev.down   },
                prop { "on-up",          ev.up     },
                prop { "on-key",         ev.key    },
                prop { "on-focus",       ev.focus  },
                prop { "on-blur",        ev.blur   },
                prop { "on-cursor",      ev.cursor },
                prop { "on-hover",       ev.hover  },

                prop { "editable",       editable  },
                prop { "opacity",        opacity   },
                prop { "content",        content   },
                prop { "font",           font      },

                prop { "text-spacing",   text_spacing },
                prop { "image-src",      drawings[operation::image]  .img     },

                prop { "fill-area",      drawings[operation::fill]   .area    },
                prop { "image-area",     drawings[operation::image]  .area    },
                prop { "outline-area",   drawings[operation::outline].area    },
                prop { "text-area",      drawings[operation::text]   .area    },
                prop { "child-area",     drawings[operation::child]  .area    },
  
                prop { "fill-color",     drawings[operation::fill]   .color   },
                prop { "image-color",    drawings[operation::image]  .color   },
                prop { "outline-color",  drawings[operation::outline].color   },
                prop { "text-color",     drawings[operation::text]   .color   },

                prop { "sel-color",      sel_color },
                prop { "sel-background", sel_background },

                prop { "outline-sz",     drawings[operation::outline].border.size },

                prop { "fill-opacity",   drawings[operation::fill]   .opacity },
                prop { "image-opacity",  drawings[operation::image]  .opacity },
                prop { "outline-opacity",drawings[operation::outline].opacity },
                prop { "text-opacity",   drawings[operation::text]   .opacity },

                prop { "fill-radius",    drawings[operation::fill]   .radius  },
                prop { "image-radius",   drawings[operation::image]  .radius  },
                prop { "outline-radius", drawings[operation::outline].radius  },

                prop { "fill-border",    drawings[operation::fill]   .border  },
                prop { "image-border",   drawings[operation::image]  .border  },
                prop { "outline-border", drawings[operation::outline].border  },
                prop { "text-border",    drawings[operation::text]   .border  },

                prop { "image-align",    drawings[operation::image]  .align   },
                prop { "child-align",    drawings[operation::child]  .align   },
                prop { "text-align",     drawings[operation::text]   .align   },

                prop { "capture",   capture   },
                prop { "focuse",    focus     },
                prop { "hover",     hover     },
                prop { "active",    active    },
                prop { "focus",     focus     },
                prop { "tab-index", tab_index },
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
            n = n->node::data->parent;
        }
        return null;
    }

    TextSel get_selection(vec2d pos, bool is_down) {
        rectd r = data->bounds;
        Element *n = this;
        /// localize mouse pos input to this control (should be done by the caller; why would we repeat this differently)
        while (n) {
            pos.x -= n->data->bounds.x + n->data->scroll.x;
            pos.y -= n->data->bounds.y + n->data->scroll.y;
            n      = (Element*)((node*)n)->data->parent;
        }
        ///
        TextSel res;
        real    y = data->text_bounds.y;
        num     row = 0;

        ///
        for (LineInfo &line: data->lines) {
            if (pos.y >= y && pos.y <= (y + line.bounds.h)) {
                res->row = row;
                if (pos.x < line.placement.x)
                    res->column = 0;
                else if (pos.x > line.placement.x + line.placement.w)
                    res->column = line.len;
                else {
                    bool col_set = false;
                    for (num i = 1; i <= line.len; i++) {
                        real adv0 = line.adv[i - 1];
                        real adv1 = line.adv[i];
                        real med  = (adv0 + adv1) / 2.0;
                        if ((pos.x - line.placement.x) < med) {
                            res->column = i - 1;
                            col_set = true;
                            break;
                        }
                    }
                    /// set end-of-last character
                    if (!col_set)
                        res->column = line.len;
                }
            }
            y += line.bounds.h;
            row++;
        }
        return res;
    }

    rgba8 color_with_opacity(rgba8 &input) {
        rgba8 result = input;
        Element  *n = (Element*)node::data->parent;
        double o = 1.0;
        while (n) {
            o *= n->data->opacity;
            n  = (Element*)n->node::data->parent;
        }
        result.a = math::round(result.a * o);
        return result;
    }

    double effective_opacity() {
        Element  *n = this;
        double o = 1.0;
        while (n) {
            o *= n->data->opacity;
            n  = (Element*)n->node::data->parent;
        }
        return o;
    }

    array<LineInfo> &get_lines(Canvas*);

    virtual void focused();
    virtual void unfocused();
    virtual void draw_text(Canvas& canvas, rectd& rect);
    virtual void draw(Canvas& canvas);

    vec2d offset() {
        Element *n = (Element*)node::data->parent;
        vec2d  o = { 0, 0 };
        while (n) {
            props::drawing &draw = n->data->drawings[operation::child];
            rectd &rect = draw.shape.bounds();
            o  +=  rect.xy();
            o  -= n->Element::data->scroll;
            n   = (Element*)n->node::data->parent;
        }
        return o;
    }

    array<Element*> select(lambda<Element*(Element*)> fn) {
        array<Element*> result;
        lambda<Element *(Element *)> recur;
        recur = [&](Element* n) -> Element* {
            Element* r = fn(n);
            if   (r) result += r;
            /// go through mount fields mx(symbol) -> Element*
            for (field<node*> &f:n->node::data->mounts)
                if (f.value) recur((Element*)f.value);
            return null;
        };
        recur(this);
        return result;
    }

    mx_object(Element, node, props);

    /// does not resolve at this point, this is purely storage in element with a static default data pointer
    /// we do not set anything in data
    Element(type_t type, initial<arg> args) : node(type, args), data(defaults<props>()) { }

    Element(symbol id, array<node> ch):node() {
        node::data->id = id;
        node::data->children = ch;
    }

    style *fetch_style() const { return  ((Element*)root())->data->root_style; } /// fetch data from Element2's accessor
    
    //Device *dev() const {
    //    return null;
    //}

    void exec(lambda<void(node*)> fn) {
        lambda<node*(node*)> recur;
        recur = [&](node* n) -> node* {
            fn(n);
            for (field<node*> &f: n->node::data->mounts)
                if (f.value) recur(f.value);
            return null;
        };
        recur(this);
    }
};

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
    node update() {
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

    node update() {
        return node::update();
    }
};

/// make a good edit box, then focus on rendering a good selection effect!
/// it must be Rich in nature.  makes no sense to split the two.  best primitives
/// placement is an ideal idea to get right here so we dont have to have multiple edits
/// so we have char color and placement on column (default = advance)
/// edit should be a node with some defaults set, something that has a name and can be styled
/// tag names should be the last thing you style.  we want to style props and not tags that go along with them
struct Edit:node {
    ///
    struct props {
        callback on_key;

        type_register(props);

        doubly<prop> meta() {
            return {
                prop { "on-key", on_key }
            };
        }
    };

    component(Edit, node, props);
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
            canvas.save();
            canvas.color(c);
            canvas.outline(vs);
            canvas.restore();
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


struct App:composer {
    struct adata {
        array<Camera> cameras;
        GPU          win;
        Canvas      *canvas;
        vec2d        cursor;
        bool         buttons[16];
        array<Element*> active;
        array<Element*> hover;
        VkEngine     e;
        lambda<node(App&)> app_fn;
        type_register(adata);
    };

    mx_object(App, composer, adata);

    App(lambda<node(App&)> app_fn) : App() {
        data->app_fn = app_fn;
    }
    
    int run();
    static void resize(vec2i &sz, App *app);

    operator int() { return run(); }

    ///
    array<node *> select_at(vec2d cur, bool active = true) {
        if (!composer::data->root_instance)
            return {};

        array<node*> inside = ((Element*)composer::data->root_instance)->select([&](Element *n) {
            real           x = cur.x, y = cur.y;
            vec2d          o = n->offset();
            vec2d        rel = cur + o;
            Element::props *edata = n->data;
            rectd &bounds = n->data->fill_bounds ? n->data->fill_bounds : n->data->bounds;
            bool in = bounds.contains(rel);
            (*n)->cursor = in ? vec2d(x, y) : vec2d(-1, -1);
            return (in && (!active || !edata->active)) ? n : null;
        });

        array<node*> actives = ((Element*)composer::data->root_instance)->select([&](node *n) -> node* {
            return (active && ((Element*)n)->data->active) ? n : null;
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


using AppFn = lambda<node(App&)>;
}
