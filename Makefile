# AdrOS Makefile
# Usage: make ARCH=x86 (default) | arm | riscv | mips

ARCH ?= x86
KERNEL_NAME := adros-$(ARCH).bin

# Directories
SRC_DIR := src
BUILD_DIR := build/$(ARCH)

# Minimal kernel sources shared across all architectures
KERNEL_COMMON := main.c console.c utils.c cmdline.c driver.c cpu_features.c
C_SOURCES := $(addprefix $(SRC_DIR)/kernel/,$(KERNEL_COMMON))

# HAL sources (architecture-specific)
C_SOURCES += $(wildcard $(SRC_DIR)/hal/$(ARCH)/*.c)

# --- x86 Configuration ---
ifeq ($(ARCH),x86)
    # x86 gets ALL kernel sources, drivers, and mm
    C_SOURCES := $(wildcard $(SRC_DIR)/kernel/*.c)
    C_SOURCES += $(wildcard $(SRC_DIR)/drivers/*.c)
    C_SOURCES += $(wildcard $(SRC_DIR)/mm/*.c)
    C_SOURCES += $(wildcard $(SRC_DIR)/hal/$(ARCH)/*.c)

    # Default Toolchain Prefix (can be overridden)
    ifdef CROSS
        TOOLPREFIX ?= i686-elf-
    endif

    # Toolchain tools (Allow user override via make CC=...)
    CC ?= $(TOOLPREFIX)gcc
    AS ?= $(TOOLPREFIX)as
    LD ?= $(TOOLPREFIX)ld
    
    # lwIP sources (NO_SYS=0, IPv4, threaded API + sockets)
    LWIPDIR := third_party/lwip/src
    LWIP_CORE := $(LWIPDIR)/core/init.c $(LWIPDIR)/core/def.c $(LWIPDIR)/core/inet_chksum.c \
        $(LWIPDIR)/core/ip.c $(LWIPDIR)/core/mem.c $(LWIPDIR)/core/memp.c \
        $(LWIPDIR)/core/netif.c $(LWIPDIR)/core/pbuf.c $(LWIPDIR)/core/raw.c \
        $(LWIPDIR)/core/stats.c $(LWIPDIR)/core/sys.c $(LWIPDIR)/core/tcp.c \
        $(LWIPDIR)/core/tcp_in.c $(LWIPDIR)/core/tcp_out.c $(LWIPDIR)/core/timeouts.c \
        $(LWIPDIR)/core/udp.c $(LWIPDIR)/core/dns.c
    LWIP_IPV4 := $(LWIPDIR)/core/ipv4/etharp.c $(LWIPDIR)/core/ipv4/icmp.c \
        $(LWIPDIR)/core/ipv4/ip4.c $(LWIPDIR)/core/ipv4/ip4_addr.c \
        $(LWIPDIR)/core/ipv4/ip4_frag.c $(LWIPDIR)/core/ipv4/dhcp.c \
        $(LWIPDIR)/core/ipv4/acd.c
    LWIP_IPV6 := $(LWIPDIR)/core/ipv6/ethip6.c $(LWIPDIR)/core/ipv6/icmp6.c \
        $(LWIPDIR)/core/ipv6/inet6.c $(LWIPDIR)/core/ipv6/ip6.c \
        $(LWIPDIR)/core/ipv6/ip6_addr.c $(LWIPDIR)/core/ipv6/ip6_frag.c \
        $(LWIPDIR)/core/ipv6/mld6.c $(LWIPDIR)/core/ipv6/nd6.c
    LWIP_NETIF := $(LWIPDIR)/netif/ethernet.c
    LWIP_API := $(LWIPDIR)/api/api_lib.c $(LWIPDIR)/api/api_msg.c \
        $(LWIPDIR)/api/err.c $(LWIPDIR)/api/if_api.c $(LWIPDIR)/api/netbuf.c \
        $(LWIPDIR)/api/netifapi.c $(LWIPDIR)/api/tcpip.c
    LWIP_SOURCES := $(LWIP_CORE) $(LWIP_IPV4) $(LWIP_IPV6) $(LWIP_NETIF) $(LWIP_API)
    NET_SOURCES := $(wildcard $(SRC_DIR)/net/*.c) $(wildcard $(SRC_DIR)/net/lwip_port/*.c)
    C_SOURCES += $(NET_SOURCES)

    # Mandatory Architecture Flags
    ARCH_CFLAGS := -m32 -ffreestanding -fno-builtin -U_FORTIFY_SOURCE -mno-sse -mno-mmx -Iinclude -Iinclude/net -Ithird_party/lwip/src/include
    ARCH_LDFLAGS := -m elf_i386 -T $(SRC_DIR)/arch/x86/linker.ld
    ARCH_ASFLAGS := --32

    # Default User Flags (Allow override via make CFLAGS=...)
    CFLAGS ?= -O2 -Wall -Wextra -Werror -Wno-error=cpp
    
    # Merge Flags
    CFLAGS := $(ARCH_CFLAGS) $(CFLAGS)
    LDFLAGS := $(ARCH_LDFLAGS) $(LDFLAGS)
    ASFLAGS := $(ARCH_ASFLAGS) $(ASFLAGS)

    ASM_SOURCES := $(wildcard $(SRC_DIR)/arch/x86/*.S)
    C_SOURCES += $(wildcard $(SRC_DIR)/arch/x86/*.c)

    FULLTEST_ELF := user/fulltest.elf
    ECHO_ELF := user/echo.elf
    SH_ELF := user/sh.elf
    CAT_ELF := user/cat.elf
    LS_ELF := user/ls.elf
    MKDIR_ELF := user/mkdir.elf
    RM_ELF := user/rm.elf
    CP_ELF := user/cp.elf
    MV_ELF := user/mv.elf
    TOUCH_ELF := user/touch.elf
    LN_ELF := user/ln.elf
    HEAD_ELF := user/head.elf
    TAIL_ELF := user/tail.elf
    WC_ELF := user/wc.elf
    SORT_ELF := user/sort.elf
    UNIQ_ELF := user/uniq.elf
    CUT_ELF := user/cut.elf
    CHMOD_ELF := user/chmod.elf
    CHOWN_ELF := user/chown.elf
    CHGRP_ELF := user/chgrp.elf
    DATE_ELF := user/date.elf
    HOSTNAME_ELF := user/hostname.elf
    UPTIME_ELF := user/uptime.elf
    MOUNT_ELF := user/mount.elf
    UMOUNT_ELF := user/umount.elf
    ENV_ELF := user/env.elf
    KILL_ELF := user/kill.elf
    SLEEP_ELF := user/sleep.elf
    CLEAR_ELF := user/clear.elf
    PS_ELF := user/ps.elf
    DF_ELF := user/df.elf
    FREE_ELF := user/free.elf
    TEE_ELF := user/tee.elf
    BASENAME_ELF := user/basename.elf
    DIRNAME_ELF := user/dirname.elf
    RMDIR_ELF := user/rmdir.elf
    GREP_ELF := user/grep.elf
    ID_ELF := user/id.elf
    UNAME_ELF := user/uname.elf
    DMESG_ELF := user/dmesg.elf
    PRINTENV_ELF := user/printenv.elf
    TR_ELF := user/tr.elf
    DD_ELF := user/dd.elf
    PWD_ELF := user/pwd.elf
    STAT_ELF := user/stat.elf
    INIT_ELF := user/init.elf
    LDSO_ELF := user/ld.so
    ULIBC_SO := user/ulibc/libc.so
    PIE_SO := user/libpietest.so
    PIE_ELF := user/pie_test.elf
    DOOM_ELF := user/doom/doom.elf
    INITRD_IMG := initrd.img
    MKINITRD := tools/mkinitrd
endif

# --- ARM64 Configuration ---
ifeq ($(ARCH),arm)
    CC := aarch64-linux-gnu-gcc
    AS := aarch64-linux-gnu-as
    LD := aarch64-linux-gnu-ld
    OBJCOPY := aarch64-linux-gnu-objcopy
    CFLAGS := -ffreestanding -O2 -Wall -Wextra -Werror -Wno-error=cpp -mno-outline-atomics -Iinclude
    LDFLAGS := -T $(SRC_DIR)/arch/arm/linker.ld
    ASFLAGS :=
    ASM_SOURCES := $(wildcard $(SRC_DIR)/arch/arm/*.S)
    C_SOURCES += $(wildcard $(SRC_DIR)/arch/arm/*.c)
endif

# --- RISC-V 64 Configuration ---
ifeq ($(ARCH),riscv)
    CC := riscv64-linux-gnu-gcc
    AS := riscv64-linux-gnu-as
    LD := riscv64-linux-gnu-ld
    OBJCOPY := riscv64-linux-gnu-objcopy
    CFLAGS := -ffreestanding -O2 -Wall -Wextra -Werror -Wno-error=cpp -Iinclude -mcmodel=medany
    LDFLAGS := -T $(SRC_DIR)/arch/riscv/linker.ld
    ASFLAGS :=
    ASM_SOURCES := $(wildcard $(SRC_DIR)/arch/riscv/*.S)
    C_SOURCES += $(wildcard $(SRC_DIR)/arch/riscv/*.c)
endif

# --- MIPS 32 Configuration ---
ifeq ($(ARCH),mips)
    CC := mipsel-linux-gnu-gcc
    AS := mipsel-linux-gnu-as
    LD := mipsel-linux-gnu-ld
    CFLAGS := -ffreestanding -O2 -Wall -Wextra -Werror -Wno-error=cpp -Iinclude -mabi=32 -march=mips32r2 -mno-abicalls -fno-pic -G0
    LDFLAGS := -T $(SRC_DIR)/arch/mips/linker.ld
    ASFLAGS := -march=mips32r2
    ASM_SOURCES := $(wildcard $(SRC_DIR)/arch/mips/*.S)
    C_SOURCES += $(wildcard $(SRC_DIR)/arch/mips/*.c)
endif

# lwIP object files (compiled from third_party, separate pattern)
LWIP_OBJ := $(patsubst %.c, $(BUILD_DIR)/lwip/%.o, $(LWIP_SOURCES))

# Object generation
OBJ := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
OBJ += $(patsubst $(SRC_DIR)/%.S, $(BUILD_DIR)/%.o, $(ASM_SOURCES))
OBJ += $(LWIP_OBJ)

QEMU_DFLAGS :=
ifneq ($(QEMU_DEBUG),)
QEMU_DFLAGS := -d guest_errors,cpu_reset -D qemu.log
endif

ifneq ($(QEMU_INT),)
QEMU_DFLAGS := $(QEMU_DFLAGS) -d int
endif

BOOT_OBJ := $(BUILD_DIR)/arch/$(ARCH)/boot.o
KERNEL_OBJ := $(filter-out $(BOOT_OBJ), $(OBJ))

all: $(KERNEL_NAME)

$(KERNEL_NAME): $(OBJ)
	@echo "  LD      $@"
	@$(LD) $(LDFLAGS) -n -o $@ $(BOOT_OBJ) $(KERNEL_OBJ) $(shell $(CC) $(ARCH_CFLAGS) -print-libgcc-file-name)

iso: $(KERNEL_NAME) $(INITRD_IMG)
	@mkdir -p iso/boot
	@cp -f $(KERNEL_NAME) iso/boot/$(KERNEL_NAME)
	@cp -f $(INITRD_IMG) iso/boot/$(INITRD_IMG)
	@echo "  GRUB-MKRESCUE  adros-$(ARCH).iso"
	@grub-mkrescue -o adros-$(ARCH).iso iso > /dev/null

$(MKINITRD): tools/mkinitrd.c include/xxhash32.h
	@gcc -Iinclude tools/mkinitrd.c -o $(MKINITRD)

ULIBC_DIR := user/ulibc
ULIBC_LIB := $(ULIBC_DIR)/libulibc.a

$(ULIBC_LIB):
	@$(MAKE) -C $(ULIBC_DIR) --no-print-directory

$(ULIBC_SO):
	@$(MAKE) -C $(ULIBC_DIR) libc.so --no-print-directory

$(FULLTEST_ELF): user/fulltest.c user/linker.ld
	@i686-elf-gcc -m32 -I include -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/linker.ld -o $(FULLTEST_ELF) user/fulltest.c user/errno.c

# --- Dynamic linking helper: compile .c to PIC .o, link as PIE with crt0 + libc.so ---
ULIBC_CRT0 := $(ULIBC_DIR)/src/crt0.o
DYN_CC := i686-elf-gcc -m32 -ffreestanding -nostdlib -O2 -Wall -Wextra -fPIC -fno-plt -I$(ULIBC_DIR)/include
DYN_LD := i686-elf-ld -m elf_i386 --dynamic-linker=/lib/ld.so -T user/dyn_linker.ld -L$(ULIBC_DIR) -rpath /lib

$(ECHO_ELF): user/echo.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/echo.c -o user/echo.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/echo.o -lc

$(SH_ELF): user/sh.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/sh.c -o user/sh.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/sh.o -lc

$(CAT_ELF): user/cat.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/cat.c -o user/cat.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/cat.o -lc

$(LS_ELF): user/ls.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/ls.c -o user/ls.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/ls.o -lc

$(MKDIR_ELF): user/mkdir.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/mkdir.c -o user/mkdir.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/mkdir.o -lc

$(RM_ELF): user/rm.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/rm.c -o user/rm.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/rm.o -lc

$(CP_ELF): user/cp.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/cp.c -o user/cp.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/cp.o -lc

$(MV_ELF): user/mv.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/mv.c -o user/mv.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/mv.o -lc

$(TOUCH_ELF): user/touch.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/touch.c -o user/touch.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/touch.o -lc

$(LN_ELF): user/ln.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/ln.c -o user/ln.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/ln.o -lc

$(HEAD_ELF): user/head.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/head.c -o user/head.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/head.o -lc

$(TAIL_ELF): user/tail.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/tail.c -o user/tail.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/tail.o -lc

$(WC_ELF): user/wc.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/wc.c -o user/wc.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/wc.o -lc

$(SORT_ELF): user/sort.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/sort.c -o user/sort.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/sort.o -lc

$(UNIQ_ELF): user/uniq.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/uniq.c -o user/uniq.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/uniq.o -lc

$(CUT_ELF): user/cut.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/cut.c -o user/cut.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/cut.o -lc

$(CHMOD_ELF): user/chmod.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/chmod.c -o user/chmod.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/chmod.o -lc

$(CHOWN_ELF): user/chown.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/chown.c -o user/chown.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/chown.o -lc

$(CHGRP_ELF): user/chgrp.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/chgrp.c -o user/chgrp.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/chgrp.o -lc

$(DATE_ELF): user/date.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/date.c -o user/date.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/date.o -lc

$(HOSTNAME_ELF): user/hostname.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/hostname.c -o user/hostname.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/hostname.o -lc

$(UPTIME_ELF): user/uptime.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/uptime.c -o user/uptime.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/uptime.o -lc

$(INIT_ELF): user/init.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/init.c -o user/init.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/init.o -lc

$(MOUNT_ELF): user/mount.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/mount.c -o user/mount.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/mount.o -lc

$(UMOUNT_ELF): user/umount.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/umount.c -o user/umount.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/umount.o -lc

$(ENV_ELF): user/env.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/env.c -o user/env.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/env.o -lc

$(KILL_ELF): user/kill.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/kill.c -o user/kill.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/kill.o -lc

$(SLEEP_ELF): user/sleep.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/sleep.c -o user/sleep.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/sleep.o -lc

$(CLEAR_ELF): user/clear.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/clear.c -o user/clear.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/clear.o -lc

$(PS_ELF): user/ps.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/ps.c -o user/ps.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/ps.o -lc

$(DF_ELF): user/df.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/df.c -o user/df.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/df.o -lc

$(FREE_ELF): user/free.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/free.c -o user/free.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/free.o -lc

$(TEE_ELF): user/tee.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/tee.c -o user/tee.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/tee.o -lc

$(BASENAME_ELF): user/basename.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/basename.c -o user/basename.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/basename.o -lc

$(DIRNAME_ELF): user/dirname.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/dirname.c -o user/dirname.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/dirname.o -lc

$(RMDIR_ELF): user/rmdir.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/rmdir.c -o user/rmdir.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/rmdir.o -lc

$(GREP_ELF): user/grep.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/grep.c -o user/grep.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/grep.o -lc

$(ID_ELF): user/id.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/id.c -o user/id.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/id.o -lc

$(UNAME_ELF): user/uname.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/uname.c -o user/uname.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/uname.o -lc

$(DMESG_ELF): user/dmesg.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/dmesg.c -o user/dmesg.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/dmesg.o -lc

$(PRINTENV_ELF): user/printenv.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/printenv.c -o user/printenv.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/printenv.o -lc

$(TR_ELF): user/tr.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/tr.c -o user/tr.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/tr.o -lc

$(DD_ELF): user/dd.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/dd.c -o user/dd.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/dd.o -lc

$(PWD_ELF): user/pwd.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/pwd.c -o user/pwd.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/pwd.o -lc

$(STAT_ELF): user/stat.c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$(DYN_CC) -c user/stat.c -o user/stat.o
	@$(DYN_LD) -o $@ $(ULIBC_CRT0) user/stat.o -lc

$(LDSO_ELF): user/ldso.c user/ldso_linker.ld
	@i686-elf-gcc -m32 -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/ldso_linker.ld -o $(LDSO_ELF) user/ldso.c

$(PIE_SO): user/pie_func.c
	@i686-elf-gcc -m32 -fPIC -fno-plt -c user/pie_func.c -o user/pie_func.o
	@i686-elf-ld -m elf_i386 -shared -soname libpietest.so -o $(PIE_SO) user/pie_func.o

$(PIE_ELF): user/pie_main.c user/pie_linker.ld $(PIE_SO)
	@i686-elf-gcc -m32 -fPIC -c user/pie_main.c -o user/pie_main.o
	@i686-elf-ld -m elf_i386 -pie --dynamic-linker=/lib/ld.so -T user/pie_linker.ld -o $(PIE_ELF) user/pie_main.o $(PIE_SO) -rpath /lib

# All dynamically-linked user commands
USER_CMDS := $(ECHO_ELF) $(SH_ELF) $(CAT_ELF) $(LS_ELF) $(MKDIR_ELF) $(RM_ELF) \
             $(CP_ELF) $(MV_ELF) $(TOUCH_ELF) $(LN_ELF) \
             $(HEAD_ELF) $(TAIL_ELF) $(WC_ELF) $(SORT_ELF) $(UNIQ_ELF) $(CUT_ELF) \
             $(CHMOD_ELF) $(CHOWN_ELF) $(CHGRP_ELF) \
             $(DATE_ELF) $(HOSTNAME_ELF) $(UPTIME_ELF) \
             $(MOUNT_ELF) $(UMOUNT_ELF) $(ENV_ELF) $(KILL_ELF) $(SLEEP_ELF) \
             $(CLEAR_ELF) $(PS_ELF) $(DF_ELF) $(FREE_ELF) $(TEE_ELF) \
             $(BASENAME_ELF) $(DIRNAME_ELF) $(RMDIR_ELF) \
             $(GREP_ELF) $(ID_ELF) $(UNAME_ELF) $(DMESG_ELF) \
             $(PRINTENV_ELF) $(TR_ELF) $(DD_ELF) $(PWD_ELF) $(STAT_ELF) \
             $(INIT_ELF)

FSTAB := rootfs/etc/fstab
INITRD_FILES := $(FULLTEST_ELF):sbin/fulltest \
    $(INIT_ELF):sbin/init \
    $(ECHO_ELF):bin/echo $(SH_ELF):bin/sh $(CAT_ELF):bin/cat $(LS_ELF):bin/ls \
    $(MKDIR_ELF):bin/mkdir $(RM_ELF):bin/rm $(CP_ELF):bin/cp $(MV_ELF):bin/mv \
    $(TOUCH_ELF):bin/touch $(LN_ELF):bin/ln \
    $(HEAD_ELF):bin/head $(TAIL_ELF):bin/tail $(WC_ELF):bin/wc \
    $(SORT_ELF):bin/sort $(UNIQ_ELF):bin/uniq $(CUT_ELF):bin/cut \
    $(CHMOD_ELF):bin/chmod $(CHOWN_ELF):bin/chown $(CHGRP_ELF):bin/chgrp \
    $(DATE_ELF):bin/date $(HOSTNAME_ELF):bin/hostname $(UPTIME_ELF):bin/uptime \
    $(MOUNT_ELF):bin/mount $(UMOUNT_ELF):bin/umount $(ENV_ELF):bin/env \
    $(KILL_ELF):bin/kill $(SLEEP_ELF):bin/sleep $(CLEAR_ELF):bin/clear \
    $(PS_ELF):bin/ps $(DF_ELF):bin/df $(FREE_ELF):bin/free \
    $(TEE_ELF):bin/tee $(BASENAME_ELF):bin/basename $(DIRNAME_ELF):bin/dirname \
    $(RMDIR_ELF):bin/rmdir \
    $(GREP_ELF):bin/grep $(ID_ELF):bin/id $(UNAME_ELF):bin/uname \
    $(DMESG_ELF):bin/dmesg $(PRINTENV_ELF):bin/printenv $(TR_ELF):bin/tr \
    $(DD_ELF):bin/dd $(PWD_ELF):bin/pwd $(STAT_ELF):bin/stat \
    $(LDSO_ELF):lib/ld.so $(ULIBC_SO):lib/libc.so \
    $(PIE_SO):lib/libpietest.so $(PIE_ELF):bin/pie_test \
    $(FSTAB):etc/fstab
INITRD_DEPS := $(MKINITRD) $(FULLTEST_ELF) $(USER_CMDS) $(LDSO_ELF) $(ULIBC_SO) $(PIE_SO) $(PIE_ELF) $(FSTAB)

# Include doom.elf if it has been built
ifneq ($(wildcard $(DOOM_ELF)),)
INITRD_FILES += $(DOOM_ELF):bin/doom
INITRD_DEPS += $(DOOM_ELF)
endif

$(INITRD_IMG): $(INITRD_DEPS)
	@./$(MKINITRD) $(INITRD_IMG) $(INITRD_FILES)

run: iso
	@rm -f serial.log qemu.log
	@test -f disk.img || dd if=/dev/zero of=disk.img bs=1M count=4 2>/dev/null
	@qemu-system-i386 -boot d -cdrom adros-$(ARCH).iso -m 128M -display none \
		-drive file=disk.img,if=ide,format=raw \
		-nic user,model=e1000 \
		-serial file:serial.log -monitor none -no-reboot -no-shutdown \
		$(QEMU_DFLAGS)

run-arm: adros-arm.bin
	@rm -f serial-arm.log
	@qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic \
		-kernel adros-arm.bin -serial mon:stdio $(QEMU_DFLAGS)

run-riscv: adros-riscv.bin
	@rm -f serial-riscv.log
	@qemu-system-riscv64 -M virt -m 128M -nographic -bios none \
		-kernel adros-riscv.bin -serial mon:stdio $(QEMU_DFLAGS)

run-mips: adros-mips.bin
	@rm -f serial-mips.log
	@qemu-system-mipsel -M malta -m 128M -nographic \
		-kernel adros-mips.bin -serial mon:stdio $(QEMU_DFLAGS)

# ---- Static Analysis ----

cppcheck:
	@cppcheck --version >/dev/null
	@cppcheck --quiet --enable=warning,performance,portability --error-exitcode=1 \
		-I include $(SRC_DIR)

# Sparse: kernel-oriented semantic checker (type safety, NULL, bitwise vs logical)
SPARSE_FLAGS := -m32 -D__i386__ -D__linux__ -Iinclude
SPARSE_SRCS := $(filter-out $(wildcard $(SRC_DIR)/arch/arm/*.c) \
                             $(wildcard $(SRC_DIR)/arch/riscv/*.c) \
                             $(wildcard $(SRC_DIR)/arch/mips/*.c) \
                             $(wildcard $(SRC_DIR)/hal/arm/*.c) \
                             $(wildcard $(SRC_DIR)/hal/riscv/*.c) \
                             $(wildcard $(SRC_DIR)/hal/mips/*.c), $(C_SOURCES))

sparse:
	@echo "[SPARSE] Running sparse on $(words $(SPARSE_SRCS)) files..."
	@fail=0; \
	for f in $(SPARSE_SRCS); do \
		sparse $(SPARSE_FLAGS) $$f 2>&1; \
	done
	@echo "[SPARSE] Done."

# GCC -fanalyzer: interprocedural static analysis (use-after-free, NULL deref, etc)
ANALYZER_FLAGS := -m32 -ffreestanding -fanalyzer -fsyntax-only -Iinclude -O2 -Wno-cpp

analyzer:
	@echo "[ANALYZER] Running gcc -fanalyzer on $(words $(SPARSE_SRCS)) files..."
	@fail=0; \
	for f in $(SPARSE_SRCS); do \
		$(CC) $(ANALYZER_FLAGS) $$f 2>&1 | grep -v "^$$" || true; \
	done
	@echo "[ANALYZER] Done."

# Combined static analysis: cppcheck + sparse
check: cppcheck sparse
	@echo "[CHECK] All static analysis passed."

# ---- Automated Smoke Test (QEMU + expect) ----

SMOKE_SMP ?= 4
SMOKE_TIMEOUT ?= 120

test: iso
	@echo "[TEST] Running smoke test (SMP=$(SMOKE_SMP), timeout=$(SMOKE_TIMEOUT)s)..."
	@expect tests/smoke_test.exp $(SMOKE_SMP) $(SMOKE_TIMEOUT)

test-1cpu: iso
	@echo "[TEST] Running smoke test (SMP=1, timeout=50s)..."
	@expect tests/smoke_test.exp 1 50

test-battery: iso
	@echo "[TEST] Running full test battery (multi-disk, ping, VFS)..."
	@expect tests/test_battery.exp $(SMOKE_TIMEOUT)

# ---- Host-Side Unit Tests ----

test-host:
	@mkdir -p build/host
	@echo "[TEST-HOST] Compiling tests/test_utils.c..."
	@gcc -m32 -Wall -Wextra -Werror -Iinclude -o build/host/test_utils tests/test_utils.c
	@./build/host/test_utils
	@echo "[TEST-HOST] Compiling tests/test_security.c..."
	@gcc -m32 -Wall -Wextra -Werror -Iinclude -o build/host/test_security tests/test_security.c
	@./build/host/test_security
	@echo "[TEST-HOST] Running userspace utility tests..."
	@bash tests/test_host_utils.sh

# ---- GDB Scripted Checks (requires QEMU + GDB) ----

test-gdb: $(KERNEL_NAME) iso
	@echo "[TEST-GDB] Starting QEMU with GDB stub..."
	@rm -f serial.log
	@test -f disk.img || dd if=/dev/zero of=disk.img bs=1M count=4 2>/dev/null
	@qemu-system-i386 -smp 4 -boot d -cdrom adros-$(ARCH).iso -m 128M -display none \
		-drive file=disk.img,if=ide,format=raw \
		-serial file:serial.log -monitor none -no-reboot -no-shutdown \
		-s -S &
	@sleep 1
	@gdb -batch -nx -x tests/gdb_checks.py adros-$(ARCH).bin || true
	@-pkill -f "qemu-system-i386.*-s -S" 2>/dev/null || true

# ---- All Tests ----

test-all: check test-host test
	@echo "[TEST-ALL] All tests passed."

scan-build:
	@command -v scan-build >/dev/null
	@scan-build --status-bugs $(MAKE) ARCH=$(ARCH) $(if $(CROSS),CROSS=$(CROSS),) all

mkinitrd-asan: $(FULLTEST_ELF)
	@mkdir -p build/host
	@gcc -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined tools/mkinitrd.c -o build/host/mkinitrd-asan
	@./build/host/mkinitrd-asan build/host/$(INITRD_IMG).asan $(FULLTEST_ELF):sbin/fulltest

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# lwIP sources (compiled with relaxed warnings)
$(BUILD_DIR)/lwip/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC      $< (lwIP)"
	@$(CC) $(CFLAGS) -Wno-address -w -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	@$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build $(KERNEL_NAME)

.PHONY: all clean iso run cppcheck sparse analyzer check test test-1cpu test-host test-gdb test-all scan-build mkinitrd-asan
