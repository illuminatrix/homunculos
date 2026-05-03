#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"

void scheduler_init(void);
void scheduler_add_task(struct task *t);
struct task *scheduler_get_current(void);
void scheduler_tick(void);
void schedule(void);

#endif
