#ifndef HEAP
#define HEAP

// Inicializa o heap. Chame após vmm_init().
//   base = endereço virtual base (recomendado: 0x30000000)
//   max  = tamanho máximo do heap em bytes (recomendado: 0x10000000 = 256 MiB)
void heap_init(unsigned int base, unsigned int max_size);

// Aloca 'size' bytes do heap. Retorna ponteiro virtual ou NULL.
// Alinhamento: 8 bytes.
void *heap_alloc(unsigned int size);

// (Opcional) Libera o último bloco alocado se 'ptr + size' == bump.
// Heap é bump-only (sem free real); esta função só "desfaz o último alloc".
// Retorna 0 em sucesso, -1 se não é o último bloco.
int heap_free_last(void *ptr, unsigned int size);

// Estatísticas
unsigned int heap_total_bytes(void);     // total reservado (mapeado)
unsigned int heap_used_bytes(void);      // usado (bump - base)
unsigned int heap_free_bytes(void);      // livre (max - usado)
void     heap_dump(void);

#endif
