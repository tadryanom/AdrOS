# AdrOS - Build & Run Guide

This guide explains how to build and run AdrOS on your local machine (Linux/WSL).

## 1. Dependencies

You will need:
- `make` and `gcc` (build system)
- `qemu-system-*` (emulators)
- `xorriso` (to create bootable ISOs)
- `grub-pc-bin` and `grub-common` (x86 bootloader)
- Cross-compilers for ARM/RISC-V

### On Ubuntu/Debian:
```bash
sudo apt update
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo \
    qemu-system-x86 qemu-system-arm qemu-system-misc \
    grub-common grub-pc-bin xorriso mtools \
    gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu
```

## 2. Building & Running (x86)

This is the primary target (standard PC).

### Build
```bash
make ARCH=x86
```
This produces `adros-x86.bin`.

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

If you are iterating on kernel changes and want to avoid hanging runs, you can wrap it with a timeout:
```bash
timeout 60s make ARCH=x86 run || true
```

Generated outputs/artifacts:
- `serial.log`: UART log (primary kernel output)
- `qemu.log`: only generated when QEMU debug logging is enabled (see below)

Static analysis helper:
```bash
make ARCH=x86 cppcheck
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
