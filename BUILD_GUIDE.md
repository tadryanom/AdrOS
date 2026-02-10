# AdrOS - Build & Run Guide

This guide explains how to build and run AdrOS on your local machine (Linux/WSL).

AdrOS is a Unix-like, POSIX-compatible OS kernel. See [POSIX_ROADMAP.md](docs/POSIX_ROADMAP.md) for the full compatibility checklist.

## 1. Dependencies

You will need:
- `make` and `gcc` (build system)
- `qemu-system-*` (emulators)
- `xorriso` (to create bootable ISOs)
- `grub-pc-bin` and `grub-common` (x86 bootloader)
- Cross-compilers for ARM/RISC-V (optional, for non-x86 targets)
- `cppcheck` (optional, for static analysis)
- `expect` (optional, for automated smoke tests)
- `sparse` (optional, for kernel-style static analysis)

### On Ubuntu/Debian:
```bash
sudo apt update
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo \
    qemu-system-x86 qemu-system-arm qemu-system-misc \
    grub-common grub-pc-bin xorriso mtools cppcheck expect sparse \
    gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu
```

## 2. Building & Running (x86)

This is the primary target (standard PC).

### Build
```bash
make ARCH=x86
```
This produces `adros-x86.bin`.

If you have an `i686-elf-*` cross toolchain installed, you can build using it via:
```bash
make ARCH=x86 CROSS=1
```

### Create a bootable ISO (GRUB)
For x86, the repository includes an `iso/` tree with GRUB already configured.

```bash
make ARCH=x86 iso
```

This produces `adros-x86.iso`.

### Run on QEMU
```bash
make ARCH=x86 run
```

Persistent storage note:
- The x86 QEMU run target attaches a `disk.img` file as an IDE drive (primary master).
- The kernel mounts two filesystems from this disk:
  - `/persist` — minimal persistence filesystem (e.g. `/persist/counter`)
  - `/disk` — hierarchical inode-based filesystem (diskfs) supporting `mkdir`, `unlink`, `rmdir`, `rename`, `getdents`, etc.
- If `disk.img` does not exist, it is created automatically by the Makefile.

If you are iterating on kernel changes and want to avoid hanging runs, you can wrap it with a timeout:
```bash
timeout 60s make ARCH=x86 run || true
```

Generated outputs/artifacts:
- `serial.log`: UART log (primary kernel output)
- `qemu.log`: only generated when QEMU debug logging is enabled (see below)

Syscall return convention note:
- The kernel follows a Linux-style convention: syscalls return `0`/positive values on success, and `-errno` (negative) on failure.
- Userland (`user/init.c`) uses a `__syscall_fix()` helper that converts negative returns to `-1` and sets a global `errno`.
- A full libc-style per-thread `errno` is not yet implemented.

### Userland smoke tests
The init program (`/bin/init.elf`) runs a comprehensive suite of smoke tests on boot, covering:
- File I/O (`open`, `read`, `write`, `close`, `lseek`, `stat`, `fstat`)
- Overlay copy-up, `dup2`, `pipe`, `select`, `poll`
- TTY/ioctl, job control (`SIGTTIN`/`SIGTTOU`)
- PTY (`/dev/ptmx` + `/dev/pts/0`)
- Signals (`sigaction`, `kill`, `sigreturn`)
- Session/process groups (`setsid`, `setpgid`, `getpgrp`)
- `isatty`, `O_NONBLOCK` (pipes + PTY), `fcntl`
- `pipe2`/`dup3` with flags
- `chdir`/`getcwd` with relative path resolution
- `openat`/`fstatat`/`unlinkat` (`AT_FDCWD`)
- `rename`, `rmdir`
- `getdents` across multiple FS types (diskfs, devfs, tmpfs)
- `fork` (100 children), `waitpid` (`WNOHANG`), `execve`
- `SIGSEGV` handler

All tests print `[init] ... OK` on success. Any failure calls `sys_exit(1)`.

### Testing

Run all tests (static analysis + host unit tests + QEMU smoke tests):
```bash
make test-all
```

Individual test targets:
```bash
make check        # cppcheck + sparse + gcc -fanalyzer
make test-host    # 47 host-side unit tests (test_utils + test_security)
make test         # QEMU smoke test (4 CPUs, 40s timeout, 19 checks)
make test-1cpu    # Single-CPU smoke test (50s timeout)
make test-gdb     # GDB scripted integrity checks (heap, PMM, VGA)
```

Static analysis helpers:
```bash
make ARCH=x86 cppcheck
make ARCH=x86 scan-build
make ARCH=x86 mkinitrd-asan
```

To enable QEMU debug logging (disabled by default to avoid excessive I/O):
```bash
make ARCH=x86 run QEMU_DEBUG=1
```

To also log hardware interrupts:
```bash
make ARCH=x86 run QEMU_DEBUG=1 QEMU_INT=1
```

Notes:
- `QEMU_DEBUG=1` enables QEMU logging to `qemu.log` using `-d guest_errors,cpu_reset -D qemu.log`.
- `QEMU_INT=1` appends `-d int` to QEMU debug flags.

## 3. Building & Running (ARM64)

### Build
```bash
make ARCH=arm
```
This produces `adros-arm.bin`.

### Run on QEMU
ARM does not use GRUB/ISO in the same way. The kernel is loaded directly into memory.

```bash
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic \
    -kernel adros-arm.bin
```
- To quit QEMU when using `-nographic`: press `Ctrl+A`, release, then `x`.

## 4. Building & Running (RISC-V)

### Build
```bash
make ARCH=riscv
```
This produces `adros-riscv.bin`.

### Run on QEMU
```bash
qemu-system-riscv64 -machine virt -m 128M -nographic \
    -bios default -kernel adros-riscv.bin
```
- To quit: `Ctrl+A`, then `x`.

## 5. Common Troubleshooting

- **"Multiboot header not found"**: Check whether `grub-file --is-x86-multiboot2 adros-x86.bin` returns success (0). If it fails, the section order in `linker.ld` may be wrong.
- **"Triple Fault (infinite reset)"**: Usually caused by paging (VMM) issues or a misconfigured IDT. Run `make ARCH=x86 run QEMU_DEBUG=1` and inspect `qemu.log`.
- **Black screen (VGA)**: If you are running with `-nographic`, you will not see the VGA output. Remove that flag to get a window.
