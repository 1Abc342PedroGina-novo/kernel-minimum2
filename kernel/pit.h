#ifndef PIT_H
#define PIT_H

// Configura o PIT no canal 0 com a frequência dada (Hz).
// Limite prático: 19 Hz .. 1 MHz (divisor 16-bit).
void pit_init(unsigned int frequency_hz);

// Retorna o número de ticks desde pit_init().
unsigned int pit_ticks(void);
void pit_set_callback(void (*cb)(void));
#endif
