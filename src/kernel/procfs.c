#include "procfs.h"

#include "process.h"
#include "utils.h"
#include "heap.h"
#include "pmm.h"
#include "timer.h"

#include <stddef.h>

static fs_node_t g_proc_root;
static fs_node_t g_proc_self;
static fs_node_t g_proc_self_status;
static fs_node_t g_proc_uptime;
static fs_node_t g_proc_meminfo;

extern struct process* ready_queue_head;

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

static fs_node_t* proc_root_finddir(fs_node_t* node, const char* name) {
    (void)node;
    if (strcmp(name, "self") == 0) return &g_proc_self;
    if (strcmp(name, "uptime") == 0) return &g_proc_uptime;
    if (strcmp(name, "meminfo") == 0) return &g_proc_meminfo;
    return NULL;
}

static int proc_root_readdir(fs_node_t* node, uint32_t* inout_index, void* buf, uint32_t buf_len) {
    (void)node;
    if (!inout_index || !buf) return -1;
    if (buf_len < sizeof(struct vfs_dirent)) return -1;

    static const char* entries[] = { "self", "uptime", "meminfo" };
    uint32_t idx = *inout_index;
    if (idx >= 3) return 0;

    struct vfs_dirent* d = (struct vfs_dirent*)buf;
    d->d_ino = 200 + idx;
    d->d_type = (idx == 0) ? 2 : 0;
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

    return &g_proc_root;
}
