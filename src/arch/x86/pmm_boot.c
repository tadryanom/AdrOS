#include "pmm.h"
#include "arch/x86/multiboot2.h"
#include "console.h"
#include "utils.h"
#include "hal/mm.h"

#include <stdint.h>

/* 32-bit x86: we cap usable RAM at MAX_RAM_SIZE (defined in pmm.c). */
#define PMM_MAX_RAM  (512U * 1024U * 1024U)

static uint32_t align_up32(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uint32_t align_down32(uint32_t value, uint32_t align) {
    return value & ~(align - 1);
}

#define MB2_TAG_NEXT(t) \
    ((struct multiboot_tag *)((uint8_t *)(t) + (((t)->size + 7U) & ~7U)))

void pmm_arch_init(void* boot_info) {
    if (!boot_info) {
        kprintf("[PMM] Error: boot_info is NULL!\n");
        return;
    }

    struct multiboot_tag *tag;
    uint32_t total_mem = 0;
    uint32_t highest_avail = 0;
    int      saw_mmap = 0;
    uint32_t freed_frames = 0;

    kprintf("[PMM] Parsing Multiboot2 info...\n");

    /* --- Pass 1: determine total usable memory size --- */
    for (tag = (struct multiboot_tag *)((uint8_t *)boot_info + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = MB2_TAG_NEXT(tag))
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_BASIC_MEMINFO) {
            struct multiboot_tag_basic_meminfo *mi =
                (struct multiboot_tag_basic_meminfo *)tag;
            total_mem = (mi->mem_upper * 1024U) + (1024U * 1024U);
        }
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            saw_mmap = 1;
            struct multiboot_tag_mmap *mmap =
                (struct multiboot_tag_mmap *)tag;
            struct multiboot_mmap_entry *e;
            for (e = mmap->entries;
                 (uint8_t *)e < (uint8_t *)mmap + mmap->size;
                 e = (struct multiboot_mmap_entry *)
                     ((uintptr_t)e + mmap->entry_size))
            {
                if (e->type != MULTIBOOT_MEMORY_AVAILABLE)
                    continue;
                uint64_t end64 = e->addr + e->len;
                /* Clamp to 32-bit address space */
                uint32_t end = (end64 > 0xFFFFFFFFULL)
                             ? 0xFFFFFFFFU : (uint32_t)end64;
                if (end > highest_avail)
                    highest_avail = end;
            }
        }
    }

    if (highest_avail > total_mem)
        total_mem = highest_avail;
    if (total_mem == 0)
        total_mem = 16U * 1024U * 1024U;
    if (total_mem > PMM_MAX_RAM)
        total_mem = PMM_MAX_RAM;

    pmm_set_limits((uint64_t)total_mem, 0);

    /* --- Pass 2: free AVAILABLE regions --- */
    for (tag = (struct multiboot_tag *)((uint8_t *)boot_info + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = MB2_TAG_NEXT(tag))
    {
        if (tag->type != MULTIBOOT_TAG_TYPE_MMAP)
            continue;

        struct multiboot_tag_mmap *mmap =
            (struct multiboot_tag_mmap *)tag;
        struct multiboot_mmap_entry *e;

        for (e = mmap->entries;
             (uint8_t *)e < (uint8_t *)mmap + mmap->size;
             e = (struct multiboot_mmap_entry *)
                 ((uintptr_t)e + mmap->entry_size))
        {
            if (e->type != MULTIBOOT_MEMORY_AVAILABLE)
                continue;

            uint64_t b64 = e->addr;
            uint64_t l64 = e->len;
            if (b64 >= total_mem) continue;
            if (b64 + l64 > total_mem)
                l64 = total_mem - b64;

            uint32_t base = align_up32((uint32_t)b64, PAGE_SIZE);
            uint32_t len  = align_down32((uint32_t)l64, PAGE_SIZE);
            if (len == 0) continue;

            pmm_mark_region(base, len, 0);
            freed_frames += len / PAGE_SIZE;
        }
    }

    /* Fallback if no MMAP tag */
    if (!saw_mmap) {
        uint32_t base = 0x00100000U;
        uint32_t len = (total_mem > base) ? (total_mem - base) : 0;
        base = align_up32(base, PAGE_SIZE);
        len  = align_down32(len, PAGE_SIZE);
        if (len) {
            pmm_mark_region(base, len, 0);
            freed_frames += len / PAGE_SIZE;
        }
    }

    /* Reserve low memory (real-mode IVT, BDA, EBDA, BIOS ROM) */
    pmm_mark_region(0, 0x00100000, 1);

    kprintf("[PMM] total_memory: %u bytes (%u MB)\n",
            total_mem, total_mem / (1024U * 1024U));
    kprintf("[PMM] freed_frames: %u (%u MB usable)\n",
            freed_frames, (freed_frames * PAGE_SIZE) / (1024U * 1024U));

    if (freed_frames == 0) {
        kprintf("[PMM] WARN: no free frames detected (MMAP missing or parse failed).\n");
    }

    /* Protect Multiboot2 modules (e.g. initrd) */
    for (tag = (struct multiboot_tag *)((uint8_t *)boot_info + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = MB2_TAG_NEXT(tag))
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module* mod =
                (struct multiboot_tag_module*)tag;
            uint32_t ms = align_down32(mod->mod_start, PAGE_SIZE);
            uint32_t me = align_up32(mod->mod_end, PAGE_SIZE);
            if (me > ms)
                pmm_mark_region(ms, me - ms, 1);
        }
    }

    /* Protect Multiboot info structure itself */
    uintptr_t kvbase = hal_mm_kernel_virt_base();
    uintptr_t bi_ptr = (uintptr_t)boot_info;
    if (!kvbase || bi_ptr < kvbase) {
        pmm_mark_region((uint64_t)bi_ptr, 4096, 1);
    }
}
