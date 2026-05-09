#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "vfs.h"

#define MAX_TASKS 256
#define TASK_STACK_SIZE 4096
#define TASK_NAME_LEN 32

enum task_state {
	TASK_STATE_READY,
	TASK_STATE_RUNNING,
	TASK_STATE_BLOCKED,
	TASK_STATE_EXITED
};

struct task_context;

struct task {
	int pid;
	enum task_state state;
	struct task_context *context;
	uint8_t *stack;
	char name[TASK_NAME_LEN];
	struct task *next;
	struct file *fd_table[VFS_MAX_FD];
};

struct task *task_create(const char *name, void (*entry)(void *), void *arg);
void task_exit(void);
void task_yield(void);
void task_init_context(struct task *t, void (*entry)(void *), void *arg);

#endif
