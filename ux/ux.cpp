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
void Element::leave()     { }

doubly<LineInfo> &Element::get_lines(Canvas *p_canvas) {

    if (data->content && !data->lines) {
        str         s_content = data->content.hold();
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

/// to debug style, place conditional breakpoint on member->s_key == "your-prop" (used in a style block) and n->data->id == "id-you-are-debugging"
/// hopefully we dont have to do this anymore.  its simple and it works.  we may be doing our own style across service component and elemental component but having one system for all is preferred,
/// and brings a sense of orthogonality to the react-like pattern, adds type-based contextual grabs and field lookups with prop accessors
style::entry *style::impl::best_match(node *n, prop *member, array<style::entry*> &entries) {
    array<style::block*> &blocks = members[*member->s_key]; /// instance function when loading and updating style, managed map of [style::block*]
    style::entry *match = null; /// key is always a symbol, and maps are keyed by symbol
    real     best_score = 0;
    Element e = n->use<Element>(); /// ideal syntax and gives us control over copyability; default = mx::ref
    if (n->data->id == "play-pause" && *member->s_key == "fill-color" && e->hover) {
        int test = 0; /// is it trying to match the blue?
    }

    /// should narrow down by type used in the various blocks by referring to the qualifier
    /// find top style pair for this member
    type_t type = n->mem->type;
    for (style::entry *e: entries) {
        block *bl = e->bl;
        real score = bl->score(n, true);
        str &prop = *member->s_key;
        if (score > 0 && score >= best_score) { /// Edit:focus is eval'd as false?
            match = bl->entries[prop];
            assert(match);
            best_score = score;
        }
    }
    return match;
}

size_t style::block::score(node *pn, bool score_state) {
    node  &n       = *pn;
    double best_sc = 0;
    node   cur     = n;

    for (Qualifier q:quals) {
        double best_this = 0;
        for (;;) {
            bool    id_match  = q->id    &&  q->id == cur->id;
            bool   id_reject  = q->id    && !id_match;
            bool  type_match  = q->type  &&  strcmp((symbol)q->type.cs(), (symbol)cur.type()->name) == 0; /// class names are actual type names
            bool type_reject  = q->type  && !type_match;
            bool state_match  = score_state && q->state;

            /// now we are looking at all enumerable meta data in mx::find_prop_value (returns address ptr, and updates reference to a member pointer prop*)
            /// this was just a base meta check until node became the basic object
            /// Element data contains things like capture, focus, etc (second level)
            if (state_match) {
                prop* member = null;
                assert(q->state.len() > 0);
                u8*   addr   = (u8*)cur.find_prop_value(q->state, member);
                if   (addr)
                    state_match = member->type->functions->boolean(null, addr);
            }

            bool state_reject = score_state && q->state && !state_match;
            if (!id_reject && !type_reject && !state_reject) {
                double sc = size_t(   id_match) << 1 |
                            size_t( type_match) << 0 |
                            size_t(state_match) << 2;
                best_this = math::max(sc, best_this);
            } else
                best_this = 0;

            if (q->parent && best_this > 0) {
                q   = q->parent; // parent qualifier
                cur = cur->parent ? *cur->parent : node(); // parent node
            } else
                break;
        }
        best_sc = math::max(best_sc, best_this);
    }
    return best_sc > 0 ? 1.0 : 0.0;
    /// we have ability to sort more like css, 
    /// but we think its better to be most simple.
    /// its simple. use the style that applies last.  no !important either.
};

double duration_millis(duration dur) {
    switch (dur.value) {
        case duration::ns: return 1.0 / 1000.0;
        case duration::ms: return 1.0;
        case duration::s:  return 1000.0;
    }
    return 0.0;
}

/// id's can be dynamic so we cant enforce symbols, and it would be a bit lame to make the caller symbolize
str node_id(node &e) {
    array<arg> &args = e->args;
    for (arg &a: args) {
        if (strcmp((symbol)a.key.mem->origin, "id") == 0)
            return (cstr)a.value.mem->origin; /// convert to str
    }
    return e->type->name;
}

void composer::update(composer::cmdata *composer, node *parent, node *&instance, node &e) {
    bool       diff = !instance;
    bool     is_new = false;
    size_t args_len = e->args.len();
    i64         now = millis();
    bool style_reload = composer->style->reloaded;

    /// recursion here
    if (e.type() == typeof(node) && e->children) {
        size_t clen = e->children.len();
        size_t i    = 0;
        array<node*> a_instances = instance ? instance->data->children : array<node*>(clen, clen);

        /// set instances node array, then we specify the item pointer for each
        if (!instance)
            instance = new node(a_instances);
        
        for (node *cn: e->children)
            update(composer, parent, a_instances[i++], *cn);
        
        return;
    }

    if (!diff) {
        /// instance != null, so we can check attributes
        /// compare args for diffs
        array<arg>    &p = (*(node*)instance)->args; /// previous args
        array<arg>    &n = e->args; /// new args
        diff = args_len != p.len();
        if (!diff) {
            /// no reason to check against a map
            for (size_t i = 0; i < args_len; i++) {
                arg &p_pair = p[i];
                arg &n_pair = n[i];
                if (p_pair.key   != n_pair.key ||
                    p_pair.value != n_pair.value) {
                    diff = true;
                    break;
                }
            }
        }
    }
    if (diff || style_reload) {
        /// if we get this far, its expected that we are a class with schema, and have data associated
        assert(e->type->schema);
        assert(e->type->schema->bind && e->type->schema->bind->data);

        /// create instance if its not there, or the type differs (if the type differs we shall delete the node)
        if (!instance || (e->type != instance->mem->type)) {
            if (instance)
                delete instance;
            is_new   = true;
            instance = e.new_instance();
            str   id = node_id(e); /// needed for style computation of available entries in the style blocks
            (*instance)->parent = parent;
            (*instance)->id     = id.hold();
            (*instance)->composer = composer;
            
            /// compute available properties for this Element given its type, placement, and props styled 
        }

        if (style_reload || is_new)
            (*instance)->style_avail = composer->style->compute(instance);

        /// arg set cache
        bool *pset = new bool[args_len];
        memset(pset, 0, args_len * sizeof(bool));

        style::style_map &style_avail = (*instance)->style_avail;
        /// stores style across the entire poly schema inside which is ok
        /// we only look them up in context of those data structures

        /// iterate through polymorphic meta info on the schema bindings on these context types (the user instantiates context, not data)
        for (type_t ctx = e->type; ctx; ctx = ctx->parent) {
            if (!ctx->schema)
                continue;
            type_t tdata = ctx->schema->bind->data;
            if (!tdata->meta) /// mx type does not contain a schema in itself
                continue;
            u8* data_origin = (u8*)instance->mem->typed_data(tdata, 0);
            prop_map* meta_map = (prop_map*)tdata->meta_map;
            doubly<prop>* props = (doubly<prop>*)tdata->meta;

            /// its possible some classes may not have meta information defined, just skip
            if (!meta_map)
                continue;

            /// apply style to props (only do this when we need to, as this will be inefficient to do per update)
            /// dom uses invalidation for this such as property changed in a parent, element added, etc
            /// it does not need to be perfect we are not making a web browser
            for (prop &p: *props) {
                str &name = *p.s_key;
                field<array<style::entry*>> *entries = style_avail->lookup(name); // ctx name is Button, name == id, and it has a null entry for entries[0] == null with count > 
                if (entries) {
                    /// get best style matching entry for this property
                    style::entry *best = composer->style->best_match(instance, &p, *entries);
                    ///
                    type_t prop_type = p.type;
                    u8    *prop_dst  = &data_origin[p.offset];
                    
                    /// dont repeat this; store best in transitions lookup for this member name
                    node::selection &sel = instance->data->selections[name];

                    /// in cases there will be no match, in other cases we have already selected
                    if (best && (best != sel.entry)) {
                        bool should_trans = best->trans && sel.member;

                        /// create instance and immediately assign if there is no transition
                        if (prop_type->traits & traits::mx_obj) {
                            if (!best->mx_instance) best->mx_instance = (mx*)prop_type->functions->from_string(null, best->value.cs());
                            if (!should_trans) prop_type->functions->set_memory(prop_dst, best->mx_instance->mem);
                        } else {
                            if (!best->raw_instance) best->raw_instance = prop_type->functions->from_string(null, best->value.cs());
                            if (!should_trans) prop_type->functions->assign(null, prop_dst, best->raw_instance);
                        }

                        /// if we had a prior transition, delete the memory
                        if (sel.from) prop_type->functions->del(null, sel.from);

                        /// handle transitions, only when we have previously selected (otherwise we are transitioning from default state)
                        if (should_trans) {
                            /// get copy of current value (new instance, and assign from current)
                            raw_t cur = prop_type->functions->alloc_new(null, null);
                            prop_type->functions->assign(null, cur, prop_dst);
                            
                            /// setup data
                            double ms = duration_millis(best->trans.dur.type);
                            sel.start  = now;
                            sel.end    = now + best->trans.dur.value * ms;
                            sel.from   = cur;
                            sel.to     = best->mx_instance ? best->mx_instance : best->raw_instance; /// redundant
                        } else {
                            sel.start  = 0;
                            sel.end    = 0;
                            sel.from   = null;
                            sel.to     = best->mx_instance ? best->mx_instance : best->raw_instance; /// redundant
                        }
                        sel.member = &p;
                        sel.entry  = best;
                        ///
                    } else if (!best) {
                        /// if there is nothing to set to, we must set to its default initialization value
                        if (prop_type->traits & traits::mx_obj) {
                            prop_type->functions->set_memory(prop_dst, ((mx*)p.init_value)->mem);
                        } else {
                            prop_type->functions->assign(null, prop_dst, p.init_value);
                        }
                        sel.start  = 0;
                        sel.end    = 0;
                        sel.from   = null;
                        sel.member = &p;
                        sel.entry = null;
                    }
                }
            }

            /// handle selected transitions
            for (auto &field: instance->data->selections) {
                node::selection &sel = field.value;
                if (sel.start > 0 && sel.member->parent_type == tdata) { /// make sure we have the correct data origin!
                    real   amount = math::clamp(real(now - sel.start) / real(sel.end - sel.start), 0.0, 1.0);
                    real   curve  = sel.entry->trans.pos(amount);
                    type_t prop_type = sel.member->type;
                    u8    *prop_dst  = &data_origin[sel.member->offset];

                    raw_t temp;
                    if (prop_type == typeof(double)) {
                        temp = new double(*(double*)sel.from * (1.0 - curve) + *(double*)sel.to * curve);
                    } else {
                        assert(prop_type->functions->mix);
                        temp = prop_type->functions->mix(sel.from, sel.to, curve);
                    }
                    
                    //mixes++;
                    ///
                    if (prop_type->traits & traits::mx_obj) {
                        prop_type->functions->set_memory(prop_dst, ((mx*)temp)->mem);
                    } else {
                        prop_type->functions->assign(null, prop_dst, temp);
                    }

                    prop_type->functions->del(null, temp);
                }
            }

            /// iterate through args, skip those we have already set
            for (size_t i = 0; i < args_len; i++) {
                if (pset[i]) 
                    continue;
                arg &a = e->args[i];
                memory *key = a.key.mem;
                /// only support a key type of char.  we can support variables that convert to char array
                if (key->type == typeof(char)) {
                    symbol s_key = (symbol)key->origin;
                    prop **pdef = meta_map->lookup(s_key);
                    
                    if (pdef) {
                        prop *def = *pdef;
                        /// duplicates are not allowed in ux; we could handle it just not now; meta map would need to return a list not the type.  iceman: ugh!
                        assert(def->parent_type == tdata);

                        type_t prop_type = def->type;
                        type_t arg_type  = a.value.type();
                        u8    *prop_dst  = &data_origin[def->offset];
                        u8    *arg_src   = (u8*)a.value.mem->typed_data(arg_type, 0); /// passing int into mx recovers int, but passing lambda will give data inside.  we want to store a context
                        u8    *conv_inst = null;

                        /// if prop type is mx, we can generalize
                        if (prop_type == typeof(mx)) {
                            if (arg_type != prop_type) {
                                conv_inst = (u8*)new mx(a.value);
                                arg_src   = (u8*)conv_inst;
                                arg_type  = typeof(mx);
                            }
                        }
                        /// if going to string and arg is not, we convert
                        else if (prop_type == typeof(str) && arg_type != prop_type) {
                            /// general to_string conversion (memory of char)
                            assert(arg_type->functions->to_string);
                            memory *m_str = arg_type->functions->to_string(arg_src);
                            conv_inst = (u8*)new str(m_str); /// no hold here
                            arg_src   = (u8*)conv_inst;
                            arg_type  = typeof(str);
                        }
                        /// general from_string conversion.  the class needs to have a cstr constructor
                        else if ((arg_type == typeof(char) || arg_type == typeof(str)) && prop_type != arg_type) {
                            assert(prop_type->functions->from_string);
                            conv_inst = (u8*)prop_type->functions->from_string(null,
                                arg_type == typeof(str) ? (cstr)a.value.mem->origin : (cstr)arg_src);
                            arg_src = (u8*)conv_inst;
                            arg_type = prop_type;
                        }
                        /// types should match
                        assert(arg_type == prop_type || arg_type->ref == prop_type);
                        if (prop_type->traits & traits::mx_obj) {
                            /// set by memory construction (cast conv_inst as mx which it must be)
                            assert(!conv_inst || (arg_type->traits & traits::mx_obj) ||
                                                 (arg_type->traits & traits::mx));
                            prop_type->functions->set_memory(prop_dst, conv_inst ? ((mx*)conv_inst)->mem : a.value.mem);
                        } else {
                            /// assign property with data that is of the same type
                            prop_type->functions->assign(null, prop_dst, arg_src);
                        }
                        pset[i] = true;
                        if (conv_inst) arg_type->functions->del(null, conv_inst);
                    }
                } else {
                    console.fault("unsupported key type");
                }
            }
        }

        delete[] pset;

        node render = instance->update(); /// needs a 'changed' arg
        if (render) {
            node &n = *(node*)instance;
            /// nodes have a children container
            if (render->children) {
                for (node *e: render->children) {
                    if (!e->data->id && !e->data->type) continue;
                    str id = node_id(*e);
                    node *&n_mount = n->mounts[id];
                    update(composer, instance, n_mount, *e);
                }
            /// can also be stored in a map
            } else if (render->type == typeof(map<node>)) {
            } else if (render->type == typeof(array<node>)) {
            } else {
                str id = node_id(render);
                update(composer, instance, n->mounts[id], render);
            }
        }
        /// need to know if its mounted, or changed by argument
        /// it can know if a style is different but i dont see major value here
        if (is_new) {
            if (instance->data->ref) {
                instance->data->ref(mx(instance->mem->hold()));
            }
            instance->data->app = app_instance();
            instance->mounted();
        }
    }
}

}
