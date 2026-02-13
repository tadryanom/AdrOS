#include "vdso.h"
#include "kernel_va_map.h"
#include "pmm.h"
#include "vmm.h"
#include "utils.h"
#include "console.h"

static uintptr_t vdso_phys = 0;
static volatile struct vdso_data* vdso_kptr = 0;

void vdso_init(void) {
    void* page = pmm_alloc_page();
    if (!page) {
        kprintf("[VDSO] OOM\n");
        return;
    }
    vdso_phys = (uintptr_t)page;

    /* Map into kernel space at a fixed VA so we can write to it. */
    uintptr_t kva = KVA_VDSO;
    vmm_map_page((uint64_t)vdso_phys, (uint64_t)kva,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW);

    vdso_kptr = (volatile struct vdso_data*)kva;
    memset((void*)vdso_kptr, 0, PAGE_SIZE);
    vdso_kptr->tick_hz = 50;

    kprintf("[VDSO] Initialized at phys=0x%x\n", (unsigned)vdso_phys);
}

void vdso_update_tick(uint32_t tick) {
    if (vdso_kptr) {
        vdso_kptr->tick_count = tick;
    }
}

uintptr_t vdso_get_phys(void) {
    return vdso_phys;
}
