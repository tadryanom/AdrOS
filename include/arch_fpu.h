#ifndef ARCH_FPU_H
#define ARCH_FPU_H

#include <stdint.h>
#include <stddef.h>

/*
 * FPU/SSE context save area size.
 * FXSAVE requires 512 bytes, 16-byte aligned.
 * FSAVE requires 108 bytes (no alignment requirement).
 * We always allocate the larger size for simplicity.
 */
#define FPU_STATE_SIZE  512
#define FPU_STATE_ALIGN 16

/* Initialize FPU hardware during boot (CR0/CR4 bits, FNINIT). */
void arch_fpu_init(void);

/* Save current FPU/SSE state into buffer (must be 16-byte aligned). */
void arch_fpu_save(uint8_t* state);

/* Restore FPU/SSE state from buffer (must be 16-byte aligned). */
void arch_fpu_restore(const uint8_t* state);

/* Copy the clean (post-FNINIT) FPU state into buffer for new processes. */
void arch_fpu_init_state(uint8_t* state);

#endif
