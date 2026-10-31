; boot.asm - Entrada do kernel (Multiboot)
BITS 32

section .multiboot
align 4
    dd 0x1BADB002            ; magic number
    dd 0x00000000            ; flags
    dd -(0x1BADB002 + 0x00)  ; checksum

section .text
global _start
extern kmain

_start:
    cli                      ; desabilita interrupções
    mov esp, stack_top       ; configura stack
    push ebx                 ; multiboot info pointer
    push eax                 ; multiboot magic
    call kmain               ; chama kernel em C

.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384               ; 16 KiB de stack
stack_top:
