#ifndef INITRD_H
#define INITRD_H

#include "fs.h"
#include <stdint.h>

// Initialize InitRD and return the root node (directory)
fs_node_t* initrd_init(uint32_t location);

#endif
