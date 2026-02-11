#include "fs.h"

#include "utils.h"
#include "errno.h"

fs_node_t* fs_root = NULL;

struct vfs_mount {
    char mountpoint[128];
    fs_node_t* root;
};

static struct vfs_mount g_mounts[8];
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
    if (node->read)
        return node->read(node, offset, size, buffer);
    return 0;
}

uint32_t vfs_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (node->write)
        return node->write(node, offset, size, buffer);
    return 0;
}

void vfs_open(fs_node_t* node) {
    if (node->open)
        node->open(node);
}

void vfs_close(fs_node_t* node) {
    if (node->close)
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

        if (!cur || !cur->finddir) return NULL;
        cur = cur->finddir(cur, part);
        if (!cur) return NULL;

        if (cur->flags == FS_SYMLINK && cur->symlink_target[0]) {
            cur = vfs_lookup_depth(cur->symlink_target, depth + 1);
            if (!cur) return NULL;
        }
    }

    return cur;
}
