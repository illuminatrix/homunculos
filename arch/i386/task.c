#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "task.h"
#include "mm.h"
#include "gdt.h"

struct task_context {
	uint32_t esp;
	uint32_t ebp;
	uint32_t ebx;
	uint32_t esi;
	uint32_t edi;
	uint32_t pdir;
};

static struct task_context task_contexts[MAX_TASKS];

void task_init_context(struct task *t, void (*entry)(void *), void *arg)
{
	t->context = &task_contexts[t->pid];

	uint32_t *stack = (uint32_t *)(t->stack + TASK_STACK_SIZE);

	*(--stack) = (uint32_t)arg;
	*(--stack) = (uint32_t)task_exit;
	*(--stack) = (uint32_t)entry;

	t->context->esp = (uint32_t)stack;
	t->context->ebp = 0;
	t->context->ebx = 0;
	t->context->esi = 0;
	t->context->edi = 0;
	t->context->pdir = (uint32_t)t->pdir;
}

void task_init_user_context(struct task *t, void (*entry)(void *), void *arg)
{
	t->context = &task_contexts[t->pid];
	t->is_user = 1;

	/*
	 * Stack layout (4KB task stack):
	 *   High: kernel stack at TSS.ESP0 = t->stack + 4096, grows down
	 *   Mid:  iretl frame (5 * 4 = 20 bytes)
	 *   Low:  user stack starts at midpoint, grows down
	 *
	 * This keeps the kernel stack (used for IRQ/syscall entry from ring 3)
	 * well clear of the user stack.
	 */
	uint32_t *usp = (uint32_t *)(t->stack + TASK_STACK_SIZE / 2);

	/* User stack: what entry sees when called */
	*(--usp) = (uint32_t)arg;				/* arg at esp+4 */
	*(--usp) = (uint32_t)task_exit;				/* return address at esp+0 */
	uint32_t user_esp = (uint32_t)usp;

	/* iretl frame above user stack, below kernel stack area */
	uint32_t *ksp = usp;

	*(--ksp) = GDT_USER_DATA;				/* ss */
	*(--ksp) = user_esp;					/* user esp */
	*(--ksp) = 0x200;					/* eflags (IF=1) */
	*(--ksp) = GDT_USER_CODE;				/* user cs */
	*(--ksp) = (uint32_t)entry;				/* eip */

	t->context->esp = (uint32_t)ksp;
	t->context->ebp = 0;
	t->context->ebx = 0;
	t->context->esi = 0;
	t->context->edi = 0;
	t->context->pdir = (uint32_t)t->pdir;
}

void task_init_fork_context(struct task *child, uint32_t fork_esp, uint32_t fork_ebp)
{
	child->context = &task_contexts[child->pid];
	child->context->esp = fork_esp;
	child->context->ebp = fork_ebp;
	child->context->ebx = 0;
	child->context->esi = 0;
	child->context->edi = 0;
	child->context->pdir = (uint32_t)child->pdir;
}
