[BITS 64]
extern idtHandler

%macro ISR_NOERRCODE 1
global idt%1Stub
idt%1Stub:
    push qword 0
    push qword %1
    jmp idtStub
%endmacro

%macro ISR_ERRCODE 1
global idt%1Stub
idt%1Stub:
    push qword %1
    jmp idtStub
%endmacro

section .text
    ISR_NOERRCODE 0 ; Divide Error
    ISR_NOERRCODE 1 ; Debug
    ISR_NOERRCODE 2 ; NMI
    ISR_NOERRCODE 3 ; Breakpoint
    ISR_NOERRCODE 4 ; Overflow
    ISR_NOERRCODE 5 ; BOUND Range Exceeded
    ISR_NOERRCODE 6 ; Invalid Opcode
    ISR_NOERRCODE 7 ; Device Not Available
    ISR_ERRCODE 8 ; Double Fault
    ISR_NOERRCODE 9 ; Coprocessor Segment Overrun
    ISR_ERRCODE 10 ; Invalid TSS
    ISR_ERRCODE 11 ; Segment Not Present
    ISR_ERRCODE 12 ; Stack-Segment Fault
    ISR_ERRCODE 13 ; General Protection Fault
    ISR_ERRCODE 14 ; Page Fault
    ISR_NOERRCODE 15 ; (reserved)
    ISR_NOERRCODE 16 ; x87 FPU Error
    ISR_ERRCODE 17 ; Alignment Check
    ISR_NOERRCODE 18 ; Machine Check
    ISR_NOERRCODE 19 ; SIMD Floating-Point Exception
    ISR_NOERRCODE 20 ; Virtualization Exception
    ISR_ERRCODE 21 ; Control Protection Exception

    global idtDefaultStub
    idtDefaultStub:
        push qword 0
        push qword 0xFFFFFFFFFFFFFFFF
        jmp idtStub

    idtStub:
        push rax
        push rcx
        push rdx
        push rbx
        push rbp
        push rsi
        push rdi

        mov rdi, rsp
        call idtHandler

        pop rdi
        pop rsi
        pop rbp
        pop rbx
        pop rdx
        pop rcx
        pop rax

        add rsp, 16
        iretq