#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "kernel.h"
#include "interrupt.h"
#include "mm.h"
#include "pic.h"
#include "irq.h"
#include "scheduler.h"
#include "task.h"
#include "vfs.h"
#include "elf.h"
#include "tmpfs.h"
#include "part.h"
#include "drivers/hello/hello.h"
#include "drivers/ata/ata.h"
#include "gdt.h"

#define USER_STACK_TOP 0xC0000000
#define USER_STACK_SIZE 0x1000

void welcome()
{
	printf("Illuminatrix Kernel!\n");
}

static int
load_elf_from_vfs(const char *path, uint32_t **pdir_out, uint32_t *entry_out)
{
	struct vfs_inode *ino = vfs_resolve_path(path);
	uint32_t file_size;
	int num_pages;
	uint8_t *elf_buf;
	uint32_t *new_pdir;
	int i;
	uint32_t old_cr3;

	if (!ino) {
		printf("shell: vfs_resolve_path failed\n");
		return -1;
	}
	if (ino->i_type != VFS_IFILE) {
		printf("shell: /bin/shell is type %d, not file\n", ino->i_type);
		return -1;
	}

	file_size = ino->i_size;
	num_pages = (file_size + 0xFFF) / 0x1000;

	elf_buf = 0;
	for (i = 0; i < num_pages; i++) {
		uint8_t *page = (uint8_t *)mm_frame_alloc();
		if (!page)
			return -1;
		if (i == 0)
			elf_buf = page;
	}

	if (ino->ops->read(ino, 0, elf_buf, file_size) < 0)
		return -1;

	printf("shell: read %d bytes\n", (int)file_size);
	if (elf_validate((const struct elf32_ehdr *)elf_buf) < 0) {
		printf("shell: invalid ELF\n");
		return -1;
	}

	new_pdir = (uint32_t *)mm_frame_alloc();
	if (!new_pdir)
		return -1;
	memset(new_pdir, 0, 0x1000);

	extern uint32_t kernel_pdir[1024];
	for (i = 0; i < 4; i++)
		new_pdir[i] = kernel_pdir[i];

	printf("shell: loading ELF segments...\n");
	if (elf_load((const struct elf32_ehdr *)elf_buf, new_pdir, entry_out) < 0) {
		printf("shell: elf_load failed\n");
		return -1;
	}

	{
		uint32_t va;
		uint32_t stack_va = USER_STACK_TOP - USER_STACK_SIZE;
		for (va = stack_va; va < USER_STACK_TOP; va += 0x1000) {
			if (!mm_alloc_at(new_pdir, va,
					 MM_PRESENT | MM_RW | MM_USER))
				return -1;
		}
	}

	asm volatile("mov %%cr3, %0" : "=r"(old_cr3));
	asm volatile("mov %0, %%cr3" :: "r"(new_pdir));
	elf_copy_segments((const struct elf32_ehdr *)elf_buf);
	asm volatile("mov %0, %%cr3" :: "r"(old_cr3));

	*pdir_out = new_pdir;
	return 0;
}

static struct task *setup_main_task(uint32_t entry, uint32_t *pdir)
{
	struct task *t = task_alloc();
	if (!t)
		return 0;

	struct vfs_inode *kbd = vfs_resolve_path("/dev/kbd");
	struct vfs_inode *vga = vfs_resolve_path("/dev/vga");
	struct vfs_inode *err = vfs_resolve_path("/dev/vgaerr");
	t->fd_table[0] = vfs_open_file(kbd);
	t->fd_table[1] = vfs_open_file(vga);
	t->fd_table[2] = vfs_open_file(err);

	strncpy(t->name, "main", TASK_NAME_LEN - 1);
	t->name[TASK_NAME_LEN - 1] = '\0';

	t->pdir = pdir;
	task_update_context_user(t, entry, USER_STACK_TOP);
	scheduler_add_task(t);
	return t;
}

void kernel_main(multiboot_info_t *mem_info_ptr)
{
	extern void syscall_init(void);
	uint32_t entry;
	uint32_t *pdir;

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

	/* Load shell ELF from ext2 */
	if (load_elf_from_vfs("/bin/shell", &pdir, &entry) < 0) {
		printf("shell: failed to load /bin/shell\n");
		while (1)
			asm volatile("hlt");
	}

	scheduler_init();
	setup_main_task(entry, pdir);

	pic_enable_irq(0);
	pic_enable_irq(1);

	while (1) {
		asm volatile("hlt");
	}
}
