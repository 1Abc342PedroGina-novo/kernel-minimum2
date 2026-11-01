CC      = gcc
AS      = nasm
LD      = ld
CFLAGS  = -m32 -ffreestanding -fno-pie -fno-stack-protector -Wall -Wextra -O2
LDFLAGS = -m elf_i386 --allow-multiple-definition -T link.ld

OBJS = boot.o cswitch.o task.o kernel.o serial.o gdt.o idt.o isr.o  paging.o irq_stub.o irq.o pit.o objpool.o tss.o pmm.o vmm.o mm_info.o heap.o

all: mykernel.elf

boot.o: boot.S
	$(AS) -f elf32 boot.S -o boot.o

cswitch.o: cswitch.S
	$(AS) -f elf32 cswitch.S -o cswitch.o

task.o: task.c task.h pmm.h vmm.h pit.h serial.h tss.h
	$(CC) $(CFLAGS) -c task.c -o task.o

isr.o: isr.S
	$(AS) -f elf32 isr.S -o isr.o

idt.o: idt.c idt.h isr.h serial.h
	$(CC) $(CFLAGS) -c idt.c -o idt.o

paging.o: paging.c paging.h serial.h
	$(CC) $(CFLAGS) -c paging.c -o paging.o

irq_stub.o: irq_stub.S
	$(AS) -f elf32 irq_stub.S -o irq_stub.o

pit.o: pit.c pit.h irq.h isr.h serial.h
	$(CC) $(CFLAGS) -c pit.c -o pit.o
 
irq.o: irq.c irq.h idt.h isr.h serial.h
	$(CC) $(CFLAGS) -c irq.c -o irq.o

objpool.o: objpool.c objpool.h serial.h
	$(CC) $(CFLAGS) -c objpool.c -o objpool.o

pmm.o: pmm.c pmm.h serial.h
	$(CC) $(CFLAGS) -c pmm.c -o pmm.o

vmm.o: vmm.c vmm.h pmm.h serial.h
	$(CC) $(CFLAGS) -c vmm.c -o vmm.o

heap.o: heap.c heap.h vmm.h serial.h
	$(CC) $(CFLAGS) -c heap.c -o heap.o

mm_info.o: mm_info.c mm_info.h pmm.h vmm.h serial.h
	$(CC) $(CFLAGS) -c mm_info.c -o mm_info.o

kernel.o: kernel.c serial.h gdt.h tss.h idt.h irq.h paging.h pit.h objpool.h pmm.h vmm.h mm_info.h isr.h heap.h link.ld
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

serial.o: serial.c serial.h
	$(CC) $(CFLAGS) -c serial.c -o serial.o

tss.o: tss.c tss.h gdt.h serial.h
	$(CC) $(CFLAGS) -c tss.c -o tss.o

gdt.o: gdt.c gdt.h serial.h tss.h
	$(CC) $(CFLAGS) -c gdt.c -o gdt.o

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
