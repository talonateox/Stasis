#pragma once

#include "../arch/x86_64/idt/idt.h"

struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

void interrupts_init();

static inline void sti() {
    asm volatile("sti" ::: "memory");
}

static inline void cli() {
    asm volatile("cli" ::: "memory" );
}
