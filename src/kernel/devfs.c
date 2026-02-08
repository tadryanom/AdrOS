#include "devfs.h"

#include "errno.h"
#include "tty.h"
#include "utils.h"

struct devfs_root {
    fs_node_t vfs;
};

static struct devfs_root g_dev_root;
static fs_node_t g_dev_null;
static fs_node_t g_dev_tty;
static uint32_t g_devfs_inited = 0;

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

static uint32_t dev_tty_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    int rc = tty_read_kbuf(buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static uint32_t dev_tty_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    (void)offset;
    int rc = tty_write_kbuf((const uint8_t*)buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static struct fs_node* devfs_finddir_impl(struct fs_node* node, const char* name) {
    (void)node;
    if (!name || name[0] == 0) return 0;

    if (strcmp(name, "null") == 0) return &g_dev_null;
    if (strcmp(name, "tty") == 0) return &g_dev_tty;
    return 0;
}

static void devfs_init_once(void) {
    if (g_devfs_inited) return;
    g_devfs_inited = 1;

    memset(&g_dev_root, 0, sizeof(g_dev_root));
    strcpy(g_dev_root.vfs.name, "dev");
    g_dev_root.vfs.flags = FS_DIRECTORY;
    g_dev_root.vfs.inode = 1;
    g_dev_root.vfs.length = 0;
    g_dev_root.vfs.read = 0;
    g_dev_root.vfs.write = 0;
    g_dev_root.vfs.open = 0;
    g_dev_root.vfs.close = 0;
    g_dev_root.vfs.finddir = &devfs_finddir_impl;

    memset(&g_dev_null, 0, sizeof(g_dev_null));
    strcpy(g_dev_null.name, "null");
    g_dev_null.flags = FS_CHARDEVICE;
    g_dev_null.inode = 2;
    g_dev_null.length = 0;
    g_dev_null.read = &dev_null_read;
    g_dev_null.write = &dev_null_write;
    g_dev_null.open = 0;
    g_dev_null.close = 0;
    g_dev_null.finddir = 0;

    memset(&g_dev_tty, 0, sizeof(g_dev_tty));
    strcpy(g_dev_tty.name, "tty");
    g_dev_tty.flags = FS_CHARDEVICE;
    g_dev_tty.inode = 3;
    g_dev_tty.length = 0;
    g_dev_tty.read = &dev_tty_read;
    g_dev_tty.write = &dev_tty_write;
    g_dev_tty.open = 0;
    g_dev_tty.close = 0;
    g_dev_tty.finddir = 0;
}

fs_node_t* devfs_create_root(void) {
    devfs_init_once();
    return &g_dev_root.vfs;
}
