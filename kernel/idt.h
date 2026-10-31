// idt.h
#ifndef IDT_H
#define IDT_H

void idt_init(void);

void idt_set_gate(unsigned char num, unsigned int handler, unsigned short sel, unsigned char flags);

struct idt_entry {
    unsigned short base_low;
    unsigned short sel;
    unsigned char  always0;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

#endif
