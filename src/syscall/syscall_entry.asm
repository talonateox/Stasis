section .text
global syscall_entry
extern syscall_handler

; When syscall executes:
;   RCX = return RIP
;   R11 = saved RFLAGS
;   RAX = syscall number
;   RDI = arg1
;   RSI = arg2
;   RDX = arg3

syscall_entry:
    ; Save user return address and flags
    push rcx        ; User RIP
    push r11        ; User RFLAGS

    ; Save all callee-saved registers
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save caller-saved registers that we'll clobber
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

    ; Set up arguments for syscall_handler(syscall_num, arg1, arg2, arg3)
    ; Currently: rax=syscall, rdi=arg1, rsi=arg2, rdx=arg3
    ; Need:      rdi=syscall, rsi=arg1, rdx=arg2, rcx=arg3
    mov rcx, rdx    ; arg3 -> rcx (4th argument)
    mov rdx, rsi    ; arg2 -> rdx (3rd argument)
    mov rsi, rdi    ; arg1 -> rsi (2nd argument)
    mov rdi, rax    ; syscall_num -> rdi (1st argument)

    ; Enable interrupts during syscall handling
    sti

    call syscall_handler

    ; Disable interrupts for sysret
    cli

    ; Restore caller-saved registers (but keep rax as return value)
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi

    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Restore user return address and flags
    pop r11         ; User RFLAGS
    pop rcx         ; User RIP

    ; Return to user mode
    ; sysret loads: RIP=RCX, RFLAGS=R11, CS/SS from STAR
    o64 sysret
