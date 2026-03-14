// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/*
 * Newlib crt0 for AdrOS (i686)
 *
 * Entry point: _start → main(argc, argv, envp) → exit()
 *
 * Stack layout at entry (set up by execve):
 *   [ESP+0]  argc
 *   [ESP+4]  argv[0], argv[1], ..., NULL
 *   [...]    envp[0], envp[1], ..., NULL
 */
.section .text
.global _start
.extern main
.extern exit
.extern environ
.extern __libc_init_array
.extern __libc_fini_array

_start:
    /* Set up user data segments (AdrOS ring3 selector = 0x23) */
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

    /* Store envp in environ global */
    mov %edx, environ

    /* Save argc/argv/envp across init_array call */
    push %edx               /* envp - save on stack */
    push %ecx               /* argv */
    push %eax               /* argc */

    /* Call C++ global constructors / Newlib init */
    call __libc_init_array

    /* Restore argc/argv/envp and call main */
    pop %eax                /* argc */
    pop %ecx                /* argv */
    pop %edx                /* envp */

    push %edx               /* envp */
    push %ecx               /* argv */
    push %eax               /* argc */
    call main
    add $12, %esp

    /* Call C++ global destructors / Newlib fini */
    push %eax               /* save main return value */
    call __libc_fini_array
    pop %eax

    /* exit(main return value) */
    push %eax
    call exit

    /* Should never reach here */
1:  jmp 1b

.section .note.GNU-stack,"",@progbits
