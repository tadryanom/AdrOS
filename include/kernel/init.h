// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef KERNEL_INIT_H
#define KERNEL_INIT_H

#include "kernel/boot_info.h"
#include "blockdev.h"
#include <stdint.h>

int init_start(const struct boot_info* bi);

/* Mount a filesystem on the given block device at the given mountpoint.
 * fstype: "fat", "ext2"
 * bdev: block device (from blockdev_find or blockdev_by_id)
 * lba: partition start LBA (0 for whole disk)
 * mountpoint: e.g. "/disk", "/fat", "/ext2"
 * flags: mount flags (MS_RDONLY, etc.) — stored in VFS mount table
 * Returns 0 on success, negative errno on failure. */
int init_mount_fs(const char* fstype, struct block_device* bdev, uint32_t lba, const char* mountpoint, unsigned long flags);

#endif
