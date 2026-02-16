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
int lz4_decompress_block(const void* src, size_t src_size,
                         void* dst, size_t dst_cap);

/*
 * InitRD LZ4 wrapper header (prepended to compressed tar):
 *   [0..3]  magic   "LZ4B"
 *   [4..7]  orig_sz  uint32_t LE — uncompressed size
 *   [8..11] comp_sz  uint32_t LE — compressed block size
 *   [12..]  LZ4 compressed block data
 */
#define LZ4B_MAGIC      "LZ4B"
#define LZ4B_MAGIC_U32  0x42345A4CU   /* "LZ4B" as little-endian uint32 */
#define LZ4B_HDR_SIZE   12

#endif /* LZ4_H */
