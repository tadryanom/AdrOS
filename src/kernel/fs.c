#include "fs.h"

#include "utils.h"

fs_node_t* fs_root = NULL;

uint32_t vfs_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node->read)
        return node->read(node, offset, size, buffer);
    return 0;
}

uint32_t vfs_write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
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

fs_node_t* vfs_lookup(const char* path) {
    if (!path || !fs_root) return NULL;

    if (strcmp(path, "/") == 0) return fs_root;

    const char* p = path;
    while (*p == '/') p++;
    if (*p == 0) return fs_root;

    fs_node_t* cur = fs_root;

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
    }

    return cur;
}
