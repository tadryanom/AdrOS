#ifndef FAT16_H
#define FAT16_H

#include "fs.h"
#include <stdint.h>

/* Mount a FAT16 filesystem starting at the given LBA offset on disk.
 * Returns a VFS root node or NULL on failure. */
fs_node_t* fat16_mount(uint32_t partition_lba);

#endif
