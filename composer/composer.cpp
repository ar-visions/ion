#include <composer/composer.hpp>
#include <watch/watch.hpp>

using namespace ion;

static str root_app_class;


/// to debug style, place conditional breakpoint on member->s_key == "your-prop" (used in a style block) and n->data->id == "id-you-are-debugging"
/// hopefully we dont have to do this anymore.  its simple and it works.  we may be doing our own style across service component and elemental component but having one system for all is preferred,
/// and brings a sense of orthogonality to the react-like pattern, adds type-based contextual grabs and field lookups with prop accessors
style::entry *style::impl::best_match(node *n, prop *member, Array<style::entry*> &entries) {
    Array<style::block*> &blocks = members.get<Array<style::block*>>(*member->s_key); /// instance function when loading and updating style, managed map of [style::block*]
    style::entry *match = null; /// key is always a symbol, and maps are keyed by symbol
    real     best_score = 0;

    /// should narrow down by type used in the various blocks by referring to the qualifier
    /// find top style pair for this member
    type_t type = n->mem->type;
    for (style::entry *e: entries.elements<style::entry*>()) {
        block *bl = e->bl;
        real score = bl->score(n, true);
        str &prop = *member->s_key;
        if (score > 0 && score >= best_score) { /// Edit:focus is eval'd as false?
            match = bl->entries.get<style::entry*>(prop);
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

    for (Qualifier &q : quals.elements<Qualifier>()) {
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
                    state_match = member->type->f.boolean(addr);
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
    array &args = e->args;
    for (arg &a: args.elements<arg>()) {
        if (strcmp((symbol)a.key.mem->origin, "id") == 0)
            return (cstr)a.value.mem->origin; /// convert to str
    }
    return e->type->name;
}

static memory *_app_memory;

void composer::set_app(memory *app_memory) {
    _app_memory = app_memory;
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
        Array<node*> a_instances = instance ? instance->data->children : Array<node*>(clen, clen);

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
            str   id = node_id(e); /// needed for style computation of available entries in the style blocks
            (*instance)->parent = parent;
            (*instance)->id     = hold(id);
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
            properties* props = (properties*)tdata->meta;

            /// its possible some classes may not have meta information defined, just skip
            if (!meta_map)
                continue;

            /// apply style to props (only do this when we need to, as this will be inefficient to do per update)
            /// dom uses invalidation for this such as property changed in a parent, element added, etc
            /// it does not need to be perfect we are not making a web browser
            for (prop &p: props->elements<prop>()) {
                str &name = *p.s_key;
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
                                void *alloc = prop_type->ctr_str(str(best->value));
                                best->mx_instance = new mx(((MX*)alloc)->mem);
                                prop_type->f.dtr(alloc);
                            }
                            if (!should_trans) {
                                prop_type->f.dtr(prop_dst);
                                prop_type->f.ctr_mem(prop_dst, hold(best->mx_instance->mem));
                            }
                        } else {
                            if (!best->raw_instance) {
                                void *alloc = prop_type->ctr_str(str(best->value));
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
                        str   str_res;
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
                        else if (prop_type == typeof(str) && arg_type != prop_type) {
                            /// general to_string conversion (memory of char)
                            assert(arg_type->f.str);
                            conv_inst = (u8*)new mx(arg_type->f.str(arg_src));
                            arg_src   = (u8*)conv_inst;
                            arg_type  = typeof(str);
                            free      = true;
                        }
                        /// general from_string conversion.  the class needs to have a cstr constructor
                        else if ((arg_type == typeof(char) || arg_type == typeof(str)) && prop_type != arg_type) {
                            assert(prop_type->f.ctr_str);
                            conv_inst = (u8*)prop_type->ctr_str(arg_type == typeof(str) ?
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
                    str id = node_id(*e);
                    node *&n_mount = n->mounts.get<node*>(id);
                    update(composer, instance, n_mount, *e);
                }
            /// can also be stored in a map
            } else if (render->type == typeof(map)) {
            } else if (render->type == typeof(Array<node>)) {
            } else {
                str id = node_id(render);
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

void composer::update_all(node e) {
    if (!data->instances)
        data->style = style();
    data->style->mtx.lock();
    update(data, null, data->instances, e); /// 'reloaded' is checked inside the update
    data->style->reloaded = false;
    data->style->mtx.unlock();
}

bool ws(cstr &cursor) {
    auto is_cmt = [](symbol c) -> bool { return c[0] == '/' && c[1] == '*'; };
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

bool scan_to(cstr &cursor, Array<char> chars) {
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
        if (!qt && !qt2)
            for (char &c: chars)
                if (*cursor == c)
                    return true;
    }
    return false;
}

doubly parse_qualifiers(style::block &bl, cstr *p) {
    str   qstr;
    cstr start = *p;
    cstr end   = null;
    cstr scan  =  start;
    
    /// read ahead to {
    do {
        if (!*scan || *scan == '{') {
            end  = scan;
            qstr = str(start, std::distance(start, scan));
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
    auto  quals = qstr.split(",");
    doubly result;

    ///
    for (str &qs:quals.elements<str>()) {
        str  qq = qs.trim();
        if (!qq) continue;
        style::Qualifier vv = style::Qualifier(); /// push new qualifier
        result->push(vv);

        /// we need to scan by >

        style::Qualifier v = vv;
        Array<str> parent_to_child = qq.split("/"); /// we choose not to use > because it interferes with ops
        style::Qualifier *processed = null;

        if (parent_to_child.len() > 1) {
            int test = 0;
        }
        /// iterate through reverse
        for (int i = parent_to_child.len() - 1; i >= 0; i--) {
            str q = parent_to_child[i].trim();
            if (processed) {
                v->parent = style::Qualifier();
                v = hold(v->parent); /// dont need to cast this
            }
            num idot = q.index_of(".");
            num icol = q.index_of(":");
            str tail;
            ///
            if (idot >= 0) {
                Array<str> sp = q.split(".");
                v->type   = sp[0];
                Array<str> sp2 = sp[1].split(":");
                v->id     = sp2[0];
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
            Array<str> ops {"!=",">=","<=",">","<","="};
            if (tail) {
                // check for ops
                bool is_op = false;
                for (str &op:ops) {
                    if (tail.index_of(op.cs()) >= 0) {
                        is_op   = true;
                        Array<str> sp = tail.split(op);
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
style::style_map style::impl::compute(node *n) {
    style_map avail(16);
    ///
    for (type_t ctx = n->mem->type; ctx; ctx = ctx->parent) {
        if (!ctx->schema || !ctx->parent)
            continue;
        type_t data = ctx->schema->bind->data;
        if (!data->meta)
            continue;

        ///
        Array<entry *> all(32);
        properties *meta = (properties*)data->meta;
        for (prop &p: meta->elements<prop>()) { /// the blocks can contain a map of types with entries associated, with a * key for all
            if (applicable(n, &p, all)) {
                str   s_name  = p.key->hold(); /// it will free if you dont hold it
                avail[s_name] = all;
                all = Array<entry *>(32);
            }
        }
    }
    return avail;
}

void style::impl::cache_members() {
    lambda<void(block*)> cache_b;
    ///
    cache_b = [&](block *bl) -> void {
        for (entry *e: bl->entries.elements<entry*>()) {
            bool  found = false;
            Array<block*> &cache = members.get<Array<block*>>(e->member);
            for (block *cb:cache)
                 found |= cb == bl;
            ///
            if (!found)
                 cache += bl;
        }
        for (block *s:bl->blocks.elements<block*>())
            cache_b(s);
    };
    if (root)
        for (block *b:root.elements<block*>())
            cache_b(b);
}

void style::impl::load(str code) {
    for (cstr sc = code.cs(); ws(sc); sc++) {
        lambda<void(block*)> parse_block;
        ///
        parse_block = [&](block *bl) {
            ws(sc);
            console.test(*sc == '.' || isalpha(*sc), "expected Type[.id], or .id", {});
            bl->quals = parse_qualifiers(*bl, &sc);
            ws(++sc);
            ///
            while (*sc && *sc != '}') {
                /// read up to ;, {, or }
                ws(sc);
                cstr start = sc;
                console.test(scan_to(sc, {';', '{', '}'}), "expected member expression or qualifier", {});
                if (*sc == '{') {
                    ///
                    block *bl_n = new style::block();
                    bl->blocks->push(bl_n);
                    bl_n->parent = bl;

                    /// parse sub-block
                    sc = start;
                    parse_block(bl_n);
                    assert(*sc == '}');
                    ws(++sc);
                    ///
                } else if (*sc == ';') {
                    /// read member
                    cstr cur = start;
                    console.test(scan_to(cur, {':'}) && (cur < sc), "expected [member:]value;");
                    str  member = str(start, std::distance(start, cur));
                    ws(++cur);

                    /// read value
                    cstr vstart = cur;
                    console.test(scan_to(cur, {';'}), "expected member:[value;]");
                    
                    /// needs escape sequencing?
                    size_t len = std::distance(vstart, cur);
                    str  cb_value = str(vstart, len).trim();
                    str       end = cb_value.mid(-1, 1);
                    bool       qs = cb_value.mid( 0, 1) == "\"";
                    bool       qe = cb_value.mid(-1, 1) == "\"";

                    if (qs && qe) {
                        cstr   cs = cb_value.cs();
                        cb_value  = str::parse_quoted(&cs, cb_value.len());
                    }

                    num         i = cb_value.index_of(",");
                    str     param = i >= 0 ? cb_value.mid(i + 1).trim() : "";
                    str     value = i >= 0 ? cb_value.mid(0, i).trim()  : cb_value;
                    style::transition trans = param ? style::transition(param) : null;
                    
                    /// check
                    console.test(member, "member cannot be blank");
                    console.test(value,  "value cannot be blank");
                    bl->entries[member] = new entry { member, value, trans, bl };
                    /// 
                    ws(++sc);
                }
            }
            console.test(!*sc || *sc == '}', "expected closed-brace");
        };
        ///
        block *n_block = new block();
        root.push(n_block);
        parse_block(n_block);
    }
}

void style::load_from_file(path css_path) {
    assert(css_path.exists());
    str  style_str  = css_path.read<str>();
    data->root = {};
    data->members = {size_t(32)};
    data->load(style_str);
    /// store blocks by member, the interface into all style: style::members[name]
    data->cache_members();
    data->loaded = true;
    data->reloaded = true; /// cache validation for composer user
}

style::style(type_t app_type) : style() {
    watch::fn reload = [&](bool first, Array<path_op> &ops) {
        style &s = (style&)*this;
        str  root_class = app_type->name;
        path css_path   = fmt { "style/{0}.css", { root_class } };
        assert(css_path.exists());
        load_from_file(css_path);
    };
    /// spawn watcher (this syncs once before returning)
    data->reloader = watch::spawn({"./style"}, {".css"}, {}, reload);
}

bool style::impl::applicable(node *n, prop *member, Array<style::entry*> &result) {
    Array<style::block*> &blocks = members.get<Array<style::block*>>(*member->s_key); /// members must be 'sample' on load, and retrieving it should return a block
    type_t type = n->mem->type;
    bool ret = false;

    for (style::block *block:blocks.elements<style::block*>()) {
        //style::Qualifier &qual = block->quals.get<style::Qualifier>(0);
        if (!block->types || block->types.index_of(type) >= 0) {
            auto f = block->entries->lookup(*member->s_key);
            if (f) {
                if (block->score(n, false) > 0) {
                    result += f->value;
                    ret = true;
                }
            }
        }
    }
    return ret;
}