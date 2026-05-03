#include "task.h"
#include "scheduler.h"
#include <string.h>

static struct task tasks[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE] __attribute__((aligned(16)));
static int next_pid = 0;

struct task *task_create(const char *name, void (*entry)(void *), void *arg) {
	if (next_pid >= MAX_TASKS)
		return 0;

	struct task *t = &tasks[next_pid];
	t->pid = next_pid;
	t->state = TASK_STATE_READY;
	t->stack = task_stacks[next_pid];
	t->next = 0;

	if (name)
		strncpy(t->name, name, TASK_NAME_LEN - 1);
	t->name[TASK_NAME_LEN - 1] = '\0';

	task_init_context(t, entry, arg);
	next_pid++;

	scheduler_add_task(t);
	return t;
}

void task_exit(void) {
	struct task *current = scheduler_get_current();
	if (!current)
		return;
	current->state = TASK_STATE_EXITED;
	task_yield();
}

void task_yield(void) {
	asm volatile("int $0x31");
}
