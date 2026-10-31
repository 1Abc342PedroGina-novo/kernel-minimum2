// isr.h
#ifndef ISR_H
#define ISR_H

// Layout do stack no momento em que o stub Assembly chama isr_handler.
// A ordem importa: deve bater exatamente com a ordem dos pushes
// em isr.asm (do último push para o primeiro).
struct regs {
    unsigned int ds;                        // segmentos (pushed por último)
    unsigned int gs;
    unsigned int fs;
    unsigned int es;
    unsigned int edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; // pusha/popa
    unsigned int int_no, err_code;                      // pushed pelo stub
    unsigned int eip, cs, eflags, useresp, ss;          // pushed pela CPU
};

void isr_handler(struct regs *r);

#endif
