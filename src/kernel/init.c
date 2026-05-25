// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "kernel/init.h"

#include "arch/arch_platform.h"
#include "hal/driver.h"

#include "fs.h"
#include "initrd.h"
#include "overlayfs.h"
#include "tmpfs.h"
#include "devfs.h"
#include "tty.h"
#include "pty.h"
/* diskfs and persistfs removed — use fat/ext2 for disk storage */
#include "procfs.h"
#include "fat.h"
#include "ext2.h"
#include "pci.h"
#include "e1000.h"
#include "net.h"
#include "socket.h"
#include "vbe.h"
#include "keyboard.h"
#include "console.h"
#include "errno.h"
#include "utils.h"

#include "ata_pio.h"
#include "blockdev.h"
#include "hal/mm.h"
#include "heap.h"
#include "kernel/cmdline.h"

#include <stddef.h>
#include <string.h>

/* ---- Mount helper: used by fstab parser and kconsole 'mount' command ---- */

int init_mount_fs(const char* fstype, int drive, uint32_t lba, const char* mountpoint, unsigned long flags) {
    fs_node_t* root = NULL;

    if (strcmp(fstype, "fat") == 0) {
        root = fat_mount(drive, lba);
    } else if (strcmp(fstype, "ext2") == 0) {
        root = ext2_mount(drive, lba);
    } else {
        kprintf("[MOUNT] Unknown filesystem type: %s\n", fstype);
        return -EINVAL;
    }

    if (!root) {
        kprintf("[MOUNT] Failed to mount %s on /dev/%s at %s\n",
                fstype, ata_drive_to_name(drive) ? ata_drive_to_name(drive) : "?",
                mountpoint);
        return -ENODEV;
    }

    /* Build device name for mount table metadata */
    char devname[32] = "none";
    const char* dname = ata_drive_to_name(drive);
    if (dname) {
        strcpy(devname, "/dev/");
        /* Append drive name after /dev/ */
        char* dp = devname + 5;
        while (*dname && (dp - devname) < (int)sizeof(devname) - 2)
            *dp++ = *dname++;
        *dp = '\0';
    }

    int rc = vfs_mount_full(mountpoint, root, fstype, devname, flags);
    if (rc < 0) {
        kprintf("[MOUNT] Failed to register mount at %s (err=%d)\n", mountpoint, rc);
        return rc;
    }

    kprintf("[MOUNT] %s on /dev/%s -> %s\n",
            fstype, ata_drive_to_name(drive) ? ata_drive_to_name(drive) : "?",
            mountpoint);
    return 0;
}

int init_start(const struct boot_info* bi) {
    /* Parse kernel command line (Linux-like triaging) */
    cmdline_parse(bi ? bi->cmdline : NULL);

    /* Apply console= parameter: serial, vga, or both (default) */
    const char* con = cmdline_get("console");
    if (con) {
        if (strcmp(con, "serial") == 0 || strcmp(con, "ttyS0") == 0) {
            console_enable_uart(1);
            console_enable_vga(0);
            kprintf("[CONSOLE] output: serial only\n");
        } else if (strcmp(con, "vga") == 0 || strcmp(con, "tty0") == 0) {
            console_enable_uart(0);
            console_enable_vga(1);
            kprintf("[CONSOLE] output: VGA only\n");
        } else {
            kprintf("[CONSOLE] unknown console=%s, using both\n", con);
        }
    }

    if (bi && bi->initrd_start) {
        uintptr_t initrd_virt = 0;
        if (hal_mm_map_physical_range((uintptr_t)bi->initrd_start, (uintptr_t)bi->initrd_end,
                                      HAL_MM_MAP_RW, &initrd_virt) == 0) {
            uint32_t initrd_sz = (uint32_t)(bi->initrd_end - bi->initrd_start);
            fs_root = initrd_init((uint32_t)initrd_virt, initrd_sz);
        } else {
            kprintf("[INITRD] Failed to map initrd physical range.\n");
        }
    }

    if (fs_root) {
        fs_node_t* upper = tmpfs_create_root();
        if (upper) {
            fs_node_t* ovl = overlayfs_create_root(fs_root, upper);
            if (ovl) {
                (void)vfs_mount_full("/", ovl, "overlayfs", "initrd", 0);
                vfs_set_initrd_root(ovl);
            }
        }
    }
    /* Create mount-point directories and mount virtual filesystems.
     * When /sbin/init runs in userspace it re-mounts these (vfs_mount
     * replaces existing entries, so it is a harmless no-op).  When a
     * different init= binary is used (e.g. fulltest), these kernel-side
     * mounts ensure the system is functional. */
    {
        int rc;
        rc = vfs_mkdir("/dev");
        if (rc < 0 && rc != -EEXIST) kprintf("[INIT] mkdir /dev failed: %d\n", rc);
        rc = vfs_mkdir("/proc");
        if (rc < 0 && rc != -EEXIST) kprintf("[INIT] mkdir /proc failed: %d\n", rc);
        /* /disk and /persist directories are created by userspace init
         * or by fstab-driven mount — no longer created by kernel */
    }

    fs_node_t* tmp = tmpfs_create_root();
    if (tmp) {
        (void)vfs_mount_full("/tmp", tmp, "tmpfs", "none", 0);
    }

    /* Register hardware drivers with HAL and init in priority order */
    pci_driver_register();      /* priority 10: bus */
    e1000_driver_register();    /* priority 20: NIC (probes PCI) */
    extern void virtio_blk_driver_register(void);
    virtio_blk_driver_register(); /* priority 25: virtio-blk */
    hal_drivers_init_all();

    net_init();
    ksocket_init();
    vbe_init(bi);

    tty_init();
    pty_init();

    /* Mount devfs — kernel-side fallback so that any init= binary
     * (including fulltest) has /dev available.  When /sbin/init runs
     * it re-mounts devfs via mount() syscall; vfs_mount replaces the
     * existing entry so this is a harmless overlap. */
    fs_node_t* dev = devfs_create_root();
    if (dev) {
        (void)vfs_mount_full("/dev", dev, "devfs", "none", 0);
    }

    vbe_register_devfs();
    keyboard_register_devfs();

    fs_node_t* proc = procfs_create_root();
    if (proc) {
        (void)vfs_mount_full("/proc", proc, "procfs", "none", 0);
    }

    /* Initialize ATA subsystem — probe all 4 drives
     * (primary/secondary x master/slave). */
    (void)ata_pio_init();

    /* Register detected ATA drives as /dev/hdX block device nodes */
    extern void ata_register_devfs(void);
    ata_register_devfs();

    /* Register ATA drives as generic block devices (used by fat/ext2) */
    blockdev_register_ata();

    /* If root= is specified on the kernel command line, mount that device
     * as the disk root filesystem.  The filesystem type is auto-detected
     * by trying each supported type in order.
     * Example:  root=/dev/hda  or  root=/dev/hdb
     *
     * If no root= is given but the primary master (hda) is present,
     * auto-mount it on /disk so that any init= binary (including fulltest)
     * has disk access.  /etc/fstab parsing is now done by /sbin/init. */
    const char* root_dev = cmdline_get("root");
    if (root_dev) {
        int drive = -1;
        if (strncmp(root_dev, "/dev/", 5) == 0)
            drive = ata_name_to_drive(root_dev + 5);
        if (drive >= 0 && ata_pio_drive_present(drive)) {
            /* Auto-detect: try ext2, then fat (non-destructive probes). */
            static const char* fstypes[] = { "ext2", "fat", NULL };
            int mounted = 0;
            for (int i = 0; fstypes[i]; i++) {
                if (init_mount_fs(fstypes[i], drive, 0, "/disk", 0) == 0) {
                    kprintf("[INIT] root=%s mounted as %s on /disk\n",
                            root_dev, fstypes[i]);
                    mounted = 1;
                    break;
                }
            }
            if (!mounted)
                kprintf("[INIT] root=%s: no supported filesystem found\n", root_dev);
        } else {
            kprintf("[INIT] root=%s: device not found\n", root_dev);
        }
    } else if (ata_pio_drive_present(0)) {
        /* No root= on cmdline, but primary master is present — auto-mount */
        static const char* fstypes[] = { "ext2", "fat", NULL };
        for (int i = 0; fstypes[i]; i++) {
            if (init_mount_fs(fstypes[i], 0, 0, "/disk", 0) == 0) {
                kprintf("[INIT] /dev/hda auto-mounted as %s on /disk\n", fstypes[i]);
                break;
            }
        }
    }

    /* Disk-based filesystems can also be mounted via /etc/fstab entries
     * (parsed by userspace /sbin/init) or manually via the kconsole
     * 'mount' command. */

    if (!fs_root) {
        kprintf("[INIT] No root filesystem -- cannot start userspace.\n");
        return -1;
    }

    int user_ret = arch_platform_start_userspace(bi);

    return user_ret;
}
