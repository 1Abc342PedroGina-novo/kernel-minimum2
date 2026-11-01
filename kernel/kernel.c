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
// worker com prioridade visível
static void worker_loop(void *arg) {
    unsigned int id = (unsigned int)arg;
    unsigned int counter = 0;
    unsigned int last_report = 0;

    while (1) {
        counter++;
        if ((counter - last_report) >= 50000000) {
            last_report = counter;
            serial_print("[worker ");
            serial_print_dec(id);
            serial_print("] counter=");
            serial_print_dec(counter);
            serial_print(" pid=");
            serial_print_dec(task_current_pid());
            serial_print(" tid=");
            serial_print_dec(task_current_tid());
            serial_print("\n");
        }
    }
}

// ===== CALLBACK DO PIT =====
// Roda a cada tick (100 Hz). Agenda + loga a cada 100 ticks (1 s).
static void scheduler_callback(void) {
    static unsigned int tick = 0;
    tick++;

    if (tick % 100 == 0) {
        serial_print("[sched] tick ");
        serial_print_dec(tick);
        serial_print(" cur=pid");
        serial_print_dec(task_current_pid());
        serial_print("/tid");
        serial_print_dec(task_current_tid());
        serial_print("\n");
    }

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

    // 7. Task manager (processos + threads + scheduler)
    serial_print("\n[task] === Inicializando gerenciador de tarefas ===\n");
    task_init();

    // 8. Registra callback do PIT para agendar
    pit_set_callback(scheduler_callback);

    // 9. Cria 3 processos + 3 threads com prioridades diferentes
    serial_print("\n[task] === Criando processos e threads (CFS) ===\n");

    // prio 1 (câmera lenta)
    unsigned int pid_low = process_create();
    if (pid_low) {
        unsigned int tid = thread_create_prio(pid_low, worker_loop, (void *)1, 1);
        serial_print("[main] pid=");
        serial_print_dec(pid_low);
        serial_print(" tid=");
        serial_print_dec(tid);
        serial_print(" prio=1 (lento)\n");
    }

    // prio 16 (baseline)
    unsigned int pid_mid = process_create();
    if (pid_mid) {
        unsigned int tid = thread_create_prio(pid_mid, worker_loop, (void *)2, 16);
        serial_print("[main] pid=");
        serial_print_dec(pid_mid);
        serial_print(" tid=");
        serial_print_dec(tid);
        serial_print(" prio=16 (normal)\n");
    }

    // prio 32 (super-velocidade)
    unsigned int pid_high = process_create();
    if (pid_high) {
        unsigned int tid = thread_create_prio(pid_high, worker_loop, (void *)3, 32);
        serial_print("[main] pid=");
        serial_print_dec(pid_high);
        serial_print(" tid=");
        serial_print_dec(tid);
        serial_print(" prio=32 (rapido)\n");
    }

    // 10. Dump inicial
    task_dump();

    // 11. Loop principal — só hlt, o scheduler roda no IRQ0
    serial_print("\n[boot] Scheduler CFS ativo. Loop principal (hlt).\n");
    serial_print("[boot] esperado: worker 3 >> worker 2 >> worker 1\n\n");

    while (1) {
        __asm__ volatile ("hlt");
    }
}
