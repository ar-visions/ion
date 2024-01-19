#include <composer/composer.hpp>
#include <watch/watch.hpp>

using namespace ion;

void composer::update_all(node e) {
    if (!data->instances)
        data->style = style::init();
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

doubly<style::Qualifier> parse_qualifiers(style::block &bl, cstr *p) {
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
    doubly<style::Qualifier> result;

    ///
    for (str &qs:quals) {
        str  qq = qs.trim();
        if (!qq) continue;
        style::Qualifier vv = style::Qualifier(); /// push new qualifier
        result->push(vv);

        /// we need to scan by >

        style::Qualifier v = vv;
        array<str> parent_to_child = qq.split("/"); /// we choose not to use > because it interferes with ops
        style::Qualifier *processed = null;

        if (parent_to_child.len() > 1) {
            int test = 0;
        }
        /// iterate through reverse
        for (int i = parent_to_child.len() - 1; i >= 0; i--) {
            str q = parent_to_child[i].trim();
            if (processed) {
                v->parent = style::Qualifier();
                v = v->parent.hold(); /// dont need to cast this
            }
            num idot = q.index_of(".");
            num icol = q.index_of(":");
            str tail;
            ///
            if (idot >= 0) {
                array<str> sp = q.split(".");
                v->type   = sp[0];
                v->id     = sp[1].split(":")[0];
                if (icol >= 0)
                    tail  = q.mid(icol + 1).trim(); /// likely fine to use the [1] component of the split
            } else {
                if (icol  >= 0) {
                    v->type = q.mid(0, icol);
                    tail   = q.mid(icol + 1).trim();
                } else
                    v->type = q;
            }
            if (v->type) {
                v->ty = types::lookup(v->type);
                if (bl.types.index_of(v->ty) == -1) {
                    assert(v->ty); /// type must exist
                    bl.types += v->ty;
                }
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
        array<entry *> all(32);
        for (prop &p: *(doubly<prop>*)data->meta) { /// the blocks can contain a map of types with entries associated, with a * key for all
            if (*p.s_key == "fill-color") {
                int test = 0;
            }
            if (applicable(n, &p, all)) {
                str   s_name  = p.key->hold(); /// it will free if you dont hold it
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
            if (bl->quals[0]->state == "test1") {
                int test = 0;
                test++;
            }
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
        static std::mutex mx; /// more checks, less locks, less time? [ssh server ends up calling this in another thread]
        mx.lock();
        if (!st->loaded) {
            /// if there are no files it doesnt load, or reload.. thats kind of not a bug
            watch::fn reload = [st](bool first, array<path_op> &ops) {
                style &s = (style&)st;
                path base_path = "style";
                s->mtx.lock();
                s->root = {};
                s->members = {size_t(32)};
                base_path.resources({".css"}, {},
                    [&](path css_file) -> void {
                        str style_str = css_file.read<str>();
                        s->load(style_str);
                    });

                /// store blocks by member, the interface into all style: style::members[name]
                s->cache_members();
                s->loaded = true;
                s->reloaded = true; /// signal to composer user
                s->mtx.unlock();
            };

            /// spawn watcher (this syncs once before returning)
            st->reloader = watch::spawn({"./style"}, {".css"}, {}, reload);
        }
        mx.unlock();
    }
    return st;
}

/// todo: move functions into itype pattern
bool style::impl::applicable(node *n, prop *member, array<style::entry*> &result) {
    array<style::block*> &blocks = members[*member->s_key]; /// members must be 'sample' on load, and retrieving it should return a block
    type_t type = n->mem->type;
    bool ret = false;

    if (n->data->id == "play-pause" && *member->s_key == "fill-color") { /// annotate should have test1 apply, because it should be a style block
        int test = 0;
    }
    for (style::block *block:blocks) {
        style::Qualifier qual = block->quals[0];
        if (!block->types || block->types.index_of(type) >= 0) {
            auto f = block->entries->lookup(*member->s_key); /// VideoView stored, and then retrieved from members map
            if (block->quals[0]->state == "hover") {
                int test = 0;
            }
            if (f) {
                int test = 0;
                if (block->score(n, false) > 0) {
                    result += f->value;
                    ret = true;
                }
            }
        }
    }
    return ret;
}