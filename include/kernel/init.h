#ifndef KERNEL_INIT_H
#define KERNEL_INIT_H

#include "kernel/boot_info.h"
#include <stdint.h>

int init_start(const struct boot_info* bi);

/* Mount a filesystem on the given ATA drive at the given mountpoint.
 * fstype: "diskfs", "fat", "ext2", "persistfs"
 * drive: ATA_DEV_PRIMARY_MASTER .. ATA_DEV_SECONDARY_SLAVE
 * lba: partition start LBA (0 for whole disk)
 * mountpoint: e.g. "/disk", "/fat", "/ext2"
 * Returns 0 on success, -1 on failure. */
int init_mount_fs(const char* fstype, int drive, uint32_t lba, const char* mountpoint);

#endif
