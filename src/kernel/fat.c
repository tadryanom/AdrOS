// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "fat.h"
#include "fs.h"
#include "blockdev.h"
#include "heap.h"
#include "utils.h"
#include "console.h"
#include "errno.h"

#include <stddef.h>

/* ---- On-disk structures ---- */

/* FAT BPB (BIOS Parameter Block) — common to FAT12/16/32 */
struct fat_bpb {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;    /* 0 for FAT32 */
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;         /* 0 for FAT32 */
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed));

/* FAT32 extended BPB (follows common BPB at offset 36) */
struct fat32_ext {
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_num;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} __attribute__((packed));

/* FAT directory entry (32 bytes) */
struct fat_dirent {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t last_access_date;
    uint16_t first_cluster_hi;    /* high 16 bits, FAT32 only */
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} __attribute__((packed));

#define FAT_ATTR_READONLY  0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LFN       0x0F

#define FAT_DIRENT_SIZE    32
#define FAT_SECTOR_SIZE    512

/* ---- In-memory filesystem state ---- */

struct fat_state {
    const block_device_t* bdev;
    int      drive;
    uint32_t part_lba;
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint32_t fat_size;            /* sectors per FAT */
    uint32_t fat_lba;             /* LBA of first FAT */
    uint32_t root_dir_lba;        /* LBA of root directory (FAT12/16) */
    uint32_t root_dir_sectors;    /* sectors used by root dir (FAT12/16), 0 for FAT32 */
    uint32_t data_lba;            /* LBA of first data cluster */
    uint32_t total_clusters;
    uint32_t root_cluster;        /* FAT32 root cluster, 0 for FAT12/16 */
    enum fat_type type;
};

/* Per-node private data */
struct fat_node {
    fs_node_t vfs;
    struct fat_mount* mount;      /* back-pointer to mount state */
    uint32_t first_cluster;       /* first cluster of this file/dir */
    uint32_t parent_cluster;      /* parent directory first cluster (0 = root for FAT12/16) */
    uint32_t dir_entry_offset;    /* byte offset of dirent within parent dir data */
};

static uint8_t g_sec_buf[FAT_SECTOR_SIZE];

/* ---- Low-level sector I/O ---- */

static int fat_read_sector(struct fat_mount* fm, uint32_t lba, void* buf) {
    if (!fm || !fm->bdev) return -ENODEV;
    return blockdev_read(fm->bdev, lba, buf);
}

static int fat_write_sector(struct fat_mount* fm, uint32_t lba, const void* buf) {
    if (!fm || !fm->bdev) return -ENODEV;
    return blockdev_write(fm->bdev, lba, buf);
}

/* ---- FAT table access ---- */

static uint32_t fat_get_entry(struct fat_mount* fm, uint32_t cluster) {
    uint32_t fat_offset;
    uint32_t val;

    switch (fm->type) {
    case FAT_TYPE_12:
        fat_offset = cluster + (cluster / 2); /* 1.5 bytes per entry */
        break;
    case FAT_TYPE_16:
        fat_offset = cluster * 2;
        break;
    case FAT_TYPE_32:
        fat_offset = cluster * 4;
        break;
    default:
        return 0x0FFFFFFF;
    }

    uint32_t fat_sector = fm->fat_lba + fat_offset / FAT_SECTOR_SIZE;
    uint32_t offset_in_sec = fat_offset % FAT_SECTOR_SIZE;

    if (fat_read_sector(fm, fat_sector, g_sec_buf) < 0) return 0x0FFFFFFF;

    switch (fm->type) {
    case FAT_TYPE_12:
        if (offset_in_sec == FAT_SECTOR_SIZE - 1) {
            /* Entry spans two sectors */
            val = g_sec_buf[offset_in_sec];
            uint8_t sec2[FAT_SECTOR_SIZE];
            if (fat_read_sector(fm, fat_sector + 1, sec2) < 0) return 0x0FFF;
            val |= (uint32_t)sec2[0] << 8;
        } else {
            val = *(uint16_t*)(g_sec_buf + offset_in_sec);
        }
        if (cluster & 1)
            val >>= 4;
        else
            val &= 0x0FFF;
        return val;

    case FAT_TYPE_16:
        return *(uint16_t*)(g_sec_buf + offset_in_sec);

    case FAT_TYPE_32:
        return (*(uint32_t*)(g_sec_buf + offset_in_sec)) & 0x0FFFFFFF;
    }

    return 0x0FFFFFFF;
}

static int fat_set_entry(struct fat_mount* fm, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset;

    switch (fm->type) {
    case FAT_TYPE_12:
        fat_offset = cluster + (cluster / 2);
        break;
    case FAT_TYPE_16:
        fat_offset = cluster * 2;
        break;
    case FAT_TYPE_32:
        fat_offset = cluster * 4;
        break;
    default:
        return -EINVAL;
    }

    /* Write to all FAT copies */
    for (uint8_t f = 0; f < fm->num_fats; f++) {
        uint32_t fat_base = fm->fat_lba + (uint32_t)f * fm->fat_size;
        uint32_t fat_sector = fat_base + fat_offset / FAT_SECTOR_SIZE;
        uint32_t offset_in_sec = fat_offset % FAT_SECTOR_SIZE;

        uint8_t sec[FAT_SECTOR_SIZE];
        if (fat_read_sector(fm, fat_sector, sec) < 0) return -EIO;

        switch (fm->type) {
        case FAT_TYPE_12:
            if (offset_in_sec == FAT_SECTOR_SIZE - 1) {
                /* Spans two sectors */
                uint8_t sec2[FAT_SECTOR_SIZE];
                if (fat_read_sector(fm, fat_sector + 1, sec2) < 0) return -EIO;
                if (cluster & 1) {
                    sec[offset_in_sec] = (sec[offset_in_sec] & 0x0F) | ((value & 0x0F) << 4);
                    sec2[0] = (uint8_t)((value >> 4) & 0xFF);
                } else {
                    sec[offset_in_sec] = (uint8_t)(value & 0xFF);
                    sec2[0] = (sec2[0] & 0xF0) | ((value >> 8) & 0x0F);
                }
                if (fat_write_sector(fm, fat_sector, sec) < 0) return -EIO;
                if (fat_write_sector(fm, fat_sector + 1, sec2) < 0) return -EIO;
            } else {
                uint16_t* p = (uint16_t*)(sec + offset_in_sec);
                if (cluster & 1) {
                    *p = (*p & 0x000F) | ((uint16_t)(value << 4));
                } else {
                    *p = (*p & 0xF000) | ((uint16_t)(value & 0x0FFF));
                }
                if (fat_write_sector(fm, fat_sector, sec) < 0) return -EIO;
            }
            break;

        case FAT_TYPE_16:
            *(uint16_t*)(sec + offset_in_sec) = (uint16_t)value;
            if (fat_write_sector(fm, fat_sector, sec) < 0) return -EIO;
            break;

        case FAT_TYPE_32: {
            uint32_t* p = (uint32_t*)(sec + offset_in_sec);
            *p = (*p & 0xF0000000) | (value & 0x0FFFFFFF);
            if (fat_write_sector(fm, fat_sector, sec) < 0) return -EIO;
            break;
        }
        }
    }

    return 0;
}

/* ---- Cluster chain helpers ---- */

static int fat_is_eoc(struct fat_mount* fm, uint32_t val) {
    switch (fm->type) {
    case FAT_TYPE_12: return val >= 0x0FF8;
    case FAT_TYPE_16: return val >= 0xFFF8;
    case FAT_TYPE_32: return val >= 0x0FFFFFF8;
    }
    return 1;
}

static uint32_t fat_eoc_mark(struct fat_mount* fm) {
    switch (fm->type) {
    case FAT_TYPE_12: return 0x0FFF;
    case FAT_TYPE_16: return 0xFFFF;
    case FAT_TYPE_32: return 0x0FFFFFFF;
    }
    return 0x0FFFFFFF;
}

static uint32_t fat_cluster_to_lba(struct fat_mount* fm, uint32_t cluster) {
    return fm->data_lba + (cluster - 2) * fm->sectors_per_cluster;
}

static uint32_t fat_cluster_size(struct fat_mount* fm) {
    return (uint32_t)fm->sectors_per_cluster * fm->bytes_per_sector;
}

/* Follow cluster chain to the N-th cluster (0-indexed).  Returns 0 on failure. */
static uint32_t fat_follow_chain(struct fat_mount* fm, uint32_t start, uint32_t n) {
    uint32_t c = start;
    for (uint32_t i = 0; i < n; i++) {
        if (c < 2 || fat_is_eoc(fm, c)) return 0;
        c = fat_get_entry(fm, c);
    }
    return (c >= 2 && !fat_is_eoc(fm, c)) ? c : (n == 0 ? start : 0);
}

/* Count clusters in chain. */
static uint32_t fat_chain_length(struct fat_mount* fm, uint32_t start) {
    if (start < 2) return 0;
    uint32_t count = 0;
    uint32_t c = start;
    while (c >= 2 && !fat_is_eoc(fm, c) && count < fm->total_clusters) {
        count++;
        c = fat_get_entry(fm, c);
    }
    return count;
}

/* Allocate one free cluster, mark it as EOC. Returns cluster number or 0. */
static uint32_t fat_alloc_cluster(struct fat_mount* fm) {
    for (uint32_t c = 2; c < fm->total_clusters + 2; c++) {
        uint32_t val = fat_get_entry(fm, c);
        if (val == 0) {
            if (fat_set_entry(fm, c, fat_eoc_mark(fm)) < 0) return 0;
            /* Zero out the new cluster data */
            uint8_t zero[FAT_SECTOR_SIZE];
            memset(zero, 0, sizeof(zero));
            uint32_t lba = fat_cluster_to_lba(fm, c);
            for (uint8_t s = 0; s < fm->sectors_per_cluster; s++) {
                (void)fat_write_sector(fm, lba + s, zero);
            }
            return c;
        }
    }
    return 0; /* no free clusters */
}

/* Extend a cluster chain to have at least 'need' clusters total.
 * If start == 0, allocates a new chain.
 * Returns first cluster of chain, or 0 on failure. */
static uint32_t fat_extend_chain(struct fat_mount* fm, uint32_t start, uint32_t need) {
    if (need == 0) return start;

    if (start < 2) {
        /* Allocate first cluster */
        start = fat_alloc_cluster(fm);
        if (start == 0) return 0;
        need--;
    }

    /* Find end of existing chain */
    uint32_t c = start;
    uint32_t count = 1;
    while (!fat_is_eoc(fm, fat_get_entry(fm, c)) && count < need) {
        c = fat_get_entry(fm, c);
        count++;
    }
    if (!fat_is_eoc(fm, fat_get_entry(fm, c))) {
        /* Chain already long enough, keep following */
        while (!fat_is_eoc(fm, fat_get_entry(fm, c))) {
            c = fat_get_entry(fm, c);
            count++;
        }
    }

    /* Allocate more clusters if needed */
    while (count < need) {
        uint32_t nc = fat_alloc_cluster(fm);
        if (nc == 0) return 0;
        if (fat_set_entry(fm, c, nc) < 0) return 0;
        c = nc;
        count++;
    }

    return start;
}

/* Free a cluster chain starting at 'start'. */
static void fat_free_chain(struct fat_mount* fm, uint32_t start) {
    uint32_t c = start;
    while (c >= 2 && !fat_is_eoc(fm, c)) {
        uint32_t next = fat_get_entry(fm, c);
        (void)fat_set_entry(fm, c, 0);
        c = next;
    }
    if (c >= 2) {
        (void)fat_set_entry(fm, c, 0);
    }
}

/* ---- Directory I/O helpers ---- */

/* Read N-th sector of a directory.
 * For FAT12/16 root dir (cluster==0), reads from fixed root area.
 * For subdirs / FAT32 root, follows cluster chain. */
static int fat_dir_read_sector(struct fat_mount* fm, uint32_t dir_cluster, uint32_t sector_index, void* buf) {
    if (dir_cluster == 0 && fm->type != FAT_TYPE_32) {
        /* FAT12/16 fixed root directory */
        if (sector_index >= fm->root_dir_sectors) return -1;
        return fat_read_sector(fm, fm->root_dir_lba + sector_index, buf);
    }

    /* Cluster-based directory */
    uint32_t cluster_index = sector_index / fm->sectors_per_cluster;
    uint32_t sec_in_cluster = sector_index % fm->sectors_per_cluster;

    uint32_t c = fat_follow_chain(fm, dir_cluster, cluster_index);
    if (c < 2) return -1;

    return fat_read_sector(fm, fat_cluster_to_lba(fm, c) + sec_in_cluster, buf);
}

static int fat_dir_write_sector(struct fat_mount* fm, uint32_t dir_cluster, uint32_t sector_index, const void* buf) {
    if (dir_cluster == 0 && fm->type != FAT_TYPE_32) {
        if (sector_index >= fm->root_dir_sectors) return -1;
        return fat_write_sector(fm, fm->root_dir_lba + sector_index, buf);
    }

    uint32_t cluster_index = sector_index / fm->sectors_per_cluster;
    uint32_t sec_in_cluster = sector_index % fm->sectors_per_cluster;

    uint32_t c = fat_follow_chain(fm, dir_cluster, cluster_index);
    if (c < 2) return -1;

    return fat_write_sector(fm, fat_cluster_to_lba(fm, c) + sec_in_cluster, buf);
}

/* Get total number of directory sectors.
 * For fixed root: root_dir_sectors.
 * For cluster-based: chain_length * sectors_per_cluster. */
static uint32_t fat_dir_total_sectors(struct fat_mount* fm, uint32_t dir_cluster) {
    if (dir_cluster == 0 && fm->type != FAT_TYPE_32) {
        return fm->root_dir_sectors;
    }
    return fat_chain_length(fm, dir_cluster) * fm->sectors_per_cluster;
}

/* ---- 8.3 name conversion ---- */

static void fat_name_to_83(const char* name, char out[11]) {
    memset(out, ' ', 11);

    const char* dot = NULL;
    for (const char* p = name; *p; p++) {
        if (*p == '.') dot = p;
    }

    int i = 0;
    const char* p = name;
    if (dot) {
        for (; p < dot && i < 8; p++, i++) {
            out[i] = (*p >= 'a' && *p <= 'z') ? (*p - 32) : *p;
        }
        p = dot + 1;
        for (i = 0; *p && i < 3; p++, i++) {
            out[8 + i] = (*p >= 'a' && *p <= 'z') ? (*p - 32) : *p;
        }
    } else {
        for (; *p && i < 8; p++, i++) {
            out[i] = (*p >= 'a' && *p <= 'z') ? (*p - 32) : *p;
        }
    }
}

static void fat_83_to_name(const struct fat_dirent* de, char* out, size_t out_sz) {
    size_t fi = 0;
    for (int j = 0; j < 8 && de->name[j] != ' ' && fi + 1 < out_sz; j++) {
        char c = de->name[j];
        out[fi++] = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
    }
    if (de->ext[0] != ' ' && fi + 2 < out_sz) {
        out[fi++] = '.';
        for (int j = 0; j < 3 && de->ext[j] != ' ' && fi + 1 < out_sz; j++) {
            char c = de->ext[j];
            out[fi++] = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        }
    }
    out[fi] = '\0';
}

static uint32_t fat_dirent_cluster(struct fat_mount* fm, const struct fat_dirent* de) {
    uint32_t cl = de->first_cluster_lo;
    if (fm->type == FAT_TYPE_32) {
        cl |= (uint32_t)de->first_cluster_hi << 16;
    }
    return cl;
}

static void fat_dirent_set_cluster(struct fat_mount* fm, struct fat_dirent* de, uint32_t cl) {
    de->first_cluster_lo = (uint16_t)(cl & 0xFFFF);
    if (fm->type == FAT_TYPE_32) {
        de->first_cluster_hi = (uint16_t)((cl >> 16) & 0xFFFF);
    }
}

/* ---- Forward declarations ---- */
static fs_node_t* fat_finddir(fs_node_t* node, const char* name);
static uint32_t fat_file_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static uint32_t fat_file_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
static int fat_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len);
static int fat_create_impl(struct fs_node* dir, const char* name, uint32_t flags, struct fs_node** out);
static int fat_mkdir_impl(struct fs_node* dir, const char* name);
static int fat_unlink_impl(struct fs_node* dir, const char* name);
static int fat_rmdir_impl(struct fs_node* dir, const char* name);
static int fat_rename_impl(struct fs_node* old_dir, const char* old_name,
                            struct fs_node* new_dir, const char* new_name);
static int fat_truncate_impl(struct fs_node* node, uint32_t length);
static void fat_close_impl(fs_node_t* node);

static const struct file_operations fat_file_fops = {
    .read     = fat_file_read,
    .write    = fat_file_write,
    .close    = fat_close_impl,
};

static const struct inode_operations fat_file_iops = {
    .truncate = fat_truncate_impl,
};

static const struct file_operations fat_dir_fops = {
    .close   = fat_close_impl,
};

static const struct inode_operations fat_dir_iops = {
    .lookup  = fat_finddir,
    .readdir = fat_readdir_impl,
    .create  = fat_create_impl,
    .mkdir   = fat_mkdir_impl,
    .unlink  = fat_unlink_impl,
    .rmdir   = fat_rmdir_impl,
    .rename  = fat_rename_impl,
};

static void fat_close_impl(fs_node_t* node) {
    if (!node) return;
    struct fat_node* fn = (struct fat_node*)node;
    kfree(fn);
}

static struct fat_node* fat_make_node(struct fat_mount* fm, const struct fat_dirent* de, uint32_t parent_cluster, uint32_t dirent_offset) {
    struct fat_node* fn = (struct fat_node*)kmalloc(sizeof(struct fat_node));
    if (!fn) return NULL;
    memset(fn, 0, sizeof(*fn));

    fn->mount = fm;
    fat_83_to_name(de, fn->vfs.name, sizeof(fn->vfs.name));
    fn->first_cluster = fat_dirent_cluster(fm, de);
    fn->parent_cluster = parent_cluster;
    fn->dir_entry_offset = dirent_offset;

    if (de->attr & FAT_ATTR_DIRECTORY) {
        fn->vfs.flags = FS_DIRECTORY;
        fn->vfs.length = 0;
        fn->vfs.inode = fn->first_cluster;
        fn->vfs.f_ops = &fat_dir_fops;
        fn->vfs.i_ops = &fat_dir_iops;
    } else {
        fn->vfs.flags = FS_FILE;
        fn->vfs.length = de->file_size;
        fn->vfs.inode = fn->first_cluster;
        fn->vfs.f_ops = &fat_file_fops;
        fn->vfs.i_ops = &fat_file_iops;
    }

    return fn;
}

/* ---- VFS: file read ---- */

static uint32_t fat_file_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) return 0;
    struct fat_node* fn = (struct fat_node*)node;
    struct fat_mount* fm = fn->mount;
    if (!fm) return 0;
    if (offset >= node->length) return 0;
    if (offset + size > node->length) size = node->length - offset;
    if (size == 0) return 0;

    uint32_t csize = fat_cluster_size(fm);
    uint32_t cluster = fn->first_cluster;
    uint32_t bytes_read = 0;

    /* Skip to cluster containing 'offset' */
    uint32_t skip = offset / csize;
    for (uint32_t i = 0; i < skip && cluster >= 2 && !fat_is_eoc(fm, cluster); i++) {
        cluster = fat_get_entry(fm, cluster);
    }
    uint32_t pos_in_cluster = offset % csize;

    while (bytes_read < size && cluster >= 2 && !fat_is_eoc(fm, cluster)) {
        uint32_t lba = fat_cluster_to_lba(fm, cluster);
        for (uint32_t s = pos_in_cluster / FAT_SECTOR_SIZE;
             s < fm->sectors_per_cluster && bytes_read < size; s++) {
            uint8_t sec[FAT_SECTOR_SIZE];
            if (fat_read_sector(fm, lba + s, sec) < 0) return bytes_read;
            uint32_t off_in_sec = (pos_in_cluster > 0 && s == pos_in_cluster / FAT_SECTOR_SIZE)
                                  ? pos_in_cluster % FAT_SECTOR_SIZE : 0;
            uint32_t to_copy = FAT_SECTOR_SIZE - off_in_sec;
            if (to_copy > size - bytes_read) to_copy = size - bytes_read;
            memcpy(buffer + bytes_read, sec + off_in_sec, to_copy);
            bytes_read += to_copy;
        }
        pos_in_cluster = 0;
        cluster = fat_get_entry(fm, cluster);
    }

    return bytes_read;
}

/* ---- VFS: file write ---- */

/* Update the dirent on disk (file size / first cluster) after a write. */
static int fat_update_dirent(struct fat_node* fn) {
    struct fat_mount* fm = fn->mount;
    if (!fm) return -EIO;
    uint32_t sec_idx = fn->dir_entry_offset / FAT_SECTOR_SIZE;
    uint32_t off_in_sec = fn->dir_entry_offset % FAT_SECTOR_SIZE;

    uint8_t sec[FAT_SECTOR_SIZE];
    if (fat_dir_read_sector(fm, fn->parent_cluster, sec_idx, sec) < 0) return -EIO;

    struct fat_dirent* de = (struct fat_dirent*)(sec + off_in_sec);
    de->file_size = fn->vfs.length;
    fat_dirent_set_cluster(fm, de, fn->first_cluster);

    return fat_dir_write_sector(fm, fn->parent_cluster, sec_idx, sec);
}

static uint32_t fat_file_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !buffer || size == 0) return 0;
    struct fat_node* fn = (struct fat_node*)node;
    struct fat_mount* fm = fn->mount;
    if (!fm) return 0;

    uint64_t end64 = (uint64_t)offset + (uint64_t)size;
    if (end64 > 0xFFFFFFFFULL) return 0;
    uint32_t end = (uint32_t)end64;

    /* Ensure enough clusters allocated */
    uint32_t csize = fat_cluster_size(fm);
    uint32_t need_clusters = (end + csize - 1) / csize;
    if (need_clusters == 0) need_clusters = 1;

    fn->first_cluster = fat_extend_chain(fm, fn->first_cluster, need_clusters);
    if (fn->first_cluster == 0) return 0;

    /* Write data */
    uint32_t cluster = fn->first_cluster;
    uint32_t total = 0;

    uint32_t skip = offset / csize;
    for (uint32_t i = 0; i < skip && cluster >= 2 && !fat_is_eoc(fm, cluster); i++) {
        cluster = fat_get_entry(fm, cluster);
    }
    uint32_t pos_in_cluster = offset % csize;

    while (total < size && cluster >= 2 && !fat_is_eoc(fm, cluster)) {
        uint32_t lba = fat_cluster_to_lba(fm, cluster);
        for (uint32_t s = pos_in_cluster / FAT_SECTOR_SIZE;
             s < fm->sectors_per_cluster && total < size; s++) {
            uint8_t sec[FAT_SECTOR_SIZE];
            uint32_t off_in_sec = (pos_in_cluster > 0 && s == pos_in_cluster / FAT_SECTOR_SIZE)
                                  ? pos_in_cluster % FAT_SECTOR_SIZE : 0;
            uint32_t chunk = FAT_SECTOR_SIZE - off_in_sec;
            if (chunk > size - total) chunk = size - total;

            /* Read-modify-write for partial sectors */
            if (off_in_sec != 0 || chunk != FAT_SECTOR_SIZE) {
                if (fat_read_sector(fm, lba + s, sec) < 0) goto done;
            }
            memcpy(sec + off_in_sec, buffer + total, chunk);
            if (fat_write_sector(fm, lba + s, sec) < 0) goto done;
            total += chunk;
        }
        pos_in_cluster = 0;
        cluster = fat_get_entry(fm, cluster);
    }

done:
    if (offset + total > fn->vfs.length) {
        fn->vfs.length = offset + total;
    }
    node->length = fn->vfs.length;
    (void)fat_update_dirent(fn);
    return total;
}

/* ---- VFS: finddir ---- */

static fs_node_t* fat_finddir(fs_node_t* node, const char* name) {
    if (!node || !name) return NULL;
    struct fat_node* dir = (struct fat_node*)node;
    struct fat_mount* fm = dir->mount;
    if (!fm) return NULL;
    uint32_t dir_cluster = dir->first_cluster;

    /* For FAT12/16 root: dir_cluster may be 0 */
    uint32_t total_sec = fat_dir_total_sectors(fm, dir_cluster);
    uint32_t ents_per_sec = FAT_SECTOR_SIZE / FAT_DIRENT_SIZE;

    for (uint32_t s = 0; s < total_sec; s++) {
        uint8_t sec[FAT_SECTOR_SIZE];
        if (fat_dir_read_sector(fm, dir_cluster, s, sec) < 0) return NULL;
        struct fat_dirent* de = (struct fat_dirent*)sec;

        for (uint32_t i = 0; i < ents_per_sec; i++) {
            if (de[i].name[0] == 0) return NULL; /* end of dir */
            if ((uint8_t)de[i].name[0] == 0xE5) continue; /* deleted */
            if (de[i].attr & FAT_ATTR_LFN) continue;
            if (de[i].attr & FAT_ATTR_VOLUME_ID) continue;

            char fname[13];
            fat_83_to_name(&de[i], fname, sizeof(fname));

            if (strcmp(fname, name) == 0) {
                uint32_t dirent_off = s * FAT_SECTOR_SIZE + i * FAT_DIRENT_SIZE;
                return (fs_node_t*)fat_make_node(fm, &de[i], dir_cluster, dirent_off);
            }
        }
    }

    return NULL;
}

/* ---- VFS: readdir ---- */

static int fat_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    if (!node || !inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    struct fat_node* dir = (struct fat_node*)node;
    struct fat_mount* fm = dir->mount;
    if (!fm) return -1;
    uint32_t dir_cluster = dir->first_cluster;
    uint32_t total_sec = fat_dir_total_sectors(fm, dir_cluster);
    uint32_t ents_per_sec = FAT_SECTOR_SIZE / FAT_DIRENT_SIZE;

    uint32_t idx = *inout_index;
    uint32_t cap = buf_len / (uint32_t)sizeof(struct vfs_dirent);
    struct vfs_dirent* out = (struct vfs_dirent*)buf;
    uint32_t written = 0;

    /* Walk directory entries from linear index */
    uint32_t cur = 0;
    for (uint32_t s = 0; s < total_sec && written < cap; s++) {
        uint8_t sec[FAT_SECTOR_SIZE];
        if (fat_dir_read_sector(fm, dir_cluster, s, sec) < 0) break;
        struct fat_dirent* de = (struct fat_dirent*)sec;

        for (uint32_t i = 0; i < ents_per_sec && written < cap; i++) {
            if (de[i].name[0] == 0) goto done; /* end of dir */
            if ((uint8_t)de[i].name[0] == 0xE5) continue;
            if (de[i].attr & FAT_ATTR_LFN) continue;
            if (de[i].attr & FAT_ATTR_VOLUME_ID) continue;

            /* Skip . and .. entries */
            if (de[i].name[0] == '.' && de[i].name[1] == ' ') continue;
            if (de[i].name[0] == '.' && de[i].name[1] == '.' && de[i].name[2] == ' ') continue;

            if (cur >= idx) {
                memset(&out[written], 0, sizeof(out[written]));
                out[written].d_ino = fat_dirent_cluster(fm, &de[i]);
                out[written].d_reclen = (uint16_t)sizeof(struct vfs_dirent);
                out[written].d_type = (de[i].attr & FAT_ATTR_DIRECTORY) ? 2 : 1;
                fat_83_to_name(&de[i], out[written].d_name, sizeof(out[written].d_name));
                written++;
            }
            cur++;
        }
    }

done:
    *inout_index = cur;
    return (int)(written * (uint32_t)sizeof(struct vfs_dirent));
}

/* ---- VFS: create file ---- */

static int fat_add_dirent(struct fat_mount* fm, uint32_t dir_cluster, const char* name, uint8_t attr,
                           uint32_t first_cluster, uint32_t file_size,
                           uint32_t* out_offset) {
    char name83[11];
    fat_name_to_83(name, name83);

    uint32_t total_sec = fat_dir_total_sectors(fm, dir_cluster);
    uint32_t ents_per_sec = FAT_SECTOR_SIZE / FAT_DIRENT_SIZE;

    for (uint32_t s = 0; s < total_sec; s++) {
        uint8_t sec[FAT_SECTOR_SIZE];
        if (fat_dir_read_sector(fm, dir_cluster, s, sec) < 0) return -EIO;
        struct fat_dirent* de = (struct fat_dirent*)sec;

        for (uint32_t i = 0; i < ents_per_sec; i++) {
            if (de[i].name[0] == 0 || (uint8_t)de[i].name[0] == 0xE5) {
                memset(&de[i], 0, sizeof(de[i]));
                memcpy(de[i].name, name83, 8);
                memcpy(de[i].ext, name83 + 8, 3);
                de[i].attr = attr;
                fat_dirent_set_cluster(fm, &de[i], first_cluster);
                de[i].file_size = file_size;
                if (fat_dir_write_sector(fm, dir_cluster, s, sec) < 0) return -EIO;
                if (out_offset) *out_offset = s * FAT_SECTOR_SIZE + i * FAT_DIRENT_SIZE;
                return 0;
            }
        }
    }

    /* Need to extend directory (only for cluster-based dirs) */
    if (dir_cluster == 0 && fm->type != FAT_TYPE_32) {
        return -ENOSPC; /* can't extend fixed root */
    }

    /* Extend directory by one cluster */
    uint32_t old_len = fat_chain_length(fm, dir_cluster);
    uint32_t new_first = fat_extend_chain(fm, dir_cluster, old_len + 1);
    if (new_first == 0) return -ENOSPC;

    /* Write dirent into first entry of new cluster */
    uint32_t new_sec_idx = old_len * fm->sectors_per_cluster;
    uint8_t sec[FAT_SECTOR_SIZE];
    if (fat_dir_read_sector(fm, dir_cluster, new_sec_idx, sec) < 0) return -EIO;
    struct fat_dirent* de = (struct fat_dirent*)sec;
    memset(&de[0], 0, sizeof(de[0]));
    memcpy(de[0].name, name83, 8);
    memcpy(de[0].ext, name83 + 8, 3);
    de[0].attr = attr;
    fat_dirent_set_cluster(fm, &de[0], first_cluster);
    de[0].file_size = file_size;
    if (fat_dir_write_sector(fm, dir_cluster, new_sec_idx, sec) < 0) return -EIO;
    if (out_offset) *out_offset = new_sec_idx * FAT_SECTOR_SIZE;
    return 0;
}

/* Find dirent by name, returns sector index via sec_idx, entry index in sector via ent_idx */
static int fat_find_dirent(struct fat_mount* fm, uint32_t dir_cluster, const char* name,
                            uint32_t* out_sec_idx, uint32_t* out_ent_idx) {
    uint32_t total_sec = fat_dir_total_sectors(fm, dir_cluster);
    uint32_t ents_per_sec = FAT_SECTOR_SIZE / FAT_DIRENT_SIZE;

    for (uint32_t s = 0; s < total_sec; s++) {
        uint8_t sec[FAT_SECTOR_SIZE];
        if (fat_dir_read_sector(fm, dir_cluster, s, sec) < 0) return -EIO;
        struct fat_dirent* de = (struct fat_dirent*)sec;

        for (uint32_t i = 0; i < ents_per_sec; i++) {
            if (de[i].name[0] == 0) return -ENOENT;
            if ((uint8_t)de[i].name[0] == 0xE5) continue;
            if (de[i].attr & FAT_ATTR_LFN) continue;
            if (de[i].attr & FAT_ATTR_VOLUME_ID) continue;

            char fname[13];
            fat_83_to_name(&de[i], fname, sizeof(fname));
            if (strcmp(fname, name) == 0) {
                if (out_sec_idx) *out_sec_idx = s;
                if (out_ent_idx) *out_ent_idx = i;
                return 0;
            }
        }
    }
    return -ENOENT;
}

static int fat_create_impl(struct fs_node* dir, const char* name, uint32_t flags, struct fs_node** out) {
    if (!dir || !name || !out) return -EINVAL;
    *out = NULL;
    struct fat_node* parent = (struct fat_node*)dir;
    struct fat_mount* fm = parent->mount;
    if (!fm) return -EIO;
    uint32_t dir_cluster = parent->first_cluster;

    /* Check if exists */
    uint32_t sec_idx, ent_idx;
    int rc = fat_find_dirent(fm, dir_cluster, name, &sec_idx, &ent_idx);
    if (rc == 0) {
        /* Already exists */
        uint8_t sec[FAT_SECTOR_SIZE];
        if (fat_dir_read_sector(fm, dir_cluster, sec_idx, sec) < 0) return -EIO;
        struct fat_dirent* de = (struct fat_dirent*)sec;
        if (de[ent_idx].attr & FAT_ATTR_DIRECTORY) return -EISDIR;

        if ((flags & 0x200U) != 0U) { /* O_TRUNC */
            uint32_t cl = fat_dirent_cluster(fm, &de[ent_idx]);
            if (cl >= 2) fat_free_chain(fm, cl);
            fat_dirent_set_cluster(fm, &de[ent_idx], 0);
            de[ent_idx].file_size = 0;
            if (fat_dir_write_sector(fm, dir_cluster, sec_idx, sec) < 0) return -EIO;
        }

        uint32_t dirent_off = sec_idx * FAT_SECTOR_SIZE + ent_idx * FAT_DIRENT_SIZE;
        struct fat_node* fn = fat_make_node(fm, &de[ent_idx], dir_cluster, dirent_off);
        if (!fn) return -ENOMEM;
        *out = &fn->vfs;
        return 0;
    }

    if ((flags & 0x40U) == 0U) return -ENOENT; /* O_CREAT not set */

    /* Create new file */
    uint32_t dirent_off = 0;
    rc = fat_add_dirent(fm, dir_cluster, name, FAT_ATTR_ARCHIVE, 0, 0, &dirent_off);
    if (rc < 0) return rc;

    /* Read back the dirent to build node */
    uint32_t s2 = dirent_off / FAT_SECTOR_SIZE;
    uint32_t e2 = (dirent_off % FAT_SECTOR_SIZE) / FAT_DIRENT_SIZE;
    uint8_t sec2[FAT_SECTOR_SIZE];
    if (fat_dir_read_sector(fm, dir_cluster, s2, sec2) < 0) return -EIO;
    struct fat_dirent* de2 = (struct fat_dirent*)sec2;
    struct fat_node* fn = fat_make_node(fm, &de2[e2], dir_cluster, dirent_off);
    if (!fn) return -ENOMEM;
    *out = &fn->vfs;
    return 0;
}

/* ---- VFS: mkdir ---- */

static int fat_mkdir_impl(struct fs_node* dir, const char* name) {
    if (!dir || !name) return -EINVAL;
    struct fat_node* parent = (struct fat_node*)dir;
    struct fat_mount* fm = parent->mount;
    if (!fm) return -EIO;
    uint32_t dir_cluster = parent->first_cluster;

    /* Check doesn't exist */
    if (fat_find_dirent(fm, dir_cluster, name, NULL, NULL) == 0) return -EEXIST;

    /* Allocate a cluster for the new directory */
    uint32_t new_cl = fat_alloc_cluster(fm);
    if (new_cl == 0) return -ENOSPC;

    /* Write . and .. entries */
    uint8_t sec[FAT_SECTOR_SIZE];
    memset(sec, 0, sizeof(sec));
    struct fat_dirent* de = (struct fat_dirent*)sec;

    /* "." entry */
    memset(de[0].name, ' ', 8);
    memset(de[0].ext, ' ', 3);
    de[0].name[0] = '.';
    de[0].attr = FAT_ATTR_DIRECTORY;
    fat_dirent_set_cluster(fm, &de[0], new_cl);

    /* ".." entry */
    memset(de[1].name, ' ', 8);
    memset(de[1].ext, ' ', 3);
    de[1].name[0] = '.';
    de[1].name[1] = '.';
    de[1].attr = FAT_ATTR_DIRECTORY;
    fat_dirent_set_cluster(fm, &de[1], dir_cluster);

    uint32_t lba = fat_cluster_to_lba(fm, new_cl);
    if (fat_write_sector(fm, lba, sec) < 0) {
        fat_free_chain(fm, new_cl);
        return -EIO;
    }

    /* Add dirent in parent */
    int rc = fat_add_dirent(fm, dir_cluster, name, FAT_ATTR_DIRECTORY, new_cl, 0, NULL);
    if (rc < 0) {
        fat_free_chain(fm, new_cl);
        return rc;
    }

    return 0;
}

/* ---- VFS: unlink ---- */

static int fat_unlink_impl(struct fs_node* dir, const char* name) {
    if (!dir || !name) return -EINVAL;
    struct fat_node* parent = (struct fat_node*)dir;
    struct fat_mount* fm = parent->mount;
    if (!fm) return -EIO;
    uint32_t dir_cluster = parent->first_cluster;

    uint32_t sec_idx, ent_idx;
    int rc = fat_find_dirent(fm, dir_cluster, name, &sec_idx, &ent_idx);
    if (rc < 0) return rc;

    uint8_t sec[FAT_SECTOR_SIZE];
    if (fat_dir_read_sector(fm, dir_cluster, sec_idx, sec) < 0) return -EIO;
    struct fat_dirent* de = (struct fat_dirent*)sec;

    if (de[ent_idx].attr & FAT_ATTR_DIRECTORY) return -EISDIR;

    /* Free cluster chain */
    uint32_t cl = fat_dirent_cluster(fm, &de[ent_idx]);
    if (cl >= 2) fat_free_chain(fm, cl);

    /* Mark entry as deleted */
    de[ent_idx].name[0] = (char)0xE5;
    return fat_dir_write_sector(fm, dir_cluster, sec_idx, sec);
}

/* ---- VFS: rmdir ---- */

static int fat_dir_is_empty(struct fat_mount* fm, uint32_t dir_cluster) {
    uint32_t total_sec = fat_dir_total_sectors(fm, dir_cluster);
    uint32_t ents_per_sec = FAT_SECTOR_SIZE / FAT_DIRENT_SIZE;

    for (uint32_t s = 0; s < total_sec; s++) {
        uint8_t sec[FAT_SECTOR_SIZE];
        if (fat_dir_read_sector(fm, dir_cluster, s, sec) < 0) return 0;
        struct fat_dirent* de = (struct fat_dirent*)sec;

        for (uint32_t i = 0; i < ents_per_sec; i++) {
            if (de[i].name[0] == 0) return 1; /* end of dir — empty */
            if ((uint8_t)de[i].name[0] == 0xE5) continue;
            if (de[i].attr & FAT_ATTR_LFN) continue;
            if (de[i].attr & FAT_ATTR_VOLUME_ID) continue;

            /* Skip . and .. */
            if (de[i].name[0] == '.' && de[i].name[1] == ' ') continue;
            if (de[i].name[0] == '.' && de[i].name[1] == '.' && de[i].name[2] == ' ') continue;

            return 0; /* has a non-dot entry */
        }
    }
    return 1;
}

static int fat_rmdir_impl(struct fs_node* dir, const char* name) {
    if (!dir || !name) return -EINVAL;
    struct fat_node* parent = (struct fat_node*)dir;
    struct fat_mount* fm = parent->mount;
    if (!fm) return -EIO;
    uint32_t dir_cluster = parent->first_cluster;

    uint32_t sec_idx, ent_idx;
    int rc = fat_find_dirent(fm, dir_cluster, name, &sec_idx, &ent_idx);
    if (rc < 0) return rc;

    uint8_t sec[FAT_SECTOR_SIZE];
    if (fat_dir_read_sector(fm, dir_cluster, sec_idx, sec) < 0) return -EIO;
    struct fat_dirent* de = (struct fat_dirent*)sec;

    if (!(de[ent_idx].attr & FAT_ATTR_DIRECTORY)) return -ENOTDIR;

    uint32_t child_cl = fat_dirent_cluster(fm, &de[ent_idx]);
    if (child_cl >= 2 && !fat_dir_is_empty(fm, child_cl)) return -ENOTEMPTY;

    /* Free cluster chain */
    if (child_cl >= 2) fat_free_chain(fm, child_cl);

    /* Mark entry deleted */
    de[ent_idx].name[0] = (char)0xE5;
    return fat_dir_write_sector(fm, dir_cluster, sec_idx, sec);
}

/* ---- VFS: rename ---- */

static int fat_rename_impl(struct fs_node* old_dir, const char* old_name,
                            struct fs_node* new_dir, const char* new_name) {
    if (!old_dir || !old_name || !new_dir || !new_name) return -EINVAL;
    struct fat_node* odir = (struct fat_node*)old_dir;
    struct fat_node* ndir = (struct fat_node*)new_dir;
    struct fat_mount* fm = odir->mount;
    if (!fm) return -EIO;

    /* Find source */
    uint32_t src_sec, src_ent;
    int rc = fat_find_dirent(fm, odir->first_cluster, old_name, &src_sec, &src_ent);
    if (rc < 0) return rc;

    uint8_t src_buf[FAT_SECTOR_SIZE];
    if (fat_dir_read_sector(fm, odir->first_cluster, src_sec, src_buf) < 0) return -EIO;
    struct fat_dirent* src_de = &((struct fat_dirent*)src_buf)[src_ent];

    /* Save source dirent data */
    struct fat_dirent saved;
    memcpy(&saved, src_de, sizeof(saved));

    /* Remove destination if exists */
    uint32_t dst_sec, dst_ent;
    rc = fat_find_dirent(fm, ndir->first_cluster, new_name, &dst_sec, &dst_ent);
    if (rc == 0) {
        uint8_t dst_buf[FAT_SECTOR_SIZE];
        if (fat_dir_read_sector(fm, ndir->first_cluster, dst_sec, dst_buf) < 0) return -EIO;
        struct fat_dirent* dst_de = &((struct fat_dirent*)dst_buf)[dst_ent];

        /* Free old destination data */
        uint32_t dst_cl = fat_dirent_cluster(fm, dst_de);
        if (dst_cl >= 2) fat_free_chain(fm, dst_cl);

        dst_de->name[0] = (char)0xE5;
        if (fat_dir_write_sector(fm, ndir->first_cluster, dst_sec, dst_buf) < 0) return -EIO;
    }

    /* Delete source entry */
    src_de->name[0] = (char)0xE5;
    if (fat_dir_write_sector(fm, odir->first_cluster, src_sec, src_buf) < 0) return -EIO;

    /* Add new entry in destination dir */
    uint32_t cl = fat_dirent_cluster(fm, &saved);
    rc = fat_add_dirent(fm, ndir->first_cluster, new_name, saved.attr, cl, saved.file_size, NULL);
    if (rc < 0) return rc;

    /* Update ".." in moved directory if applicable */
    if (saved.attr & FAT_ATTR_DIRECTORY) {
        if (cl >= 2 && odir->first_cluster != ndir->first_cluster) {
            uint8_t dsec[FAT_SECTOR_SIZE];
            if (fat_dir_read_sector(fm, cl, 0, dsec) == 0) {
                struct fat_dirent* entries = (struct fat_dirent*)dsec;
                if (entries[1].name[0] == '.' && entries[1].name[1] == '.') {
                    fat_dirent_set_cluster(fm, &entries[1], ndir->first_cluster);
                    (void)fat_dir_write_sector(fm, cl, 0, dsec);
                }
            }
        }
    }

    return 0;
}

/* ---- VFS: truncate ---- */

static int fat_truncate_impl(struct fs_node* node, uint32_t length) {
    if (!node) return -EINVAL;
    struct fat_node* fn = (struct fat_node*)node;
    struct fat_mount* fm = fn->mount;
    if (!fm) return -EIO;

    if (length >= fn->vfs.length) return 0; /* only shrink */

    uint32_t csize = fat_cluster_size(fm);
    uint32_t need_clusters = (length + csize - 1) / csize;

    if (need_clusters == 0) {
        /* Free everything */
        if (fn->first_cluster >= 2) {
            fat_free_chain(fm, fn->first_cluster);
            fn->first_cluster = 0;
        }
    } else {
        /* Keep first N clusters, free the rest */
        uint32_t c = fn->first_cluster;
        for (uint32_t i = 1; i < need_clusters; i++) {
            c = fat_get_entry(fm, c);
        }
        uint32_t next = fat_get_entry(fm, c);
        (void)fat_set_entry(fm, c, fat_eoc_mark(fm));
        if (next >= 2 && !fat_is_eoc(fm, next)) {
            fat_free_chain(fm, next);
        }
    }

    fn->vfs.length = length;
    node->length = length;
    (void)fat_update_dirent(fn);
    return 0;
}

/* ---- Mount ---- */

vfs_mount_result_t fat_mount(const block_device_t* bdev, uint32_t partition_lba) {
    vfs_mount_result_t result = {NULL, NULL};

    if (!bdev) {
        kprintf("[FAT] No block device provided\n");
        return result;
    }

    /* Allocate mount structure */
    struct fat_mount* fm = (struct fat_mount*)kmalloc(sizeof(struct fat_mount));
    if (!fm) {
        kprintf("[FAT] Failed to allocate mount structure\n");
        return result;
    }
    memset(fm, 0, sizeof(*fm));

    fm->bdev = bdev;

    uint8_t boot_sec[FAT_SECTOR_SIZE];
    if (fat_read_sector(fm, partition_lba, boot_sec) < 0) {
        kprintf("[FAT] Failed to read BPB at LBA %u\n", partition_lba);
        kfree(fm);
        return result;
    }

    struct fat_bpb* bpb = (struct fat_bpb*)boot_sec;

    if (bpb->bytes_per_sector != 512) {
        kprintf("[FAT] Unsupported sector size %u\n", bpb->bytes_per_sector);
        kfree(fm);
        return result;
    }
    if (bpb->num_fats == 0 || bpb->sectors_per_cluster == 0) {
        kprintf("[FAT] Invalid BPB\n");
        kfree(fm);
        return result;
    }

    fm->part_lba = partition_lba;
    fm->bytes_per_sector = bpb->bytes_per_sector;
    fm->sectors_per_cluster = bpb->sectors_per_cluster;
    fm->reserved_sectors = bpb->reserved_sectors;
    fm->num_fats = bpb->num_fats;
    fm->root_entry_count = bpb->root_entry_count;

    /* Determine FAT size */
    if (bpb->fat_size_16 != 0) {
        fm->fat_size = bpb->fat_size_16;
    } else {
        struct fat32_ext* ext32 = (struct fat32_ext*)(boot_sec + 36);
        fm->fat_size = ext32->fat_size_32;
        fm->root_cluster = ext32->root_cluster;
    }

    fm->fat_lba = partition_lba + bpb->reserved_sectors;
    fm->root_dir_lba = fm->fat_lba + (uint32_t)bpb->num_fats * fm->fat_size;
    fm->root_dir_sectors = ((uint32_t)bpb->root_entry_count * 32 + 511) / 512;
    fm->data_lba = fm->root_dir_lba + fm->root_dir_sectors;

    /* Total data sectors & cluster count determine FAT type */
    uint32_t total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;
    uint32_t data_sectors = total_sectors - (fm->data_lba - partition_lba);
    fm->total_clusters = data_sectors / fm->sectors_per_cluster;

    /* Microsoft FAT spec: type is determined by cluster count */
    if (fm->total_clusters < 4085) {
        fm->type = FAT_TYPE_12;
    } else if (fm->total_clusters < 65525) {
        fm->type = FAT_TYPE_16;
    } else {
        fm->type = FAT_TYPE_32;
    }

    /* Build root node */
    struct fat_node* root = (struct fat_node*)kmalloc(sizeof(struct fat_node));
    if (!root) {
        kprintf("[FAT] Failed to allocate root node\n");
        kfree(fm);
        return result;
    }
    memset(root, 0, sizeof(*root));
    root->mount = fm;
    memcpy(root->vfs.name, "fat", 4);
    root->vfs.flags = FS_DIRECTORY;
    root->vfs.inode = 0;
    root->first_cluster = (fm->type == FAT_TYPE_32) ? fm->root_cluster : 0;
    root->parent_cluster = 0;
    root->dir_entry_offset = 0;
    root->vfs.f_ops = &fat_dir_fops;
    root->vfs.i_ops = &fat_dir_iops;

    /* Build superblock */
    vfs_superblock_t* sb = (vfs_superblock_t*)kmalloc(sizeof(vfs_superblock_t));
    if (!sb) {
        kprintf("[FAT] Failed to allocate superblock\n");
        kfree(root);
        kfree(fm);
        return result;
    }
    memset(sb, 0, sizeof(*sb));
    sb->bdev = bdev;
    sb->lba = partition_lba;
    sb->private_data = fm;
    sb->root = &root->vfs;
    /* fstype will be set by caller */

    kprintf("[FAT] Mounted FAT%u at LBA %u (%u clusters)\n",
            (unsigned)fm->type, partition_lba, fm->total_clusters);

    result.root = &root->vfs;
    result.sb = sb;
    return result;
}

void fat_umount(struct fat_mount* fm) {
    if (fm) {
        kfree(fm);
    }
}
