section .data
global in_syscall
in_syscall: dq 0
global saved_syscall_rsp
saved_syscall_rsp: dq 0
global saved_user_rsp
saved_user_rsp: dq 0
global current_kernel_stack
current_kernel_stack: dq 0

section .text
global syscall_entry
extern syscall_handler

syscall_entry:
    mov qword [rel in_syscall], 1
    mov [rel saved_user_rsp], rsp
    mov rsp, [rel current_kernel_stack]

    push qword [rel saved_user_rsp]
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

    mov [rel saved_syscall_rsp], rsp

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
    pop rsp

    mov qword [rel in_syscall], 0
    o64 sysret

global fork_child_return
fork_child_return:
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
    pop rsp

    xor rax, rax

    mov qword [rel in_syscall], 0
    o64 sysret
