// vmm.h
#ifndef VMM_H
#define VMM_H

#define VMM_PRESENT  0x001
#define VMM_WRITE    0x002
#define VMM_USER     0x004
#define VMM_ACCESSED 0x020
#define VMM_DIRTY    0x040
#define VMM_COW      0x200

void vmm_init(void);


int vmm_map(unsigned int virt, unsigned int phys, unsigned int flags);

// Desmapeia 1 página. Retorna 0 em sucesso, -1 se não mapeada.
int vmm_unmap(unsigned int virt);

// Mapeia 'n' páginas contíguas: virt..virt+n*4K → phys..phys+n*4K.
int vmm_map_range(unsigned int virt, unsigned int phys, unsigned int n, unsigned int flags);

// Desmapeia 'n' páginas.
int vmm_unmap_range(unsigned int virt, unsigned int n);

// Traduz virtual → físico. Retorna 0 se não mapeada.
unsigned int vmm_get_phys(unsigned int virt);

// Estatísticas / introspecção
void vmm_dump(void);                       // dump do PD
void vmm_dump_entry(unsigned int virt);        // dump de 1 entrada (PD + PT)
unsigned int vmm_mapped_pages(void);           // total de páginas mapeadas
unsigned int vmm_page_tables_used(void);       // PTs alocadas

// Aloca 'n' frames físicos do PMM e mapeia em virt..virt+n*4K.
// Retorna virt em sucesso, 0 em erro.
// Se virt == 0, o VMM escolhe um endereço livre (busca linear).
unsigned int vmm_alloc(unsigned int virt, unsigned int n, unsigned int flags);

// Desmapeia 'n' páginas a partir de virt e libera o físico de volta ao PMM.
// Retorna 0 em sucesso.
int vmm_free(unsigned int virt, unsigned int n);
#endif
