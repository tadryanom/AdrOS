#ifndef SIGNAL_H
#define SIGNAL_H

#include <stdint.h>

#define SA_SIGINFO 0x00000004U

struct sigaction {
    uintptr_t sa_handler;
    uintptr_t sa_sigaction;
    uint32_t sa_mask;
    uint32_t sa_flags;
};

struct siginfo {
    int si_signo;
    int si_code;
    void* si_addr;
};

typedef struct siginfo siginfo_t;

struct ucontext {
    uint32_t reserved;
};

typedef struct ucontext ucontext_t;

#endif
