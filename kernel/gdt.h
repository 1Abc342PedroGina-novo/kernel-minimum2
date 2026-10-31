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

#endif
