#ifndef _io_h_
#define _io_h_

//#include <tinycthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <memory.h>

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