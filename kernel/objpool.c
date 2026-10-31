#include "objpool.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

static unsigned int next_pool_addr = 0x00200000;

// ---- Bump allocator ----

// Aloca 'size' bytes alinhados a 8
static void *bump_alloc(unsigned int size) {
    size = ALIGN(size, 8);
    unsigned int p = next_pool_addr;
    next_pool_addr += size;
    if (next_pool_addr > 0x00400000) {
        serial_print("[objpool] ERRO: memoria esgotada\n");
        return NULL;
    }
    return (void *)p;
}

// Aloca uma página alinhada a 4 KiB (para objetos)
static void *alloc_page_aligned(void) {
    // Alinha next_pool_addr para cima, a 4 KiB
    next_pool_addr = (next_pool_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    unsigned int p = next_pool_addr;
    next_pool_addr += PAGE_SIZE;
    if (next_pool_addr > 0x00400000) {
        serial_print("[objpool] ERRO: memoria esgotada\n");
        return NULL;
    }
    return (void *)p;
}

// Zera uma página
static void zero_page(void *p) {
    unsigned char *b = (unsigned char *)p;
    for (unsigned int i = 0; i < PAGE_SIZE; i++) b[i] = 0;
}

// ---- API ----

struct objpool *objpool_create(unsigned int obj_size, unsigned int objs_per_slab) {
    if (obj_size == 0 || objs_per_slab == 0) return NULL;

    obj_size = ALIGN(obj_size, 8);
    if (obj_size * objs_per_slab > PAGE_SIZE) {
        serial_print("[objpool] ERRO: obj_size * objs_per_slab > 4096\n");
        return NULL;
    }

    struct objpool *pool = (struct objpool *)bump_alloc(sizeof(struct objpool));
    if (!pool) return NULL;

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

static int objpool_grow(struct objpool *pool) {
    if (pool->slab_count >= OBJPOOL_MAX_SLABS) {
        serial_print("[objpool] max slabs atingido\n");
        return -1;
    }

    // ★ 1. struct slab no bump allocator (FORA da página dos objetos)
    struct slab *slab = (struct slab *)bump_alloc(sizeof(struct slab));
    if (!slab) return -1;

    // ★ 2. página alinhada a 4 KiB, dedicada 100% aos objetos
    unsigned char *page = (unsigned char *)alloc_page_aligned();
    if (!page) return -1;
    zero_page(page);

    slab->next       = pool->slabs;
    slab->free_count = pool->objs_per_slab;
    slab->mem        = page;   // ★ objetos começam no INÍCIO da página

    // Encadeia todos os objetos livres (LIFO: último empilhado é o primeiro alocado)
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

    struct slab *s = pool->slabs;
    while (s && !s->free_list) s = s->next;

    if (!s) {
        if (objpool_grow(pool) != 0) return NULL;
        s = pool->slabs;
    }

    struct free_obj *obj = s->free_list;
    s->free_list = obj->next;
    s->free_count--;
    pool->used++;

    return (void *)obj;
}

int objpool_free(struct objpool *pool, void *obj) {
    if (!pool || !obj) return -1;

    unsigned long addr = (unsigned long)obj;
    struct slab *s = pool->slabs;
    while (s) {
        unsigned char *start = s->mem;
        unsigned char *end   = start + pool->obj_size * pool->objs_per_slab;
        if (addr >= (unsigned long)start && addr < (unsigned long)end) {
            if ((addr - (unsigned long)start) % pool->obj_size != 0) {
                serial_print("[objpool] ERRO: ponteiro desalinhado\n");
                return -1;
            }

            struct free_obj *f = (struct free_obj *)obj;
            f->next = s->free_list;
            s->free_list = f;
            s->free_count++;
            pool->used--;

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
