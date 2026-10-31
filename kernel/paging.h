#ifndef PAGING_H
#define PAGING_H


// Inicializa paginação (4 KiB), identity-mapping dos primeiros 4 MiB.
// Deve ser chamada DEPOIS de gdt_init() e idt_init(), porque se algo
// der errado, a IDT já está lá para capturar #PF.
void paging_init(void);

// Faz identity-map de um range [start, end) com flags.
//   flags: PAGE_PRESENT | PAGE_WRITE | PAGE_USER
// Retorna 0 em sucesso, -1 se faltar page table.
int paging_map_range(unsigned int start, unsigned int end, unsigned int flags);

// Desmapeia um range [start, end) (apenas em granularidade de 4 KiB).
void paging_unmap_range(unsigned int start, unsigned int end);

// Helpers de flags
#define PAGE_PRESENT  0x001
#define PAGE_WRITE    0x002
#define PAGE_USER     0x004
#define PAGE_4MB      0x080   // só para PSE (não usado aqui)

#define PAGE_SIZE     4096

#endif
