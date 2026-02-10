#include "pty.h"

#include "errno.h"
#include "process.h"
#include "spinlock.h"
#include "uaccess.h"

#include "hal/cpu.h"

#include <stddef.h>

#define PTY_BUF_CAP 1024
#define PTY_WAITQ_MAX 16

static spinlock_t pty_lock = {0};

static uint8_t m2s_buf[PTY_BUF_CAP];
static uint32_t m2s_head = 0;
static uint32_t m2s_tail = 0;

static uint8_t s2m_buf[PTY_BUF_CAP];
static uint32_t s2m_head = 0;
static uint32_t s2m_tail = 0;

static struct process* m2s_waitq[PTY_WAITQ_MAX];
static uint32_t m2s_wq_head = 0;
static uint32_t m2s_wq_tail = 0;

static struct process* s2m_waitq[PTY_WAITQ_MAX];
static uint32_t s2m_wq_head = 0;
static uint32_t s2m_wq_tail = 0;

static uint32_t pty_session_id = 0;
static uint32_t pty_fg_pgrp = 0;

enum {
    SIGTTIN = 21,
    SIGTTOU = 22,
};

enum {
    TTY_TIOCGPGRP = 0x540F,
    TTY_TIOCSPGRP = 0x5410,
};

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

void pty_init(void) {
    spinlock_init(&pty_lock);
    m2s_head = m2s_tail = 0;
    s2m_head = s2m_tail = 0;
    m2s_wq_head = m2s_wq_tail = 0;
    s2m_wq_head = s2m_wq_tail = 0;
    pty_session_id = 0;
    pty_fg_pgrp = 0;
}

static int pty_jobctl_write_check(void) {
    if (current_process && pty_session_id != 0 && current_process->session_id == pty_session_id &&
        pty_fg_pgrp != 0 && current_process->pgrp_id != pty_fg_pgrp) {
        (void)process_kill(current_process->pid, SIGTTOU);
        return -EINTR;
    }
    return 0;
}

static int pty_jobctl_read_check(void) {
    if (!current_process) return -ECHILD;
    if (pty_session_id != 0 && current_process->session_id == pty_session_id &&
        pty_fg_pgrp != 0 && current_process->pgrp_id != pty_fg_pgrp) {
        (void)process_kill(current_process->pid, SIGTTIN);
        return -EINTR;
    }
    return 0;
}

int pty_master_can_read(void) {
    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    int ready = (rb_count(s2m_head, s2m_tail) != 0U) ? 1 : 0;
    spin_unlock_irqrestore(&pty_lock, flags);
    return ready;
}

int pty_master_can_write(void) {
    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    int ready = (rb_free(m2s_head, m2s_tail) != 0U) ? 1 : 0;
    spin_unlock_irqrestore(&pty_lock, flags);
    return ready;
}

int pty_slave_can_read(void) {
    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    int ready = (rb_count(m2s_head, m2s_tail) != 0U) ? 1 : 0;
    spin_unlock_irqrestore(&pty_lock, flags);
    return ready;
}

int pty_slave_can_write(void) {
    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    int ready = (rb_free(s2m_head, s2m_tail) != 0U) ? 1 : 0;
    spin_unlock_irqrestore(&pty_lock, flags);
    return ready;
}

int pty_master_read_kbuf(void* kbuf, uint32_t len) {
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    int jc = pty_jobctl_read_check();
    if (jc < 0) return jc;

    while (1) {
        uintptr_t flags = spin_lock_irqsave(&pty_lock);
        uint32_t avail = rb_count(s2m_head, s2m_tail);
        if (avail != 0U) {
            uint32_t to_read = len;
            if (to_read > avail) to_read = avail;
            for (uint32_t i = 0; i < to_read; i++) {
                uint8_t c = 0;
                (void)rb_pop(s2m_buf, &s2m_head, &s2m_tail, &c);
                ((uint8_t*)kbuf)[i] = c;
            }
            spin_unlock_irqrestore(&pty_lock, flags);
            return (int)to_read;
        }

        if (current_process) {
            if (waitq_push(s2m_waitq, &s2m_wq_head, &s2m_wq_tail, current_process) == 0) {
                current_process->state = PROCESS_BLOCKED;
            }
        }

        spin_unlock_irqrestore(&pty_lock, flags);
        hal_cpu_enable_interrupts();
        schedule();
    }
}

int pty_master_write_kbuf(const void* kbuf, uint32_t len) {
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    int jc = pty_jobctl_write_check();
    if (jc < 0) return jc;

    enum { SIGINT_NUM = 2, SIGQUIT_NUM = 3, SIGTSTP_NUM = 20 };

    const uint8_t* bytes = (const uint8_t*)kbuf;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t ch = bytes[i];
        int sig = 0;
        if (ch == 0x03) sig = SIGINT_NUM;
        else if (ch == 0x1C) sig = SIGQUIT_NUM;
        else if (ch == 0x1A) sig = SIGTSTP_NUM;
        if (sig && pty_fg_pgrp != 0) {
            process_kill_pgrp(pty_fg_pgrp, sig);
        }
    }

    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    uint32_t free = rb_free(m2s_head, m2s_tail);
    uint32_t to_write = len;
    if (to_write > free) to_write = free;

    for (uint32_t i = 0; i < to_write; i++) {
        rb_push(m2s_buf, &m2s_head, &m2s_tail, ((const uint8_t*)kbuf)[i]);
    }

    if (to_write) {
        waitq_wake_one(m2s_waitq, &m2s_wq_head, &m2s_wq_tail);
    }

    spin_unlock_irqrestore(&pty_lock, flags);
    return (int)to_write;
}

int pty_slave_read_kbuf(void* kbuf, uint32_t len) {
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    int jc = pty_jobctl_read_check();
    if (jc < 0) return jc;

    while (1) {
        uintptr_t flags = spin_lock_irqsave(&pty_lock);
        uint32_t avail = rb_count(m2s_head, m2s_tail);
        if (avail != 0U) {
            uint32_t to_read = len;
            if (to_read > avail) to_read = avail;
            for (uint32_t i = 0; i < to_read; i++) {
                uint8_t c = 0;
                (void)rb_pop(m2s_buf, &m2s_head, &m2s_tail, &c);
                ((uint8_t*)kbuf)[i] = c;
            }
            spin_unlock_irqrestore(&pty_lock, flags);
            return (int)to_read;
        }

        if (current_process) {
            if (waitq_push(m2s_waitq, &m2s_wq_head, &m2s_wq_tail, current_process) == 0) {
                current_process->state = PROCESS_BLOCKED;
            }
        }

        spin_unlock_irqrestore(&pty_lock, flags);
        hal_cpu_enable_interrupts();
        schedule();
    }
}

int pty_slave_write_kbuf(const void* kbuf, uint32_t len) {
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    int jc = pty_jobctl_write_check();
    if (jc < 0) return jc;

    uintptr_t flags = spin_lock_irqsave(&pty_lock);
    uint32_t free = rb_free(s2m_head, s2m_tail);
    uint32_t to_write = len;
    if (to_write > free) to_write = free;

    for (uint32_t i = 0; i < to_write; i++) {
        rb_push(s2m_buf, &s2m_head, &s2m_tail, ((const uint8_t*)kbuf)[i]);
    }

    if (to_write) {
        waitq_wake_one(s2m_waitq, &s2m_wq_head, &s2m_wq_tail);
    }

    spin_unlock_irqrestore(&pty_lock, flags);
    return (int)to_write;
}

int pty_slave_ioctl(uint32_t cmd, void* user_arg) {
    if (!user_arg) return -EFAULT;

    if (current_process && pty_session_id == 0 && current_process->session_id != 0) {
        pty_session_id = current_process->session_id;
        pty_fg_pgrp = current_process->pgrp_id;
    }

    if (cmd == TTY_TIOCGPGRP) {
        if (user_range_ok(user_arg, sizeof(int)) == 0) return -EFAULT;
        int fg = (int)pty_fg_pgrp;
        if (copy_to_user(user_arg, &fg, sizeof(fg)) < 0) return -EFAULT;
        return 0;
    }

    if (cmd == TTY_TIOCSPGRP) {
        if (user_range_ok(user_arg, sizeof(int)) == 0) return -EFAULT;
        int fg = 0;
        if (copy_from_user(&fg, user_arg, sizeof(fg)) < 0) return -EFAULT;
        if (!current_process) return -EINVAL;

        if (pty_session_id == 0) {
            if (fg != 0) return -EPERM;
            pty_fg_pgrp = 0;
            return 0;
        }

        if (current_process->session_id != pty_session_id) return -EPERM;
        if (fg < 0) return -EINVAL;
        pty_fg_pgrp = (uint32_t)fg;
        return 0;
    }

    return -EINVAL;
}
