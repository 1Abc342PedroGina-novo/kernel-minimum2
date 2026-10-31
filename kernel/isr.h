// isr.h
#ifndef ISR_H
#define ISR_H

// Layout do stack no momento em que o stub Assembly chama isr_handler.
// A ordem importa: deve bater exatamente com a ordem dos pushes
// em isr.asm (do último push para o primeiro).
struct regs {
    unsigned int ds;                                         // 1 dword (push eax)
    unsigned int edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; // 8 dwords (pusha)
    unsigned int int_no, err_code;                           // 2 dwords (stub/CPU)
    unsigned int eip, cs, eflags, useresp, ss;               // 5 dwords (CPU)
};

void isr_handler(struct regs *r);

#endif
