#include "lz4.h"

/*
 * LZ4 block decompressor — minimal, standalone, no dependencies beyond memcpy.
 *
 * Reference: https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md
 *
 * Each sequence:
 *   token byte  — high nibble = literal length, low nibble = match length - 4
 *   [extra literal length bytes if high nibble == 15]
 *   literal data
 *   (if not last sequence):
 *     match offset  — 2 bytes little-endian
 *     [extra match length bytes if low nibble == 15]
 */

int lz4_decompress_block(const void *src, size_t src_size,
                         void *dst, size_t dst_cap)
{
    const uint8_t *ip  = (const uint8_t *)src;
    const uint8_t *ip_end = ip + src_size;
    uint8_t       *op  = (uint8_t *)dst;
    uint8_t       *op_end = op + dst_cap;

    for (;;) {
        if (ip >= ip_end) return -1;        /* truncated input */

        /* --- token --- */
        uint8_t token = *ip++;
        size_t lit_len = (size_t)(token >> 4);
        size_t match_len = (size_t)(token & 0x0F);

        /* extended literal length */
        if (lit_len == 15) {
            uint8_t extra;
            do {
                if (ip >= ip_end) return -1;
                extra = *ip++;
                lit_len += extra;
            } while (extra == 255);
        }

        /* copy literals */
        if (ip + lit_len > ip_end) return -1;
        if (op + lit_len > op_end) return -1;
        for (size_t i = 0; i < lit_len; i++) op[i] = ip[i];
        ip += lit_len;
        op += lit_len;

        /* last sequence has no match part — ends right after literals */
        if (ip >= ip_end) break;

        /* --- match offset (16-bit LE) --- */
        if (ip + 2 > ip_end) return -1;
        size_t offset = (size_t)ip[0] | ((size_t)ip[1] << 8);
        ip += 2;
        if (offset == 0) return -1;            /* offset 0 is invalid */

        /* extended match length */
        if (match_len == 15) {
            uint8_t extra;
            do {
                if (ip >= ip_end) return -1;
                extra = *ip++;
                match_len += extra;
            } while (extra == 255);
        }
        match_len += 4;  /* minimum match length is 4 */

        /* copy match (byte-by-byte for overlapping copies) */
        const uint8_t *match_src = op - offset;
        if (match_src < (const uint8_t *)dst) return -1;  /* underflow */
        if (op + match_len > op_end) return -1;
        for (size_t i = 0; i < match_len; i++) op[i] = match_src[i];
        op += match_len;
    }

    return (int)(op - (uint8_t *)dst);
}
