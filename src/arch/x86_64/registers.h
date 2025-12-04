#pragma once

#include <stdint.h>
#include "../../interrupts/interrupts.h"

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t error_code;
    struct interrupt_frame iframe;
} __attribute__((packed)) registers_t;