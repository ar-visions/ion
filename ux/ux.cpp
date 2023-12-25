#include <ux/ux.hpp>
#include <array>
#include <set>
#include <stack>
#include <queue>
#include <ux/canvas.hpp>

namespace ion {

//static NewFn<path> path_fn = NewFn<path>(path::_new);
//static NewFn<array<rgba8>> ari_fn = NewFn<array<rgba8>>(array<rgba8>::_new);
//static NewFn<image> img_fn = NewFn<image>(image::_new);

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

doubly<LineInfo> &Element::get_lines(Canvas *p_canvas) {

    if (data->content && !data->lines) {
        str         s_content = data->content.grab();
        array<str>  line_strs = s_content.split("\n");
        int        line_count = (int)line_strs.len();
        ///
        data->lines = doubly<LineInfo>();
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

array<double> font::advances(Canvas& canvas, str text) {
    num l = text.len();
    array<double> res(l+1, l+1);
    ///
    for (num i = 0; i <= l; i++) {
        str    s   = text.mid(0, i);
        double adv = canvas.measure_advance(s.data, i);
        res[i]     = adv;
    }
    return res;
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
    doubly<LineInfo> &lines = get_lines(&canvas);

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
    rectd    bounds         = data->bounds;
    bool     compute        = false;
    bool     compute_left   = false,
             compute_right  = false,
             compute_top    = false,
             compute_bottom = false;

    /// left is relative to prior control left
    if (child->area.tl.x_rel) {
        compute = true;
        compute_left = true;
    }
    /// top is relative to prior control top
    if (child->area.tl.y_rel) {
        compute = true;
        compute_top = true;
    }

    /// if child does not have any relative boundary then return parent bounds;
    if (!compute)
        return vec2d { 0, 0 };
    
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
    if (!b)
        return vec2d { 0, 0 };
    
    rectd &bb = b->data->bounds;
    if (compute_left) {
        double px = bounds.x;
        bounds.x  = bb.x + bb.w;
        bounds.w += (px - bounds.x);
    }
    if (compute_top) {
        double py = bounds.y;
        bounds.y  = bb.y + bb.h;
        bounds.h += (py - bounds.y);
    }
    return bounds.xy();
}

void Element::draw(Canvas& canvas) {
    type_t        type       = mem->type;
    node::edata  *edata      = ((node*)this)->data;
    Element      *eparent    = (Element*)edata->parent;
    if (edata->id == "record") {
        int test = 0;
        test++;
    }
    vec2d             offset = eparent ? eparent->child_offset(*this) : vec2d { 0, 0 };
    rectd      parent_bounds = eparent ? eparent->data->bounds : 
        rectd { 0, 0, r64(canvas.get_virtual_width()), r64(canvas.get_virtual_height()) };
    props::drawing &fill     = data->drawings[operation::fill];
    props::drawing &image    = data->drawings[operation::image];
    props::drawing &text     = data->drawings[operation::text];
    props::drawing &outline  = data->drawings[operation::outline]; /// outline is more AR than border.  and border is a bad idea, badly defined and badly understood. outline is on the 0 pt.  just offset it if you want.
    props::drawing &children = data->drawings[operation::child]; /// outline is more AR than border.  and border is a bad idea, badly defined and badly understood. outline is on the 0 pt.  just offset it if you want.
    
    /// debug the bounds computed on 'record' id
    if (edata->id == "record") {
        /// needs a menu option or toolbar option for camera selection
        /// also lighting output can be in there
        /// memus can have hotkeys and dictation phrases to loosely match (pretty neat idea in general)
        int test = 0;
        test++;
    }
    canvas.save();
    
    /// icons must paint using the new + (offset) syntax in coord/region
    data->bounds      = data->area.relative_rect(parent_bounds);
    data->bounds.x   += offset.x; 
    data->bounds.y   += offset.y;

    /// all of the delegate areas should not allow for peer bounds,
    /// for the obvious reason that these are purely relative to self computed bounds (which allow for peer)
    data->fill_bounds = fill.area.relative_rect(data->bounds); /// use this bounds if we do not have specific areas defined
    
    /// if there is a fill color
    if (fill.color) { /// free'd prematurely during style change (not a transition)
        if (fill.radius) {
            vec4d r = fill.radius;
            vec2d tl { r.x, r.x };
            vec2d tr { r.y, r.y };
            vec2d br { r.w, r.w };
            vec2d bl { r.z, r.z };
            data->fill_bounds.set_rounded(tl, tr, br, bl);
        }
        canvas.save();
        canvas.color(fill.color);
        canvas.opacity(effective_opacity());
        canvas.fill(data->fill_bounds);
        canvas.restore();
    }

    /// if there is fill image
    if (image.img) {
        image.shape = image.area.relative_rect(data->bounds); // support a area: 50% or so; easy syntax for scaling images
        canvas.save();
        canvas.opacity(effective_opacity());
        canvas.image(image.img, image.shape, image.align, {0,0});
        canvas.restore();
    }
    
    /// if there is text (its not alpha 0, and there is text)
    if (data->content && ((data->content.type() == typeof(char)) ||
                          (data->content.type() == typeof(str)))) {
        rectd rect = text.area ? text.area.relative_rect(data->bounds) : data->fill_bounds;
        draw_text(canvas, rect);
        text.shape = rect;
    }

    /// if there is an effective border to draw
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
        canvas.color(outline.color);
        canvas.opacity(effective_opacity());
        canvas.outline_sz(outline.border.size);
        canvas.outline(outline_rect); /// this needs to work with vshape, or border
        outline.shape = outline_rect; /// updating for interactive logic
    }

    for (node *n: edata->mounts) {
        Element* c = (Element*)n;
        /// clip to child
        /// translate to child location
        canvas.save();
        rectd &bounds = c->data->bounds;
        canvas.translate(bounds.xy());
        canvas.opacity(effective_opacity());
        rectd clip_bounds = { 0, 0, bounds.w, bounds.h };
        canvas.clip(clip_bounds);
        c->draw(canvas);
        canvas.restore();
    }

    canvas.restore();
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
    array<str> text = e->text.split("\n");
    bool      enter = e->text == "\n";
    bool       back = e->text == "\b";

    /// do not insert control characters
    if (back)
        text = array<str> {""};

    doubly<LineInfo> add;
    for (str &line: text) {
        LineInfo &l = add->push();
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

array<Element*> Element::select(lambda<Element*(Element*)> fn) {
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

void Element::exec(lambda<void(node*)> fn) {
    lambda<node*(node*)> recur;
    recur = [&](node* n) -> node* {
        fn(n);
        for (field<node*> &f: n->node::data->mounts)
            if (f.value) recur(f.value);
        return null;
    };
    recur(this);
}

}
