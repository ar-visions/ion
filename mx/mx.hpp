#pragma once

#pragma warning(disable:4005) /// skia
#pragma warning(disable:4566) /// escape sequences in canvas
#pragma warning(disable:5050) ///
#pragma warning(disable:4244) /// skia-header warnings
#pragma warning(disable:5244) /// odd bug
#pragma warning(disable:4291) /// 'no exceptions'
#pragma warning(disable:4996) /// strncpy warning
#pragma warning(disable:4267) /// size_t to int warnings (minimp3)

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sched.h>
#endif

/// wall of shame
#include <functional>
#include <atomic>
#include <filesystem>
//#include <condition_variable>
//#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <memory>
#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <sstream>
#include <initializer_list>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include <new>
#include <array>
#include <set>
#include <stack>
#include <queue>
#include <utility>
#include <type_traits>
#include <string.h>
#include <stdarg.h>
#ifndef _WIN32
#   include <unistd.h>
#endif

#include <assert.h>
#include <io/io.h>

namespace ion {

struct idata;
struct mx;
struct memory;

using raw_t = void*;
using type_t = idata *;

/// need to forward these
memory*       grab(memory *mem);
memory*       drop(memory *mem);
memory* mem_symbol(symbol cs, type_t ty, i64 id);
void*   mem_origin(memory *mem);
memory*    cstring(cstr cs, size_t len, size_t reserve, bool is_constant);

template <typename T> T *mdata(memory *mem, size_t index);

/*
/// i8 -> 64
using i8  = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = long long;

/// u8 -> 64
using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

/// basic types
using ulong    = unsigned long;
using r32      = float;
using r64      = double;
using real     = r64;
using cstr     = char*;
using symbol   = const char*;
using cchar_t  = const char;
//using ssize_t = i64;
using num      = i64;
using handle_t = void*;
using raw_t    = void*;
 */

using        null_t      = std::nullptr_t;
static const null_t null = nullptr;

template <typename T>
constexpr bool is_mx();

int snprintf(char *str, size_t size, const char *format, ...);

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

template <typename T> const T nan() { return std::numeric_limits<T>::quiet_NaN(); };

constexpr bool  is_win() {
#if defined(_WIN32) || defined(_WIN64)
	return true;
#else
	return false;
#endif
};

constexpr bool  is_android() {
#if defined(__ANDROID_API__)
	return true;
#else
	return false;
#endif
};

constexpr bool  is_apple() {
#if defined(__APPLE__)
	return true;
#else
	return false;
#endif
};

constexpr int num_occurances(const char* cs, char c) {
    return cs[0] ? (cs[0] == c) + num_occurances(cs + 1, c) : 0; 
}

#define num_args(...) (num_occurances(#__VA_ARGS__, ',') + 1)

#define enums(C,D,S,...)\
    struct C:ex {\
        inline static bool init;\
        enum etype { __VA_ARGS__ };\
        enum etype&    value;\
        static memory* lookup(symbol sym) { return typeof(C)->lookup(sym); }\
        static memory* lookup(u64    id)  { return typeof(C)->lookup(id);  }\
        inline static const int count = num_args(__VA_ARGS__);\
        static void initialize() {\
            init            = true;\
            array<str>   sp = str(S).split(", ");\
            size_t        c = sp.len();\
            type_t       ty = typeof(C);\
            for (size_t i  = 0; i < c; i++)\
                mem_symbol(sp[i].data, ty, i64(i));\
        }\
        ::symbol symbol() {\
            memory *mem = typeof(C)->lookup(u64(value));\
            assert(mem);\
            return (char*)mem->origin;\
        }\
        str name() { return (char*)symbol(); }\
        static enum etype convert(mx raw) {\
            if (!init) initialize();\
            type_t   type = typeof(C);\
            memory **psym = null;\
            if (raw.type() == typeof(char)) {\
                char  *d = &raw.ref<char>();\
                u64 hash = djb2(d);\
                psym     = type->symbols->djb2.lookup(hash);\
            } else if (raw.type() == typeof(int)) {\
                i64   id = i64(raw.ref<int>());\
                psym     = type->symbols->ids.lookup(id);\
            } else if (raw.type() == typeof(i64)) {\
                i64   id = raw.ref<i64>();\
                psym     = type->symbols->ids.lookup(id);\
            } else if (raw.type() == typeof(etype)) {\
                i64   id = raw.ref<etype>();\
                psym     = type->symbols->ids.lookup(id);\
            }\
            if (!psym) throw C();\
            return (enum etype)((*psym)->id);\
        }\
        C(enum etype t =        etype::D) :ex(t, this), value(ref<enum etype>()) { if (!init) initialize(); }\
        C(str raw):C(C::convert(raw)) { }\
        C(mx  raw):C(C::convert(raw)) { }\
        inline  operator etype() { return value; }\
        static doubly<memory*> &symbols() {\
            if (!init) initialize();\
            return typeof(C)->symbols->list;\
        }\
        C&      operator=  (const C b)  { return (C&)assign_mx(*this, b); }\
        bool    operator== (enum etype v) { return value == v; }\
        bool    operator!= (enum etype v) { return value != v; }\
        bool    operator>  (C &b)       { return value >  b.value; }\
        bool    operator<  (C &b)       { return value <  b.value; }\
        bool    operator>= (C &b)       { return value >= b.value; }\
        bool    operator<= (C &b)       { return value <= b.value; }\
        explicit operator int()         { return int(value); }\
        explicit operator u64()         { return u64(value); }\
    };\

#define infinite_loop() { for (;;) { usleep(1000); } }

#define typeof(T)   ident::for_type<T>()
#define typesize(T) ident::type_size<T>()

#ifdef __cplusplus
#    define decl(x)   decltype(x)
#else
#    define decl(x) __typeof__(x)
#endif

static void breakp() {
    #ifdef _MSC_VER
    #ifndef NDEBUG
    DebugBreak();
    #endif
    #endif
}

#define type_assert(CODE_ASSERT, TYPE_ASSERT) \
    do {\
        const auto   v = CODE_ASSERT;\
        const type_t t = typeof(decl(v));\
        static_assert(decl(v) != typeof(TYPE));\
        if (t != typeof(TYPE)) {\
            faultf("(type-assert) %s:%d: :: %s\n", __FILE__, __LINE__, xstr_0(TYPE_ASSERT));\
        }\
        if (!bool(v)) {\
            faultf("(assert) %s:%d: :: %s\n", __FILE__, __LINE__, xstr_0(CODE_ASSERT));\
        }\
    } while (0);

#define interops(C) \
    template <typename X>\
    operator X &() {\
        return (X &)mx::data<typename C::intern>();\
    }\

#define register(C)\
    static size_t _type_sz(C *src) {\
        static type_t type    = typeof(C);\
        static size_t type_sz = (type->schema) ? type->schema->total_bytes : type->base_sz;\
        assert(type_sz > 0);\
        return        type_sz;\
    }\
    static bool boolean(C *src) { if constexpr (has_bool_op<C>::value) return bool(*ctx); else return false; }\
    static void    free(C *dst, size_t count) {\
        for (size_t i = 0; i < count; i++) {\
                 if constexpr ( has_static_free<C>()) C::free(dst);\
            else if constexpr (      !is_opaque<C>()) dst -> ~C();\
        }\
    }\
    static void assign(C *dst, C *src) {\
        if constexpr (!is_opaque<C>()) {\
            dst -> ~C();\
            new (dst)(*src);\
        }\
    }\
    static void copy(C *dst, C *src, size_t count) { \
        if constexpr (std::is_assignable_v<C&, const C&>) for (size_t i = 0; i < count; i++) dst[i] = src[i];\
        else assert(false);\
    }\
    static void construct(C *dst) {\
        if constexpr (      !is_opaque<C>()) new (dst) C();\
        if constexpr ( has_static_init<C>()) C::init(dst);\
    }\

template<typename T, typename = void>
struct has_intern : std::false_type { };

template<typename T>
struct has_intern<T, std::void_t<typename T::intern>> : std::true_type { };

#define ptr_declare(C, B, D) \
    using parent_class  = B;\
    using context_class = C;\
    using intern        = D;\
    C(intern&  data);\
    C(intern&& data);\
    C(memory*  mem);\
    C(intern*  data);\
    C(mx o);\
    C();\
    intern *data;\
    intern &operator *();\
    intern *operator->();\
    intern *operator &();\
            operator intern *();\
            operator intern &();\
    C      &operator=(const C b);\
    intern *operator=(const intern *b);

#define ptr_implement(C, B) \
    C::C(memory*   mem) : B(mem), data(mdata<intern>(mem, 0)) { }\
    C::C(mx          o) : C(o.mem->grab()) { }\
    C::C()              : C(mx::alloc<C>()) { }\
    C::C(intern  *data) : C(mx::wrap<intern>(data, 1)) { }\
    C::C(intern  &data) : C(mx::alloc<C>(&data)) { }\
    C::C(intern &&data) : C(mx::alloc<C>(&data)) { }\
    C::intern  &C::operator *() { return *data; }\
    C::intern  *C::operator->() { return  data; }\
    C::intern  *C::operator &() { return  data; }\
    C::operator C::intern   *() { return  data; }\
    C::operator C::intern   &() { return *data; }\
    C &C::operator=(const C b) { mem->type->functions->assign((void*)this, (void*)&b); return *this; }\
    C::intern *C::operator=(const C::intern *b) {\
        drop();\
        mem = mx::wrap<C>(raw_t(b), 1);\
        data = (C::intern*)mem->origin;\
        return data;\
    }

/// for templates in header
/// the following is delegated to movable(C): (array<T> blows up for good reason)
///     inline C(intern    ref) : B(mx::alloc<C>(&ref)), data(mx::data<D>()) { }
#define ptr(C, B, D) \
    using parent_class   = B;\
    using context_class  = C;\
    using intern         = D;\
    intern*    data;\
    C(memory*   mem) : B(mem), data(mx::data<D>()) { }\
    C(intern** memb) : B(mx::alloc<C>()),     data(mx::data<D>()) { assert(memb == &data); }\
    C(intern*  data) : C(mx::wrap <C>(raw_t(data), 1)) { }\
    C(mx          o) : C(o.mem->grab()) { }\
    C()              : C(mx::alloc<C>()) { }\
    intern    &operator *() { return *data; }\
    intern    *operator->() { return  data; }\
    intern    *operator &() { return  data; }\
    operator     intern *() { return  data; }\
    operator     intern &() { return *data; }\
    C      &operator=(const C b) { mx::mem->type->functions->assign((void*)this, (void*)&b); return *this; }\
    intern *operator=(const intern *b) {\
        if (data != b) {\
            mx::drop();\
            mx::mem = mx::wrap<C>(raw_t(b), 1);\
            data = (intern*)mx::mem->origin;\
        }\
        return data;\
    }

#define movable(C)\
    inline C(intern data) : C(mx::alloc<C>((raw_t)&data, 1)) { }\

template <typename DC>
static constexpr void inplace(DC *p, bool dtr) {
    if (dtr) (p) -> ~DC(); 
    new (p) DC();
}

template <typename DC>
static constexpr void inplace(DC *p, DC &data, bool dtr) {
    if (dtr) (p) -> ~DC(); 
    new (p) DC(data);
}

template <typename DC, typename ARG>
static constexpr void inplace(DC *p, ARG *arg, bool dtr) {
    if (dtr) (p) -> ~DC(); 
    new (p) DC(arg);
}

template <class T, class Enable = void>
struct is_defined {
    static constexpr bool value = false;
};

template <class T>
struct is_defined<T, std::enable_if_t<(sizeof(T) > 0)>> {
    static constexpr bool value = true;
};

template <typename T> constexpr bool is_realistic() { return std::is_floating_point<T>(); }
template <typename T> constexpr bool is_integral()  { return std::is_integral<T>(); }
template <typename T> constexpr bool is_numeric()   {
    return std::is_integral<T>()       ||
           std::is_floating_point<T>();
}

template <typename A, typename B>
constexpr bool inherits() { return std::is_base_of<A, B>(); }

template <typename A, typename B> constexpr bool identical()       { return std::is_same<A, B>(); }
template <typename T>             constexpr bool is_primitive()    { return  identical<T, char*>() || identical<T, const char*>() || is_numeric<T>() || std::is_enum<T>(); }
template <typename T>             constexpr bool is_class()        { return !is_primitive<T>() && std::is_default_constructible<T>(); }
template <typename T>             constexpr bool is_destructible() { return  is_class<T>()     && std::is_destructible<T>(); }
template <typename A, typename B> constexpr bool is_convertible()  { return std::is_same<A, B>() || std::is_convertible<A, B>::value; }

template <typename T, typename = void>
struct has_etype : std::false_type {
    constexpr operator bool() const { return value; }
};

template <typename T>
struct has_etype<T, std::void_t<typename T::etype>> : std::true_type {
    constexpr operator bool() const { return value; }
};

template<typename...>
using void_t = void;

template <typename T, typename = void>
struct has_multiply_double : std::false_type {};

template <typename T>
struct has_multiply_double<T, void_t<decltype(std::declval<T>() * std::declval<double>())>> : std::true_type {};

template <typename T, typename = void>
struct has_addition : std::false_type {};

template <typename T>
struct has_addition<T, void_t<decltype(std::declval<T>() + std::declval<T>())>> : std::true_type {};

template <typename T>
constexpr bool transitionable() {
    return has_multiply_double<T>::value && has_addition<T>::value;
}

static inline void yield() {
#ifdef _WIN32
    SwitchToThread(); // turns out you can. lol.
#else
    sched_yield();
#endif
}

static inline void sleep(u64 u) {
    #ifdef _WIN32
        Sleep(u);
    #else
        usleep(useconds_t(u));
    #endif
}

struct traits {
    enum bit {
        primitive = 1,
        integral  = 2,
        realistic = 4,
        singleton = 8,
        mx        = 16,
        opaque    = 32 /// may simply check for sizeof() in completeness check, unless we want complete types as opaque too which i dont see point of
    };
};

///
template <typename T> using initial = std::initializer_list<T>;
template <typename T> using func    = std::function<T>;


//template <typename T> using lambda  = std::function<T>;

template <typename K, typename V> using umap = std::unordered_map<K,V>;
namespace fs  = std::filesystem;

template <typename T, typename B>
                      T mix(T a, T b, B f) { return a * (B(1.0) - f) + b * f; }
template <typename T> T radians(T degrees) { return degrees * static_cast<T>(0.01745329251994329576923690768489); }
template <typename T> T degrees(T radians) { return radians * static_cast<T>(57.295779513082320876798154814105); }

template <typename T>
struct item {
    struct item *next;
    struct item *prev;
    T            data;

    /// return the wrapped item memory, just 3 doors down.
    static item<T> *container(T& i) {
        return (item<T> *)(
            (symbol)&i - sizeof(struct item*) * 2
        );
    }
};

/// iterator unique for doubly
template <typename T>
struct liter {
    item<T>* cur;
    ///
    liter& operator++  () { cur = cur->next; return *this;          }
    liter& operator--  () { cur = cur->prev; return *this;          }

    T& operator *  ()                  const { return cur->data;     }
       operator T& ()                  const { return cur->data;     }

    inline bool operator==  (liter& b) const { return cur == b.cur; }
    inline bool operator!=  (liter& b) const { return cur != b.cur; }
};

struct true_type {
    static constexpr bool value = true;
    constexpr operator bool() const noexcept { return value; }
};

struct false_type {
    static constexpr bool value = false;
    constexpr operator bool() const noexcept { return value; }
};

template <typename> struct is_opaque    : false_type { };
template <typename> struct is_singleton : false_type { };

template <> struct is_singleton<std::nullptr_t> : true_type { };

template <typename A, typename B, typename = void>
struct has_operator : false_type { };
template <typename A, typename B>
struct has_operator <A, B, decltype((void)(void (A::*)(B))& A::operator())> : true_type { };

template<typename, typename = void> constexpr bool type_complete = false;
template<typename T>                constexpr bool type_complete <T, std::void_t<decltype(sizeof(T))>> = true;

template <typename T, typename = void>
struct has_convert : std::false_type {};
template <typename T>
struct has_convert<T, std::void_t<decltype(std::declval<T>().convert((memory*)nullptr))>> : std::true_type {};

template <typename T, typename = void>
struct has_compare : std::false_type {};
template <typename T>
struct has_compare<T, std::void_t<decltype(std::declval<T>().compare((T &)*(T*)nullptr))>> : std::true_type {};


template <typename T> struct is_array : std::false_type {};
template <typename T> struct is_map   : std::false_type {};

void chdir(std::string c);

struct ident;

template <typename T, const size_t SZ>
struct buffer {
    T      values[SZ];
    size_t count;
};

struct size:buffer<num, 16> {
    using base = buffer<num, 16>;

    inline size(num        sz = 0) { memset(values, 0, sizeof(values)); values[0] = sz; count = 1; }
    inline size(null_t           ) : size(num(0))  { }
    inline size(size_t         sz) : size(num(sz)) { }
    inline size(i8             sz) : size(num(sz)) { }
    inline size(u8             sz) : size(num(sz)) { }
    inline size(i16            sz) : size(num(sz)) { }
    inline size(u16            sz) : size(num(sz)) { }
    inline size(i32            sz) : size(num(sz)) { }
    inline size(u32            sz) : size(num(sz)) { }
    inline size(initial<num> args) : base() {
        size_t i = 0;
        for (auto &v: args)
            values[i++] = v;
        count = args.size();
    }

    size_t    x() const { return values[0];    }
    size_t    y() const { return values[1];    }
    size_t    z() const { return values[2];    }
    size_t    w() const { return values[3];    }
    num    area() const {
        num v = (count < 1) ? 0 : 1;
        for (size_t i = 0; i < count; i++)
            v *= values[i];
        return v;
    }
    size_t dims() const { return count; }

    void assert_index(const size &b) const {
        assert(count == b.count);
        for (size_t i = 0; i < count; i++)
            assert(b.values[i] >= 0 && values[i] > b.values[i]);
    }

    bool operator==(size_t b) const { return  area() ==  b; }
    bool operator!=(size_t b) const { return  area() !=  b; }
    bool operator==(size &sb) const { return count == sb.count && memcmp(values, sb.values, count * sizeof(num)) == 0; }
    bool operator!=(size &sb) const { return !operator==(sb); }
    
    void   operator++(int)          { values[count - 1] ++; }
    void   operator--(int)          { values[count - 1] --; }
    size  &operator+=(size_t sz)    { values[count - 1] += sz; return *this; }
    size  &operator-=(size_t sz)    { values[count - 1] -= sz; return *this; }

    num &operator[](size_t index) const {
        return (num&)values[index];
    }

    size &zero() { memset(values, 0, sizeof(values)); return *this; }

    template <typename T>
    size_t operator()(T *addr, const size &index) {
        size_t i = index_value(index);
        return addr[i];
    }
    
             operator num() const { return     area();  }
    explicit operator  i8() const { return  i8(area()); }
    explicit operator  u8() const { return  u8(area()); }
    explicit operator i16() const { return i16(area()); }
    explicit operator u16() const { return u16(area()); }
    explicit operator i32() const { return i32(area()); }
    explicit operator u32() const { return u32(area()); }
  //explicit operator i64() const { return i64(area()); }
    explicit operator u64() const { return u64(area()); }
    explicit operator r32() const { return r32(area()); }
    explicit operator r64() const { return r64(area()); }

    inline num &operator[](num i) { return values[i]; }

    size &operator=(i8   i) { *this = size(i); return *this; }
    size &operator=(u8   i) { *this = size(i); return *this; }
    size &operator=(i16  i) { *this = size(i); return *this; }
    size &operator=(u16  i) { *this = size(i); return *this; }
    size &operator=(i32  i) { *this = size(i); return *this; }
    size &operator=(u32  i) { *this = size(i); return *this; }
    size &operator=(i64  i) { *this = size(i); return *this; }
    size &operator=(u64  i) { *this = size(i64(i)); return *this; }

    size &operator=(const size b);

    size_t index_value(const size &index) const {
        size &shape = (size &)*this;
        assert_index(index);
        num result = 0;
        for (size_t i = 0; i < count; i++) {
            num vdim = index.values[i];
            for (size_t si = i + 1; si < count; si++)
                vdim *= shape.values[si];
            
            result += vdim;
        }
        return result;
    }
};

template <typename T>
memory *talloc(size_t count = 1, size_t reserve = 0);

/// doubly is usable two ways. vector patterns are the same with v2 [struct] vs vector2 [mx-struct]
template <typename T>
struct doubly {
    memory *mem;
    /// memory should have a struct *marker and if you are reading that, you are at origin header
    /// so you allocate type+micro_header which is just the memory* origin; again when it equals the same address space one may read into it
    struct ldata { /// the reduced form of that is just read the pointer
        size_t   icount;
        item<T> *ifirst, *ilast;

        /// life-cycle on items, in data destructor
        ~ldata() {
            item<T>* d = ifirst;
            while   (d) {
                item<T>* dn = d->next;
                delete   d;
                d      = dn;
            }
            icount = 0;
            ifirst = ilast = 0;
        }

        operator  bool() const { return ifirst != null; }
        bool operator!() const { return ifirst == null; }

        /// push by value, return its new instance
        T &push(T v) {
            item<T> *plast = ilast;
            ilast = new item<T> { null, ilast, v };
            ///
            (!ifirst) ? 
            ( ifirst      = ilast) : 
            ( plast->next = ilast);
            ///
            icount++;
            return ilast->data;
        }

        /// push and return default instance
        T &push() {
            item<T> *plast = ilast;
                ilast = new item<T> { null, ilast }; /// default-construction on data
            ///
            (!ifirst) ? 
                (ifirst      = ilast) : 
                (  plast->next = ilast);
            ///
            icount++;
            return ilast->data;
        }

        /// 
        item<T> *get(num index) const {
            item<T> *i;
            if (index < 0) { /// if i is negative, its from the end.
                i = ilast;
                while (++index <  0 && i)
                    i = i->prev;
                assert(index == 0); // (negative-count) length mismatch
            } else { /// otherwise 0 is positive and thats an end of it.
                i = ifirst;
                while (--index >= 0 && i)
                    i = i->next;
                assert(index == -1); // (positive-count) length mismatch
            }
            return i;
        }

        size_t    len() { return icount; }
        size_t length() { return icount; }

        bool remove(item<T> *i) {
            if (i) {
                if (i->next)    i->next->prev = i->prev;
                if (i->prev)    i->prev->next = i->next;
                if (ifirst == i) ifirst   = i->next;
                if (ilast  == i) ilast    = i->prev;
                --icount;
                delete i;
                return true;
            }
            return false;
        }

        bool remove(num index) {
            item<T> *i = get(index);
            return remove(i);
        }
        size_t             count() const { return icount;       }
        T                 &first() const { return ifirst->data; }
        T                  &last() const { return ilast->data;  }
        ///
        void          pop(T *prev = null) { assert(icount); if (prev) *prev = last();  remove(-1);     }
        void        shift(T *prev = null) { assert(icount); if (prev) *prev = first(); remove(int(0)); }
        ///
        T                  pop_v()       { assert(icount); T cp =  last(); remove(-1); return cp; }
        T                shift_v()       { assert(icount); T cp = first(); remove( 0); return cp; }
        T   &operator[]   (num ix) const { assert(ix >= 0 && ix < num(icount)); return get(ix)->data; }
        liter<T>           begin() const { return { ifirst }; }
        liter<T>             end() const { return { null  }; }
        T   &operator+=    (T   v)       { return push  (v); }
        bool operator-=    (num i)       { return remove(i); }
    } *data;

    doubly(memory *mem) : mem(mem), data(mdata<ldata>(mem, 0)) { }
    
    doubly() : doubly(talloc<ldata>()) { }
    doubly(initial<T> a) : doubly() { for (auto &v: a) push(v); }

    operator bool() const { return bool(*data); }
    
    ldata &operator *() { return *data; }
    ldata *operator->() { return  data; }

    inline T   &operator[]   (num ix) const { return (*data)[ix];         }
    inline liter<T>           begin() const { return (*data).begin();     }
	inline liter<T>             end() const { return (*data).end();       }
    inline T   &operator+=    (T   v)       { return (*data) += v;        }
    inline bool operator-=    (num i)       { return (*data) -= i;        }

    inline doubly<T>& operator=(const doubly<T> &b) {
             this -> ~doubly( ); /// destruct this
        new (this)    doubly(b); /// copy construct into this, from b; lets us reset refs
        return *this;
    }

     doubly(const doubly &ref) : doubly(grab(ref.mem)) { }
    ~doubly() { drop(mem); }
};

/// iterator
template <typename T>
struct iter {
    T      *start;
    size_t  index;

    iter       &operator++()       { index++; return *this; }
    iter       &operator--()       { index--; return *this; }
    T&         operator * () const { return start[index];   }
    bool operator==(iter &b) const { return start == b.start && index == b.index; }
    bool operator!=(iter &b) const { return start != b.start || index != b.index; }
};

struct hash { u64 value; hash(u64 v) : value(v) { } operator u64() { return value; } };

template <typename T>
u64 hash_value(T &key);

template <typename T>
u64 hash_index(T &key, size_t mod);

template <typename K, typename V>
struct pair {
    V value;
    K key;
    ///
    inline pair() { }
    inline pair(K k, V v) : key(k), value(v)  { }
};

/// a field can be single param, value only resorting and that reduces code in many cases
/// to remove pair is a better idea if we want to reduce the template arg confusion away
template <typename V, typename K=mx>
struct field:pair<K, V> {
    inline field()         : pair<K,V>()     { }
    inline field(K k, V v) : pair<K,V>(k, v) { }
    operator V &() { return pair<K, V>::value; }
};

/// string with precision of float/double
template <typename T> std::string string_from_real(T a_value, int n = 6) {
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
}

/// a simple hash K -> V impl
template <typename K, typename V>
struct hmap {
  //using hmap   = ion::hmap <K,V>;
    using pair   = ion::pair <K,V>;
    using bucket = typename doubly<pair>::ldata; /// no reason i should have to put typename. theres no conflict on data.
    memory *mem  = null;

    struct hmdata {
        bucket *h_pairs;
        size_t sz;

        /// i'll give you a million quid, or, This Bucket.
        bucket &operator[](u64 k) {
            if (!h_pairs)
                 h_pairs = (bucket*)calloc(sz, sizeof(bucket) * 16);
            return h_pairs[k];
        }
        
        ~hmdata() {
            for (size_t  i = 0; i < sz; i++) h_pairs[i]. ~ bucket(); // data destructs but does not construct and thats safe to do
            free(h_pairs);
        }
    };

    hmdata *data;

    hmap(memory     *mem) : mem(mem), data(mdata<hmdata>(mem, 0)) { }
    hmap(size_t   sz = 0) : hmap(talloc<hmdata>()) { data->sz = sz; }
    hmap(initial<pair> a) : hmap() { for (auto &v: a) push(v); }

    hmap& operator=(hmap a) {
        drop(mem);
        mem = grab(a.mem);
        return *this;
    }

    inline pair* shared_lookup(K key); 
    
    V* lookup(K input, u64 *pk = null, bucket **b = null) const;
    V &operator[](K key);

    inline size_t    len() { return data->sz; }
    inline operator bool() { return data->sz > 0; }
};

#undef min
#undef max

struct math {
    template <typename T>
    static inline T min (T a, T b)                             { return a <= b ? a : b; }
    template <typename T> static inline T max  (T a, T b)      { return a >= b ? a : b; }
    template <typename T> static inline T clamp(T v, T m, T x) { return v >= x ? x : x < m ? m : v; }
    template <typename T> static inline T round(T v)           { return std::round(v);  }
};

/// mx-mock-type
struct ident {
    memory *mem;

    template <typename T>
    static idata* for_type();

    template <typename T>
    static size_t type_size(); /// does not strip the pointer; sz is for its non pointer base sz (will call it that)
    
    ident(memory* mem) : mem(mem) { }
};

memory *mem_symbol(symbol cs, type_t ty = typeof(char), i64 id = 0);

/// now that we have allowed for any, make entry for meta description
struct prop {
    u8     *container; /// address of container.
    memory *key;
    size_t  member_addr;
    size_t  offset; /// this is set by once-per-type meta fetcher

    template <typename MC, typename M>
    prop(MC &container, symbol name, M &member) : container((u8*)&container), key(mem_symbol(name)), member_addr(&member) { }

    template <typename M, typename D>
    M &member_ref(D &m) {
        return *(M *)handle_t(&cstr(&m)[offset]);
    }

    symbol name() const;
};

/// must cache by cstr, and id; ideally support any sort of id range
/// symbols 
using symbol_djb2  = hmap<u64, memory*>;
using symbol_ids   = hmap<i64, memory*>;
using     prop_map = hmap<symbol, prop>;

struct context_bind;

struct alloc_schema {
    doubly<prop>  meta;
    bool          pointer; /// allocate enough space for a pointer to be stored
    prop_map      meta_map;
    size_t        bind_count;  /// bind-count
    size_t        total_bytes; /// total-allocation-size
    context_bind *composition; /// vector-of-pairs (context and its data struct; better to store the data along with it!)
    context_bind *bind; // this is a pointer to the top bind
};

struct symbol_data {
    hmap<u64, memory*> djb2 { 32 };
    hmap<i64, memory*> ids  { 32 };
    doubly<memory*>    list { };
};

struct none { };

static inline u64 djb2(cstr str) {
    u64     h = 5381;
    u64     v;
    while ((v = *str++))
            h = h * 33 + v;
    return  h;
}

template <typename T> using        FreeFn =          void(*)(T*);              /// src
template <typename T> using        InitFn =          void(*)(T*);              /// src
template <typename T> using     CompareFn =           int(*)(T*, T*);          /// a, b
template <typename T> using     BooleanFn =          bool(*)(T*);              /// src
template <typename T> using    ToStringFn =       memory*(*)(T*);              /// src
template <typename T> using  FromStringFn =            T*(*)(cstr);            /// src-cstring
template <typename T> using        CopyFn =          void(*)(T*, T*, size_t);  /// dst, src, count (realloc case)
template <typename T> using    DestructFn =          void(*)(T*, size_t);      /// src, count
template <typename T> using   ConstructFn =          void(*)(T*, size_t);      /// src, count
template <typename T> using      AssignFn =          void(*)(T*, T*);          /// dst, src
template <typename T> using        HashFn =        size_t(*)(T*, size_t);      /// src, count
template <typename T> using        MetaFn =  doubly<prop>(*)(T*);

/// type-api-check
/// typeof usage need not apply registration, registration is about ops on the object not its identification; that only requires a name)
template <typename T, typename = void> struct registered             : std::false_type { bool operator()() const { return value; }};

/// granular-checks
template <typename T, typename = void> struct registered_assign      : std::false_type { bool operator()() const { return value; }};
template <typename T, typename = void> struct registered_compare     : std::false_type { bool operator()() const { return value; }};
template <typename T, typename = void> struct registered_bool        : std::false_type { bool operator()() const { return value; }};
template <typename T, typename = void> struct registered_copy        : std::false_type { bool operator()() const { return value; }};
template <typename T, typename = void> struct registered_to_string   : std::false_type { bool operator()() const { return value; }};
template <typename T, typename = void> struct registered_from_string : std::false_type { bool operator()() const { return value; }};
template <typename T, typename = void> struct registered_init        : std::false_type { bool operator()() const { return value; }};
template <typename T, typename = void> struct registered_free        : std::false_type { bool operator()() const { return value; }};
template <typename T, typename = void> struct registered_meta        : std::false_type { bool operator()() const { return value; }};
template <typename T, typename = void> struct registered_hash        : std::false_type { bool operator()() const { return value; }};

template <typename T> struct registered            <T, std::enable_if_t<std::is_same_v<decltype(_type_sz    (std::declval<T*>  ())),
    size_t>>>       : std::true_type { bool operator()() const { return value; }};
template <typename T> struct registered_assign     <T, std::enable_if_t<std::is_same_v<decltype(assign      (std::declval<T*>  (), std::declval<T*>())),
    void>>>         : std::true_type { bool operator()() const { return value; }};
template <typename T> struct registered_compare    <T, std::enable_if_t<std::is_same_v<decltype(compare     (std::declval<T*>  (), std::declval<T*>())),
    int>>>          : std::true_type { bool operator()() const { return value; }};
template <typename T> struct registered_bool       <T, std::enable_if_t<std::is_same_v<decltype(boolean     (std::declval<T*>  ())),
    bool>>>         : std::true_type { bool operator()() const { return value; }};
template <typename T> struct registered_copy       <T, std::enable_if_t<std::is_same_v<decltype(copy        (std::declval<T*>  (), 
    std::declval<T*>(), size_t(0))), void>>>         : std::true_type { bool operator()() const { return value; }};

/// merge dtr and free
/// dtr called from free.


static int *from_string(cstr in) {
    return null;
}


template <typename T> struct registered_to_string  <T, std::enable_if_t<std::is_same_v<decltype(  to_string (std::declval<T*>  ())), memory*>>>      : std::true_type { bool operator()() const { return value; }};
template <typename T> struct registered_from_string<T, std::enable_if_t<std::is_same_v<decltype(from_string (std::declval<cstr>())), T*>>>           : std::true_type { bool operator()() const { return value; }};
template <typename T> struct registered_init       <T, std::enable_if_t<std::is_same_v<decltype(init        (std::declval<T*>  ())), void>>>         : std::true_type { bool operator()() const { return value; }};
template <typename T> struct registered_free       <T, std::enable_if_t<std::is_same_v<decltype(free        (std::declval<T*>  ())), void>>>         : std::true_type { bool operator()() const { return value; }};
template <typename T> struct registered_meta       <T, std::enable_if_t<std::is_same_v<decltype(meta        (std::declval<T*>  ())), doubly<prop>>>> : std::true_type { bool operator()() const { return value; }};
template <typename T> struct registered_hash       <T, std::enable_if_t<std::is_same_v<decltype(meta        (std::declval<T*>  ())), size_t>>>       : std::true_type { bool operator()() const { return value; }};

/// djb2 should be the hash_fn for char/8bit

/// use functions, not lambdas
template <typename T>
struct ops {
         FreeFn<T> free;
         InitFn<T> init;
      CompareFn<T> compare;
      BooleanFn<T> boolean;
     ToStringFn<T> to_string;
   FromStringFn<T> from_string;
         CopyFn<T> copy;
     DestructFn<T> destruct;
    ConstructFn<T> construct;
       AssignFn<T> assign;
         HashFn<T> hash;
         MetaFn<T> meta;
};

template <typename T>
static ops<T> *ftable();

///
struct idata {
    void            *src;     /// the source.
    cstr             name;
    size_t           base_sz; /// a types base sz is without regards to pointer state
    size_t           traits;
    alloc_schema    *schema;  /// primitives dont need this one
    ops<void>       *functions;
    symbol_data     *symbols;
    memory          *singleton;

    size_t size() {
        return schema ? schema->total_bytes : base_sz;
    }

    memory *lookup(symbol sym) {
        u64 hash = djb2(cstr(sym));
        memory **result = symbols ? symbols->djb2.lookup(hash) : null;
        return   result ? *result : null;
    }

    memory *lookup(i64 id) {
        memory **result = symbols ? symbols->ids.lookup(id) : null;
        return   result ? *result : null;
    }
};

void *mem_origin(memory *mem);

template <typename T>
T* mdata(raw_t origin, size_t index, type_t ctx);

struct context_bind {
    type_t        ctx;
    type_t        data;
    size_t        data_sz;
    size_t        ref_sz;  /// if not 0, this data points and occupies this size.  data_sz unchanged
    size_t        offset;
};

i64 integer_value(memory *mem);

template <typename T>
T real_value(memory *mem) {
    cstr c = mdata<char>(mem, 0);
    while (isalpha(char(*c)))
        c++;
    return T(strtod(c, null));
}

template <typename T>
size_t schema_info(alloc_schema *schema, int depth, T *p, idata *ctx_type);

struct util {
    static cstr copy(symbol cs) {
        size_t   sz = strlen(cs) + 1;
        cstr result = cstr(malloc(sz));
        memcpy(result, cs, sz - 1);
        result[sz - 1] = 0;
        return result;
    }
};

template <typename T>
size_t schema_info(alloc_schema *schema, int depth, T *p, idata *ctx_type) {
    ///
    if constexpr (!identical<T, none>()) {
        /// count is set to schema_info after first return
        if (schema->bind_count) {
            context_bind &bind = schema->composition[schema->bind_count - 1 - depth]; /// would be not-quite-ideal casting-wise to go the other way, lol.
            bind.ctx     = ctx_type ? ctx_type : typeof(T);
            bind.data    = identical<typename T::intern, none>() ? null : typeof(typename T::intern);
            bind.data_sz = bind.data ? typesize(typename T::intern) : 0;
            ///
            //if constexpr (!identical<typename T::DC, none>() && identical<typename T::MEM, mem_ptr_token>()) {
            if constexpr (identical<typename T::intern, none>()) {
                bind.ref_sz = sizeof(T*);
                schema->total_bytes += bind.ref_sz;
            }
        }
        
        typename T::parent_class *placeholder = null;
        return schema_info(schema, depth + 1, placeholder, null);
        ///
    } else
        return depth;
}


template <typename T>
void schema_populate(idata *type, T *p) {
    type->schema         = (alloc_schema*)calloc(1, sizeof(alloc_schema) * 16);
    alloc_schema &schema = *type->schema;
    ///
    if constexpr (registered_meta<T>()) {
        static T *def = new T();
        schema.meta   = T::meta (*def);
        delete def;
    }
    if (schema.meta) {
        schema.meta_map  = prop_map(size_t(16));
        for (ion::prop &prop: schema.meta) {
            symbol prop_name = prop.name();
            schema.meta_map[prop_name] = prop;
        }
    }
    ///
    schema.bind_count    = schema_info(&schema, 0, (T*)null, type);
    schema.composition   = (context_bind*)calloc(schema.bind_count, sizeof(context_bind) * 16);
    schema_info(&schema, 0, (T*)null, type);
    /// summarize recursion
    size_t offset = 0;
    for (size_t i = 0; i < schema.bind_count; i++) {
        context_bind &bind = schema.composition[i];
        bind.offset        = offset;
        offset            += bind.data_sz;
    }
    schema.total_bytes = offset;
    schema.bind = &schema.composition[schema.bind_count - 1];
}

template <typename T>
u64 hash_value(T &key) {
    if constexpr (is_primitive<T>()) {
        return u64(key);
    } else if constexpr (identical<T, cstr>() || identical<T, symbol>()) {
        return djb2(cstr(key));
    } else if constexpr (is_convertible<hash, T>()) {
        return hash(key);
    } else if constexpr (inherits<mx, T>() || is_mx<T>()) {
        return key.mx::mem->type->functions->hash(key.mem->origin, 1);
    } else {
        return 0;
    }
}

template <typename T>
u64 hash_index(T &key, size_t mod) {
    return hash_value(key) % mod;
}

constexpr bool is_debug() {
#ifndef NDEBUG
    return true;
#else
    return false;
#endif
}

void usleep(u64 u);

struct prop;

template <typename T>
inline bool vequals(T* b, size_t c, T v) {
    for (size_t i = 0; i < c; i++)
        if (b[i] != v)
            return false;
    return true;
}

struct attachment {
    const char    *id;
    void          *data;
    func<void()>   release;
};

using mutex = std::mutex;

struct memory {
    enum attr { constant = 1 };
    size_t              refs;
    u64                 attrs;
    type_t              type;
    size_t              count, reserve;
    size               *shape; // if null, then, its just 1 dim, of count.  otherwise this is the 'shape' of the data and strides through according
    doubly<attachment> *atts;
    size_t              id;
    bool                managed; /// origin is allocated by us
    raw_t               origin;

    size_t type_size() { return type->base_sz; } /// base_sz is the sizeof(context_class)

    static memory *raw_alloc(type_t type, size_t sz, size_t count, size_t res);
    static memory *    alloc(type_t type, size_t count, size_t reserve, raw_t src);
           void   *  realloc(size_t res,  bool fill_default);

    /// destruct data and set count to 0
    void clear();

    void drop();
    attachment *attach(symbol id, void *data, func<void()> release);
    attachment *find_attachment(symbol id);
    
    static inline const size_t autolen = UINT64_MAX;

    static memory *stringify(cstr cs, size_t len = autolen, size_t rsv = 0, bool constant = false, type_t ctype = typeof(char), i64 id = 0);
    static memory *string   (std::string s);
    static memory *cstring  (cstr        s);
    static memory *symbol   (::symbol s, type_t ty = typeof(char), i64 id = 0);
    
    ::symbol symbol() {
        return ::symbol(origin);
    }

    operator ::symbol*() {
        assert(attrs & constant);
        return (::symbol *)origin;
    }

    memory *copy(size_t reserve = 0);
    memory *grab();
    
    /// now it has a schema with types to each data structure, their offset in allocation, and the total allocation bytes
    /// also supports the primative store, non-mx
    template <typename T>
    T *data(size_t index) const;

    template <typename T>
    T &ref() const { return *data<T>(0); }
};

template <typename T>
memory *talloc(size_t count, size_t reserve) {
    return memory::alloc(typeof(T), count, reserve, (T*)null);
}

symbol prop::name() const {
    return symbol(key->origin);
}

template <typename T>
class has_string {
    typedef char one;
    struct two { char x[2]; };
    template <typename C> static one test( decltype(&C::string) ) ;
    template <typename C> static two test(...);    
public:
    enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

i64 integer_value(memory *mem);

struct str;

template <typename T>
T &defaults() {
    static T def_instance;
    return   def_instance; //typeof(T)->defaults->ref<T>();
}

template <typename T> T* mdata(memory *mem, size_t index) { return mem ? mem->data<T>(index) : null; }

template <typename T>
inline T &assign_mx(T &a, const T &b) {
    typeof(T)->functions->assign((void *)&a, b.mem);
    return a;
}

memory *cstring(cstr cs, size_t len = memory::autolen, size_t reserve = 0, bool sym = false);


using fn_t = func<void()>;

struct rand {
    struct seq {
        enum seeding {
            Machine,
            Seeded
        };
        inline seq(i64 seed, seq::seeding t = Seeded) {
            if (t == Machine) {
                std::random_device r;
                e = std::default_random_engine(r());
            } else
                e.seed(u32(seed)); // windows doesnt have the same seeding 
        }
        std::default_random_engine e;
        i64 iter = 0;
    };

    static inline seq global = seq(0, seq::Machine);

    static std::default_random_engine& global_engine() {
        return global.e;
    }

    static void seed(i64 seed) {
        global = seq(seed);
        global.e.seed(u32(seed)); // todo: seed with 64b, doesnt work on windows to be 32b seed
    }

    template <typename T>
    static T uniform(T from, T to, seq& s = global) {
        real r = std::uniform_real_distribution<real>(real(from), real(to))(s.e);
        return   std::is_integral<T>() ? T(math::round(r)) : T(r);
    }

    static bool coin(seq& s = global) { return uniform(0.0, 1.0) >= 0.5; }
};

using arg = pair<mx, mx>;
using ax  = doubly<arg>;

struct size;

struct mx {
    memory *mem = null; ///type_t  ctx = null; // ctx == mem->type for contextual classes, with schema populated
    using parent_class  = none;
    using context_class = none;
    using intern        = none;

    void *realloc(size_t reserve, bool fill_default);

    template <typename T>
    static inline memory *wrap(void *m, size_t count = 1, T *placeholder = (T*)null) {
        memory*     mem = (memory*)calloc(1, sizeof(memory));
        mem->count      = count;
        mem->reserve    = 0;
        mem->refs       = 1;
        mem->type       = typeof(T);
        mem->origin     = m;
        return mem;
    }

    /// T is context, not always the data type.  sometimes it is for simple cases, but not in context -> data 
    template <typename T>
    static inline memory *alloc(void *cp, size_t count = 1, size_t reserve = 1, T *ph = null) {
        return memory::alloc(typeof(T), count, reserve, raw_t(cp));
    }

    template <typename T>
    static inline memory *alloc(T *cp = null) {
        return memory::alloc(typeof(T), 1, 1, raw_t(cp));
    }

    template <typename DP>
    inline mx(DP **dptr) {
        mx::alloc((DP*)null);
        *dptr = (DP*)mem->origin;
    }
    
    ///
    inline mx(std::string s) : mem(memory:: string(s)) { }
    inline mx(cstr        s) : mem(memory::cstring(s)) { }

    template <typename T>
    static memory *alloc(size_t count, size_t reserve) {
        memory*     mem = (memory*)calloc(1, sizeof(memory)); /// there was a 16 multiplier prior.  todo: add address sanitizer support with appropriate clang stuff
        mem->count      = count;
        mem->reserve    = math::max(count, reserve);
        mem->refs       = 1;
        mem->type       = typeof(T); /// design-time should already know this is opaque if its indeed setup that way in declaration; use ptr_decl_opaque() or something
        return mem;
    }

    /// instance is fine here
    /// should be agnostic to vector; meaning the embedded types should not repeat across schema; reduce and simplify!
    template <typename T>
    static memory *import(T *addr, size_t count, size_t reserve, bool managed) {
        memory*     mem = alloc<T>(count, reserve); /// there was a 16 multiplier prior.  todo: add address sanitizer support with appropriate clang stuff
        mem->managed    = managed;
        mem->origin     = addr;
        return mem;
    }

    /// must move lambda code away from lambda table, and into constructor code gen
    template <typename T>
    memory *copy(T *src, size_t count, size_t reserve) {
        memory*     mem = (memory*)calloc(1, sizeof(memory)); /// there was a 16 multiplier prior.  todo: add address sanitizer support with appropriate clang stuff
        mem->count      = count;
        mem->reserve    = math::max(count, reserve);
        mem->refs       = 1;
        mem->type       = typeof(T); /// design-time should already know this is opaque if its indeed setup that way in declaration; use ptr_decl_opaque() or something
        mem->managed    = true;
        mem->origin     = (T*)calloc(mem->reserve, sizeof(T));
        ///
        if constexpr (is_primitive<T>()) {
            memcpy(mem->origin, src, sizeof(T) * count);
        } else {
            for (size_t i = 0; i < count; i++)
                new (&((T*)mem->origin)[i]) T(&src[i]);
        }
        return mem;
    }

    ///
    inline memory *grab() const { return mem->grab(); }
    inline size  *shape() const { return mem->shape;  }
    inline void    drop() const { mem->drop();        }


    inline attachment *find_attachment(symbol id) {
        return mem->find_attachment(id);
    }

    inline attachment *attach(symbol id, void *data, func<void()> release) {
        return mem->attach(id, data, release);
    }

    doubly<prop> meta() { return { }; }
    
    bool is_const() const { return mem->attrs & memory::constant; }

    prop *lookup(cstr cs) { return lookup(mx(mem_symbol(cs))); }

    prop *lookup(mx key) const {
        if (key.is_const() && mem->type->schema) {
            for (prop &p: mem->type->schema->meta)
                if (key.mem == p.key) 
                    return &p;
        }
        return null;
    }

    inline ~mx() { if (mem) mem->drop(); }
    
    

    /// interop with shared; needs just base type functionality for lambda
    inline mx(null_t n = null): mem(alloc<null_t>()) { }
    inline mx(memory *mem)    : mem(mem) { }
    inline mx(symbol ccs, type_t type = typeof(char)) : mx(mem_symbol(ccs, type)) { }
    inline mx(  cstr  cs, type_t type = typeof(char)) : mx(memory::stringify(cs, memory::autolen, 0, false, type)) { }
    inline mx(const mx     & b) :  mx( b.mem ?  b.mem->grab() : null) { }

    //template <typename T>
    //mx(T& ref, bool cp1) : mx(memory::alloc(typeof(T), 1, 0, cp1 ? &ref : (T*)null), ctx) { }

    inline mx(cstr src, size_t len = memory::autolen, size_t rs = 0) :
        mem(mx::copy<char>(src, len, 0)) { }

    template <typename T> inline T *data() const { return  mem->data<T>(0); }
    template <typename T> inline T &ref () const { return *mem->data<T>(0); }
    
    template <typename T>
    inline T *get(size_t index) const { return mem->data<T>(index); }

    inline mx(bool   v) : mem(copy(&v, 1, 1)) { }
    inline mx(u8     v) : mem(copy(&v, 1, 1)) { }
    inline mx(i8     v) : mem(copy(&v, 1, 1)) { }
    inline mx(u16    v) : mem(copy(&v, 1, 1)) { }
    inline mx(i16    v) : mem(copy(&v, 1, 1)) { }
    inline mx(u32    v) : mem(copy(&v, 1, 1)) { }
    inline mx(i32    v) : mem(copy(&v, 1, 1)) { }
    inline mx(u64    v) : mem(copy(&v, 1, 1)) { }
    inline mx(i64    v) : mem(copy(&v, 1, 1)) { }
    inline mx(r32    v) : mem(copy(&v, 1, 1)) { }
    inline mx(r64    v) : mem(copy(&v, 1, 1)) { }

    template <typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
    inline mx(E v) : mem(alloc(&v)) { }

    inline size_t  type_size() const { return mem->type_size(); }
    inline size_t   byte_len() const { return count() * type_size(); }

    memory    *copy(size_t res = 0) const { return mem->copy(res); }
    memory *quantum(size_t res = 0) const { return (res == 0 && mem->refs == 1) ? mem : mem->copy(); }

    bool is_string() const {
        return mem->type == typeof(char) || (mem->type->schema && mem->type->schema->bind->data == typeof(char));
    }

    void set(memory *m) {
        if (m != mem) {
            if (mem) mem->drop();
            mem = m ?  m->grab() : null;
        }
    }

    template <typename T>
    T &set(T v) {
        memory *nm = alloc(v);
        set(nm);
        return ref<T>();
    }

    size_t count() const { return mem ? mem->count : 0;    }
    type_t  type() const { return mem ? mem->type  : null; }

    memory  *to_string() const {
        if      (mem->type == typeof(i8) ) return memory::string(std::to_string(*(i8*)  mem->origin));
        else if (mem->type == typeof(i16)) return memory::string(std::to_string(*(i16*) mem->origin));
        else if (mem->type == typeof(i32)) return memory::string(std::to_string(*(i32*) mem->origin));
        else if (mem->type == typeof(i64)) return memory::string(std::to_string(*(i64*) mem->origin));
        else if (mem->type == typeof(u8) ) return memory::string(std::to_string(*(u8*)  mem->origin));
        else if (mem->type == typeof(u16)) return memory::string(std::to_string(*(u16*) mem->origin));
        else if (mem->type == typeof(u32)) return memory::string(std::to_string(*(u32*) mem->origin));
        else if (mem->type == typeof(u64)) return memory::string(std::to_string(*(u64*) mem->origin));
        else if (mem->type == typeof(r32)) return memory::string(std::to_string(*(r32*) mem->origin));
        else if (mem->type == typeof(r64)) return memory::string(std::to_string(*(r64*) mem->origin));
        else if (mem->type == typeof(bool))return memory::string(std::to_string(*(bool*)mem->origin));
        
        else if   (mem->type->functions->to_string)
            return mem->type->functions->to_string(mem->origin); /// call to_string() on context class
        
        else   if (mem->type->schema &&
                   mem->type->schema->bind->data->functions &&
                   mem->type->schema->bind->data->functions->to_string)
            return mem->type->schema->bind->data->functions->to_string(mem->origin); /// or data...
        
        else if (mem->type == typeof(char))
            return mem->grab();
        
        else {
            type_t id = mem->type;
            static char buf[128];
            const int l = snprintf(buf, sizeof(buf), "%s/%p", id->name, (void*)mem);
            return memory::stringify(cstr(buf), l);
        }
    }

    inline bool operator==(mx &b) const {
        if (mem == b.mem)
            return true;
    
        if (mem && type() == b.type() && mem->count == b.mem->count) {
            type_t ty = mem->type;
            size_t cn = mem->count;
            if (ty->traits & traits::primitive)
                return memcmp(mem->origin, b.mem->origin, ty->base_sz * cn) == 0;
            else if (ty->functions && ty->functions->compare)
                return ty->functions->compare(mem->origin, b.mem->origin); /// must pass this in; dont change to memory* type, deprecate those
        }
        return false;
    }

    inline bool operator!=(mx &b)    const { return !operator==(b); }
    
    inline bool operator==(symbol b) const {
        if (mem) {
            if (mem->attrs & memory::attr::constant) {
                return mem == mem_symbol(b);
            } else {
                return strcmp(b, mem->data<char>(0)) == 0;
            }
        }
        return false;
    }
    inline bool operator!=(symbol b) const { return !operator==(b); }

    mx &operator=(const mx &b) {
        mx &a = *this;
        if (a.mem != b.mem) {
            if (b.mem) b.mem->grab();
            if (a.mem) a.mem->drop();
            a.mem = b.mem;
        }
        return *this;
    }

    /// without this as explicit, there are issues with enum mx types casting to their etype.
    explicit operator bool() const {
        if (mem) { /// if mem, origin is always set
            type_t ty = mem->type;

            if (is_string())
                return mem->origin && *(char*)mem->origin != 0;
            
            if (ty == typeof(null_t))
                return false;

            /// always try context first. in theory gives more context
            if (ty->functions->boolean) {
                BooleanFn<void> ctx = ty->functions->boolean;
                return    ctx((void*)this);
            }
            
            /// then try this chain necklace
            if (ty->schema) {
                BooleanFn<void> data = ty->schema->bind->data->functions->boolean;
                return    data((void*)mem->origin);
            }
            
            /// then we've got backup.  if you got count i got an op
            return mem->count > 0;
        } else
            return false;
    }
    
    inline bool operator!() const { return !(operator bool()); }

    ///
    explicit operator   i8()      { return mem->ref<i8>();  }
    explicit operator   u8()      { return mem->ref<u8>();  }
    explicit operator  i16()      { return mem->ref<i16>(); }
    explicit operator  u16()      { return mem->ref<u16>(); }
    explicit operator  i32()      { return mem->ref<i32>(); }
    explicit operator  u32()      { return mem->ref<u32>(); }
    explicit operator  i64()      { return mem->ref<i64>(); }
    explicit operator  u64()      { return mem->ref<u64>(); }
    explicit operator  r32()      { return mem->ref<r32>(); }
    explicit operator  r64()      { return mem->ref<r64>(); }
    explicit operator  memory*()  { return mem->grab(); } /// trace its uses
    explicit operator  symbol()   { assert(mem->attrs & memory::constant); return mem->ref<symbol>(); }
};

template <typename T>
constexpr bool is_mx() {
    return identical<T, mx>();
}

//struct mutex:mx { ptr_declare(mutex, mx, std::shared_mutex); };

template <typename T>
struct lambda;

template <typename T, typename = std::void_t<>> struct has_bool_op: std::false_type {};
template <typename T>                           struct has_bool_op<T, std::void_t<decltype(static_cast<bool>(std::declval<T>()))>> : std::true_type {};
template <typename T> constexpr bool has_bool        = has_bool_op<T>::value;

template <typename> struct is_lambda : false_type { };

template <typename R, typename... Args>
struct lambda<R(Args...)>:mx {
    using fdata = std::function<R(Args...)>;
    
    /// just so we can set it in construction
    struct container {
        fdata *fn;
        ~container() {
            delete fn;
        }
    };
    
    ptr(lambda, mx, container);
    
    template <typename F>
    lambda(F&& fn);
    
    R operator()(Args... args) const {
        return (*data->fn)(std::forward<Args>(args)...);
    }
    
    operator bool() {
        return data && data->fn && *data->fn;
    }
};

template <typename T>
struct lambda_traits;

// function pointer
template <typename R, typename... Args>
struct lambda_traits<R(*)(Args...)> : public lambda_traits<R(Args...)> {};

// member function pointer
template <typename C, typename R, typename... Args>
struct lambda_traits<R(C::*)(Args...)> : public lambda_traits<R(Args...)> {};

// const member function pointer
template <typename C, typename R, typename... Args>
struct lambda_traits<R(C::*)(Args...) const> : public lambda_traits<R(Args...)> {};

// member object pointer
template <typename C, typename R>
struct lambda_traits<R(C::*)> : public lambda_traits<R()> {};

// functor / lambda function
template <typename F>
struct lambda_traits {
private:
    using call_type = lambda_traits<decltype(&F::operator())>;
public:
    using args_tuple = typename call_type::args_tuple;
};

template <typename R, typename... Args>
struct lambda_traits<R(Args...)> {
    using args_tuple = std::tuple<Args...>;
};


template <typename T> struct is_lambda<lambda<T>> : true_type  { };

template <typename R, typename... Args>
template <typename F>
lambda<R(Args...)>::lambda(F&& fn) : mx() {
    if constexpr (is_lambda<std::remove_reference_t<F>>::value) {
        mx::mem = fn.mem->grab();
        data    = (container*)mem->origin;
    } else {
        if constexpr (std::is_invocable_r_v<R, F, Args...>) {
            mx::mem  = mx::alloc<container>();
            data     = (container*)mem->origin;
            data->fn = new fdata(std::forward<F>(fn));
        } else {
            static_assert("args conversion not supported");
        }
    }
}

template <typename T>
inline void vset(T *data, u8 bv, size_t c) {
    memset((void*)data, int(bv), sizeof(T) * c);
}

template <typename T>
inline void vset(T *data, T v, size_t c) {
    for (size_t i = 0; i < c; i++)
        data[i] = v;
}

/// --------------------------------
/// this one is to showcase new-found data heart.
/// --------------------------------
/// array must be basis of str; it need only allocate 1 default reserve, or null char
/// --------------------------------
template <typename T>
struct array:mx {
protected:
    size_t alloc_size() const {
        size_t usz = size_t(mem->count);
        size_t ual = size_t(mem->reserve);
        return ual == 0 ? usz : ual;
    }

public:
    static inline type_t element_type() { return typeof(T); }
    ///
    ptr(array, mx, T);

    /// push an element, return its reference
    T &push(T &v) {
        size_t csz = mem->count;
        if (csz + 1 > alloc_size())
            data = (T*)mem->realloc(csz + 1, false); /// when you realloc you must have a pointer. 
        ///
        new (&data[csz]) T(v);
        mem->count++;
        return data[csz];
    }

    T &push() {
        size_t csz = mem->count;
        if (csz + 1 > alloc_size())
            data = mem->realloc(csz + 1, false);

        new (&data[csz]) T();
        mem->count++;
        return data[csz];
    }

    void push(T *pv, size_t push_count) {
        if (!push_count) return; 
        ///
        size_t usz = size_t(mem->count);
        size_t rsv = size_t(mem->reserve);

        if (usz + push_count > rsv)
            data = (T*)mem->realloc(usz + push_count, false);
        
        if constexpr (is_primitive<T>()) {
            memcpy(&data[usz], pv, sizeof(T) * push_count);
        } else {
            /// call copy-constructor
            for (size_t i = 0; i < push_count; i++)
                new (&data[usz + i]) T(&pv[i]);
        }
        usz += push_count;
        mem->count = usz;
    }

    void pop(T *prev = null) {
        size_t i = size_t(mem->count);
        assert(i > 0);
        if (prev)
        *prev = data[i];
        if constexpr (!is_primitive<T>())
            data[i]. ~T();
        --mem->count;
    }

    ///
    template <typename S>
    memory *convert_elements(array<S> &a_src) {
        constexpr bool can_convert = std::is_convertible<S, T>::value;
        if constexpr (can_convert) {
            memory* src = a_src.mem;
            if (!src) return null;
            memory* dst;
            if constexpr (identical<T, S>()) {
                dst = src->grab();
            } else {
                type_t ty = typeof(T);
                size_t sz = a_src.len();
                dst       = memory::alloc(ty, sz, sz, (T*)null);
                T*      b = dst->data<T>(0);
                S*      a = src->data<S>(0);
                for (size_t i = 0; i < sz; i++)
                    new (&b[i]) T(a[i]);
            }
            return dst;
        } else {
            printf("cannot convert elements on array\n");
            return null;
        }
    }

    static inline type_t data_type() { return typeof(T); }
    
    static array<T> empty() { return array<T>(size_t(1)); }

    array(initial<T> args) : array(size(args.size()), size(args.size())) {
        num i = 0; /// typesize must be sizeof mx if its an mx type
        T* e = data;
        for (auto &v:args) new (&data[i++]) T(v); /// copy construct, we allocated memory raw in reserve
    }

    inline void set_size(size sz) {
        if (mem->reserve < sz)
            data = (T*)mem->realloc(sz, true);
    }

    ///
    size_t count(T v) {
        size_t c = 0;
        for (size_t i = 0; i < mem->count; i++)
            if (data[i] == v)
                c++;
        return c;
    }

    ///
    T* elements() const { return data; }

    ///
    int index_of(T v) const {
        for (size_t i = 0; i < mem->count; i++) {
            if (data[i] == v)
                return int(i);
        }
        return -1;
    }

    ///
    template <typename R>
    R select_first(lambda<R(T&)> qf) const {
        for (size_t i = 0; i < mem->count; i++) {
            R r = qf(data[i]);
            if (r)
                return r;
        }
        if constexpr (is_numeric<R>()) /// constexpr implicit when valid?
            return R(0);
        else
            return R();
    }

    ///
    template <typename R>
    R select(lambda<R(T&)> qf) const {
        array<R> res(mem->count);
        ///
        for (size_t i = 0; i < mem->count; i++) {
            R r = qf(data[i]);
            if (r)
                res += r;
        }
        return res;
    }

    array(size_t count, T value) : array(count) {
        for (size_t i = 0; i < count; i++)
            push(value);
    }

    /// quick-sort
    array<T> sort(func<int(T &a, T &b)> cmp) {
        /// create reference list of identical size as given, pointing to the respective index
        size_t sz = len();
        mx **refs = (mx **)calloc(len(), sizeof(mx*));
        for (size_t i = 0; i < sz; i++)
            refs[i] = &data[i];

        /// recursion lambda
        func<void(int, int)> qk;
        qk = [&](int s, int e) {
            if (s >= e)
                return;
            int i  = s, j = e, pv = s;
            while (i < j) {
                while (cmp(refs[i]->ref<T>(), refs[pv]->ref<T>()) <= 0 && i < e)
                    i++;
                while (cmp(refs[j]->ref<T>(), refs[pv]->ref<T>()) > 0)
                    j--;
                if (i < j)
                    std::swap(refs[i], refs[j]);
            }
            std::swap(refs[pv], refs[j]);
            qk(s, j - 1);
            qk(j + 1, e);
        };

        /// launch recursion
        qk(0, len() - 1);

        /// create final result array from references
        array<T> result = { size(), [&](size_t i) { return *refs[i]; }};
        //free(refs);
        return result;
    }
    
    inline size     shape() const { return mem->shape ? *mem->shape : size(mem->count); }
    inline size_t     len() const { return mem->count; }
    inline size_t  length() const { return mem->count; }
    inline size_t reserve() const { return mem->reserve; }

    array(T* d, size_t sz, size_t al = 0) : array(talloc<T>(0, sz)) {
        for (size_t i = 0; i < sz; i++)
            new (&data[i]) T(d[i]);
    }

    array(size al, size sz = size(0), lambda<T(size_t)> fn = lambda<T(size_t)>(nullptr)) : 
            array(talloc<T>(sz, al)) {
        if (al.count != 1)
            mem->shape = new size(al); /// only allocate a shape in the shaped cases; a 0 count is not valid atm.
        if (fn)
            for (size_t i = 0; i < size_t(sz); i++)
                data[i] = T(fn(i));
    }

    /// constructor for allocating with a space to fill (and construct; this allocation does not run the constructor (if non-primitive) until append)
    /// indexing outside of shape space does cause error
    array(size_t  reserve) : array(talloc<T>(0, size_t(reserve))) { }
    array(   u32  reserve) : array(talloc<T>(0, size_t(reserve))) { }
    array(   i32  reserve) : array(talloc<T>(0, size_t(reserve))) { }
    array(   i64  reserve) : array(talloc<T>(0, size_t(reserve))) { }

    inline T &push_default()  { return push();  }

    /// important that this should always assign or
    /// copy construct the elements in, as supposed to  a simple reference
    array<T> mid(int start, int len = -1) const {
        /// make sure we dont stride out of bounds
        int ilen = int(this->len());
        assert(abs(start) < ilen);
        if (start < 0) start = ilen + start;
        size_t cp_count = len < 0 ? math::max(0, (ilen - start) + len) : len; /// 0 is a value to keep as 0 len, auto starts at -1 (+ -1 from remaining len)
        assert(start + cp_count <= ilen);

        array<T> result(cp_count);
        for (size_t i = 0; i < cp_count; i++)
            result.push(data[start + i]);
        ///
        return result;
    }

    /// get has more logic in the indexing than the delim.  wouldnt think of messing with delim logic but this is useful in general
    inline T &get(num i) const {
        if (i < 0) {
            i += num(mem->count);
            assert(i >= 0);
        }
        assert(i >= 0 && i < num(mem->count));
        return data[i];
    }


    iter<T>  begin() const { return { (T *)data, size_t(0)    }; }
	iter<T>    end() const { return { (T *)data, size_t(mem->count) }; }
    T&       first() const { return data[0];        }
	T&        last() const { return data[mem->count - 1]; }
    
    void realloc(size s_to) {
        data = mx::realloc(s_to, false);
    }

    operator  bool() const { return mx::mem && mem->count > 0; }
    bool operator!() const { return !(operator bool()); }

    bool operator==(array b) const {
        if (mx::mem == b.mem) return true;
        if (mx::mem && b.mem) {
            if (mem->count != b.mem->count) return false;
            for (size_t i = 0; i < mem->count; i++)
                if (!(data[i] == b.data[i])) return false;
        }
        return true;
    }

    /// not-equals
    bool operator!=(array b) const { return !(operator==(b)); }
    
    inline T &operator[](size index) {
        if (index.count == 1)
            return (T &)data[size_t(index.values[0])];
        else {
            assert(mem->shape);
            mem->shape->assert_index(index);
            size_t i = mem->shape->index_value(index);
            return (T &)data[i];
        }
    }

    inline T &operator[](size_t index) { return data[index]; }

    template <typename IX>
    inline T &operator[](IX index) {
        if constexpr (identical<IX, size>()) {
            mem->shape->assert_index(index);
            size_t i = mem->shape->index_value(index);
            return (T &)data[i];
        } else if constexpr (std::is_enum<IX>() || is_integral<IX>()) { /// just dont bitch at people.  people index by so many types.. most type casts are crap and noise as result
            size_t i = size_t(index);
            return (T &)data[i];
        } else {
            /// now this is where you bitch...
            assert(!"index type must be size or integral type");
            return (T &)data[0];
        }
    }

    inline T &operator+=(T v) { 
        return push(v);
    }

    void clear() {
        data = (T*)mem->realloc(1, false);
    }
};

using ichar = int;

static lambda<void(u32)> fn = {};


/// UTF8 :: Bjoern Hoehrmann <bjoern@hoehrmann.de>
/// https://bjoern.hoehrmann.de/utf-8/decoder/dfa/
struct utf {
    static inline int decode(u8* cs, lambda<void(u32)> fn = {}) {
        if (!cs) return 0;

        static const u8 utable[] = {
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
            7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
            8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
            0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
            0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
            0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
            1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
            1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
            1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
        };

        u32    code;
        u32    state = 0;
        size_t ln    = 0;
        ///
        auto     dc    = [&](u32 input) {
            u32      u = utable[input];
            code	   = (state != 1) ? (input & 0x3fu) | (code << 6) : (0xff >> u) & input;
            state	   = utable[256 + state * 16 + u];
            return state;
        };

        for (u8 *cs0 = cs; *cs0; ++cs0)
            if (!dc(*cs0)) {
                if (fn) fn(code);
                ln++;
            }

        return (state != 0) ? -1 : int(ln);
    }

    static inline int len(uint8_t* cs) {
        int l = decode(cs, null);
        return math::max<int>(0, l);
    }
};

///
struct ex:mx {
    ///
    template <typename C>
    ex(memory *m, C *inst) : mx(m) {
        mem->attach("container", typeof(C), null);
    }
    ///
    ex() : mx() { }
    ///
    template <typename E, typename C>
    ex(E v, C *inst) : ex(alloc<E>(&v), this) { }
};

/// useful for constructors that deal with ifstream
size_t length(std::ifstream& in);

struct str:mx {
    enum MatchType {
        Alpha,
        Numeric,
        WS,
        Printable,
        String,
        CIString
    };

    cstr data;
    
    operator std::string() { return std::string(data); }

    memory * symbolize() { return mem_symbol(data, typeof(char)); } 

    /// \\ = \ ... \x = \x
    static str parse_quoted(cstr *cursor, size_t max_len = 0) {
        ///
        cstr first = *cursor;
        if (*first != '"')
            return "";
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
            return "";
        ///
        size_t len = (size_t)(s - start);
        if (max_len > 0 && len > max_len)
            return "";
        ///
        *cursor = &s[1];
        str result = "";
        for (int i = 0; i < int(len); i++)
            result += start[i];
        ///
        return result;
    }

    /// static methods
    static str combine(const str sa, const str sb) {
        cstr       ap = (cstr)sa.data;
        cstr       bp = (cstr)sb.data;
        ///
        if (!ap) return sb.copy();
        if (!bp) return sa.copy();
        ///
        size_t    ac = sa.count();
        size_t    bc = sb.count();
        str       sc = sa.copy(ac + bc + 1);
        cstr      cp = (cstr)sc.data;
        ///
        memcpy(&cp[ac], bp, bc);
        sc.mem->count = ac + bc;
        return sc;
    }

    /// skip to next non-whitespace, this could be 
    static char &non_wspace(cstr cs) {
        cstr sc = cs;
        while (isspace(*sc))
            sc++;
        return *sc;
    }

    str(memory        *mem) : mx(mem->type == typeof(null_t) ? alloc<char>(null, 0, 16) : mem), data(&mx::ref<char>()) { }
    str(mx               m) : mx(m.grab())                       { } // damn teh expense!
    str(nullptr_t n = null) : str(alloc<char>(null, 0, 16))      { }
    str(symbol         ccs) : str(mem_symbol(ccs, typeof(char))) { }
    str(char            ch) : str(alloc<char>(null, 1, 2))       { *data = ch; }
    str(size_t          sz) : str(alloc<char>(null, 0, sz + 1))  { }
    
    //str(std::ifstream  &in) : str((memory*)null) { }
    str(std::ifstream &in) : str(alloc<char>(null, 0, ion::length(in) + 1)) {
        mem->count = mem->reserve - 1;
        in.read((char*)data, mem->count);
    }
                                  
    static str from_integer(i64 i) { return str(memory::string(std::to_string(i))); }

    str(float          f32, int n = 6) : str(memory::string(string_from_real(f32, n))) { }
    str(double         f64, int n = 6) : str(memory::string(string_from_real(f64, n))) { }

    str(cstr cs, size_t len = memory::autolen, size_t rs = 0) : str(cstring(cs, len, rs)) { } 
    str(std::string s) : str(cstr(s.c_str()), s.length()) { } // error: member initializer 'str' does not name a non-static data member or base class


    str(const str &s) : str(s.mem->grab()) { }

    inline cstr cs() { return cstr(data); }

    /// tested.
    str expr(lambda<str(str)> fn) const {
        cstr   pa = data;
        auto   be = [&](int i) -> bool { return pa[i] == '{' && pa[i + 1] != '{'; };
        auto   en = [&](int i) -> bool { return pa[i] == '}'; };
        bool   in = be(0);
        int    fr = 0;
        size_t ln = byte_len();
        static size_t static_m = 4;
        for (;;) {
            bool exploding = false;
            size_t sz      = math::max(size_t(1024), ln * static_m);
            str    rs      = str(sz);
            for (int i = 0; i <= int(ln); i++) {
                if (i == ln || be(i) || en(i)) {
                    bool is_b = be(i);
                    bool is_e = en(i);
                    int  cr = int(i - fr);
                    if (cr > 0) {
                        if (in) {
                            str exp_in = str(&pa[fr], cr);
                            str out = fn(exp_in);
                            if ((rs.byte_len() + out.byte_len()) > sz) {
                                exploding = true;
                                break;
                            }
                            rs += out;
                        } else {
                            str out = str(&pa[fr], cr);
                            if ((rs.byte_len() + out.byte_len()) > sz) {
                                exploding = true;
                                break;
                            }
                            rs += out;
                        }
                    }
                    fr = i + 1;
                    in = be(i);
                }
            }
            if (exploding) {
                static_m *= 2;
                continue;
            }
            return rs;

        }
        return null;
    }

    /// format is a user of expr
    str format(array<mx> args) const {
        return expr([&](str e) -> str {
            size_t index = size_t(e.integer_value());
            if (index >= 0 && index < args.len()) {
                mx     &a    = args[index];
                memory *smem = a.to_string();
                return  smem;
            }
            return null;
        });
    }

    /// just using cs here, for how i typically use it you could cache the strings
    static str format(symbol cs, array<mx> args) {
        return str(cs).format(args);
    }

    operator fs::path()                     const { return fs::path(std::string(data));  }
    void        clear()                     const { if    (mem)  mem->count = *data = 0; }
    bool        contains   (array<str>   a) const { return index_of_first(a, null) >= 0; }
    str         operator+  (symbol       s) const { return combine(*this, (cstr )s);     }
    bool        operator<  (const str    b) const { return strcmp(data, b.data) <  0;    }
    bool        operator>  (const str    b) const { return strcmp(data, b.data) >  0;    }
    bool        operator<  (symbol       b) const { return strcmp(data, b)      <  0;    }
    bool        operator>  (symbol       b) const { return strcmp(data, b)	     >  0;   }
    bool        operator<= (const str    b) const { return strcmp(data, b.data) <= 0;    }
    bool        operator>= (const str    b) const { return strcmp(data, b.data) >= 0;    }
    bool        operator<= (symbol       b) const { return strcmp(data, b)      <= 0;    }
    bool        operator>= (symbol       b) const { return strcmp(data, b)      >= 0;    }
  //bool        operator== (std::string  b) const { return strcmp(data, b.c_str()) == 0;  }
  //bool        operator!= (std::string  b) const { return strcmp(data, b.c_str()) != 0;  }
    bool        operator== (str          b) const { return strcmp(data, b.data)  == 0;  }
    bool        operator== (symbol       b) const { return strcmp(data, b)       == 0;  }
    bool        operator!= (symbol       b) const { return strcmp(data, b)       != 0;  }
    char&		operator[] (size_t       i) const { return (char&)data[i];               }
    int         operator[] (str          b) const { return index_of(b);                  }
                operator             bool() const { return count() > 0;                  }
    bool        operator!()                 const { return !(operator bool());           }
    inline str  operator+    (const str sb) const { return combine(*this, sb);           }
    inline str &operator+=   (str        b) {
        if (!mem->constant && (mem->reserve >= (mem->count + b.mem->count) + 1)) {
            cstr    ap =   data;
            cstr    bp = b.data;
            size_t  bc = b.mem->count;
            size_t  ac =   mem->count;
            memcpy(&ap[ac], bp, bc); /// when you think of data size changes, think of updating the count. [/mops-away]
            ac        += bc;
            ap[ac]     = 0;
            mem->count = ac;
        } else {
            *this = combine(*this, b);
        }
        
        return *this;
    }

    str &operator+= (const char b) { return operator+=(str((char) b)); }
    str &operator+= (symbol b) { return operator+=(str((cstr )b)); } /// not utilizing cchar_t yet.  not the full power.

    /// add some compatibility with those iostream things.
    friend std::ostream &operator<< (std::ostream& os, str const& s) {
        return os << std::string(cstr(s.data));
    }

    bool iequals(str b) const { return len() == b.len() && lcase() == b.lcase(); }

    template <typename F>
    static str fill(size_t n, F fn) {
        auto ret = str(n);
        for (size_t i = 0; i < n; i++)
            ret += fn(num(i));
        return ret;
    }
    
    int index_of_first(array<str> elements, int *str_index) const {
        int  less  = -1;
        int  index = -1;
        for (iter<str> it = elements.begin(), e = elements.end(); it != e; ++it) {
            ///
            str &find = (str &)it;
            ///
            //for (str &find:elements) {
            ++index;
            int i = index_of(find);
            if (i >= 0 && (less == -1 || i < less)) {
                less = i;
                if (str_index)
                    *str_index = index;
            }
            //}
        }
        if (less == -1 && str_index) *str_index = -1;
        return less;
    }

    bool starts_with(symbol s) const {
        size_t l0 = strlen(s);
        size_t l1 = len();
        if (l1 < l0)
            return false;
        return memcmp(s, data, l0) == 0;
    }

    size_t len() const {
        return count();
    }

    size_t utf_len() const {
        int ilen = utf::len((uint8_t*)data);
        return size_t(ilen >= 0 ? ilen : 0);
    }

    bool ends_with(symbol s) const {
        size_t l0 = strlen(s);
        size_t l1 = len();
        if (l1 < l0) return false;
        cstr e = &data[l1 - l0];
        return memcmp(s, e, l0) == 0;
    }

    static str read_file(fs::path path) {
        std::ifstream in(path);
        return str(in);
    }
                                  
    static str read_file(std::ifstream& in) {
        return str(in);
    }
                                  
    str recase(bool lower = true) const {
        str     b  = copy();
        cstr    rp = b.data;
        num     rc = b.byte_len();
        int     iA = lower ? 'A' : 'a';
        int     iZ = lower ? 'Z' : 'z';
        int   base = lower ? 'a' : 'A';
        for (num i = 0; i < rc; i++) {
            char c = rp[i];
            rp[i]  = (c >= iA && c <= iZ) ? (base + (c - iA)) : c;
        }
        return b;
    }

    str ucase() const { return recase(false); }
    str lcase() const { return recase(true);  } 

    static int nib_value(char c) {
        return (c >= '0' && c <= '9') ? (     c - '0') :
               (c >= 'a' && c <= 'f') ? (10 + c - 'a') :
               (c >= 'A' && c <= 'F') ? (10 + c - 'A') : -1;
    }

    static char char_from_nibs(char c1, char c2) {
        int nv0 = nib_value(c1);
        int nv1 = nib_value(c2);
        return (nv0 == -1 || nv1 == -1) ? ' ' : ((nv0 * 16) + nv1);
    }

    str replace(str fr, str to, bool all = true) const {
        str&   sc   = (str&)*this;
        cstr   sc_p = sc.data;
        cstr   fr_p = fr.data;
        cstr   to_p = to.data;
        size_t sc_c = math::max(size_t(0), sc.byte_len() - 1);
        size_t fr_c = math::max(size_t(0), fr.byte_len() - 1);
        size_t to_c = math::max(size_t(0), to.byte_len() - 1);
        size_t diff = to_c > fr_c ? math::max(size_t(0), to_c - fr_c) : 0;
        str    res  = str(sc_c + diff * (sc_c / fr_c + 1));
        cstr   rp   = res.data;
        size_t w    = 0;
        bool   once = true;

        /// iterate over string, check strncmp() at each index
        for (size_t i = 0; i < sc_c; ) {
            if ((all || once) && strncmp(&sc_p[i], fr_p, fr_c) == 0) {
                /// write the 'to' string, incrementing count to to_c
                memcpy (&rp[w], to_p, to_c);
                i   += to_c;
                w   += to_c;
                once = false;
            } else {
                /// write single char
                rp[w++] = sc_p[i++];
            }
        }

        /// end string, set count (count includes null char in our data)
        rp[w++] = 0;
        res.mem->count = w;

        /// validate allocation and write
        assert(w <= res.mem->reserve);
        return res;
    }

    /// mid = substr; also used with array so i thought it would be useful to see them as same
    str mid(int start, int len = -1) const {
        int ilen = int(count());
        assert(abs(start) < ilen);
        if (start < 0) start = ilen + start;
        int cp_count = len <= 0 ? (ilen - start) : len;
        assert(start + cp_count <= ilen);
        return str(&data[start], cp_count);
    }

    str mid(size_t start, int len = -1) const { return mid(int(start), len); }

    ///
    template <typename L>
    array<str> split(L delim) const {

        array<str>  result;
        size_t start = 0;
        size_t end   = 0;
        cstr   pc    = (cstr)data;

        ///
        if constexpr (inherits<str, L>()) {
            int  delim_len = int(delim.byte_len());
            ///
            if (len() > 0) {
                while ((end = index_of(delim, int(start))) != -1) {
                    str  mm = mid(start, int(end - start));
                    result += mm;
                    start   = end + delim_len;
                }
                result += mid(start);
            } else
                result += str();
        } else {
            ///
            if (mem->count) {
                for (size_t i = 0; i < mem->count; i++) {
                    int sps = delim(pc[i]);
                    bool discard;
                    if (sps != 0) {
                        discard = sps == 1;
                        result += str { &pc[start], discard ? (i - start) : (i - start + 1) };
                        start   = int(i + 1);
                    }
                    continue;
                }
                result += &pc[start];
            } else
                result += str();
        }
        ///
        return result;
    }

    iter<char> begin() { return { data, 0 }; }
    iter<char>   end() { return { data, size_t(byte_len()) }; }

    array<str> split(symbol s) const { return split(str(s)); }
    array<str> split() { /// common split, if "abc, and 123", result is "abc", "and", "123"}
        array<str> result;
        str        chars;
        ///
        cstr pc = data;
        for (;;) {
            char &c = *pc++;
            if  (!c) break;
            bool is_ws = isspace(c) || c == ',';
            if (is_ws) {
                if (chars) {
                    result += chars;
                    chars.clear();
                }
            } else
                chars += c;
        }
        ///
        if (chars || !result)
            result += chars;
        ///
        return result;
    }

    enum index_base {
        forward,
        reverse
    };

    /// if from is negative, search is backwards from + 1 (last char not skipped but number used to denote direction and then offset by unit)
    int index_of(str b, int from = 0) const {
        if (!b.mem || b.mem->count == 0) return  0; /// a base string is found at base index.. none of this empty string permiates crap.  thats stupid.   ooooo there are 0-true differences everywhere!
        if (!  mem ||   mem->count == 0) return -1;

        cstr   ap =   data;
        cstr   bp = b.data;
        int    ac = int(  mem->count);
        int    bc = int(b.mem->count);
        
        /// dont even try if b is larger
        if (bc > ac) return -1;

        /// search for b, reverse or forward dir
        if (from < 0) {
            for (int index = ac - bc + from + 1; index >= 0; index--) {
                if (strncmp(&ap[index], bp, bc) == 0)
                    return index;
            }
        } else {
            for (int index = from; index <= ac - bc; index++)
                if (strncmp(&ap[index], bp, bc) == 0)
                    return index;
        }
        
        /// we aint found ****. [/combs]
        return -1;
    }

    int index_of(MatchType ct, symbol mp = null) const;

    i64 integer_value() const {
        return ion::integer_value(mx::mem);
    }

    template <typename T>
    T real_value() const {
        return T(ion::real_value<T>(mx::mem));
    }

    bool has_prefix(str i) const {
        char    *s = data;
        size_t isz = i.byte_len();
        size_t  sz =   byte_len();
        return  sz >= isz ? strncmp(s, i.data, isz) == 0 : false;
    }

    bool numeric() const {
        return byte_len() && (data[0] == '-' || isdigit(*data));
    }

    bool matches(str input) const {
        lambda<bool(cstr, cstr)> fn;
        str&  a = (str &)*this;
        str&  b = input;
             fn = [&](cstr s, cstr p) -> bool {
                return (p &&  *p == '*') ? ((*s && fn(&s[1], &p[0])) || fn(&s[0], &p[1])) :
                      (!p || (*p == *s && fn(&s[1],   &p[1])));
        };
        for (cstr ap = a.data, bp = b.data; *ap != 0; ap++)
            if (fn(ap, bp))
                return true;
        return false;
    }

    str trim() const {
        cstr  s = data;
        int   c = int(byte_len());
        int   h = 0;
        int   t = 0;

        /// measure head
        while (isspace(s[h]))
            h++;

        /// measure tail
        while (isspace(s[c - 1 - t]) && (c - t) > h)
            t++;

        /// take body
        return str(&s[h], c - (h + t));
    }

    size_t reserve() const { return mx::mem->reserve; }
};

enums(split_signal, none,
    "none, discard, retain",
     none, discard, retain)

struct fmt:str {
    struct hex:str {
        template <typename T>
        cstr  construct(T* v) {
            type_t id = typeof(T);
            static char buf[1024];
            snprintf(buf, sizeof(buf), "%s/%p", id->name, (void*)v);
            return buf;
        }
        template <typename T>
        hex(T* v) : str(construct(v)) { }
    };

    /// format string given template, and mx-based arguments
    inline fmt(str templ, array<mx> args) : str(templ.format(args)) { }
    fmt():str() { }

    operator memory*() { return grab(); }
};

/// cstr operator overload
inline str operator+(cstr cs, str rhs) { return str(cs) + rhs; }

template <typename V>
struct map:mx {
    static inline type_t value_type() { return typeof(V); }
    inline static const size_t default_hash_size = 64;
    using hmap = ion::hmap<mx, field<V>*>;

    struct mdata {
        doubly<field<V>>  fields;
        hmap           *hash_map;

        /// boolean operator for key
        bool operator()(mx key) {
            if (hash_map)
                return hash_map->lookup(key);
            else {
                for (field<V> &f:fields)
                    if (f.key.mem == key.mem)
                        return true;
            }
            return false; 
        }

        template <typename K>
        inline V &operator[](K k) {
            if constexpr (inherits<mx, K>()) {
                return fetch(k).value;
            } else {
                mx io_key = mx(k);
                return fetch(io_key).value;
            }
        }

        field<V> &first()  { return fields.first(); }
        field<V> & last()  { return fields. last(); }
        size_t    count()  { return fields.len();   }

        size_t    count(mx k) {
            type_t tk = k.type();
            memory *b = k.mem;
            if (!(k.mem->attrs & memory::constant) && (tk->traits & traits::primitive)) {
                size_t res = 0;
                for (field<V> &f:fields) {
                    memory *a = f.key.mem;
                    assert(a->type->size() == b->type->size());
                    if (a == b || (a->count == b->count && memcmp(a->origin, b->origin, a->count * a->type->size()) == 0)) // think of new name instead of type_* ; worse yet is handle base types in type
                        res++;
                }
                return res;
            } else {
                /// cstring symbol identity not working atm, i think they are not registered in all cases in mx
                size_t res = 0;
                for (field<V> &f:fields)
                    if (f.key.mem == b)
                        res++;
                return res;
            }
        }

        size_t count(cstr cs) {
            size_t res = 0;
            for (field<V> &f:fields)
                if (strcmp(symbol(f.key.mem->origin), symbol(cs)) == 0)
                    res++;
            return res;
        }

        field<V> *lookup(mx &k) const {
            if (hash_map) {
                field<V> **ppf = hash_map->lookup(k);
                return  ppf ? *ppf : null;
            } else {
                for (field<V> & f : fields)
                    if (f.key == k)
                        return &f;
            }
            return null;
        }

        bool remove(field<V> &f) {
            item<field<V>> *i = item<field<V>>::container(f); // the container on field<V> is item<field<V>>, a negative offset
            fields.remove(i);
            return true;
        }

        bool remove(mx &k) {
            field<V> *f = lookup(k);
            return f ? remove(*f) : false;
        }

        field<V> &fetch(mx &k) {
            field<V> *f = lookup(k);
            if (f) return *f;
            ///
            fields += { k, V() };
            field<V> &last = fields->last();

            if (hash_map)
              (*hash_map)[k] = &last;

            return last;
        }

        operator bool() { return ((hash_map && hash_map->len() > 0) || (fields->len() > 0)); }
    };
    
    void print();

    /// props bypass dependency with this operator returning a list of field, which is supported by mx (map would not be)
    operator doubly<field<V>> &() { return data->fields; }
    bool       operator()(mx key) { return (*data)(key); }
    operator               bool() { return mx::mem && *data; }

    template <typename K>  K &get(K k) const { return lookup(k)->value; }
    inline liter<field<V>>     begin() const { return data->fields->begin(); }
    inline liter<field<V>>       end() const { return data->fields->end();   }

    ptr(map, mx, mdata);

    /// when a size is specified to map, it engages hash map mode
    map(size sz) : map() {
        data->fields = doubly<field<V>>();
        if (sz) data->hash_map = new hmap(sz);
    }

    map(size_t sz) : map() {
        data->fields = doubly<field<V>>();
        if (sz) data->hash_map = new hmap(sz);
    }

    map(initial<field<V>> args) : map(size(0)) {
        for(auto &f:args) data->fields += f;
    }

    map(array<field<V>> arr) : map(size(default_hash_size)) {
        for(field<V> &f:arr)  data->fields += f;
    }

    map(doubly<field<V>> li) : map(size(default_hash_size)) {
        for(field<V> &f:li)   data->fields += f;
    }
    
    /// pass through key operator
    template <typename K>
    inline V &operator[](K k) { return (*data)[k]; }
};

template <typename T> struct is_array<array<T>> : std::true_type  {};
template <typename T> struct is_map  <map  <T>> : std::true_type  {};

template <typename T>
map<T> args(int argc, cstr *argv, map<mx> &def) {
    map iargs = map<T>();

    for (int ai = 0; ai < argc; ai++) {
        cstr ps = argv[ai];
        ///
        if (ps[0] == '-') {
            bool   is_single = ps[1] != '-';
            mx key {
                cstr(&ps[is_single ? 1 : 2]), typeof(char)
            };

            field<mx>* found;
            if (is_single) {
                for (field<T> &df: def) {
                    symbol s = symbol(df.key);
                    if (ps[1] == s[0]) {
                        found = &df;
                        break;
                    }
                }
            } else found = def->lookup(key);
            
            if (found) {
                str     aval = str(argv[ai + 1]);
                type_t  type = found->value.type();
                iargs[key]   = type->functions->from_string((cstr)aval.mem->origin);
            } else {
                printf("arg not found: %s\n", str(key.mem).data);
                return null;
            }
        }
    }
    ///
    /// return results in order of defaults, with default value given
    map res = map<T>();
    for(field<T> &df:def.data->fields) {
        field<mx> *ov = iargs.lookup(df.key);
        res.data->fields += { df.key, T(ov ? ov->value : df.value) };
    }
    return res;
}

template <typename T>
struct assigner {
    protected:
    T val;
    lambda<void(bool&)> &on_assign;
    public:

    ///
    inline assigner(T v, lambda<void(bool&)> &fn) : val(v), on_assign(fn) { }

    ///
    inline operator T() { return val; }

    /// the cycle must end [/picard-shoots-picard]
    inline void operator=(T n) {
        val = n; /// probably wont set this
        on_assign(val);
    }
};

using Map = map<mx>;

template <typename T>
class has_count {
    using one = char;
    struct two { char x[2]; };
    template <typename C> static one test( decltype(C::count) ) ;
    template <typename C> static two test(...);    
public:
    enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

template <typename T>
struct states:mx {
    struct fdata {
        type_t type;
        u64    bits;
    };

    /// get the bitmask (1 << value); 0 = 1bit set
    inline static u64 to_flag(u64 v) { return u64(1) << v; }

    ptr(states, mx, fdata);
    
    /// initial states with initial list of values
    inline states(initial<T> args) : states() {
        for (auto  v:args) {
            u64 ord = u64(v); /// from an enum type
            u64 fla = to_flag(ord);
            data->bits |= fla;
        }
    }

    memory *string() const {
        if (!data->bits)
            return memory::symbol("null");

        if constexpr (has_count<T>::value) {
            const size_t sz = 32 * (T::count + 1);
            char format[sz];
            ///
            doubly<memory*> &symbols = T::symbols();
            ///
            u64    v     = data->bits;
            size_t c_id  = 0;
            size_t c_len = 0;
            bool   first = true;
            ///
            for (memory *sym:symbols) {
                if (v & 1 && c_id == sym->id) {
                    char *name = (char*)sym->origin;
                    if (!first)
                        c_len += snprintf(&format[c_len], sz - c_len, ",");
                    else
                        first  = false;
                    c_len += snprintf(&format[c_len], sz - c_len, "%s", name);   
                }
                v = v >> 1;
                c_id++;
            }
            return memory::cstring(format);
        } else {
            assert(false);
            return null;
        }
    }

    template <typename ET>
    inline assigner<bool> operator[](ET varg) const {
        u64 f = 0;
        if constexpr (identical<ET, initial<T>>()) {
            for (auto &v:varg)
                f |= to_flag(u64(v));
        } else
            f = to_flag(u64(varg));
        
        static lambda<void(bool&)> fn {
            [&](bool &from_assign) -> void {
                u64 f = to_flag(from_assign);
                if (from_assign)
                    data->bits |=  f;
                else
                    data->bits &= ~f;
            }
        };
        return { bool((data->bits & f) == f), fn };
    }

    operator memory*() { return grab(); }
};

// just a mere lexical user of cwd
struct dir {
    static fs::path cwd() {
        fs::path p = fs::current_path();
        assert(p != "/");
        return p;
    }
    fs::path prev;
     ///
     dir(fs::path p) : prev(cwd()) { chdir(p   .string().c_str()); }
    ~dir()						   { chdir(prev.string().c_str()); }
};

static void brexit() {
    #ifndef NDEBUG
    breakp();
    #else
    exit(1);
    #endif
}

/// console interface for printing, it could get input as well
struct logger {
    enum option {
        err,
        append
    };

    protected:
    static void _print(const str &st, const array<mx> &ar, const states<option> opts) {
        static std::mutex mtx;
        mtx.lock();
        str msg = st.format(ar);
        std::cout << msg.data << std::endl;
        mtx.unlock();
     ///auto pipe = opts[err] ? stderr : stdout; -- odd thing but stderr/stdout doesnt seem to be where it should be in modules
    }


    public:
    inline void log(mx m, array<mx> ar = {}) {
        str st = m.to_string();
        _print(st, ar, { });
    }

    void test(const bool cond, mx templ = {}, array<mx> ar = {}) {
        #ifndef NDEBUG
        if (!cond) {
            str st = templ.count() ? templ.to_string() : null;
            _print(st, ar, { });
            exit(1);
        }
        #endif
    }

    template <typename T>
    static T input() {
        str      s  = str(size_t(1024)); /// strip integer conversions from constructors here in favor of converting to reserve alone.  that is simple.
        sscanf(s.data, "%s", s.reserve());
        type_t   ty = typeof(T);
        mx       rs = ty->functions->from_string((cstr)s.mem->origin);
        return T(rs);
    }

    inline void fault(mx m, array<mx> ar = array<mx> { }) { str s = m.to_string(); _print(s, ar, { err }); brexit(); }
    inline void error(mx m, array<mx> ar = array<mx> { }) { str s = m.to_string(); _print(s, ar, { err }); brexit(); }
    inline void print(mx m, array<mx> ar = array<mx> { }) { str s = m.to_string(); _print(s, ar, { append }); }
    inline void debug(mx m, array<mx> ar = array<mx> { }) { str s = m.to_string(); _print(s, ar, { append }); }
};

/// define a logger for global use; 'console' can certainly be used as delegate in mx or node, for added context
extern logger console;

/// use char as base.
struct path:mx {
    inline static std::error_code ec;

    using Fn = lambda<void(path)>;
    
    enums(option, recursion,
        "recursion, no-sym-links, no-hidden, use-git-ignores",
         recursion, no_sym_links, no_hidden, use_git_ignores);

    enums(op, none,
        "none, deleted, modified, created",
         none, deleted, modified, created);

    static path cwd() {
        static std::string st;
        st = fs::current_path().string();
        return str(st);
    }
    ///
    ptr(path, mx, fs::path);
    movable(path);

    inline path(str      s) : path(new fs::path(symbol(s.cs()))) { }
    inline path(symbol  cs) : path(new fs::path(cs)) { }

    template <typename T> T     read() const;
    template <typename T> bool write(T input) const;

    str        mime_type();
    str             stem() const { return !data->empty() ? str(data->stem().string()) : str(null);    }
    bool      remove_all() const { std::error_code ec;          return fs::remove_all(*data, ec); }
    bool          remove() const { std::error_code ec;          return fs::remove(*data, ec); }
    bool       is_hidden() const { auto st = data->stem().string(); return st.length() > 0 && st[0] == '.'; }
    bool          exists() const {                              return fs::exists(*data); }
    bool          is_dir() const {                              return fs::is_directory(*data); }
    bool        make_dir() const { std::error_code ec;          return !data->empty() ? fs::create_directories(*data, ec) : false; }
    path remove_filename()       {
        fs::path p = data->remove_filename();
        return path(p);
    }
    bool    has_filename() const {                              return data->has_filename(); }
    path            link() const {                              return fs::is_symlink(*data) ? path(*data) : path(); }
    bool         is_file() const {                              return !fs::is_directory(*data) && fs::is_regular_file(*data); }
    char             *cs() const {
        static std::string static_thing;
        static_thing = data->string();
        return (char*)static_thing.c_str();
    }
    operator cstr() const {
        return cstr(cs());
    }
    str         ext () const { return str(data->extension().string()); }
    str         ext4() const { return data->extension().string(); }
    path        file() const { return fs::is_regular_file(*data) ? path(*data) : path(); }
    bool copy(path to) const {
        assert(!data->empty());
        assert(exists());
        if (!to.exists())
            (to.has_filename() ?
                to.remove_filename() : to).make_dir();

        std::error_code ec;
        fs::copy(*data, *(to.is_dir() ? to / path(str(data->filename().string())) : to), ec);
        return ec.value() == 0;
    }
    
    path &assert_file() {
        assert(fs::is_regular_file(*data) && exists());
        return *this;
    }

    /// create an alias; if successful return its location
    path link(path alias) const {
        fs::path &ap = *data;
        fs::path &bp = *alias;
        if (ap.empty() || bp.empty())
            return {};
        std::error_code ec;
        is_dir() ? fs::create_directory_symlink(ap, bp) : fs::create_symlink(ap, bp, ec);
        return alias.exists() ? alias : path();
    }

    /// operators
    operator        bool()         const { return data->string().length() > 0; }
    operator         str()         const { return str(data->string()); }
    path          parent()         const { return  data->parent_path(); }
    
    path operator / (path       s) const { return path(*data / *s); }
    path operator / (symbol     s) const { return path(*data /  s); }
    path operator / (const str& s) const { return path(*data / fs::path(s.data)); }
    path relative   (path    from) const { return path(fs::relative(*data, *from)); }
    
    bool  operator==(path&      b) const { return  *data == *b; }
    bool  operator!=(path&      b) const { return !(operator==(b)); }
    
    bool  operator==(symbol     b) const { return *data == b; }
    bool  operator!=(symbol     b) const { return !(operator==(b)); }

    ///
    static path uid(path b) {
        auto rand = [](num i) -> str { return rand::uniform('a', 'z'); };
        fs::path p { };
        do { p = fs::path(str::fill(6, rand)); }
        while (fs::exists(*b / p));
        return  p;
    }

    static path  format(str t, array<mx> args) {
        return t.format(args);
    }

    int64_t modified_at() const {
        using namespace std::chrono_literals;
        std::string ps = data->string();
        auto        wt = fs::last_write_time(*data);
        const auto  ms = wt.time_since_epoch().count(); // offset needed atm
        return int64_t(ms);
    }

    bool read(size_t bs, lambda<void(symbol, size_t)> fn) {
        cstr  buf = new char[bs];
        try {
            std::error_code ec;
            size_t rsize = fs::file_size(*data, ec);
            /// dont throw in the towel. no-exceptions.
            if (!ec)
                return false;
            std::ifstream f(*data);
            for (num i = 0, n = (rsize / bs) + (rsize % bs != 0); i < n; i++) {
                size_t sz = i == (n - 1) ? rsize - (rsize / bs * bs) : bs;
                fn((symbol)buf, sz);
            }
        } catch (std::ofstream::failure e) {
            std::cerr << "read failure: " << data->string();
        }
        delete[] buf;
        return true;
    }

    bool append(array<uint8_t> bytes) {
        try {
            size_t sz = bytes.len();
            std::ofstream f(*data, std::ios::out | std::ios::binary | std::ios::app);
            if (sz)
                f.write((symbol)bytes.data, sz);
        } catch (std::ofstream::failure e) {
            std::cerr << "read failure on resource: " << data->string() << std::endl;
        }
        return true;
    }

    bool same_as  (path b) const { std::error_code ec; return fs::equivalent(*data, *b, ec); }

    void resources(array<str> exts, states<option> states, Fn fn) {
        bool use_gitignore	= states[ option::use_git_ignores ];
        bool recursive		= states[ option::recursion       ];
        bool no_hidden		= states[ option::no_hidden       ];
        array<str> ignore   = states[ option::use_git_ignores ] ? path(*data / ".gitignore").read<str>().split("\n") : array<str>();
        lambda<void(path)> res;
        map<mx>    fetched_dir;  /// this is temp and map needs a hash lambdas pronto
        fs::path   parent   = *data; /// parent relative to the git-ignore index; there may be multiple of these things.
        fs::path&  fsp		= *data;
        
        ///
        res = [&](path a) {
            auto fn_filter = [&](path p) -> bool {
                str      ext = ext4();
                bool proceed = false;
                /// proceed if the extension is matching one, or no extensions are given
                for (size_t i = 0; i < exts.len(); i++)
                    if (!exts || ext == (const str)exts[i]) {
                        proceed = !no_hidden || !is_hidden();
                        break;
                    }

                /// proceed if proceeding, and either not using git ignore,
                /// or the file passes the git ignore collection of patterns
                
                if (proceed && use_gitignore) {
                    path    pp = path(parent);
                    path   rel = pp.relative(pp); // original parent, not resource parent
                    str   srel = rel;
                    ///
                    for (str& i: ignore)
                        if (i && srel.has_prefix(i)) {
                            proceed = false;
                            break;
                        }
                }
                
                /// call lambda for resource
                if (proceed) {
                    fn(p);
                    return true;
                }
                return false;
            };

            /// directory of resources
            if (a.is_dir()) {
                if (!no_hidden || !a.is_hidden())
                    for (fs::directory_entry e: fs::directory_iterator(*a)) {
                        path p  = path(e.path());
                        path li = p.link();
                        if (li)
                            continue;
                        path pp = li ? li : p;
                        if (recursive && pp.is_dir()) {
                            if (fetched_dir.lookup(mx(pp)))
                                return;
                            fetched_dir[pp] = mx(true);
                            res(pp);
                        }
                        ///
                        if (pp.is_file())
                            fn_filter(pp);
                    }
            } /// single resource
            else if (a.exists())
                fn_filter(a);
        };
        return res(path(*data));
    }

    array<path> matching(array<str> exts) {
        auto ret = array<path>();
        resources(exts, { }, [&](path p) { ret += p; });
        return ret;
    }

    operator str() { return str(cs(), memory::autolen); }
};

using path_t = path;

i64 millis();

struct base64 {
    static umap<size_t, size_t> &enc_map() {
        static umap<size_t, size_t> mm;
        static bool init;
        if (!init) {
            init = true;
            /// --------------------------------------------------------
            for (size_t i = 0; i < 26; i++) mm[i]          = size_t('A') + size_t(i);
            for (size_t i = 0; i < 26; i++) mm[26 + i]     = size_t('a') + size_t(i);
            for (size_t i = 0; i < 10; i++) mm[26 * 2 + i] = size_t('0') + size_t(i);
            /// --------------------------------------------------------
            mm[26 * 2 + 10 + 0] = size_t('+');
            mm[26 * 2 + 10 + 1] = size_t('/');
            /// --------------------------------------------------------
        }
        return mm;
    }

    static umap<size_t, size_t> &dec_map() {
        static umap<size_t, size_t> m;
        static bool init;
        if (!init) {
            init = true;
            for (int i = 0; i < 26; i++)
                m['A' + i] = i;
            for (int i = 0; i < 26; i++)
                m['a' + i] = 26 + i;
            for (int i = 0; i < 10; i++)
                m['0' + i] = 26 * 2 + i;
            m['+'] = 26 * 2 + 10 + 0;
            m['/'] = 26 * 2 + 10 + 1;
        }
        return m;
    }

    static str encode(symbol data, size_t len) {
        umap<size_t, size_t> &enc = enc_map();
        size_t p_len = ((len + 2) / 3) * 4;
        str encoded(p_len + 1);

        u8    *e = (u8 *)encoded.data;
        size_t b = 0;
        
        for (size_t  i = 0; i < len; i += 3) {
            *(e++)     = u8(enc[data[i] >> 2]);
            ///
            if (i + 1 <= len) *(e++) = u8(enc[((data[i]     << 4) & 0x3f) | data[i + 1] >> 4]);
            if (i + 2 <= len) *(e++) = u8(enc[((data[i + 1] << 2) & 0x3f) | data[i + 2] >> 6]);
            if (i + 3 <= len) *(e++) = u8(enc[  data[i + 2]       & 0x3f]);
            ///
            b += math::min(size_t(4), len - i + 1);
        }
        for (size_t i = b; i < p_len; i++) {
            *(e++) = '=';
            b++;
        }
        *e = 0;
        encoded.mem->count = b; /// str allocs atleast 1 more than size given above
        return encoded;
    }

    static array<u8> decode(symbol b64, size_t b64_len, size_t *alloc_sz) {
        assert(b64_len % 4 == 0);
        /// --------------------------------------
        umap<size_t, size_t> &dec = dec_map();
        *alloc_sz = b64_len / 4 * 3;

        array<u8> out(size_t(*alloc_sz + 1));
        u8     *o = out.data;
        size_t  n = 0;
        size_t  e = 0;
        /// --------------------------------------
        for (size_t ii = 0; ii < b64_len; ii += 4) {
            size_t a[4], w[4];
            for (int i = 0; i < 4; i++) {
                bool is_e = a[i] == '=';
                char   ch = b64[ii + i];
                a[i]      = ch;
                w[i]      = is_e ? 0 : dec[ch];
                if (a[i] == '=')
                    e++;
            }
            u8  b0 = u8(w[0]) << 2 | u8(w[1]) >> 4;
            u8  b1 = u8(w[1]) << 4 | u8(w[2]) >> 2;
            u8  b2 = u8(w[2]) << 6 | u8(w[3]) >> 0;
            if (e <= 2) o[n++] = b0;
            if (e <= 1) o[n++] = b1;
            if (e <= 0) o[n++] = b2;
        }
        assert(n + e == *alloc_sz);
        o[n] = 0;
        return out;
    }
};

struct var:mx {
protected:
    void push(mx v) {        array<mx>((mx*)this).push(v); }
    mx  &last()     { return array<mx>((mx*)this).last();  }

    static mx parse_obj(cstr *start) {
        /// {} is a null state on map<mx>
        map<mx> m_result = map<mx>();

        cstr cur = *start;
        assert(*cur == '{');
        ws(&(++cur));

        /// read this object level, parse_values work recursively with 'cur' updated
        while (*cur != '}') {
            /// parse next field name
            mx field = parse_quoted(&cur).symbolize();

            /// assert field length, skip over the ':'
            ws(&cur);
            assert(field);
            assert(*cur == ':');
            ws(&(++cur));

            /// parse value at newly listed mx value in field, upon requesting it
            *start = cur;
            m_result[field] = parse_value(start);

            cur = *start;
            /// skip ws, continue past ',' if there is another
            if (ws(&cur) == ',')
                ws(&(++cur));
        }

        /// assert we arrived at end of block
        assert(*cur == '}');
        ws(&(++cur));

        *start = cur;
        return m_result;
    }

    /// no longer will store compacted data
    static mx parse_arr(cstr *start) {
        array<mx> a_result = array<mx>();
        cstr cur = *start;
        assert(*cur == '[');
        ws(&(++cur));
        if (*cur == ']')
            ws(&(++cur));
        else {
            for (;;) {
                *start = cur;
                a_result += parse_value(start);
                cur = *start;
                ws(&cur);
                if (*cur == ',') {
                    ws(&(++cur));
                    continue;
                }
                else if (*cur == ']') {
                    ws(&(++cur));
                    break;
                }
                assert(false);
            }
        }
        *start = cur;
        return a_result;
    }

    static void skip_alpha(cstr *start) {
        cstr cur = *start;
        while (*cur && isalpha(*cur))
            cur++;
        *start = cur;
    }

    static mx parse_value(cstr *start) {
        char first_chr = **start;
		bool numeric   =   first_chr == '-' || isdigit(first_chr);

        if (first_chr == '{') {
            return parse_obj(start);
        } else if (first_chr == '[') {
            return parse_arr(start);
        } else if (first_chr == 't' || first_chr == 'f') {
            bool   v = first_chr == 't';
            skip_alpha(start);
            return mx(mx::alloc(&v));
        } else if (first_chr == '"') {
            return parse_quoted(start); /// this updates the start cursor
        } else if (numeric) {
            bool floaty;
            str value = parse_numeric(start, floaty);
            assert(value != "");
            if (floaty) {
                real v = value.real_value<real>();
                return mx::alloc(&v);
            }
            i64 v = value.integer_value();
            return mx::alloc(&v);
        } 

        /// symbol can be undefined or null.  stored as instance of null_t
        skip_alpha(start);
        return memory::alloc(typeof(null_t), 1, 1, null);
    }

    static str parse_numeric(cstr * cursor, bool &floaty) {
        cstr  s = *cursor;
        floaty  = false;
        if (*s != '-' && !isdigit(*s))
            return "";
        ///
        const int max_sane_number = 128;
        cstr      number_start = s;
        ///
        for (++s; ; ++s) {
            if (*s == '.' || *s == 'e' || *s == '-') {
                floaty = true;
                continue;
            }
            if (!isdigit(*s))
                break;
        }
        ///
        size_t number_len = s - number_start;
        if (number_len == 0 || number_len > max_sane_number)
            return null;
        
        /// 
        *cursor = &number_start[number_len];
        return str(number_start, int(number_len));
    }

    /// \\ = \ ... \x = \x
    static str parse_quoted(cstr *cursor) {
        symbol first = *cursor;
        if (*first != '"')
            return "";
        ///
        char         ch = 0;
        bool last_slash = false;
        cstr      start = ++(*cursor);
        str      result { size_t(256) };
        size_t     read = 0;
        ///
        
        for (; (ch = start[read]) != 0; read++) {
            if (ch == '\\')
                last_slash = !last_slash;
            else if (last_slash) {
                switch (ch) {
                    case 'n': ch = '\n'; break;
                    case 'r': ch = '\r'; break;
                    case 't': ch = '\t'; break;
                    ///
                    default:  ch = ' ';  break;
                }
                last_slash = false;
            } else if (ch == '"') {
                read++; /// make sure cursor goes where it should, after quote, after this parse
                break;
            }
            
            if (!last_slash)
                result += ch;
        }
        *cursor = &start[read];
        return result;
    }

    static char ws(cstr *cursor) {
        cstr s = *cursor;
        while (isspace(*s))
            s++;
        *cursor = s;
        if (*s == 0)
            return 0;
        return *s;
    }

    public:

    /// called from path::read<var>()
    static var parse(cstr js) {
        return var(parse_value(&js));
    }

    str stringify() const {
        /// main recursion function
        lambda<str(const mx&)> fn;
        fn = [&](const mx &i) -> str {

            /// used to output specific mx value -- can be any, may call upper recursion
            auto format_v = [&](const mx &e, size_t n_fields) {
                type_t  vt   = e.type();
                memory *smem = e.to_string();
                str     res(size_t(1024));
                assert (smem);

                if (n_fields > 0)
                    res += ",";
                
                if (!e.mem || vt == typeof(null_t))
                    res += "null";
                else if (vt == typeof(char)) {
                    res += "\"";
                    res += str(smem);
                    res += "\"";
                } else if (vt == typeof(int))
                    res += smem;
                else if (vt == typeof(mx) || vt == typeof(map<mx>))
                    res += fn(e);
                else
                    assert(false);
                
                return res;
            };
            ///
            str ar(size_t(1024));
            if (i.type() ==  typeof(map<mx>)) {
                map<mx> m = map<mx>(i);
                ar += "{";
                size_t n_fields = 0;
                for (field<mx> &fx: m) {
                    str skey = str(fx.key.mem->grab());
                    ar += "\"";
                    ar += skey;
                    ar += "\"";
                    ar += ":";
                    ar += format_v(fx.value, n_fields++);
                }
                ar += "}";
            } else if (i.type() == typeof(mx)) {
                mx *mx_p = i.mem->data<mx>(0);
                ar += "[";
                for (size_t ii = 0; ii < i.count(); ii++) {
                    mx &e = mx_p[ii];
                    ar += format_v(e, ii);
                }
                ar += "]";
            } else
                ar += format_v(i, 0);

            return ar;
        };

        return fn(*this);
    }

    /// default constructor constructs map
    var()          : mx(mx::alloc<map<mx>>()) { } /// var poses as different classes.
    var(mx b)      : mx(b.mem->grab()) { }
    var(map<mx> m) : mx(m.mem->grab()) { }

    template <typename T>
    var(const T b) : mx(alloc(&b)) { }

    // abstract between array and map based on the type
    template <typename KT>
    var &operator[](KT key) {
        /// if key is numeric, this must be an array
        type_t kt        = typeof(KT);
        type_t data_type = type();

        if (kt == typeof(char)) {
            /// if key is something else just pass the mx to map
            map<mx>::mdata &dref = mx::mem->ref<map<mx>::mdata>();
            assert(&dref);
            mx skey = mx(key);
            return *(var*)&dref[skey];
        } else if (kt->traits & traits::integral) {    
            assert(data_type == typeof(mx)); // array<mx> or mx
            mx *dref = mx::mem->data<mx>(0);
            assert(dref);
            size_t ix;
                 if (kt == typeof(i32)) ix = size_t(key);
            else if (kt == typeof(u32)) ix = size_t(key);
            else if (kt == typeof(i64)) ix = size_t(key);
            else if (kt == typeof(u64)) ix = size_t(key);
            else {
                console.fault("weird integral type given for indexing var: {0}\n", array<mx> { mx((symbol)kt->name) });
                ix = 0;
            }
            return *(var*)&dref[ix];
        }
        console.fault("incorrect indexing type provided to var");
        return *this;
    }

    static var json(mx i) {
        type_t ty = i.type();
        cstr   cs = null;
        if (ty == typeof(u8))
            cs = cstr(i.data<u8>());
        else if (ty == typeof(i8))
            cs = cstr(i.data<i8>());
        else if (ty == typeof(char))
            cs = cstr(i.data<char>());
        else {
            console.fault("unsupported type: {0}", { str(ty->name) });
            return null;
        }
        return parse(cs);
    }

    memory *string() {
        return stringify().grab(); /// output should be ref count of 1.
    }

    operator str() {
        assert(type() == typeof(char));
        return is_string() ? str(mx::mem->grab()) : str(stringify());
    }

    explicit operator cstr() {
        assert(is_string()); /// this operator should not allocate data
        return mem->data<char>(0);
    }

    operator i64() {
        type_t t = type();
        if (t == typeof(char)) {
            return str(mx::mem->grab()).integer_value();
        } else if (t == typeof(real)) {
            i64 v = i64(ref<real>());
            return v;
        } else if (t == typeof(i64)) {
            i64 v = ref<i64>();
            return v;
        } else if (t == typeof(null_t)) {
            return 0;
        }
        assert(false);
        return 0;
    }

    operator real() {
        type_t t = type();
        if (t == typeof(char)) {
            return str(mx::mem->grab()).real_value<real>();
        } else if (t == typeof(real)) {
            real v = ref<real>();
            return v;
        } else if (t == typeof(i64)) {
            real v = real(ref<i64>());
            return v;
        } else if (t == typeof(null_t)) {
            return 0;
        }
        assert(false);
        return 0;
    }

    operator bool() {
        type_t t = type();
        if (t == typeof(char)) {
            return mx::mem->count > 0;
        } else if (t == typeof(real)) {
            real v = ref<real>();
            return v > 0;
        } else if (t == typeof(i64)) {
            real v = real(ref<i64>());
            return v > 0;
        }
        return false;
    }

    inline liter<field<var>> begin() const { return map<var>(mx::mem->grab())->fields.begin(); }
    inline liter<field<var>>   end() const { return map<var>(mx::mem->grab())->fields.end();   }
};

using FnFuture = lambda<void(mx)>;


template <typename T>
struct sp:mx {
    ptr(sp, mx, T);
};

void         chdir(std::string c);
memory* mem_symbol(::symbol cs, type_t ty, i64 id);
void *  mem_origin(memory *mem);
memory *   cstring(cstr cs, size_t len, size_t reserve, bool is_constant);

template <typename K, typename V>
pair<K,V> *hmap<K, V>::shared_lookup(K input) {
    pair* p = lookup(input);
    return p;
}

/// no lambdas i think just call statics on types.  not a problem there.
/// to call all statics with the proper data pointers is all one needs
/// origin = data pointer [class at 0]
/// 
template <typename T>
idata *ident::for_type() {
    /// get string name of class (works on win, mac and linux)
    static auto parse_fn = [](std::string cn) -> cstr {
        std::string      st = is_win() ? "<"  : "T =";
        std::string      en = is_win() ? ">(" : "]";
        num		         p  = cn.find(st) + st.length();
        num              ln = cn.find(en, p) - p;
        std::string      nm = cn.substr(p, ln);
        auto             sp = nm.find(' ');
        std::string   namco = (sp != std::string::npos) ? nm.substr(sp + 1) : nm;
        return util::copy(namco.c_str());
    };
    ///
    static idata *type;

    if (!type) {
        ///
        if constexpr (std::is_function_v<T>) {
            memory         *mem = memory::raw_alloc(null, sizeof(idata), 1, 1);
            type                = (idata*)mem_origin(mem);
            type->src           = type;
            type->name          = parse_fn(__PRETTY_FUNCTION__);
            type->base_sz       = sizeof(T);
        } else if constexpr (!type_complete<T> || is_opaque<T>()) {
            /// minimal processing on 'opaque'; certain std design-time calls blow up the vulkan types
            memory         *mem = memory::raw_alloc(null, sizeof(idata), 1, 1);
            type                = (idata*)mem_origin(mem);
            type->src           = type;
            type->name          = parse_fn(__PRETTY_FUNCTION__);
            type->base_sz       = sizeof(T*); /// must retain a flag of sort
            type->traits        = traits::opaque;
        } else if constexpr (std::is_const    <T>::value) return ident::for_type<std::remove_const_t    <T>>();
          else if constexpr (std::is_reference<T>::value) return ident::for_type<std::remove_reference_t<T>>();
          else if constexpr (std::is_pointer  <T>::value) return ident::for_type<std::remove_pointer_t  <T>>();
          else {
            bool is_mx = ion::is_mx<T>();
            cstr curious0 = parse_fn(__PRETTY_FUNCTION__);
            memory *mem  = memory::raw_alloc(null, sizeof(idata), 1, 1);
            type         = (idata*)mem_origin(mem);
            type->name    = parse_fn(__PRETTY_FUNCTION__);
            type->base_sz = sizeof(T);
            type->traits  = (is_primitive<T> () ? traits::primitive : 0) |
                            (is_integral <T> () ? traits::integral  : 0) |
                            (is_realistic<T> () ? traits::realistic : 0) | // if references radioshack catalog
                            (is_singleton<T> () ? traits::singleton : 0) |
                            (is_mx              ? traits::mx        : 0);
            /// a bit more open ended, and common apis (return type must be checked as well)            
            type->functions = (ops<void>*)ftable<T>();
            ///
            if constexpr (!is_array<T>() && !identical<ion::mx, T>() && inherits<ion::mx, T>()) {
                schema_populate(type, (T*)null);
            }
        }
    }
    return type_t(type);
}

template <typename T>
T *memory::data(size_t index) const {
    if constexpr (type_complete<T>) {
        type_t queried_type = ident::for_type<T>();
        size_t mxc          = math::max(reserve, count);
        static type_t mx_t  = typeof(mx);
        alloc_schema *schema = type->schema;
        if (queried_type != type && schema) { // dont insert schema for mx type duh
            size_t offset = 0;
            for (size_t i = 0; i < schema->bind_count; i++) {
                context_bind &c = schema->composition[i];
                if (c.data == queried_type)
                    return (T*)&cstr(origin)[c.offset * mxc + c.data->base_sz * index];
            }
            console.fault("type not found in schema: {0}", { str(queried_type->name) });
            return (T*)null;
        } else
            return (T*)origin + index;
    } else {
        assert(index == 0);
        return (T*)origin;
    }
}

template <typename T>
size_t ident::type_size() {
    static type_t type    = typeof(T);
    if constexpr (type_complete<T>) {
        static size_t type_sz = std::is_pointer<T>::value ? sizeof(T) : 
            (type->schema && type->schema->total_bytes)
        ? type->schema->total_bytes : type->base_sz;
        return type_sz;
    } else {
        return sizeof(T*);
    }
}

template <typename T>
ops<T> *ftable() {
    static ops<T> gen;
    if constexpr (registered_free<T>()) gen.free = FreeFn<T>(free);
    return (ops<T>*)&gen;
}

template <typename V>
void map<V>::print() {
    for (field<V> &f: *this) {
        str k = str(f.key.grab());
        str v = str(f.value.grab());
        console.log("key: {0}, value: {1}", { k, v });
    }
}

template <typename K, typename V>
V &hmap<K,V>::operator[](K input) {
    u64 k;
    
    /// lookup a pointer to V
    using ldata = typename doubly<pair>::ldata;
    ldata *b = null;
    V*  pv = lookup(input, &k, &b); /// this needs to lock in here
    if (pv) return *pv;
    
    /// default allocation on V; here we need constexpr switch on V type, res
    if        constexpr (std::is_integral<V>()) {
        *b += pair { input, V(0) };
    } else if constexpr (std::is_pointer<V>()) {
        *b += pair { input, V(null) };
    } else if constexpr (std::is_default_constructible<V>()) {
        *b += pair { input, V() };
    } else
        static_assert("hash V-type is not default initializable (and you Know it must be!)");
    
    V &result = b->last().value;
    return result;
}

template <typename K, typename V>
V* hmap<K,V>::lookup(K input, u64 *pk, bucket **pbucket) const {
    u64 k = hash_index(input, data->sz);
    if (pk) *pk  =   k;
    bucket &hist = (*data)[k];
    
    for (pair &p: hist)
        if (p.key == input) {
            if (pbucket) *pbucket = &hist;
            return &p.value;
        }
    if (pbucket) *pbucket = &hist;
    return null;
}

template <typename T>
bool path::write(T input) const {
    if (!fs::is_regular_file(*data))
        return false;

    if constexpr (is_array<T>()) {
        std::ofstream f(*data, std::ios::out | std::ios::binary);
        if (input.len() > 0)
            f.write((symbol)input.data, input.data_type()->base_sz * input.len());
    } else if constexpr (identical<T, var>()) {
        str    v  = input.stringify(); /// needs to escape strings
        std::ofstream f(*data, std::ios::out | std::ios::binary);
        if (v.len() > 0)
            f.write((symbol)v.data, v.len());
    } else if constexpr (identical<T,str>()) {
        std::ofstream f(*data, std::ios::out | std::ios::binary);
        if (input.len() > 0)
            f.write((symbol)input.cs(), input.len());
    } else {
        console.log("path conversion to {0} is not implemented. do it, im sure it wont take long.", { mx(typeof(T)->name) });
        return false;
    }
    return true;
}

template<typename T, typename = void>
struct has_int_conversion : std::false_type {};

template<typename T>
struct has_int_conversion<T, std::void_t<decltype(static_cast<int>(std::declval<T>()))>> : std::true_type {};


/// use-case: any kind of file [Joey]
template <typename T>
T path::read() const {
    const bool nullable = std::is_constructible<T, std::nullptr_t>::value;
    const bool integral = !has_int_conversion<T>::value || std::is_integral<T>::value;
    
    if (!fs::is_regular_file(*data)) {
        if constexpr (nullable)
            return null;
        else if constexpr (integral)
            return 0;
        else
            return T();
    }

    if constexpr (identical<T, array<u8>>()) {
        std::ifstream input(*data, std::ios::binary);
        std::vector<char> buffer(std::istreambuf_iterator<char>(input), { });
        return array<u8>((u8*)buffer.data(), buffer.size());
    } else {
        std::ifstream fs;
        fs.open(*data);
        std::ostringstream sstr;
        sstr << fs.rdbuf();
        fs.close();
        std::string st = sstr.str();

        if constexpr (identical<T, str>()) {
            return str((cstr )st.c_str(), int(st.length()));
        } else if constexpr (inherits<T, var>()) {
            return var::parse(cstr(st.c_str()));
        } else {
            console.fault("not implemented");
            if constexpr (nullable)
                return null;
            else if constexpr (integral)
                return 0;
            else
                return T();
        }
    }
}

}