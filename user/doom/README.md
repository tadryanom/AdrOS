# DOOM on AdrOS

Port of DOOM using [doomgeneric](https://github.com/ozkl/doomgeneric) with an AdrOS-specific platform adapter.

## Prerequisites

- AdrOS cross-compiler (`i686-elf-gcc`)
- ulibc built (`../ulibc/libulibc.a`)
- DOOM1.WAD (shareware, freely distributable)

## Setup

```bash
# 1. Clone doomgeneric into this directory
cd user/doom
git clone https://github.com/ozkl/doomgeneric.git

# 2. Download DOOM1.WAD shareware
# Place doom1.wad in the AdrOS filesystem (e.g., /bin/doom1.wad in the initrd)

# 3. Build
make

# 4. The resulting doom.elf must be added to the initrd via mkinitrd
```

## Architecture

```
user/doom/
├── doomgeneric/          # cloned engine source (~70 C files)
├── doomgeneric_adros.c   # AdrOS platform adapter
├── Makefile              # build rules
└── README.md
```

### Platform Adapter (`doomgeneric_adros.c`)

Implements the doomgeneric interface functions:

| Function | AdrOS Implementation |
|----------|---------------------|
| `DG_Init` | Opens `/dev/fb0`, queries resolution via ioctl, mmaps framebuffer; opens `/dev/kbd` |
| `DG_DrawFrame` | Nearest-neighbor scales 320×200 DOOM buffer to physical framebuffer |
| `DG_GetKey` | Reads raw PS/2 scancodes from `/dev/kbd`, maps to DOOM key codes |
| `DG_GetTicksMs` | `clock_gettime(CLOCK_MONOTONIC)` |
| `DG_SleepMs` | `nanosleep()` |
| `DG_SetWindowTitle` | No-op |

### Kernel Requirements (all implemented)

- `/dev/fb0` — framebuffer device with `ioctl` + `mmap`
- `/dev/kbd` — raw PS/2 scancode device (non-blocking read)
- `mmap` syscall — anonymous + fd-backed
- `brk` syscall — dynamic heap growth
- `nanosleep` / `clock_gettime` — frame timing
- Standard file I/O — WAD file loading (`open`/`read`/`lseek`/`close`)

## Running

From AdrOS shell (or as init process):
```
/bin/doom.elf -iwad /bin/doom1.wad
```

QEMU must be started with a VBE framebuffer (not text mode):
```
qemu-system-i386 -cdrom adros-x86.iso -vga std
```
