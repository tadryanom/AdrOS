// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>
#include "spinlock.h"

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_SYMLINK     0x05
#define FS_SOCKET      0x06

/* Mount flags (match userspace sys/mount.h values) */
#define MS_RDONLY      1       /* Mount read-only */
#define MS_NOSUID      2       /* Ignore suid/sgid bits */
#define MS_NODEV       4       /* Disallow access to device special files */
#define MS_NOEXEC      8       /* Disallow program execution */
#define MS_SYNCHRONOUS 16
#define MS_REMOUNT     32      /* Alter flags of existing mount */

/* poll() event flags — shared between kernel VFS and syscall layer */
#define VFS_POLL_IN    0x0001
#define VFS_POLL_OUT   0x0004
#define VFS_POLL_ERR   0x0008
#define VFS_POLL_HUP   0x0010

struct fs_node; /* forward declaration for file_operations */

/* File operations — per-open-fd I/O (requires an open file descriptor) */
struct file_operations {
    uint32_t (*read)(struct fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct fs_node* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
    void (*open)(struct fs_node* node);
    void (*close)(struct fs_node* node);
    int (*ioctl)(struct fs_node* node, uint32_t cmd, void* arg);
    uintptr_t (*mmap)(struct fs_node* node, uintptr_t addr, uint32_t length, uint32_t prot, uint32_t offset);
    int (*poll)(struct fs_node* node, int events);
};

/* Inode operations — namespace / metadata (no open fd required) */
struct inode_operations {
    struct fs_node* (*lookup)(struct fs_node* dir, const char* name);
    int (*readdir)(struct fs_node* dir, uint32_t* inout_index, void* buf, uint32_t buf_len);
    int (*create)(struct fs_node* dir, const char* name, uint32_t flags, struct fs_node** out);
    int (*mkdir)(struct fs_node* dir, const char* name);
    int (*unlink)(struct fs_node* dir, const char* name);
    int (*rmdir)(struct fs_node* dir, const char* name);
    int (*rename)(struct fs_node* old_dir, const char* old_name,
                  struct fs_node* new_dir, const char* new_name);
    int (*truncate)(struct fs_node* node, uint32_t length);
    int (*link)(struct fs_node* dir, const char* name, struct fs_node* target);
};

typedef struct fs_node {
    char name[128];
    uint32_t flags;
    uint32_t inode;
    uint32_t length;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    char symlink_target[128];

    const struct file_operations* f_ops;
    const struct inode_operations* i_ops;
} fs_node_t;

struct vfs_dirent {
    uint32_t d_ino;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[24];
};

// Standard VFS functions
uint32_t vfs_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
void vfs_open(fs_node_t* node);
void vfs_close(fs_node_t* node);

fs_node_t* vfs_lookup(const char* path);

/* Lookup from the saved initrd root (immune to pivot_root). */
void vfs_set_initrd_root(fs_node_t* root);
fs_node_t* vfs_lookup_initrd(const char* path);

// Resolve path to (parent_dir, basename).  Returns parent node or NULL.
fs_node_t* vfs_lookup_parent(const char* path, char* name_out, size_t name_sz);

// Directory mutation wrappers — route through mount points transparently
int vfs_create(const char* path, uint32_t flags, fs_node_t** out);
int vfs_mkdir(const char* path);
int vfs_mkdirp(const char* path);
int vfs_unlink(const char* path);
int vfs_rmdir(const char* path);
int vfs_rename(const char* old_path, const char* new_path);
int vfs_truncate(const char* path, uint32_t length);
int vfs_link(const char* old_path, const char* new_path);

int vfs_mount(const char* mountpoint, fs_node_t* root);
int vfs_mount_full(const char* mountpoint, fs_node_t* root,
                    const char* fstype, const char* source,
                    unsigned long flags);
int vfs_umount(const char* mountpoint);

/* _nolock variants — caller must already hold g_vfs_lock.
 * Used by compound operations like pivot_root that need atomicity
 * across multiple mount-table modifications. */
int vfs_mount_nolock(const char* mountpoint, fs_node_t* root);
int vfs_mount_nolock_full(const char* mountpoint, fs_node_t* root,
                            const char* fstype, const char* source,
                            unsigned long flags);
int vfs_umount_nolock(const char* mountpoint);

/* Read mount table for /proc/mounts. Returns bytes written. */
uint32_t vfs_mounts_read(uint8_t* buffer, uint32_t size);

/* Look up mount flags for the filesystem containing the given path. */
unsigned long vfs_mount_flags(const char* path);

/* Look up mount flags by mount root node pointer. */
unsigned long vfs_node_mount_flags(const fs_node_t* root);

/* Find the mount root fs_node for a given path. */
fs_node_t* vfs_find_mount_root(const char* path);

/* Increment/decrement mount refcount (called on file open/close). */
void vfs_mount_ref(fs_node_t* mount_root);
void vfs_mount_unref(fs_node_t* mount_root);

/* Global VFS spinlock — protects fs_root, g_mounts[], g_mount_count.
 * Acquire via spin_lock_irqsave() for any compound VFS mutation. */
extern spinlock_t g_vfs_lock;

// Global root of the filesystem
extern fs_node_t* fs_root;

#endif
