// vmm.c - Gerenciador de memória virtual (sobre paging)
#include "vmm.h"
#include "pmm.h"
#include "serial.h"

#define PAGE_SIZE  4096
#define PAGE_SHIFT 12
#define PD_INDEX(v) (((v) >> 22) & 0x3FF)
#define PT_INDEX(v) (((v) >> 12) & 0x3FF)

// ---- Estado ----

static unsigned int *page_directory = 0;      // PD gerenciado pelo VMM
static unsigned int  pt_count = 0;           // page tables alocadas
static unsigned int  mapped_pages = 0;       // total de páginas mapeadas

// ---- Helpers ----

static inline void invlpg(unsigned int virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static inline void load_cr3(unsigned int pd_phys) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys) : "memory");
}

static inline unsigned int read_cr3(void) {
    unsigned int v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}

// Aloca e zera 1 frame via PMM. Retorna endereço físico.
static unsigned int alloc_frame_zeroed(void) {
    unsigned int *p = (unsigned int *)pmm_alloc_frame();
    if (!p) return 0;
    for (unsigned int i = 0; i < PAGE_SIZE / 4; i++) p[i] = 0;
    return (unsigned int)p;
}

// ---- API ----

void vmm_init(void) {
    // 1. Aloca um PD novo via PMM
    unsigned int *new_pd = (unsigned int *)alloc_frame_zeroed();
    if (!new_pd) {
        serial_print("[vmm] ERRO: sem frame para PD\n");
        return;
    }

    // 2. Copia o PD atual (do paging_init) para o novo
    //    O CR3 atual aponta para o PD do paging.c
    unsigned int *old_pd = (unsigned int *)(read_cr3() & ~0xFFF);
    for (unsigned int i = 0; i < 1024; i++) new_pd[i] = old_pd[i];

    // 3. Troca o CR3
    load_cr3((unsigned int)new_pd);
    page_directory = new_pd;

    // 4. Conta page tables existentes (para estatística)
    pt_count = 0;
    for (unsigned int i = 0; i < 1024; i++) {
        if (new_pd[i] & VMM_PRESENT) pt_count++;
    }

    // 5. Conta páginas mapeadas
    mapped_pages = 0;
    for (unsigned int i = 0; i < 1024; i++) {
        if (!(new_pd[i] & VMM_PRESENT)) continue;
        unsigned int *pt = (unsigned int *)(new_pd[i] & ~0xFFF);
        for (unsigned int j = 0; j < 1024; j++) {
            if (pt[j] & VMM_PRESENT) mapped_pages++;
        }
    }

    serial_print("[vmm] PD @ ");
    serial_print_hex((unsigned int)page_directory);
    serial_print(", PTs=");
    serial_print_dec(pt_count);
    serial_print(", paginas mapeadas=");
    serial_print_dec(mapped_pages);
    serial_print("\n");
}

// Garante que a PT para 'virt' existe. Retorna ponteiro para a PT (virtual).
// Se alocou, retorna o endereço físico da PT em *pt_phys_out.
static unsigned int *get_or_create_pt(unsigned int virt, unsigned int flags, unsigned int *pt_phys_out) {
    unsigned int pdi = PD_INDEX(virt);

    if (!(page_directory[pdi] & VMM_PRESENT)) {
        // Aloca PT nova
        unsigned int pt_phys = alloc_frame_zeroed();
        if (!pt_phys) return 0;
        // Flags: presente, escrita, e as flags do chamador (sem dirty/accessed)
        page_directory[pdi] = pt_phys | VMM_PRESENT | VMM_WRITE | (flags & VMM_USER);
        pt_count++;
        *pt_phys_out = pt_phys;
        // Identity-map do frame (o kernel vê tudo em 4 MiB)
        return (unsigned int *)pt_phys;
    }

    *pt_phys_out = page_directory[pdi] & ~0xFFF;
    return (unsigned int *)(page_directory[pdi] & ~0xFFF);
}

int vmm_map(unsigned int virt, unsigned int phys, unsigned int flags) {
    if (!page_directory) return -1;
    if (virt & (PAGE_SIZE - 1)) return -1;
    if (phys & (PAGE_SIZE - 1)) return -1;

    unsigned int pt_phys;
    unsigned int *pt = get_or_create_pt(virt, flags, &pt_phys);
    if (!pt) {
        serial_print("[vmm] ERRO: sem PT para ");
        serial_print_hex(virt);
        serial_print("\n");
        return -1;
    }

    unsigned int pti = PT_INDEX(virt);
    if (pt[pti] & VMM_PRESENT) {
        // Já mapeado — atualiza flags (mas mantém o físico)
        pt[pti] = (pt[pti] & ~0xFFF) | (flags & 0xFFF);
    } else {
        pt[pti] = (phys & ~0xFFF) | (flags & 0xFFF);
        mapped_pages++;
    }

    invlpg(virt);
    return 0;
}

int vmm_unmap(unsigned int virt) {
    if (!page_directory) return -1;
    if (virt & (PAGE_SIZE - 1)) return -1;

    unsigned int pdi = PD_INDEX(virt);
    if (!(page_directory[pdi] & VMM_PRESENT)) return -1;

    unsigned int *pt = (unsigned int *)(page_directory[pdi] & ~0xFFF);
    unsigned int pti = PT_INDEX(virt);
    if (!(pt[pti] & VMM_PRESENT)) return -1;

    pt[pti] = 0;
    mapped_pages--;
    invlpg(virt);
    return 0;
}

int vmm_map_range(unsigned int virt, unsigned int phys, unsigned int n, unsigned int flags) {
    for (unsigned int i = 0; i < n; i++) {
        if (vmm_map(virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, flags) != 0)
            return -1;
    }
    return 0;
}

int vmm_unmap_range(unsigned int virt, unsigned int n) {
    for (unsigned int i = 0; i < n; i++) {
        if (vmm_unmap(virt + i * PAGE_SIZE) != 0) return -1;
    }
    return 0;
}

unsigned int vmm_get_phys(unsigned int virt) {
    if (!page_directory) return 0;

    unsigned int pdi = PD_INDEX(virt);
    if (!(page_directory[pdi] & VMM_PRESENT)) return 0;

    unsigned int *pt = (unsigned int *)(page_directory[pdi] & ~0xFFF);
    unsigned int pti = PT_INDEX(virt);
    if (!(pt[pti] & VMM_PRESENT)) return 0;

    unsigned int phys = (pt[pti] & ~0xFFF) | (virt & (PAGE_SIZE - 1));
    return phys;
}

// ---- Introspecção ----

void vmm_dump(void) {
    if (!page_directory) {
        serial_print("[vmm] dump: PD nao inicializado\n");
        return;
    }

    serial_print("\n[vmm] === Page Directory ===\n");
    serial_print("[vmm] PD @ ");
    serial_print_hex((unsigned int)page_directory);
    serial_print(", PTs=");
    serial_print_dec(pt_count);
    serial_print(", paginas=");
    serial_print_dec(mapped_pages);
    serial_print("\n");

    for (unsigned int i = 0; i < 1024; i++) {
        unsigned int pde = page_directory[i];
        if (!(pde & VMM_PRESENT)) continue;

        unsigned int *pt = (unsigned int *)(pde & ~0xFFF);
        unsigned int count = 0;
        for (unsigned int j = 0; j < 1024; j++)
            if (pt[j] & VMM_PRESENT) count++;

        serial_print("[vmm] PD[");
        serial_print_dec(i);
        serial_print("] = ");
        serial_print_hex(pde);
        serial_print(" (PT @ ");
        serial_print_hex(pde & ~0xFFF);
        serial_print(", ");
        serial_print_dec(count);
        serial_print(" paginas)  virt range: ");
        serial_print_hex(i << 22);
        serial_print("..");
        serial_print_hex(((i + 1) << 22) - 1);
        serial_print("\n");
    }
    serial_print("[vmm] === Fim ===\n\n");
}

void vmm_dump_entry(unsigned int virt) {
    if (!page_directory) return;

    unsigned int pdi = PD_INDEX(virt);
    serial_print("[vmm] virt=");
    serial_print_hex(virt);
    serial_print(" PD[");
    serial_print_dec(pdi);
    serial_print("]=");
    serial_print_hex(page_directory[pdi]);

    if (!(page_directory[pdi] & VMM_PRESENT)) {
        serial_print(" (nao mapeado)\n");
        return;
    }

    unsigned int *pt = (unsigned int *)(page_directory[pdi] & ~0xFFF);
    unsigned int pti = PT_INDEX(virt);
    serial_print(" PT[");
    serial_print_dec(pti);
    serial_print("]=");
    serial_print_hex(pt[pti]);

    if (pt[pti] & VMM_PRESENT) {
        serial_print(" -> phys=");
        serial_print_hex(pt[pti] & ~0xFFF);
    } else {
        serial_print(" (pagina nao mapeada)");
    }
    serial_print("\n");
}

unsigned int vmm_mapped_pages(void) { return mapped_pages; }
unsigned int vmm_page_tables_used(void) { return pt_count; }
