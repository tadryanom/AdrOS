# AdrOS — Deep POSIX Audit, Self-Hosting Assessment & Implementation Plan

**Date:** 2026-03-14
**Base Commit:** f2506b7 (master)

---

## Part 1: FULL_POSIX_AUDIT.md vs POSIX_ROADMAP.md — Reconciliation

### Summary

`POSIX_ROADMAP.md` tracks 75 features as all completed (✅) and declares "~98% POSIX".
`FULL_POSIX_AUDIT.md` identifies critical gaps that block Bash/Busybox/GCC porting.

**Both documents are partially outdated.** Since the audit was written (commit 2deaf85), significant work was done in commits `9b56e33` through `f2506b7`, adding:
- 9 new kernel syscalls (gettimeofday, mprotect, getrlimit/setrlimit, setsockopt/getsockopt, shutdown, getpeername/getsockname)
- 15+ new ulibc headers and functions (setjmp.h, locale.h, pwd.h, grp.h, fnmatch.h, getopt.h, sys/select.h, sys/resource.h)
- Complete cross-toolchain (binutils 2.42 + GCC 13.2.0 + Newlib 4.4.0)
- Bash 5.2.21 cross-compiled as static ELF 32-bit i386
- Proper malloc with free() (free-list allocator with coalescing)

### Items the Audit flagged as missing that are NOW IMPLEMENTED

| Item | Status | Evidence |
|------|--------|----------|
| `mprotect` syscall | ✅ | syscall 128, `syscall.c` line ~3939 |
| `gettimeofday` syscall | ✅ | syscall 127, `syscall.c` line ~3880 |
| `getrlimit`/`setrlimit` | ✅ | syscalls 129/130, `syscall.c` line ~3900 |
| `setsockopt`/`getsockopt` | ✅ | syscalls 131/132 |
| `shutdown` (socket) | ✅ | syscall 133 |
| `getpeername`/`getsockname` | ✅ | syscalls 134/135 |
| `<setjmp.h>` | ✅ | i386 asm: `setjmp.S` (setjmp/longjmp/sigsetjmp/siglongjmp) |
| `<locale.h>` | ✅ | C locale stubs: `locale.c` |
| `<pwd.h>` / `<grp.h>` | ✅ | `pwd_grp.c` with getpwnam/getpwuid/getpwent/getgrnam/getgrgid/getgrent |
| `<fnmatch.h>` | ✅ | `fnmatch.c` with pattern matching |
| `<getopt.h>` | ✅ | `getopt.c` with getopt/getopt_long |
| `<sys/select.h>` | ✅ | FD_SET/FD_CLR/FD_ISSET/FD_ZERO macros |
| `<sys/resource.h>` | ✅ | getrlimit/setrlimit wrappers |
| `strtoul`/`strtoll`/`strtoull` | ✅ | `strtoul.c` |
| `setenv`/`unsetenv`/`putenv` | ✅ | `environ.c` |
| `atexit`/`abort` | ✅ | `abort_atexit.c` |
| `rand`/`srand` | ✅ | `rand.c` |
| `strerror` | ✅ | `strerror.c` |
| `strtok_r`/`strpbrk`/`strspn`/`strcspn`/`strnlen` | ✅ | `string.c` |
| `sleep`/`usleep` | ✅ | `sleep.c` |
| `execvp`/`execlp`/`execl` | ✅ | `execvp.c` |
| `signal()` wrapper | ✅ | `signal_wrap.c` |
| `perror`/`fileno`/`fdopen` | ✅ | `stdio.c` |
| `free()` (proper allocator) | ✅ | `stdlib.c` — free-list with coalescing |
| `.gitignore` | ✅ | exists at root |
| `.gitmodules` | ✅ | exists (lwip + doomgeneric) |

---

## Part 2: What's STILL Truly Missing (verified against source code)

### 2A. Missing Kernel Syscalls

| Syscall | Priority | Needed By | Effort |
|---------|----------|-----------|--------|
| `getrusage` | High | Bash (`time` builtin), make | Small — wrapper over process utime/stime |
| `wait4` | Medium | Some programs (fallback for waitpid) | Small — wrapper over waitpid + rusage |
| `ioctl FIONREAD` | High | Bash, many programs | Small — return bytes available in pipe/tty buffer |
| `umount2` | Low | Full mount/umount | Small |
| `lstat` | Medium | ls -l, find, many programs | Small — stat without following symlinks |
| `fchmod`/`fchown` | Medium | Bash, tar, cp -p | Small — chmod/chown by fd |
| `uname` syscall | Medium | autoconf, Bash | Small — return system info struct |
| `getrusage` | High | Bash `time`, make | Small |

### 2B. Missing ulibc Headers (not in `user/ulibc/include/`)

| Header | Priority | Needed By | Effort |
|--------|----------|-----------|--------|
| `<regex.h>` | **Critical** | Bash `[[ =~ ]]`, grep, sed | Large — need regex engine |
| `<glob.h>` | **Critical** | Bash wildcard expansion | Medium — glob/globfree |
| `<sys/utsname.h>` | High | autoconf, uname command, Bash | Small — struct + uname() |
| `<poll.h>` | High | Many programs | Small — poll/struct pollfd |
| `<libgen.h>` | Medium | POSIX basename/dirname | Small |
| `<netdb.h>` | Medium | Network programs | Medium |
| `<netinet/in.h>` | Medium | Network programs | Small |
| `<arpa/inet.h>` | Medium | Network programs | Small |
| `<sys/socket.h>` | Medium | Network programs | Medium |
| `<dlfcn.h>` | Low | GCC plugins, shared libs | Small |
| `<spawn.h>` | Low | Some programs | Small |
| `<wordexp.h>` | Low | Bash | Medium |
| `<sys/un.h>` | Low | Unix domain sockets | Small |
| `<syslog.h>` | Low | Daemons | Small (stubs) |

### 2C. Missing Functions in EXISTING ulibc Headers

#### `<stdio.h>` — Missing:
| Function | Priority | Effort |
|----------|----------|--------|
| `popen`/`pclose` | High (Bash) | Medium — fork+exec+pipe |
| `getline`/`getdelim` | High (many programs) | Small |
| `ungetc` | Medium | Small — 1 byte pushback |
| `clearerr` | Medium | Trivial |
| `scanf`/`fscanf` | Medium | Large (full implementation) |
| `tmpfile`/`tmpnam` | Medium (GCC) | Small |

#### `<unistd.h>` — Missing:
| Function | Priority | Effort |
|----------|----------|--------|
| `sysconf`/`pathconf`/`fpathconf` | **Critical** (autoconf) | Medium — return constants |
| `tcgetpgrp`/`tcsetpgrp` | High (Bash job control) | Small — ioctl wrapper |
| `pipe2` | Medium | Small — syscall exists |
| `gethostname` | Medium | Small |
| `ttyname`/`ttyname_r` | Medium | Small |
| `getlogin` | Low | Small |
| `confstr` | Low | Small |

#### `<signal.h>` — Missing:
| Function | Priority | Effort |
|----------|----------|--------|
| `sigemptyset`/`sigfillset`/`sigaddset`/`sigdelset`/`sigismember` | **Critical** | Trivial — macros on uint32_t |

#### `<pthread.h>` — Missing:
| Function | Priority | Effort |
|----------|----------|--------|
| `pthread_mutex_init/lock/unlock/destroy` | Medium | Small — futex-based |
| `pthread_cond_*` | Medium | Medium |
| `pthread_key_*` (TLS) | Medium | Medium |
| `pthread_once` | Low | Small |

#### `<termios.h>` — Missing:
| Function | Priority | Effort |
|----------|----------|--------|
| `cfmakeraw` | High (Bash) | Trivial — set flags |
| `cfgetispeed`/`cfsetispeed` etc. | Medium | Small |
| `tcdrain`/`tcflush`/`tcflow` | Medium | Small — ioctl wrappers |

### 2D. Other Gaps

| Gap | Priority | Impact |
|-----|----------|--------|
| `/etc/passwd` and `/etc/group` files | High | pwd.h functions return hardcoded "root" |
| `sigsetjmp` doesn't save/restore signal mask | Medium | Bash error recovery |
| `opendir`/`readdir`/`closedir` in ulibc | **Critical** | Bash glob, ls, find, many programs |
| `ssize_t` return types for read/write | Low | POSIX compliance |

---

## Part 3: Self-Hosting Assessment

### 3A. Cross-Toolchain Status: ✅ COMPLETE

| Component | Version | Status |
|-----------|---------|--------|
| `i686-adros-gcc` | 13.2.0 | ✅ Installed at `/opt/adros-toolchain/bin/` |
| `i686-adros-g++` | 13.2.0 | ✅ Available |
| `i686-adros-as` | 2.42 | ✅ Binutils |
| `i686-adros-ld` | 2.42 | ✅ Binutils |
| Newlib (libc) | 4.4.0 | ✅ Installed in sysroot |
| libgloss/adros | — | ✅ crt0.o + libadros.a in sysroot |
| Bash | 5.2.21 | ✅ Cross-compiled (static ELF 32-bit i386) |

### 3B. Can Bash Run on AdrOS? — NOT YET

**The Bash binary exists** (`toolchain/build/bash/bash`, ELF 32-bit i386, static), but it was compiled with **stub implementations** in `posix_stubs.c`. These stubs return `ENOSYS` for critical operations:

| Stubbed Function | Real AdrOS Syscall | Status |
|-----------------|-------------------|--------|
| `dup(fd)` | SYSCALL_DUP (12) | ❌ Stub returns ENOSYS |
| `dup2(old, new)` | SYSCALL_DUP2 (13) | ❌ Stub returns ENOSYS |
| `fcntl(fd, cmd)` | SYSCALL_FCNTL (31) | ❌ Stub returns ENOSYS |
| `pipe(fds)` | SYSCALL_PIPE (14) | ❌ Stub returns ENOSYS |
| `select(...)` | SYSCALL_SELECT (20) | ❌ Stub returns ENOSYS |
| `sigaction(...)` | SYSCALL_SIGACTION (25) | ❌ Stub returns ENOSYS |
| `setpgid(...)` | SYSCALL_SETPGID (23) | ❌ Stub returns ENOSYS |
| `setsid()` | SYSCALL_SETSID (22) | ❌ Stub returns ENOSYS |
| `tcgetattr(...)` | SYSCALL_IOCTL/TCGETS (21) | ❌ Stub returns ENOSYS |
| `tcsetattr(...)` | SYSCALL_IOCTL/TCSETS (21) | ❌ Stub returns ENOSYS |
| `chdir(path)` | SYSCALL_CHDIR (32) | ❌ Stub returns ENOSYS |
| `getcwd(buf, sz)` | SYSCALL_GETCWD (33) | ❌ Returns "/" hardcoded |
| `ioctl(fd, req)` | SYSCALL_IOCTL (21) | ❌ Stub returns ENOSYS |
| `opendir/readdir/closedir` | SYSCALL_GETDENTS (30) | ❌ Stub returns ENOSYS/NULL |
| `access(path, mode)` | SYSCALL_ACCESS (74) | ❌ Stub always returns 0 |
| `waitpid(...)` | SYSCALL_WAITPID (7) | ❌ Stub returns ECHILD |

**The kernel has ALL these syscalls implemented.** The blocker is that the Newlib libgloss stubs don't call them.

### 3C. Can GCC/Binutils Run Natively? — NO (far from it)

Native self-hosting requires:
1. **Bash or sh** working natively (for configure scripts) — NOT YET
2. **GCC + binutils** as native ELF binaries running on AdrOS — NOT BUILT
3. **Enough RAM** — GCC needs ~64MB+ for compilation
4. **Working /tmp** — tmpfs exists ✅
5. **Working pipes/process creation** — kernel supports ✅, libgloss stubs ❌
6. **Time functions** — gettimeofday ✅, clock_gettime ✅
7. **Large file support** — 32-bit off_t limits files to 4GB (acceptable for now)

**Critical path to self-hosting:**
```
Fix libgloss stubs → Bash runs on AdrOS → Cross-compile native GCC/binutils
→ Package on disk image → Boot AdrOS → Run native GCC
```

### 3D. Can Busybox Run on AdrOS? — NOT YET

Same libgloss stub problem as Bash, plus Busybox needs additional APIs:
- `getpwent`/`getgrent` iteration
- `syslog()` (can be stubbed)
- Network socket wrappers in libc

---

## Part 4: Prioritized Implementation Plan

### Phase 0: IMMEDIATE — Fix libgloss posix_stubs.c (1-2 days)

**This is THE critical blocker.** Convert all ENOSYS stubs in `newlib/libgloss/adros/posix_stubs.c` to real AdrOS syscall wrappers. The kernel already has every required syscall.

Functions to implement (in order of importance):

1. **dup, dup2** → INT 0x80 with SYSCALL_DUP/DUP2
2. **pipe** → INT 0x80 with SYSCALL_PIPE
3. **fcntl** → INT 0x80 with SYSCALL_FCNTL
4. **sigaction, sigprocmask** → INT 0x80 with SYSCALL_SIGACTION/SIGPROCMASK
5. **waitpid** → INT 0x80 with SYSCALL_WAITPID
6. **setpgid, setsid, getpgrp** → INT 0x80 with SYSCALL_SETPGID/SETSID/GETPGRP
7. **chdir, getcwd** → INT 0x80 with SYSCALL_CHDIR/GETCWD
8. **select** → INT 0x80 with SYSCALL_SELECT
9. **tcgetattr, tcsetattr** → INT 0x80 with SYSCALL_IOCTL + TCGETS/TCSETS
10. **tcgetpgrp, tcsetpgrp** → INT 0x80 with SYSCALL_IOCTL + TIOCGPGRP/TIOCSPGRP
11. **ioctl** → INT 0x80 with SYSCALL_IOCTL
12. **access** → INT 0x80 with SYSCALL_ACCESS
13. **alarm** → INT 0x80 with SYSCALL_ALARM
14. **chown** → INT 0x80 with SYSCALL_CHOWN
15. **opendir/readdir/closedir** → wrap SYSCALL_OPEN + SYSCALL_GETDENTS + SYSCALL_CLOSE
16. **sleep** → INT 0x80 with SYSCALL_NANOSLEEP
17. **umask** → INT 0x80 with SYSCALL_UMASK
18. **getuid/geteuid/getgid/getegid/getppid/setuid/setgid** → direct syscalls

After fixing: **rebuild Bash** with `./toolchain/build.sh` and test on AdrOS.

### Phase 1: Critical ulibc Gaps for Bash (2-3 days)

Even with Newlib as the runtime libc, these gaps affect programs compiled against ulibc (the native AdrOS userland):

1. **sigset macros** — `sigemptyset`/`sigfillset`/`sigaddset`/`sigdelset`/`sigismember` (trivial bitops)
2. **`<sys/utsname.h>`** + `uname` syscall — struct utsname with sysname/nodename/release/version/machine
3. **`<poll.h>`** — struct pollfd + poll() wrapper (syscall exists)
4. **`<glob.h>`** — glob/globfree (Bash wildcard expansion)
5. **`opendir`/`readdir`/`closedir`** in ulibc — wrap getdents syscall
6. **`sysconf`/`pathconf`** — return compile-time constants (_SC_CLK_TCK, _SC_PAGE_SIZE, etc.)
7. **`tcgetpgrp`/`tcsetpgrp`** — ioctl wrappers
8. **`cfmakeraw`** — termios convenience function
9. **`getline`/`getdelim`** — dynamic line reading
10. **`popen`/`pclose`** — fork+exec+pipe
11. **`clearerr`/`ungetc`** — stdio functions

### Phase 2: Additional Syscalls (1 day)

1. **`getrusage`** — aggregate utime/stime into struct rusage
2. **`uname`** — return static struct utsname ("AdrOS", "adros", "1.0.0", "i686")
3. **`lstat`** — stat without following symlinks
4. **`fchmod`/`fchown`** — by file descriptor
5. **`ioctl FIONREAD`** — return bytes available in pipe/tty/socket buffer
6. **`wait4`** — waitpid + rusage

### Phase 3: Regex Engine (2-3 days)

Bash needs `<regex.h>` for `[[ string =~ pattern ]]`. Options:
- **Port TRE** (lightweight POSIX regex, ~5000 lines) — recommended
- **Port musl regex** — smaller but less featureful
- **Write minimal engine** — only ERE subset needed by Bash

### Phase 4: Test Bash on AdrOS (1 day)

1. Rebuild Bash with fixed libgloss stubs
2. Package `bash` binary into AdrOS initrd
3. Boot with `init=/bin/bash`
4. Test: command execution, pipes, redirects, job control, `$PATH`, wildcards, `~` expansion
5. Fix issues iteratively

### Phase 5: Cross-compile Busybox (1 week)

1. Configure Busybox with minimal config (disable applets that need missing APIs)
2. Cross-compile with `i686-adros-gcc`
3. Fix link errors iteratively (add missing stubs/implementations)
4. Package in initrd — replaces individual `/bin/*` utilities
5. Key applets to target first: sh, ls, cat, cp, mv, rm, mkdir, rmdir, mount, umount, ps, grep, sed, awk, find, xargs, tar

### Phase 6: Cross-compile Native Binutils + GCC (2-3 weeks)

This is the final step to self-hosting:

1. **Native Binutils** — cross-compile binutils with `--host=i686-adros --target=i686-adros`
2. **Native GCC (stage 1)** — cross-compile GCC C-only with `--host=i686-adros --target=i686-adros`
3. **Package on ext2 disk image** — toolchain binaries + headers + libraries
4. **Test on AdrOS** — boot, mount ext2, run `gcc -o hello hello.c`
5. **Bootstrap (stage 2)** — use native GCC to rebuild itself on AdrOS

**Prerequisites for Phase 6:**
- Working Bash (for configure scripts)
- Working make (needs fork/exec/waitpid/pipes — all available after Phase 0)
- Working sed, grep, awk (Busybox or standalone)
- At least 64MB RAM available to processes
- /tmp with enough space for GCC temporaries

---

## Part 5: Effort Estimate Summary

| Phase | Description | Effort | Dependency |
|-------|-------------|--------|------------|
| **Phase 0** | Fix libgloss posix_stubs.c | 1-2 days | None |
| **Phase 1** | Critical ulibc gaps | 2-3 days | None (parallel) |
| **Phase 2** | Additional syscalls | 1 day | None (parallel) |
| **Phase 3** | Regex engine | 2-3 days | Phase 1 |
| **Phase 4** | Test Bash on AdrOS | 1 day | Phase 0 |
| **Phase 5** | Busybox cross-compile | 1 week | Phase 0, 2, 3 |
| **Phase 6** | Native GCC/Binutils | 2-3 weeks | Phase 4, 5 |

**Total to Bash running:** ~1 week
**Total to Busybox running:** ~2-3 weeks
**Total to self-hosting GCC:** ~1-2 months

---

## Part 6: Current Score Assessment

| Category | Audit Claim | Actual (verified) |
|----------|-------------|-------------------|
| Kernel syscalls | ~85% POSIX | **~92%** — 135 syscall numbers, missing ~8 |
| ulibc coverage | "NOT sufficient" | **Much improved** — 25 headers, proper malloc, but still missing regex/glob/network headers |
| Cross-toolchain | Not started | **✅ COMPLETE** — GCC 13.2.0 + Newlib + Binutils |
| Bash cross-compiled | Not started | **✅ DONE** — static ELF, needs runtime stubs |
| Bash running on AdrOS | ❌ | ❌ — libgloss stubs return ENOSYS |
| Busybox | Not started | ❌ |
| Native GCC | Not started | ❌ |

**Bottom line:** The kernel is ~92% POSIX-ready. The cross-toolchain is complete. The #1 blocker to running Bash natively is **converting the ~30 ENOSYS stubs in libgloss/posix_stubs.c to real AdrOS syscall wrappers** — the kernel already supports every required operation. This is a straightforward 1-2 day task that unblocks everything else.
