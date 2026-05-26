// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include <stdint.h>
#include <stddef.h>
#include "errno.h"
#include "spinlock.h"

/* Generic block device interface — abstracts ATA/virtio-blk/etc.
 * Filesystems (fat, ext2) use this instead of calling ATA directly,
 * so they work with any registered block device. */

#define BLOCKDEV_MAX 8

struct block_device;

struct block_device_ops {
    int (*read)(struct block_device* dev, uint32_t lba, void* buf);
    int (*write)(struct block_device* dev, uint32_t lba, const void* buf);
};

typedef struct block_device {
    char name[16];              /* e.g. "hda", "hdb" */
    uint32_t sector_size;       /* typically 512 */
    uint32_t sector_count;      /* total sectors (0 if unknown) */
    int drive_id;               /* opaque identifier passed to ops */
    int refcount;               /* number of filesystems using this device */
    const struct block_device_ops* ops;
} block_device_t;

/* Register a block device. Returns 0 on success, -ENOSPC if table full. */
int blockdev_register(const block_device_t* dev);

/* Look up a block device by name (e.g. "hda"). Returns pointer or NULL. */
block_device_t* blockdev_find(const char* name);

/* Look up a block device by drive_id. Returns pointer or NULL. */
block_device_t* blockdev_by_id(int drive_id);

/* Increment block device refcount (called when filesystem mounts). */
void blockdev_claim(block_device_t* dev);

/* Decrement block device refcount (called when filesystem unmounts). */
void blockdev_release(block_device_t* dev);

/* Convenience: read one sector from a block device. */
static inline int blockdev_read(block_device_t* dev, uint32_t lba, void* buf) {
    if (!dev || !dev->ops || !dev->ops->read) return -ENODEV;
    return dev->ops->read(dev, lba, buf);
}

/* Convenience: write one sector to a block device. */
static inline int blockdev_write(block_device_t* dev, uint32_t lba, const void* buf) {
    if (!dev || !dev->ops || !dev->ops->write) return -ENODEV;
    return dev->ops->write(dev, lba, buf);
}

/* Register all detected ATA drives as block devices. */
void blockdev_register_ata(void);

/* Initialize the blockdev lock (must be called before any blockdev operations). */
void blockdev_init_lock(void);

#endif
