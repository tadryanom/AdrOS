// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "hal/cpu_features.h"
#include "console.h"

#include <stddef.h>

static struct cpu_features g_default_features;

__attribute__((weak))
void hal_cpu_detect_features(void) {
    for (size_t i = 0; i < sizeof(g_default_features); i++)
        ((uint8_t*)&g_default_features)[i] = 0;
    kprintf("[CPU] No arch-specific feature detection.\n");
}

__attribute__((weak))
const struct cpu_features* hal_cpu_get_features(void) {
    return &g_default_features;
}

__attribute__((weak))
void hal_cpu_print_features(void) {
    kprintf("[CPU] Feature detection not available.\n");
}
