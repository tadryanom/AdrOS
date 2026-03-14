# SPDX-License-Identifier: BSD-3-Clause
#
# AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
# All rights reserved.
# See LICENSE for details.
#
# Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
# Mirror: https://github.com/tadryanom/AdrOS
#

# Newlib libgloss Makefile for AdrOS
#
# Usage: make CC=i686-adros-gcc AR=i686-adros-ar
# Or from the Newlib build system which sets these automatically.

CC  ?= i686-elf-gcc
AR  ?= i686-elf-ar
AS  ?= i686-elf-as

CFLAGS  := -m32 -ffreestanding -nostdlib -Wall -Wextra -O2
ASFLAGS := --32

OBJS := crt0.o syscalls.o

all: crt0.o libadros.a

crt0.o: crt0.S
	$(CC) $(CFLAGS) -c crt0.S -o crt0.o

syscalls.o: syscalls.c
	$(CC) $(CFLAGS) -c syscalls.c -o syscalls.o

libadros.a: syscalls.o
	$(AR) rcs $@ $^

clean:
	rm -f *.o libadros.a

.PHONY: all clean
