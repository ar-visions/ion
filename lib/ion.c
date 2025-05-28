#include <import>
#include <math.h>

static const real PI = 3.1415926535897932384; // M_PI;
static const real c1 = 1.70158;
static const real c2 = c1 * 1.525;
static const real c3 = c1 + 1;
static const real c4 = (2 * PI) / 3;
static const real c5 = (2 * PI) / 4.5;

string coord_cast_string(coord a) {
    return f(string, "%c%f%s%s %c%f%s%s",
        e_str(xalign, a->x_type)->chars[0], a->offset.x,
        a->x_per ? "%" : "", a->x_rel ? "+" : "",
        e_str(yalign, a->y_type)->chars[0], a->offset.y,
        a->y_per ? "%" : "", a->y_rel ? "+" : "");
}

coord coord_with_string(coord a, string s) {
    array  sp = split(s, " ");
    verify(sp, "expected unit-value token such as x-10");
    verify(len(sp) == 2, "expected two values");
    string sx = sp->elements[0];
    string sy = sp->elements[1];
    
    /// set x/y types
    a->x_type = e_val(xalign, sx->chars);
    a->y_type = e_val(yalign, sy->chars);
    string sx_offset = mid(sx, 1, len(sx) - 1);
    string sy_offset = mid(sy, 1, len(sy) - 1);

    /// get numeric attributes (percentage and relative)
    if (last(sx_offset) == '%') {
        sx_offset = mid(sx_offset, 0, len(sx_offset) - 1);
        a->x_per = true;
    }
    if (last(sy_offset) == '%') {
        sy_offset = mid(sy_offset, 0, len(sy_offset) - 1);
        a->y_per = true;
    }
    if (sx_offset)
        if (last(sx_offset) == '+') {
            sx_offset = mid(sx_offset, 0, len(sx_offset) - 1);
            a->x_rel = true;
        }
    if (sy_offset)
        if (last(sy_offset) == '+') {
            sy_offset = mid(sy_offset, 0, len(sy_offset) - 1);
            a->y_rel = true;
        }

    /// set offset
    a->offset = vec2f(
        real_value(sx_offset), real_value(sy_offset));
    
    /// set align
    f32 x = 0, y = 0;
    switch (a->x_type) {
        case xalign_left:   x = 0.0; break;
        case xalign_middle: x = 0.5; break;
        case xalign_right:  x = 1.0; break;
        case xalign_width:  x = 0.0; break;
        default: break;
    }
    switch (a->y_type) {
        case yalign_top:    y = 0.0; break;
        case yalign_middle: y = 0.5; break;
        case yalign_bottom: y = 1.0; break;
        case yalign_height: y = 0.0; break;
        default: break;
    }
    a->align = alignment(x, x, y, y);
    return a;
}

coord coord_mix(coord a, coord b, f32 f) {
    alignment   align  = mix( a->align,   b->align,  f);
    vec2f       offset = vec2f_mix(&a->offset, &b->offset, f);
    xalign      x_type = b->x_type;
    yalign      y_type = b->y_type;
    bool        x_rel  = b->x_rel;
    bool        y_rel  = b->y_rel;
    bool        x_per  = b->x_per;
    bool        y_per  = b->y_per;
    return coord(
        align,  align,  offset, offset,
        x_type, x_type, y_type, y_type,
        x_rel,  x_rel,  y_rel,  y_rel,
        x_per,  x_per,  y_per,  y_per);
}

vec2f coord_plot(coord a, rect r, vec2f rel, f32 void_width, f32 void_height) {
    f32 x;
    f32 y;
    f32 sc_x = 1.0f - a->align->x * 2.0f;
    f32 sc_y = 1.0f - a->align->y * 2.0f;
    f32 ox = a->x_per ? (a->offset.x * sc_x / 100.0) * (r->w - void_width)  : a->offset.x * sc_x;
    f32 oy = a->y_per ? (a->offset.y * sc_y / 100.0) * (r->h - void_height) : a->offset.y * sc_y;

    if (a->x_type == xalign_width)
        x = rel.x + ox;
    else
        x = r->x + (r->w - void_width) * a->align->x + ox;
    
    if (a->y_type == yalign_height)
        y = rel.y + oy;
    else
        y = r->y + (r->h - void_height) * a->align->y + oy;

    return vec2f(x, y);
}

coord coord_with_cstr(coord a, cstr cs) {
    return coord_with_string(new(coord), string(cs));
}

bool coord_cast_bool(coord a) {
    return a->x_type != xalign_undefined;
}





alignment alignment_with_vec2f(alignment a, vec2f xy) {
    a->x = xy.x;
    a->y = xy.y;
    a->set = true;
    return a;
}

alignment alignment_with_string(alignment a, string s) {
    array  ar = split(s, " ");
    string s0 = ar->elements[0];
    string s1 = len(ar) > 1 ? ar->elements[1] : ar->elements[0];

    if (is_numeric(s0)) {
        a->x = real_value(s0);
    } else {
        xalign  xa = e_val(xalign, s0->chars);
        switch (xa) {
            case xalign_left:   a->x = 0.0; break;
            case xalign_middle: a->x = 0.5; break; /// todo: needs ability to set a middle in coord/region
            case xalign_right:  a->x = 1.0; break;
            default: break;
        }
    }

    if (is_numeric(s1)) {
        a->y = real_value(s1);
    } else {
        yalign  ya = e_val(yalign, s1->chars);
        switch (ya) {
            case yalign_top:    a->y = 0.0; break;
            case yalign_middle: a->y = 0.5; break; /// todo: needs ability to set a middle in coord/region
            case yalign_bottom: a->y = 1.0; break;
            default: break;
        }
    }
    return a;
}

alignment alignment_with_cstr(alignment a, cstr cs) {
    return alignment_with_string(a, string(cs));
}

alignment alignment_mix(alignment a, alignment b, f32 f) {
    vec2f v2 = vec2f(a->x * (1.0f - f) + b->x * f,
                     a->y * (1.0f - f) + b->y * f);
    return alignment_with_vec2f(new(alignment), v2);
}





/// good primitive for ui, implemented in basic canvas ops.
/// regions can be constructed from rects if area is static or composed in another way

string region_cast_string(region r) {
    return f(string, "%o %o", r->tl, r->br);
}

region region_with_f32(region reg, f32 f) {
    reg->tl  = f(coord, "l%f t%f", f, f);
    reg->br  = f(coord, "r%f b%f", f, f);
    reg->set = true;
    return reg;
}

/// simple rect
region region_with_rect(region reg, rect r) {
    reg->tl  = f(coord, "l%f t%f", r->x, r->y);
    reg->br  = f(coord, "w%f h%f", r->w, r->h);
    reg->set = true;
    return reg;
}

region region_with_array(region reg, array corners) {
    verify(len(corners) >= 2, "expected 2 coord");
    coord tl = get(corners, 0);
    coord br = get(corners, 1);
    verify(isa(tl) == typeid(coord), "expected coord");
    verify(isa(br) == typeid(coord), "expected coord");
    reg->tl  = tl;
    reg->br  = br;
    reg->set = true;
    return reg;
}

region region_with_string(region reg, string s) {
    array a = split(s, " ");
    if (len(a) == 4) {
        reg->tl    = f(coord, "%o %o", a->elements[0], a->elements[1]);
        reg->br    = f(coord, "%o %o", a->elements[2], a->elements[3]);
    } else if (len(a) == 1) {
        string f   = first(a);
        real n     = real_value(f);
        bool per   = last(f) == '%';
        reg->tl    = coord(align, alignment(x, 0.0f, y, 0.0f),
            x_type, xalign_left,
            y_type, yalign_top,
            x_per, per, y_per, per);
        reg->br    = coord(align, alignment(x, 1.0f, y, 1.0f),
            x_type, xalign_right,
            y_type, yalign_bottom,
            y_per, per, y_per, per);
    }
    reg->set = true;
    return reg;
}

region region_with_cstr(region a, cstr s) {
    return region_with_string(new(region), string(s));
}

bool region_cast_bool(region a) { return a->set; }

rect region_relative_rect(region data, rect win, f32 void_width, f32 void_height) {
    vec2f rel  = xy(win);
    vec2f v_tl = plot(data->tl, win, rel,  void_width, void_height);
    vec2f v_br = plot(data->br, win, v_tl, void_width, void_height);
    rect r = rect_from_plots(v_tl, v_br);
    r->x -= win->x;
    r->y -= win->y;
    return r;
}

rect region_rectangle(region data, rect win) {
    vec2f rel = vec2f(win->x, win->y);
    return rect_from_plots(
        plot(data->tl, win, rel, 0, 0), plot(data->br, win, rel, 0, 0));
}

region region_mix(region data, region b, f32 a) {
    coord m_tl = mix(data->tl, b->tl, a);
    coord m_tr = mix(data->br, b->br, a);
    return region(a(m_tl, m_tr));
}

bool qualifier_cast_bool(qualifier q) {
    return len(q->type) || q->id || q->state;
}

bool style_transition_cast_bool(style_transition a) {
    return a->duration->scale_v > 0;
}

style_transition style_transition_with_string(style_transition a, string s) {
    print("transition: %o", s);
    array sp = split(s, " ");
    sz    ln = len(sp);
    /// syntax:
    /// 500ms [ease [out]]
    /// 0.2s -- will be linear with in (argument meaningless for linear but applies to all others)
    string dur_string = sp->elements[0];
    a->duration = unit_with_string(new(tcoord), dur_string);
    a->easing = ln > 1 ? e_val(Ease,      sp->elements[1]) : Ease_linear;
    a->dir    = ln > 2 ? e_val(Direction, sp->elements[2]) : Direction_in;
    return a;
}

static real ease_linear        (real x) { return x; }
static real ease_in_quad       (real x) { return x * x; }
static real ease_out_quad      (real x) { return 1 - (1 - x) * (1 - x); }
static real ease_in_out_quad   (real x) { return x < 0.5 ? 2 * x * x : 1 - pow(-2 * x + 2, 2) / 2; }
static real ease_in_cubic      (real x) { return x * x * x; }
static real ease_out_cubic     (real x) { return 1 - pow(1 - x, 3); }
static real ease_in_out_cubic  (real x) { return x < 0.5 ? 4 * x * x * x : 1 - pow(-2 * x + 2, 3) / 2; }
static real ease_in_quart      (real x) { return x * x * x * x; }
static real ease_out_quart     (real x) { return 1 - pow(1 - x, 4); }
static real ease_in_out_quart  (real x) { return x < 0.5 ? 8 * x * x * x * x : 1 - pow(-2 * x + 2, 4) / 2; }
static real ease_in_quint      (real x) { return x * x * x * x * x; }
static real ease_out_quint     (real x) { return 1 - pow(1 - x, 5); }
static real ease_in_out_quint  (real x) { return x < 0.5 ? 16 * x * x * x * x * x : 1 - pow(-2 * x + 2, 5) / 2; }
static real ease_in_sine       (real x) { return 1 - cos((x * PI) / 2); }
static real ease_out_sine      (real x) { return sin((x * PI) / 2); }
static real ease_in_out_sine   (real x) { return -(cos(PI * x) - 1) / 2; }
static real ease_in_expo       (real x) { return x == 0 ? 0 : pow(2, 10 * x - 10); }
static real ease_out_expo      (real x) { return x == 1 ? 1 : 1 - pow(2, -10 * x); }
static real ease_in_out_expo   (real x) {
    return x == 0
        ? 0
        : x == 1
        ? 1
        : x < 0.5
        ? pow(2, 20 * x - 10) / 2
        : (2 - pow(2, -20 * x + 10)) / 2;
}
static real ease_in_circ       (real x) { return 1 - sqrt(1 - pow(x, 2)); }
static real ease_out_circ      (real x) { return sqrt(1 - pow(x - 1, 2)); }
static real ease_in_out_circ   (real x) {
    return x < 0.5
        ? (1 - sqrt(1 - pow(2 * x, 2))) / 2
        : (sqrt(1 - pow(-2 * x + 2, 2)) + 1) / 2;
}
static real ease_in_back       (real x) { return c3 * x * x * x - c1 * x * x; }
static real ease_out_back      (real x) { return 1 + c3 * pow(x - 1, 3) + c1 * pow(x - 1, 2); }
static real ease_in_out_back   (real x) {
    return x < 0.5
        ? (pow(2 * x, 2) * ((c2 + 1) * 2 * x - c2)) / 2
        : (pow(2 * x - 2, 2) * ((c2 + 1) * (x * 2 - 2) + c2) + 2) / 2;
}
static real ease_in_elastic    (real x) {
    return x == 0
        ? 0
        : x == 1
        ? 1
        : -pow(2, 10 * x - 10) * sin((x * 10 - 10.75) * c4);
}
static real ease_out_elastic   (real x) {
    return x == 0
        ? 0
        : x == 1
        ? 1
        : pow(2, -10 * x) * sin((x * 10 - 0.75) * c4) + 1;
}
static real ease_in_out_elastic(real x) {
    return x == 0
        ? 0
        : x == 1
        ? 1
        : x < 0.5
        ? -(pow(2, 20 * x - 10) * sin((20 * x - 11.125) * c5)) / 2
        : (pow(2, -20 * x + 10) * sin((20 * x - 11.125) * c5)) / 2 + 1;
}
static real bounce_out(real x) {
    const real n1 = 7.5625;
    const real d1 = 2.75;
    if (x < 1 / d1) {
        return n1 * x * x;
    } else if (x < 2 / d1) {
        return n1 * (x - 1.5 / d1) * x + 0.75;
    } else if (x < 2.5 / d1) {
        return n1 * (x - 2.25 / d1) * x + 0.9375;
    } else {
        return n1 * (x - 2.625 / d1) * x + 0.984375;
    }
}
static real ease_in_bounce     (real x) {
    return 1 - bounce_out(1 - x);
}
static real ease_out_bounce    (real x) { return bounce_out(x); }
static real ease_in_out_bounce (real x) {
    return x < 0.5
        ? (1 - bounce_out(1 - 2 * x)) / 2
        : (1 + bounce_out(2 * x - 1)) / 2;
}

static i64 distance(cstr s0, cstr s1) {
    i64 r = (i64)s1 - (i64)s0;
    return r < 0 ? -r : r;
}

/// functions are courtesy of easings.net; just organized them into 2 enumerables compatible with web
real style_transition_pos(style_transition a, real tf) {
    real x = clamp(tf, 0.0, 1.0);
    switch (a->easing) {
        case Ease_linear:
            switch (a->dir) {
                case Direction_in:      return ease_linear(x);
                case Direction_out:     return ease_linear(x);
                case Direction_in_out:  return ease_linear(x);
            }
            break;
        case Ease_quad:
            switch (a->dir) {
                case Direction_in:      return ease_in_quad(x);
                case Direction_out:     return ease_out_quad(x);
                case Direction_in_out:  return ease_in_out_quad(x);
            }
            break;
        case Ease_cubic:
            switch (a->dir) {
                case Direction_in:      return ease_in_cubic(x);
                case Direction_out:     return ease_out_cubic(x);
                case Direction_in_out:  return ease_in_out_cubic(x);
            }
            break;
        case Ease_quart:
            switch (a->dir) {
                case Direction_in:      return ease_in_quart(x);
                case Direction_out:     return ease_out_quart(x);
                case Direction_in_out:  return ease_in_out_quart(x);
            }
            break;
        case Ease_quint:
            switch (a->dir) {
                case Direction_in:      return ease_in_quint(x);
                case Direction_out:     return ease_out_quint(x);
                case Direction_in_out:  return ease_in_out_quint(x);
            }
            break;
        case Ease_sine:
            switch (a->dir) {
                case Direction_in:      return ease_in_sine(x);
                case Direction_out:     return ease_out_sine(x);
                case Direction_in_out:  return ease_in_out_sine(x);
            }
            break;
        case Ease_expo:
            switch (a->dir) {
                case Direction_in:      return ease_in_expo(x);
                case Direction_out:     return ease_out_expo(x);
                case Direction_in_out:  return ease_in_out_expo(x);
            }
            break;
        case Ease_circ:
            switch (a->dir) {
                case Direction_in:      return ease_in_circ(x);
                case Direction_out:     return ease_out_circ(x);
                case Direction_in_out:  return ease_in_out_circ(x);
            }
            break;
        case Ease_back:
            switch (a->dir) {
                case Direction_in:      return ease_in_back(x);
                case Direction_out:     return ease_out_back(x);
                case Direction_in_out:  return ease_in_out_back(x);
            }
            break;
        case Ease_elastic:
            switch (a->dir) {
                case Direction_in:      return ease_in_elastic(x);
                case Direction_out:     return ease_out_elastic(x);
                case Direction_in_out:  return ease_in_out_elastic(x);
            }
            break;
        case Ease_bounce:
            switch (a->dir) {
                case Direction_in:      return ease_in_bounce(x);
                case Direction_out:     return ease_out_bounce(x);
                case Direction_in_out:  return ease_in_out_bounce(x);
            }
            break;
    };
    return x;
}

/// to debug style, place conditional breakpoint on member->s_key == "your-prop" (used in a style block) and n->data->id == "id-you-are-debugging"
/// hopefully we dont have to do this anymore.  its simple and it works.  we may be doing our own style across service component and elemental component but having one system for all is preferred,
/// and brings a sense of orthogonality to the react-like pattern, adds type-based contextual grabs and field lookups with prop accessors

style_entry style_best_match(style a, ion n, string prop_name, array entries) {
  //array       blocks     = get(members, prop_name);
    style_entry match      = null; /// key is always a symbol, and maps are keyed by symbol
    real        best_score = 0;
    AType       type       = isa(n);
    each (entries, style_entry, e) {
        style_block  bl = e->bl;
        real   sc = score(bl, n, true);
        if (sc > 0 && sc >= best_score) {
            match = get(bl->entries, prop_name);
            verify(match, "should have matched");
            best_score = sc;
        }
    }
    return match;
}

void style_block_init(style_block a) {
    if (!a->entries) a->entries = map(hsize, 16);
    if (!a->blocks)  a->blocks  = array(alloc, 16);
}

num style_block_score(style_block a, ion n, bool score_state) {
    f64 best_sc = 0;
    ion   cur     = n;

    for (item i = a->quals->first; i; i = i->next) {
        qualifier q = instanceof(i->value, qualifier);
        f64 best_this = 0;
        for (;;) {
            bool    id_match  = q->id &&  eq(q->id, cur->id->chars);
            bool   id_reject  = q->id && !id_match;
            bool  type_match  = q->ty &&  A_inherits(isa(cur), q->ty);
            bool type_reject  = q->ty && !type_match;
            bool state_match  = score_state && q->state;

            if (state_match) {
                verify(len(q->state) > 0, "null state");
                object addr = A_get_property(cur, cstring(q->state));
                if (addr)
                    state_match = A_header(addr)->type->cast_bool(addr);
            }

            bool state_reject = score_state && q->state && !state_match;
            if (!id_reject && !type_reject && !state_reject) {
                f64 sc = (sz)(   id_match) << 1 |
                         (sz)( type_match) << 0 |
                         (sz)(state_match) << 2;
                best_this = max(sc, best_this);
            } else
                best_this = 0;

            if (q->parent && best_this > 0) {
                q   = q->parent; // parent qualifier
                cur = cur->parent ? cur->parent : ion(); // parent ion
            } else
                break;
        }
        best_sc = max(best_sc, best_this);
    }
    return best_sc > 0 ? 1.0 : 0.0;
};

f64 Duration_base_millis(Duration duration) {
    switch (duration) {
        case Duration_ns: return 1.0 / 1000.0;
        case Duration_ms: return 1.0;
        case Duration_s:  return 1000.0;
    }
    return 0.0;
}

i64 tcoord_get_millis(tcoord a) {
    Duration u = (Duration)(i32)a->enum_v;
    f64 base = Duration_base_millis(u);
    return base * a->scale_v;
}

bool style_applicable(style s, ion n, string prop_name, array result) {
    array blocks = get(s->members, prop_name);
    AType type   = isa(n);
    bool  ret    = false;

    clear(result);
    if (blocks)
        each (blocks, style_block, block) {
            if (!len(block->types) || index_of(block->types, type) >= 0) {
                item f = lookup(block->entries, prop_name); // this returns the wrong kind of item reference
                if (f && score(block, n, false) > 0) {
                    pair p = f->value; // forgive us!  the lookup come froms from a map, and this item we lookup does not store the value on its item->value 
                    AType ftype = isa(p->value);
                    push(result, p->value);
                    ret = true;
                }
            }
        }
    return ret;
}

void  event_prevent_default (event e)        {         e->prevent_default = true; }
bool  event_is_default      (event e)        { return !e->prevent_default; }
bool  event_should_propagate(event e)        { return !e->stop_propagation; }
bool  event_stop_propagation(event e)        { return  e->stop_propagation = true; }
bool  event_key_down        (event e, num u) { return  e->key->unicode   == u && !e->key->up; }
bool  event_key_up          (event e, num u) { return  e->key->unicode   == u &&  e->key->up; }
bool  event_scan_down       (event e, num s) { return  e->key->scan_code == s && !e->key->up; }
bool  event_scan_up         (event e, num s) { return  e->key->scan_code == s &&  e->key->up; }


int ion_compare(ion a, ion b) {
    AType type = isa(a);
    if (type != isa(b))
        return -1;
    if (a == b)
        return 0;
    for (int m = 0; m < type->member_count; m++) {
        type_member_t* mem = &type->members[m];
        bool is_prop = mem->member_type & A_MEMBER_PROP;
        if (!is_prop || strcmp(mem->name, "elements") == 0) continue;
        if (A_is_inlay(mem)) { // works for structs and primitives
            ARef cur = (ARef)((cstr)a + mem->offset);
            ARef nxt = (ARef)((cstr)b + mem->offset);
            bool is_same = (cur == nxt || 
                memcmp(cur, nxt, mem->type->size) == 0);
            if (!is_same)
                return -1;
        } else {
            object* cur = (object*)((cstr)a + mem->offset);
            object* nxt = (object*)((cstr)b + mem->offset);
            if (*cur != *nxt) {
                bool is_same = (*cur && *nxt) ? 
                    compare(*cur, *nxt) == 0 : false;
                if (!is_same)
                    return -1;
            }
        }
    }
    return 0;
}

map ion_render(ion a, list changed) {
    return a->elements; /// elements is not allocated for non-container elements, so default behavior is to not host components
}

none ion_init(ion a) {
}


style style_with_path(style a, path css_path) {
    verify(exists(css_path), "css path does not exist");
    string style_str = read(css_path, typeid(string));
    a->base    = array(alloc, 32);
    a->members = map(hsize, 32);
    process(a, style_str);
    cache_members(a);
    a->loaded   = true;
    a->reloaded = true; /// cache validation for composer user
    return a;
}

none style_watch_reload(style a, array css, ARef arg) {
    style_with_path(a, css);
}

style style_with_object(style a, object app) {
    AType  app_type  = isa(app);
    string root_type = string(app_type->name);
    path   css_path  = form(path, "style/%o.css", root_type);
    /*a->reloader = watch(
        res,        css_path,
        callback,   style_watch_reload); -- todo: implement watch [perfectly good one] */
    return style_with_path(a, css_path);
}

bool is_cmt(symbol c) {
    return c[0] == '/' && c[1] == '*';
}

bool ws(cstr* p_cursor) {
    cstr cursor = *p_cursor;
    while (isspace(*cursor) || is_cmt(cursor)) {
        while (isspace(*cursor))
            cursor++;
        if (is_cmt(cursor)) {
            cstr f = strstr(cursor, "*/");
            cursor = f ? &f[2] : &cursor[strlen(cursor) - 1];
        }
    }
    *p_cursor = cursor;
    return *cursor != 0;
}

static bool scan_to(cstr* p_cursor, string chars) {
    bool sl  = false;
    bool qt  = false;
    bool qt2 = false;
    cstr cursor = *p_cursor;
    for (; *cursor; cursor++) {
        if (!sl) {
            if (*cursor == '"')
                qt = !qt;
            else if (*cursor == '\'')
                qt2 = !qt2;
        }
        sl = *cursor == '\\';
        if (!qt && !qt2) {
            char cur[2] = { *cursor, 0 };
            if (index_of(chars, cur) >= 0) {
                 *p_cursor = cursor;
                 return true;
            }
        }
    }
    *p_cursor = null;
    return false;
}

static list parse_qualifiers(style_block bl, cstr *p) {
    string   qstr;
    cstr start = *p;
    cstr end   = null;
    cstr scan  =  start;
    
    /// read ahead to {
    do {
        if (!*scan || *scan == '{') {
            end  = scan;
            qstr = string(chars, start, ref_length, distance(start, scan));
            break;
        }
    } while (*++scan);
    
    ///
    if (!qstr) {
        end = scan;
        *p  = end;
        return null;
    }
    
    ///
    array quals = split(qstr, ",");
    list result = list();
    
    array ops = array_of_cstr("!=", ">=", "<=", ">", "<", "=", null);
    ///
    each (quals, string, qs) {
        string  qq = trim(qs);
        if (!len(qq)) continue;
        qualifier vv = qualifier(); /// push new qualifier
        push(result, vv);

        /// we need to scan by >

        qualifier v = vv;
        array parent_to_child = split(qq, "/"); /// we choose not to use > because it interferes with ops
        qualifier processed = null;

        /// iterate through reverse
        for (int i = len(parent_to_child) - 1; i >= 0; i--) {
            string q = trim((string)parent_to_child->elements[i]);
            if (processed) {
                v->parent = qualifier();
                v = hold(v->parent); /// dont need to cast this
            }
            num idot = index_of(q, ".");
            num icol = index_of(q, ":");
            string tail = null;
            ///
            if (idot >= 0) {
                array sp = split(q, ".");
                v->type   = trim((string)first(sp));
                array sp2 = split((string)sp->elements[1], ":");
                v->id     = first(sp2);
                if (icol >= 0)
                    tail  = trim(mid(q, icol + 1, len(q) - (icol + 1))); /// likely fine to use the [1] component of the split
            } else {
                if (icol  >= 0) {
                    v->type = trim(mid(q, 0, icol));
                    tail   = trim(mid(q, icol + 1, len(q) - (icol + 1)));
                } else
                    v->type = trim(q);
            }

            if (v->type) { /// todo: verify idata is correctly registered and looked up
                v->ty = A_find_type(v->type->chars);
                verify(v->ty, "type must exist");
                if (index_of(bl->types, v->ty) == -1)
                    push(bl->types, v->ty);
            }
            
            if (tail) {
                // check for ops
                bool is_op = false;
                each (ops, string, op) {
                    if (index_of(tail, cstring(op)) >= 0) {
                        is_op   = true;
                        array sp = split(tail, cstring(op));
                        string f = first(sp);
                        v->state = trim(f);
                        v->oper  = op;
                        int istart = len(f) + len(op);
                        v->value = trim(mid(tail, istart, len(tail) - istart));
                        break;
                    }
                }
                if (!is_op)
                    v->state = tail;
            }
            processed = v;
        }
    }
    *p = end;
    return result;
}

/// compute available entries for props on a Element
map style_compute(style a, ion n) {
    map avail = map(hsize, 16);
    AType ty = isa(n);
    verify(instanceof(n, ion), "must inherit ion");
    while (ty != typeid(ion)) {
        array all = array(alloc, 32);
        for (int m = 0; m < ty->member_count; m++) {
            type_member_t* mem = &ty->members[m];
            if (mem->member_type != A_MEMBER_PROP)
                continue;
            string name = string(mem->name);
            if (applicable(a, n, name, all)) {
                set(avail, name, all);
                all = array(alloc, 32);
            }
        }
        ty = ty->parent_type;
    }
    return avail;
}

static void cache_b(style a, style_block bl) {
    pairs (bl->entries, i) {
        string      key = i->key;
        style_entry e   = i->value;
        bool  found = false;
        array cache = get(a->members, e->member);
        if (!cache) {
             cache = array();
             set(a->members, e->member, cache);
        }
        each (cache, style_block, cb)
            found |= cb == bl;
        ///
        if (!found)
            push(cache, bl);
    }
    each (bl->blocks, style_block, s)
        cache_b(a, s);
}

void style_cache_members(style a) {
    if (a->base)
        each (a->base, style_block, b)
            cache_b(a, b);
}

/// \\ = \ ... \x = \x
static string parse_quoted(cstr *cursor, size_t max_len) {
    cstr first = *cursor;
    if (*first != '"')
        return string("");
    ///
    bool last_slash = false;
    cstr start     = ++(*cursor);
    cstr s         = start;
    ///
    for (; *s != 0; ++s) {
        if (*s == '\\')
            last_slash = true;
        else if (*s == '"' && !last_slash)
            break;
        else
            last_slash = false;
    }
    if (*s == 0)
        return string("");
    ///
    size_t len = (size_t)(s - start);
    if (max_len > 0 && len > max_len)
        return string("");
    ///
    *cursor = &s[1];
    return string(chars, start, ref_length, len);
}


none parse_block(style_block bl, cstr* p_sc) {
    cstr sc = *p_sc;
    ws(&sc);
    verify(*sc == '.' || isalpha(*sc), "expected Type[.id], or .id");
    bl->quals = parse_qualifiers(bl, &sc);
    sc++;
    ws(&sc);
    ///
    while (*sc && *sc != '}') {
        /// read up to ;, {, or }
        ws(&sc);
        cstr start = sc;
        verify(scan_to(&sc, string(";{}")), "expected member expression or qualifier");
        if (*sc == '{') {
            ///
            style_block bl_n = style_block(types, array(unmanaged, true));
            push(bl->blocks, bl_n);
            bl_n->parent = bl;

            /// parse sub-block
            sc = start;
            parse_block(bl_n, sc);
            verify(*sc == '}', "expected }");
            sc++;
            ws(&sc);
            ///
        } else if (*sc == ';') {
            /// read member
            cstr cur = start;
            verify(scan_to(&cur, string(":")) && (cur < sc), "expected [member:]value;");
            string  member = string(chars, start, ref_length, distance(start, cur));
            cur++;
            ws(&cur);

            /// read value
            cstr vstart = cur;
            verify(scan_to(&cur, string(";")), "expected member:[value;]");
            
            /// needs escape sequencing?
            size_t len      = distance(vstart, cur);
            string cb_value = trim(string(chars, vstart, ref_length, len));
            string end      = mid(cb_value, -1, 1);
            bool   qs       = eq(mid(cb_value,  0, 1), "\"");
            bool   qe       = eq(mid(cb_value, -1, 1), "\"");

            if (qs && qe) {
                cstr   cs = cstring(cb_value);
                cb_value  = parse_quoted(&cs, len(cb_value));
            }

            num         i = index_of(cb_value, ",");
            string  param = i >= 0 ? trim(mid(cb_value, i + 1, len(cb_value) - (i + 1))) : null;
            string  value = i >= 0 ? trim(mid(cb_value, 0, i))  : cb_value;
            style_transition trans = param ? style_transition(param) : null;
            
            /// check
            verify(member, "member cannot be blank");
            verify(value,  "value cannot be blank");
            style_entry e = style_entry(
                member, member, value, value, trans, trans, bl, bl);
            set(bl->entries, member, e);
            /// 
            sc++;
            ws(&sc);
        }
    }
    verify(!*sc || *sc == '}', "expected closed-brace");
    if (*sc == '}')
        sc++;
    *p_sc = sc;
}

void style_process(style a, string code) {
    a->base = array(alloc, 32);
    for (cstr sc = cstring(code); sc && *sc; ws(&sc)) {
        style_block n_block = style_block(types, array(unmanaged, true));
        push(a->base, n_block);
        parse_block(n_block, &sc);
    }
}

list composer_apply_args(composer ux, ion i, ion e) {
    AType type    = isa(e);
    list  changed = list();
    u64   f_user  = A_fbits(e);

    // check the difference between members (not elements within)
    while (type != typeid(ion) && type != typeid(A)) { 
        for (int m = 0; m < type->member_count; m++) {
            type_member_t* mem = &type->members[m];
            bool is_prop = mem->member_type & A_MEMBER_PROP;
            if (!is_prop || strcmp(mem->name, "elements") == 0) continue;
            bool is_set = ((1 << mem->id) & f_user) != 0;
            if (A_is_inlay(mem)) { // works for structs and primitives
                ARef cur = (ARef)((cstr)i + mem->offset);
                ARef nxt = (ARef)((cstr)e + mem->offset);
                bool is_same = (cur == nxt || 
                    memcmp(cur, nxt, mem->type->size) == 0);
                
                /// primitive memory of zero is effectively unset for args
                if (!is_same && is_set) {
                    memcpy(cur, nxt, mem->type->size);
                    push(changed, string(mem->name));
                }
            } else {
                object* cur = (object*)((cstr)i + mem->offset);
                object* nxt = (object*)((cstr)e + mem->offset);
                if (*nxt && *cur != *nxt) {
                    bool is_same = (*cur && *nxt) ? compare(*cur, *nxt) == 0 : false;
                    if (!is_same && is_set) {
                        object obj_value = A_header(*cur);
                        print("prop different: %s, prev value: %o", mem->name, *cur);
                        drop(*cur);
                        *cur = hold(*nxt);
                        push(changed, string(mem->name));
                    }
                }
            }
        }
        type = type->parent_type;
    }
    return changed;
}

list composer_apply_style(composer ux, ion i, map style_avail, list exceptions) {
    AType type = isa(i);
    list changed = list();
    while (type != typeid(A)) {
        for (int m = 0; m < type->member_count; m++) {
            type_member_t* mem = &type->members[m];
            bool is_prop = mem->member_type & A_MEMBER_PROP;
            if (!is_prop || strcmp(mem->name, "elements") == 0)
                continue;
            
            string prop    = string(mem->name);
            list   entries = get(style_avail, prop);
            if (!entries)
                continue;
            // dont apply over these exceptional args
            if (exceptions && index_of(exceptions, prop) >= 0)
                continue;
            
            /// compute best match for this prop against style_entries
            style_entry best = best_match(ux->style, i, prop, entries);
            if (!best)
                continue;

            // lazily create instance value from string on style entry
            if (!best->instance)
                best->instance = A_formatter(
                    mem->type, null, (object)false,
                    (symbol)"%s", best->value->chars);
            verify(best->instance, "instance must be initialized");

            push(changed, prop);
            object* cur = (object*)((cstr)i + mem->offset);

            style_transition t  = best->trans;
            style_transition ct = null;
            bool should_trans = false;
            if (t) {
                if (t && !i->transitions)
                    i->transitions = map(hsize, 16);
                ct = i->transitions ? get(i->transitions, prop) : null;
                if (!ct) {
                    ct = copy(t);
                    ct->reference = hold(t);
                    should_trans = true;
                    set(i->transitions, prop, ct);
                } else {
                    should_trans = ct->reference != t;
                }
            }
            
            // we know this is a different transition assigned
            if (ct && should_trans) {
                // save the value where it is now
                if (A_is_inlay(mem)) {
                    ct->from = A_alloc(mem->type, 1, true);
                    memcpy(ct->from, cur, mem->type->size);
                } else {
                    ct->from = *cur ? hold(*cur) : hold(best->instance);
                }
                ct->type     = isa(best->instance);
                element ii = i;
                ct->location = cur; /// hold onto pointer location
                if (eq(prop, "area")) {
                    int test2 = 2;
                    test2 += 2;
                    verify(&ii->area == ct->location, "member not set correctly");
                }
                ct->to       = hold(best->instance);
                ct->start    = epoch_millis();
                ct->is_inlay = A_is_inlay(mem);
            } else if (!ct) {
                if (A_is_inlay(mem)) {
                    print("%s cur = %p", mem->name, cur);
                    f32* f_cur = cur;
                    element ii = i;
                    memcpy(cur, best->instance, mem->type->size);
                    int test2 = 2;
                    test2 += 2;
                } else {
                    drop(*cur);
                    verify(!A_is_inlay(mem), "support inlay copy from instance");
                    AType vtype = isa(best->instance);
                    *cur = hold(best->instance);
                }
            }
        }
        type = type->parent_type;
    }
    return changed;
}

none composer_update(composer ux, ion parent, map rendered_elements) {
    pairs(rendered_elements, ir) {
        string id       = ir->key;
        ion    e        = ir->value;
        ion    instance = parent->elements ? get(parent->elements, id) : null;
        AType  type     = isa(e);
        list   changed  = null;
        bool   new_inst = false;
        bool   restyle  = false;

        if (!instance) {
            new_inst         = true;
            restyle          = true;
            instance         = e;
            instance->id     = hold(id);
            instance->mounts = map (hsize, 32);
            instance->parent = parent; /// weak reference
            if (!parent->elements)
                 parent->elements = map(hsize, 16);
            set (parent->elements, id, instance);
            //set (parent->mounts,   id, instance);
        } else {
            element ee = e;
            changed = apply_args(ux, instance, e);
            restyle = index_of(changed, string("tags")) >= 0; // tags effects style application
        }
        /*
        
        [ ] element event call 
            think about making callback a base type, 
            one that could turn into event dispatch.
            for security and simplicity, singular
            events is superior and thus why have the
            engineering if you dont want it?

        [ ] scrollable regions
            scrollbar should always be tall enough to
            be comfortable to grab, hwoever it may be
            a pill shape with essentially the page size
            as a kind of gem state within it

        [ ] transitions
            transitions are gathered but not applied
        */
        if (restyle) {
            map  avail  = compute(ux->style, instance);
            list styled = apply_style(ux, instance, avail, changed);
            
            element e_inst = instance;
            /// merge unique props changed from style
            if (styled && changed)
                each(styled, string, prop) {
                    int i = index_of(changed, prop);
                    if (i == -1)
                        push(changed, prop);
                }
        }
        map irender = render(instance, changed);     // first render has a null changed; clear way to perform init/mount logic
        if (irender)  update(ux, instance, irender); // there is no mount or init on these (please dont override init!)
    }
}

void animate_element(composer ux, element e) {
    if (e->transitions) {
        i64 cur_millis = epoch_millis();

        pairs(e->transitions, i) {
            string prop = i->key;
            style_transition ct = i->value;
            i64 dur = tcoord_get_millis(ct->duration);
            i64 millis = cur_millis - ct->start;
            f64 cur_pos = style_transition_pos(ct, (f64)millis / (f64)dur);
            if (ct->type->traits & A_TRAIT_PRIMITIVE) {
                verify(ct->is_inlay, "unsupported member type (primitive in object form)");
                /// mix these primitives; lets support i32 / enum, i64, f32, f64
                AType ct_type = ct->type;

            } else {
                type_member_t* fmix = A_member(ct->type, A_MEMBER_IMETHOD, "mix", false);
                verify(fmix, "animate: implement mix for type %s", ct->type->name);
                typedef object(*mix_fn)(object, object, f32);
                drop(*ct->location);
                *ct->location = hold(((mix_fn)fmix->ptr)(ct->from, ct->to, cur_pos));
                if (eq(prop, "area")) {
                    verify(e->area == *ct->location, "strange");
                }
                /// call mix dynamically; A_method to look it up
            }
        }
    }
    pairs(e->elements, i) { // todo: do we need mounts as separate map?
        element ee = i->value;
        animate_element(ux, ee);
    }
}

void composer_animate(composer ux) {
    animate_element(ux, ux->root);
}

void composer_update_all(composer ux, map render) {
    if (!ux->style)
         ux->style = style((object)ux->app); // effectively loads style/app.css for type-name app
    
    update(ux, ux->root, render); /// 'reloaded' is checked inside the update
    ux->style->reloaded = false;
    
}

void composer_init(composer ux) {
    ux->root = element(id, string("root"));
}

define_class(tcoord, unit, Duration)
define_enum(Ease)
define_enum(Direction)
define_enum(Duration)
define_enum(xalign)
define_enum(yalign)
define_typed_enum(Fill, f32)

define_class(coord,             A)
define_class(alignment,         A)
define_class(region,            A)
define_class(mouse_state,       A)
define_class(keyboard_state,    A)
define_class(qualifier,         A)
define_class(line_info,         A)
define_class(text_sel,          A)
define_class(text,              A)
define_class(composer,          A)
define_class(arg,               A)
define_class(style,             A)
define_class(style_block,       A)
define_class(style_entry,       A)
define_class(style_qualifier,   A)
define_class(style_transition,  A)
define_class(style_selection,   A)

define_class(ion,               A)

define_class(element,           ion)
define_class(button,            element)
define_class(pane,              element)
// future plan (actual app server concept on 'system')
// any classes using ion will have mmap members, as they must be publically accessible on the system
