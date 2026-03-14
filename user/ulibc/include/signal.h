// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

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
#define SIGVTALRM 26
#define SIGPROF  27

#define SA_RESTART  0x10000000U
#define SA_SIGINFO  0x00000004U
#define SA_NOCLDSTOP 0x00000001U
#define SA_NOCLDWAIT 0x00000002U
#define SA_NODEFER   0x40000000U
#define SA_RESETHAND 0x80000000U

#define SIG_ERR ((void (*)(int))-1)

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);

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

/* sigset_t manipulation macros (signal mask is a 32-bit bitmask) */
typedef uint32_t sigset_t;

#define _SIGMASK(sig) (1U << ((sig) - 1))

static inline int sigemptyset(sigset_t* set) { *set = 0; return 0; }
static inline int sigfillset(sigset_t* set)  { *set = 0xFFFFFFFFU; return 0; }
static inline int sigaddset(sigset_t* set, int sig) {
    if (sig < 1 || sig > 32) return -1;
    *set |= _SIGMASK(sig); return 0;
}
static inline int sigdelset(sigset_t* set, int sig) {
    if (sig < 1 || sig > 32) return -1;
    *set &= ~_SIGMASK(sig); return 0;
}
static inline int sigismember(const sigset_t* set, int sig) {
    if (sig < 1 || sig > 32) return -1;
    return (*set & _SIGMASK(sig)) ? 1 : 0;
}

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SS_DISABLE 2
typedef struct {
    void*    ss_sp;
    int      ss_flags;
    unsigned ss_size;
} stack_t;

int sigaltstack(const stack_t* ss, stack_t* old_ss);

#endif
