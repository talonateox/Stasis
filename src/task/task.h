#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../mem/paging/paging.h"

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

#define USER_STACK_TOP 0x7FFFFFF00000ULL

typedef struct task {
    uint32_t pid;
    uint32_t parent_pid;
    task_state_t state;
    cpu_state_t* context;
    void* stack;
    page_table_t* page_table;
    uint64_t stack_size;
    void (*entry_point)();
    struct task* next;
    uint64_t wake_tick;
    void* user_stack;
    uint64_t user_stack_virt;
    uint64_t user_stack_size;
    uint8_t is_user;
    int exit_code;
} task_t;

void task_init();
task_t* task_create(void (*entry_point)(), uint64_t stack_size);
task_t* task_create_user(void (*entry_point)(), uint64_t stack_size);
task_t* task_create_elf(const char* path, uint64_t stack_size);
task_t* task_current();
void task_switch(task_t* next);
void task_yield();
void task_block();
void task_unblock(task_t* task);
void sleep_ms(uint64_t ms);
void task_exit(int code);

task_t* task_fork();
int task_waitpid(uint32_t pid);
task_t* task_find_by_pid(uint32_t pid);
