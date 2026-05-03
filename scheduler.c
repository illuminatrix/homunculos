#include "scheduler.h"
#include "task.h"
#include "pit.h"
#include <string.h>

static struct task *run_queue_head = 0;
static struct task *run_queue_tail = 0;
static struct task *current_task = 0;
static uint32_t tick_count = 0;
#define TIME_SLICE 10

static void idle_task(void *arg) {
	(void)arg;
	while (1) {
		asm volatile("hlt");
	}
}

void scheduler_init(void) {
	struct task *idle = task_create("idle", idle_task, 0);
	if (idle) {
		idle->state = TASK_STATE_READY;
	}
	pit_init(100);
	pit_set_callback(scheduler_tick);
}

void scheduler_add_task(struct task *t) {
	if (!t)
		return;
	t->next = 0;
	if (!run_queue_head) {
		run_queue_head = t;
		run_queue_tail = t;
	} else {
		run_queue_tail->next = t;
		run_queue_tail = t;
	}
}

struct task *scheduler_get_current(void) {
	return current_task;
}

void scheduler_tick(void) {
	tick_count++;
	if ((tick_count % TIME_SLICE) == 0) {
		schedule();
	}
}

static int task_is_idle(struct task *t) {
	return t && t->pid == 0;
}

static void move_to_tail(struct task *t) {
	/* find t in the queue and move it to the tail */
	if (!run_queue_head || !t || run_queue_head == run_queue_tail)
		return;
	/* don't move idle task - keep it as fallback */
	if (task_is_idle(t))
		return;
	if (run_queue_head == t) {
		/* t is the head, simple detach */
		run_queue_head = t->next;
	} else {
		/* find predecessor of t */
		struct task *prev = run_queue_head;
		while (prev && prev->next != t)
			prev = prev->next;
		if (!prev)
			return;
		prev->next = t->next;
	}
	if (t == run_queue_tail)
		return;
	t->next = 0;
	run_queue_tail->next = t;
	run_queue_tail = t;
}

void schedule(void) {
	struct task *prev = current_task;
	struct task *next = 0;

	if (prev) {
		if (prev->state == TASK_STATE_RUNNING)
			prev->state = TASK_STATE_READY;
		if (prev->state != TASK_STATE_EXITED)
			move_to_tail(prev);
	}

	/* skip exited tasks in selection */
	/* first pass: find a ready non-idle task */
	next = run_queue_head;
	while (next) {
		if (next->state == TASK_STATE_READY && !task_is_idle(next))
			break;
		next = next->next;
	}
	/* second pass: include idle if nothing else is ready */
	if (!next) {
		next = run_queue_head;
		while (next) {
			if (next->state == TASK_STATE_READY)
				break;
			next = next->next;
		}
	}

	if (!next)
		return;

	if (next == prev)
		return;

	if (prev) {
		current_task = next;
		next->state = TASK_STATE_RUNNING;
		extern void switch_context(struct task_context*, struct task_context*);
		switch_context(prev->context, next->context);
	} else {
		current_task = next;
		next->state = TASK_STATE_RUNNING;
		extern void context_restore(struct task_context*);
		context_restore(next->context);
	}
}
