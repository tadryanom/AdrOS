#ifndef FAT_H
#define FAT_H

#include "fs.h"
#include <stdint.h>

/* Mount a FAT12/16/32 filesystem on the given ATA drive starting at
 * the given LBA offset.  Auto-detects FAT type from BPB.
 * Returns a VFS root node or NULL on failure. */
fs_node_t* fat_mount(int drive, uint32_t partition_lba);

#endif
