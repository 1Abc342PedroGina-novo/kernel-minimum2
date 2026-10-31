// idt.c - IDT + handlers de exceção
#include "idt.h"
#include "isr.h"
#include "serial.h"
#include "irq.h"

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

// ---- Stubs Assembly (definidos em isr.asm) ----

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

// ---- Tabela de nomes legíveis ----

static const char *exception_names[32] = {
    "Divide by Zero",        "Debug",
    "NMI",                   "Breakpoint",
    "Overflow",              "Bound Range Exceeded",
    "Invalid Opcode",        "Device Not Available",
    "Double Fault",          "Coprocessor Segment Overrun",
    "Invalid TSS",           "Segment Not Present",
    "Stack-Segment Fault",   "General Protection Fault",
    "Page Fault",            "Reserved",
    "x87 FP Exception",      "Alignment Check",
    "Machine Check",         "SIMD FP Exception",
    "Virtualization",        "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Security",              "Reserved"
};

// ---- Implementação ----

void idt_set_gate(unsigned char num, unsigned int handler, unsigned short sel, unsigned char flags) {
    idt[num].base_low  = handler & 0xFFFF;
    idt[num].base_high = (handler >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (unsigned int)&idt;

    // Zera todas as entradas (não-presentes)
    for (int i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate((unsigned char)i, 0, 0, 0);

    // Atributos: Present | DPL0 | 32-bit interrupt gate (0x8E)
    const unsigned char attr = 0x8E;

    // Instala as 32 exceções
    idt_set_gate(0,  (unsigned int)isr0,  0x08, attr);
    idt_set_gate(1,  (unsigned int)isr1,  0x08, attr);
    idt_set_gate(2,  (unsigned int)isr2,  0x08, attr);
    idt_set_gate(3,  (unsigned int)isr3,  0x08, attr);
    idt_set_gate(4,  (unsigned int)isr4,  0x08, attr);
    idt_set_gate(5,  (unsigned int)isr5,  0x08, attr);
    idt_set_gate(6,  (unsigned int)isr6,  0x08, attr);
    idt_set_gate(7,  (unsigned int)isr7,  0x08, attr);
    idt_set_gate(8,  (unsigned int)isr8,  0x08, attr);
    idt_set_gate(9,  (unsigned int)isr9,  0x08, attr);
    idt_set_gate(10, (unsigned int)isr10, 0x08, attr);
    idt_set_gate(11, (unsigned int)isr11, 0x08, attr);
    idt_set_gate(12, (unsigned int)isr12, 0x08, attr);
    idt_set_gate(13, (unsigned int)isr13, 0x08, attr);
    idt_set_gate(14, (unsigned int)isr14, 0x08, attr);
    idt_set_gate(15, (unsigned int)isr15, 0x08, attr);
    idt_set_gate(16, (unsigned int)isr16, 0x08, attr);
    idt_set_gate(17, (unsigned int)isr17, 0x08, attr);
    idt_set_gate(18, (unsigned int)isr18, 0x08, attr);
    idt_set_gate(19, (unsigned int)isr19, 0x08, attr);
    idt_set_gate(20, (unsigned int)isr20, 0x08, attr);
    idt_set_gate(21, (unsigned int)isr21, 0x08, attr);
    idt_set_gate(22, (unsigned int)isr22, 0x08, attr);
    idt_set_gate(23, (unsigned int)isr23, 0x08, attr);
    idt_set_gate(24, (unsigned int)isr24, 0x08, attr);
    idt_set_gate(25, (unsigned int)isr25, 0x08, attr);
    idt_set_gate(26, (unsigned int)isr26, 0x08, attr);
    idt_set_gate(27, (unsigned int)isr27, 0x08, attr);
    idt_set_gate(28, (unsigned int)isr28, 0x08, attr);
    idt_set_gate(29, (unsigned int)isr29, 0x08, attr);
    idt_set_gate(30, (unsigned int)isr30, 0x08, attr);
    idt_set_gate(31, (unsigned int)isr31, 0x08, attr);

    // Carrega a IDT
    __asm__ volatile ("lidt %0" : : "m"(idtp));

    serial_print("[idt] IDT carregada (256 entradas, base=");
    serial_print_hex(idtp.base);
    serial_print(", limite=");
    serial_print_dec(idtp.limit);
    serial_print(")\n");
    serial_print("[idt] handlers de excecao instalados (0x00..0x1F)\n");
}

// ---- Handler C chamado pelos stubs ----

void isr_handler(struct regs *r) {
    // Vetores 0x20..0x2F são IRQs — delega
    if (r->int_no >= 0x20 && r->int_no < 0x30) {
        irq_handler(r);
        return;
    }

    serial_print("[isr] entrou com int_no=");
serial_print_hex(r->int_no);
serial_print("\n");

    
    // Exceção real (0x00..0x1F): dump e halt
    serial_print("\n[exception] ");
    if (r->int_no < 32) {
        serial_print(exception_names[r->int_no]);
        serial_print(" (");
        serial_print_dec(r->int_no);
        serial_print(")");
    } else {
        serial_print("desconhecido (");
        serial_print_dec(r->int_no);
        serial_print(")");
    }
    serial_print(" err=");
    serial_print_hex(r->err_code);
    serial_print("\n");

    serial_print("[exception]   EIP=");
    serial_print_hex(r->eip);
    serial_print("  CS=");
    serial_print_hex(r->cs);
    serial_print("  EFLAGS=");
    serial_print_hex(r->eflags);
    serial_print("\n");

    if (r->int_no == 14) {
        unsigned int cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        serial_print("[exception]   CR2=");
        serial_print_hex(cr2);
        serial_print("\n");
    }

    serial_print("[exception]   EAX=");
    serial_print_hex(r->eax);
    serial_print("  EBX=");
    serial_print_hex(r->ebx);
    serial_print("  ECX=");
    serial_print_hex(r->ecx);
    serial_print("  EDX=");
    serial_print_hex(r->edx);
    serial_print("\n");

    serial_print("[exception] --- halt ---\n");
    while (1)
        __asm__ volatile ("hlt");
}
