// task.h
#ifndef TASK_H
#define TASK_H


// Estados de thread
#define THREAD_READY    0
#define THREAD_RUNNING  1
#define THREAD_BLOCKED  2
#define THREAD_ZOMBIE   3

// Prioridade (1..32)
#define THREAD_PRIO_MIN     1
#define THREAD_PRIO_MAX     32
#define THREAD_PRIO_DEFAULT 16

#define MAX_PROCESSES 16
#define MAX_THREADS   64

// ---- Estruturas ----

struct thread_context {
    unsigned int edi, esi, ebp, ebx;
    unsigned int esp;
    unsigned int eip;
};

struct thread {
    unsigned int tid;
    unsigned int state;
    unsigned int priority;         // 1..32
    unsigned long long vruntime;         // tempo virtual (escala 1024*1024)
    unsigned long long real_time;        // ticks reais executados

    unsigned int wakeup_tick;      // para sleep

    struct task *proc;
    struct thread_context ctx;

    unsigned int *kernel_stack;
    unsigned int  kernel_stack_top;

    void (*entry)(void *arg);
    void *arg;

    struct thread *next;
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
