#include "keyboard.h"
#include "uart_console.h"
#include <stddef.h>

#include "hal/keyboard.h"

#include "process.h"
#include "spinlock.h"

static keyboard_callback_t active_callback = NULL;

#define KBD_BUF_SIZE 256
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;
static char kbd_buf[KBD_BUF_SIZE];

static spinlock_t kbd_lock = {0};
static struct process* kbd_waiter = NULL;

static void kbd_push_char(char c) {
    uint32_t next = (kbd_head + 1U) % KBD_BUF_SIZE;
    if (next == kbd_tail) {
        kbd_tail = (kbd_tail + 1U) % KBD_BUF_SIZE;
    }
    kbd_buf[kbd_head] = c;
    kbd_head = next;
}

static void hal_kbd_bridge(char c) {
    // IRQ context: push to buffer and wake a single blocked waiter (if any)
    uintptr_t flags = spin_lock_irqsave(&kbd_lock);
    kbd_push_char(c);

    if (kbd_waiter) {
        if (kbd_waiter->state == PROCESS_BLOCKED) {
            kbd_waiter->state = PROCESS_READY;
            sched_enqueue_ready(kbd_waiter);
        }
        kbd_waiter = NULL;
    }

    spin_unlock_irqrestore(&kbd_lock, flags);

    if (active_callback) {
        active_callback(c);
    }
}

void keyboard_init(void) {
    uart_print("[KBD] Initializing Keyboard Driver...\n");
    spinlock_init(&kbd_lock);
    kbd_head = 0;
    kbd_tail = 0;
    kbd_waiter = NULL;
    hal_keyboard_init(hal_kbd_bridge);
}

void keyboard_set_callback(keyboard_callback_t callback) {
    active_callback = callback;
}

int keyboard_read_nonblock(char* out, uint32_t max_len) {
    if (!out || max_len == 0) return 0;

    uintptr_t flags = spin_lock_irqsave(&kbd_lock);

    uint32_t count = 0;
    while (count < max_len) {
        if (kbd_tail == kbd_head) break;
        out[count++] = kbd_buf[kbd_tail];
        kbd_tail = (kbd_tail + 1U) % KBD_BUF_SIZE;
    }

    spin_unlock_irqrestore(&kbd_lock, flags);
    return (int)count;
}

int keyboard_read_blocking(char* out, uint32_t max_len) {
    if (!out || max_len == 0) return 0;
    if (!current_process) return 0;

    while (1) {
        int rd = keyboard_read_nonblock(out, max_len);
        if (rd > 0) return rd;

        uintptr_t flags = spin_lock_irqsave(&kbd_lock);
        if (kbd_waiter == NULL) {
            kbd_waiter = current_process;
            current_process->state = PROCESS_BLOCKED;
        }
        spin_unlock_irqrestore(&kbd_lock, flags);

        schedule();
    }
}
