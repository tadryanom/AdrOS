#include "fs.h"

#include "utils.h"
#include "errno.h"

fs_node_t* fs_root = NULL;

struct vfs_mount {
    char mountpoint[128];
    fs_node_t* root;
};

static struct vfs_mount g_mounts[16];
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

int vfs_mount(const char* mountpoint, fs_node_t* root) {
    if (!root) return -EINVAL;
    if (g_mount_count >= (int)(sizeof(g_mounts) / sizeof(g_mounts[0]))) return -ENOSPC;

    char mp[128];
    normalize_mountpoint(mountpoint, mp, sizeof(mp));

    for (int i = 0; i < g_mount_count; i++) {
        if (strcmp(g_mounts[i].mountpoint, mp) == 0) {
            g_mounts[i].root = root;
            return 0;
        }
    }

    strcpy(g_mounts[g_mount_count].mountpoint, mp);
    g_mounts[g_mount_count].root = root;
    g_mount_count++;
    return 0;
}

uint32_t vfs_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node->f_ops && node->f_ops->read)
        return node->f_ops->read(node, offset, size, buffer);
    if (node->read)
        return node->read(node, offset, size, buffer);
    return 0;
}

uint32_t vfs_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (node->f_ops && node->f_ops->write)
        return node->f_ops->write(node, offset, size, buffer);
    if (node->write)
        return node->write(node, offset, size, buffer);
    return 0;
}

void vfs_open(fs_node_t* node) {
    if (node->f_ops && node->f_ops->open)
        node->f_ops->open(node);
    else if (node->open)
        node->open(node);
}

void vfs_close(fs_node_t* node) {
    if (node->f_ops && node->f_ops->close)
        node->f_ops->close(node);
    else if (node->close)
        node->close(node);
}

static fs_node_t* vfs_lookup_depth(const char* path, int depth);

fs_node_t* vfs_lookup(const char* path) {
    return vfs_lookup_depth(path, 0);
}

static fs_node_t* vfs_lookup_depth(const char* path, int depth) {
    if (!path || !fs_root) return NULL;
    if (depth > 8) return NULL;

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
        if (cur->f_ops && cur->f_ops->finddir) fn_finddir = cur->f_ops->finddir;
        else if (cur->finddir) fn_finddir = cur->finddir;
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
    if (parent->f_ops && parent->f_ops->create)
        return parent->f_ops->create(parent, name, flags, out);
    if (!parent->create) return -ENOSYS;
    return parent->create(parent, name, flags, out);
}

int vfs_mkdir(const char* path) {
    if (!path) return -EINVAL;
    char name[128];
    fs_node_t* parent = vfs_lookup_parent(path, name, sizeof(name));
    if (!parent) return -ENOENT;
    if (parent->flags != FS_DIRECTORY) return -ENOTDIR;
    if (parent->f_ops && parent->f_ops->mkdir)
        return parent->f_ops->mkdir(parent, name);
    if (!parent->mkdir) return -ENOSYS;
    return parent->mkdir(parent, name);
}

int vfs_unlink(const char* path) {
    if (!path) return -EINVAL;
    char name[128];
    fs_node_t* parent = vfs_lookup_parent(path, name, sizeof(name));
    if (!parent) return -ENOENT;
    if (parent->flags != FS_DIRECTORY) return -ENOTDIR;
    if (parent->f_ops && parent->f_ops->unlink)
        return parent->f_ops->unlink(parent, name);
    if (!parent->unlink) return -ENOSYS;
    return parent->unlink(parent, name);
}

int vfs_rmdir(const char* path) {
    if (!path) return -EINVAL;
    char name[128];
    fs_node_t* parent = vfs_lookup_parent(path, name, sizeof(name));
    if (!parent) return -ENOENT;
    if (parent->flags != FS_DIRECTORY) return -ENOTDIR;
    if (parent->f_ops && parent->f_ops->rmdir)
        return parent->f_ops->rmdir(parent, name);
    if (!parent->rmdir) return -ENOSYS;
    return parent->rmdir(parent, name);
}

int vfs_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return -EINVAL;
    char old_name[128], new_name[128];
    fs_node_t* old_parent = vfs_lookup_parent(old_path, old_name, sizeof(old_name));
    fs_node_t* new_parent = vfs_lookup_parent(new_path, new_name, sizeof(new_name));
    if (!old_parent || !new_parent) return -ENOENT;
    if (old_parent->f_ops && old_parent->f_ops->rename)
        return old_parent->f_ops->rename(old_parent, old_name, new_parent, new_name);
    if (!old_parent->rename) return -ENOSYS;
    return old_parent->rename(old_parent, old_name, new_parent, new_name);
}

int vfs_truncate(const char* path, uint32_t length) {
    if (!path) return -EINVAL;
    fs_node_t* node = vfs_lookup(path);
    if (!node) return -ENOENT;
    if (node->flags != FS_FILE) return -EISDIR;
    if (node->f_ops && node->f_ops->truncate)
        return node->f_ops->truncate(node, length);
    if (!node->truncate) return -ENOSYS;
    return node->truncate(node, length);
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
    if (parent->f_ops && parent->f_ops->link)
        return parent->f_ops->link(parent, name, target);
    if (!parent->link) return -ENOSYS;
    return parent->link(parent, name, target);
}
