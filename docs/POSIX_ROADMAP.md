# AdrOS POSIX Roadmap (Checklist)

This document tracks **what is already implemented** versus **what is missing** to reach a practical Unix-like system with full POSIX compatibility.

Notes:
- This is intentionally pragmatic: items are ordered to unlock userland capabilities quickly.
- Checkboxes reflect the current state of the `master` branch.

## Status Legend
- `[x]` implemented (works end-to-end, smoke-tested)
- `[~]` partial (exists but incomplete/limited)
- `[ ]` not implemented

---

## 1. Syscalls — File I/O

| Syscall | Status | Notes |
|---------|--------|-------|
| `open` | [x] | Supports `O_CREAT`, `O_TRUNC`; works on diskfs, devfs, tmpfs, overlayfs |
| `openat` | [x] | `AT_FDCWD` supported; other dirfd values return `ENOSYS` |
| `read` | [x] | Files, pipes, TTY, PTY; `O_NONBLOCK` returns `EAGAIN` |
| `write` | [x] | Files, pipes, TTY, PTY; `O_NONBLOCK` returns `EAGAIN` |
| `close` | [x] | Refcounted file objects |
| `lseek` | [x] | `SEEK_SET`, `SEEK_CUR`, `SEEK_END` |
| `stat` | [x] | Minimal `struct stat` (mode/type/size/inode) |
| `fstat` | [x] | |
| `fstatat` | [x] | `AT_FDCWD` supported |
| `dup` | [x] | |
| `dup2` | [x] | |
| `dup3` | [x] | Flags parameter (currently only `flags=0` accepted) |
| `pipe` | [x] | In-kernel ring buffer |
| `pipe2` | [x] | Supports `O_NONBLOCK` flag |
| `select` | [x] | Minimal (pipes, TTY) |
| `poll` | [x] | Minimal (pipes, TTY, `/dev/null`) |
| `ioctl` | [x] | `TCGETS`, `TCSETS`, `TIOCGPGRP`, `TIOCSPGRP` |
| `fcntl` | [x] | `F_GETFL`, `F_SETFL` (for `O_NONBLOCK`) |
| `getdents` | [x] | Generic across all VFS (diskfs, tmpfs, devfs, overlayfs) |
| `pread`/`pwrite` | [ ] | |
| `readv`/`writev` | [ ] | |
| `truncate`/`ftruncate` | [ ] | |
| `fsync`/`fdatasync` | [ ] | No-op acceptable for now |

## 2. Syscalls — Directory & Path Operations

| Syscall | Status | Notes |
|---------|--------|-------|
| `mkdir` | [x] | diskfs |
| `rmdir` | [x] | diskfs; checks directory is empty (`ENOTEMPTY`) |
| `unlink` | [x] | diskfs; returns `EISDIR` for directories |
| `unlinkat` | [x] | `AT_FDCWD` supported |
| `rename` | [x] | diskfs; handles same-type overwrite |
| `chdir` | [x] | Per-process `cwd` |
| `getcwd` | [x] | |
| `link` | [ ] | Hard links |
| `symlink` | [ ] | Symbolic links |
| `readlink` | [ ] | |
| `access` | [ ] | Permission checks |
| `umask` | [ ] | |
| `realpath` | [ ] | Userland (needs libc) |

## 3. Syscalls — Process Management

| Syscall | Status | Notes |
|---------|--------|-------|
| `fork` | [x] | Full COW implemented (`vmm_as_clone_user_cow` + `vmm_handle_cow_fault`) |
| `execve` | [x] | Loads ELF from VFS; argv/envp; `O_CLOEXEC` FDs closed |
| `exit` / `_exit` | [x] | Closes FDs, marks zombie, notifies parent |
| `waitpid` | [x] | `-1` (any child), specific pid, `WNOHANG` |
| `getpid` | [x] | |
| `getppid` | [x] | |
| `setsid` | [x] | |
| `setpgid` | [x] | |
| `getpgrp` | [x] | |
| `getuid`/`getgid`/`geteuid`/`getegid` | [ ] | No user/group model yet |
| `setuid`/`setgid` | [ ] | |
| `brk`/`sbrk` | [x] | `syscall_brk_impl()` — per-process heap break |
| `mmap`/`munmap` | [ ] | Memory-mapped I/O |
| `clone` | [ ] | Thread creation |
| `nanosleep`/`sleep` | [x] | `syscall_nanosleep_impl()` with tick-based sleep |
| `clock_gettime` | [x] | `CLOCK_REALTIME` and `CLOCK_MONOTONIC` |
| `alarm` | [ ] | |
| `times`/`getrusage` | [ ] | |

## 4. Syscalls — Signals

| Syscall | Status | Notes |
|---------|--------|-------|
| `sigaction` | [x] | Installs handlers; `sa_flags` minimal |
| `sigprocmask` | [x] | Block/unblock signals |
| `kill` | [x] | Send signal to process/group |
| `sigreturn` | [x] | Trampoline-based return from signal handlers |
| `raise` | [ ] | Userland (needs libc) |
| `sigpending` | [ ] | |
| `sigsuspend` | [ ] | |
| `sigqueue` | [ ] | |
| `sigaltstack` | [ ] | Alternate signal stack |
| Signal defaults | [x] | `SIGKILL`/`SIGSEGV`/`SIGUSR1`/`SIGINT`/`SIGTSTP`/`SIGTTOU`/`SIGTTIN` handled |

## 5. File Descriptor Layer

| Feature | Status | Notes |
|---------|--------|-------|
| Per-process fd table | [x] | Up to `PROCESS_MAX_FILES` entries |
| Refcounted file objects | [x] | Shared across `dup`/`fork` |
| File offset tracking | [x] | |
| `O_NONBLOCK` | [x] | Pipes, TTY, PTY via `fcntl` or `pipe2` |
| `O_CLOEXEC` | [x] | Close-on-exec via `pipe2`, `open` flags |
| `O_APPEND` | [ ] | |
| `FD_CLOEXEC` via `fcntl` | [x] | `F_GETFD`/`F_SETFD` implemented; `execve` closes marked FDs |
| File locking (`flock`/`fcntl`) | [ ] | |

## 6. Filesystem / VFS

| Feature | Status | Notes |
|---------|--------|-------|
| VFS mount table | [x] | Up to 8 mounts |
| `vfs_lookup` path resolution | [x] | Absolute + relative (via `cwd`) |
| `fs_node_t` with `read`/`write`/`finddir`/`readdir` | [x] | |
| `struct vfs_dirent` (generic) | [x] | Unified format across all FS |
| **tmpfs** | [x] | In-memory; dirs + files; `readdir` |
| **overlayfs** | [x] | Copy-up; `readdir` delegates to upper/lower |
| **devfs** | [x] | `/dev/null`, `/dev/tty`, `/dev/ptmx`, `/dev/pts/0`; `readdir` |
| **diskfs** (on-disk) | [x] | Hierarchical inodes; `open`/`read`/`write`/`stat`/`mkdir`/`unlink`/`rmdir`/`rename`/`getdents` |
| **persistfs** | [x] | Minimal persistence at `/persist` |
| **procfs** | [~] | `/proc/meminfo` exists; no per-process `/proc/[pid]` |
| Permissions (`uid`/`gid`/mode) | [ ] | No permission model |
| Hard links | [ ] | |
| Symbolic links | [ ] | |
| ext2 / FAT support | [ ] | |

## 7. TTY / PTY

| Feature | Status | Notes |
|---------|--------|-------|
| Canonical input (line-buffered) | [x] | |
| Echo + backspace | [x] | |
| Blocking reads + wait queue | [x] | |
| `TCGETS`/`TCSETS` | [x] | Minimal termios |
| `TIOCGPGRP`/`TIOCSPGRP` | [x] | |
| Job control (`SIGTTIN`/`SIGTTOU`) | [x] | Background pgrp enforcement |
| `isatty` (via `ioctl TCGETS`) | [x] | |
| PTY master/slave | [x] | `/dev/ptmx` + `/dev/pts/0` |
| Non-blocking PTY I/O | [x] | |
| Raw mode (non-canonical) | [x] | Clear `ICANON` via `TCSETS` |
| VMIN/VTIME | [ ] | |
| Signal characters (Ctrl+C → `SIGINT`, etc.) | [x] | Ctrl+C→SIGINT, Ctrl+Z→SIGTSTP, Ctrl+D→EOF, Ctrl+\\→SIGQUIT |
| Multiple PTY pairs | [ ] | Only 1 pair currently |
| Window size (`TIOCGWINSZ`/`TIOCSWINSZ`) | [x] | Get/set `struct winsize` |

## 8. Memory Management

| Feature | Status | Notes |
|---------|--------|-------|
| PMM (bitmap allocator) | [x] | Spinlock-protected, frame refcounting |
| VMM (x86 paging) | [x] | Higher-half kernel, recursive page directory |
| Per-process address spaces | [x] | Page directory per process |
| Kernel heap (`kmalloc`/`kfree`) | [x] | Dynamic growth up to 64MB |
| Slab allocator | [x] | `slab_cache_t` with free-list-in-place |
| W^X for user ELFs | [x] | Text segments read-only after load |
| SMEP | [x] | Enabled in CR4 if CPU supports |
| `brk`/`sbrk` | [x] | Per-process heap break |
| `mmap`/`munmap` | [ ] | |
| Shared memory (`shmget`/`shmat`/`shmdt`) | [x] | System V IPC style |
| Copy-on-write (COW) fork | [x] | PTE bit 9 as CoW marker + page fault handler |
| PAE + NX bit | [ ] | |
| Guard pages | [ ] | |
| ASLR | [ ] | |

## 9. Drivers & Hardware

| Feature | Status | Notes |
|---------|--------|-------|
| UART serial console | [x] | |
| VGA text console (x86) | [x] | |
| PS/2 keyboard | [x] | |
| PIT timer | [x] | |
| LAPIC timer | [x] | Calibrated, used when APIC available |
| ATA PIO (IDE) | [x] | Primary master |
| ATA DMA (Bus Master IDE) | [x] | Bounce buffer, PRDT, IRQ-coordinated |
| PCI enumeration | [x] | Full bus/slot/func scan with BAR + IRQ |
| ACPI (MADT parsing) | [x] | CPU topology + IOAPIC discovery |
| LAPIC + IOAPIC | [x] | Replaces legacy PIC |
| SMP (multi-CPU boot) | [x] | 4 CPUs via INIT-SIPI-SIPI, per-CPU data via GS |
| CPUID feature detection | [x] | Leaf 0/1/7/extended; SMEP/SMAP detection |
| VBE framebuffer | [x] | Maps LFB, pixel drawing, font rendering |
| SYSENTER fast syscall | [x] | MSR setup + handler |
| RTC (real-time clock) | [ ] | |
| Network (e1000/virtio-net) | [ ] | |
| Virtio-blk | [ ] | |

## 10. Userland

| Feature | Status | Notes |
|---------|--------|-------|
| ELF32 loader | [x] | Secure with W^X enforcement |
| `/bin/init.elf` (smoke tests) | [x] | Comprehensive test suite |
| `/bin/echo.elf` | [x] | Minimal argv/envp test |
| Minimal libc (ulibc) | [x] | `printf`, `malloc`/`free`/`calloc`/`realloc`, `string.h`, `unistd.h`, `errno.h` |
| Shell (`sh`) | [ ] | |
| Core utilities (`ls`, `cat`, `cp`, `mv`, `rm`, `mkdir`) | [ ] | |
| Dynamic linking | [ ] | |
| `$PATH` search in `execve` | [ ] | |

## 11. Networking (future)

| Feature | Status | Notes |
|---------|--------|-------|
| `socket` | [ ] | |
| `bind`/`listen`/`accept` | [ ] | |
| `connect`/`send`/`recv` | [ ] | |
| TCP/IP stack | [ ] | |
| UDP | [ ] | |
| DNS resolver | [ ] | |
| `/etc/hosts` | [ ] | |
| `getaddrinfo` | [ ] | Userland (needs libc) |

---

## Priority Roadmap (next steps)

### Near-term (unlock a usable shell)
1. ~~Minimal libc~~ ✅ ulibc implemented (`printf`, `malloc`, `string.h`, `unistd.h`, `errno.h`)
2. **Shell** — `sh`-compatible; all required syscalls are implemented
3. **Core utilities** — `ls` (uses `getdents`), `cat`, `echo`, `mkdir`, `rm`
4. ~~Signal characters~~ ✅ Ctrl+C→SIGINT, Ctrl+Z→SIGTSTP, Ctrl+D→EOF
5. ~~Raw TTY mode~~ ✅ ICANON clearable via TCSETS
6. **`/dev/zero`** + **`/dev/random`** — simple device nodes
7. **Multiple PTY pairs** — currently only 1

### Medium-term (real POSIX compliance)
8. ~~`brk`/`sbrk`~~ ✅ Implemented
9. **`mmap`/`munmap`** — memory-mapped files, shared memory
10. **Permissions** — `uid`/`gid`, mode bits, `chmod`, `chown`, `access`, `umask`
11. ~~`O_CLOEXEC`~~ ✅ Implemented
12. **`/proc` per-process** — `/proc/[pid]/status`, `/proc/[pid]/maps`
13. **Hard/symbolic links** — `link`, `symlink`, `readlink`
14. **VMIN/VTIME** — termios non-canonical timing
15. **Generic wait queue abstraction** — replace ad-hoc blocking

### Long-term (full Unix experience)
16. **Networking** — socket API, TCP/IP stack
17. **Multi-arch bring-up** — ARM/RISC-V functional kernels
18. ~~COW fork~~ ✅ Implemented
19. **Threads** (`clone`/`pthread`)
20. **Dynamic linking** (`ld.so`)
21. **ext2/FAT** filesystem support
22. **PAE + NX bit** — hardware W^X
23. **Per-thread errno** (needs TLS)
24. **vDSO** — fast `clock_gettime` without syscall
