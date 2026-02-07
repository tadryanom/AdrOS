#include "hal/usermode.h"

int hal_usermode_enter(uintptr_t user_eip, uintptr_t user_esp) {
    (void)user_eip;
    (void)user_esp;
    return -1;
}
