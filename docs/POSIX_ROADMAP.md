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
| `open` | [x] | Supports `O_CREAT`, `O_TRUNC`, `O_APPEND`; works on diskfs, devfs, tmpfs, overlayfs |
| `openat` | [x] | `AT_FDCWD` supported; other dirfd values return `ENOSYS` |
| `read` | [x] | Files, pipes, TTY, PTY, sockets; `O_NONBLOCK` returns `EAGAIN` |
| `write` | [x] | Files, pipes, TTY, PTY, sockets; `O_NONBLOCK` returns `EAGAIN`; `O_APPEND` support |
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
| `pread`/`pwrite` | [x] | Atomic read/write at offset without changing file position |
| `readv`/`writev` | [x] | Scatter/gather I/O via `struct iovec` |
| `truncate`/`ftruncate` | [x] | Truncate file to given length |
| `fsync`/`fdatasync` | [x] | No-op stubs (accepted — no write cache to flush) |

## 2. Syscalls — Directory & Path Operations

| Syscall | Status | Notes |
|---------|--------|-------|
| `mkdir` | [x] | diskfs |
| `rmdir` | [x] | diskfs; checks directory is empty (`ENOTEMPTY`) |
| `unlink` | [x] | diskfs; returns `EISDIR` for directories; respects hard link count |
| `unlinkat` | [x] | `AT_FDCWD` supported |
| `rename` | [x] | diskfs; handles same-type overwrite |
| `chdir` | [x] | Per-process `cwd` |
| `getcwd` | [x] | |
| `link` | [x] | Hard links in diskfs with `nlink` tracking and shared data blocks |
| `symlink` | [x] | Symbolic links in diskfs |
| `readlink` | [x] | |
| `chmod` | [x] | Set mode bits on VFS nodes |
| `chown` | [x] | Set uid/gid on VFS nodes |
| `access` | [x] | Permission checks (`R_OK`, `W_OK`, `X_OK`, `F_OK`) |
| `umask` | [x] | Per-process file creation mask |
| `realpath` | [x] | Userland ulibc implementation (resolves `.`, `..`, normalizes) |

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
| `geteuid`/`getegid` | [x] | Effective uid/gid |
| `setuid`/`setgid` | [x] | Set process uid/gid; permission checks (only root can set arbitrary) |
| `seteuid`/`setegid` | [x] | Set effective uid/gid; permission checks |
| `brk`/`sbrk` | [x] | `syscall_brk_impl()` — per-process heap break |
| `mmap`/`munmap` | [x] | Anonymous mappings, shared memory backing, and file-backed (fd) mappings |
| `clone` | [x] | Thread creation with `CLONE_VM`/`CLONE_FILES`/`CLONE_THREAD`/`CLONE_SETTLS` |
| `set_thread_area` | [x] | GDT-based TLS via GS segment (GDT entry 22, ring 3) |
| `nanosleep`/`sleep` | [x] | `syscall_nanosleep_impl()` with tick-based sleep |
| `clock_gettime` | [x] | `CLOCK_REALTIME` (RTC-backed) and `CLOCK_MONOTONIC` (tick-based) |
| `alarm` | [x] | Per-process alarm timer; delivers `SIGALRM` on expiry |
| `times` | [x] | Returns `struct tms` with per-process `utime`/`stime` accounting |
| `futex` | [x] | `FUTEX_WAIT`/`FUTEX_WAKE` with global waiter table |

## 4. Syscalls — Signals

| Syscall | Status | Notes |
|---------|--------|-------|
| `sigaction` | [x] | Installs handlers; `SA_SIGINFO` supported |
| `sigprocmask` | [x] | Block/unblock signals |
| `kill` | [x] | Send signal to process/group |
| `sigreturn` | [x] | Trampoline-based return from signal handlers |
| `raise` | [x] | ulibc implementation (`kill(getpid(), sig)`) |
| `sigpending` | [x] | Returns pending signal mask |
| `sigsuspend` | [x] | Atomically set signal mask and wait |
| `sigaltstack` | [x] | Alternate signal stack per-process (`ss_sp`/`ss_size`/`ss_flags`) |
| `sigqueue` | [x] | Queued real-time signals via `rt_sigqueueinfo` |
| Signal defaults | [x] | `SIGKILL`/`SIGSEGV`/`SIGUSR1`/`SIGINT`/`SIGTSTP`/`SIGTTOU`/`SIGTTIN`/`SIGQUIT`/`SIGALRM` handled |

## 5. File Descriptor Layer

| Feature | Status | Notes |
|---------|--------|-------|
| Per-process fd table | [x] | Up to `PROCESS_MAX_FILES` entries |
| Refcounted file objects | [x] | Shared across `dup`/`fork`/`clone` with atomic refcounts |
| File offset tracking | [x] | |
| `O_NONBLOCK` | [x] | Pipes, TTY, PTY, sockets via `fcntl` or `pipe2` |
| `O_CLOEXEC` | [x] | Close-on-exec via `pipe2`, `open` flags |
| `O_APPEND` | [x] | Append mode for `write()` — seeks to end before writing |
| `FD_CLOEXEC` via `fcntl` | [x] | `F_GETFD`/`F_SETFD` implemented; `execve` closes marked FDs |
| File locking (`flock`) | [x] | Advisory locking no-op stub (validates fd, always succeeds) |

## 6. Filesystem / VFS

| Feature | Status | Notes |
|---------|--------|-------|
| VFS mount table | [x] | Up to 8 mounts |
| `vfs_lookup` path resolution | [x] | Absolute + relative (via `cwd`); follows symlinks |
| `fs_node_t` with `read`/`write`/`finddir`/`readdir` | [x] | |
| `struct vfs_dirent` (generic) | [x] | Unified format across all FS |
| **tmpfs** | [x] | In-memory; dirs + files; `readdir` |
| **overlayfs** | [x] | Copy-up; `readdir` delegates to upper/lower |
| **devfs** | [x] | `/dev/null`, `/dev/zero`, `/dev/random`, `/dev/urandom`, `/dev/console`, `/dev/tty`, `/dev/ptmx`, `/dev/pts/N`, `/dev/fb0`, `/dev/kbd` |
| **diskfs** (on-disk) | [x] | Hierarchical inodes; full POSIX ops; symlinks; hard links with `nlink` tracking |
| **persistfs** | [x] | Minimal persistence at `/persist` |
| **procfs** | [x] | `/proc/meminfo` + per-process `/proc/[pid]/status`, `/proc/[pid]/maps` |
| **FAT12/16/32** (full RW) | [x] | Unified FAT driver, auto-detection by cluster count (MS spec), 8.3 filenames, subdirs, cluster chain management, all VFS mutation ops (create/write/delete/mkdir/rmdir/rename/truncate) |
| **ext2** (full RW) | [x] | Superblock + block group descriptors, inode read/write, block/inode bitmaps, direct/indirect/doubly-indirect/triply-indirect block mapping, directory entry add/remove/split, hard links, symlinks (inline), create/write/delete/mkdir/rmdir/rename/truncate/link |
| Permissions (`uid`/`gid`/`euid`/`egid`/mode) | [x] | `chmod`, `chown` with permission checks; VFS `open()` enforces rwx bits vs process euid/egid and file uid/gid/mode |
| Hard links | [x] | `diskfs_link()` with shared data blocks and `nlink` tracking |
| Symbolic links | [x] | `symlink`, `readlink`; followed by VFS lookup | |

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
| PMM contiguous block alloc | [x] | `pmm_alloc_blocks(count)` / `pmm_free_blocks()` for multi-page DMA |
| VMM (x86 PAE paging) | [x] | Higher-half kernel, recursive page directory, PAE mode |
| Per-process address spaces | [x] | PDPT + 4 PDs per process |
| Kernel heap (`kmalloc`/`kfree`) | [x] | 8MB Buddy Allocator (power-of-2 blocks 32B–8MB, circular free lists, buddy coalescing) |
| Slab allocator | [x] | `slab_cache_t` with free-list-in-place |
| W^X for user ELFs | [x] | Text segments read-only after load |
| SMEP | [x] | Enabled in CR4 if CPU supports |
| `brk`/`sbrk` | [x] | Per-process heap break |
| `mmap`/`munmap` | [x] | Anonymous mappings, shared memory backing, file-backed (fd) mappings |
| Shared memory (`shmget`/`shmat`/`shmdt`) | [x] | System V IPC style |
| Copy-on-write (COW) fork | [x] | PTE bit 9 as CoW marker + page fault handler |
| PAE + NX bit | [x] | PAE paging with NX (bit 63) on data segments |
| Guard pages | [x] | 32KB user stack with unmapped guard page below (triggers SIGSEGV on overflow); kernel stacks in guard-paged region at `0xC8000000` |
| ASLR | [x] | TSC-seeded xorshift32 PRNG; randomizes user stack base by up to 1MB per `execve` |
| vDSO shared page | [x] | Kernel-updated `tick_count` mapped read-only at `0x007FE000` in every user process |

## 9. Drivers & Hardware

| Feature | Status | Notes |
|---------|--------|-------|
| UART serial console | [x] | |
| VGA text console (x86) | [x] | |
| PS/2 keyboard | [x] | |
| PIT timer | [x] | |
| LAPIC timer | [x] | Calibrated, used when APIC available |
| ATA PIO (IDE) | [x] | Multi-drive support: 4 drives (hda/hdb/hdc/hdd) across 2 channels |
| ATA DMA (Bus Master IDE) | [x] | Bounce buffer + zero-copy direct DMA, PRDT, IRQ-coordinated |
| PCI enumeration | [x] | Full bus/slot/func scan with BAR + IRQ |
| ACPI (MADT parsing) | [x] | CPU topology + IOAPIC discovery |
| LAPIC + IOAPIC | [x] | Replaces legacy PIC; ISA edge-triggered + PCI level-triggered routing |
| SMP (multi-CPU boot) | [x] | 4 CPUs via INIT-SIPI-SIPI, per-CPU data via GS |
| CPUID feature detection | [x] | Leaf 0/1/7/extended; SMEP/SMAP detection |
| VBE framebuffer | [x] | Maps LFB, pixel drawing, font rendering; `/dev/fb0` device with `ioctl`/`mmap` |
| Raw keyboard (`/dev/kbd`) | [x] | PS/2 scancode ring buffer; non-blocking read for game input |
| SYSENTER fast syscall | [x] | MSR setup + handler |
| E1000 NIC (Intel 82540EM) | [x] | MMIO-based, IRQ-driven, lwIP integration |
| RTC (real-time clock) | [x] | CMOS RTC driver; provides wall-clock time for `CLOCK_REALTIME` |
| MTRR write-combining | [x] | `mtrr_init`/`mtrr_set_range` for variable-range MTRR programming |
| Kernel console (kconsole) | [x] | Interactive debug shell with readline, scrollback, command history |
| Virtio-blk | [x] | PCI legacy virtio-blk driver with virtqueue I/O |

## 10. Networking

| Feature | Status | Notes |
|---------|--------|-------|
| E1000 NIC driver | [x] | Intel 82540EM, MMIO, IRQ 11 via IOAPIC (level-triggered, active-low), interrupt-driven RX |
| lwIP TCP/IP stack | [x] | NO_SYS=0 threaded mode (kernel semaphores/mutexes/mailboxes), IPv4, static IP (10.0.2.15) |
| ICMP ping test | [x] | Kernel-level ping to QEMU gateway (10.0.2.2) during boot; verified via smoke test |
| `socket` | [x] | `AF_INET`, `SOCK_STREAM` (TCP), `SOCK_DGRAM` (UDP) |
| `bind`/`listen`/`accept` | [x] | TCP server support |
| `connect`/`send`/`recv` | [x] | TCP client support |
| `sendto`/`recvfrom` | [x] | UDP support |
| DNS resolver | [x] | lwIP DNS enabled; kernel `dns_resolve()` wrapper with async callback + timeout |
| `/etc/hosts` | [x] | Kernel-level hosts file parsing and lookup |
| `getaddrinfo` | [x] | Kernel-level hostname resolution with hosts file + DNS fallback |

## 11. Threads & Synchronization

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
| Futex | [x] | `FUTEX_WAIT`/`FUTEX_WAKE` with 32-entry global waiter table |

## 12. Dynamic Linking

| Feature | Status | Notes |
|---------|--------|-------|
| `ET_DYN` ELF support | [x] | Kernel ELF loader accepts position-independent executables |
| `PT_INTERP` detection | [x] | Kernel reads interpreter path from ELF |
| Interpreter loading | [x] | `elf32_load_interp` loads ld.so at `INTERP_BASE=0x40000000` |
| ELF auxiliary vector types | [x] | `AT_PHDR`, `AT_PHENT`, `AT_PHNUM`, `AT_ENTRY`, `AT_BASE`, `AT_PAGESZ` defined |
| ELF relocation types | [x] | `R_386_RELATIVE`, `R_386_32`, `R_386_GLOB_DAT`, `R_386_JMP_SLOT` defined |
| `Elf32_Dyn`/`Elf32_Rel`/`Elf32_Sym` | [x] | Full dynamic section structures in `elf.h` |
| Userspace `ld.so` | [x] | Full relocation processing (`R_386_RELATIVE`, `R_386_32`, `R_386_GLOB_DAT`, `R_386_JMP_SLOT`, `R_386_COPY`, `R_386_PC32`) |
| Shared libraries (.so) | [x] | `dlopen`/`dlsym`/`dlclose` syscalls for runtime shared library loading |

## 13. Userland

| Feature | Status | Notes |
|---------|--------|-------|
| ELF32 loader | [x] | Secure with W^X + ASLR; supports `ET_EXEC` + `ET_DYN` + `PT_INTERP` |
| `/bin/init.elf` (smoke tests) | [x] | Comprehensive test suite (35 checks: file I/O, signals, memory, IPC, devices, procfs) |
| `/bin/echo` | [x] | argv/envp test |
| `/bin/sh` | [x] | POSIX sh-compatible shell; builtins, pipes, redirects, `$PATH` search |
| `/bin/cat` | [x] | |
| `/bin/ls` | [x] | Uses `getdents` |
| `/bin/mkdir` | [x] | |
| `/bin/rm` | [x] | |
| `/bin/doom.elf` | [x] | DOOM (doomgeneric port) running on `/dev/fb0` + `/dev/kbd` |
| `/lib/ld.so` | [~] | Stub dynamic linker (placeholder for future shared library support) |
| Minimal libc (ulibc) | [x] | `printf`, `malloc`, `string.h`, `unistd.h`, `errno.h`, `pthread.h`, `signal.h`, `stdio.h`, `stdlib.h`, `ctype.h`, `sys/mman.h`, `sys/ioctl.h`, `time.h`, `math.h`, `assert.h`, `fcntl.h`, `strings.h`, `inttypes.h`, `sys/types.h`, `sys/stat.h`, `sys/times.h`, `sys/uio.h`, `linux/futex.h` |
| `$PATH` search | [x] | Shell resolves commands via `$PATH` |

## 14. Scheduler

| Feature | Status | Notes |
|---------|--------|-------|
| O(1) bitmap scheduler | [x] | Bitmap + active/expired arrays, 32 priority levels |
| Decay-based priority | [x] | Priority decay on time slice exhaustion; boost on sleep wake |
| Per-process CPU time accounting | [x] | `utime`/`stime` fields incremented per scheduler tick |
| Per-CPU runqueues | [x] | Per-CPU load counters with atomics, least-loaded CPU query |

---

## Implementation Progress

### All 31 planned tasks completed ✅ + 35 additional features (66 total)

**High Priority (8/8):**
1. ~~`raise()` em ulibc~~ ✅
2. ~~`fsync`/`fdatasync` no-op stubs~~ ✅
3. ~~`O_APPEND` support in `write()` + `fcntl`~~ ✅
4. ~~`sigpending()` syscall~~ ✅
5. ~~`pread`/`pwrite` syscalls~~ ✅
6. ~~`access()` syscall~~ ✅
7. ~~`umask()` syscall~~ ✅
8. ~~`setuid`/`setgid` syscalls~~ ✅

**Medium Priority (13/13):**
9. ~~`truncate`/`ftruncate`~~ ✅
10. ~~`sigsuspend()`~~ ✅
11. ~~`$PATH` search in shell~~ ✅
12. ~~User-Buffered I/O (`stdio.h` em ulibc)~~ ✅
13. ~~`realpath()` em ulibc~~ ✅
14. ~~`readv`/`writev` syscalls~~ ✅
15. ~~RTC driver + `CLOCK_REALTIME`~~ ✅
16. ~~`alarm()` syscall + `SIGALRM` timer~~ ✅
17. ~~Guard pages (32KB stack + unmapped guard)~~ ✅
18. ~~PMM contiguous block alloc~~ ✅
19. ~~Hard links (`diskfs_link()` with shared storage)~~ ✅
20. ~~`times()` syscall — CPU time accounting~~ ✅
21. ~~Futex — `FUTEX_WAIT`/`FUTEX_WAKE`~~ ✅

**Low Priority (10/10):**
22. ~~`sigaltstack`~~ ✅
23. ~~File locking (`flock`)~~ ✅
24. ~~Write-Combining MTRRs~~ ✅
25. ~~Decay-based scheduler~~ ✅
26. ~~vDSO shared page~~ ✅
27. ~~DNS resolver~~ ✅
28. ~~FAT16 filesystem~~ ✅
29. ~~Zero-Copy DMA I/O~~ ✅
30. ~~Userspace `ld.so` stub~~ ✅
31. ~~ASLR~~ ✅

**Additional Features (post-31 tasks):**
32. ~~DevFS ↔ TTY/PTY decoupling (`devfs_register_device` API)~~ ✅
33. ~~VMM arch separation (`src/mm/vmm.c` generic wrappers)~~ ✅
34. ~~`/dev/fb0` framebuffer device (ioctl + mmap)~~ ✅
35. ~~`/dev/kbd` raw scancode device~~ ✅
36. ~~fd-backed `mmap` (file descriptor memory mapping)~~ ✅
37. ~~Kernel stack guard pages (0xC8000000 region)~~ ✅
38. ~~uid/gid/euid/egid with VFS permission enforcement~~ ✅
39. ~~DOOM port (doomgeneric + AdrOS adapter, 450KB doom.elf)~~ ✅
40. ~~Buddy Allocator heap (replaced doubly-linked-list heap)~~ ✅
41. ~~lwIP NO_SYS=0 threaded mode (kernel semaphores/mutexes/mailboxes)~~ ✅
42. ~~kprintf migration (all uart_print→kprintf, 16KB ring buffer, dmesg)~~ ✅
43. ~~Unified FAT12/16/32 full RW driver (replaced read-only FAT16)~~ ✅
44. ~~ext2 filesystem full RW~~ ✅
45. ~~Kernel command line parser (Linux-like: init=, root=, ring3, quiet, /proc/cmdline)~~ ✅
46. ~~Multi-drive ATA support (4 drives across 2 channels)~~ ✅
47. ~~IOAPIC level-triggered routing for PCI interrupts~~ ✅
48. ~~ICMP ping test (kernel-level, verified in smoke test)~~ ✅
49. ~~SMAP — CR4 bit 21~~ ✅
50. ~~F_GETPIPE_SZ / F_SETPIPE_SZ — pipe capacity fcntl~~ ✅
51. ~~waitid — extended wait~~ ✅
52. ~~sigqueue — queued real-time signals~~ ✅
53. ~~select/poll for regular files~~ ✅
54. ~~setitimer/getitimer — interval timers~~ ✅
55. ~~posix_spawn — efficient process creation~~ ✅
56. ~~POSIX message queues mq_*~~ ✅
57. ~~POSIX semaphores sem_*~~ ✅
58. ~~getaddrinfo / /etc/hosts~~ ✅
59. ~~DHCP client~~ ✅
60. ~~E1000 rx_thread scheduling fix~~ ✅
61. ~~Virtio-blk driver~~ ✅
62. ~~Full ld.so relocation processing~~ ✅
63. ~~IPv6 support (lwIP dual-stack)~~ ✅
64. ~~Shared libraries .so — dlopen/dlsym/dlclose~~ ✅
65. ~~Per-CPU scheduler runqueue infrastructure~~ ✅
66. ~~Multi-arch ARM64/RISC-V bring-up (QEMU virt boot)~~ ✅

---

## Remaining Work

All previously identified gaps have been implemented. Potential future enhancements:

| Area | Description |
|------|-------------|
| **Full SMP scheduling** | Move processes to AP runqueues (infrastructure in place) |
| **ARM64/RISC-V subsystems** | PMM, VMM, scheduler, syscalls for non-x86 |
| **`epoll`** | Scalable I/O event notification |
| **`inotify`** | Filesystem event monitoring |
| **`sendmsg`/`recvmsg`** | Advanced socket I/O with ancillary data |
| **Shared library lazy binding** | PLT/GOT lazy resolution in ld.so |
| **`aio_*`** | POSIX asynchronous I/O |
