# AdrOS Testing Infrastructure Plan

## Current State

All testing layers are **implemented and operational**:

- **Static analysis** (`make check`): cppcheck + sparse + gcc -fanalyzer
- **QEMU smoke tests** (`make test`): expect-based, 120 checks (file I/O, signals, memory, IPC, devices, procfs, networking, epoll, epollet, inotify, aio, nanosleep, CLOCK_REALTIME/CLOCK_MONOTONIC, /dev/urandom, /proc/cmdline, CoW fork, readv/writev, fsync, truncate, getuid/getgid, chmod, flock, times, gettid, posix_spawn, TSC ns precision, SIGSEGV, gettimeofday, mprotect, getrlimit/setrlimit, uname, LZ4 initrd decomp, lazy PLT, execve, clone, pivot_root, dlopen/dlsym/dlclose, execveat, futex, sigaltstack, socket API, mqueue, semaphores, chown, mount/umount2), 4-CPU SMP, 120s timeout
- **Test battery** (`make test-battery`): 152 checks across QEMU scenarios — 120 smoke patterns + SMP=1 boot (12 checks) + SMP=2 boot (6 checks) + multi-disk ATA (hda+hdb+hdd) + VFS mount + ICMP ping + diskfs ops
- **Host unit tests** (`make test-host`): 212 tests — `test_utils.c` (63: itoa/atoi, path_normalize, align, tar_parse_octal, mount prefix/normalize, VFS permission, ELF validation) + `test_security.c` (38: user_range_ok, bitmap, eflags, signal mask logic, chmod symbolic parsing) + `test_host_utils.sh` (111 cross-compiled utility tests)
- **GDB scripted checks** (`make test-gdb`): 10 checks — heap integrity, PMM bitmap sanity, VGA mapping, PID 1 state, scheduler runqueue bitmap, mount count, frame 0 refcount
- **Full suite** (`make test-all`): runs check + test-host + test
- **Multi-arch build verification**: `make ARCH=arm`, `make ARCH=riscv`, and `make ARCH=mips` compile and boot on QEMU

## Available Tools (already installed)

| Tool | Path | Purpose |
|------|------|---------|
| cppcheck | /usr/bin/cppcheck | Static analysis (already in use) |
| sparse | /usr/bin/sparse | Kernel-oriented static analysis (C semantics, type checking) |
| gcc | /usr/bin/gcc | Compiler with `-fsanitize`, `-fanalyzer` |
| qemu-system-i386 | /usr/bin/qemu-system-i386 | x86 emulation + smoke tests |
| qemu-system-aarch64 | /usr/bin/qemu-system-aarch64 | ARM64 emulation |
| qemu-system-riscv64 | /usr/bin/qemu-system-riscv64 | RISC-V 64 emulation |
| qemu-system-mipsel | /usr/bin/qemu-system-mipsel | MIPS32 little-endian emulation |
| gdb | /usr/bin/gdb | Debugging via QEMU `-s -S` |
| expect | /usr/bin/expect | Scripted QEMU serial interaction |
| python3 | /usr/bin/python3 | Test runner orchestration |

## Proposed Testing Layers

### Layer 1: Enhanced Static Analysis (`make check`)

**Tools**: cppcheck + sparse + gcc -fanalyzer

- **cppcheck**: Already in use. Keep as-is.
- **sparse**: Kernel-focused. Catches `__user` pointer misuse, bitwise vs logical
  confusion, type width issues. Run with `C=1` or as a standalone pass.
- **gcc -fanalyzer**: Interprocedural static analysis. Catches use-after-free,
  double-free, NULL deref paths, buffer overflows across function boundaries.

**Implementation**: Single `make check` target that runs all three.

### Layer 2: QEMU + expect Automated Regression (`make test`)

**Tools**: expect (or Python pexpect) + QEMU serial

This replaces the manual `grep serial.log` approach with a scripted test that:
1. Boots QEMU with serial output to a PTY
2. Waits for specific strings in order (with timeouts)
3. Reports PASS/FAIL per test case
4. Detects PANIC, OOM, or unexpected output

**Why expect over Unity/KUnit**:
- **Unity** requires linking a test framework into the kernel binary, which changes
  the binary layout and can mask bugs. It also requires a host-side test runner.
- **KUnit** is Linux-specific and not applicable to a custom kernel.
- **expect** tests the actual kernel binary as-is, catching real boot/runtime bugs
  that unit tests would miss. It's the right tool for an OS kernel.

**Implementation**: `tests/smoke_test.exp` expect script + `make test` target.

A separate **test battery** (`tests/test_battery.exp`) exercises multi-disk ATA detection,
VFS mount verification, and ICMP ping across multiple QEMU configurations.

### Layer 3: QEMU + GDB Scripted Debugging (`make test-gdb`)

**Tools**: QEMU `-s -S` + GDB with Python scripting

For targeted regression tests that need to inspect kernel state:
- Verify page table entries after VMM operations
- Check PMM bitmap consistency
- Validate heap integrity after stress allocation
- Breakpoint on `uart_print("[HEAP] Corruption")` to catch corruption early

**Implementation**: `tests/gdb_checks.py` GDB Python script + `make test-gdb` target.

### Layer 4: Host-Side Unit Tests for Pure Functions (`make test-host`)

**Tools**: Simple C test harness (no external framework needed)

Some kernel functions are pure computation with no hardware dependency:
- `itoa`, `itoa_hex`, `atoi`, `strlen`, `strcmp`, `memcpy`, `memset`
- `path_normalize_inplace` (critical for security)
- `align_up`, `align_down`
- Bitmap operations
- `tar_parse_octal` (initrd header parsing)
- `path_is_mountpoint_prefix`, `normalize_mountpoint` (VFS mount logic)
- `vfs_check_permission` (file permission checks)
- `elf32_validate` (ELF header validation)
- Signal mask logic (KILL/STOP always deliverable, sigpending = pending & blocked)
- `parse_symbolic` (chmod symbolic mode parsing)

These can be compiled and run on the host with `gcc -m32` and a minimal test harness.
No need for Unity — a simple `assert()` + `main()` is sufficient for a kernel project.

**Implementation**: `tests/test_utils.c` compiled with host gcc.

## Recommended Implementation Order

1. **Layer 1** (sparse + gcc -fanalyzer) — immediate value, zero runtime cost
2. **Layer 2** (expect smoke test) — replaces manual grep, catches regressions
3. **Layer 4** (host unit tests) — catches logic bugs in pure functions
4. **Layer 3** (GDB scripted) — for deep debugging, lower priority

## DOOM Smoke Test

The DOOM port (`/bin/doom.elf`) serves as a heavyweight integration test:
- Exercises `mmap` (fd-backed for `/dev/fb0`), `brk` (Z_Malloc heap), `nanosleep`, `clock_gettime`
- Tests `/dev/fb0` ioctl (`FBIOGET_VSCREENINFO`, `FBIOGET_FSCREENINFO`) and framebuffer mmap
- Tests `/dev/kbd` raw scancode reads (non-blocking)
- Stresses the VFS, memory allocator, and process model simultaneously

To run manually: boot AdrOS with `-vga std`, then execute `/bin/doom.elf` from the shell.

## Makefile Targets

```makefile
make check        # cppcheck + sparse + gcc -fanalyzer
make test         # QEMU + expect automated smoke test (120 checks incl. ICMP ping, epoll, epollet, inotify, aio, CoW fork, flock, posix_spawn, gettimeofday, mprotect, uname, LZ4, lazy PLT, clone, pivot_root, dlopen/dlsym/dlclose, execveat, futex, sigaltstack, socket API, mqueue, semaphores, chown, mount/umount2)
make test-battery # Full test battery: 120 smoke + SMP=1 (12) + SMP=2 (6) + multi-disk ATA + VFS mount + ping + diskfs (152 checks)
make test-host    # Host-side unit tests for pure functions
make test-gdb     # QEMU + GDB scripted checks (optional)
make test-all     # All of the above

# Multi-arch build verification
make ARCH=arm     # Build ARM64 kernel
make run-arm      # Boot ARM64 on QEMU virt (aarch64, cortex-a57)
make ARCH=riscv   # Build RISC-V 64 kernel
make run-riscv    # Boot RISC-V 64 on QEMU virt
make ARCH=mips    # Build MIPS32 kernel
make run-mips     # Boot MIPS32 on QEMU Malta
```
