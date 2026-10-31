// tss.c - Task State Segment
#include "tss.h"
#include "gdt.h"
#include "serial.h"

// ---- Estrutura do TSS (32-bit) ----

struct tss_entry {
    unsigned int prev_tss;      // link para TSS anterior (não usado)
    unsigned int esp0;          // ★ stack do kernel em ring 0
    unsigned int ss0;           // ★ segmento de stack em ring 0 (0x10)
    unsigned int esp1;          // ring 1 (não usado)
    unsigned int ss1;
    unsigned int esp2;          // ring 2 (não usado)
    unsigned int ss2;
    unsigned int cr3;           // hardware task switch (não usado)
    unsigned int eip;
    unsigned int eflags;
    unsigned int eax, ecx, edx, ebx;
    unsigned int esp, ebp, esi, edi;
    unsigned int es, cs, ss, ds, fs, gs;
    unsigned int ldt;           // seletor de LDT (não usado)
    unsigned short trap;          // bit de trap (debug)
    unsigned short iomap_base;    // offset do I/O bitmap (0 = sem bitmap)
} __attribute__((packed));

// ---- Estado ----

static struct tss_entry tss;
static unsigned short tss_sel = 0;

// Stack do kernel para troca ring 3 → ring 0.
// Uma página (4 KiB) acima do .bss. Ajuste conforme seu layout.
static unsigned char kernel_stack[4096] __attribute__((aligned(16)));

// ---- Helpers ----

static void memset_tss(void) {
    unsigned char *p = (unsigned char *)&tss;
    for (unsigned int i = 0; i < sizeof(tss); i++) p[i] = 0;
}

// ---- API ----

void tss_init(void) {
    memset_tss();

    // ★ Campos essenciais para troca ring 3 → ring 0:
    tss.ss0  = 0x10;   // kernel data selector
    tss.esp0 = (unsigned int)(kernel_stack + sizeof(kernel_stack));

    // iomap_base = sizeof(tss) significa "sem I/O bitmap".
    // Ring 3 NÃO pode acessar portas (tudo bloqueado).
    tss.iomap_base = sizeof(tss);

    // Instala o TSS na GDT no índice 5 (seletor 0x28)
    gdt_set_tss(5, (unsigned int)&tss, sizeof(tss) - 1, 0x89);
    // 0x89 = Present | DPL0 | 32-bit TSS available

    tss_sel = 5 * 8;   // 0x28

    // Carrega o task register
    __asm__ volatile ("ltr %0" : : "r"(tss_sel));

    serial_print("[tss] TSS carregado (seletor=");
    serial_print_hex(tss_sel);
    serial_print(", esp0=");
    serial_print_hex(tss.esp0);
    serial_print(", ss0=");
    serial_print_hex(tss.ss0);
    serial_print(")\n");
    serial_print("[tss] stack do kernel: 0x");
    serial_print_hex((unsigned int)kernel_stack);
    serial_print("..0x");
    serial_print_hex((unsigned int)kernel_stack + sizeof(kernel_stack) - 1);
    serial_print("\n");
}

void tss_set_kernel_stack(unsigned int esp0) {
    tss.esp0 = esp0;
}

unsigned short tss_selector(void) {
    return tss_sel;
}
