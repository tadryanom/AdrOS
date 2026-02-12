#ifndef ULIBC_SIGNAL_H
#define ULIBC_SIGNAL_H

#include <stdint.h>

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1  10
#define SIGSEGV  11
#define SIGUSR2  12
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGCHLD  17
#define SIGCONT  18
#define SIGSTOP  19
#define SIGTSTP  20
#define SIGTTIN  21
#define SIGTTOU  22

#define SA_SIGINFO 0x00000004U

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

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

int kill(int pid, int sig);
int raise(int sig);
int sigaction(int signum, const struct sigaction* act,
              struct sigaction* oldact);
int sigprocmask(int how, const uint32_t* set, uint32_t* oldset);
int sigpending(uint32_t* set);
int sigsuspend(const uint32_t* mask);

#define SS_DISABLE 2
typedef struct {
    void*    ss_sp;
    int      ss_flags;
    unsigned ss_size;
} stack_t;

int sigaltstack(const stack_t* ss, stack_t* old_ss);

#endif
