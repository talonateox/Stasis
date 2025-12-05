#include "task.h"

#include "../mem/alloc/heap.h"
#include "../io/terminal.h"
#include "../sync/spinlock.h"
#include "../drivers/timer/timer.h"
#include "../usermode/usermode.h"
#include "../arch/x86_64/gdt/gdt.h"

#include <stddef.h>

static task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint32_t next_pid = 1;

static spinlock_t task_lock = {0};

extern void scheduler_schedule();
extern void task_switch_impl(cpu_state_t** old_context, cpu_state_t* new_context);

static void task_entry_wrapper() {
    asm volatile("sti");

    task_t* self = task_current();
    if(self != NULL && self->entry_point != NULL) {
        self->entry_point();
    }

    if(self != NULL) {
        self->state = TASK_TERMINATED;
    }

    while(1) {
        scheduler_schedule();
    }
}

static void user_task_entry_wrapper() {
    task_t* self = task_current();
    if(self == NULL || self->entry_point == NULL) {
        task_exit();
    }

    uint64_t user_rsp = (uint64_t)self->user_stack + self->user_stack_size;
    user_rsp &= ~0xFULL;

    uint64_t kernel_stack_top = (uint64_t)self->stack + self->stack_size;
    tss_set_kernel_stack(kernel_stack_top);

    jump_to_usermode((uint64_t)self->entry_point, user_rsp);
}

void task_init() {
    current_task = NULL;
    task_list = NULL;
    next_pid = 1;
}

task_t* task_create(void (*entry_point)(), uint64_t stack_size) {
    task_t* task = (task_t*)malloc(sizeof(task_t));
    if(task == NULL) {
        printkf_error("task_create(): failed to allocate task\n");
        return NULL;
    }

    task->stack = malloc(stack_size);
    if(task->stack == NULL) {
        printkf_error("task_create(): failed to allocate task stack\n");
        free(task);
        return NULL;
    }

    task->stack_size = stack_size;
    task->pid = next_pid++;
    task->state = TASK_READY;
    task->entry_point = entry_point;
    task->wake_tick = 0;
    task->user_stack = NULL;
    task->user_stack_size = 0;
    task->is_user = 0;

    uint64_t stack_top = (uint64_t)task->stack + stack_size;

    stack_top &= ~0xFULL;

    uint64_t* sp = (uint64_t*)stack_top;

    *--sp = (uint64_t)task_entry_wrapper;

    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    task->context = (cpu_state_t*)sp;

    task->next = task_list;
    task_list = task;

    return task;
}

task_t* task_create_user(void (*entry_point)(), uint64_t stack_size) {
    task_t* task = (task_t*)malloc(sizeof(task_t));
    if(task == NULL) {
        printkf_error("task_create_user(): failed to allocate task\n");
        return NULL;
    }

    task->stack = malloc(8192);
    if(task->stack == NULL) {
        printkf_error("task_create_user(): failed to allocate kernel stack\n");
        free(task);
        return NULL;
    }
    task->stack_size = 8192;

    task->user_stack = malloc(stack_size);
    if(task->user_stack == NULL) {
        printkf_error("task_create_user(): failed to allocate user stack\n");
        free(task->stack);
        free(task);
        return NULL;
    }
    task->user_stack_size = stack_size;

    task->pid = next_pid++;
    task->state = TASK_READY;
    task->entry_point = entry_point;
    task->wake_tick = 0;
    task->is_user = 1;

    uint64_t kstack_top = (uint64_t)task->stack + task->stack_size;
    kstack_top &= ~0xFULL;

    uint64_t* sp = (uint64_t*)kstack_top;

    *--sp = (uint64_t)user_task_entry_wrapper;

    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    task->context = (cpu_state_t*)sp;

    task->next = task_list;
    task_list = task;

    printkf_info("Created user task PID %d\n", task->pid);

    return task;
}

task_t* task_current() {
    return current_task;
}

void task_switch(task_t* next) {
    if(next == NULL || next == current_task) {
        return;
    }

    task_t* old_task = current_task;

    if(old_task != NULL && old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }

    next->state = TASK_RUNNING;
    current_task = next;

    uint64_t kernel_stack_top = (uint64_t)next->stack + next->stack_size;
    tss_set_kernel_stack(kernel_stack_top);

    if(old_task != NULL) {
        task_switch_impl(&old_task->context, next->context);
    } else {
        task_switch_impl(NULL, next->context);
    }
}

void task_yield() {
    scheduler_schedule();
}

void task_block() {
    uint64_t flags = spin_lock(&task_lock);

    if(current_task != NULL) {
        current_task->state = TASK_BLOCKED;
    }

    spin_unlock(&task_lock, flags);

    scheduler_schedule();
}

void task_unblock(task_t* task) {
    if(task == NULL) return;

    uint64_t flags = spin_lock(&task_lock);

    if(task->state == TASK_BLOCKED) {
        task->state = TASK_READY;
        task->wake_tick = 0;
    }

    spin_unlock(&task_lock, flags);
}

void sleep_ms(uint64_t ms) {
    if(ms == 0) return;
    if(current_task == NULL) return;

    uint64_t ticks_to_sleep = (ms + 9) / 10;
    if(ticks_to_sleep == 0) ticks_to_sleep = 1;

    asm volatile("cli");

    uint64_t now = timer_get_ticks();
    current_task->wake_tick = now + ticks_to_sleep;
    current_task->state = TASK_BLOCKED;

    printkf("[T%d] sleep %llums, now=%llu, wake_at=%llu\n",
            current_task->pid, ms, now, current_task->wake_tick);

    scheduler_schedule();
}

void task_exit() {
    if(current_task != NULL) {
        current_task->state = TASK_TERMINATED;
    }
    scheduler_schedule();
    while(1) {
        asm volatile("hlt");
    }
}
