#include "pmm.h"
#include "multiboot2.h"
#include "utils.h"
#include "uart_console.h"
#include "hal/cpu.h"
#include "hal/mm.h"
#include <stddef.h>
#include <stdint.h>

// Defined in linker script
extern uint8_t _start;
extern uint8_t _end;

// Simple bitmap allocator
// Supports up to 512MB RAM for now to keep bitmap small (16KB)
// 512MB / 4KB pages = 131072 pages
// 131072 bits / 8 = 16384 bytes
#define MAX_RAM_SIZE (512 * 1024 * 1024)
#define BITMAP_SIZE (MAX_RAM_SIZE / PAGE_SIZE / 8)

static uint8_t memory_bitmap[BITMAP_SIZE];
static uint16_t frame_refcount[MAX_RAM_SIZE / PAGE_SIZE];
static uint64_t total_memory = 0;
static uint64_t used_memory = 0;
static uint64_t max_frames = 0;
static uint64_t last_alloc_frame = 1;

static uint64_t align_down(uint64_t value, uint64_t align) {
    return value & ~(align - 1);
}

static uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + align - 1) & ~(align - 1);
}

static void bitmap_set(uint64_t bit) {
    memory_bitmap[bit / 8] |= (1 << (bit % 8));
}

static void bitmap_unset(uint64_t bit) {
    memory_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static int bitmap_test(uint64_t bit) {
    return memory_bitmap[bit / 8] & (1 << (bit % 8));
}

// Mark a range of physical memory as used (1) or free (0)
static void pmm_mark_region(uint64_t base, uint64_t size, int used) {
    uint64_t start_frame = base / PAGE_SIZE;
    uint64_t frames_count = size / PAGE_SIZE;

    for (uint64_t i = 0; i < frames_count; i++) {
        if (start_frame + i >= max_frames) break;

        uint64_t frame = start_frame + i;
        int was_used = bitmap_test(frame) ? 1 : 0;

        if (used) {
            if (!was_used) {
                bitmap_set(frame);
                used_memory += PAGE_SIZE;
            }
        } else {
            if (was_used) {
                bitmap_unset(frame);
                used_memory -= PAGE_SIZE;
            }
        }
    }
}

void pmm_init(void* boot_info) {
    // 1. Mark EVERYTHING as used initially to be safe
    for (int i = 0; i < BITMAP_SIZE; i++) {
        memory_bitmap[i] = 0xFF;
    }

#if defined(__i386__) || defined(__x86_64__)
    // Parse Multiboot2 Info
    if (boot_info) {
        struct multiboot_tag *tag;
        (void)*(uint32_t *)boot_info;
        uint64_t highest_addr = 0;
        int saw_mmap = 0;
        uint64_t freed_frames = 0;
        
        uart_print("[PMM] Parsing Multiboot2 info...\n");

        for (tag = (struct multiboot_tag *)((uint8_t *)boot_info + 8);
           tag->type != MULTIBOOT_TAG_TYPE_END;
           tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7)))
        {
            if (tag->type == MULTIBOOT_TAG_TYPE_BASIC_MEMINFO) {
                 struct multiboot_tag_basic_meminfo *meminfo = (struct multiboot_tag_basic_meminfo *)tag;
                 // Basic info just gives lower/upper KB
                 uint64_t mem_kb = meminfo->mem_upper; // Upper memory only
                 total_memory = (mem_kb * 1024) + (1024*1024); // +1MB low mem
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

                    // Only mark AVAILABLE regions as free
                    if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                        pmm_mark_region(entry->addr, entry->len, 0); // 0 = Free
                    }
                }
            }
        }

        // Finalize memory limits (must be done BEFORE pmm_mark_region is meaningful)
        if (highest_addr > total_memory) {
            total_memory = highest_addr;
        }

        if (total_memory == 0) {
            total_memory = 16 * 1024 * 1024; // Fallback to 16MB
        }

        if (total_memory > MAX_RAM_SIZE) {
            total_memory = MAX_RAM_SIZE;
        }

        total_memory = align_down(total_memory, PAGE_SIZE);
        max_frames = total_memory / PAGE_SIZE;

        // After "everything used", used_memory should reflect that state.
        used_memory = max_frames * PAGE_SIZE;

        // Re-run map processing to free AVAILABLE regions now that max_frames is valid.
        for (tag = (struct multiboot_tag *)((uint8_t *)boot_info + 8);
           tag->type != MULTIBOOT_TAG_TYPE_END;
           tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7)))
        {
            if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
                saw_mmap = 1;
                struct multiboot_tag_mmap *mmap = (struct multiboot_tag_mmap *)tag;
                struct multiboot_mmap_entry *entry;

                for (entry = mmap->entries;
                     (uint8_t *)entry < (uint8_t *)mmap + mmap->size;
                     entry = (struct multiboot_mmap_entry *)((uint32_t)entry + mmap->entry_size))
                {
                    if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                        // Clamp to our supported range
                        uint64_t base = entry->addr;
                        uint64_t len = entry->len;

                        if (base >= total_memory) continue;
                        if (base + len > total_memory) {
                            len = total_memory - base;
                        }
                        base = align_up(base, PAGE_SIZE);
                        len = align_down(len, PAGE_SIZE);
                        if (len == 0) continue;

                        pmm_mark_region(base, len, 0);
                        freed_frames += (len / PAGE_SIZE);
                    }
                }
            }
        }

        // Fallback: if we didn't manage to see an MMAP tag, assume everything above 1MB
        // up to total_memory is usable. This is less accurate, but prevents "all used"
        // which breaks early-boot allocators.
        if (!saw_mmap) {
            uint64_t base = 0x00100000;
            uint64_t len = (total_memory > base) ? (total_memory - base) : 0;
            base = align_up(base, PAGE_SIZE);
            len = align_down(len, PAGE_SIZE);
            if (len) {
                pmm_mark_region(base, len, 0);
                freed_frames += (len / PAGE_SIZE);
            }
        }

        // Reserve low memory and frame 0.
        // Frame 0 must never be returned because it aliases NULL in C.
        // Also keep the first 1MB used (BIOS/real-mode areas, multiboot scratch, etc.).
        pmm_mark_region(0, 0x00100000, 1);

        uart_print("[PMM] total_memory bytes: ");
        char tmp[11];
        itoa_hex((uint32_t)total_memory, tmp);
        uart_print(tmp);
        uart_print("\n");
        uart_print("[PMM] max_frames: ");
        itoa_hex((uint32_t)max_frames, tmp);
        uart_print(tmp);
        uart_print("\n");
        uart_print("[PMM] freed_frames: ");
        itoa_hex((uint32_t)freed_frames, tmp);
        uart_print(tmp);
        uart_print("\n");

        if (freed_frames == 0) {
            uart_print("[PMM] WARN: no free frames detected (MMAP missing or parse failed).\n");
        }
    } else {
        uart_print("[PMM] Error: boot_info is NULL!\n");
    }
#else
    // Manual setup for ARM/RISC-V (assuming fixed RAM for now)
    // TODO: Parse Device Tree (DTB)
    uart_print("[PMM] Manual memory config (ARM/RISC-V)\n");
    
    uint64_t ram_base = 0;
    #ifdef __aarch64__
        ram_base = 0x40000000;
    #elif defined(__riscv)
        ram_base = 0x80000000;
    #endif

    uint64_t ram_size = 128 * 1024 * 1024; // Assume 128MB
    if (ram_size > MAX_RAM_SIZE) ram_size = MAX_RAM_SIZE;
    ram_size = align_down(ram_size, PAGE_SIZE);
    max_frames = ram_size / PAGE_SIZE;
    used_memory = max_frames * PAGE_SIZE;
    
    // Mark all RAM as free
    pmm_mark_region(ram_base, ram_size, 0);

#endif

    // 2. Protect Kernel Memory (Critical!)
    // We must ensure the kernel code itself is marked USED
    
    // Check if we are Higher Half (start > 3GB)
    uintptr_t virt_start_ptr = (uintptr_t)&_start;
    uintptr_t virt_end_ptr = (uintptr_t)&_end;
    
    uint64_t phys_start = (uint64_t)virt_start_ptr;
    uint64_t phys_end = (uint64_t)virt_end_ptr;

    uintptr_t kvbase = hal_mm_kernel_virt_base();
    if (kvbase && virt_start_ptr >= kvbase) {
        phys_start -= kvbase;
        phys_end -= kvbase;
        uart_print("[PMM] Detected Higher Half Kernel. Adjusting protection range.\n");
    }

#if defined(__mips__)
    // MIPS KSEG0 virtual addresses are 0x80000000..0x9FFFFFFF mapped to physical 0x00000000..
    if (virt_start >= 0x80000000ULL && virt_start < 0xA0000000ULL) {
        phys_start = virt_start - 0x80000000ULL;
        phys_end = virt_end - 0x80000000ULL;
    }
#endif

    uint64_t phys_start_aligned = align_down(phys_start, PAGE_SIZE);
    uint64_t phys_end_aligned = align_up(phys_end, PAGE_SIZE);
    if (phys_end_aligned < phys_start_aligned) {
        phys_end_aligned = phys_start_aligned;
    }
    uint64_t kernel_size = phys_end_aligned - phys_start_aligned;

    pmm_mark_region(phys_start_aligned, kernel_size, 1); // Mark Used

#if defined(__i386__) || defined(__x86_64__)
    // 3. Protect Multiboot2 modules (e.g. initrd)
    // The initrd is loaded by GRUB into physical memory. If we don't reserve it,
    // the PMM may allocate those frames and overwrite the initrd header/data.
    if (boot_info) {
        struct multiboot_tag *tag;
        for (tag = (struct multiboot_tag *)((uint8_t *)boot_info + 8);
             tag->type != MULTIBOOT_TAG_TYPE_END;
             tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7)))
        {
            if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
                struct multiboot_tag_module* mod = (struct multiboot_tag_module*)tag;
                uint64_t mod_start = (uint64_t)mod->mod_start;
                uint64_t mod_end = (uint64_t)mod->mod_end;
                if (mod_end < mod_start) mod_end = mod_start;

                uint64_t mod_start_aligned = align_down(mod_start, PAGE_SIZE);
                uint64_t mod_end_aligned = align_up(mod_end, PAGE_SIZE);
                if (mod_end_aligned < mod_start_aligned) {
                    mod_end_aligned = mod_start_aligned;
                }

                pmm_mark_region(mod_start_aligned, mod_end_aligned - mod_start_aligned, 1);
            }
        }

        // 4. Protect Multiboot info (if x86)
        uintptr_t bi_ptr = (uintptr_t)boot_info;
        if (!kvbase || bi_ptr < kvbase) {
            pmm_mark_region((uint64_t)bi_ptr, 4096, 1); // Protect at least 1 page
        }
    }
#endif

    uart_print("[PMM] Initialized.\n");
}

void* pmm_alloc_page(void) {
    // Start from frame 1 so we never return physical address 0.
    if (last_alloc_frame < 1) last_alloc_frame = 1;
    if (last_alloc_frame >= max_frames) last_alloc_frame = 1;

    for (uint64_t scanned = 0; scanned < (max_frames - 1); scanned++) {
        uint64_t i = last_alloc_frame + scanned;
        if (i >= max_frames) {
            i = 1 + (i - max_frames);
        }

        if (!bitmap_test(i)) {
            bitmap_set(i);
            frame_refcount[i] = 1;
            used_memory += PAGE_SIZE;
            last_alloc_frame = i + 1;
            if (last_alloc_frame >= max_frames) last_alloc_frame = 1;
            return (void*)(uintptr_t)(i * PAGE_SIZE);
        }
    }
    return NULL; // OOM
}

void pmm_free_page(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    uint64_t frame = addr / PAGE_SIZE;
    if (frame == 0 || frame >= max_frames) return;

    uint16_t rc = frame_refcount[frame];
    if (rc > 1) {
        __sync_sub_and_fetch(&frame_refcount[frame], 1);
        return;
    }

    frame_refcount[frame] = 0;
    bitmap_unset(frame);
    used_memory -= PAGE_SIZE;
}

void pmm_incref(uintptr_t paddr) {
    uint64_t frame = paddr / PAGE_SIZE;
    if (frame == 0 || frame >= max_frames) return;
    __sync_fetch_and_add(&frame_refcount[frame], 1);
}

uint16_t pmm_decref(uintptr_t paddr) {
    uint64_t frame = paddr / PAGE_SIZE;
    if (frame == 0 || frame >= max_frames) return 0;
    uint16_t new_val = __sync_sub_and_fetch(&frame_refcount[frame], 1);
    if (new_val == 0) {
        bitmap_unset(frame);
        used_memory -= PAGE_SIZE;
    }
    return new_val;
}

uint16_t pmm_get_refcount(uintptr_t paddr) {
    uint64_t frame = paddr / PAGE_SIZE;
    if (frame >= max_frames) return 0;
    return frame_refcount[frame];
}
