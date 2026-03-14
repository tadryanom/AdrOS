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

# ---- Apply AdrOS target patches (sed-based, robust against line shifts) ----

# Patch config.sub to recognise 'adros' as an OS.
# Two insertions: (1) canonicalisation case, (2) OS validation list.
patch_config_sub() {
    local f="$1"
    # 1) Add adros case before the pikeos case in the canonicalisation switch
    if ! grep -q 'adros' "$f"; then
        sed -i '/^[[:space:]]*pikeos\*)/i\
\tadros*)\
\t\tos=adros\
\t\t;;' "$f"
        # 2) Add adros* to the validation list (same line as dicos*)
        sed -i 's/| dicos\*/| dicos* | adros*/' "$f"
        step "Patched $(basename "$(dirname "$f")")/config.sub"
    fi
}

patch_binutils() {
    local d="$SRC_DIR/binutils-${BINUTILS_VER}"
    local marker="$d/.adros_patched"
    [[ -f "$marker" ]] && { step "Binutils already patched"; return; }

    patch_config_sub "$d/config.sub"

    # bfd/config.bfd — add before i[3-7]86-*-linux-*
    if ! grep -q 'adros' "$d/bfd/config.bfd"; then
        sed -i '/^  i\[3-7\]86-\*-linux-\*)/i\
  i[3-7]86-*-adros*)\
    targ_defvec=i386_elf32_vec\
    targ_selvecs=\
    targ64_selvecs=x86_64_elf64_vec\
    ;;' "$d/bfd/config.bfd"
        step "Patched bfd/config.bfd"
    fi

    # gas/configure.tgt — add after i386-*-darwin*
    if ! grep -q 'adros' "$d/gas/configure.tgt"; then
        sed -i '/i386-\*-darwin\*)/a\
  i386-*-adros*)\t\t\t\tfmt=elf ;;' "$d/gas/configure.tgt"
        step "Patched gas/configure.tgt"
    fi

    # ld/configure.tgt — add before i[3-7]86-*-linux-*
    if ! grep -q 'adros' "$d/ld/configure.tgt"; then
        sed -i '/^i\[3-7\]86-\*-linux-\*)/i\
i[3-7]86-*-adros*)\ttarg_emul=elf_i386\
\t\t\t\t;;' "$d/ld/configure.tgt"
        step "Patched ld/configure.tgt"
    fi

    touch "$marker"
}

patch_gcc() {
    local d="$SRC_DIR/gcc-${GCC_VER}"
    local marker="$d/.adros_patched"
    [[ -f "$marker" ]] && { step "GCC already patched"; return; }

    patch_config_sub "$d/config.sub"

    # gcc/config.gcc — add before x86_64-*-elf*
    if ! grep -q 'adros' "$d/gcc/config.gcc"; then
        sed -i '/^x86_64-\*-elf\*)/i\
i[34567]86-*-adros*)\
\ttm_file="${tm_file} i386/unix.h i386/att.h elfos.h newlib-stdint.h i386/adros.h"\
\ttmake_file="${tmake_file} i386/t-crtstuff"\
\tuse_gcc_stdint=wrap\
\tdefault_use_cxa_atexit=yes\
\t;;' "$d/gcc/config.gcc"
        step "Patched gcc/config.gcc"
    fi

    # gcc/config/i386/adros.h — create target header
    if [[ ! -f "$d/gcc/config/i386/adros.h" ]]; then
        cat > "$d/gcc/config/i386/adros.h" <<'ADROS_H'
/* Target definitions for i386 AdrOS. */

#undef  TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()         \
  do {                                    \
    builtin_define ("__adros__");         \
    builtin_define ("__AdrOS__");         \
    builtin_define ("__unix__");          \
    builtin_assert ("system=adros");      \
    builtin_assert ("system=unix");       \
    builtin_assert ("system=posix");      \
  } while (0)

#undef  OBJECT_FORMAT_ELF
#define OBJECT_FORMAT_ELF 1

#undef  TARGET_64BIT_DEFAULT
#define TARGET_64BIT_DEFAULT 0

#undef  STARTFILE_SPEC
#define STARTFILE_SPEC "crt0.o%s crti.o%s crtbegin.o%s"

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC "crtend.o%s crtn.o%s"

#undef  LIB_SPEC
#define LIB_SPEC "--start-group -lc -ladros --end-group -lgcc"

#undef  LINK_SPEC
#define LINK_SPEC "-m elf_i386 %{shared:-shared} %{static:-static} %{!static: %{rdynamic:-export-dynamic}}"

#undef  SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef  WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32
ADROS_H
        step "Created gcc/config/i386/adros.h"
    fi

    # libgcc/config.host — add before x86_64-*-elf*
    if ! grep -q 'adros' "$d/libgcc/config.host"; then
        sed -i '/^x86_64-\*-elf\* | x86_64-\*-rtems\*)/i\
i[34567]86-*-adros*)\
\ttmake_file="$tmake_file i386/t-crtstuff t-crtstuff-pic t-libgcc-pic"\
\textra_parts="$extra_parts crti.o crtn.o crtbegin.o crtend.o"\
\t;;' "$d/libgcc/config.host"
        step "Patched libgcc/config.host"
    fi

    # crti.S and crtn.S for AdrOS
    if [[ ! -f "$d/libgcc/config/i386/crti-adros.S" ]]; then
        cat > "$d/libgcc/config/i386/crti-adros.S" <<'EOF'
/* crti.S for AdrOS — .init/.fini prologue */
	.section .init
	.global _init
	.type _init, @function
_init:
	push %ebp
	mov  %esp, %ebp

	.section .fini
	.global _fini
	.type _fini, @function
_fini:
	push %ebp
	mov  %esp, %ebp
EOF
        cat > "$d/libgcc/config/i386/crtn-adros.S" <<'EOF'
/* crtn.S for AdrOS — .init/.fini epilogue */
	.section .init
	pop %ebp
	ret

	.section .fini
	pop %ebp
	ret

	.section .note.GNU-stack,"",@progbits
EOF
        step "Created crti-adros.S / crtn-adros.S"
    fi

    touch "$marker"
}

patch_newlib() {
    local d="$SRC_DIR/newlib-${NEWLIB_VER}"
    local marker="$d/.adros_patched"
    [[ -f "$marker" ]] && { step "Newlib already patched"; return; }

    patch_config_sub "$d/config.sub"

    # newlib/configure.host — add after i[34567]86-*-rdos* block
    # have_crt0="no" because crt0.o is provided by libgloss/adros, not libc/sys
    if ! grep -q 'adros' "$d/newlib/configure.host"; then
        sed -i '/i\[34567\]86-\*-rdos\*)/,/;;/{/;;/a\
  i[34567]86-*-adros*)\
\tsys_dir=adros\
\thave_crt0="no"\
\tnewlib_cflags="${newlib_cflags} -DSIGNAL_PROVIDED -DHAVE_OPENDIR -DHAVE_SYSTEM"\
\t;;
}' "$d/newlib/configure.host"
        step "Patched newlib/configure.host"
    fi

    # newlib/libc/include/sys/config.h — add after __rtems__ block
    if ! grep -q '__adros__' "$d/newlib/libc/include/sys/config.h"; then
        sed -i '/#if defined(__rtems__)/,/#endif/{/#endif/a\
\
/* AdrOS target configuration */\
#ifdef __adros__\
#define _READ_WRITE_RETURN_TYPE int\
#define HAVE_SYSTEM\
#define HAVE_OPENDIR\
#endif
}' "$d/newlib/libc/include/sys/config.h"
        step "Patched newlib/libc/include/sys/config.h"
    fi

    # Create newlib/libc/sys/adros/ stub directory (non-recursive Makefile.inc)
    # This is empty because all syscalls are in libgloss/adros.
    if [[ ! -d "$d/newlib/libc/sys/adros" ]]; then
        mkdir -p "$d/newlib/libc/sys/adros"
        cat > "$d/newlib/libc/sys/adros/Makefile.inc" <<'EOF'
## AdrOS system directory — empty (syscalls provided by libgloss/adros)
EOF
        step "Created newlib/libc/sys/adros/"
    fi

    # libgloss/configure.ac — add after i[[3456]]86-*-elf* block
    if ! grep -q 'adros' "$d/libgloss/configure.ac"; then
        sed -i '/i\[\[3456\]\]86-\*-elf\* | i\[\[3456\]\]86-\*-coff\*)/,/;;/{/;;/a\
  i[[3456]]86-*-adros*)\
\tAC_CONFIG_FILES([adros/Makefile])\
\tsubdirs="$subdirs adros"\
\t;;
}' "$d/libgloss/configure.ac"
        step "Patched libgloss/configure.ac"
    fi

    # Copy/sync our libgloss/adros stubs into the Newlib source tree
    mkdir -p "$d/libgloss/adros"
    cp -u "$ADROS_ROOT/newlib/libgloss/adros/"*.{c,S,h} "$d/libgloss/adros/" 2>/dev/null || true
    step "Synced libgloss/adros/ stubs"

    # Create libgloss/adros autoconf files if not present
    if [[ ! -f "$d/libgloss/adros/configure.in" ]]; then
        cat > "$d/libgloss/adros/configure.in" <<'EOF'
dnl AdrOS libgloss configure
AC_PREREQ(2.59)
AC_INIT([libgloss-adros],[0.1])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([cygnus])
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_AS
AC_PROG_AR
AC_PROG_RANLIB
AM_PROG_AS

host_makefile_frag=${srcdir}/../config/default.mh
AC_SUBST(host_makefile_frag)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
EOF
        step "Created libgloss/adros/configure.in"
    fi

    if [[ ! -f "$d/libgloss/adros/Makefile.in" ]]; then
        cat > "$d/libgloss/adros/Makefile.in" <<'EOF'
# Makefile for AdrOS libgloss (autotools-generated template)

srcdir = @srcdir@
VPATH = @srcdir@

prefix = @prefix@
exec_prefix = @exec_prefix@
tooldir = $(exec_prefix)/$(target_alias)

INSTALL = @INSTALL@
INSTALL_PROGRAM = @INSTALL_PROGRAM@
INSTALL_DATA = @INSTALL_DATA@

CC = @CC@
AS = @AS@
AR = @AR@
RANLIB = @RANLIB@

CFLAGS = -g

BSP = libadros.a
OBJS = syscalls.o
CRT0 = crt0.o

all: $(CRT0) $(BSP)

$(BSP): $(OBJS)
	$(AR) rcs $@ $^
	$(RANLIB) $@

crt0.o: crt0.S
	$(CC) $(CFLAGS) -c $< -o $@

syscalls.o: syscalls.c
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	$(INSTALL_DATA) $(CRT0) $(tooldir)/lib/$(CRT0)
	$(INSTALL_DATA) $(BSP) $(tooldir)/lib/$(BSP)

clean:
	rm -f $(OBJS) $(CRT0) $(BSP)

.PHONY: all install clean
EOF
        step "Created libgloss/adros/Makefile.in"
    fi

    touch "$marker"
}

patch_bash() {
    local d="$SRC_DIR/bash-${BASH_VER}"
    [[ ! -d "$d" ]] && return
    local marker="$d/.adros_patched"
    [[ -f "$marker" ]] && { step "Bash already patched"; return; }

    patch_config_sub "$d/support/config.sub"

    touch "$marker"
}

msg "Applying AdrOS target patches"
patch_binutils
patch_gcc
patch_newlib
patch_bash

# ---- Install AdrOS-specific sysroot headers ----
# NOTE: We do NOT copy the full ulibc headers here because they conflict
# with Newlib's POSIX-compliant headers (signal.h, errno.h, stdio.h, etc.).
# ulibc headers are for AdrOS userland built with -nostdlib; the cross-
# toolchain uses Newlib headers instead.  Only truly AdrOS-specific headers
# (syscall numbers, ioctl defs) are installed.
msg "Installing AdrOS-specific sysroot headers"
mkdir -p "${SYSROOT}/include/sys"
for h in syscall.h; do
    if [[ -f "$ADROS_ROOT/user/ulibc/include/$h" ]]; then
        cp "$ADROS_ROOT/user/ulibc/include/$h" "${SYSROOT}/include/$h"
        step "Installed $h"
    fi
done

# Install Linux/POSIX compatibility headers from newlib/sysroot_headers/.
# These provide stubs for headers that newlib doesn't supply but that
# ported software (Bash, Busybox) expects: asm/*, linux/*, net/*,
# netinet/*, sys/socket.h, poll.h, mntent.h, etc.
COMPAT_HEADERS="$ADROS_ROOT/newlib/sysroot_headers"
if [[ -d "$COMPAT_HEADERS" ]]; then
    cp -r "$COMPAT_HEADERS"/* "${SYSROOT}/include/"
    step "Installed $(find "$COMPAT_HEADERS" -type f | wc -l) sysroot compat headers"
fi

# Patch newlib headers that need small AdrOS-specific additions.
# sys/stat.h — expose lstat()/mknod() for __adros__ (newlib guards them)
if ! grep -q '__adros__' "${SYSROOT}/include/sys/stat.h" 2>/dev/null; then
    sed -i 's/defined(__SPU__) || defined(__rtems__) || defined(__CYGWIN__)/defined(__SPU__) || defined(__rtems__) || defined(__CYGWIN__) || defined(__adros__)/' \
        "${SYSROOT}/include/sys/stat.h"
    step "Patched sys/stat.h (lstat/mknod for __adros__)"
fi

# sys/signal.h — add SA_RESTART and friends to the non-rtems block
if ! grep -q 'SA_RESTART' "${SYSROOT}/include/sys/signal.h" 2>/dev/null; then
    sed -i '/^#define SA_NOCLDSTOP 1/a\
#define SA_RESTART   0x10000000\
#define SA_NODEFER   0x40000000\
#define SA_RESETHAND 0x80000000\
#define SA_NOCLDWAIT 0x20000000\
#define SA_SIGINFO   0x2' \
        "${SYSROOT}/include/sys/signal.h"
    step "Patched sys/signal.h (SA_RESTART etc.)"
fi

# sys/wait.h — add WCOREDUMP macro
if ! grep -q 'WCOREDUMP' "${SYSROOT}/include/sys/wait.h" 2>/dev/null; then
    sed -i '/#define WTERMSIG/a\
#define WCOREDUMP(w) ((w) \& 0x80)' \
        "${SYSROOT}/include/sys/wait.h"
    step "Patched sys/wait.h (WCOREDUMP)"
fi

# glob.h — add GLOB_NOMATCH
if ! grep -q 'GLOB_NOMATCH' "${SYSROOT}/include/glob.h" 2>/dev/null; then
    sed -i '/#define.*GLOB_ABEND/a\
#define GLOB_NOMATCH    (-3)    /* No match found. */' \
        "${SYSROOT}/include/glob.h"
    step "Patched glob.h (GLOB_NOMATCH)"
fi

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
# STEP 3b: Build libgloss/adros (crt0.o + libadros.a)
# ==================================================================
msg "Building libgloss/adros (crt0.o + libadros.a)"
if [[ ! -f "${SYSROOT}/lib/libadros.a" ]]; then
    local_gloss="$SRC_DIR/newlib-${NEWLIB_VER}/libgloss/adros"
    ${TARGET}-gcc -Os -c "$local_gloss/crt0.S"          -o /tmp/adros_crt0.o
    ${TARGET}-gcc -Os -c "$local_gloss/syscalls.c"      -o /tmp/adros_syscalls.o
    ${TARGET}-gcc -Os -c "$local_gloss/posix_stubs.c"   -o /tmp/adros_posix_stubs.o
    ${TARGET}-gcc -Os -c "$local_gloss/posix_compat.c"  -o /tmp/adros_posix_compat.o
    ${TARGET}-ar rcs /tmp/libadros.a /tmp/adros_syscalls.o /tmp/adros_posix_stubs.o /tmp/adros_posix_compat.o
    cp /tmp/adros_crt0.o   "${SYSROOT}/lib/crt0.o"
    cp /tmp/libadros.a     "${SYSROOT}/lib/libadros.a"
    rm -f /tmp/adros_crt0.o /tmp/adros_syscalls.o /tmp/adros_posix_stubs.o /tmp/adros_posix_compat.o /tmp/libadros.a
    step "crt0.o + libadros.a installed to sysroot"
else
    step "libadros.a already installed"
fi

# ==================================================================
# STEP 3c: Create GCC specs file (fix LIB_SPEC link order)
# ==================================================================
SPECS_FILE="$PREFIX/lib/gcc/${TARGET}/${GCC_VER}/specs"
if [[ ! -f "$SPECS_FILE" ]]; then
    ${TARGET}-gcc -dumpspecs > "$SPECS_FILE"
    sed -i 's/-lc -ladros -lgcc/--start-group -lc -ladros --end-group -lgcc/' "$SPECS_FILE"
    step "Created specs file with corrected LIB_SPEC"
else
    step "Specs file already exists"
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

    make -j"$JOBS" all-gcc all-target-libgcc \
        2>&1 | tee "$LOG_DIR/gcc-full-build.log"
    make install-gcc install-target-libgcc \
        2>&1 | tee "$LOG_DIR/gcc-full-install.log"
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
        # Comprehensive cross-compilation cache for Bash on AdrOS/newlib.
        # Cross-compile configure tests can't run programs, so we must
        # pre-seed results for functions/headers that newlib provides.
        cat > config.cache <<'CACHE_EOF'
# --- Types ---
ac_cv_type_getgroups=gid_t
ac_cv_type_sigset_t=yes
ac_cv_type_sig_atomic_t=yes
ac_cv_type_clock_t=yes
ac_cv_c_long_double=yes
ac_cv_sizeof_int=4
ac_cv_sizeof_long=4
ac_cv_sizeof_char_p=4
ac_cv_sizeof_double=8
ac_cv_sizeof_long_long=8
ac_cv_sizeof_intmax_t=4
ac_cv_sizeof_wchar_t=4

# --- Headers (newlib provides these) ---
ac_cv_header_unistd_h=yes
ac_cv_header_stdlib_h=yes
ac_cv_header_string_h=yes
ac_cv_header_strings_h=yes
ac_cv_header_memory_h=yes
ac_cv_header_locale_h=yes
ac_cv_header_termios_h=yes
ac_cv_header_termio_h=no
ac_cv_header_sys_wait_h=yes
ac_cv_header_sys_select_h=yes
ac_cv_header_sys_file_h=no
ac_cv_header_sys_resource_h=yes
ac_cv_header_sys_param_h=yes
ac_cv_header_sys_socket_h=no
ac_cv_header_sys_ioctl_h=yes
ac_cv_header_sys_mman_h=no
ac_cv_header_sys_pte_h=no
ac_cv_header_sys_ptem_h=no
ac_cv_header_sys_stream_h=no
ac_cv_header_dirent_h=yes
ac_cv_header_dirent_dirent_h=yes
ac_cv_header_grp_h=yes
ac_cv_header_pwd_h=yes
ac_cv_header_regex_h=yes
ac_cv_header_fnmatch_h=yes
ac_cv_header_dlfcn_h=no
ac_cv_header_netdb_h=no
ac_cv_header_netinet_in_h=no
ac_cv_header_arpa_inet_h=no
ac_cv_header_wctype_h=yes
ac_cv_header_wchar_h=yes
ac_cv_header_langinfo_h=no
ac_cv_header_libintl_h=no
ac_cv_header_stdint_h=yes
ac_cv_header_inttypes_h=yes
ac_cv_header_stdbool_h=yes
ac_cv_header_sys_stat_h=yes
ac_cv_header_sys_types_h=yes
ac_cv_header_fcntl_h=yes
ac_cv_header_signal_h=yes
ac_cv_header_limits_h=yes

# --- Functions in newlib libc.a ---
ac_cv_func_memmove=yes
ac_cv_func_memset=yes
ac_cv_func_strchr=yes
ac_cv_func_strerror=yes
ac_cv_func_strtol=yes
ac_cv_func_strtoul=yes
ac_cv_func_strtod=yes
ac_cv_func_strtoimax=yes
ac_cv_func_strtoumax=yes
ac_cv_func_snprintf=yes
ac_cv_func_vsnprintf=yes
ac_cv_func_setlocale=yes
ac_cv_func_putenv=yes
ac_cv_func_setenv=yes
ac_cv_func_mkstemp=yes
ac_cv_func_rename=yes
ac_cv_func_mbrtowc=yes
ac_cv_func_wcrtomb=yes
ac_cv_func_wctomb=yes
ac_cv_func_mbrlen=yes
ac_cv_func_regcomp=yes
ac_cv_func_regexec=yes
ac_cv_func_fnmatch=yes
ac_cv_func_strsignal=yes
ac_cv_func_raise=yes
ac_cv_func_getopt=no

# --- Functions provided by libadros.a stubs ---
ac_cv_func_dup2=yes
ac_cv_func_fcntl=yes
ac_cv_func_getcwd=yes
ac_cv_func_pipe=yes
ac_cv_func_select=yes
ac_cv_func_pselect=yes
ac_cv_func_chown=yes
ac_cv_func_lstat=yes
ac_cv_func_readlink=no
ac_cv_func_killpg=yes
ac_cv_func_tcgetattr=yes
ac_cv_func_tcsetattr=yes
ac_cv_func_tcgetpgrp=yes
ac_cv_func_tcsetpgrp=yes
ac_cv_func_tcsendbreak=no
ac_cv_func_cfgetospeed=no
ac_cv_func_sigaction=yes
ac_cv_func_sigprocmask=yes
ac_cv_func_siginterrupt=no
ac_cv_func_waitpid=yes
ac_cv_func_gethostname=yes
ac_cv_func_getpwnam=yes
ac_cv_func_getpwuid=yes
ac_cv_func_getpwent=no
ac_cv_func_getgroups=no
ac_cv_func_getrlimit=no
ac_cv_func_setrlimit=no
ac_cv_func_sysconf=no
ac_cv_func_pathconf=no
ac_cv_func_getpagesize=no
ac_cv_func_getdtablesize=no
ac_cv_func_mkfifo=yes
ac_cv_func_opendir=yes
ac_cv_func_readdir=yes
ac_cv_func_closedir=yes
ac_cv_func_mmap_fixed_mapped=no
ac_cv_func_setvbuf_reversed=no
ac_cv_func_strcoll_works=yes
ac_cv_func_working_mktime=yes
ac_cv_func_getenv=yes
ac_cv_func_setpgid=yes
ac_cv_func_setsid=yes
ac_cv_func_getpgrp=yes
ac_cv_func_setpgrp=no
ac_cv_func_getpeername=no
ac_cv_func_gethostbyname=no
ac_cv_func_getaddrinfo=no
ac_cv_func_getservbyname=no
ac_cv_func_getservent=no
ac_cv_func_inet_aton=no
ac_cv_func_dlopen=no
ac_cv_func_dlclose=no
ac_cv_func_dlsym=no
ac_cv_func_confstr=no
ac_cv_func_eaccess=no
ac_cv_func_faccessat=no
ac_cv_func_arc4random=no
ac_cv_func_getrandom=no
ac_cv_func_getentropy=no
ac_cv_func_iconv=no
ac_cv_func_getwd=no
ac_cv_func_doprnt=no
ac_cv_func_mbscasecmp=no
ac_cv_func_mbschr=no
ac_cv_func_mbscmp=no
ac_cv_func_setdtablesize=no
ac_cv_func_getrusage=no
ac_cv_func_locale_charset=no

# --- Bash-specific cross-compile overrides ---
ac_cv_rl_version=8.2
bash_cv_func_sigsetjmp=missing
bash_cv_func_ctype_nonascii=no
bash_cv_func_snprintf=yes
bash_cv_func_vsnprintf=yes
bash_cv_printf_a_format=no
bash_cv_must_reinstall_sighandlers=no
bash_cv_pgrp_pipe=no
bash_cv_sys_named_pipes=missing
bash_cv_job_control_missing=present
bash_cv_sys_siglist=yes
bash_cv_under_sys_siglist=yes
bash_cv_opendir_not_robust=no
bash_cv_ulimit_maxfds=no
bash_cv_getenv_redef=yes
bash_cv_getcwd_malloc=yes
bash_cv_type_rlimit=long
bash_cv_type_intmax_t=int
bash_cv_type_uintmax_t=unsigned
bash_cv_posix_signals=yes
bash_cv_bsd_signals=no
bash_cv_sysv_signals=no
bash_cv_speed_t_in_sys_types=no
bash_cv_struct_winsize_header=other
bash_cv_struct_winsize_in_ioctl=yes
bash_cv_tiocstat_in_ioctl=no
bash_cv_tiocgwinsz_in_ioctl=yes
bash_cv_unusedvar=yes
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
            CFLAGS="-Os -static -D_POSIX_VERSION=200112L" \
            LDFLAGS="-static -Wl,--allow-multiple-definition" \
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
