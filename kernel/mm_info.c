// meminfo.c
#include "mm_info.h"
#include "pmm.h"
#include "vmm.h"
#include "serial.h"

void pmm_dump(void) {
    serial_print("\n[pmm] === Estado do PMM ===\n");
    serial_print("[pmm] total=");
    serial_print_dec(pmm_total_frames());
    serial_print(" frames (");
    serial_print_dec(pmm_total_frames() * 4);
    serial_print(" KiB)\n");
    serial_print("[pmm] usado=");
    serial_print_dec(pmm_used_frames());
    serial_print(" (");
    serial_print_dec(pmm_used_frames() * 4);
    serial_print(" KiB)\n");
    serial_print("[pmm] livre=");
    serial_print_dec(pmm_free_count());
    serial_print(" (");
    serial_print_dec(pmm_free_count() * 4);
    serial_print(" KiB)\n");

    if (pmm_total_frames() != pmm_used_frames() + pmm_free_count()) {
        serial_print("[pmm] *** INCONSISTENTE ***\n");
    }
}

void mem_dump(void) {
    pmm_dump();
    vmm_dump();
}

void pmm_dump_bitmap(unsigned int n_words) {
    // Acessa o bitmap via API? Não temos acesso direto.
    // Se você expuser o bitmap no pmm.h, podemos dumpar.
    // Alternativa: dumpar frames via API frame-by-frame.
    serial_print("\n[pmm] === Bitmap (primeiras ");
    serial_print_dec(n_words);
    serial_print(" palavras) ===\n");
    serial_print("[pmm] (requer pmm_get_bitmap() — adicione se quiser)\n");
}

void heap_dump(void) {
    serial_print("\n[heap] === Regioes conhecidas ===\n");
    serial_print("[heap] kernel 0x00100000.._kernel_end\n");
    serial_print("[heap] kernel_end = ");
    // extern unsigned char _kernel_end;
    // serial_print_hex((unsigned int)&_kernel_end);
    serial_print("(ver log)\n");
    serial_print("[heap] bitmap PMM: alocado no kernel.c\n");
    serial_print("[heap] pool objs: 0x00200000..\n");
}
