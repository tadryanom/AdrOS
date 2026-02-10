#include "devfs.h"

#include "errno.h"
#include "tty.h"
#include "pty.h"
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
static fs_node_t g_dev_console;
static fs_node_t g_dev_tty;
static fs_node_t g_dev_ptmx;
static fs_node_t g_dev_pts_dir;
static uint32_t g_devfs_inited = 0;

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

static uint32_t dev_console_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    int rc = tty_read_kbuf(buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static uint32_t dev_console_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    (void)offset;
    int rc = tty_write_kbuf((const uint8_t*)buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static uint32_t dev_tty_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    int rc = tty_read_kbuf(buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static uint32_t dev_tty_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    (void)offset;
    int rc = tty_write_kbuf((const uint8_t*)buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static uint32_t dev_ptmx_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    int idx = pty_ino_to_idx(node->inode);
    if (idx < 0) idx = 0;
    int rc = pty_master_read_idx(idx, buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static uint32_t dev_ptmx_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)offset;
    int idx = pty_ino_to_idx(node->inode);
    if (idx < 0) idx = 0;
    int rc = pty_master_write_idx(idx, buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static struct fs_node* devfs_finddir_impl(struct fs_node* node, const char* name) {
    (void)node;
    if (!name || name[0] == 0) return 0;

    if (strcmp(name, "null") == 0) return &g_dev_null;
    if (strcmp(name, "zero") == 0) return &g_dev_zero;
    if (strcmp(name, "random") == 0) return &g_dev_random;
    if (strcmp(name, "urandom") == 0) return &g_dev_urandom;
    if (strcmp(name, "console") == 0) return &g_dev_console;
    if (strcmp(name, "tty") == 0) return &g_dev_tty;
    if (strcmp(name, "ptmx") == 0) return &g_dev_ptmx;
    if (strcmp(name, "pts") == 0) return &g_dev_pts_dir;
    return 0;
}

static struct fs_node* devfs_pts_finddir_impl(struct fs_node* node, const char* name) {
    (void)node;
    if (!name || name[0] == 0) return 0;
    int count = pty_pair_count();
    for (int i = 0; i < count; i++) {
        char num[4];
        num[0] = '0' + (char)i;
        num[1] = '\0';
        if (strcmp(name, num) == 0) {
            return pty_get_slave_node(i);
        }
    }
    return 0;
}

static int devfs_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    (void)node;
    if (!inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    static const struct { const char* name; uint32_t ino; uint8_t type; } devs[] = {
        { "null",    2,  FS_CHARDEVICE },
        { "zero",    7,  FS_CHARDEVICE },
        { "random",  8,  FS_CHARDEVICE },
        { "urandom", 9,  FS_CHARDEVICE },
        { "console", 10, FS_CHARDEVICE },
        { "tty",     3,  FS_CHARDEVICE },
        { "ptmx",    4,  FS_CHARDEVICE },
        { "pts",     5,  FS_DIRECTORY },
    };
    enum { NDEVS = 8 };

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
            if (di >= NDEVS) break;
            e.d_ino = devs[di].ino;
            e.d_type = devs[di].type;
            strcpy(e.d_name, devs[di].name);
        }

        e.d_reclen = (uint16_t)sizeof(e);
        ents[written] = e;
        written++;
        idx++;
    }

    *inout_index = idx;
    return (int)(written * (uint32_t)sizeof(struct vfs_dirent));
}

static int devfs_pts_readdir_impl(struct fs_node* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    (void)node;
    if (!inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    int count = pty_pair_count();
    uint32_t idx = *inout_index;
    uint32_t cap = buf_len / (uint32_t)sizeof(struct vfs_dirent);
    struct vfs_dirent* ents = (struct vfs_dirent*)buf;
    uint32_t written = 0;

    while (written < cap) {
        struct vfs_dirent e;
        memset(&e, 0, sizeof(e));

        if (idx == 0) {
            e.d_ino = 5; e.d_type = FS_DIRECTORY; strcpy(e.d_name, ".");
        } else if (idx == 1) {
            e.d_ino = 1; e.d_type = FS_DIRECTORY; strcpy(e.d_name, "..");
        } else {
            int pi = (int)(idx - 2);
            if (pi >= count) break;
            e.d_ino = PTY_SLAVE_INO_BASE + (uint32_t)pi;
            e.d_type = FS_CHARDEVICE;
            e.d_name[0] = '0' + (char)pi;
            e.d_name[1] = '\0';
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
    g_dev_null.open = 0;
    g_dev_null.close = 0;
    g_dev_null.finddir = 0;

    memset(&g_dev_zero, 0, sizeof(g_dev_zero));
    strcpy(g_dev_zero.name, "zero");
    g_dev_zero.flags = FS_CHARDEVICE;
    g_dev_zero.inode = 7;
    g_dev_zero.read = &dev_zero_read;
    g_dev_zero.write = &dev_zero_write;

    memset(&g_dev_random, 0, sizeof(g_dev_random));
    strcpy(g_dev_random.name, "random");
    g_dev_random.flags = FS_CHARDEVICE;
    g_dev_random.inode = 8;
    g_dev_random.read = &dev_random_read;
    g_dev_random.write = &dev_random_write;

    memset(&g_dev_urandom, 0, sizeof(g_dev_urandom));
    strcpy(g_dev_urandom.name, "urandom");
    g_dev_urandom.flags = FS_CHARDEVICE;
    g_dev_urandom.inode = 9;
    g_dev_urandom.read = &dev_random_read;
    g_dev_urandom.write = &dev_random_write;

    memset(&g_dev_console, 0, sizeof(g_dev_console));
    strcpy(g_dev_console.name, "console");
    g_dev_console.flags = FS_CHARDEVICE;
    g_dev_console.inode = 10;
    g_dev_console.read = &dev_console_read;
    g_dev_console.write = &dev_console_write;

    memset(&g_dev_tty, 0, sizeof(g_dev_tty));
    strcpy(g_dev_tty.name, "tty");
    g_dev_tty.flags = FS_CHARDEVICE;
    g_dev_tty.inode = 3;
    g_dev_tty.length = 0;
    g_dev_tty.read = &dev_tty_read;
    g_dev_tty.write = &dev_tty_write;
    g_dev_tty.open = 0;
    g_dev_tty.close = 0;
    g_dev_tty.finddir = 0;

    memset(&g_dev_ptmx, 0, sizeof(g_dev_ptmx));
    strcpy(g_dev_ptmx.name, "ptmx");
    g_dev_ptmx.flags = FS_CHARDEVICE;
    g_dev_ptmx.inode = PTY_MASTER_INO_BASE;
    g_dev_ptmx.read = &dev_ptmx_read;
    g_dev_ptmx.write = &dev_ptmx_write;

    memset(&g_dev_pts_dir, 0, sizeof(g_dev_pts_dir));
    strcpy(g_dev_pts_dir.name, "pts");
    g_dev_pts_dir.flags = FS_DIRECTORY;
    g_dev_pts_dir.inode = 5;
    g_dev_pts_dir.length = 0;
    g_dev_pts_dir.read = 0;
    g_dev_pts_dir.write = 0;
    g_dev_pts_dir.open = 0;
    g_dev_pts_dir.close = 0;
    g_dev_pts_dir.finddir = &devfs_pts_finddir_impl;
    g_dev_pts_dir.readdir = &devfs_pts_readdir_impl;

}

fs_node_t* devfs_create_root(void) {
    devfs_init_once();
    return &g_dev_root.vfs;
}
