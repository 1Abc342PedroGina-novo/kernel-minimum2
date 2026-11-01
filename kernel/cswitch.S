BITS 32
section .text
global context_switch

context_switch:
    ; 1. Captura os ponteiros das estruturas passados por argumento
    mov eax, [esp + 4]     ; eax = &old_thread->ctx
    mov edx, [esp + 8]     ; edx = &new_thread->ctx

    ; 2. Salva os registradores callee-saved da tarefa antiga
    mov [eax + 0],  edi
    mov [eax + 4],  esi
    mov [eax + 8],  ebp
    mov [eax + 12], ebx

    ; Salva o ESP da tarefa antiga exatamente como estava ANTES de mudar
    mov [eax + 16], esp

    ; Salva o endereço de retorno (EIP) que o C colocou na pilha
    mov ecx, [esp]
    mov [eax + 20], ecx

    ; ==========================================================
    ; TROCA EFETIVA DE CONTEXTO (MUDANÇA DE PILHA)
    ; ==========================================================

    ; 3. Carrega o ESP da NOVA tarefa (Agora apontamos para a pilha dela)
    mov esp, [edx + 16]

    ; 4. Restaura os registradores callee-saved da nova tarefa
    mov edi, [edx + 0]
    mov esi, [edx + 4]
    mov ebp, [edx + 8]
    mov ebx, [edx + 12]

    ; 5. Habilita as interrupções com o STI de forma segura
    sti

    ; 6. Salta para o ponto onde a nova tarefa tinha parado (EIP)
    ; Não usamos 'push' e 'ret' aqui para não violar o topo da nova pilha
    jmp [edx + 20]
