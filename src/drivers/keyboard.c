#include "keyboard.h"
#include "uart_console.h"
#include <stddef.h>

#include "hal/keyboard.h"

static keyboard_callback_t active_callback = NULL;

#define KBD_BUF_SIZE 256
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;
static char kbd_buf[KBD_BUF_SIZE];

static void kbd_push_char(char c) {
    uint32_t next = (kbd_head + 1U) % KBD_BUF_SIZE;
    if (next == kbd_tail) {
        kbd_tail = (kbd_tail + 1U) % KBD_BUF_SIZE;
    }
    kbd_buf[kbd_head] = c;
    kbd_head = next;
}

static void hal_kbd_bridge(char c) {
    kbd_push_char(c);
    if (active_callback) {
        active_callback(c);
    }
}

void keyboard_init(void) {
    uart_print("[KBD] Initializing Keyboard Driver...\n");
    kbd_head = 0;
    kbd_tail = 0;
    hal_keyboard_init(hal_kbd_bridge);
}

void keyboard_set_callback(keyboard_callback_t callback) {
    active_callback = callback;
}

int keyboard_read_nonblock(char* out, uint32_t max_len) {
    if (!out || max_len == 0) return 0;

    uint32_t count = 0;
    while (count < max_len) {
        if (kbd_tail == kbd_head) break;
        out[count++] = kbd_buf[kbd_tail];
        kbd_tail = (kbd_tail + 1U) % KBD_BUF_SIZE;
    }
    return (int)count;
}
