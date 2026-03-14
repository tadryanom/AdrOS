# AdrOS Third-Party Ports

Cross-compiled software packages for AdrOS using the `i686-adros` toolchain.

## Prerequisites

Build the AdrOS toolchain first:

```bash
./toolchain/build.sh
export PATH=/opt/adros-toolchain/bin:$PATH
```

## Available Ports

### Bash 5.2.21

Built as part of the toolchain. See `toolchain/build.sh`.

```bash
./toolchain/build.sh          # builds bash along with toolchain
# Binary: toolchain/build/bash/bash
```

### Busybox 1.36.1

Minimal set of ~60 UNIX utilities in a single static binary.

```bash
./ports/busybox/build.sh
# Binary: ports/busybox/build/busybox
# Applets installed to: ports/busybox/install/
```

## Adding to AdrOS initrd

After building, copy binaries into the initrd staging area:

```bash
# Bash
cp toolchain/build/bash/bash rootfs/bin/bash

# Busybox — install as individual symlinks
cp ports/busybox/build/busybox rootfs/bin/busybox
for cmd in ls cat cp mv rm mkdir grep sed awk sort; do
    ln -sf busybox rootfs/bin/$cmd
done

# Rebuild the ISO
make iso
```

## Adding New Ports

1. Create `ports/<name>/build.sh` with download + configure + build steps
2. Use `i686-adros-gcc` with `-static` and `-D_POSIX_VERSION=200112L`
3. Add a defconfig or patch directory as needed
4. Document in this README
