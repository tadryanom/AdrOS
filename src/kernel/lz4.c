#include "lz4.h"
#include "xxhash32.h"

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

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_le64(const uint8_t *p) {
    return (uint64_t)read_le32(p) | ((uint64_t)read_le32(p + 4) << 32);
}

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

/*
 * LZ4 Frame decompressor — official LZ4 Frame format.
 *
 * Reference: https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md
 *
 * Supports:
 *   - Block-independent mode
 *   - Content size field (optional, used for validation)
 *   - Content checksum (optional, verified if present)
 *   - Single and multi-block frames
 *
 * Does NOT support:
 *   - Block checksums (skipped if flag set)
 *   - Linked blocks (returns error)
 *   - Dictionary IDs (ignored)
 */
int lz4_decompress_frame(const void *src, size_t src_size,
                         void *dst, size_t dst_cap)
{
    const uint8_t *ip = (const uint8_t *)src;
    const uint8_t *ip_end = ip + src_size;
    uint8_t *op = (uint8_t *)dst;
    size_t total_out = 0;

    /* --- Magic Number (4 bytes) --- */
    if (src_size < 7) return -1;  /* minimum: magic + FLG + BD + HC */
    if (read_le32(ip) != LZ4_FRAME_MAGIC) return -1;
    ip += 4;

    /* --- Frame Descriptor --- */
    const uint8_t *desc_start = ip;

    uint8_t flg = *ip++;
    uint8_t bd  = *ip++;
    (void)bd;  /* block max size — not enforced by decompressor */

    /* Parse FLG */
    int version         = (flg >> 6) & 0x03;
    int block_indep     = (flg >> 5) & 0x01;
    int block_checksum  = (flg >> 4) & 0x01;
    int content_size_flag = (flg >> 3) & 0x01;
    int content_checksum = (flg >> 2) & 0x01;
    /* bit 1 reserved, bit 0 = dict ID */
    int dict_id_flag    = (flg >> 0) & 0x01;

    if (version != 1) return -1;      /* only version 01 defined */
    if (!block_indep) return -1;       /* linked blocks not supported */

    uint64_t content_size = 0;
    if (content_size_flag) {
        if (ip + 8 > ip_end) return -1;
        content_size = read_le64(ip);
        ip += 8;
    }

    if (dict_id_flag) {
        if (ip + 4 > ip_end) return -1;
        ip += 4;  /* skip dictionary ID */
    }

    /* Header Checksum (1 byte) = (xxHash32(descriptor) >> 8) & 0xFF */
    if (ip + 1 > ip_end) return -1;
    {
        size_t desc_len = (size_t)(ip - desc_start);
        uint8_t expected_hc = (uint8_t)((xxh32(desc_start, desc_len, 0) >> 8) & 0xFF);
        if (*ip != expected_hc) return -1;
    }
    ip++;

    /* --- Data Blocks --- */
    for (;;) {
        if (ip + 4 > ip_end) return -1;
        uint32_t block_size = read_le32(ip);
        ip += 4;

        if (block_size == 0) break;  /* EndMark */

        int is_uncompressed = (block_size >> 31) & 1;
        block_size &= 0x7FFFFFFFU;

        if (ip + block_size > ip_end) return -1;

        if (is_uncompressed) {
            if (total_out + block_size > dst_cap) return -1;
            for (uint32_t i = 0; i < block_size; i++)
                op[i] = ip[i];
            op += block_size;
            total_out += block_size;
        } else {
            int ret = lz4_decompress_block(ip, block_size,
                                           op, dst_cap - total_out);
            if (ret < 0) return -1;
            op += ret;
            total_out += (size_t)ret;
        }
        ip += block_size;

        /* Skip block checksum if present */
        if (block_checksum) {
            if (ip + 4 > ip_end) return -1;
            ip += 4;
        }
    }

    /* --- Content Checksum (optional) --- */
    if (content_checksum) {
        if (ip + 4 > ip_end) return -1;
        uint32_t expected = read_le32(ip);
        uint32_t actual = xxh32(dst, total_out, 0);
        if (expected != actual) return -1;
    }

    /* Validate content size if declared */
    if (content_size_flag && (uint64_t)total_out != content_size)
        return -1;

    return (int)total_out;
}
