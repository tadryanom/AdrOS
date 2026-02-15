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
    ARCH_CFLAGS := -m32 -ffreestanding -fno-builtin -U_FORTIFY_SOURCE -Iinclude -Iinclude/net -Ithird_party/lwip/src/include
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

    USER_ELF := user/init.elf
    ECHO_ELF := user/echo.elf
    SH_ELF := user/sh.elf
    CAT_ELF := user/cat.elf
    LS_ELF := user/ls.elf
    MKDIR_ELF := user/mkdir.elf
    RM_ELF := user/rm.elf
    LDSO_ELF := user/ld.so
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
    CFLAGS := -ffreestanding -O2 -Wall -Wextra -Werror -Wno-error=cpp -Iinclude -mabi=32 -march=mips32
    LDFLAGS := -T $(SRC_DIR)/arch/mips/linker.ld
    ASFLAGS :=
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
	@$(LD) $(LDFLAGS) -n -o $@ $(BOOT_OBJ) $(KERNEL_OBJ)

iso: $(KERNEL_NAME) $(INITRD_IMG)
	@mkdir -p iso/boot
	@cp -f $(KERNEL_NAME) iso/boot/$(KERNEL_NAME)
	@cp -f $(INITRD_IMG) iso/boot/$(INITRD_IMG)
	@echo "  GRUB-MKRESCUE  adros-$(ARCH).iso"
	@grub-mkrescue -o adros-$(ARCH).iso iso > /dev/null

$(MKINITRD): tools/mkinitrd.c
	@gcc tools/mkinitrd.c -o $(MKINITRD)

ULIBC_DIR := user/ulibc
ULIBC_LIB := $(ULIBC_DIR)/libulibc.a

$(ULIBC_LIB):
	@$(MAKE) -C $(ULIBC_DIR) --no-print-directory

$(USER_ELF): user/init.c user/linker.ld
	@i686-elf-gcc -m32 -I include -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/linker.ld -o $(USER_ELF) user/init.c user/errno.c

$(ECHO_ELF): user/echo.c user/linker.ld
	@i686-elf-gcc -m32 -I include -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/linker.ld -o $(ECHO_ELF) user/echo.c user/errno.c

$(SH_ELF): user/sh.c user/linker.ld
	@i686-elf-gcc -m32 -I include -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/linker.ld -o $(SH_ELF) user/sh.c user/errno.c

$(CAT_ELF): user/cat.c user/linker.ld
	@i686-elf-gcc -m32 -I include -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/linker.ld -o $(CAT_ELF) user/cat.c user/errno.c

$(LS_ELF): user/ls.c user/linker.ld
	@i686-elf-gcc -m32 -I include -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/linker.ld -o $(LS_ELF) user/ls.c user/errno.c

$(MKDIR_ELF): user/mkdir.c user/linker.ld
	@i686-elf-gcc -m32 -I include -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/linker.ld -o $(MKDIR_ELF) user/mkdir.c user/errno.c

$(RM_ELF): user/rm.c user/linker.ld
	@i686-elf-gcc -m32 -I include -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/linker.ld -o $(RM_ELF) user/rm.c user/errno.c

$(LDSO_ELF): user/ldso.c user/linker.ld
	@i686-elf-gcc -m32 -I include -ffreestanding -fno-pie -no-pie -nostdlib -Wl,-T,user/linker.ld -o $(LDSO_ELF) user/ldso.c

FSTAB := rootfs/etc/fstab
INITRD_FILES := $(USER_ELF):bin/init.elf $(ECHO_ELF):bin/echo.elf $(SH_ELF):bin/sh $(CAT_ELF):bin/cat $(LS_ELF):bin/ls $(MKDIR_ELF):bin/mkdir $(RM_ELF):bin/rm $(LDSO_ELF):lib/ld.so $(FSTAB):etc/fstab
INITRD_DEPS := $(MKINITRD) $(USER_ELF) $(ECHO_ELF) $(SH_ELF) $(CAT_ELF) $(LS_ELF) $(MKDIR_ELF) $(RM_ELF) $(LDSO_ELF) $(FSTAB)

# Include doom.elf if it has been built
ifneq ($(wildcard $(DOOM_ELF)),)
INITRD_FILES += $(DOOM_ELF):bin/doom.elf
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
SMOKE_TIMEOUT ?= 90

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

mkinitrd-asan: $(USER_ELF)
	@mkdir -p build/host
	@gcc -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined tools/mkinitrd.c -o build/host/mkinitrd-asan
	@./build/host/mkinitrd-asan build/host/$(INITRD_IMG).asan $(USER_ELF):bin/init.elf

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
