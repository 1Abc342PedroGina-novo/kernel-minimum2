// pit.c (esqueleto)
#include "pit.h"
#include "irq.h"
#include "serial.h"

#define PIT_CH0   0x40
#define PIT_CMD   0x43
#define PIT_BASE  1193182   // Hz do oscilador

volatile unsigned int ticks = 0;
unsigned int freq_hz = 0;   // frequência REAL (após arredondamento)
static void (*pit_callback)(void) = 0;

static void pit_tick(struct regs *r) {
    (void)r;
    ticks++;

    if (ticks <= 5 || (freq_hz && ticks % freq_hz == 0)) {
        serial_print("[pit] tick ");
        serial_print_dec(ticks);
        serial_print("\n");
    }

    if (pit_callback) pit_callback();
}

void pit_init(unsigned int hz) {
    unsigned int divisor = PIT_BASE / hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 1)      divisor = 1;
freq_hz = PIT_BASE / divisor;  
    
    outb(PIT_CMD, 0x36);                       // canal 0, lobyte/hibyte, modo 3, binário
    outb(PIT_CH0, (unsigned char)(divisor & 0xFF));
    outb(PIT_CH0, (unsigned char)((divisor >> 8) & 0xFF));

    irq_register_handler(0, pit_tick);
    irq_clear_mask(0);
    __asm__ volatile ("sti");

    serial_print("[pit] freq=");
    serial_print_dec(hz);
    serial_print(" Hz, divisor=");
    serial_print_dec(divisor);
    serial_print("\n");
}

unsigned int pit_ticks(void) {
    return ticks;
}

unsigned int pit_frequency(void) {
    return freq_hz ? freq_hz : 1;   // ★ NUNCA retorna 0
}

void pit_set_callback(void (*cb)(void)) {
    pit_callback = cb;
}


