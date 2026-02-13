#ifndef ARCH_SYSCALL_H
#define ARCH_SYSCALL_H

/*
 * Architecture-agnostic syscall register mapping.
 *
 * Each architecture provides macros that map generic syscall
 * argument / return-value names to the concrete CPU registers
 * in struct registers:
 *
 *   sc_num(r)   — read the syscall number
 *   sc_arg0(r)  — first user argument
 *   sc_arg1(r)  — second user argument
 *   sc_arg2(r)  — third user argument
 *   sc_arg3(r)  — fourth user argument
 *   sc_arg4(r)  — fifth user argument
 *   sc_ret(r)   — return value  (lvalue, writable)
 *   sc_ip(r)    — user instruction pointer (lvalue, for execve)
 *   sc_usp(r)   — user stack pointer      (lvalue, for execve)
 *
 * Include "interrupts.h" before this header so that struct registers
 * is visible.
 */

#include "interrupts.h"

#if defined(__i386__) || defined(__x86_64__)
#include "arch/x86/arch_syscall.h"
#else

/* Stub for non-x86 — replace with real arch header when porting. */
extern uint32_t __arch_syscall_stub_sink;
#define sc_num(r)   ((r)->int_no)
#define sc_arg0(r)  ((r)->int_no)
#define sc_arg1(r)  ((r)->int_no)
#define sc_arg2(r)  ((r)->int_no)
#define sc_arg3(r)  ((r)->int_no)
#define sc_arg4(r)  ((r)->int_no)
#define sc_ret(r)   (__arch_syscall_stub_sink)
#define sc_ip(r)    (__arch_syscall_stub_sink)
#define sc_usp(r)   (__arch_syscall_stub_sink)

#endif

/*
 * arch_syscall_init — Register the syscall entry point(s) for this
 *                     architecture (e.g. INT 0x80 + SYSENTER on x86).
 *                     Called once from the generic syscall_init().
 */
void arch_syscall_init(void);

#endif /* ARCH_SYSCALL_H */
