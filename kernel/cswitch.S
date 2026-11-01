BITS 32
section .text
global context_switch

context_switch:
    mov eax, [esp + 4]     ; old
    mov edx, [esp + 8]     ; new

    ; Salva callee-saved de old
    mov [eax + 0], edi
    mov [eax + 4], esi
    mov [eax + 8], ebp
    mov [eax + 12], ebx

    ; Salva esp de old (antes do call)
    lea ecx, [esp + 12]
    mov [eax + 16], ecx

    ; Salva eip de old
    mov ecx, [esp]
    mov [eax + 20], ecx

    ; Salva eflags de old
    pushfd
    pop ecx
    mov [eax + 24], ecx

    ; Salva cs de old
    xor ecx, ecx
    mov cx, cs
    mov [eax + 28], ecx

    ; Carrega callee-saved de new
    mov edi, [edx + 0]
    mov esi, [edx + 4]
    mov ebp, [edx + 8]
    mov ebx, [edx + 12]

    ; Carrega esp de new
    mov esp, [edx + 16]

    ; ★ Empilha eflags FORÇANDO IF=1
    mov eax, [edx + 24]     ; eflags de new
    or eax, 0x200           ; ★ seta IF=1
    push eax

    ; Empilha cs, eip
    push dword [edx + 28]   ; cs
    push dword [edx + 20]   ; eip

    ; ★ iret (salta para new->eip com IF=1)
    iret
