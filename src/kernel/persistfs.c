#include "persistfs.h"

#include "ata_pio.h"
#include "diskfs.h"
#include "errno.h"
#include "heap.h"
#include "utils.h"

// Persistent storage wrapper over diskfs:
// - Exposes /persist/counter with legacy 512-byte semantics.
// - Backed by a diskfs file named "persist.counter".
// - Migrates the legacy LBA1 counter value into diskfs once.

#define PERSISTFS_LBA_COUNTER 1U
#define PERSISTFS_BACKING_NAME "persist.counter"

enum {
    PERSIST_O_CREAT = 0x40,
    PERSIST_O_TRUNC = 0x200,
};

static fs_node_t g_root;
static fs_node_t g_counter;
static uint32_t g_ready = 0;

static fs_node_t* persistfs_backing_open(uint32_t flags) {
    fs_node_t* n = 0;
    if (diskfs_open_file(PERSISTFS_BACKING_NAME, flags, &n) < 0) return 0;
    return n;
}

static void persistfs_backing_close(fs_node_t* n) {
    if (!n) return;
    vfs_close(n);
}

static uint32_t persist_counter_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    if (!buffer) return 0;
    if (!g_ready) return 0;

    if (offset >= 512U) return 0;
    if (offset + size > 512U) size = 512U - offset;

    fs_node_t* b = persistfs_backing_open(PERSIST_O_CREAT);
    if (!b) return 0;
    uint32_t rd = vfs_read(b, offset, size, buffer);
    persistfs_backing_close(b);
    return rd;
}

static uint32_t persist_counter_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    if (!buffer) return 0;
    if (!g_ready) return 0;

    if (offset >= 512U) return 0;
    if (offset + size > 512U) size = 512U - offset;

    fs_node_t* b = persistfs_backing_open(PERSIST_O_CREAT);
    if (!b) return 0;
    uint32_t wr = vfs_write(b, offset, size, buffer);
    persistfs_backing_close(b);
    return wr;
}

static struct fs_node* persist_root_finddir(struct fs_node* node, const char* name) {
    (void)node;
    if (!name || name[0] == 0) return 0;
    if (strcmp(name, "counter") == 0) return &g_counter;
    return 0;
}

fs_node_t* persistfs_create_root(void) {
    if (!g_ready) {
        if (ata_pio_init_primary_master() == 0) {
            g_ready = 1;
        } else {
            g_ready = 0;
        }

        if (g_ready) {
            // Ensure diskfs is initialized even if /disk mount happens later.
            (void)diskfs_create_root();

            // One-time migration from legacy LBA1 counter storage.
            uint8_t sec[512];
            if (ata_pio_read28(PERSISTFS_LBA_COUNTER, sec) == 0) {
                fs_node_t* b = persistfs_backing_open(PERSIST_O_CREAT);
                if (b) {
                    uint8_t cur4[4];
                    uint32_t rd = vfs_read(b, 0, 4, cur4);
                    if (rd == 0) {
                        (void)vfs_write(b, 0, 4, sec);
                    }
                    persistfs_backing_close(b);
                }
            }
        }

        memset(&g_root, 0, sizeof(g_root));
        strcpy(g_root.name, "persist");
        g_root.flags = FS_DIRECTORY;
        g_root.inode = 1;
        g_root.length = 0;
        g_root.read = 0;
        g_root.write = 0;
        g_root.open = 0;
        g_root.close = 0;
        g_root.finddir = &persist_root_finddir;

        memset(&g_counter, 0, sizeof(g_counter));
        strcpy(g_counter.name, "counter");
        g_counter.flags = FS_FILE;
        g_counter.inode = 2;
        g_counter.length = 512;
        g_counter.read = &persist_counter_read;
        g_counter.write = &persist_counter_write;
        g_counter.open = 0;
        g_counter.close = 0;
        g_counter.finddir = 0;
    }

    return g_ready ? &g_root : 0;
}
