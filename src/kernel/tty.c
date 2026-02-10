#include "tty.h"

#include "keyboard.h"
#include "process.h"
#include "spinlock.h"
#include "uart_console.h"
#include "uaccess.h"
#include "errno.h"

#include "hal/cpu.h"
#include "utils.h"

#define TTY_LINE_MAX 256
#define TTY_CANON_BUF 1024
#define TTY_WAITQ_MAX 16

static spinlock_t tty_lock = {0};

static char line_buf[TTY_LINE_MAX];
static uint32_t line_len = 0;

static char canon_buf[TTY_CANON_BUF];
static uint32_t canon_head = 0;
static uint32_t canon_tail = 0;

static struct process* waitq[TTY_WAITQ_MAX];
static uint32_t waitq_head = 0;
static uint32_t waitq_tail = 0;

static uint32_t tty_lflag = TTY_ICANON | TTY_ECHO | TTY_ISIG;
static uint8_t tty_cc[NCCS] = {0, 0, 0, 0, 1, 0, 0, 0};

static struct winsize tty_winsize = { 24, 80, 0, 0 };

extern uint32_t get_tick_count(void);

static uint32_t tty_session_id = 0;
static uint32_t tty_fg_pgrp = 0;

enum {
    SIGTSTP = 20,
    SIGTTIN = 21,
    SIGTTOU = 22,
};

static int canon_empty(void) {
    return canon_head == canon_tail;
}

static uint32_t canon_count(void);
static int waitq_push(struct process* p);

int tty_write_kbuf(const void* kbuf, uint32_t len) {
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    // Job control: background writes to controlling TTY generate SIGTTOU.
    if (current_process && tty_session_id != 0 && current_process->session_id == tty_session_id &&
        tty_fg_pgrp != 0 && current_process->pgrp_id != tty_fg_pgrp) {
        (void)process_kill(current_process->pid, SIGTTOU);
        return -EINTR;
    }

    const char* p = (const char*)kbuf;
    for (uint32_t i = 0; i < len; i++) {
        uart_put_char(p[i]);
    }
    return (int)len;
}

static int tty_drain_locked(void* kbuf, uint32_t len) {
    uint32_t avail = canon_count();
    if (avail == 0) return 0;
    uint32_t n = (len < avail) ? len : avail;
    for (uint32_t i = 0; i < n; i++) {
        ((char*)kbuf)[i] = canon_buf[canon_tail];
        canon_tail = (canon_tail + 1U) % TTY_CANON_BUF;
    }
    return (int)n;
}

int tty_read_kbuf(void* kbuf, uint32_t len) {
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;
    if (!current_process) return -ECHILD;

    if (tty_session_id != 0 && current_process->session_id == tty_session_id &&
        tty_fg_pgrp != 0 && current_process->pgrp_id != tty_fg_pgrp) {
        (void)process_kill(current_process->pid, SIGTTIN);
        return -EINTR;
    }

    uintptr_t fl = spin_lock_irqsave(&tty_lock);
    int is_canon = (tty_lflag & TTY_ICANON) != 0;
    uint32_t vmin  = tty_cc[VMIN];
    uint32_t vtime = tty_cc[VTIME];
    spin_unlock_irqrestore(&tty_lock, fl);

    if (is_canon) {
        while (1) {
            uintptr_t flags = spin_lock_irqsave(&tty_lock);
            if (!canon_empty()) {
                int rc = tty_drain_locked(kbuf, len);
                spin_unlock_irqrestore(&tty_lock, flags);
                return rc;
            }
            if (waitq_push(current_process) == 0)
                current_process->state = PROCESS_BLOCKED;
            spin_unlock_irqrestore(&tty_lock, flags);
            hal_cpu_enable_interrupts();
            schedule();
        }
    }

    /* Non-canonical: VMIN=0,VTIME=0 => poll */
    if (vmin == 0 && vtime == 0) {
        uintptr_t flags = spin_lock_irqsave(&tty_lock);
        int rc = tty_drain_locked(kbuf, len);
        spin_unlock_irqrestore(&tty_lock, flags);
        return rc;
    }

    uint32_t target = vmin;
    if (target > len) target = len;
    if (target == 0) target = 1;

    /* VTIME in tenths-of-second => ticks at 50 Hz */
    uint32_t timeout_ticks = 0;
    if (vtime > 0) {
        timeout_ticks = (vtime * 5U);
        if (timeout_ticks == 0) timeout_ticks = 1;
    }

    uint32_t start = get_tick_count();

    while (1) {
        uintptr_t flags = spin_lock_irqsave(&tty_lock);
        uint32_t avail = canon_count();

        if (avail >= target) {
            int rc = tty_drain_locked(kbuf, len);
            spin_unlock_irqrestore(&tty_lock, flags);
            return rc;
        }

        if (vtime > 0) {
            uint32_t elapsed = get_tick_count() - start;
            if (elapsed >= timeout_ticks) {
                int rc = tty_drain_locked(kbuf, len);
                spin_unlock_irqrestore(&tty_lock, flags);
                return rc;
            }
        }

        if (waitq_push(current_process) == 0)
            current_process->state = PROCESS_BLOCKED;
        spin_unlock_irqrestore(&tty_lock, flags);
        hal_cpu_enable_interrupts();
        schedule();
    }
}

static uint32_t canon_count(void) {
    if (canon_head >= canon_tail) return canon_head - canon_tail;
    return (TTY_CANON_BUF - canon_tail) + canon_head;
}

int tty_can_read(void) {
    uintptr_t flags = spin_lock_irqsave(&tty_lock);
    int ready = canon_empty() ? 0 : 1;
    spin_unlock_irqrestore(&tty_lock, flags);
    return ready;
}

int tty_can_write(void) {
    return 1;
}

static void canon_push(char c) {
    uint32_t next = (canon_head + 1U) % TTY_CANON_BUF;
    if (next == canon_tail) {
        canon_tail = (canon_tail + 1U) % TTY_CANON_BUF;
    }
    canon_buf[canon_head] = c;
    canon_head = next;
}

static int waitq_empty(void) {
    return waitq_head == waitq_tail;
}

static int waitq_push(struct process* p) {
    uint32_t next = (waitq_head + 1U) % TTY_WAITQ_MAX;
    if (next == waitq_tail) return -1;
    waitq[waitq_head] = p;
    waitq_head = next;
    return 0;
}

static struct process* waitq_pop(void) {
    if (waitq_empty()) return NULL;
    struct process* p = waitq[waitq_tail];
    waitq_tail = (waitq_tail + 1U) % TTY_WAITQ_MAX;
    return p;
}

static void tty_wake_one(void) {
    struct process* p = waitq_pop();
    if (p && p->state == PROCESS_BLOCKED) {
        p->state = PROCESS_READY;
        sched_enqueue_ready(p);
    }
}

enum {
    TTY_TCGETS = 0x5401,
    TTY_TCSETS = 0x5402,
    TTY_TIOCGPGRP = 0x540F,
    TTY_TIOCSPGRP = 0x5410,
    TTY_TIOCGWINSZ = 0x5413,
    TTY_TIOCSWINSZ = 0x5414,
};

int tty_ioctl(uint32_t cmd, void* user_arg) {
    if (!user_arg) return -EFAULT;

    if (current_process && tty_session_id == 0 && current_process->session_id != 0) {
        tty_session_id = current_process->session_id;
        tty_fg_pgrp = current_process->pgrp_id;
    }

    if (cmd == TTY_TIOCGPGRP) {
        if (user_range_ok(user_arg, sizeof(int)) == 0) return -EFAULT;
        int fg = (int)tty_fg_pgrp;
        if (copy_to_user(user_arg, &fg, sizeof(fg)) < 0) return -EFAULT;
        return 0;
    }

    if (cmd == TTY_TIOCSPGRP) {
        if (user_range_ok(user_arg, sizeof(int)) == 0) return -EFAULT;
        int fg = 0;
        if (copy_from_user(&fg, user_arg, sizeof(fg)) < 0) return -EFAULT;
        if (!current_process) return -EINVAL;

        // If there is no controlling session yet, only allow setting fg=0.
        // This matches early-boot semantics used by userland smoke tests.
        if (tty_session_id == 0) {
            if (fg != 0) return -EPERM;
            tty_fg_pgrp = 0;
            return 0;
        }

        if (current_process->session_id != tty_session_id) return -EPERM;
        if (fg < 0) return -EINVAL;
        tty_fg_pgrp = (uint32_t)fg;
        return 0;
    }

    if (user_range_ok(user_arg, sizeof(struct termios)) == 0) return -EFAULT;

    if (cmd == TTY_TCGETS) {
        struct termios t;
        memset(&t, 0, sizeof(t));
        uintptr_t flags = spin_lock_irqsave(&tty_lock);
        t.c_lflag = tty_lflag;
        for (int i = 0; i < NCCS; i++) t.c_cc[i] = tty_cc[i];
        spin_unlock_irqrestore(&tty_lock, flags);
        if (copy_to_user(user_arg, &t, sizeof(t)) < 0) return -EFAULT;
        return 0;
    }

    if (cmd == TTY_TCSETS) {
        struct termios t;
        if (copy_from_user(&t, user_arg, sizeof(t)) < 0) return -EFAULT;
        uintptr_t flags = spin_lock_irqsave(&tty_lock);
        tty_lflag = t.c_lflag & (TTY_ICANON | TTY_ECHO | TTY_ISIG);
        for (int i = 0; i < NCCS; i++) tty_cc[i] = t.c_cc[i];
        spin_unlock_irqrestore(&tty_lock, flags);
        return 0;
    }

    if (cmd == TTY_TIOCGWINSZ) {
        if (user_range_ok(user_arg, sizeof(struct winsize)) == 0) return -EFAULT;
        if (copy_to_user(user_arg, &tty_winsize, sizeof(tty_winsize)) < 0) return -EFAULT;
        return 0;
    }

    if (cmd == TTY_TIOCSWINSZ) {
        if (user_range_ok(user_arg, sizeof(struct winsize)) == 0) return -EFAULT;
        if (copy_from_user(&tty_winsize, user_arg, sizeof(tty_winsize)) < 0) return -EFAULT;
        return 0;
    }

    return -EINVAL;
}

void tty_input_char(char c) {
    uintptr_t flags = spin_lock_irqsave(&tty_lock);
    uint32_t lflag = tty_lflag;

    enum { SIGINT_NUM = 2, SIGQUIT_NUM = 3, SIGTSTP_NUM = 20 };

    if (lflag & TTY_ISIG) {
        if (c == 0x03) {
            spin_unlock_irqrestore(&tty_lock, flags);
            if (lflag & TTY_ECHO) {
                uart_print("^C\n");
            }
            if (tty_fg_pgrp != 0) {
                process_kill_pgrp(tty_fg_pgrp, SIGINT_NUM);
            }
            return;
        }

        if (c == 0x1C) {
            spin_unlock_irqrestore(&tty_lock, flags);
            if (lflag & TTY_ECHO) {
                uart_print("^\\\n");
            }
            if (tty_fg_pgrp != 0) {
                process_kill_pgrp(tty_fg_pgrp, SIGQUIT_NUM);
            }
            return;
        }

        if (c == 0x1A) {
            spin_unlock_irqrestore(&tty_lock, flags);
            if (lflag & TTY_ECHO) {
                uart_print("^Z\n");
            }
            if (tty_fg_pgrp != 0) {
                process_kill_pgrp(tty_fg_pgrp, SIGTSTP_NUM);
            }
            return;
        }
    }

    if (c == 0x04 && (lflag & TTY_ICANON)) {
        if (lflag & TTY_ECHO) {
            uart_print("^D");
        }
        for (uint32_t i = 0; i < line_len; i++) {
            canon_push(line_buf[i]);
        }
        line_len = 0;
        tty_wake_one();
        spin_unlock_irqrestore(&tty_lock, flags);
        return;
    }

    if ((lflag & TTY_ICANON) == 0) {
        if (c == '\r') c = '\n';
        canon_push(c);
        tty_wake_one();
        if (lflag & TTY_ECHO) {
            uart_put_char(c);
        }
        spin_unlock_irqrestore(&tty_lock, flags);
        return;
    }

    if (c == '\b') {
        if (line_len > 0) {
            line_len--;
            if (lflag & TTY_ECHO) {
                uart_print("\b \b");
            }
        }
        spin_unlock_irqrestore(&tty_lock, flags);
        return;
    }

    if (c == '\r') c = '\n';

    if (c == '\n') {
        if (lflag & TTY_ECHO) {
            uart_put_char('\n');
        }

        for (uint32_t i = 0; i < line_len; i++) {
            canon_push(line_buf[i]);
        }
        canon_push('\n');
        line_len = 0;

        tty_wake_one();
        spin_unlock_irqrestore(&tty_lock, flags);
        return;
    }

    if (c >= ' ' && c <= '~') {
        if (line_len + 1 < sizeof(line_buf)) {
            line_buf[line_len++] = c;
            if (lflag & TTY_ECHO) {
                uart_put_char(c);
            }
        }
    }

    spin_unlock_irqrestore(&tty_lock, flags);
}

static void tty_keyboard_cb(char c) {
    tty_input_char(c);
}

void tty_init(void) {
    spinlock_init(&tty_lock);
    line_len = 0;
    canon_head = canon_tail = 0;
    waitq_head = waitq_tail = 0;
    tty_session_id = 0;
    tty_fg_pgrp = 0;

    keyboard_set_callback(tty_keyboard_cb);
}

int tty_write(const void* user_buf, uint32_t len) {
    if (!user_buf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;
    if (user_range_ok(user_buf, (size_t)len) == 0) return -EFAULT;

    // Job control: background writes to controlling TTY generate SIGTTOU.
    if (current_process && tty_session_id != 0 && current_process->session_id == tty_session_id &&
        tty_fg_pgrp != 0 && current_process->pgrp_id != tty_fg_pgrp) {
        (void)process_kill(current_process->pid, SIGTTOU);
        return -EINTR;
    }

    char kbuf[256];
    uint32_t remaining = len;
    uintptr_t up = (uintptr_t)user_buf;

    while (remaining) {
        uint32_t chunk = remaining;
        if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);

        if (copy_from_user(kbuf, (const void*)up, (size_t)chunk) < 0) return -EFAULT;

        for (uint32_t i = 0; i < chunk; i++) {
            uart_put_char(kbuf[i]);
        }

        up += chunk;
        remaining -= chunk;
    }

    return (int)len;
}

int tty_read(void* user_buf, uint32_t len) {
    if (!user_buf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;
    if (user_range_ok(user_buf, (size_t)len) == 0) return -EFAULT;

    char kbuf[256];
    uint32_t total = 0;
    while (total < len) {
        uint32_t chunk = len - total;
        if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);

        int rc = tty_read_kbuf(kbuf, chunk);
        if (rc < 0) return (total > 0) ? (int)total : rc;
        if (rc == 0) break;

        if (copy_to_user((uint8_t*)user_buf + total, kbuf, (size_t)rc) < 0)
            return -EFAULT;

        total += (uint32_t)rc;
        if ((uint32_t)rc < chunk) break;
    }
    return (int)total;
}
