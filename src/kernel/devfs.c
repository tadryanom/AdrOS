// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "devfs.h"
#include "fs.h"
#include "blockdev.h"

#include "errno.h"
#include "utils.h"
#include "csprng.h"

extern uint32_t get_tick_count(void);

struct devfs_root {
    fs_node_t vfs;
};

static struct devfs_root g_dev_root;
static fs_node_t g_dev_null;
static fs_node_t g_dev_zero;
static fs_node_t g_dev_random;
static fs_node_t g_dev_urandom;
static fs_node_t g_dev_vda;
static uint32_t g_devfs_inited = 0;

static struct fs_node* devfs_finddir_impl(struct fs_node* node, const char* name);
static int devfs_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len);
static uint32_t dev_null_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static uint32_t dev_null_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
static uint32_t dev_zero_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static uint32_t dev_zero_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
static uint32_t dev_random_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static uint32_t dev_random_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
static int dev_null_poll(fs_node_t* node, int events);
static int dev_always_ready_poll(fs_node_t* node, int events);

static const struct file_operations devfs_dir_ops = {0};

static const struct inode_operations devfs_dir_iops = {
    .lookup  = devfs_finddir_impl,
    .readdir = devfs_readdir_impl,
};

static const struct file_operations dev_null_ops = {
    .read  = dev_null_read,
    .write = dev_null_write,
    .poll  = dev_null_poll,
};

static const struct file_operations dev_zero_ops = {
    .read  = dev_zero_read,
    .write = dev_zero_write,
    .poll  = dev_always_ready_poll,
};

static const struct file_operations dev_random_ops = {
    .read  = dev_random_read,
    .write = dev_random_write,
    .poll  = dev_always_ready_poll,
};

/* --- Device registry --- */
static fs_node_t* g_registered[DEVFS_MAX_DEVICES];
static int g_registered_count = 0;

int devfs_register_device(fs_node_t *node) {
    if (!node) return -1;
    if (g_registered_count >= DEVFS_MAX_DEVICES) return -1;
    g_registered[g_registered_count++] = node;
    return 0;
}

/* ---- Partition device nodes ---- */
/* Register partition devices (placeholder for future implementation) */
void devfs_register_partitions(void) {
    /* Placeholder - will be implemented when partition scanning is active */
}

static uint32_t dev_null_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    (void)size;
    (void)buffer;
    return 0;
}

static uint32_t dev_null_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    (void)offset;
    (void)buffer;
    return size;
}

static uint32_t dev_zero_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    if (buffer && size > 0)
        memset(buffer, 0, size);
    return size;
}

static uint32_t dev_zero_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    (void)offset;
    (void)buffer;
    return size;
}

static uint32_t dev_random_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    if (!buffer || size == 0) return 0;
    /* M8: Use central CSPRNG for cryptographic randomness */
    csprng_get_bytes(buffer, size);
    return size;
}

static uint32_t dev_random_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    (void)offset;
    /* M8: Add entropy to central CSPRNG */
    if (buffer && size > 0) {
        csprng_add_entropy(buffer, size);
    }
    return size;
}

static int dev_null_poll(fs_node_t* node, int events) {
    (void)node;
    int revents = 0;
    if (events & VFS_POLL_IN) revents |= VFS_POLL_IN | VFS_POLL_HUP;
    if (events & VFS_POLL_OUT) revents |= VFS_POLL_OUT;
    return revents;
}

static int dev_always_ready_poll(fs_node_t* node, int events) {
    (void)node;
    int revents = 0;
    if (events & VFS_POLL_IN) revents |= VFS_POLL_IN;
    if (events & VFS_POLL_OUT) revents |= VFS_POLL_OUT;
    return revents;
}

static struct fs_node* devfs_finddir_impl(struct fs_node* node, const char* name) {
    (void)node;
    if (!name || name[0] == 0) return NULL;

    if (strcmp(name, "null") == 0) return &g_dev_null;
    if (strcmp(name, "zero") == 0) return &g_dev_zero;
    if (strcmp(name, "random") == 0) return &g_dev_random;
    if (strcmp(name, "urandom") == 0) return &g_dev_urandom;

    for (int i = 0; i < g_registered_count; i++) {
        if (strcmp(g_registered[i]->name, name) == 0)
            return g_registered[i];
    }
    return NULL;
}

static int devfs_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    (void)node;
    if (!inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    /* Built-in devices (owned by devfs) */
    static const struct { const char* name; uint32_t ino; uint8_t type; } builtins[] = {
        { "null",    2,  FS_CHARDEVICE },
        { "zero",    7,  FS_CHARDEVICE },
        { "random",  8,  FS_CHARDEVICE },
        { "urandom", 9,  FS_CHARDEVICE },
    };
    enum { NBUILTINS = 4 };

    uint32_t total = (uint32_t)(NBUILTINS + g_registered_count);
    uint32_t idx = *inout_index;
    uint32_t cap = buf_len / (uint32_t)sizeof(struct vfs_dirent);
    struct vfs_dirent* ents = (struct vfs_dirent*)buf;
    uint32_t written = 0;

    while (written < cap) {
        struct vfs_dirent e;
        memset(&e, 0, sizeof(e));

        if (idx == 0) {
            e.d_ino = 1; e.d_type = FS_DIRECTORY; strcpy(e.d_name, ".");
        } else if (idx == 1) {
            e.d_ino = 1; e.d_type = FS_DIRECTORY; strcpy(e.d_name, "..");
        } else {
            uint32_t di = idx - 2;
            if (di >= total) break;
            if (di < NBUILTINS) {
                e.d_ino = builtins[di].ino;
                e.d_type = builtins[di].type;
                strncpy(e.d_name, builtins[di].name, sizeof(e.d_name) - 1);
                e.d_name[sizeof(e.d_name) - 1] = '\0';
            } else {
                fs_node_t* rn = g_registered[di - NBUILTINS];
                e.d_ino = rn->inode;
                e.d_type = (uint8_t)rn->flags;
                strncpy(e.d_name, rn->name, sizeof(e.d_name) - 1);
                e.d_name[sizeof(e.d_name) - 1] = '\0';
            }
        }

        e.d_reclen = (uint16_t)sizeof(e);
        ents[written] = e;
        written++;
        idx++;
    }

    *inout_index = idx;
    return (int)(written * (uint32_t)sizeof(struct vfs_dirent));
}

static void devfs_init_once(void) {
    if (g_devfs_inited) return;
    g_devfs_inited = 1;

    memset(&g_dev_root, 0, sizeof(g_dev_root));
    strcpy(g_dev_root.vfs.name, "dev");
    g_dev_root.vfs.flags = FS_DIRECTORY;
    g_dev_root.vfs.inode = 1;
    g_dev_root.vfs.length = 0;
    g_dev_root.vfs.f_ops = &devfs_dir_ops;
    g_dev_root.vfs.i_ops = &devfs_dir_iops;

    memset(&g_dev_null, 0, sizeof(g_dev_null));
    strcpy(g_dev_null.name, "null");
    g_dev_null.flags = FS_CHARDEVICE;
    g_dev_null.inode = 2;
    g_dev_null.f_ops = &dev_null_ops;

    memset(&g_dev_zero, 0, sizeof(g_dev_zero));
    strcpy(g_dev_zero.name, "zero");
    g_dev_zero.flags = FS_CHARDEVICE;
    g_dev_zero.inode = 7;
    g_dev_zero.f_ops = &dev_zero_ops;

    memset(&g_dev_random, 0, sizeof(g_dev_random));
    strcpy(g_dev_random.name, "random");
    g_dev_random.flags = FS_CHARDEVICE;
    g_dev_random.inode = 8;
    g_dev_random.f_ops = &dev_random_ops;

    memset(&g_dev_urandom, 0, sizeof(g_dev_urandom));
    strcpy(g_dev_urandom.name, "urandom");
    g_dev_urandom.flags = FS_CHARDEVICE;
    g_dev_urandom.inode = 9;
    g_dev_urandom.f_ops = &dev_random_ops;

    /* Initialize /dev/vda block device node (placeholder, will be registered by virtio-blk) */
    memset(&g_dev_vda, 0, sizeof(g_dev_vda));
    strcpy(g_dev_vda.name, "vda");
    g_dev_vda.flags = FS_BLOCKDEVICE;
    g_dev_vda.inode = 10;
    /* f_ops will be set by virtio-blk driver when it registers */
    devfs_register_device(&g_dev_vda);
}

fs_node_t* devfs_create_root(void) {
    devfs_init_once();
    return &g_dev_root.vfs;
}

/* ---- VFS mount interface ---- */
vfs_mount_result_t devfs_mount(block_device_t* bdev, uint32_t lba) {
    (void)bdev;
    (void)lba;
    vfs_mount_result_t result = {NULL, NULL};
    result.root = devfs_create_root();
    return result;
}

void devfs_kill_sb(vfs_superblock_t* sb) {
    (void)sb;
    /* devfs uses static globals, no cleanup needed */
}
