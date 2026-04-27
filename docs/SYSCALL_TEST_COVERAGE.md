# AdrOS — Syscall Test Coverage & POSIX Compliance Analysis

**Date:** 2026-04-27
**Kernel syscalls:** 141 (defined in `include/syscall.h`)
**Fulltest smoke tests:** 120
**Test battery checks:** 152
**Host tests:** 212

---

## 1. Relationship: 120 Tests ≠ 1 Test per Syscall

The 120 smoke tests do **not** correspond 1:1 to syscalls. The relationship is:

- **Some tests cover multiple syscalls** combined (e.g., "open/read/close" tests `write`, `open`, `read`, `close`; "lseek/stat/fstat" tests 3 syscalls)
- **Some tests cover a single syscall** (e.g., "brk", "umask", "flock")
- **Some tests cover functionality** that is not a syscall (e.g., "Heap init", "PCI enumeration", "overlay copy-up", "CoW fork", "PING network", "LZ4 Frame decomp")
- **Many syscalls are tested indirectly** as infrastructure for other tests (e.g., `write(1)` is used in ~100 tests, `open`/`close` in ~60, `fork`/`waitpid` in ~20)

**Test-to-syscall ratio:** ~0.85:1 (not 1:1)

---

## 2. Syscall-by-Syscall Test Mapping

### Tested Syscalls (124/141 — 87.9%)

| # | Syscall | Test(s) That Exercise It |
|---|---------|--------------------------|
| 1 | `write` | Almost all tests (via `sys_write(1, ...)`) |
| 2 | `exit` | Almost all tests (child processes) |
| 3 | `getpid` | getppid, setuid, gettid, etc. |
| 4 | `open` | open/read/close, overlay, tmpfs, etc. |
| 5 | `read` | open/read/close, pipe, etc. |
| 6 | `close` | Almost all tests with fd |
| 7 | `waitpid` | kill, setsid, 100 children, WNOHANG |
| 9 | `lseek` | lseek/stat/fstat, tmpfs, etc. |
| 10 | `fstat` | lseek/stat/fstat, tmpfs, ftruncate |
| 11 | `stat` | lseek/stat/fstat, rename, rmdir |
| 12 | `dup` | F5: dup standalone |
| 13 | `dup2` | dup2 restore, pipe |
| 14 | `pipe` | pipe, poll, select, epoll |
| 15 | `execve` | echo execve, PLT test |
| 16 | `fork` | kill, setsid, CoW, SMP, etc. |
| 17 | `getppid` | getppid, orphan reparent |
| 18 | `poll` | poll pipe, poll /dev/null, poll regfile |
| 19 | `kill` | kill SIGKILL, sigaction, sigprocmask |
| 20 | `select` | select pipe, select regfile |
| 21 | `ioctl` | ioctl tty, isatty |
| 22 | `setsid` | setsid/setpgid, job control |
| 23 | `setpgid` | setsid/setpgid, job control |
| 24 | `getpgrp` | setsid/setpgid, ioctl |
| 25 | `sigaction` | sigaction SIGUSR1, sigreturn, SIGSEGV |
| 26 | `sigprocmask` | sigprocmask/sigpending, sigsuspend |
| 27 | `sigreturn` | sigreturn test (implicit) |
| 28 | `mkdir` | diskfs mkdir, chdir, mount |
| 29 | `unlink` | diskfs unlink, symlink, etc. |
| 30 | `getdents` | getdents multi-fs, readdir /proc |
| 31 | `fcntl` | O_NONBLOCK, pipe capacity, FD_CLOEXEC |
| 32 | `chdir` | chdir/getcwd |
| 33 | `getcwd` | chdir/getcwd |
| 34 | `pipe2` | pipe2/dup3 |
| 35 | `dup3` | pipe2/dup3 |
| 36 | `openat` | *at syscalls, mount test |
| 37 | `fstatat` | *at syscalls |
| 38 | `unlinkat` | *at syscalls |
| 39 | `rename` | rename/rmdir |
| 40 | `rmdir` | rename/rmdir |
| 41 | `brk` | brk heap, mprotect |
| 42 | `nanosleep` | nanosleep, CLOCK_MONOTONIC |
| 43 | `clock_gettime` | clock_gettime, CLOCK_REALTIME |
| 44 | `mmap` | mmap/munmap, sigaltstack, clone |
| 45 | `munmap` | mmap/munmap, sigaltstack, clone |
| 46 | `shmget` | shmget/shmat/shmdt |
| 47 | `shmat` | shmget/shmat/shmdt |
| 50 | `chmod` | chmod |
| 51 | `chown` | chown |
| 52 | `getuid` | getuid/getgid, setuid |
| 53 | `getgid` | getuid/getgid, setgid |
| 54 | `link` | hard link |
| 55 | `symlink` | symlink/readlink |
| 56 | `readlink` | symlink/readlink |
| 58 | `socket` | socket API |
| 59 | `bind` | socket API |
| 60 | `listen` | socket API |
| 67 | `clone` | clone test |
| 68 | `gettid` | gettid |
| 69 | `fsync` | fsync |
| 71 | `sigpending` | sigprocmask/sigpending |
| 72 | `pread` | pread/pwrite |
| 73 | `pwrite` | pread/pwrite |
| 74 | `access` | access |
| 75 | `umask` | umask |
| 76 | `setuid` | setuid/setgid |
| 77 | `setgid` | setuid/setgid |
| 78 | `truncate` | truncate path |
| 79 | `ftruncate` | ftruncate |
| 80 | `sigsuspend` | sigsuspend, sigqueue |
| 81 | `readv` | readv/writev |
| 82 | `writev` | readv/writev |
| 83 | `alarm` | alarm/SIGALRM |
| 84 | `times` | times |
| 85 | `futex` | futex |
| 86 | `sigaltstack` | sigaltstack |
| 87 | `flock` | flock |
| 88 | `geteuid` | geteuid/getegid |
| 89 | `getegid` | geteuid/getegid |
| 90 | `seteuid` | seteuid/setegid, setuid |
| 91 | `setegid` | seteuid/setegid |
| 92 | `setitimer` | setitimer/getitimer |
| 93 | `getitimer` | setitimer/getitimer |
| 94 | `waitid` | waitid |
| 95 | `sigqueue` | sigqueue |
| 96 | `posix_spawn` | posix_spawn |
| 97 | `mq_open` | mqueue |
| 98 | `mq_close` | mqueue |
| 99 | `mq_send` | mqueue |
| 100 | `mq_receive` | mqueue |
| 101 | `mq_unlink` | mqueue |
| 102 | `sem_open` | named semaphore |
| 103 | `sem_close` | named semaphore |
| 104 | `sem_wait` | named semaphore |
| 105 | `sem_post` | named semaphore |
| 106 | `sem_unlink` | named semaphore |
| 107 | `sem_getvalue` | named semaphore |
| 109 | `dlopen` | dlopen/dlsym/dlclose |
| 110 | `dlsym` | dlopen/dlsym/dlclose |
| 111 | `dlclose` | dlopen/dlsym/dlclose |
| 112 | `epoll_create` | epoll |
| 113 | `epoll_ctl` | epoll, epollet |
| 114 | `epoll_wait` | epoll, epollet |
| 115 | `inotify_init` | inotify, inotify_init1 |
| 116 | `inotify_add_watch` | inotify |
| 117 | `inotify_rm_watch` | inotify |
| 120 | `pivot_root` | pivot_root |
| 121 | `aio_read` | aio |
| 122 | `aio_write` | aio |
| 123 | `aio_error` | aio |
| 124 | `aio_return` | aio |
| 126 | `mount` | mount/umount2, pivot_root |
| 127 | `gettimeofday` | gettimeofday |
| 128 | `mprotect` | mprotect |
| 129 | `getrlimit` | getrlimit/setrlimit |
| 130 | `setrlimit` | getrlimit/setrlimit |
| 133 | `shutdown` | socket API |
| 135 | `getsockname` | socket API |
| 136 | `uname` | uname |
| 137 | `getrusage` | getrusage |
| 138 | `umount2` | mount/umount2 |
| 140 | `madvise` | madvise |
| 141 | `execveat` | execveat |

### Untested Syscalls (17/141 — 12.1%)

| # | Syscall | Reason Not Tested |
|---|---------|-------------------|
| 49 | `shmctl` | Shared memory control (IPC_RMID/IPC_STAT) — no test |
| 57 | `set_thread_area` | Used implicitly by clone/TLS — no direct test |
| 61 | `accept` | TCP connect/accept hangs in QEMU — test disabled |
| 62 | `connect` | TCP loopback hangs in QEMU — test disabled |
| 63 | `send` | Depends on connect — no test |
| 64 | `recv` | Depends on connect — no test |
| 65 | `sendto` | UDP I/O not tested |
| 66 | `recvfrom` | UDP I/O not tested |
| 70 | `fdatasync` | Similar to fsync but data-only — no test |
| 108 | `getaddrinfo` | DNS resolution not tested in fulltest |
| 118 | `sendmsg` | Scatter/gather socket I/O not tested |
| 119 | `recvmsg` | Scatter/gather socket I/O not tested |
| 125 | `aio_suspend` | Async I/O wait not tested |
| 131 | `setsockopt` | Socket options not tested |
| 132 | `getsockopt` | Socket options not tested |
| 134 | `getpeername` | Depends on connect — no test |
| 139 | `wait4` | Different from waitpid — no test |

---

## 3. Implemented but Deficient Features

These syscalls are implemented and tested, but have known limitations:

| Area | Issue | Impact |
|------|-------|--------|
| **TCP loopback** | `connect()`/`accept()` hang in QEMU — network I/O tests disabled | Cannot test `send`/`recv`/`connect`/`accept`/`getpeername` |
| **Fork + VFS global** | `pivot_root` corrupts global VFS state (mitigated with `vfs_lookup_initrd`) | pivot_root affects entire system, not just the calling process |
| **clone() return** | Returns 0 in parent (bug) — workaround via `CLONE_PARENT_SETTID` | Non-standard behavior vs Linux clone() |
| **SHMCTL** | Syscall exists but no `IPC_RMID`/`IPC_STAT` operations | Shared memory cannot be properly cleaned up |
| **Futex** | Only non-blocking WAIT/WAKE tested; no timeout parameter | Blocking futex with fork+COW was unreliable |
| **Mkdir** | Kernel ignores `mode` argument (passes 0) | Created directories always have mode 0 |
| **Per-process mount namespace** | `pivot_root` affects whole system, not just process | Cannot isolate filesystem changes per-process |
| **Real UID vs effective UID** | No saved set-user-ID (POSIX requires) | Credential transitions are incomplete |
| **Signal queueing** | Real-time signals not reliably queued | `sigqueue` test warns but doesn't fail |
| **TTY job control** | `SIGTTIN`/`SIGTTOU` has race condition | Occasional flaky tests |

---

## 4. POSIX Compliance Gap Analysis

### What's Missing for 100% POSIX Compliance (~55+ features)

#### Process & Credentials
- `chroot` — Change root directory
- `getgroups` / `setgroups` — Supplementary group IDs
- `getpgid` / `getsid` — Query PGID/SID of arbitrary process
- `ptrace` — Process tracing (for gdb/strace)
- `nice` / `getpriority` / `setpriority` — Process priority manipulation
- `pause` — Wait for signal (sigsuspend partially covers)
- Saved set-user-ID — POSIX requires three-tier UID model

#### Filesystem
- `mkfifo` / `mknod` — Create FIFOs and device nodes
- `fchdir` — chdir via fd
- `fchmod` / `fchown` / `lchown` — chmod/chown by fd or without symlink follow
- `sync` / `syncfs` — Synchronize entire filesystem
- `statfs` / `fstatfs` — Filesystem statistics (used by `df`)
- `fpathconf` / `pathconf` — Configurable path variables
- `readlinkat` / `mkdirat` / `fchmodat` — Missing `*at()` variants

#### Signals
- `sigwait` / `sigwaitinfo` / `sigtimedwait` — Synchronous signal waiting

#### POSIX Timers
- `timer_create` / `timer_delete` / `timer_settime` / `timer_gettime` — POSIX timers (different from itimer)
- `clock_settime` / `clock_getres` / `clock_nanosleep` — Clock management

#### IPC
- `shmctl IPC_RMID/IPC_STAT` — Shared memory control operations
- `sem_init` / `sem_destroy` — Unnamed (anonymous) semaphores (POSIX requires both named and unnamed)
- `mq_notify` / `mq_getattr` / `mq_setattr` — Message queue notifications and attributes

#### Memory
- `mremap` — Remap virtual memory region
- `msync` — Synchronize mmap with file
- `mincore` — Check page residency

#### Network
- `socketpair` — Connected socket pairs (UNIX domain)
- TCP loopback (`connect`/`accept`/`send`/`recv`) — currently hangs in QEMU
- `AF_UNIX` / `AF_INET6` address families — only `AF_INET` implemented

#### Threads (pthreads)
- Full thread lifecycle: `pthread_cancel`, `pthread_testcancel`, `pthread_detach`
- Thread-safe `errno` via TLS (per-thread errno)
- `pthread_atfork` handlers

#### Terminal
- `tcsendbreak` / `tcdrain` / `tcflush` / `tcflow` — Terminal control (ulibc has wrappers, kernel ioctl needed)

---

## 5. Summary Metrics

| Metric | Value |
|--------|-------|
| Syscalls implemented in kernel | 141 |
| Syscalls tested by fulltest | 124 (87.9%) |
| Syscalls without test | 17 (12.1%) |
| Smoke tests | 120 |
| Battery checks | 152 |
| Host tests | 212 |
| Test:syscall ratio | ~0.85:1 (not 1:1) |
| POSIX-mandated features missing | ~55+ |
| Major missing categories | Threads, TCP loopback, mkfifo/mknod, chroot, getgroups/setgroups, POSIX timers, msync, fchmod, pathconf, socketpair |
