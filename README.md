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
  - W^X-oriented linker layout (separate RX/R/RW segments)
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
  - Basic shell with built-in commands
- **InitRD + VFS glue**
  - InitRD-backed filesystem node tree
  - Minimal VFS helpers (`vfs_read`/`vfs_write`/open/close)
- **Syscalls & ring3 bring-up (x86)**
  - `int 0x80` syscall gate
  - `SYSCALL_WRITE`, `SYSCALL_EXIT`, `SYSCALL_GETPID`
  - Centralized user-pointer access API (`user_range_ok`, `copy_from_user`, `copy_to_user`)
  - Ring3 stub test program with fault-injection for invalid pointers

## TODO
- **Multi-architecture kernel bring-up**
  - Implement VMM/interrupts/scheduler for ARM/RISC-V/MIPS
  - Standardize arch entrypoint behavior (`arch_start`) across architectures
- **Userspace**
  - Real userspace loader (e.g., ELF)
  - Process address spaces + page fault handling
  - Safer syscall ABI expansion
- **Virtual memory hardening**
  - Reduce early identity mapping further (keep only what is required)
  - Guard pages, user/kernel separation checks beyond current page-walk
- **Filesystem**
  - Persisted storage (ATA/AHCI/virtio-blk or similar)
  - Path resolution, directories, permissions
- **Observability & tooling**
  - Better memory stats (`mem` shell command)
  - Debug facilities (panic backtraces, symbolization, structured logs)

## Directory Structure
- `src/kernel/` - Architecture-independent kernel code
- `src/arch/` - Architecture-specific code (boot, context switch, interrupts)
- `src/drivers/` - Device drivers
- `include/` - Header files
