# AdrOS Testing Infrastructure Plan

## Current State

All four testing layers are **implemented and operational**:

- **Static analysis** (`make check`): cppcheck + sparse + gcc -fanalyzer (59 x86 C files)
- **QEMU smoke tests** (`make test`): expect-based, 19 checks, 4-CPU SMP, 40s timeout
- **Host unit tests** (`make test-host`): 47 tests — `test_utils.c` (28) + `test_security.c` (19)
- **GDB scripted checks** (`make test-gdb`): heap/PMM/VGA integrity validation
- **Full suite** (`make test-all`): runs check + test-host + test

## Available Tools (already installed)

| Tool | Path | Purpose |
|------|------|---------|
| cppcheck | /usr/bin/cppcheck | Static analysis (already in use) |
| sparse | /usr/bin/sparse | Kernel-oriented static analysis (C semantics, type checking) |
| gcc | /usr/bin/gcc | Compiler with `-fsanitize`, `-fanalyzer` |
| qemu-system-i386 | /usr/bin/qemu-system-i386 | Emulation + smoke tests |
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
make check      # cppcheck + sparse + gcc -fanalyzer
make test        # QEMU + expect automated smoke test
make test-host   # Host-side unit tests for pure functions
make test-gdb    # QEMU + GDB scripted checks (optional)
make test-all    # All of the above
```
