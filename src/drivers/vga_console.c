#include "vga_console.h"

#include "hal/video.h"

#include "spinlock.h"

static volatile uint16_t* VGA_BUFFER = 0;
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

static int term_col = 0;
static int term_row = 0;
static uint8_t term_color = 0x0F; // White on Black

static spinlock_t vga_lock = {0};

/* --- Scrollback buffer --- */
#define SB_LINES 200
static uint16_t sb_buf[SB_LINES * VGA_WIDTH];
static int sb_head  = 0;  /* next write line (circular) */
static int sb_count = 0;  /* stored lines (max SB_LINES) */

static int view_offset = 0; /* 0 = live view, >0 = scrolled back N lines */
static uint16_t live_buf[VGA_HEIGHT * VGA_WIDTH]; /* saved live VGA when scrolled */

static void vga_update_hw_cursor(void) {
    hal_video_set_cursor(term_row, term_col);
}

static void vga_scroll(void) {
    /* Save row 0 (about to be lost) into scrollback ring */
    for (int x = 0; x < VGA_WIDTH; x++) {
        sb_buf[sb_head * VGA_WIDTH + x] = VGA_BUFFER[x];
    }
    sb_head = (sb_head + 1) % SB_LINES;
    if (sb_count < SB_LINES) sb_count++;

    /* Shift visible content up */
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = VGA_BUFFER[y * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)' ' | (uint16_t)term_color << 8;
    }
    term_row = VGA_HEIGHT - 1;
}

/* Restore live view if currently scrolled back */
static void vga_unscroll(void) {
    if (view_offset > 0) {
        for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            VGA_BUFFER[i] = live_buf[i];
        }
        view_offset = 0;
    }
}

/* Render scrollback + live content at current view_offset */
static void render_scrollback_view(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        /*
         * line_from_end: how far this row is from the bottom of live content.
         * 0..VGA_HEIGHT-1 = live rows, VGA_HEIGHT+ = scrollback
         */
        int line_from_end = view_offset + (VGA_HEIGHT - 1 - y);

        if (line_from_end < VGA_HEIGHT) {
            /* Live content */
            int live_row = VGA_HEIGHT - 1 - line_from_end;
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFFER[y * VGA_WIDTH + x] = live_buf[live_row * VGA_WIDTH + x];
            }
        } else {
            /* Scrollback: sb_idx 0 = most recent scrolled-off line */
            int sb_idx = line_from_end - VGA_HEIGHT;
            if (sb_idx < sb_count) {
                int buf_line = (sb_head - 1 - sb_idx + SB_LINES) % SB_LINES;
                for (int x = 0; x < VGA_WIDTH; x++) {
                    VGA_BUFFER[y * VGA_WIDTH + x] = sb_buf[buf_line * VGA_WIDTH + x];
                }
            } else {
                /* Beyond scrollback — blank */
                for (int x = 0; x < VGA_WIDTH; x++) {
                    VGA_BUFFER[y * VGA_WIDTH + x] = (uint16_t)' ' | (uint16_t)term_color << 8;
                }
            }
        }
    }
    /* Hide cursor when scrolled back */
    hal_video_set_cursor(VGA_HEIGHT, 0);
}

static void vga_put_char_unlocked(char c) {
    if (!VGA_BUFFER) return;

    /* Any new output auto-returns to live view */
    vga_unscroll();

    switch (c) {
    case '\n':
        term_col = 0;
        term_row++;
        break;
    case '\r':
        term_col = 0;
        break;
    case '\b':
        if (term_col > 0) {
            term_col--;
        } else if (term_row > 0) {
            term_row--;
            term_col = VGA_WIDTH - 1;
        }
        break;
    case '\t':
        term_col = (term_col + 8) & ~7;
        if (term_col >= VGA_WIDTH) {
            term_col = 0;
            term_row++;
        }
        break;
    default:
        if ((unsigned char)c >= ' ') {
            const int index = term_row * VGA_WIDTH + term_col;
            VGA_BUFFER[index] = (uint16_t)(unsigned char)c | (uint16_t)term_color << 8;
            term_col++;
        }
        break;
    }

    if (term_col >= VGA_WIDTH) {
        term_col = 0;
        term_row++;
    }

    if (term_row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_init(void) {
    VGA_BUFFER = (volatile uint16_t*)hal_video_text_buffer();
    term_col = 0;
    term_row = 0;
    term_color = 0x07; // Light Grey on Black

    if (!VGA_BUFFER) {
        return;
    }
    
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int index = y * VGA_WIDTH + x;
            VGA_BUFFER[index] = (uint16_t) ' ' | (uint16_t) term_color << 8;
        }
    }
    vga_update_hw_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);
    term_color = fg | (bg << 4);
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_put_char(char c) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);
    vga_put_char_unlocked(c);
    vga_update_hw_cursor();
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_print(const char* str) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);

    if (!VGA_BUFFER) {
        spin_unlock_irqrestore(&vga_lock, flags);
        return;
    }

    for (int i = 0; str[i] != '\0'; i++) {
        vga_put_char_unlocked(str[i]);
    }

    vga_update_hw_cursor();
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_clear(void) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);

    if (!VGA_BUFFER) {
        spin_unlock_irqrestore(&vga_lock, flags);
        return;
    }

    for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        VGA_BUFFER[i] = (uint16_t)' ' | (uint16_t)term_color << 8;
    }
    term_col = 0;
    term_row = 0;
    view_offset = 0;
    sb_count = 0;
    sb_head = 0;
    vga_update_hw_cursor();

    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_scroll_back(void) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);

    if (!VGA_BUFFER || sb_count == 0) {
        spin_unlock_irqrestore(&vga_lock, flags);
        return;
    }

    if (view_offset == 0) {
        /* First scroll back — save current live screen */
        for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            live_buf[i] = VGA_BUFFER[i];
        }
    }

    view_offset += VGA_HEIGHT / 2;
    if (view_offset > sb_count) view_offset = sb_count;

    render_scrollback_view();
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_scroll_fwd(void) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);

    if (!VGA_BUFFER || view_offset == 0) {
        spin_unlock_irqrestore(&vga_lock, flags);
        return;
    }

    if (view_offset <= VGA_HEIGHT / 2) {
        /* Return to live view */
        for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            VGA_BUFFER[i] = live_buf[i];
        }
        view_offset = 0;
        vga_update_hw_cursor();
    } else {
        view_offset -= VGA_HEIGHT / 2;
        render_scrollback_view();
    }

    spin_unlock_irqrestore(&vga_lock, flags);
}
