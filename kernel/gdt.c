// gdt.c - Global Descriptor Table mínima (flat model)
#include "gdt.h"
#include "serial.h"
#include "tss.h"
 
// ---- Estado ----

#define GDT_ENTRIES 6

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gp;

// ---- Helpers ----

// Access byte:
//   bit 7: Present
//   bit 6-5: DPL (0=kernel, 3=user)
//   bit 4: Descriptor type (1=code/data)
//   bit 3: Executable (1=code)
//   bit 2: Direction/Conforming
//   bit 1: Read/Write
//   bit 0: Accessed
#define GDT_ACCESS_PRESENT   0x80
#define GDT_ACCESS_RING0     0x00
#define GDT_ACCESS_RING3     0x60
#define GDT_ACCESS_CODE_DATA 0x10
#define GDT_ACCESS_EXEC      0x08
#define GDT_ACCESS_RW        0x02

// Granularity byte:
//   bit 7: Granularity (1 = limite em páginas de 4 KiB)
//   bit 6: Size (1 = 32-bit)
//   bit 5: Long mode (0 em 32-bit)
//   bit 4: AVL
#define GDT_GRAN_4K          0x80
#define GDT_GRAN_32BIT       0x40

static void gdt_set_entry(int idx, unsigned int base, unsigned int limit,
                          unsigned char access, unsigned char gran) {
    gdt[idx].base_low    = (base & 0xFFFF);
    gdt[idx].base_middle = (base >> 16) & 0xFF;
    gdt[idx].base_high   = (base >> 24) & 0xFF;

    gdt[idx].limit_low   = (limit & 0xFFFF);
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);

    gdt[idx].access      = access;
}

// ---- Entrada ----

// Instala um descritor de TSS (formato especial: type=0x9, S=0)
void gdt_set_tss(int idx, unsigned int base, unsigned int limit, unsigned char flags) {
    if (idx < 0 || idx >= GDT_ENTRIES) return;

    gdt[idx].base_low    = (base & 0xFFFF);
    gdt[idx].base_middle = (base >> 16) & 0xFF;
    gdt[idx].base_high   = (base >> 24) & 0xFF;

    gdt[idx].limit_low   = (limit & 0xFFFF);
    gdt[idx].granularity = ((limit >> 16) & 0x0F);

    // TSS: o byte 'access' é 0x89 (Present | DPL0 | Type=1001b = TSS 32-bit available)
    // O bit 4 (S) deve ser 0 para system segments (TSS, LDT), diferente de code/data.
    gdt[idx].access      = flags;
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (unsigned int)&gdt;

    // 0: null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);

    // 1: kernel code (ring 0), base=0, limite=4 GiB (flat)
    gdt_set_entry(1, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 |
                  GDT_ACCESS_CODE_DATA | GDT_ACCESS_EXEC | GDT_ACCESS_RW,
                  GDT_GRAN_4K | GDT_GRAN_32BIT);

    // 2: kernel data (ring 0), base=0, limite=4 GiB
    gdt_set_entry(2, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 |
                  GDT_ACCESS_CODE_DATA | GDT_ACCESS_RW,
                  GDT_GRAN_4K | GDT_GRAN_32BIT);

    // 3: user code (ring 3)
    gdt_set_entry(3, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 |
                  GDT_ACCESS_CODE_DATA | GDT_ACCESS_EXEC | GDT_ACCESS_RW,
                  GDT_GRAN_4K | GDT_GRAN_32BIT);

    // 4: user data (ring 3)
    gdt_set_entry(4, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 |
                  GDT_ACCESS_CODE_DATA | GDT_ACCESS_RW,
                  GDT_GRAN_4K | GDT_GRAN_32BIT);

    serial_print("[gdt] GDT carregada (5 entradas, flat, base=0, limite=4GiB)\n");
    serial_print("[gdt] endereco base = ");
    serial_print_hex(gp.base);
    serial_print(", limite = ");
    serial_print_dec(gp.limit);
    serial_print("\n");

    // Carrega a GDT e recarrega os segmentos
    __asm__ volatile (
        "lgdt %0\n"
        "ljmp $0x08, $1f\n"     // recarrega CS com seletor 0x08 (kernel code)
        "1:\n"
        "mov $0x10, %%ax\n"     // seletor 0x10 = kernel data
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        :
        : "m"(gp)
        : "eax", "memory"
    );

    serial_print("[gdt] segmentos recarregados (CS=0x08, DS/SS=0x10)\n");
}
