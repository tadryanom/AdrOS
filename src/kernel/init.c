#include "kernel/init.h"

#include "arch/arch_platform.h"

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

#include "hal/mm.h"

#include <stddef.h>

static int cmdline_has_token(const char* cmdline, const char* token) {
    if (!cmdline || !token) return 0;

    for (size_t i = 0; cmdline[i] != 0; i++) {
        size_t j = 0;
        while (token[j] != 0 && cmdline[i + j] == token[j]) {
            j++;
        }
        if (token[j] == 0) {
            char before = (i == 0) ? ' ' : cmdline[i - 1];
            char after = cmdline[i + j];
            int before_ok = (before == ' ' || before == '\t');
            int after_ok = (after == 0 || after == ' ' || after == '\t');
            if (before_ok && after_ok) return 1;
        }
    }

    return 0;
}

int init_start(const struct boot_info* bi) {
    if (bi && bi->initrd_start) {
        uintptr_t initrd_virt = 0;
        if (hal_mm_map_physical_range((uintptr_t)bi->initrd_start, (uintptr_t)bi->initrd_end,
                                      HAL_MM_MAP_RW, &initrd_virt) == 0) {
            fs_root = initrd_init((uint32_t)initrd_virt);
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

    pci_init();
    e1000_init();
    net_init();
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

    fs_node_t* persist = persistfs_create_root();
    if (persist) {
        (void)vfs_mount("/persist", persist);
    }

    fs_node_t* disk = diskfs_create_root();
    if (disk) {
        (void)vfs_mount("/disk", disk);
    }

    fs_node_t* proc = procfs_create_root();
    if (proc) {
        (void)vfs_mount("/proc", proc);
    }

    /* Probe second IDE disk partition (LBA 0) for FAT or ext2.
     * The primary disk is used by diskfs; a second partition could
     * be formatted as FAT or ext2 and mounted at /mnt. */
    {
        fs_node_t* fatfs = fat_mount(0);
        if (fatfs) {
            (void)vfs_mount("/fat", fatfs);
        }
    }
    {
        fs_node_t* ext2fs = ext2_mount(0);
        if (ext2fs) {
            (void)vfs_mount("/ext2", ext2fs);
        }
    }

    if (!fs_root) {
        kprintf("[INIT] No root filesystem — cannot start userspace.\n");
        return -1;
    }

    int user_ret = arch_platform_start_userspace(bi);

    if (bi && cmdline_has_token(bi->cmdline, "ring3")) {
        arch_platform_usermode_test_start();
    }

    return user_ret;
}
