// task.c - Gerenciador de processos e threads
#include "task.h"
#include "pmm.h"
#include "vmm.h"
#include "pit.h"
#include "serial.h"
#include "tss.h"

// ---- Estado global ----

static struct task *process_list = 0;
static struct task *current_process = 0;
static struct thread  *current_thread  = 0;
static unsigned int next_pid = 1;
static unsigned int next_tid = 1;

// Processo kernel (PID 0) e thread idle (TID 0)
static struct task kernel_process;
static struct thread  idle_thread;

// ---- Helpers ----

static void memset8(void *p, unsigned char v, unsigned int n) {
    unsigned char *b = (unsigned char *)p;
    for (unsigned int i = 0; i < n; i++) b[i] = v;
}

// Thread idle (roda quando não há outra pronta) — só hlt
static void idle_entry(void *arg) {
    (void)arg;
    while (1) __asm__ volatile ("hlt");
}

// Aloca uma stack de kernel (4 KiB) do PMM.
// Retorna o topo (esp0) ou 0 em erro.
static unsigned int alloc_kernel_stack(unsigned int **out_base) {
    unsigned int *stack = (unsigned int *)pmm_alloc_frame();
    if (!stack) return 0;
    *out_base = stack;
    return (unsigned int)stack + 4096;
}

// ---- task_init ----

void task_init(void) {
    memset8(&kernel_process, 0, sizeof(kernel_process));
    memset8(&idle_thread, 0, sizeof(idle_thread));

    // Processo kernel (PID 0)
    kernel_process.pid = 0;
    kernel_process.state = 0;
    kernel_process.page_directory = 0;   // usa o do kernel
    kernel_process.threads = &idle_thread;
    kernel_process.thread_count = 1;
    kernel_process.next = 0;

    // Thread idle (TID 0) — roda o loop hlt atual
    idle_thread.tid = 0;
    idle_thread.state = THREAD_READY;
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

    // Contexto inicial: EIP = idle_entry, ESP = topo da stack
    idle_thread.ctx.edi = 0;
    idle_thread.ctx.esi = 0;
    idle_thread.ctx.ebp = 0;
    idle_thread.ctx.ebx = 0;
    idle_thread.ctx.esp = idle_thread.kernel_stack_top;
    idle_thread.ctx.eip = (unsigned int)idle_entry;

    process_list = &kernel_process;
    current_process = &kernel_process;
    current_thread = &idle_thread;

    serial_print("[task] init: pid=0 (kernel), tid=0 (idle)\n");
    serial_print("[task] stack idle: ");
    serial_print_hex((unsigned int)stack_base);
    serial_print("..");
    serial_print_hex(idle_thread.kernel_stack_top - 1);
    serial_print("\n");
}

// ---- process_create ----

unsigned int process_create(void) {
    // 1. Aloca struct do processo
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

    // 2. Aloca 1 frame físico para o page directory
    void *pd_phys = pmm_alloc_frame();
    if (!pd_phys) {
        serial_print("[task] ERRO: sem frame para PD\n");
        pmm_free_frame(p);
        return 0;
    }

    // 3. Mapeia o PD em um endereço virtual temporário para escrever
    //    (0x40000000 é livre no espaço virtual do kernel)
    unsigned int pd_virt = 0x40000000;
    if (vmm_map(pd_virt, (unsigned int)pd_phys, VMM_PRESENT | VMM_WRITE) != 0) {
        serial_print("[task] ERRO: nao conseguiu mapear PD temporario\n");
        pmm_free_frame(pd_phys);
        pmm_free_frame(p);
        return 0;
    }

    // 4. Copia o PD do kernel (CR3 atual) para pd_virt
    unsigned int cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    unsigned int *src = (unsigned int *)(cr3 & ~0xFFF);
    unsigned int *dst = (unsigned int *)pd_virt;
    for (unsigned int i = 0; i < 1024; i++) dst[i] = src[i];

    // 5. Desmapeia o temporário
    vmm_unmap(pd_virt);

    // 6. Guarda o FÍSICO do PD (para CR3)
    p->page_directory = (unsigned int *)pd_phys;

    // 7. Insere na lista
    p->next = process_list->next;
    process_list->next = p;

    serial_print("[task] processo criado pid=");
    serial_print_dec(p->pid);
    serial_print(" pd_phys=");
    serial_print_hex((unsigned int)pd_phys);
    serial_print("\n");

    return p->pid;
}
// ---- thread_create ----

unsigned int thread_create(unsigned int pid, void (*entry)(void *), void *arg) {
    if (!entry) return 0;

    // Acha o processo
    struct task *p = process_list;
    while (p && p->pid != pid) p = p->next;
    if (!p) {
        serial_print("[task] ERRO: pid ");
        serial_print_dec(pid);
        serial_print(" nao encontrado\n");
        return 0;
    }
    if (p->thread_count >= MAX_THREADS) return 0;

    // Aloca struct da thread
    struct thread *t = (struct thread *)pmm_alloc_frame();
    if (!t) return 0;
    memset8(t, 0, sizeof(*t));

    t->tid = next_tid++;
    t->state = THREAD_READY;
    t->proc = p;
    t->entry = entry;
    t->arg = arg;
    t->wakeup_tick = 0;

    // Stack
    unsigned int *stack_base;
    unsigned int stack_top = alloc_kernel_stack(&stack_base);
    if (!stack_top) {
        pmm_free_frame(t);
        return 0;
    }
    t->kernel_stack = stack_base;
    t->kernel_stack_top = stack_top;

    // Contexto inicial: EIP = entry, ESP = topo da stack
    // Empilha o argumento na stack (para a entry ler)
    unsigned int *sp = (unsigned int *)stack_top;
    *(--sp) = (unsigned int)arg;              // 1º argumento (arg)
    *(--sp) = (unsigned int)0;                // endereço de retorno fake (thread_exit)

    t->ctx.edi = 0;
    t->ctx.esi = 0;
    t->ctx.ebp = 0;
    t->ctx.ebx = 0;
    t->ctx.esp = (unsigned int)sp;
    t->ctx.eip = (unsigned int)entry;

    // Insere na lista do processo
    t->next = p->threads;
    p->threads = t;
    p->thread_count++;

    serial_print("[task] thread criada tid=");
    serial_print_dec(t->tid);
    serial_print(" pid=");
    serial_print_dec(p->pid);
    serial_print(" entry=");
    serial_print_hex((unsigned int)entry);
    serial_print("\n");

    return t->tid;
}

// ---- thread_exit ----

void thread_exit(void) {
    __asm__ volatile ("cli");
    if (current_thread) {
        current_thread->state = THREAD_ZOMBIE;
    }
    // Agenda outra thread (não retorna)
    task_schedule();

    // Se task_schedule retornar, é bug
    while (1) __asm__ volatile ("hlt");
}

// ---- thread_sleep ----

void thread_sleep(unsigned int ticks) {
    if (!current_thread || ticks == 0) return;
    current_thread->wakeup_tick = pit_ticks() + ticks;
    current_thread->state = THREAD_BLOCKED;
    task_schedule();
}

// ---- Scheduler ----

static struct thread *find_next_ready(struct thread *start) {
    // Percorre a lista de processos e suas threads
    struct task *p = current_process;
    struct thread  *t = start->next;

    for (unsigned int round = 0; round < 256; round++) {
        // Avança para a próxima thread
        if (!t) {
            p = p->next ? p->next : process_list;
            t = p->threads;
        }
        if (t && t->state == THREAD_READY) return t;
        t = t->next;
    }
    return current_thread;   // fallback: mantém
}

void task_schedule(void) {
    if (!current_thread) return;

    // Acorda threads cujo sleep acabou
    unsigned int now = pit_ticks();
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

    // Encontra próxima pronta
    struct thread *next = find_next_ready(current_thread);
    if (!next || next == current_thread) return;

    struct thread *prev = current_thread;
    if (prev->state == THREAD_RUNNING) prev->state = THREAD_READY;
    next->state = THREAD_RUNNING;

    current_thread = next;
    current_process = next->proc;

    // Atualiza TSS.esp0
    tss_set_kernel_stack(next->kernel_stack_top);

    // Troca CR3 (processo diferente?)
    // Por simplicidade, só troca se o processo tem PD próprio
    if (next->proc->page_directory) {
        __asm__ volatile ("mov %0, %%cr3" : : "r"(next->proc->page_directory) : "memory");
    }

    // Troca contexto
    context_switch(&prev->ctx, &next->ctx);
}

// ---- API de consulta ----

unsigned int task_current_tid(void) { return current_thread ? current_thread->tid : 0; }
unsigned int task_current_pid(void) { return current_process ? current_process->pid : 0; }

void task_dump(void) {
    serial_print("\n[task] === Processos/Threads ===\n");
    serial_print("[task] atual: pid=");
    serial_print_dec(task_current_pid());
    serial_print(" tid=");
    serial_print_dec(task_current_tid());
    serial_print("\n");

    struct task *p = process_list;
    while (p) {
        serial_print("[task] pid=");
        serial_print_dec(p->pid);
        serial_print(" state=");
        serial_print_dec(p->state);
        serial_print(" threads=");
        serial_print_dec(p->thread_count);
        serial_print("\n");

        struct thread *t = p->threads;
        while (t) {
            serial_print("[task]   tid=");
            serial_print_dec(t->tid);
            serial_print(" state=");
            serial_print_dec(t->state);
            serial_print(" esp=");
            serial_print_hex(t->ctx.esp);
            serial_print(" eip=");
            serial_print_hex(t->ctx.eip);
            serial_print("\n");
            t = t->next;
        }
        p = p->next;
    }
}
