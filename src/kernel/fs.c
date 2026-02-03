#include "fs.h"

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
