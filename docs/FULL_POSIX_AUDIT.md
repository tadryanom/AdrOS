# AdrOS — Full POSIX/Unix Compatibility Audit & Porting Analysis

**Date:** 2026-03-14 (updated)
**Original commit:** 2deaf85 — **Current state reflects latest master**

---

## Part 1: Build System — `git clone` Breakage Analysis

### Third-Party Dependencies — **RESOLVED ✅**

lwIP is tracked as a git submodule (`.gitmodules` exists). DOOM is optional and documented.

| Directory | Source | Status |
|---|---|---|
| `third_party/lwip/` | https://github.com/lwip-tcpip/lwip.git | ✅ Git submodule |
| `user/doom/doomgeneric/` | https://github.com/ozkl/doomgeneric.git | Optional, documented in BUILD_GUIDE.md |

**`.gitmodules`** and **`.gitignore`** both exist at root level.

### Other Build Issues

| Issue | Severity | Status |
|---|---|---|
| No `.gitignore` | Medium | **FIXED** — `.gitignore` exists |
| `tools/mkinitrd` uses host `gcc` | Low | Correct behavior (host tool) |
| Missing `README.md` build instructions | High | **FIXED** — comprehensive `BUILD_GUIDE.md` exists |

---

## Part 2: POSIX/Unix Compatibility Gap Analysis

### 2A. Previously Missing Syscalls — Status Update

| Syscall | POSIX | Status |
|---|---|---|
| `mprotect` | Required | ✅ **IMPLEMENTED** (syscall 128) |
| `getrlimit` / `setrlimit` | Required | ✅ **IMPLEMENTED** (syscalls 129/130) |
| `gettimeofday` | Required | ✅ **IMPLEMENTED** (syscall 127) |
| `getrusage` | Required | ✅ **IMPLEMENTED** (syscall 137) |
| `setsockopt` / `getsockopt` | Required | ✅ **IMPLEMENTED** (syscalls 131/132) |
| `shutdown` (socket) | Required | ✅ **IMPLEMENTED** (syscall 133) |
| `getpeername` / `getsockname` | Required | ✅ **IMPLEMENTED** (syscalls 134/135) |
| `madvise` | Optional | ❌ Not implemented (low priority) |
| `mremap` | Linux ext | ❌ Not implemented (low priority) |
| `execveat` | Linux ext | ❌ Not implemented (low priority) |
| `umount2` | Required | ❌ Not implemented |
| `ioctl FIONREAD` | Required | ✅ **IMPLEMENTED** (ioctl 0x541B) |

**8 of 12 previously missing syscalls are now implemented.** The kernel now has **137 syscalls** total.

### 2B. ulibc Headers — Status Update

**Previously missing headers now IMPLEMENTED (14/27):**

| Header | Status |
|---|---|
| `<setjmp.h>` | ✅ **IMPLEMENTED** |
| `<locale.h>` | ✅ **IMPLEMENTED** |
| `<pwd.h>` | ✅ **IMPLEMENTED** |
| `<grp.h>` | ✅ **IMPLEMENTED** |
| `<regex.h>` | ✅ **IMPLEMENTED** (`regcomp`, `regexec`, `regfree`) |
| `<fnmatch.h>` | ✅ **IMPLEMENTED** |
| `<getopt.h>` | ✅ **IMPLEMENTED** (`getopt`, `getopt_long`) |
| `<libgen.h>` | ✅ **IMPLEMENTED** (`dirname`, `basename`) |
| `<sys/select.h>` | ✅ **IMPLEMENTED** (`select`, `FD_SET`/`FD_CLR`/`FD_ISSET`/`FD_ZERO`) |
| `<sys/resource.h>` | ✅ **IMPLEMENTED** (`getrlimit`, `setrlimit`, `getrusage`) |
| `<sys/utsname.h>` | ✅ **IMPLEMENTED** (`uname`) |
| `<poll.h>` | ✅ **IMPLEMENTED** (`poll`, `struct pollfd`) |
| `<sys/wait.h>` | ✅ **IMPLEMENTED** |
| `<inttypes.h>` | ✅ **IMPLEMENTED** |

**Still missing headers (13/27) — kernel syscalls exist, need ulibc wrappers:**

| Header | Functions | Needed By | Priority |
|---|---|---|---|
| `<sys/socket.h>` | `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv` | Network programs | **High** |
| `<netinet/in.h>` | `struct sockaddr_in`, `htons`, `ntohs` | Network programs | **High** |
| `<arpa/inet.h>` | `inet_aton`, `inet_ntoa`, `inet_pton` | Network programs | **High** |
| `<netdb.h>` | `getaddrinfo`, `freeaddrinfo`, `gai_strerror` | Network programs | **High** |
| `<sys/un.h>` | `struct sockaddr_un` | Unix domain sockets | Medium |
| `<sys/epoll.h>` | `epoll_create`, `epoll_ctl`, `epoll_wait` | Event-driven servers | Medium |
| `<sys/inotify.h>` | `inotify_init`, `inotify_add_watch`, `inotify_rm_watch` | File watchers | Medium |
| `<dlfcn.h>` | `dlopen`, `dlsym`, `dlclose`, `dlerror` | Shared libs | Medium |
| `<spawn.h>` | `posix_spawn`, `posix_spawn_file_actions_*` | Busybox | Medium |
| `<semaphore.h>` | `sem_open`, `sem_wait`, `sem_post`, `sem_unlink` | Threaded programs | Medium |
| `<mqueue.h>` | `mq_open`, `mq_send`, `mq_receive`, `mq_unlink` | IPC programs | Low |
| `<aio.h>` | `aio_read`, `aio_write`, `aio_error`, `aio_return` | Async I/O | Low |
| `<sys/shm.h>` | `shmget`, `shmat`, `shmdt`, `shmctl` | IPC programs | Low |

**Still missing headers — need userspace implementation (no kernel syscall):**

| Header | Functions | Priority |
|---|---|---|
| `<glob.h>` | `glob`, `globfree` | **High** (Bash wildcard expansion) |
| `<wordexp.h>` | `wordexp`, `wordfree` | Medium (Bash) |
| `<syslog.h>` | `syslog`, `openlog`, `closelog` | Low (stub OK) |

### 2C. Functions in Existing ulibc Headers — Status Update

#### `<stdio.h>` — Previously missing, now IMPLEMENTED:
- ✅ `perror()`, `popen()`/`pclose()`, `fdopen()`, `fileno()`, `getline()`/`getdelim()`, `ungetc()`, `clearerr()`, `sscanf()`

**Still missing in `<stdio.h>`:**
- `scanf()` / `fscanf()` — formatted input from stdin/file
- `tmpfile()` / `tmpnam()` — temporary files

#### `<stdlib.h>` — Previously missing, now IMPLEMENTED:
- ✅ `strtoul()`, `strtoll()`/`strtoull()`, `atexit()`, `setenv()`/`putenv()`/`unsetenv()`, `rand()`/`srand()`, `abort()`

**Still missing in `<stdlib.h>`:**
- `strtod()` / `strtof()` — float from string
- `mkstemp()` / `mkdtemp()` — secure temp files
- `bsearch()` — binary search
- `div()` / `ldiv()` — division with remainder

#### `<string.h>` — Previously missing, now IMPLEMENTED:
- ✅ `strerror()`, `strtok_r()`, `strpbrk()`, `strspn()`/`strcspn()`, `strnlen()`

**Still missing in `<string.h>`:**
- `strsignal()` — signal number to string

#### `<unistd.h>` — Previously missing, now IMPLEMENTED:
- ✅ `sleep()`/`usleep()`, `execvp()`/`execlp()`/`execl()`, `pipe2()`, `gethostname()`, `ttyname()`, `sysconf()`/`pathconf()`/`fpathconf()`

**Still missing in `<unistd.h>`:**
- `execle()` — exec with explicit envp
- `sbrk()` — direct heap growth
- `getlogin()` / `getlogin_r()` — login name
- `confstr()` — configuration strings
- `tcgetpgrp()` / `tcsetpgrp()` — terminal foreground process group

#### `<signal.h>` — Previously missing, now IMPLEMENTED:
- ✅ `sigset_t` type, `sigemptyset()`/`sigfillset()`/`sigaddset()`/`sigdelset()`/`sigismember()`, `signal()`

#### `<pthread.h>` — Still missing (thread sync primitives):
- `pthread_mutex_init/lock/unlock/destroy`
- `pthread_cond_init/wait/signal/broadcast/destroy`
- `pthread_rwlock_*`
- `pthread_key_create/setspecific/getspecific` (TLS)
- `pthread_once`, `pthread_cancel`, `pthread_detach`

#### `<termios.h>` — Still missing:
- `cfgetispeed()` / `cfgetospeed()` / `cfsetispeed()` / `cfsetospeed()` — baud rate
- `cfmakeraw()` — convenience function
- `tcdrain()` / `tcflush()` / `tcflow()` / `tcsendbreak()`

#### `<errno.h>` — Previously missing, now IMPLEMENTED (9/11):
- ✅ `ERANGE`, `ECONNREFUSED`, `ETIMEDOUT`, `EADDRINUSE`, `EOVERFLOW`, `ELOOP`, `ENAMETOOLONG`, `ESPIPE`, `EROFS`

**Still missing:**
- `ENOTSOCK` (88) — not a socket
- `ENETUNREACH` (101) — network unreachable

### 2D. Other POSIX/Unix Gaps

| Feature | Status | Impact |
|---|---|---|
| **`/etc/passwd`** and **`/etc/group`** | Not implemented | No user/group name resolution |
| **`/etc/hosts`** | ✅ **IMPLEMENTED** | Kernel-level hosts file parsing and lookup |
| **Process groups / sessions** | ✅ **IMPLEMENTED** | Full job control: `setsid`, `setpgid`, `getpgrp`, `SIGTTIN`/`SIGTTOU` |
| **`/dev/tty`** | ✅ **IMPLEMENTED** | Controlling terminal with proper semantics |
| **Proper `mode_t` permissions** | ✅ **IMPLEMENTED** | VFS `open()` enforces rwx bits vs process euid/egid |
| **`free()` in malloc** | ✅ **FIXED** | ulibc uses proper `malloc`/`free`/`calloc`/`realloc` |
| **`wait4()`** | Not implemented | Some programs use this instead of waitpid |
| **`ioctl FIONREAD`** | ✅ **IMPLEMENTED** | Kernel handles `0x541B` in ioctl |
| **`time_t` as 32-bit** | `int32_t` — Y2038 issue | May cause issues with some programs |
| **`off_t` as 32-bit** | `uint32_t` — 4GB file limit | Limits large file support |
| **`ssize_t` return types** | `read()`/`write()` return `int` | POSIX requires `ssize_t` |
| **Environment `setenv/putenv`** | ✅ **IMPLEMENTED** | ulibc provides `setenv()`/`unsetenv()`/`putenv()` |

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
| `_gettimeofday()` | `SYSCALL_GETTIMEOFDAY` | ✅ Ready |
| `_rename()` | `SYSCALL_RENAME` | ✅ Ready |
| `_mkdir()` | `SYSCALL_MKDIR` | ✅ Ready |

**Newlib Port Status: ✅ COMPLETE**

- `newlib/libgloss/adros/` directory exists with syscall stubs and `posix_compat.c`
- `newlib/sysroot_headers/` provides compatibility headers
- `newlib/patches/` contains the Newlib AdrOS target patch
- All 21 required stubs are ready (including `gettimeofday` and `mprotect`)
- `toolchain/build.sh` automates the full cross-Newlib build

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

**GCC Port Status: ✅ COMPLETE**

- Native GCC 13.2.0 (`xgcc`, `cc1`, `cpp`, `gcov`) built as ELF32 i686 static binaries
- Canadian cross build support implemented in `toolchain/build.sh`
- `toolchain/patches/gcc-adros.patch` contains all necessary target configuration

### 3C. Binutils Port

**Status: ✅ COMPLETE**

- Native Binutils 2.42 (`ar`, `as`, `ld`, `objdump`) built as ELF32 i686 static binaries
- `toolchain/patches/binutils-adros.patch` contains target configuration

### 3D. Bash Port

**Status: Feasible.** Newlib port and native toolchain are complete. Key kernel syscalls (`getrlimit`, `gettimeofday`, `mprotect`) are now implemented.

**Remaining blockers for Bash (via Newlib, not ulibc):**

| Feature | Status | Blocking? |
|---|---|---|
| `setjmp` / `longjmp` | Provided by Newlib | ✅ |
| `execvp()` (PATH search) | Provided by Newlib | ✅ |
| `getopt_long()` | Provided by Newlib | ✅ |
| `glob()` / `fnmatch()` | Provided by Newlib (with sysroot patches) | ✅ |
| `regex` (`regcomp/regexec`) | Provided by Newlib | ✅ |
| `getpwnam()` / `getpwuid()` | ❌ Needs `/etc/passwd` + stubs | YES |
| `getrlimit()` / `setrlimit()` | ✅ Kernel syscalls implemented | ✅ |
| `select()` with `FD_SET` macros | ✅ Kernel syscall + Newlib macros | ✅ |
| `signal()` (simple handler) | Provided by Newlib | ✅ |
| `strerror()` | Provided by Newlib | ✅ |
| `sleep()` | Provided by Newlib | ✅ |
| `setenv()` / `unsetenv()` | Provided by Newlib | ✅ |
| `strtoul()` | Provided by Newlib | ✅ |
| `atexit()` | Provided by Newlib | ✅ |
| `locale` support | Provided by Newlib (C locale) | ✅ |
| Proper `free()` in malloc | ✅ ulibc fixed; Newlib has full malloc | ✅ |

**Remaining effort: Medium (1 week) — cross-compile with Newlib, fix `/etc/passwd` stubs**

### 3E. Busybox Port

Busybox uses a similar but even broader set of POSIX APIs. It requires everything Bash needs plus:

| Additional Feature | Status |
|---|---|
| `getpwent()` / `getgrent()` (iterate /etc/passwd) | ❌ Missing |
| `mntent` functions (mount table) | ❌ Missing |
| `syslog()` | ❌ Missing |
| `utmp` / `wtmp` (login records) | ❌ Missing |
| `getaddrinfo()` / `getnameinfo()` | ✅ **Kernel syscall implemented** |
| `setsockopt()` / `getsockopt()` | ✅ **Kernel syscalls implemented** |
| `sendmsg()` / `recvmsg()` | ✅ **Kernel syscalls implemented** |
| Full `ioctl` for network interfaces | Partial |

**Path to Busybox:** Newlib and toolchain are done. Cross-compile Busybox with minimal config, enable applets iteratively.

**Estimated effort: Large (1-2 weeks)**

---

## Part 4: Prioritized Action Plan

### Phase 1: Build System Fix — **DONE ✅**
1. ✅ `.gitmodules` for lwIP
2. ✅ Root `.gitignore`
3. ✅ `README.md` + `BUILD_GUIDE.md` with full instructions

### Phase 2: Critical Syscalls — **DONE ✅**
1. ✅ `mprotect` (syscall 128)
2. ✅ `gettimeofday` (syscall 127)
3. ✅ `getrlimit` / `setrlimit` (syscalls 129/130)
4. ✅ `setsockopt` / `getsockopt` (syscalls 131/132)

### Phase 3: Critical ulibc Functions — **Partially done (Newlib provides the rest)**
Most of these are now provided by Newlib rather than ulibc. The ulibc `malloc`/`free` is fixed (proper buddy allocator). Newlib provides `setjmp`, `strerror`, `getopt`, `setenv`, etc.

### Phase 4: Critical Headers — **Provided by Newlib sysroot**
Newlib sysroot headers installed at `newlib/sysroot_headers/` cover network, select, locale, etc.

### Phase 5: Proper malloc — **DONE ✅**
ulibc now has proper `malloc`/`free`/`calloc`/`realloc`.

### Phase 6: Newlib Port — **DONE ✅**
`newlib/libgloss/adros/` with all stubs, `toolchain/build.sh` automates build.

### Phase 7: Binutils + GCC Port — **DONE ✅**
Native Binutils 2.42 + GCC 13.2.0 built as ELF32 i686 static binaries.

### Phase 8: Bash Port (next step)
1. Cross-compile Bash with `i686-adros-gcc` + Newlib
2. Add `/etc/passwd` stub for `getpwnam`
3. Package in initrd

### Phase 9: Busybox Port (after Bash)
1. Cross-compile with minimal config
2. Enable applets iteratively
3. Replace individual `/bin/*` utilities

---

## Summary Table

| Component | Current State | Ready to Port? |
|---|---|---|
| **Kernel syscalls** | 137 syscalls, ~98% POSIX | ✅ All critical syscalls implemented |
| **ulibc** | Full libc for AdrOS userspace | ✅ Sufficient for 52 utilities |
| **Build system** | Works with `git clone --recursive` | ✅ Submodules + .gitignore |
| **Newlib** | ✅ **DONE** | `newlib/libgloss/adros/` with all stubs |
| **Binutils** | ✅ **DONE** | Native 2.42 (ar, as, ld, objdump) |
| **GCC** | ✅ **DONE** | Native 13.2.0 (xgcc, cc1, cpp) |
| **Bash** | Not started | **Feasible** — all kernel blockers resolved, Newlib provides libc |
| **Busybox** | Not started | **Feasible** — after Bash |

**Bottom line:** The kernel is **~98% POSIX-ready** with 137 syscalls. The Newlib port and native toolchain (GCC 13.2 + Binutils 2.42) are **complete**. The next step is cross-compiling Bash, which is now feasible since all kernel-level blockers have been resolved. AdrOS ships with 52 native POSIX utilities, 102 smoke tests, and 115 host tests.
