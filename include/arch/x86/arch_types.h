#ifndef ARCH_X86_TYPES_H
#define ARCH_X86_TYPES_H

/*
 * Size in bytes of the saved user-mode register frame (struct registers).
 * x86: ds(4) + pusha(32) + int_no+err(8) + iret_frame(20) = 64
 *
 * Used by struct process to hold an opaque register snapshot without
 * pulling in the full x86 IDT / register definitions.
 */
#define ARCH_REGS_SIZE 64

#endif /* ARCH_X86_TYPES_H */
