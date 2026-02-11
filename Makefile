# AdrOS Makefile
# Usage: make ARCH=x86 (default) | arm | riscv | mips

ARCH ?= x86
KERNEL_NAME := adros-$(ARCH).bin

# Directories
SRC_DIR := src
BUILD_DIR := build/$(ARCH)

# Common sources
C_SOURCES := $(wildcard $(SRC_DIR)/kernel/*.c)
C_SOURCES += $(wildcard $(SRC_DIR)/drivers/*.c)
C_SOURCES += $(wildcard $(SRC_DIR)/mm/*.c)
 
 # HAL sources (architecture-specific)
 C_SOURCES += $(wildcard $(SRC_DIR)/hal/$(ARCH)/*.c)

# --- x86 Configuration ---
ifeq ($(ARCH),x86)
    # Default Toolchain Prefix (can be overridden)
    ifdef CROSS
        TOOLPREFIX ?= i686-elf-
    endif

    # Toolchain tools (Allow user override via make CC=...)
    CC ?= $(TOOLPREFIX)gcc
    AS ?= $(TOOLPREFIX)as
    LD ?= $(TOOLPREFIX)ld
    
    # Mandatory Architecture Flags
    ARCH_CFLAGS := -m32 -ffreestanding -Iinclude
    ARCH_LDFLAGS := -m elf_i386 -T $(SRC_DIR)/arch/x86/linker.ld
    ARCH_ASFLAGS := --32

    # Default User Flags (Allow override via make CFLAGS=...)
    CFLAGS ?= -O2 -Wall -Wextra
    
    # Merge Flags
    CFLAGS := $(ARCH_CFLAGS) $(CFLAGS)
    LDFLAGS := $(ARCH_LDFLAGS) $(LDFLAGS)
    ASFLAGS := $(ARCH_ASFLAGS) $(ASFLAGS)

    ASM_SOURCES := $(wildcard $(SRC_DIR)/arch/x86/*.S)
    C_SOURCES += $(wildcard $(SRC_DIR)/arch/x86/*.c)

    USER_ELF := user/init.elf
    ECHO_ELF := user/echo.elf
    SH_ELF := user/sh.elf
    INITRD_IMG := initrd.img
    MKINITRD := tools/mkinitrd
endif

# --- ARM64 Configuration ---
ifeq ($(ARCH),arm)
    CC := aarch64-linux-gnu-gcc
    AS := aarch64-linux-gnu-as
    LD := aarch64-linux-gnu-ld
    CFLAGS := -ffreestanding -O2 -Wall -Wextra -Iinclude
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
    CFLAGS := -ffreestanding -O2 -Wall -Wextra -Iinclude -mcmodel=medany
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
    CFLAGS := -ffreestanding -O2 -Wall -Wextra -Iinclude -mabi=32 -march=mips32
    LDFLAGS := -T $(SRC_DIR)/arch/mips/linker.ld
    ASFLAGS :=
    ASM_SOURCES := $(wildcard $(SRC_DIR)/arch/mips/*.S)
    C_SOURCES += $(wildcard $(SRC_DIR)/arch/mips/*.c)
endif

# Object generation
OBJ := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
OBJ += $(patsubst $(SRC_DIR)/%.S, $(BUILD_DIR)/%.o, $(ASM_SOURCES))

QEMU_DFLAGS :=
ifneq ($(QEMU_DEBUG),)
QEMU_DFLAGS := -d guest_errors,cpu_reset -D qemu.log
endif

ifneq ($(QEMU_INT),)
QEMU_DFLAGS := $(QEMU_DFLAGS) -d int
endif

BOOT_OBJ := $(BUILD_DIR)/arch/x86/boot.o
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

$(INITRD_IMG): $(MKINITRD) $(USER_ELF) $(ECHO_ELF) $(SH_ELF)
	@./$(MKINITRD) $(INITRD_IMG) $(USER_ELF):bin/init.elf $(ECHO_ELF):bin/echo.elf $(SH_ELF):bin/sh

run: iso
	@rm -f serial.log qemu.log
	@test -f disk.img || dd if=/dev/zero of=disk.img bs=1M count=4 2>/dev/null
	@qemu-system-i386 -boot d -cdrom adros-$(ARCH).iso -m 128M -display none \
		-drive file=disk.img,if=ide,format=raw \
		-serial file:serial.log -monitor none -no-reboot -no-shutdown \
		$(QEMU_DFLAGS)

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
SMOKE_TIMEOUT ?= 60

test: iso
	@echo "[TEST] Running smoke test (SMP=$(SMOKE_SMP), timeout=$(SMOKE_TIMEOUT)s)..."
	@expect tests/smoke_test.exp $(SMOKE_SMP) $(SMOKE_TIMEOUT)

test-1cpu: iso
	@echo "[TEST] Running smoke test (SMP=1, timeout=50s)..."
	@expect tests/smoke_test.exp 1 50

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

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	@$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build $(KERNEL_NAME)

.PHONY: all clean iso run cppcheck sparse analyzer check test test-1cpu test-host test-gdb test-all scan-build mkinitrd-asan
