// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SETJMP_H
#define ULIBC_SETJMP_H

/* jmp_buf layout for i386:
 *   [0] EBX  [1] ESI  [2] EDI  [3] EBP  [4] ESP  [5] EIP
 */
typedef unsigned long jmp_buf[6];
typedef unsigned long sigjmp_buf[6 + 1 + 1]; /* +saved_sigmask +sigmask */

int  setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val) __attribute__((noreturn));
int  sigsetjmp(sigjmp_buf env, int savesigs);
void siglongjmp(sigjmp_buf env, int val) __attribute__((noreturn));

int  _setjmp(jmp_buf env);
void _longjmp(jmp_buf env, int val) __attribute__((noreturn));

#endif
