// serial.h
#ifndef SERIAL_H
#define SERIAL_H

void serial_init(void);
void serial_putchar(char c);
void serial_print(const char *s);
void serial_print_hex(unsigned int value);
void serial_print_dec(unsigned int value);
void serial_print_dec64(unsigned long long value);

#endif
