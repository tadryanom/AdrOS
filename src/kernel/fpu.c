#include "arch_fpu.h"
#include "console.h"
#include <string.h>

__attribute__((weak))
void arch_fpu_init(void) {
    kprintf("[FPU] No arch-specific FPU support.\n");
}

__attribute__((weak))
void arch_fpu_save(uint8_t* state) {
    (void)state;
}

__attribute__((weak))
void arch_fpu_restore(const uint8_t* state) {
    (void)state;
}

__attribute__((weak))
void arch_fpu_init_state(uint8_t* state) {
    memset(state, 0, FPU_STATE_SIZE);
}
