// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_MATH_H
#define ULIBC_MATH_H

static inline double fabs(double x) { return x < 0 ? -x : x; }
static inline float fabsf(float x) { return x < 0 ? -x : x; }

#endif
