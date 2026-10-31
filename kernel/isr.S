; isr.asm - stubs de exceção (0x00..0x1F)
BITS 32

section .text

; Macro para exceções SEM código de erro
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0        ; err_code falso
    push dword %1       ; int_no
    jmp isr_common
%endmacro

; Macro para exceções COM código de erro (CPU já empilha)
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push dword %1       ; int_no (err_code já está no stack)
    jmp isr_common
%endmacro

; --- Exceções 0x00..0x1F ---
ISR_NOERR 0    ; #DE divide by zero
ISR_NOERR 1    ; #DB debug
ISR_NOERR 2    ; NMI
ISR_NOERR 3    ; #BP breakpoint
ISR_NOERR 4    ; #OF overflow
ISR_NOERR 5    ; #BR bound range
ISR_NOERR 6    ; #UD invalid opcode
ISR_NOERR 7    ; #NM device not available
ISR_ERR   8    ; #DF double fault
ISR_NOERR 9    ; coprocessor segment overrun
ISR_ERR   10   ; #TS invalid TSS
ISR_ERR   11   ; #NP segment not present
ISR_ERR   12   ; #SS stack fault
ISR_ERR   13   ; #GP general protection
ISR_ERR   14   ; #PF page fault
ISR_NOERR 15   ; reservado
ISR_NOERR 16   ; #MF x87 FP
ISR_ERR   17   ; #AC alignment check
ISR_NOERR 18   ; #MC machine check
ISR_NOERR 19   ; #XM SIMD FP
ISR_NOERR 20   ; #VE virtualization
ISR_ERR   21   ; #CP control protection
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30   ; #SX security
ISR_NOERR 31   ; reservado

extern isr_handler

isr_common:
    pusha                      ; edi,esi,ebp,esp,ebx,edx,ecx,eax

    mov ax, ds
    push eax                   ; salva ds (32-bit, parte alta zerada)

    mov ax, 0x10               ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                   ; passa struct regs* como argumento
    call isr_handler
    add esp, 4

    pop eax                    ; restaura ds original
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8                 ; descarta int_no e err_code
    iret
