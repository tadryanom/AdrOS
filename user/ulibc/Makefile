# SPDX-License-Identifier: BSD-3-Clause
#
# AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
# Mirror: https://github.com/tadryanom/AdrOS
#

# ulibc — Minimal C library for AdrOS userspace
CC  ?= i686-elf-gcc
AS  ?= i686-elf-as
AR  ?= i686-elf-ar
LD  ?= i686-elf-ld

# Out-of-tree build directory (override from root Makefile)
BUILDDIR ?= .

# Source-relative include path (works from any BUILDDIR)
SRCDIR := $(dir $(lastword $(MAKEFILE_LIST)))src
INCDIR := $(dir $(lastword $(MAKEFILE_LIST)))include

CFLAGS := -m32 -ffreestanding -fno-pie -no-pie -nostdlib -O2 -Wall -Wextra -Werror -I$(INCDIR)
CFLAGS_PIC := -m32 -ffreestanding -nostdlib -O2 -Wall -Wextra -Werror -I$(INCDIR) -fPIC -fno-plt
ASFLAGS := --32

SRC_C := $(wildcard $(SRCDIR)/*.c)
SRC_S := $(wildcard $(SRCDIR)/*.S)
OBJ := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRC_C)) $(patsubst $(SRCDIR)/%.S,$(BUILDDIR)/%.o,$(SRC_S))

# PIC objects for shared library (exclude crt0 — it's linked separately)
PIC_C := $(filter-out $(SRCDIR)/crt0.S,$(SRC_C))
PIC_OBJ := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.pic.o,$(PIC_C))

all: $(BUILDDIR)/libulibc.a

$(BUILDDIR)/libulibc.a: $(OBJ)
	@$(AR) rcs $@ $^
	@echo "  AR      $@"

$(BUILDDIR)/libc.so: $(PIC_OBJ)
	@$(LD) -m elf_i386 -shared -soname libc.so -o $@ $^
	@echo "  LD      $@ (shared)"

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "  CC      $<"

$(BUILDDIR)/%.pic.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	@$(CC) $(CFLAGS_PIC) -c $< -o $@
	@echo "  CC [PIC] $<"

$(BUILDDIR)/%.o: $(SRCDIR)/%.S
	@mkdir -p $(BUILDDIR)
	@$(AS) $(ASFLAGS) $< -o $@
	@echo "  AS      $<"

clean:
	rm -f $(OBJ) $(PIC_OBJ) $(BUILDDIR)/libulibc.a $(BUILDDIR)/libc.so
	rm -f src/*.o src/*.pic.o libulibc.a libc.so

.PHONY: all clean
