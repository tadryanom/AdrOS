/*
 * ulibc crt0 — C runtime startup for AdrOS userspace
 * Entry point: _start → main(argc, argv, envp) → exit()
 *
 * Stack layout at entry (set up by execve or ld.so):
 *   [ESP+0]  argc
 *   [ESP+4]  argv[0], argv[1], ..., NULL
 *   [...]    envp[0], envp[1], ..., NULL
 *   [...]    auxv entries (if ld.so present)
 */
.section .text
.global _start
.extern main
.extern exit
.extern __environ

_start:
    /* Set up user data segments */
    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* Parse stack: argc at (%esp), argv at 4(%esp) */
    mov (%esp), %eax        /* argc */
    lea 4(%esp), %ecx       /* argv = &argv[0] */

    /* envp = argv + (argc + 1) * 4  (skip past argv[] and its NULL) */
    mov %eax, %edx
    add $1, %edx
    shl $2, %edx
    add %ecx, %edx          /* edx = envp */

    /* Store envp in __environ global (weak, may not exist) */
    mov %edx, __environ

    /* Call main(argc, argv, envp) */
    push %edx               /* envp */
    push %ecx               /* argv */
    push %eax               /* argc */
    call main
    add $12, %esp

    /* exit(main return value) */
    push %eax
    call exit

    /* Should never reach here */
1:  jmp 1b

.section .note.GNU-stack,"",@progbits
