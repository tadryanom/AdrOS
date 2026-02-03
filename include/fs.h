#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04

typedef struct fs_node {
    char name[128];
    uint32_t flags;
    uint32_t inode;
    uint32_t length;
    
    // Function pointers for operations (Polymorphism in C)
    uint32_t (*read)(struct fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    void (*open)(struct fs_node* node);
    void (*close)(struct fs_node* node);
    struct fs_node* (*finddir)(struct fs_node* node, char* name);
} fs_node_t;

// Standard VFS functions
uint32_t vfs_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
void vfs_open(fs_node_t* node);
void vfs_close(fs_node_t* node);

// Global root of the filesystem
extern fs_node_t* fs_root;

#endif
