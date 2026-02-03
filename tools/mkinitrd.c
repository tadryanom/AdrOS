#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct initrd_header {
    uint32_t nfiles;
};

struct initrd_file_header {
    uint8_t magic;
    char name[64];
    uint32_t offset;
    uint32_t length;
};

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <output.img> <file1> [file2] ...\n", argv[0]);
        return 1;
    }

    int nfiles = argc - 2;
    struct initrd_header header;
    header.nfiles = nfiles;

    struct initrd_file_header *headers = malloc(sizeof(struct initrd_file_header) * nfiles);
    
    // First pass: Calculate offsets
    uint32_t data_offset = sizeof(struct initrd_header) + (sizeof(struct initrd_file_header) * nfiles);
    
    printf("Creating InitRD with %d files...\n", nfiles);

    for (int i = 0; i < nfiles; i++) {
        char* fname = argv[i+2];
        
        // Strip path, keep filename
        char* basename = strrchr(fname, '/');
        if (basename) basename++; else basename = fname;
        
        FILE* f = fopen(fname, "rb");
        if (!f) {
            perror("Error opening file");
            return 1;
        }
        fseek(f, 0, SEEK_END);
        uint32_t len = ftell(f);
        fclose(f);

        headers[i].magic = 0xBF;
        strncpy(headers[i].name, basename, 63);
        headers[i].offset = data_offset; // Absolute offset from file start
        headers[i].length = len;
        
        printf("  File: %s (Size: %d bytes, Offset: %d)\n", basename, len, data_offset);
        
        data_offset += len;
    }

    FILE *w = fopen(argv[1], "wb");
    if (!w) {
        perror("Error opening output");
        return 1;
    }

    fwrite(&header, sizeof(struct initrd_header), 1, w);
    fwrite(headers, sizeof(struct initrd_file_header), nfiles, w);

    // Second pass: Write data
    for (int i = 0; i < nfiles; i++) {
        FILE* f = fopen(argv[i+2], "rb");
        uint8_t *buf = malloc(headers[i].length);
        fread(buf, 1, headers[i].length, f);
        fwrite(buf, 1, headers[i].length, w);
        free(buf);
        fclose(f);
    }

    fclose(w);
    free(headers);
    printf("Done! Wrote %s\n", argv[1]);
    
    return 0;
}
