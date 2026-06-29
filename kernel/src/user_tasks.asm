[BITS 64]

section .text

global userTaskACodeStart
global userTaskACodeEnd
global userTaskBCodeStart
global userTaskBCodeEnd

userTaskACodeStart:
    lea rdi, [rel msgA]
    mov rsi, 16
    mov rax, 0xFF00000000000001
    syscall
.loopA:
    mov ecx, 2000000
.innerA:
    nop
    dec ecx
    jnz .innerA
    jmp .loopA
msgA: db "TASK-A: started!", 10
userTaskACodeEnd:

userTaskBCodeStart:
    lea rdi, [rel msgB]
    mov rsi, 16
    mov rax, 0xFF00000000000001
    syscall
.loopB:
    mov ecx, 2000000
.innerB:
    nop
    dec ecx
    jnz .innerB
    jmp .loopB
msgB: db "TASK-B: started!", 10
userTaskBCodeEnd:
