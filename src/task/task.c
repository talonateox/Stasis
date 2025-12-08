#include "task.h"

#include "../mem/alloc/heap.h"
#include "../mem/alloc/page_frame_alloc.h"
#include "../io/terminal.h"
#include "../sync/spinlock.h"
#include "../drivers/timer/timer.h"
#include "../usermode/usermode.h"
#include "../arch/x86_64/gdt/gdt.h"
#include "../std/string.h"
#include "../fs/vfs/vfs.h"
#include "../elf/elf.h"
#include "scheduler.h"

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
        task_exit(0);
    }

    uint64_t user_rsp = self->user_stack_virt + self->user_stack_size;
    user_rsp &= ~0xFULL;

    uint64_t kernel_stack_top = (uint64_t)self->stack + self->stack_size;
    tss_set_kernel_stack(kernel_stack_top);

    extern uint64_t current_kernel_stack;
    current_kernel_stack = kernel_stack_top;

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
    task->parent_pid = 0;
    task->state = TASK_READY;
    task->entry_point = entry_point;
    task->wake_tick = 0;
    task->user_stack = NULL;
    task->user_stack_size = 0;
    task->is_user = 0;
    task->exit_code = 0;

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

    task->page_table = page_table_create_user();
    if(task->page_table == NULL) {
        printkf_error("task_create_user(): failed to create page table\n");
        free(task->stack);
        free(task);
        return NULL;
    }

    task->user_stack_size = 0x1000;
    task->user_stack = pfallocator_request_page();
    if(task->user_stack == NULL) {
        printkf_error("task_create_user(): failed to allocate user stack\n");
        free(task->stack);
        free(task);
        return NULL;
    }
    memset(task->user_stack, 0, 0x1000);

    task->user_stack_virt = USER_STACK_TOP - task->user_stack_size;

    uint64_t hhdm_offset = page_get_offset();
    void* phys_addr = (void*)((uint64_t)task->user_stack - hhdm_offset);
    page_map_memory_to(task->page_table, (void*)task->user_stack_virt, phys_addr);

    task->pid = next_pid++;
    task->parent_pid = 0;
    task->state = TASK_READY;
    task->entry_point = entry_point;
    task->wake_tick = 0;
    task->is_user = 1;
    task->exit_code = 0;

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

    return task;
}

task_t* task_create_elf(const char* path, uint64_t stack_size) {
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        printkf_error("task_create_from_elf: failed to open '%s'\n", path);
        return NULL;
    }

    int64_t size = vfs_seek(fd, 0, SEEK_END);
    vfs_seek(fd, 0, SEEK_SET);

    void* elf_data = malloc(size);
    if (elf_data == NULL) {
        vfs_close(fd);
        printkf_error("task_create_from_elf(): out of memory\n");
        return NULL;
    }

    vfs_read(fd, elf_data, size);
    vfs_close(fd);

    task_t* task = task_create_user(NULL, stack_size);
    if (task == NULL) {
        free(elf_data);
        return NULL;
    }

    uint64_t entry = 0;
    if (elf_load(elf_data, size, &entry, task->page_table) < 0) {
        printkf_error("task_create_from_elf(): failed to load '%s'\n", path);
        free(elf_data);
        return NULL;
    }
    free(elf_data);

    task->entry_point = (void(*)())entry;

    return task;
}

task_t* task_find_by_pid(uint32_t pid) {
    uint64_t flags = spin_lock(&task_lock);
    task_t* t = task_list;
    while (t != NULL) {
        if (t->pid == pid) {
            spin_unlock(&task_lock, flags);
            return t;
        }
        t = t->next;
    }
    spin_unlock(&task_lock, flags);
    return NULL;
}

task_t* task_fork() {
    task_t* parent = task_current();
    if (parent == NULL) {
        printkf_error("fork(): no current task\n");
        return NULL;
    }

    task_t* child = (task_t*)malloc(sizeof(task_t));
    if (child == NULL) {
        printkf_error("fork(): failed to allocate child task\n");
        return NULL;
    }

    child->stack = malloc(parent->stack_size);
    if (child->stack == NULL) {
        free(child);
        return NULL;
    }
    child->stack_size = parent->stack_size;
    memcpy(child->stack, parent->stack, parent->stack_size);

    child->user_stack_size = parent->user_stack_size;
    child->user_stack_virt = parent->user_stack_virt;

    child->page_table = page_table_clone_for_user();
    if (child->page_table == NULL) {
        printkf_error("fork(): failed to clone page table\n");
        free(child->stack);
        free(child);
        return NULL;
    }

    uint64_t hhdm_offset = page_get_offset();
    child->user_stack = pfallocator_request_page();
    if (child->user_stack == NULL) {
        printkf_error("fork(): failed to allocate user stack\n");
        free(child->stack);
        free(child);
        return NULL;
    }
    memcpy(child->user_stack, parent->user_stack, 0x1000);
    void* phys_addr = (void*)((uint64_t)child->user_stack - hhdm_offset);
    page_map_memory_to(child->page_table, (void*)child->user_stack_virt, phys_addr);

    child->pid = next_pid++;
    child->parent_pid = parent->pid;
    child->state = TASK_READY;
    child->entry_point = parent->entry_point;
    child->wake_tick = 0;
    child->is_user = parent->is_user;
    child->exit_code = 0;

    extern void fork_child_return();
    extern uint64_t saved_syscall_rsp;

    uint64_t syscall_offset = saved_syscall_rsp - (uint64_t)parent->stack;
    uint64_t child_syscall_rsp = (uint64_t)child->stack + syscall_offset;

    uint64_t* child_sp = (uint64_t*)(child_syscall_rsp - 7 * sizeof(uint64_t));
    child_sp[0] = 0;
    child_sp[1] = 0;
    child_sp[2] = 0;
    child_sp[3] = 0;
    child_sp[4] = 0;
    child_sp[5] = 0;
    child_sp[6] = (uint64_t)fork_child_return;

    child->context = (cpu_state_t*)child_sp;

    uint64_t flags = spin_lock(&task_lock);
    child->next = task_list;
    task_list = child;
    spin_unlock(&task_lock, flags);
    scheduler_add_task(child);

    return child;
}

int task_waitpid(uint32_t pid) {
    task_t* child = task_find_by_pid(pid);
    if (child == NULL) {
        return -1;
    }

    task_t* parent = task_current();
    if (child->parent_pid != parent->pid) {
        return -1;
    }

    while (child->state != TASK_TERMINATED) {
        task_yield();
    }

    int exit_code = child->exit_code;

    return exit_code;
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

    uint64_t hhdm_offset = page_get_offset();
    uint64_t new_cr3_phys = (uint64_t)next->page_table - hhdm_offset;

    asm volatile("mov %0, %%cr3" : : "r"(new_cr3_phys) : "memory");

    uint64_t kernel_stack_top = (uint64_t)next->stack + next->stack_size;
    tss_set_kernel_stack(kernel_stack_top);

    extern uint64_t current_kernel_stack;
    current_kernel_stack = kernel_stack_top;

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

    scheduler_schedule();
}

void task_exit(int code) {
    task_t* current = task_current();

    if(current != NULL) {
        current->state = TASK_TERMINATED;
        current->exit_code = code;
    }

    cli();

    scheduler_schedule();

    printkf_error("task_exit(): SHOULD NEVER REACH HERE\n");

    while(1) {
        asm volatile("cli; hlt");
    }
}
