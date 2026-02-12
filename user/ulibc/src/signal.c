#include "signal.h"
#include "syscall.h"
#include "errno.h"

int kill(int pid, int sig) {
    return __syscall_ret(_syscall2(SYS_KILL, pid, sig));
}

int raise(int sig) {
    return kill(_syscall0(SYS_GETPID), sig);
}

int sigprocmask(int how, const uint32_t* set, uint32_t* oldset) {
    return __syscall_ret(_syscall3(SYS_SIGPROCMASK, how, (int)set, (int)oldset));
}

int sigpending(uint32_t* set) {
    return __syscall_ret(_syscall1(SYS_SIGPENDING, (int)set));
}

int sigsuspend(const uint32_t* mask) {
    return __syscall_ret(_syscall1(SYS_SIGSUSPEND, (int)mask));
}
