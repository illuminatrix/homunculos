#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "interrupt.h"
#include "mm.h"
#include "pic.h"
#include "irq.h"
#include "scheduler.h"
#include "task.h"
#include "vfs.h"
#include "tmpfs.h"
#include "part.h"
#include "drivers/hello/hello.h"
#include "drivers/ata/ata.h"
#include "shell.h"
#include "gdt.h"

void welcome()
{
	printf("Illuminatrix Kernel!\n");
}

static void setup_main_task(void (*entry)(void *), void *arg)
{
	struct task *t = task_alloc();
	if (!t)
		return;

	struct vfs_inode *kbd = vfs_resolve_path("/dev/kbd");
	struct vfs_inode *vga = vfs_resolve_path("/dev/vga");
	struct vfs_inode *err = vfs_resolve_path("/dev/vgaerr");
	t->fd_table[0] = vfs_open_file(kbd);
	t->fd_table[1] = vfs_open_file(vga);
	t->fd_table[2] = vfs_open_file(err);

	strncpy(t->name, "main", TASK_NAME_LEN - 1);
	t->name[TASK_NAME_LEN - 1] = '\0';

	task_init_user_context(t, entry, arg);
	scheduler_add_task(t);
}

void kernel_main(multiboot_info_t *mem_info_ptr)
{
	extern void syscall_init(void);

	syscall_init();
	pic_init();
	load_idt();
	vfs_init();
	vfs_inode_init();
	tmpfs_init();

	/* Create device nodes in initial tmpfs /dev for early boot output */
	vfs_create_device_nodes();
	hello_driver_init();

	welcome();
	init_mm((mmap_entry_t *)mem_info_ptr->mmap_addr,
		mem_info_ptr->mmap_length);

	gdt_init();

	/* Init block device drivers (ATA), parse partitions, mount ext2 */
	printf("ata: probing...\n");
	ata_init();
	printf("ata: done\n");

	/* Parse MBR partitions and register block device VFS devices */
	part_init();

	/* Mount ext2 at root via mount syscall */
	extern int sys_mount(const char *, const char *, const char *);
	sys_mount("/dev/hda1", "/", "ext2");
	sys_mount(0, "/dev", "tmpfs");

	scheduler_init();
	setup_main_task(shell_main, 0);

	pic_enable_irq(0);
	pic_enable_irq(1);

	while (1) {
		asm volatile("hlt");
	}
}
