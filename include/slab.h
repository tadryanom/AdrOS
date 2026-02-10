#ifndef SLAB_H
#define SLAB_H

#include <stdint.h>
#include <stddef.h>
#include "spinlock.h"

typedef struct slab_cache {
    const char*  name;
    uint32_t     obj_size;
    uint32_t     objs_per_slab;
    void*        free_list;
    uint32_t     total_allocs;
    uint32_t     total_frees;
    spinlock_t   lock;
} slab_cache_t;

void slab_cache_init(slab_cache_t* cache, const char* name, uint32_t obj_size);
void* slab_alloc(slab_cache_t* cache);
void  slab_free(slab_cache_t* cache, void* obj);

#endif
