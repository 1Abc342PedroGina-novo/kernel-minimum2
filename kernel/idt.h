// idt.h
#ifndef IDT_H
#define IDT_H
'.
void idt_init(void);

void idt_set_gate(unsigned char num, unsigned int handler, unsigned short sel, unsigned char flags);

#endif
