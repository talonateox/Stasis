#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED,
} task_state_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) cpu_state_t;

typedef struct task {
    uint32_t pid;
    task_state_t state;
    cpu_state_t* context;
    void* stack;
    uint64_t stack_size;
    void (*entry_point)(void);
    struct task* next;
} task_t;

void task_init();
task_t* task_create(void (*entry_point)(), uint64_t stack_size);
task_t* task_current();
void task_switch(task_t* next);
void task_yield();
