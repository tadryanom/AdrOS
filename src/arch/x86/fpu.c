#include "arch_fpu.h"
#include "console.h"
#include "hal/cpu_features.h"

#include <stdint.h>
#include <string.h>

/* CR0 bits */
#define CR0_EM  (1U << 2)   /* Emulate coprocessor (must be CLEAR for real FPU) */
#define CR0_TS  (1U << 3)   /* Task Switched (lazy FPU — we clear it) */
#define CR0_NE  (1U << 5)   /* Numeric Error (use native FPU exceptions) */
#define CR0_MP  (1U << 1)   /* Monitor coprocessor */

/* CR4 bits */
#define CR4_OSFXSR    (1U << 9)   /* OS supports FXSAVE/FXRSTOR */
#define CR4_OSXMMEXCPT (1U << 10) /* OS supports SSE exceptions */

static int g_fpu_has_fxsr = 0;

/* Clean FPU state captured right after FNINIT — used as template for new processes */
static uint8_t g_fpu_clean_state[FPU_STATE_SIZE] __attribute__((aligned(FPU_STATE_ALIGN)));

static inline uint32_t read_cr0(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint32_t val) {
    __asm__ volatile("mov %0, %%cr0" :: "r"(val) : "memory");
}

static inline uint32_t read_cr4(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint32_t val) {
    __asm__ volatile("mov %0, %%cr4" :: "r"(val) : "memory");
}

void arch_fpu_init(void) {
    const struct cpu_features* f = hal_cpu_get_features();

    /* Set CR0: clear EM (no emulation), set MP+NE, clear TS */
    uint32_t cr0 = read_cr0();
    cr0 &= ~(CR0_EM | CR0_TS);
    cr0 |= CR0_MP | CR0_NE;
    write_cr0(cr0);

    /* Initialize x87 FPU */
    __asm__ volatile("fninit");

    /* Enable FXSAVE/FXRSTOR if supported */
    if (f->has_fxsr) {
        uint32_t cr4 = read_cr4();
        cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
        write_cr4(cr4);
        g_fpu_has_fxsr = 1;
        kprintf("[FPU] FXSAVE/FXRSTOR enabled (SSE context support).\n");
    } else {
        kprintf("[FPU] Using legacy FSAVE/FRSTOR.\n");
    }

    /* Capture clean FPU state as template for new processes */
    memset(g_fpu_clean_state, 0, FPU_STATE_SIZE);
    arch_fpu_save(g_fpu_clean_state);

    kprintf("[FPU] FPU/SSE context switching initialized.\n");
}

void arch_fpu_save(uint8_t* state) {
    if (g_fpu_has_fxsr) {
        __asm__ volatile("fxsave (%0)" :: "r"(state) : "memory");
    } else {
        __asm__ volatile("fnsave (%0)" :: "r"(state) : "memory");
        /* fnsave resets the FPU — reinitialize so current process can keep using it */
        __asm__ volatile("fninit");
    }
}

void arch_fpu_restore(const uint8_t* state) {
    if (g_fpu_has_fxsr) {
        __asm__ volatile("fxrstor (%0)" :: "r"(state) : "memory");
    } else {
        __asm__ volatile("frstor (%0)" :: "r"(state) : "memory");
    }
}

void arch_fpu_init_state(uint8_t* state) {
    memcpy(state, g_fpu_clean_state, FPU_STATE_SIZE);
}
