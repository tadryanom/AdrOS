// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_PRCTL_H
#define _SYS_PRCTL_H
#define PR_SET_NAME 15
#define PR_GET_NAME 16
int prctl(int option, ...);
#endif
