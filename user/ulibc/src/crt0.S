/*
 * ulibc crt0 — C runtime startup for AdrOS userspace
 * Entry point: _start → main() → exit()
 */
.section .text
.global _start
.extern main
.extern exit

_start:
    /* Set up user data segments */
    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* Call main() — no argc/argv yet */
    push $0         /* envp = NULL */
    push $0         /* argv = NULL */
    push $0         /* argc = 0 */
    call main
    add $12, %esp

    /* exit(main return value) */
    push %eax
    call exit

    /* Should never reach here */
1:  jmp 1b

.section .note.GNU-stack,"",@progbits
