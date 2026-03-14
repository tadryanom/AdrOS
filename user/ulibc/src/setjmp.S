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
 * setjmp / longjmp for i386
 *
 * jmp_buf layout: [0]=EBX [1]=ESI [2]=EDI [3]=EBP [4]=ESP [5]=EIP
 */
.text

.global setjmp
.global _setjmp
.global longjmp
.global _longjmp
.global sigsetjmp
.global siglongjmp

/* int setjmp(jmp_buf env) */
setjmp:
_setjmp:
    mov  4(%esp), %eax       /* eax = env */
    mov  %ebx, 0(%eax)
    mov  %esi, 4(%eax)
    mov  %edi, 8(%eax)
    mov  %ebp, 12(%eax)
    lea  4(%esp), %ecx       /* caller's ESP (before call pushed retaddr) */
    mov  %ecx, 16(%eax)
    mov  (%esp), %ecx        /* return address = caller's EIP */
    mov  %ecx, 20(%eax)
    xor  %eax, %eax          /* return 0 */
    ret

/* void longjmp(jmp_buf env, int val) */
longjmp:
_longjmp:
    mov  4(%esp), %edx       /* edx = env */
    mov  8(%esp), %eax       /* eax = val */
    test %eax, %eax
    jnz  1f
    inc  %eax                /* if val==0, return 1 */
1:
    mov  0(%edx), %ebx
    mov  4(%edx), %esi
    mov  8(%edx), %edi
    mov  12(%edx), %ebp
    mov  16(%edx), %esp
    jmp  *20(%edx)           /* jump to saved EIP */

/* int sigsetjmp(sigjmp_buf env, int savesigs) — minimal, no signal mask save */
sigsetjmp:
    mov  4(%esp), %eax       /* eax = env */
    mov  %ebx, 0(%eax)
    mov  %esi, 4(%eax)
    mov  %edi, 8(%eax)
    mov  %ebp, 12(%eax)
    lea  4(%esp), %ecx
    mov  %ecx, 16(%eax)
    mov  (%esp), %ecx
    mov  %ecx, 20(%eax)
    /* env[6] = savesigs flag */
    mov  8(%esp), %ecx
    mov  %ecx, 24(%eax)
    /* env[7] = 0 (signal mask placeholder) */
    movl $0, 28(%eax)
    xor  %eax, %eax
    ret

/* void siglongjmp(sigjmp_buf env, int val) */
siglongjmp:
    mov  4(%esp), %edx
    mov  8(%esp), %eax
    test %eax, %eax
    jnz  1f
    inc  %eax
1:
    mov  0(%edx), %ebx
    mov  4(%edx), %esi
    mov  8(%edx), %edi
    mov  12(%edx), %ebp
    mov  16(%edx), %esp
    jmp  *20(%edx)

.section .note.GNU-stack,"",@progbits
