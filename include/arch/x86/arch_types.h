// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_X86_TYPES_H
#define ARCH_X86_TYPES_H

/*
 * Size in bytes of the saved user-mode register frame (struct registers).
 * x86: gs(4) + ds(4) + pusha(32) + int_no+err(8) + iret_frame(20) = 68
 *
 * Used by struct process to hold an opaque register snapshot without
 * pulling in the full x86 IDT / register definitions.
 */
#define ARCH_REGS_SIZE 68

#endif /* ARCH_X86_TYPES_H */
