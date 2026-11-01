// task.h
#ifndef TASK_H
#define TASK_H


// Estados de thread
#define THREAD_READY    0
#define THREAD_RUNNING  1
#define THREAD_BLOCKED  2
#define THREAD_ZOMBIE   3

// Prioridade (futuro)
#define THREAD_PRIO_NORMAL 0

#define MAX_PROCESSES 16
#define MAX_THREADS   64

// ---- Estruturas ----

// Contexto salvo em context_switch (callee-saved + EIP/ESP)
struct thread_context {
    unsigned int edi, esi, ebp, ebx;   // callee-saved (salvos no switch)
    unsigned int esp;                  // stack pointer
    unsigned int eip;                  // instruction pointer (retorno)
};

// Thread
struct thread {
    unsigned int tid;
    unsigned int state;                // THREAD_READY / RUNNING / BLOCKED / ZOMBIE
    unsigned int priority;
    unsigned int wakeup_tick;          // para sleep (0 = sem sleep)

    struct task *proc;          // processo dono

    struct thread_context ctx;     // contexto salvo

    unsigned int *kernel_stack;        // stack do kernel (alocada do PMM)
    unsigned int  kernel_stack_top;    // TSS.esp0 quando esta thread roda

    void (*entry)(void *arg);      // função de entrada
    void *arg;

    struct thread *next;           // lista circular
};

// Processo
struct task {
    unsigned int pid;
    unsigned int state;                // 0 = ativo, 1 = morto

    unsigned int *page_directory;      // CR3 próprio (0 = usa o do kernel)

    struct thread *threads;        // lista de threads
    unsigned int thread_count;

    struct task *next;
};

// ---- API ----

// Inicializa o gerenciador. Chame após vmm_init() e pit_init().
// Cria o processo "kernel" (PID 0) e a thread "idle" (TID 0).
void task_init(void);

// Cria um novo processo (espaço de endereçamento próprio).
// Retorna PID (>0) ou 0 em erro.
unsigned int process_create(void);

// Cria uma thread no processo 'pid' (0 = kernel).
//   entry = função de entrada (ring 0)
//   arg   = argumento
// Retorna TID (>0) ou 0 em erro.
unsigned int thread_create(unsigned int pid, void (*entry)(void *), void *arg);

// Termina a thread atual. Nunca retorna.
void thread_exit(void);

// Bloqueia a thread atual até 'wakeup_tick'.
void thread_sleep(unsigned int ticks);

// Scheduler (chamado pelo IRQ0 handler).
void task_schedule(void);

// Estatísticas
unsigned int task_current_tid(void);
unsigned int task_current_pid(void);
void task_dump(void);

// Troca de contexto (Assembly)
extern void context_switch(struct thread_context *old_ctx,
                           struct thread_context *new_ctx);

#endif
