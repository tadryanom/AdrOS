// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "partition.h"
#include "blockdev.h"
#include "spinlock.h"
#include "console.h"
#include "errno.h"

#include <string.h>

static partition_t g_partitions[PARTITION_MAX];
static int g_partition_count = 0;
static spinlock_t g_partition_lock;

void partition_init_lock(void) {
    spinlock_init(&g_partition_lock);
}

int partition_register(const partition_t* part) {
    if (!part) return -EINVAL;
    if (!part->parent) return -EINVAL;

    uintptr_t flags = spin_lock_irqsave(&g_partition_lock);

    if (g_partition_count >= PARTITION_MAX) {
        spin_unlock_irqrestore(&g_partition_lock, flags);
        return -ENOSPC;
    }

    /* Check for duplicate name */
    for (int i = 0; i < g_partition_count; i++) {
        if (strcmp(g_partitions[i].name, part->name) == 0) {
            /* Update existing entry */
            g_partitions[i] = *part;
            spin_unlock_irqrestore(&g_partition_lock, flags);
            return 0;
        }
    }

    g_partitions[g_partition_count++] = *part;
    spin_unlock_irqrestore(&g_partition_lock, flags);
    return 0;
}

partition_t* partition_find(const char* name) {
    if (!name) return NULL;

    uintptr_t flags = spin_lock_irqsave(&g_partition_lock);
    partition_t* result = NULL;
    for (int i = 0; i < g_partition_count; i++) {
        if (strcmp(g_partitions[i].name, name) == 0) {
            result = &g_partitions[i];
            break;
        }
    }
    spin_unlock_irqrestore(&g_partition_lock, flags);
    return result;
}

partition_t* partition_find_by_device(block_device_t* parent, uint8_t partition_number) {
    if (!parent) return NULL;

    uintptr_t flags = spin_lock_irqsave(&g_partition_lock);
    partition_t* result = NULL;
    for (int i = 0; i < g_partition_count; i++) {
        if (g_partitions[i].parent == parent && 
            g_partitions[i].partition_number == partition_number) {
            result = &g_partitions[i];
            break;
        }
    }
    spin_unlock_irqrestore(&g_partition_lock, flags);
    return result;
}

void partition_claim(partition_t* part) {
    if (!part) return;
    uintptr_t flags = spin_lock_irqsave(&g_partition_lock);
    part->refcount++;
    spin_unlock_irqrestore(&g_partition_lock, flags);
}

void partition_release(partition_t* part) {
    if (!part) return;
    uintptr_t flags = spin_lock_irqsave(&g_partition_lock);
    if (part->refcount > 0)
        part->refcount--;
    spin_unlock_irqrestore(&g_partition_lock, flags);
}

/* MBR signature at offset 510-511 */
#define MBR_SIGNATURE 0xAA55

int partition_scan_mbr(block_device_t* bdev) {
    if (!bdev) return -EINVAL;

    /* Skip devices with unknown sector count (likely not present) */
    if (bdev->sector_count == 0) {
        return 0;
    }

    /* Read MBR sector (LBA 0) */
    uint8_t mbr_sector[512];
    int rc = blockdev_read(bdev, 0, mbr_sector);
    if (rc < 0) {
        return 0;
    }

    /* Check MBR signature */
    uint16_t signature = *(uint16_t*)(mbr_sector + 510);
    if (signature != MBR_SIGNATURE) {
        return 0;
    }

    /* Parse 4 primary partition entries (starting at offset 446) */
    mbr_partition_entry_t* entries = (mbr_partition_entry_t*)(mbr_sector + 446);
    int partitions_found = 0;

    for (int i = 0; i < 4; i++) {
        mbr_partition_entry_t* entry = &entries[i];

        /* Skip empty partitions (type 0) */
        if (entry->partition_type == PART_TYPE_EMPTY)
            continue;

        /* Create partition structure */
        partition_t part;
        memset(&part, 0, sizeof(part));

        part.parent = bdev;
        part.start_lba = entry->start_lba;
        part.sector_count = entry->sector_count;
        part.partition_type = entry->partition_type;
        part.partition_number = i + 1; /* 1-4 for primary partitions */
        part.refcount = 0;

        /* Generate partition name (e.g. "hda1", "vda2") */
        strcpy(part.name, bdev->name);
        int name_len = strlen(part.name);
        if (name_len < (int)sizeof(part.name) - 2) {
            part.name[name_len] = '0' + (i + 1);
            part.name[name_len + 1] = '\0';
        }

        /* Register the partition */
        rc = partition_register(&part);
        if (rc < 0) {
            continue;
        }

        kprintf("[PARTITION] %s: type=0x%02X, start_lba=%u, sectors=%u\n",
                part.name, part.partition_type, part.start_lba, part.sector_count);
        partitions_found++;
    }

    if (partitions_found > 0) {
        kprintf("[PARTITION] Scanned %s: found %d partition(s)\n", bdev->name, partitions_found);
    }
    return partitions_found;
}
