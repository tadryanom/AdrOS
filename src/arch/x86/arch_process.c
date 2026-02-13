#include "arch_process.h"
#include "arch/x86/idt.h"

#if defined(__i386__)

/*
 * x86 kernel stack layout expected by context_switch (process.S):
 *
 *   context_switch saves:  pushf, push edi, esi, ebx, ebp
 *   context_switch restores: popf, pop edi, esi, ebx, ebp, ret
 *
 * So for a NEW process we build a fake frame that context_switch will
 * "restore":
 *
 *   sp -> [EFLAGS  0x202]   <- popf  (IF=1, reserved bit 1)
 *         [EDI     0]       <- pop edi
 *         [ESI     0]       <- pop esi
 *         [EBX     0]       <- pop ebx
 *         [EBP     0]       <- pop ebp
 *         [wrapper addr]    <- ret jumps here
 *         [0  (fake retaddr for wrapper)]
 *         [arg]             <- first argument to wrapper (cdecl)
 */
uintptr_t arch_kstack_init(void* stack_top,
                            void (*wrapper)(void (*)(void)),
                            void (*arg)(void))
{
    uint32_t* sp = (uint32_t*)stack_top;

    *--sp = (uint32_t)(uintptr_t)arg;       /* argument for wrapper */
    *--sp = 0;                               /* fake return address  */
    *--sp = (uint32_t)(uintptr_t)wrapper;    /* ret target           */
    *--sp = 0;                               /* EBP                  */
    *--sp = 0;                               /* EBX                  */
    *--sp = 0;                               /* ESI                  */
    *--sp = 0;                               /* EDI                  */
    *--sp = 0x002;                           /* EFLAGS: IF=0 (thread_wrapper enables interrupts) */

    return (uintptr_t)sp;
}

void arch_regs_set_retval(void* opaque, uint32_t val)
{
    struct registers* regs = (struct registers*)opaque;
    if (regs) regs->eax = val;
}

void arch_regs_set_ustack(void* opaque, uintptr_t sp)
{
    struct registers* regs = (struct registers*)opaque;
    if (regs) regs->useresp = (uint32_t)sp;
}

#endif /* __i386__ */
