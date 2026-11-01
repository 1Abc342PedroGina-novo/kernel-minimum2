BITS 32
section .text
global context_switch

context_switch:
    ; Pega os argumentos da pilha atual
    mov eax, [esp + 4]     ; eax = &old_task->esp
    mov edx, [esp + 8]     ; edx = &new_task->esp

    ; 1. Salva o contexto da tarefa antiga na PRÓPRIA PILHA dela
    push ebp
    push ebx
    push esi
    push edi

    ; Salva o ESP atual na estrutura da tarefa antiga
    ; (O endereço de retorno EIP já foi salvo na pilha automaticamente pelo 'call')
    mov [eax], esp

    ; ==========================================================
    ; NESTE PONTO OCORRE A TROCA EFETIVA DE TAREFAS
    ; ==========================================================

    ; 2. Carrega o ESP da nova tarefa
    mov esp, [edx]

    ; 3. Restaura o contexto da nova tarefa (desempilhando)
    pop edi
    pop esi
    pop ebx
    pop ebp

    ; Habilita as interrupções com segurança antes de voltar
    sti

    ; Retorna para o EIP da nova tarefa (que já está no topo da nova pilha!)
    ret
