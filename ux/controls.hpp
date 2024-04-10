#pragma once
#include <ux/ux.hpp>

namespace ion {

struct Button:Element {
    enums(Behavior, push,
         push, label, toggle, radio);
    
    struct props {
        Button::Behavior    behavior;
        bool                selected;
        callback            on_change;
        properties meta() {
            return {
                prop {"behavior",  behavior},
                prop {"on-change", on_change},
                prop {"selected",  selected}
            };
        }
    };

    component(Button, Element, props);

    void mounted() {
        if (state->selected)
            node::data->value = mx(true);
    }

    void down() {
        if (state->behavior == Button::Behavior::radio) {
            Array<node*> buttons = collect(node::data->group, false);
            for (node *b: buttons) {
                if (b->type() != typeof(Button))
                    continue;
                Button *button = (Button*)b;
                button->state->selected = button == this;
                button->node::data->value = mx(button->state->selected);
            }
        } else if (state->behavior == Button::Behavior::toggle) {
            state->selected = !state->selected;
            node::data->value = state->selected;
        }

        if (state->on_change) {
            event ev { this };
            // set target to this; the user can then lookup the component and its properties such as selected
            // we prefer this to throwing new args around
            state->on_change(ev);
        }
    }

    node update() {
        return node::update();
    }
};

struct Edit:Element {
    ///
    struct props {
        struct events {
            callback change;
        } ev;

        properties meta() {
            return {
                prop { "on-change", ev.change },
            };
        }
    };

    component(Edit, Element, props);
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
    
    Array<Column> columns;
    using Columns = Array<Column>;
    
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

        properties meta() {
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
    } m;
    
    void update_columns() {
        columns       = Array<Column>(m.column.ids);
        double  tsz   = 0;
        double  warea = node::std.drawings[operation::fill].shape.w();
        double  t     = 0;
        
        for (Column &c: columns)
            if (c->scale)
                tsz += c->value;
        ///
        for (Column &c: columns)
            if (c->scale) {
                c->value /= tsz;
                t += c->value;
            } else
                warea = math::max(0.0, warea - c->value);
        ///
        for (Column &c: columns)
            if (c->scale)
                c->final = math::round(warea * (c->value / t));
            else
                c->final = c->value;
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
        ion::rect &rect   = node::std.drawings[operation::fill].rect();
        ion::rect  h_line = { rect.x, rect.y, rect.w, 1.0 };

        /// would be nice to just draw 1 outline here, canvas should work with that.
        auto draw_rect = [&](rgba8 &c, ion::rect &h_line) {
            rectr  r   = h_line;
            shape vs  = r; /// todo: decide on final xy scaling for text and gfx; we need a the same unit scales
            canvas.save();
            canvas.color(c);
            canvas.outline(vs);
            canvas.restore();
        };
        Column  nc = null;
        auto cells = [&] (lambda<void(Column &, ion::rect &)> fn) {
            if (columns) {
                v2d p = h_line.xy();
                for (Column &c: columns) {
                    double w = c->final;
                    ion::rect cell = { p.x, p.y, w - 1.0, 1.0 };
                    fn(c, cell);
                    p.x += w;
                }
            } else
                fn(nc, h_line);
        };

        ///
        auto pad_rect = [&](ion::rect r) -> ion::rect {
            return {r.x + 1.0, r.y, r.w - 2.0, r.h};
        };
        
        /// paint column text, fills and outlines
        if (columns) {
            bool prev = false;
            cells([&](Column &c, ion::rect &cell) {
                //var d_id = c.id;
                str label = "n/a";//context("resolve")(d_id); -- c
                canvas.color(text.color);
                canvas.text(label, pad_rect(cell), { xalign::middle, yalign::middle }, { 0, 0 }, true);
                if (prev) {
                    ion::rect cr = cr;
                    draw_rect(outline.border.color(), cr);
                } else
                    prev = true;
            });
            ion::rect rr = { h_line.x - 1.0, h_line.y + 1.0, h_line.w + 2.0, 0.0 }; /// needs to be in props
            draw_rect(outline.border.color(), rr);
            h_line.y += 2.0; /// this too..
        }
        
        /// paint visible rows
        Array<mx> d_array(data.hold());
        double  sy = std.scroll.data.y;
        int  limit = sy + fill.h() - (columns ? 2.0 : 0.0);
        for (int i = sy; i < math::min(limit, int(d_array.length())); i++) {
            mx &d = d_array[i];
            cells([&](Column &c, ion::rect &cell) {
                str s = c ? str(d[c->id]) : str(d);
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

}
