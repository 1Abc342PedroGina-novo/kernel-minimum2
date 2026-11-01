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
#define PMM_BITMAP_MAX (128 * 1024)
static unsigned char pmm_bitmap[PMM_BITMAP_MAX] __attribute__((aligned(4096)));
extern unsigned char _kernel_end;

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

    serial_print("\n[pmm] === Gerenciador de Paginas Fisicas ===\n");
    pmm_init(0x00400000, pmm_bitmap, PMM_BITMAP_MAX);

// 1. BIOS + VGA (primeiro MiB inteiro) — sempre reservar
pmm_reserve(0x00000000, 0x00100000);

// 2. Kernel inteiro (ELF + .bss) — usa o símbolo do linker
pmm_reserve(0x00100000, (unsigned int)&_kernel_end);
serial_print("[pmm] kernel_end = ");
serial_print_hex((unsigned int)&_kernel_end);
serial_print("\n");


    serial_print("[pmm] total=");
    serial_print_dec(pmm_total_frames());
    serial_print(" usado=");
    serial_print_dec(pmm_used_frames());
    serial_print(" livre=");
    serial_print_dec(pmm_free_count());   // ← pmm_free_count()
    serial_print("\n");

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
    serial_print_dec(pmm_free_count());   // ← pmm_free_count()
    serial_print("\n");

    for (int i = 0; i < 5; i++) pmm_free_frame(frames[i]);

    void *reused[3];
    for (int i = 0; i < 3; i++) {
        reused[i] = pmm_alloc_frame();
        serial_print("[pmm] realloc = ");
        serial_print_hex((unsigned int)reused[i]);
        serial_print("\n");
    }

    void *block = pmm_alloc_frames(4);
    serial_print("[pmm] alloc 4 contiguos = ");
    serial_print_hex((unsigned int)block);
    serial_print(" (esperado alinhado a 0x4000)\n");

    int r1 = pmm_free_frame(frames[5]);
    int r2 = pmm_free_frame(frames[5]);
    serial_print("[pmm] free valido=");
    serial_print_dec((unsigned int)r1);
    serial_print(" double_free=");
    serial_print_dec((unsigned int)r2);
    serial_print(" (esperado 0 e -1)\n");

    serial_print("[pmm] final: total=");
    serial_print_dec(pmm_total_frames());
    serial_print(" usado=");
    serial_print_dec(pmm_used_frames());
    serial_print(" livre=");
    serial_print_dec(pmm_free_count());   // ← pmm_free_count()
    serial_print("\n");

    // ★ Inicializa VMM (após PMM, antes de usar vmm_map)
vmm_init();

// ★ Dump do estado inicial
mem_dump();

// ★ Teste 1: mapear uma página física nova em endereço virtual alto
serial_print("\n[vmm] === Teste 1: map 0x10000000 -> phys novo ===\n");
uint32_t phys1 = (uint32_t)pmm_alloc_frame();
if (phys1) {
    if (vmm_map(0x10000000, phys1, VMM_PRESENT | VMM_WRITE) == 0) {
        serial_print("[vmm] mapped 0x10000000 -> ");
        serial_print_hex(phys1);
        serial_print("\n");
        serial_print("[vmm] get_phys(0x10000000) = ");
        serial_print_hex(vmm_get_phys(0x10000000));
        serial_print(" (esperado ");
        serial_print_hex(phys1);
        serial_print(")\n");
        // Escreve e lê de volta
        volatile uint32_t *p = (uint32_t *)0x10000000;
        *p = 0xCAFEBABE;
        serial_print("[vmm] escreveu 0xCAFEBABE, leu ");
        serial_print_hex(*p);
        serial_print("\n");
    }
}

// ★ Teste 2: mapear 4 páginas contíguas em 0x20000000
serial_print("\n[vmm] === Teste 2: map 4 paginas em 0x20000000 ===\n");
uint32_t phys2 = (uint32_t)pmm_alloc_frames(4);
if (phys2) {
    if (vmm_map_range(0x20000000, phys2, 4, VMM_PRESENT | VMM_WRITE) == 0) {
        serial_print("[vmm] mapped 4 paginas: 0x20000000 -> ");
        serial_print_hex(phys2);
        serial_print("\n");
        for (uint32_t i = 0; i < 4; i++) {
            volatile uint32_t *p = (uint32_t *)(0x20000000 + i * 4096);
            *p = 0xA0000000 + i;
        }
        for (uint32_t i = 0; i < 4; i++) {
            volatile uint32_t *p = (uint32_t *)(0x20000000 + i * 4096);
            serial_print("[vmm]   [");
            serial_print_dec(i);
            serial_print("] = ");
            serial_print_hex(*p);
            serial_print("\n");
        }
    }
}

// ★ Teste 3: desmapear
serial_print("\n[vmm] === Teste 3: unmap 0x10000000 ===\n");
vmm_unmap(0x10000000);
serial_print("[vmm] get_phys(0x10000000) apos unmap = ");
serial_print_hex(vmm_get_phys(0x10000000));
serial_print(" (esperado 0)\n");
pmm_free_frame((void *)phys1);   // devolve o físico também

// ★ Dump final
mem_dump();
    
    pit_init(100);
    print("Timer 100 Hz ativo.\n");

    // ===== Object Pool =====
    serial_print("\n[demo] === Object Pool ===\n");
    struct objpool *tasks = objpool_create(64, 64);
    if (!tasks) {
        serial_print("[demo] FALHOU criar pool\n");
        while (1) __asm__ volatile ("hlt");
    }

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

    objpool_free(tasks, objs[0]);
    objpool_free(tasks, objs[2]);

    void *reused_obj = objpool_alloc(tasks);
    serial_print("[demo] realloc @");
    serial_print_hex((unsigned int)reused_obj);
    serial_print(" (deve reusar objs[0] ou objs[2])\n");

    int ret = objpool_free(tasks, (void *)0xDEADBEEF);
    serial_print("[demo] free invalido retornou ");
    serial_print_dec((unsigned int)ret);
    serial_print(" (esperado -1)\n");

    serial_print("\n[boot] Entrando em loop principal\n");
    // ... loop ...
}
