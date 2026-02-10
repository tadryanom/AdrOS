// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef KCONSOLE_H
#define KCONSOLE_H

// Enter the kernel emergency console (kernel-mode, like HelenOS kconsole).
// Called when VFS mount or init fails. Runs in a loop until reboot.
void kconsole_enter(void);

#endif
