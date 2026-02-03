#include "keyboard.h"
#include "idt.h"
#include "io.h"
#include "uart_console.h"
#include <stddef.h>

static keyboard_callback_t active_callback = NULL;

// US QWERTY Map (Scancodes 0x00 - 0x39)
// 0 means unmapped or special key
static char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', /* Backspace */
    '\t', /* Tab */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
    0, /* 29   - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, /* Right shift */
    '*',
    0,  /* Alt */
    ' ', /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
    -1, /* Minus */
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
    '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

void keyboard_handler_impl(struct registers* regs) {
    (void)regs;
    
    // Read status
    uint8_t status = inb(0x64);
    
    // Check if buffer is full
    if (status & 0x01) {
        uint8_t scancode = inb(0x60);
        
        // If highest bit set, it's a key release
        if (scancode & 0x80) {
            // TODO: Handle shift release
        } else {
            // Key Press
            if (scancode < 128) {
                char c = scancode_map[scancode];
                if (c != 0 && active_callback) {
                    active_callback(c);
                }
            }
        }
    }
}

void keyboard_init(void) {
    uart_print("[KBD] Initializing Keyboard Driver...\n");
    register_interrupt_handler(33, keyboard_handler_impl);
}

void keyboard_set_callback(keyboard_callback_t callback) {
    active_callback = callback;
}
