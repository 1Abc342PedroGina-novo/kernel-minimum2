#ifndef IRQ_H
#define IRQ_H

#include "isr.h"

void irq_init(void);

// Registra um handler para uma IRQ (0..15).
//   handler = NULL remove o handler.
void irq_register_handler(unsigned char irq, void (*handler)(struct regs *));

// Remove o handler de uma IRQ.
void irq_unregister_handler(unsigned char irq);

// Máscara do PIC: bit=1 desabilita. Útil para mascarar IRQs não usadas.
void irq_set_mask(unsigned char  irq);
void irq_clear_mask(unsigned char  irq);

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
#endif
