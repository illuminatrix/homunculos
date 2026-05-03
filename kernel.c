#include <stdio.h>
#include "kernel.h"
#include "interrupt.h"
#include "mm.h"
#include "pic.h"
#include "irq.h"
#include "scheduler.h"
#include "task.h"

void welcome()
{
    printf("Illuminatrix Kernel!\n");
}

void task_a(void *arg) {
	(void)arg;
	while (1) {
		printf("A");
		task_yield();
	}
}

void task_b(void *arg) {
	(void)arg;
	while (1) {
		printf("B");
		task_yield();
	}
}

void kernel_main(multiboot_info_t *mem_info_ptr)
{
    extern void syscall_init(void);

    syscall_init();
    pic_init();
    load_idt();

    welcome();
    init_mm((mmap_entry_t *)mem_info_ptr->mmap_addr,
            mem_info_ptr->mmap_length);

    scheduler_init();
    task_create("task_a", task_a, 0);
    task_create("task_b", task_b, 0);

    pic_enable_irq(0);

    while (1) {
        asm volatile("hlt");
    }
}
