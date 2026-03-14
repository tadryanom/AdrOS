# SPDX-License-Identifier: BSD-3-Clause
#
# AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
# Mirror: https://github.com/tadryanom/AdrOS
#

# pie_test — PIE/shared library test binary
NAME := pie_test

TOPDIR   ?= $(abspath ../../..)
BUILDDIR := $(TOPDIR)/build/user/cmds/$(NAME)
USER_CC  ?= i686-elf-gcc
USER_LD  ?= i686-elf-ld

PIE_SO  := $(BUILDDIR)/libpietest.so
PIE_ELF := $(BUILDDIR)/pie_test.elf

all: $(PIE_SO) $(PIE_ELF)

$(PIE_SO): pie_func.c
	@mkdir -p $(BUILDDIR)
	@echo "  CC [PIC] $<"
	@$(USER_CC) -m32 -fPIC -fno-plt -c pie_func.c -o $(BUILDDIR)/pie_func.o
	@$(USER_LD) -m elf_i386 -shared -soname libpietest.so -o $@ $(BUILDDIR)/pie_func.o

$(PIE_ELF): pie_main.c $(PIE_SO)
	@mkdir -p $(BUILDDIR)
	@echo "  CC [PIE] $<"
	@$(USER_CC) -m32 -fPIC -c pie_main.c -o $(BUILDDIR)/pie_main.o
	@$(USER_LD) -m elf_i386 -pie --dynamic-linker=/lib/ld.so \
		-T $(TOPDIR)/user/pie_linker.ld -o $@ $(BUILDDIR)/pie_main.o $(PIE_SO) -rpath /lib

clean:
	rm -f $(BUILDDIR)/pie_func.o $(BUILDDIR)/pie_main.o $(PIE_SO) $(PIE_ELF)

.PHONY: all clean
