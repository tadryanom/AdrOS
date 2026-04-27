# AdrOS - Build & Run Guide

This guide explains how to build and run AdrOS on your local machine (Linux/WSL).

AdrOS is a Unix-like, POSIX-compatible, multi-architecture OS kernel with threads, futex synchronization, networking (TCP/IP + DNS + ICMP + IPv6 + DHCP via lwIP), dynamic linking (`dlopen`/`dlsym`), FAT12/16/32 + ext2 filesystems, POSIX IPC (message queues, semaphores, shared memory), ASLR, SMAP/SMEP, vDSO, zero-copy DMA, virtio-blk, multi-drive ATA, interval timers, `posix_spawn`, epoll, inotify, aio_*, sendmsg/recvmsg, pivot_root, a POSIX shell, framebuffer graphics, per-CPU runqueue infrastructure, and ARM64/RISC-V/MIPS bring-up. See [POSIX_ROADMAP.md](docs/POSIX_ROADMAP.md) for the full compatibility checklist.

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

## 2. Getting the Source

```bash
git clone --recursive https://github.com/tadryanom/AdrOS.git
cd AdrOS
```

If you already cloned without `--recursive`, initialize the submodules:
```bash
git submodule update --init --recursive
```

The build system will auto-initialize submodules and apply required patches
if they haven't been set up yet.

## 3. Building & Running (x86)

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
- The x86 QEMU run target attaches a `disk.img` file as an IDE drive (primary master) and enables an E1000 NIC.
- The kernel mounts two filesystems from this disk:
  - `/persist` — minimal persistence filesystem (e.g. `/persist/counter`)
  - `/disk` — hierarchical inode-based filesystem (diskfs) supporting `mkdir`, `unlink`, `rmdir`, `rename`, `getdents`, etc.
- If `disk.img` does not exist, it is created automatically by the Makefile.
- The `root=` kernel parameter can override which device is mounted at `/disk` (e.g. `root=/dev/hdb`).

Multi-disk testing:
- QEMU supports up to 3 IDE hard drives alongside the CD-ROM boot device:
  - **hda** (index 0) — Primary Master
  - **hdb** (index 1) — Primary Slave
  - **hdd** (index 3) — Secondary Slave (hdc = CD-ROM)
- The kernel auto-detects all attached ATA drives and logs `[ATA] /dev/hdX detected`.

Kernel command line parameters:
- `root=/dev/hdX` — mount specified device at `/disk` (auto-detects diskfs/FAT/ext2)
- `init=/path/to/binary` — override init binary (default: `/bin/init.elf`)
- `ring3` — enable userspace (ring 3) execution
- `quiet` — suppress non-critical boot messages
- `noapic` / `nosmp` — disable APIC / SMP

If you are iterating on kernel changes and want to avoid hanging runs, you can wrap it with a timeout:
```bash
timeout 60s make ARCH=x86 run || true
```

Generated outputs/artifacts:
- `serial.log`: UART log (primary kernel output)
- `qemu.log`: only generated when QEMU debug logging is enabled (see below)

Syscall return convention note:
- The kernel follows a Linux-style convention: syscalls return `0`/positive values on success, and `-errno` (negative) on failure.
- Userland ulibc uses a `__syscall_ret()` helper that converts negative returns to `-1` and sets a per-thread `errno`.
- Per-thread `errno` is supported via `set_thread_area` + TLS.

### Userland programs
The following ELF binaries are bundled in the initrd:
- `/sbin/fulltest` — comprehensive smoke test suite (120 checks)
- `/sbin/init` — SysV-like init process (inittab, runlevels, respawn)
- `/bin/sh` — POSIX sh-compatible shell with `$PATH` search, pipes, redirects, builtins
- `/bin/echo`, `/bin/cat`, `/bin/ls`, `/bin/mkdir`, `/bin/rm` — core utilities
- `/bin/cp`, `/bin/mv`, `/bin/touch`, `/bin/ln` — file operations
- `/bin/head`, `/bin/tail`, `/bin/wc`, `/bin/sort`, `/bin/uniq`, `/bin/cut` — text processing
- `/bin/grep`, `/bin/sed`, `/bin/awk`, `/bin/tr` — pattern matching and text transformation
- `/bin/find`, `/bin/which` — file search and command lookup
- `/bin/chmod`, `/bin/chown`, `/bin/chgrp` — permission management
- `/bin/mount`, `/bin/umount` — filesystem mount/unmount
- `/bin/ps`, `/bin/top`, `/bin/kill` — process management
- `/bin/df`, `/bin/du`, `/bin/free` — disk and memory usage
- `/bin/date`, `/bin/hostname`, `/bin/uptime`, `/bin/uname` — system information
- `/bin/env`, `/bin/printenv`, `/bin/id` — environment and identity
- `/bin/tee`, `/bin/dd`, `/bin/pwd`, `/bin/stat` — I/O and file info
- `/bin/basename`, `/bin/dirname`, `/bin/sleep`, `/bin/clear`, `/bin/rmdir`, `/bin/dmesg`, `/bin/who` — misc utilities
- `/bin/pie_test` — PIE/shared library test binary
- `/bin/doom.elf` — DOOM (doomgeneric port) — included in initrd if built (see below)
- `/lib/ld.so` — dynamic linker with auxv parsing, PLT/GOT lazy relocation
- `/lib/libc.so` — shared C library (ulibc)
- `/lib/libpietest.so` — test shared library

The ulibc provides: `printf`, `malloc`/`free`/`calloc`/`realloc`, `string.h`, `unistd.h`, `errno.h`, `pthread.h`, `signal.h` (with `raise`, `sigaltstack`, `sigpending`, `sigsuspend`), `stdio.h` (buffered I/O with `fopen`/`fread`/`fwrite`/`fclose`), `stdlib.h` (`atof`, `strtol`, `getenv` stub, `system` stub), `ctype.h`, `sys/mman.h` (`mmap`/`munmap`), `sys/ioctl.h` (`ioctl`), `time.h` (`nanosleep`/`clock_gettime`), `sys/times.h`, `sys/uio.h`, `sys/types.h`, `sys/stat.h`, `math.h` (`fabs`), `assert.h`, `fcntl.h`, `strings.h`, `inttypes.h`, `linux/futex.h`, and `realpath()`.

### Smoke tests
The fulltest binary (`/sbin/fulltest`) runs a comprehensive suite of 120 smoke tests on boot, covering:
- File I/O (`open`, `read`, `write`, `close`, `lseek`, `stat`, `fstat`)
- Overlay copy-up, `dup2`, `pipe`, `select`, `poll`
- TTY/ioctl, job control (`SIGTTIN`/`SIGTTOU`)
- PTY (`/dev/ptmx` + `/dev/pts/N`)
- Signals (`sigaction`, `kill`, `sigreturn`, `SA_SIGINFO`)
- Session/process groups (`setsid`, `setpgid`, `getpgrp`)
- `isatty`, `O_NONBLOCK` (pipes + PTY), `fcntl`
- `pipe2`/`dup3` with flags
- `chdir`/`getcwd` with relative path resolution
- `openat`/`fstatat`/`unlinkat` (`AT_FDCWD`)
- `rename`, `rmdir`
- `getdents` across multiple FS types (diskfs, devfs, tmpfs)
- `fork` (100 children), `waitpid` (`WNOHANG`), `execve`
- `SIGSEGV` handler
- diskfs mkdir/unlink/getdents
- Persistent counter (`/persist/counter`)
- `/dev/tty` write test
- Memory: `brk`, `mmap`/`munmap`, `clock_gettime`, shared memory (`shmget`/`shmat`/`shmdt`)
- Advanced: `pread`/`pwrite`, `ftruncate`, `symlink`/`readlink`, `access`, `sigprocmask`/`sigpending`, `alarm`/`SIGALRM`, `O_APPEND`, `umask`, pipe capacity (`F_GETPIPE_SZ`/`F_SETPIPE_SZ`), `waitid`, `setitimer`/`getitimer`, `select`/`poll` on regular files, hard links
- Advanced I/O: `epoll` (create/ctl/wait on pipe), `epollet` (edge-triggered), `inotify` (init/add_watch/rm_watch), `aio_*` (read/write/error/return)
- System: `gettimeofday`, `mprotect`, `getrlimit`/`setrlimit`, `uname`, `mount`/`umount2`
- Dynamic linking: lazy PLT resolution, PLT caching, `dlopen`/`dlsym`/`dlclose`
- LZ4 initrd decompression
- Threads: `clone` (thread creation), `futex` (FUTEX_WAIT/WAKE)
- Signals: `sigaltstack`, `sigqueue`, `sigsuspend`
- IPC: POSIX message queues (`mq_*`), named semaphores (`sem_*`)
- Network: socket API (`socket`/`bind`/`listen`/`getsockname`/`shutdown`)
- Credentials: `chown`, `geteuid`/`getegid`, `seteuid`/`setegid`
- Advanced: `pivot_root`, `execveat`, `CLOCK_MONOTONIC`

All tests print `[test] ... OK` on success. Any failure calls `sys_exit(1)`.

### Testing

Run all tests (static analysis + host unit tests + QEMU smoke tests):
```bash
make test-all
```

Individual test targets:
```bash
make check        # cppcheck + sparse + gcc -fanalyzer
make test-host    # 69 host-side tests (test_utils + test_security + test_host_utils.sh)
make test         # QEMU smoke test (4 CPUs, 120s timeout, 120 checks incl. ICMP ping, epoll, epollet, inotify, aio, LZ4, lazy PLT, clone, pivot_root, dlopen/dlsym/dlclose, execveat, futex, sigaltstack, socket API, mqueue, semaphores, chown, mount/umount2)
make test-1cpu    # Single-CPU smoke test (50s timeout)
make test-battery # Full test battery: multi-disk ATA, VFS mount, ping, diskfs, clone, socket API, mqueue, semaphores, futex, sigaltstack, chown, mount/umount2 (33 checks)
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

## 4. Building DOOM

The DOOM port uses the [doomgeneric](https://github.com/ozkl/doomgeneric) engine (included as a git submodule) with an AdrOS-specific platform adapter.

### Setup
```bash
# Submodules should already be initialized (see section 2).
# If not: git submodule update --init --recursive
make doom
```

This produces `user/doom/doom.elf` (~450KB). The main `Makefile` will **automatically include** `doom.elf` in the initrd if it exists.

### Running DOOM
```bash
make iso
qemu-system-i386 -cdrom adros-x86.iso -m 128M -vga std \
    -drive file=disk.img,if=ide,format=raw
```

From the AdrOS shell:
```
/bin/doom.elf -iwad /path/to/doom1.wad
```

DOOM requires:
- A VBE framebuffer (`-vga std` in QEMU)
- `doom1.wad` (shareware) accessible from the filesystem
- Kernel support: `/dev/fb0` (mmap framebuffer), `/dev/kbd` (raw PS/2 scancodes), `nanosleep`, `clock_gettime`

## 5. Building & Running (ARM64)

### Build
```bash
make ARCH=arm
```
This produces `adros-arm.bin`.

### Run on QEMU
ARM does not use GRUB/ISO. The kernel is loaded directly into memory.

```bash
make run-arm
# or manually:
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic \
    -kernel adros-arm.bin -serial mon:stdio
```

Expected output:
```
[AdrOS/arm64] Booting on QEMU virt...
[CPU] No arch-specific feature detection.
Welcome to AdrOS (x86/ARM/RISC-V/MIPS)!
```

- To quit QEMU when using `-nographic`: press `Ctrl+A`, release, then `x`.
- ARM64 boots with PL011 UART at 0x09000000, EL2→EL1 transition, FP/SIMD enabled.

## 6. Building & Running (RISC-V 64)

### Build
```bash
make ARCH=riscv
```
This produces `adros-riscv.bin`.

### Run on QEMU
```bash
make run-riscv
# or manually:
qemu-system-riscv64 -M virt -m 128M -nographic -bios none \
    -kernel adros-riscv.bin -serial mon:stdio
```

Expected output:
```
[AdrOS/riscv64] Booting on QEMU virt...
[CPU] No arch-specific feature detection.
Welcome to AdrOS (x86/ARM/RISC-V/MIPS)!
```

- To quit: `Ctrl+A`, then `x`.
- RISC-V boots with NS16550 UART at 0x10000000, M-mode, `-bios none`.

## 7. Building & Running (MIPS32)

### Build
```bash
make ARCH=mips
```
This produces `adros-mips.bin`.

### Run on QEMU
```bash
make run-mips
# or manually:
qemu-system-mipsel -M malta -m 128M -nographic \
    -kernel adros-mips.bin -serial mon:stdio
```

Expected output:
```
[AdrOS/mips32] Booting on QEMU Malta...
[CPU] No arch-specific feature detection.
Welcome to AdrOS (x86/ARM/RISC-V/MIPS)!
```

- To quit: `Ctrl+A`, then `x`.
- MIPS32 boots with 16550 UART at ISA I/O 0x3F8 (KSEG1 0xB80003F8), `-march=mips32r2`.

## 8. Common Troubleshooting

- **"Multiboot header not found"**: Check whether `grub-file --is-x86-multiboot2 adros-x86.bin` returns success (0). If it fails, the section order in `linker.ld` may be wrong.
- **"Triple Fault (infinite reset)"**: Usually caused by paging (VMM) issues or a misconfigured IDT. Run `make ARCH=x86 run QEMU_DEBUG=1` and inspect `qemu.log`.
- **Black screen (VGA)**: If you are running with `-nographic`, you will not see the VGA output. Remove that flag to get a window.
