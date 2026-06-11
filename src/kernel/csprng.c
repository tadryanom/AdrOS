// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "csprng.h"
#include "spinlock.h"
#include "timer.h"
#include "utils.h"

#include <stddef.h>

/* M8: Central CSPRNG for kernel cryptographic randomness
 * Uses a ChaCha20-based DRBG with entropy accumulation from:
 * - RDTSC (high-resolution timer)
 * - Timer tick count
 * - Interrupt timing variations
 * - User-provided entropy via /dev/random writes
 */

static spinlock_t g_csprng_lock = {0};

/* ChaCha20 state (simplified for DRBG use) */
static struct {
    uint32_t state[16];
    uint32_t counter;
    uint8_t  initialized;
} g_csprng_chacha;

/* Entropy pool */
static struct {
    uint8_t pool[64];
    uint32_t pool_pos;
    uint32_t reseed_counter;
} g_entropy_pool;

/* Rotate left macro */
#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

/* ChaCha20 quarter round */
static void chacha_quarter_round(uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    *a += *b; *d ^= *a; *d = ROTL32(*d, 16);
    *c += *d; *b ^= *c; *b = ROTL32(*b, 12);
    *a += *b; *d ^= *a; *d = ROTL32(*d, 8);
    *c += *d; *b ^= *c; *b = ROTL32(*b, 7);
}

/* ChaCha20 block function (single round for DRBG) */
static void chacha20_block(uint32_t* state) {
    for (int i = 0; i < 10; i++) {
        /* Column rounds */
        chacha_quarter_round(&state[0], &state[4], &state[8],  &state[12]);
        chacha_quarter_round(&state[1], &state[5], &state[9],  &state[13]);
        chacha_quarter_round(&state[2], &state[6], &state[10], &state[14]);
        chacha_quarter_round(&state[3], &state[7], &state[11], &state[15]);
        
        /* Diagonal rounds */
        chacha_quarter_round(&state[0], &state[5], &state[10], &state[15]);
        chacha_quarter_round(&state[1], &state[6], &state[11], &state[12]);
        chacha_quarter_round(&state[2], &state[7], &state[8],  &state[13]);
        chacha_quarter_round(&state[3], &state[4], &state[9],  &state[14]);
    }
}

/* Read RDTSC for entropy */
static uint64_t rdtsc_entropy(void) {
    uint64_t tsc = 0;
    /* Inline RDTSC */
    __asm__ volatile("rdtsc" : "=A"(tsc));
    return tsc;
}

/* Mix entropy into pool */
static void entropy_mix(const uint8_t* data, uint32_t len) {
    uintptr_t irqf = spin_lock_irqsave(&g_csprng_lock);
    
    for (uint32_t i = 0; i < len; i++) {
        g_entropy_pool.pool[g_entropy_pool.pool_pos] ^= data[i];
        g_entropy_pool.pool_pos = (g_entropy_pool.pool_pos + 1) % 64;
    }
    
    spin_unlock_irqrestore(&g_csprng_lock, irqf);
}

/* Initialize CSPRNG with boot entropy */
void csprng_init(void) {
    uintptr_t irqf = spin_lock_irqsave(&g_csprng_lock);
    
    /* Seed with multiple entropy sources */
    uint64_t tsc = rdtsc_entropy();
    uint32_t ticks = get_tick_count();
    
    /* Initialize ChaCha20 state with "expand 32-byte k" constant */
    const char* constant = "expand 32-byte k";
    for (int i = 0; i < 4; i++) {
        g_csprng_chacha.state[i] = ((uint32_t)constant[i*4+0]) << 0 |
                                   ((uint32_t)constant[i*4+1]) << 8 |
                                   ((uint32_t)constant[i*4+2]) << 16 |
                                   ((uint32_t)constant[i*4+3]) << 24;
    }
    
    /* Mix in entropy */
    for (int i = 4; i < 16; i++) {
        uint64_t mix = tsc ^ ((uint64_t)ticks << 32);
        mix += i * 0x9E3779B9; /* Golden ratio */
        g_csprng_chacha.state[i] = (uint32_t)(mix ^ (mix >> 32));
        tsc ^= (mix << 13) | (mix >> 51);
    }
    
    g_csprng_chacha.counter = 1;
    g_csprng_chacha.initialized = 1;
    
    /* Initialize entropy pool */
    for (int i = 0; i < 64; i++) {
        g_entropy_pool.pool[i] = (uint8_t)(tsc >> (i * 8));
    }
    g_entropy_pool.pool_pos = 0;
    g_entropy_pool.reseed_counter = 0;
    
    spin_unlock_irqrestore(&g_csprng_lock, irqf);
}

/* Add entropy to CSPRNG (for /dev/random writes) */
void csprng_add_entropy(const uint8_t* data, uint32_t len) {
    if (!data || len == 0) return;
    
    entropy_mix(data, len);
    
    uintptr_t irqf = spin_lock_irqsave(&g_csprng_lock);
    g_entropy_pool.reseed_counter++;
    
    /* Reseed every 256 entropy additions */
    if (g_entropy_pool.reseed_counter >= 256) {
        /* Mix entropy pool into ChaCha20 state */
        for (int i = 0; i < 16; i++) {
            uint32_t pool_word = 0;
            for (int j = 0; j < 4; j++) {
                pool_word |= ((uint32_t)g_entropy_pool.pool[(i*4 + j) % 64]) << (j * 8);
            }
            g_csprng_chacha.state[i] ^= pool_word;
        }
        g_entropy_pool.reseed_counter = 0;
    }
    
    spin_unlock_irqrestore(&g_csprng_lock, irqf);
}

/* Generate random bytes */
void csprng_get_bytes(uint8_t* out, uint32_t len) {
    if (!out || len == 0) return;
    
    uintptr_t irqf = spin_lock_irqsave(&g_csprng_lock);
    
    if (!g_csprng_chacha.initialized) {
        spin_unlock_irqrestore(&g_csprng_lock, irqf);
        csprng_init();
        irqf = spin_lock_irqsave(&g_csprng_lock);
    }
    
    /* Add timing entropy before generation */
    uint64_t tsc = rdtsc_entropy();
    g_csprng_chacha.state[12] ^= (uint32_t)tsc;
    g_csprng_chacha.state[13] ^= (uint32_t)(tsc >> 32);
    
    for (uint32_t i = 0; i < len; i++) {
        if ((i & 63) == 0) {
            /* Generate new block every 64 bytes */
            g_csprng_chacha.counter++;
            g_csprng_chacha.state[12] = g_csprng_chacha.counter;
            chacha20_block(g_csprng_chacha.state);
        }
        
        /* Extract byte from state (little-endian) */
        uint32_t word_idx = ((i & 63) / 4) % 16;
        uint8_t byte_idx = (i & 3);
        out[i] = (g_csprng_chacha.state[word_idx] >> (byte_idx * 8)) & 0xFF;
    }
    
    spin_unlock_irqrestore(&g_csprng_lock, irqf);
}

/* Generate 32-bit random value */
uint32_t csprng_get_u32(void) {
    uint32_t out;
    csprng_get_bytes((uint8_t*)&out, 4);
    return out;
}

/* Generate 64-bit random value */
uint64_t csprng_get_u64(void) {
    uint64_t out;
    csprng_get_bytes((uint8_t*)&out, 8);
    return out;
}
