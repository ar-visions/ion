#include <mx/mx.hpp>

namespace ion {
    
logger console;

size_t length(std::ifstream& in) {
    std::streamsize base = in.tellg();
    in.seekg(0, std::ios::end);
    std::streamsize to_end = in.tellg();
    in.seekg(base);
    return to_end - base;
}

///
int str::index_of(MatchType ct, symbol mp) const {
    int  index = 0;
    
    using Fn = func<bool(const char &)>;
    static umap<MatchType, Fn> match_table {
        { Alpha,     Fn([&](const char &c) -> bool { return  isalpha (c); }) },
        { Numeric,   Fn([&](const char &c) -> bool { return  isdigit (c); }) },
        { WS,        Fn([&](const char &c) -> bool { return  isspace (c); }) }, // lambda must used copy-constructed syntax
        { Printable, Fn([&](const char &c) -> bool { return !isspace (c); }) },
        { String,    Fn([&](const char &c) -> bool { return  strcmp  (&c, mp) == 0; }) },
        { CIString,  Fn([&](const char &c) -> bool { return  strcmp  (&c, mp) == 0; }) }
    };
    
    /// msvc thinks its ambiguous, so i am removing this iterator from str atm.
    cstr pc = data;
    for (;;) {
        char &c = pc[index];
        if  (!c)
            break;
        if (match_table[ct](c))
            return index;
        index++;
    }
    return -1;
}

int snprintf(cstr str, size_t size, const char *format, ...) {
    int n;
    va_list args;
    va_start(args, format);
    
#ifdef _MSC_VER
    n = _vsnprintf_s(str, size, _TRUNCATE, format, args);
#else
    n = vsnprintf(str, size, format, args);
#endif
    
    va_end(args);
    if (n < 0 || n >= (int)size) {
        // handle error here
    }
    return n;
}

str path::mime_type() {
    /// prefix builds have their own resources gathered throughout.
    /// depend on a module and we copy (res/*) -> binary_dir/
    mx e = ext().symbolize();
    static path  data = "data/mime-type.json";
    static map<mx> js =  data.read<var>();
    return js->count(e) ? ((memory*)js[e])->grab() : ((memory*)js["default"])->grab();
}

i64 integer_value(memory *mem) {
    symbol   c = mdata<char>(mem, 0);
    bool blank = c[0] == 0;
    while (isalpha(*c))
        c++;
    return blank ? i64(0) : i64(strtol(c, nullptr, 10));
}

memory *drop(memory *mem) {
    if (mem) mem->drop();
    return mem;
}

memory *grab(memory *mem) {
    if (mem) mem->grab();
    return mem;
}

i64 millis() {
    return i64(
               std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                     std::chrono::system_clock::now().time_since_epoch()).count()
               );
}

/// attach arbs to memory (uses a pointer)
attachment *memory::attach(ion::symbol id, void *data, func<void()> release) {
    if (!atts)
         atts = new doubly<attachment>();
    doubly<attachment> &a = *atts;
    a->push(attachment {id, data, release});
    return &a->last();
}

attachment *memory::find_attachment(ion::symbol id) {
    if (!atts) return nullptr;
    /// const char * symbol should work fine for the cases used
    for (attachment &att:*atts)
        if (id == att.id)
            return &att;
    return nullptr;
}

void *memory::realloc(size_t alloc_reserve, bool fill_default) {
    size_t          sz        = type->base_sz; /// this function 
    u8             *dst       = (u8*)calloc(alloc_reserve, sz);
    u8             *src       = (u8*)origin;
    size_t          mn        = math::min(alloc_reserve, count);
    const bool      prim      = (type->traits & traits::primitive) != 0;
    ///
    if (prim) {
        memcpy(dst, src, sz * mn);
    } else {
        alloc_schema *mx = type->schema;
        /// 1-depth of mx here; not supporting schema
        if (mx) {
            for (size_t i = 0; i < mx->bind_count; i++) {
                context_bind &c  = mx->composition[i];
                c.data->functions->copy    (&dst[c.offset], &src[c.offset], mn); /// copy prior data
                c.data->functions->destruct(&src[c.offset], mn); /// destruct prior data
            }
        } else {
            type->functions->copy    (dst, origin, mn);
            type->functions->destruct(origin, mn);
        }
    }
    /// this controls the 'count' which is in-effect the constructed count.  if we are not constructing, no updating count
    if (fill_default) {
        count = alloc_reserve;
        if (prim)
            memset(&dst[sz * mn], 0, sz * (alloc_reserve - mn));
        else /// subtract off constructed-already
            type->functions->construct(&dst[mn], alloc_reserve - mn);
    }
    free(origin);
    origin  = raw_t(dst);
    reserve = alloc_reserve;
    return origin;
}

/// mx-objects are clearable which brings their count to 0 after destruction
void memory::clear() {
    alloc_schema *mx  = type->schema; ///
    u8           *dst = (u8*)origin;
    /// 1-depth of mx here; not supporting schema
    if (mx) {
        for (size_t i = 0; i < mx->bind_count; i++) { /// count should be called bind_count or something; its too ambiguous with memory
            context_bind &c  = mx->composition[i];
            c.data->functions->destruct(&dst[c.offset], count); /// destruct prior data
        }
    } else
        type->functions->destruct(dst, count);
    
    count = 0;
}

memory *memory::stringify(cstr cs, size_t len, size_t rsv, bool constant, type_t ctype, i64 id) {
    ion::symbol sym = (ion::symbol)(cs ? cs : "");
    if (constant) {
        if(!ctype->symbols)
            ctype->symbols = new symbol_data { };
        u64  h_sym = djb2(cstr(sym));
        memory *&m = ctype->symbols->djb2[h_sym];
        if (!m) {
            size_t ln = (len == memory::autolen) ? strlen(sym) : len; /// like auto-wash
            m = memory::alloc(typeof(char), len, len + 1, raw_t(sym));
            m->attrs = attr::constant;
            ctype->symbols->ids[id] = m; /// was not hashing by id, it was the djb2 again (incorrect)
            //ctype->symbol_djb2[h_sym] = m; 'redundant due to the setting of the memory*& (which [] operator always inserts item)
            ctype->symbols->list->push(m);
        }
        return m->grab();
    } else {
        size_t     ln = (len == memory::autolen) ? strlen(sym) : len;
        size_t     al = rsv ? rsv : (strlen(sym) + 1);
        memory*   mem = memory::alloc(typeof(char), ln, al, raw_t(sym));
        cstr  start   = mem->data<char>(0);
        start[ln]     = 0;
        return mem;
    }
}

memory *memory::string (std::string s) { return stringify(cstr(s.c_str()), s.length(), 0, false, typeof(char), 0); }
memory *memory::cstring(cstr s)        { return stringify(cstr(s),         strlen(s),  0, false, typeof(char), 0); }

memory *memory::symbol (ion::symbol s, type_t ty, i64 id) {
    return stringify(cstr(s), strlen(s), 0, true, ty, id);
}

memory *memory::raw_alloc(type_t type, size_t sz, size_t count, size_t res) {
    size_t elements = math::max(count, res);
    memory*     mem = (memory*)calloc(1, sizeof(memory) + elements * sz); /// there was a 16 multiplier prior.  todo: add address sanitizer support with appropriate clang stuff
    mem->count      = count;
    mem->reserve    = res;
    mem->refs       = 1;
    mem->type       = type;
    mem->origin     = (void*)&mem[1];
    return mem;
}
/*
memory *memory::wrap(raw_t m, type_t ty) {
    memory*     mem = (memory*)calloc(1, sizeof(memory)); /// there was a 16 multiplier prior.  todo: add address sanitizer support with appropriate clang stuff
    mem->count      = 1;
    mem->reserve    = 1;
    mem->refs       = 1;
    mem->type       = ty;
    mem->origin     = m;
    return mem;
}
*/

/// starting at 1, it should remain active.  shall not be freed as a result
void memory::drop() {
    if (--refs <= 0 && !constant) { /// <= because ptr does a defer on the actual construction of the container class
        origin = null;
        // delete attachment lambda after calling it
        if (atts) {
            for (attachment &att: *atts)
                att.release();
            delete atts;
            atts = null;
        }
        if (managed)
            free(origin);
        if (shape) {
            delete shape;
            shape = null;
        }
        //free(this);
    }
}

/// now we start allocating the total_size (or type->base_sz if not schema-bound (mx classes accept schema.  they say, icanfly.. imapilot))
memory *memory::alloc(type_t type, size_t count, size_t reserve, raw_t v_src) {
    memory *result = null;
    size_t type_sz = type->size();

    if (type->singleton)
        return type->singleton->grab();
    
    memory *mem = memory::raw_alloc(type, type_sz, count, reserve);

    if (type->singleton)
        type->singleton = mem;
    
    /// schema needs to have if its a ptr or not
    /// if allocating a schema-based object (mx being first user of this)
    if (count > 0) {
        if (type->schema) {
            if (v_src) {
                /// if schema-copy-construct (call cpctr for each data type in composition)
                for (size_t i = 0; i < type->schema->bind_count; i++) {
                    context_bind &bind = type->schema->composition[i];
                    u8 *dst = &((u8*)mem->origin)[bind.offset];
                    u8 *src = &((u8*)      v_src)[bind.offset];
                    if (bind.data) bind.data->functions->copy(dst, src, count);
                }
            } else {
                /// ctr: call construct across the composition
                for (size_t i = 0; i < type->schema->bind_count; i++) {
                    context_bind &bind = type->schema->composition[i];
                    u8 *dst  = &((u8*)mem->origin)[bind.offset];
                    if (bind.data) bind.data->functions->construct(dst, count); /// issue is origin passed and effectively reused memory! /// probably good to avoid the context bind at all if there is no data; in some cases it may not be required and the context does offer some insight by itself
                }
            }
        } else {
            const bool prim = (type->traits & traits::primitive);
            if (v_src)
                type->functions->copy(mem->origin, v_src, count);
            else if (!prim)
                type->functions->construct(mem->origin, count);
        }
    }
    return mem;
}

memory *memory::copy(size_t reserve) {
    memory *a   = this;
    memory *res = alloc(a->type, a->count, reserve, a->origin);
    return  res;
}

memory *memory::grab() {
    refs++;
    return this;
}

size &size::operator=(const size b) {
    memcpy(values, b.values, sizeof(values));
    count = b.count;
    return *this;
}


void chdir(std::string c) {
#if defined(_WIN32)
    // replace w global for both cases; its not worth it
    //SetCurrentDirectory(c.c_str());
#else
    ::chdir(c.c_str());
#endif
}

memory* mem_symbol(ion::symbol cs, type_t ty, i64 id) {
    return memory::symbol(cs, ty, id);
}

void *mem_origin(memory *mem) {
    return mem->origin;
}

memory *cstring(cstr cs, size_t len, size_t reserve, bool is_constant) {
    return memory::stringify(cs, len, 0, is_constant, typeof(char), 0);
}
}