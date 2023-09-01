#include <composer/composer.hpp>

using namespace ion;

/// id's can be dynamic so we cant enforce symbols, and it would be a bit lame to make the caller symbolize
str node_id(node &e) {
    array<arg> &args = e->args;
    for (arg &a: args) {
        if (strcmp((symbol)a.key.mem->origin, "id") == 0)
            return (cstr)a.value.mem->origin; /// convert to str
    }
    return e->type->name;
}

double duration_millis(duration dur) {
    switch (dur.value) {
        case duration::ns: return 1.0 / 1000.0;
        case duration::ms: return 1.0;
        case duration::s:  return 1000.0;
    }
    return 0.0;
}

void composer::update(composer::cdata *composer, node *parent, node *&instance, node &e) {
    bool       diff = !instance;
    bool     is_new = false;
    size_t args_len = e->args.len();
    i64         now = millis();

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
    if (diff) {
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
            (*instance)->id     = id.grab();
        
            /// compute available properties for this Element given its type, placement, and props styled 
            (*instance)->style_avail = composer->style->compute(instance);
        }

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
                    type_t prop_type = p.member_type;
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
                    type_t prop_type = sel.member->member_type;
                    u8    *prop_dst  = &data_origin[sel.member->offset];

                    assert(prop_type->functions->mix);
                    raw_t temp = prop_type->functions->mix(sel.from, sel.to, curve);
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

                        type_t prop_type = def->member_type;
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
                            conv_inst = (u8*)new str(m_str); /// no grab here
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
                        assert(arg_type == prop_type);

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
            if (render->children) {
                /// each child is mounted
                for (node *e: render->children) {
                    str id = node_id(*e);
                    update(composer, instance, n->mounts[id], *e);
                }
            } else {
                str id = node_id(render);
                update(composer, instance, n->mounts[id], render);
            }
        }
        /// need to know if its mounted, or changed by argument
        /// it can know if a style is different but i dont see major value here
        if (is_new) {
            instance->mounted();
        }
    }
}

void composer::update_all(node e) {
    if (!data->instances)
        data->style = style::init();
    
    update(data, null, data->instances, e);
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

bool scan_to(cstr &cursor, array<char> chars) {
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

doubly<style::qualifier*> parse_qualifiers(style::block &bl, cstr *p) {
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
    doubly<style::qualifier*> result;

    ///
    for (str &qs:quals) {
        str  q = qs.trim();
        if (!q) continue;
        style::qualifier *v = new style::qualifier();
        result->push(v);

        num idot = q.index_of(".");
        num icol = q.index_of(":");
        str tail;
        ///
        if (idot >= 0) {
            array<str> sp = q.split(".");
            v->type   = sp[0];
            v->id     = sp[1];
            if (icol >= 0)
                tail  = q.mid(icol + 1).trim();
        } else {
            if (icol  >= 0) {
                v->type = q.mid(0, icol);
                tail   = q.mid(icol + 1).trim();
            } else
                v->type = q;
        }
        if (v->type) {
            v->ty = types::lookup(v->type);
            if (bl.types.index_of(v->ty) == -1)
                bl.types += v->ty;
        }
        array<str> ops {"!=",">=","<=",">","<","="};
        if (tail) {
            // check for ops
            bool is_op = false;
            for (str &op:ops) {
                if (tail.index_of(op.cs()) >= 0) {
                    is_op   = true;
                    auto sp = tail.split(op);
                    v->state = sp[0].trim();
                    v->oper  = op;
                    v->value = tail.mid(sp[0].len() + op.len()).trim();
                    break;
                }
            }
            if (!is_op)
                v->state = tail;
        }
    }
    *p = end;
    return result;
}

size_t style::block::score(node *pn, bool score_state) {
    node &n = *pn;
    double best_sc = 0;
    node::edata *edata = ((node*)pn)->data;
    for (qualifier *q:quals) {
        qualifier &qd = *q;
        bool    id_match  = qd.id    &&  qd.id == n->id;
        bool   id_reject  = qd.id    && !id_match;
        bool  type_match  = qd.type  &&  strcmp((symbol)qd.type.cs(), (symbol)n.mem->type->name) == 0; /// class names are actual type names
        bool type_reject  = qd.type  && !type_match;
        bool state_match  = score_state && qd.state && get_bool(typeof(node::edata), edata, qd.state); /// a useful thing for string const input would be to flag that its already symbolized.  that way you dont look it up when you operate it back
        bool state_reject = score_state && qd.state && !state_match;

        ///
        if (!id_reject && !type_reject && !state_reject) {
            double sc = size_t(   id_match) << 1 |
                        size_t( type_match) << 0 |
                        size_t(state_match) << 2;
            best_sc = math::max(sc, best_sc);
        }
    }
    return best_sc;
};

/// each qualifier is defined as it, and all of the blocked qualifiers below.
/// there are more optimal data structures to use for this matching routine
/// state > state2 would be a nifty one, no operation indicates bool, as-is current normal syntax
double style::block::match(node *from, bool match_state) {
    block          *bl = this;
    double total_score = 0;
    int            div = 0;
    node            *e = from;
    ///
    while (bl && e) {
        bool   is_root = !bl->parent;
        double score   = 0;
        ///
        while (e) { ///
            auto sc = bl->score(e, match_state);
            e       = (node*)e->node::data->parent;
            if (sc == 0 && is_root) {
                score = 0;
                break;
            } else if (sc > 0) {
                score += double(sc) / ++div;
                break;
            }
        }
        total_score += score;
        ///
        if (score > 0) {
            /// proceed...
            bl = bl->parent;
        } else {
            /// style not matched
            bl = null;
            total_score = 0;
        }
    }
    return total_score;
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
        array<entry *> all(32);
        for (prop &p: *(doubly<prop>*)data->meta) { /// the blocks can contain a map of types with entries associated, with a * key for all
            if (applicable(n, &p, all)) {
                str   s_name  = p.key->grab(); /// it will free if you dont grab it
                avail[s_name] = all;
                all = array<entry *>(32);
            }
        }
    }
    return avail;
}

void style::impl::cache_members() {
    lambda<void(block*)> cache_b;
    ///
    cache_b = [&](block *bl) -> void {
        for (entry *e: bl->entries) {
            bool  found = false;
            ///
            array<block*> &cache = members[e->member];
            for (block *cb:cache)
                 found |= cb == bl;
            ///
            if (!found)
                 cache += bl;
        }
        for (block *s:bl->blocks)
            cache_b(s);
    };
    if (root)
        for (block *b:root)
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

style style::init() {
    style st;
    if (!st->loaded) {
        path base_path = "style";
        /// there could be sub-dirs here with an argument
        base_path.resources({".css"}, {},
            [&](path css_file) -> void {
                str style_str = css_file.read<str>();
                st->load(style_str);
            });

        /// store blocks by member, the interface into all style: style::members[name]
        st->cache_members();
        st->loaded = true;
    }
    return st;
}

style::entry *style::impl::best_match(node *n, prop *member, array<style::entry*> &entries) {
    array<style::block*> &blocks = members[*member->s_key]; /// instance function when loading and updating style, managed map of [style::block*]
    style::entry *match = null; /// key is always a symbol, and maps are keyed by symbol
    real     best_score = 0;

    /// should narrow down by type used in the various blocks by referring to the qualifier
    /// find top style pair for this member
    type_t type = n->mem->type;
    for (style::entry *e: entries) {
        block *bl = e->bl;
        real score = bl->match(n, true);
        if (score > 0 && score >= best_score) {
            match = bl->entries[*member->s_key];
            assert(match);
            best_score = score;
        }
    }
    return match;
}

/// todo: move functions into itype pattern
bool style::impl::applicable(node *n, prop *member, array<style::entry*> &result) {
    array<style::block*> &blocks = members[*member->s_key];
    type_t type = n->mem->type;
    bool ret = false;

    for (style::block *block:blocks) {
        if (!block->types || block->types.index_of(type) >= 0) {
            auto f = block->entries->lookup(*member->s_key);
            if (f && block->match(n, false) > 0) {
                result += f->value;
                ret = true;
            }
        }
    }
    return ret;
}