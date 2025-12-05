#include "scheduler.h"
#include "task.h"

#include <stddef.h>

#include "../io/terminal.h"
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
    if (task == NULL) {
        return;
    }

    uint64_t flags = spin_lock(&scheduler_lock);

    task->next = NULL;

    if (ready_queue_tail == NULL) {
        ready_queue_head = task;
        ready_queue_tail = task;
    } else {
        ready_queue_tail->next = task;
        ready_queue_tail = task;
    }

    spin_unlock(&scheduler_lock, flags);
}

void scheduler_remove_task(task_t* task) {
    if (task == NULL || ready_queue_head == NULL) {
        return;
    }

    uint64_t flags = spin_lock(&scheduler_lock);

    if (ready_queue_head == NULL) {
        spin_unlock(&scheduler_lock, flags);
        return;
    }

    task_t* prev = NULL;
    task_t* curr = ready_queue_head;

    while (curr != NULL) {
        if (curr == task) {
            if (prev == NULL) {
                ready_queue_head = curr->next;
            } else {
                prev->next = curr->next;
            }

            if (curr == ready_queue_tail) {
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

void scheduler_schedule() {
    if (!scheduler_enabled) {
        return;
    }

    uint64_t flags = spin_lock(&scheduler_lock);

    task_t* current = task_current();
    task_t* next = NULL;

    task_t* start = (current != NULL && current->next != NULL)
                    ? current->next
                    : ready_queue_head;

    task_t* candidate = start;

    do {
        if (candidate == NULL) {
            candidate = ready_queue_head;
        }

        if (candidate == NULL) {
            break;
        }

        if (candidate->state == TASK_READY ||
            (candidate->state == TASK_RUNNING && candidate == current)) {
            next = candidate;
            break;
        }

        candidate = candidate->next;
        if (candidate == NULL) {
            candidate = ready_queue_head;
        }

    } while (candidate != start);

    spin_unlock(&scheduler_lock, flags);

    if (next != NULL && next != current) {
        task_switch(next);
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
    scheduler_schedule();
}
