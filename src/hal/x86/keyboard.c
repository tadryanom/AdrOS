#include "hal/keyboard.h"

#if defined(__i386__)
#include "idt.h"
#include "io.h"

static hal_keyboard_char_cb_t g_cb = 0;

static char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
    '*',
    0,
    ' ',
    0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    -1,
    0,
    0,
    0,
    '+',
    0,
    0,
    0,
    0,
    0,
    0, 0, 0,
    0,
    0,
    0,
};

static void kbd_irq(struct registers* regs) {
    (void)regs;

    uint8_t status = inb(0x64);
    if (status & 0x01) {
        uint8_t scancode = inb(0x60);

        if (!(scancode & 0x80)) {
            if (scancode < 128) {
                char c = scancode_map[scancode];
                if (c != 0 && g_cb) {
                    g_cb(c);
                }
            }
        }
    }
}

void hal_keyboard_init(hal_keyboard_char_cb_t cb) {
    g_cb = cb;
    register_interrupt_handler(33, kbd_irq);
}
#else
void hal_keyboard_init(hal_keyboard_char_cb_t cb) {
    (void)cb;
}
#endif
