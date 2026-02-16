#ifndef LZ4_H
#define LZ4_H

#include <stdint.h>
#include <stddef.h>

/*
 * LZ4 block decompressor for AdrOS.
 *
 * Decompresses a single LZ4 block (raw format, no frame header).
 *
 * src       - pointer to compressed data
 * src_size  - size of compressed data in bytes
 * dst       - pointer to output buffer (must be >= dst_cap bytes)
 * dst_cap   - capacity of the output buffer
 *
 * Returns the number of bytes written to dst, or negative on error.
 */
int lz4_decompress_block(const void *src, size_t src_size,
                         void *dst, size_t dst_cap);

/*
 * LZ4 Frame decompressor (official LZ4 Frame format).
 *
 * Reference: https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md
 *
 * Parses the frame header, decompresses all data blocks, and optionally
 * verifies the content checksum.
 *
 * src       - pointer to LZ4 frame data (starts with magic 0x184D2204)
 * src_size  - total size of frame data in bytes
 * dst       - pointer to output buffer
 * dst_cap   - capacity of the output buffer
 *
 * Returns total decompressed bytes, or negative on error.
 */
int lz4_decompress_frame(const void *src, size_t src_size,
                         void *dst, size_t dst_cap);

/* Official LZ4 Frame magic number (little-endian) */
#define LZ4_FRAME_MAGIC  0x184D2204U

/* Legacy custom "LZ4B" magic (kept for backward-compat detection) */
#define LZ4B_MAGIC_U32   0x42345A4CU   /* "LZ4B" as little-endian uint32 */
#define LZ4B_HDR_SIZE    12

#endif /* LZ4_H */
