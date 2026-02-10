#ifndef HAL_USERMODE_H
#define HAL_USERMODE_H

#include <stdint.h>
#include "interrupts.h"

int hal_usermode_enter(uintptr_t user_eip, uintptr_t user_esp);

void hal_usermode_enter_regs(const struct registers* regs);

#endif
