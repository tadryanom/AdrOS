// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "blockdev.h"
#include "ata_pio.h"
#include "errno.h"
#include "console.h"

#include <string.h>

static block_device_t g_blockdevs[BLOCKDEV_MAX];
static int g_blockdev_count = 0;
static spinlock_t g_blockdev_lock;

void blockdev_init_lock(void) {
    spinlock_init(&g_blockdev_lock);
}

int blockdev_register(const block_device_t* dev) {
    if (!dev) return -EINVAL;

    uintptr_t flags = spin_lock_irqsave(&g_blockdev_lock);

    if (g_blockdev_count >= BLOCKDEV_MAX) {
        spin_unlock_irqrestore(&g_blockdev_lock, flags);
        return -ENOSPC;
    }

    /* Check for duplicate name */
    for (int i = 0; i < g_blockdev_count; i++) {
        if (strcmp(g_blockdevs[i].name, dev->name) == 0) {
            /* Update existing entry */
            g_blockdevs[i] = *dev;
            spin_unlock_irqrestore(&g_blockdev_lock, flags);
            return 0;
        }
    }

    g_blockdevs[g_blockdev_count++] = *dev;
    spin_unlock_irqrestore(&g_blockdev_lock, flags);
    return 0;
}

block_device_t* blockdev_find(const char* name) {
    if (!name) return NULL;

    uintptr_t flags = spin_lock_irqsave(&g_blockdev_lock);
    block_device_t* result = NULL;
    for (int i = 0; i < g_blockdev_count; i++) {
        if (strcmp(g_blockdevs[i].name, name) == 0) {
            result = &g_blockdevs[i];
            break;
        }
    }
    spin_unlock_irqrestore(&g_blockdev_lock, flags);
    return result;
}

block_device_t* blockdev_by_id(int drive_id) {
    uintptr_t flags = spin_lock_irqsave(&g_blockdev_lock);
    block_device_t* result = NULL;
    for (int i = 0; i < g_blockdev_count; i++) {
        if (g_blockdevs[i].drive_id == drive_id) {
            result = &g_blockdevs[i];
            break;
        }
    }
    spin_unlock_irqrestore(&g_blockdev_lock, flags);
    return result;
}

void blockdev_claim(block_device_t* dev) {
    if (!dev) return;
    uintptr_t flags = spin_lock_irqsave(&g_blockdev_lock);
    dev->refcount++;
    spin_unlock_irqrestore(&g_blockdev_lock, flags);
}

void blockdev_release(block_device_t* dev) {
    if (!dev) return;
    uintptr_t flags = spin_lock_irqsave(&g_blockdev_lock);
    if (dev->refcount > 0)
        dev->refcount--;
    spin_unlock_irqrestore(&g_blockdev_lock, flags);
}

/* ---- ATA block device ops ---- */

static int ata_bd_read(block_device_t* dev, uint32_t lba, void* buf) {
    return ata_pio_read28(dev->drive_id, lba, (uint8_t*)buf);
}

static int ata_bd_write(block_device_t* dev, uint32_t lba, const void* buf) {
    return ata_pio_write28(dev->drive_id, lba, (const uint8_t*)buf);
}

static const struct block_device_ops ata_bd_ops = {
    .read  = ata_bd_read,
    .write = ata_bd_write,
};

void blockdev_register_ata(void) {
    static const char* names[ATA_MAX_DRIVES] = { "hda", "hdb", "hdc", "hdd" };
    for (int i = 0; i < ATA_MAX_DRIVES; i++) {
        if (!ata_pio_drive_present(i)) continue;
        block_device_t bd;
        memset(&bd, 0, sizeof(bd));
        strncpy(bd.name, names[i], sizeof(bd.name) - 1);
        bd.sector_size = 512;
        bd.sector_count = 0; /* unknown */
        bd.drive_id = i;
        bd.ops = &ata_bd_ops;
        blockdev_register(&bd);
        kprintf("[BLKDEV] %s registered (ATA drive %d)\n", names[i], i);
    }
}
