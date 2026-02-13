#include "keyboard.h"
#include "devfs.h"
#include "console.h"
#include "utils.h"
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

/* --- Raw scancode ring buffer for /dev/kbd --- */

#define SCAN_BUF_SIZE 256
static volatile uint32_t scan_head = 0;
static volatile uint32_t scan_tail = 0;
static uint8_t scan_buf[SCAN_BUF_SIZE];
static spinlock_t scan_lock = {0};

static void scan_push(uint8_t sc) {
    uint32_t next = (scan_head + 1U) % SCAN_BUF_SIZE;
    if (next == scan_tail) {
        scan_tail = (scan_tail + 1U) % SCAN_BUF_SIZE;
    }
    scan_buf[scan_head] = sc;
    scan_head = next;
}

static void hal_scan_bridge(uint8_t scancode) {
    uintptr_t flags = spin_lock_irqsave(&scan_lock);
    scan_push(scancode);
    spin_unlock_irqrestore(&scan_lock, flags);
}

static uint32_t kbd_dev_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node; (void)offset;
    if (!buffer || size == 0) return 0;

    uintptr_t flags = spin_lock_irqsave(&scan_lock);
    uint32_t count = 0;
    while (count < size && scan_tail != scan_head) {
        buffer[count++] = scan_buf[scan_tail];
        scan_tail = (scan_tail + 1U) % SCAN_BUF_SIZE;
    }
    spin_unlock_irqrestore(&scan_lock, flags);
    return count;
}

static int kbd_dev_poll(fs_node_t* node, int events) {
    (void)node;
    int revents = 0;
    if (events & VFS_POLL_IN) {
        uintptr_t flags = spin_lock_irqsave(&scan_lock);
        if (scan_head != scan_tail) revents |= VFS_POLL_IN;
        spin_unlock_irqrestore(&scan_lock, flags);
    }
    if (events & VFS_POLL_OUT) revents |= VFS_POLL_OUT;
    return revents;
}

static fs_node_t g_dev_kbd_node;

void keyboard_init(void) {
    kprintf("[KBD] Initializing Keyboard Driver...\n");
    spinlock_init(&kbd_lock);
    spinlock_init(&scan_lock);
    kbd_head = 0;
    kbd_tail = 0;
    scan_head = 0;
    scan_tail = 0;
    kbd_waiter = NULL;
    hal_keyboard_init(hal_kbd_bridge);
    hal_keyboard_set_scancode_cb(hal_scan_bridge);
}

void keyboard_register_devfs(void) {
    static const struct file_operations kbd_fops = {
        .read = kbd_dev_read,
        .poll = kbd_dev_poll,
    };

    memset(&g_dev_kbd_node, 0, sizeof(g_dev_kbd_node));
    strcpy(g_dev_kbd_node.name, "kbd");
    g_dev_kbd_node.flags = FS_CHARDEVICE;
    g_dev_kbd_node.inode = 21;
    g_dev_kbd_node.f_ops = &kbd_fops;
    g_dev_kbd_node.read = &kbd_dev_read;
    g_dev_kbd_node.poll = &kbd_dev_poll;
    devfs_register_device(&g_dev_kbd_node);
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
