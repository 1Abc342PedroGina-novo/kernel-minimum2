// tss.c - Task State Segment
#include "tss.h"
#include "gdt.h"
#include "serial.h"

// ---- Estrutura do TSS (32-bit) ----

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
    serial_print("[tss] stack do kernel: ");
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
