// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _SHADOW_H
#define _SHADOW_H
struct spwd {
    char* sp_namp; char* sp_pwdp;
    long sp_lstchg; long sp_min; long sp_max;
    long sp_warn; long sp_inact; long sp_expire;
    unsigned long sp_flag;
};
struct spwd* getspnam(const char* name);
void endspent(void);
#endif
