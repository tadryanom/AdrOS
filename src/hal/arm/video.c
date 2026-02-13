#include "hal/video.h"

uint16_t* hal_video_text_buffer(void) {
    return (uint16_t*)0;
}

void hal_video_set_cursor(int row, int col) {
    (void)row; (void)col;
}
