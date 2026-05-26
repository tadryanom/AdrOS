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
