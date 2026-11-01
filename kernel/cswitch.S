
BITS 32

section .text

global context_switch

; void context_switch(struct thread_context *old, struct thread_context *new)
;   [esp+4] = old (thread_context*)
;   [esp+8] = new (thread_context*)
;
; thread_context:
;   offset 0:  edi
;   offset 4:  esi
;   offset 8:  ebp
;   offset 12: ebx
;   offset 16: esp
;   offset 20: eip
context_switch:
    mov eax, [esp + 4]     ; eax = old
    mov edx, [esp + 8]     ; edx = new

    ; Salva contexto atual em old
    mov [eax + 0], edi
    mov [eax + 4], esi
    mov [eax + 8], ebp
    mov [eax + 12], ebx
    ; esp: antes do call, era esp+16 (com retorno + args)
    ; depois do call, é o esp atual + 12 (retorno 4 + 2 args 8)
    lea ecx, [esp + 12]    ; ecx = esp "antes" do call
    mov [eax + 16], ecx
    mov ecx, [esp]         ; ecx = endereço de retorno (eip)
    mov [eax + 20], ecx

    ; Carrega contexto de new
    mov edi, [edx + 0]
    mov esi, [edx + 4]
    mov ebp, [edx + 8]
    mov ebx, [edx + 12]
    mov esp, [edx + 16]
    ; Carrega eip de new (faz ret para lá)
    mov eax, [edx + 20]
    ; Vai para o novo contexto: empilha eip e ret
    push eax
    ret
