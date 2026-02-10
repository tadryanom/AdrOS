#include "pty.h"

#include "errno.h"
#include "process.h"
#include "spinlock.h"
#include "uaccess.h"
#include "utils.h"

#include "hal/cpu.h"

#include <stddef.h>

#define PTY_BUF_CAP 1024
#define PTY_WAITQ_MAX 16

enum {
    SIGTTIN = 21,
    SIGTTOU = 22,
};

enum {
    TTY_TIOCGPGRP = 0x540F,
    TTY_TIOCSPGRP = 0x5410,
};

struct pty_pair {
    uint8_t  m2s_buf[PTY_BUF_CAP];
    uint32_t m2s_head;
    uint32_t m2s_tail;

    uint8_t  s2m_buf[PTY_BUF_CAP];
    uint32_t s2m_head;
    uint32_t s2m_tail;

    struct process* m2s_waitq[PTY_WAITQ_MAX];
    uint32_t m2s_wq_head;
    uint32_t m2s_wq_tail;

    struct process* s2m_waitq[PTY_WAITQ_MAX];
    uint32_t s2m_wq_head;
    uint32_t s2m_wq_tail;

    uint32_t session_id;
    uint32_t fg_pgrp;
    int      active;

    fs_node_t master_node;
    fs_node_t slave_node;
};

static spinlock_t pty_lock = {0};
static struct pty_pair g_ptys[PTY_MAX_PAIRS];
static int g_pty_count = 0;

static uint32_t rb_count(uint32_t head, uint32_t tail) {
    if (head >= tail) return head - tail;
    return (PTY_BUF_CAP - tail) + head;
}

static uint32_t rb_free(uint32_t head, uint32_t tail) {
    return (PTY_BUF_CAP - 1U) - rb_count(head, tail);
}

static void rb_push(uint8_t* buf, uint32_t* head, uint32_t* tail, uint8_t c) {
    uint32_t next = (*head + 1U) % PTY_BUF_CAP;
    if (next == *tail) {
        *tail = (*tail + 1U) % PTY_BUF_CAP;
    }
    buf[*head] = c;
    *head = next;
}

static int rb_pop(uint8_t* buf, uint32_t* head, uint32_t* tail, uint8_t* out) {
    if (*head == *tail) return 0;
    *out = buf[*tail];
    *tail = (*tail + 1U) % PTY_BUF_CAP;
    return 1;
}

static int waitq_push(struct process** q, uint32_t* head, uint32_t* tail, struct process* p) {
    uint32_t next = (*head + 1U) % PTY_WAITQ_MAX;
    if (next == *tail) return -1;
    q[*head] = p;
    *head = next;
    return 0;
}

static struct process* waitq_pop(struct process** q, uint32_t* head, uint32_t* tail) {
    if (*head == *tail) return NULL;
    struct process* p = q[*tail];
    *tail = (*tail + 1U) % PTY_WAITQ_MAX;
    return p;
}

static void waitq_wake_one(struct process** q, uint32_t* head, uint32_t* tail) {
    struct process* p = waitq_pop(q, head, tail);
    if (p && p->state == PROCESS_BLOCKED) {
        p->state = PROCESS_READY;
        sched_enqueue_ready(p);
    }
}

static uint32_t pty_master_read_fn(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static uint32_t pty_master_write_fn(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
static uint32_t pty_slave_read_fn(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static uint32_t pty_slave_write_fn(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);

static void pty_init_pair(int idx) {
    struct pty_pair* p = &g_ptys[idx];
    memset(p, 0, sizeof(*p));
    p->active = 1;

    char name[8];
    memset(&p->master_node, 0, sizeof(p->master_node));
    strcpy(p->master_node.name, "ptmx");
    p->master_node.flags = FS_CHARDEVICE;
    p->master_node.inode = PTY_MASTER_INO_BASE + (uint32_t)idx;
    p->master_node.read = &pty_master_read_fn;
    p->master_node.write = &pty_master_write_fn;

    memset(&p->slave_node, 0, sizeof(p->slave_node));
    name[0] = '0' + (char)idx;
    name[1] = '\0';
    strcpy(p->slave_node.name, name);
    p->slave_node.flags = FS_CHARDEVICE;
    p->slave_node.inode = PTY_SLAVE_INO_BASE + (uint32_t)idx;
    p->slave_node.read = &pty_slave_read_fn;
    p->slave_node.write = &pty_slave_write_fn;
}

void pty_init(void) {
    spinlock_init(&pty_lock);
    memset(g_ptys, 0, sizeof(g_ptys));
    g_pty_count = 0;
    pty_init_pair(0);
    g_pty_count = 1;
}

int pty_alloc_pair(void) {
    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    if (g_pty_count >= PTY_MAX_PAIRS) {
        spin_unlock_irqrestore(&pty_lock, flags);
        return -ENOMEM;
    }
    int idx = g_pty_count;
    g_pty_count++;
    spin_unlock_irqrestore(&pty_lock, flags);
    pty_init_pair(idx);
    return idx;
}

int pty_pair_count(void) {
    return g_pty_count;
}

int pty_pair_active(int idx) {
    if (idx < 0 || idx >= g_pty_count) return 0;
    return g_ptys[idx].active;
}

fs_node_t* pty_get_master_node(int idx) {
    if (idx < 0 || idx >= g_pty_count) return NULL;
    return &g_ptys[idx].master_node;
}

fs_node_t* pty_get_slave_node(int idx) {
    if (idx < 0 || idx >= g_pty_count) return NULL;
    return &g_ptys[idx].slave_node;
}

static int pty_jobctl_write_check(struct pty_pair* p) {
    if (current_process && p->session_id != 0 && current_process->session_id == p->session_id &&
        p->fg_pgrp != 0 && current_process->pgrp_id != p->fg_pgrp) {
        (void)process_kill(current_process->pid, SIGTTOU);
        return -EINTR;
    }
    return 0;
}

static int pty_jobctl_read_check(struct pty_pair* p) {
    if (!current_process) return -ECHILD;
    if (p->session_id != 0 && current_process->session_id == p->session_id &&
        p->fg_pgrp != 0 && current_process->pgrp_id != p->fg_pgrp) {
        (void)process_kill(current_process->pid, SIGTTIN);
        return -EINTR;
    }
    return 0;
}

int pty_master_can_read_idx(int idx) {
    if (idx < 0 || idx >= g_pty_count) return 0;
    struct pty_pair* p = &g_ptys[idx];
    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    int ready = (rb_count(p->s2m_head, p->s2m_tail) != 0U) ? 1 : 0;
    spin_unlock_irqrestore(&pty_lock, flags);
    return ready;
}

int pty_master_can_write_idx(int idx) {
    if (idx < 0 || idx >= g_pty_count) return 0;
    struct pty_pair* p = &g_ptys[idx];
    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    int ready = (rb_free(p->m2s_head, p->m2s_tail) != 0U) ? 1 : 0;
    spin_unlock_irqrestore(&pty_lock, flags);
    return ready;
}

int pty_slave_can_read_idx(int idx) {
    if (idx < 0 || idx >= g_pty_count) return 0;
    struct pty_pair* p = &g_ptys[idx];
    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    int ready = (rb_count(p->m2s_head, p->m2s_tail) != 0U) ? 1 : 0;
    spin_unlock_irqrestore(&pty_lock, flags);
    return ready;
}

int pty_slave_can_write_idx(int idx) {
    if (idx < 0 || idx >= g_pty_count) return 0;
    struct pty_pair* p = &g_ptys[idx];
    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    int ready = (rb_free(p->s2m_head, p->s2m_tail) != 0U) ? 1 : 0;
    spin_unlock_irqrestore(&pty_lock, flags);
    return ready;
}

int pty_master_read_idx(int idx, void* kbuf, uint32_t len) {
    if (idx < 0 || idx >= g_pty_count) return -ENODEV;
    struct pty_pair* p = &g_ptys[idx];
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    int jc = pty_jobctl_read_check(p);
    if (jc < 0) return jc;

    while (1) {
        uintptr_t flags = spin_lock_irqsave(&pty_lock);
        uint32_t avail = rb_count(p->s2m_head, p->s2m_tail);
        if (avail != 0U) {
            uint32_t to_read = len;
            if (to_read > avail) to_read = avail;
            for (uint32_t i = 0; i < to_read; i++) {
                uint8_t c = 0;
                (void)rb_pop(p->s2m_buf, &p->s2m_head, &p->s2m_tail, &c);
                ((uint8_t*)kbuf)[i] = c;
            }
            spin_unlock_irqrestore(&pty_lock, flags);
            return (int)to_read;
        }

        if (current_process) {
            if (waitq_push(p->s2m_waitq, &p->s2m_wq_head, &p->s2m_wq_tail, current_process) == 0) {
                current_process->state = PROCESS_BLOCKED;
            }
        }

        spin_unlock_irqrestore(&pty_lock, flags);
        hal_cpu_enable_interrupts();
        schedule();
    }
}

int pty_master_write_idx(int idx, const void* kbuf, uint32_t len) {
    if (idx < 0 || idx >= g_pty_count) return -ENODEV;
    struct pty_pair* p = &g_ptys[idx];
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    int jc = pty_jobctl_write_check(p);
    if (jc < 0) return jc;

    enum { SIGINT_NUM = 2, SIGQUIT_NUM = 3, SIGTSTP_NUM = 20 };

    const uint8_t* bytes = (const uint8_t*)kbuf;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t ch = bytes[i];
        int sig = 0;
        if (ch == 0x03) sig = SIGINT_NUM;
        else if (ch == 0x1C) sig = SIGQUIT_NUM;
        else if (ch == 0x1A) sig = SIGTSTP_NUM;
        if (sig && p->fg_pgrp != 0) {
            process_kill_pgrp(p->fg_pgrp, sig);
        }
    }

    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    uint32_t free_space = rb_free(p->m2s_head, p->m2s_tail);
    uint32_t to_write = len;
    if (to_write > free_space) to_write = free_space;

    for (uint32_t i = 0; i < to_write; i++) {
        rb_push(p->m2s_buf, &p->m2s_head, &p->m2s_tail, ((const uint8_t*)kbuf)[i]);
    }

    if (to_write) {
        waitq_wake_one(p->m2s_waitq, &p->m2s_wq_head, &p->m2s_wq_tail);
    }

    spin_unlock_irqrestore(&pty_lock, flags);
    return (int)to_write;
}

int pty_slave_read_idx(int idx, void* kbuf, uint32_t len) {
    if (idx < 0 || idx >= g_pty_count) return -ENODEV;
    struct pty_pair* p = &g_ptys[idx];
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    int jc = pty_jobctl_read_check(p);
    if (jc < 0) return jc;

    while (1) {
        uintptr_t flags = spin_lock_irqsave(&pty_lock);
        uint32_t avail = rb_count(p->m2s_head, p->m2s_tail);
        if (avail != 0U) {
            uint32_t to_read = len;
            if (to_read > avail) to_read = avail;
            for (uint32_t i = 0; i < to_read; i++) {
                uint8_t c = 0;
                (void)rb_pop(p->m2s_buf, &p->m2s_head, &p->m2s_tail, &c);
                ((uint8_t*)kbuf)[i] = c;
            }
            spin_unlock_irqrestore(&pty_lock, flags);
            return (int)to_read;
        }

        if (current_process) {
            if (waitq_push(p->m2s_waitq, &p->m2s_wq_head, &p->m2s_wq_tail, current_process) == 0) {
                current_process->state = PROCESS_BLOCKED;
            }
        }

        spin_unlock_irqrestore(&pty_lock, flags);
        hal_cpu_enable_interrupts();
        schedule();
    }
}

int pty_slave_write_idx(int idx, const void* kbuf, uint32_t len) {
    if (idx < 0 || idx >= g_pty_count) return -ENODEV;
    struct pty_pair* p = &g_ptys[idx];
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    int jc = pty_jobctl_write_check(p);
    if (jc < 0) return jc;

    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    uint32_t free_space = rb_free(p->s2m_head, p->s2m_tail);
    uint32_t to_write = len;
    if (to_write > free_space) to_write = free_space;

    for (uint32_t i = 0; i < to_write; i++) {
        rb_push(p->s2m_buf, &p->s2m_head, &p->s2m_tail, ((const uint8_t*)kbuf)[i]);
    }

    if (to_write) {
        waitq_wake_one(p->s2m_waitq, &p->s2m_wq_head, &p->s2m_wq_tail);
    }

    spin_unlock_irqrestore(&pty_lock, flags);
    return (int)to_write;
}

int pty_slave_ioctl_idx(int idx, uint32_t cmd, void* user_arg) {
    if (idx < 0 || idx >= g_pty_count) return -ENODEV;
    struct pty_pair* p = &g_ptys[idx];
    if (!user_arg) return -EFAULT;

    if (current_process && p->session_id == 0 && current_process->session_id != 0) {
        p->session_id = current_process->session_id;
        p->fg_pgrp = current_process->pgrp_id;
    }

    if (cmd == TTY_TIOCGPGRP) {
        if (user_range_ok(user_arg, sizeof(int)) == 0) return -EFAULT;
        int fg = (int)p->fg_pgrp;
        if (copy_to_user(user_arg, &fg, sizeof(fg)) < 0) return -EFAULT;
        return 0;
    }

    if (cmd == TTY_TIOCSPGRP) {
        if (user_range_ok(user_arg, sizeof(int)) == 0) return -EFAULT;
        int fg = 0;
        if (copy_from_user(&fg, user_arg, sizeof(fg)) < 0) return -EFAULT;
        if (!current_process) return -EINVAL;

        if (p->session_id == 0) {
            if (fg != 0) return -EPERM;
            p->fg_pgrp = 0;
            return 0;
        }

        if (current_process->session_id != p->session_id) return -EPERM;
        if (fg < 0) return -EINVAL;
        p->fg_pgrp = (uint32_t)fg;
        return 0;
    }

    return -EINVAL;
}

static uint32_t pty_master_read_fn(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    int idx = pty_ino_to_idx(node->inode);
    int rc = pty_master_read_idx(idx, buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static uint32_t pty_master_write_fn(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)offset;
    int idx = pty_ino_to_idx(node->inode);
    int rc = pty_master_write_idx(idx, buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static uint32_t pty_slave_read_fn(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    int idx = pty_ino_to_idx(node->inode);
    int rc = pty_slave_read_idx(idx, buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

static uint32_t pty_slave_write_fn(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)offset;
    int idx = pty_ino_to_idx(node->inode);
    int rc = pty_slave_write_idx(idx, buffer, size);
    if (rc < 0) return 0;
    return (uint32_t)rc;
}

int pty_master_can_read(void)  { return pty_master_can_read_idx(0); }
int pty_master_can_write(void) { return pty_master_can_write_idx(0); }
int pty_slave_can_read(void)   { return pty_slave_can_read_idx(0); }
int pty_slave_can_write(void)  { return pty_slave_can_write_idx(0); }

int pty_master_read_kbuf(void* kbuf, uint32_t len)        { return pty_master_read_idx(0, kbuf, len); }
int pty_master_write_kbuf(const void* kbuf, uint32_t len)  { return pty_master_write_idx(0, kbuf, len); }
int pty_slave_read_kbuf(void* kbuf, uint32_t len)         { return pty_slave_read_idx(0, kbuf, len); }
int pty_slave_write_kbuf(const void* kbuf, uint32_t len)   { return pty_slave_write_idx(0, kbuf, len); }
int pty_slave_ioctl(uint32_t cmd, void* user_arg)          { return pty_slave_ioctl_idx(0, cmd, user_arg); }
