#include "hal/usermode.h"

int hal_usermode_enter(uintptr_t user_eip, uintptr_t user_esp) {
    (void)user_eip;
    (void)user_esp;
    return -1;
}

void hal_usermode_enter_regs(const void* regs) {
    (void)regs;
}
