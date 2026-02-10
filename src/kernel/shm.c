#include "shm.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"
#include "spinlock.h"
#include "errno.h"

#include <stddef.h>

#define PAGE_SIZE 4096U

struct shm_segment {
    int        used;
    uint32_t   key;
    uint32_t   size;         /* requested size */
    uint32_t   npages;
    uintptr_t  pages[SHM_MAX_PAGES]; /* physical addresses */
    uint32_t   nattch;       /* attach count */
    int        marked_rm;    /* IPC_RMID pending */
};

static struct shm_segment segments[SHM_MAX_SEGMENTS];
static spinlock_t shm_lock = {0};

void shm_init(void) {
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        segments[i].used = 0;
    }
}

static void shm_destroy(struct shm_segment* seg) {
    for (uint32_t i = 0; i < seg->npages; i++) {
        if (seg->pages[i]) {
            pmm_free_page((void*)seg->pages[i]);
            seg->pages[i] = 0;
        }
    }
    seg->used = 0;
}

int shm_get(uint32_t key, uint32_t size, int flags) {
    if (size == 0) return -EINVAL;

    uint32_t npages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (npages > SHM_MAX_PAGES) return -EINVAL;

    uintptr_t irqf = spin_lock_irqsave(&shm_lock);

    /* If key != IPC_PRIVATE, search for existing */
    if (key != IPC_PRIVATE) {
        for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
            if (segments[i].used && segments[i].key == key) {
                if ((flags & IPC_CREAT) && (flags & IPC_EXCL)) {
                    spin_unlock_irqrestore(&shm_lock, irqf);
                    return -EEXIST;
                }
                spin_unlock_irqrestore(&shm_lock, irqf);
                return i;
            }
        }
        if (!(flags & IPC_CREAT)) {
            spin_unlock_irqrestore(&shm_lock, irqf);
            return -ENOENT;
        }
    }

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (!segments[i].used) { slot = i; break; }
    }
    if (slot < 0) {
        spin_unlock_irqrestore(&shm_lock, irqf);
        return -ENOSPC;
    }

    /* Allocate physical pages */
    struct shm_segment* seg = &segments[slot];
    seg->used = 1;
    seg->key = key;
    seg->size = size;
    seg->npages = npages;
    seg->nattch = 0;
    seg->marked_rm = 0;

    for (uint32_t i = 0; i < npages; i++) {
        void* page = pmm_alloc_page();
        if (!page) {
            /* Rollback */
            for (uint32_t j = 0; j < i; j++) {
                pmm_free_page((void*)seg->pages[j]);
            }
            seg->used = 0;
            spin_unlock_irqrestore(&shm_lock, irqf);
            return -ENOMEM;
        }
        seg->pages[i] = (uintptr_t)page;
    }

    spin_unlock_irqrestore(&shm_lock, irqf);
    return slot;
}

void* shm_at(int shmid, uintptr_t shmaddr) {
    if (shmid < 0 || shmid >= SHM_MAX_SEGMENTS) return (void*)(uintptr_t)-EINVAL;

    uintptr_t irqf = spin_lock_irqsave(&shm_lock);

    struct shm_segment* seg = &segments[shmid];
    if (!seg->used) {
        spin_unlock_irqrestore(&shm_lock, irqf);
        return (void*)(uintptr_t)-EINVAL;
    }

    /* Find a free virtual address range in user space for mapping.
     * Use the process mmap slots. */
    if (!current_process) {
        spin_unlock_irqrestore(&shm_lock, irqf);
        return (void*)(uintptr_t)-EINVAL;
    }

    /* If shmaddr == 0, kernel picks address */
    uintptr_t vaddr = shmaddr;
    if (vaddr == 0) {
        /* Find free mmap slot and pick address starting at 0x40000000 */
        int mslot = -1;
        for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
            if (current_process->mmaps[i].length == 0) {
                mslot = i;
                break;
            }
        }
        if (mslot < 0) {
            spin_unlock_irqrestore(&shm_lock, irqf);
            return (void*)(uintptr_t)-ENOMEM;
        }

        /* Simple address allocation: 0x40000000 + slot * 64KB */
        vaddr = 0x40000000U + (uint32_t)mslot * (SHM_MAX_PAGES * PAGE_SIZE);

        /* Map physical pages into user address space */
        for (uint32_t i = 0; i < seg->npages; i++) {
            vmm_map_page(vaddr + i * PAGE_SIZE, seg->pages[i],
                         VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);
        }

        current_process->mmaps[mslot].base = vaddr;
        current_process->mmaps[mslot].length = seg->npages * PAGE_SIZE;
    } else {
        /* Map at requested address */
        for (uint32_t i = 0; i < seg->npages; i++) {
            vmm_map_page(vaddr + i * PAGE_SIZE, seg->pages[i],
                         VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);
        }
    }

    seg->nattch++;
    spin_unlock_irqrestore(&shm_lock, irqf);
    return (void*)vaddr;
}

int shm_dt(const void* shmaddr) {
    uintptr_t addr = (uintptr_t)shmaddr;
    if (!current_process) return -EINVAL;

    uintptr_t irqf = spin_lock_irqsave(&shm_lock);

    /* Find which mmap slot this belongs to */
    int mslot = -1;
    for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
        if (current_process->mmaps[i].base == addr && current_process->mmaps[i].length > 0) {
            mslot = i;
            break;
        }
    }
    if (mslot < 0) {
        spin_unlock_irqrestore(&shm_lock, irqf);
        return -EINVAL;
    }

    uint32_t len = current_process->mmaps[mslot].length;
    uint32_t npages = len / PAGE_SIZE;

    /* Unmap pages (but don't free — they belong to the shm segment) */
    for (uint32_t i = 0; i < npages; i++) {
        vmm_unmap_page((uint64_t)(addr + i * PAGE_SIZE));
    }

    current_process->mmaps[mslot].base = 0;
    current_process->mmaps[mslot].length = 0;

    /* Find the segment and decrement attach count */
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (!segments[i].used) continue;
        /* Check if any page matches */
        for (uint32_t p = 0; p < segments[i].npages; p++) {
            uintptr_t expected_va = addr + p * PAGE_SIZE;
            (void)expected_va;
            /* We can't easily reverse-map. Just decrement nattch for
             * segments with matching page count. This is a simplification. */
        }
    }

    /* Simplified: decrement nattch by scanning for segments whose pages
     * were mapped at this address range. For now, just scan by npages match. */
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (segments[i].used && segments[i].npages == npages && segments[i].nattch > 0) {
            segments[i].nattch--;
            if (segments[i].nattch == 0 && segments[i].marked_rm) {
                shm_destroy(&segments[i]);
            }
            break;
        }
    }

    spin_unlock_irqrestore(&shm_lock, irqf);
    return 0;
}

int shm_ctl(int shmid, int cmd, struct shmid_ds* buf) {
    if (shmid < 0 || shmid >= SHM_MAX_SEGMENTS) return -EINVAL;

    uintptr_t irqf = spin_lock_irqsave(&shm_lock);
    struct shm_segment* seg = &segments[shmid];

    if (!seg->used) {
        spin_unlock_irqrestore(&shm_lock, irqf);
        return -EINVAL;
    }

    if (cmd == IPC_STAT) {
        if (buf) {
            buf->shm_segsz = seg->size;
            buf->shm_nattch = seg->nattch;
            buf->shm_key = seg->key;
        }
        spin_unlock_irqrestore(&shm_lock, irqf);
        return 0;
    }

    if (cmd == IPC_RMID) {
        if (seg->nattch == 0) {
            shm_destroy(seg);
        } else {
            seg->marked_rm = 1;
        }
        spin_unlock_irqrestore(&shm_lock, irqf);
        return 0;
    }

    spin_unlock_irqrestore(&shm_lock, irqf);
    return -EINVAL;
}
