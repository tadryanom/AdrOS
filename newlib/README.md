# Newlib Port for AdrOS

This directory contains the libgloss (OS glue layer) needed to build
[Newlib](https://sourceware.org/newlib/) for AdrOS.

## Prerequisites

1. A working `i686-elf-gcc` cross-compiler (or the future `i686-adros-gcc`)
2. Newlib source tree (clone from `git://sourceware.org/git/newlib-cygwin.git`)

## Quick Start (using i686-elf toolchain)

The libgloss stubs can be compiled standalone for testing:

```bash
cd newlib/libgloss/adros
make CC=i686-elf-gcc AR=i686-elf-ar
```

This produces:
- `crt0.o` — C runtime startup (entry point `_start`)
- `libadros.a` — syscall stubs library

## Full Newlib Build (after creating i686-adros target)

### Step 1: Patch Newlib source tree

Copy the integration patches into your Newlib source tree:

```bash
NEWLIB_SRC=/path/to/newlib-cygwin

# Copy libgloss port
cp -r newlib/libgloss/adros $NEWLIB_SRC/libgloss/adros

# Apply configure patches
patch -d $NEWLIB_SRC -p1 < newlib/patches/newlib-adros-target.patch
```

### Step 2: Build

```bash
mkdir build-newlib && cd build-newlib
export PATH=/opt/adros-toolchain/bin:$PATH

../newlib-cygwin/configure \
    --target=i686-adros \
    --prefix=/opt/adros-toolchain/i686-adros \
    --disable-multilib \
    --enable-newlib-nano-malloc

make -j$(nproc)
make install
```

### Step 3: Link user programs

```bash
i686-adros-gcc -o hello hello.c -lm
```

The toolchain will automatically use:
- `crt0.o` as the startup file
- `libadros.a` for syscall stubs
- Newlib's `libc.a` and `libm.a`

## Implemented Stubs

| Function | AdrOS Syscall | Notes |
|---|---|---|
| `_exit()` | `SYS_EXIT (2)` | |
| `_read()` | `SYS_READ (5)` | |
| `_write()` | `SYS_WRITE (1)` | |
| `_open()` | `SYS_OPEN (4)` | |
| `_close()` | `SYS_CLOSE (6)` | |
| `_lseek()` | `SYS_LSEEK (9)` | |
| `_fstat()` | `SYS_FSTAT (10)` | |
| `_stat()` | `SYS_STAT (11)` | |
| `_isatty()` | `SYS_IOCTL (21)` | Uses TIOCGPGRP probe |
| `_kill()` | `SYS_KILL (19)` | |
| `_getpid()` | `SYS_GETPID (3)` | |
| `_sbrk()` | `SYS_BRK (41)` | Newlib malloc backend |
| `_link()` | `SYS_LINK (54)` | |
| `_unlink()` | `SYS_UNLINK (29)` | |
| `_fork()` | `SYS_FORK (16)` | |
| `_execve()` | `SYS_EXECVE (15)` | |
| `_wait()` | `SYS_WAITPID (7)` | Wraps waitpid(-1, ...) |
| `_times()` | `SYS_TIMES (84)` | |
| `_gettimeofday()` | `SYS_GETTIMEOFDAY (127)` | RTC epoch + TSC µs |
| `_rename()` | `SYS_RENAME (39)` | |
| `_mkdir()` | `SYS_MKDIR (28)` | |
