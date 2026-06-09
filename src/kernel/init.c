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
#include "fat.h"
#include "ext2.h"
#include "initrd.h"
#include "overlayfs.h"
#include "tmpfs.h"
#include "devfs.h"
#include "tty.h"
#include "pty.h"
#include "procfs.h"
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
#include "partition.h"
#include "hal/mm.h"
#include "heap.h"
#include "kernel/cmdline.h"

#include <stddef.h>
#include <string.h>

/* ---- Mount helper: used by fstab parser and kconsole 'mount' command ---- */

static void init_build_mount_source_name(const char* source_name, block_device_t* bdev, char* out, size_t out_size) {
    if (!out || out_size == 0) return;

    if (source_name && source_name[0] != '\0') {
        if (!bdev) {
            strncpy(out, source_name, out_size - 1);
            out[out_size - 1] = '\0';
            return;
        }

        if (strncmp(source_name, "/dev/", 5) == 0) {
            strncpy(out, source_name, out_size - 1);
            out[out_size - 1] = '\0';
            return;
        }

        strcpy(out, "/dev/");
        char* dp = out + 5;
        const char* sp = source_name;
        while (*sp && (size_t)(dp - out) < out_size - 1)
            *dp++ = *sp++;
        *dp = '\0';
        return;
    }

    if (!bdev) {
        strncpy(out, "none", out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    strcpy(out, "/dev/");
    char* dp = out + 5;
    const char* sp = bdev->name;
    while (*sp && (size_t)(dp - out) < out_size - 1)
        *dp++ = *sp++;
    *dp = '\0';
}

int init_resolve_mount_device(const char* device, block_device_t** bdev, uint32_t* lba) {
    if (!device || !bdev || !lba) return -EINVAL;

    const char* devname = device;
    if (strncmp(devname, "/dev/", 5) == 0) devname += 5;

    block_device_t* resolved_bdev = blockdev_find(devname);
    if (resolved_bdev) {
        *bdev = resolved_bdev;
        *lba = 0;
        return 0;
    }

    partition_t* part = partition_find(devname);
    if (!part || !part->parent) return -ENODEV;

    *bdev = part->parent;
    *lba = part->start_lba;
    return 0;
}

int init_mount_fs(const char* fstype, block_device_t* bdev, uint32_t lba, const char* mountpoint, unsigned long flags, const char* source_name) {
    /* Validate mountpoint exists and is a directory */
    fs_node_t* mp_node = vfs_lookup(mountpoint);
    if (!mp_node) {
        kprintf("[MOUNT] Mountpoint does not exist: %s\n", mountpoint);
        return -ENOENT;
    }
    if (!(mp_node->flags & FS_DIRECTORY)) {
        kprintf("[MOUNT] Mountpoint is not a directory: %s\n", mountpoint);
        return -ENOTDIR;
    }

    const vfs_fs_type_t* fst = vfs_fs_type_find(fstype);
    if (!fst) {
        kprintf("[MOUNT] Unknown filesystem type: %s\n", fstype);
        return -EINVAL;
    }

    if ((fst->flags & FS_NEEDS_BDEV) && !bdev) {
        kprintf("[MOUNT] Filesystem %s requires a block device\n", fstype);
        return -ENODEV;
    }

    char devname[32];
    init_build_mount_source_name(source_name, bdev, devname, sizeof(devname));

    vfs_mount_result_t mres = fst->mount(bdev, lba);
    if (!mres.root) {
        kprintf("[MOUNT] Failed to mount %s on %s at %s\n",
                fstype, devname,
                mountpoint);
        return -ENODEV;
    }

    /* Set fstype pointer in superblock */
    if (mres.sb) {
        mres.sb->fstype = fst;
    }

    /* Claim the block device */
    if (bdev) {
        blockdev_claim(bdev);
    }

    int rc = vfs_mount_full(mountpoint, mres.root, fstype, devname, flags, bdev, mres.sb);
    if (rc < 0) {
        kprintf("[MOUNT] Failed to register mount at %s (err=%d)\n", mountpoint, rc);
        if (bdev) {
            blockdev_release(bdev);
        }
        /* Cleanup filesystem state if mount registration failed */
        if (mres.sb && mres.sb->fstype && mres.sb->fstype->kill_sb) {
            mres.sb->fstype->kill_sb(mres.sb);
        } else if (mres.root) {
            kfree(mres.root);
        }
        return rc;
    }

    kprintf("[MOUNT] %s on /dev/%s -> %s\n",
            fstype, devname,
            mountpoint);
    return 0;
}

static void fat_kill_sb(vfs_superblock_t* sb) {
    if (sb) {
        /* Free the root node allocated in fat_mount */
        if (sb->root) {
            kfree(sb->root);
        }
        /* Free the mount structure */
        if (sb->private_data) {
            fat_umount((struct fat_mount*)sb->private_data);
        }
        kfree(sb);
    }
}

static void ext2_kill_sb(vfs_superblock_t* sb) {
    if (sb) {
        /* Free the root node allocated in ext2_mount */
        if (sb->root) {
            kfree(sb->root);
        }
        /* Free the mount structure */
        if (sb->private_data) {
            ext2_umount((struct ext2_mount*)sb->private_data);
        }
        kfree(sb);
    }
}

int init_start(const struct boot_info* bi) {
    /* Register filesystem types */
    static vfs_fs_type_t fat_fs_type = {
        .name = "fat",
        .flags = FS_NEEDS_BDEV,
        .mount = fat_mount,
        .kill_sb = fat_kill_sb
    };
    vfs_fs_type_register(&fat_fs_type);

    static vfs_fs_type_t ext2_fs_type = {
        .name = "ext2",
        .flags = FS_NEEDS_BDEV,
        .mount = ext2_mount,
        .kill_sb = ext2_kill_sb
    };
    vfs_fs_type_register(&ext2_fs_type);

    /* Register virtual filesystems (no block device needed) */
    static vfs_fs_type_t tmpfs_fs_type = {
        .name = "tmpfs",
        .flags = 0,
        .mount = tmpfs_mount,
        .kill_sb = tmpfs_kill_sb
    };
    vfs_fs_type_register(&tmpfs_fs_type);

    static vfs_fs_type_t devfs_fs_type = {
        .name = "devfs",
        .flags = 0,
        .mount = devfs_mount,
        .kill_sb = devfs_kill_sb
    };
    vfs_fs_type_register(&devfs_fs_type);

    static vfs_fs_type_t procfs_fs_type = {
        .name = "procfs",
        .flags = 0,
        .mount = procfs_mount,
        .kill_sb = procfs_kill_sb
    };
    vfs_fs_type_register(&procfs_fs_type);

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
            if (fs_root) {
                vfs_set_initrd_root(fs_root);
            }
        } else {
            kprintf("[INITRD] Failed to map initrd physical range.\n");
        }
    }

    if (fs_root) {
        fs_node_t* upper = tmpfs_create_root();
        if (upper) {
            fs_node_t* ovl = overlayfs_create_root(fs_root, upper);
            if (ovl) {
                (void)vfs_mount_full("/", ovl, "overlayfs", "initrd", 0, NULL, NULL);
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
        rc = vfs_mkdir("/tmp");
        if (rc < 0 && rc != -EEXIST) kprintf("[INIT] mkdir /tmp failed: %d\n", rc);
        rc = vfs_mkdir("/dev");
        if (rc < 0 && rc != -EEXIST) kprintf("[INIT] mkdir /dev failed: %d\n", rc);
        rc = vfs_mkdir("/proc");
        if (rc < 0 && rc != -EEXIST) kprintf("[INIT] mkdir /proc failed: %d\n", rc);
        rc = vfs_mkdir("/disk");
        if (rc < 0 && rc != -EEXIST) kprintf("[INIT] mkdir /disk failed: %d\n", rc);
    }

    (void)init_mount_fs("tmpfs", NULL, 0, "/tmp", 0, "none");

    /* Register hardware drivers with HAL and init in priority order */
    pci_driver_register();      /* priority 10: bus */
    e1000_driver_register();    /* priority 20: NIC (probes PCI) */
    extern void virtio_blk_driver_register(void);
    virtio_blk_driver_register(); /* priority 25: virtio-blk */
    hal_drivers_init_all();

    /* Register virtio-blk as a block device if initialized */
    extern void virtio_blk_register_blockdev(void);
    virtio_blk_register_blockdev();

    net_init();
    ksocket_init();
    vbe_init(bi);

    tty_init();
    pty_init();

    /* Mount devfs — kernel-side fallback so that any init= binary
     * (including fulltest) has /dev available.  When /sbin/init runs
     * it re-mounts devfs via mount() syscall; vfs_mount replaces the
     * existing entry so this is a harmless overlap. */
    (void)init_mount_fs("devfs", NULL, 0, "/dev", 0, "none");

    vbe_register_devfs();
    keyboard_register_devfs();

    (void)init_mount_fs("procfs", NULL, 0, "/proc", 0, "none");

    /* Initialize ATA subsystem — probe all 4 drives
     * (primary/secondary x master/slave). */
    (void)ata_pio_init();

    /* Register detected ATA drives as /dev/hdX block device nodes */
    extern void ata_register_devfs(void);
    ata_register_devfs();

    /* Register ATA drives as generic block devices (used by fat/ext2) */
    blockdev_init_lock();
    blockdev_register_ata();

    /* Initialize partition subsystem and scan for partitions */
    partition_init_lock();
    for (int i = 0; i < blockdev_count(); i++) {
        block_device_t* bdev = blockdev_get(i);
        if (bdev) {
            partition_scan_mbr(bdev);
        }
    }

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
        block_device_t* bdev = NULL;
        uint32_t lba = 0;
        if (init_resolve_mount_device(root_dev, &bdev, &lba) == 0) {
            /* Auto-detect: try ext2, then fat (non-destructive probes). */
            static const char* fstypes[] = { "ext2", "fat", NULL };
            int mounted = 0;
            for (int i = 0; fstypes[i]; i++) {
                if (init_mount_fs(fstypes[i], bdev, lba, "/disk", 0, root_dev) == 0) {
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
    } else {
        /* No root= on cmdline — try to auto-mount primary master if present */
        block_device_t* bdev = blockdev_find("hda");
        if (bdev) {
            static const char* fstypes[] = { "ext2", "fat", NULL };
            for (int i = 0; fstypes[i]; i++) {
                if (init_mount_fs(fstypes[i], bdev, 0, "/disk", 0, "/dev/hda") == 0) {
                    kprintf("[INIT] /dev/hda auto-mounted as %s on /disk\n", fstypes[i]);
                    break;
                }
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
