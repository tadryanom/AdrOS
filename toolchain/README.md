# AdrOS Cross-Compilation Toolchain

Build a complete `i686-adros` toolchain: **Binutils → GCC (bootstrap) → Newlib → GCC (full) → Bash**.

## Prerequisites

Install on Ubuntu/Debian:
```bash
sudo apt install build-essential texinfo libgmp-dev libmpfr-dev libmpc-dev libisl-dev wget
```

Install on Fedora:
```bash
sudo dnf install gcc gcc-c++ make texinfo gmp-devel mpfr-devel libmpc-devel isl-devel wget
```

## Quick Build

```bash
# Build everything (installs to /opt/adros-toolchain by default)
sudo mkdir -p /opt/adros-toolchain && sudo chown $USER /opt/adros-toolchain
./toolchain/build.sh

# Or specify a custom prefix
./toolchain/build.sh --prefix $HOME/adros-toolchain --jobs 8

# Skip Bash if you only need the C compiler
./toolchain/build.sh --skip-bash
```

Build takes approximately 20-40 minutes depending on hardware.

## What Gets Built

| Step | Component | Output |
|------|-----------|--------|
| 1 | Binutils 2.42 | `i686-adros-ld`, `i686-adros-as`, `i686-adros-objdump`, ... |
| 2 | GCC 13.2 (bootstrap) | `i686-adros-gcc` (C only, no libc) |
| 3 | Newlib 4.4.0 | `libc.a`, `libm.a` (installed to sysroot) |
| 4 | GCC 13.2 (full) | `i686-adros-gcc`, `i686-adros-g++` (C/C++ with Newlib) |
| 5 | Bash 5.2.21 | Statically linked `bash` binary for AdrOS |

## Usage

```bash
export PATH=/opt/adros-toolchain/bin:$PATH

# Compile a C program
i686-adros-gcc -o hello hello.c

# Compile with math library
i686-adros-gcc -o calc calc.c -lm

# Static linking (default)
i686-adros-gcc -static -o myapp myapp.c
```

## Sysroot Layout

```
/opt/adros-toolchain/i686-adros/
├── include/        # Newlib + AdrOS headers
│   ├── stdio.h     # Newlib
│   ├── stdlib.h    # Newlib
│   ├── sys/        # Newlib + AdrOS system headers
│   └── ...
├── lib/
│   ├── libc.a      # Newlib C library
│   ├── libm.a      # Newlib math library
│   ├── libadros.a  # AdrOS syscall stubs
│   ├── crt0.o      # C runtime startup
│   └── libgcc.a    # GCC support library
└── ...
```

## AdrOS Target Patches

The `patches/` directory contains **reference diffs** documenting the changes
that `build.sh` applies inline via `sed`.  They are not applied with `patch`;
they exist for review and auditing:

- **`binutils-adros.patch`**: `config.sub`, `bfd/config.bfd`, `gas/configure.tgt`, `ld/configure.tgt`
- **`gcc-adros.patch`**: `config.sub`, `gcc/config.gcc`, `gcc/config/i386/adros.h`, `libgcc/config.host`, `crti.S`/`crtn.S`
- **`newlib-adros.patch`**: `config.sub`, `newlib/configure.host`, `newlib/libc/include/sys/config.h`, `libgloss/configure.in`, autoconf files
- **`native-toolchain.patch`**: GMP/MPFR/MPC/ISL `config.sub` patches for native (Canadian cross) builds

### Key GCC Target Header (`adros.h`)

Defines for the AdrOS target:
- `__adros__`, `__AdrOS__`, `__unix__` preprocessor macros
- ELF32 object format, i386 architecture
- Startup: `crt0.o` → `_start` → `main()` → `exit()`
- Links: `-lc -ladros -lgcc`

## Troubleshooting

**"configure: error: cannot compute sizeof..."**
→ Normal for cross-compilation. The build script passes `--with-newlib` to avoid
running target executables on the host.

**"crt0.o: No such file"**
→ Newlib hasn't been built yet. Run the full build script or build Newlib first.

**GCC can't find headers**
→ Ensure `--with-sysroot` points to the correct sysroot directory.

## Directory Structure

```
toolchain/
├── build.sh            # Master build script
├── README.md           # This file
├── patches/
│   ├── binutils-adros.patch
│   ├── gcc-adros.patch
│   └── newlib-adros.patch
├── build/              # (generated) Build directories
├── src/                # (generated) Downloaded source tarballs
└── logs/               # (generated) Build logs
```
