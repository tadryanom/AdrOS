#include "vga_console.h"

#include "hal/video.h"

#include "spinlock.h"

static volatile uint16_t* VGA_BUFFER = 0;
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_CELLS  (VGA_WIDTH * VGA_HEIGHT)

static int term_col = 0;
static int term_row = 0;
static uint8_t term_color = 0x0F; // White on Black

static spinlock_t vga_lock = {0};

/* Shadow buffer in RAM — all writes target this, flushed to VGA MMIO lazily */
static uint16_t shadow[VGA_CELLS];
static int dirty_lo = VGA_CELLS;  /* first dirty cell index  */
static int dirty_hi = -1;         /* last dirty cell index   */

static void dirty_mark(int lo, int hi) {
    if (lo < dirty_lo) dirty_lo = lo;
    if (hi > dirty_hi) dirty_hi = hi;
}

static void vga_flush_to_hw(void) {
    if (dirty_lo <= dirty_hi && VGA_BUFFER) {
        for (int i = dirty_lo; i <= dirty_hi; i++) {
            VGA_BUFFER[i] = shadow[i];
        }
        dirty_lo = VGA_CELLS;
        dirty_hi = -1;
    }
    hal_video_set_cursor(term_row, term_col);
}

/* --- Scrollback buffer --- */
#define SB_LINES 200
static uint16_t sb_buf[SB_LINES * VGA_WIDTH];
static int sb_head  = 0;  /* next write line (circular) */
static int sb_count = 0;  /* stored lines (max SB_LINES) */

static int view_offset = 0; /* 0 = live view, >0 = scrolled back N lines */
static uint16_t live_buf[VGA_CELLS]; /* saved live screen when scrolled */

static void vga_scroll(void) {
    /* Save row 0 (about to be lost) into scrollback ring */
    __builtin_memcpy(&sb_buf[sb_head * VGA_WIDTH], &shadow[0],
                     VGA_WIDTH * sizeof(uint16_t));
    sb_head = (sb_head + 1) % SB_LINES;
    if (sb_count < SB_LINES) sb_count++;

    /* Shift shadow content up (RAM speed — no MMIO) */
    __builtin_memmove(&shadow[0], &shadow[VGA_WIDTH],
                      (VGA_HEIGHT - 1) * VGA_WIDTH * sizeof(uint16_t));
    {
        uint16_t blank = (uint16_t)' ' | (uint16_t)term_color << 8;
        for (int x = 0; x < VGA_WIDTH; x++)
            shadow[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
    dirty_mark(0, VGA_CELLS - 1);
    term_row = VGA_HEIGHT - 1;
}

/* Restore live view if currently scrolled back */
static void vga_unscroll(void) {
    if (view_offset > 0) {
        __builtin_memcpy(shadow, live_buf, sizeof(shadow));
        dirty_mark(0, VGA_CELLS - 1);
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
            if (VGA_BUFFER) {
                for (int x = 0; x < VGA_WIDTH; x++)
                    VGA_BUFFER[y * VGA_WIDTH + x] = live_buf[live_row * VGA_WIDTH + x];
            }
        } else {
            /* Scrollback: sb_idx 0 = most recent scrolled-off line */
            int sb_idx = line_from_end - VGA_HEIGHT;
            if (sb_idx < sb_count) {
                int buf_line = (sb_head - 1 - sb_idx + SB_LINES) % SB_LINES;
                if (VGA_BUFFER) {
                    for (int x = 0; x < VGA_WIDTH; x++)
                        VGA_BUFFER[y * VGA_WIDTH + x] = sb_buf[buf_line * VGA_WIDTH + x];
                }
            } else if (VGA_BUFFER) {
                /* Beyond scrollback — blank */
                for (int x = 0; x < VGA_WIDTH; x++)
                    VGA_BUFFER[y * VGA_WIDTH + x] = (uint16_t)' ' | (uint16_t)term_color << 8;
            }
        }
    }
    /* Hide cursor when scrolled back */
    hal_video_set_cursor(VGA_HEIGHT, 0);
}

static int ansi_state = 0; /* 0=normal, 1=after ESC, 2=after ESC[, 3=after ESC[2 */

static void vga_put_char_unlocked(char c) {
    /* Any new output auto-returns to live view */
    vga_unscroll();

    /* Minimal ANSI support for common terminal clear/home sequences.
     * Handles:
     *   ESC [ 2 J  (clear screen)
     *   ESC [ H    (cursor home)
     */
    if (ansi_state != 0) {
        if (ansi_state == 1) {
            if (c == '[') {
                ansi_state = 2;
                return;
            }
            ansi_state = 0;
        }
        if (ansi_state == 2) {
            if (c == 'H') {
                term_col = 0;
                term_row = 0;
                ansi_state = 0;
                return;
            }
            if (c == '2') {
                ansi_state = 3;
                return;
            }
            ansi_state = 0;
        }
        if (ansi_state == 3) {
            if (c == 'J') {
                uint16_t blank = (uint16_t)' ' | (uint16_t)term_color << 8;
                for (int i = 0; i < VGA_CELLS; i++) {
                    shadow[i] = blank;
                }
                dirty_mark(0, VGA_CELLS - 1);
                term_col = 0;
                term_row = 0;
                view_offset = 0;
                sb_count = 0;
                sb_head = 0;
            }
            ansi_state = 0;
            return;
        }
    }

    if ((unsigned char)c == 0x1B) {
        ansi_state = 1;
        return;
    }

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
            shadow[index] = (uint16_t)(unsigned char)c | (uint16_t)term_color << 8;
            dirty_mark(index, index);
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

    uint16_t blank = (uint16_t)' ' | (uint16_t)term_color << 8;
    for (int i = 0; i < VGA_CELLS; i++)
        shadow[i] = blank;

    if (!VGA_BUFFER) return;

    for (int i = 0; i < VGA_CELLS; i++)
        VGA_BUFFER[i] = blank;

    dirty_lo = VGA_CELLS;
    dirty_hi = -1;
    hal_video_set_cursor(0, 0);
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);
    term_color = fg | (bg << 4);
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_put_char(char c) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);
    vga_put_char_unlocked(c);
    vga_flush_to_hw();
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_write_buf(const char* buf, uint32_t len) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);
    for (uint32_t i = 0; i < len; i++) {
        vga_put_char_unlocked(buf[i]);
    }
    vga_flush_to_hw();
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_print(const char* str) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);
    for (int i = 0; str[i] != '\0'; i++) {
        vga_put_char_unlocked(str[i]);
    }
    vga_flush_to_hw();
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_flush(void) {
    /* Quick unlocked check: if nothing is dirty, skip entirely.
     * All write paths (vga_write_buf, vga_put_char, vga_print) already
     * flush immediately, so this timer-tick path is just a safety net. */
    if (dirty_lo > dirty_hi) return;

    uintptr_t flags = spin_lock_irqsave(&vga_lock);
    vga_flush_to_hw();
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_clear(void) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);

    uint16_t blank = (uint16_t)' ' | (uint16_t)term_color << 8;
    for (int i = 0; i < VGA_CELLS; i++)
        shadow[i] = blank;
    dirty_mark(0, VGA_CELLS - 1);
    term_col = 0;
    term_row = 0;
    view_offset = 0;
    sb_count = 0;
    sb_head = 0;
    vga_flush_to_hw();

    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_scroll_back(void) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);

    if (!VGA_BUFFER || sb_count == 0) {
        spin_unlock_irqrestore(&vga_lock, flags);
        return;
    }

    if (view_offset == 0) {
        /* First scroll back — save current live screen from shadow */
        __builtin_memcpy(live_buf, shadow, sizeof(live_buf));
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
        __builtin_memcpy(shadow, live_buf, sizeof(shadow));
        dirty_mark(0, VGA_CELLS - 1);
        view_offset = 0;
        vga_flush_to_hw();
    } else {
        view_offset -= VGA_HEIGHT / 2;
        render_scrollback_view();
    }

    spin_unlock_irqrestore(&vga_lock, flags);
}
