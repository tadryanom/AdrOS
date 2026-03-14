# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://github.com/tadryanom/AdrOS
#

# fulltest — statically linked smoke test binary
NAME := fulltest

TOPDIR   ?= $(abspath ../../..)
BUILDDIR ?= $(TOPDIR)/build/x86/user/cmds/$(NAME)
USER_CC  ?= i686-elf-gcc

ELF := $(BUILDDIR)/fulltest.elf

all: $(ELF)

$(ELF): fulltest.c errno.c user_errno.h
	@mkdir -p $(BUILDDIR)
	@echo "  CC+LD   $@"
	@$(USER_CC) -m32 -I $(TOPDIR)/include -ffreestanding -fno-pie -no-pie -nostdlib \
		-Wl,-T,$(TOPDIR)/user/linker.ld -o $@ fulltest.c errno.c

clean:
	rm -f $(ELF)

.PHONY: all clean
