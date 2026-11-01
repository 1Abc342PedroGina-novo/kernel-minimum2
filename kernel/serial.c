// serial.c - Driver para UART 16550 (COM1)
#include "serial.h"

#define COM1 0x3F8

void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_print_dec64(unsigned long long value) {
    char buf[21];
    int i = 0;
    if (value == 0) { serial_putchar('0'); return; }
    while (value > 0) {
        buf[i++] = '0' + (uint64_t)(value % 10);
        value /= 10;
    }
    while (i-- > 0) serial_putchar(buf[i]);
}

void serial_init(void) {
    outb(COM1 + 1, 0x00); // desabilita interrupções
    outb(COM1 + 3, 0x80); // habilita DLAB
    outb(COM1 + 0, 0x03); // divisor low  = 3 -> 38400 baud
    outb(COM1 + 1, 0x00); // divisor high = 0
    outb(COM1 + 3, 0x03); // 8 bits, sem paridade, 1 stop bit
    outb(COM1 + 2, 0xC7); // FIFO habilitado, limpo, 14 bytes
    outb(COM1 + 4, 0x0B); // IRQs off, RTS/DSR set
}

static int serial_tx_ready(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (!serial_tx_ready())
        ;
    outb(COM1, (unsigned char)c);
}

void serial_print(const char *s) {
    while (*s)
        serial_putchar(*s++);
}

void serial_print_hex(unsigned int value) {
    const char *hex = "0123456789ABCDEF";
    serial_print("0x");
    for (int i = 28; i >= 0; i -= 4)
        serial_putchar(hex[(value >> i) & 0xF]);
}

void serial_print_dec(unsigned int value) {
    char buf[11];
    int i = 0;
    if (value == 0) {
        serial_putchar('0');
        return;
    }
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    while (i-- > 0)
        serial_putchar(buf[i]);
}
