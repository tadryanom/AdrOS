# AdrOS — Full POSIX/Unix Compatibility Audit & Porting Analysis

**Date:** 2026-03-13
**Commit:** 2deaf85 (master)

---

## Part 1: Build System — `git clone` Breakage Analysis

### CRITICAL: Third-Party Dependencies Are NOT Tracked

After `git clone https://github.com/.../AdrOS.git`, the following directories will be **EMPTY**:

| Directory | Source | Status |
|---|---|---|
| `third_party/lwip/` | https://github.com/lwip-tcpip/lwip.git | Nested git repo, NOT a submodule |
| `user/doom/doomgeneric/` | https://github.com/ozkl/doomgeneric.git | Nested git repo, NOT a submodule |

**Result:** `make` fails immediately — lwIP sources referenced in `LWIP_SOURCES` don't exist.
`make iso` also fails because DOOM (optional) and lwIP (required) are missing.

**No `.gitmodules` file exists.** No `.gitignore` exists at root level.

### Recommended Fix (Priority: CRITICAL)

**Option A — Git Submodules (recommended):**
```bash
git submodule add https://github.com/lwip-tcpip/lwip.git third_party/lwip
git submodule add https://github.com/ozkl/doomgeneric.git user/doom/doomgeneric
```
Then users do `git clone --recursive` or `git submodule update --init`.

**Option B — Setup script + documentation:**
Add a `scripts/setup-deps.sh`:
```bash
#!/bin/bash
git clone https://github.com/lwip-tcpip/lwip.git third_party/lwip
git clone https://github.com/ozkl/doomgeneric.git user/doom/doomgeneric
```

### Other Build Issues

| Issue | Severity | Details |
|---|---|---|
| No `.gitignore` | Medium | `*.o`, `*.elf`, `*.iso`, `build/`, `disk.img`, `serial.log` etc. are committed or litter the workspace |
| `tools/mkinitrd` uses host `gcc` | Low | Correct behavior (host tool), but should be explicit: `HOST_CC ?= gcc` |
| `user/ulibc/src/*.o` tracked by git | Medium | Build artifacts in the repo — need `.gitignore` |
| Missing `README.md` build instructions | High | No documentation on prerequisites (cross-compiler, grub-mkrescue, expect, qemu, cppcheck) |

---

## Part 2: POSIX/Unix Compatibility Gap Analysis

### 2A. Missing Syscalls (kernel does NOT implement)

| Syscall | POSIX | Needed By | Priority |
|---|---|---|---|
| `mprotect` | Required | Newlib, GCC runtime, any JIT | **Critical** |
| `getrlimit` / `setrlimit` | Required | Bash, GCC, Busybox | **Critical** |
| `gettimeofday` | Required | Many programs (fallback for clock_gettime) | **High** |
| `getrusage` | Required | Bash (time builtin), make | High |
| `setsockopt` / `getsockopt` | Required | Any network program | High |
| `shutdown` (socket) | Required | Network programs | Medium |
| `getpeername` / `getsockname` | Required | Network programs | Medium |
| `madvise` | Optional | GCC, large programs | Low |
| `mremap` | Linux ext | realloc with mmap | Low |
| `execveat` | Linux ext | Nice to have | Low |
| `umount2` | Required | Full mount/umount | Medium |
| `ioctl FIONREAD` | Required | Many programs, Bash | High |

### 2B. Missing ulibc Headers (completely absent)

These POSIX headers do NOT exist at all in `user/ulibc/include/`:

| Header | Functions | Needed By | Priority |
|---|---|---|---|
| `<setjmp.h>` | `setjmp`, `longjmp`, `sigsetjmp`, `siglongjmp` | **Bash, GCC, Busybox, any error recovery** | **Critical** |
| `<locale.h>` | `setlocale`, `localeconv` | GCC, Bash, Busybox, many programs | **Critical** |
| `<pwd.h>` | `getpwnam`, `getpwuid`, `getpwent` | Bash, Busybox, login, id, ls -l | **Critical** |
| `<grp.h>` | `getgrnam`, `getgrgid`, `getgrent` | Bash, Busybox, id, ls -l | **Critical** |
| `<regex.h>` | `regcomp`, `regexec`, `regfree` | Bash, grep, sed, awk, Busybox | **Critical** |
| `<glob.h>` | `glob`, `globfree` | Bash (wildcard expansion) | **Critical** |
| `<fnmatch.h>` | `fnmatch` | Bash (pattern matching), find | High |
| `<getopt.h>` | `getopt`, `getopt_long` | Almost every Unix utility | **Critical** |
| `<libgen.h>` | `dirname`, `basename` (POSIX versions) | Many programs | High |
| `<netdb.h>` | `gethostbyname`, `getaddrinfo`, `freeaddrinfo` | Network programs | High |
| `<netinet/in.h>` | `struct sockaddr_in`, `htons`, `ntohs` | Network programs | High |
| `<arpa/inet.h>` | `inet_aton`, `inet_ntoa`, `inet_pton` | Network programs | High |
| `<sys/socket.h>` | `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv` | Network programs | High |
| `<sys/select.h>` | `select`, `FD_SET`, `FD_CLR`, `FD_ISSET`, `FD_ZERO` | Bash, many programs | High |
| `<sys/resource.h>` | `getrlimit`, `setrlimit`, `getrusage` | Bash, GCC | High |
| `<sys/utsname.h>` | `uname` | Bash, Busybox, autoconf | High |
| `<poll.h>` | `poll`, `struct pollfd` | Many programs | High |
| `<dlfcn.h>` | `dlopen`, `dlsym`, `dlclose`, `dlerror` | GCC plugins, shared libs | Medium |
| `<spawn.h>` | `posix_spawn` | Busybox, some programs | Medium |
| `<wordexp.h>` | `wordexp`, `wordfree` | Bash | Medium |
| `<sys/shm.h>` | `shmget`, `shmat`, `shmdt`, `shmctl` | IPC programs | Medium |
| `<semaphore.h>` | `sem_open`, `sem_wait`, `sem_post` | Threaded programs | Medium |
| `<mqueue.h>` | `mq_open`, `mq_send`, `mq_receive` | IPC programs | Low |
| `<aio.h>` | `aio_read`, `aio_write` | Async I/O programs | Low |
| `<sys/epoll.h>` | `epoll_create`, `epoll_ctl`, `epoll_wait` | Event-driven servers | Medium |
| `<sys/inotify.h>` | `inotify_init`, `inotify_add_watch` | File watchers | Low |
| `<syslog.h>` | `syslog`, `openlog`, `closelog` | Daemons, Busybox | Low |
| `<sys/un.h>` | `struct sockaddr_un` | Unix domain sockets | Medium |

### 2C. Missing Functions in EXISTING ulibc Headers

#### `<stdio.h>` — Missing:
- `perror()` — error printing (used everywhere)
- `popen()` / `pclose()` — pipe to/from command
- `fdopen()` — FILE* from fd
- `fileno()` — fd from FILE*
- `getline()` / `getdelim()` — dynamic line reading
- `tmpfile()` / `tmpnam()` — temporary files
- `ungetc()` — push back character
- `clearerr()` — clear error/EOF indicators
- `scanf()` / `fscanf()` — formatted input (only `sscanf` exists)

#### `<stdlib.h>` — Missing:
- `strtoul()` — unsigned long from string (**critical for Bash/Busybox**)
- `strtod()` / `strtof()` — float from string
- `strtoll()` / `strtoull()` — 64-bit integers
- `atexit()` — exit handler registration
- `mkstemp()` / `mkdtemp()` — secure temp files
- `setenv()` / `putenv()` / `unsetenv()` — environment modification
- `bsearch()` — binary search
- `div()` / `ldiv()` — division with remainder
- `rand()` / `srand()` — random number generation
- `abort()` — abnormal termination

#### `<string.h>` — Missing:
- `strerror()` — error code to string (**critical**)
- `strsignal()` — signal number to string
- `strtok_r()` — reentrant tokenizer
- `strpbrk()` — scan for character set
- `strspn()` / `strcspn()` — span character sets
- `strnlen()` — bounded strlen

#### `<unistd.h>` — Missing:
- `sleep()` / `usleep()` — wrappers over nanosleep (**critical**)
- `execvp()` / `execlp()` / `execl()` / `execle()` — exec family (**critical for Bash**)
- `getopt()` — option parsing (**critical**)
- `sbrk()` — heap growth (Newlib needs this)
- `pipe2()` — pipe with flags
- `gethostname()` / `sethostname()`
- `getlogin()` / `getlogin_r()`
- `ttyname()` / `ttyname_r()`
- `sysconf()` / `pathconf()` / `fpathconf()` — system configuration (**critical for autoconf**)
- `confstr()`
- `tcgetpgrp()` / `tcsetpgrp()` — terminal foreground process group

#### `<signal.h>` — Missing:
- `sigset_t` type (currently using raw `uint32_t`)
- `sigemptyset()` / `sigfillset()` / `sigaddset()` / `sigdelset()` / `sigismember()`
- `signal()` — simple signal handler (SIG_DFL, SIG_IGN, function)

#### `<pthread.h>` — Missing:
- `pthread_mutex_init/lock/unlock/destroy`
- `pthread_cond_init/wait/signal/broadcast/destroy`
- `pthread_rwlock_*`
- `pthread_key_create/setspecific/getspecific` (TLS)
- `pthread_once`
- `pthread_cancel`
- `pthread_detach`

#### `<termios.h>` — Missing:
- `cfgetispeed()` / `cfgetospeed()` / `cfsetispeed()` / `cfsetospeed()` — baud rate
- `cfmakeraw()` — convenience function
- `tcdrain()` / `tcflush()` / `tcflow()` / `tcsendbreak()`

#### `<errno.h>` — Missing error codes:
- `ERANGE` (34) — result too large (**critical for strtol**)
- `ENOTSOCK` (88) — not a socket
- `ECONNREFUSED` (111) — connection refused
- `ETIMEDOUT` (110) — connection timed out
- `EADDRINUSE` (98) — address in use
- `ENETUNREACH` (101) — network unreachable
- `EOVERFLOW` (75) — value overflow
- `ELOOP` (40) — symlink loop
- `ENAMETOOLONG` (36) — filename too long
- `ESPIPE` (29) — invalid seek
- `EROFS` (30) — read-only filesystem

### 2D. Other POSIX/Unix Gaps

| Feature | Status | Impact |
|---|---|---|
| **`/etc/passwd`** and **`/etc/group`** | Not implemented | No user/group name resolution |
| **`/etc/hosts`** | Not implemented | No local hostname resolution |
| **Process groups / sessions** | Partial (basic) | Job control works but incomplete for Bash |
| **`/dev/tty`** | Exists as devfs entry | Need to verify controlling terminal semantics |
| **Proper `mode_t` permissions** | Stored but not enforced on open/exec | Need real permission checks |
| **`free()` in malloc** | No-op (bump allocator) | Programs that allocate/free heavily will OOM |
| **`wait4()`** | Missing | Some programs use this instead of waitpid |
| **`time_t` as 32-bit** | `int32_t` — Y2038 issue | May cause issues with some programs |
| **`off_t` as 32-bit** | `uint32_t` — 4GB file limit | Limits large file support |
| **`ssize_t` return types** | `read()`/`write()` return `int` | POSIX requires `ssize_t` |
| **Environment `setenv/putenv`** | Not implemented | Cannot modify environment at runtime |

---

## Part 3: Porting Feasibility Analysis

### 3A. Newlib Port

Newlib is the most important prerequisite — GCC, Binutils, Bash, and Busybox all need a proper C library.

**What Newlib needs from the OS (libgloss/AdrOS stubs):**

| Stub Function | AdrOS Syscall | Status |
|---|---|---|
| `_exit()` | `SYSCALL_EXIT` | ✅ Ready |
| `_read()` | `SYSCALL_READ` | ✅ Ready |
| `_write()` | `SYSCALL_WRITE` | ✅ Ready |
| `_open()` | `SYSCALL_OPEN` | ✅ Ready |
| `_close()` | `SYSCALL_CLOSE` | ✅ Ready |
| `_lseek()` | `SYSCALL_LSEEK` | ✅ Ready |
| `_fstat()` | `SYSCALL_FSTAT` | ✅ Ready |
| `_stat()` | `SYSCALL_STAT` | ✅ Ready |
| `_isatty()` | via `SYSCALL_IOCTL` TIOCGPGRP | ✅ Ready |
| `_kill()` | `SYSCALL_KILL` | ✅ Ready |
| `_getpid()` | `SYSCALL_GETPID` | ✅ Ready |
| `_sbrk()` | `SYSCALL_BRK` | ✅ Ready (need wrapper) |
| `_link()` | `SYSCALL_LINK` | ✅ Ready |
| `_unlink()` | `SYSCALL_UNLINK` | ✅ Ready |
| `_fork()` | `SYSCALL_FORK` | ✅ Ready |
| `_execve()` | `SYSCALL_EXECVE` | ✅ Ready |
| `_wait()` | `SYSCALL_WAITPID` | ✅ Ready |
| `_times()` | `SYSCALL_TIMES` | ✅ Ready |
| `_gettimeofday()` | **MISSING** — need new syscall | ❌ **TODO** |
| `_rename()` | `SYSCALL_RENAME` | ✅ Ready |
| `_mkdir()` | `SYSCALL_MKDIR` | ✅ Ready |

**Steps to port Newlib:**

1. **Add `gettimeofday` syscall** (wrapper over `clock_gettime` CLOCK_REALTIME)
2. **Add `mprotect` syscall** (needed by Newlib's `mmap`-based malloc)
3. **Create `libgloss/adros/` directory** with AdrOS-specific stubs:
   - `syscalls.c` — maps `_read`, `_write`, `_open`, etc. to AdrOS `int 0x80`/`sysenter`
   - `crt0.S` — C runtime startup (similar to existing ulibc `crt0.S`)
4. **Add AdrOS target to Newlib's configure** — `newlib/configure.host` entry for `i686-*-adros*`
5. **Build cross-Newlib:**
   ```bash
   mkdir build-newlib && cd build-newlib
   ../newlib/configure --target=i686-adros --prefix=/opt/adros-toolchain
   make && make install
   ```

**Estimated effort: Medium (2-3 days)**

### 3B. GCC Port (cross-compiler targeting AdrOS)

**Prerequisites:** Newlib port must be done first.

**What's needed:**

1. **`config.sub` patch** — add `adros` as recognized OS
2. **`gcc/config/i386/adros.h`** — target header:
   ```c
   #undef  TARGET_OS_CPP_BUILTINS
   #define TARGET_OS_CPP_BUILTINS()       \
     do {                                  \
       builtin_define ("__adros__");       \
       builtin_define ("__unix__");        \
       builtin_assert ("system=adros");    \
       builtin_assert ("system=unix");     \
     } while (0)
   ```
3. **`gcc/config.gcc`** — add `i686-*-adros*` case
4. **`libgcc/config.host`** — add `i686-*-adros*` case
5. **`fixincludes/mkfixinc.sh`** — skip fix-includes for AdrOS

**Build sequence:**
```bash
# Phase 1: binutils
../binutils/configure --target=i686-adros --prefix=/opt/adros
make && make install

# Phase 2: GCC (bootstrap, C only)
../gcc/configure --target=i686-adros --prefix=/opt/adros \
    --without-headers --enable-languages=c --disable-shared
make all-gcc all-target-libgcc && make install-gcc install-target-libgcc

# Phase 3: Newlib
../newlib/configure --target=i686-adros --prefix=/opt/adros
make && make install

# Phase 4: Full GCC (with C library)
../gcc/configure --target=i686-adros --prefix=/opt/adros \
    --enable-languages=c,c++ --with-newlib
make && make install
```

**Estimated effort: Large (1 week)**

### 3C. Binutils Port

**What's needed:**

1. **`bfd/config.bfd`** — add `i686-*-adros*` entry mapping to `bfd_elf32_i386_vec`
2. **`ld/emulparams/elf_i386_adros.sh`** — linker emulation parameters
3. **`ld/configure.tgt`** — add `i686-*-adros*` case
4. **`config.sub`** — add `adros` OS recognition

Binutils is simpler than GCC — the i386 ELF support already exists, just needs OS target wiring.

**Estimated effort: Small (1 day)**

### 3D. Bash Port

**Critical missing kernel/libc features for Bash:**

| Feature | Status | Blocking? |
|---|---|---|
| `setjmp` / `longjmp` | ❌ Missing from ulibc | **YES** — used for error recovery, command abort |
| `execvp()` (PATH search) | ❌ Missing from ulibc | **YES** — core functionality |
| `getopt_long()` | ❌ Missing | **YES** — option parsing |
| `glob()` / `fnmatch()` | ❌ Missing | **YES** — wildcard expansion |
| `regex` (`regcomp/regexec`) | ❌ Missing | **YES** — `[[ =~ ]]` operator |
| `getpwnam()` / `getpwuid()` | ❌ Missing | **YES** — `~user` expansion, `$HOME` |
| `getrlimit()` / `setrlimit()` | ❌ Missing syscall | YES — `ulimit` builtin |
| `select()` with `FD_SET` macros | Syscall exists, macros missing | YES |
| `signal()` (simple handler) | ❌ Missing wrapper | YES |
| `strerror()` | ❌ Missing | YES |
| `sleep()` | ❌ Missing wrapper | YES |
| `setenv()` / `unsetenv()` | ❌ Missing | YES — environment modification |
| `strtoul()` | ❌ Missing | YES — arithmetic expansion |
| `atexit()` | ❌ Missing | YES — cleanup handlers |
| `locale` support | ❌ Missing | Partial — can stub |
| Proper `free()` in malloc | ❌ Bump allocator | **YES** — Bash allocates/frees constantly |
| `fork()` + `execve()` | ✅ Ready | — |
| `pipe()` + `dup2()` | ✅ Ready | — |
| `waitpid()` | ✅ Ready | — |
| `sigaction()` | ✅ Ready | — |
| `termios` (tcgetattr/tcsetattr) | ✅ Ready | — |
| `getcwd()` / `chdir()` | ✅ Ready | — |
| `stat()` / `fstat()` | ✅ Ready | — |
| `getenv()` | ✅ Ready | — |

**Path to Bash:** Port Newlib first, then Bash becomes feasible. With ulibc alone, Bash is NOT portable.

**Estimated effort: Large (1-2 weeks, after Newlib)**

### 3E. Busybox Port

Busybox uses a similar but even broader set of POSIX APIs. It requires everything Bash needs plus:

| Additional Feature | Status |
|---|---|
| `getpwent()` / `getgrent()` (iterate /etc/passwd) | ❌ Missing |
| `mntent` functions (mount table) | ❌ Missing |
| `syslog()` | ❌ Missing |
| `utmp` / `wtmp` (login records) | ❌ Missing |
| `getaddrinfo()` / `getnameinfo()` | Syscall exists, libc wrapper missing |
| `setsockopt()` / `getsockopt()` | ❌ Missing syscall |
| `sendmsg()` / `recvmsg()` | Syscall exists |
| Full `ioctl` for network interfaces | Partial |

**Path to Busybox:** Port Newlib → port Bash → then Busybox is the next logical step. Busybox has `CONFIG_` options to disable features it can't use.

**Estimated effort: Very Large (2-3 weeks, after Newlib+Bash)**

---

## Part 4: Prioritized Action Plan

### Phase 1: Build System Fix (immediate)
1. Add `.gitmodules` for lwIP and doomgeneric
2. Create root `.gitignore`
3. Add README.md with build prerequisites and instructions
4. Clean tracked build artifacts (`*.o`, `*.elf` in ulibc/src/)

### Phase 2: Critical Syscalls (1-2 days)
1. `mprotect(addr, len, prot)` — map to VMM page protection change
2. `gettimeofday(tv, tz)` — wrapper over RTC + clock_gettime
3. `getrlimit` / `setrlimit` — per-process resource limits (RLIMIT_NOFILE, RLIMIT_STACK)
4. `setsockopt` / `getsockopt` — wire to lwIP

### Phase 3: Critical ulibc Functions (2-3 days)
1. `setjmp` / `longjmp` (i386 assembly — save/restore ESP, EBP, EBX, ESI, EDI, EIP)
2. `sleep()` / `usleep()` (wrappers over nanosleep)
3. `execvp()` / `execlp()` (PATH search + execve)
4. `getopt()` / `getopt_long()`
5. `strerror()` / `perror()`
6. `strtoul()` / `strtoll()` / `strtoull()`
7. `setenv()` / `unsetenv()` / `putenv()`
8. `signal()` (simple wrapper over sigaction)
9. `abort()` / `atexit()`
10. `rand()` / `srand()`

### Phase 4: Critical Headers (2-3 days)
1. `<setjmp.h>` — with i386 asm implementation
2. `<locale.h>` — stubs (C locale only)
3. `<pwd.h>` / `<grp.h>` — parse /etc/passwd and /etc/group
4. `<regex.h>` — minimal regex engine or port TRE/PCRE
5. `<glob.h>` / `<fnmatch.h>`
6. `<getopt.h>`
7. `<sys/select.h>` — FD_SET/FD_CLR/FD_ISSET/FD_ZERO macros
8. Network headers (`<netdb.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, `<sys/socket.h>`)

### Phase 5: Proper malloc (1-2 days)
Replace bump allocator with a proper free-list or dlmalloc/K&R allocator in ulibc.

### Phase 6: Newlib Port (2-3 days)
1. Create `libgloss/adros/` with syscall stubs
2. Add AdrOS target to Newlib configure
3. Build and validate

### Phase 7: Binutils + GCC Port (1 week)
1. Add `i686-adros` target to Binutils
2. Add `i686-adros` target to GCC
3. Bootstrap cross-compiler

### Phase 8: Bash Port (1-2 weeks)
1. Cross-compile Bash with `i686-adros-gcc` + Newlib
2. Fix missing stubs iteratively
3. Package in initrd

### Phase 9: Busybox Port (2-3 weeks)
1. Cross-compile with minimal config
2. Enable applets iteratively
3. Replace individual `/bin/*` utilities

---

## Summary Table

| Component | Current State | Ready to Port? |
|---|---|---|
| **Kernel syscalls** | 126 syscalls, ~85% POSIX | Missing: mprotect, getrlimit, gettimeofday |
| **ulibc** | 27 headers, basic functions | **NOT sufficient** for Bash/Busybox |
| **Build system** | Works locally, breaks on git clone | Needs submodules + .gitignore |
| **Newlib** | Not started | **Feasible** — 19/21 required stubs ready |
| **Binutils** | Not started | Easy — just target config |
| **GCC** | Not started | Feasible after Newlib |
| **Bash** | Not started | Needs Newlib + setjmp + glob + regex |
| **Busybox** | Not started | Needs Newlib + extensive libc |

**Bottom line:** The kernel is ~85% POSIX-ready. The main blocker is **ulibc** — it lacks too many functions for real-world programs. The fastest path to Bash/Busybox is: **fix critical syscalls → port Newlib → build cross-toolchain → cross-compile Bash**.
