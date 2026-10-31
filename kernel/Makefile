CC      = gcc
AS      = nasm
LD      = ld
CFLAGS  = -m32 -ffreestanding -fno-pie -fno-stack-protector -Wall -Wextra -O2
LDFLAGS = -m elf_i386 -T link.ld

OBJS = boot.o kernel.o serial.o

all: mykernel.elf

boot.o: boot.S
	$(AS) -f elf32 boot.asm -o boot.o

kernel.o: kernel.c serial.h
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

serial.o: serial.c serial.h
	$(CC) $(CFLAGS) -c serial.c -o serial.o

mykernel.elf: $(OBJS) link.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: mykernel.elf
	mkdir -p iso/boot/grub
	cp mykernel.elf iso/boot/
	printf 'set timeout=0\nset default=0\nmenuentry "kernel" {\n  multiboot /boot/mykernel.elf\n  boot\n}\n' > iso/boot/grub/grub.cfg
	grub-mkrescue -o kernel.iso iso

run: iso
	qemu-system-i386 -cdrom kernel.iso -serial stdio

# Alternativa: serial em arquivo, para inspecionar depois
run-log: iso
	qemu-system-i386 -cdrom kernel.iso -serial file:serial.log -display none

clean:
	rm -f *.o mykernel.elf kernel.iso serial.log
	rm -rf iso
