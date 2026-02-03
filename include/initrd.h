#ifndef INITRD_H
#define INITRD_H

#include "fs.h"
#include <stdint.h>

typedef struct {
    uint8_t magic; // Magic number 0xBF
    char name[64];
    uint32_t offset; // Offset relative to start of initrd
    uint32_t length;
} initrd_file_header_t;

// Initialize InitRD and return the root node (directory)
fs_node_t* initrd_init(uint32_t location);

#endif
