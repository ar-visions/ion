#pragma once

#include <mx/mx.hpp>
#include <math/math.hpp>
#include <watch/watch.hpp>
#include <regex/regex.hpp>

namespace ion {
/// style can go in composer if we are good at separating functionality

enums(duration, ms,
     ns, ms, s);

/// needs more distance units.  pc = parsec
enums(distance, px,
     px, m, cm, in, ft, pc, percent); /// needs % -> percent generic

template <typename U>
struct unit {
    real value;
    U    type;

    unit() : value(0) { }

    unit(str s) {
        s = s.trim();
        if (s.len() == 0) {
            value = 0;
        } else {
            char   first = s[0];
            bool   is_numeric = first == '-' || isdigit(first);
            assert(is_numeric);
            array<str> v = s.split([&](char ch) -> int {
                bool n = ch == '-' || ch == '.' || isdigit(ch);
                if (is_numeric != n) {
                    is_numeric = !is_numeric;
                    return -1;
                }
                return 0;
            });
            value = v[0].real_value<real>();
            if (v.length() > 1) {
                type = v[1];
            }
        }
    }
};

struct node;

struct style:mx {
    /// qualifier for style block
    struct Qualifier:mx {
        struct M {
            type_t    ty; /// its useful to look this up (which we cant by string)
            str       type;
            str       id;
            str       state; /// member to perform operation on (boolean, if not specified)
            str       oper;  /// if specified, use non-boolean operator
            str       value;
            mx        parent; /// top level is the > one:on-the-end { and parents are-here > not-there {
            operator bool() {
                return type || id || state;
            }
            register(M)
        };
        mx_basic(Qualifier)
    };

    ///
    struct transition {
        enums(ease, linear,
             linear, quad, cubic, quart, quint, sine, expo, circ, back, elastic, bounce);
        ///
        enums(direction, in,
             in, out, in_out);

        ease easing;
        direction dir;
        unit<duration> dur;

        transition(null_t n = null) { }

        static inline const real PI = 3.1415926535897932384; // M_PI;
        static inline const real c1 = 1.70158;
        static inline const real c2 = c1 * 1.525;
        static inline const real c3 = c1 + 1;
        static inline const real c4 = (2 * PI) / 3;
        static inline const real c5 = (2 * PI) / 4.5;

        static real ease_linear        (real x) { return x; }
        static real ease_in_quad       (real x) { return x * x; }
        static real ease_out_quad      (real x) { return 1 - (1 - x) * (1 - x); }
        static real ease_in_out_quad   (real x) { return x < 0.5 ? 2 * x * x : 1 - std::pow(-2 * x + 2, 2) / 2; }
        static real ease_in_cubic      (real x) { return x * x * x; }
        static real ease_out_cubic     (real x) { return 1 - std::pow(1 - x, 3); }
        static real ease_in_out_cubic  (real x) { return x < 0.5 ? 4 * x * x * x : 1 - std::pow(-2 * x + 2, 3) / 2; }
        static real ease_in_quart      (real x) { return x * x * x * x; }
        static real ease_out_quart     (real x) { return 1 - std::pow(1 - x, 4); }
        static real ease_in_out_quart  (real x) { return x < 0.5 ? 8 * x * x * x * x : 1 - std::pow(-2 * x + 2, 4) / 2; }
        static real ease_in_quint      (real x) { return x * x * x * x * x; }
        static real ease_out_quint     (real x) { return 1 - std::pow(1 - x, 5); }
        static real ease_in_out_quint  (real x) { return x < 0.5 ? 16 * x * x * x * x * x : 1 - std::pow(-2 * x + 2, 5) / 2; }
        static real ease_in_sine       (real x) { return 1 - cos((x * PI) / 2); }
        static real ease_out_sine      (real x) { return sin((x * PI) / 2); }
        static real ease_in_out_sine   (real x) { return -(cos(PI * x) - 1) / 2; }
        static real ease_in_expo       (real x) { return x == 0 ? 0 : std::pow(2, 10 * x - 10); }
        static real ease_out_expo      (real x) { return x == 1 ? 1 : 1 - std::pow(2, -10 * x); }
        static real ease_in_out_expo   (real x) {
            return x == 0
                ? 0
                : x == 1
                ? 1
                : x < 0.5
                ? std::pow(2, 20 * x - 10) / 2
                : (2 - std::pow(2, -20 * x + 10)) / 2;
        }
        static real ease_in_circ       (real x) { return 1 - sqrt(1 - std::pow(x, 2)); }
        static real ease_out_circ      (real x) { return sqrt(1 - std::pow(x - 1, 2)); }
        static real ease_in_out_circ   (real x) {
            return x < 0.5
                ? (1 - sqrt(1 - std::pow(2 * x, 2))) / 2
                : (sqrt(1 - std::pow(-2 * x + 2, 2)) + 1) / 2;
        }
        static real ease_in_back       (real x) { return c3 * x * x * x - c1 * x * x; }
        static real ease_out_back      (real x) { return 1 + c3 * std::pow(x - 1, 3) + c1 * std::pow(x - 1, 2); }
        static real ease_in_out_back   (real x) {
            return x < 0.5
                ? (std::pow(2 * x, 2) * ((c2 + 1) * 2 * x - c2)) / 2
                : (std::pow(2 * x - 2, 2) * ((c2 + 1) * (x * 2 - 2) + c2) + 2) / 2;
        }
        static real ease_in_elastic    (real x) {
            return x == 0
                ? 0
                : x == 1
                ? 1
                : -std::pow(2, 10 * x - 10) * sin((x * 10 - 10.75) * c4);
        }
        static real ease_out_elastic   (real x) {
            return x == 0
                ? 0
                : x == 1
                ? 1
                : std::pow(2, -10 * x) * sin((x * 10 - 0.75) * c4) + 1;
        }
        static real ease_in_out_elastic(real x) {
            return x == 0
                ? 0
                : x == 1
                ? 1
                : x < 0.5
                ? -(std::pow(2, 20 * x - 10) * sin((20 * x - 11.125) * c5)) / 2
                : (std::pow(2, -20 * x + 10) * sin((20 * x - 11.125) * c5)) / 2 + 1;
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

        /// functions are courtesy of easings.net; just organized them into 2 enumerables compatible with web
        real pos(real tf) const {
            real x = math::clamp(tf, 0.0, 1.0);
            switch (easing.value) {
                case ease::linear:
                    switch (dir.value) {
                        case direction::in:      return ease_linear(x);
                        case direction::out:     return ease_linear(x);
                        case direction::in_out:  return ease_linear(x);
                    }
                    break;
                case ease::quad:
                    switch (dir.value) {
                        case direction::in:      return ease_in_quad(x);
                        case direction::out:     return ease_out_quad(x);
                        case direction::in_out:  return ease_in_out_quad(x);
                    }
                    break;
                case ease::cubic:
                    switch (dir.value) {
                        case direction::in:      return ease_in_cubic(x);
                        case direction::out:     return ease_out_cubic(x);
                        case direction::in_out:  return ease_in_out_cubic(x);
                    }
                    break;
                case ease::quart:
                    switch (dir.value) {
                        case direction::in:      return ease_in_quart(x);
                        case direction::out:     return ease_out_quart(x);
                        case direction::in_out:  return ease_in_out_quart(x);
                    }
                    break;
                case ease::quint:
                    switch (dir.value) {
                        case direction::in:      return ease_in_quint(x);
                        case direction::out:     return ease_out_quint(x);
                        case direction::in_out:  return ease_in_out_quint(x);
                    }
                    break;
                case ease::sine:
                    switch (dir.value) {
                        case direction::in:      return ease_in_sine(x);
                        case direction::out:     return ease_out_sine(x);
                        case direction::in_out:  return ease_in_out_sine(x);
                    }
                    break;
                case ease::expo:
                    switch (dir.value) {
                        case direction::in:      return ease_in_expo(x);
                        case direction::out:     return ease_out_expo(x);
                        case direction::in_out:  return ease_in_out_expo(x);
                    }
                    break;
                case ease::circ:
                    switch (dir.value) {
                        case direction::in:      return ease_in_circ(x);
                        case direction::out:     return ease_out_circ(x);
                        case direction::in_out:  return ease_in_out_circ(x);
                    }
                    break;
                case ease::back:
                    switch (dir.value) {
                        case direction::in:      return ease_in_back(x);
                        case direction::out:     return ease_out_back(x);
                        case direction::in_out:  return ease_in_out_back(x);
                    }
                    break;
                case ease::elastic:
                    switch (dir.value) {
                        case direction::in:      return ease_in_elastic(x);
                        case direction::out:     return ease_out_elastic(x);
                        case direction::in_out:  return ease_in_out_elastic(x);
                    }
                    break;
                case ease::bounce:
                    switch (dir.value) {
                        case direction::in:      return ease_in_bounce(x);
                        case direction::out:     return ease_out_bounce(x);
                        case direction::in_out:  return ease_in_out_bounce(x);
                    }
                    break;
            };
            return x;
        }

        template <typename T>
        T operator()(T &fr, T &to, real value) const {
            if constexpr (has_mix<T>()) {
                real trans = pos(value);
                return fr.mix(to, trans);
            } else
                return to;
        }

        transition(str s) : transition() {
            if (s) {
                array<str> sp = s.split();
                size_t    len = sp.length();
                /// syntax:
                /// 500ms [ease [out]]
                /// 0.2s -- will be linear with in (argument meaningless for linear but applies to all others)
                dur    = unit<duration>(sp[0]);
                easing = sp.len() > 1 ? ease(sp[1])      : ease();
                dir    = sp.len() > 2 ? direction(sp[2]) : direction();
            }
        }
        ///
        operator  bool() { return dur.value >  0; }
        bool operator!() { return dur.value <= 0; }

        type_register(transition);
    };

    struct block;

    /// Element2 or style block entry
    struct entry {
        mx              member;
        str             value;
        transition      trans; 
        block          *bl; /// block location would be nice to have; you compute a list of entries and props once and apply on update
        mx             *mx_instance;  ///
        void           *raw_instance; /// we run type->from_string(null, value.cs()), on style load.  we need not do this every time
    };

    ///
    struct block {
        block*             parent; /// pattern: reduce type rather than create pointer to same type in delegation
        doubly<Qualifier>  quals;  /// an array of qualifiers it > could > be > this:state, or > just > that [it picks the best score, moves up for a given node to match style in]
        map<entry*>        entries;
        doubly<block*>     blocks;
        array<type_t>      types; // if !types then its all types.

        size_t score(node *n, bool score_state);
        
        ///
        inline operator bool() { return quals || entries || blocks; }
    };

    using style_map = map<array<entry*>>;

    struct impl {
        mutex               mtx;
        array<block*>       root;
        map<array<block*>>  members;
        watch               reloader;
        bool                reloaded;
        bool                loaded; /// composer user will need to lock to use; clear root and members for
        style_map        compute(node *dst);
        entry        *best_match(node *n, prop *member, array<entry*> &entries);
        bool          applicable(node *n, prop *member, array<entry*> &all);
        void                load(str code);

        /// optimize member access by caching by member name, and type
        void cache_members();

        type_register(impl);
    };

    /// load all style sheets in resources
    static style init();
    mx_object(style, mx, impl);
};

/// no reason to have style separated in a single app
/// if we have multiple styles, just reload
template <> struct is_singleton<style> : true_type { };


enums(keyboard, none,
     none, caps_lock, shift, ctrl, alt, meta);

enums(mouse, none,
     none, left, right, middle, inactive);

namespace user {
    using chr = num;

    struct key:mx {
        struct kdata {
            num                 unicode;
            num                 scan_code;
            states<keyboard>    modifiers;
            bool                repeat;
            bool                up;
            type_register(kdata);
        };
        mx_object(key, mx, kdata);
    };
};

struct event:mx {
    ///
    struct edata {
        node*                   target;
        user::chr               unicode;
        user::key               key;
        str                     text;
        vec2d                   wheel_delta;
        vec2d                   cursor;
        states<mouse>           buttons;
        states<keyboard>        modifiers;
        size                    resize;
        mouse::etype            button_id;
        bool                    prevent_default;
        bool                    stop_propagation;
        type_register(edata);
    };

    mx_object(event, mx, edata);

    event(node* target) : event() {
        data->target = target;
    }
    
    event(const mx &a) = delete;
    ///
    inline void prevent_default()   {         data->prevent_default = true; }
    inline bool is_default()        { return !data->prevent_default; }
    inline bool should_propagate()  { return !data->stop_propagation; }
    inline bool stop_propagation()  { return  data->stop_propagation = true; }
    inline vec2d cursor_pos()       { return  data->cursor; }
    inline bool mouse_down(mouse m) { return  data->buttons[m]; }
    inline bool mouse_up  (mouse m) { return !data->buttons[m]; }
    inline num  unicode   ()        { return  data->key->unicode; }
    inline num  scan_code ()        { return  data->key->scan_code; }
    inline bool key_down  (num u)   { return  data->key->unicode   == u && !data->key->up; }
    inline bool key_up    (num u)   { return  data->key->unicode   == u &&  data->key->up; }
    inline bool scan_down (num s)   { return  data->key->scan_code == s && !data->key->up; }
    inline bool scan_up   (num s)   { return  data->key->scan_code == s &&  data->key->up; }
};

using fn_render = lambda<node()>;
using fn_stub   = lambda<void()>;
using callback  = lambda<void(event)>;

struct dispatch;

struct listener:mx {
    struct ldata {
        dispatch *src;
        callback  cb;
        fn_stub   detatch;
        bool      detached;
        ///
        ~ldata() { printf("listener, ...destroyed\n"); }
        type_register(ldata);
    };
    
    ///
    mx_object(listener, mx, ldata);

    void cancel() {
        data->detatch();
        data->detached = true;
    }

    ///
    ~listener() {
        if (!data->detached && mem->refs == 1) /// only reference is the binding reference, detect and make that ref:0, ~data() should be fired when its successfully removed from the list
            cancel();
    }
};

/// keep it simple for sending events, with a dispatch.  listen to them with listen()
/// need to implement an assign with a singular callback.  these can support more than one, though.  in a component tree you can use multiple with use of context
struct dispatch:mx {
    struct ddata {
        doubly<listener> listeners;
        type_register(ddata);
    };
    ///
    mx_object(dispatch, mx, ddata);
    ///
    void operator()(event e);
    ///
    listener &listen(callback cb);
};

struct node;

struct composer:mx {
    ///
    struct cmdata {
        memory* app;
        //node* root_instance;
        node* instances;

        struct vk_interface *vk;
        //fn_render     render;
        lambda<node()> render;
        map<mx>       args;
        ion::style    style;
        bool          shift;
        bool          alt;
        int           buttons[16];
        type_register(cmdata);
    };
    
    /// called from app
    void update_all(node e);

    /// called recursively from update_all -> update(), also called from node generic render (node will need a composer data member)
    static void update(cmdata *composer, node *parent, node *&instance, node &e);

    ///
    mx_object(composer, mx, cmdata);
};

struct node:mx {
    /// style implemented at node data and in composer
    /// this is so one can style their services
    struct selection {
        prop         *member;
        raw_t         from, to; /// these we must call new() and del()
        i64           start, end;
        style::entry *entry;
        /// the memory being set is the actual prop, but we need an origin stored in form of raw_t or a doubl
    };
    ///
    struct edata {
        type_t                  type;       /// type given
        str                     id;         /// identifier 
        ax                      args;       /// arguments
        str                     group;      /// group used for button behaviors
        mx                      value;
        array<str>              tags;       /// style tags
        array<node*>            children;   /// children elements (if provided in children { } pair<mx,mx> inherited data struct; sets key='children')
        node*                   instance;   /// node instance is 1:1 (allocated, then context is copied in place)
        map<node*>              mounts;     /// store instances of nodes in element data, so the cache management can go here where element turns to node
        node*                   parent;
        style::style_map        style_avail; /// properties outside of meta are effectively internal state only; it can be labelled as such
        map<selection>          selections; /// style selected per member
        ion::composer::cmdata*  composer;
        lambda<void(mx)>        ref;

        ///
        doubly<prop> meta() {
            return {
                prop { "id",        id        },
                prop { "children",  children  },
                prop { "group",     group     },
                prop { "value",     value     },
                prop { "tags",      tags      },
                prop { "ref",       ref       }
            };
        }

        ///
        operator bool() {
            return (type || children.len() > 0);
        }
        
        ///
        type_register(edata);
    };

    template <typename T>
    T *grab() {
        node *n = data->parent;
        while (n) {
            if (n->mem->type == typeof(T)) {
                return (T*)n;
            }
            n = n->data->parent;
        }
        return null;
    }

    /// collect from a group -- lets just use a single depth here
    array<node*> collect(str group_name, bool include_this) {
        node *dont_inc = include_this ? this : null;
        array<node*> res;
        console.log("looking for group called {0}", {group_name});
        if (data->parent) {
            for (field<node*> &f: data->parent->data->mounts) {
                node *c = f.value;
                console.log("found element {0}", {c->data->id});
                if (c->data->group == group_name && c != dont_inc)
                    res += c;
            }
        }
        return res;
    }

    /// context works by looking at meta() on the context types polymorphically, and up the parent nodes to the root context (your app)
    template <typename T>
    T *context(str id);

    bool operator!() {
        return !data || !bool(*data);
    }

    operator bool() {
        return data && *data;
    }

    /// test this path in composer
    virtual node update() {
        return *this;
    }

    virtual void mounted() { }

    node *root() const {
        node  *pe = (node*)this;
        while (pe) {
            if (!pe->data->parent) return pe;
            pe = pe->data->parent;
        }
        return null;
    }

    mx_object(node, mx, edata);

    static memory *args_id(type_t type, initial<arg> args) {
        static memory *m_id = memory::symbol("id"); /// im a token!
        for (auto &a:args)
            if (a.key.mem == m_id)
                return a.value.mem;
        
        return memory::symbol(symbol(type->name));
    }

    node(str id, array<str> tags, array<node> ch):node(ch) {
        node::data->id = id;
        node::data->tags = tags;
    }

    node(str id, array<node> ch):node(ch) {
        node::data->id = id;
    }

    node(array<node*> ch) : node() {
        data->children = ch; /// data must be set here
    }

    node(array<node> ch) : node() {
        data->children = array<node*>(size(ch.len()), size(0));
        for (auto &child:ch) {
            data->children += new node(child);
        }
    }

    node(type_t type, initial<arg> args) : node() {
        data->type = type;
        data->id   = args_id(type, args);
        data->args = args;
    }

    /// used below in each(); a simple allocation of Elements
    node(type_t type, size_t sz) : node() { 
        data->type = type;
        data->id   = null;
        data->children = array<node*>(sz, size_t(0));
    }

    template <typename T>
    static node each(array<T> a, lambda<node(T &v)> fn) {
        node res(typeof(array<node>), a.length());
        for (auto &v:a) {
            node ve = fn(v);
            if  (ve) res.data->children += new node(ve);
        }
        return res;
    }
    
    template <typename K, typename V>
    static node each(map<V> m, lambda<node(K &k, V &v)> fn) {
        node res(typeof(map<node>), m.len());
        for (field<V> f:m) {
            K key = f.key.hold();
            node r = fn(key, f.value);
            if  (r)
                res.data->children += new node(r);
        }
        return res;
    }

    inline size_t count(memory *symbol) {
        for (node *c: node::data->children)
            if (c->data->id == symbol)
                return 1;
        ///
        return 0;
    }

    node *select_first(lambda<node*(node*)> fn) {
        lambda<node*(node*)> recur;
        recur = [&](node* n) -> node* {
            node* r = fn(n);
            if   (r) return r;
            for (field<node*> &f: n->node::data->mounts) {
                if (!f.value) continue;
                node* r = recur(f.value);
                if   (r) return r;
            }
            return null;
        };
        return recur(this);
    }

    /// the element can create its instance.. that instance is a sub-class of Element2 too so we use a forward
    struct node *new_instance() {
        return (struct node*)data->type->functions->alloc_new((struct node*)null, (struct node*)null);
    }
};


/// defines a component with a restricted subset and props initializer_list.
/// this macro is used for template purposes and bypasses memory allocation.
/// the actual instances of the component are managed by the composer.
/// -------------------------------------------
#define component(C, B, D) \
    using parent_class   = B;\
    using context_class  = C;\
    using intern         = D;\
    static const inline type_t ctx_t  = typeof(C);\
    static const inline type_t data_t = typeof(D);\
    intern* state;\
    C(memory*         mem) : B(mem), state(mx::data<D>()) { }\
    C(type_t ty, initial<arg>  props) : B(ty,        props), state(defaults<intern>()) { }\
    C(initial<arg>  props) :            B(typeof(C), props), state(defaults<intern>()) { }\
    C(nullptr_t) : C() { }\
    C(mx                o) : C(o.mem->hold())  { }\
    C()                    : C(mx::alloc<C>()) { }\
    intern    *operator->() { return  state; }\
    type_register(C);

//typedef node* (*FnFactory)();
using FnFactory = node*(*)();

template <typename T>
T *node::context(str id) {
    node *n = (node*)this;
    prop* member = null;
    /// fetch context through self/parents in tree
    while (n) {
        u8*   addr   = property_find(n->mem->origin, n->mem->type, id, member);
        if (addr) {
            assert(member->type == typeof(T));
            return (T*)addr;
        }
        n = n->data->parent;
    }
    /// get context from app composer lastly (use-case: video sink service)
    memory *app  = data->composer->app;
    u8     *addr = property_find(app->origin, app->type, id, member);
    ///
    if (addr) {
        assert(member->type == typeof(T));
        return (T*)addr;
    }

    return null;
}

}