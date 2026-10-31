// kernel.c - Kernel mínimo com saída serial
#include "serial.h"

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
    // VGA
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

    // Serial (inclui '\n' para formatar o terminal)
    if (c == '\n')
        serial_putchar('\r');
    serial_putchar(c);
}

void print(const char *s) {
    while (*s) putchar(*s++);
}

// Protótipo (definido no boot.asm via multiboot)
void kmain(unsigned int magic, unsigned int *mbinfo) {
    serial_init();

    serial_print("[boot] Kernel inicializado\n");

    clear_screen();
    print("=== Kernel Minimo de Teste ===\n");
    print("Boot via Multiboot OK!\n");
    print("Saida VGA + Serial ativa.\n");

    serial_print("[boot] Multiboot magic = ");
    serial_print_hex(magic);
    serial_print("\n");

    serial_print("[boot] mbinfo = ");
    serial_print_hex((unsigned int)mbinfo);
    serial_print("\n");

    if (magic == 0x2BADB002)
        serial_print("[boot] Multiboot valido.\n");
    else
        serial_print("[boot] AVISO: magic inesperado!\n");

    serial_print("[boot] Entrando em loop infinito (hlt)\n");

    while (1)
        __asm__ volatile ("hlt");
}
