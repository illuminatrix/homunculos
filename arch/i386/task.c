#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "task.h"
#include "mm.h"

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
