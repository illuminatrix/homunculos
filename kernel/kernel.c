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
#include "block.h"
#include "ext2.h"
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

	/* Mount ext2 from first partition (hda1), fall back to raw disk */
	{
		struct block_device *ata_dev = block_find_device("hda1");
		const char *dev_name = "hda1";
		if (!ata_dev) {
			ata_dev = block_find_device("hda");
			dev_name = "hda";
		}
		if (ata_dev) {
			printf("ext2: mounting %s...\n", dev_name);
			struct vfs_inode *ext2_root = ext2_mount(ata_dev);
			if (ext2_root) {
				/* Mount ext2 at root / (shadows tmpfs root) */
				vfs_mount_create(vfs_root_inode(), ext2_root);
				printf("ext2: mounted at /\n");

				/* Mount tmpfs at /dev in ext2 */
				struct vfs_inode *dev_dir = vfs_resolve_path("/dev");
				if (dev_dir) {
					struct vfs_inode *dev_tmpfs = tmpfs_create_mount();
					if (dev_tmpfs)
						vfs_mount_create(dev_dir, dev_tmpfs);
				}
			} else {
				printf("ext2: mount failed (no ext2?)\n");
			}
		}
	}

	/* Re-create device nodes in new /dev (through tmpfs mount) */
	vfs_create_device_nodes();
	hello_driver_init();

	scheduler_init();
	setup_main_task(shell_main, 0);

	pic_enable_irq(0);
	pic_enable_irq(1);

	while (1) {
		asm volatile("hlt");
	}
}
