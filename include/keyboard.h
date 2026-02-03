#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Callback function type: called when a char is decoded
typedef void (*keyboard_callback_t)(char);

void keyboard_init(void);
void keyboard_set_callback(keyboard_callback_t callback);

#endif
