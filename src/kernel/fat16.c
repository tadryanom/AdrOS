#include "fat16.h"
#include "ata_pio.h"
#include "heap.h"
#include "utils.h"
#include "uart_console.h"
#include "errno.h"

#include <stddef.h>

/* FAT16 BPB (BIOS Parameter Block) */
struct fat16_bpb {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed));

/* FAT16 directory entry */
struct fat16_dirent {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t first_cluster;
    uint32_t file_size;
} __attribute__((packed));

#define FAT16_ATTR_READONLY  0x01
#define FAT16_ATTR_HIDDEN    0x02
#define FAT16_ATTR_SYSTEM    0x04
#define FAT16_ATTR_VOLUME_ID 0x08
#define FAT16_ATTR_DIRECTORY 0x10
#define FAT16_ATTR_ARCHIVE   0x20
#define FAT16_ATTR_LFN       0x0F

struct fat16_state {
    uint32_t part_lba;
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t fat_size_16;
    uint32_t fat_lba;
    uint32_t root_dir_lba;
    uint32_t data_lba;
};

static struct fat16_state g_fat;
static fs_node_t g_fat_root;
static uint8_t g_sector_buf[512];

static int fat16_read_sector(uint32_t lba, void* buf) {
    return ata_pio_read28(lba, (uint8_t*)buf);
}

static uint32_t fat16_cluster_to_lba(uint16_t cluster) {
    return g_fat.data_lba + (uint32_t)(cluster - 2) * g_fat.sectors_per_cluster;
}

static uint16_t fat16_next_cluster(uint16_t cluster) {
    uint32_t fat_offset = (uint32_t)cluster * 2;
    uint32_t fat_sector = g_fat.fat_lba + fat_offset / 512;
    uint32_t entry_offset = fat_offset % 512;

    if (fat16_read_sector(fat_sector, g_sector_buf) < 0) return 0xFFFF;
    return *(uint16_t*)(g_sector_buf + entry_offset);
}

/* VFS read callback for FAT16 files */
static uint32_t fat16_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer) return 0;
    if (offset >= node->length) return 0;
    if (offset + size > node->length) size = node->length - offset;

    uint16_t cluster = (uint16_t)node->inode;
    uint32_t cluster_size = (uint32_t)g_fat.sectors_per_cluster * g_fat.bytes_per_sector;
    uint32_t bytes_read = 0;

    /* Skip to the cluster containing 'offset' */
    uint32_t skip = offset / cluster_size;
    for (uint32_t i = 0; i < skip && cluster < 0xFFF8; i++) {
        cluster = fat16_next_cluster(cluster);
    }
    uint32_t pos_in_cluster = offset % cluster_size;

    while (bytes_read < size && cluster >= 2 && cluster < 0xFFF8) {
        uint32_t lba = fat16_cluster_to_lba(cluster);
        for (uint32_t s = pos_in_cluster / 512; s < g_fat.sectors_per_cluster && bytes_read < size; s++) {
            if (fat16_read_sector(lba + s, g_sector_buf) < 0) return bytes_read;
            uint32_t off_in_sec = (pos_in_cluster > 0 && s == pos_in_cluster / 512) ? pos_in_cluster % 512 : 0;
            uint32_t to_copy = 512 - off_in_sec;
            if (to_copy > size - bytes_read) to_copy = size - bytes_read;
            memcpy(buffer + bytes_read, g_sector_buf + off_in_sec, to_copy);
            bytes_read += to_copy;
        }
        pos_in_cluster = 0;
        cluster = fat16_next_cluster(cluster);
    }

    return bytes_read;
}

/* VFS finddir for root directory */
static fs_node_t* fat16_finddir(fs_node_t* node, const char* name) {
    (void)node;
    if (!name) return NULL;

    uint32_t entries_per_sector = 512 / sizeof(struct fat16_dirent);
    uint32_t root_sectors = (g_fat.root_entry_count * 32 + 511) / 512;

    for (uint32_t s = 0; s < root_sectors; s++) {
        if (fat16_read_sector(g_fat.root_dir_lba + s, g_sector_buf) < 0) return NULL;
        struct fat16_dirent* de = (struct fat16_dirent*)g_sector_buf;
        for (uint32_t i = 0; i < entries_per_sector; i++) {
            if (de[i].name[0] == 0) return NULL; /* end of dir */
            if ((uint8_t)de[i].name[0] == 0xE5) continue; /* deleted */
            if (de[i].attr & FAT16_ATTR_LFN) continue;
            if (de[i].attr & FAT16_ATTR_VOLUME_ID) continue;

            /* Build 8.3 filename */
            char fname[13];
            int fi = 0;
            for (int j = 0; j < 8 && de[i].name[j] != ' '; j++)
                fname[fi++] = de[i].name[j] | 0x20; /* lowercase */
            if (de[i].ext[0] != ' ') {
                fname[fi++] = '.';
                for (int j = 0; j < 3 && de[i].ext[j] != ' '; j++)
                    fname[fi++] = de[i].ext[j] | 0x20;
            }
            fname[fi] = '\0';

            if (strcmp(fname, name) == 0) {
                fs_node_t* fn = (fs_node_t*)kmalloc(sizeof(fs_node_t));
                if (!fn) return NULL;
                memset(fn, 0, sizeof(*fn));
                memcpy(fn->name, fname, fi + 1);
                fn->flags = (de[i].attr & FAT16_ATTR_DIRECTORY) ? FS_DIRECTORY : FS_FILE;
                fn->length = de[i].file_size;
                fn->inode = de[i].first_cluster;
                fn->read = fat16_read;
                return fn;
            }
        }
    }
    return NULL;
}

fs_node_t* fat16_mount(uint32_t partition_lba) {
    if (fat16_read_sector(partition_lba, g_sector_buf) < 0) {
        uart_print("[FAT16] Failed to read BPB\n");
        return NULL;
    }

    struct fat16_bpb* bpb = (struct fat16_bpb*)g_sector_buf;

    if (bpb->bytes_per_sector != 512) {
        uart_print("[FAT16] Unsupported sector size\n");
        return NULL;
    }
    if (bpb->fat_size_16 == 0 || bpb->num_fats == 0) {
        uart_print("[FAT16] Invalid BPB\n");
        return NULL;
    }

    g_fat.part_lba = partition_lba;
    g_fat.bytes_per_sector = bpb->bytes_per_sector;
    g_fat.sectors_per_cluster = bpb->sectors_per_cluster;
    g_fat.reserved_sectors = bpb->reserved_sectors;
    g_fat.num_fats = bpb->num_fats;
    g_fat.root_entry_count = bpb->root_entry_count;
    g_fat.fat_size_16 = bpb->fat_size_16;

    g_fat.fat_lba = partition_lba + bpb->reserved_sectors;
    g_fat.root_dir_lba = g_fat.fat_lba + (uint32_t)bpb->num_fats * bpb->fat_size_16;
    uint32_t root_dir_sectors = ((uint32_t)bpb->root_entry_count * 32 + 511) / 512;
    g_fat.data_lba = g_fat.root_dir_lba + root_dir_sectors;

    memset(&g_fat_root, 0, sizeof(g_fat_root));
    memcpy(g_fat_root.name, "fat", 4);
    g_fat_root.flags = FS_DIRECTORY;
    g_fat_root.finddir = fat16_finddir;

    uart_print("[FAT16] Mounted at LBA ");
    char buf[12];
    int bi = 0;
    uint32_t v = partition_lba;
    if (v == 0) { buf[bi++] = '0'; }
    else {
        char tmp[12]; int ti = 0;
        while (v) { tmp[ti++] = (char)('0' + v % 10); v /= 10; }
        while (ti--) buf[bi++] = tmp[ti];
    }
    buf[bi] = '\0';
    uart_print(buf);
    uart_print("\n");

    return &g_fat_root;
}
