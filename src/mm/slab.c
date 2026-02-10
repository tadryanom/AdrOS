#include "slab.h"
#include "pmm.h"
#include "uart_console.h"

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
    void* page = pmm_alloc_page();
    if (!page) return -1;

    uint8_t* base = (uint8_t*)(uintptr_t)page;

    /* In higher-half kernel the physical page needs to be accessible.
     * For simplicity we assume the kernel heap region or identity-mapped
     * low memory is used. We map via the kernel virtual address. */
    /* TODO: For pages above 4MB, a proper kernel mapping is needed.
     * For now, slab pages come from pmm_alloc_page which returns
     * physical addresses. We need to convert to virtual. */

    /* Use kernel virtual = phys + 0xC0000000 for higher-half */
    uint8_t* vbase = base + 0xC0000000U;

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
