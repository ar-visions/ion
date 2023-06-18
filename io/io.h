#ifndef _io_h_
#define _io_h_

#include <tinycthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <memory.h>

/// AR.. here be some fine types. #not-a-real-sailor
typedef signed char       i8;
typedef signed short      i16;
typedef signed int        i32;
typedef long long         i64;
typedef unsigned char     u8;
typedef unsigned short    u16;
typedef unsigned int      u32;
typedef unsigned long long u64;
typedef unsigned long     ulong;
typedef float             r32;
typedef double            r64;
typedef r64               real;
typedef char*             cstr;
typedef const char*       symbol;
typedef const char        cchar_t;
// typedef i64            ssize_t; // ssize_t is typically predefined in sys/types.h
typedef i64               num;
typedef void*             handle_t;

/// io is an allocator with reference counts
/// io allocates what it needs at the head including mtx_t
/// the pointer of which is a bool indicating its in sync
typedef void (*DestroyFn)(void*);

typedef struct io_t {
    mtx_t      *sync;
	int   	    refs;
    int         reserve;
    int         count;
	DestroyFn   destroy_fn;
} io;

void *_io_init(io *object, DestroyFn destroy_fn, bool sync);
void *_io_grab(io *object);
void *_io_drop(io *object);
///
void  _io_sync_enable(io *object, bool sync);

#define io_addr(user) \
    &((io *)user)[-1]

#define io_sync(user) ({\
        io *addr = io_addr(user);\
        if (addr) mtx_lock(addr->sync);\
        user;\
    });

#define io_unsync(user) ({\
        io *addr = io_addr(user);\
        if (addr) mtx_unlock(addr->sync);\
        user;\
    });

#define io_sync_enable(user, sync) \
    _io_sync_enable(io_addr(user), sync)

#define  io_grab(type, user) (type*)_io_grab (io_addr(user))
#define  io_drop(type, user) (type*)_io_drop (io_addr(user))
#define  io_refs(type, user)                 (io_addr(user))->refs
#define io_alloc(T) ({ \
    T *var = (T*)&calloc(1, sizeof(mtx_t) + sizeof(io) + sizeof(T))[sizeof(mtx_t) + sizeof(io)]; \
    static void *var ## _grab_proc = (void*)T ## _grab; \
    static void *var ## _drop_proc = (void*)T ## _drop; \
    io*   head   =  io_addr(var); \
	T    *result = _io_init(head, (void*)T ## _free, false); \
    result; \
})

#define io_new(T) ({ \
    T *var = (T*)&calloc(1, sizeof(mtx_t) + sizeof(io) + sizeof(T))[sizeof(mtx_t) + sizeof(io)]; \
    static void *var ## _grab_proc = (void*)T ## _grab; \
    static void *var ## _drop_proc = (void*)T ## _drop; \
    io*   head   =  io_addr(var); \
	T    *result = _io_init(head, (void*)T ## _free, true); \
    result; \
})

#endif