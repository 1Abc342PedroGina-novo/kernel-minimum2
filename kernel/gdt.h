// gdt.h
#ifndef GDT_H
#define GDT_H

void gdt_init(void);

struct {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char  base_mid;
    unsigned char access;
    unsigned char  flags;
    unsigned char  base_high;
} __attribute__((packed)) desc;

struct gdt_entry {
    unsigned short limit_low;     // bits 0..15 do limite
    unsigned short base_low;      // bits 0..15 da base
    unsigned char  base_middle;   // bits 16..23 da base
    unsigned char  access;        // flags de acesso
    unsigned char  granularity;   // limite alto (4 bits) + flags
    unsigned char  base_high;     // bits 24..31 da base
} __attribute__((packed));

struct gdt_ptr {
    unsigned short limit;         // tamanho da GDT - 1
    unsigned int base;          // endereço linear da GDT
} __attribute__((packed));

#endif
