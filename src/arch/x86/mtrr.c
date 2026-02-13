#include "mtrr.h"
#include "console.h"

#define IA32_MTRRCAP       0xFE
#define IA32_MTRR_DEF_TYPE 0x2FF
#define IA32_MTRR_PHYS_BASE(n) (0x200 + 2*(n))
#define IA32_MTRR_PHYS_MASK(n) (0x201 + 2*(n))

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

static uint8_t mtrr_count = 0;
static uint8_t mtrr_enabled = 0;

void mtrr_init(void) {
    uint32_t eax, edx;
    __asm__ volatile("cpuid" : "=a"(eax) : "a"(1) : "ebx", "ecx", "edx");
    /* Check MTRR support: CPUID.1:EDX bit 12 */
    __asm__ volatile("cpuid" : "=a"(eax), "=d"(edx) : "a"(1) : "ebx", "ecx");
    if (!(edx & (1U << 12))) {
        kprintf("[MTRR] Not supported by CPU\n");
        return;
    }

    uint64_t cap = rdmsr(IA32_MTRRCAP);
    mtrr_count = (uint8_t)(cap & 0xFF);
    if (mtrr_count == 0) {
        kprintf("[MTRR] No variable-range MTRRs available\n");
        return;
    }

    mtrr_enabled = 1;
    kprintf("[MTRR] Initialized, %u variable-range registers\n",
            (unsigned)mtrr_count);
}

int mtrr_set_range(uint64_t base, uint64_t size, uint8_t type) {
    if (!mtrr_enabled) return -1;
    if (size == 0) return -1;
    /* Size must be a power of 2 and base must be aligned to size */
    if (size & (size - 1)) return -1;
    if (base & (size - 1)) return -1;

    /* Find a free variable-range MTRR register */
    int slot = -1;
    for (int i = 0; i < mtrr_count; i++) {
        uint64_t mask = rdmsr(IA32_MTRR_PHYS_MASK(i));
        if (!(mask & (1ULL << 11))) { /* Valid bit not set = free */
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1; /* No free MTRR slots */

    /* 36-bit physical address mask (common for 32-bit x86) */
    uint64_t addr_mask = 0x0000000FFFFFFFFFULL;
    uint64_t phys_mask = (~(size - 1)) & addr_mask;

    /* Disable interrupts during MTRR programming */
    __asm__ volatile("cli");

    /* Flush caches */
    __asm__ volatile("wbinvd");

    wrmsr(IA32_MTRR_PHYS_BASE(slot), (base & addr_mask) | type);
    wrmsr(IA32_MTRR_PHYS_MASK(slot), phys_mask | (1ULL << 11)); /* Set valid bit */

    /* Flush caches again */
    __asm__ volatile("wbinvd");

    __asm__ volatile("sti");

    return 0;
}
