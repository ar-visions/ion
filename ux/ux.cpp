#define SKIA_
#include <ux/ux.hpp>
#include <array>
#include <set>
#include <stack>
#include <queue>
#include <skia/canvas.hpp>

namespace ion {

struct color:mx {
    mx_object(color, mx, rgba8);
};

listener &dispatch::listen(callback cb) {
    data->listeners += new listener::ldata { this, cb };
    listener &last = data->listeners->last(); /// last is invalid offset

    last->detatch = fn_stub([
            ls_mem=last.mem, 
            listeners=data->listeners.data
    ]() -> void {
        size_t i = 0;
        bool found = false;
        for (listener &ls: *listeners) {
            if (ls.mem == ls_mem) {
                found = true;
                break;
            }
            i++;
        }
        console.test(found);
        listeners->remove(num(i)); 
    });

    return last;
}

void dispatch::operator()(event e) {
    for (listener &l: data->listeners)
        l->cb(e);
}

//void Element::layout()

void Element::scroll(real x, real y) { }

void Element::focused()   { }
void Element::unfocused() { }
void Element::move()      { }
void Element::down()      { }
void Element::up()        { }
void Element::click()     { }
void Element::leave()     { }

doubly &Element::get_lines(Canvas *p_canvas) {

    if (data->content && !data->lines) {
        str         s_content = data->content.hold();
        Array<str>  line_strs = s_content.split("\n");
        int        line_count = (int)line_strs.len();
        ///
        data->lines = doubly();
        ///
        size_t i = 0;
        for (int i = 0; i < line_count; i++) {
            LineInfo &l = data->lines->push();
            l.bounds = {}; /// falsey rects have not been computed yet
            l.data   = line_strs[i];
            l.len    = l.data.len();
            l.adv    = data->font.advances(*p_canvas, l.data);
            l.bounds.w = l.adv[l.len - 1];
        }
    } else {
        /// this must be done after an update; could set a flag for it too..
        for (LineInfo &l: data->lines) {
            if (l.len && !l.adv) {
                l.bounds = {};
                l.adv = data->font.advances(*p_canvas, l.data);
                l.bounds.w = l.adv[l.len - 1];
            }
        }
    }
    return data->lines;
}

/// we draw the text and plot its placement; the text() function gives us back effectively aligned text
void Element::draw_text(Canvas& canvas, rectd& rect) {
    props::drawing &text = data->drawings[operation::text];
    canvas.save();
    canvas.color(text.color);
    canvas.opacity(effective_opacity());
    canvas.font(data->font);
    text_metrics tm = canvas.measure("W"); /// unit data for line
    const double line_height_basis = 2.0;
    double       lh = tm.line_height * (Element::data->text_spacing.y * line_height_basis); /// this stuff can all transition, best to live update the bounds
    ///
    doubly &lines = get_lines(&canvas);

    /// iterate through lines
    int i = 0;
    int n = lines->len();

    /// if in center, the first line is going to be offset by half the height of all the lines n * (lh / 2)
    /// if bottom, the first is going to be offset by total height of lines n * lh

    /// for center y
    double total_h = lh * n;
    data->text_bounds.x = rect.x; /// each line is independently aligned
    data->text_bounds.y = rect.y + ((rect.h * text.align.y) - (total_h * text.align.y));
    data->text_bounds.w = rect.w;
    data->text_bounds.h = total_h;

    TextSel sel_start = data->sel_start;
    TextSel sel_end   = data->sel_end;

    if (sel_start > sel_end) {
        TextSel t = sel_start;
        sel_start = sel_end;
        sel_end   = t;
    }

    bool in_sel = false;
    for (LineInfo &line:lines) {
        /// implement interactive state to detect mouse clicks and set the cursor position
        line.bounds.x = data->text_bounds.x;
        line.bounds.y = data->text_bounds.y + i * lh;
        line.bounds.w = data->text_bounds.w; /// these are set prior
        line.bounds.h = lh;

        if (!in_sel && sel_start.row == i)
            in_sel = true;

        /// get more accurate placement rectangle
        canvas.color(text.color);
        alignment t_align = { text.align.x, 0.5 };
        canvas.text(line.data, line.bounds, t_align, {0.0, 0.0}, true, &line.placement);

        if (in_sel) {
            rectd sel_rect = line.bounds;
            str sel;
            num start = 0;

            sel_rect.w = 0;
            if (sel_start.row == i) {
                start      = sel_start.column;
                sel_rect.x = line.placement.x + line.adv[sel_start.column];
            } else {
                sel_rect.x = line.bounds.x;
            }

            if (sel_end.row == i) {
                num st = sel_start.row == i ? sel_start.column : 0;
                if (sel_start.row == i) {
                    sel_rect.x = line.placement.x + line.adv[sel_start.column];
                    sel_rect.w = line.adv[sel_end.column] - line.adv[sel_start.column];
                } else {
                    if (sel_end.column == line.len)
                        sel_rect.w = (line.bounds.x + line.bounds.w) - sel_rect.x;
                    else
                        sel_rect.w = line.adv[sel_end.column];
                }
                sel = line.data.mid(start, sel_end.column - start);
            } else {
                sel_rect.w = (line.bounds.x + line.bounds.w) - sel_rect.x;
                if (sel_start.row == i) {
                    sel = &line.data[sel_start.column];
                } else {
                    sel = line.data;
                }
            }
            if (sel_rect.w) {
                canvas.color(data->sel_background);
                canvas.fill(sel_rect);
                canvas.color(data->sel_color);
                canvas.text(sel, sel_rect, { text.align.x, 0.5 }, vec2d { 0, 0 }, false);
            } else {
                sel_rect.x -= 1;
                sel_rect.w  = 2;
                canvas.color(data->sel_background);
                canvas.fill(sel_rect);
            }
        }

        if (in_sel && sel_end.row == i)
            in_sel = false;

        /// correct these for line-height selectability
        line.placement.y = line.bounds.y;
        line.placement.h = lh;
        i++;
    }
    canvas.restore();
}

/// this may have access to canvas
vec2d Element::child_offset(Element &child) {
    Element *b              = null;
    vec2d    offset         = { 0, 0 };
    bool     compute        = false;
    bool     compute_left   = false,
             compute_right  = false,
             compute_top    = false,
             compute_bottom = false;

    /// left is relative to prior control left
    if (child->area->tl.x_rel) {
        compute = true;
        compute_left = true;
    }
    /// top is relative to prior control top
    if (child->area->tl.y_rel) {
        compute = true;
        compute_top = true;
    }

    /// if child does not have any relative boundary then return parent bounds;
    if (compute) {
        node::edata *edata = ((node*)this)->data;
        for (node *n: edata->mounts) {
            Element* c = (Element*)n;
            /// complex, but bounds is computed on all of these up to child;
            /// we just need flags given for what is +'d
            if (c == &child)
                break;
            else
                b = c;
        }
        /// first item gets its parent coords
        if (b) {
            rectd &bb = b->data->bounds;
            if (compute_left) offset.x = bb.x + bb.w;
            if (compute_top)  offset.y = bb.y + bb.h;
        }
    }
    return offset;
}

void Element::update_bounds(Canvas &canvas) {
    node::edata  *edata      = ((node*)this)->data;
    Element      *eparent    = (Element*)edata->parent;
    vec2d         offset     = eparent ? eparent->child_offset(*this) : vec2d { 0, 0 };
    rectd      parent_bounds = eparent ? eparent->data->bounds : 
        rectd { 0, 0, r64(canvas.get_virtual_width()), r64(canvas.get_virtual_height()) };

    /// get void_w/h from parent
    double void_width  = eparent ? eparent->data->void_width  : 0;
    double void_height = eparent ? eparent->data->void_height : 0;
    
    props::drawing &fill     = data->drawings[operation::fill];
    props::drawing &image    = data->drawings[operation::image];
    props::drawing &outline  = data->drawings[operation::outline];

    data->bounds      = data->area.relative_rect(parent_bounds, void_width, void_height);
    data->bounds.x   += offset.x; 
    data->bounds.y   += offset.y;

    data->fill_bounds = fill.area.relative_rect(data->bounds);
    if (fill.radius) {
        vec4d r = fill.radius;
        vec2d tl { r.x, r.x };
        vec2d tr { r.y, r.y };
        vec2d br { r.w, r.w };
        vec2d bl { r.z, r.z };
        data->fill_bounds.set_rounded(tl, tr, br, bl);
    }

    if (image.img) {
        image.shape = image.area.relative_rect(data->bounds);
    }

    if (outline.color && outline.border.size > 0.0) {
        rectd outline_rect = outline.area ? outline.area.relative_rect(data->bounds) : data->fill_bounds;
        if (outline.radius) {
            vec4d r = outline.radius;
            vec2d tl { r.x, r.x };
            vec2d tr { r.y, r.y };
            vec2d br { r.w, r.w };
            vec2d bl { r.z, r.z };
            outline_rect.set_rounded(tl, tr, br, bl);
        }
        outline.shape = outline_rect;
    }

    /// we accumulate void scalers so things like headers
    /// will not be taken into account for liquid layouts
    data->void_width  = 0;
    data->void_height = 0;
    for (node *n: edata->mounts) {
        Element* e = (Element*)n;
        if (!e->data->count_width)
            data->void_width  += e->data->bounds.w;
        if (!e->data->count_height)
            data->void_height += e->data->bounds.h;
    }
    if (strcmp(type()->name, "Ribbon") == 0) {
        Element* e0 = (Element*)edata->mounts["side-profile-header"];
        e0->update_bounds(canvas);
        Element* e1 = (Element*)edata->mounts["side-profile-content"];
        e1->update_bounds(canvas);
        Element* e2 = (Element*)edata->mounts["forward-profile-header"];
        e2->update_bounds(canvas);
        Element* e3 = (Element*)edata->mounts["forward-profile-content"];
        e3->update_bounds(canvas);
    } else
    for (node *n: edata->mounts) { /// debug this: for type == Ribbon
        Element* e = (Element*)n;
        e->update_bounds(canvas);
    }
}

void Element::draw(Canvas& canvas) {
    props::drawing &fill     = data->drawings[operation::fill];
    props::drawing &image    = data->drawings[operation::image];
    props::drawing &text     = data->drawings[operation::text];
    props::drawing &outline  = data->drawings[operation::outline]; /// outline is more AR than border.  and border is a bad idea, badly defined and badly understood. outline is on the 0 pt.  just offset it if you want.

    if (fill.color) {
        canvas.save();
        canvas.color(fill.color);
        canvas.opacity(effective_opacity());
        canvas.fill(data->fill_bounds);
        canvas.restore();
    }

    if (image.img) {
        image.shape = image.area.relative_rect(data->bounds); // support simplified area syntax: 50% or so; easy syntax for scaling images
        canvas.save();
        canvas.opacity(effective_opacity());

        image.img.set_props(data->image_props);

        canvas.image(image.img, image.shape, image.align, {0,0});
        canvas.restore();
    }
    
    /// if there is text (its not alpha 0, and there is text)
    if (data->content && ((data->content.type() == typeof(char)) ||
                          (data->content.type() == typeof(str)))) {
        rectd text_bounds = text.shape;
        if (!text_bounds)
            text_bounds = data->fill_bounds;
        draw_text(canvas, text_bounds);
    }

    /// if there is an effective border to draw
    if (outline.color && outline.border.size > 0.0) {
        canvas.color(outline.color);
        canvas.opacity(effective_opacity());
        canvas.outline_sz(outline.border.size);
        rectd &rect = outline.shape;
        canvas.outline(rect); /// this needs to work with vshape, or border
    }

    for (node *n: node::data->mounts) {
        Element* c = (Element*)n;
        /// clip to child
        /// translate to child location
        canvas.save();
        rectd &bounds = c->data->bounds;
        canvas.translate(bounds.xy());
        canvas.opacity(effective_opacity());
        rectd clip_bounds = { 0, 0, bounds.w, bounds.h };

        /// adjust clip for outline if there is one
        props::drawing &outline  = c->data->drawings[operation::outline];
        if (outline.color && outline.border.size > 0.0) {
            rectd  r0   = clip_bounds;
            rectd  r1   = (rectd &)outline.shape;
            double minx = math::min(r0.x, r1.x - outline.border.size / 2.0);
            double miny = math::min(r0.y, r1.y - outline.border.size / 2.0);
            double maxx = math::max(r0.x, r1.x + r1.w + outline.border.size / 2.0);
            double maxy = math::max(r0.y, r1.y + r1.h + outline.border.size / 2.0);
            clip_bounds = rectd {
                minx, miny, maxx - minx, maxy - miny
            };
        }
        canvas.clip(clip_bounds);
        canvas.font(c->data->font); /// set effective opacity here too
        c->draw(canvas);
        canvas.restore();
    }
}

TextSel Element::get_selection(vec2d pos, bool is_down) {
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
            res.row = row;
            if (pos.x < line.placement.x)
                res.column = 0;
            else if (pos.x > line.placement.x + line.placement.w)
                res.column = line.len;
            else {
                bool col_set = false;
                for (num i = 1; i <= line.len; i++) {
                    real adv0 = line.adv[i - 1];
                    real adv1 = line.adv[i];
                    real med  = (adv0 + adv1) / 2.0;
                    if ((pos.x - line.placement.x) < med) {
                        res.column = i - 1;
                        col_set = true;
                        break;
                    }
                }
                /// set end-of-last character
                if (!col_set)
                    res.column = line.len;
            }
        }
        y += line.bounds.h;
        row++;
    }
    return res;
}

/// Element-based generic text handler; dispatches text
void Element::on_text(event e) {
    /// just for Elements which allow text editing
    if (!data->editable)
        return;

    /// order the selection, and split the lines (enter retains info of initial line insertion by user)
    bool       swap = data->sel_start > data->sel_end;
    TextSel     &ss = swap ? data->sel_end : data->sel_start;
    TextSel     &se = swap ? data->sel_start : data->sel_end;
    Array<str> text = e->text.split("\n");
    bool      enter = e->text == "\n";
    bool       back = e->text == "\b";

    /// do not insert control characters
    if (back)
        text = Array<str> {""};

    doubly add;
    for (str &line: text) {
        LineInfo &l = add->push_default<LineInfo>();
        l.data = line;
        l.len  = line.len();
    }

    /// handle backspace
    if (back) {
        /// shift start back 1 when our len is 0
        if (ss.row == se.row && ss.column == se.column)
            ss.column--;
        
        /// handle line deletion
        if (ss.column < 0) {
            if (ss.row > 0) {
                ss.row--;
                ss.column = data->lines[ss.row].len;
            } else {
                se.column = 0;
                ss.column = 0;
            }
        }
    }

    /// replace and update text sels (all done in TextSel::replace)
    TextSel::replace(data->lines, ss, se, add);

    if (!back) {
        if (enter) {
            se.row++;
            ss.row++;
            se.column = 0;
            ss.column = 0;
        } else {
            se.column++;
            ss.column++;
        }
    }

    /// turn off for very large text boxes
    size_t len = 0;
    for (LineInfo &li: data->lines)
        len += li.len;
    str content { len };
    for (LineInfo &li: data->lines)
        content += li.data;
    data->content = content;

    /// avoid updating content otherwise
    if (data->ev.text) {
        e->text = content;
        data->ev.text(e);
    }
}

rgba8 Element::color_with_opacity(rgba8 &input) {
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

double Element::effective_opacity() {
    Element  *n = this;
    double o = 1.0;
    while (n) {
        o *= n->data->opacity;
        n  = (Element*)n->node::data->parent;
    }
    return o;
}

vec2d Element::offset() {
    Element *n = (Element*)this;
    vec2d  o = { 0, 0 };
    while (n) {
        o  += n->Element::data->bounds.xy(); /// this is calculated in the generic draw function
        o  -= n->Element::data->scroll;
        n   = (Element*)n->node::data->parent;
    }
    return o;
}

Array<Element*> Element::select(lambda<Element*(Element*)> fn) {
    Array<Element*> result;
    lambda<Element *(Element *)> recur;
    recur = [&](Element* n) -> Element* {
        Element* r = fn(n);
        if   (r) result += r;
        /// go through mount fields mx(symbol) -> Element*
        for (field &f:n->node::data->mounts) {
            if (f.value) recur(f.value.ref<Element*>());
        }
        return null;
    };
    recur(this);
    return result;
}

void Element::exec(lambda<void(node*)> fn) {
    lambda<node*(node*)> recur;
    recur = [&](node* n) -> node* {
        fn(n);
        for (field &f: n->node::data->mounts)
            if (f.value) recur(f.value.ref<node*>());
        return null;
    };
    recur(this);
}


}
