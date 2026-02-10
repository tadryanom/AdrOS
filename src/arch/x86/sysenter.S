/*
 * AdrOS — SYSENTER fast syscall entry (x86-32)
 *
 * Userspace calling convention:
 *   EAX = syscall number
 *   EBX = arg1
 *   ECX = arg2
 *   EDX = arg3
 *   ESI = arg4
 *   EDI = arg5
 *
 * Before invoking SYSENTER, userspace must:
 *   push %ecx          ; save arg2 (SYSENTER clobbers ECX)
 *   push %edx          ; save arg3 (SYSENTER clobbers EDX)
 *   push $return_eip   ; push return address
 *   mov  %esp, %ecx    ; ECX = user ESP (for kernel to read)
 *   lea  return_eip, %edx  ; EDX = return EIP (unused, we read from stack)
 *   sysenter
 * return_eip:
 *   pop %edx           ; restore arg3
 *   pop %ecx           ; restore arg2
 *   ; EAX = return value
 *
 * Kernel entry:
 *   ESP = kernel stack (from MSR 0x175)
 *   ECX = user ESP (points to: [return_eip, saved_edx, saved_ecx])
 *   EDX = user return EIP
 *
 * We build a struct registers frame on the kernel stack so that
 * the existing syscall_handler() works unchanged.
 *
 * struct registers (low addr → high addr):
 *   ds edi esi ebp esp_dummy ebx edx ecx eax int_no err_code
 *   eip cs eflags useresp ss
 */

.section .text

.extern syscall_handler

.global sysenter_entry
sysenter_entry:
    /*
     * State at entry:
     *   Ring 0, interrupts disabled.
     *   ESP = per-task kernel stack top (MSR 0x175)
     *   ECX = user ESP
     *   EDX = user return EIP
     *   EAX = syscall number
     *   EBX = arg1, ESI = arg4, EDI = arg5
     *   User stack: [return_eip, saved_edx(arg3), saved_ecx(arg2)]
     */

    /* Build iret-style frame: ss, useresp, eflags, cs, eip */
    push $0x23          /* ss  = user data segment (GDT entry 4 | RPL 3) */
    push %ecx           /* useresp = user ESP */
    pushf               /* eflags (kernel flags — close enough, user had IF set) */
    orl $0x200, (%esp)  /* ensure IF is set in saved eflags */
    push $0x1B          /* cs  = user code segment (GDT entry 3 | RPL 3) */
    push %edx           /* eip = user return EIP */

    /* int_no and err_code (fake, for struct registers compatibility) */
    push $0             /* err_code */
    push $128           /* int_no = 128 (same as INT 0x80) */

    /* Recover arg2 (ECX) and arg3 (EDX) from user stack.
     * User stack layout at ECX: [return_eip][saved_edx][saved_ecx]
     * We already have return_eip in EDX, so:
     *   real arg3 = *(ECX + 4)
     *   real arg2 = *(ECX + 8)
     */
    mov 4(%ecx), %edx   /* EDX = arg3 (was saved by user) */
    mov 8(%ecx), %ecx   /* ECX = arg2 (was saved by user) */

    /* pusha-equivalent: eax ecx edx ebx esp ebp esi edi */
    push %eax
    push %ecx
    push %edx
    push %ebx
    push %esp           /* esp_dummy (not used) */
    push %ebp
    push %esi
    push %edi

    /* ds */
    push $0x10          /* kernel data segment */

    /* Load kernel segments */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* Enable interrupts (SYSENTER disables them) */
    sti

    /* Call syscall_handler(regs) */
    push %esp
    call syscall_handler
    add $4, %esp

    /* Disable interrupts for SYSEXIT */
    cli

    /* Restore registers from the frame.
     * syscall_handler may have modified regs->eax (return value).
     */
    pop %eax            /* ds (discard) */
    pop %edi
    pop %esi
    pop %ebp
    add $4, %esp        /* skip esp_dummy */
    pop %ebx
    pop %edx            /* arg3 (don't care, user will pop from stack) */
    pop %ecx            /* arg2 (don't care) */
    pop %eax            /* return value! */

    add $8, %esp        /* skip int_no, err_code */

    /* Now stack has: eip, cs, eflags, useresp, ss */
    /* SYSEXIT: ECX = user ESP, EDX = user EIP */
    mov 12(%esp), %ecx  /* useresp */
    mov 0(%esp), %edx   /* eip (user return address) */

    /* Restore user segments before returning */
    push $0x23
    pop %ds
    push $0x23
    pop %es

    sti
    sysexit

.section .note.GNU-stack,"",@progbits
