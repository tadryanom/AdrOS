/*
 * doomgeneric_adros.c — AdrOS platform adapter for doomgeneric
 *
 * Implements the DG_* interface that the doomgeneric engine requires.
 * Uses /dev/fb0 (mmap) for video, /dev/kbd for raw scancode input,
 * and clock_gettime/nanosleep for timing.
 */

#include "doomgeneric.h"
#include "doomkeys.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* AdrOS ulibc headers */
#include "unistd.h"
#include "stdio.h"
#include "sys/mman.h"
#include "sys/ioctl.h"
#include "time.h"

/* Framebuffer ioctl definitions (must match kernel's fb.h) */
#define FBIOGET_VSCREENINFO  0x4600
#define FBIOGET_FSCREENINFO  0x4602

struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t bits_per_pixel;
};

struct fb_fix_screeninfo {
    uint32_t smem_start;
    uint32_t smem_len;
    uint32_t line_length;
};

/* ---- State ---- */

static int fb_fd = -1;
static int kbd_fd = -1;
static uint32_t* framebuffer = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;  /* bytes per line */
static uint32_t fb_size = 0;

/* Scancode ring buffer for key events */
#define KEY_QUEUE_SIZE 64
static struct {
    int pressed;
    unsigned char key;
} key_queue[KEY_QUEUE_SIZE];
static int key_queue_head = 0;
static int key_queue_tail = 0;

static void key_queue_push(int pressed, unsigned char key) {
    int next = (key_queue_head + 1) % KEY_QUEUE_SIZE;
    if (next == key_queue_tail) return; /* full, drop */
    key_queue[key_queue_head].pressed = pressed;
    key_queue[key_queue_head].key = key;
    key_queue_head = next;
}

/* PS/2 Set 1 scancode to DOOM key mapping */
static unsigned char scancode_to_doomkey(uint8_t sc) {
    switch (sc & 0x7F) {
    case 0x01: return KEY_ESCAPE;
    case 0x1C: return KEY_ENTER;
    case 0x0F: return KEY_TAB;
    case 0x39: return KEY_USE;         /* space */
    case 0x1D: return KEY_FIRE;        /* left ctrl */
    case 0x2A: return KEY_RSHIFT;      /* left shift */
    case 0x38: return KEY_LALT;        /* left alt */

    /* Arrow keys */
    case 0x48: return KEY_UPARROW;
    case 0x50: return KEY_DOWNARROW;
    case 0x4B: return KEY_LEFTARROW;
    case 0x4D: return KEY_RIGHTARROW;

    /* WASD */
    case 0x11: return KEY_UPARROW;     /* W */
    case 0x1F: return KEY_DOWNARROW;   /* S */
    case 0x1E: return KEY_LEFTARROW;   /* A */
    case 0x20: return KEY_RIGHTARROW;  /* D */

    /* Number keys 1-9,0 for weapon select */
    case 0x02: return '1';
    case 0x03: return '2';
    case 0x04: return '3';
    case 0x05: return '4';
    case 0x06: return '5';
    case 0x07: return '6';
    case 0x08: return '7';
    case 0x09: return '8';
    case 0x0A: return '9';
    case 0x0B: return '0';

    case 0x0E: return KEY_BACKSPACE;
    case 0x19: return 'p';             /* pause */
    case 0x32: return 'm';             /* map toggle */
    case 0x15: return 'y';
    case 0x31: return 'n';

    /* F1-F12 */
    case 0x3B: return KEY_F1;
    case 0x3C: return KEY_F2;
    case 0x3D: return KEY_F3;
    case 0x3E: return KEY_F4;
    case 0x3F: return KEY_F5;
    case 0x40: return KEY_F6;
    case 0x41: return KEY_F7;
    case 0x42: return KEY_F8;
    case 0x43: return KEY_F9;
    case 0x44: return KEY_F10;
    case 0x57: return KEY_F11;
    case 0x58: return KEY_F12;

    case 0x0C: return KEY_MINUS;
    case 0x0D: return KEY_EQUALS;

    default: return 0;
    }
}

static void poll_keyboard(void) {
    uint8_t buf[32];
    int n = read(kbd_fd, buf, sizeof(buf));
    if (n <= 0) return;
    for (int i = 0; i < n; i++) {
        uint8_t sc = buf[i];
        int pressed = !(sc & 0x80);
        unsigned char dk = scancode_to_doomkey(sc);
        if (dk) {
            key_queue_push(pressed, dk);
        }
    }
}

/* ---- DG interface implementation ---- */

void DG_Init(void) {
    /* Open framebuffer */
    fb_fd = open("/dev/fb0", 0 /* O_RDONLY */);
    if (fb_fd < 0) {
        printf("[DOOM] Cannot open /dev/fb0\n");
        _exit(1);
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        printf("[DOOM] ioctl FBIOGET_VSCREENINFO failed\n");
        _exit(1);
    }
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        printf("[DOOM] ioctl FBIOGET_FSCREENINFO failed\n");
        _exit(1);
    }

    fb_width = vinfo.xres;
    fb_height = vinfo.yres;
    fb_pitch = finfo.line_length;
    fb_size = finfo.smem_len;

    printf("[DOOM] Framebuffer: %ux%u %ubpp pitch=%u size=%u\n",
           fb_width, fb_height, vinfo.bits_per_pixel, fb_pitch, fb_size);

    /* mmap the framebuffer */
    framebuffer = (uint32_t*)mmap(NULL, fb_size,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fb_fd, 0);
    if (framebuffer == MAP_FAILED) {
        printf("[DOOM] mmap /dev/fb0 failed\n");
        _exit(1);
    }

    /* Open raw keyboard */
    kbd_fd = open("/dev/kbd", 0);
    if (kbd_fd < 0) {
        printf("[DOOM] Cannot open /dev/kbd\n");
        _exit(1);
    }

    printf("[DOOM] AdrOS adapter initialized.\n");
}

void DG_DrawFrame(void) {
    if (!framebuffer || !DG_ScreenBuffer) return;

    /*
     * DG_ScreenBuffer is DOOMGENERIC_RESX * DOOMGENERIC_RESY in ARGB format.
     * Scale to the physical framebuffer using nearest-neighbor.
     */
    uint32_t sx = (DOOMGENERIC_RESX > 0) ? fb_width / DOOMGENERIC_RESX : 1;
    uint32_t sy = (DOOMGENERIC_RESY > 0) ? fb_height / DOOMGENERIC_RESY : 1;
    uint32_t scale = (sx < sy) ? sx : sy;
    if (scale == 0) scale = 1;

    uint32_t off_x = (fb_width - DOOMGENERIC_RESX * scale) / 2;
    uint32_t off_y = (fb_height - DOOMGENERIC_RESY * scale) / 2;

    for (uint32_t y = 0; y < DOOMGENERIC_RESY; y++) {
        uint32_t* src_row = &DG_ScreenBuffer[y * DOOMGENERIC_RESX];
        for (uint32_t dy = 0; dy < scale; dy++) {
            uint32_t fb_y = off_y + y * scale + dy;
            if (fb_y >= fb_height) break;
            uint32_t* dst_row = (uint32_t*)((uint8_t*)framebuffer + fb_y * fb_pitch);
            for (uint32_t x = 0; x < DOOMGENERIC_RESX; x++) {
                uint32_t pixel = src_row[x];
                for (uint32_t dx = 0; dx < scale; dx++) {
                    uint32_t fb_x = off_x + x * scale + dx;
                    if (fb_x < fb_width) {
                        dst_row[fb_x] = pixel;
                    }
                }
            }
        }
    }
}

void DG_SleepMs(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

uint32_t DG_GetTicksMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    /* Poll for new scancodes */
    if (kbd_fd >= 0) {
        poll_keyboard();
    }

    if (key_queue_tail == key_queue_head) return 0;

    *pressed = key_queue[key_queue_tail].pressed;
    *doomKey = key_queue[key_queue_tail].key;
    key_queue_tail = (key_queue_tail + 1) % KEY_QUEUE_SIZE;
    return 1;
}

void DG_SetWindowTitle(const char* title) {
    (void)title;
}

int main(int argc, char** argv) {
    doomgeneric_Create(argc, argv);
    for (;;) {
        doomgeneric_Tick();
    }
    return 0;
}
