#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://github.com/tadryanom/AdrOS
#

#
# AdrOS Toolchain Builder
#
# Builds a complete i686-adros cross-compilation toolchain:
#   1. Binutils  (assembler, linker)
#   2. GCC       (bootstrap, C only, no libc headers)
#   3. Newlib    (C library)
#   4. GCC       (full rebuild with Newlib sysroot)
#
# Usage:
#   ./toolchain/build.sh [--prefix /opt/adros] [--jobs 4]
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

# ---- Versions ----
BINUTILS_VER="2.42"
GCC_VER="13.2.0"
NEWLIB_VER="4.4.0.20231231"

BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VER}.tar.xz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.xz"
NEWLIB_URL="https://sourceware.org/pub/newlib/newlib-${NEWLIB_VER}.tar.gz"

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

msg "Extracting sources"
extract "$SRC_DIR/binutils-${BINUTILS_VER}.tar.xz" "$SRC_DIR/binutils-${BINUTILS_VER}"
extract "$SRC_DIR/gcc-${GCC_VER}.tar.xz"           "$SRC_DIR/gcc-${GCC_VER}"
extract "$SRC_DIR/newlib-${NEWLIB_VER}.tar.gz"      "$SRC_DIR/newlib-${NEWLIB_VER}"

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

    # gas/config/tc-i386.c — fix type mismatch (uint32_t vs unsigned int)
    # The header declares uint32_t but the source used unsigned int.
    if grep -q 'x86_scfi_callee_saved_p (unsigned int' "$d/gas/config/tc-i386.c" 2>/dev/null; then
        sed -i 's/x86_scfi_callee_saved_p (unsigned int dw2reg_num)/x86_scfi_callee_saved_p (uint32_t dw2reg_num)/' \
            "$d/gas/config/tc-i386.c"
        step "Patched gas/config/tc-i386.c (uint32_t fix)"
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
#define LINK_SPEC \
  "-m elf_i386 -z noexecstack " \
  "%{shared:-shared} " \
  "%{!shared:-Ttext-segment=0x00400000} " \
  "%{static:-static} " \
  "%{rdynamic:-export-dynamic}"

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
\ttmake_file="$tmake_file i386/t-crtstuff i386/t-adros t-crtstuff-pic t-libgcc-pic"\
\textra_parts="$extra_parts crti.o crtn.o crtbegin.o crtend.o"\
\t;;' "$d/libgcc/config.host"
        step "Patched libgcc/config.host"
    fi

    # GCC prerequisites (GMP, MPFR, MPC, ISL) — patch config.sub for adros
    # These are only present if contrib/download_prerequisites was run.
    for sub in \
        "$d/gmp/configfsf.sub" \
        "$d/mpfr-*/config.sub" \
        "$d/mpc-*/build-aux/config.sub" \
        "$d/isl-*/config.sub"; do
        # Expand glob
        for f in $sub; do
            [[ -f "$f" ]] || continue
            if ! grep -q 'adros' "$f"; then
                # GMP uses configfsf.sub with a different format
                if [[ "$f" == *"configfsf.sub" ]]; then
                    sed -i 's/| nsk\* | powerunix\* | genode\* | zvmoe\* | qnx\* | emx\* \\$/\| nsk* | powerunix* | genode* | zvmoe* | qnx* | emx* \\\n\t     | adros*)/' "$f"
                else
                    sed -i 's/| -midnightbsd\*)/| -midnightbsd* | -adros*)/' "$f"
                    sed -i 's/| nsk\* | powerunix\*)/| nsk* | powerunix* | adros*)/' "$f"
                fi
                step "Patched $(echo "$f" | sed "s|$d/||") for adros"
            fi
        done
    done

    # NOTE: libcody is kept in host_libs — it is required by cc1plus (C++
    # frontend) which is built in step 4.  Only a Canadian cross (host!=build)
    # would have trouble, but our normal cross build has host==build.

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

	.section .note.GNU-stack,"",@progbits
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

    # t-adros — override CRT build rules to use AdrOS-specific sources
    if [[ ! -f "$d/libgcc/config/i386/t-adros" ]]; then
        cat > "$d/libgcc/config/i386/t-adros" <<'EOF'
# Build AdrOS CRT files from custom sources (with .note.GNU-stack)
crti.o: $(srcdir)/config/i386/crti-adros.S
	$(gcc_compile) -c $<

crtn.o: $(srcdir)/config/i386/crtn-adros.S
	$(gcc_compile) -c $<
EOF
        step "Created libgcc/config/i386/t-adros"
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

    # Create libgloss/adros directory and autoconf files if not present
    mkdir -p "$d/libgloss/adros"
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


msg "Applying AdrOS target patches"
patch_binutils
patch_gcc
patch_newlib

# Always sync libgloss/adros source files (even if patches are already applied)
# so that edits to our stubs are picked up on rebuild.
_gloss_dir="$SRC_DIR/newlib-${NEWLIB_VER}/libgloss/adros"
mkdir -p "$_gloss_dir"
cp -f "$ADROS_ROOT/newlib/libgloss/adros/"*.c "$ADROS_ROOT/newlib/libgloss/adros/"*.S "$_gloss_dir/" 2>/dev/null || true
step "Synced libgloss/adros/ stubs (forced)"

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

if [[ ! -f "$BUILD_DIR/newlib/.built" ]]; then
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
    touch "$BUILD_DIR/newlib/.built"
    step "Newlib installed to sysroot"
else
    step "Newlib already installed"
fi

# ---- Install AdrOS-specific sysroot headers ----
# NOTE: These MUST be installed AFTER Newlib's make install, because Newlib
# overwrites headers (e.g. sys/dirent.h) that we need to replace with
# AdrOS-specific versions.  On the first build the overwrite matters;
# on subsequent builds Newlib install is skipped but we re-install anyway.
#
# We do NOT copy the full ulibc headers here because they conflict with
# Newlib's POSIX-compliant headers (signal.h, errno.h, stdio.h, etc.).
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

# ---- Patch Newlib headers that need small AdrOS-specific additions ----
# These must run AFTER Newlib install populates ${SYSROOT}/include/.
msg "Patching Newlib sysroot headers for AdrOS"

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

    make -j"$JOBS" all-libcody all-gcc all-target-libgcc \
        2>&1 | tee "$LOG_DIR/gcc-full-build.log"
    make install-gcc install-target-libgcc \
        2>&1 | tee "$LOG_DIR/gcc-full-install.log"
    touch "$BUILD_DIR/gcc-full/.built"
    step "Full GCC installed: ${PREFIX}/bin/${TARGET}-g++"
else
    step "Full GCC already installed"
fi

# ==================================================================
# STEP 4b: Create GCC specs file (Newlib-safe defaults)
# ==================================================================
# Must run AFTER step 4 — make install-gcc overwrites the GCC lib directory.
# Generates a specs file matching adros.h (Newlib-compatible):
#   LIB_SPEC:  --start-group -lc -ladros --end-group -lgcc
#   LINK_SPEC: noexecstack, base 0x00400000, static-friendly (no dynamic-linker)
#
# If ulibc is available (step 4c), the specs will be re-patched for ulibc.
SPECS_FILE="$PREFIX/lib/gcc/${TARGET}/${GCC_VER}/specs"
${TARGET}-gcc -dumpspecs > "$SPECS_FILE"
# Ensure LINK_SPEC has noexecstack and AdrOS base address
if ! grep -q 'noexecstack' "$SPECS_FILE"; then
    sed -i '/^\*link:/{n;s|.*|-m elf_i386 -z noexecstack %{shared:-shared} %{!shared:-Ttext-segment=0x00400000} %{static:-static} %{rdynamic:-export-dynamic}|}' "$SPECS_FILE"
fi
step "Created specs file (Newlib defaults)"

# ==================================================================
# STEP 4c: Install ulibc runtime to sysroot
# ==================================================================
# The toolchain uses ulibc as its C library (replacing Newlib for
# user-facing builds).  ulibc provides both static (libc.a) and
# shared (libc.so) libraries plus the dynamic linker (ld.so).
#
# ulibc is built by 'make iso' via i686-elf-gcc.  We copy the
# pre-built artifacts into the sysroot.  If they don't exist yet,
# the user must run 'make ARCH=x86 iso' first.
msg "Installing ulibc runtime to sysroot"
ULIBC_BUILDDIR="${ADROS_ROOT}/build/x86/user/ulibc"
LDSO_BUILDDIR="${ADROS_ROOT}/build/x86/user/cmds/ldso"
ULIBC_INCDIR="${ADROS_ROOT}/user/ulibc/include"
mkdir -p "${SYSROOT}/lib"

_ulibc_ok=1
for _f in "$ULIBC_BUILDDIR/crt0.o" "$ULIBC_BUILDDIR/libulibc.a" \
          "$ULIBC_BUILDDIR/libc.so" "$LDSO_BUILDDIR/ld.so"; do
    if [[ ! -f "$_f" ]]; then
        step "WARNING: $_f not found — run 'make ARCH=x86 iso' first"
        _ulibc_ok=0
    fi
done

# Always back up Newlib headers so newlib.specs can find them
if [[ ! -d "${SYSROOT}/include/newlib" ]]; then
    msg "Backing up Newlib headers to include/newlib/"
    mkdir -p "${SYSROOT}/include/newlib"
    cp -r "${SYSROOT}/include"/* "${SYSROOT}/include/newlib/" 2>/dev/null || true
    rm -rf "${SYSROOT}/include/newlib/newlib"  # no recursion
fi

# Always back up Newlib libs so newlib.specs can find them
[[ -f "${SYSROOT}/lib/libc.a" && ! -f "${SYSROOT}/lib/libnewlib.a" ]] && \
    cp "${SYSROOT}/lib/libc.a" "${SYSROOT}/lib/libnewlib.a"
[[ -f "${SYSROOT}/lib/crt0.o" && ! -f "${SYSROOT}/lib/crt0-newlib.o" ]] && \
    cp "${SYSROOT}/lib/crt0.o" "${SYSROOT}/lib/crt0-newlib.o"

if [[ $_ulibc_ok -eq 1 ]]; then
    # Replace Newlib with ulibc as the default libc
    cp "$ULIBC_BUILDDIR/crt0.o"     "${SYSROOT}/lib/crt0.o"
    cp "$ULIBC_BUILDDIR/libulibc.a" "${SYSROOT}/lib/libc.a"
    cp "$ULIBC_BUILDDIR/libc.so"    "${SYSROOT}/lib/libc.so"
    cp "$LDSO_BUILDDIR/ld.so"       "${SYSROOT}/lib/ld.so"

    # Install ulibc headers (overwrite Newlib headers for standard names)
    cp -r "$ULIBC_INCDIR"/* "${SYSROOT}/include/"

    # Re-patch specs file for ulibc: remove -ladros, add dynamic-linker
    sed -i 's/--start-group -lc -ladros --end-group -lgcc/-lc -lgcc/' "$SPECS_FILE"
    sed -i 's/-lc -ladros -lgcc/-lc -lgcc/' "$SPECS_FILE"
    sed -i '/^\*link:/{n;s|.*|-m elf_i386 -z noexecstack %{shared:-shared} %{!shared:-Ttext-segment=0x00400000} %{static:-static} %{!static:%{!shared:-dynamic-linker /lib/ld.so}} %{rdynamic:-export-dynamic}|}' "$SPECS_FILE"

    step "ulibc installed — dynamic linking by default, static with -static"
else
    step "ulibc NOT installed — toolchain will use Newlib (static only)"
fi

# ==================================================================
# STEP 4d: Create newlib.specs for optional Newlib builds
# ==================================================================
# Users can compile with Newlib instead of ulibc:
#   i686-adros-gcc -specs=<path>/newlib.specs -o out in.c
#   i686-adros-gcc-newlib -o out in.c   (wrapper script)
#
# Newlib mode is always static (Newlib has no shared library).
NEWLIB_SPECS="$PREFIX/lib/gcc/${TARGET}/${GCC_VER}/newlib.specs"
cat > "$NEWLIB_SPECS" <<EOFSPECS
%rename cpp old_cpp
%rename link old_link

*cpp:
-nostdinc -isystem ${SYSROOT}/include/newlib -isystem $PREFIX/lib/gcc/${TARGET}/${GCC_VER}/include %(old_cpp)

*startfile:
crt0-newlib.o%s crti.o%s crtbegin.o%s

*lib:
--start-group -lnewlib -ladros --end-group -lgcc

*link:
-m elf_i386 -z noexecstack %{shared:-shared} %{!shared:-Ttext-segment=0x00400000} -static %{rdynamic:-export-dynamic}
EOFSPECS

# Create convenience wrapper script
cat > "$PREFIX/bin/${TARGET}-gcc-newlib" <<EOFWRAP
#!/bin/sh
# Wrapper: compile with Newlib instead of ulibc (static only)
exec i686-adros-gcc -specs="${PREFIX}/lib/gcc/${TARGET}/${GCC_VER}/newlib.specs" "\$@"
EOFWRAP
chmod +x "$PREFIX/bin/${TARGET}-gcc-newlib"
step "Created newlib.specs + i686-adros-gcc-newlib wrapper"

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
echo "    ${TARGET}-gcc        — C compiler (ulibc, dynamic by default)"
echo "    ${TARGET}-gcc-newlib — C compiler (Newlib, static only)"
echo "    ${TARGET}-g++        — C++ compiler"
echo "    ${TARGET}-ld         — Linker"
echo "    ${TARGET}-as         — Assembler"
echo "    ${TARGET}-ar         — Archiver"
echo "    ${TARGET}-objdump    — Disassembler"
echo ""
echo "  Usage:"
echo "    export PATH=${PREFIX}/bin:\$PATH"
echo "    ${TARGET}-gcc -o hello hello.c          # dynamic (ulibc)"
echo "    ${TARGET}-gcc -static -o hello hello.c  # static  (ulibc)"
echo "    ${TARGET}-gcc-newlib -o hello hello.c   # static  (Newlib)"
echo ""
