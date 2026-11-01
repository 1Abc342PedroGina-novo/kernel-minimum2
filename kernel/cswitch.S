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

    ; Salva eip de old (endereço de retorno)
    mov ecx, [esp]
    mov [eax + 20], ecx

    ; Carrega callee-saved de new
    mov edi, [edx + 0]
    mov esi, [edx + 4]
    mov ebp, [edx + 8]
    mov ebx, [edx + 12]

    ; Carrega esp de new
    mov esp, [edx + 16]

    ; ★ Retorna para new->eip (ponto de retorno do call)
    mov eax, [edx + 20]
    push eax
    ret
