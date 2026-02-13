#include "procfs.h"

#include "process.h"
#include "utils.h"
#include "heap.h"
#include "pmm.h"
#include "timer.h"
#include "kernel/cmdline.h"

#include <stddef.h>

static fs_node_t g_proc_root;
static fs_node_t g_proc_self;
static fs_node_t g_proc_self_status;
static fs_node_t g_proc_uptime;
static fs_node_t g_proc_meminfo;
static fs_node_t g_proc_cmdline;

#define PID_NODE_POOL 8
static fs_node_t g_pid_dir[PID_NODE_POOL];
static fs_node_t g_pid_status[PID_NODE_POOL];
static fs_node_t g_pid_maps[PID_NODE_POOL];
static uint32_t g_pid_pool_idx = 0;

extern struct process* ready_queue_head;

static struct process* proc_find_pid(uint32_t pid) {
    if (!ready_queue_head) return NULL;
    struct process* it = ready_queue_head;
    const struct process* start = it;
    do {
        if (it->pid == pid) return it;
        it = it->next;
    } while (it && it != start);
    return NULL;
}

static int proc_snprintf(char* buf, uint32_t sz, const char* key, uint32_t val) {
    if (sz < 2) return 0;
    uint32_t w = 0;
    const char* p = key;
    while (*p && w + 1 < sz) buf[w++] = *p++;
    char num[16];
    itoa(val, num, 10);
    p = num;
    while (*p && w + 1 < sz) buf[w++] = *p++;
    if (w + 1 < sz) buf[w++] = '\n';
    buf[w] = 0;
    return (int)w;
}

static uint32_t proc_self_status_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    if (!current_process) return 0;

    char tmp[512];
    uint32_t len = 0;

    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "Pid:\t", current_process->pid);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "PPid:\t", current_process->parent_pid);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "Pgrp:\t", current_process->pgrp_id);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "Session:\t", current_process->session_id);

    const char* state_str = "unknown\n";
    switch (current_process->state) {
        case PROCESS_READY:    state_str = "R (ready)\n"; break;
        case PROCESS_RUNNING:  state_str = "R (running)\n"; break;
        case PROCESS_BLOCKED:  state_str = "S (blocked)\n"; break;
        case PROCESS_SLEEPING: state_str = "S (sleeping)\n"; break;
        case PROCESS_ZOMBIE:   state_str = "Z (zombie)\n"; break;
    }
    const char* s = "State:\t";
    while (*s && len + 1 < sizeof(tmp)) tmp[len++] = *s++;
    s = state_str;
    while (*s && len + 1 < sizeof(tmp)) tmp[len++] = *s++;

    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "SigPnd:\t", current_process->sig_pending_mask);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "SigBlk:\t", current_process->sig_blocked_mask);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "HeapStart:\t", (uint32_t)current_process->heap_start);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "HeapBreak:\t", (uint32_t)current_process->heap_break);

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    memcpy(buffer, tmp + offset, size);
    return size;
}

static uint32_t proc_cmdline_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    const char* raw = cmdline_raw();
    uint32_t len = (uint32_t)strlen(raw);
    char tmp[CMDLINE_MAX + 1];
    memcpy(tmp, raw, len);
    tmp[len] = '\n';
    len++;
    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    memcpy(buffer, tmp + offset, size);
    return size;
}

static uint32_t proc_uptime_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    uint32_t ticks = get_tick_count();
    uint32_t secs = (ticks * 20) / 1000;
    uint32_t frac = ((ticks * 20) % 1000) / 10;

    char tmp[64];
    uint32_t len = 0;
    char num[16];
    itoa(secs, num, 10);
    const char* p = num;
    while (*p && len + 2 < sizeof(tmp)) tmp[len++] = *p++;
    if (len + 2 < sizeof(tmp)) tmp[len++] = '.';
    if (frac < 10 && len + 2 < sizeof(tmp)) tmp[len++] = '0';
    itoa(frac, num, 10);
    p = num;
    while (*p && len + 2 < sizeof(tmp)) tmp[len++] = *p++;
    if (len + 1 < sizeof(tmp)) tmp[len++] = '\n';
    if (len < sizeof(tmp)) tmp[len] = 0;
    else tmp[sizeof(tmp) - 1] = 0;

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    memcpy(buffer, tmp + offset, size);
    return size;
}

extern void pmm_print_stats(void);

static uint32_t proc_meminfo_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;

    char tmp[256];
    uint32_t len = 0;

    /* Count processes */
    uint32_t nprocs = 0;
    if (ready_queue_head) {
        struct process* it = ready_queue_head;
        const struct process* start = it;
        do {
            nprocs++;
            it = it->next;
        } while (it && it != start);
    }

    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "Processes:\t", nprocs);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "TickCount:\t", get_tick_count());

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    memcpy(buffer, tmp + offset, size);
    return size;
}

/* --- per-PID status read (inode == target pid) --- */

static uint32_t proc_pid_status_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t pid = node->inode;
    struct process* p = proc_find_pid(pid);
    if (!p) return 0;

    char tmp[512];
    uint32_t len = 0;

    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "Pid:\t", p->pid);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "PPid:\t", p->parent_pid);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "Pgrp:\t", p->pgrp_id);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "Session:\t", p->session_id);

    const char* state_str = "unknown\n";
    switch (p->state) {
        case PROCESS_READY:    state_str = "R (ready)\n"; break;
        case PROCESS_RUNNING:  state_str = "R (running)\n"; break;
        case PROCESS_BLOCKED:  state_str = "S (blocked)\n"; break;
        case PROCESS_SLEEPING: state_str = "S (sleeping)\n"; break;
        case PROCESS_ZOMBIE:   state_str = "Z (zombie)\n"; break;
    }
    const char* s = "State:\t";
    while (*s && len + 1 < sizeof(tmp)) tmp[len++] = *s++;
    s = state_str;
    while (*s && len + 1 < sizeof(tmp)) tmp[len++] = *s++;

    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "SigPnd:\t", p->sig_pending_mask);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "SigBlk:\t", p->sig_blocked_mask);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "HeapStart:\t", (uint32_t)p->heap_start);
    len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "HeapBreak:\t", (uint32_t)p->heap_break);

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    memcpy(buffer, tmp + offset, size);
    return size;
}

/* --- per-PID maps read (inode == target pid) --- */

static uint32_t proc_pid_maps_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t pid = node->inode;
    struct process* p = proc_find_pid(pid);
    if (!p) return 0;

    char tmp[1024];
    uint32_t len = 0;

    if (p->heap_start && p->heap_break > p->heap_start) {
        len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "heap:\t", (uint32_t)p->heap_start);
        len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "brk:\t", (uint32_t)p->heap_break);
    }

    for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
        if (p->mmaps[i].length == 0) continue;
        len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "mmap:\t", (uint32_t)p->mmaps[i].base);
        len += (uint32_t)proc_snprintf(tmp + len, sizeof(tmp) - len, "len:\t", p->mmaps[i].length);
    }

    if (len == 0) {
        const char* msg = "(empty)\n";
        while (*msg && len + 1 < sizeof(tmp)) tmp[len++] = *msg++;
    }

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    memcpy(buffer, tmp + offset, size);
    return size;
}

/* --- per-PID directory --- */

static fs_node_t* proc_pid_finddir(fs_node_t* node, const char* name) {
    uint32_t pid = node->inode;
    uint32_t slot = g_pid_pool_idx;

    if (strcmp(name, "status") == 0) {
        g_pid_pool_idx = (slot + 1) % PID_NODE_POOL;
        memset(&g_pid_status[slot], 0, sizeof(fs_node_t));
        strcpy(g_pid_status[slot].name, "status");
        g_pid_status[slot].flags = FS_FILE;
        g_pid_status[slot].inode = pid;
        g_pid_status[slot].read = proc_pid_status_read;
        return &g_pid_status[slot];
    }
    if (strcmp(name, "maps") == 0) {
        g_pid_pool_idx = (slot + 1) % PID_NODE_POOL;
        memset(&g_pid_maps[slot], 0, sizeof(fs_node_t));
        strcpy(g_pid_maps[slot].name, "maps");
        g_pid_maps[slot].flags = FS_FILE;
        g_pid_maps[slot].inode = pid;
        g_pid_maps[slot].read = proc_pid_maps_read;
        return &g_pid_maps[slot];
    }
    return NULL;
}

static int proc_pid_readdir(fs_node_t* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    (void)node;
    if (!inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    static const char* entries[] = { "status", "maps" };
    uint32_t idx = *inout_index;
    if (idx >= 2) return 0;

    struct vfs_dirent* d = (struct vfs_dirent*)buf;
    d->d_ino = 300 + idx;
    d->d_type = FS_FILE;
    d->d_reclen = sizeof(struct vfs_dirent);
    {
        const char* s = entries[idx];
        uint32_t j = 0;
        while (s[j] && j + 1 < sizeof(d->d_name)) { d->d_name[j] = s[j]; j++; }
        d->d_name[j] = 0;
    }
    *inout_index = idx + 1;
    return (int)sizeof(struct vfs_dirent);
}

static fs_node_t* proc_get_pid_dir(uint32_t pid) {
    if (!proc_find_pid(pid)) return NULL;
    uint32_t slot = g_pid_pool_idx;
    g_pid_pool_idx = (slot + 1) % PID_NODE_POOL;
    memset(&g_pid_dir[slot], 0, sizeof(fs_node_t));
    char num[16];
    itoa(pid, num, 10);
    strcpy(g_pid_dir[slot].name, num);
    g_pid_dir[slot].flags = FS_DIRECTORY;
    g_pid_dir[slot].inode = pid;
    g_pid_dir[slot].finddir = proc_pid_finddir;
    g_pid_dir[slot].readdir = proc_pid_readdir;
    return &g_pid_dir[slot];
}

/* --- /proc/self --- */

static fs_node_t* proc_self_finddir(fs_node_t* node, const char* name) {
    (void)node;
    if (strcmp(name, "status") == 0) return &g_proc_self_status;
    return NULL;
}

static int proc_self_readdir(fs_node_t* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    (void)node;
    if (!inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    static const char* entries[] = { "status" };
    uint32_t idx = *inout_index;
    if (idx >= 1) return 0;

    struct vfs_dirent* d = (struct vfs_dirent*)buf;
    d->d_ino = 100 + idx;
    d->d_type = 0;
    d->d_reclen = sizeof(struct vfs_dirent);
    {
        const char* s = entries[idx];
        uint32_t j = 0;
        while (s[j] && j + 1 < sizeof(d->d_name)) { d->d_name[j] = s[j]; j++; }
        d->d_name[j] = 0;
    }
    *inout_index = idx + 1;
    return (int)sizeof(struct vfs_dirent);
}

static int is_numeric(const char* s) {
    if (!s || !*s) return 0;
    while (*s) { if (*s < '0' || *s > '9') return 0; s++; }
    return 1;
}

static uint32_t parse_uint(const char* s) {
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t)(*s - '0'); s++; }
    return v;
}

static fs_node_t* proc_root_finddir(fs_node_t* node, const char* name) {
    (void)node;
    if (strcmp(name, "self") == 0) return &g_proc_self;
    if (strcmp(name, "uptime") == 0) return &g_proc_uptime;
    if (strcmp(name, "meminfo") == 0) return &g_proc_meminfo;
    if (strcmp(name, "cmdline") == 0) return &g_proc_cmdline;
    if (is_numeric(name)) return proc_get_pid_dir(parse_uint(name));
    return NULL;
}

static int proc_root_readdir(fs_node_t* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    (void)node;
    if (!inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    uint32_t idx = *inout_index;
    struct vfs_dirent* d = (struct vfs_dirent*)buf;

    static const char* fixed[] = { "self", "uptime", "meminfo", "cmdline" };
    if (idx < 4) {
        d->d_ino = 200 + idx;
        d->d_type = (idx == 0) ? FS_DIRECTORY : FS_FILE;
        d->d_reclen = sizeof(struct vfs_dirent);
        const char* s = fixed[idx];
        uint32_t j = 0;
        while (s[j] && j + 1 < sizeof(d->d_name)) { d->d_name[j] = s[j]; j++; }
        d->d_name[j] = 0;
        *inout_index = idx + 1;
        return (int)sizeof(struct vfs_dirent);
    }

    /* After fixed entries, list numeric PIDs */
    uint32_t pi = idx - 4;
    uint32_t count = 0;
    if (ready_queue_head) {
        struct process* it = ready_queue_head;
        const struct process* start = it;
        do {
            if (count == pi) {
                char num[16];
                itoa(it->pid, num, 10);
                d->d_ino = 400 + it->pid;
                d->d_type = FS_DIRECTORY;
                d->d_reclen = sizeof(struct vfs_dirent);
                uint32_t j = 0;
                while (num[j] && j + 1 < sizeof(d->d_name) && j < sizeof(num) - 1) { d->d_name[j] = num[j]; j++; }
                d->d_name[j] = 0;
                *inout_index = idx + 1;
                return (int)sizeof(struct vfs_dirent);
            }
            count++;
            it = it->next;
        } while (it && it != start);
    }

    return 0;
}

fs_node_t* procfs_create_root(void) {
    memset(&g_proc_root, 0, sizeof(g_proc_root));
    strcpy(g_proc_root.name, "proc");
    g_proc_root.flags = FS_DIRECTORY;
    g_proc_root.finddir = proc_root_finddir;
    g_proc_root.readdir = proc_root_readdir;

    memset(&g_proc_self, 0, sizeof(g_proc_self));
    strcpy(g_proc_self.name, "self");
    g_proc_self.flags = FS_DIRECTORY;
    g_proc_self.finddir = proc_self_finddir;
    g_proc_self.readdir = proc_self_readdir;

    memset(&g_proc_self_status, 0, sizeof(g_proc_self_status));
    strcpy(g_proc_self_status.name, "status");
    g_proc_self_status.flags = FS_FILE;
    g_proc_self_status.read = proc_self_status_read;

    memset(&g_proc_uptime, 0, sizeof(g_proc_uptime));
    strcpy(g_proc_uptime.name, "uptime");
    g_proc_uptime.flags = FS_FILE;
    g_proc_uptime.read = proc_uptime_read;

    memset(&g_proc_meminfo, 0, sizeof(g_proc_meminfo));
    strcpy(g_proc_meminfo.name, "meminfo");
    g_proc_meminfo.flags = FS_FILE;
    g_proc_meminfo.read = proc_meminfo_read;

    memset(&g_proc_cmdline, 0, sizeof(g_proc_cmdline));
    strcpy(g_proc_cmdline.name, "cmdline");
    g_proc_cmdline.flags = FS_FILE;
    g_proc_cmdline.read = proc_cmdline_read;

    return &g_proc_root;
}
