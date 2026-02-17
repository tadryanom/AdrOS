#include "overlayfs.h"

#include "errno.h"
#include "heap.h"
#include "utils.h"
#include "tmpfs.h"

#include <stdint.h>

struct overlayfs {
    fs_node_t* lower;
    fs_node_t* upper;
};

struct overlay_node {
    fs_node_t vfs;
    struct overlayfs* ofs;

    fs_node_t* lower;
    fs_node_t* upper;

    char path[256];
};

static struct fs_node* overlay_finddir_impl(struct fs_node* node, const char* name);
static int overlay_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len);
static int overlay_mkdir_impl(struct fs_node* dir, const char* name);
static int overlay_unlink_impl(struct fs_node* dir, const char* name);
static int overlay_rmdir_impl(struct fs_node* dir, const char* name);
static int overlay_create_impl(struct fs_node* dir, const char* name, uint32_t flags, struct fs_node** out);

static void overlay_str_copy_n(char* dst, size_t dst_sz, const char* src, size_t src_n) {
    if (!dst || dst_sz == 0) return;
    size_t i = 0;
    for (; i + 1 < dst_sz && i < src_n; i++) {
        char c = src[i];
        if (c == 0) break;
        dst[i] = c;
    }
    dst[i] = 0;
}

static uint32_t overlay_read_impl(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node) return 0;
    struct overlay_node* on = (struct overlay_node*)node;

    fs_node_t* src = on->upper ? on->upper : on->lower;
    if (!src) return 0;
    return vfs_read(src, offset, size, buffer);
}

static fs_node_t* overlay_copy_up_file(struct overlay_node* on) {
    if (!on || on->upper) return on ? on->upper : NULL;
    if (!on->ofs || !on->ofs->upper) return NULL;
    if (!on->lower) return NULL;
    if (on->vfs.flags != FS_FILE) return NULL;

    uint32_t len = on->lower->length;
    uint8_t* buf = NULL;
    if (len) {
        buf = (uint8_t*)kmalloc(len);
        if (!buf) return NULL;
        uint32_t rd = vfs_read(on->lower, 0, len, buf);
        if (rd != len) {
            kfree(buf);
            return NULL;
        }
    }

    fs_node_t* created = tmpfs_create_file(on->ofs->upper, on->path, buf, len);
    if (buf) kfree(buf);
    if (!created) return NULL;

    on->upper = created;
    on->vfs.length = created->length;
    on->vfs.inode = created->inode;
    return created;
}

static uint32_t overlay_write_impl(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node) return 0;
    struct overlay_node* on = (struct overlay_node*)node;

    fs_node_t* dst = on->upper;
    if (!dst) {
        dst = overlay_copy_up_file(on);
    }
    if (!dst) return 0;

    uint32_t wr = vfs_write(dst, offset, size, buffer);
    if (dst->length > on->vfs.length) on->vfs.length = dst->length;
    return wr;
}

static const struct file_operations overlay_file_ops = {
    .read  = overlay_read_impl,
    .write = overlay_write_impl,
};

static const struct file_operations overlay_dir_ops = {
    .read    = overlay_read_impl,
};

static const struct inode_operations overlay_dir_iops = {
    .lookup  = overlay_finddir_impl,
    .readdir = overlay_readdir_impl,
    .mkdir   = overlay_mkdir_impl,
    .unlink  = overlay_unlink_impl,
    .rmdir   = overlay_rmdir_impl,
    .create  = overlay_create_impl,
};

static fs_node_t* overlay_wrap_child(struct overlay_node* parent, const char* name, fs_node_t* lower_child, fs_node_t* upper_child) {
    if (!parent || !parent->ofs || !name) return NULL;
    if (!lower_child && !upper_child) return NULL;

    struct overlay_node* c = (struct overlay_node*)kmalloc(sizeof(*c));
    if (!c) return NULL;
    memset(c, 0, sizeof(*c));

    strcpy(c->vfs.name, name);
    c->ofs = parent->ofs;
    c->lower = lower_child;
    c->upper = upper_child;

    if (upper_child) {
        c->vfs.flags = upper_child->flags;
        c->vfs.inode = upper_child->inode;
        c->vfs.length = upper_child->length;
    } else {
        c->vfs.flags = lower_child->flags;
        c->vfs.inode = lower_child->inode;
        c->vfs.length = lower_child->length;
    }

    if (c->vfs.flags == FS_DIRECTORY) {
        c->vfs.f_ops = &overlay_dir_ops;
        c->vfs.i_ops = &overlay_dir_iops;
    } else {
        c->vfs.f_ops = &overlay_file_ops;
    }

    if (parent->path[0] == 0) {
        c->path[0] = '/';
        c->path[1] = 0;
    } else {
        strcpy(c->path, parent->path);
    }

    size_t l = strlen(c->path);
    if (l == 0) {
        c->path[0] = '/';
        c->path[1] = 0;
        l = 1;
    }
    if (l + 1 < sizeof(c->path) && c->path[l - 1] != '/') {
        c->path[l++] = '/';
        c->path[l] = 0;
    }

    size_t cur = strlen(c->path);
    if (cur + 1 < sizeof(c->path)) {
        size_t rem = sizeof(c->path) - cur - 1;
        overlay_str_copy_n(c->path + cur, rem + 1, name, strlen(name));
    }

    return &c->vfs;
}

static int overlay_count_upper(struct overlay_node* dir) {
    if (!dir->upper || !dir->upper->i_ops || !dir->upper->i_ops->readdir) return 0;
    int count = 0;
    uint32_t idx = 0;
    struct vfs_dirent tmp;
    while (1) {
        int rc = dir->upper->i_ops->readdir(dir->upper, &idx, &tmp, sizeof(tmp));
        if (rc <= 0) break;
        count += rc / (int)sizeof(struct vfs_dirent);
    }
    return count;
}

static int overlay_upper_has_name(struct overlay_node* dir, const char* name) {
    if (!dir->upper || !dir->upper->i_ops || !dir->upper->i_ops->lookup) return 0;
    fs_node_t* found = dir->upper->i_ops->lookup(dir->upper, name);
    return found ? 1 : 0;
}

static int overlay_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    if (!node || !inout_index || !buf) return -1;
    if (node->flags != FS_DIRECTORY) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    struct overlay_node* dir = (struct overlay_node*)node;
    uint32_t idx = *inout_index;
    uint32_t cap = buf_len / (uint32_t)sizeof(struct vfs_dirent);
    struct vfs_dirent* ents = (struct vfs_dirent*)buf;
    uint32_t written = 0;

    /* Phase 1: emit upper layer entries */
    if (dir->upper && dir->upper->i_ops && dir->upper->i_ops->readdir) {
        uint32_t upper_idx = idx;
        int rc = dir->upper->i_ops->readdir(dir->upper, &upper_idx, ents, buf_len);
        if (rc > 0) {
            written = (uint32_t)rc / (uint32_t)sizeof(struct vfs_dirent);
            *inout_index = upper_idx;
            return rc;
        }
        /* Upper exhausted — switch to lower layer.
         * Count total upper entries to know offset for lower phase. */
    }

    /* Phase 2: emit lower layer entries, skipping those already in upper */
    if (!dir->lower || !dir->lower->i_ops || !dir->lower->i_ops->readdir) {
        *inout_index = idx;
        return 0;
    }

    int upper_total = overlay_count_upper(dir);
    /* Lower-phase index: subtract upper_total from our global index */
    uint32_t lower_idx = (idx >= (uint32_t)upper_total) ? idx - (uint32_t)upper_total : 0;

    while (written < cap) {
        struct vfs_dirent tmp;
        uint32_t tmp_idx = lower_idx;
        int rc = dir->lower->i_ops->readdir(dir->lower, &tmp_idx, &tmp, sizeof(tmp));
        if (rc <= 0) break;
        lower_idx = tmp_idx;
        /* Skip . and .. (already emitted by upper) and entries that exist in upper */
        if (strcmp(tmp.d_name, ".") == 0 || strcmp(tmp.d_name, "..") == 0) continue;
        if (overlay_upper_has_name(dir, tmp.d_name)) continue;
        ents[written++] = tmp;
    }

    *inout_index = (uint32_t)upper_total + lower_idx;
    return (int)(written * (uint32_t)sizeof(struct vfs_dirent));
}

static struct fs_node* overlay_finddir_impl(struct fs_node* node, const char* name) {
    if (!node || !name) return 0;
    if (node->flags != FS_DIRECTORY) return 0;

    struct overlay_node* dir = (struct overlay_node*)node;

    fs_node_t* upper_child = NULL;
    fs_node_t* lower_child = NULL;

    if (dir->upper && dir->upper->i_ops && dir->upper->i_ops->lookup)
        upper_child = dir->upper->i_ops->lookup(dir->upper, name);
    if (dir->lower && dir->lower->i_ops && dir->lower->i_ops->lookup)
        lower_child = dir->lower->i_ops->lookup(dir->lower, name);

    if (!upper_child && !lower_child) return 0;
    return overlay_wrap_child(dir, name, lower_child, upper_child);
}

static int overlay_mkdir_impl(struct fs_node* dir, const char* name) {
    if (!dir || !name) return -EINVAL;
    struct overlay_node* on = (struct overlay_node*)dir;
    if (!on->upper) return -EROFS;
    if (on->upper->i_ops && on->upper->i_ops->mkdir)
        return on->upper->i_ops->mkdir(on->upper, name);
    return -ENOSYS;
}

static int overlay_unlink_impl(struct fs_node* dir, const char* name) {
    if (!dir || !name) return -EINVAL;
    struct overlay_node* on = (struct overlay_node*)dir;
    /* Try upper layer first */
    if (on->upper && on->upper->i_ops && on->upper->i_ops->unlink) {
        int rc = on->upper->i_ops->unlink(on->upper, name);
        if (rc == 0 || rc != -ENOENT) return rc;
    }
    /* File only in lower (read-only) layer — cannot delete */
    return -EROFS;
}

static int overlay_rmdir_impl(struct fs_node* dir, const char* name) {
    if (!dir || !name) return -EINVAL;
    struct overlay_node* on = (struct overlay_node*)dir;
    if (on->upper && on->upper->i_ops && on->upper->i_ops->rmdir) {
        int rc = on->upper->i_ops->rmdir(on->upper, name);
        if (rc == 0 || rc != -ENOENT) return rc;
    }
    return -EROFS;
}

static int overlay_create_impl(struct fs_node* dir, const char* name, uint32_t flags, struct fs_node** out) {
    if (!dir || !name || !out) return -EINVAL;
    struct overlay_node* on = (struct overlay_node*)dir;
    if (!on->upper) return -EROFS;
    if (on->upper->i_ops && on->upper->i_ops->create)
        return on->upper->i_ops->create(on->upper, name, flags, out);
    return -ENOSYS;
}

fs_node_t* overlayfs_create_root(fs_node_t* lower_root, fs_node_t* upper_root) {
    if (!lower_root || !upper_root) return NULL;

    struct overlayfs* ofs = (struct overlayfs*)kmalloc(sizeof(*ofs));
    if (!ofs) return NULL;
    ofs->lower = lower_root;
    ofs->upper = upper_root;

    struct overlay_node* root = (struct overlay_node*)kmalloc(sizeof(*root));
    if (!root) {
        kfree(ofs);
        return NULL;
    }
    memset(root, 0, sizeof(*root));

    root->ofs = ofs;
    root->lower = lower_root;
    root->upper = upper_root;

    root->vfs.name[0] = 0;
    root->vfs.flags = FS_DIRECTORY;
    root->vfs.inode = upper_root->inode;
    root->vfs.length = 0;
    root->vfs.f_ops = &overlay_dir_ops;
    root->vfs.i_ops = &overlay_dir_iops;

    root->path[0] = 0;

    return &root->vfs;
}
