#include "tty.h"

#include "keyboard.h"
#include "process.h"
#include "spinlock.h"
#include "uart_console.h"
#include "uaccess.h"
#include "errno.h"

#include "hal/cpu.h"

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

static int canon_empty(void) {
    return canon_head == canon_tail;
}

static uint32_t canon_count(void);
static int waitq_push(struct process* p);

int tty_write_kbuf(const void* kbuf, uint32_t len) {
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;

    const char* p = (const char*)kbuf;
    for (uint32_t i = 0; i < len; i++) {
        uart_put_char(p[i]);
    }
    return (int)len;
}

int tty_read_kbuf(void* kbuf, uint32_t len) {
    if (!kbuf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;
    if (!current_process) return -ECHILD;

    while (1) {
        uintptr_t flags = spin_lock_irqsave(&tty_lock);

        if (!canon_empty()) {
            uint32_t avail = canon_count();
            uint32_t to_read = len;
            if (to_read > avail) to_read = avail;

            uint32_t total = 0;
            while (total < to_read) {
                uint32_t chunk = to_read - total;
                if (chunk > 256U) chunk = 256U;

                for (uint32_t i = 0; i < chunk; i++) {
                    ((char*)kbuf)[total + i] = canon_buf[canon_tail];
                    canon_tail = (canon_tail + 1U) % TTY_CANON_BUF;
                }
                total += chunk;
            }

            spin_unlock_irqrestore(&tty_lock, flags);
            return (int)to_read;
        }

        if (waitq_push(current_process) == 0) {
            current_process->state = PROCESS_BLOCKED;
        }

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
    }
}

void tty_input_char(char c) {
    uintptr_t flags = spin_lock_irqsave(&tty_lock);

    if (c == '\b') {
        if (line_len > 0) {
            line_len--;
            uart_print("\b \b");
        }
        spin_unlock_irqrestore(&tty_lock, flags);
        return;
    }

    if (c == '\r') c = '\n';

    if (c == '\n') {
        uart_put_char('\n');

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
            uart_put_char(c);
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

    keyboard_set_callback(tty_keyboard_cb);
}

int tty_write(const void* user_buf, uint32_t len) {
    if (!user_buf) return -EFAULT;
    if (len > 1024 * 1024) return -EINVAL;
    if (user_range_ok(user_buf, (size_t)len) == 0) return -EFAULT;

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
    if (!current_process) return -ECHILD;

    while (1) {
        uintptr_t flags = spin_lock_irqsave(&tty_lock);

        if (!canon_empty()) {
            uint32_t avail = canon_count();
            uint32_t to_read = len;
            if (to_read > avail) to_read = avail;

            char kbuf[256];
            uint32_t total = 0;
            while (total < to_read) {
                uint32_t chunk = to_read - total;
                if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);

                for (uint32_t i = 0; i < chunk; i++) {
                    kbuf[i] = canon_buf[canon_tail];
                    canon_tail = (canon_tail + 1U) % TTY_CANON_BUF;
                }

                spin_unlock_irqrestore(&tty_lock, flags);

                if (copy_to_user((uint8_t*)user_buf + total, kbuf, (size_t)chunk) < 0) return -EFAULT;

                total += chunk;
                flags = spin_lock_irqsave(&tty_lock);
            }

            spin_unlock_irqrestore(&tty_lock, flags);
            return (int)to_read;
        }

        if (waitq_push(current_process) == 0) {
            current_process->state = PROCESS_BLOCKED;
        }

        spin_unlock_irqrestore(&tty_lock, flags);

        hal_cpu_enable_interrupts();
        schedule();
    }
}
