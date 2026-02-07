#ifndef HAL_USERMODE_H
#define HAL_USERMODE_H

#include <stdint.h>

int hal_usermode_enter(uintptr_t user_eip, uintptr_t user_esp);

#endif
