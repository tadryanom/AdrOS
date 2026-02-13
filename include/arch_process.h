#ifndef ARCH_PROCESS_H
#define ARCH_PROCESS_H

#include <stdint.h>
#include "interrupts.h"

/*
 * arch_kstack_init — Prepare a kernel stack for a brand-new process/thread
 *                    so that context_switch will resume into wrapper(arg).
 *
 * stack_top  : pointer to the TOP of the kernel stack (base + size).
 * wrapper    : function that context_switch's "ret" will jump to
 *              (e.g. thread_wrapper).
 * arg        : argument passed to wrapper (e.g. actual entry point or
 *              fork_child_trampoline).
 *
 * Returns the initial SP value to store in proc->sp.
 */
uintptr_t arch_kstack_init(void* stack_top,
                            void (*wrapper)(void (*)(void)),
                            void (*arg)(void));

/*
 * Set the "return value" register in a saved trapframe.
 * On x86 this is EAX; on ARM it would be R0, etc.
 */
void arch_regs_set_retval(struct registers* regs, uint32_t val);

/*
 * Set the user-mode stack pointer in a saved trapframe.
 * On x86 this is useresp; on ARM it would be SP_usr, etc.
 */
void arch_regs_set_ustack(struct registers* regs, uintptr_t sp);

#endif /* ARCH_PROCESS_H */
