# AdrOS

## Overview
AdrOS is a multi-architecture operating system developed for research and academic purposes. The goal is to build a secure, monolithic kernel from scratch, eventually serving as a platform for security testing and exploit development.

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
- **File descriptors + syscalls (x86)**
  - `int 0x80` syscall gate
  - `SYSCALL_WRITE`, `SYSCALL_EXIT`, `SYSCALL_GETPID`, `SYSCALL_GETPPID`, `SYSCALL_OPEN`, `SYSCALL_READ`, `SYSCALL_CLOSE`
  - `SYSCALL_LSEEK`, `SYSCALL_STAT`, `SYSCALL_FSTAT`
  - `SYSCALL_DUP`, `SYSCALL_DUP2`, `SYSCALL_PIPE`
  - `SYSCALL_FORK`, `SYSCALL_EXECVE` (with minimal argv/envp stack setup)
  - Per-process fd table (starting at fd=3)
  - Centralized user-pointer access API (`user_range_ok`, `copy_from_user`, `copy_to_user`)
  - Ring3 init program (`/bin/init.elf`) exercising IO + process + exec smoke tests
  - Error returns use negative errno codes (Linux-style)
- **TTY (canonical line discipline)**
  - Keyboard -> TTY input path
  - Canonical mode input (line-buffered until `\n`)
  - Echo + backspace handling
  - Blocking reads with a simple wait queue (multiple waiters)
  - `fd=0` wired to `tty_read`, `fd=1/2` wired to `tty_write`
- **Devices (devfs)**
  - `/dev` mount
  - `/dev/null`
  - `/dev/tty`
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
- **Multi-architecture kernel bring-up**
  - Implement VMM/interrupts/scheduler for ARM/RISC-V/MIPS
  - Standardize arch entrypoint behavior (`arch_early_setup`) across architectures
- **Userspace / POSIX process model**
  - `brk`/`sbrk`
  - Signals (at least `SIGKILL`/`SIGSEGV` basics)
- **Syscalls / ABI**
  - `ioctl` (TTY), `getcwd`, `chdir`
  - Userspace `errno` variable + libc-style wrappers (`-1` return + `errno` set)
- **Virtual memory hardening**
  - Option 2: PAE + NX enforcement (execute disable for data/stack)
  - Guard pages, and tighter user/kernel separation checks
- **Filesystem**
  - Real persisted storage (ATA/AHCI/virtio-blk or similar)
  - Persisted storage (ATA/AHCI/virtio-blk or similar)
  - Permissions/ownership (`uid/gid`, mode bits) and `umask`
  - Special files: block devices, `/proc`
  - Real on-disk fs (ext2/fat)
- **TTY / PTY**
  - Termios-like mode flags (canonical/raw, echo, erase, intr)
  - Sessions / process groups / controlling terminal
  - PTYs for userland shells
- **Observability & tooling**
  - Better memory stats (`mem` shell command)
  - Debug facilities (panic backtraces, symbolization, structured logs)
  - CI-ish targets: `cppcheck`, `scan-build`, `mkinitrd-asan`

## Directory Structure
- `src/kernel/` - Architecture-independent kernel code
- `src/arch/` - Architecture-specific code (boot, context switch, interrupts)
- `src/drivers/` - Device drivers
- `include/` - Header files
