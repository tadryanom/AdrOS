#ifndef INITRD_H
#define INITRD_H

#include "fs.h"
#include <stdint.h>

// Initialize InitRD and return the root node (directory)
// location: virtual address of initrd data
// size: total size in bytes (initrd_end - initrd_start)
fs_node_t* initrd_init(uint32_t location, uint32_t size);

#endif
