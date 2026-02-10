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
- **Multi-arch build system**
  - `make ARCH=x86|arm|riscv|mips`
  - x86 is the primary, working target
- **x86 (i386) boot & memory layout**
  - Multiboot2 (via GRUB)
  - Higher-half kernel mapping (3GB+)
  - Early paging + VMM initialization
  - W^X-oriented userspace layout (separate RX/R and RW segments)
  - Non-executable stack markers in assembly (`.note.GNU-stack`)
- **Memory management**
  - Physical Memory Manager (PMM)
  - Virtual Memory Manager (x86)
  - Kernel heap allocator (`kmalloc`/`kfree`)
- **Basic drivers & console**
  - UART serial console logging
  - VGA text console (x86)
  - Keyboard driver + input callback
  - PIT timer + periodic tick
- **Kernel services**
  - Simple scheduler / multitasking (kernel threads)
  - Minimal process lifecycle: parent/child tracking, zombies, `waitpid`
  - Basic shell with built-in commands (fallback when userspace fails)
- **InitRD + VFS + mounts**
  - InitRD image in TAR/USTAR format (with directory support)
  - InitRD-backed filesystem node tree (`fs_node_t` + `finddir`)
  - Absolute path lookup (`vfs_lookup("/bin/init.elf")`)
  - Mount table support (`vfs_mount`) + `tmpfs` and `overlayfs`
- **Syscalls (x86, `int 0x80`)**
  - **File I/O:** `open`, `openat`, `read`, `write`, `close`, `lseek`, `stat`, `fstat`, `fstatat`, `dup`, `dup2`, `dup3`, `pipe`, `pipe2`, `select`, `poll`, `ioctl`, `fcntl`
  - **Directory ops:** `mkdir`, `rmdir`, `unlink`, `unlinkat`, `rename`, `getdents`, `chdir`, `getcwd`
  - **Process:** `fork`, `execve`, `exit`, `waitpid` (incl. `WNOHANG`), `getpid`, `getppid`, `setsid`, `setpgid`, `getpgrp`
  - **Signals:** `sigaction`, `sigprocmask`, `kill`, `sigreturn` (trampoline-based return path)
  - **Modern POSIX:** `openat`/`fstatat`/`unlinkat` (`AT_FDCWD`), `dup3`, `pipe2` (with `O_NONBLOCK` flags)
  - Per-process fd table with refcounted file objects
  - Per-process current working directory (`cwd`) with relative path resolution
  - Non-blocking I/O (`O_NONBLOCK`) on pipes, TTY, and PTY via `fcntl`
  - Centralized user-pointer access API (`user_range_ok`, `copy_from_user`, `copy_to_user`)
  - Ring3 init program (`/bin/init.elf`) with comprehensive smoke tests
  - Error returns use negative errno codes (Linux-style)
- **TTY (canonical line discipline)**
  - Keyboard -> TTY input path
  - Canonical mode input (line-buffered until `\n`)
  - Echo + backspace handling
  - Blocking reads with a simple wait queue (multiple waiters)
  - `fd=0` wired to `tty_read`, `fd=1/2` wired to `tty_write`
  - Minimal termios/ioctl support (`TCGETS`, `TCSETS`, `TIOCGPGRP`, `TIOCSPGRP`)
  - Basic job control enforcement (`SIGTTIN`/`SIGTTOU` when background pgrp touches the controlling TTY)
- **Devices (devfs)**
  - `/dev` mount with `readdir` support
  - `/dev/null`, `/dev/tty`
  - `/dev/ptmx` + `/dev/pts/0` (pseudo-terminal)
- **PTY subsystem**
  - PTY master/slave pair (`/dev/ptmx` + `/dev/pts/0`)
  - Non-blocking I/O support
  - Used by userland smoke tests
- **On-disk filesystem (diskfs)**
  - ATA PIO driver (primary master IDE)
  - Hierarchical inode-based filesystem mounted at `/disk`
  - Supports: `open` (create/truncate), `read`, `write`, `stat`, `mkdir`, `unlink`, `rmdir`, `rename`, `getdents`
  - Persistence filesystem mounted at `/persist` (smoke tests)
- **Generic `readdir`/`getdents` across all VFS**
  - Works on diskfs, tmpfs, devfs, and overlayfs
  - Unified `struct vfs_dirent` format
- **W^X (Option 1) for user ELFs (x86)**
  - User segments are mapped RW during load, then write permissions are dropped for non-writable segments
  - This provides "text is read-only" hardening without requiring NX/PAE

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

- **Syscalls / POSIX gaps**
  - `brk`/`sbrk` (heap management)
  - `mmap`/`munmap` (memory-mapped I/O)
  - `access`, `chmod`, `chown`, `umask` (permissions)
  - `link`, `symlink`, `readlink` (hard/symbolic links)
  - `truncate`/`ftruncate`
  - `socket`/`bind`/`listen`/`accept`/`connect`/`send`/`recv` (networking)
  - Userspace `errno` variable + libc-style wrappers
- **Filesystem**
  - Permissions/ownership (`uid/gid`, mode bits)
  - `/proc` filesystem
  - Real on-disk FS (ext2/FAT) as alternative to diskfs
- **TTY / terminal**
  - Full termios flags (raw mode, VMIN/VTIME, signal chars)
  - Multiple PTY pairs
- **Virtual memory hardening**
  - PAE + NX enforcement (execute disable for data/stack)
  - Guard pages, ASLR
- **Multi-architecture kernel bring-up**
  - Implement VMM/interrupts/scheduler for ARM/RISC-V/MIPS
- **Userland**
  - Minimal libc (`printf`, `malloc`, `string.h`, etc.)
  - Shell (sh-compatible)
  - Core utilities (`ls`, `cat`, `cp`, `mv`, `rm`, `echo`, `mkdir`)
- **Observability & tooling**
  - Panic backtraces, symbolization
  - CI pipeline with `cppcheck`, `scan-build`

## Directory Structure
- `src/kernel/` - Architecture-independent kernel code (VFS, syscalls, scheduler, tmpfs, diskfs, devfs, overlayfs, PTY, TTY)
- `src/arch/` - Architecture-specific code (boot, context switch, interrupts, VMM)
- `src/hal/` - Hardware abstraction layer (CPU, keyboard, timer, UART, video)
- `src/drivers/` - Device drivers (ATA, initrd, keyboard, timer, UART, VGA)
- `src/mm/` - Memory management (PMM, heap)
- `include/` - Header files
- `user/` - Userland programs (`init.c`, `echo.c`)
- `docs/` - Documentation (POSIX roadmap)
