# SPDX-License-Identifier: BSD-3-Clause
#
# AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
# Mirror: https://github.com/tadryanom/AdrOS
#

# ldso — dynamic linker (statically linked)
NAME := ldso

TOPDIR   ?= $(abspath ../../..)
BUILDDIR ?= $(TOPDIR)/build/x86/user/cmds/$(NAME)
USER_CC  ?= i686-elf-gcc

ELF := $(BUILDDIR)/ld.so

all: $(ELF)

$(ELF): ldso.c
	@mkdir -p $(BUILDDIR)
	@echo "  CC+LD   $@"
	@$(USER_CC) -m32 -ffreestanding -fno-pie -no-pie -nostdlib \
		-Wl,-T,$(TOPDIR)/user/ldso_linker.ld -o $@ ldso.c

clean:
	rm -f $(ELF)

.PHONY: all clean
