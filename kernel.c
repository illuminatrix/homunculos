#include <stdio.h>
#include "kernel.h"
#include "interrupt.h"
#include "mm.h"
#include "pic.h"
#include "irq.h"
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
	task_create("main", task_main, 0);

	pic_enable_irq(0);

	while (1) {
		asm volatile("hlt");
	}
}
