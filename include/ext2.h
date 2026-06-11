// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef EXT2_H
#define EXT2_H

#include "fs.h"
#include "blockdev.h"
#include <stdint.h>

struct partition;
struct ext2_group_desc;

/* Per-mount filesystem state */
struct ext2_mount {
    block_device_t* bdev;
    uint32_t part_lba;        /* partition start LBA */
    uint32_t block_size;      /* bytes per block (1024, 2048, or 4096) */
    uint32_t sectors_per_block;
    uint32_t inodes_per_group;
    uint32_t blocks_per_group;
    uint32_t inode_size;      /* on-disk inode size (128 or 256) */
    uint32_t num_groups;
    uint32_t first_data_block;
    uint32_t total_blocks;
    uint32_t total_inodes;
    struct ext2_group_desc* gdt; /* group descriptor table (heap-allocated) */
    uint32_t gdt_blocks;      /* number of blocks occupied by GDT */
};

/* Mount an ext2 filesystem on the given block device starting at the given
 * LBA offset.  Returns a mount result with root node and superblock, or {NULL, NULL} on failure. */
vfs_mount_result_t ext2_mount(block_device_t* bdev, uint32_t partition_lba);

/* Mount ext2 filesystem from a partition (uses partition's parent and start_lba) */
vfs_mount_result_t ext2_mount_partition(struct partition* part);

/* Verify ext2 filesystem state before mount
 * Returns 0 if filesystem is clean and safe to mount
 * Returns -EFSCK if filesystem needs fsck (dirty state)
 * Returns -EROFS if filesystem has errors that require read-only mount
 * Returns -EINVAL if filesystem has errors that prevent mount */
int ext2_verify_state(block_device_t* bdev, uint32_t partition_lba);

/* Get ext2 filesystem UUID (16 bytes) and volume name
 * Returns 0 on success, -errno on failure */
int ext2_get_uuid_label(block_device_t* bdev, uint32_t partition_lba,
                        uint8_t uuid[16], char label[16]);

/* Unmount an ext2 filesystem and free its resources */
void ext2_umount(struct ext2_mount* em);

#endif
