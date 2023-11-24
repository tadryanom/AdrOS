# Tools
AS = nasm
CC = gcc
LD = ld
QEMU = qemu-system-i386

# Flags to Assembler, Compiller and Linker
ASFLAGS = -f elf
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
	-nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c -I./include
LDFLAGS = -T scripts/link.ld -melf_i386

# Kernel objects
OBJECTS = src/start.o src/kmain.o

# Build
all: kernel.elf

kernel.elf: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o bin/kernel.elf

%.o: %.c
	$(CC) $(CFLAGS)  $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

# ISO file
adros.iso: kernel.elf
	cp bin/kernel.elf iso/boot/kernel.elf
	if [ -f iso/adros.iso ]; then rm -vf iso/adros.iso ; fi
#	genisoimage -R                  \
#	-b boot/grub/stage2_eltorito    \
#	-no-emul-boot                   \
#	-boot-load-size 4               \
#	-A AdrOS                        \
#	-input-charset utf8             \
#	-boot-info-table                \
#	-o iso/adros.iso                \
#	iso
	grub-mkrescue -o iso/adros.iso iso

# Test run
run: adros.iso
	$(QEMU) -monitor stdio -m 32 -cdrom iso/adros.iso -boot d

# Clean
clean:
	rm -vrf src/*.o bin/kernel.elf iso/boot/kernel.elf iso/adros.iso
