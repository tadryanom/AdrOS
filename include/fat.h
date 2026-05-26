// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef FAT_H
#define FAT_H

#include "fs.h"
#include "blockdev.h"
#include "partition.h"
#include <stdint.h>

enum fat_type {
    FAT_TYPE_12 = 12,
    FAT_TYPE_16 = 16,
    FAT_TYPE_32 = 32,
};

/* Per-mount filesystem state */
struct fat_mount {
    block_device_t* bdev;
    uint32_t part_lba;
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint32_t fat_size;            /* sectors per FAT */
    uint32_t fat_lba;             /* LBA of first FAT */
    uint32_t root_dir_lba;        /* LBA of root directory (FAT12/16) */
    uint32_t root_dir_sectors;    /* sectors used by root dir (FAT12/16), 0 for FAT32 */
    uint32_t data_lba;            /* LBA of first data cluster */
    uint32_t total_clusters;
    uint32_t root_cluster;        /* FAT32 root cluster, 0 for FAT12/16 */
    enum fat_type type;
};

/* Mount a FAT12/16/32 filesystem on the given block device starting at
 * the given LBA offset.  Auto-detects FAT type from BPB.
 * Returns a mount result with root node and superblock, or {NULL, NULL} on failure. */
vfs_mount_result_t fat_mount(block_device_t* bdev, uint32_t partition_lba);

/* Mount FAT filesystem from a partition (uses partition's parent and start_lba) */
vfs_mount_result_t fat_mount_partition(partition_t* part);

/* Unmount a FAT filesystem and free its resources */
void fat_umount(struct fat_mount* fm);

#endif
