#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "task.h"
#include "scheduler.h"
#include "vfs.h"
#include "syscall.h"

uint32_t systemcall_table[255];

int
sys_write(int fd, const void *buf, size_t len)
{
	struct task *current = scheduler_get_current();
	struct file *f;

	if (!current || fd < 0 || fd >= VFS_MAX_FD)
		return -1;

	f = current->fd_table[fd];
	if (!f || !f->ops || !f->ops->write)
		return -1;

	return f->ops->write(f, buf, len);
}

int
sys_exit(int status)
{
	(void)status;
	task_exit();
	return 0;
}

int
sys_read(int fd, void *buf, size_t len)
{
	struct task *current = scheduler_get_current();
	struct file *f;

	if (!current || fd < 0 || fd >= VFS_MAX_FD)
		return -1;

	f = current->fd_table[fd];
	if (!f || !f->ops || !f->ops->read)
		return -1;

	return f->ops->read(f, buf, len);
}

int
sys_yield(void)
{
	task_yield();
	return 0;
}

int
sys_fork(void)
{
	uint32_t eip, cs, eflags, fp;

	__asm__ volatile(
		"movl 20(%%ebp), %0\n\t"
		"movl 24(%%ebp), %1\n\t"
		"movl 28(%%ebp), %2\n\t"
		"movl %%ebp, %3\n\t"
		: "=r"(eip), "=r"(cs), "=r"(eflags), "=r"(fp)
	);

	struct task *child = task_fork(eip, cs, eflags, fp);
	if (!child)
		return -1;

	return child->pid;
}

void
syscall_init(void)
{
	systemcall_table[SYS_exit]        = (uint32_t)sys_exit;
	systemcall_table[SYS_fork]        = (uint32_t)sys_fork;
	systemcall_table[SYS_read]        = (uint32_t)sys_read;
	systemcall_table[SYS_write]       = (uint32_t)sys_write;
	systemcall_table[SYS_sched_yield] = (uint32_t)sys_yield;
}
