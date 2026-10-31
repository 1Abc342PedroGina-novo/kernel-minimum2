// pmm.h
#ifndef PMM_H
#define PMM_H


// Inicializa o PMM cobrindo [0, total_bytes).
//   total_bytes = tamanho da RAM a gerenciar (múltiplo de 4 KiB)
//   bitmap_addr = endereço onde o bitmap será armazenado (deve estar livre!)
//   bitmap_max  = quantos bytes de bitmap cabem lá
// Retorna 0 em sucesso.
int pmm_init(unsigned int total_bytes, void *bitmap_addr, unsigned int bitmap_max);

// Aloca 1 frame de 4 KiB. Retorna endereço físico ou NULL.
void *pmm_alloc_frame(void);

// Libera 1 frame. Retorna 0 em sucesso, -1 se inválido.
int pmm_free_frame(void *addr);

// Aloca 'n' frames contíguos. Retorna endereço físico ou NULL.
void *pmm_alloc_frames(unsigned int n);

// Libera 'n' frames contíguos.
void pmm_free_frames(void *addr, unsigned int n);

// Marca um range como usado (para reservar kernel, bitmap, etc.)
int pmm_reserve(unsigned int start, unsigned int end);

// Marca um range como livre.
int pmm_release(unsigned int start, unsigned int end);

// Estatísticas
unsigned int pmm_total_frames(void);
unsigned int pmm_used_frames(void);
unsigned int pmm_free_frames(void);

#endif
