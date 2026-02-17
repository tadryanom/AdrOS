# DOOM for AdrOS — Build system
#
# Prerequisites:
#   1. Clone doomgeneric into this directory:
#      git clone https://github.com/ozkl/doomgeneric.git
#   2. Place DOOM1.WAD (shareware) in this directory
#   3. Run: make
#
# The output is doom.elf which gets packaged into the AdrOS initrd.

CC := i686-elf-gcc
LD := i686-elf-ld

ULIBC_DIR := ../ulibc
ULIBC_INC := $(ULIBC_DIR)/include
CRT0      := $(ULIBC_DIR)/src/crt0.o
DYN_LINKER := ../dyn_linker.ld

CFLAGS := -m32 -O2 -ffreestanding -nostdlib -fPIC -fno-plt \
          -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-pointer-sign \
          -Wno-missing-field-initializers -Wno-implicit-fallthrough \
          -Wno-format \
          -I$(ULIBC_INC) -I. -Idoomgeneric/doomgeneric \
          -DNORMALUNIX -DLINUX

LIBGCC  := $(shell $(CC) -print-libgcc-file-name)
LDFLAGS := -m elf_i386 --dynamic-linker=/lib/ld.so -T $(DYN_LINKER) -L$(ULIBC_DIR) -rpath /lib

# doomgeneric engine sources (nested inside cloned repo)
# Exclude platform adapters and files requiring external libraries
DG_EXCLUDE := doomgeneric/doomgeneric/doomgeneric_allegro.c \
              doomgeneric/doomgeneric/doomgeneric_emscripten.c \
              doomgeneric/doomgeneric/doomgeneric_linuxvt.c \
              doomgeneric/doomgeneric/doomgeneric_sdl.c \
              doomgeneric/doomgeneric/doomgeneric_soso.c \
              doomgeneric/doomgeneric/doomgeneric_sosox.c \
              doomgeneric/doomgeneric/doomgeneric_win.c \
              doomgeneric/doomgeneric/doomgeneric_xlib.c \
              doomgeneric/doomgeneric/i_sdlmusic.c \
              doomgeneric/doomgeneric/i_sdlsound.c \
              doomgeneric/doomgeneric/i_allegromusic.c \
              doomgeneric/doomgeneric/i_allegrosound.c \
              doomgeneric/doomgeneric/icon.c
DG_SRC := $(filter-out $(DG_EXCLUDE),$(wildcard doomgeneric/doomgeneric/*.c))
DG_OBJ := $(DG_SRC:.c=.o)

# AdrOS adapter
ADAPTER_OBJ := doomgeneric_adros.o

ALL_OBJ := $(DG_OBJ) $(ADAPTER_OBJ)

.PHONY: all clean check-doomgeneric

all: check-doomgeneric doom.elf

check-doomgeneric:
	@if [ ! -d doomgeneric ]; then \
		echo "ERROR: doomgeneric/ directory not found."; \
		echo "Run: git clone https://github.com/ozkl/doomgeneric.git"; \
		exit 1; \
	fi

doom.elf: $(ALL_OBJ) $(CRT0)
	@$(LD) $(LDFLAGS) -o $@ $(CRT0) $(ALL_OBJ) -lc $(LIBGCC)
	@echo "  LD      $@"

doomgeneric/doomgeneric/%.o: doomgeneric/doomgeneric/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "  CC      $<"

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "  CC      $<"

clean:
	rm -f $(ALL_OBJ) doom.elf

