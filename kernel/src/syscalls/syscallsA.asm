[BITS 64]

extern syscallDispatcher
global syscall_entry

section .text
    syscall_entry:
        swapgs

        mov [gs:8], rsp
        mov rsp, [gs:0]

        push qword [gs:8] ; RSP
        push r11 ; RFLAGS
        push rcx ; RIP

        push rbp
        push rbx
        push rdi
        push rsi
        push rdx
        push rcx
        push r8
        push r9
        push r10
        push r11
        push r12
        push r13
        push r14
        push r15

        mov rdi, rax
        call syscallDispatcher

        pop r15
        pop r14
        pop r13
        pop r12
        pop r11
        pop r10
        pop r9
        pop r8
        pop rcx
        pop rdx
        pop rsi
        pop rdi
        pop rbx
        pop rbp

        pop rcx
        pop r11
        pop rsp

        swapgs
        sysret