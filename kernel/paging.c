#include "paging.h"
#include "serial.h"

// Page directory: 1024 entradas de 4 bytes = 4 KiB
// Page table:    1024 entradas de 4 bytes = 4 KiB cada
// Cobrimos 4 MiB → 1 page table (1024 páginas × 4 KiB = 4 MiB)

#define PAGE_DIR_ENTRIES 1024
#define PAGE_TBL_ENTRIES 1024
#define MAPPED_MIB       4
#define MAPPED_BYTES     (MAPPED_MIB * 1024 * 1024)

static unsigned int page_directory[PAGE_DIR_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static unsigned int page_table[PAGE_TBL_ENTRIES]    __attribute__((aligned(PAGE_SIZE)));

// ---- Helpers internos ----

void invlpg(unsigned int virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

// Alinha 'addr' para baixo ao múltiplo de 4096
unsigned int align_down(unsigned int addr) {
    return addr & ~(PAGE_SIZE - 1);
}

// Alinha 'addr' para cima ao múltiplo de 4096
static inline unsigned int align_up(unsigned int addr) {
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

// --- API pública ---

void paging_init(void) {
    serial_print("[paging] inicializando paginacao\n");

    // 1. Zera page directory e a primeira page table
    for (int i = 0; i < PAGE_DIR_ENTRIES; i++)
        page_directory[i] = 0;
    for (int i = 0; i < PAGE_TBL_ENTRIES; i++)
        page_table[i] = 0;

    // 2. Identity-map dos primeiros 4 MiB
    //    Page directory[0] -> page_table
    //    page_table[i] -> i * 4096 | PRESENT | WRITE
    page_directory[0] = (unsigned int)page_table | PAGE_PRESENT | PAGE_WRITE;

    for (int i = 0; i < (MAPPED_BYTES / PAGE_SIZE); i++) {
        page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
    }

    serial_print("[paging] page_directory @ ");
    serial_print_hex((unsigned int)page_directory);
    serial_print(", page_table @ ");
    serial_print_hex((unsigned int)page_table);
    serial_print("\n");

    // 3. Carrega CR3 com o page directory
    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory) : "memory");

    // 4. Liga paginação: CR0.PG (bit 31)
    unsigned int cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1u << 31);
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");

    serial_print("[paging] identity-mapped 4 MiB (0x00000000..0x003FFFFF)\n");
    serial_print("[paging] CR0.PG ligado, paging ATIVO\n");
}

int paging_map_range(unsigned int start, unsigned int end, unsigned int flags) {
    if (start > end) return -1;

    start = align_down(start);
    end   = align_up(end);

    // Só suportamos o primeiro MiB de diretório (4 GiB via uma page table).
    // Para o teste atual, basta: o range deve caber no page_table[0].
    if (end > MAPPED_BYTES) {
        serial_print("[paging] ERRO: map_range fora dos 4 MiB (range=");
        serial_print_hex(start);
        serial_print("..");
        serial_print_hex(end);
        serial_print(")\n");
        return -1;
    }

    for (unsigned int a = start; a < end; a += PAGE_SIZE) {
        unsigned int idx = a / PAGE_SIZE;
        page_table[idx] = a | (flags & (PAGE_PRESENT | PAGE_WRITE | PAGE_USER));
        invlpg(a);
    }
    return 0;
}

void paging_unmap_range(unsigned int start, unsigned int end) {
    if (start > end) return;
    start = align_down(start);
    end   = align_up(end);
    if (end > MAPPED_BYTES) end = MAPPED_BYTES;

    for (unsigned int a = start; a < end; a += PAGE_SIZE) {
        unsigned int idx = a / PAGE_SIZE;
        page_table[idx] = 0;
        invlpg(a);
    }
}
