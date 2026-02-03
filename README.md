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

## Directory Structure
- `src/kernel/` - Architecture-independent kernel code
- `src/arch/` - Architecture-specific code (boot, context switch, interrupts)
- `src/drivers/` - Device drivers
- `include/` - Header files
