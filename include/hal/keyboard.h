#ifndef HAL_KEYBOARD_H
#define HAL_KEYBOARD_H

#include <stdint.h>

typedef void (*hal_keyboard_char_cb_t)(char c);
typedef void (*hal_keyboard_scan_cb_t)(uint8_t scancode);

void hal_keyboard_init(hal_keyboard_char_cb_t cb);
void hal_keyboard_set_scancode_cb(hal_keyboard_scan_cb_t cb);

#endif
