OBJECTS = src/loader.o src/kmain.o
CC = gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
	-nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c
LDFLAGS = -T scripts/link.ld -melf_i386
AS = nasm
ASFLAGS = -f elf

all: kernel.elf

kernel.elf: $(OBJECTS)
	ld $(LDFLAGS) $(OBJECTS) -o bin/kernel.elf

adros.iso: kernel.elf
	cp bin/kernel.elf iso/boot/kernel.elf
	if [ -f iso/adros.iso ]; then rm -vf iso/adros.iso ; fi
	genisoimage -R                  \
	-b boot/grub/stage2_eltorito    \
	-no-emul-boot                   \
	-boot-load-size 4               \
	-A AdrOS                        \
	-input-charset utf8             \
	-boot-info-table                \
	-o iso/adros.iso                \
	iso

run: adros.iso
	qemu-system-i386 -monitor stdio -m 32 -cdrom iso/adros.iso

%.o: %.c
	$(CC) $(CFLAGS)  $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -vrf src/*.o bin/kernel.elf iso/boot/kernel.elf iso/adros.iso
