#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
#
# AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
# Mirror: https://github.com/tadryanom/AdrOS
#

#
# AdrOS — Busybox cross-compile script
#
# Builds a minimal Busybox for AdrOS using the i686-adros toolchain.
#
# Prerequisites:
#   - AdrOS toolchain built (toolchain/build.sh)
#   - PATH includes /opt/adros-toolchain/bin
#
# Usage:
#   ./ports/busybox/build.sh [--prefix /opt/adros-toolchain] [--jobs 4]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ADROS_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# ---- Defaults ----
PREFIX="/opt/adros-toolchain"
TARGET="i686-adros"
JOBS="$(nproc 2>/dev/null || echo 4)"
BUSYBOX_VER="1.36.1"
BUSYBOX_URL="https://busybox.net/downloads/busybox-${BUSYBOX_VER}.tar.bz2"

SRC_DIR="$ADROS_ROOT/ports/busybox/src"
BUILD_DIR="$ADROS_ROOT/ports/busybox/build"
LOG_DIR="$ADROS_ROOT/ports/busybox/logs"
DEFCONFIG="$SCRIPT_DIR/adros_defconfig"

# ---- Parse args ----
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)  PREFIX="$2"; shift 2 ;;
        --jobs)    JOBS="$2"; shift 2 ;;
        --help|-h)
            echo "Usage: $0 [--prefix DIR] [--jobs N]"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

export PATH="$PREFIX/bin:$PATH"

msg() { echo -e "\n\033[1;34m==> $1\033[0m"; }
step() { echo "  [OK] $1"; }
die() { echo -e "\033[1;31mERROR: $1\033[0m" >&2; exit 1; }

# Verify toolchain
command -v "${TARGET}-gcc" >/dev/null 2>&1 || die "Toolchain not found. Run toolchain/build.sh first."

# ---- Download ----
mkdir -p "$SRC_DIR" "$BUILD_DIR" "$LOG_DIR"

if [[ ! -d "$SRC_DIR/busybox-${BUSYBOX_VER}" ]]; then
    msg "Downloading Busybox ${BUSYBOX_VER}..."
    TARBALL="$SRC_DIR/busybox-${BUSYBOX_VER}.tar.bz2"
    if [[ ! -f "$TARBALL" ]]; then
        wget -q -O "$TARBALL" "$BUSYBOX_URL" || die "Download failed"
    fi
    tar xf "$TARBALL" -C "$SRC_DIR"
    step "Busybox source extracted"
fi

# ---- Configure ----
msg "Configuring Busybox..."
cd "$BUILD_DIR"

BUSYBOX_SRC="$SRC_DIR/busybox-${BUSYBOX_VER}"

if [[ -f "$DEFCONFIG" ]]; then
    cp "$DEFCONFIG" "$BUSYBOX_SRC/.config"
    make -C "$BUSYBOX_SRC" O="$BUILD_DIR" oldconfig \
        CROSS_COMPILE="${TARGET}-" \
        2>&1 | tee "$LOG_DIR/busybox-configure.log"
else
    # Generate minimal defconfig
    make -C "$BUSYBOX_SRC" O="$BUILD_DIR" defconfig \
        CROSS_COMPILE="${TARGET}-" \
        2>&1 | tee "$LOG_DIR/busybox-configure.log"
fi

step "Busybox configured"

# ---- Build ----
msg "Building Busybox..."
make -C "$BUILD_DIR" -j"$JOBS" \
    CROSS_COMPILE="${TARGET}-" \
    CFLAGS="-Os -static -D_POSIX_VERSION=200112L" \
    LDFLAGS="-static" \
    2>&1 | tee "$LOG_DIR/busybox-build.log"

step "Busybox built: $BUILD_DIR/busybox"

# ---- Install to initrd staging area ----
msg "Installing Busybox applets..."
INSTALL_DIR="$ADROS_ROOT/ports/busybox/install"
rm -rf "$INSTALL_DIR"
make -C "$BUILD_DIR" install \
    CROSS_COMPILE="${TARGET}-" \
    CONFIG_PREFIX="$INSTALL_DIR" \
    2>&1 | tee "$LOG_DIR/busybox-install.log"

step "Busybox installed to $INSTALL_DIR"

# ---- Summary ----
echo ""
echo "Busybox build complete!"
echo ""
echo "  Binary:   $BUILD_DIR/busybox"
echo "  Install:  $INSTALL_DIR"
echo ""
echo "  To add to AdrOS initrd:"
echo "    cp $BUILD_DIR/busybox rootfs/bin/"
echo "    # Create symlinks for desired applets"
echo ""
echo "  Applets included:"
ls "$INSTALL_DIR/bin/" 2>/dev/null | head -20
echo "  ..."
