// heap.c - Heap do kernel (bump allocator sobre VMM)
#include "heap.h"
#include "vmm.h"
#include "serial.h"

#define PAGE_SIZE  4096
#define HEAP_ALIGN 8

// ---- Estado ----

static unsigned int heap_base  = 0;   // 0x30000000
static unsigned int heap_bump  = 0;   // próximo endereço livre
static unsigned int heap_end   = 0;   // limite máximo (base + max_size)
static unsigned int heap_mapped= 0;   // até onde está mapeado

static inline unsigned int align_up(unsigned int x, unsigned int a) {
    return (x + a - 1) & ~(a - 1);
}

// ---- API ----

void heap_init(unsigned int base, unsigned int max_size) {
    heap_base   = base;
    heap_bump   = base;
    heap_end    = base + max_size;
    heap_mapped = base;

    serial_print("[heap] base=");
    serial_print_hex(base);
    serial_print(" max=");
    serial_print_dec(max_size / 1024);
    serial_print(" KiB\n");
}

// Garante que [heap_mapped, upto) está mapeado.
static int heap_grow(unsigned int upto) {
    if (upto > heap_end) {
        serial_print("[heap] ERRO: excede max (bump=");
        serial_print_hex(upto);
        serial_print(", end=");
        serial_print_hex(heap_end);
        serial_print(")\n");
        return -1;
    }

    // Alinha upto à página
    unsigned int need = align_up(upto, PAGE_SIZE);
    if (need <= heap_mapped) return 0;

    unsigned int npages = (need - heap_mapped) / PAGE_SIZE;

    serial_print("[heap] grow: mapeando ");
    serial_print_dec(npages);
    serial_print(" paginas em ");
    serial_print_hex(heap_mapped);
    serial_print("\n");

    unsigned int r = vmm_alloc(heap_mapped, npages, VMM_PRESENT | VMM_WRITE);
    if (!r) {
        serial_print("[heap] ERRO: vmm_alloc falhou\n");
        return -1;
    }

    heap_mapped = need;
    return 0;
}

void *heap_alloc(unsigned int size) {
    if (!heap_base) {
        serial_print("[heap] ERRO: heap_alloc antes de heap_init\n");
        return 0;
    }
    if (size == 0) return 0;

    size = align_up(size, HEAP_ALIGN);

    unsigned int old_bump = heap_bump;
    unsigned int new_bump = heap_bump + size;

    if (new_bump > heap_end) {
        serial_print("[heap] OOM: pedido ");
        serial_print_dec(size);
        serial_print(" bytes excede heap\n");
        return 0;
    }

    // Cresce se necessário
    if (new_bump > heap_mapped) {
        if (heap_grow(new_bump) != 0) return 0;
    }

    heap_bump = new_bump;
    return (void *)old_bump;
}

int heap_free_last(void *ptr, unsigned int size) {
    if (!ptr || size == 0) return -1;
    size = align_up(size, HEAP_ALIGN);
    unsigned int p = (unsigned int)ptr;

    if (p + size != heap_bump) {
        serial_print("[heap] free_last: nao eh o ultimo bloco\n");
        return -1;
    }

    heap_bump = p;
    return 0;
}

unsigned int heap_total_bytes(void) { return heap_mapped - heap_base; }
unsigned int heap_used_bytes(void)  { return heap_bump - heap_base; }
unsigned int heap_free_bytes(void)  { return heap_end - heap_bump; }

void heap_dump(void) {
    serial_print("\n[heap] === Heap ===\n");
    serial_print("[heap] base=");
    serial_print_hex(heap_base);
    serial_print(" bump=");
    serial_print_hex(heap_bump);
    serial_print(" mapped=");
    serial_print_hex(heap_mapped);
    serial_print(" end=");
    serial_print_hex(heap_end);
    serial_print("\n");
    serial_print("[heap] total=");
    serial_print_dec(heap_total_bytes() / 1024);
    serial_print(" KiB, usado=");
    serial_print_dec(heap_used_bytes() / 1024);
    serial_print(" KiB, livre=");
    serial_print_dec(heap_free_bytes() / 1024);
    serial_print(" KiB\n");
}
