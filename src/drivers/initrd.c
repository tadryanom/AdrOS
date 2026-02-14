#include "initrd.h"
#include "utils.h"
#include "heap.h"
#include "console.h"
#include "errno.h"

#define TAR_BLOCK 512

typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed)) tar_header_t;

typedef struct {
    char name[128];
    uint32_t flags;
    uint32_t data_offset;
    uint32_t length;
    int parent;
    int first_child;
    int next_sibling;
} initrd_entry_t;

static uint32_t initrd_location_base = 0;

static initrd_entry_t* entries = NULL;
static fs_node_t* nodes = NULL;
static int entry_count = 0;
static int entry_cap = 0;

static uint32_t tar_parse_octal(const char* s, size_t n) {
    uint32_t v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == 0) break;
        if (s[i] < '0' || s[i] > '7') continue;
        v = (v << 3) + (uint32_t)(s[i] - '0');
    }
    return v;
}

static int tar_is_zero_block(const uint8_t* p) {
    for (int i = 0; i < TAR_BLOCK; i++) {
        if (p[i] != 0) return 0;
    }
    return 1;
}

static void str_copy_n(char* dst, size_t dst_sz, const char* src, size_t src_n) {
    if (dst_sz == 0) return;
    size_t i = 0;
    for (; i + 1 < dst_sz && i < src_n; i++) {
        char c = src[i];
        if (c == 0) break;
        dst[i] = c;
    }
    dst[i] = 0;
}

static int entry_alloc(void) {
    if (entry_count == entry_cap) {
        int new_cap = (entry_cap == 0) ? 32 : entry_cap * 2;
        initrd_entry_t* new_entries = (initrd_entry_t*)kmalloc(sizeof(initrd_entry_t) * (size_t)new_cap);
        fs_node_t* new_nodes = (fs_node_t*)kmalloc(sizeof(fs_node_t) * (size_t)new_cap);
        if (!new_entries || !new_nodes) return -ENOMEM;

        if (entries) {
            memcpy(new_entries, entries, sizeof(initrd_entry_t) * (size_t)entry_count);
            kfree(entries);
        }
        if (nodes) {
            memcpy(new_nodes, nodes, sizeof(fs_node_t) * (size_t)entry_count);
            kfree(nodes);
        }

        entries = new_entries;
        nodes = new_nodes;
        entry_cap = new_cap;
    }

    int idx = entry_count++;
    memset(&entries[idx], 0, sizeof(entries[idx]));
    memset(&nodes[idx], 0, sizeof(nodes[idx]));
    entries[idx].parent = -1;
    entries[idx].first_child = -1;
    entries[idx].next_sibling = -1;
    return idx;
}

static int entry_find_child(int parent, const char* name) {
    int c = entries[parent].first_child;
    while (c != -1) {
        if (strcmp(entries[c].name, name) == 0) return c;
        c = entries[c].next_sibling;
    }
    return -1;
}

static int entry_add_child(int parent, const char* name, uint32_t flags) {
    int idx = entry_alloc();
    if (idx < 0) return idx;

    strcpy(entries[idx].name, name);
    entries[idx].flags = flags;
    entries[idx].parent = parent;

    entries[idx].next_sibling = entries[parent].first_child;
    entries[parent].first_child = idx;

    return idx;
}

static int ensure_dir(int parent, const char* name) {
    int child = entry_find_child(parent, name);
    if (child != -1) return child;
    return entry_add_child(parent, name, FS_DIRECTORY);
}

static int ensure_path_dirs(int root_idx, const char* path, char* leaf_out, size_t leaf_out_sz) {
    int cur = root_idx;
    const char* p = path;

    if (!path || !leaf_out || leaf_out_sz == 0) return -EINVAL;

    while (*p == '/') p++;

    char part[128];
    while (*p != 0) {
        size_t i = 0;
        while (*p != 0 && *p != '/') {
            if (i + 1 < sizeof(part)) part[i++] = *p;
            p++;
        }
        part[i] = 0;
        while (*p == '/') p++;

        if (part[0] == 0) continue;

        if (*p == 0) {
            str_copy_n(leaf_out, leaf_out_sz, part, strlen(part));
            return cur;
        }

        cur = ensure_dir(cur, part);
        if (cur < 0) return cur;
    }

    return -EINVAL;
}

static uint32_t initrd_read_impl(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node) return 0;
    uint32_t idx = node->inode;
    if ((int)idx < 0 || (int)idx >= entry_count) return 0;

    initrd_entry_t* e = &entries[idx];
    if ((e->flags & FS_FILE) == 0) return 0;

    if (offset > e->length) return 0;
    if (offset + size > e->length) size = e->length - offset;

    const uint8_t* src = (const uint8_t*)(initrd_location_base + e->data_offset + offset);
    memcpy(buffer, src, size);
    return size;
}

static struct fs_node* initrd_finddir(struct fs_node* node, const char* name) {
    if (!node || !name) return 0;
    int parent = (int)node->inode;
    if (parent < 0 || parent >= entry_count) return 0;

    int c = entries[parent].first_child;
    while (c != -1) {
        if (strcmp(entries[c].name, name) == 0) {
            return &nodes[c];
        }
        c = entries[c].next_sibling;
    }
    return 0;
}

static const struct file_operations initrd_file_ops = {
    .read = initrd_read_impl,
};

static const struct file_operations initrd_dir_ops = {0};

static const struct inode_operations initrd_dir_iops = {
    .lookup = initrd_finddir,
};

static void initrd_finalize_nodes(void) {
    for (int i = 0; i < entry_count; i++) {
        fs_node_t* n = &nodes[i];
        initrd_entry_t* e = &entries[i];

        strcpy(n->name, e->name);
        n->inode = (uint32_t)i;
        n->length = e->length;
        n->flags = e->flags;

        if (e->flags & FS_FILE) {
            n->f_ops = &initrd_file_ops;
        } else if (e->flags & FS_DIRECTORY) {
            n->f_ops = &initrd_dir_ops;
            n->i_ops = &initrd_dir_iops;
        }
    }
}

fs_node_t* initrd_init(uint32_t location) {
    initrd_location_base = location;

    // Initialize root
    entry_count = 0;

    int root = entry_alloc();
    if (root < 0) return 0;
    strcpy(entries[root].name, "");
    entries[root].flags = FS_DIRECTORY;
    entries[root].data_offset = 0;
    entries[root].length = 0;
    entries[root].parent = -1;

    const uint8_t* p = (const uint8_t*)(uintptr_t)location;
    int files = 0;

    while (1) {
        if (tar_is_zero_block(p)) break;
        const tar_header_t* h = (const tar_header_t*)p;

        char name[256];
        name[0] = 0;
        if (h->prefix[0]) {
            str_copy_n(name, sizeof(name), h->prefix, sizeof(h->prefix));
            size_t cur = strlen(name);
            if (cur + 1 < sizeof(name)) {
                name[cur] = '/';
                name[cur + 1] = 0;
            }
            size_t rem = sizeof(name) - strlen(name) - 1;
            str_copy_n(name + strlen(name), rem + 1, h->name, sizeof(h->name));
        } else {
            str_copy_n(name, sizeof(name), h->name, sizeof(h->name));
        }

        uint32_t size = tar_parse_octal(h->size, sizeof(h->size));
        char tf = h->typeflag;
        if (tf == 0) tf = '0';

        // Normalize: strip leading './'
        if (name[0] == '.' && name[1] == '/') {
            size_t l = strlen(name);
            for (size_t i = 0; i + 2 <= l; i++) {
                name[i] = name[i + 2];
            }
        }

        // Directories in tar often end with '/'
        size_t nlen = strlen(name);
        if (nlen && name[nlen - 1] == '/') {
            name[nlen - 1] = 0;
            tf = '5';
        }

        if (name[0] != 0) {
            char leaf[128];
            int parent = ensure_path_dirs(root, name, leaf, sizeof(leaf));
            if (parent >= 0) {
                if (tf == '5') {
                    (void)ensure_dir(parent, leaf);
                } else {
                    int existing = entry_find_child(parent, leaf);
                    int idx = existing;
                    if (idx == -1) {
                        idx = entry_add_child(parent, leaf, FS_FILE);
                    }
                    if (idx >= 0) {
                        entries[idx].flags = FS_FILE;
                        entries[idx].data_offset = (uint32_t)((uintptr_t)(p + TAR_BLOCK) - (uintptr_t)location);
                        entries[idx].length = size;
                        files++;
                    }
                }
            }
        }

        uint32_t adv = TAR_BLOCK + ((size + (TAR_BLOCK - 1)) & ~(TAR_BLOCK - 1));
        p += adv;
    }

    initrd_finalize_nodes();

    kprintf("[INITRD] Found %d files.\n", files);

    return &nodes[root];
}
