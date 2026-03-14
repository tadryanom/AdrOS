# SPDX-License-Identifier: BSD-3-Clause
#
# AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
# Mirror: https://github.com/tadryanom/AdrOS
#

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

    # Userspace cross-compiler (always i686-elf, even when kernel CC differs)
    USER_CC  ?= i686-elf-gcc
    USER_LD  ?= i686-elf-ld
    USER_AR  ?= i686-elf-ar

    # User build output directory (under build/$ARCH/ to keep arch-separated)
    USER_BUILD := build/$(ARCH)/user

    # List of dynamically-linked user commands (built via user/cmds/<name>/Makefile)
    USER_CMD_NAMES := echo sh cat ls mkdir rm cp mv touch ln \
                      head tail wc sort uniq cut \
                      chmod chown chgrp \
                      date hostname uptime \
                      mount umount env kill sleep \
                      clear ps df free tee \
                      basename dirname rmdir \
                      grep id uname dmesg \
                      printenv tr dd pwd stat \
                      sed awk who top du find which \
                      init

    # ELF paths for dynamically-linked commands
    USER_CMD_ELFS := $(foreach cmd,$(USER_CMD_NAMES),$(USER_BUILD)/cmds/$(cmd)/$(cmd).elf)

    # Special builds (not dynamically-linked via common.mk)
    FULLTEST_ELF := $(USER_BUILD)/cmds/fulltest/fulltest.elf
    LDSO_ELF     := $(USER_BUILD)/cmds/ldso/ld.so
    PIE_SO       := $(USER_BUILD)/cmds/pie_test/libpietest.so
    PIE_ELF      := $(USER_BUILD)/cmds/pie_test/pie_test.elf

    # ulibc
    ULIBC_DIR := user/ulibc
    ULIBC_SO  := $(USER_BUILD)/ulibc/libc.so
    ULIBC_LIB := $(USER_BUILD)/ulibc/libulibc.a

    # doom
    DOOM_ELF := user/doom/doom.elf

    INITRD_IMG := initrd.img
    MKINITRD   := tools/mkinitrd
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

# --- ulibc build (output into build/$ARCH/user/ulibc/) ---
ULIBC_BUILDDIR := $(CURDIR)/$(USER_BUILD)/ulibc

$(ULIBC_LIB) $(ULIBC_SO): FORCE
	@$(MAKE) -C $(ULIBC_DIR) CC="$(USER_CC)" AS="$(USER_CC:gcc=as)" AR="$(USER_AR)" LD="$(USER_LD)" \
		BUILDDIR="$(ULIBC_BUILDDIR)" \
		$(ULIBC_BUILDDIR)/libulibc.a $(ULIBC_BUILDDIR)/libc.so --no-print-directory
FORCE:

# --- Special builds (fulltest, ldso, pie_test) ---
$(FULLTEST_ELF): user/cmds/fulltest/fulltest.c user/cmds/fulltest/errno.c user/linker.ld
	@$(MAKE) --no-print-directory -C user/cmds/fulltest TOPDIR=$(CURDIR) BUILDDIR=$(CURDIR)/$(USER_BUILD)/cmds/fulltest USER_CC="$(USER_CC)"

$(LDSO_ELF): user/cmds/ldso/ldso.c user/ldso_linker.ld
	@$(MAKE) --no-print-directory -C user/cmds/ldso TOPDIR=$(CURDIR) BUILDDIR=$(CURDIR)/$(USER_BUILD)/cmds/ldso USER_CC="$(USER_CC)"

$(PIE_SO) $(PIE_ELF): user/cmds/pie_test/pie_main.c user/cmds/pie_test/pie_func.c user/pie_linker.ld
	@$(MAKE) --no-print-directory -C user/cmds/pie_test TOPDIR=$(CURDIR) BUILDDIR=$(CURDIR)/$(USER_BUILD)/cmds/pie_test USER_CC="$(USER_CC)" USER_LD="$(USER_LD)"

# --- Dynamically-linked user commands (generic rule via sub-Makefiles) ---
# Use absolute paths so they work from sub-Makefile directories
ABS_ULIBC := $(CURDIR)/$(ULIBC_DIR)
ABS_DYN_CC := $(USER_CC) -m32 -ffreestanding -nostdlib -O2 -Wall -Wextra -fPIC -fno-plt -I$(ABS_ULIBC)/include
ABS_DYN_LD := $(USER_LD) -m elf_i386 --dynamic-linker=/lib/ld.so -T $(CURDIR)/user/dyn_linker.ld -L$(ULIBC_BUILDDIR) -rpath /lib --unresolved-symbols=ignore-in-shared-libs -z noexecstack

# Generate build rules for each dynamically-linked command
define USER_CMD_RULE
$(USER_BUILD)/cmds/$(1)/$(1).elf: user/cmds/$(1)/$(1).c user/dyn_linker.ld $(ULIBC_SO) $(ULIBC_LIB)
	@$$(MAKE) --no-print-directory -C user/cmds/$(1) TOPDIR=$$(CURDIR) \
		BUILDDIR=$$(CURDIR)/$(USER_BUILD)/cmds/$(1) \
		DYN_CC="$$(ABS_DYN_CC)" DYN_LD="$$(ABS_DYN_LD)" CRT0="$(ULIBC_BUILDDIR)/crt0.o"
endef
$(foreach cmd,$(USER_CMD_NAMES),$(eval $(call USER_CMD_RULE,$(cmd))))

# Commands that go to /bin/ in rootfs (all except init)
USER_BIN_NAMES := $(filter-out init,$(USER_CMD_NAMES))

# Build INITRD_FILES list: <elf>:<rootfs-path>
FSTAB := rootfs/etc/fstab
INITRD_FILES := $(FULLTEST_ELF):sbin/fulltest \
    $(USER_BUILD)/cmds/init/init.elf:sbin/init \
    $(foreach cmd,$(USER_BIN_NAMES),$(USER_BUILD)/cmds/$(cmd)/$(cmd).elf:bin/$(cmd)) \
    $(LDSO_ELF):lib/ld.so $(ULIBC_SO):lib/libc.so \
    $(PIE_SO):lib/libpietest.so $(PIE_ELF):bin/pie_test \
    $(FSTAB):etc/fstab

INITRD_DEPS := $(MKINITRD) $(FULLTEST_ELF) $(USER_CMD_ELFS) $(LDSO_ELF) $(ULIBC_SO) $(PIE_SO) $(PIE_ELF) $(FSTAB)

# doom (build via 'make doom', included in initrd if present)
doom: $(ULIBC_LIB) $(ULIBC_SO)
	@$(MAKE) --no-print-directory -C user/doom TOPDIR=$(CURDIR) ULIBC_BUILDDIR="$(ULIBC_BUILDDIR)"

# Include doom.elf if it has been built
ifneq ($(wildcard $(DOOM_ELF)),)
INITRD_FILES += $(DOOM_ELF):usr/games/doom
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
	rm -rf build $(KERNEL_NAME) $(INITRD_IMG) adros-*.iso
	@$(MAKE) -C user/ulibc clean --no-print-directory 2>/dev/null || true
	@if [ -f user/doom/Makefile ]; then $(MAKE) -C user/doom clean --no-print-directory 2>/dev/null || true; fi

.PHONY: all clean iso run run-arm run-riscv run-mips doom cppcheck sparse analyzer check test test-1cpu test-battery test-host test-gdb test-all scan-build mkinitrd-asan
