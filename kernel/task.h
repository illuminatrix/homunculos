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
	uint32_t *pdir;
};

struct task *task_alloc(void);
struct task *task_fork(uint32_t eip, uint32_t cs, uint32_t eflags, uint32_t parent_fp);
void task_exit(void);
void task_yield(void);
void task_init_context(struct task *t, void (*entry)(void *), void *arg);
void task_init_fork_context(struct task *child, uint32_t fork_esp, uint32_t fork_ebp);

#endif
