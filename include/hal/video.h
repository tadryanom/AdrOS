#ifndef HAL_VIDEO_H
#define HAL_VIDEO_H

#include <stdint.h>

uint16_t* hal_video_text_buffer(void);
void hal_video_set_cursor(int row, int col);

#endif
