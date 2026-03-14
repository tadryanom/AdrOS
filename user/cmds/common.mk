# SPDX-License-Identifier: BSD-3-Clause
#
# AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
# Mirror: https://github.com/tadryanom/AdrOS
#

# Common build rules for AdrOS user commands (dynamically linked)
# Each program Makefile sets NAME and SRCS, then includes this file.
#
# Usage from per-program Makefile:
#   NAME := echo
#   SRCS := echo.c
#   include ../common.mk

TOPDIR   ?= $(abspath ../../..)
BUILDDIR ?= $(TOPDIR)/build/x86/user/cmds/$(NAME)

ULIBC_DIR := $(TOPDIR)/user/ulibc
DYN_CC   ?= i686-elf-gcc -m32 -ffreestanding -nostdlib -O2 -Wall -Wextra -fPIC -fno-plt -I$(ULIBC_DIR)/include
DYN_LD   ?= i686-elf-ld -m elf_i386 --dynamic-linker=/lib/ld.so -T $(TOPDIR)/user/dyn_linker.ld -L$(ULIBC_DIR) -rpath /lib
CRT0     ?= $(ULIBC_DIR)/src/crt0.o

OBJS := $(addprefix $(BUILDDIR)/,$(SRCS:.c=.o))
ELF  := $(BUILDDIR)/$(NAME).elf

all: $(ELF)

$(ELF): $(OBJS)
	@echo "  LD      $@"
	@$(DYN_LD) -o $@ $(CRT0) $(OBJS) -lc

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(BUILDDIR)
	@echo "  CC      $<"
	@$(DYN_CC) -c $< -o $@

clean:
	rm -f $(OBJS) $(ELF)

.PHONY: all clean
