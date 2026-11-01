BITS 32
section .text
global context_switch

; void context_switch(struct thread_context *old, struct thread_context *new)
;   [esp+4] = old
;   [esp+8] = new
;
; thread_context:
;   offset 0:  edi
;   offset 4:  esi
;   offset 8:  ebp
;   offset 12: ebx
;   offset 16: esp
;   offset 20: eip
;   offset 24: eflags
;   offset 28: cs
context_switch:
    mov eax, [esp + 4]     ; old
    mov edx, [esp + 8]     ; new

    ; ---- Salva callee-saved de old ----
    mov [eax + 0], edi
    mov [eax + 4], esi
    mov [eax + 8], ebp
    mov [eax + 12], ebx

    ; ---- Salva esp de old (antes do call) ----
    lea ecx, [esp + 12]
    mov [eax + 16], ecx

    ; ---- Salva eip de old (endereço de retorno) ----
    mov ecx, [esp]
    mov [eax + 20], ecx

    ; ---- Salva eflags de old ----
    pushfd
    pop ecx
    mov [eax + 24], ecx

    ; ---- Salva cs de old ----
    xor ecx, ecx
    mov cx, cs
    mov [eax + 28], ecx

    ; ---- Carrega callee-saved de new ----
    mov edi, [edx + 0]
    mov esi, [edx + 4]
    mov ebp, [edx + 8]
    mov ebx, [edx + 12]

    ; ---- Carrega esp de new ----
    mov esp, [edx + 16]

    ; ---- Salta para new->eip com new->eflags e new->cs ----
    ; (via iret, que popa eip, cs, eflags)
    push dword [edx + 24]   ; eflags
    push dword [edx + 28]   ; cs
    push dword [edx + 20]   ; eip
    iret
