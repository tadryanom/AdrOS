#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define TAR_BLOCK 512

/* ---- LZ4 block compressor (standalone, no external dependency) ---- */

#define LZ4_HASH_BITS  16
#define LZ4_HASH_SIZE  (1 << LZ4_HASH_BITS)
#define LZ4_MIN_MATCH  4
#define LZ4_LAST_LITERALS 5   /* last 5 bytes are always literals */

static uint32_t lz4_hash4(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return (v * 2654435761U) >> (32 - LZ4_HASH_BITS);
}

/*
 * Compress src[0..src_size) into dst[0..dst_cap).
 * Returns compressed size, or 0 on failure (output too large).
 */
static size_t lz4_compress_block(const uint8_t *src, size_t src_size,
                                 uint8_t *dst, size_t dst_cap)
{
    if (src_size == 0) return 0;
    if (src_size > 0x7E000000) return 0; /* too large */

    uint32_t *htab = calloc(LZ4_HASH_SIZE, sizeof(uint32_t));
    if (!htab) return 0;

    const uint8_t *ip = src;
    const uint8_t *ip_end = src + src_size;
    const uint8_t *ip_limit = ip_end - LZ4_LAST_LITERALS;
    const uint8_t *anchor = ip; /* start of pending literals */
    uint8_t *op = dst;
    uint8_t *op_end = dst + dst_cap;

    ip++; /* first byte can't match */

    while (ip < ip_limit) {
        /* find a match */
        uint32_t h = lz4_hash4(ip);
        const uint8_t *ref = src + htab[h];
        htab[h] = (uint32_t)(ip - src);

        if (ref < src || ip - ref > 65535 ||
            memcmp(ip, ref, 4) != 0) {
            ip++;
            continue;
        }

        /* extend match forward */
        size_t match_len = LZ4_MIN_MATCH;
        while (ip + match_len < ip_end && ip[match_len] == ref[match_len])
            match_len++;

        /* emit sequence */
        size_t lit_len = (size_t)(ip - anchor);
        size_t token_pos_needed = 1 + (lit_len >= 15 ? 1 + lit_len / 255 : 0)
                                  + lit_len + 2
                                  + (match_len - 4 >= 15 ? 1 + (match_len - 4 - 15) / 255 : 0);
        if (op + token_pos_needed > op_end) { free(htab); return 0; }

        /* token byte */
        size_t ml_code = match_len - LZ4_MIN_MATCH;
        uint8_t token = (uint8_t)((lit_len >= 15 ? 15 : lit_len) << 4);
        token |= (uint8_t)(ml_code >= 15 ? 15 : ml_code);
        *op++ = token;

        /* extended literal length */
        if (lit_len >= 15) {
            size_t rem = lit_len - 15;
            while (rem >= 255) { *op++ = 255; rem -= 255; }
            *op++ = (uint8_t)rem;
        }

        /* literal data */
        memcpy(op, anchor, lit_len);
        op += lit_len;

        /* match offset (16-bit LE) */
        uint16_t off = (uint16_t)(ip - ref);
        *op++ = (uint8_t)(off & 0xFF);
        *op++ = (uint8_t)(off >> 8);

        /* extended match length */
        if (ml_code >= 15) {
            size_t rem = ml_code - 15;
            while (rem >= 255) { *op++ = 255; rem -= 255; }
            *op++ = (uint8_t)rem;
        }

        ip += match_len;
        anchor = ip;
    }

    /* emit remaining literals */
    {
        size_t lit_len = (size_t)(ip_end - anchor);
        size_t needed = 1 + (lit_len >= 15 ? 1 + lit_len / 255 : 0) + lit_len;
        if (op + needed > op_end) { free(htab); return 0; }

        uint8_t token = (uint8_t)((lit_len >= 15 ? 15 : lit_len) << 4);
        *op++ = token;
        if (lit_len >= 15) {
            size_t rem = lit_len - 15;
            while (rem >= 255) { *op++ = 255; rem -= 255; }
            *op++ = (uint8_t)rem;
        }
        memcpy(op, anchor, lit_len);
        op += lit_len;
    }

    free(htab);
    return (size_t)(op - dst);
}

/* LZ4B header: magic(4) + orig_size(4) + comp_size(4) */
#define LZ4B_HDR_SIZE 12

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* ---- end LZ4 ---- */

typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed)) tar_header_t;

static void tar_write_octal(char* out, size_t out_sz, uint32_t val) {
    // Write N-1 digits + NUL (or space) padding
    // Common tar field style: leading zeros, terminated by NUL.
    if (out_sz == 0) return;
    memset(out, '0', out_sz);
    out[out_sz - 1] = '\0';

    size_t i = out_sz - 2;
    uint32_t v = val;
    while (1) {
        out[i] = (char)('0' + (v & 7));
        v >>= 3;
        if (v == 0 || i == 0) break;
        i--;
    }
}

static uint32_t tar_checksum(const tar_header_t* h) {
    const uint8_t* p = (const uint8_t*)h;
    uint32_t sum = 0;
    for (size_t i = 0; i < sizeof(*h); i++) {
        sum += p[i];
    }
    return sum;
}

static int split_src_dest(const char* arg, char* src_out, size_t src_sz, char* dest_out, size_t dest_sz) {
    const char* colon = strchr(arg, ':');
    if (!colon) return 0;

    size_t src_len = (size_t)(colon - arg);
    size_t dest_len = strlen(colon + 1);
    if (src_len == 0 || dest_len == 0) return 0;
    if (src_len >= src_sz || dest_len >= dest_sz) return 0;

    memcpy(src_out, arg, src_len);
    src_out[src_len] = 0;
    memcpy(dest_out, colon + 1, dest_len);
    dest_out[dest_len] = 0;
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s output.img file1[:dest] [file2[:dest] ...]\n", argv[0]);
        return 1;
    }

    const char* out_name = argv[1];
    int nfiles = argc - 2;

    /* Build the tar archive in memory so we can compress it */
    size_t tar_cap = 4 * 1024 * 1024; /* 4MB initial */
    uint8_t* tar_buf = malloc(tar_cap);
    if (!tar_buf) { perror("malloc"); return 1; }
    size_t tar_len = 0;

    printf("Creating InitRD (USTAR+LZ4) with %d files...\n", nfiles);

    for (int i = 0; i < nfiles; i++) {
        char src[256];
        char dest[256];

        const char* arg = argv[i + 2];
        if (split_src_dest(arg, src, sizeof(src), dest, sizeof(dest))) {
            // ok
        } else {
            strncpy(src, arg, sizeof(src) - 1);
            src[sizeof(src) - 1] = 0;

            const char* base = strrchr(arg, '/');
            base = base ? base + 1 : arg;
            strncpy(dest, base, sizeof(dest) - 1);
            dest[sizeof(dest) - 1] = 0;
        }

        printf("Adding: %s -> %s\n", src, dest);

        FILE* in = fopen(src, "rb");
        if (!in) {
            perror("fopen input");
            free(tar_buf);
            return 1;
        }

        fseek(in, 0, SEEK_END);
        long len = ftell(in);
        fseek(in, 0, SEEK_SET);
        if (len < 0) { fclose(in); free(tar_buf); return 1; }

        uint32_t pad = (uint32_t)((TAR_BLOCK - ((uint32_t)len % TAR_BLOCK)) % TAR_BLOCK);
        size_t needed = TAR_BLOCK + (size_t)len + pad;

        while (tar_len + needed > tar_cap) {
            tar_cap *= 2;
            tar_buf = realloc(tar_buf, tar_cap);
            if (!tar_buf) { perror("realloc"); fclose(in); return 1; }
        }

        /* Write header into buffer */
        {
            tar_header_t h;
            memset(&h, 0, sizeof(h));
            strncpy(h.name, dest, sizeof(h.name) - 1);
            tar_write_octal(h.mode, sizeof(h.mode), 0644);
            tar_write_octal(h.uid, sizeof(h.uid), 0);
            tar_write_octal(h.gid, sizeof(h.gid), 0);
            tar_write_octal(h.size, sizeof(h.size), (uint32_t)len);
            tar_write_octal(h.mtime, sizeof(h.mtime), 0);
            memset(h.chksum, ' ', sizeof(h.chksum));
            h.typeflag = '0';
            memcpy(h.magic, "ustar", 5);
            memcpy(h.version, "00", 2);
            uint32_t sum = tar_checksum(&h);
            tar_write_octal(h.chksum, 7, sum);
            h.chksum[6] = '\0';
            h.chksum[7] = ' ';
            memcpy(tar_buf + tar_len, &h, sizeof(h));
            tar_len += sizeof(h);
        }

        /* Read file data */
        size_t rd = fread(tar_buf + tar_len, 1, (size_t)len, in);
        if ((long)rd != len) { fclose(in); free(tar_buf); return 1; }
        tar_len += (size_t)len;
        fclose(in);

        /* Pad to 512 */
        if (pad) { memset(tar_buf + tar_len, 0, pad); tar_len += pad; }
    }

    /* Two zero blocks end-of-archive */
    while (tar_len + TAR_BLOCK * 2 > tar_cap) {
        tar_cap *= 2;
        tar_buf = realloc(tar_buf, tar_cap);
        if (!tar_buf) { perror("realloc"); return 1; }
    }
    memset(tar_buf + tar_len, 0, TAR_BLOCK * 2);
    tar_len += TAR_BLOCK * 2;

    printf("TAR size: %zu bytes\n", tar_len);

    /* Compress with LZ4 */
    size_t comp_cap = tar_len + tar_len / 255 + 16; /* worst case */
    uint8_t* comp_buf = malloc(comp_cap);
    if (!comp_buf) { perror("malloc comp"); free(tar_buf); return 1; }

    size_t comp_sz = lz4_compress_block(tar_buf, tar_len, comp_buf, comp_cap);
    if (comp_sz == 0) {
        printf("LZ4 compression failed, writing uncompressed tar.\n");
        FILE* out = fopen(out_name, "wb");
        if (!out) { perror("fopen"); free(tar_buf); free(comp_buf); return 1; }
        fwrite(tar_buf, 1, tar_len, out);
        fclose(out);
        printf("Done. InitRD size: %zu bytes (uncompressed).\n", tar_len);
    } else {
        printf("LZ4: %zu -> %zu bytes (%.1f%%)\n",
               tar_len, comp_sz, 100.0 * (double)comp_sz / (double)tar_len);

        FILE* out = fopen(out_name, "wb");
        if (!out) { perror("fopen"); free(tar_buf); free(comp_buf); return 1; }

        /* Write LZ4B header */
        uint8_t hdr[LZ4B_HDR_SIZE];
        memcpy(hdr, "LZ4B", 4);
        write_le32(hdr + 4, (uint32_t)tar_len);
        write_le32(hdr + 8, (uint32_t)comp_sz);
        fwrite(hdr, 1, LZ4B_HDR_SIZE, out);
        fwrite(comp_buf, 1, comp_sz, out);
        fclose(out);

        printf("Done. InitRD size: %zu bytes (LZ4B header + compressed).\n",
               LZ4B_HDR_SIZE + comp_sz);
    }

    free(tar_buf);
    free(comp_buf);
    return 0;
}
