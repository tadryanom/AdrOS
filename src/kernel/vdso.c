#include "vdso.h"
#include "pmm.h"
#include "vmm.h"
#include "utils.h"
#include "uart_console.h"

static uintptr_t vdso_phys = 0;
static volatile struct vdso_data* vdso_kptr = 0;

void vdso_init(void) {
    void* page = pmm_alloc_page();
    if (!page) {
        uart_print("[VDSO] OOM\n");
        return;
    }
    vdso_phys = (uintptr_t)page;

    /* Map into kernel space at a fixed VA so we can write to it.
     * Use 0xC0230000 (above ATA DMA bounce at 0xC0221000). */
    uintptr_t kva = 0xC0230000U;
    vmm_map_page((uint64_t)vdso_phys, (uint64_t)kva,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW);

    vdso_kptr = (volatile struct vdso_data*)kva;
    memset((void*)vdso_kptr, 0, PAGE_SIZE);
    vdso_kptr->tick_hz = 50;

    uart_print("[VDSO] Initialized at phys=0x");
    /* Simple hex print */
    char hex[9];
    for (int i = 7; i >= 0; i--) {
        uint8_t nib = (uint8_t)((vdso_phys >> (i * 4)) & 0xF);
        hex[7 - i] = (char)(nib < 10 ? '0' + nib : 'a' + nib - 10);
    }
    hex[8] = '\0';
    uart_print(hex);
    uart_print("\n");
}

void vdso_update_tick(uint32_t tick) {
    if (vdso_kptr) {
        vdso_kptr->tick_count = tick;
    }
}

uintptr_t vdso_get_phys(void) {
    return vdso_phys;
}
