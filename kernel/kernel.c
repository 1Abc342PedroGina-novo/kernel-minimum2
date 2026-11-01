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
#include "mm_info.h"
#include "vmm.h"
#include "task.h"
#include "heap.h"

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

static void pit_handler_with_schedule(struct regs *r) {
    (void)r;
    // Conta ticks
    static unsigned int ticks = 0;
    ticks++;
    if (ticks % 100 == 0) {
        serial_print("[pit] tick ");
        serial_print_dec(ticks);
        serial_print(" (pid=");
        serial_print_dec(task_current_pid());
        serial_print(", tid=");
        serial_print_dec(task_current_tid());
        serial_print(")\n");
    }

    // Scheduler
    task_schedule();
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

// Thread que conta e loga periodicamente
static void worker_loop(void *arg) {
    unsigned int id = (unsigned int)arg;
    unsigned int counter = 0;

    while (1) {
        counter++;
        if ((counter % 1000000) == 0) {
            serial_print("[worker ");
            serial_print_dec(id);
            serial_print("] pid=");
            serial_print_dec(task_current_pid());
            serial_print(" tid=");
            serial_print_dec(task_current_tid());
            serial_print(" counter=");
            serial_print_dec(counter);
            serial_print("\n");
        }
    }
}

// Callback do PIT: agenda
static void scheduler_callback(void) {
    task_schedule();
}

// ===== MAIN =====

void kmain(unsigned int magic, unsigned int *mbinfo) {
    (void)magic;
    (void)mbinfo;

    serial_init();
    serial_print("[boot] Kernel inicializado\n");

    // 1. Init de baixo nível
    gdt_init();
    tss_init();
    idt_init();
    irq_init();
    paging_init();

    // 2. PMM
    serial_print("\n[pmm] === Gerenciador de Paginas Fisicas ===\n");
    pmm_init(0x00400000, pmm_bitmap, PMM_BITMAP_MAX);
    pmm_reserve(0x00000000, 0x00100000);
    pmm_reserve(0x00100000, (unsigned int)&_kernel_end);
    serial_print("[pmm] kernel_end = ");
    serial_print_hex((unsigned int)&_kernel_end);
    serial_print("\n");
    serial_print("[pmm] total=");
    serial_print_dec(pmm_total_frames());
    serial_print(" usado=");
    serial_print_dec(pmm_used_frames());
    serial_print(" livre=");
    serial_print_dec(pmm_free_count());
    serial_print("\n");

    // 3. VMM
    vmm_init();

    // 4. Heap
    heap_init(0x30000000, 0x10000000);
    serial_print("[heap] base=0x30000000 max=256 MiB\n");

    // 5. PIT (timer) — SEM callback ainda
    pit_init(100);
    print("Timer 100 Hz ativo.\n");

    // 6. Object pool (demo rápido)
    serial_print("\n[demo] === Object Pool ===\n");
    struct objpool *tasks_pool = objpool_create(64, 64);
    if (tasks_pool) {
        void *o = objpool_alloc(tasks_pool);
        serial_print("[demo] objpool alloc @");
        serial_print_hex((unsigned int)o);
        serial_print("\n");
        objpool_free(tasks_pool, o);
    }

    // 7. ★ Task manager (processos + threads + scheduler)
    serial_print("\n[task] === Inicializando gerenciador de tarefas ===\n");
    task_init();

    // 8. ★ Registra callback do PIT para agendar
    pit_set_callback(scheduler_callback);

    // 9. ★ Cria 2 processos + 2 threads
    serial_print("\n[task] === Criando processos e threads ===\n");

    unsigned int pid1 = process_create();
    if (pid1) {
        unsigned int tid1 = thread_create(pid1, worker_loop, (void *)1);
        serial_print("[main] processo pid=");
        serial_print_dec(pid1);
        serial_print(" thread tid=");
        serial_print_dec(tid1);
        serial_print("\n");
    }

    unsigned int pid2 = process_create();
    if (pid2) {
        unsigned int tid2 = thread_create(pid2, worker_loop, (void *)2);
        serial_print("[main] processo pid=");
        serial_print_dec(pid2);
        serial_print(" thread tid=");
        serial_print_dec(tid2);
        serial_print("\n");
    }

    // 10. Dump inicial
    task_dump();

    // 11. Loop principal — só hlt, o scheduler roda no IRQ0
    serial_print("\n[boot] Scheduler ativo. Loop principal (hlt).\n");
    serial_print("[boot] idle thread sera interrompida pelo timer.\n\n");

    while (1) {
        __asm__ volatile ("hlt");
    }
}
