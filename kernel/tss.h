// tss.h
#ifndef TSS_H
#define TSS_H


void tss_init(void);
void tss_set_kernel_stack(unsigned int esp0);
unsigned short tss_selector(void);

struct tss_entry {
    unsigned int prev_tss;      // link para TSS anterior (não usado)
    unsigned int esp0;          // ★ stack do kernel em ring 0
    unsigned int ss0;           // ★ segmento de stack em ring 0 (0x10)
    unsigned int esp1;          // ring 1 (não usado)
    unsigned int ss1;
    unsigned int esp2;          // ring 2 (não usado)
    unsigned int ss2;
    unsigned int cr3;           // hardware task switch (não usado)
    unsigned int eip;
    unsigned int eflags;
    unsigned int eax, ecx, edx, ebx;
    unsigned int esp, ebp, esi, edi;
    unsigned int es, cs, ss, ds, fs, gs;
    unsigned int ldt;           // seletor de LDT (não usado)
    unsigned short trap;          // bit de trap (debug)
    unsigned short iomap_base;    // offset do I/O bitmap (0 = sem bitmap)
} __attribute__((packed));

#endif
