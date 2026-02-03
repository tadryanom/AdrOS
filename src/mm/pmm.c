#include "pmm.h"
#include "multiboot2.h"
#include "uart_console.h"
#include <stddef.h>

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
static uint64_t total_memory = 0;
static uint64_t used_memory = 0;
static uint64_t max_frames = 0;

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
        
        if (used) {
            bitmap_set(start_frame + i);
            used_memory += PAGE_SIZE;
        } else {
            bitmap_unset(start_frame + i);
            used_memory -= PAGE_SIZE;
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
    struct multiboot_tag *tag;
    uint32_t mbi_size = *(uint32_t *)boot_info;
    
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
            struct multiboot_tag_mmap *mmap = (struct multiboot_tag_mmap *)tag;
            struct multiboot_mmap_entry *entry;
            
            for (entry = mmap->entries;
                 (uint8_t *)entry < (uint8_t *)mmap + mmap->size;
                 entry = (struct multiboot_mmap_entry *)((uint32_t)entry + mmap->entry_size))
            {
                // Only mark AVAILABLE regions as free
                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    pmm_mark_region(entry->addr, entry->len, 0); // 0 = Free
                }
            }
        }
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
    max_frames = ram_size / PAGE_SIZE;
    
    // Mark all RAM as free
    pmm_mark_region(ram_base, ram_size, 0);

#endif

    // 2. Protect Kernel Memory (Critical!)
    // We must ensure the kernel code itself is marked USED
    
    // Check if we are Higher Half (start > 3GB)
    uint64_t virt_start = (uint64_t)&_start;
    uint64_t virt_end = (uint64_t)&_end;
    
    uint64_t phys_start = virt_start;
    uint64_t phys_end = virt_end;

    if (virt_start >= 0xC0000000) {
        phys_start -= 0xC0000000;
        phys_end -= 0xC0000000;
        uart_print("[PMM] Detected Higher Half Kernel. Adjusting protection range.\n");
    }

    uint64_t kernel_size = phys_end - phys_start;
    
    pmm_mark_region(phys_start, kernel_size, 1); // Mark Used

    // 3. Protect Multiboot info (if x86)
    if (boot_info) {
        pmm_mark_region((uint64_t)boot_info, 4096, 1); // Protect at least 1 page
    }

    uart_print("[PMM] Initialized.\n");
}

void* pmm_alloc_page(void) {
    for (uint64_t i = 0; i < max_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_memory += PAGE_SIZE;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return NULL; // OOM
}

void pmm_free_page(void* ptr) {
    uint64_t addr = (uint64_t)ptr;
    uint64_t frame = addr / PAGE_SIZE;
    bitmap_unset(frame);
    used_memory -= PAGE_SIZE;
}
