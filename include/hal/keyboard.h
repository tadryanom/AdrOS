#ifndef HAL_KEYBOARD_H
#define HAL_KEYBOARD_H

typedef void (*hal_keyboard_char_cb_t)(char c);

void hal_keyboard_init(hal_keyboard_char_cb_t cb);

#endif
