#include <io/io.h>
#include <stdbool.h>
#include <stdlib.h>

/// io does not interop with mx and i dont think thats a useful 
/// thing to do, but it is lifecycle on C without placing member
/// structs.  works with or without initializing its mutices for sync mode

void _io_sync_enable(io *mem, bool sync) {
	if (sync) {
        if (!mem->sync) {
            mem->sync = &((mtx_t*)mem)[-1]; /// the space is always allocated but sync not always set to its pointer and initialized as mtx_recursive
            mtx_init(mem->sync, mtx_recursive);
        }
    } else {
        if (!mem->sync) {
            mtx_destroy(mem->sync);
            mem->sync = (mtx_t*)NULL;
        }
    }
}

void* _io_init(io *mem, DestroyFn destroy_fn, bool sync) {
    _io_sync_enable(mem, sync);
	mem->refs       = 1;
	mem->destroy_fn = destroy_fn;
    return &mem[1]; /// return user data
}


void* _io_grab(io *mem) {
    if (mem->sync) {
        mtx_lock(mem->sync);
        mem->refs++;
        mtx_unlock(mem->sync);
    } else
        mem->refs++;
	return &mem[1];
}

void* _io_drop(io *mem) {
    if (mem->sync) { /// the only type used in sync
        mtx_lock(mem->sync);
        mem->refs--;
        mtx_unlock(mem->sync);
    } else
        mem->refs--;
    ///
	if (mem->refs == 0) {
        if  (mem->destroy_fn)
		     mem->destroy_fn((void*)mem);
        if  (mem->sync) mtx_destroy(mem->sync);
        free(mem);
		return NULL;
    }
	return &mem[1];
}
