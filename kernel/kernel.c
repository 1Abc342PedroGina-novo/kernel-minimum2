// kernel.c
#include "gdt.h"
#include "serial.h"
#include "idt.h"
#include "irq.h"
#include "pit.h"
#include "paging.h"
#include "objpool.h"
#include "tss.h"
#include "pmm.h"

#ifndef idt_init
extern void idt_init(void);
#endif


#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile unsigned short*)0xB8000)

static unsigned int cursor_x = 0;
static unsigned int cursor_y = 0;
static unsigned char color = 0x0F;

static unsigned short vga_entry(char c) {
    return (unsigned short)c | ((unsigned short)color << 8);
}

void clear_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = vga_entry(' ');
    cursor_x = cursor_y = 0;
}

void putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c);
        if (++cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    if (cursor_y >= VGA_HEIGHT)
        clear_screen();

    if (c == '\n')
        serial_putchar('\r');
    serial_putchar(c);
}

void print(const char *s) {
    while (*s) putchar(*s++);
}

void kmain(unsigned int magic, unsigned int *mbinfo) {
    serial_init();
    serial_print("[boot] Kernel inicializado\n");

    gdt_init();
    tss_init(); 
    idt_init();
    irq_init();
    paging_init();

    
    // ★ PMM: gerencia 4 MiB (mesmo range do identity-map)
    serial_print("\n[pmm] === Gerenciador de Paginas Fisicas ===\n");
    pmm_init(0x00400000, pmm_bitmap, PMM_BITMAP_MAX);   // 4 MiB

    // ★ Reservar regiões em uso:
    // - 0x0..0x100000: BIOS, VGA, etc. (1 MiB)
    pmm_reserve(0x00000000, 0x00100000);
    // - 0x100000..kernel_end: kernel ELF (até ~1 MiB)
    pmm_reserve(0x00100000, 0x00180000);   // ajuste conforme o kernel real
    // - bitmap
    pmm_reserve((unsigned int)pmm_bitmap, (unsigned int)pmm_bitmap + PMM_BITMAP_MAX);
    // - page directory + page table (0x108000..0x10A000)
    pmm_reserve(0x00108000, 0x0010B000);
    // - kernel_stack do TSS (0x10B000..0x10D000)
    pmm_reserve(0x0010B000, 0x0010D000);

    serial_print("[pmm] total=");
    serial_print_dec(pmm_total_frames());
    serial_print(" usado=");
    serial_print_dec(pmm_used_frames());
    serial_print(" livre=");
    serial_print_dec(pmm_free_frames());
    serial_print("\n");

    // ★ Demo: aloca 10 frames
    void *frames[10];
    for (int i = 0; i < 10; i++) {
        frames[i] = pmm_alloc_frame();
        serial_print("[pmm] alloc_frame = ");
        serial_print_hex((unsigned int)frames[i]);
        serial_print("\n");
    }

    serial_print("[pmm] apos 10 alloc: usado=");
    serial_print_dec(pmm_used_frames());
    serial_print(" livre=");
    serial_print_dec(pmm_free_frames());
    serial_print("\n");

    // ★ Libera 5
    for (int i = 0; i < 5; i++) pmm_free_frame(frames[i]);

    // ★ Realoca 3 (deve reusar os primeiros livres)
    void *reused[3];
    for (int i = 0; i < 3; i++) {
        reused[i] = pmm_alloc_frame();
        serial_print("[pmm] realloc = ");
        serial_print_hex((unsigned int)reused[i]);
        serial_print("\n");
    }

    // ★ Testa contíguos
    void *block = pmm_alloc_frames(4);
    serial_print("[pmm] alloc 4 contiguos = ");
    serial_print_hex((unsigned int)block);
    serial_print(" (esperado alinhado a 0x4000)\n");

    // ★ Double-free e free inválido
    int r1 = pmm_free_frame(frames[5]);
    int r2 = pmm_free_frame(frames[5]);   // 2ª vez: double free
    serial_print("[pmm] free valido="); serial_print_dec((unsigned int)r1);
    serial_print(" double_free="); serial_print_dec((unsigned int)r2);
    serial_print(" (esperado 0 e -1)\n");

    serial_print("[pmm] final: total=");
    serial_print_dec(pmm_total_frames());
    serial_print(" usado=");
    serial_print_dec(pmm_used_frames());
    serial_print(" livre=");
    serial_print_dec(pmm_free_frames());
    serial_print("\n");
    
    pit_init(100);
    print("Timer 100 Hz ativo.\n");

    // ===== DEMO: Object Pool =====
    serial_print("\n[demo] === Object Pool ===\n");

    // Pool de "tasks" — 64 bytes cada, 64 objetos por página
    struct objpool *tasks = objpool_create(64, 64);
    if (!tasks) {
        serial_print("[demo] FALHOU criar pool\n");
        while (1) __asm__ volatile ("hlt");
    }

    // Aloca 5 objetos
    void *objs[5];
    for (int i = 0; i < 5; i++) {
        objs[i] = objpool_alloc(tasks);
        serial_print("[demo] alloc[");
        serial_print_dec(i);
        serial_print("] @");
        serial_print_hex((unsigned int)objs[i]);
        serial_print("\n");
    }

    serial_print("[demo] total=");
    serial_print_dec(objpool_total(tasks));
    serial_print(" used=");
    serial_print_dec(objpool_used(tasks));
    serial_print(" free=");
    serial_print_dec(objpool_free_count(tasks));
    serial_print("\n");

    // Libera 2 e aloca 1
    objpool_free(tasks, objs[0]);
    objpool_free(tasks, objs[2]);

    void *reused = objpool_alloc(tasks);
    serial_print("[demo] realloc @");
    serial_print_hex((unsigned int)reused);
    serial_print(" (deve reusar objs[0] ou objs[2])\n");

    // Tenta liberar ponteiro inválido
    int ret = objpool_free(tasks, (void *)0xDEADBEEF);
    serial_print("[demo] free invalido retornou ");
    serial_print_dec((unsigned int)ret);
    serial_print(" (esperado -1)\n");

    // ===== FIM DEMO =====

    serial_print("\n[boot] Entrando em loop principal\n");
    // ... loop ...
}
