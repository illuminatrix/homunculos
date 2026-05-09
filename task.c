#include "task.h"
#include "scheduler.h"
#include "vfs.h"
#include "mm.h"
#include <string.h>

static struct task tasks[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE] __attribute__((aligned(16)));
static int next_pid = 0;

extern void fork_return(void);

struct task *task_alloc(void)
{
	if (next_pid >= MAX_TASKS)
		return 0;

	struct task *t = &tasks[next_pid];
	t->pid = next_pid;
	t->state = TASK_STATE_READY;
	t->stack = task_stacks[next_pid];
	t->next = 0;
	t->pdir = kernel_pdir;
	next_pid++;

	return t;
}

struct task *task_fork(uint32_t eip, uint32_t cs, uint32_t eflags,
		       uint32_t parent_fp)
{
	int i;
	struct task *parent = scheduler_get_current();
	if (!parent)
		return 0;

	struct task *child = task_alloc();
	if (!child)
		return 0;

	strncpy(child->name, parent->name, TASK_NAME_LEN - 1);
	child->name[TASK_NAME_LEN - 1] = '\0';

	for (i = 0; i < VFS_MAX_FD; i++)
		child->fd_table[i] = parent->fd_table[i];

	child->pdir = mm_clone_pdir(parent->pdir);
	if (!child->pdir) {
		child->state = TASK_STATE_EXITED;
		return 0;
	}

	/*
	 * Copy parent's stack data below the int $0x80 frame to child.
	 *
	 * Parent's stack layout at parent_fp (sys_fork's frame pointer):
	 *   parent_fp       = saved ebp (task_main's frame pointer)
	 *   parent_fp + 4   = return addr (to syscall_handler)
	 *   parent_fp + 8   = saved ebx (syscall_handler)
	 *   parent_fp + 12  = saved ecx
	 *   parent_fp + 16  = saved edx
	 *   parent_fp + 20  = eip (from int $0x80)
	 *   parent_fp + 24  = cs
	 *   parent_fp + 28  = eflags
	 *   parent_fp + 32  = data below int $0x80 frame (task_main's locals, frames...)
	 *
	 * task_main's ebp was saved by sys_fork's prologue at parent_fp+0.
	 * We need to adjust it so the child's ebp points into its own stack.
	 */
	uint32_t parent_ebp_val = *(uint32_t *)parent_fp;
	uint32_t stack_bottom = (uint32_t)(parent->stack + TASK_STACK_SIZE);
	uint32_t below_size = stack_bottom - (parent_fp + 32);

	/*
	 * Build child's stack from bottom (high addr) to top (low addr):
	 *   [data below int $0x80 frame]   <- child's esp after fork_return iret
	 *   [eflags]                       <- iret pops
	 *   [cs]
	 *   [eip]
	 *   [fork_return]                  <- context_restore ret pops; child->esp here
	 */
	uint32_t child_end = (uint32_t)(child->stack + TASK_STACK_SIZE);
	uint32_t dest = child_end - below_size;

	memcpy((void *)dest, (void *)(parent_fp + 32), below_size);

	uint32_t *sp = (uint32_t *)dest;
	*(--sp) = eflags;
	*(--sp) = cs;
	*(--sp) = eip;
	*(--sp) = (uint32_t)fork_return;

	/* adjust ebp for child's stack */
	uint32_t child_ebp = (uint32_t)child->stack
		+ (parent_ebp_val - (uint32_t)parent->stack);

	task_init_fork_context(child, (uint32_t)sp, child_ebp);
	scheduler_add_task(child);
	return child;
}

void task_exit(void)
{
	struct task *current = scheduler_get_current();
	if (!current)
		return;
	current->state = TASK_STATE_EXITED;
	task_yield();
}

void task_yield(void)
{
	asm volatile("int $0x31");
}
