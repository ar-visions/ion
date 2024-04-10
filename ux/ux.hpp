#pragma once

#include <math/math.hpp>
#include <async/async.hpp>
#include <net/net.hpp>
#include <media/media.hpp>
#include <composer/composer.hpp>
#include <skia/canvas.hpp>

struct GLFWwindow;

namespace ion {

struct Device;
struct Texture;

using Path    = path;
using Image   = image;
using strings = Array<str>;
using Assets  = map;

struct coord {
    alignment   align;
    vec2d       offset;
    xalign      x_type;
    yalign      y_type;
    bool        x_rel = false; // relative to peer, this is not the standard relative in terms of width or height as they are relative to their own origin
    bool        y_rel = false;
    bool        x_per = false;
    bool        y_per = false;

    coord(alignment &al, vec2d &offset, xalign &xt, yalign &yt, bool &x_rel, bool &y_rel, bool &x_per, bool &y_per) :
        align(al), offset(offset), x_type(xt), y_type(yt), x_rel(x_rel), y_rel(y_rel), x_per(x_per), y_per(y_per) { }

    coord(str s) {
        Array<str> sp = s.split();
        str       &sx = sp[0];
        str       &sy = sp[1];
        assert(sp.len() == 2);

        x_type = str(sx[0]);
        y_type = str(sy[0]);
        str sx_offset = &sx[1];
        str sy_offset = &sy[1];

        if (sx_offset[sx_offset.len() - 1] == '%') {
            sx_offset = sx_offset.mid(0, sx_offset.len() - 1);
            x_per = true;
        }
        if (sy_offset[sy_offset.len() - 1] == '%') {
            sy_offset = sy_offset.mid(0, sy_offset.len() - 1);
            y_per = true;
        }
        if (sx_offset)
            if (sx_offset[sx_offset.len() - 1] == '+') {
                sx_offset = sx_offset.mid(0, sx_offset.len() - 1);
                x_rel = true;
            }
        if (sy_offset)
            if (sy_offset[sy_offset.len() - 1] == '+') {
                sy_offset = sy_offset.mid(0, sy_offset.len() - 1);
                y_rel = true;
            }

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
        bool        x_rel  = b.x_rel;
        bool        y_rel  = b.y_rel;
        bool        x_per  = b.x_per;
        bool        y_per  = b.y_per;
        return coord {
            align, offset, x_type, y_type, x_rel, y_rel, x_per, y_per
        };
    }

    vec2d plot(ion::rect &rect, vec2d &rel, double void_width = 0, double void_height = 0) {
        double x;
        double y;
        double ox = x_per ? (offset.x / 100.0) * (rect.w - void_width)  : offset.x;
        double oy = y_per ? (offset.y / 100.0) * (rect.h - void_height) : offset.y;

        if (x_type.value == xalign::width)
            x = rel.x + ox;
        else
            x = rect.x + (rect.w - void_width) * align.x + ox;
        
        if (y_type.value == yalign::height)
            y = rel.y + oy;
        else
            y = rect.y + (rect.h - void_height) * align.y + oy;

        return { x, y };
    }

    coord(cstr cs) : coord(str(cs)) { }
};

/// good primitive for ui, implemented in basic canvas ops.
/// regions can be constructed from rects if area is static or composed in another way
struct region:mx {
    struct M {
        coord tl = (cstr)"l0 t0";
        coord br = (cstr)"r0 b0";
        double per = -1;
        bool set = false;
    };
    mx_basic(region);

    /// simple rect
    region(ion::rect r) : region() {
        data->tl  = fmt { "l{0} t{1}", { r.x, r.y }};
        data->br  = fmt { "w{0} h{1}", { r.w, r.h }};
        data->set = true;
    }

    region(coord &tl, coord &br) : region() {
        data->tl = tl;
        data->br = br;
        data->set = true;
    }

    region(str s) : region() {
        Array<str> a = s.split();
        if (a.length() == 4) {
            str s0  = a[0] + " " + a[1];
            str s1  = a[2] + " " + a[3];
            data->tl      = s0;
            data->br      = s1;
        } else {
            assert(a.length() == 1);
            /// convert to m-H% mH%
            str a0 = a[0];
            str suf = "";
            if (a0[a0.len() - 1] == '%') {
                a0  = a0.mid(0, a0.len() - 1);
                suf = "%";
            }
            real sv = a0.real_value<real>();
            str h   = str(sv / 2);
            str s0  = str("m-") + h + suf + " m-" + h + suf;
            str s1  = str("m")  + h + suf + " m"  + h + suf;
            data->tl      = s0;
            data->br      = s1;
        }
        data->set = true;
    }

    region(cstr s):region(str(s)) { }

    operator bool() { return data->set; }
    
    ion::rect relative_rect(ion::rect &win, double void_width = 0, double void_height = 0) {
        vec2d rel  = win.xy();
        vec2d v_tl = data->tl.plot(win, rel, void_width, void_height);
        vec2d v_br = data->br.plot(win, v_tl, void_width, void_height);
        ion::rect r { v_tl, v_br };
        r.x -= win.x;
        r.y -= win.y;
        return r;
    }
    
    ion::rect rect(ion::rect &win) {
        vec2d rel = win.xy();
        return ion::rect { data->tl.plot(win, rel), data->br.plot(win, rel) };
    }
    
    region mix(region &b, double a) {
        coord m_tl = data->tl.mix(b->tl, a);
        coord m_tr = data->br.mix(b->br, a);
        return region {
            m_tl, m_tr
        };
    }
};

struct Element;

struct adata *app_instance();

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
        bool                    test1;
        double                  void_width, void_height;

        /// if we consider events handled contextually and by many users, it makes sense to have dispatch
        struct events {
            callback            hover;
            callback            out;
            callback            down;
            callback            click;
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
            rgba                color;
            rgba                secondary;
            real                opacity;
            alignment           align;
            SVG                 img; /// feature, not bug or even restriction.  no raster images in UI!
            region              area;
            vec4d               radius;
            outline             shape; 
            ion::cap            cap;   
            ion::join           join;
            ion::border         border;
            inline ion::rect rect() { return shape.bounds(); }
        };

        inline ion::outline shape() {
            return drawings[operation::child].shape;
        }

        inline ion::rect shape_bounds() {
            return ion::rect((ion::rect&)shape());
        }

        drawing             drawings[operation::count];
        region              area; /// directly sets the bounds ion::rect
        rgba                sel_color;
        rgba                sel_background;
        vec2d               scroll = { 0, 0 };
        std::queue<fn_t>    queue;
        vec2d               text_spacing = { 1.0, 1.0 }; /// kerning & line-height scales here
        mx                  content;    /// lines could be an 'attachment' to content; thus when teh user sets content explicitly, it will then be recomputed
        doubly              lines;
        double              opacity = 1.0;
        ion::rect               bounds;     /// local coordinates of this control; xy are relative to parent xy
        ion::rect               fill_bounds;
        ion::rect               text_bounds;
        ion::font           font;
        EProps              image_props;
        bool                editable   = false;
        bool                selectable = true;
        bool                multiline  = false;
        TextSel             sel_start, sel_end;
        bool                count_height = true;
        bool                count_width  = true;

        properties meta() const {
            return {
                prop { "on-hover",       ev.hover  },
                prop { "on-out",         ev.out    },
                prop { "on-down",        ev.down   },
                prop { "on-click",       ev.click  },
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
                prop { "image-src",      drawings[operation::image]  .img },
                prop { "image-props",    image_props },

                prop { "area",           area },

                prop { "test1",          test1 },

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

                prop { "capture",      capture      },
                prop { "hover",        hover        },
                prop { "active",       active       },
                prop { "focus",        focus        },
                prop { "tab-index",    tab_index    },
                prop { "count-width",  count_width  },
                prop { "count-height", count_height }
            };
        }
    };

    void debug() {
    }

    TextSel get_selection(vec2d pos, bool is_down);

    /// Element-based generic text handler; dispatches text event
    void on_text(event e);

    rgba8 color_with_opacity(rgba8 &input);

    double effective_opacity();

    doubly &get_lines(Canvas*);

    virtual void focused();
    virtual void unfocused();
    virtual void scroll(real x, real y);
    virtual void move();
    virtual void leave();
    virtual void down();
    virtual void click();
    virtual void up();
    virtual void draw_text(Canvas& canvas, ion::rect& rect);
    virtual void draw(Canvas& canvas);
    virtual void update_bounds(Canvas &canvas);
    virtual vec2d child_offset(Element &child);

    vec2d offset();

    Array<Element*> select(lambda<Element*(Element*)> fn);

    mx_object(Element, node, props);

    Element(type_t type, std::initializer_list<arg> args) : node(type, args), data(defaults<props>()) { }

    Element(str id, Array<node> ch):node(id, ch) { }

    style *fetch_style() const { return  ((Element*)root())->data->root_style; }

    void exec(lambda<void(node*)> fn);
};

}
