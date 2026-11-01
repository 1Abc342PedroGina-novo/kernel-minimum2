// task.c — Round-Robin com prioridade
#include "task.h"
#include "pmm.h"
#include "vmm.h"
#include "pit.h"
#include "tss.h"
#include "serial.h"

#define MAX_RUN_MS  150   // ★ preempção por tempo: 150 ms
#define TSC_FREQ_HZ 1000000000ULL

static struct task *process_list = 0;
static struct task *current_process = 0;
static struct thread  *current_thread  = 0;
static unsigned int next_pid = 1;
static unsigned int next_tid = 1;

static struct task kernel_process;
static struct thread  idle_thread;

static void memset8(void *p, unsigned char v, unsigned int n) {
    unsigned char *b = (unsigned char *)p;
    for (unsigned int i = 0; i < n; i++) b[i] = v;
}

static inline unsigned long long rdtsc(void) {
    unsigned int lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}

static inline unsigned long long tsc_to_ms(unsigned long long tsc) {
    return tsc / (TSC_FREQ_HZ / 1000);
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

// ★ Escolhe a próxima thread:
//   1. Maior prioridade
//   2. Empate → menor run_time
//   3. Se current rodou > 150 ms → força troca
static struct thread *pick_next(void) {
    struct thread *best = 0;

    struct task *p = process_list;
    while (p) {
        struct thread *t = p->threads;
        while (t) {
            if (t->state == THREAD_READY) {
                if (!best) {
                    best = t;
                } else if (t->priority > best->priority) {
                    best = t;
                } else if (t->priority == best->priority) {
                    // Round-robin: menor run_time
                    if (t->run_time < best->run_time) best = t;
                }
            }
            t = t->next;
        }
        p = p->next;
    }

    // ★ Se current rodou > 150 ms, força troca para best
    if (current_thread && best == current_thread) {
        unsigned long long run_ms = tsc_to_ms(rdtsc() - current_thread->exec_start_tsc);
        if (run_ms > MAX_RUN_MS) {
            // Procura outra thread (qualquer uma READY diferente de current)
            p = process_list;
            while (p) {
                struct thread *t = p->threads;
                while (t) {
                    if (t->state == THREAD_READY && t != current_thread) {
                        return t;
                    }
                    t = t->next;
                }
                p = p->next;
            }
        }
    }

    return best;
}

void task_init(void) {
    memset8(&kernel_process, 0, sizeof(kernel_process));
    memset8(&idle_thread, 0, sizeof(idle_thread));

    kernel_process.pid = 0;
    kernel_process.threads = &idle_thread;
    kernel_process.thread_count = 1;

    idle_thread.tid = 0;
    idle_thread.state = THREAD_RUNNING;
    idle_thread.priority = 1;
    idle_thread.run_time = 0;
    idle_thread.exec_start_tsc = rdtsc();
    idle_thread.proc = &kernel_process;
    idle_thread.entry = idle_entry;

    unsigned int *stack_base;
    idle_thread.kernel_stack_top = alloc_kernel_stack(&stack_base);
    if (!idle_thread.kernel_stack_top) {
        serial_print("[task] ERRO: sem stack para idle\n");
        return;
    }
    idle_thread.kernel_stack = stack_base;

    idle_thread.ctx.esp = idle_thread.kernel_stack_top;
    idle_thread.ctx.eip = (unsigned int)idle_entry;

    process_list = &kernel_process;
    current_process = &kernel_process;
    current_thread = &idle_thread;

    serial_print("[task] init: pid=0 (kernel), tid=0 (idle, prio=1)\n");
}

unsigned int process_create(void) {
    struct task *p = (struct task *)pmm_alloc_frame();
    if (!p) return 0;
    memset8(p, 0, sizeof(*p));

    p->pid = next_pid++;

    void *pd_phys = pmm_alloc_frame();
    if (!pd_phys) { pmm_free_frame(p); return 0; }

    unsigned int pd_virt = 0x40000000;
    if (vmm_map(pd_virt, (unsigned int)pd_phys, VMM_PRESENT | VMM_WRITE) != 0) {
        pmm_free_frame(pd_phys);
        pmm_free_frame(p);
        return 0;
    }

    unsigned int cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    unsigned int *src = (unsigned int *)(cr3 & ~0xFFF);
    unsigned int *dst = (unsigned int *)pd_virt;
    for (unsigned int i = 0; i < 1024; i++) dst[i] = src[i];

    vmm_unmap(pd_virt);
    p->page_directory = (unsigned int *)pd_phys;

    struct task *tail = process_list;
    while (tail->next) tail = tail->next;
    tail->next = p;

    serial_print("[task] processo criado pid=");
    serial_print_dec(p->pid);
    serial_print("\n");

    return p->pid;
}

unsigned int thread_create_prio(unsigned int pid, void (*entry)(void *), void *arg, unsigned int prio) {
    if (!entry) return 0;

    struct task *p = process_list;
    while (p && p->pid != pid) p = p->next;
    if (!p) return 0;

    if (prio < THREAD_PRIO_MIN) prio = THREAD_PRIO_MIN;
    if (prio > THREAD_PRIO_MAX) prio = THREAD_PRIO_MAX;

    struct thread *t = (struct thread *)pmm_alloc_frame();
    if (!t) return 0;
    memset8(t, 0, sizeof(*t));

    t->tid = next_tid++;
    t->state = THREAD_READY;
    t->priority = prio;
    t->run_time = 0;
    t->exec_start_tsc = 0;
    t->proc = p;
    t->entry = entry;
    t->arg = arg;

    unsigned int *stack_base;
    unsigned int stack_top = alloc_kernel_stack(&stack_base);
    if (!stack_top) { pmm_free_frame(t); return 0; }
    t->kernel_stack = stack_base;
    t->kernel_stack_top = stack_top;

    unsigned int *sp = (unsigned int *)stack_top;
    *(--sp) = (unsigned int)arg;
    *(--sp) = 0;

    t->ctx.esp = (unsigned int)sp;
    t->ctx.eip = (unsigned int)entry;

    t->next = p->threads;
    p->threads = t;
    p->thread_count++;

    serial_print("[task] thread tid=");
    serial_print_dec(t->tid);
    serial_print(" pid=");
    serial_print_dec(p->pid);
    serial_print(" prio=");
    serial_print_dec(prio);
    serial_print("\n");

    return t->tid;
}

unsigned int thread_create(unsigned int pid, void (*entry)(void *), void *arg) {
    return thread_create_prio(pid, entry, arg, THREAD_PRIO_DEFAULT);
}

void thread_exit(void) {
    __asm__ volatile ("cli");
    if (current_thread) current_thread->state = THREAD_ZOMBIE;
    task_schedule();
    while (1) __asm__ volatile ("hlt");
}

void thread_sleep(unsigned int ticks) {
    if (!current_thread || ticks == 0) return;
    current_thread->wakeup_tick = pit_ticks() + ticks;
    current_thread->state = THREAD_BLOCKED;
    task_schedule();
}

void thread_yield(void) {
    task_schedule();
}

// ★ Scheduler Round-Robin com prioridade
void task_schedule(void) {
    if (!current_thread) return;

    unsigned long long now_tick = pit_ticks();

    // Acorda threads bloqueadas
    struct task *p = process_list;
    while (p) {
        struct thread *t = p->threads;
        while (t) {
            if (t->state == THREAD_BLOCKED && t->wakeup_tick && now_tick >= t->wakeup_tick) {
                t->state = THREAD_READY;
                t->wakeup_tick = 0;
            }
            t = t->next;
        }
        p = p->next;
    }

    struct thread *prev = current_thread;

    // ★ Atualiza run_time de prev
    unsigned long long now_tsc = rdtsc();
    prev->run_time = tsc_to_ms(now_tsc - prev->exec_start_tsc);

    // Escolhe próxima
    struct thread *next = pick_next();

    // Log
    static unsigned int sched_count = 0;
    if (++sched_count <= 30) {
        serial_print("[sched] cur=");
        serial_print_dec(prev->tid);
        serial_print(" run_ms=");
        serial_print_dec64(prev->run_time);
        serial_print(" next=");
        serial_print_dec(next ? next->tid : 999);
        serial_print("\n");
    }

    if (!next) {
        prev->state = THREAD_RUNNING;
        return;
    }

    if (next == prev) {
        // Mesma thread continua — mas se run_time > 150ms, zera (já rodou o suficiente)
        if (prev->run_time > MAX_RUN_MS) {
            prev->run_time = 0;
            prev->exec_start_tsc = rdtsc();
        }
        prev->state = THREAD_RUNNING;
        return;
    }

    // Troca
    next->state = THREAD_RUNNING;
    current_thread = next;
    current_process = next->proc;

    // Zera run_time da nova thread
    next->run_time = 0;
    next->exec_start_tsc = rdtsc();

    tss_set_kernel_stack(next->kernel_stack_top);
    if (next->proc->page_directory) {
        __asm__ volatile ("mov %0, %%cr3" : : "r"(next->proc->page_directory) : "memory");
    }

    context_switch(&prev->ctx, &next->ctx);

    // Restaura prev
    current_thread = prev;
    current_process = prev->proc;
    prev->state = THREAD_RUNNING;
    prev->exec_start_tsc = rdtsc();   // ★ reseta o início de execução
    tss_set_kernel_stack(prev->kernel_stack_top);
    if (prev->proc->page_directory) {
        __asm__ volatile ("mov %0, %%cr3" : : "r"(prev->proc->page_directory) : "memory");
    }
}

unsigned int task_current_tid(void) { return current_thread ? current_thread->tid : 0; }
unsigned int task_current_pid(void) { return current_process ? current_process->pid : 0; }

void task_dump(void) {
    serial_print("\n[task] === Processos/Threads (RR+Prio) ===\n");
    serial_print("[task] atual: pid=");
    serial_print_dec(task_current_pid());
    serial_print(" tid=");
    serial_print_dec(task_current_tid());
    serial_print("\n");

    struct task *p = process_list;
    while (p) {
        serial_print("[task] pid=");
        serial_print_dec(p->pid);
        serial_print(" threads=");
        serial_print_dec(p->thread_count);
        serial_print("\n");

        struct thread *t = p->threads;
        while (t) {
            serial_print("[task]   tid=");
            serial_print_dec(t->tid);
            serial_print(" prio=");
            serial_print_dec(t->priority);
            serial_print(" run_ms=");
            serial_print_dec64(t->run_time);
            serial_print(" state=");
            serial_print_dec(t->state);
            serial_print("\n");
            t = t->next;
        }
        p = p->next;
    }
    serial_print("[task] === Fim ===\n");
}
