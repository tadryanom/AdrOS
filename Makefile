# Tools
AS = nasm
AR = ar
CC = gcc
LD = ld
GENISO = grub-mkrescue
GENISO2 = genisoimage
QEMU = qemu-system-i386

# Flags to Assembler, Compiller and Linker
ASFLAGS = -f elf
CFLAGS = -m32 -Wall -Wextra -Werror -O -nostdlib -nostdinc -fno-builtin \
	-nostartfiles -nodefaultlibs -fno-stack-protector -fstrength-reduce \
	-fomit-frame-pointer -finline-functions -I./include -c
LDFLAGS = -T scripts/link.ld -melf_i386

# Kernel objects
KOBJ = src/start.o src/kmain.o src/system.o src/screen.o

# Kernel libc objects
KLIBOBJ = src/string.o src/stdlib.o src/stdio.o

# Kernel/klibc
KERNEL = bin/kernel.elf
LIBK = bin/libk.a

# CD-ROM ISO image
CDROM = iso/adros.iso
CDROM2 = iso/adros2.iso

# Build
all: $(KERNEL)

$(KERNEL): $(KOBJ) $(LIBK)
	@echo "[LD] $(KERNEL)"
	@$(LD) $(LDFLAGS) $(KOBJ) $(LIBK) -o $(KERNEL)

$(LIBK): $(KLIBOBJ)
	@if ! [ -d bin ] ; then mkdir -p bin ; fi
	@echo "[AR] $(LIBK)"
	@$(AR) -rcs $(LIBK) $(KLIBOBJ)

%.o: %.s
	@echo "[AS] $@"
	@$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	@echo "[CC] $@"
	@$(CC) $(CFLAGS)  $< -o $@

$(CDROM): $(KERNEL)
	@echo "Creating CD-ROM ISO image..."
	@cp $(KERNEL) iso/boot/kernel.elf
	@if [ -f $(CDROM) ]; then rm -vf $(CDROM) ; fi
	@$(GENISO) -o $(CDROM) iso

$(CDROM2): $(KERNEL)
	@echo "Creating CD-ROM ISO image..."
	@cp $(KERNEL) iso/boot/kernel.elf
	@if [ -f $(CDROM2) ]; then rm -vf $(CDROM2) ; fi
	@$(GENISO2) -R               \
	-b boot/grub/stage2_eltorito \
	-no-emul-boot                \
	-boot-load-size 4            \
	-A AdrOS                     \
	-input-charset utf8          \
	-boot-info-table             \
	-o $(CDROM2)                 \
	iso

# Test run
run: $(CDROM)
	@echo "Running kernel test..."
	@$(QEMU) -monitor stdio -m 32 -cdrom $(CDROM) -boot d

run2: $(CDROM2)
	@echo "Running kernel test..."
	@$(QEMU) -monitor stdio -m 32 -cdrom $(CDROM2) -boot d

# Clean
clean:
	@echo "Cleaning compiled objects..."
	@rm -vf src/*.o $(LIBK) $(KERNEL) iso/boot/kernel.elf $(CDROM) $(CDROM2)
