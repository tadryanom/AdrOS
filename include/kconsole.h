// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef KCONSOLE_H
#define KCONSOLE_H

// Enter the kernel emergency console (kernel-mode, like HelenOS kconsole).
// Called when VFS mount or init fails. Runs in a loop until reboot.
void kconsole_enter(void);

#endif
