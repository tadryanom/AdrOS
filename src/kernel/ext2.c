#include "ext2.h"
#include "ata_pio.h"
#include "heap.h"
#include "utils.h"
#include "console.h"
#include "errno.h"

#include <stddef.h>

/* ---- ext2 on-disk structures ---- */

#define EXT2_SUPER_MAGIC   0xEF53
#define EXT2_SUPER_OFFSET  1024   /* superblock is at byte offset 1024 */

#define EXT2_ROOT_INO      2

#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFLNK  0xA000

#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_SYMLINK  7

#define EXT2_NDIR_BLOCKS  12
#define EXT2_IND_BLOCK    12
#define EXT2_DIND_BLOCK   13
#define EXT2_TIND_BLOCK   14
#define EXT2_N_BLOCKS     15

struct ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    /* EXT2_DYNAMIC_REV fields */
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;
    /* ... more fields follow but we don't need them */
} __attribute__((packed));

struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t  bg_reserved[12];
} __attribute__((packed));

struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;       /* 512-byte blocks */
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[EXT2_N_BLOCKS];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;      /* i_size_high for regular files in rev1 */
    uint32_t i_faddr;
    uint8_t  i_osd2[12];
} __attribute__((packed));

struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];          /* variable length, NOT null-terminated */
} __attribute__((packed));

/* ---- In-memory filesystem state ---- */

#define EXT2_SECTOR_SIZE 512

struct ext2_state {
    uint32_t part_lba;        /* partition start LBA */
    uint32_t block_size;      /* bytes per block (1024, 2048, or 4096) */
    uint32_t sectors_per_block;
    uint32_t inodes_per_group;
    uint32_t blocks_per_group;
    uint32_t inode_size;      /* on-disk inode size (128 or 256) */
    uint32_t num_groups;
    uint32_t first_data_block;
    uint32_t total_blocks;
    uint32_t total_inodes;
    struct ext2_group_desc* gdt; /* group descriptor table (heap-allocated) */
    uint32_t gdt_blocks;      /* number of blocks occupied by GDT */
};

struct ext2_node {
    fs_node_t vfs;
    uint32_t ino;             /* inode number */
};

static struct ext2_state g_ext2;
static struct ext2_node g_ext2_root;
static int g_ext2_ready = 0;

/* ---- Block I/O ---- */

static int ext2_read_block(uint32_t block, void* buf) {
    uint32_t lba = g_ext2.part_lba + block * g_ext2.sectors_per_block;
    uint8_t* p = (uint8_t*)buf;
    for (uint32_t s = 0; s < g_ext2.sectors_per_block; s++) {
        if (ata_pio_read28(lba + s, p + s * EXT2_SECTOR_SIZE) < 0)
            return -EIO;
    }
    return 0;
}

static int ext2_write_block(uint32_t block, const void* buf) {
    uint32_t lba = g_ext2.part_lba + block * g_ext2.sectors_per_block;
    const uint8_t* p = (const uint8_t*)buf;
    for (uint32_t s = 0; s < g_ext2.sectors_per_block; s++) {
        if (ata_pio_write28(lba + s, p + s * EXT2_SECTOR_SIZE) < 0)
            return -EIO;
    }
    return 0;
}

/* ---- Superblock I/O ---- */

static int ext2_read_superblock(struct ext2_superblock* sb) {
    /* Superblock is at byte offset 1024, which is LBA 2-3 relative to partition */
    uint8_t sec[EXT2_SECTOR_SIZE];
    uint32_t sb_lba = g_ext2.part_lba + EXT2_SUPER_OFFSET / EXT2_SECTOR_SIZE;

    uint8_t raw[1024];
    for (uint32_t i = 0; i < 1024 / EXT2_SECTOR_SIZE; i++) {
        if (ata_pio_read28(sb_lba + i, sec) < 0) return -EIO;
        memcpy(raw + i * EXT2_SECTOR_SIZE, sec, EXT2_SECTOR_SIZE);
    }
    memcpy(sb, raw, sizeof(*sb));
    return 0;
}

static int ext2_write_superblock(const struct ext2_superblock* sb) __attribute__((unused));
static int ext2_write_superblock(const struct ext2_superblock* sb) {
    uint32_t sb_lba = g_ext2.part_lba + EXT2_SUPER_OFFSET / EXT2_SECTOR_SIZE;
    uint8_t raw[1024];
    memset(raw, 0, sizeof(raw));
    memcpy(raw, sb, sizeof(*sb));

    for (uint32_t i = 0; i < 1024 / EXT2_SECTOR_SIZE; i++) {
        if (ata_pio_write28(sb_lba + i, raw + i * EXT2_SECTOR_SIZE) < 0)
            return -EIO;
    }
    return 0;
}

/* ---- GDT I/O ---- */

static int ext2_write_gdt(void) {
    /* GDT starts at block after superblock (block 1 for 1KB blocks, block 1 for others too
     * since superblock is in block 0 or 1 depending on block_size) */
    uint32_t gdt_block = g_ext2.first_data_block + 1;
    uint8_t* p = (uint8_t*)g_ext2.gdt;

    for (uint32_t b = 0; b < g_ext2.gdt_blocks; b++) {
        uint8_t blk_buf[4096]; /* max block size */
        memset(blk_buf, 0, g_ext2.block_size);
        uint32_t bytes = g_ext2.block_size;
        uint32_t remain = g_ext2.num_groups * (uint32_t)sizeof(struct ext2_group_desc) - b * g_ext2.block_size;
        if (bytes > remain) bytes = remain;
        memcpy(blk_buf, p + b * g_ext2.block_size, bytes);
        if (ext2_write_block(gdt_block + b, blk_buf) < 0) return -EIO;
    }
    return 0;
}

/* ---- Inode I/O ---- */

static int ext2_read_inode(uint32_t ino, struct ext2_inode* out) {
    if (ino == 0 || !out) return -EINVAL;
    uint32_t group = (ino - 1) / g_ext2.inodes_per_group;
    uint32_t index = (ino - 1) % g_ext2.inodes_per_group;

    if (group >= g_ext2.num_groups) return -EINVAL;

    uint32_t inode_table_block = g_ext2.gdt[group].bg_inode_table;
    uint32_t byte_offset = index * g_ext2.inode_size;
    uint32_t block = inode_table_block + byte_offset / g_ext2.block_size;
    uint32_t offset_in_block = byte_offset % g_ext2.block_size;

    uint8_t blk_buf[4096];
    if (ext2_read_block(block, blk_buf) < 0) return -EIO;
    memcpy(out, blk_buf + offset_in_block, sizeof(*out));
    return 0;
}

static int ext2_write_inode(uint32_t ino, const struct ext2_inode* in) {
    if (ino == 0 || !in) return -EINVAL;
    uint32_t group = (ino - 1) / g_ext2.inodes_per_group;
    uint32_t index = (ino - 1) % g_ext2.inodes_per_group;

    if (group >= g_ext2.num_groups) return -EINVAL;

    uint32_t inode_table_block = g_ext2.gdt[group].bg_inode_table;
    uint32_t byte_offset = index * g_ext2.inode_size;
    uint32_t block = inode_table_block + byte_offset / g_ext2.block_size;
    uint32_t offset_in_block = byte_offset % g_ext2.block_size;

    uint8_t blk_buf[4096];
    if (ext2_read_block(block, blk_buf) < 0) return -EIO;
    memcpy(blk_buf + offset_in_block, in, sizeof(*in));
    return ext2_write_block(block, blk_buf);
}

/* ---- Block mapping: logical block → physical block ---- */

/* Resolve logical block number within an inode to physical block number.
 * Handles direct, indirect, doubly-indirect, and triply-indirect blocks. */
static uint32_t ext2_block_map(const struct ext2_inode* inode, uint32_t logical) {
    uint32_t ptrs_per_block = g_ext2.block_size / 4;

    /* Direct blocks (0..11) */
    if (logical < EXT2_NDIR_BLOCKS) {
        return inode->i_block[logical];
    }
    logical -= EXT2_NDIR_BLOCKS;

    /* Singly indirect (12..12+ptrs-1) */
    if (logical < ptrs_per_block) {
        uint32_t ind_block = inode->i_block[EXT2_IND_BLOCK];
        if (ind_block == 0) return 0;
        uint8_t blk_buf[4096];
        if (ext2_read_block(ind_block, blk_buf) < 0) return 0;
        return ((uint32_t*)blk_buf)[logical];
    }
    logical -= ptrs_per_block;

    /* Doubly indirect */
    if (logical < ptrs_per_block * ptrs_per_block) {
        uint32_t dind_block = inode->i_block[EXT2_DIND_BLOCK];
        if (dind_block == 0) return 0;
        uint8_t blk_buf[4096];
        memset(blk_buf, 0, sizeof(blk_buf));
        if (ext2_read_block(dind_block, blk_buf) < 0) return 0;
        uint32_t ind = ((uint32_t*)blk_buf)[logical / ptrs_per_block];
        if (ind == 0) return 0;
        if (ext2_read_block(ind, blk_buf) < 0) return 0;
        return ((uint32_t*)blk_buf)[logical % ptrs_per_block];
    }
    logical -= ptrs_per_block * ptrs_per_block;

    /* Triply indirect */
    {
        uint32_t tind_block = inode->i_block[EXT2_TIND_BLOCK];
        if (tind_block == 0) return 0;
        uint8_t blk_buf[4096];
        memset(blk_buf, 0, sizeof(blk_buf));
        if (ext2_read_block(tind_block, blk_buf) < 0) return 0;
        uint32_t dind = ((uint32_t*)blk_buf)[logical / (ptrs_per_block * ptrs_per_block)];
        if (dind == 0) return 0;
        uint32_t rem = logical % (ptrs_per_block * ptrs_per_block);
        if (ext2_read_block(dind, blk_buf) < 0) return 0;
        uint32_t ind = ((uint32_t*)blk_buf)[rem / ptrs_per_block];
        if (ind == 0) return 0;
        if (ext2_read_block(ind, blk_buf) < 0) return 0;
        return ((uint32_t*)blk_buf)[rem % ptrs_per_block];
    }
}

/* ---- Bitmap helpers (for RW) ---- */

/* Allocate a free block from group, returns block number or 0. */
static uint32_t ext2_alloc_block(void) {
    for (uint32_t g = 0; g < g_ext2.num_groups; g++) {
        if (g_ext2.gdt[g].bg_free_blocks_count == 0) continue;

        uint8_t bmap[4096];
        memset(bmap, 0, sizeof(bmap));
        if (ext2_read_block(g_ext2.gdt[g].bg_block_bitmap, bmap) < 0) continue;

        uint32_t blocks_in_group = g_ext2.blocks_per_group;
        if (g == g_ext2.num_groups - 1) {
            uint32_t rem = g_ext2.total_blocks - g * g_ext2.blocks_per_group;
            if (rem < blocks_in_group) blocks_in_group = rem;
        }

        for (uint32_t bit = 0; bit < blocks_in_group; bit++) {
            if ((bmap[bit / 8] & (1 << (bit % 8))) == 0) {
                bmap[bit / 8] |= (1 << (bit % 8));
                if (ext2_write_block(g_ext2.gdt[g].bg_block_bitmap, bmap) < 0) return 0;
                g_ext2.gdt[g].bg_free_blocks_count--;
                (void)ext2_write_gdt();
                return g * g_ext2.blocks_per_group + bit + g_ext2.first_data_block;
            }
        }
    }
    return 0;
}

static void ext2_free_block(uint32_t block) {
    if (block == 0) return;
    uint32_t adj = block - g_ext2.first_data_block;
    uint32_t g = adj / g_ext2.blocks_per_group;
    uint32_t bit = adj % g_ext2.blocks_per_group;

    if (g >= g_ext2.num_groups) return;

    uint8_t bmap[4096];
    memset(bmap, 0, sizeof(bmap));
    if (ext2_read_block(g_ext2.gdt[g].bg_block_bitmap, bmap) < 0) return;
    bmap[bit / 8] &= ~(1 << (bit % 8));
    (void)ext2_write_block(g_ext2.gdt[g].bg_block_bitmap, bmap);
    g_ext2.gdt[g].bg_free_blocks_count++;
    (void)ext2_write_gdt();
}

/* Allocate a free inode, returns inode number or 0. */
static uint32_t ext2_alloc_inode(void) {
    for (uint32_t g = 0; g < g_ext2.num_groups; g++) {
        if (g_ext2.gdt[g].bg_free_inodes_count == 0) continue;

        uint8_t bmap[4096];
        memset(bmap, 0, sizeof(bmap));
        if (ext2_read_block(g_ext2.gdt[g].bg_inode_bitmap, bmap) < 0) continue;

        for (uint32_t bit = 0; bit < g_ext2.inodes_per_group; bit++) {
            if ((bmap[bit / 8] & (1 << (bit % 8))) == 0) {
                bmap[bit / 8] |= (1 << (bit % 8));
                if (ext2_write_block(g_ext2.gdt[g].bg_inode_bitmap, bmap) < 0) return 0;
                g_ext2.gdt[g].bg_free_inodes_count--;
                (void)ext2_write_gdt();
                return g * g_ext2.inodes_per_group + bit + 1;
            }
        }
    }
    return 0;
}

static void ext2_free_inode(uint32_t ino) {
    if (ino == 0) return;
    uint32_t g = (ino - 1) / g_ext2.inodes_per_group;
    uint32_t bit = (ino - 1) % g_ext2.inodes_per_group;

    if (g >= g_ext2.num_groups) return;

    uint8_t bmap[4096];
    memset(bmap, 0, sizeof(bmap));
    if (ext2_read_block(g_ext2.gdt[g].bg_inode_bitmap, bmap) < 0) return;
    bmap[bit / 8] &= ~(1 << (bit % 8));
    (void)ext2_write_block(g_ext2.gdt[g].bg_inode_bitmap, bmap);
    g_ext2.gdt[g].bg_free_inodes_count++;
    (void)ext2_write_gdt();
}

/* ---- Block mapping write: set logical→physical mapping in inode ---- */

/* Allocate an indirect block if val is zero. Returns the block number (existing or new), or 0 on failure. */
static uint32_t ext2_ensure_indirect(uint32_t val) {
    if (val != 0) return val;
    uint32_t nb = ext2_alloc_block();
    if (nb == 0) return 0;
    /* Zero out the new indirect block */
    uint8_t zero[4096];
    memset(zero, 0, g_ext2.block_size);
    if (ext2_write_block(nb, zero) < 0) {
        ext2_free_block(nb);
        return 0;
    }
    return nb;
}

/* Set the physical block for a logical block in an inode.
 * Allocates indirect blocks as needed. Writes inode back to disk. */
static int ext2_block_map_set(uint32_t ino, struct ext2_inode* inode,
                               uint32_t logical, uint32_t phys_block) {
    uint32_t ptrs_per_block = g_ext2.block_size / 4;

    if (logical < EXT2_NDIR_BLOCKS) {
        inode->i_block[logical] = phys_block;
        return ext2_write_inode(ino, inode);
    }
    logical -= EXT2_NDIR_BLOCKS;

    if (logical < ptrs_per_block) {
        uint32_t ind_blk = ext2_ensure_indirect(inode->i_block[EXT2_IND_BLOCK]);
        if (ind_blk == 0) return -ENOSPC;
        inode->i_block[EXT2_IND_BLOCK] = ind_blk;
        if (ext2_write_inode(ino, inode) < 0) return -EIO;

        uint8_t blk_buf[4096];
        if (ext2_read_block(inode->i_block[EXT2_IND_BLOCK], blk_buf) < 0) return -EIO;
        ((uint32_t*)blk_buf)[logical] = phys_block;
        return ext2_write_block(inode->i_block[EXT2_IND_BLOCK], blk_buf);
    }
    logical -= ptrs_per_block;

    if (logical < ptrs_per_block * ptrs_per_block) {
        uint32_t dind_blk = ext2_ensure_indirect(inode->i_block[EXT2_DIND_BLOCK]);
        if (dind_blk == 0) return -ENOSPC;
        inode->i_block[EXT2_DIND_BLOCK] = dind_blk;
        if (ext2_write_inode(ino, inode) < 0) return -EIO;

        uint8_t blk_buf[4096];
        if (ext2_read_block(inode->i_block[EXT2_DIND_BLOCK], blk_buf) < 0) return -EIO;
        uint32_t idx1 = logical / ptrs_per_block;
        uint32_t idx2 = logical % ptrs_per_block;
        uint32_t ind = ((uint32_t*)blk_buf)[idx1];
        if (ind == 0) {
            ind = ext2_alloc_block();
            if (ind == 0) return -ENOSPC;
            uint8_t zero[4096];
            memset(zero, 0, g_ext2.block_size);
            if (ext2_write_block(ind, zero) < 0) { ext2_free_block(ind); return -EIO; }
            ((uint32_t*)blk_buf)[idx1] = ind;
            if (ext2_write_block(inode->i_block[EXT2_DIND_BLOCK], blk_buf) < 0) return -EIO;
        }

        if (ext2_read_block(ind, blk_buf) < 0) return -EIO;
        ((uint32_t*)blk_buf)[idx2] = phys_block;
        return ext2_write_block(ind, blk_buf);
    }

    /* Triply indirect — not implemented for now */
    return -ENOSPC;
}

/* Free all data blocks referenced by an inode (direct + indirect). */
static void ext2_free_inode_blocks(struct ext2_inode* inode) {
    uint32_t ptrs_per_block = g_ext2.block_size / 4;

    /* Direct */
    for (uint32_t i = 0; i < EXT2_NDIR_BLOCKS; i++) {
        if (inode->i_block[i]) {
            ext2_free_block(inode->i_block[i]);
            inode->i_block[i] = 0;
        }
    }

    /* Singly indirect */
    if (inode->i_block[EXT2_IND_BLOCK]) {
        uint8_t blk_buf[4096];
        if (ext2_read_block(inode->i_block[EXT2_IND_BLOCK], blk_buf) == 0) {
            uint32_t* ptrs = (uint32_t*)blk_buf;
            for (uint32_t i = 0; i < ptrs_per_block; i++) {
                if (ptrs[i]) ext2_free_block(ptrs[i]);
            }
        }
        ext2_free_block(inode->i_block[EXT2_IND_BLOCK]);
        inode->i_block[EXT2_IND_BLOCK] = 0;
    }

    /* Doubly indirect */
    if (inode->i_block[EXT2_DIND_BLOCK]) {
        uint8_t blk_buf[4096];
        if (ext2_read_block(inode->i_block[EXT2_DIND_BLOCK], blk_buf) == 0) {
            uint32_t* l1 = (uint32_t*)blk_buf;
            for (uint32_t i = 0; i < ptrs_per_block; i++) {
                if (l1[i]) {
                    uint8_t blk2[4096];
                    if (ext2_read_block(l1[i], blk2) == 0) {
                        uint32_t* l2 = (uint32_t*)blk2;
                        for (uint32_t j = 0; j < ptrs_per_block; j++) {
                            if (l2[j]) ext2_free_block(l2[j]);
                        }
                    }
                    ext2_free_block(l1[i]);
                }
            }
        }
        ext2_free_block(inode->i_block[EXT2_DIND_BLOCK]);
        inode->i_block[EXT2_DIND_BLOCK] = 0;
    }

    /* Triply indirect — free top level only for safety */
    if (inode->i_block[EXT2_TIND_BLOCK]) {
        ext2_free_block(inode->i_block[EXT2_TIND_BLOCK]);
        inode->i_block[EXT2_TIND_BLOCK] = 0;
    }

    inode->i_blocks = 0;
    inode->i_size = 0;
}

/* ---- Forward declarations ---- */
static fs_node_t* ext2_finddir(fs_node_t* node, const char* name);
static uint32_t ext2_file_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static uint32_t ext2_file_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
static int ext2_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len);
static int ext2_create_impl(struct fs_node* dir, const char* name, uint32_t flags, struct fs_node** out);
static int ext2_mkdir_impl(struct fs_node* dir, const char* name);
static int ext2_unlink_impl(struct fs_node* dir, const char* name);
static int ext2_rmdir_impl(struct fs_node* dir, const char* name);
static int ext2_rename_impl(struct fs_node* old_dir, const char* old_name,
                             struct fs_node* new_dir, const char* new_name);
static int ext2_truncate_impl(struct fs_node* node, uint32_t length);
static int ext2_link_impl(struct fs_node* dir, const char* name, struct fs_node* target);

static void ext2_close_impl(fs_node_t* node) {
    if (!node) return;
    struct ext2_node* en = (struct ext2_node*)node;
    kfree(en);
}

static void ext2_set_dir_ops(fs_node_t* vfs) {
    if (!vfs) return;
    vfs->finddir = &ext2_finddir;
    vfs->readdir = &ext2_readdir_impl;
    vfs->create = &ext2_create_impl;
    vfs->mkdir = &ext2_mkdir_impl;
    vfs->unlink = &ext2_unlink_impl;
    vfs->rmdir = &ext2_rmdir_impl;
    vfs->rename = &ext2_rename_impl;
    vfs->link = &ext2_link_impl;
}

static struct ext2_node* ext2_make_node(uint32_t ino, const struct ext2_inode* inode, const char* name) {
    struct ext2_node* en = (struct ext2_node*)kmalloc(sizeof(struct ext2_node));
    if (!en) return NULL;
    memset(en, 0, sizeof(*en));

    en->ino = ino;

    size_t nlen = strlen(name);
    if (nlen >= sizeof(en->vfs.name)) nlen = sizeof(en->vfs.name) - 1;
    memcpy(en->vfs.name, name, nlen);
    en->vfs.name[nlen] = '\0';
    en->vfs.inode = ino;
    en->vfs.uid = inode->i_uid;
    en->vfs.gid = inode->i_gid;
    en->vfs.mode = inode->i_mode;
    en->vfs.close = &ext2_close_impl;

    if ((inode->i_mode & 0xF000) == EXT2_S_IFDIR) {
        en->vfs.flags = FS_DIRECTORY;
        en->vfs.length = inode->i_size;
        ext2_set_dir_ops(&en->vfs);
    } else if ((inode->i_mode & 0xF000) == EXT2_S_IFLNK) {
        en->vfs.flags = FS_SYMLINK;
        en->vfs.length = inode->i_size;
        /* For small symlinks, target is stored inline in i_block */
        if (inode->i_size < sizeof(inode->i_block)) {
            memcpy(en->vfs.symlink_target, inode->i_block, inode->i_size);
            en->vfs.symlink_target[inode->i_size] = '\0';
        }
    } else {
        en->vfs.flags = FS_FILE;
        en->vfs.length = inode->i_size;
        en->vfs.read = &ext2_file_read;
        en->vfs.write = &ext2_file_write;
        en->vfs.truncate = &ext2_truncate_impl;
    }

    return en;
}

/* ---- File read ---- */

static uint32_t ext2_file_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) return 0;
    struct ext2_node* en = (struct ext2_node*)node;

    struct ext2_inode inode;
    if (ext2_read_inode(en->ino, &inode) < 0) return 0;

    uint32_t file_size = inode.i_size;
    if (offset >= file_size) return 0;
    if (offset + size > file_size) size = file_size - offset;
    if (size == 0) return 0;

    uint32_t bs = g_ext2.block_size;
    uint32_t total = 0;

    while (total < size) {
        uint32_t pos = offset + total;
        uint32_t logical_block = pos / bs;
        uint32_t offset_in_block = pos % bs;
        uint32_t chunk = bs - offset_in_block;
        if (chunk > size - total) chunk = size - total;

        uint32_t phys_block = ext2_block_map(&inode, logical_block);
        if (phys_block == 0) break;

        uint8_t blk_buf[4096];
        if (ext2_read_block(phys_block, blk_buf) < 0) break;
        memcpy(buffer + total, blk_buf + offset_in_block, chunk);
        total += chunk;
    }

    return total;
}

/* ---- File write ---- */

static uint32_t ext2_file_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !buffer || size == 0) return 0;
    struct ext2_node* en = (struct ext2_node*)node;

    struct ext2_inode inode;
    if (ext2_read_inode(en->ino, &inode) < 0) return 0;

    uint64_t end64 = (uint64_t)offset + (uint64_t)size;
    if (end64 > 0xFFFFFFFFULL) return 0;

    uint32_t bs = g_ext2.block_size;
    uint32_t total = 0;

    while (total < size) {
        uint32_t pos = offset + total;
        uint32_t logical_block = pos / bs;
        uint32_t offset_in_block = pos % bs;
        uint32_t chunk = bs - offset_in_block;
        if (chunk > size - total) chunk = size - total;

        uint32_t phys_block = ext2_block_map(&inode, logical_block);
        if (phys_block == 0) {
            /* Need to allocate a new block */
            phys_block = ext2_alloc_block();
            if (phys_block == 0) break;
            if (ext2_block_map_set(en->ino, &inode, logical_block, phys_block) < 0) {
                ext2_free_block(phys_block);
                break;
            }
            inode.i_blocks += g_ext2.block_size / EXT2_SECTOR_SIZE;
        }

        uint8_t blk_buf[4096];
        if (offset_in_block != 0 || chunk != bs) {
            if (ext2_read_block(phys_block, blk_buf) < 0) break;
        }
        memcpy(blk_buf + offset_in_block, buffer + total, chunk);
        if (ext2_write_block(phys_block, blk_buf) < 0) break;
        total += chunk;
    }

    if (offset + total > inode.i_size) {
        inode.i_size = offset + total;
    }
    (void)ext2_write_inode(en->ino, &inode);
    node->length = inode.i_size;

    return total;
}

/* ---- finddir ---- */

static fs_node_t* ext2_finddir(fs_node_t* node, const char* name) {
    if (!node || !name) return NULL;
    struct ext2_node* en = (struct ext2_node*)node;

    struct ext2_inode dir_inode;
    if (ext2_read_inode(en->ino, &dir_inode) < 0) return NULL;
    if ((dir_inode.i_mode & 0xF000) != EXT2_S_IFDIR) return NULL;

    uint32_t dir_size = dir_inode.i_size;
    uint32_t bs = g_ext2.block_size;
    uint32_t name_len = strlen(name);

    for (uint32_t pos = 0; pos < dir_size; ) {
        uint32_t logical = pos / bs;
        uint32_t phys = ext2_block_map(&dir_inode, logical);
        if (phys == 0) break;

        uint8_t blk_buf[4096];
        if (ext2_read_block(phys, blk_buf) < 0) break;

        uint32_t off = pos % bs;
        while (off < bs && pos < dir_size) {
            struct ext2_dir_entry* de = (struct ext2_dir_entry*)(blk_buf + off);
            if (de->rec_len == 0) goto done;

            if (de->inode != 0 && de->name_len == name_len) {
                if (memcmp(de->name, name, name_len) == 0) {
                    struct ext2_inode child_inode;
                    if (ext2_read_inode(de->inode, &child_inode) < 0) return NULL;
                    char child_name[128];
                    if (name_len >= sizeof(child_name)) name_len = sizeof(child_name) - 1;
                    memcpy(child_name, name, name_len);
                    child_name[name_len] = '\0';
                    return (fs_node_t*)ext2_make_node(de->inode, &child_inode, child_name);
                }
            }

            off += de->rec_len;
            pos = (pos / bs) * bs + off;
        }
        if (off >= bs) {
            pos = ((pos / bs) + 1) * bs;
        }
    }

done:
    return NULL;
}

/* ---- readdir ---- */

static int ext2_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    if (!node || !inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    struct ext2_node* en = (struct ext2_node*)node;
    struct ext2_inode dir_inode;
    if (ext2_read_inode(en->ino, &dir_inode) < 0) return -1;

    uint32_t dir_size = dir_inode.i_size;
    uint32_t bs = g_ext2.block_size;
    uint32_t idx = *inout_index;
    uint32_t cap = buf_len / (uint32_t)sizeof(struct vfs_dirent);
    struct vfs_dirent* out = (struct vfs_dirent*)buf;
    uint32_t written = 0;
    uint32_t cur = 0;

    for (uint32_t pos = 0; pos < dir_size && written < cap; ) {
        uint32_t logical = pos / bs;
        uint32_t phys = ext2_block_map(&dir_inode, logical);
        if (phys == 0) break;

        uint8_t blk_buf[4096];
        if (ext2_read_block(phys, blk_buf) < 0) break;

        uint32_t off = pos % bs;
        while (off < bs && pos < dir_size && written < cap) {
            struct ext2_dir_entry* de = (struct ext2_dir_entry*)(blk_buf + off);
            if (de->rec_len == 0) goto done;

            if (de->inode != 0) {
                /* Skip . and .. */
                int skip = 0;
                if (de->name_len == 1 && de->name[0] == '.') skip = 1;
                if (de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.') skip = 1;

                if (!skip) {
                    if (cur >= idx) {
                        memset(&out[written], 0, sizeof(out[written]));
                        out[written].d_ino = de->inode;
                        out[written].d_reclen = (uint16_t)sizeof(struct vfs_dirent);
                        out[written].d_type = de->file_type;
                        uint8_t nlen = de->name_len;
                        if (nlen >= sizeof(out[written].d_name))
                            nlen = sizeof(out[written].d_name) - 1;
                        memcpy(out[written].d_name, de->name, nlen);
                        out[written].d_name[nlen] = '\0';
                        written++;
                    }
                    cur++;
                }
            }

            off += de->rec_len;
            pos = (pos / bs) * bs + off;
        }
        if (off >= bs) {
            pos = ((pos / bs) + 1) * bs;
        }
    }

done:
    *inout_index = cur;
    return (int)(written * (uint32_t)sizeof(struct vfs_dirent));
}

/* ---- Directory entry add/remove helpers ---- */

/* Add a directory entry (name → ino) to a directory inode. */
static int ext2_dir_add_entry(uint32_t dir_ino, const char* name, uint32_t child_ino, uint8_t file_type) {
    struct ext2_inode dir_inode;
    if (ext2_read_inode(dir_ino, &dir_inode) < 0) return -EIO;

    uint32_t bs = g_ext2.block_size;
    uint32_t dir_size = dir_inode.i_size;
    uint32_t name_len = strlen(name);
    uint32_t needed = ((uint32_t)sizeof(struct ext2_dir_entry) + name_len + 3) & ~3U; /* 4-byte aligned */

    /* Scan existing blocks for space */
    for (uint32_t pos = 0; pos < dir_size; ) {
        uint32_t logical = pos / bs;
        uint32_t phys = ext2_block_map(&dir_inode, logical);
        if (phys == 0) break;

        uint8_t blk_buf[4096];
        if (ext2_read_block(phys, blk_buf) < 0) return -EIO;

        uint32_t off = 0;
        while (off < bs) {
            struct ext2_dir_entry* de = (struct ext2_dir_entry*)(blk_buf + off);
            if (de->rec_len == 0) break;

            uint32_t actual_len = ((uint32_t)sizeof(struct ext2_dir_entry) + de->name_len + 3) & ~3U;
            uint32_t free_space = de->rec_len - actual_len;

            if (de->inode == 0 && de->rec_len >= needed) {
                /* Reuse deleted entry */
                de->inode = child_ino;
                de->name_len = (uint8_t)name_len;
                de->file_type = file_type;
                memcpy(de->name, name, name_len);
                return ext2_write_block(phys, blk_buf);
            }

            if (free_space >= needed) {
                /* Split entry */
                de->rec_len = (uint16_t)actual_len;
                struct ext2_dir_entry* new_de = (struct ext2_dir_entry*)(blk_buf + off + actual_len);
                new_de->inode = child_ino;
                new_de->rec_len = (uint16_t)free_space;
                new_de->name_len = (uint8_t)name_len;
                new_de->file_type = file_type;
                memcpy(new_de->name, name, name_len);
                return ext2_write_block(phys, blk_buf);
            }

            off += de->rec_len;
        }
        pos = ((pos / bs) + 1) * bs;
    }

    /* Need a new block for the directory */
    uint32_t new_block = ext2_alloc_block();
    if (new_block == 0) return -ENOSPC;

    uint32_t new_logical = dir_size / bs;
    if (ext2_block_map_set(dir_ino, &dir_inode, new_logical, new_block) < 0) {
        ext2_free_block(new_block);
        return -EIO;
    }
    dir_inode.i_size += bs;
    dir_inode.i_blocks += bs / EXT2_SECTOR_SIZE;
    (void)ext2_write_inode(dir_ino, &dir_inode);

    uint8_t blk_buf[4096];
    memset(blk_buf, 0, bs);
    struct ext2_dir_entry* de = (struct ext2_dir_entry*)blk_buf;
    de->inode = child_ino;
    de->rec_len = (uint16_t)bs;
    de->name_len = (uint8_t)name_len;
    de->file_type = file_type;
    memcpy(de->name, name, name_len);
    return ext2_write_block(new_block, blk_buf);
}

/* Remove a directory entry by name. Returns the removed inode number. */
static int ext2_dir_remove_entry(uint32_t dir_ino, const char* name, uint32_t* removed_ino) {
    struct ext2_inode dir_inode;
    if (ext2_read_inode(dir_ino, &dir_inode) < 0) return -EIO;

    uint32_t bs = g_ext2.block_size;
    uint32_t dir_size = dir_inode.i_size;
    uint32_t name_len = strlen(name);

    for (uint32_t pos = 0; pos < dir_size; ) {
        uint32_t logical = pos / bs;
        uint32_t phys = ext2_block_map(&dir_inode, logical);
        if (phys == 0) break;

        uint8_t blk_buf[4096];
        if (ext2_read_block(phys, blk_buf) < 0) return -EIO;

        uint32_t off = 0;
        uint32_t prev_off = 0;
        int is_first = 1;

        while (off < bs) {
            struct ext2_dir_entry* de = (struct ext2_dir_entry*)(blk_buf + off);
            if (de->rec_len == 0) break;

            if (de->inode != 0 && de->name_len == name_len &&
                memcmp(de->name, name, name_len) == 0) {
                if (removed_ino) *removed_ino = de->inode;

                if (is_first) {
                    /* First entry in block: just zero inode */
                    de->inode = 0;
                } else {
                    /* Merge with previous entry */
                    struct ext2_dir_entry* prev = (struct ext2_dir_entry*)(blk_buf + prev_off);
                    prev->rec_len += de->rec_len;
                }
                return ext2_write_block(phys, blk_buf);
            }

            prev_off = off;
            off += de->rec_len;
            is_first = 0;
        }
        pos = ((pos / bs) + 1) * bs;
    }
    return -ENOENT;
}

/* Find a directory entry by name, return its inode number. */
static int ext2_dir_find(uint32_t dir_ino, const char* name, uint32_t* out_ino) {
    struct ext2_inode dir_inode;
    if (ext2_read_inode(dir_ino, &dir_inode) < 0) return -EIO;

    uint32_t bs = g_ext2.block_size;
    uint32_t dir_size = dir_inode.i_size;
    uint32_t name_len = strlen(name);

    for (uint32_t pos = 0; pos < dir_size; ) {
        uint32_t logical = pos / bs;
        uint32_t phys = ext2_block_map(&dir_inode, logical);
        if (phys == 0) break;

        uint8_t blk_buf[4096];
        if (ext2_read_block(phys, blk_buf) < 0) return -EIO;

        uint32_t off = pos % bs;
        while (off < bs) {
            struct ext2_dir_entry* de = (struct ext2_dir_entry*)(blk_buf + off);
            if (de->rec_len == 0) goto not_found;

            if (de->inode != 0 && de->name_len == name_len &&
                memcmp(de->name, name, name_len) == 0) {
                if (out_ino) *out_ino = de->inode;
                return 0;
            }

            off += de->rec_len;
        }
        pos = ((pos / bs) + 1) * bs;
    }

not_found:
    return -ENOENT;
}

/* Check if a directory is empty (only . and .. entries). */
static int ext2_dir_is_empty(uint32_t dir_ino) {
    struct ext2_inode dir_inode;
    if (ext2_read_inode(dir_ino, &dir_inode) < 0) return 0;

    uint32_t bs = g_ext2.block_size;
    uint32_t dir_size = dir_inode.i_size;

    for (uint32_t pos = 0; pos < dir_size; ) {
        uint32_t logical = pos / bs;
        uint32_t phys = ext2_block_map(&dir_inode, logical);
        if (phys == 0) break;

        uint8_t blk_buf[4096];
        if (ext2_read_block(phys, blk_buf) < 0) return 0;

        uint32_t off = 0;
        while (off < bs) {
            struct ext2_dir_entry* de = (struct ext2_dir_entry*)(blk_buf + off);
            if (de->rec_len == 0) return 1;

            if (de->inode != 0) {
                int is_dot = (de->name_len == 1 && de->name[0] == '.');
                int is_dotdot = (de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.');
                if (!is_dot && !is_dotdot) return 0;
            }

            off += de->rec_len;
        }
        pos = ((pos / bs) + 1) * bs;
    }
    return 1;
}

/* ---- VFS: create ---- */

static int ext2_create_impl(struct fs_node* dir, const char* name, uint32_t flags, struct fs_node** out) {
    if (!dir || !name || !out) return -EINVAL;
    *out = NULL;
    struct ext2_node* parent = (struct ext2_node*)dir;

    /* Check if exists */
    uint32_t existing_ino;
    int rc = ext2_dir_find(parent->ino, name, &existing_ino);
    if (rc == 0) {
        struct ext2_inode existing;
        if (ext2_read_inode(existing_ino, &existing) < 0) return -EIO;
        if ((existing.i_mode & 0xF000) == EXT2_S_IFDIR) return -EISDIR;

        if ((flags & 0x200U) != 0U) { /* O_TRUNC */
            ext2_free_inode_blocks(&existing);
            existing.i_size = 0;
            existing.i_blocks = 0;
            (void)ext2_write_inode(existing_ino, &existing);
        }

        struct ext2_node* en = ext2_make_node(existing_ino, &existing, name);
        if (!en) return -ENOMEM;
        *out = &en->vfs;
        return 0;
    }

    if ((flags & 0x40U) == 0U) return -ENOENT; /* O_CREAT not set */

    /* Allocate new inode */
    uint32_t new_ino = ext2_alloc_inode();
    if (new_ino == 0) return -ENOSPC;

    struct ext2_inode new_inode;
    memset(&new_inode, 0, sizeof(new_inode));
    new_inode.i_mode = EXT2_S_IFREG | 0644;
    new_inode.i_links_count = 1;
    if (ext2_write_inode(new_ino, &new_inode) < 0) {
        ext2_free_inode(new_ino);
        return -EIO;
    }

    rc = ext2_dir_add_entry(parent->ino, name, new_ino, EXT2_FT_REG_FILE);
    if (rc < 0) {
        ext2_free_inode(new_ino);
        return rc;
    }

    struct ext2_node* en = ext2_make_node(new_ino, &new_inode, name);
    if (!en) return -ENOMEM;
    *out = &en->vfs;
    return 0;
}

/* ---- VFS: mkdir ---- */

static int ext2_mkdir_impl(struct fs_node* dir, const char* name) {
    if (!dir || !name) return -EINVAL;
    struct ext2_node* parent = (struct ext2_node*)dir;

    if (ext2_dir_find(parent->ino, name, NULL) == 0) return -EEXIST;

    uint32_t new_ino = ext2_alloc_inode();
    if (new_ino == 0) return -ENOSPC;

    /* Allocate one block for . and .. */
    uint32_t new_block = ext2_alloc_block();
    if (new_block == 0) {
        ext2_free_inode(new_ino);
        return -ENOSPC;
    }

    struct ext2_inode new_inode;
    memset(&new_inode, 0, sizeof(new_inode));
    new_inode.i_mode = EXT2_S_IFDIR | 0755;
    new_inode.i_size = g_ext2.block_size;
    new_inode.i_links_count = 2; /* . and parent's entry */
    new_inode.i_blocks = g_ext2.block_size / EXT2_SECTOR_SIZE;
    new_inode.i_block[0] = new_block;
    if (ext2_write_inode(new_ino, &new_inode) < 0) {
        ext2_free_block(new_block);
        ext2_free_inode(new_ino);
        return -EIO;
    }

    /* Write . and .. entries */
    uint8_t blk_buf[4096];
    memset(blk_buf, 0, g_ext2.block_size);

    struct ext2_dir_entry* dot = (struct ext2_dir_entry*)blk_buf;
    dot->inode = new_ino;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';

    struct ext2_dir_entry* dotdot = (struct ext2_dir_entry*)(blk_buf + 12);
    dotdot->inode = parent->ino;
    dotdot->rec_len = (uint16_t)(g_ext2.block_size - 12);
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    if (ext2_write_block(new_block, blk_buf) < 0) {
        ext2_free_block(new_block);
        ext2_free_inode(new_ino);
        return -EIO;
    }

    /* Add entry in parent */
    int rc = ext2_dir_add_entry(parent->ino, name, new_ino, EXT2_FT_DIR);
    if (rc < 0) {
        ext2_free_block(new_block);
        ext2_free_inode(new_ino);
        return rc;
    }

    /* Increment parent link count (for ..) */
    struct ext2_inode parent_inode;
    if (ext2_read_inode(parent->ino, &parent_inode) == 0) {
        parent_inode.i_links_count++;
        (void)ext2_write_inode(parent->ino, &parent_inode);
    }

    /* Update group used_dirs_count */
    uint32_t g = (new_ino - 1) / g_ext2.inodes_per_group;
    if (g < g_ext2.num_groups) {
        g_ext2.gdt[g].bg_used_dirs_count++;
        (void)ext2_write_gdt();
    }

    return 0;
}

/* ---- VFS: unlink ---- */

static int ext2_unlink_impl(struct fs_node* dir, const char* name) {
    if (!dir || !name) return -EINVAL;
    struct ext2_node* parent = (struct ext2_node*)dir;

    uint32_t child_ino;
    int rc = ext2_dir_remove_entry(parent->ino, name, &child_ino);
    if (rc < 0) return rc;

    struct ext2_inode child;
    if (ext2_read_inode(child_ino, &child) < 0) return -EIO;
    if ((child.i_mode & 0xF000) == EXT2_S_IFDIR) return -EISDIR;

    child.i_links_count--;
    if (child.i_links_count == 0) {
        ext2_free_inode_blocks(&child);
        ext2_free_inode(child_ino);
    }
    return ext2_write_inode(child_ino, &child);
}

/* ---- VFS: rmdir ---- */

static int ext2_rmdir_impl(struct fs_node* dir, const char* name) {
    if (!dir || !name) return -EINVAL;
    struct ext2_node* parent = (struct ext2_node*)dir;

    uint32_t child_ino;
    int rc = ext2_dir_find(parent->ino, name, &child_ino);
    if (rc < 0) return rc;

    struct ext2_inode child;
    if (ext2_read_inode(child_ino, &child) < 0) return -EIO;
    if ((child.i_mode & 0xF000) != EXT2_S_IFDIR) return -ENOTDIR;
    if (!ext2_dir_is_empty(child_ino)) return -ENOTEMPTY;

    /* Remove entry from parent */
    rc = ext2_dir_remove_entry(parent->ino, name, NULL);
    if (rc < 0) return rc;

    /* Free directory blocks and inode */
    ext2_free_inode_blocks(&child);
    child.i_links_count = 0;
    (void)ext2_write_inode(child_ino, &child);
    ext2_free_inode(child_ino);

    /* Decrement parent link count (child's ".." pointed to parent) */
    struct ext2_inode parent_inode;
    if (ext2_read_inode(parent->ino, &parent_inode) == 0) {
        if (parent_inode.i_links_count > 0)
            parent_inode.i_links_count--;
        (void)ext2_write_inode(parent->ino, &parent_inode);
    }

    uint32_t g = (child_ino - 1) / g_ext2.inodes_per_group;
    if (g < g_ext2.num_groups && g_ext2.gdt[g].bg_used_dirs_count > 0) {
        g_ext2.gdt[g].bg_used_dirs_count--;
        (void)ext2_write_gdt();
    }

    return 0;
}

/* ---- VFS: rename ---- */

static int ext2_rename_impl(struct fs_node* old_dir, const char* old_name,
                             struct fs_node* new_dir, const char* new_name) {
    if (!old_dir || !old_name || !new_dir || !new_name) return -EINVAL;
    struct ext2_node* odir = (struct ext2_node*)old_dir;
    struct ext2_node* ndir = (struct ext2_node*)new_dir;

    /* Find source */
    uint32_t src_ino;
    int rc = ext2_dir_find(odir->ino, old_name, &src_ino);
    if (rc < 0) return rc;

    struct ext2_inode src_inode;
    if (ext2_read_inode(src_ino, &src_inode) < 0) return -EIO;

    uint8_t ft = ((src_inode.i_mode & 0xF000) == EXT2_S_IFDIR) ? EXT2_FT_DIR : EXT2_FT_REG_FILE;

    /* Remove destination if exists */
    uint32_t dst_ino;
    rc = ext2_dir_find(ndir->ino, new_name, &dst_ino);
    if (rc == 0 && dst_ino != src_ino) {
        struct ext2_inode dst_inode;
        if (ext2_read_inode(dst_ino, &dst_inode) == 0) {
            (void)ext2_dir_remove_entry(ndir->ino, new_name, NULL);
            dst_inode.i_links_count--;
            if (dst_inode.i_links_count == 0) {
                ext2_free_inode_blocks(&dst_inode);
                ext2_free_inode(dst_ino);
            }
            (void)ext2_write_inode(dst_ino, &dst_inode);
        }
    }

    /* Remove from old dir */
    (void)ext2_dir_remove_entry(odir->ino, old_name, NULL);

    /* Add to new dir */
    rc = ext2_dir_add_entry(ndir->ino, new_name, src_ino, ft);
    if (rc < 0) return rc;

    /* If moving a directory, update ".." to point to new parent */
    if (ft == EXT2_FT_DIR && odir->ino != ndir->ino) {
        /* Update ".." entry in moved dir */
        struct ext2_inode moved;
        if (ext2_read_inode(src_ino, &moved) == 0 && moved.i_block[0] != 0) {
            uint8_t blk_buf[4096];
            if (ext2_read_block(moved.i_block[0], blk_buf) == 0) {
                /* ".." is typically the second entry */
                struct ext2_dir_entry* dot = (struct ext2_dir_entry*)blk_buf;
                struct ext2_dir_entry* dotdot = (struct ext2_dir_entry*)(blk_buf + dot->rec_len);
                if (dotdot->name_len == 2 && dotdot->name[0] == '.' && dotdot->name[1] == '.') {
                    dotdot->inode = ndir->ino;
                    (void)ext2_write_block(moved.i_block[0], blk_buf);
                }
            }
        }

        /* Adjust link counts */
        struct ext2_inode old_parent;
        if (ext2_read_inode(odir->ino, &old_parent) == 0) {
            if (old_parent.i_links_count > 0) old_parent.i_links_count--;
            (void)ext2_write_inode(odir->ino, &old_parent);
        }
        struct ext2_inode new_parent;
        if (ext2_read_inode(ndir->ino, &new_parent) == 0) {
            new_parent.i_links_count++;
            (void)ext2_write_inode(ndir->ino, &new_parent);
        }
    }

    return 0;
}

/* ---- VFS: truncate ---- */

static int ext2_truncate_impl(struct fs_node* node, uint32_t length) {
    if (!node) return -EINVAL;
    struct ext2_node* en = (struct ext2_node*)node;

    struct ext2_inode inode;
    if (ext2_read_inode(en->ino, &inode) < 0) return -EIO;

    if (length >= inode.i_size) return 0; /* only shrink */

    uint32_t bs = g_ext2.block_size;
    uint32_t new_blocks = (length + bs - 1) / bs;
    uint32_t old_blocks = (inode.i_size + bs - 1) / bs;

    /* Free blocks beyond new size */
    for (uint32_t b = new_blocks; b < old_blocks; b++) {
        uint32_t phys = ext2_block_map(&inode, b);
        if (phys != 0) {
            ext2_free_block(phys);
            /* Note: we don't zero out the pointer in the inode for simplicity,
             * since the size field prevents access to freed blocks. */
        }
    }

    inode.i_size = length;
    inode.i_blocks = new_blocks * (bs / EXT2_SECTOR_SIZE);
    (void)ext2_write_inode(en->ino, &inode);
    node->length = length;
    return 0;
}

/* ---- VFS: link (hard link) ---- */

static int ext2_link_impl(struct fs_node* dir, const char* name, struct fs_node* target) {
    if (!dir || !name || !target) return -EINVAL;
    struct ext2_node* parent = (struct ext2_node*)dir;
    struct ext2_node* src = (struct ext2_node*)target;

    /* Check doesn't already exist */
    if (ext2_dir_find(parent->ino, name, NULL) == 0) return -EEXIST;

    struct ext2_inode src_inode;
    if (ext2_read_inode(src->ino, &src_inode) < 0) return -EIO;
    if ((src_inode.i_mode & 0xF000) == EXT2_S_IFDIR) return -EPERM;

    int rc = ext2_dir_add_entry(parent->ino, name, src->ino, EXT2_FT_REG_FILE);
    if (rc < 0) return rc;

    src_inode.i_links_count++;
    return ext2_write_inode(src->ino, &src_inode);
}

/* ---- Mount ---- */

fs_node_t* ext2_mount(uint32_t partition_lba) {
    memset(&g_ext2, 0, sizeof(g_ext2));
    g_ext2.part_lba = partition_lba;

    struct ext2_superblock sb;
    if (ext2_read_superblock(&sb) < 0) {
        kprintf("[EXT2] Failed to read superblock\n");
        return NULL;
    }

    if (sb.s_magic != EXT2_SUPER_MAGIC) {
        kprintf("[EXT2] Invalid magic: 0x%x\n", sb.s_magic);
        return NULL;
    }

    g_ext2.block_size = 1024U << sb.s_log_block_size;
    if (g_ext2.block_size > 4096) {
        kprintf("[EXT2] Unsupported block size %u\n", g_ext2.block_size);
        return NULL;
    }
    g_ext2.sectors_per_block = g_ext2.block_size / EXT2_SECTOR_SIZE;
    g_ext2.inodes_per_group = sb.s_inodes_per_group;
    g_ext2.blocks_per_group = sb.s_blocks_per_group;
    g_ext2.first_data_block = sb.s_first_data_block;
    g_ext2.total_blocks = sb.s_blocks_count;
    g_ext2.total_inodes = sb.s_inodes_count;

    if (sb.s_rev_level >= 1 && sb.s_inode_size != 0) {
        g_ext2.inode_size = sb.s_inode_size;
    } else {
        g_ext2.inode_size = 128;
    }

    g_ext2.num_groups = (sb.s_blocks_count + sb.s_blocks_per_group - 1) / sb.s_blocks_per_group;

    /* Read Group Descriptor Table */
    g_ext2.gdt_blocks = (g_ext2.num_groups * (uint32_t)sizeof(struct ext2_group_desc) +
                          g_ext2.block_size - 1) / g_ext2.block_size;
    uint32_t gdt_bytes = g_ext2.num_groups * (uint32_t)sizeof(struct ext2_group_desc);
    g_ext2.gdt = (struct ext2_group_desc*)kmalloc(gdt_bytes);
    if (!g_ext2.gdt) {
        kprintf("[EXT2] Failed to allocate GDT (%u bytes)\n", gdt_bytes);
        return NULL;
    }
    memset(g_ext2.gdt, 0, gdt_bytes);

    uint32_t gdt_block = g_ext2.first_data_block + 1;
    uint8_t* gp = (uint8_t*)g_ext2.gdt;
    for (uint32_t b = 0; b < g_ext2.gdt_blocks; b++) {
        uint8_t blk_buf[4096];
        if (ext2_read_block(gdt_block + b, blk_buf) < 0) {
            kprintf("[EXT2] Failed to read GDT block %u\n", gdt_block + b);
            kfree(g_ext2.gdt);
            g_ext2.gdt = NULL;
            return NULL;
        }
        uint32_t to_copy = g_ext2.block_size;
        if (to_copy > gdt_bytes - b * g_ext2.block_size)
            to_copy = gdt_bytes - b * g_ext2.block_size;
        memcpy(gp + b * g_ext2.block_size, blk_buf, to_copy);
    }

    /* Read root inode */
    struct ext2_inode root_inode;
    if (ext2_read_inode(EXT2_ROOT_INO, &root_inode) < 0) {
        kprintf("[EXT2] Failed to read root inode\n");
        kfree(g_ext2.gdt);
        g_ext2.gdt = NULL;
        return NULL;
    }

    /* Build root node */
    memset(&g_ext2_root, 0, sizeof(g_ext2_root));
    memcpy(g_ext2_root.vfs.name, "ext2", 5);
    g_ext2_root.vfs.flags = FS_DIRECTORY;
    g_ext2_root.vfs.inode = EXT2_ROOT_INO;
    g_ext2_root.vfs.length = root_inode.i_size;
    g_ext2_root.vfs.uid = root_inode.i_uid;
    g_ext2_root.vfs.gid = root_inode.i_gid;
    g_ext2_root.vfs.mode = root_inode.i_mode;
    g_ext2_root.ino = EXT2_ROOT_INO;
    ext2_set_dir_ops(&g_ext2_root.vfs);

    g_ext2_ready = 1;

    kprintf("[EXT2] Mounted at LBA %u (%u blocks, %u inodes, %u groups, %uB/block)\n",
            partition_lba, g_ext2.total_blocks, g_ext2.total_inodes,
            g_ext2.num_groups, g_ext2.block_size);

    return &g_ext2_root.vfs;
}
