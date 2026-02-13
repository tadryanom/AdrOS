#include "devfs.h"

#include "errno.h"
#include "utils.h"

extern uint32_t get_tick_count(void);

struct devfs_root {
    fs_node_t vfs;
};

static struct devfs_root g_dev_root;
static fs_node_t g_dev_null;
static fs_node_t g_dev_zero;
static fs_node_t g_dev_random;
static fs_node_t g_dev_urandom;
static uint32_t g_devfs_inited = 0;

/* --- Device registry --- */
static fs_node_t* g_registered[DEVFS_MAX_DEVICES];
static int g_registered_count = 0;

int devfs_register_device(fs_node_t *node) {
    if (!node) return -1;
    if (g_registered_count >= DEVFS_MAX_DEVICES) return -1;
    g_registered[g_registered_count++] = node;
    return 0;
}

static uint32_t prng_state = 0x12345678;

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

static uint32_t dev_zero_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    if (buffer && size > 0)
        memset(buffer, 0, size);
    return size;
}

static uint32_t dev_zero_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    (void)offset;
    (void)buffer;
    return size;
}

static uint32_t prng_next(void) {
    uint32_t s = prng_state;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    prng_state = s;
    return s;
}

static uint32_t dev_random_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    if (!buffer || size == 0) return 0;
    prng_state ^= get_tick_count();
    for (uint32_t i = 0; i < size; i++) {
        if ((i & 3) == 0) {
            uint32_t r = prng_next();
            buffer[i] = (uint8_t)(r & 0xFF);
        } else {
            buffer[i] = (uint8_t)((prng_next() >> ((i & 3) * 8)) & 0xFF);
        }
    }
    return size;
}

static uint32_t dev_random_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    (void)offset;
    if (buffer && size >= 4) {
        uint32_t seed = 0;
        memcpy(&seed, buffer, 4);
        prng_state ^= seed;
    }
    return size;
}

static int dev_null_poll(fs_node_t* node, int events) {
    (void)node;
    int revents = 0;
    if (events & VFS_POLL_IN) revents |= VFS_POLL_IN | VFS_POLL_HUP;
    if (events & VFS_POLL_OUT) revents |= VFS_POLL_OUT;
    return revents;
}

static int dev_always_ready_poll(fs_node_t* node, int events) {
    (void)node;
    int revents = 0;
    if (events & VFS_POLL_IN) revents |= VFS_POLL_IN;
    if (events & VFS_POLL_OUT) revents |= VFS_POLL_OUT;
    return revents;
}

static struct fs_node* devfs_finddir_impl(struct fs_node* node, const char* name) {
    (void)node;
    if (!name || name[0] == 0) return 0;

    if (strcmp(name, "null") == 0) return &g_dev_null;
    if (strcmp(name, "zero") == 0) return &g_dev_zero;
    if (strcmp(name, "random") == 0) return &g_dev_random;
    if (strcmp(name, "urandom") == 0) return &g_dev_urandom;

    for (int i = 0; i < g_registered_count; i++) {
        if (strcmp(g_registered[i]->name, name) == 0)
            return g_registered[i];
    }
    return 0;
}

static int devfs_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    (void)node;
    if (!inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    /* Built-in devices (owned by devfs) */
    static const struct { const char* name; uint32_t ino; uint8_t type; } builtins[] = {
        { "null",    2,  FS_CHARDEVICE },
        { "zero",    7,  FS_CHARDEVICE },
        { "random",  8,  FS_CHARDEVICE },
        { "urandom", 9,  FS_CHARDEVICE },
    };
    enum { NBUILTINS = 4 };

    uint32_t total = (uint32_t)(NBUILTINS + g_registered_count);
    uint32_t idx = *inout_index;
    uint32_t cap = buf_len / (uint32_t)sizeof(struct vfs_dirent);
    struct vfs_dirent* ents = (struct vfs_dirent*)buf;
    uint32_t written = 0;

    while (written < cap) {
        struct vfs_dirent e;
        memset(&e, 0, sizeof(e));

        if (idx == 0) {
            e.d_ino = 1; e.d_type = FS_DIRECTORY; strcpy(e.d_name, ".");
        } else if (idx == 1) {
            e.d_ino = 1; e.d_type = FS_DIRECTORY; strcpy(e.d_name, "..");
        } else {
            uint32_t di = idx - 2;
            if (di >= total) break;
            if (di < NBUILTINS) {
                e.d_ino = builtins[di].ino;
                e.d_type = builtins[di].type;
                strcpy(e.d_name, builtins[di].name);
            } else {
                fs_node_t* rn = g_registered[di - NBUILTINS];
                e.d_ino = rn->inode;
                e.d_type = (uint8_t)rn->flags;
                strcpy(e.d_name, rn->name);
            }
        }

        e.d_reclen = (uint16_t)sizeof(e);
        ents[written] = e;
        written++;
        idx++;
    }

    *inout_index = idx;
    return (int)(written * (uint32_t)sizeof(struct vfs_dirent));
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
    g_dev_root.vfs.readdir = &devfs_readdir_impl;

    memset(&g_dev_null, 0, sizeof(g_dev_null));
    strcpy(g_dev_null.name, "null");
    g_dev_null.flags = FS_CHARDEVICE;
    g_dev_null.inode = 2;
    g_dev_null.length = 0;
    g_dev_null.read = &dev_null_read;
    g_dev_null.write = &dev_null_write;
    g_dev_null.poll = &dev_null_poll;
    g_dev_null.open = 0;
    g_dev_null.close = 0;
    g_dev_null.finddir = 0;

    memset(&g_dev_zero, 0, sizeof(g_dev_zero));
    strcpy(g_dev_zero.name, "zero");
    g_dev_zero.flags = FS_CHARDEVICE;
    g_dev_zero.inode = 7;
    g_dev_zero.read = &dev_zero_read;
    g_dev_zero.write = &dev_zero_write;
    g_dev_zero.poll = &dev_always_ready_poll;

    memset(&g_dev_random, 0, sizeof(g_dev_random));
    strcpy(g_dev_random.name, "random");
    g_dev_random.flags = FS_CHARDEVICE;
    g_dev_random.inode = 8;
    g_dev_random.read = &dev_random_read;
    g_dev_random.write = &dev_random_write;
    g_dev_random.poll = &dev_always_ready_poll;

    memset(&g_dev_urandom, 0, sizeof(g_dev_urandom));
    strcpy(g_dev_urandom.name, "urandom");
    g_dev_urandom.flags = FS_CHARDEVICE;
    g_dev_urandom.inode = 9;
    g_dev_urandom.read = &dev_random_read;
    g_dev_urandom.write = &dev_random_write;
    g_dev_urandom.poll = &dev_always_ready_poll;
}

fs_node_t* devfs_create_root(void) {
    devfs_init_once();
    return &g_dev_root.vfs;
}
