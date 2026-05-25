// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "fs.h"

#include "utils.h"
#include "errno.h"
#include "spinlock.h"
#include "console.h"

#include <string.h>

fs_node_t* fs_root = NULL;
static fs_node_t* g_initrd_root = NULL;

/* Global VFS spinlock — protects fs_root, g_mounts[], and g_mount_count.
 * Must be held across any compound operation (e.g. pivot_root) that
 * modifies more than one of these fields. */
spinlock_t g_vfs_lock;

struct vfs_mount {
    char mountpoint[128];
    char fstype[32];      /* e.g. "overlayfs", "tmpfs", "devfs", "procfs", "fat", "ext2" */
    char source[64];     /* e.g. "/dev/hda", "none", "initrd" */
    unsigned long flags; /* MS_RDONLY, MS_NOSUID, etc. */
    int refcount;        /* number of open files on this mount */
    const block_device_t* bdev; /* block device (NULL for virtual FS) */
    fs_node_t* root;
};

static struct vfs_mount g_mounts[32];
static int g_mount_count = 0;

static int path_is_mountpoint_prefix(const char* mp, const char* path) {
    size_t mpl = strlen(mp);
    if (mpl == 0) return 0;
    if (strcmp(mp, "/") == 0) return 1;

    if (strncmp(path, mp, mpl) != 0) return 0;
    if (path[mpl] == 0) return 1;
    if (path[mpl] == '/') return 1;
    return 0;
}

static void normalize_mountpoint(const char* in, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = 0;
    if (!in || in[0] == 0) {
        strcpy(out, "/");
        return;
    }

    size_t i = 0;
    if (in[0] != '/') {
        out[i++] = '/';
    }

    for (size_t j = 0; in[j] != 0 && i + 1 < out_sz; j++) {
        out[i++] = in[j];
    }
    out[i] = 0;

    size_t l = strlen(out);
    while (l > 1 && out[l - 1] == '/') {
        out[l - 1] = 0;
        l--;
    }
}

int vfs_mount_nolock_full(const char* mountpoint, fs_node_t* root,
                            const char* fstype, const char* source,
                            unsigned long flags, const block_device_t* bdev) {
    char mp[128];
    normalize_mountpoint(mountpoint, mp, sizeof(mp));

    for (int i = 0; i < g_mount_count; i++) {
        if (strcmp(g_mounts[i].mountpoint, mp) == 0) {
            /* Remount: update provided fields only */
            if (root) g_mounts[i].root = root;
            if (fstype) {
                strncpy(g_mounts[i].fstype, fstype, sizeof(g_mounts[i].fstype) - 1);
                g_mounts[i].fstype[sizeof(g_mounts[i].fstype) - 1] = '\0';
            }
            if (source) {
                strncpy(g_mounts[i].source, source, sizeof(g_mounts[i].source) - 1);
                g_mounts[i].source[sizeof(g_mounts[i].source) - 1] = '\0';
            }
            /* Always update flags on remount (even if 0, caller explicitly set them) */
            g_mounts[i].flags = flags;
            g_mounts[i].bdev = bdev;
            return 0;
        }
    }

    /* New mount entry — check table space first, then require root */
    if (g_mount_count >= (int)(sizeof(g_mounts) / sizeof(g_mounts[0]))) return -ENOSPC;
    if (!root) return -EINVAL;

    strcpy(g_mounts[g_mount_count].mountpoint, mp);
    g_mounts[g_mount_count].root = root;
    if (fstype) {
        strncpy(g_mounts[g_mount_count].fstype, fstype, sizeof(g_mounts[g_mount_count].fstype) - 1);
        g_mounts[g_mount_count].fstype[sizeof(g_mounts[g_mount_count].fstype) - 1] = '\0';
    } else {
        g_mounts[g_mount_count].fstype[0] = '\0';
    }
    if (source) {
        strncpy(g_mounts[g_mount_count].source, source, sizeof(g_mounts[g_mount_count].source) - 1);
        g_mounts[g_mount_count].source[sizeof(g_mounts[g_mount_count].source) - 1] = '\0';
    } else {
        g_mounts[g_mount_count].source[0] = '\0';
    }
    g_mounts[g_mount_count].flags = flags;
    g_mounts[g_mount_count].bdev = bdev;
    g_mount_count++;
    return 0;
}

int vfs_mount_nolock(const char* mountpoint, fs_node_t* root) {
    return vfs_mount_nolock_full(mountpoint, root, NULL, NULL, 0, NULL);
}

int vfs_mount_full(const char* mountpoint, fs_node_t* root,
                    const char* fstype, const char* source,
                    unsigned long flags, const block_device_t* bdev) {
    uintptr_t fl = spin_lock_irqsave(&g_vfs_lock);
    int ret = vfs_mount_nolock_full(mountpoint, root, fstype, source, flags, bdev);
    spin_unlock_irqrestore(&g_vfs_lock, fl);
    return ret;
}

int vfs_mount(const char* mountpoint, fs_node_t* root) {
    return vfs_mount_full(mountpoint, root, NULL, NULL, 0, NULL);
}

int vfs_umount_nolock(const char* mountpoint) {
    char mp[128];
    normalize_mountpoint(mountpoint, mp, sizeof(mp));

    if (strcmp(mp, "/") == 0) return -EBUSY;

    /* Find the mount entry */
    int idx = -1;
    for (int i = 0; i < g_mount_count; i++) {
        if (strcmp(g_mounts[i].mountpoint, mp) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return -EINVAL;

    /* Busy check: reject if there are open files on this mount */
    if (g_mounts[idx].refcount > 0) return -EBUSY;

    /* Busy check: reject if any other mount is a child of this one */
    size_t mplen = strlen(mp);
    for (int i = 0; i < g_mount_count; i++) {
        if (i == idx) continue;
        const char* other = g_mounts[i].mountpoint;
        size_t olen = strlen(other);
        if (olen > mplen &&
            strncmp(other, mp, mplen) == 0 &&
            other[mplen] == '/') {
            return -EBUSY;  /* child mount still active */
        }
    }

    /* Release the block device */
    if (g_mounts[idx].bdev) {
        blockdev_release(g_mounts[idx].bdev);
    }

    for (int j = idx; j < g_mount_count - 1; j++)
        g_mounts[j] = g_mounts[j + 1];
    g_mount_count--;
    return 0;
}

int vfs_umount(const char* mountpoint) {
    uintptr_t fl = spin_lock_irqsave(&g_vfs_lock);
    int ret = vfs_umount_nolock(mountpoint);
    spin_unlock_irqrestore(&g_vfs_lock, fl);
    return ret;
}

/* Read the mount table into a user buffer for /proc/mounts.
 * Format per line: <source> <mountpoint> <fstype> <options>\n
 * Returns number of bytes written. */
uint32_t vfs_mounts_read(uint8_t* buffer, uint32_t size) {
    uintptr_t fl = spin_lock_irqsave(&g_vfs_lock);

    char tmp[2048];
    uint32_t len = 0;

    for (int i = 0; i < g_mount_count && len < sizeof(tmp) - 80; i++) {
        const char* src = g_mounts[i].source[0] ? g_mounts[i].source : "none";
        const char* fst = g_mounts[i].fstype[0] ? g_mounts[i].fstype : "unknown";

        /* Build options string from flags */
        char opts[64];
        uint32_t olen = 0;

        /* rw/ro */
        if (g_mounts[i].flags & MS_RDONLY) {
            opts[olen++] = 'r'; opts[olen++] = 'o';
        } else {
            opts[olen++] = 'r'; opts[olen++] = 'w';
        }
        if (g_mounts[i].flags & MS_NOSUID) { opts[olen++] = ','; const char* s = "nosuid"; while (*s) opts[olen++] = *s++; }
        if (g_mounts[i].flags & MS_NODEV)  { opts[olen++] = ','; const char* s = "nodev"; while (*s) opts[olen++] = *s++; }
        if (g_mounts[i].flags & MS_NOEXEC) { opts[olen++] = ','; const char* s = "noexec"; while (*s) opts[olen++] = *s++; }
        opts[olen] = '\0';

        /* source mountpoint fstype options */
        len += (uint32_t)ksnprintf(tmp + len, sizeof(tmp) - len,
                    "%s %s %s %s 0 0\n", src, g_mounts[i].mountpoint, fst, opts);
    }

    spin_unlock_irqrestore(&g_vfs_lock, fl);

    if (len > size) len = size;
    memcpy(buffer, tmp, len);
    return len;
}

uint32_t vfs_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node->f_ops && node->f_ops->read)
        return node->f_ops->read(node, offset, size, buffer);
    return 0;
}

uint32_t vfs_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (node->f_ops && node->f_ops->write)
        return node->f_ops->write(node, offset, size, buffer);
    return 0;
}

void vfs_open(fs_node_t* node) {
    if (node->f_ops && node->f_ops->open)
        node->f_ops->open(node);
}

void vfs_close(fs_node_t* node) {
    if (node->f_ops && node->f_ops->close)
        node->f_ops->close(node);
}

static fs_node_t* vfs_lookup_depth(const char* path, int depth);

fs_node_t* vfs_lookup(const char* path) {
    return vfs_lookup_depth(path, 0);
}

void vfs_set_initrd_root(fs_node_t* root) {
    g_initrd_root = root;
}

fs_node_t* vfs_lookup_initrd(const char* path) {
    if (!path || !g_initrd_root) return NULL;

    /* Direct finddir traversal from the saved initrd root.
     * Bypasses the VFS mount table so that pivot_root (which changes
     * fs_root and the "/" mount entry) does not break execve lookups. */
    const char* p = path;
    while (*p == '/') p++;
    if (*p == 0) return g_initrd_root;

    fs_node_t* cur = g_initrd_root;
    char part[128];
    while (*p != 0) {
        size_t i = 0;
        while (*p != 0 && *p != '/') {
            if (i + 1 < sizeof(part)) part[i++] = *p;
            p++;
        }
        part[i] = 0;
        while (*p == '/') p++;
        if (part[0] == 0) continue;
        if (!cur) return NULL;
        fs_node_t* (*fn_finddir)(fs_node_t*, const char*) = NULL;
        if (cur->i_ops && cur->i_ops->lookup) fn_finddir = cur->i_ops->lookup;
        if (!fn_finddir) return NULL;
        cur = fn_finddir(cur, part);
        if (!cur) return NULL;
    }
    return cur;
}

static fs_node_t* vfs_lookup_depth(const char* path, int depth) {
    if (!path) return NULL;
    if (depth > 8) return NULL;

    /* Snapshot mount-table state under the VFS lock so that concurrent
     * mount/umount/pivot_root on another CPU cannot corrupt our view. */
    uintptr_t fl = spin_lock_irqsave(&g_vfs_lock);

    if (!fs_root) { spin_unlock_irqrestore(&g_vfs_lock, fl); return NULL; }

    fs_node_t* base = fs_root;
    const char* rel = path;
    size_t best_len = 0;

    for (int i = 0; i < g_mount_count; i++) {
        const char* mp = g_mounts[i].mountpoint;
        if (!mp[0] || !g_mounts[i].root) continue;

        if (path_is_mountpoint_prefix(mp, path)) {
            size_t mpl = strlen(mp);
            if (mpl >= best_len) {
                best_len = mpl;
                base = g_mounts[i].root;
                rel = path + mpl;
            }
        }
    }

    spin_unlock_irqrestore(&g_vfs_lock, fl);

    if (!rel) return NULL;
    while (*rel == '/') rel++;
    if (*rel == 0) return base;

    const char* p = rel;
    fs_node_t* cur = base;

    char part[128];
    while (*p != 0) {
        size_t i = 0;
        while (*p != 0 && *p != '/') {
            if (i + 1 < sizeof(part)) {
                part[i++] = *p;
            }
            p++;
        }
        part[i] = 0;

        while (*p == '/') p++;

        if (part[0] == 0) continue;

        if (!cur) return NULL;
        fs_node_t* (*fn_finddir)(fs_node_t*, const char*) = NULL;
        if (cur->i_ops && cur->i_ops->lookup) fn_finddir = cur->i_ops->lookup;
        if (!fn_finddir) return NULL;
        cur = fn_finddir(cur, part);
        if (!cur) return NULL;

        if (cur->flags == FS_SYMLINK && cur->symlink_target[0]) {
            cur = vfs_lookup_depth(cur->symlink_target, depth + 1);
            if (!cur) return NULL;
        }
    }

    return cur;
}

/* Split path into dirname + basename.  Returns the parent directory node. */
fs_node_t* vfs_lookup_parent(const char* path, char* name_out, size_t name_sz) {
    if (!path || !name_out || name_sz == 0) return NULL;
    name_out[0] = 0;

    /* Find last '/' separator */
    const char* last_slash = NULL;
    for (const char* p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }

    if (!last_slash) return NULL; /* no slash = relative, not supported */

    /* Build parent path */
    char parent_path[128];
    size_t plen = (size_t)(last_slash - path);
    if (plen == 0) plen = 1; /* root "/" */
    if (plen >= sizeof(parent_path)) plen = sizeof(parent_path) - 1;
    memcpy(parent_path, path, plen);
    parent_path[plen] = 0;

    /* Extract basename */
    const char* base = last_slash + 1;
    size_t blen = strlen(base);
    if (blen == 0) return NULL; /* trailing slash, no basename */
    if (blen >= name_sz) blen = name_sz - 1;
    memcpy(name_out, base, blen);
    name_out[blen] = 0;

    return vfs_lookup(parent_path);
}

int vfs_create(const char* path, uint32_t flags, fs_node_t** out) {
    if (!path || !out) return -EINVAL;
    char name[128];
    fs_node_t* parent = vfs_lookup_parent(path, name, sizeof(name));
    if (!parent) return -ENOENT;
    if (parent->flags != FS_DIRECTORY) return -ENOTDIR;
    if (parent->i_ops && parent->i_ops->create)
        return parent->i_ops->create(parent, name, flags, out);
    return -ENOSYS;
}

int vfs_mkdir(const char* path) {
    if (!path) return -EINVAL;
    char name[128];
    fs_node_t* parent = vfs_lookup_parent(path, name, sizeof(name));
    if (!parent) return -ENOENT;
    if (parent->flags != FS_DIRECTORY) return -ENOTDIR;
    if (parent->i_ops && parent->i_ops->mkdir)
        return parent->i_ops->mkdir(parent, name);
    return -ENOSYS;
}

/* Recursive mkdir -p: create intermediate directories as needed.
 * Returns 0 on success, -EEXIST if the directory already exists,
 * or a negative errno on failure. */
int vfs_mkdirp(const char* path) {
    if (!path || path[0] != '/') return -EINVAL;

    /* Check if it already exists */
    fs_node_t* node = vfs_lookup(path);
    if (node) {
        if (node->flags & FS_DIRECTORY) return -EEXIST;
        return -ENOTDIR;
    }

    /* Walk the path creating each missing component */
    char tmp[128];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    /* Skip leading '/' */
    char* p = tmp + 1;
    char* slash;

    while ((slash = p) && *p) {
        /* Find next '/' */
        while (*p && *p != '/') p++;
        if (*p == '/') { *p = '\0'; p++; }

        /* Check if this component exists */
        fs_node_t* check = vfs_lookup(tmp);
        if (!check) {
            /* Create it */
            int rc = vfs_mkdir(tmp);
            if (rc < 0 && rc != -EEXIST) return rc;
        } else if (!(check->flags & FS_DIRECTORY)) {
            return -ENOTDIR;
        }

        /* Restore slash for next iteration */
        if (*p) p[-1] = '/';
    }

    return 0;
}

int vfs_unlink(const char* path) {
    if (!path) return -EINVAL;
    char name[128];
    fs_node_t* parent = vfs_lookup_parent(path, name, sizeof(name));
    if (!parent) return -ENOENT;
    if (parent->flags != FS_DIRECTORY) return -ENOTDIR;
    if (parent->i_ops && parent->i_ops->unlink)
        return parent->i_ops->unlink(parent, name);
    return -ENOSYS;
}

int vfs_rmdir(const char* path) {
    if (!path) return -EINVAL;
    char name[128];
    fs_node_t* parent = vfs_lookup_parent(path, name, sizeof(name));
    if (!parent) return -ENOENT;
    if (parent->flags != FS_DIRECTORY) return -ENOTDIR;
    if (parent->i_ops && parent->i_ops->rmdir)
        return parent->i_ops->rmdir(parent, name);
    return -ENOSYS;
}

int vfs_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return -EINVAL;
    char old_name[128], new_name[128];
    fs_node_t* old_parent = vfs_lookup_parent(old_path, old_name, sizeof(old_name));
    fs_node_t* new_parent = vfs_lookup_parent(new_path, new_name, sizeof(new_name));
    if (!old_parent || !new_parent) return -ENOENT;
    if (old_parent->i_ops && old_parent->i_ops->rename)
        return old_parent->i_ops->rename(old_parent, old_name, new_parent, new_name);
    return -ENOSYS;
}

int vfs_truncate(const char* path, uint32_t length) {
    if (!path) return -EINVAL;
    int rc = vfs_require_writable_path(path);
    if (rc < 0) return rc;
    fs_node_t* node = vfs_lookup(path);
    if (!node) return -ENOENT;
    if (node->flags != FS_FILE) return -EISDIR;
    if (node->i_ops && node->i_ops->truncate)
        return node->i_ops->truncate(node, length);
    return -ENOSYS;
}

int vfs_link(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return -EINVAL;
    fs_node_t* target = vfs_lookup(old_path);
    if (!target) return -ENOENT;
    if (target->flags != FS_FILE) return -EPERM;

    char name[128];
    fs_node_t* parent = vfs_lookup_parent(new_path, name, sizeof(name));
    if (!parent) return -ENOENT;
    if (parent->flags != FS_DIRECTORY) return -ENOTDIR;
    if (parent->i_ops && parent->i_ops->link)
        return parent->i_ops->link(parent, name, target);
    return -ENOSYS;
}

/* Look up the mount flags for the filesystem that contains the given path.
 * Returns 0 if no mount matches (default: no restrictions). */
unsigned long vfs_mount_flags(const char* path) {
    if (!path) return 0;

    uintptr_t fl = spin_lock_irqsave(&g_vfs_lock);

    size_t best_len = 0;
    unsigned long best_flags = 0;

    for (int i = 0; i < g_mount_count; i++) {
        const char* mp = g_mounts[i].mountpoint;
        if (!mp[0]) continue;
        if (path_is_mountpoint_prefix(mp, path)) {
            size_t mpl = strlen(mp);
            if (mpl >= best_len) {
                best_len = mpl;
                best_flags = g_mounts[i].flags;
            }
        }
    }

    spin_unlock_irqrestore(&g_vfs_lock, fl);
    return best_flags;
}

/* Look up mount flags by matching the mount's root node pointer.
 * Useful when you have a fs_node_t* but not a path string. */
unsigned long vfs_node_mount_flags(const fs_node_t* root) {
    if (!root) return 0;

    uintptr_t fl = spin_lock_irqsave(&g_vfs_lock);

    unsigned long flags = 0;
    for (int i = 0; i < g_mount_count; i++) {
        if (g_mounts[i].root == root) {
            flags = g_mounts[i].flags;
            break;
        }
    }

    spin_unlock_irqrestore(&g_vfs_lock, fl);
    return flags;
}

/* Find the mount root fs_node for the given path.
 * Returns NULL if the path is not on any mount. */
fs_node_t* vfs_find_mount_root(const char* path) {
    if (!path) return NULL;

    uintptr_t fl = spin_lock_irqsave(&g_vfs_lock);

    size_t best_len = 0;
    fs_node_t* best_root = NULL;

    for (int i = 0; i < g_mount_count; i++) {
        const char* mp = g_mounts[i].mountpoint;
        if (!mp[0] || !g_mounts[i].root) continue;
        if (path_is_mountpoint_prefix(mp, path)) {
            size_t mpl = strlen(mp);
            if (mpl >= best_len) {
                best_len = mpl;
                best_root = g_mounts[i].root;
            }
        }
    }

    spin_unlock_irqrestore(&g_vfs_lock, fl);
    return best_root;
}

/* Increment the refcount on the mount that owns the given root node. */
void vfs_mount_ref(fs_node_t* mount_root) {
    if (!mount_root) return;

    uintptr_t fl = spin_lock_irqsave(&g_vfs_lock);
    for (int i = 0; i < g_mount_count; i++) {
        if (g_mounts[i].root == mount_root) {
            g_mounts[i].refcount++;
            break;
        }
    }
    spin_unlock_irqrestore(&g_vfs_lock, fl);
}

/* Decrement the refcount on the mount that owns the given root node. */
void vfs_mount_unref(fs_node_t* mount_root) {
    if (!mount_root) return;

    uintptr_t fl = spin_lock_irqsave(&g_vfs_lock);
    for (int i = 0; i < g_mount_count; i++) {
        if (g_mounts[i].root == mount_root) {
            if (g_mounts[i].refcount > 0)
                g_mounts[i].refcount--;
            break;
        }
    }
    spin_unlock_irqrestore(&g_vfs_lock, fl);
}

/* Check if the filesystem containing the given path is writable.
 * Returns 0 if writable, -EROFS if MS_RDONLY is set. */
int vfs_require_writable_path(const char* path) {
    if (!path) return -EINVAL;
    unsigned long mflags = vfs_mount_flags(path);
    if (mflags & MS_RDONLY) return -EROFS;
    return 0;
}
