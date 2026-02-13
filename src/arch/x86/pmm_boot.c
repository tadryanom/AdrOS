#include "pmm.h"
#include "arch/x86/multiboot2.h"
#include "console.h"
#include "utils.h"
#include "hal/mm.h"

#include <stdint.h>

static uint64_t align_up_local(uint64_t value, uint64_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uint64_t align_down_local(uint64_t value, uint64_t align) {
    return value & ~(align - 1);
}

void pmm_arch_init(void* boot_info) {
    if (!boot_info) {
        kprintf("[PMM] Error: boot_info is NULL!\n");
        return;
    }

    struct multiboot_tag *tag;
    (void)*(uint32_t *)boot_info;
    uint64_t highest_addr = 0;
    uint64_t total_memory = 0;
    int saw_mmap = 0;
    uint64_t freed_frames = 0;

    kprintf("[PMM] Parsing Multiboot2 info...\n");

    // First pass: determine total memory size
    for (tag = (struct multiboot_tag *)((uint8_t *)boot_info + 8);
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7)))
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_BASIC_MEMINFO) {
             struct multiboot_tag_basic_meminfo *meminfo = (struct multiboot_tag_basic_meminfo *)tag;
             uint64_t mem_kb = meminfo->mem_upper;
             total_memory = (mem_kb * 1024) + (1024*1024);
        }
        else if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            saw_mmap = 1;
            struct multiboot_tag_mmap *mmap = (struct multiboot_tag_mmap *)tag;
            struct multiboot_mmap_entry *entry;

            for (entry = mmap->entries;
                 (uint8_t *)entry < (uint8_t *)mmap + mmap->size;
                 entry = (struct multiboot_mmap_entry *)((uint32_t)entry + mmap->entry_size))
            {
                uint64_t end = entry->addr + entry->len;
                if (end > highest_addr) highest_addr = end;
            }
        }
    }

    if (highest_addr > total_memory) {
        total_memory = highest_addr;
    }
    if (total_memory == 0) {
        total_memory = 16 * 1024 * 1024;
    }

    // pmm_set_limits clamps and configures the bitmap
    pmm_set_limits(total_memory, 0);

    // Second pass: free AVAILABLE regions
    for (tag = (struct multiboot_tag *)((uint8_t *)boot_info + 8);
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7)))
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mmap = (struct multiboot_tag_mmap *)tag;
            struct multiboot_mmap_entry *entry;

            for (entry = mmap->entries;
                 (uint8_t *)entry < (uint8_t *)mmap + mmap->size;
                 entry = (struct multiboot_mmap_entry *)((uint32_t)entry + mmap->entry_size))
            {
                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    uint64_t base = entry->addr;
                    uint64_t len = entry->len;

                    if (base >= total_memory) continue;
                    if (base + len > total_memory) {
                        len = total_memory - base;
                    }
                    base = align_up_local(base, PAGE_SIZE);
                    len = align_down_local(len, PAGE_SIZE);
                    if (len == 0) continue;

                    pmm_mark_region(base, len, 0);
                    freed_frames += (len / PAGE_SIZE);
                }
            }
        }
    }

    // Fallback if no MMAP tag
    if (!saw_mmap) {
        uint64_t base = 0x00100000;
        uint64_t len = (total_memory > base) ? (total_memory - base) : 0;
        base = align_up_local(base, PAGE_SIZE);
        len = align_down_local(len, PAGE_SIZE);
        if (len) {
            pmm_mark_region(base, len, 0);
            freed_frames += (len / PAGE_SIZE);
        }
    }

    // Reserve low memory and frame 0
    pmm_mark_region(0, 0x00100000, 1);

    kprintf("[PMM] total_memory bytes: 0x%x\n", (unsigned)total_memory);
    kprintf("[PMM] freed_frames: 0x%x\n", (unsigned)freed_frames);

    if (freed_frames == 0) {
        kprintf("[PMM] WARN: no free frames detected (MMAP missing or parse failed).\n");
    }

    // Protect Multiboot2 modules (e.g. initrd)
    for (tag = (struct multiboot_tag *)((uint8_t *)boot_info + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7)))
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module* mod = (struct multiboot_tag_module*)tag;
            uint64_t mod_start = (uint64_t)mod->mod_start;
            uint64_t mod_end = (uint64_t)mod->mod_end;
            if (mod_end < mod_start) mod_end = mod_start;

            uint64_t mod_start_aligned = align_down_local(mod_start, PAGE_SIZE);
            uint64_t mod_end_aligned = align_up_local(mod_end, PAGE_SIZE);
            if (mod_end_aligned < mod_start_aligned) {
                mod_end_aligned = mod_start_aligned;
            }

            pmm_mark_region(mod_start_aligned, mod_end_aligned - mod_start_aligned, 1);
        }
    }

    // Protect Multiboot info structure itself
    uintptr_t kvbase = hal_mm_kernel_virt_base();
    uintptr_t bi_ptr = (uintptr_t)boot_info;
    if (!kvbase || bi_ptr < kvbase) {
        pmm_mark_region((uint64_t)bi_ptr, 4096, 1);
    }
}
