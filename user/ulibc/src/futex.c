#include "linux/futex.h"
#include "syscall.h"

int futex(uint32_t* uaddr, int op, uint32_t val) {
    return _syscall3(SYS_FUTEX, (int)uaddr, op, (int)val);
}
