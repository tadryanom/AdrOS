# AdrOS

## Overview
AdrOS is a Unix-like, POSIX-compatible, multi-architecture operating system developed for research and academic purposes. The goal is to build a secure, monolithic kernel from scratch, eventually serving as a platform for security testing and exploit development.

## Architectures Targeted
- **x86** (32-bit, PAE) — primary, fully functional target
- **ARM** (64-bit) — build infrastructure only
- **MIPS** — build infrastructure only
- **RISC-V** (64-bit) — build infrastructure only

## Technical Stack
- **Language:** C and Assembly
- **Bootloader:** GRUB2 (Multiboot2 compliant)
- **Build System:** Make + Cross-Compilers
- **TCP/IP:** lwIP (lightweight IP stack, NO_SYS=1 mode)

## Features

### Boot & Architecture
- **Multi-arch build system** — `make ARCH=x86|arm|riscv|mips` (x86 is the primary, working target)
- **Multiboot2** (via GRUB), higher-half kernel mapping (3GB+)
- **CPUID feature detection** — leaf 0/1/7/extended; SMEP/SMAP detection
- **SYSENTER fast syscall path** — MSR setup + handler
- **PAE paging with NX bit** — hardware W^X enforcement on data segments

### Memory Management
- **PMM** — bitmap allocator with spinlock protection and frame reference counting
- **VMM** — PAE recursive page directory, per-process address spaces (PDPT + 4 PDs), TLB flush
- **Copy-on-Write (CoW) fork** — PTE bit 9 as CoW marker + page fault handler
- **Kernel heap** — doubly-linked free list with coalescing, dynamic growth up to 64MB
- **Slab allocator** — `slab_cache_t` with free-list-in-place and spinlock
- **Shared memory** — System V IPC style (`shmget`/`shmat`/`shmdt`/`shmctl`)
- **`mmap`/`munmap`** — anonymous mappings + shared memory backing
- **SMEP** — Supervisor Mode Execution Prevention enabled in CR4
- **W^X** — user `.text` segments marked read-only after ELF load; NX on data segments

### Process & Scheduling
- **O(1) scheduler** — bitmap + active/expired arrays, 32 priority levels
- **Process model** — `fork` (CoW), `execve`, `exit`, `waitpid` (`WNOHANG`), `getpid`, `getppid`
- **Threads** — `clone` syscall with `CLONE_VM`/`CLONE_FILES`/`CLONE_THREAD`/`CLONE_SETTLS`
- **TLS** — `set_thread_area` via GDT entry 22 (user GS segment, ring 3)
- **Sessions & groups** — `setsid`, `setpgid`, `getpgrp`
- **Permissions** — per-process `uid`/`gid`; `chmod`, `chown`
- **User heap** — `brk`/`sbrk` syscall
- **Time** — `nanosleep`, `clock_gettime` (`CLOCK_REALTIME`, `CLOCK_MONOTONIC`)

### Syscalls (x86, `int 0x80` + SYSENTER)
- **File I/O:** `open`, `openat`, `read`, `write`, `close`, `lseek`, `stat`, `fstat`, `fstatat`, `dup`, `dup2`, `dup3`, `pipe`, `pipe2`, `select`, `poll`, `ioctl`, `fcntl`, `getdents`
- **Directory ops:** `mkdir`, `rmdir`, `unlink`, `unlinkat`, `rename`, `chdir`, `getcwd`, `symlink`, `readlink`, `chmod`, `chown`
- **Signals:** `sigaction` (`SA_SIGINFO`), `sigprocmask`, `kill`, `sigreturn` (full trampoline)
- **FD flags:** `O_NONBLOCK`, `O_CLOEXEC`, `FD_CLOEXEC` via `fcntl` (`F_GETFD`/`F_SETFD`/`F_GETFL`/`F_SETFL`)
- **Shared memory:** `shmget`, `shmat`, `shmdt`, `shmctl`
- **Threads:** `clone`, `gettid`, `set_thread_area`
- **Networking:** `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `sendto`, `recvfrom`
- Per-process fd table with atomic refcounted file objects
- Centralized user-pointer access API (`user_range_ok`, `copy_from_user`, `copy_to_user`)
- Error returns use negative errno codes (Linux-style)

### TTY / PTY
- **Canonical + raw mode** — `ICANON` clearable via `TCSETS`
- **Signal characters** — Ctrl+C→SIGINT, Ctrl+Z→SIGTSTP, Ctrl+D→EOF, Ctrl+\\→SIGQUIT
- **Job control** — `SIGTTIN`/`SIGTTOU` enforcement for background process groups
- **PTY** — `/dev/ptmx` + `/dev/pts/N` (up to 8 dynamic pairs) with non-blocking I/O
- **Window size** — `TIOCGWINSZ`/`TIOCSWINSZ`
- **termios** — `TCGETS`, `TCSETS`, `TIOCGPGRP`, `TIOCSPGRP`, `VMIN`/`VTIME`
- **Wait queues** — generic `waitqueue_t` abstraction for blocking I/O

### Filesystems (7 types)
- **tmpfs** — in-memory filesystem
- **devfs** — `/dev/null`, `/dev/zero`, `/dev/random`, `/dev/urandom`, `/dev/console`, `/dev/tty`, `/dev/ptmx`, `/dev/pts/N`
- **overlayfs** — copy-up semantics
- **diskfs** — hierarchical inode-based on-disk filesystem at `/disk` with symlinks
- **persistfs** — minimal persistence at `/persist`
- **procfs** — `/proc/meminfo` + per-process `/proc/[pid]/status`, `/proc/[pid]/maps`
- Generic `readdir`/`getdents` across all VFS types; symlink following in path resolution

### Networking
- **E1000 NIC** — Intel 82540EM driver (MMIO, IRQ via IOAPIC)
- **lwIP TCP/IP stack** — IPv4, static IP (10.0.2.15 via QEMU user-net)
- **Socket API** — `socket`/`bind`/`listen`/`accept`/`connect`/`send`/`recv`/`sendto`/`recvfrom`
- **Protocols** — TCP (`SOCK_STREAM`) + UDP (`SOCK_DGRAM`)

### Drivers & Hardware
- **PCI** — full bus/slot/func enumeration with BAR + IRQ
- **ATA PIO + DMA** — Bus Master IDE with bounce buffer, PRDT, IRQ coordination
- **LAPIC + IOAPIC** — replaces legacy PIC; ISA IRQ routing
- **SMP** — 4 CPUs via INIT-SIPI-SIPI, per-CPU data via GS segment
- **ACPI** — MADT parsing for CPU topology and IOAPIC discovery
- **VBE framebuffer** — maps LFB, pixel drawing, font rendering
- **UART**, **VGA text**, **PS/2 keyboard**, **PIT timer**, **LAPIC timer**
- **E1000 NIC** — Intel 82540EM Ethernet controller

### Userland
- **ulibc** — `printf`, `malloc`/`free`/`calloc`/`realloc`, `string.h`, `unistd.h`, `errno.h`, `pthread.h`
- **ELF32 loader** — secure with W^X; supports `ET_EXEC` + `ET_DYN` + `PT_INTERP` (dynamic linking)
- **Shell** — `/bin/sh` (POSIX sh-compatible with builtins, pipes, redirects)
- **Core utilities** — `/bin/cat`, `/bin/ls`, `/bin/mkdir`, `/bin/rm`, `/bin/echo`
- `/bin/init.elf` — comprehensive smoke test suite

### Dynamic Linking (infrastructure)
- **Kernel-side** — `PT_INTERP` detection, interpreter loading at `0x40000000`, `ET_DYN` support
- **ELF types** — `Elf32_Dyn`, `Elf32_Rel`, `Elf32_Sym`, auxiliary vector (`AT_PHDR`, `AT_ENTRY`, `AT_BASE`)
- **Relocation types** — `R_386_RELATIVE`, `R_386_32`, `R_386_GLOB_DAT`, `R_386_JMP_SLOT`
- Userspace `ld.so` not yet implemented (kernel infrastructure ready)

### Threads
- **`clone` syscall** — `CLONE_VM`, `CLONE_FILES`, `CLONE_SIGHAND`, `CLONE_THREAD`, `CLONE_SETTLS`
- **TLS via GDT** — `set_thread_area` sets GS-based TLS segment (GDT entry 22, ring 3)
- **`gettid`** — per-thread unique ID
- **ulibc `pthread.h`** — `pthread_create`, `pthread_join`, `pthread_exit`, `pthread_self`
- Thread-group IDs (tgid) for POSIX `getpid()` semantics

### Security
- **SMEP** enabled (prevents kernel executing user-mapped pages)
- **PAE + NX bit** — hardware W^X on data segments
- **user_range_ok** hardened (rejects kernel addresses)
- **sigreturn eflags** sanitized (clears IOPL, ensures IF)
- **Atomic file refcounts** (`__sync_*` builtins)
- **PMM spinlock** for SMP safety

### Testing
- **47 host-side unit tests** — `test_utils.c` (28) + `test_security.c` (19)
- **19 QEMU smoke tests** — 4-CPU, expect-based
- **Static analysis** — cppcheck, sparse, gcc -fanalyzer
- **GDB scripted checks** — heap/PMM/VGA integrity
- `make test-all` runs everything

## Running (x86)
- `make ARCH=x86 iso`
- `make ARCH=x86 run`
- Logs:
  - `serial.log`: kernel UART output
  - `qemu.log`: QEMU debug output when enabled

QEMU debug helpers:
- `make ARCH=x86 run QEMU_DEBUG=1`
- `make ARCH=x86 run QEMU_DEBUG=1 QEMU_INT=1`

## TODO

See [POSIX_ROADMAP.md](docs/POSIX_ROADMAP.md) for a detailed checklist.

All 15 planned implementation tasks are complete. Remaining future work:

### Future enhancements
- **Userspace `ld.so`** — full dynamic linker with relocation processing
- **Shared libraries (.so)** — `dlopen`/`dlsym`/`dlclose`
- **Futex** — efficient thread synchronization primitive
- **Multi-arch bring-up** — ARM/RISC-V functional kernels
- **ext2/FAT** filesystem support
- **ASLR** — address space layout randomization
- **vDSO** — fast `clock_gettime` without syscall
- **DNS resolver** + `/etc/hosts`
- **RTC driver** — real-time clock

## Directory Structure
- `src/kernel/` — Architecture-independent kernel (VFS, syscalls, scheduler, tmpfs, diskfs, devfs, overlayfs, PTY, TTY, shm, signals, networking, threads)
- `src/arch/x86/` — x86-specific (boot, VMM, IDT, LAPIC, IOAPIC, SMP, ACPI, CPUID, SYSENTER, ELF loader)
- `src/hal/x86/` — HAL x86 (CPU, keyboard, timer, UART, PCI, ATA PIO/DMA, E1000 NIC)
- `src/drivers/` — Device drivers (VBE, initrd, VGA)
- `src/mm/` — Memory management (PMM, heap, slab)
- `include/` — Header files
- `user/` — Userland programs (`init.c`, `echo.c`, `sh.c`, `cat.c`, `ls.c`, `mkdir.c`, `rm.c`)
- `user/ulibc/` — Minimal C library (`printf`, `malloc`, `string.h`, `errno.h`, `pthread.h`)
- `tests/` — Host unit tests, smoke tests, GDB scripted checks
- `docs/` — Documentation (POSIX roadmap, audit report, supplementary analysis, testing plan)
- `third_party/lwip/` — lwIP TCP/IP stack (vendored)
