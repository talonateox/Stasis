#include "scheduler.h"
#include "task.h"

#include <stddef.h>

#include "../io/terminal.h"
#include "../drivers/timer/timer.h"
#include "../sync/spinlock.h"

static task_t* ready_queue_head = NULL;
static task_t* ready_queue_tail = NULL;
static int scheduler_enabled = 0;

static spinlock_t scheduler_lock = {0};

void scheduler_init(void) {
    ready_queue_head = NULL;
    ready_queue_tail = NULL;
    scheduler_enabled = 0;
}

void scheduler_add_task(task_t* task) {
    if(task == NULL) {
        return;
    }

    uint64_t flags = spin_lock(&scheduler_lock);

    task->next = NULL;

    if(ready_queue_tail == NULL) {
        ready_queue_head = task;
        ready_queue_tail = task;
    } else {
        ready_queue_tail->next = task;
        ready_queue_tail = task;
    }

    spin_unlock(&scheduler_lock, flags);
}

void scheduler_remove_task(task_t* task) {
    if(task == NULL || ready_queue_head == NULL) {
        return;
    }

    uint64_t flags = spin_lock(&scheduler_lock);

    if(ready_queue_head == NULL) {
        spin_unlock(&scheduler_lock, flags);
        return;
    }

    task_t* prev = NULL;
    task_t* curr = ready_queue_head;

    while(curr != NULL) {
        if(curr == task) {
            if(prev == NULL) {
                ready_queue_head = curr->next;
            } else {
                prev->next = curr->next;
            }

            if(curr == ready_queue_tail) {
                ready_queue_tail = prev;
            }

            task->next = NULL;
            spin_unlock(&scheduler_lock, flags);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    spin_unlock(&scheduler_lock, flags);
}

static void wake_sleeping_tasks(void) {
    uint64_t current_tick = timer_get_ticks();
    task_t* task = ready_queue_head;

    while(task != NULL) {
        if(task->state == TASK_BLOCKED && task->wake_tick != 0) {
            if(current_tick >= task->wake_tick) {
                task->state = TASK_READY;
                task->wake_tick = 0;
            }
        }
        task = task->next;
    }
}

void scheduler_schedule() {
    if(!scheduler_enabled) {
        return;
    }

restart: ;
    uint64_t flags = spin_lock(&scheduler_lock);

    task_t* current = task_current();
    task_t* next = NULL;

    uint64_t current_tick = timer_get_ticks();
    for(task_t* t = ready_queue_head; t != NULL; t = t->next) {
        if(t->state == TASK_BLOCKED && t->wake_tick != 0) {
            if(current_tick >= t->wake_tick) {
                t->state = TASK_READY;
                t->wake_tick = 0;
            }
        }
    }

    task_t* start;
    if(current != NULL && current->next != NULL) {
        start = current->next;
    } else {
        start = ready_queue_head;
    }

    task_t* candidate = start;
    int checked = 0;
    int total_tasks = 0;

    for(task_t* t = ready_queue_head; t != NULL; t = t->next) {
        total_tasks++;
    }

    while(checked < total_tasks) {
        if(candidate == NULL) {
            candidate = ready_queue_head;
        }

        if(candidate == NULL) {
            break;
        }

        if(candidate->state == TASK_READY) {
            next = candidate;
            break;
        }

        candidate = candidate->next;
        if(candidate == NULL) {
            candidate = ready_queue_head;
        }
        checked++;
    }

    spin_unlock(&scheduler_lock, flags);

    if(next != NULL) {
        task_switch(next);
        asm volatile("sti");
    } else if(current != NULL && current->state == TASK_RUNNING) {
        asm volatile("sti");
    } else {
        asm volatile("sti; hlt");
        goto restart;
    }
}


void scheduler_enable() {
    scheduler_enabled = 1;
    printkf_ok("Scheduler enabled\n");
}

void scheduler_disable() {
    scheduler_enabled = 0;
}

int scheduler_is_enabled() {
    return scheduler_enabled;
}

void scheduler_tick() {
    if(!scheduler_enabled) {
        return;
    }

    uint64_t flags = spin_lock(&scheduler_lock);
    wake_sleeping_tasks();
    spin_unlock(&scheduler_lock, flags);

    scheduler_schedule();
}

void scheduler_print_tasks() {
    uint64_t flags = spin_lock(&scheduler_lock);
    task_t* cur = ready_queue_head;
    if(cur == NULL) {
        spin_unlock(&scheduler_lock, flags);
        return;
    }

    while(cur) {
        const char* state_str = "UNKNOWN";
        switch(cur->state) {
            case TASK_READY: state_str = "READY"; break;
            case TASK_RUNNING: state_str = "RUNNING"; break;
            case TASK_BLOCKED: state_str = "BLOCKED"; break;
            case TASK_TERMINATED: state_str = "TERMINATED"; break;
        }

        printkf("%d: task=%p state=%s stack=%llu entry=%p\n", cur->pid, (void*)cur, state_str, cur->stack_size, (void*)cur->entry_point);

        cur = cur->next;
    }
    spin_unlock(&scheduler_lock, flags);
}
