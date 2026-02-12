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
| `read` | [x] | Files, pipes, TTY, PTY, sockets; `O_NONBLOCK` returns `EAGAIN` |
| `write` | [x] | Files, pipes, TTY, PTY, sockets; `O_NONBLOCK` returns `EAGAIN` |
| `close` | [x] | Refcounted file objects |
| `lseek` | [x] | `SEEK_SET`, `SEEK_CUR`, `SEEK_END` |
| `stat` | [x] | `struct stat` with mode/type/size/inode/uid/gid/nlink |
| `fstat` | [x] | |
| `fstatat` | [x] | `AT_FDCWD` supported |
| `dup` | [x] | |
| `dup2` | [x] | |
| `dup3` | [x] | Flags parameter |
| `pipe` | [x] | In-kernel ring buffer |
| `pipe2` | [x] | Supports `O_NONBLOCK` flag |
| `select` | [x] | Pipes, TTY, sockets |
| `poll` | [x] | Pipes, TTY, PTY, `/dev/null`, sockets |
| `ioctl` | [x] | `TCGETS`, `TCSETS`, `TIOCGPGRP`, `TIOCSPGRP`, `TIOCGWINSZ`, `TIOCSWINSZ` |
| `fcntl` | [x] | `F_GETFL`, `F_SETFL`, `F_GETFD`, `F_SETFD` |
| `getdents` | [x] | Generic across all VFS (diskfs, tmpfs, devfs, overlayfs, procfs) |
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
| `link` | [x] | Hard links (stub — returns `ENOSYS` for cross-fs) |
| `symlink` | [x] | Symbolic links in diskfs |
| `readlink` | [x] | |
| `chmod` | [x] | Set mode bits on VFS nodes |
| `chown` | [x] | Set uid/gid on VFS nodes |
| `access` | [ ] | Permission checks |
| `umask` | [ ] | |
| `realpath` | [ ] | Userland (needs libc) |

## 3. Syscalls — Process Management

| Syscall | Status | Notes |
|---------|--------|-------|
| `fork` | [x] | Full COW implemented (`vmm_as_clone_user_cow` + `vmm_handle_cow_fault`) |
| `execve` | [x] | Loads ELF from VFS; argv/envp; `O_CLOEXEC` FDs closed; `PT_INTERP` support |
| `exit` / `_exit` | [x] | Closes FDs, marks zombie, notifies parent |
| `waitpid` | [x] | `-1` (any child), specific pid, `WNOHANG` |
| `getpid` | [x] | |
| `getppid` | [x] | |
| `gettid` | [x] | Returns per-thread ID |
| `setsid` | [x] | |
| `setpgid` | [x] | |
| `getpgrp` | [x] | |
| `getuid`/`getgid` | [x] | Per-process uid/gid |
| `setuid`/`setgid` | [ ] | |
| `brk`/`sbrk` | [x] | `syscall_brk_impl()` — per-process heap break |
| `mmap`/`munmap` | [x] | Anonymous mappings + shared memory |
| `clone` | [x] | Thread creation with `CLONE_VM`/`CLONE_FILES`/`CLONE_THREAD`/`CLONE_SETTLS` |
| `set_thread_area` | [x] | GDT-based TLS via GS segment (GDT entry 22, ring 3) |
| `nanosleep`/`sleep` | [x] | `syscall_nanosleep_impl()` with tick-based sleep |
| `clock_gettime` | [x] | `CLOCK_REALTIME` and `CLOCK_MONOTONIC` |
| `alarm` | [ ] | |
| `times`/`getrusage` | [ ] | |

## 4. Syscalls — Signals

| Syscall | Status | Notes |
|---------|--------|-------|
| `sigaction` | [x] | Installs handlers; `SA_SIGINFO` supported |
| `sigprocmask` | [x] | Block/unblock signals |
| `kill` | [x] | Send signal to process/group |
| `sigreturn` | [x] | Trampoline-based return from signal handlers |
| `raise` | [ ] | Userland (needs libc) |
| `sigpending` | [ ] | |
| `sigsuspend` | [ ] | |
| `sigqueue` | [ ] | |
| `sigaltstack` | [ ] | Alternate signal stack |
| Signal defaults | [x] | `SIGKILL`/`SIGSEGV`/`SIGUSR1`/`SIGINT`/`SIGTSTP`/`SIGTTOU`/`SIGTTIN`/`SIGQUIT` handled |

## 5. File Descriptor Layer

| Feature | Status | Notes |
|---------|--------|-------|
| Per-process fd table | [x] | Up to `PROCESS_MAX_FILES` entries |
| Refcounted file objects | [x] | Shared across `dup`/`fork`/`clone` with atomic refcounts |
| File offset tracking | [x] | |
| `O_NONBLOCK` | [x] | Pipes, TTY, PTY, sockets via `fcntl` or `pipe2` |
| `O_CLOEXEC` | [x] | Close-on-exec via `pipe2`, `open` flags |
| `O_APPEND` | [ ] | |
| `FD_CLOEXEC` via `fcntl` | [x] | `F_GETFD`/`F_SETFD` implemented; `execve` closes marked FDs |
| File locking (`flock`/`fcntl`) | [ ] | |

## 6. Filesystem / VFS

| Feature | Status | Notes |
|---------|--------|-------|
| VFS mount table | [x] | Up to 8 mounts |
| `vfs_lookup` path resolution | [x] | Absolute + relative (via `cwd`); follows symlinks |
| `fs_node_t` with `read`/`write`/`finddir`/`readdir` | [x] | |
| `struct vfs_dirent` (generic) | [x] | Unified format across all FS |
| **tmpfs** | [x] | In-memory; dirs + files; `readdir` |
| **overlayfs** | [x] | Copy-up; `readdir` delegates to upper/lower |
| **devfs** | [x] | `/dev/null`, `/dev/zero`, `/dev/random`, `/dev/urandom`, `/dev/console`, `/dev/tty`, `/dev/ptmx`, `/dev/pts/N` |
| **diskfs** (on-disk) | [x] | Hierarchical inodes; full POSIX ops; symlinks |
| **persistfs** | [x] | Minimal persistence at `/persist` |
| **procfs** | [x] | `/proc/meminfo` + per-process `/proc/[pid]/status`, `/proc/[pid]/maps` |
| Permissions (`uid`/`gid`/mode) | [x] | `chmod`, `chown`; mode bits stored in VFS nodes |
| Hard links | [~] | `link` syscall exists (stub for cross-fs) |
| Symbolic links | [x] | `symlink`, `readlink`; followed by VFS lookup |
| ext2 / FAT support | [ ] | |

## 7. TTY / PTY

| Feature | Status | Notes |
|---------|--------|-------|
| Canonical input (line-buffered) | [x] | |
| Echo + backspace | [x] | |
| Blocking reads + wait queue | [x] | Generic `waitqueue_t` abstraction |
| `TCGETS`/`TCSETS` | [x] | Full termios with `c_cc[NCCS]` |
| `TIOCGPGRP`/`TIOCSPGRP` | [x] | |
| Job control (`SIGTTIN`/`SIGTTOU`) | [x] | Background pgrp enforcement |
| `isatty` (via `ioctl TCGETS`) | [x] | |
| PTY master/slave | [x] | `/dev/ptmx` + `/dev/pts/N` (dynamic, up to 8 pairs) |
| Non-blocking PTY I/O | [x] | |
| Raw mode (non-canonical) | [x] | Clear `ICANON` via `TCSETS` |
| VMIN/VTIME | [x] | Non-canonical timing with `c_cc[VMIN]`/`c_cc[VTIME]` |
| Signal characters (Ctrl+C → `SIGINT`, etc.) | [x] | Ctrl+C→SIGINT, Ctrl+Z→SIGTSTP, Ctrl+D→EOF, Ctrl+\\→SIGQUIT |
| Multiple PTY pairs | [x] | Up to `PTY_MAX_PAIRS=8`, dynamic `/dev/pts/N` |
| Window size (`TIOCGWINSZ`/`TIOCSWINSZ`) | [x] | Get/set `struct winsize` |

## 8. Memory Management

| Feature | Status | Notes |
|---------|--------|-------|
| PMM (bitmap allocator) | [x] | Spinlock-protected, frame refcounting |
| VMM (x86 PAE paging) | [x] | Higher-half kernel, recursive page directory, PAE mode |
| Per-process address spaces | [x] | PDPT + 4 PDs per process |
| Kernel heap (`kmalloc`/`kfree`) | [x] | Dynamic growth up to 64MB |
| Slab allocator | [x] | `slab_cache_t` with free-list-in-place |
| W^X for user ELFs | [x] | Text segments read-only after load |
| SMEP | [x] | Enabled in CR4 if CPU supports |
| `brk`/`sbrk` | [x] | Per-process heap break |
| `mmap`/`munmap` | [x] | Anonymous mappings, shared memory backing |
| Shared memory (`shmget`/`shmat`/`shmdt`) | [x] | System V IPC style |
| Copy-on-write (COW) fork | [x] | PTE bit 9 as CoW marker + page fault handler |
| PAE + NX bit | [x] | PAE paging with NX (bit 63) on data segments |
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
| E1000 NIC (Intel 82540EM) | [x] | MMIO-based, IRQ-driven, lwIP integration |
| RTC (real-time clock) | [ ] | |
| Virtio-blk | [ ] | |

## 10. Networking

| Feature | Status | Notes |
|---------|--------|-------|
| E1000 NIC driver | [x] | Intel 82540EM, MMIO, IRQ 11 via IOAPIC |
| lwIP TCP/IP stack | [x] | NO_SYS=1, IPv4, static IP (10.0.2.15) |
| `socket` | [x] | `AF_INET`, `SOCK_STREAM` (TCP), `SOCK_DGRAM` (UDP) |
| `bind`/`listen`/`accept` | [x] | TCP server support |
| `connect`/`send`/`recv` | [x] | TCP client support |
| `sendto`/`recvfrom` | [x] | UDP support |
| DNS resolver | [ ] | |
| `/etc/hosts` | [ ] | |
| `getaddrinfo` | [ ] | Userland (needs libc) |

## 11. Threads & TLS

| Feature | Status | Notes |
|---------|--------|-------|
| `clone` syscall | [x] | `CLONE_VM`, `CLONE_FILES`, `CLONE_THREAD`, `CLONE_SETTLS`, `CLONE_SIGHAND` |
| `gettid` syscall | [x] | Returns per-thread unique ID |
| `set_thread_area` | [x] | GDT entry 22, ring 3 data segment for user TLS via GS |
| Thread-group ID (tgid) | [x] | `getpid()` returns tgid for POSIX compliance |
| Shared address space | [x] | Threads share addr_space; thread reap doesn't destroy it |
| `CLONE_PARENT_SETTID` | [x] | Writes child tid to parent address |
| `CLONE_CHILD_CLEARTID` | [x] | Stores address for futex-wake on thread exit |
| ulibc `pthread.h` | [x] | `pthread_create`, `pthread_join`, `pthread_exit`, `pthread_self` |
| Per-thread errno | [x] | Via `set_thread_area` + TLS |
| Futex | [ ] | Required for efficient pthread_join/mutex |

## 12. Dynamic Linking

| Feature | Status | Notes |
|---------|--------|-------|
| `ET_DYN` ELF support | [x] | Kernel ELF loader accepts position-independent executables |
| `PT_INTERP` detection | [x] | Kernel reads interpreter path from ELF |
| Interpreter loading | [x] | `elf32_load_interp` loads ld.so at `INTERP_BASE=0x40000000` |
| ELF auxiliary vector types | [x] | `AT_PHDR`, `AT_PHENT`, `AT_PHNUM`, `AT_ENTRY`, `AT_BASE`, `AT_PAGESZ` defined |
| ELF relocation types | [x] | `R_386_RELATIVE`, `R_386_32`, `R_386_GLOB_DAT`, `R_386_JMP_SLOT` defined |
| `Elf32_Dyn`/`Elf32_Rel`/`Elf32_Sym` | [x] | Full dynamic section structures in `elf.h` |
| Userspace `ld.so` | [ ] | Stub; full relocation processing not yet implemented |
| Shared libraries (.so) | [ ] | Requires `ld.so` + `dlopen`/`dlsym` |

## 13. Userland

| Feature | Status | Notes |
|---------|--------|-------|
| ELF32 loader | [x] | Secure with W^X; supports `ET_EXEC` + `ET_DYN` + `PT_INTERP` |
| `/bin/init.elf` (smoke tests) | [x] | Comprehensive test suite (19+ checks) |
| `/bin/echo` | [x] | argv/envp test |
| `/bin/sh` | [x] | POSIX sh-compatible shell; builtins, pipes, redirects |
| `/bin/cat` | [x] | |
| `/bin/ls` | [x] | Uses `getdents` |
| `/bin/mkdir` | [x] | |
| `/bin/rm` | [x] | |
| Minimal libc (ulibc) | [x] | `printf`, `malloc`/`free`/`calloc`/`realloc`, `string.h`, `unistd.h`, `errno.h`, `pthread.h` |
| `$PATH` search in `execve` | [ ] | Shell does VFS lookup directly |

---

## Priority Roadmap (remaining work)

### All 15 planned features are now implemented ✅

1. ~~`/dev/zero`, `/dev/random`, `/dev/urandom`, `/dev/console`~~ ✅
2. ~~Multiple PTY pairs (dynamic `/dev/pts/N`)~~ ✅
3. ~~VMIN/VTIME termios~~ ✅
4. ~~Shell (`sh`-compatible)~~ ✅
5. ~~Core utilities (`cat`, `ls`, `mkdir`, `rm`)~~ ✅
6. ~~Generic wait queue abstraction~~ ✅
7. ~~`/proc` per-process (`/proc/[pid]/status`, `/proc/[pid]/maps`)~~ ✅
8. ~~Permissions (`chmod`, `chown`, `getuid`, `getgid`)~~ ✅
9. ~~Symbolic links (`symlink`, `readlink`)~~ ✅
10. ~~PAE + NX bit~~ ✅
11. ~~Per-thread errno + `set_thread_area` stub~~ ✅
12. ~~Networking (E1000 + lwIP + socket syscalls)~~ ✅
13. ~~Socket syscalls (`socket`/`bind`/`listen`/`accept`/`connect`/`send`/`recv`/`sendto`/`recvfrom`)~~ ✅
14. ~~Threads (`clone`/`pthread`)~~ ✅
15. ~~Dynamic linking infrastructure (`PT_INTERP`, `ET_DYN`, ELF relocation types)~~ ✅

### Future enhancements (beyond the 15 planned tasks)
- **Userspace `ld.so`** — full dynamic linker with relocation processing
- **Shared libraries (.so)** — `dlopen`/`dlsym`/`dlclose`
- **Futex** — efficient thread synchronization primitive
- **Multi-arch bring-up** — ARM/RISC-V functional kernels
- **ext2/FAT** filesystem support
- **ASLR** — address space layout randomization
- **vDSO** — fast `clock_gettime` without syscall
- **`O_APPEND`** — append mode for file writes
- **File locking** — `flock`/`fcntl` advisory locks
- **`pread`/`pwrite`/`readv`/`writev`** — scatter/gather I/O
- **`sigaltstack`** — alternate signal stack
- **DNS resolver** + `/etc/hosts`
- **RTC driver** — real-time clock for wall-clock time
- **`alarm`/`setitimer`** — timer signals
