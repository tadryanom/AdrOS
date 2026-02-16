#include "heap.h"
#include "console.h"
#include "pmm.h"
#include "vmm.h"
#include "spinlock.h"
#include "utils.h"
#include "hal/cpu.h"
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Buddy Allocator                                                    */
/*                                                                    */
/* Power-of-2 block sizes from 2^MIN_ORDER (32B) to 2^MAX_ORDER (8MB)*/
/* O(log N) alloc/free with automatic buddy coalescing.               */
/* Each block carries an 8-byte header; free blocks embed list ptrs   */
/* in their data area.                                                */
/* ------------------------------------------------------------------ */

#define KHEAP_START       0xD0000000U

#define BUDDY_MIN_ORDER   5                                   /* 32 B  */
#define BUDDY_MAX_ORDER   23                                  /* 8 MB  */
#define BUDDY_NUM_ORDERS  (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1)
#define BUDDY_HEAP_SIZE   (1U << BUDDY_MAX_ORDER)

#define BUDDY_MAGIC       0xBD00CAFEU

/* Block header — always at the start of every block (free or alloc) */
typedef struct block_hdr {
    uint32_t magic;
    uint8_t  order;       /* 5..23 */
    uint8_t  is_free;     /* 1 = free, 0 = allocated */
    uint16_t pad;
    uint32_t pad2[2];     /* Pad to 16 bytes for 16-byte aligned returns */
} block_hdr_t;            /* 16 bytes → FXSAVE-safe alignment */

/* Free-list node, embedded in the data area of a free block */
typedef struct free_node {
    struct free_node* next;
    struct free_node* prev;
} free_node_t;

/* Sentinel-based circular doubly-linked free lists, one per order */
static free_node_t free_lists[BUDDY_NUM_ORDERS];

static spinlock_t heap_lock = {0};

/* ---- Inline helpers ---- */

static inline free_node_t* blk_to_fn(block_hdr_t* h) {
    return (free_node_t*)((uint8_t*)h + sizeof(block_hdr_t));
}
static inline block_hdr_t* fn_to_blk(free_node_t* fn) {
    return (block_hdr_t*)((uint8_t*)fn - sizeof(block_hdr_t));
}

static inline void fl_init(free_node_t* s)  { s->next = s; s->prev = s; }
static inline int  fl_empty(free_node_t* s) { return s->next == s; }

static inline void fl_add(free_node_t* s, free_node_t* n) {
    n->next = s->next;
    n->prev = s;
    s->next->prev = n;
    s->next = n;
}
static inline void fl_del(free_node_t* n) {
    n->prev->next = n->next;
    n->next->prev = n->prev;
}
static inline free_node_t* fl_pop(free_node_t* s) {
    free_node_t* n = s->next;
    fl_del(n);
    return n;
}

/* Buddy address via XOR on the offset from heap start */
static inline block_hdr_t* buddy_of(block_hdr_t* b, int order) {
    uintptr_t off = (uintptr_t)b - KHEAP_START;
    return (block_hdr_t*)(KHEAP_START + (off ^ (1U << order)));
}

/* Minimum order that can hold `size` user bytes (+ header) */
static inline int size_to_order(size_t size) {
    size_t total = size + sizeof(block_hdr_t);
    int order = BUDDY_MIN_ORDER;
    while ((1U << order) < total && order < BUDDY_MAX_ORDER)
        order++;
    return order;
}

/* ---- Public API ---- */

void kheap_init(void) {
    kprintf("[HEAP] Initializing Buddy Allocator...\n");

    uintptr_t flags = spin_lock_irqsave(&heap_lock);

    for (int i = 0; i < BUDDY_NUM_ORDERS; i++)
        fl_init(&free_lists[i]);

    /* Map physical pages for the 8 MB heap region */
    uint32_t pages = BUDDY_HEAP_SIZE / PAGE_SIZE;
    uintptr_t va = KHEAP_START;
    for (uint32_t i = 0; i < pages; i++) {
        void* phys = pmm_alloc_page();
        if (!phys) {
            spin_unlock_irqrestore(&heap_lock, flags);
            kprintf("[HEAP] OOM during init!\n");
            return;
        }
        vmm_map_page((uint64_t)(uintptr_t)phys, (uint64_t)va,
                     VMM_FLAG_PRESENT | VMM_FLAG_RW);
        va += PAGE_SIZE;
    }

    /* Single free block spanning the whole heap */
    block_hdr_t* root = (block_hdr_t*)KHEAP_START;
    root->magic   = BUDDY_MAGIC;
    root->order   = BUDDY_MAX_ORDER;
    root->is_free = 1;
    root->pad     = 0;
    fl_add(&free_lists[BUDDY_MAX_ORDER - BUDDY_MIN_ORDER], blk_to_fn(root));

    spin_unlock_irqrestore(&heap_lock, flags);
    kprintf("[HEAP] 8MB Buddy Allocator Ready.\n");
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    int order = size_to_order(size);
    if (order > BUDDY_MAX_ORDER) return NULL;

    uintptr_t flags = spin_lock_irqsave(&heap_lock);

    /* Find the smallest available order >= requested */
    int k;
    for (k = order; k <= BUDDY_MAX_ORDER; k++) {
        if (!fl_empty(&free_lists[k - BUDDY_MIN_ORDER]))
            break;
    }

    if (k > BUDDY_MAX_ORDER) {
        spin_unlock_irqrestore(&heap_lock, flags);
        kprintf("[HEAP] OOM: kmalloc failed.\n");
        return NULL;
    }

    /* Remove the block from its free list */
    free_node_t* fn = fl_pop(&free_lists[k - BUDDY_MIN_ORDER]);
    block_hdr_t* blk = fn_to_blk(fn);

    if (blk->magic != BUDDY_MAGIC || !blk->is_free) {
        spin_unlock_irqrestore(&heap_lock, flags);
        kprintf("[HEAP] Corruption in kmalloc!\n");
        for (;;) hal_cpu_idle();
    }

    /* Split down to the required order */
    while (k > order) {
        k--;
        /* Upper buddy */
        block_hdr_t* buddy = (block_hdr_t*)((uintptr_t)blk + (1U << k));
        buddy->magic   = BUDDY_MAGIC;
        buddy->order   = (uint8_t)k;
        buddy->is_free = 1;
        buddy->pad     = 0;
        fl_add(&free_lists[k - BUDDY_MIN_ORDER], blk_to_fn(buddy));

        blk->order = (uint8_t)k;
    }

    blk->is_free = 0;

    spin_unlock_irqrestore(&heap_lock, flags);
    return (void*)((uint8_t*)blk + sizeof(block_hdr_t));
}

void kfree(void* ptr) {
    if (!ptr) return;

    uintptr_t flags = spin_lock_irqsave(&heap_lock);

    block_hdr_t* blk = (block_hdr_t*)((uint8_t*)ptr - sizeof(block_hdr_t));

    if (blk->magic != BUDDY_MAGIC) {
        spin_unlock_irqrestore(&heap_lock, flags);
        kprintf("[HEAP] Corruption in kfree! (bad magic)\n");
        kprintf("[HEAP] hdr=0x%x magic=0x%x\n",
                (unsigned)(uintptr_t)blk, (unsigned)blk->magic);
        for (;;) hal_cpu_idle();
    }

    if (blk->is_free) {
        spin_unlock_irqrestore(&heap_lock, flags);
        kprintf("[HEAP] Double free!\n");
        for (;;) hal_cpu_idle();
    }

    blk->is_free = 1;
    int order = blk->order;

    /* Coalesce with buddy while possible */
    while (order < BUDDY_MAX_ORDER) {
        block_hdr_t* buddy = buddy_of(blk, order);

        /* Buddy must be valid, free, and at the same order */
        if (buddy->magic != BUDDY_MAGIC || !buddy->is_free ||
            buddy->order != (uint8_t)order)
            break;

        /* Remove buddy from its free list */
        fl_del(blk_to_fn(buddy));

        /* Merged block starts at the lower address */
        if ((uintptr_t)buddy < (uintptr_t)blk)
            blk = buddy;

        order++;
        blk->order = (uint8_t)order;
    }

    /* Insert the (possibly merged) block into the correct free list */
    fl_add(&free_lists[order - BUDDY_MIN_ORDER], blk_to_fn(blk));

    spin_unlock_irqrestore(&heap_lock, flags);
}
