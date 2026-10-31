// kernel.c
#include "gdt.h"
#include "serial.h"
#include "idt.h"
#include "irq.h"
#include "pit.h"
#include "paging.h"
#include "objdump.h"

#ifndef idt_init
extern void idt_init(void);
#endif


#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile unsigned short*)0xB8000)

static unsigned int cursor_x = 0;
static unsigned int cursor_y = 0;
static unsigned char color = 0x0F;

static unsigned short vga_entry(char c) {
    return (unsigned short)c | ((unsigned short)color << 8);
}

void clear_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = vga_entry(' ');
    cursor_x = cursor_y = 0;
}

void putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c);
        if (++cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    if (cursor_y >= VGA_HEIGHT)
        clear_screen();

    if (c == '\n')
        serial_putchar('\r');
    serial_putchar(c);
}

void print(const char *s) {
    while (*s) putchar(*s++);
}

void kmain(unsigned int magic, unsigned int *mbinfo) {
    serial_init();
    serial_print("[boot] Kernel inicializado\n");

    gdt_init();
    idt_init();
    irq_init();
    paging_init();

    pit_init(100);
    print("Timer 100 Hz ativo.\n");

    // ===== DEMO: Object Pool =====
    serial_print("\n[demo] === Object Pool ===\n");

    // Pool de "tasks" — 64 bytes cada, 64 objetos por página
    struct objpool *tasks = objpool_create(64, 64);
    if (!tasks) {
        serial_print("[demo] FALHOU criar pool\n");
        while (1) __asm__ volatile ("hlt");
    }

    // Aloca 5 objetos
    void *objs[5];
    for (int i = 0; i < 5; i++) {
        objs[i] = objpool_alloc(tasks);
        serial_print("[demo] alloc[");
        serial_print_dec(i);
        serial_print("] @");
        serial_print_hex((uint32_t)objs[i]);
        serial_print("\n");
    }

    serial_print("[demo] total=");
    serial_print_dec(objpool_total(tasks));
    serial_print(" used=");
    serial_print_dec(objpool_used(tasks));
    serial_print(" free=");
    serial_print_dec(objpool_free_count(tasks));
    serial_print("\n");

    // Libera 2 e aloca 1
    objpool_free(tasks, objs[0]);
    objpool_free(tasks, objs[2]);

    void *reused = objpool_alloc(tasks);
    serial_print("[demo] realloc @");
    serial_print_hex((uint32_t)reused);
    serial_print(" (deve reusar objs[0] ou objs[2])\n");

    // Tenta liberar ponteiro inválido
    int ret = objpool_free(tasks, (void *)0xDEADBEEF);
    serial_print("[demo] free invalido retornou ");
    serial_print_dec((uint32_t)ret);
    serial_print(" (esperado -1)\n");

    // ===== FIM DEMO =====

    serial_print("\n[boot] Entrando em loop principal\n");
    // ... loop ...
}
