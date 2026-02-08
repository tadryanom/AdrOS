#include "persistfs.h"

#include "ata_pio.h"
#include "errno.h"
#include "heap.h"
#include "utils.h"

// Minimal on-disk persistent storage:
// - LBA0 reserved
// - LBA1 holds one 512-byte file called "counter" (first 4 bytes are the counter value)

#define PERSISTFS_LBA_COUNTER 1U

static fs_node_t g_root;
static fs_node_t g_counter;
static uint32_t g_ready = 0;

static uint32_t persist_counter_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    if (!buffer) return 0;
    if (!g_ready) return 0;

    uint8_t sec[512];
    if (ata_pio_read28(PERSISTFS_LBA_COUNTER, sec) < 0) return 0;

    if (offset >= 512U) return 0;
    if (offset + size > 512U) size = 512U - offset;

    memcpy(buffer, sec + offset, size);
    return size;
}

static uint32_t persist_counter_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    if (!buffer) return 0;
    if (!g_ready) return 0;

    if (offset >= 512U) return 0;
    if (offset + size > 512U) size = 512U - offset;

    uint8_t sec[512];
    if (ata_pio_read28(PERSISTFS_LBA_COUNTER, sec) < 0) return 0;

    memcpy(sec + offset, buffer, size);

    if (ata_pio_write28(PERSISTFS_LBA_COUNTER, sec) < 0) return 0;
    return size;
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
