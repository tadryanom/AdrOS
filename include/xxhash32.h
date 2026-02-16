#ifndef XXHASH32_H
#define XXHASH32_H

/*
 * xxHash-32 — standalone, header-only implementation.
 *
 * Reference: https://github.com/Cyan4973/xxHash/blob/dev/doc/xxhash_spec.md
 *
 * Used by the LZ4 Frame format for header and content checksums.
 * Works in both freestanding (kernel) and hosted (tools) environments.
 */

#include <stdint.h>
#include <stddef.h>

#define XXH_PRIME32_1  0x9E3779B1U
#define XXH_PRIME32_2  0x85EBCA77U
#define XXH_PRIME32_3  0xC2B2AE3DU
#define XXH_PRIME32_4  0x27D4EB2FU
#define XXH_PRIME32_5  0x165667B1U

static inline uint32_t xxh32_rotl(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

static inline uint32_t xxh32_read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint32_t xxh32(const void *input, size_t len, uint32_t seed) {
    const uint8_t *p = (const uint8_t *)input;
    const uint8_t *end = p + len;
    uint32_t h32;

    if (len >= 16) {
        const uint8_t *limit = end - 16;
        uint32_t v1 = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
        uint32_t v2 = seed + XXH_PRIME32_2;
        uint32_t v3 = seed + 0;
        uint32_t v4 = seed - XXH_PRIME32_1;

        do {
            v1 += xxh32_read32(p) * XXH_PRIME32_2;
            v1 = xxh32_rotl(v1, 13) * XXH_PRIME32_1;
            p += 4;
            v2 += xxh32_read32(p) * XXH_PRIME32_2;
            v2 = xxh32_rotl(v2, 13) * XXH_PRIME32_1;
            p += 4;
            v3 += xxh32_read32(p) * XXH_PRIME32_2;
            v3 = xxh32_rotl(v3, 13) * XXH_PRIME32_1;
            p += 4;
            v4 += xxh32_read32(p) * XXH_PRIME32_2;
            v4 = xxh32_rotl(v4, 13) * XXH_PRIME32_1;
            p += 4;
        } while (p <= limit);

        h32 = xxh32_rotl(v1, 1) + xxh32_rotl(v2, 7) +
              xxh32_rotl(v3, 12) + xxh32_rotl(v4, 18);
    } else {
        h32 = seed + XXH_PRIME32_5;
    }

    h32 += (uint32_t)len;

    while (p + 4 <= end) {
        h32 += xxh32_read32(p) * XXH_PRIME32_3;
        h32 = xxh32_rotl(h32, 17) * XXH_PRIME32_4;
        p += 4;
    }

    while (p < end) {
        h32 += (uint32_t)(*p) * XXH_PRIME32_5;
        h32 = xxh32_rotl(h32, 11) * XXH_PRIME32_1;
        p++;
    }

    h32 ^= h32 >> 15;
    h32 *= XXH_PRIME32_2;
    h32 ^= h32 >> 13;
    h32 *= XXH_PRIME32_3;
    h32 ^= h32 >> 16;

    return h32;
}

#endif /* XXHASH32_H */
