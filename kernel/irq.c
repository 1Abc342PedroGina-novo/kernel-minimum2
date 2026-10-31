#include "idt.h"
#include "serial.h"
#include "irq.h"

// ---- Portas do PIC ----

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI   0x20
#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

// Vetores após remap
#define IRQ_BASE  0x20   // IRQ0..7  → 0x20..0x27
#define IRQ_BASE2 0x28   // IRQ8..15 → 0x28..0x2F

// ---- I/O helpers ----


static inline void io_wait(void) {
    // Escrever em porta não usada ~ pequena espera
    outb(0x80, 0);
}

// ---- Stubs Assembly (definidos em irq_stubs.asm) ----

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

// ---- Tabela de handlers ----

static void (*irq_handlers[16])(struct regs *) = { 0 };

// ---- Remap do PIC ----

static void pic_remap(void) {
    // Salva máscaras atuais
    unsigned char mask1 = inb(PIC1_DATA);
    unsigned char mask2 = inb(PIC2_DATA);

    // ICW1: inicia o processo de inicialização
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();

    // ICW2: offsets dos vetores
    outb(PIC1_DATA, IRQ_BASE);  io_wait();   // master → 0x20
    outb(PIC2_DATA, IRQ_BASE2); io_wait();   // slave  → 0x28

    // ICW3: conexão master/slave
    outb(PIC1_DATA, 0x04); io_wait();        // slave no IRQ2 do master
    outb(PIC2_DATA, 0x02); io_wait();        // slave cascateado no IRQ2

    // ICW4: modo 8086
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    // Restaura máscaras (nada habilitado ainda)
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

// ---- Dispatcher chamado pelo stub Assembly ----

void irq_handler(struct regs *r) {
    unsigned int irq = r->int_no - IRQ_BASE;

    if (irq < 16 && irq_handlers[irq] != 0) {
        irq_handlers[irq](r);
    } else {
        // IRQ espúria ou sem handler — loga e segue
        serial_print("[irq] IRQ ");
        serial_print_dec(irq);
        serial_print(" sem handler (int_no=");
        serial_print_hex(r->int_no);
        serial_print(")\n");
    }

    // EOI: se veio do slave (IRQ 8..15), manda pro slave e depois master
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

// ---- API pública ----

void irq_init(void) {
    serial_print("[irq] remapeando PIC (master=0x20, slave=0x28)\n");
    pic_remap();

    // Instala os 16 stubs na IDT com atributos de interrupt gate
    const unsigned char attr = 0x8E;
    idt_set_gate(0x20, (unsigned int)irq0,  0x08, attr);
    idt_set_gate(0x21, (unsigned int)irq1,  0x08, attr);
    idt_set_gate(0x22, (unsigned int)irq2,  0x08, attr);
    idt_set_gate(0x23, (unsigned int)irq3,  0x08, attr);
    idt_set_gate(0x24, (unsigned int)irq4,  0x08, attr);
    idt_set_gate(0x25, (unsigned int)irq5,  0x08, attr);
    idt_set_gate(0x26, (unsigned int)irq6,  0x08, attr);
    idt_set_gate(0x27, (unsigned int)irq7,  0x08, attr);
    idt_set_gate(0x28, (unsigned int)irq8,  0x08, attr);
    idt_set_gate(0x29, (unsigned int)irq9,  0x08, attr);
    idt_set_gate(0x2A, (unsigned int)irq10, 0x08, attr);
    idt_set_gate(0x2B, (unsigned int)irq11, 0x08, attr);
    idt_set_gate(0x2C, (unsigned int)irq12, 0x08, attr);
    idt_set_gate(0x2D, (unsigned int)irq13, 0x08, attr);
    idt_set_gate(0x2E, (unsigned int)irq14, 0x08, attr);
    idt_set_gate(0x2F, (unsigned int)irq15, 0x08, attr);

    // Mascara TODAS as IRQs por padrão (ninguém dispara até habilitar)
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    serial_print("[irq] 16 stubs instalados na IDT (0x20..0x2F)\n");
    serial_print("[irq] todas as IRQs mascaradas — habilite com irq_clear_mask()\n");
}

void irq_register_handler(unsigned char irq, void (*handler)(struct regs *)) {
    if (irq < 16)
        irq_handlers[irq] = handler;
}

void irq_unregister_handler(unsigned char irq) {
    if (irq < 16)
        irq_handlers[irq] = 0;
}

void irq_set_mask(unsigned char irq) {
    unsigned short port;
   unsigned char value;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void irq_clear_mask(unsigned char irq) {
    unsigned short port;
    unsigned char value;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}
