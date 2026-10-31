; irq_stubs.asm - stubs das 16 IRQs (0x20..0x2F)
BITS 32

section .text

%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0        ; err_code falso (IRQ não empilha erro)
    push dword %2       ; int_no (0x20..0x2F)
    jmp irq_common
%endmacro

IRQ 0,  0x20
IRQ 1,  0x21
IRQ 2,  0x22
IRQ 3,  0x23
IRQ 4,  0x24
IRQ 5,  0x25
IRQ 6,  0x26
IRQ 7,  0x27
IRQ 8,  0x28
IRQ 9,  0x29
IRQ 10, 0x2A
IRQ 11, 0x2B
IRQ 12, 0x2C
IRQ 13, 0x2D
IRQ 14, 0x2E
IRQ 15, 0x2F

extern irq_handler

irq_common:
    pusha

    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8         ; descarta int_no e err_code
    iret
