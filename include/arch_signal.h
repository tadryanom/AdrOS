// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_SIGNAL_H
#define ARCH_SIGNAL_H

/*
 * arch_sigreturn — Restore user registers from a signal frame on the
 *                  user stack.  Architecture-specific because the frame
 *                  layout and register sanitisation depend on the ISA.
 *
 * regs       : current trapframe (will be overwritten on success).
 *              Points to the arch-specific register struct (e.g. struct registers on x86).
 * user_frame : user-space pointer to the signal frame pushed by the
 *              signal delivery trampoline.
 *
 * Returns 0 on success, negative errno on failure.
 */
int arch_sigreturn(void* regs, const void* user_frame);

#endif /* ARCH_SIGNAL_H */
