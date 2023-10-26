#pragma once

#include <async/async.hpp>
#include <net/net.hpp>
#include <image/image.hpp>
#include <media/media.hpp>
#include <vk/vk.hpp>
#include <vkh/vkh.h>
//#include <camera/camera.hpp>
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
        none, blunt, round);
    
    enums(join, miter,
        miter, round, bevel);

    struct shape:mx {
        using Rect    = ion::Rect   <r64>;
        using Rounded = ion::Rounded<r64>;

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
            Line::ldata ld { data->mv, l };
            data->ops += ion::Line(ld);
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

enums(nil, none, none);

enums(xalign, left,
     left, middle, right, width);
    
enums(yalign, top,
     top, middle, bottom, height);

enums(blending, undefined,
     undefined, clear, src, dst, src_over, dst_over, src_in, dst_in, src_out, dst_out, src_atop, dst_atop, Xor, plus, modulate, screen, overlay, color_dodge, color_burn, hard_light, soft_light, difference, exclusion, multiply, hue, saturation, color);

enums(filter, none,
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

struct Element;

struct ch:pair<mx,mx> {
    ch(array<node> a) {
        key   = "children";
        value = a;
    }
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

    struct M {
        array<V>   vbo;
        map<group> groups;
        type_register(M);
    };

    mx_object(OBJ, mx, M);

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
        auto indices  = std::unordered_map<memory*, u32>(); /// symbol memory as key; you can instantiate str or other objects with this data
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
                    auto key = w[i].symbolize();
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

    font(real sz, str name) : font() {
        data->sz = sz;
        data->name = name;
    }

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
        fdata fd { sz, data->name };
        return font(fd);
    }

    array<double> advances(Canvas& canvas);
    array<double> advances(Canvas& canvas, str line);

            operator bool()    { return  data->sz; }
    bool    operator!()        { return !data->sz; }
};

enums(operation, fill,
     fill, image, outline, text, child);

template <typename> struct simple_content : true_type { };

struct LineInfo {
    str            data;
    num            len;
    array<double>  adv;
    rectd          bounds;     /// bounds of area of the text line
    rectd          placement;  /// effective bounds of the aligned text, with y and h same as bounds
};

struct TextSel {
    num     column = 0;
    num     row = 0;

    /// common function for TextSel used in 2 parts; updates the sel_start and sel_end
    static void replace(doubly<LineInfo> &lines, TextSel &sel_start, TextSel &sel_end, doubly<LineInfo> &text) {
        assert(sel_start.row <= sel_end.row);
        assert(sel_start.row != sel_end.row || sel_start.column <= sel_end.column);
        LineInfo &ls = lines[sel_start.row];
        LineInfo &le = lines[sel_end  .row]; /// this is fine to do before, because its a doubly
        bool line_insert = text->len() > 1;

        /// a buffer with \n is len == 2.  make sense because you start at 1 line, and a new line gives you 2.
        str left   = str(&ls.data[0], sel_start.column);
        str right  = str(&le.data[sel_end.column]);

        /// delete LineInfo's inbetween start and end
        for (int i = sel_start.row + 1; i <= sel_end.row; i++)
            lines->remove(sel_start.row + 1);

        /// manage lines when inserting new ones
        if (line_insert) {
            /// first line is set to text to left of sel + first line of sel
            ls.data = left + text[0].data;
            ls.len  = ls.data.len();

            /// recompute advances by clearing
            ls.adv.clear();

            /// insert lines[1] and above
            int index = 0;
            int last  = text->len();

            for (LineInfo &l: text) {
                item<LineInfo> *item;
                if (index) {
                    item = lines->insert(sel_start.row + index, l); // this should insert on end, it doesnt with a 1 item list with index = 1
                    /// this function insert before, unless index == count then its a new tail
                    /// always runs as final op, this merges last line with the data we read on right side prior
                    if (index + 1 == last) {
                        LineInfo &n = item->data;
                        n.data = l.data + right;
                        n.len  = n.data.len();
                        sel_start.column = l.data.len();
                        break;
                    }
                    sel_start.row++; /// we set both of these
                }
                index++;
            }
        } else {
            /// result is lines removed from ls+1 to le, ls.data = ls[left] + data + le[right]
            ls.data    = left + text[0].data + right;
            ls.len     = ls.data.len();
            ls.adv.clear();
        }
        sel_end.row    = sel_start.row;
        sel_end.column = sel_start.column;
    }
    ///
    bool operator<= (const TextSel &b) { return !operator>(b); }
    bool operator>  (const TextSel &b) { return row > b.row || ((row == b.row) && column > b.column); }
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

        /// if we consider events handled contextually and by many users, it makes sense to have dispatch
        struct events {
            callback            hover;
            callback            out;
            callback            down;
            callback            up;
            callback            key;
            callback            focus;
            callback            blur;
            callback            cursor;
            callback            text;
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
        std::queue<fn_t>    queue;

        /// kerning not yet supported by skia for some odd reason; we need to add this
        vec2d               text_spacing = { 1.0, 1.0 }; /// kerning & line-height scales here

        mx                  content; /// lines could be an 'attachment' to content; thus when teh user sets content explicitly, it will then be recomputed
        doubly<LineInfo>    lines;
        double              opacity = 1.0;
        rectd               bounds;     /// local coordinates of this control, so x and y are 0 based
        rectd               fill_bounds;
        rectd               text_bounds;
        ion::font           font;  
        bool                editable   = false;
        bool                selectable = true;
        bool                multiline  = false;
        TextSel             sel_start, sel_end;

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
                prop { "on-text",        ev.text   },
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
                prop { "hover",     hover     },
                prop { "active",    active    },
                prop { "focus",     focus     },
                prop { "tab-index", tab_index },
            };
        }
        type_register(props);
    };

    void debug() {
        int test = 0;
        test++;
    }

    TextSel get_selection(vec2d pos, bool is_down);

    /// Element-based generic text handler; dispatches text event
    void on_text(event e);

    rgba8 color_with_opacity(rgba8 &input);

    double effective_opacity();

    doubly<LineInfo> &get_lines(Canvas*);

    virtual void focused();
    virtual void unfocused();
    virtual void draw_text(Canvas& canvas, rectd& rect);
    virtual void draw(Canvas& canvas);

    vec2d offset();

    array<Element*> select(lambda<Element*(Element*)> fn);

    mx_object(Element, node, props);

    Element(type_t type, initial<arg> args) : node(type, args), data(defaults<props>()) { }

    Element(symbol id, array<node> ch):node() {
        node::data->id = id;
        node::data->children = ch;
    }

    style *fetch_style() const { return  ((Element*)root())->data->root_style; }

    void exec(lambda<void(node*)> fn);
};

}
