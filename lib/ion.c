#include <import>
#include <math.h>

static const real PI = 3.1415926535897932384; // M_PI;
static const real c1 = 1.70158;
static const real c2 = c1 * 1.525;
static const real c3 = c1 + 1;
static const real c4 = (2 * PI) / 3;
static const real c5 = (2 * PI) / 4.5;


bool qualifier_cast_bool(qualifier q) {
    return len(q->type) || q->id || q->state;
}

bool style_transition_cast_bool(style_transition a) {
    return a->dur > 0;
}

style_transition style_transition_with_string(style_transition a, string s) {
    array sp = split(s, " ");
    sz    ln = len(sp);
    /// syntax:
    /// 500ms [ease [out]]
    /// 0.2s -- will be linear with in (argument meaningless for linear but applies to all others)
    a->dur    = unit_Duration((string)sp->elements[0]);
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

style_entry style_best_match(node n, string prop_name, array entries) {
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

num style_block_score(style_block a, node n, bool score_state) {
    f64 best_sc = 0;
    node   cur     = n;

    each (a->quals, qualifier, q) {
        f64 best_this = 0;
        for (;;) {
            bool    id_match  = q->id &&  q->id == cur->id;
            bool   id_reject  = q->id && !id_match;
            bool  type_match  = q->ty &&  q->ty == isa(cur);
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
                cur = cur->parent ? cur->parent : node(); // parent node
            } else
                break;
        }
        best_sc = max(best_sc, best_this);
    }
    return best_sc > 0 ? 1.0 : 0.0;
};

f64 Duration_base_millis(Duration dur) {
    switch (dur) {
        case Duration_undefined: return 0;
        case Duration_ns: return 1.0 / 1000.0;
        case Duration_ms: return 1.0;
        case Duration_s:  return 1000.0;
    }
    return 0.0;
}

bool style_applicable(style s, node n, string prop_name, array result) {
    array blocks = get(s->members, prop_name);
    AType type   = isa(n);
    bool  ret    = false;

    clear(result);
    each (blocks, style_block, block) {
        if (!len(block->types) || index_of(block->types, type) >= 0) {
            item f = lookup(block->entries, prop_name); // this returns the wrong kind of item reference
            if (f && score(block, n, false) > 0) {
                AType verify1 = isa(f);
                push(result, f->value);
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


none composer_update(composer ux, node parent, ARef r_instance, node e) {
    node* instance = r_instance;
}

style style_with_path(style a, path css_path) {
    verify(exists(css_path), "css path does not exist");
    string style_str = read(css_path, typeid(string));
    a->root    = array(alloc, 4);
    a->members = map(hsize, 32);
    load(a, style_str);
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
    path   css_path  = form(path, "style/{0}.css", root_type);
    a->reloader = watch(
        res,        css_path,
        callback,   style_watch_reload);
    return a;
}



/// id's can be dynamic so we cant enforce symbols, and it would be a bit lame to make the caller symbolize
string node_id(node e) {
    array &args = e->args;
    for (arg &a: args.elements<arg>()) {
        if (strcmp((symbol)a.key.mem->origin, "id") == 0)
            return (cstr)a.value.mem->origin; /// convert to string
    }
    return e->type->name;
}

static memory *_app_memory;

void composer::set_app(memory *app_memory) {
    _app_memory = app_memory;
}

void composer_update(composer ux, node parent, ARef r_instance, node e) {
    node*  instance = r_instance;
    bool       diff = !instance;
    bool     is_new = false;
    size_t args_len = e->args.len();
    i64         now = millis();
    bool style_reload = ux->style->reloaded;

    /// recursion here
    if (e.type() == typeof(node) && e->children) {
        sz clen = len(e->children);
        sz i    = 0;
        array a_instances = (instance && *instance) ? (*instance)->children : 
            array(alloc, clen);

        /// set instances node array, then we specify the item pointer for each
        if (!(instance && *instance))
            *instance = node(instances, a_instances);
        
        each (e->children, node, cn)
            update(ux, parent, &a_instances->elements[i++], cn);
        
        return;
    }

    if (!diff) {
        /// instance != null, so we can check attributes
        /// compare args for diffs
        Array<arg>    &p = (*(node*)instance)->args; /// previous args
        Array<arg>    &n = e->args; /// new args
        diff = args_len != p.len();
        if (!diff) {
            /// no reason to check against a map
            for (size_t i = 0; i < args_len; i++) {
                arg &p_pair = p[i];
                arg &n_pair = n[i];
                mx key  (p_pair.key);
                mx value(p_pair.value);
                if (key   != n_pair.key ||
                    value != n_pair.value) {
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
            string   id = node_id(e); /// needed for style computation of available entries in the style blocks
            (*instance)->parent = parent;
            (*instance)->id     = hold(id);
            (*instance)->composer = ux;
            
            /// compute available properties for this Element given its type, placement, and props styled 
        }

        if (style_reload || is_new)
            (*instance)->style_avail = compute(ux->style, instance);

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
            properties* props = (properties*)tdata->meta;

            /// its possible some classes may not have meta information defined, just skip
            if (!meta_map)
                continue;

            /// apply style to props (only do this when we need to, as this will be inefficient to do per update)
            /// dom uses invalidation for this such as property changed in a parent, element added, etc
            /// it does not need to be perfect we are not making a web browser
            for (prop &p: props->elements<prop>()) {
                string &name = *p.s_key;
                field *entries = style_avail->lookup(name); // ctx name is Button, name == id, and it has a null entry for entries[0] == null with count > 
                if (entries) {
                    /// this should be in a single function:
                    /// get best style matching entry for this property
                    Array<style::entry*> e(entries->value);
                    style::entry *best = composer->style->best_match(instance, &p, e);
                    ///
                    type_t prop_type = p.type;
                    u8    *prop_dst  = &data_origin[p.offset];
                    
                    /// dont repeat this; store best in transitions lookup for this member name
                    /// this must be allocated in the map
                    node::selection &sel = instance->data->selections.get<node::selection>(name);

                    /// in cases there will be no match, in other cases we have already selected
                    if (best && (best != sel.entry)) {
                        bool should_trans = best->trans && sel.member;

                        /// create instance and immediately assign if there is no transition
                        if (prop_type->traits & traits::mx_obj) {
                            if (!best->mx_instance) {
                                void *alloc = prop_type->ctr_str(string(best->value));
                                best->mx_instance = new mx(((MX*)alloc)->mem);
                                prop_type->f.dtr(alloc);
                            }
                            if (!should_trans) {
                                prop_type->f.dtr(prop_dst);
                                prop_type->f.ctr_mem(prop_dst, hold(best->mx_instance->mem));
                            }
                        } else {
                            if (!best->raw_instance) {
                                void *alloc = prop_type->ctr_str(string(best->value));
                                best->raw_instance = alloc;
                            }
                            if (!should_trans) {
                                prop_type->f.dtr(prop_dst);
                                prop_type->f.ctr_cp(prop_dst, best->raw_instance);
                            }
                        }

                        /// if we had a prior transition, delete the memory
                        if (sel.from) prop_type->f.dtr(sel.from);

                        /// handle transitions, only when we have previously selected (otherwise we are transitioning from default state)
                        if (should_trans) {
                            /// get copy of current value (new instance, and assign from current)
                            raw_t cur;
                            if (prop_type->traits & traits::mx_obj)
                                cur = prop_type->ctr_mem(((MX*)prop_dst)->mem);
                            else
                                cur = prop_type->ctr_cp(prop_dst);
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
                        if (prop_type->traits & traits::mx_obj)
                            prop_type->f.ctr_mem(prop_dst, hold(((mx*)p.init_value)->mem));
                        else
                            prop_type->f.ctr_cp(prop_dst, p.init_value);
                        
                        sel.start  = 0;
                        sel.end    = 0;
                        sel.from   = null;
                        sel.member = &p;
                        sel.entry = null;
                    }
                }
            }

            /// handle selected transitions
            for (auto &field: instance->data->selections.fields()) {
                node::selection &sel = field.value.ref<node::selection>();
                if (sel.start > 0 && sel.member->parent_type == tdata) { /// make sure we have the correct data origin!
                    real   amount = math::clamp(real(now - sel.start) / real(sel.end - sel.start), 0.0, 1.0);
                    real   curve  = sel.entry->trans.pos(amount);
                    type_t prop_type = sel.member->type;
                    u8    *prop_dst  = &data_origin[sel.member->offset];

                    raw_t temp = calloc(1, sizeof(prop_type->base_sz));
                    if (prop_type == typeof(double)) {
                        temp = new double(*(double*)sel.from * (1.0 - curve) + *(double*)sel.to * curve);
                    } else {
                        assert(prop_type->f.mix);
                        prop_type->f.mix(sel.from, sel.to, temp, curve);
                    }
                    if (prop_type->traits & traits::mx_obj)
                        prop_type->f.ctr_mem(prop_dst, hold(((mx*)temp)->mem));
                    else
                        prop_type->f.ctr_cp(prop_dst, temp);

                    prop_type->f.dtr(temp);
                    free(temp);
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
                    mx      pdef = meta_map->lookup(s_key);
                    
                    if (pdef) {
                        prop *def = pdef.get<prop>(0);
                        /// duplicates are not allowed in ux; we could handle it just not now; meta map would need to return a list not the type.  iceman: ugh!
                        assert(def->parent_type == tdata);

                        type_t prop_type = def->type;
                        type_t arg_type  = a.value.mem->type;
                        u8    *prop_dst  = &data_origin[def->offset];
                        u8    *arg_src   = (u8*)a.value.mem->typed_data(arg_type, 0); /// passing int into mx recovers int, but passing lambda will give data inside.  we want to store a context
                        u8    *conv_inst = null;
                        string   str_res;
                        bool  free = false;

                        /// if prop type is mx, we can generalize
                        if (prop_type == typeof(mx)) {
                            if (arg_type != prop_type) {
                                conv_inst = (u8*)new mx(a.value);
                                arg_src   = (u8*)conv_inst;
                                arg_type  = typeof(mx);
                                free      = true;
                            }
                        }
                        /// if going to string and arg is not, we convert
                        else if (prop_type == typeof(string) && arg_type != prop_type) {
                            /// general to_string conversion (memory of char)
                            assert(arg_type->f.to_str);
                            conv_inst = (u8*)new mx(arg_type->f.to_str(arg_src));
                            arg_src   = (u8*)conv_inst;
                            arg_type  = typeof(string);
                            free      = true;
                        }
                        /// general from_string conversion.  the class needs to have a cstr constructor
                        else if ((arg_type == typeof(char) || arg_type == typeof(string)) && prop_type != arg_type) {
                            assert(prop_type->f.ctr_str);
                            conv_inst = (u8*)prop_type->ctr_str(arg_type == typeof(string) ?
                                (cstr)a.value.mem->origin : (cstr)arg_src);
                            arg_src = (u8*)conv_inst;
                            arg_type = prop_type;
                            free     = false;
                        }
                        /// types should match
                        assert(arg_type == prop_type || arg_type->ref == prop_type);

                        prop_type->f.dtr(prop_dst);
                        if (prop_type->traits & traits::mx_obj) {
                            /// set by memory construction (cast conv_inst as mx which it must be)
                            assert(!conv_inst || (arg_type->traits & traits::mx_obj) ||
                                                 (arg_type->traits & traits::mx));
                            prop_type->f.ctr_mem(prop_dst, hold(conv_inst ? ((mx*)conv_inst)->mem : a.value.mem));
                        } else {
                            /// assign property with data that is of the same type
                            prop_type->f.ctr_cp(prop_dst, arg_src);
                        }
                        pset[i] = true;
                        if (conv_inst) {
                            arg_type->f.dtr(conv_inst);
                            if (free) ::free(conv_inst);
                        }
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
                    string id = node_id(*e);
                    node *&n_mount = n->mounts.get<node*>(id);
                    update(composer, instance, n_mount, *e);
                }
            /// can also be stored in a map
            } else if (render->type == typeof(map)) {
            } else if (render->type == typeof(Array<node>)) {
            } else {
                string id = node_id(render);
                node *&n_mount = n->mounts.get<node*>(id);
                update(composer, instance, n_mount, render);
            }
        }
        /// need to know if its mounted, or changed by argument
        /// it can know if a style is different but i dont see major value here
        if (is_new) {
            if (instance->data->ref) {
                instance->data->ref(mx(instance->mem->hold()));
            }
            instance->data->app = (adata*)_app_memory->origin;
            instance->mounted();
        }
    }
}

void composer_update_all(composer ux, node e) {
    if (!ux->instances)
        ux->style = style();
    lock(ux->style->mtx);
    update(ux, null, ux->instances, e); /// 'reloaded' is checked inside the update
    ux->style->reloaded = false;
    unlock(ux->style->mtx);
}

bool is_cmt(symbol c) {
    return c[0] == '/' && c[1] == '*';
}

bool ws(cstr &cursor) {
    while (isspace(*cursor) || is_cmt(cursor)) {
        while (isspace(*cursor))
            cursor++;
        if (is_cmt(cursor)) {
            cstr f = strstr(cursor, "*/");
            cursor = f ? &f[2] : &cursor[strlen(cursor) - 1];
        }
    }
    return *cursor != 0;
}

bool scan_to(cstr &cursor, string chars) {
    bool sl  = false;
    bool qt  = false;
    bool qt2 = false;
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
            if (index_of(chars, cur) >= 0)
                 return true;
        }
    }
    return false;
}

doubly parse_qualifiers(style::block &bl, cstr *p) {
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
    list result;

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
            string q = trim(parent_to_child[i]);
            if (processed) {
                v->parent = qualifier();
                v = hold(v->parent); /// dont need to cast this
            }
            num idot = index_of(q, ".");
            num icol = index_of(q, ":");
            string tail;
            ///
            if (idot >= 0) {
                array sp = split(q, ".");
                v->type   = sp[0];
                string sp2 = split((string)sp->elements[1], ":");
                v->id     = sp2->elements[0];
                if (icol >= 0)
                    tail  = q.mid(icol + 1).trim(); /// likely fine to use the [1] component of the split
            } else {
                if (icol  >= 0) {
                    v->type = q.mid(0, icol);
                    tail   = q.mid(icol + 1).trim();
                } else
                    v->type = q;
            }
            if (v->type) { /// todo: verify idata is correctly registered and looked up
                v->ty = ident::types->lookup(v->type).get<idata>();
                if (bl.types.index_of(v->ty) == -1) {
                    assert(v->ty); /// type must exist
                    bl.types += v->ty;
                }
            }
            Array<string> ops {"!=",">=","<=",">","<","="};
            if (tail) {
                // check for ops
                bool is_op = false;
                for (string &op:ops) {
                    if (tail.index_of(op.cs()) >= 0) {
                        is_op   = true;
                        Array<string> sp = tail.split(op);
                        v->state = sp[0].trim();
                        v->oper  = op;
                        v->value = tail.mid(sp[0].len() + op.len()).trim();
                        break;
                    }
                }
                if (!is_op)
                    v->state = tail;
            }
            processed = &v;
        }
    }
    *p = end;
    return result;
}

/// compute available entries for props on a Element
map style_compute(style a, node n) {
    map avail = map(hmap, 16);
    AType ctx = isa(n);
    while (ctx) {
        array all = array(alloc, 32);
        for (int m = 0; m < ty->member_count; m++) {
            member_type_t* mem = ty->members[m];
            if (mem->member_type != A_MEMBER_PROP)
                continue;
            if (applicable(a, n, prop_name, all)) {
                set(avail, prop_name, all);
                all = array(alloc, 32);
            }
        }
        ctx = ctx->parent_type;
    }
    return avail;
}

static void cache_b(style a, style_block bl) {
    each (bl->entries, entry, e) {
        bool  found = false;
        array cache = get(a->members, e->member);
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
    if (a->root)
        each (root->elements, style_block, b)
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


none parse_block(block bl, cstr sc) {
    ws(sc);
    verify(*sc == '.' || isalpha(*sc), "expected Type[.id], or .id");
    bl->quals = parse_qualifiers(*bl, &sc);
    ws(++sc);
    ///
    while (*sc && *sc != '}') {
        /// read up to ;, {, or }
        ws(sc);
        cstr start = sc;
        verify(scan_to(sc, ";{}"), "expected member expression or qualifier");
        if (*sc == '{') {
            ///
            block *bl_n = new style::block();
            bl->blocks->push(bl_n);
            bl_n->parent = bl;

            /// parse sub-block
            sc = start;
            parse_block(bl_n);
            verify(*sc == '}', "expected }");
            ws(++sc);
            ///
        } else if (*sc == ';') {
            /// read member
            cstr cur = start;
            console.test(scan_to(cur, ":") && (cur < sc), "expected [member:]value;");
            string  member = string(chars, start, ref_length, distance(start, cur));
            ws(++cur);

            /// read value
            cstr vstart = cur;
            console.test(scan_to(cur, ";"), "expected member:[value;]");
            
            /// needs escape sequencing?
            size_t len = distance(vstart, cur);
            string cb_value = trim(string(chars, vstart, ref_length, len));
            string      end = cb_value.mid(-1, 1);
            bool       qs = mid(cb_value,  0, 1) == "\"";
            bool       qe = mid(cb_value, -1, 1) == "\"";

            if (qs && qe) {
                cstr   cs = cb_value.cs();
                cb_value  = parse_quoted(&cs, cb_value.len());
            }

            num         i = index_of(cb_value, ",");
            string  param = i >= 0 ? trim(mid(cb_value, i + 1, len(cb_value) - (i + 1))) : "";
            string  value = i >= 0 ? trim(mid(cb_value, 0, i))  : cb_value;
            style_transition trans = param ? style_transition(param) : null;
            
            /// check
            verify(member, "member cannot be blank");
            verify(value,  "value cannot be blank");
            bl->entries[member] = new entry { member, value, trans, bl };
            /// 
            ws(++sc);
        }
    }
    console.test(!*sc || *sc == '}', "expected closed-brace");
}

void style_load(style a, string code) {
    for (cstr sc = code.cs(); ws(sc); sc++) {
        lambda<void(block*)> parse_block;
        style_block n_block = style_block();
        push(root, n_block);
        parse_block(n_block);
    }
}

define_meta(unit_Duration, unit, Duration)

define_enum(Ease)
define_enum(Direction)
define_enum(Duration)

define_class(mouse_state)
define_class(keyboard_state)

define_class(qualifier)
define_class(line_info)
define_class(text_sel)
define_class(text)

define_class(composer)

define_class(style)
define_class(style_block)
define_class(style_entry)
define_class(style_qualifier)
define_class(style_transition)
define_class(style_selection)

define_class(node)
define_class(element)

