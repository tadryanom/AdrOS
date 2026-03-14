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
# AdrOS Toolchain Builder
#
# Builds a complete i686-adros cross-compilation toolchain:
#   1. Binutils  (assembler, linker)
#   2. GCC       (bootstrap, C only, no libc headers)
#   3. Newlib    (C library)
#   4. GCC       (full rebuild with Newlib sysroot)
#   5. Bash      (optional, cross-compiled)
#
# Usage:
#   ./toolchain/build.sh [--prefix /opt/adros] [--jobs 4] [--skip-bash]
#
# Prerequisites:
#   - GCC (host), G++, Make, Texinfo, GMP, MPFR, MPC, ISL (dev packages)
#   - wget or git for downloading sources
#   - ~4 GB disk space
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ADROS_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ---- Defaults ----
PREFIX="/opt/adros-toolchain"
TARGET="i686-adros"
JOBS="$(nproc 2>/dev/null || echo 4)"
SKIP_BASH=0

# ---- Versions ----
BINUTILS_VER="2.42"
GCC_VER="13.2.0"
NEWLIB_VER="4.4.0.20231231"
BASH_VER="5.2.21"

BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VER}.tar.xz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.xz"
NEWLIB_URL="https://sourceware.org/pub/newlib/newlib-${NEWLIB_VER}.tar.gz"
BASH_URL="https://ftp.gnu.org/gnu/bash/bash-${BASH_VER}.tar.gz"

# ---- Parse args ----
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)  PREFIX="$2"; shift 2 ;;
        --jobs)    JOBS="$2"; shift 2 ;;
        --skip-bash) SKIP_BASH=1; shift ;;
        --help|-h)
            echo "Usage: $0 [--prefix DIR] [--jobs N] [--skip-bash]"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

SYSROOT="${PREFIX}/${TARGET}"
BUILD_DIR="${ADROS_ROOT}/toolchain/build"
SRC_DIR="${ADROS_ROOT}/toolchain/src"
PATCH_DIR="${ADROS_ROOT}/toolchain/patches"
LOG_DIR="${ADROS_ROOT}/toolchain/logs"

export PATH="${PREFIX}/bin:${PATH}"

msg()  { echo -e "\n\033[1;32m==>\033[0m \033[1m$1\033[0m"; }
err()  { echo -e "\n\033[1;31m==> ERROR:\033[0m $1" >&2; exit 1; }
step() { echo -e "  \033[1;34m->\033[0m $1"; }

mkdir -p "$BUILD_DIR" "$SRC_DIR" "$LOG_DIR" "$SYSROOT"

# ---- Download sources ----
download() {
    local url="$1" dest="$2"
    if [[ -f "$dest" ]]; then
        step "Already downloaded: $(basename "$dest")"
        return
    fi
    step "Downloading $(basename "$dest")..."
    wget -q --show-progress -O "$dest" "$url"
}

extract() {
    local archive="$1" dir="$2"
    if [[ -d "$dir" ]]; then
        step "Already extracted: $(basename "$dir")"
        return
    fi
    step "Extracting $(basename "$archive")..."
    case "$archive" in
        *.tar.xz) tar xf "$archive" -C "$SRC_DIR" ;;
        *.tar.gz) tar xzf "$archive" -C "$SRC_DIR" ;;
        *) err "Unknown archive type: $archive" ;;
    esac
}

msg "Downloading sources"
download "$BINUTILS_URL" "$SRC_DIR/binutils-${BINUTILS_VER}.tar.xz"
download "$GCC_URL"      "$SRC_DIR/gcc-${GCC_VER}.tar.xz"
download "$NEWLIB_URL"   "$SRC_DIR/newlib-${NEWLIB_VER}.tar.gz"
[[ $SKIP_BASH -eq 0 ]] && download "$BASH_URL" "$SRC_DIR/bash-${BASH_VER}.tar.gz"

msg "Extracting sources"
extract "$SRC_DIR/binutils-${BINUTILS_VER}.tar.xz" "$SRC_DIR/binutils-${BINUTILS_VER}"
extract "$SRC_DIR/gcc-${GCC_VER}.tar.xz"           "$SRC_DIR/gcc-${GCC_VER}"
extract "$SRC_DIR/newlib-${NEWLIB_VER}.tar.gz"      "$SRC_DIR/newlib-${NEWLIB_VER}"
[[ $SKIP_BASH -eq 0 ]] && extract "$SRC_DIR/bash-${BASH_VER}.tar.gz" "$SRC_DIR/bash-${BASH_VER}"

# ---- Apply patches ----
apply_patch() {
    local src_dir="$1" patch_file="$2" marker="$1/.adros_patched_$(basename "$2")"
    if [[ -f "$marker" ]]; then
        step "Already patched: $(basename "$patch_file")"
        return
    fi
    step "Applying $(basename "$patch_file") to $(basename "$src_dir")..."
    patch -d "$src_dir" -p1 < "$patch_file"
    touch "$marker"
}

msg "Applying AdrOS target patches"
apply_patch "$SRC_DIR/binutils-${BINUTILS_VER}" "$PATCH_DIR/binutils-adros.patch"
apply_patch "$SRC_DIR/gcc-${GCC_VER}"           "$PATCH_DIR/gcc-adros.patch"
apply_patch "$SRC_DIR/newlib-${NEWLIB_VER}"      "$PATCH_DIR/newlib-adros.patch"

# Copy libgloss stubs into Newlib source tree
if [[ ! -d "$SRC_DIR/newlib-${NEWLIB_VER}/libgloss/adros" ]]; then
    step "Copying libgloss/adros/ into Newlib source tree..."
    cp -r "$ADROS_ROOT/newlib/libgloss/adros" "$SRC_DIR/newlib-${NEWLIB_VER}/libgloss/adros"
fi

# ---- Install sysroot headers ----
msg "Installing sysroot headers"
step "Copying ulibc headers to ${SYSROOT}/include..."
mkdir -p "${SYSROOT}/include/sys" "${SYSROOT}/include/linux"
cp -r "$ADROS_ROOT/user/ulibc/include/"*.h "${SYSROOT}/include/" 2>/dev/null || true
cp -r "$ADROS_ROOT/user/ulibc/include/sys/"*.h "${SYSROOT}/include/sys/" 2>/dev/null || true
cp -r "$ADROS_ROOT/user/ulibc/include/linux/"*.h "${SYSROOT}/include/linux/" 2>/dev/null || true

# Copy kernel headers needed by the toolchain
step "Copying kernel headers to sysroot..."
for h in errno.h syscall.h socket.h; do
    [[ -f "$ADROS_ROOT/include/$h" ]] && cp "$ADROS_ROOT/include/$h" "${SYSROOT}/include/kernel_${h}"
done

# ==================================================================
# STEP 1: Build Binutils
# ==================================================================
msg "Building Binutils ${BINUTILS_VER}"
mkdir -p "$BUILD_DIR/binutils"
cd "$BUILD_DIR/binutils"

if [[ ! -f "$PREFIX/bin/${TARGET}-ld" ]]; then
    "$SRC_DIR/binutils-${BINUTILS_VER}/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --with-sysroot="$SYSROOT" \
        --disable-nls \
        --disable-werror \
        2>&1 | tee "$LOG_DIR/binutils-configure.log"

    make -j"$JOBS" 2>&1 | tee "$LOG_DIR/binutils-build.log"
    make install    2>&1 | tee "$LOG_DIR/binutils-install.log"
    step "Binutils installed: ${PREFIX}/bin/${TARGET}-ld"
else
    step "Binutils already installed"
fi

# ==================================================================
# STEP 2: Build GCC (bootstrap — no libc)
# ==================================================================
msg "Building GCC ${GCC_VER} (bootstrap, C only)"
mkdir -p "$BUILD_DIR/gcc-bootstrap"
cd "$BUILD_DIR/gcc-bootstrap"

if [[ ! -f "$PREFIX/bin/${TARGET}-gcc" ]]; then
    "$SRC_DIR/gcc-${GCC_VER}/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --with-sysroot="$SYSROOT" \
        --without-headers \
        --enable-languages=c \
        --disable-nls \
        --disable-shared \
        --disable-threads \
        --disable-libssp \
        --disable-libquadmath \
        --disable-libgomp \
        --disable-libatomic \
        --with-newlib \
        2>&1 | tee "$LOG_DIR/gcc-bootstrap-configure.log"

    make -j"$JOBS" all-gcc all-target-libgcc \
        2>&1 | tee "$LOG_DIR/gcc-bootstrap-build.log"
    make install-gcc install-target-libgcc \
        2>&1 | tee "$LOG_DIR/gcc-bootstrap-install.log"
    step "Bootstrap GCC installed: ${PREFIX}/bin/${TARGET}-gcc"
else
    step "Bootstrap GCC already installed"
fi

# ==================================================================
# STEP 3: Build Newlib
# ==================================================================
msg "Building Newlib ${NEWLIB_VER}"
mkdir -p "$BUILD_DIR/newlib"
cd "$BUILD_DIR/newlib"

if [[ ! -f "${SYSROOT}/lib/libc.a" ]]; then
    "$SRC_DIR/newlib-${NEWLIB_VER}/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --disable-multilib \
        --enable-newlib-nano-malloc \
        --enable-newlib-io-long-long \
        --enable-newlib-io-c99-formats \
        --disable-newlib-supplied-syscalls \
        2>&1 | tee "$LOG_DIR/newlib-configure.log"

    make -j"$JOBS" 2>&1 | tee "$LOG_DIR/newlib-build.log"
    make install   2>&1 | tee "$LOG_DIR/newlib-install.log"
    step "Newlib installed to sysroot"
else
    step "Newlib already installed"
fi

# ==================================================================
# STEP 4: Rebuild GCC (full, with Newlib)
# ==================================================================
msg "Rebuilding GCC ${GCC_VER} (full, with Newlib sysroot)"
mkdir -p "$BUILD_DIR/gcc-full"
cd "$BUILD_DIR/gcc-full"

# Only rebuild if the full gcc doesn't link against newlib yet
if [[ ! -f "$BUILD_DIR/gcc-full/.built" ]]; then
    "$SRC_DIR/gcc-${GCC_VER}/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --with-sysroot="$SYSROOT" \
        --enable-languages=c,c++ \
        --disable-nls \
        --disable-shared \
        --with-newlib \
        --disable-libssp \
        --disable-libquadmath \
        2>&1 | tee "$LOG_DIR/gcc-full-configure.log"

    make -j"$JOBS" 2>&1 | tee "$LOG_DIR/gcc-full-build.log"
    make install   2>&1 | tee "$LOG_DIR/gcc-full-install.log"
    touch "$BUILD_DIR/gcc-full/.built"
    step "Full GCC installed: ${PREFIX}/bin/${TARGET}-g++"
else
    step "Full GCC already installed"
fi

# ==================================================================
# STEP 5: Cross-compile Bash (optional)
# ==================================================================
if [[ $SKIP_BASH -eq 0 ]]; then
    msg "Cross-compiling Bash ${BASH_VER}"
    mkdir -p "$BUILD_DIR/bash"
    cd "$BUILD_DIR/bash"

    if [[ ! -f "$BUILD_DIR/bash/bash" ]]; then
        # Bash needs a config.cache for cross-compilation
        cat > config.cache <<'CACHE_EOF'
ac_cv_func_mmap_fixed_mapped=no
ac_cv_func_setvbuf_reversed=no
ac_cv_func_strcoll_works=yes
ac_cv_func_working_mktime=yes
ac_cv_type_getgroups=gid_t
ac_cv_rl_version=8.2
bash_cv_func_sigsetjmp=present
bash_cv_func_ctype_nonascii=no
bash_cv_must_reinstall_sighandlers=no
bash_cv_func_snprintf=yes
bash_cv_func_vsnprintf=yes
bash_cv_printf_a_format=no
bash_cv_pgrp_pipe=no
bash_cv_sys_named_pipes=missing
bash_cv_job_control_missing=present
bash_cv_sys_siglist=yes
bash_cv_under_sys_siglist=yes
bash_cv_opendir_not_robust=no
bash_cv_ulimit_maxfds=yes
bash_cv_getenv_redef=yes
bash_cv_getcwd_malloc=yes
bash_cv_type_rlimit=long
bash_cv_type_intmax_t=int
bash_cv_type_uintmax_t=unsigned
CACHE_EOF

        "$SRC_DIR/bash-${BASH_VER}/configure" \
            --host="$TARGET" \
            --prefix=/usr \
            --without-bash-malloc \
            --disable-nls \
            --cache-file=config.cache \
            CC="${TARGET}-gcc" \
            AR="${TARGET}-ar" \
            RANLIB="${TARGET}-ranlib" \
            CFLAGS="-Os -static" \
            LDFLAGS="-static" \
            2>&1 | tee "$LOG_DIR/bash-configure.log"

        make -j"$JOBS" 2>&1 | tee "$LOG_DIR/bash-build.log"
        step "Bash built: $BUILD_DIR/bash/bash"

        # Copy to AdrOS initrd
        if [[ -d "$ADROS_ROOT/iso/boot" ]]; then
            step "Installing bash to AdrOS filesystem..."
            mkdir -p "$ADROS_ROOT/iso/bin"
            cp "$BUILD_DIR/bash/bash" "$ADROS_ROOT/iso/bin/bash"
        fi
    else
        step "Bash already built"
    fi
fi

# ==================================================================
# Summary
# ==================================================================
msg "Toolchain build complete!"
echo ""
echo "  Prefix:    $PREFIX"
echo "  Target:    $TARGET"
echo "  Sysroot:   $SYSROOT"
echo ""
echo "  Tools:"
echo "    ${TARGET}-gcc    — C compiler"
echo "    ${TARGET}-g++    — C++ compiler"
echo "    ${TARGET}-ld     — Linker"
echo "    ${TARGET}-as     — Assembler"
echo "    ${TARGET}-ar     — Archiver"
echo "    ${TARGET}-objdump — Disassembler"
echo ""
echo "  Usage:"
echo "    export PATH=${PREFIX}/bin:\$PATH"
echo "    ${TARGET}-gcc -o hello hello.c"
echo ""
if [[ $SKIP_BASH -eq 0 ]]; then
    echo "  Bash: $BUILD_DIR/bash/bash"
fi
