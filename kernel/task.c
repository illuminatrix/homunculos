#include "task.h"
#include "scheduler.h"
#include "vfs.h"
#include "mm.h"
#include "gdt.h"
#include "signal.h"
#include <string.h>

struct task tasks[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE] __attribute__((aligned(16)));
int next_pid = 0;

extern void fork_return(void);

struct task *task_alloc(void)
{
	if (next_pid >= MAX_TASKS)
		return 0;

	struct task *t = &tasks[next_pid];
	t->pid = next_pid;
	t->parent_pid = -1;
	t->exit_status = 0;
	t->state = TASK_STATE_READY;
	t->stack = task_stacks[next_pid];
	t->next = 0;
	t->pdir = kernel_pdir;
	t->is_user = 0;
	t->brk_start = 0;
	t->program_break = 0;
	t->vma_count = 0;
	t->cwd[0] = '/';
	t->cwd[1] = '\0';
	signal_init_task(t);
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

	for (i = 0; i < VFS_MAX_FD; i++) {
		child->fd_table[i] = parent->fd_table[i];
		if (child->fd_table[i])
			child->fd_table[i]->refcount++;
		child->fd_flags[i] = parent->fd_flags[i];
	}

	child->parent_pid = parent->pid;
	if (parent->is_user && parent->pdir != kernel_pdir) {
		child->pdir = mm_clone_pdir(parent->pdir);
		if (!child->pdir)
			return 0;
	} else {
		child->pdir = parent->pdir;
	}
	child->is_user = parent->is_user;
	child->brk_start = parent->brk_start;
	child->program_break = parent->program_break;
	child->vma_count = parent->vma_count;
	memcpy(child->vmas, parent->vmas,
	       parent->vma_count * sizeof(struct vm_area));
	strncpy(child->cwd, parent->cwd, sizeof(child->cwd) - 1);
	child->cwd[sizeof(child->cwd) - 1] = '\0';

	/*
	 * Copy parent's stack data below the int $0x80 frame to child.
	 *
	 * Parent's stack layout at parent_fp (sys_fork's frame pointer):
	 *   parent_fp       = saved ebp (task_main's frame pointer)
	 *   parent_fp + 4   = return addr (to syscall_handler)
	 *   parent_fp + 8   = saved ebx (arg1, syscall_handler)
	 *   parent_fp + 12  = saved ecx (arg2)
	 *   parent_fp + 16  = saved edx (arg3)
	 *   parent_fp + 20  = saved esi (arg4)
	 *   parent_fp + 24  = saved edi (arg5)
	 *   parent_fp + 28  = saved ebp (arg6)
	 *   parent_fp + 32  = eip (from int $0x80)
	 *   parent_fp + 36  = cs
	 *   parent_fp + 40  = eflags
	 *   parent_fp + 44  = user esp (user mode only)
	 *   parent_fp + 48  = user ss (user mode only)
	 *   parent_fp + 52  = data below int $0x80 frame (task_main's locals, frames...)
	 *
	 * task_main's ebp was saved by sys_fork's prologue at parent_fp+0.
	 * We need to adjust it so the child's ebp points into its own stack.
	 */
	uint32_t parent_ebp_val = *(uint32_t *)parent_fp;
	uint32_t stack_bottom = (uint32_t)(parent->stack + TASK_STACK_SIZE);

	/*
	 * Build child's stack from bottom (high addr) to top (low addr):
	 * For user mode (cs == 0x18):
	 *   [ss = 0x20]                    <- iretl pops (if CS.DPL=3)
	 *   [esp = child user stack top]
	 *   [eflags]
	 *   [cs = 0x18]
	 *   [eip]
	 *   [fork_return]                  <- context_restore ret pops; child->esp here
	 *
	 * For kernel mode (cs == 0x08):
	 *   [eflags]
	 *   [cs]
	 *   [eip]
	 *   [fork_return]                  <- context_restore ret pops; child->esp here
	 */
	uint32_t child_end = (uint32_t)(child->stack + TASK_STACK_SIZE);
	uint32_t below_size;
	int user_fork = (cs == GDT_USER_CODE);
	if (user_fork) {
		below_size = stack_bottom - (parent_fp + 52);
	} else {
		below_size = stack_bottom - (parent_fp + 40);
	}
	uint32_t dest = child_end - below_size;

	memcpy((void *)dest, (void *)(parent_fp + (user_fork ? 52 : 40)), below_size);

	if (user_fork) {
		uint32_t user_esp = *(uint32_t *)(parent_fp + 44);
		uint32_t user_stack_top = (uint32_t)(parent->stack
					+ TASK_STACK_SIZE / 2);

		/*
		 * Copy user stack data only if it lives inside the task stack area
		 * (kernel-created user tasks via task_init_user_context).
		 * ELF-loaded tasks (loaded via load_elf_from_vfs / sys_exec) have
		 * their user stack in separately mapped pages at USER_STACK_TOP,
		 * and the child shares the parent's page directory, so no copy
		 * is needed (and the internal arithmetic would be wrong for
		 * external user stacks).
		 */
		if (user_esp >= (uint32_t)parent->stack &&
		    user_esp < user_stack_top) {
			uint32_t user_size = user_stack_top - user_esp;
			uint32_t child_user_esp = (uint32_t)child->stack
					+ (user_esp - (uint32_t)parent->stack);
			memcpy((void *)child_user_esp, (void *)user_esp,
			       user_size);
		}
	}

	uint32_t *sp = (uint32_t *)(dest + below_size);

	if (user_fork) {
		*(--sp) = GDT_USER_DATA;                     /* ss */
		*(--sp) = *(uint32_t *)(parent_fp + 44);     /* user esp */
	}
	*(--sp) = eflags;
	*(--sp) = cs;
	*(--sp) = eip;
	*(--sp) = (uint32_t)fork_return;

	/*
	 * Adjust ebp for child's stack.
	 * If parent's ebp points into the kernel stack area, translate
	 * relative to child's stack.  Otherwise it's a user-space address
	 * (ELF-loaded task) — keep as-is since child shares the pdir.
	 */
	uint32_t child_ebp;
	if (parent_ebp_val >= (uint32_t)parent->stack &&
	    parent_ebp_val < (uint32_t)(parent->stack + TASK_STACK_SIZE)) {
		child_ebp = (uint32_t)child->stack
			+ (parent_ebp_val - (uint32_t)parent->stack);
	} else {
		child_ebp = parent_ebp_val;
	}

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

	/* Wake parent that may be blocked in waitpid */
	if (current->parent_pid >= 0) {
		task_wake(current->parent_pid);
		/* Send SIGCHLD to parent */
		signal_send(current->parent_pid, SIGCHLD);
	}

	task_yield();
}

void task_block(void)
{
	struct task *current = scheduler_get_current();
	if (!current)
		return;
	current->state = TASK_STATE_BLOCKED;
	task_yield();
}

void task_wake(int pid)
{
	for (int i = 0; i < next_pid; i++) {
		if (tasks[i].pid == pid
		    && tasks[i].state == TASK_STATE_BLOCKED) {
			tasks[i].state = TASK_STATE_READY;
			return;
		}
	}
}

int task_waitpid(int pid, int *status, int options)
{
	struct task *current = scheduler_get_current();
	if (!current)
		return -1;

	while (1) {
		struct task *child = task_find_child_exited_pid(current->pid,
							       pid);
		if (child) {
			int cpid = child->pid;
			if (status)
				*status = child->exit_status;
			child->parent_pid = -1;
			return cpid;
		}
		if (options & WNOHANG)
			return 0;
		task_block();
	}
}

void task_set_exit_status(int status)
{
	struct task *current = scheduler_get_current();
	if (!current)
		return;
	current->exit_status = status;
}

struct task *task_find_child_exited(int parent_pid)
{
	return task_find_child_exited_pid(parent_pid, -1);
}

struct task *task_find_child_exited_pid(int parent_pid, int target_pid)
{
	for (int i = 0; i < next_pid; i++) {
		if (tasks[i].parent_pid == parent_pid
		    && tasks[i].state == TASK_STATE_EXITED) {
			if (target_pid <= 0 || tasks[i].pid == target_pid)
				return &tasks[i];
		}
	}
	return 0;
}



void task_yield(void)
{
	schedule();
}
