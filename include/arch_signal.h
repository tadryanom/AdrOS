#ifndef ARCH_SIGNAL_H
#define ARCH_SIGNAL_H

#include "interrupts.h"

/*
 * arch_sigreturn — Restore user registers from a signal frame on the
 *                  user stack.  Architecture-specific because the frame
 *                  layout and register sanitisation depend on the ISA.
 *
 * regs       : current trapframe (will be overwritten on success).
 * user_frame : user-space pointer to the signal frame pushed by the
 *              signal delivery trampoline.
 *
 * Returns 0 on success, negative errno on failure.
 */
int arch_sigreturn(struct registers* regs, const void* user_frame);

#endif /* ARCH_SIGNAL_H */
