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
#include "persistfs.h"
#include "diskfs.h"
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

#include "ata_pio.h"
#include "hal/mm.h"
#include "heap.h"
#include "utils.h"
#include "kernel/cmdline.h"

#include <stddef.h>

/* ---- Mount helper: used by fstab parser and kconsole 'mount' command ---- */

int init_mount_fs(const char* fstype, int drive, uint32_t lba, const char* mountpoint) {
    fs_node_t* root = NULL;

    if (strcmp(fstype, "diskfs") == 0) {
        root = diskfs_create_root(drive);
    } else if (strcmp(fstype, "fat") == 0) {
        root = fat_mount(drive, lba);
    } else if (strcmp(fstype, "ext2") == 0) {
        root = ext2_mount(drive, lba);
    } else if (strcmp(fstype, "persistfs") == 0) {
        root = persistfs_create_root(drive);
    } else {
        kprintf("[MOUNT] Unknown filesystem type: %s\n", fstype);
        return -1;
    }

    if (!root) {
        kprintf("[MOUNT] Failed to mount %s on /dev/%s at %s\n",
                fstype, ata_drive_to_name(drive) ? ata_drive_to_name(drive) : "?",
                mountpoint);
        return -1;
    }

    if (vfs_mount(mountpoint, root) < 0) {
        kprintf("[MOUNT] Failed to register mount at %s\n", mountpoint);
        return -1;
    }

    kprintf("[MOUNT] %s on /dev/%s -> %s\n",
            fstype, ata_drive_to_name(drive) ? ata_drive_to_name(drive) : "?",
            mountpoint);
    return 0;
}

/* ---- /etc/fstab parser ---- */

/* fstab format (one entry per line, '#' comments):
 *   <device>  <mountpoint>  <fstype>  [options]
 * Example:
 *   /dev/hda  /disk    diskfs   defaults
 *   /dev/hda  /persist persistfs defaults
 *   /dev/hdb  /ext2    ext2     defaults
 */
static void init_parse_fstab(void) {
    fs_node_t* fstab = vfs_lookup("/etc/fstab");
    if (!fstab) return;

    uint32_t len = fstab->length;
    if (len == 0 || len > 4096) return;

    uint8_t* buf = (uint8_t*)kmalloc(len + 1);
    if (!buf) return;

    uint32_t rd = vfs_read(fstab, 0, len, buf);
    buf[rd] = '\0';

    kprintf("[FSTAB] Parsing /etc/fstab (%u bytes)\n", rd);

    /* Parse line by line */
    char* p = (char*)buf;
    while (*p) {
        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        if (*p == '#' || *p == '\n') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }

        /* Extract device field */
        char* dev_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        char dev_end_ch = *p; *p = '\0';
        char device[32];
        strncpy(device, dev_start, sizeof(device) - 1);
        device[sizeof(device) - 1] = '\0';
        *p = dev_end_ch;
        if (*p == '\n' || *p == '\0') { if (*p == '\n') p++; continue; }

        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;

        /* Extract mountpoint field */
        char* mp_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        char mp_end_ch = *p; *p = '\0';
        char mountpoint[64];
        strncpy(mountpoint, mp_start, sizeof(mountpoint) - 1);
        mountpoint[sizeof(mountpoint) - 1] = '\0';
        *p = mp_end_ch;
        if (*p == '\n' || *p == '\0') { if (*p == '\n') p++; continue; }

        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;

        /* Extract fstype field */
        char* fs_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        char fs_end_ch = *p; *p = '\0';
        char fstype[16];
        strncpy(fstype, fs_start, sizeof(fstype) - 1);
        fstype[sizeof(fstype) - 1] = '\0';
        *p = fs_end_ch;

        /* Skip rest of line */
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        /* Parse device: expect /dev/hdX */
        int drive = -1;
        if (strncmp(device, "/dev/", 5) == 0) {
            drive = ata_name_to_drive(device + 5);
        }
        if (drive < 0) {
            kprintf("[FSTAB] Unknown device: %s\n", device);
            continue;
        }
        if (!ata_pio_drive_present(drive)) {
            kprintf("[FSTAB] Device %s not present, skipping\n", device);
            continue;
        }

        (void)init_mount_fs(fstype, drive, 0, mountpoint);
    }

    kfree(buf);
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
                (void)vfs_mount("/", ovl);
            }
        }
    }

    fs_node_t* tmp = tmpfs_create_root();
    if (tmp) {
        static const uint8_t hello[] = "hello from tmpfs\n";
        (void)tmpfs_add_file(tmp, "hello.txt", hello, (uint32_t)(sizeof(hello) - 1));
        (void)vfs_mount("/tmp", tmp);
    }

    /* Register hardware drivers with HAL and init in priority order */
    pci_driver_register();      /* priority 10: bus */
    e1000_driver_register();    /* priority 20: NIC (probes PCI) */
    extern void virtio_blk_driver_register(void);
    virtio_blk_driver_register(); /* priority 25: virtio-blk */
    hal_drivers_init_all();

    net_init();
    net_ping_test();
    ksocket_init();
    vbe_init(bi);

    tty_init();
    pty_init();

    fs_node_t* dev = devfs_create_root();
    if (dev) {
        (void)vfs_mount("/dev", dev);
    }

    vbe_register_devfs();
    keyboard_register_devfs();

    fs_node_t* proc = procfs_create_root();
    if (proc) {
        (void)vfs_mount("/proc", proc);
    }

    /* Initialize ATA subsystem — probe all 4 drives
     * (primary/secondary x master/slave). */
    (void)ata_pio_init();

    /* If root= is specified on the kernel command line, mount that device
     * as the disk root filesystem.  The filesystem type is auto-detected
     * by trying each supported type in order.
     * Example:  root=/dev/hda  or  root=/dev/hdb */
    const char* root_dev = cmdline_get("root");
    if (root_dev) {
        int drive = -1;
        if (strncmp(root_dev, "/dev/", 5) == 0)
            drive = ata_name_to_drive(root_dev + 5);
        if (drive >= 0 && ata_pio_drive_present(drive)) {
            /* Try auto-detect: diskfs, fat, ext2 */
            static const char* fstypes[] = { "diskfs", "fat", "ext2", NULL };
            int mounted = 0;
            for (int i = 0; fstypes[i]; i++) {
                if (init_mount_fs(fstypes[i], drive, 0, "/disk") == 0) {
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
    }

    /* Disk-based filesystems can also be mounted via /etc/fstab entries
     * or manually via the kconsole 'mount' command. */
    init_parse_fstab();

    if (!fs_root) {
        kprintf("[INIT] No root filesystem -- cannot start userspace.\n");
        return -1;
    }

    int user_ret = arch_platform_start_userspace(bi);

    if (cmdline_has("ring3")) {
        arch_platform_usermode_test_start();
    }

    return user_ret;
}
