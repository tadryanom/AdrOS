#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define TAR_BLOCK 512

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

static void tar_write_header(FILE* out, const char* name, uint32_t size, char typeflag) {
    tar_header_t h;
    memset(&h, 0, sizeof(h));

    // Minimal USTAR header; we keep paths in the name field.
    strncpy(h.name, name, sizeof(h.name) - 1);
    tar_write_octal(h.mode, sizeof(h.mode), 0644);
    tar_write_octal(h.uid, sizeof(h.uid), 0);
    tar_write_octal(h.gid, sizeof(h.gid), 0);
    tar_write_octal(h.size, sizeof(h.size), size);
    tar_write_octal(h.mtime, sizeof(h.mtime), 0);

    memset(h.chksum, ' ', sizeof(h.chksum));
    h.typeflag = typeflag;
    memcpy(h.magic, "ustar", 5);
    memcpy(h.version, "00", 2);

    uint32_t sum = tar_checksum(&h);
    // chksum is 6 digits, NUL, space
    tar_write_octal(h.chksum, 7, sum);
    h.chksum[6] = '\0';
    h.chksum[7] = ' ';

    fwrite(&h, 1, sizeof(h), out);
}

static void write_zeros(FILE* out, size_t n) {
    static uint8_t z[TAR_BLOCK];
    memset(z, 0, sizeof(z));
    while (n) {
        size_t chunk = n;
        if (chunk > sizeof(z)) chunk = sizeof(z);
        fwrite(z, 1, chunk, out);
        n -= chunk;
    }
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

    FILE* out = fopen(out_name, "wb");
    if (!out) {
        perror("fopen output");
        return 1;
    }

    printf("Creating InitRD (TAR USTAR) with %d files...\n", nfiles);

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
            fclose(out);
            return 1;
        }

        fseek(in, 0, SEEK_END);
        long len = ftell(in);
        fseek(in, 0, SEEK_SET);
        if (len < 0) {
            fclose(in);
            fclose(out);
            return 1;
        }

        tar_write_header(out, dest, (uint32_t)len, '0');

        uint8_t buf[4096];
        long remaining = len;
        while (remaining > 0) {
            size_t chunk = (size_t)remaining;
            if (chunk > sizeof(buf)) chunk = sizeof(buf);
            size_t rd = fread(buf, 1, chunk, in);
            if (rd != chunk) {
                fclose(in);
                fclose(out);
                return 1;
            }
            fwrite(buf, 1, rd, out);
            remaining -= (long)rd;
        }

        fclose(in);

        // pad file to 512
        uint32_t pad = (uint32_t)((TAR_BLOCK - ((uint32_t)len % TAR_BLOCK)) % TAR_BLOCK);
        if (pad) write_zeros(out, pad);
    }

    // Two zero blocks end-of-archive
    write_zeros(out, TAR_BLOCK * 2);

    long end = ftell(out);
    fclose(out);

    printf("Done. InitRD size: %ld bytes.\n", end);
    return 0;
}
