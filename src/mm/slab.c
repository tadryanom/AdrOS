#include "slab.h"
#include "pmm.h"
#include "heap.h"
#include "hal/mm.h"
#include "console.h"

#include <stddef.h>

struct slab_free_node {
    struct slab_free_node* next;
};

void slab_cache_init(slab_cache_t* cache, const char* name, uint32_t obj_size) {
    if (!cache) return;
    cache->name = name;
    if (obj_size < sizeof(struct slab_free_node)) {
        obj_size = sizeof(struct slab_free_node);
    }
    cache->obj_size = (obj_size + 7U) & ~7U;
    cache->objs_per_slab = PAGE_SIZE / cache->obj_size;
    cache->free_list = NULL;
    cache->total_allocs = 0;
    cache->total_frees = 0;
    spinlock_init(&cache->lock);
}

static int slab_grow(slab_cache_t* cache) {
    /* Allocate from the kernel heap instead of raw pmm_alloc_page.
     * The heap is already VMM-mapped at valid kernel VAs, so we avoid
     * the hal_mm_phys_to_virt bug where phys addresses above 16MB
     * translate to VAs that collide with the heap range (0xD0000000+). */
    uint8_t* vbase = (uint8_t*)kmalloc(PAGE_SIZE);
    if (!vbase) return -1;

    for (uint32_t i = 0; i < cache->objs_per_slab; i++) {
        struct slab_free_node* node = (struct slab_free_node*)(vbase + i * cache->obj_size);
        node->next = (struct slab_free_node*)cache->free_list;
        cache->free_list = node;
    }

    return 0;
}

void* slab_alloc(slab_cache_t* cache) {
    if (!cache) return NULL;

    uintptr_t flags = spin_lock_irqsave(&cache->lock);

    if (!cache->free_list) {
        if (slab_grow(cache) < 0) {
            spin_unlock_irqrestore(&cache->lock, flags);
            return NULL;
        }
    }

    struct slab_free_node* node = (struct slab_free_node*)cache->free_list;
    cache->free_list = node->next;
    cache->total_allocs++;

    spin_unlock_irqrestore(&cache->lock, flags);

    return (void*)node;
}

void slab_free(slab_cache_t* cache, void* obj) {
    if (!cache || !obj) return;

    uintptr_t flags = spin_lock_irqsave(&cache->lock);

    struct slab_free_node* node = (struct slab_free_node*)obj;
    node->next = (struct slab_free_node*)cache->free_list;
    cache->free_list = node;
    cache->total_frees++;

    spin_unlock_irqrestore(&cache->lock, flags);
}
