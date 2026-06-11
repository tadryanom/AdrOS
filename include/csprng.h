// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef CSPRNG_H
#define CSPRNG_H

#include <stdint.h>

/* Initialize the central CSPRNG with boot entropy */
void csprng_init(void);

/* Add entropy to the CSPRNG (for /dev/random writes) */
void csprng_add_entropy(const uint8_t* data, uint32_t len);

/* Generate random bytes */
void csprng_get_bytes(uint8_t* out, uint32_t len);

/* Generate 32-bit random value */
uint32_t csprng_get_u32(void);

/* Generate 64-bit random value */
uint64_t csprng_get_u64(void);

#endif
