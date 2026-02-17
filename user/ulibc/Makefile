# ulibc — Minimal C library for AdrOS userspace
CC := i686-elf-gcc
AS := i686-elf-as
AR := i686-elf-ar
LD := i686-elf-ld

CFLAGS := -m32 -ffreestanding -fno-pie -no-pie -nostdlib -O2 -Wall -Wextra -Iinclude
CFLAGS_PIC := -m32 -ffreestanding -nostdlib -O2 -Wall -Wextra -Iinclude -fPIC -fno-plt
ASFLAGS := --32

SRC_C := $(wildcard src/*.c)
SRC_S := $(wildcard src/*.S)
OBJ := $(SRC_C:.c=.o) $(SRC_S:.S=.o)

# PIC objects for shared library (exclude crt0 — it's linked separately)
PIC_C := $(filter-out src/crt0.S,$(SRC_C))
PIC_OBJ := $(PIC_C:.c=.pic.o)

all: libulibc.a

libulibc.a: $(OBJ)
	@$(AR) rcs $@ $^
	@echo "  AR      $@"

libc.so: $(PIC_OBJ)
	@$(LD) -m elf_i386 -shared -soname libc.so -o $@ $^
	@echo "  LD      $@ (shared)"

src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "  CC      $<"

src/%.pic.o: src/%.c
	@$(CC) $(CFLAGS_PIC) -c $< -o $@
	@echo "  CC [PIC] $<"

src/%.o: src/%.S
	@$(AS) $(ASFLAGS) $< -o $@
	@echo "  AS      $<"

clean:
	rm -f src/*.o src/*.pic.o libulibc.a libc.so

.PHONY: all clean
