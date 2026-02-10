# AdrOS

## Overview
AdrOS is a Unix-like, POSIX-compatible, multi-architecture operating system developed for research and academic purposes. The goal is to build a secure, monolithic kernel from scratch, eventually serving as a platform for security testing and exploit development.

## Architectures Targeted
- **x86** (32-bit & 64-bit)
- **ARM** (32-bit & 64-bit)
- **MIPS**
- **RISC-V** (32-bit & 64-bit)

## Technical Stack
- **Language:** C/C++ and Assembly
- **Bootloader:** GRUB2 (Multiboot2 compliant)
- **Build System:** Make + Cross-Compilers

## Features

### Boot & Architecture
- **Multi-arch build system** — `make ARCH=x86|arm|riscv|mips` (x86 is the primary, working target)
- **Multiboot2** (via GRUB), higher-half kernel mapping (3GB+)
- **CPUID feature detection** — leaf 0/1/7/extended; SMEP/SMAP detection
- **SYSENTER fast syscall path** — MSR setup + handler

### Memory Management
- **PMM** — bitmap allocator with spinlock protection and frame reference counting
- **VMM** — recursive page directory, per-process address spaces, TLB flush
- **Copy-on-Write (CoW) fork** — PTE bit 9 as CoW marker + page fault handler
- **Kernel heap** — doubly-linked free list with coalescing, dynamic growth up to 64MB
- **Slab allocator** — `slab_cache_t` with free-list-in-place and spinlock
- **Shared memory** — System V IPC style (`shmget`/`shmat`/`shmdt`/`shmctl`)
- **SMEP** — Supervisor Mode Execution Prevention enabled in CR4
- **W^X** — user `.text` segments marked read-only after ELF load

### Process & Scheduling
- **O(1) scheduler** — bitmap + active/expired arrays, 32 priority levels
- **Process model** — `fork` (CoW), `execve`, `exit`, `waitpid` (`WNOHANG`), `getpid`, `getppid`
- **Sessions & groups** — `setsid`, `setpgid`, `getpgrp`
- **User heap** — `brk`/`sbrk` syscall
- **Time** — `nanosleep`, `clock_gettime` (`CLOCK_REALTIME`, `CLOCK_MONOTONIC`)

### Syscalls (x86, `int 0x80` + SYSENTER)
- **File I/O:** `open`, `openat`, `read`, `write`, `close`, `lseek`, `stat`, `fstat`, `fstatat`, `dup`, `dup2`, `dup3`, `pipe`, `pipe2`, `select`, `poll`, `ioctl`, `fcntl`, `getdents`
- **Directory ops:** `mkdir`, `rmdir`, `unlink`, `unlinkat`, `rename`, `chdir`, `getcwd`
- **Signals:** `sigaction`, `sigprocmask`, `kill`, `sigreturn` (full trampoline with `SA_SIGINFO`)
- **FD flags:** `O_NONBLOCK`, `O_CLOEXEC`, `FD_CLOEXEC` via `fcntl` (`F_GETFD`/`F_SETFD`/`F_GETFL`/`F_SETFL`)
- **Shared memory:** `shmget`, `shmat`, `shmdt`, `shmctl`
- Per-process fd table with atomic refcounted file objects
- Centralized user-pointer access API (`user_range_ok`, `copy_from_user`, `copy_to_user`)
- Error returns use negative errno codes (Linux-style)

### TTY / PTY
- **Canonical + raw mode** — `ICANON` clearable via `TCSETS`
- **Signal characters** — Ctrl+C→SIGINT, Ctrl+Z→SIGTSTP, Ctrl+D→EOF, Ctrl+\\→SIGQUIT
- **Job control** — `SIGTTIN`/`SIGTTOU` enforcement for background process groups
- **PTY** — `/dev/ptmx` + `/dev/pts/0` with non-blocking I/O
- **Window size** — `TIOCGWINSZ`/`TIOCSWINSZ`
- **termios** — `TCGETS`, `TCSETS`, `TIOCGPGRP`, `TIOCSPGRP`

### Filesystems (6 types)
- **tmpfs** — in-memory filesystem
- **devfs** — `/dev/null`, `/dev/tty`, `/dev/ptmx`, `/dev/pts/0`
- **overlayfs** — copy-up semantics
- **diskfs** — hierarchical inode-based on-disk filesystem at `/disk`
- **persistfs** — minimal persistence at `/persist`
- **procfs** — `/proc/meminfo`
- Generic `readdir`/`getdents` across all VFS types

### Drivers & Hardware
- **PCI** — full bus/slot/func enumeration with BAR + IRQ
- **ATA PIO + DMA** — Bus Master IDE with bounce buffer, PRDT, IRQ coordination
- **LAPIC + IOAPIC** — replaces legacy PIC; ISA IRQ routing
- **SMP** — 4 CPUs via INIT-SIPI-SIPI, per-CPU data via GS segment
- **ACPI** — MADT parsing for CPU topology and IOAPIC discovery
- **VBE framebuffer** — maps LFB, pixel drawing, font rendering
- **UART**, **VGA text**, **PS/2 keyboard**, **PIT timer**, **LAPIC timer**

### Userland
- **ulibc** — `printf`, `malloc`/`free`/`calloc`/`realloc`, `string.h`, `unistd.h`, `errno.h`
- **ELF32 loader** — secure with W^X enforcement, rejects kernel-range vaddrs
- `/bin/init.elf` — comprehensive smoke test suite
- `/bin/echo.elf` — argv/envp test

### Security
- **SMEP** enabled (prevents kernel executing user-mapped pages)
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

### Near-term (unlock interactive use)
- **Shell** (`sh`-compatible) — all required syscalls are implemented
- **Core utilities** — `ls`, `cat`, `echo`, `mkdir`, `rm`
- **`/dev/zero`**, **`/dev/random`** — simple device nodes
- **Multiple PTY pairs** — currently only 1

### Medium-term (POSIX compliance)
- **`mmap`/`munmap`** — memory-mapped files
- **Permissions** — `uid`/`gid`/mode, `chmod`, `chown`, `access`, `umask`
- **`/proc` per-process** — `/proc/[pid]/status`, `/proc/[pid]/maps`
- **Hard/symbolic links** — `link`, `symlink`, `readlink`
- **VMIN/VTIME** — termios non-canonical timing

### Long-term (full Unix experience)
- **Networking** — socket API, TCP/IP stack
- **Threads** — `clone`/`pthread`
- **PAE + NX bit** — hardware W^X
- **Dynamic linking** — `ld.so`
- **ext2/FAT** filesystem support

## Directory Structure
- `src/kernel/` — Architecture-independent kernel (VFS, syscalls, scheduler, tmpfs, diskfs, devfs, overlayfs, PTY, TTY, shm, signals)
- `src/arch/x86/` — x86-specific (boot, VMM, IDT, LAPIC, IOAPIC, SMP, ACPI, CPUID, SYSENTER)
- `src/hal/x86/` — HAL x86 (CPU, keyboard, timer, UART, PCI, ATA PIO/DMA)
- `src/drivers/` — Device drivers (VBE, initrd, VGA)
- `src/mm/` — Memory management (PMM, heap, slab)
- `include/` — Header files
- `user/` — Userland programs (`init.c`, `echo.c`)
- `user/ulibc/` — Minimal C library (`printf`, `malloc`, `string.h`, `errno.h`)
- `tests/` — Host unit tests, smoke tests, GDB scripted checks
- `docs/` — Documentation (POSIX roadmap, audit report, supplementary analysis, testing plan)
