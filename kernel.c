#include <stdio.h>
#include "kernel.h"
#include "interrupt.h"
#include "mm.h"
#include "pic.h"
#include "irq.h"
#include <string.h>
#include "scheduler.h"
#include "task.h"
#include "vfs.h"
#include <unistd.h>

void welcome()
{
	printf("Illuminatrix Kernel!\n");
}

void task_main(void *arg) {
	(void)arg;

	int pid = fork();

	if (pid == 0) {
		while (1) {
			printf("B");
			task_yield();
		}
	} else {
		while (1) {
			printf("A");
			task_yield();
		}
	}
}

static void setup_main_task(void (*entry)(void *), void *arg)
{
	struct task *t = task_alloc();
	if (!t)
		return;

	t->fd_table[0] = NULL;
	t->fd_table[1] = vfs_get_stdout();
	t->fd_table[2] = vfs_get_stderr();

	strncpy(t->name, "main", TASK_NAME_LEN - 1);
	t->name[TASK_NAME_LEN - 1] = '\0';

	task_init_context(t, entry, arg);
	scheduler_add_task(t);
}

void kernel_main(multiboot_info_t *mem_info_ptr)
{
	extern void syscall_init(void);

	syscall_init();
	pic_init();
	load_idt();
	vfs_init();

	welcome();
	init_mm((mmap_entry_t *)mem_info_ptr->mmap_addr,
		mem_info_ptr->mmap_length);

	scheduler_init();
	setup_main_task(task_main, 0);

	pic_enable_irq(0);

	while (1) {
		asm volatile("hlt");
	}
}
