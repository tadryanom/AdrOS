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
# AdrOS — Bash cross-compile script
#
# Cross-compiles GNU Bash for AdrOS using the i686-adros toolchain.
#
# Prerequisites:
#   - AdrOS toolchain built (toolchain/build.sh)
#   - PATH includes /opt/adros-toolchain/bin
#
# Usage:
#   ./ports/bash/build.sh [--prefix /opt/adros-toolchain] [--jobs 4]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ADROS_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# ---- Defaults ----
PREFIX="/opt/adros-toolchain"
TARGET="i686-adros"
JOBS="$(nproc 2>/dev/null || echo 4)"
BASH_VER="5.2.21"
BASH_URL="https://ftp.gnu.org/gnu/bash/bash-${BASH_VER}.tar.gz"

SRC_DIR="$ADROS_ROOT/ports/bash/src"
BUILD_DIR="$ADROS_ROOT/ports/bash/build"
LOG_DIR="$ADROS_ROOT/ports/bash/logs"

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
NEWLIB_SPECS="$PREFIX/lib/gcc/${TARGET}/13.2.0/newlib.specs"

export PATH="$PREFIX/bin:$PATH"

msg() { echo -e "\n\033[1;34m==> $1\033[0m"; }
step() { echo "  [OK] $1"; }
die() { echo -e "\033[1;31mERROR: $1\033[0m" >&2; exit 1; }

# Verify toolchain
command -v "${TARGET}-gcc" >/dev/null 2>&1 || die "Toolchain not found. Run toolchain/build.sh first."

# Verify newlib.specs exists (bash is built with newlib for static linking)
if [[ ! -f "$NEWLIB_SPECS" ]]; then
    die "newlib.specs not found at $NEWLIB_SPECS. Run toolchain/build.sh first."
fi

# ---- Download ----
mkdir -p "$SRC_DIR" "$BUILD_DIR" "$LOG_DIR"

TARBALL="$SRC_DIR/bash-${BASH_VER}.tar.gz"
if [[ ! -d "$SRC_DIR/bash-${BASH_VER}" ]]; then
    msg "Downloading Bash ${BASH_VER}..."
    if [[ ! -f "$TARBALL" ]]; then
        wget -q -O "$TARBALL" "$BASH_URL" || die "Download failed"
    fi
    tar xzf "$TARBALL" -C "$SRC_DIR"
    step "Bash source extracted"
fi

# ---- Patch config.sub for AdrOS target ----
patch_config_sub() {
    local f="$1"
    if ! grep -q 'adros' "$f"; then
        # 1) Add adros case before the pikeos case in the canonicalisation switch
        sed -i '/^[[:space:]]*pikeos\*)/i\
\tadros*)\
\t\tos=adros\
\t\t;;' "$f"
        # 2) Add adros* to the validation list (same line as dicos*)
        sed -i 's/| dicos\*/| dicos* | adros*/' "$f"
        step "Patched $(basename "$(dirname "$f")")/config.sub"
    fi
}

msg "Patching Bash for AdrOS target"
BASH_SRC="$SRC_DIR/bash-${BASH_VER}"
MARKER="$BASH_SRC/.adros_patched"
if [[ ! -f "$MARKER" ]]; then
    patch_config_sub "$BASH_SRC/support/config.sub"
    touch "$MARKER"
else
    step "Bash already patched"
fi

# ---- Build ----
msg "Cross-compiling Bash ${BASH_VER}"
cd "$BUILD_DIR"

if [[ ! -f "$BUILD_DIR/bash" ]]; then
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

    "$BASH_SRC/configure" \
        --host="$TARGET" \
        --prefix=/usr \
        --without-bash-malloc \
        --disable-nls \
        --cache-file=config.cache \
        CC="${TARGET}-gcc -specs=$NEWLIB_SPECS" \
        AR="${TARGET}-ar" \
        RANLIB="${TARGET}-ranlib" \
        CFLAGS="-Os -D_POSIX_VERSION=200112L" \
        LDFLAGS="-Wl,--allow-multiple-definition" \
        2>&1 | tee "$LOG_DIR/bash-configure.log"

    make -j"$JOBS" 2>&1 | tee "$LOG_DIR/bash-build.log"
    step "Bash built: $BUILD_DIR/bash"
else
    step "Bash already built"
fi

# ---- Install to initrd staging area ----
msg "Installing Bash to AdrOS filesystem..."
if [[ -d "$ADROS_ROOT/iso/boot" ]]; then
    mkdir -p "$ADROS_ROOT/iso/bin"
    cp "$BUILD_DIR/bash" "$ADROS_ROOT/iso/bin/bash"
    step "Bash installed to $ADROS_ROOT/iso/bin/bash"
fi

# Also copy to rootfs if it exists
if [[ -d "$ADROS_ROOT/rootfs/bin" ]]; then
    cp "$BUILD_DIR/bash" "$ADROS_ROOT/rootfs/bin/bash"
    step "Bash installed to $ADROS_ROOT/rootfs/bin/bash"
fi

# ---- Summary ----
echo ""
echo "Bash build complete!"
echo ""
echo "  Binary:   $BUILD_DIR/bash"
echo ""
echo "  To add to AdrOS initrd:"
echo "    cp $BUILD_DIR/bash rootfs/bin/"
echo ""
