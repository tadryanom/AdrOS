#include "tmpfs.h"

#include "heap.h"
#include "utils.h"

struct tmpfs_node {
    fs_node_t vfs;
    struct tmpfs_node* parent;
    struct tmpfs_node* first_child;
    struct tmpfs_node* next_sibling;

    uint8_t* data;
    uint32_t cap;
};

static uint32_t g_tmpfs_next_inode = 1;

static struct tmpfs_node* tmpfs_node_alloc(const char* name, uint32_t flags) {
    struct tmpfs_node* n = (struct tmpfs_node*)kmalloc(sizeof(*n));
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));

    if (name) {
        strcpy(n->vfs.name, name);
    } else {
        n->vfs.name[0] = 0;
    }

    n->vfs.flags = flags;
    n->vfs.inode = g_tmpfs_next_inode++;
    n->vfs.length = 0;

    return n;
}

static struct tmpfs_node* tmpfs_child_find(struct tmpfs_node* dir, const char* name) {
    if (!dir || !name) return NULL;
    struct tmpfs_node* c = dir->first_child;
    while (c) {
        if (strcmp(c->vfs.name, name) == 0) return c;
        c = c->next_sibling;
    }
    return NULL;
}

static void tmpfs_child_add(struct tmpfs_node* dir, struct tmpfs_node* child) {
    child->parent = dir;
    child->next_sibling = dir->first_child;
    dir->first_child = child;
}

static uint32_t tmpfs_read_impl(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) return 0;
    if (node->flags != FS_FILE) return 0;

    struct tmpfs_node* tn = (struct tmpfs_node*)node;
    if (offset > tn->vfs.length) return 0;
    if (offset + size > tn->vfs.length) size = tn->vfs.length - offset;

    if (size == 0) return 0;
    memcpy(buffer, tn->data + offset, size);
    return size;
}

static uint32_t tmpfs_write_impl(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) return 0;
    if (node->flags != FS_FILE) return 0;

    struct tmpfs_node* tn = (struct tmpfs_node*)node;
    uint64_t end = (uint64_t)offset + (uint64_t)size;
    if (end > 0xFFFFFFFFULL) return 0;

    if ((uint32_t)end > tn->cap) {
        uint32_t new_cap = tn->cap ? tn->cap : 64;
        while (new_cap < (uint32_t)end) {
            new_cap *= 2;
        }

        uint8_t* new_data = (uint8_t*)kmalloc(new_cap);
        if (!new_data) return 0;
        memset(new_data, 0, new_cap);
        if (tn->data && tn->vfs.length) {
            memcpy(new_data, tn->data, tn->vfs.length);
        }
        if (tn->data) kfree(tn->data);
        tn->data = new_data;
        tn->cap = new_cap;
    }

    memcpy(tn->data + offset, buffer, size);
    if ((uint32_t)end > tn->vfs.length) tn->vfs.length = (uint32_t)end;
    return size;
}

static struct fs_node* tmpfs_finddir_impl(struct fs_node* node, char* name) {
    if (!node || !name) return 0;
    if (node->flags != FS_DIRECTORY) return 0;

    struct tmpfs_node* dir = (struct tmpfs_node*)node;
    struct tmpfs_node* child = tmpfs_child_find(dir, name);
    if (!child) return 0;
    return &child->vfs;
}

fs_node_t* tmpfs_create_root(void) {
    struct tmpfs_node* root = tmpfs_node_alloc("", FS_DIRECTORY);
    if (!root) return NULL;

    root->vfs.read = 0;
    root->vfs.write = 0;
    root->vfs.open = 0;
    root->vfs.close = 0;
    root->vfs.finddir = &tmpfs_finddir_impl;

    return &root->vfs;
}

int tmpfs_add_file(fs_node_t* root_dir, const char* name, const uint8_t* data, uint32_t len) {
    if (!root_dir || root_dir->flags != FS_DIRECTORY) return -1;
    if (!name || name[0] == 0) return -1;

    struct tmpfs_node* dir = (struct tmpfs_node*)root_dir;
    if (tmpfs_child_find(dir, name)) return -1;

    struct tmpfs_node* f = tmpfs_node_alloc(name, FS_FILE);
    if (!f) return -1;

    f->vfs.read = &tmpfs_read_impl;
    f->vfs.write = &tmpfs_write_impl;
    f->vfs.open = 0;
    f->vfs.close = 0;
    f->vfs.finddir = 0;

    if (len) {
        f->data = (uint8_t*)kmalloc(len);
        if (!f->data) {
            kfree(f);
            return -1;
        }
        memcpy(f->data, data, len);
        f->cap = len;
        f->vfs.length = len;
    }

    tmpfs_child_add(dir, f);
    return 0;
}
