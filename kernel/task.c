// task.c - Gerenciador de processos/threads com scheduler CFS
#include "task.h"
#include "pmm.h"
#include "vmm.h"
#include "pit.h"
#include "tss.h"
#include "serial.h"

// ---- Estado global ----

static struct task *process_list = 0;
static struct task *current_process = 0;
static struct thread  *current_thread  = 0;
static unsigned int next_pid = 1;
static unsigned int next_tid = 1;

static struct task kernel_process;
static struct thread  idle_thread;

// CFS: escala para evitar overflow e manter precisão
#define VRUNTIME_SCALE (1024ULL * 1024ULL)

// Último tick em que o scheduler rodou
static unsigned long long last_sched_tick = 0;

// ---- Helpers ----

static void memset8(void *p, unsigned char v, unsigned int n) {
    unsigned char *b = (unsigned char *)p;
    for (unsigned int i = 0; i < n; i++) b[i] = v;
}

static void idle_entry(void *arg) {
    (void)arg;
    while (1) __asm__ volatile ("hlt");
}

static unsigned int alloc_kernel_stack(unsigned int **out_base) {
    unsigned int *stack = (unsigned int *)pmm_alloc_frame();
    if (!stack) return 0;
    *out_base = stack;
    return (unsigned int)stack + 4096;
}

// ---- CFS: atualização de vruntime ----

// Atualiza vruntime de uma thread que rodou 'delta_ticks' com prioridade 'prio'.
// Fórmula: vruntime += delta * (SCALE / prio)
// ★ Converte ms para ciclos de CPU (TSC)
#define TSC_FREQ_HZ  1000000000ULL   // 1 GHz (ajuste conforme seu CPU real)
#define MS_TO_CYCLES(ms) ((ms) * TSC_FREQ_HZ / 1000)

// Tempo mínimo de execução: 0.75 ms
#define EXEC_MIN_MS  1               // arredonda para 1 ms (mínimo prático)

// Lê o TSC
static inline unsigned long long rdtsc(void) {
    unsigned int lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}

// Atualiza vruntime da thread (CFS)
static void update_vruntime(struct thread *t, unsigned long long delta_ticks) {
    unsigned int prio = t->priority;
    if (prio < THREAD_PRIO_MIN) prio = THREAD_PRIO_MIN;
    if (prio > THREAD_PRIO_MAX) prio = THREAD_PRIO_MAX;

    t->vruntime += delta_ticks * (VRUNTIME_SCALE / prio);
    t->real_time += delta_ticks;
}

// ★ Verifica se a thread atual já cumpriu o tempo mínimo
static int thread_exec_min_done(struct thread *t) {
    if (!t || t->exec_min_cycles == 0) return 1;   // sem mínimo → OK
   unsigned long long now = rdtsc();
    return (now - t->exec_start_tsc) >= t->exec_min_cycles;
}

// ★ Reseta o início de execução (chamado quando a thread começa a rodar)
static void thread_exec_begin(struct thread *t) {
    if (!t) return;
    t->exec_start_tsc = rdtsc();
    t->exec_min_cycles = MS_TO_CYCLES(EXEC_MIN_MS);
}

// ---- CFS: escolha da próxima thread ----

// Encontra a thread READY com menor vruntime (desempate: menor tid)
static struct thread *pick_next(void) {
    struct thread *best = 0;
    unsigned long long best_vrt = (unsigned long long)-1;

    struct task *p = process_list;
    while (p) {
        struct thread *t = p->threads;
        while (t) {
            if (t->state == THREAD_READY) {
                if (!best ||
                    t->vruntime < best_vrt ||
                    (t->vruntime == best_vrt && t->tid < best->tid)) {
                    best = t;
                    best_vrt = t->vruntime;
                }
            }
            t = t->next;
        }
        p = p->next;
    }
    return best;
}

// ---- task_init ----

void task_init(void) {
    memset8(&kernel_process, 0, sizeof(kernel_process));
    memset8(&idle_thread, 0, sizeof(idle_thread));

    kernel_process.pid = 0;
    kernel_process.state = 0;
    kernel_process.page_directory = 0;
    kernel_process.threads = &idle_thread;
    kernel_process.thread_count = 1;
    kernel_process.next = 0;

    idle_thread.tid = 0;
    idle_thread.state = THREAD_RUNNING;
    idle_thread.priority = THREAD_PRIO_MIN;   // idle = prioridade mínima
    idle_thread.vruntime = 0;
    idle_thread.real_time = 0;
    idle_thread.proc = &kernel_process;
    idle_thread.entry = idle_entry;
    idle_thread.arg = 0;

    unsigned int *stack_base;
    idle_thread.kernel_stack_top = alloc_kernel_stack(&stack_base);
    if (!idle_thread.kernel_stack_top) {
        serial_print("[task] ERRO: sem stack para idle\n");
        return;
    }
    idle_thread.kernel_stack = stack_base;

    idle_thread.ctx.edi = 0;
    idle_thread.ctx.esi = 0;
    idle_thread.ctx.ebp = 0;
    idle_thread.ctx.ebx = 0;
    idle_thread.ctx.esp = idle_thread.kernel_stack_top;
    idle_thread.ctx.eip = (unsigned int)idle_entry;

    process_list = &kernel_process;
    current_process = &kernel_process;
    current_thread = &idle_thread;

    last_sched_tick = pit_ticks();

    serial_print("[task] init: pid=0 (kernel), tid=0 (idle, prio=");
    serial_print_dec(idle_thread.priority);
    serial_print(")\n");
    serial_print("[task] stack idle: ");
    serial_print_hex((unsigned int)stack_base);
    serial_print("..");
    serial_print_hex(idle_thread.kernel_stack_top - 1);
    serial_print("\n");
}

// ---- process_create ----

unsigned int process_create(void) {
    struct task *p = (struct task *)pmm_alloc_frame();
    if (!p) {
        serial_print("[task] ERRO: sem memoria para processo\n");
        return 0;
    }
    memset8(p, 0, sizeof(*p));

    p->pid = next_pid++;
    p->state = 0;
    p->threads = 0;
    p->thread_count = 0;

    // Aloca frame físico para page directory
    void *pd_phys = pmm_alloc_frame();
    if (!pd_phys) {
        serial_print("[task] ERRO: sem frame para PD\n");
        pmm_free_frame(p);
        return 0;
    }

    // Mapeia temporariamente em 0x40000000 para escrever
    unsigned int pd_virt = 0x40000000;
    if (vmm_map(pd_virt, (unsigned int)pd_phys, VMM_PRESENT | VMM_WRITE) != 0) {
        serial_print("[task] ERRO: nao conseguiu mapear PD temporario\n");
        pmm_free_frame(pd_phys);
        pmm_free_frame(p);
        return 0;
    }

    // Copia PD do kernel (CR3 atual)
    unsigned int cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    unsigned int *src = (unsigned int *)(cr3 & ~0xFFF);
    unsigned int *dst = (unsigned int *)pd_virt;
    for (unsigned int i = 0; i < 1024; i++) dst[i] = src[i];

    vmm_unmap(pd_virt);

    p->page_directory = (unsigned int *)pd_phys;

    // Insere no final da lista (ordem estável)
    struct task *tail = process_list;
    while (tail->next) tail = tail->next;
    tail->next = p;
    p->next = 0;

    serial_print("[task] processo criado pid=");
    serial_print_dec(p->pid);
    serial_print(" pd_phys=");
    serial_print_hex((unsigned int)pd_phys);
    serial_print("\n");

    return p->pid;
}

// ---- thread_create_prio ----

unsigned int thread_create_prio(unsigned int pid, void (*entry)(void *), void *arg, unsigned int prio) {
    if (!entry) return 0;

    struct task *p = process_list;
    while (p && p->pid != pid) p = p->next;
    if (!p) {
        serial_print("[task] ERRO: pid ");
        serial_print_dec(pid);
        serial_print(" nao encontrado\n");
        return 0;
    }
    if (p->thread_count >= MAX_THREADS) return 0;

    if (prio < THREAD_PRIO_MIN) prio = THREAD_PRIO_MIN;
    if (prio > THREAD_PRIO_MAX) prio = THREAD_PRIO_MAX;

    struct thread *t = (struct thread *)pmm_alloc_frame();
    if (!t) return 0;
    memset8(t, 0, sizeof(*t));

    t->tid = next_tid++;
    t->state = THREAD_READY;
    t->priority = prio;
    t->vruntime = 0;
    t->real_time = 0;
    t->exec_start_tsc = 0;
    t->exec_min_cycles = MS_TO_CYCLES(EXEC_MIN_MS);
    t->proc = p;
    t->entry = entry;
    t->arg = arg;
    t->wakeup_tick = 0;

    unsigned int *stack_base;
    unsigned int stack_top = alloc_kernel_stack(&stack_base);
    if (!stack_top) {
        pmm_free_frame(t);
        return 0;
    }
    t->kernel_stack = stack_base;
    t->kernel_stack_top = stack_top;

    // Empilha arg e retorno fake na stack
    unsigned int *sp = (unsigned int *)stack_top;
    *(--sp) = (unsigned int)arg;
    *(--sp) = 0;                      // endereço de retorno fake

    t->ctx.edi = 0;
    t->ctx.esi = 0;
    t->ctx.ebp = 0;
    t->ctx.ebx = 0;
    t->ctx.esp = (unsigned int)sp;
    t->ctx.eip = (unsigned int)entry;
    t->ctx.eflags = 0x202;   // ★ IF=1, bit 1 reservado (sempre 1)
    t->ctx.cs = 0x08;        // ★ kernel code selector

    // Insere na lista do processo
    t->next = p->threads;
    p->threads = t;
    p->thread_count++;

    serial_print("[task] thread criada tid=");
    serial_print_dec(t->tid);
    serial_print(" pid=");
    serial_print_dec(p->pid);
    serial_print(" prio=");
    serial_print_dec(prio);
    serial_print(" entry=");
    serial_print_hex((unsigned int)entry);
    serial_print("\n");

    return t->tid;
}

unsigned int thread_create(unsigned int pid, void (*entry)(void *), void *arg) {
    return thread_create_prio(pid, entry, arg, THREAD_PRIO_DEFAULT);
}

// ---- thread_exit ----

void thread_exit(void) {
    __asm__ volatile ("cli");
    if (current_thread) {
        current_thread->state = THREAD_ZOMBIE;
    }
    task_schedule();
    while (1) __asm__ volatile ("hlt");
}

// ---- thread_sleep ----

void thread_sleep(unsigned int ticks) {
    if (!current_thread || ticks == 0) return;
    current_thread->wakeup_tick = pit_ticks() + ticks;
    current_thread->state = THREAD_BLOCKED;
    task_schedule();
}

void thread_yield(void) {
    // ★ Força a troca (ignora o tempo mínimo)
    current_thread->exec_min_cycles = 0;
    task_schedule();
    current_thread->exec_min_cycles = MS_TO_CYCLES(EXEC_MIN_MS);
}


// ---- Scheduler CFS ----

void task_schedule(void) {
    if (!current_thread) return;

    unsigned long long now = pit_ticks();
    unsigned long long delta = now - last_sched_tick;
    last_sched_tick = now;

    // 1. Acorda threads bloqueadas
    struct task *p = process_list;
    while (p) {
        struct thread *t = p->threads;
        while (t) {
            if (t->state == THREAD_BLOCKED && t->wakeup_tick && now >= t->wakeup_tick) {
                t->state = THREAD_READY;
                t->wakeup_tick = 0;
            }
            t = t->next;
        }
        p = p->next;
    }

    // 2. ★ SEMPRE atualiza vruntime da thread atual
    //    (não importa o estado — ela rodou 'delta' ticks)
    struct thread *prev = current_thread;
    update_vruntime(prev, delta);
    prev->state = THREAD_READY;

    // 3. Escolhe próxima
    struct thread *next = pick_next();

    // ★ Log de diagnóstico
    static unsigned int sched_count = 0;
    if (++sched_count <= 30) {
        serial_print("[sched] cur=");
        serial_print_dec(prev ? prev->tid : 999);
        serial_print(" vrt_cur=");
        serial_print_dec64(prev ? prev->vruntime : 0);
        serial_print(" next=");
        serial_print_dec(next ? next->tid : 999);
        serial_print(" vrt_next=");
        serial_print_dec64(next ? next->vruntime : 0);
        serial_print("\n");
    }

    // 4. Se ninguém, mantém atual
    if (!next) {
        prev->state = THREAD_RUNNING;
        return;
    }

    if (next == prev) {
        prev->state = THREAD_RUNNING;
        return;
    }

    // 5. Troca
    next->state = THREAD_RUNNING;
    current_thread = next;
    current_process = next->proc;

    tss_set_kernel_stack(next->kernel_stack_top);
    if (next->proc->page_directory) {
        __asm__ volatile ("mov %0, %%cr3" : : "r"(next->proc->page_directory) : "memory");
    }

    context_switch(&prev->ctx, &next->ctx);

    // 6. Restaura prev
    current_thread = prev;
    current_process = prev->proc;
    prev->state = THREAD_RUNNING;      // ★ marca como RUNNING
    tss_set_kernel_stack(prev->kernel_stack_top);
    if (prev->proc->page_directory) {
        __asm__ volatile ("mov %0, %%cr3" : : "r"(prev->proc->page_directory) : "memory");
    }
}

// ---- Consulta ----

unsigned int task_current_tid(void) { return current_thread ? current_thread->tid : 0; }
unsigned int task_current_pid(void) { return current_process ? current_process->pid : 0; }

// ---- Dump ----

void task_dump(void) {
    serial_print("\n[task] === Processos/Threads (CFS) ===\n");
    serial_print("[task] atual: pid=");
    serial_print_dec(task_current_pid());
    serial_print(" tid=");
    serial_print_dec(task_current_tid());
    serial_print("\n");
    serial_print("[task] (prio = 1..32, vrt = vruntime, real = ticks)\n");

    struct task *p = process_list;
    while (p) {
        serial_print("[task] pid=");
        serial_print_dec(p->pid);
        serial_print(" threads=");
        serial_print_dec(p->thread_count);
        serial_print(" state=");
        serial_print_dec(p->state);
        serial_print("\n");

        struct thread *t = p->threads;
        while (t) {
            serial_print("[task]   tid=");
            serial_print_dec(t->tid);
            serial_print(" prio=");
            serial_print_dec(t->priority);
            serial_print(" vrt=");
            serial_print_dec64(t->vruntime);
            serial_print(" real=");
            serial_print_dec(t->real_time);
            serial_print(" state=");
            serial_print_dec(t->state);
            serial_print("\n");
            t = t->next;
        }
        p = p->next;
    }
    serial_print("[task] === Fim ===\n");
}
