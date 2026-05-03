#include <stdint.h>
#include "task.h"

struct task_context {
	uint32_t esp;
	uint32_t ebp;
	uint32_t ebx;
	uint32_t esi;
	uint32_t edi;
};

static struct task_context task_contexts[MAX_TASKS];

void task_init_context(struct task *t, void (*entry)(void *), void *arg) {
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
}
