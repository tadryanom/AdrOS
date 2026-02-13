#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_SYMLINK     0x05
#define FS_SOCKET      0x06

/* poll() event flags — shared between kernel VFS and syscall layer */
#define VFS_POLL_IN    0x0001
#define VFS_POLL_OUT   0x0004
#define VFS_POLL_ERR   0x0008
#define VFS_POLL_HUP   0x0010

struct fs_node; /* forward declaration for file_operations */

/* Shared file operations table — filesystems define one static instance
 * per node type (file, dir, device) and point every node's f_ops at it.
 * During the migration period, the VFS checks f_ops first, then falls
 * back to per-node function pointers (legacy). */
struct file_operations {
    uint32_t (*read)(struct fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct fs_node* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
    void (*open)(struct fs_node* node);
    void (*close)(struct fs_node* node);
    struct fs_node* (*finddir)(struct fs_node* node, const char* name);
    int (*readdir)(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len);
    int (*ioctl)(struct fs_node* node, uint32_t cmd, void* arg);
    uintptr_t (*mmap)(struct fs_node* node, uintptr_t addr, uint32_t length, uint32_t prot, uint32_t offset);
    int (*poll)(struct fs_node* node, int events);
    int (*create)(struct fs_node* dir, const char* name, uint32_t flags, struct fs_node** out);
    int (*mkdir)(struct fs_node* dir, const char* name);
    int (*unlink)(struct fs_node* dir, const char* name);
    int (*rmdir)(struct fs_node* dir, const char* name);
    int (*rename)(struct fs_node* old_dir, const char* old_name,
                  struct fs_node* new_dir, const char* new_name);
    int (*truncate)(struct fs_node* node, uint32_t length);
    int (*link)(struct fs_node* dir, const char* name, struct fs_node* target);
};

typedef struct fs_node {
    char name[128];
    uint32_t flags;
    uint32_t inode;
    uint32_t length;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    char symlink_target[128];

    const struct file_operations* f_ops;

    // Legacy per-node function pointers (will be removed after migration)
    uint32_t (*read)(struct fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct fs_node* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
    void (*open)(struct fs_node* node);
    void (*close)(struct fs_node* node);
    struct fs_node* (*finddir)(struct fs_node* node, const char* name);
    int (*readdir)(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len);
    int (*ioctl)(struct fs_node* node, uint32_t cmd, void* arg);
    uintptr_t (*mmap)(struct fs_node* node, uintptr_t addr, uint32_t length, uint32_t prot, uint32_t offset);
    int (*poll)(struct fs_node* node, int events);
    int (*create)(struct fs_node* dir, const char* name, uint32_t flags, struct fs_node** out);
    int (*mkdir)(struct fs_node* dir, const char* name);
    int (*unlink)(struct fs_node* dir, const char* name);
    int (*rmdir)(struct fs_node* dir, const char* name);
    int (*rename)(struct fs_node* old_dir, const char* old_name,
                  struct fs_node* new_dir, const char* new_name);
    int (*truncate)(struct fs_node* node, uint32_t length);
    int (*link)(struct fs_node* dir, const char* name, struct fs_node* target);
} fs_node_t;

struct vfs_dirent {
    uint32_t d_ino;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[24];
};

// Standard VFS functions
uint32_t vfs_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
void vfs_open(fs_node_t* node);
void vfs_close(fs_node_t* node);

fs_node_t* vfs_lookup(const char* path);

// Resolve path to (parent_dir, basename).  Returns parent node or NULL.
fs_node_t* vfs_lookup_parent(const char* path, char* name_out, size_t name_sz);

// Directory mutation wrappers — route through mount points transparently
int vfs_create(const char* path, uint32_t flags, fs_node_t** out);
int vfs_mkdir(const char* path);
int vfs_unlink(const char* path);
int vfs_rmdir(const char* path);
int vfs_rename(const char* old_path, const char* new_path);
int vfs_truncate(const char* path, uint32_t length);
int vfs_link(const char* old_path, const char* new_path);

int vfs_mount(const char* mountpoint, fs_node_t* root);

// Global root of the filesystem
extern fs_node_t* fs_root;

#endif
