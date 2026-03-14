// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _BYTESWAP_H
#define _BYTESWAP_H

#include <stdint.h>

static inline uint16_t bswap_16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}

static inline uint32_t bswap_32(uint32_t x) {
    return ((x >> 24) & 0x000000FFU) |
           ((x >>  8) & 0x0000FF00U) |
           ((x <<  8) & 0x00FF0000U) |
           ((x << 24) & 0xFF000000U);
}

static inline uint64_t bswap_64(uint64_t x) {
    return ((uint64_t)bswap_32((uint32_t)x) << 32) |
           (uint64_t)bswap_32((uint32_t)(x >> 32));
}

#endif
