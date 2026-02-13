#ifndef ARCH_X86_ARCH_SYSCALL_H
#define ARCH_X86_ARCH_SYSCALL_H

/*
 * x86 syscall ABI register mapping (Linux INT 0x80 convention).
 *
 * These macros abstract the mapping between syscall arguments and CPU
 * registers so that the generic syscall dispatcher (syscall.c) contains
 * no architecture-specific register names.
 *
 * All macros expand to lvalues so they can be used for both reading
 * arguments and writing the return value.
 *
 *   syscall number : eax
 *   arg0           : ebx
 *   arg1           : ecx
 *   arg2           : edx
 *   arg3           : esi
 *   arg4           : edi
 *   return value   : eax
 *   user ip        : eip      (set by execve to jump to new entry)
 *   user sp        : useresp  (set by execve to new user stack)
 */

#define sc_num(r)   ((r)->eax)
#define sc_arg0(r)  ((r)->ebx)
#define sc_arg1(r)  ((r)->ecx)
#define sc_arg2(r)  ((r)->edx)
#define sc_arg3(r)  ((r)->esi)
#define sc_arg4(r)  ((r)->edi)
#define sc_ret(r)   ((r)->eax)
#define sc_ip(r)    ((r)->eip)
#define sc_usp(r)   ((r)->useresp)

#endif /* ARCH_X86_ARCH_SYSCALL_H */
