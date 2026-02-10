#include "diskfs.h"

#include "ata_pio.h"
#include "errno.h"
#include "heap.h"
#include "utils.h"

#include <stddef.h>
#include <stdint.h>

// Very small on-disk FS stored starting at LBA2.
// - LBA0 reserved
// - LBA1 used by legacy persist counter storage
// - LBA2..LBA3 superblock (2 sectors)
// - data blocks allocated linearly from DISKFS_LBA_DATA_START upward
//
// Not a full POSIX FS; goal is persistent files with minimal directory hierarchy.

#define DISKFS_LBA_SUPER 2U
#define DISKFS_LBA_SUPER2 3U
#define DISKFS_LBA_DATA_START 4U

#define DISKFS_MAGIC 0x44465331U /* 'DFS1' */
#define DISKFS_VERSION 3U

#define DISKFS_MAX_INODES 24
#define DISKFS_NAME_MAX 24

#define DISKFS_SECTOR 512U

#define DISKFS_DEFAULT_CAP_SECTORS 8U /* 4KB */

enum {
    DISKFS_INODE_FREE = 0,
    DISKFS_INODE_FILE = 1,
    DISKFS_INODE_DIR = 2,
};

struct diskfs_inode {
    uint8_t type;
    uint8_t reserved0;
    uint16_t parent;
    char name[DISKFS_NAME_MAX];
    uint32_t start_lba;
    uint32_t size_bytes;
    uint32_t cap_sectors;
};

struct diskfs_super {
    uint32_t magic;
    uint32_t version;
    uint32_t next_free_lba;
    struct diskfs_inode inodes[DISKFS_MAX_INODES];
};

// v2 on-disk format (flat dirents) for migration.
struct diskfs_super_v2 {
    uint32_t magic;
    uint32_t version;
    uint32_t file_count;
    uint32_t next_free_lba;
    struct {
        char name[32];
        uint32_t start_lba;
        uint32_t size_bytes;
        uint32_t cap_sectors;
    } files[12];
};

struct diskfs_node {
    fs_node_t vfs;
    uint16_t ino;
};

static struct diskfs_node g_root;
static uint32_t g_ready = 0;

static int diskfs_super_store(const struct diskfs_super* sb);

static void diskfs_strlcpy(char* dst, const char* src, size_t dst_sz) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    size_t i = 0;
    for (; src[i] != 0 && i + 1 < dst_sz; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

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

    if (sb->magic != DISKFS_MAGIC) {
        memset(sb, 0, sizeof(*sb));
        sb->magic = DISKFS_MAGIC;
        sb->version = DISKFS_VERSION;
        sb->next_free_lba = DISKFS_LBA_DATA_START;

        // Root inode
        sb->inodes[0].type = DISKFS_INODE_DIR;
        sb->inodes[0].parent = 0;
        sb->inodes[0].name[0] = 0;

        return diskfs_super_store(sb);
    }

    if (sb->version == DISKFS_VERSION) {
        if (sb->next_free_lba < DISKFS_LBA_DATA_START) sb->next_free_lba = DISKFS_LBA_DATA_START;
        if (sb->inodes[0].type != DISKFS_INODE_DIR) {
            sb->inodes[0].type = DISKFS_INODE_DIR;
            sb->inodes[0].parent = 0;
            sb->inodes[0].name[0] = 0;
            (void)diskfs_super_store(sb);
        }
        return 0;
    }

    // Migration path: v2 -> v3
    if (sb->version == 2U) {
        struct diskfs_super_v2 old;
        memset(&old, 0, sizeof(old));
        if (sizeof(old) > (size_t)(DISKFS_SECTOR * 2U)) return -EIO;
        memcpy(&old, sec0, DISKFS_SECTOR);
        if (sizeof(old) > DISKFS_SECTOR) {
            memcpy(((uint8_t*)&old) + DISKFS_SECTOR, sec1, sizeof(old) - DISKFS_SECTOR);
        }

        if (old.magic != DISKFS_MAGIC || old.version != 2U) return -EIO;

        memset(sb, 0, sizeof(*sb));
        sb->magic = DISKFS_MAGIC;
        sb->version = DISKFS_VERSION;
        sb->next_free_lba = old.next_free_lba;
        if (sb->next_free_lba < DISKFS_LBA_DATA_START) sb->next_free_lba = DISKFS_LBA_DATA_START;

        sb->inodes[0].type = DISKFS_INODE_DIR;
        sb->inodes[0].parent = 0;
        sb->inodes[0].name[0] = 0;

        uint32_t n = old.file_count;
        if (n > 12U) n = 12U;
        uint16_t ino = 1;
        for (uint32_t i = 0; i < n && ino < DISKFS_MAX_INODES; i++) {
            if (old.files[i].name[0] == 0) continue;
            sb->inodes[ino].type = DISKFS_INODE_FILE;
            sb->inodes[ino].parent = 0;
            diskfs_strlcpy(sb->inodes[ino].name, old.files[i].name, sizeof(sb->inodes[ino].name));
            sb->inodes[ino].start_lba = old.files[i].start_lba;
            sb->inodes[ino].size_bytes = old.files[i].size_bytes;
            sb->inodes[ino].cap_sectors = old.files[i].cap_sectors;
            ino++;
        }

        return diskfs_super_store(sb);
    }

    // Unknown version -> re-init (best-effort)
    memset(sb, 0, sizeof(*sb));
    sb->magic = DISKFS_MAGIC;
    sb->version = DISKFS_VERSION;
    sb->next_free_lba = DISKFS_LBA_DATA_START;
    sb->inodes[0].type = DISKFS_INODE_DIR;
    sb->inodes[0].parent = 0;
    sb->inodes[0].name[0] = 0;
    return diskfs_super_store(sb);
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

static int diskfs_segment_valid(const char* name) {
    if (!name || name[0] == 0) return 0;
    for (uint32_t i = 0; name[i] != 0; i++) {
        if (i + 1 >= DISKFS_NAME_MAX) return 0;
    }
    return 1;
}

static int diskfs_find_child(const struct diskfs_super* sb, uint16_t parent, const char* name) {
    if (!sb || !name) return -1;
    for (uint16_t i = 0; i < DISKFS_MAX_INODES; i++) {
        if (sb->inodes[i].type == DISKFS_INODE_FREE) continue;
        if (sb->inodes[i].parent != parent) continue;
        if (sb->inodes[i].name[0] == 0) continue;
        if (strcmp(sb->inodes[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static int diskfs_alloc_inode_file(struct diskfs_super* sb, uint16_t parent, const char* name, uint32_t cap_sectors, uint16_t* out_ino) {
    if (!sb || !name || !out_ino) return -EINVAL;
    if (!diskfs_segment_valid(name)) return -EINVAL;

    for (uint16_t i = 1; i < DISKFS_MAX_INODES; i++) {
        if (sb->inodes[i].type != DISKFS_INODE_FREE) continue;
        sb->inodes[i].type = DISKFS_INODE_FILE;
        sb->inodes[i].parent = parent;
        memset(sb->inodes[i].name, 0, sizeof(sb->inodes[i].name));
        strcpy(sb->inodes[i].name, name);
        sb->inodes[i].start_lba = sb->next_free_lba;
        sb->inodes[i].size_bytes = 0;
        sb->inodes[i].cap_sectors = cap_sectors ? cap_sectors : DISKFS_DEFAULT_CAP_SECTORS;

        sb->next_free_lba += sb->inodes[i].cap_sectors;

        uint8_t zero[DISKFS_SECTOR];
        memset(zero, 0, sizeof(zero));
        for (uint32_t s = 0; s < sb->inodes[i].cap_sectors; s++) {
            (void)ata_pio_write28(sb->inodes[i].start_lba + s, zero);
        }

        *out_ino = i;
        return 0;
    }

    return -ENOSPC;
}

static int diskfs_split_next(const char** p_inout, char* out, size_t out_sz) {
    if (!p_inout || !*p_inout || !out || out_sz == 0) return 0;
    const char* p = *p_inout;
    while (*p == '/') p++;
    if (*p == 0) {
        *p_inout = p;
        out[0] = 0;
        return 0;
    }

    size_t i = 0;
    while (*p != 0 && *p != '/') {
        if (i + 1 < out_sz) out[i++] = *p;
        p++;
    }
    out[i] = 0;
    while (*p == '/') p++;
    *p_inout = p;
    return out[0] != 0;
}

static int diskfs_lookup_path(struct diskfs_super* sb, const char* path, uint16_t* out_ino, uint16_t* out_parent, char* out_last, size_t out_last_sz) {
    if (!sb || !path || !out_ino) return -EINVAL;
    const char* p = path;
    uint16_t cur = 0;
    uint16_t parent = 0;
    char part[DISKFS_NAME_MAX];
    char last[DISKFS_NAME_MAX];
    last[0] = 0;

    while (diskfs_split_next(&p, part, sizeof(part))) {
        if (!diskfs_segment_valid(part)) return -EINVAL;
        parent = cur;
        strcpy(last, part);
        int c = diskfs_find_child(sb, cur, part);
        if (c < 0) {
            if (out_parent) *out_parent = parent;
            if (out_last && out_last_sz) {
                diskfs_strlcpy(out_last, last, out_last_sz);
            }
            return -ENOENT;
        }
        cur = (uint16_t)c;
        if (sb->inodes[cur].type != DISKFS_INODE_DIR && *p != 0) {
            if (out_parent) *out_parent = parent;
            if (out_last && out_last_sz) diskfs_strlcpy(out_last, last, out_last_sz);
            return -ENOTDIR;
        }
    }

    if (out_parent) *out_parent = parent;
    if (out_last && out_last_sz) diskfs_strlcpy(out_last, last, out_last_sz);
    *out_ino = cur;
    return 0;
}

struct diskfs_kdirent {
    uint32_t d_ino;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[DISKFS_NAME_MAX];
};

static uint32_t diskfs_read_impl(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) return 0;
    if (node->flags != FS_FILE) return 0;
    if (!g_ready) return 0;

    struct diskfs_node* dn = (struct diskfs_node*)node;
    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return 0;
    if (dn->ino >= DISKFS_MAX_INODES) return 0;
    if (sb.inodes[dn->ino].type != DISKFS_INODE_FILE) return 0;

    struct diskfs_inode* de = &sb.inodes[dn->ino];
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
    if (dn->ino >= DISKFS_MAX_INODES) return 0;
    if (sb.inodes[dn->ino].type != DISKFS_INODE_FILE) return 0;

    struct diskfs_inode* de = &sb.inodes[dn->ino];

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
    struct diskfs_node* parent = (struct diskfs_node*)node;
    if (!g_ready) return 0;
    if (!name || name[0] == 0) return 0;
    if (!diskfs_segment_valid(name)) return 0;

    uint16_t parent_ino = parent ? parent->ino : 0;

    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return 0;
    if (parent_ino >= DISKFS_MAX_INODES) return 0;
    if (sb.inodes[parent_ino].type != DISKFS_INODE_DIR) return 0;

    int child = diskfs_find_child(&sb, parent_ino, name);
    if (child < 0) return 0;
    uint16_t cino = (uint16_t)child;

    struct diskfs_node* dn = (struct diskfs_node*)kmalloc(sizeof(*dn));
    if (!dn) return 0;
    memset(dn, 0, sizeof(*dn));

    strcpy(dn->vfs.name, name);
    dn->vfs.inode = 100 + (uint32_t)cino;
    dn->vfs.open = 0;
    dn->vfs.close = &diskfs_close_impl;
    dn->ino = cino;

    if (sb.inodes[cino].type == DISKFS_INODE_DIR) {
        dn->vfs.flags = FS_DIRECTORY;
        dn->vfs.length = 0;
        dn->vfs.read = 0;
        dn->vfs.write = 0;
        dn->vfs.finddir = &diskfs_root_finddir;
    } else {
        dn->vfs.flags = FS_FILE;
        dn->vfs.length = sb.inodes[cino].size_bytes;
        dn->vfs.read = &diskfs_read_impl;
        dn->vfs.write = &diskfs_write_impl;
        dn->vfs.finddir = 0;
    }

    return &dn->vfs;
}

int diskfs_open_file(const char* rel_path, uint32_t flags, fs_node_t** out_node) {
    if (!out_node) return -EINVAL;
    *out_node = 0;
    if (!g_ready) return -ENODEV;
    if (!rel_path || rel_path[0] == 0) return -EINVAL;

    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return -EIO;

    uint16_t ino = 0;
    uint16_t parent = 0;
    char last[DISKFS_NAME_MAX];
    last[0] = 0;
    int rc = diskfs_lookup_path(&sb, rel_path, &ino, &parent, last, sizeof(last));
    if (rc == -ENOENT) {
        if ((flags & 0x40U) == 0U) return -ENOENT; // O_CREAT
        if (last[0] == 0) return -EINVAL;

        // Ensure intermediate dirs exist: lookup again but stop before last segment.
        // We already have parent inode from lookup_path failure.
        if (parent >= DISKFS_MAX_INODES) return -EIO;
        if (sb.inodes[parent].type != DISKFS_INODE_DIR) return -ENOTDIR;

        uint16_t new_ino = 0;
        rc = diskfs_alloc_inode_file(&sb, parent, last, DISKFS_DEFAULT_CAP_SECTORS, &new_ino);
        if (rc < 0) return rc;
        if (diskfs_super_store(&sb) < 0) return -EIO;
        ino = new_ino;
    } else if (rc < 0) {
        return rc;
    }

    if (ino >= DISKFS_MAX_INODES) return -EIO;
    if (sb.inodes[ino].type != DISKFS_INODE_FILE) return -EISDIR;

    if ((flags & 0x200U) != 0U) { // O_TRUNC
        sb.inodes[ino].size_bytes = 0;
        if (diskfs_super_store(&sb) < 0) return -EIO;
    }

    // Build a transient vfs node for this inode.
    struct diskfs_node* dn = (struct diskfs_node*)kmalloc(sizeof(*dn));
    if (!dn) return -ENOMEM;
    memset(dn, 0, sizeof(*dn));
    diskfs_strlcpy(dn->vfs.name, last, sizeof(dn->vfs.name));
    dn->vfs.flags = FS_FILE;
    dn->vfs.inode = 100 + (uint32_t)ino;
    dn->vfs.length = sb.inodes[ino].size_bytes;
    dn->vfs.read = &diskfs_read_impl;
    dn->vfs.write = &diskfs_write_impl;
    dn->vfs.open = 0;
    dn->vfs.close = &diskfs_close_impl;
    dn->vfs.finddir = 0;
    dn->ino = ino;

    *out_node = &dn->vfs;
    return 0;
}

int diskfs_mkdir(const char* rel_path) {
    if (!g_ready) return -ENODEV;
    if (!rel_path || rel_path[0] == 0) return -EINVAL;

    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return -EIO;

    uint16_t ino = 0;
    uint16_t parent = 0;
    char last[DISKFS_NAME_MAX];
    last[0] = 0;
    int rc = diskfs_lookup_path(&sb, rel_path, &ino, &parent, last, sizeof(last));
    if (rc == 0) {
        return -EEXIST;
    }
    if (rc != -ENOENT) return rc;
    if (last[0] == 0) return -EINVAL;

    if (parent >= DISKFS_MAX_INODES) return -EIO;
    if (sb.inodes[parent].type != DISKFS_INODE_DIR) return -ENOTDIR;

    for (uint16_t i = 1; i < DISKFS_MAX_INODES; i++) {
        if (sb.inodes[i].type != DISKFS_INODE_FREE) continue;
        sb.inodes[i].type = DISKFS_INODE_DIR;
        sb.inodes[i].parent = parent;
        memset(sb.inodes[i].name, 0, sizeof(sb.inodes[i].name));
        diskfs_strlcpy(sb.inodes[i].name, last, sizeof(sb.inodes[i].name));
        sb.inodes[i].start_lba = 0;
        sb.inodes[i].size_bytes = 0;
        sb.inodes[i].cap_sectors = 0;
        return diskfs_super_store(&sb);
    }

    return -ENOSPC;
}

int diskfs_unlink(const char* rel_path) {
    if (!g_ready) return -ENODEV;
    if (!rel_path || rel_path[0] == 0) return -EINVAL;

    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return -EIO;

    uint16_t ino = 0;
    int rc = diskfs_lookup_path(&sb, rel_path, &ino, 0, 0, 0);
    if (rc < 0) return rc;
    if (ino == 0) return -EPERM;
    if (ino >= DISKFS_MAX_INODES) return -EIO;

    if (sb.inodes[ino].type == DISKFS_INODE_DIR) return -EISDIR;
    if (sb.inodes[ino].type != DISKFS_INODE_FILE) return -ENOENT;

    memset(&sb.inodes[ino], 0, sizeof(sb.inodes[ino]));
    return diskfs_super_store(&sb);
}

int diskfs_rmdir(const char* rel_path) {
    if (!g_ready) return -ENODEV;
    if (!rel_path || rel_path[0] == 0) return -EINVAL;

    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return -EIO;

    uint16_t ino = 0;
    int rc = diskfs_lookup_path(&sb, rel_path, &ino, 0, 0, 0);
    if (rc < 0) return rc;
    if (ino == 0) return -EPERM;
    if (ino >= DISKFS_MAX_INODES) return -EIO;
    if (sb.inodes[ino].type != DISKFS_INODE_DIR) return -ENOTDIR;

    // Check directory is empty (no children).
    for (uint16_t i = 0; i < DISKFS_MAX_INODES; i++) {
        if (sb.inodes[i].type == DISKFS_INODE_FREE) continue;
        if (sb.inodes[i].parent == ino && i != ino) return -ENOTEMPTY;
    }

    memset(&sb.inodes[ino], 0, sizeof(sb.inodes[ino]));
    return diskfs_super_store(&sb);
}

int diskfs_rename(const char* old_rel, const char* new_rel) {
    if (!g_ready) return -ENODEV;
    if (!old_rel || old_rel[0] == 0) return -EINVAL;
    if (!new_rel || new_rel[0] == 0) return -EINVAL;

    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return -EIO;

    uint16_t src_ino = 0;
    int rc = diskfs_lookup_path(&sb, old_rel, &src_ino, 0, 0, 0);
    if (rc < 0) return rc;
    if (src_ino == 0) return -EPERM;
    if (src_ino >= DISKFS_MAX_INODES) return -EIO;

    // Resolve destination: if it exists, it must be same type or we fail.
    uint16_t dst_ino = 0;
    uint16_t dst_parent = 0;
    char dst_last[DISKFS_NAME_MAX];
    dst_last[0] = 0;
    rc = diskfs_lookup_path(&sb, new_rel, &dst_ino, &dst_parent, dst_last, sizeof(dst_last));
    if (rc == 0) {
        // Destination exists: if it's a dir and source is file (or vice-versa), error.
        if (sb.inodes[dst_ino].type != sb.inodes[src_ino].type) return -EINVAL;
        if (dst_ino == src_ino) return 0; // same inode
        // Remove destination.
        memset(&sb.inodes[dst_ino], 0, sizeof(sb.inodes[dst_ino]));
    } else if (rc == -ENOENT) {
        // Parent must exist and be a dir.
        if (dst_parent >= DISKFS_MAX_INODES) return -EIO;
        if (sb.inodes[dst_parent].type != DISKFS_INODE_DIR) return -ENOTDIR;
    } else {
        return rc;
    }

    // Move: update parent and name.
    sb.inodes[src_ino].parent = dst_parent;
    memset(sb.inodes[src_ino].name, 0, sizeof(sb.inodes[src_ino].name));
    diskfs_strlcpy(sb.inodes[src_ino].name, dst_last, sizeof(sb.inodes[src_ino].name));

    return diskfs_super_store(&sb);
}

int diskfs_getdents(uint16_t dir_ino, uint32_t* inout_index, void* out, uint32_t out_len) {
    if (!inout_index || !out) return -EINVAL;
    if (!g_ready) return -ENODEV;
    if (out_len < sizeof(struct diskfs_kdirent)) return -EINVAL;

    struct diskfs_super sb;
    if (diskfs_super_load(&sb) < 0) return -EIO;

    if (dir_ino >= DISKFS_MAX_INODES) return -ENOENT;
    if (sb.inodes[dir_ino].type != DISKFS_INODE_DIR) return -ENOTDIR;

    uint32_t idx = *inout_index;
    uint32_t written = 0;
    struct diskfs_kdirent* ents = (struct diskfs_kdirent*)out;
    uint32_t cap = out_len / (uint32_t)sizeof(struct diskfs_kdirent);

    // index 0 => '.' ; index 1 => '..' ; index >=2 => scan inodes
    while (written < cap) {
        struct diskfs_kdirent e;
        memset(&e, 0, sizeof(e));

        if (idx == 0) {
            e.d_ino = (uint32_t)dir_ino;
            e.d_type = (uint8_t)DISKFS_INODE_DIR;
            diskfs_strlcpy(e.d_name, ".", sizeof(e.d_name));
        } else if (idx == 1) {
            e.d_ino = (uint32_t)sb.inodes[dir_ino].parent;
            e.d_type = (uint8_t)DISKFS_INODE_DIR;
            diskfs_strlcpy(e.d_name, "..", sizeof(e.d_name));
        } else {
            uint16_t scan = (uint16_t)(idx - 2);
            int found = 0;
            for (; scan < DISKFS_MAX_INODES; scan++) {
                if (sb.inodes[scan].type == DISKFS_INODE_FREE) continue;
                if (sb.inodes[scan].parent != dir_ino) continue;
                if (sb.inodes[scan].name[0] == 0) continue;
                e.d_ino = (uint32_t)scan;
                e.d_type = sb.inodes[scan].type;
                diskfs_strlcpy(e.d_name, sb.inodes[scan].name, sizeof(e.d_name));
                found = 1;
                scan++;
                idx = (uint32_t)scan + 2U;
                break;
            }
            if (!found) break;
        }

        e.d_reclen = (uint16_t)sizeof(e);
        ents[written] = e;
        written++;

        if (idx == 0) idx = 1;
        else if (idx == 1) idx = 2;
    }

    *inout_index = idx;
    return (int)(written * (uint32_t)sizeof(struct diskfs_kdirent));
}

fs_node_t* diskfs_create_root(void) {
    if (!g_ready) {
        if (ata_pio_init_primary_master() == 0) {
            g_ready = 1;
        } else {
            g_ready = 0;
        }

        memset(&g_root, 0, sizeof(g_root));
        strcpy(g_root.vfs.name, "disk");
        g_root.vfs.flags = FS_DIRECTORY;
        g_root.vfs.inode = 100;
        g_root.vfs.length = 0;
        g_root.vfs.read = 0;
        g_root.vfs.write = 0;
        g_root.vfs.open = 0;
        g_root.vfs.close = 0;
        g_root.vfs.finddir = &diskfs_root_finddir;
        g_root.ino = 0;

        if (g_ready) {
            struct diskfs_super sb;
            (void)diskfs_super_load(&sb);
        }
    }

    return g_ready ? &g_root.vfs : 0;
}
