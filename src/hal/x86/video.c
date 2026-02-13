#include "hal/video.h"
#include "io.h"

#if defined(__i386__) || defined(__x86_64__)

// This address must be mapped by VMM in higher half (see vmm_init mapping)
#define VGA_TEXT_BUFFER_VIRT ((uint16_t*)0xC00B8000)
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA  0x3D5
#define VGA_WIDTH      80

uint16_t* hal_video_text_buffer(void) {
    return (uint16_t*)VGA_TEXT_BUFFER_VIRT;
}

void hal_video_set_cursor(int row, int col) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(VGA_CRTC_INDEX, 0x0F);          /* cursor low byte register */
    outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(VGA_CRTC_INDEX, 0x0E);          /* cursor high byte register */
    outb(VGA_CRTC_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

#else
uint16_t* hal_video_text_buffer(void) {
    return (uint16_t*)0;
}
void hal_video_set_cursor(int row, int col) {
    (void)row; (void)col;
}
#endif
