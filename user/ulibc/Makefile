# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://github.com/tadryanom/AdrOS
#

# ulibc — Minimal C library for AdrOS userspace
CC := i686-elf-gcc
AS := i686-elf-as
AR := i686-elf-ar

CFLAGS := -m32 -ffreestanding -fno-pie -no-pie -nostdlib -O2 -Wall -Wextra -Iinclude
ASFLAGS := --32

SRC_C := $(wildcard src/*.c)
SRC_S := $(wildcard src/*.S)
OBJ := $(SRC_C:.c=.o) $(SRC_S:.S=.o)

all: libulibc.a

libulibc.a: $(OBJ)
	@$(AR) rcs $@ $^
	@echo "  AR      $@"

src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "  CC      $<"

src/%.o: src/%.S
	@$(AS) $(ASFLAGS) $< -o $@
	@echo "  AS      $<"

clean:
	rm -f src/*.o libulibc.a

.PHONY: all clean
