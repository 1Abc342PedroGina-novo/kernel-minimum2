#ifndef _OBJPOOL_H
#define _OBJPOOL_H

#include "serial.h"

#define OBJPOOL_MAX_SLABS 16   // máximo de páginas por pool

struct objpool;

#define PAGE_SIZE  4096
#define ALIGN(x,a) (((x) + (a) - 1) & ~((a) - 1))

// ---- Estruturas ----

struct free_obj {
    struct free_obj *next;   // encadeia objetos livres
};

struct slab {
    struct slab     *next;       // próximo slab do pool
    struct free_obj *free_list;  // lista de objetos livres neste slab
    unsigned int         free_count; // quantos livres neste slab
    unsigned char         *mem;        // início da memória (dentro da página)
    // o resto da página é usada pelos objetos
};

struct objpool {
    unsigned int obj_size;      // tamanho alinhado do objeto
    unsigned int objs_per_slab; // objetos por página
    unsigned int slab_count;    // slabs alocados
    unsigned int total;         // objetos totais
    unsigned int used;          // objetos em uso
    struct slab *slabs;     // lista de slabs
    char name[16];
};

struct objpool *objpool_create(unsigned int obj_size, unsigned int objs_per_slab);

// Aloca 1 objeto. Retorna NULL se o pool estiver cheio.
void *objpool_alloc(struct objpool *pool);

// Libera um objeto. Retorna 0 em sucesso, -1 se o ponteiro não pertence ao pool.
int objpool_free(struct objpool *pool, void *obj);

// Estatísticas
unsigned int objpool_total(struct objpool *pool);
unsigned int objpool_used(struct objpool *pool);
unsigned int objpool_free_count(struct objpool *pool);

#endif
