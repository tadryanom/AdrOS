# AdrOS

## Overview
AdrOS is a Unix-like, POSIX-compatible, multi-architecture operating system developed for research and academic purposes. The goal is to build a secure, monolithic kernel from scratch, eventually serving as a platform for security testing and exploit development.

## Architectures Targeted
- **x86** (32-bit, PAE) — primary, fully functional target
- **ARM64** (AArch64) — boots on QEMU virt, UART console, minimal kernel
- **RISC-V 64** — boots on QEMU virt, UART console, minimal kernel
- **MIPS32** (little-endian) — boots on QEMU Malta, UART console, minimal kernel

## Technical Stack
- **Language:** C and Assembly
- **Bootloader:** GRUB2 (Multiboot2 compliant)
- **Build System:** Make + Cross-Compilers
- **TCP/IP:** lwIP (lightweight IP stack, NO_SYS=0 threaded mode)

## Features

### Boot & Architecture
- **Multi-arch build system** — `make ARCH=x86|arm|riscv|mips`; ARM64 and RISC-V boot on QEMU virt with UART console
- **Multiboot2** (via GRUB), higher-half kernel mapping (3GB+)
- **CPUID feature detection** — leaf 0/1/7/extended; SMEP/SMAP detection
- **SYSENTER fast syscall path** — MSR setup + handler
- **PAE paging with NX bit** — hardware W^X enforcement on data segments

### Memory Management
- **PMM** — bitmap allocator with spinlock protection, frame reference counting, and contiguous block allocation
- **VMM** — PAE recursive page directory, per-process address spaces (PDPT + 4 PDs), TLB flush
- **Copy-on-Write (CoW) fork** — PTE bit 9 as CoW marker + page fault handler
- **Kernel heap** — 8MB Buddy Allocator (power-of-2 blocks 32B–8MB, circular free lists, buddy coalescing, corruption detection)
- **Slab allocator** — `slab_cache_t` with free-list-in-place and spinlock
- **Shared memory** — System V IPC style (`shmget`/`shmat`/`shmdt`/`shmctl`)
- **`mmap`/`munmap`** — anonymous mappings, shared memory backing, and file-backed (fd) mappings
- **SMEP** — Supervisor Mode Execution Prevention enabled in CR4
- **SMAP** — Supervisor Mode Access Prevention enabled in CR4 (bit 21)
- **W^X** — user `.text` segments marked read-only after ELF load; NX on data segments
- **Guard pages** — 32KB user stack with unmapped guard page below (triggers SIGSEGV on overflow); kernel stacks use dedicated guard-paged region at `0xC8000000`
- **ASLR** — TSC-seeded xorshift32 PRNG randomizes user stack base by up to 1MB per `execve`
- **vDSO** — kernel-updated shared page mapped read-only into every user process at `0x007FE000`

### Process & Scheduling
- **O(1) scheduler** — bitmap + active/expired arrays, 32 priority levels, decay-based priority adjustment
- **Per-CPU runqueue infrastructure** — per-CPU load counters with atomic operations, least-loaded CPU query
- **`posix_spawn`** — efficient fork+exec in single syscall with file actions and attributes
- **Interval timers** — `setitimer`/`getitimer` (`ITIMER_REAL`, `ITIMER_VIRTUAL`, `ITIMER_PROF`)
- **Process model** — `fork` (CoW), `execve`, `exit`, `waitpid` (`WNOHANG`), `getpid`, `getppid`
- **Threads** — `clone` syscall with `CLONE_VM`/`CLONE_FILES`/`CLONE_THREAD`/`CLONE_SETTLS`
- **TLS** — `set_thread_area` via GDT entry 22 (user GS segment, ring 3)
- **Futex** — `FUTEX_WAIT`/`FUTEX_WAKE` with global waiter table for efficient thread synchronization
- **Sessions & groups** — `setsid`, `setpgid`, `getpgrp`
- **Permissions** — per-process `uid`/`gid`/`euid`/`egid`; `chmod`, `chown`, `setuid`, `setgid`, `seteuid`, `setegid`, `access`, `umask`; VFS permission enforcement on `open()`
- **User heap** — `brk`/`sbrk` syscall
- **Time** — `nanosleep`, `clock_gettime` (`CLOCK_REALTIME` via RTC, `CLOCK_MONOTONIC` with TSC nanosecond precision), `alarm`/`SIGALRM`, `times` (CPU accounting)

### Syscalls (x86, `int 0x80` + SYSENTER)
- **File I/O:** `open`, `openat`, `read`, `write`, `close`, `lseek`, `stat`, `fstat`, `fstatat`, `dup`, `dup2`, `dup3`, `pipe`, `pipe2`, `select`, `poll`, `ioctl`, `fcntl`, `getdents`, `pread`, `pwrite`, `readv`, `writev`, `truncate`, `ftruncate`, `fsync`, `fdatasync`, `flock`
- **Directory ops:** `mkdir`, `rmdir`, `unlink`, `unlinkat`, `rename`, `chdir`, `getcwd`, `link`, `symlink`, `readlink`, `chmod`, `chown`, `access`, `umask`
- **Signals:** `sigaction` (`SA_SIGINFO`), `sigprocmask`, `kill`, `sigreturn` (full trampoline), `sigpending`, `sigsuspend`, `sigaltstack`, `sigqueue`
- **Process:** `setuid`, `setgid`, `seteuid`, `setegid`, `getuid`, `getgid`, `geteuid`, `getegid`, `alarm`, `times`, `futex`, `waitid`, `posix_spawn`, `setitimer`, `getitimer`, `pivot_root`, `gettimeofday`, `mprotect`, `getrlimit`, `setrlimit`, `uname`, `getrusage`, `mount`
- **IPC:** `mq_open`, `mq_close`, `mq_unlink`, `mq_send`, `mq_receive`, `mq_getattr`, `mq_setattr`, `sem_open`, `sem_close`, `sem_unlink`, `sem_wait`, `sem_post`, `sem_getvalue`
- **I/O multiplexing (advanced):** `epoll_create`, `epoll_ctl`, `epoll_wait`, `inotify_init`, `inotify_add_watch`, `inotify_rm_watch`
- **Async I/O:** `aio_read`, `aio_write`, `aio_error`, `aio_return`, `aio_suspend`
- **FD flags:** `O_NONBLOCK`, `O_CLOEXEC`, `O_APPEND`, `FD_CLOEXEC` via `fcntl` (`F_GETFD`/`F_SETFD`/`F_GETFL`/`F_SETFL`)
- **File locking:** `flock` (advisory locking with per-inode lock table)
- **Shared memory:** `shmget`, `shmat`, `shmdt`, `shmctl`
- **Threads:** `clone`, `gettid`, `set_thread_area`, `futex`
- **Networking:** `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `sendto`, `recvfrom`, `sendmsg`, `recvmsg`, `setsockopt`, `getsockopt`, `shutdown`, `getpeername`, `getsockname`
- Per-process fd table with atomic refcounted file objects
- Centralized user-pointer access API (`user_range_ok`, `copy_from_user`, `copy_to_user`)
- Error returns use negative errno codes (Linux-style)

### TTY / PTY
- **Canonical + raw mode** — `ICANON` clearable via `TCSETS`
- **Signal characters** — Ctrl+C→SIGINT, Ctrl+Z→SIGTSTP, Ctrl+D→EOF, Ctrl+\\→SIGQUIT
- **Job control** — `SIGTTIN`/`SIGTTOU` enforcement for background process groups
- **OPOST output processing** — `ONLCR` (`\n` → `\r\n`) on TTY and PTY slave output; `c_oflag` exposed via `TCGETS`/`TCSETS`
- **Console output routing** — userspace `write(fd 1/2)` goes through VFS → `/dev/console` → TTY line discipline → UART + VGA (industry-standard path matching Linux)
- **fd 0/1/2 init** — kernel opens `/dev/console` as fd 0, 1, 2 before exec init (mirrors Linux `kernel_init`)
- **PTY** — `/dev/ptmx` + `/dev/pts/N` (up to 8 dynamic pairs) with non-blocking I/O, per-PTY `c_oflag` and OPOST/ONLCR line discipline
- **Window size** — `TIOCGWINSZ`/`TIOCSWINSZ`
- **termios** — `TCGETS`, `TCSETS`, `TIOCGPGRP`, `TIOCSPGRP`, `VMIN`/`VTIME`, `c_oflag`
- **Wait queues** — generic `waitqueue_t` abstraction for blocking I/O

### Filesystems (10 types)
- **tmpfs** — in-memory filesystem
- **devfs** — `/dev/null`, `/dev/zero`, `/dev/random`, `/dev/urandom`, `/dev/console`, `/dev/tty`, `/dev/ptmx`, `/dev/pts/N`, `/dev/fb0` (framebuffer), `/dev/kbd` (raw scancodes)
- **overlayfs** — copy-up semantics
- **diskfs** — hierarchical inode-based on-disk filesystem at `/disk` with symlinks and hard links
- **persistfs** — minimal persistence at `/persist`
- **procfs** — `/proc/meminfo` + per-process `/proc/[pid]/status`, `/proc/[pid]/maps`
- **FAT12/16/32** — unified FAT driver with full RW support (auto-detection by cluster count per MS spec), 8.3 filenames, subdirectories, cluster chain management, all VFS mutation ops (create/write/delete/mkdir/rmdir/rename/truncate)
- **ext2** — full RW ext2 filesystem: superblock + block group descriptors, inode read/write, block bitmaps, inode bitmaps, direct/indirect/doubly-indirect/triply-indirect block mapping, directory entry add/remove/split, hard links, symlinks (inline small targets), create/write/delete/mkdir/rmdir/rename/truncate/link
- Generic `readdir`/`getdents` across all VFS types; symlink following in path resolution

### Networking
- **E1000 NIC** — Intel 82540EM driver (MMIO, IRQ 11 via IOAPIC level-triggered, interrupt-driven RX)
- **lwIP TCP/IP stack** — NO_SYS=0 threaded mode, IPv4, static IP (10.0.2.15 via QEMU user-net)
- **Socket API** — `socket`/`bind`/`listen`/`accept`/`connect`/`send`/`recv`/`sendto`/`recvfrom`
- **Protocols** — TCP (`SOCK_STREAM`) + UDP (`SOCK_DGRAM`) + ICMP
- **IPv6** — lwIP dual-stack (IPv4 + IPv6), link-local auto-configuration
- **ICMP ping** — kernel-level ping test to QEMU gateway (10.0.2.2) during boot
- **DNS resolver** — lwIP-based with async callback and timeout; kernel `dns_resolve()` wrapper
- **DHCP client** — automatic IPv4 configuration via lwIP DHCP; fallback to static IP
- **`getaddrinfo`** / `/etc/hosts` — kernel-level hostname resolution with hosts file lookup

### Drivers & Hardware
- **PCI** — full bus/slot/func enumeration with BAR + IRQ
- **ATA PIO + DMA** — Bus Master IDE with bounce buffer + zero-copy direct DMA, PRDT, IRQ coordination; multi-drive support (4 drives: hda/hdb/hdc/hdd across 2 channels)
- **LAPIC + IOAPIC** — replaces legacy PIC; ISA edge-triggered + PCI level-triggered IRQ routing
- **SMP** — 4 CPUs via INIT-SIPI-SIPI, per-CPU data via GS segment
- **ACPI** — MADT parsing for CPU topology and IOAPIC discovery
- **VBE framebuffer** — maps LFB, pixel drawing, font rendering; `/dev/fb0` device with `ioctl`/`mmap` for userspace access
- **UART**, **VGA text**, **PS/2 keyboard**, **PIT timer**, **LAPIC timer**
- **Kernel console (kconsole)** — interactive debug shell with readline, scrollback, command history
- **RTC** — CMOS real-time clock driver for wall-clock time (`CLOCK_REALTIME`)
- **E1000 NIC** — Intel 82540EM Ethernet controller (interrupt-driven RX thread)
- **Virtio-blk driver** — PCI legacy virtio-blk with virtqueue, interrupt-driven I/O
- **MTRR** — write-combining support via variable-range MTRR programming

### Boot & Kernel Infrastructure
- **Kernel command line** — Linux-like parser (`init=`, `root=`, `console=`, `ring3`, `quiet`, `noapic`, `nosmp`); unknown tokens forwarded to init as argv/envp
- **`/proc/cmdline`** — exposes raw kernel command line to userspace
- **`root=` parameter** — auto-detect and mount filesystem from specified ATA device at `/disk`
- **Kernel synchronization** — counting semaphores (`ksem_t`), mutexes (`kmutex_t`), condition variables (`kcond_t`), mailboxes (`kmbox_t`) with IRQ-safe signaling
- **TSC nanosecond clock** — `clock_gettime_ns()` calibrated during LAPIC/PIT window; `CLOCK_MONOTONIC` uses sub-microsecond TSC precision
- **IRQ chaining** — shared-IRQ support via static pool of handler nodes; `register_interrupt_handler` auto-chains, `unregister_interrupt_handler` removes
- **FPU/SSE context** — per-process FXSAVE/FXRSTOR save/restore across context switches; 16-byte aligned heap
- **Rump Kernel scaffold** — `rumpuser` hypercall layer (init, malloc/free, console, clock, random); Phase 1+3 complete

### Userland
- **ulibc** — `printf`, `malloc`/`free`/`calloc`/`realloc`, `string.h`, `unistd.h`, `errno.h`, `pthread.h`, `signal.h`, `stdio.h` (buffered I/O with line-buffered stdout, unbuffered stderr, `setvbuf`/`setbuf`, `isatty`), `stdlib.h` (`atof`, `strtol`), `ctype.h`, `sys/mman.h` (`mmap`/`munmap`), `sys/ioctl.h`, `sys/times.h`, `sys/uio.h`, `sys/types.h`, `sys/stat.h`, `time.h` (`nanosleep`/`clock_gettime`), `math.h`, `assert.h`, `fcntl.h`, `strings.h`, `inttypes.h`, `linux/futex.h`, `realpath()`
- **ELF32 loader** — secure with W^X + ASLR; supports `ET_EXEC` + `ET_DYN` + `PT_INTERP` (dynamic linking)
- **Shell** — `/bin/sh` (POSIX sh-compatible with builtins, pipes, redirects, `$PATH` search)
- **52 userland programs** — `/bin/cat`, `/bin/ls`, `/bin/mkdir`, `/bin/rm`, `/bin/echo`, `/bin/cp`, `/bin/mv`, `/bin/touch`, `/bin/ln`, `/bin/head`, `/bin/tail`, `/bin/wc`, `/bin/sort`, `/bin/uniq`, `/bin/cut`, `/bin/grep`, `/bin/sed`, `/bin/awk`, `/bin/find`, `/bin/which`, `/bin/chmod`, `/bin/chown`, `/bin/chgrp`, `/bin/mount`, `/bin/umount`, `/bin/ps`, `/bin/top`, `/bin/kill`, `/bin/df`, `/bin/du`, `/bin/free`, `/bin/date`, `/bin/hostname`, `/bin/uptime`, `/bin/uname`, `/bin/env`, `/bin/printenv`, `/bin/id`, `/bin/tee`, `/bin/dd`, `/bin/tr`, `/bin/basename`, `/bin/dirname`, `/bin/pwd`, `/bin/stat`, `/bin/sleep`, `/bin/clear`, `/bin/rmdir`, `/bin/dmesg`, `/bin/who`, `/bin/pie_test`
- `/sbin/init` — SysV-like init process (inittab, runlevels, respawn)
- `/sbin/fulltest` — comprehensive smoke test suite (102 checks)
- `/bin/doom.elf` — DOOM (doomgeneric port) — runs on `/dev/fb0` + `/dev/kbd`
- `/lib/ld.so` — dynamic linker with auxv parsing, PLT/GOT lazy relocation

### Dynamic Linking
- **Full `ld.so`** — kernel-side relocation processing for `R_386_RELATIVE`, `R_386_32`, `R_386_GLOB_DAT`, `R_386_JMP_SLOT`, `R_386_COPY`, `R_386_PC32`
- **Shared libraries (.so)** — `dlopen`/`dlsym`/`dlclose` syscalls for runtime shared library loading
- **ELF types** — `Elf32_Dyn`, `Elf32_Rel`, `Elf32_Sym`, auxiliary vector (`AT_PHDR`, `AT_ENTRY`, `AT_BASE`)
- **`PT_INTERP`** — interpreter loading at `0x40000000`, `ET_DYN` support

### Threads & Synchronization
- **`clone` syscall** — `CLONE_VM`, `CLONE_FILES`, `CLONE_SIGHAND`, `CLONE_THREAD`, `CLONE_SETTLS`
- **TLS via GDT** — `set_thread_area` sets GS-based TLS segment (GDT entry 22, ring 3)
- **`gettid`** — per-thread unique ID
- **ulibc `pthread.h`** — `pthread_create`, `pthread_join`, `pthread_exit`, `pthread_self`
- **Futex** — `FUTEX_WAIT`/`FUTEX_WAKE` with 32-entry global waiter table
- Thread-group IDs (tgid) for POSIX `getpid()` semantics

### Security
- **SMEP** enabled (prevents kernel executing user-mapped pages)
- **PAE + NX bit** — hardware W^X on data segments
- **ASLR** — stack base randomized per-process via TSC-seeded xorshift32 PRNG
- **Guard pages** — unmapped page below user stack catches overflows
- **user_range_ok** hardened (rejects kernel addresses)
- **sigreturn eflags** sanitized (clears IOPL, ensures IF)
- **Atomic file refcounts** (`__sync_*` builtins)
- **PMM spinlock** for SMP safety

### Testing
- **115 host-side tests** — `test_utils.c` (28) + `test_security.c` (19) + `test_host_utils.sh` (68 cross-compiled utility tests)
- **102 QEMU smoke tests** — 4-CPU expect-based (file I/O, signals, memory mgmt, IPC, devices, procfs, networking, epoll, epollet, inotify, aio, nanosleep, CoW fork, readv/writev, fsync, flock, posix_spawn, TSC precision, gettimeofday, mprotect, getrlimit/setrlimit, uname, LZ4, lazy PLT, execve)
- **16-check test battery** — multi-disk ATA (hda+hdb+hdd), VFS mount, ping, diskfs ops (`make test-battery`)
- **Static analysis** — cppcheck, sparse, gcc -fanalyzer
- **GDB scripted checks** — heap/PMM/VGA integrity
- `make test-all` runs everything

## Running

### x86 (primary)
```
make ARCH=x86 iso
make ARCH=x86 run
```

### ARM64 (QEMU virt)
```
make ARCH=arm
make run-arm
```

### RISC-V 64 (QEMU virt)
```
make ARCH=riscv
make run-riscv
```

### MIPS32 (QEMU Malta)
```
make ARCH=mips
make run-mips
```

QEMU debug helpers:
- `make ARCH=x86 run QEMU_DEBUG=1`
- `make ARCH=x86 run QEMU_DEBUG=1 QEMU_INT=1`

## Status

See [POSIX_ROADMAP.md](docs/POSIX_ROADMAP.md) for a detailed checklist.

**All 31 planned POSIX tasks are complete**, plus 59 additional features (90 total). The kernel covers **~98%** of the core POSIX interfaces needed for a practical Unix-like system. All 102 smoke tests, 16 battery checks, and 115 host tests pass clean. ARM64, RISC-V 64, and MIPS32 boot on QEMU.

Rump Kernel integration is in progress — prerequisites (condition variables, TSC nanosecond clock, IRQ chaining) are implemented and the `rumpuser` hypercall scaffold is in place.

## Directory Structure
- `src/kernel/` — Architecture-independent kernel (VFS, syscalls, scheduler, tmpfs, diskfs, devfs, overlayfs, procfs, FAT12/16/32, ext2, PTY, TTY, shm, signals, networking, threads, vDSO, KASLR, permissions)
- `src/arch/x86/` — x86-specific (boot, VMM, IDT, LAPIC, IOAPIC, SMP, ACPI, CPUID, SYSENTER, ELF loader, MTRR)
- `src/arch/arm/` — ARM64-specific (boot, EL2→EL1, PL011 UART, stubs)
- `src/arch/riscv/` — RISC-V 64-specific (boot, NS16550 UART, stubs)
- `src/arch/mips/` — MIPS32-specific (boot, 16550 UART, stubs)
- `src/hal/x86/` — HAL x86 (CPU, keyboard, timer, UART, PCI, ATA PIO/DMA, E1000 NIC, RTC)
- `src/drivers/` — Device drivers (VBE, initrd, VGA, timer)
- `src/mm/` — Memory management (PMM, heap, slab, arch-independent VMM wrappers)
- `src/net/` — Networking (lwIP port, E1000 netif, DNS resolver, ICMP ping test)
- `src/rump/` — Rump Kernel hypercall scaffold (`rumpuser_adros.c`)
- `include/` — Header files
- `user/` — Userland programs (52 commands: `init.c`, `sh.c`, `cat.c`, `ls.c`, `echo.c`, `cp.c`, `mv.c`, `grep.c`, `sed.c`, `awk.c`, `find.c`, `which.c`, `ps.c`, `top.c`, `kill.c`, `mount.c`, etc. + `ldso.c`, `fulltest.c`, `pie_main.c`)
- `user/doom/` — DOOM port (doomgeneric engine + AdrOS platform adapter)
- `user/ulibc/` — Minimal C library (`printf`, `malloc`, `string.h`, `errno.h`, `pthread.h`, `signal.h`, `stdio.h`, `stdlib.h`, `ctype.h`, `math.h`, `sys/mman.h`, `sys/ioctl.h`, `sys/uio.h`, `time.h`, `linux/futex.h`)
- `tests/` — Host unit tests, smoke tests, GDB scripted checks
- `tools/` — Build tools (`mkinitrd`)
- `docs/` — Documentation (POSIX roadmap, audit report, supplementary analysis, testing plan)
- `third_party/lwip/` — lwIP TCP/IP stack (vendored)
