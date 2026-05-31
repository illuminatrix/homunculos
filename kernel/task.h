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
	int parent_pid;
	int exit_status;
	enum task_state state;
	struct task_context *context;
	uint8_t *stack;
	char name[TASK_NAME_LEN];
	struct task *next;
	struct file *fd_table[VFS_MAX_FD];
	uint32_t *pdir;
	int is_user;
	uint32_t brk_start;
	uint32_t program_break;
	char cwd[256];
	uint32_t wakeup_tick;
};

struct task *task_alloc(void);
struct task *task_fork(uint32_t eip, uint32_t cs, uint32_t eflags, uint32_t parent_fp);
void task_exit(void);
void task_yield(void);
void task_init_context(struct task *t, void (*entry)(void *), void *arg);
void task_init_user_context(struct task *t, void (*entry)(void *), void *arg);
void task_init_fork_context(struct task *child, uint32_t fork_esp, uint32_t fork_ebp);
void task_set_exit_status(int status);
struct task *task_find_child_exited(int parent_pid);
void task_update_context_user(struct task *t, uint32_t entry, uint32_t user_esp_top);
void task_set_pdir(struct task *t, uint32_t *pdir);
void task_block(void);
#define WNOHANG 1

void task_wake(int pid);
int task_waitpid(int pid, int *status, int options);
struct task *task_find_child_exited_pid(int parent_pid, int target_pid);

#endif
