// tss.h
#ifndef TSS_H
#define TSS_H


void tss_init(void);
void tss_set_kernel_stack(unsigned int esp0);
unsigned short tss_selector(void);

#endif
