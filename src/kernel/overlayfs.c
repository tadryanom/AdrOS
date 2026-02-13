#include "overlayfs.h"

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
    .finddir = overlay_finddir_impl,
    .readdir = overlay_readdir_impl,
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

static int overlay_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    if (!node || !inout_index || !buf) return -1;
    if (node->flags != FS_DIRECTORY) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    struct overlay_node* dir = (struct overlay_node*)node;

    // Prefer upper layer readdir; fall back to lower.
    fs_node_t* src = dir->upper ? dir->upper : dir->lower;
    if (!src) return 0;
    if (src->f_ops && src->f_ops->readdir)
        return src->f_ops->readdir(src, inout_index, buf, buf_len);
    return 0;
}

static struct fs_node* overlay_finddir_impl(struct fs_node* node, const char* name) {
    if (!node || !name) return 0;
    if (node->flags != FS_DIRECTORY) return 0;

    struct overlay_node* dir = (struct overlay_node*)node;

    fs_node_t* upper_child = NULL;
    fs_node_t* lower_child = NULL;

    if (dir->upper && dir->upper->f_ops && dir->upper->f_ops->finddir)
        upper_child = dir->upper->f_ops->finddir(dir->upper, name);
    if (dir->lower && dir->lower->f_ops && dir->lower->f_ops->finddir)
        lower_child = dir->lower->f_ops->finddir(dir->lower, name);

    if (!upper_child && !lower_child) return 0;
    return overlay_wrap_child(dir, name, lower_child, upper_child);
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

    root->path[0] = 0;

    return &root->vfs;
}
