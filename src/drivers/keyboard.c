#include "keyboard.h"
#include "uart_console.h"
#include <stddef.h>

#include "hal/keyboard.h"

static keyboard_callback_t active_callback = NULL;

static void hal_kbd_bridge(char c) {
    if (active_callback) {
        active_callback(c);
    }
}

void keyboard_init(void) {
    uart_print("[KBD] Initializing Keyboard Driver...\n");
    hal_keyboard_init(hal_kbd_bridge);
}

void keyboard_set_callback(keyboard_callback_t callback) {
    active_callback = callback;
}
