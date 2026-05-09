#include <stdint.h>
#include "task.h"
#include "scheduler.h"
#include "vfs.h"

uint32_t systemcall_table[255];

int
sys_write(int fd, const char *buf, int len)
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

void
sys_create_task(char *name, void (*entry)(void *), void *arg)
{
	task_create(name, entry, arg);
}

void
sys_exit(void)
{
	task_exit();
}

void
sys_yield(void)
{
	task_yield();
}

void
syscall_init(void)
{
	systemcall_table[1] = (uint32_t)sys_write;
	systemcall_table[2] = (uint32_t)sys_create_task;
	systemcall_table[3] = (uint32_t)sys_exit;
	systemcall_table[4] = (uint32_t)sys_yield;
}
