#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"

void scheduler_init();
void scheduler_schedule();
void scheduler_add_task(task_t* task);
void scheduler_enable();

#endif
