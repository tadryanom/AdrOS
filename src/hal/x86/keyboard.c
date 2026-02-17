#include "hal/keyboard.h"

#if defined(__i386__)
#include "arch/x86/idt.h"
#include "io.h"
#include "vga_console.h"

static hal_keyboard_char_cb_t g_cb = 0;
static hal_keyboard_scan_cb_t g_scan_cb = 0;

/* Modifier state */
static volatile int shift_held = 0;
static volatile int ctrl_held = 0;
static volatile int alt_held = 0;

/* Extended scancode state (0xE0 prefix) */
static volatile int e0_prefix = 0;

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

static char scancode_map_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,
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

/* Emit a VT100 escape sequence through the char callback */
static void emit_escape_seq(const char* seq) {
    if (!g_cb) return;
    for (int i = 0; seq[i]; i++) {
        g_cb(seq[i]);
    }
}

/* PS/2 scan set 1 scancodes */
#define SC_LSHIFT_PRESS  0x2A
#define SC_RSHIFT_PRESS  0x36
#define SC_LSHIFT_REL    0xAA
#define SC_RSHIFT_REL    0xB6
#define SC_LCTRL_PRESS   0x1D
#define SC_LCTRL_REL     0x9D
#define SC_LALT_PRESS    0x38
#define SC_LALT_REL      0xB8

/* Extended (0xE0-prefixed) scancodes */
#define SC_E0_UP    0x48
#define SC_E0_DOWN  0x50
#define SC_E0_LEFT  0x4B
#define SC_E0_RIGHT 0x4D
#define SC_E0_HOME  0x47
#define SC_E0_END   0x4F
#define SC_E0_PGUP  0x49
#define SC_E0_PGDN  0x51
#define SC_E0_DEL   0x53

static void handle_extended_press(uint8_t sc) {
    switch (sc) {
    case SC_E0_UP:    emit_escape_seq("\033[A"); break;
    case SC_E0_DOWN:  emit_escape_seq("\033[B"); break;
    case SC_E0_RIGHT: emit_escape_seq("\033[C"); break;
    case SC_E0_LEFT:  emit_escape_seq("\033[D"); break;
    case SC_E0_HOME:  emit_escape_seq("\033[H"); break;
    case SC_E0_END:   emit_escape_seq("\033[F"); break;
    case SC_E0_PGUP:
        if (shift_held) {
            vga_scroll_back();
        } else {
            emit_escape_seq("\033[5~");
        }
        break;
    case SC_E0_PGDN:
        if (shift_held) {
            vga_scroll_fwd();
        } else {
            emit_escape_seq("\033[6~");
        }
        break;
    case SC_E0_DEL:   emit_escape_seq("\033[3~"); break;
    default: break;
    }
}

static void kbd_irq(struct registers* regs) {
    (void)regs;

    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return;

    uint8_t scancode = inb(0x60);

    /* Raw scancode callback (key press and release) */
    if (g_scan_cb) {
        g_scan_cb(scancode);
    }

    /* 0xE0 prefix: next byte is an extended scancode */
    if (scancode == 0xE0) {
        e0_prefix = 1;
        return;
    }

    if (e0_prefix) {
        e0_prefix = 0;
        /* Right CTRL (E0 1D / E0 9D) */
        if (scancode == SC_LCTRL_PRESS) { ctrl_held = 1; return; }
        if (scancode == SC_LCTRL_REL)  { ctrl_held = 0; return; }
        /* Right ALT (E0 38 / E0 B8) */
        if (scancode == SC_LALT_PRESS) { alt_held = 1; return; }
        if (scancode == SC_LALT_REL)  { alt_held = 0; return; }
        if (!(scancode & 0x80)) {
            handle_extended_press(scancode);
        }
        return;
    }

    /* Track modifier state */
    if (scancode == SC_LSHIFT_PRESS || scancode == SC_RSHIFT_PRESS) {
        shift_held = 1;
        return;
    }
    if (scancode == SC_LSHIFT_REL || scancode == SC_RSHIFT_REL) {
        shift_held = 0;
        return;
    }
    if (scancode == SC_LCTRL_PRESS) {
        ctrl_held = 1;
        return;
    }
    if (scancode == SC_LCTRL_REL) {
        ctrl_held = 0;
        return;
    }
    if (scancode == SC_LALT_PRESS) {
        alt_held = 1;
        return;
    }
    if (scancode == SC_LALT_REL) {
        alt_held = 0;
        return;
    }

    /* Ignore key releases for normal keys */
    if (scancode & 0x80) return;

    if (scancode < 128) {
        char c = shift_held ? scancode_map_shift[scancode] : scancode_map[scancode];
        if (c != 0 && g_cb) {
            if (ctrl_held && c >= 'a' && c <= 'z') {
                g_cb(c - 'a' + 1);  /* Ctrl+A=0x01 .. Ctrl+Z=0x1A */
            } else if (ctrl_held && c >= 'A' && c <= 'Z') {
                g_cb(c - 'A' + 1);
            } else if (alt_held) {
                g_cb('\033');  /* ESC prefix for Alt+key */
                g_cb(c);
            } else {
                g_cb(c);
            }
        }
    }
}

void hal_keyboard_init(hal_keyboard_char_cb_t cb) {
    g_cb = cb;
    register_interrupt_handler(33, kbd_irq);
}

void hal_keyboard_set_scancode_cb(hal_keyboard_scan_cb_t cb) {
    g_scan_cb = cb;
}
#else
void hal_keyboard_init(hal_keyboard_char_cb_t cb) {
    (void)cb;
}

void hal_keyboard_set_scancode_cb(hal_keyboard_scan_cb_t cb) {
    (void)cb;
}
#endif
