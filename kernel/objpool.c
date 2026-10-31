#include "objpool.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

static unsigned int next_pool_addr = 0x00200000; // começa em 2 MiB (dentro dos 4 MiB mapeados)

// Aloca uma página fisicamente contígua (bump allocator sobre a região identity-mapped)
static void *alloc_page(void) {
    void *p = (void *)next_pool_addr;
    next_pool_addr += PAGE_SIZE;
    if (next_pool_addr > 0x00400000) {
        serial_print("[objpool] ERRO: memória esgotada (acima de 4 MiB)\n");
        return NULL;
    }
    return p;
}

// Zera uma página
static void zero_page(void *p) {
    unsigned char *b = (unsigned char *)p;
    for (unsigned int i = 0; i < PAGE_SIZE; i++) b[i] = 0;
}

// ---- API ----

struct objpool *objpool_create(unsigned int obj_size, unsigned int objs_per_slab) {
    if (obj_size == 0 || objs_per_slab == 0) return NULL;

    // Alinha o tamanho do objeto a 8 bytes
    obj_size = ALIGN(obj_size, 8);
    if (obj_size * objs_per_slab > PAGE_SIZE) {
        serial_print("[objpool] ERRO: obj_size * objs_per_slab > 4096\n");
        return NULL;
    }

    // Aloca o pool (fora de páginas, no mesmo bump allocator)
    struct objpool *pool = (struct objpool *)next_pool_addr;
    next_pool_addr += ALIGN(sizeof(struct objpool), 8);
    if (next_pool_addr > 0x00400000) {
        serial_print("[objpool] ERRO: sem espaco para o pool\n");
        return NULL;
    }

    pool->obj_size      = obj_size;
    pool->objs_per_slab = objs_per_slab;
    pool->slab_count    = 0;
    pool->total         = 0;
    pool->used          = 0;
    pool->slabs         = NULL;

    serial_print("[objpool] pool criado: obj_size=");
    serial_print_dec(obj_size);
    serial_print(" objs_per_slab=");
    serial_print_dec(objs_per_slab);
    serial_print(" pool@");
    serial_print_hex((unsigned int)pool);
    serial_print("\n");

    return pool;
}

// Aloca um novo slab (página) e o adiciona ao pool
static int objpool_grow(struct objpool *pool) {
    if (pool->slab_count >= OBJPOOL_MAX_SLABS) {
        serial_print("[objpool] max slabs atingido\n");
        return -1;
    }

    unsigned char *page = (unsigned char *)alloc_page();
    if (!page) return -1;
    zero_page(page);

    // O slab (metadados) fica no início da página, seguido pelos objetos
    struct slab *slab = (struct slab *)page;
    slab->next       = pool->slabs;
    slab->free_count = pool->objs_per_slab;
    slab->mem        = page + ALIGN(sizeof(struct slab), 8);

    // Encadeia todos os objetos livres
    slab->free_list = NULL;
    unsigned char *obj = slab->mem;
    for (unsigned int i = 0; i < pool->objs_per_slab; i++) {
        struct free_obj *f = (struct free_obj *)obj;
        f->next = slab->free_list;
        slab->free_list = f;
        obj += pool->obj_size;
    }

    pool->slabs = slab;
    pool->slab_count++;
    pool->total += pool->objs_per_slab;

    serial_print("[objpool] slab criado @");
    serial_print_hex((unsigned int)page);
    serial_print(" (slab_count=");
    serial_print_dec(pool->slab_count);
    serial_print(")\n");

    return 0;
}

void *objpool_alloc(struct objpool *pool) {
    if (!pool) return NULL;

    // Procura slab com objeto livre
    struct slab *s = pool->slabs;
    while (s && !s->free_list) s = s->next;

    // Se nenhum, cresce
    if (!s) {
        if (objpool_grow(pool) != 0) return NULL;
        s = pool->slabs;
    }

    // Retira da free list
    struct free_obj *obj = s->free_list;
    s->free_list = obj->next;
    s->free_count--;
    pool->used++;

    return (void *)obj;
}

int objpool_free(struct objpool *pool, void *obj) {
    if (!pool || !obj) return -1;

    // Verifica se o ponteiro pertence a algum slab do pool
    unsigned long addr = (unsigned long)obj;
    struct slab *s = pool->slabs;
    while (s) {
        unsigned char *start = s->mem;
        unsigned char *end   = start + pool->obj_size * pool->objs_per_slab;
        if (addr >= (unsigned long)start && addr < (unsigned long)end) {
            // Valida alinhamento (deve ser múltiplo de obj_size a partir de start)
            if ((addr - (unsigned long)start) % pool->obj_size != 0) {
                serial_print("[objpool] ERRO: ponteiro desalinhado\n");
                return -1;
            }

            // Devolve para a free list
            struct free_obj *f = (struct free_obj *)obj;
            f->next = s->free_list;
            s->free_list = f;
            s->free_count++;
            pool->used--;

            // Se o slab ficou vazio, pode ser liberado (opcional — não fazemos agora)
            return 0;
        }
        s = s->next;
    }

    serial_print("[objpool] ERRO: ponteiro nao pertence ao pool\n");
    return -1;
}

unsigned int objpool_total(struct objpool *pool)      { return pool ? pool->total : 0; }
unsigned int objpool_used(struct objpool *pool)       { return pool ? pool->used : 0; }
unsigned int objpool_free_count(struct objpool *pool) { return pool ? pool->total - pool->used : 0; }
