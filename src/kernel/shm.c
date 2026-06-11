// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "shm.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"
#include "spinlock.h"
#include "uaccess.h"
#include "errno.h"
#include "utils.h"
#include "hal/mm.h"

#include <stddef.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096U
#endif

struct shm_segment {
    int        used;
    uint32_t   key;
    uint32_t   size;         /* requested size */
    uint32_t   npages;
    uintptr_t  pages[SHM_MAX_PAGES]; /* physical addresses */
    uint32_t   nattch;       /* attach count */
    int        marked_rm;    /* IPC_RMID pending */
    /* K14: Permission metadata */
    uint32_t   uid;
    uint32_t   gid;
    uint32_t   mode;
};

static struct shm_segment segments[SHM_MAX_SEGMENTS];
static spinlock_t shm_lock = {0};

void shm_init(void) {
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        memset(&segments[i], 0, sizeof(segments[i]));
    }
}

static void shm_destroy(struct shm_segment* seg) {
    for (uint32_t i = 0; i < seg->npages; i++) {
        if (seg->pages[i]) {
            pmm_free_page((void*)seg->pages[i]);
        }
    }
    memset(seg, 0, sizeof(*seg));
}

/* Check POSIX permission for SHM segment */
static int shm_perm_check(struct shm_segment* seg, int perm) {
    if (!current_process) return 0;
    if (current_process->euid == 0) return 1;  /* Root can do anything */

    uint32_t mode = seg->mode;
    uint32_t uid = seg->uid;
    uint32_t gid = seg->gid;

    /* Owner permissions */
    if (current_process->uid == uid || current_process->euid == uid) {
        return (mode & (perm << 6)) != 0;
    }

    /* Group permissions */
    if (current_process->gid == gid || current_process->egid == gid) {
        return (mode & (perm << 3)) != 0;
    }

    /* Other permissions */
    return (mode & perm) != 0;
}

int shm_get(uint32_t key, uint32_t size, int flags) {
    if (size == 0) return -EINVAL;

    uint32_t npages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (npages > SHM_MAX_PAGES) return -EINVAL;

    uintptr_t irqf = spin_lock_irqsave(&shm_lock);

    /* If key != IPC_PRIVATE, search for existing */
    if (key != IPC_PRIVATE) {
        for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
            if (segments[i].used && segments[i].key == key && !segments[i].marked_rm) {
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
    /* K14: Initialize permission metadata */
    seg->uid = current_process ? current_process->uid : 0;
    seg->gid = current_process ? current_process->gid : 0;
    seg->mode = 0600;  /* rw------- by default */

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

void* shm_at(int shmid, uintptr_t shmaddr, int shmflg) {
    if (shmid < 0 || shmid >= SHM_MAX_SEGMENTS) return (void*)(uintptr_t)-EINVAL;

    uintptr_t irqf = spin_lock_irqsave(&shm_lock);

    struct shm_segment* seg = &segments[shmid];
    if (!seg->used) {
        spin_unlock_irqrestore(&shm_lock, irqf);
        return (void*)(uintptr_t)-EINVAL;
    }

    if (seg->marked_rm) {
        spin_unlock_irqrestore(&shm_lock, irqf);
        return (void*)(uintptr_t)-EIDRM;
    }

    if (!current_process) {
        spin_unlock_irqrestore(&shm_lock, irqf);
        return (void*)(uintptr_t)-EINVAL;
    }

    /* Check POSIX read permission (always required) */
    if (!shm_perm_check(seg, 04)) {  /* R_OK = 4 */
        spin_unlock_irqrestore(&shm_lock, irqf);
        return (void*)(uintptr_t)-EACCES;
    }

    /* If not SHM_RDONLY, also check write permission */
    if (!(shmflg & SHM_RDONLY)) {
        if (!shm_perm_check(seg, 02)) {  /* W_OK = 2 */
            spin_unlock_irqrestore(&shm_lock, irqf);
            return (void*)(uintptr_t)-EACCES;
        }
    }

    /* Find a free mmap slot (always needed to track the mapping) */
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

    /* If shmaddr == 0, kernel picks address */
    uintptr_t vaddr = shmaddr;
    uint32_t map_len = seg->npages * PAGE_SIZE;

    if (vaddr != 0) {
        /* K03: validate user-supplied address */
        if (vaddr & 0xFFF) {
            spin_unlock_irqrestore(&shm_lock, irqf);
            return (void*)(uintptr_t)-EINVAL;
        }
        uintptr_t kern_base = hal_mm_kernel_virt_base();
        if (vaddr >= kern_base || vaddr + map_len > kern_base || vaddr + map_len < vaddr) {
            spin_unlock_irqrestore(&shm_lock, irqf);
            return (void*)(uintptr_t)-EINVAL;
        }
    } else {
        /* K03: use vmm_find_free_area instead of fixed formula */
        vaddr = vmm_find_free_area(0x40000000U, 0x7FF00000U, map_len);
        if (!vaddr) {
            spin_unlock_irqrestore(&shm_lock, irqf);
            return (void*)(uintptr_t)-ENOMEM;
        }
    }

    /* Map physical pages into user address space.
     * vmm_map_page signature: (phys, virt, flags)
     * NX by default - IA32_EFER.NXE MSR is now enabled (A01 completed)
     * H3: If SHM_RDONLY is set, map without VMM_FLAG_RW for read-only access */
    uint64_t map_flags = VMM_FLAG_PRESENT | VMM_FLAG_USER | VMM_FLAG_NX;
    if (!(shmflg & SHM_RDONLY)) {
        map_flags |= VMM_FLAG_RW;
    }
    for (uint32_t i = 0; i < seg->npages; i++) {
        vmm_map_page((uint64_t)seg->pages[i],
                     (uint64_t)(vaddr + i * PAGE_SIZE),
                     map_flags);
    }

    /* Record mapping in process mmap table with shmid for detach lookup */
    current_process->mmaps[mslot].base = vaddr;
    current_process->mmaps[mslot].length = seg->npages * PAGE_SIZE;
    current_process->mmaps[mslot].shmid = shmid;

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
    int shmid = current_process->mmaps[mslot].shmid;

    /* Unmap pages (but don't free — they belong to the shm segment) */
    for (uint32_t i = 0; i < npages; i++) {
        vmm_unmap_page((uint64_t)(addr + i * PAGE_SIZE));
    }

    /* Clear the mmap slot */
    current_process->mmaps[mslot].base = 0;
    current_process->mmaps[mslot].length = 0;
    current_process->mmaps[mslot].shmid = -1;

    /* Decrement attach count using the stored shmid */
    if (shmid >= 0 && shmid < SHM_MAX_SEGMENTS && segments[shmid].used) {
        if (segments[shmid].nattch > 0) {
            segments[shmid].nattch--;
        }
        if (segments[shmid].nattch == 0 && segments[shmid].marked_rm) {
            shm_destroy(&segments[shmid]);
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
        /* Check read permission */
        if (!shm_perm_check(seg, 04)) {
            spin_unlock_irqrestore(&shm_lock, irqf);
            return -EACCES;
        }
        /* Copy to local struct first, then release lock before
         * writing to userspace to avoid deadlock on page fault. */
        struct shmid_ds local;
        local.shm_segsz = seg->size;
        local.shm_nattch = seg->nattch;
        local.shm_key = seg->key;
        spin_unlock_irqrestore(&shm_lock, irqf);
        if (buf) {
            if (copy_to_user(buf, &local, sizeof(local)) < 0) {
                return -EFAULT;
            }
        }
        return 0;
    }

    if (cmd == IPC_RMID) {
        /* Only owner or root can remove */
        if (seg->uid != current_process->uid && current_process->euid != 0) {
            spin_unlock_irqrestore(&shm_lock, irqf);
            return -EPERM;
        }
        if (seg->nattch == 0) {
            shm_destroy(seg);
        } else {
            seg->marked_rm = 1;
        }
        spin_unlock_irqrestore(&shm_lock, irqf);
        return 0;
    }

    if (cmd == IPC_SET) {
        /* Only owner or root can change permissions */
        if (seg->uid != current_process->uid && current_process->euid != 0) {
            spin_unlock_irqrestore(&shm_lock, irqf);
            return -EPERM;
        }
        if (buf) {
            struct shmid_ds local;
            if (copy_from_user(&local, buf, sizeof(local)) < 0) {
                spin_unlock_irqrestore(&shm_lock, irqf);
                return -EFAULT;
            }
            /* Update mode from IPC_SET */
            seg->mode = local.shm_perm.mode & 0777;
        }
        spin_unlock_irqrestore(&shm_lock, irqf);
        return 0;
    }

    spin_unlock_irqrestore(&shm_lock, irqf);
    return -EINVAL;
}
