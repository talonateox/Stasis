section .data
global in_syscall
in_syscall: dq 0

section .text
global syscall_entry
extern syscall_handler

syscall_entry:
    mov qword [rel in_syscall], 1
    push rcx
    push r11

    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax

    sti

    call syscall_handler

    cli

    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    pop r11
    pop rcx

    mov qword [rel in_syscall], 0
    o64 sysret
