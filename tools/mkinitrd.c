#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct initrd_header {
    uint8_t magic; // Unused for now
    char name[64];
    uint32_t offset;
    uint32_t length;
};

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <output> <file1> [file2] ...\n", argv[0]);
        return 1;
    }
    
    int nheader = argc - 2;
    struct initrd_header headers[nheader];
    
    // Calculate offsets
    // Data starts after: [4 bytes count] + [headers * n]
    uint32_t data_offset = sizeof(uint32_t) + (sizeof(struct initrd_header) * nheader);
    
    printf("Creating InitRD with %d files...\n", nheader);
    
    // Prepare headers
    for(int i = 0; i < nheader; i++) {
        printf("Adding: %s\n", argv[i+2]);
        strcpy(headers[i].name, argv[i+2]); // Warning: Buffer overflow unsafe, good enough for tool
        headers[i].offset = data_offset;
        headers[i].magic = 0xBF;
        
        FILE *stream = fopen(argv[i+2], "r");
        if(!stream) {
            printf("Error opening file: %s\n", argv[i+2]);
            return 1;
        }
        fseek(stream, 0, SEEK_END);
        headers[i].length = ftell(stream);
        data_offset += headers[i].length;
        fclose(stream);
    }
    
    FILE *wstream = fopen(argv[1], "w");
    if(!wstream) {
        printf("Error opening output: %s\n", argv[1]);
        return 1;
    }
    
    // Write count
    fwrite(&nheader, sizeof(uint32_t), 1, wstream);
    // Write headers
    fwrite(headers, sizeof(struct initrd_header), nheader, wstream);
    
    // Write data
    for(int i = 0; i < nheader; i++) {
        FILE *stream = fopen(argv[i+2], "r");
        unsigned char *buf = (unsigned char *)malloc(headers[i].length);
        fread(buf, 1, headers[i].length, stream);
        fwrite(buf, 1, headers[i].length, wstream);
        fclose(stream);
        free(buf);
    }
    
    fclose(wstream);
    printf("Done. InitRD size: %d bytes.\n", data_offset);
    
    return 0;
}
