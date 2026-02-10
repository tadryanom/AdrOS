#include "diskfs.h"

#include "ata_pio.h"
#include "errno.h"
#include "heap.h"
#include "utils.h"

#include <stddef.h>
#include <stdint.h>

// Very small on-disk FS (flat namespace) stored starting at LBA2.
// - LBA0 reserved
// - LBA1 used by persistfs counter (legacy)
// - LBA2 superblock + directory entries
// - data blocks allocated linearly from LBA3 upward
//
// Not a full POSIX FS; goal is persistent files with read/write + create/trunc.

#define DISKFS_LBA_SUPER 2U
#define DISKFS_LBA_SUPER2 3U
#define DISKFS_LBA_DATA_START 4U

#define DISKFS_MAGIC 0x44465331U /* 'DFS1' */
#define DISKFS_VERSION 2U

#define DISKFS_MAX_FILES 12
#define DISKFS_NAME_MAX 32

#define DISKFS_SECTOR 512U

#define DISKFS_DEFAULT_CAP_SECTORS 8U /* 4KB */

struct diskfs_dirent {
    char name[DISKFS_NAME_MAX];
    uint32_t start_lba;
    uint32_t size_bytes;
    uint32_t cap_sectors;
};

struct diskfs_super {
    uint32_t magic;
    uint32_t version;
    uint32_t file_count;
    uint32_t next_free_lba;
    struct diskfs_dirent files[DISKFS_MAX_FILES];
};

struct diskfs_node {
    fs_node_t vfs;
    uint32_t idx;
};

static fs_node_t g_root;
static uint32_t g_ready = 0;

static void diskfs_close_impl(fs_node_t* node) {
    if (!node) return;
    struct diskfs_node* dn = (struct diskfs_node*)node;
    kfree(dn);
}

static int diskfs_super_load(struct diskfs_super* sb) {
    if (!sb) return -EINVAL;
    uint8_t sec0[DISKFS_SECTOR];
    uint8_t sec1[DISKFS_SECTOR];
    if (ata_pio_read28(DISKFS_LBA_SUPER, sec0) < 0) return -EIO;
    if (ata_pio_read28(DISKFS_LBA_SUPER2, sec1) < 0) return -EIO;

    if (sizeof(*sb) > (size_t)(DISKFS_SECTOR * 2U)) return -EIO;
    memcpy(sb, sec0, DISKFS_SECTOR);
    if (sizeof(*sb) > DISKFS_SECTOR) {
        memcpy(((uint8_t*)sb) + DISKFS_SECTOR, sec1, sizeof(*sb) - DISKFS_SECTOR);
    }

    if (sb->magic != DISKFS_MAGIC || sb->version != DISKFS_VERSION) {
        memset(sb, 0, sizeof(*sb));
        sb->magic = DISKFS_MAGIC;
        sb->version = DISKFS_VERSION;
        sb->file_count = 0;
        sb->next_free_lba = DISKFS_LBA_DATA_START;

        memset(sec0, 0, sizeof(sec0));
        memset(sec1, 0, sizeof(sec1));
        memcpy(sec0, sb, DISKFS_SECTOR);
        if (sizeof(*sb) > DISKFS_SECTOR) {
            memcpy(sec1, ((const uint8_t*)sb) + DISKFS_SECTOR, sizeof(*sb) - DISKFS_SECTOR);
        }
        if (ata_pio_write28(DISKFS_LBA_SUPER, sec0) < 0) return -EIO;
        if (ata_pio_write28(DISKFS_LBA_SUPER2, sec1) < 0) return -EIO;
    }

    if (sb->file_count > DISKFS_MAX_FILES) sb->file_count = DISKFS_MAX_FILES;
    if (sb->next_free_lba < DISKFS_LBA_DATA_START) sb->next_free_lba = DISKFS_LBA_DATA_START;
    return 0;
}

static int diskfs_super_store(const struct diskfs_super* sb) {
    if (!sb) return -EINVAL;
    uint8_t sec0[DISKFS_SECTOR];
    uint8_t sec1[DISKFS_SECTOR];
    if (sizeof(*sb) > (size_t)(DISKFS_SECTOR * 2U)) return -EIO;

    memset(sec0, 0, sizeof(sec0));
    memset(sec1, 0, sizeof(sec1));
    memcpy(sec0, sb, DISKFS_SECTOR);
    if (sizeof(*sb) > DISKFS_SECTOR) {
        memcpy(sec1, ((const uint8_t*)sb) + DISKFS_SECTOR, sizeof(*sb) - DISKFS_SECTOR);
    }

    if (ata_pio_write28(DISKFS_LBA_SUPER, sec0) < 0) return -EIO;
    if (ata_pio_write28(DISKFS_LBA_SUPER2, sec1) < 0) return -EIO;
    return 0;
}

static int diskfs_name_valid(const char* name) {
    if (!name || name[0] == 0) return 0;
    for (uint32_t i = 0; name[i] != 0; i++) {
        if (name[i] == '/') return 0;
        if (i + 1 >= DISKFS_NAME_MAX) return 0;
    }
    return 1;
}

static int diskfs_find(const struct diskfs_super* sb, const char* name) {
    if (!sb || !name) return -1;
    for (uint32_t i = 0; i < sb->file_count && i < DISKFS_MAX_FILES; i++) {
        if (sb->files[i].name[0] != 0 && strcmp(sb->files[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int diskfs_alloc_file(struct diskfs_super* sb, const char* name, uint32_t cap_sectors, uint32_t* out_idx) {
    if (!sb || !name || !out_idx) return -EINVAL;
    if (sb->file_count >= DISKFS_MAX_FILES) return -ENOSPC;

    uint32_t idx = sb->file_count;
    sb->file_count++;

    memset(&sb->files[idx], 0, sizeof(sb->files[idx]));
    strcpy(sb->files[idx].name, name);
    sb->files[idx].start_lba = sb->next_free_lba;
    sb->files[idx].size_bytes = 0;
    sb->files[idx].cap_sectors = cap_sectors ? cap_sectors : DISKFS_DEFAULT_CAP_SECTORS;

    sb->next_free_lba += sb->files[idx].cap_sectors;

    // Zero-init allocated sectors.
    uint8_t zero[DISKFS_SECTOR];
    memset(zero, 0, sizeof(zero));
    for (uint32_t s = 0; s < sb->files[idx].cap_sectors; s++) {
        (void)ata_pio_write28(sb->files[idx].start_lba + s, zero);
    }

    *out_idx = idx;
    return 0;
}

static uint32_t diskfs_read_impl(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) return 0;
    if (node->flags != FS_FILE) return 0;
    if (!g_ready) return 0;

    struct diskfs_node* dn = (struct diskfs_node*)node;
    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return 0;
    if (dn->idx >= sb.file_count || dn->idx >= DISKFS_MAX_FILES) return 0;

    struct diskfs_dirent* de = &sb.files[dn->idx];
    if (offset >= de->size_bytes) return 0;
    if (offset + size > de->size_bytes) size = de->size_bytes - offset;
    if (size == 0) return 0;

    uint32_t total = 0;
    while (total < size) {
        uint32_t pos = offset + total;
        uint32_t lba_off = pos / DISKFS_SECTOR;
        uint32_t sec_off = pos % DISKFS_SECTOR;
        uint32_t chunk = size - total;
        if (chunk > (DISKFS_SECTOR - sec_off)) chunk = DISKFS_SECTOR - sec_off;
        if (lba_off >= de->cap_sectors) break;

        uint8_t sec[DISKFS_SECTOR];
        if (ata_pio_read28(de->start_lba + lba_off, sec) < 0) break;
        memcpy(buffer + total, sec + sec_off, chunk);
        total += chunk;
    }

    return total;
}

static uint32_t diskfs_write_impl(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !buffer) return 0;
    if (node->flags != FS_FILE) return 0;
    if (!g_ready) return 0;

    struct diskfs_node* dn = (struct diskfs_node*)node;
    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return 0;
    if (dn->idx >= sb.file_count || dn->idx >= DISKFS_MAX_FILES) return 0;

    struct diskfs_dirent* de = &sb.files[dn->idx];

    uint64_t end = (uint64_t)offset + (uint64_t)size;
    if (end > 0xFFFFFFFFULL) return 0;

    uint32_t need_bytes = (uint32_t)end;
    uint32_t need_sectors = (need_bytes + DISKFS_SECTOR - 1U) / DISKFS_SECTOR;

    if (need_sectors > de->cap_sectors) {
        // Grow by allocating a new extent at end, copy old contents.
        uint32_t new_cap = de->cap_sectors;
        while (new_cap < need_sectors) {
            new_cap *= 2U;
            if (new_cap == 0) return 0;
        }

        uint32_t new_start = sb.next_free_lba;
        sb.next_free_lba += new_cap;

        uint8_t sec[DISKFS_SECTOR];
        for (uint32_t s = 0; s < new_cap; s++) {
            memset(sec, 0, sizeof(sec));
            if (s < de->cap_sectors) {
                if (ata_pio_read28(de->start_lba + s, sec) < 0) {
                    return 0;
                }
            }
            (void)ata_pio_write28(new_start + s, sec);
        }

        de->start_lba = new_start;
        de->cap_sectors = new_cap;
    }

    uint32_t total = 0;
    while (total < size) {
        uint32_t pos = offset + total;
        uint32_t lba_off = pos / DISKFS_SECTOR;
        uint32_t sec_off = pos % DISKFS_SECTOR;
        uint32_t chunk = size - total;
        if (chunk > (DISKFS_SECTOR - sec_off)) chunk = DISKFS_SECTOR - sec_off;
        if (lba_off >= de->cap_sectors) break;

        uint8_t sec[DISKFS_SECTOR];
        if (sec_off != 0 || chunk != DISKFS_SECTOR) {
            if (ata_pio_read28(de->start_lba + lba_off, sec) < 0) break;
        } else {
            memset(sec, 0, sizeof(sec));
        }

        memcpy(sec + sec_off, buffer + total, chunk);
        if (ata_pio_write28(de->start_lba + lba_off, sec) < 0) break;

        total += chunk;
    }

    if (offset + total > de->size_bytes) {
        de->size_bytes = offset + total;
    }

    if (diskfs_super_store(&sb) < 0) return total;
    node->length = de->size_bytes;
    return total;
}

static struct fs_node* diskfs_root_finddir(struct fs_node* node, const char* name) {
    (void)node;
    if (!g_ready) return 0;
    if (!name || name[0] == 0) return 0;
    if (!diskfs_name_valid(name)) return 0;

    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return 0;

    int idx = diskfs_find(&sb, name);
    if (idx < 0) return 0;

    struct diskfs_node* dn = (struct diskfs_node*)kmalloc(sizeof(*dn));
    if (!dn) return 0;
    memset(dn, 0, sizeof(*dn));

    strcpy(dn->vfs.name, name);
    dn->vfs.flags = FS_FILE;
    dn->vfs.inode = 100 + (uint32_t)idx;
    dn->vfs.length = sb.files[idx].size_bytes;
    dn->vfs.read = &diskfs_read_impl;
    dn->vfs.write = &diskfs_write_impl;
    dn->vfs.open = 0;
    dn->vfs.close = &diskfs_close_impl;
    dn->vfs.finddir = 0;
    dn->idx = (uint32_t)idx;

    return &dn->vfs;
}

int diskfs_open_file(const char* rel_path, uint32_t flags, fs_node_t** out_node) {
    if (!out_node) return -EINVAL;
    *out_node = 0;
    if (!g_ready) return -ENODEV;
    if (!diskfs_name_valid(rel_path)) return -EINVAL;

    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return -EIO;

    int idx = diskfs_find(&sb, rel_path);
    if (idx < 0) {
        if ((flags & 0x40U) == 0U) { // O_CREAT
            return -ENOENT;
        }
        uint32_t new_idx = 0;
        int rc = diskfs_alloc_file(&sb, rel_path, DISKFS_DEFAULT_CAP_SECTORS, &new_idx);
        if (rc < 0) return rc;
        if (diskfs_super_store(&sb) < 0) return -EIO;
        idx = (int)new_idx;
    }

    if (idx < 0) return -ENOENT;

    if ((flags & 0x200U) != 0U) { // O_TRUNC
        sb.files[idx].size_bytes = 0;
        if (diskfs_super_store(&sb) < 0) return -EIO;
    }

    fs_node_t* n = diskfs_root_finddir(&g_root, rel_path);
    if (!n) return -EIO;
    *out_node = n;
    return 0;
}

fs_node_t* diskfs_create_root(void) {
    if (!g_ready) {
        if (ata_pio_init_primary_master() == 0) {
            g_ready = 1;
        } else {
            g_ready = 0;
        }

        memset(&g_root, 0, sizeof(g_root));
        strcpy(g_root.name, "disk");
        g_root.flags = FS_DIRECTORY;
        g_root.inode = 1;
        g_root.length = 0;
        g_root.read = 0;
        g_root.write = 0;
        g_root.open = 0;
        g_root.close = 0;
        g_root.finddir = &diskfs_root_finddir;

        if (g_ready) {
            struct diskfs_super sb;
            (void)diskfs_super_load(&sb);
        }
    }

    return g_ready ? &g_root : 0;
}
