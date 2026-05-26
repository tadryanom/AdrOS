// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef PARTITION_H
#define PARTITION_H

#include "blockdev.h"
#include <stddef.h>
#include <stdint.h>

/* Maximum number of partitions in the system */
#define PARTITION_MAX 32

/* Partition types (MBR) */
#define PART_TYPE_EMPTY       0x00
#define PART_TYPE_FAT12       0x01
#define PART_TYPE_FAT16       0x04
#define PART_TYPE_EXTENDED    0x05
#define PART_TYPE_FAT16B      0x06
#define PART_TYPE_NTFS        0x07
#define PART_TYPE_FAT32       0x0B
#define PART_TYPE_FAT32_LBA   0x0C
#define PART_TYPE_EXTENDED_LBA 0x0F
#define PART_TYPE_LINUX       0x83
#define PART_TYPE_LINUX_SWAP  0x82

typedef struct partition {
    block_device_t* parent;        /* Parent block device (hda, vda, etc.) */
    uint32_t start_lba;           /* Starting LBA of partition */
    uint32_t sector_count;        /* Number of sectors in partition */
    uint8_t partition_type;       /* Partition type (MBR/GPT) */
    uint8_t partition_number;     /* Partition number (1-4 for primary MBR) */
    char name[32];                /* e.g. "hda1", "vda2" */
    int refcount;                 /* Number of filesystems using this partition */
} partition_t;

/* Register a partition. Returns 0 on success, -ENOSPC if table full. */
int partition_register(const partition_t* part);

/* Look up a partition by name (e.g. "hda1"). Returns pointer or NULL. */
partition_t* partition_find(const char* name);

/* Look up a partition by parent device and partition number. */
partition_t* partition_find_by_device(block_device_t* parent, uint8_t partition_number);

/* Increment partition refcount (called when filesystem mounts). */
void partition_claim(partition_t* part);

/* Decrement partition refcount (called when filesystem unmounts). */
void partition_release(partition_t* part);

/* Initialize partition subsystem lock */
void partition_init_lock(void);

#endif /* PARTITION_H */
