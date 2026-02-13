#include "pmm.h"
#include "utils.h"
#include "console.h"
#include "hal/cpu.h"
#include "hal/mm.h"
#include "spinlock.h"
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
static spinlock_t pmm_lock = {0};

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

void pmm_mark_region(uint64_t base, uint64_t size, int used) {
    uint64_t start_frame = base / PAGE_SIZE;
    uint64_t frames_count = size / PAGE_SIZE;

    uintptr_t flags = spin_lock_irqsave(&pmm_lock);
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
    spin_unlock_irqrestore(&pmm_lock, flags);
}

void pmm_set_limits(uint64_t total_mem, uint64_t max_fr) {
    if (total_mem > MAX_RAM_SIZE) total_mem = MAX_RAM_SIZE;
    total_mem = align_down(total_mem, PAGE_SIZE);
    total_memory = total_mem;
    max_frames = max_fr ? max_fr : (total_mem / PAGE_SIZE);
    used_memory = max_frames * PAGE_SIZE;
}

// Weak default: architectures that don't implement pmm_arch_init yet
__attribute__((weak))
void pmm_arch_init(void* boot_info) {
    (void)boot_info;
    kprintf("[PMM] No arch-specific memory init. Assuming 16MB.\n");
    pmm_set_limits(16 * 1024 * 1024, 0);
}

void pmm_init(void* boot_info) {
    // 1. Mark EVERYTHING as used initially to be safe
    for (int i = 0; i < BITMAP_SIZE; i++) {
        memory_bitmap[i] = 0xFF;
    }

    // 2. Let arch-specific code discover memory and call
    //    pmm_set_limits() + pmm_mark_region()
    pmm_arch_init(boot_info);

    // 3. Protect Kernel Memory (Critical!)
    uintptr_t virt_start_ptr = (uintptr_t)&_start;
    uintptr_t virt_end_ptr = (uintptr_t)&_end;

    uint64_t phys_start = (uint64_t)hal_mm_virt_to_phys(virt_start_ptr);
    uint64_t phys_end = (uint64_t)hal_mm_virt_to_phys(virt_end_ptr);

    // Fallback: if hal_mm_virt_to_phys returns 0 (not implemented),
    // try subtracting kernel virtual base manually
    if (phys_start == 0 && virt_start_ptr != 0) {
        phys_start = (uint64_t)virt_start_ptr;
        phys_end = (uint64_t)virt_end_ptr;
        uintptr_t kvbase = hal_mm_kernel_virt_base();
        if (kvbase && virt_start_ptr >= kvbase) {
            phys_start -= kvbase;
            phys_end -= kvbase;
        }
    }

    uint64_t phys_start_aligned = align_down(phys_start, PAGE_SIZE);
    uint64_t phys_end_aligned = align_up(phys_end, PAGE_SIZE);
    if (phys_end_aligned < phys_start_aligned) {
        phys_end_aligned = phys_start_aligned;
    }
    uint64_t kernel_size = phys_end_aligned - phys_start_aligned;

    pmm_mark_region(phys_start_aligned, kernel_size, 1);

    kprintf("[PMM] Initialized.\n");
}

void* pmm_alloc_page(void) {
    uintptr_t flags = spin_lock_irqsave(&pmm_lock);

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
            spin_unlock_irqrestore(&pmm_lock, flags);
            return (void*)(uintptr_t)(i * PAGE_SIZE);
        }
    }

    spin_unlock_irqrestore(&pmm_lock, flags);
    return NULL; // OOM
}

void* pmm_alloc_blocks(uint32_t count) {
    if (count == 0) return NULL;
    if (count == 1) return pmm_alloc_page();

    uintptr_t flags = spin_lock_irqsave(&pmm_lock);

    for (uint64_t start = 1; start + count <= max_frames; start++) {
        int found = 1;
        for (uint32_t j = 0; j < count; j++) {
            if (bitmap_test(start + j)) {
                start += j; /* skip ahead past the used frame */
                found = 0;
                break;
            }
        }
        if (found) {
            for (uint32_t j = 0; j < count; j++) {
                bitmap_set(start + j);
                frame_refcount[start + j] = 1;
                used_memory += PAGE_SIZE;
            }
            spin_unlock_irqrestore(&pmm_lock, flags);
            return (void*)(uintptr_t)(start * PAGE_SIZE);
        }
    }

    spin_unlock_irqrestore(&pmm_lock, flags);
    return NULL;
}

void pmm_free_blocks(void* ptr, uint32_t count) {
    uintptr_t addr = (uintptr_t)ptr;
    for (uint32_t i = 0; i < count; i++) {
        pmm_free_page((void*)(addr + i * PAGE_SIZE));
    }
}

void pmm_free_page(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    uint64_t frame = addr / PAGE_SIZE;
    if (frame == 0 || frame >= max_frames) return;

    uintptr_t flags = spin_lock_irqsave(&pmm_lock);

    uint16_t rc = frame_refcount[frame];
    if (rc > 1) {
        frame_refcount[frame]--;
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }

    frame_refcount[frame] = 0;
    bitmap_unset(frame);
    used_memory -= PAGE_SIZE;

    spin_unlock_irqrestore(&pmm_lock, flags);
}

void pmm_incref(uintptr_t paddr) {
    uint64_t frame = paddr / PAGE_SIZE;
    if (frame == 0 || frame >= max_frames) return;
    uintptr_t flags = spin_lock_irqsave(&pmm_lock);
    frame_refcount[frame]++;
    spin_unlock_irqrestore(&pmm_lock, flags);
}

uint16_t pmm_decref(uintptr_t paddr) {
    uint64_t frame = paddr / PAGE_SIZE;
    if (frame == 0 || frame >= max_frames) return 0;
    uintptr_t flags = spin_lock_irqsave(&pmm_lock);
    uint16_t new_val = --frame_refcount[frame];
    if (new_val == 0) {
        bitmap_unset(frame);
        used_memory -= PAGE_SIZE;
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
    return new_val;
}

uint16_t pmm_get_refcount(uintptr_t paddr) {
    uint64_t frame = paddr / PAGE_SIZE;
    if (frame >= max_frames) return 0;
    uintptr_t flags = spin_lock_irqsave(&pmm_lock);
    uint16_t rc = frame_refcount[frame];
    spin_unlock_irqrestore(&pmm_lock, flags);
    return rc;
}

void pmm_print_stats(void) {
    uintptr_t flags = spin_lock_irqsave(&pmm_lock);
    uint64_t total_kb = total_memory / 1024;
    uint64_t used_kb  = used_memory / 1024;
    uint64_t free_kb  = (total_memory > used_memory) ? (total_memory - used_memory) / 1024 : 0;
    spin_unlock_irqrestore(&pmm_lock, flags);

    kprintf("  Total RAM: %u KB (%u MB)\n", (unsigned)total_kb, (unsigned)(total_kb / 1024));
    kprintf("  Used:      %u KB (%u MB)\n", (unsigned)used_kb, (unsigned)(used_kb / 1024));
    kprintf("  Free:      %u KB (%u MB)\n", (unsigned)free_kb, (unsigned)(free_kb / 1024));
}
