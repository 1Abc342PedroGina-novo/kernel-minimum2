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

    ; ---- Salva esp (antes do call, com args + retorno) ----
    lea ecx, [esp + 12]
    mov [eax + 16], ecx

    ; ---- Salva eip (endereço de retorno) ----
    mov ecx, [esp]
    mov [eax + 20], ecx

    ; ---- Salva eflags ----
    pushfd
    pop ecx
    mov [eax + 24], ecx

    ; ---- Salva cs ----
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

    ; ---- Prepara a stack da nova thread para o stub de IRQ ----
    ; O stub de IRQ, após 'ret' de context_switch, vai executar:
    ;   pop eax        (ds)
    ;   mov ds, ax
    ;   popa           (8 registradores)
    ;   add esp, 8     (int_no, err_code)
    ;   iret           (eip, cs, eflags)
    ;
    ; Então precisamos empilhar, na ordem inversa:
    ;   eflags, cs, eip, err_code, int_no, eax..edi, ds
    ;
    ; Ordem correta na stack (do topo para baixo):
    ;   [esp+0]  ds
    ;   [esp+4]  edi
    ;   [esp+8]  esi
    ;   [esp+12] ebp
    ;   [esp+16] esp_dummy
    ;   [esp+20] ebx
    ;   [esp+24] edx
    ;   [esp+28] ecx
    ;   [esp+32] eax
    ;   [esp+36] int_no
    ;   [esp+40] err_code
    ;   [esp+44] eip
    ;   [esp+48] cs
    ;   [esp+52] eflags

    ; Empilha na ordem inversa (eflags primeiro, ds por último)
    push dword [edx + 24]   ; eflags
    push dword [edx + 28]   ; cs
    push dword [edx + 20]   ; eip
    push dword 0            ; err_code
    push dword 0            ; int_no
    pusha                   ; 8 registradores (eax..edi)
    push dword 0x10         ; ds

    ; ---- Retorna para task_schedule ----
    ; O 'ret' vai saltar para o endereço de retorno do 'call context_switch'
    ; em task_schedule. Quando o stub de IRQ continuar (após task_schedule
    ; retornar), ele vai popar os valores empilhados acima.
    ret
