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

all: $(KERNEL_NAME)

$(KERNEL_NAME): $(OBJ)
	@echo "  LD      $@"
	@$(LD) $(LDFLAGS) -n -o $@ $(OBJ)

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

.PHONY: all clean
