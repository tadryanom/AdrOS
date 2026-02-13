#ifndef EXT2_H
#define EXT2_H

#include "fs.h"
#include <stdint.h>

/* Mount an ext2 filesystem on the given ATA drive starting at the given
 * LBA offset.  Returns a VFS root node or NULL on failure. */
fs_node_t* ext2_mount(int drive, uint32_t partition_lba);

#endif
