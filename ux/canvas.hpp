#pragma once
#include <async/async.hpp>
#include <watch/watch.hpp>
#include <media/media.hpp>
#include <media/image.hpp>
#include <trinity/trinity.hpp>

struct SkCanvas;

namespace ion {

struct Canvas;

struct text_metrics {
    real            w = 0,
                    h = 0;
    real       ascent = 0,
              descent = 0;
    real  line_height = 0,
           cap_height = 0;
};

using tm_t = text_metrics;

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

/// todo: needs ability to set a middle in coord/region
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
    bool is_default = false;

    alignment() { is_default = true; }

    alignment(real x, real y) : x(x), y(y), is_default(false) { }

    alignment(str s) : is_default(false) {
        array<str> a = s.split();
        str s0 = a[0];
        str s1 = a.len() > 1 ? a[1] : a[0];

        if (s0.numeric()) {
            x = s0.real_value<real>();
        } else {
            xalign xa { s0 };
            switch (xa.value) {
                case xalign::left:   x = 0.0; break;
                case xalign::middle: x = 0.5; break; /// todo: needs ability to set a middle in coord/region
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
                case yalign::middle: y = 0.5; break; /// todo: needs ability to set a middle in coord/region
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

struct font:mx {
    struct fdata {
        real sz = 22;
        str  name = "RobotoMono-Regular";
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
        data->sz   = sp[0].real_value<real>();
        if (sp.len() > 1)
            data->name = sp[1];
    }

    font(cstr s):font(str(s)) { }

    font(real sz):font() {
        data->sz = sz;
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


enums(mode, regular,
      regular, headless);

struct Canvas;

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

#ifdef CANVAS_IMPL
    using svgdom_t = sk_sp<SkSVGDOM>*;
#else
    using svgdom_t = void*;
#endif

struct Canvas;

struct EStr {
    double value;
    str    suffix;

    EStr() { }

    EStr(str combined) {
        for (int i = 0; i < combined.len(); i++) {
            char ch = combined[i];
            if ((ch >= 'a' && ch <= 'z') || ch == '%') {
                suffix = combined.mid(i);
                value  = combined.mid(0, i).real_value<real>();
                break;
            }
        }
        if (!suffix)
            value = combined.real_value<real>();
    }
    
    EStr(double value, str suffix) : value(value), suffix(suffix) { }

    EStr mix(EStr &b, double f) {
        assert(suffix == b.suffix);
        return EStr {
            value * (1.0 - f) + b.value * f, suffix
        };
    }

    operator str() {
        return fmt { "{0}{1}", { value, suffix } };
    }

    register(EStr);
};

/// external props, used to manage SVG
/// may have variable suffix but cannot interpolate between units as they are not known here, so dont do 1px to 3cm or 1% to 100px
struct EProps:mx {
    struct M {
        map<EStr> eprops;
        operator bool() {
            return eprops.len() > 0;
        }
        register(M)
    };
    mx_basic(EProps);

    EProps(str s) : EProps() {
        array<str> a = s.split("|");
        for (str &v: a) {
            array<str> kv = v.split("=");
            assert(kv.len() == 2);
            data->eprops[kv[0]] = EStr(kv[1]);
        }
    }
    
    EProps(cstr cs) : EProps(str(cs)) { }

    EProps mix(EProps &b, double f) {
        EProps &a = *this;
        EProps r;
        assert(a->eprops.len() == b->eprops.len());
        for(field<EStr> &af: a->eprops) {
            assert(b->eprops->count(af.key));
            r->eprops[af.key] = af.value.mix(b->eprops[af.key], f);
        }
        return r;
    }
};

struct SVG:mx {
    struct M {
        svgdom_t   svg_dom;
        int        w, h;
        int       rw, rh;
        operator bool();
        register(M);
    };

    SVG(path p);
    SVG(cstr p);
    void render(SkCanvas *sk_canvas, int w = -1, int h = -1);
    void set_props(EProps &eprops);
    vec2i sz();

    mx_basic(SVG);
};


struct ICanvas;
struct iSVG;
struct Window;

struct Canvas:mx {
    mx_declare(Canvas, mx, ICanvas);

    Canvas(Texture texture);

    u32 get_virtual_width();
    u32 get_virtual_height();

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
    void image(SVG   img, rectd rect, alignment align, vec2d offset);
    void image(ion::image img, rectd rect, alignment align, vec2d offset, bool attach_tx = false);
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

}
