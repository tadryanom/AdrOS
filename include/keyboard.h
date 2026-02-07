#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Callback function type: called when a char is decoded
typedef void (*keyboard_callback_t)(char);

void keyboard_init(void);
void keyboard_set_callback(keyboard_callback_t callback);

int keyboard_read_nonblock(char* out, uint32_t max_len);

int keyboard_read_blocking(char* out, uint32_t max_len);

#endif
