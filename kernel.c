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

static void shell_prompt(void)
{
	printf("> ");
}

static void shell_main(void *arg)
{
	(void)arg;
	char buf[64];
	int pos = 0;

	shell_prompt();

	while (1) {
		char c;
		int n = read(0, &c, 1);
		if (n <= 0) {
			task_yield();
			continue;
		}

		if (c == '\n') {
			printf("\n");
			buf[pos] = '\0';
			if (pos == 8 && buf[0] == 'g' && buf[1] == 'r'
			    && buf[2] == 'e' && buf[3] == 'e'
			    && buf[4] == 't' && buf[5] == 'i'
			    && buf[6] == 'n' && buf[7] == 'g')
				printf("hello\n");
			else if (pos > 0)
				printf("unknown\n");
			pos = 0;
			shell_prompt();
		} else if (c == '\b') {
			if (pos > 0) {
				pos--;
				printf("\b \b");
			}
		} else if (pos < (int)sizeof(buf) - 1) {
			buf[pos++] = c;
			printf("%c", c);
		}
	}
}

static void setup_main_task(void (*entry)(void *), void *arg)
{
	struct task *t = task_alloc();
	if (!t)
		return;

	t->fd_table[0] = vfs_get_stdin();
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
	setup_main_task(shell_main, 0);

	pic_enable_irq(0);
	pic_enable_irq(1);

	while (1) {
		asm volatile("hlt");
	}
}
