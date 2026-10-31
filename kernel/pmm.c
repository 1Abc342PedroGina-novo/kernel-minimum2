// pmm.c - Gerenciador de páginas físicas (bitmap allocator)
#include "pmm.h"
#include "serial.h"

#define PAGE_SIZE  4096
#define PAGE_SHIFT 12

// ---- Estado ----

static unsigned int *bitmap       = NULL;   // 1 bit por frame
static unsigned int  total_frames = 0;      // frames totais
static unsigned int  used_frames  = 0;      // frames usados
static unsigned int  bitmap_words = 0;      // palavras de 32 bits

// ---- Bitmap helpers ----

static inline int bit_test(unsigned int frame) {
    return (bitmap[frame >> 5] >> (frame & 31)) & 1;
}

static inline void bit_set(unsigned int frame) {
    bitmap[frame >> 5] |= (1u << (frame & 31));
}

static inline void bit_clear(unsigned int frame) {
    bitmap[frame >> 5] &= ~(1u << (frame & 31));
}

// Encontra o próximo frame livre a partir de 'start'.
// Retorna -1 se não houver.
static int find_free_frame(unsigned int start) {
    for (unsigned int w = start >> 5; w < bitmap_words; w++) {
        if (bitmap[w] == 0xFFFFFFFF) continue;   // palavra cheia, pula
        for (unsigned int b = 0; b < 32; b++) {
            unsigned int f = (w << 5) | b;
            if (f >= total_frames) return -1;
            if (!(bitmap[w] & (1u << b))) return (int)f;
        }
    }
    return -1;
}

// ---- API ----

int pmm_init(unsigned int total_bytes, void *bitmap_addr, unsigned int bitmap_max) {
    total_frames = total_bytes / PAGE_SIZE;
    bitmap_words = (total_frames + 31) / 32;
    unsigned int needed = bitmap_words * 4;

    if (needed > bitmap_max) {
        serial_print("[pmm] ERRO: bitmap nao cabe (precisa ");
        serial_print_dec(needed);
        serial_print(" bytes, tem ");
        serial_print_dec(bitmap_max);
        serial_print(")\n");
        return -1;
    }

    bitmap = (unsigned int *)bitmap_addr;
    used_frames = 0;

    // Zera tudo (tudo LIVRE inicialmente)
    for (unsigned int i = 0; i < bitmap_words; i++) bitmap[i] = 0;

    serial_print("[pmm] init: total=");
    serial_print_dec(total_bytes / 1024);
    serial_print(" KiB (");
    serial_print_dec(total_frames);
    serial_print(" frames), bitmap=");
    serial_print_dec(needed);
    serial_print(" bytes\n");

    return 0;
}

int pmm_reserve(unsigned int start, unsigned int end) {
    start &= ~(PAGE_SIZE - 1);
    end   = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    unsigned int f0 = start >> PAGE_SHIFT;
    unsigned int f1 = end   >> PAGE_SHIFT;
    if (f1 > total_frames) f1 = total_frames;

    unsigned int n = 0;
    for (unsigned int f = f0; f < f1; f++) {
        if (!bit_test(f)) {
            bit_set(f);
            used_frames++;
            n++;
        }
    }

    serial_print("[pmm] reserve ");
    serial_print_hex(start);
    serial_print("..");
    serial_print_hex(end);
    serial_print(" (");
    serial_print_dec(n);
    serial_print(" frames)\n");

    return 0;
}

int pmm_release(unsigned int start, unsigned int end) {
    start &= ~(PAGE_SIZE - 1);
    end   = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    unsigned int f0 = start >> PAGE_SHIFT;
    unsigned int f1 = end   >> PAGE_SHIFT;
    if (f1 > total_frames) f1 = total_frames;

    for (unsigned int f = f0; f < f1; f++) {
        if (bit_test(f)) {
            bit_clear(f);
            used_frames--;
        }
    }
    return 0;
}

void *pmm_alloc_frame(void) {
    int f = find_free_frame(0);
    if (f < 0) {
        serial_print("[pmm] OOM (sem frames livres)\n");
        return NULL;
    }
    bit_set((unsigned int)f);
    used_frames++;
    return (void *)((unsigned int)f << PAGE_SHIFT);
}

int pmm_free_frame(void *addr) {
    unsigned int a = (unsigned int)addr;
    if (a & (PAGE_SIZE - 1)) {
        serial_print("[pmm] ERRO: free desalinhado\n");
        return -1;
    }
    unsigned int f = a >> PAGE_SHIFT;
    if (f >= total_frames) {
        serial_print("[pmm] ERRO: free fora do range\n");
        return -1;
    }
    if (!bit_test(f)) {
        serial_print("[pmm] ERRO: double free\n");
        return -1;
    }
    bit_clear(f);
    used_frames--;
    return 0;
}

void *pmm_alloc_frames(unsigned int n) {
    if (n == 0) return NULL;
    if (n == 1) return pmm_alloc_frame();

    // Busca sequência de n frames livres
    for (unsigned int f = 0; f + n <= total_frames; f++) {
        int ok = 1;
        for (unsigned int i = 0; i < n; i++) {
            if (bit_test(f + i)) { ok = 0; break; }
        }
        if (!ok) continue;

        for (unsigned int i = 0; i < n; i++) {
            bit_set(f + i);
            used_frames++;
        }
        return (void *)(f << PAGE_SHIFT);
    }

    serial_print("[pmm] OOM: sem ");
    serial_print_dec(n);
    serial_print(" frames contiguos\n");
    return NULL;
}

void pmm_free_frames(void *addr, unsigned int n) {
    unsigned int a = (unsigned int)addr;
    if (a & (PAGE_SIZE - 1)) return;
    unsigned int f0 = a >> PAGE_SHIFT;
    if (f0 + n > total_frames) return;

    for (unsigned int i = 0; i < n; i++) {
        if (bit_test(f0 + i)) {
            bit_clear(f0 + i);
            used_frames--;
        }
    }
}

unsigned int pmm_total_frames(void) { return total_frames; }
unsigned int pmm_used_frames(void)  { return used_frames; }
unsigned int pmm_free_frames(void)  { return total_frames - used_frames; }
