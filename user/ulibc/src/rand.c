// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "stdlib.h"

static unsigned long _rand_seed = 1;

void srand(unsigned int seed) {
    _rand_seed = seed;
}

int rand(void) {
    _rand_seed = _rand_seed * 1103515245UL + 12345UL;
    return (int)((_rand_seed >> 16) & 0x7FFF);
}

#define RAND_MAX 0x7FFF
