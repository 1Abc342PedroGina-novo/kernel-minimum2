#ifndef MM_INFO
#define MM_INFO

// Dump consolidado do PMM (bitmap stats, reservas).
void pmm_dump(void);

// Dump consolidado: PMM + VMM.
void mem_dump(void);

// Dump das primeiras N entradas do bitmap (frames em uso).
void pmm_dump_bitmap(unsigned int n_words);

// Dump do heap (regiões conhecidas do kernel: .bss, bitmap, etc).
void heap_dump(void);

#endif
