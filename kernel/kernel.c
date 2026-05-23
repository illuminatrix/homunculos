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
#include "panic.h"
#include "drivers/hello/hello.h"
#include "drivers/ata/ata.h"
#include "gdt.h"

#define USER_STACK_TOP 0xC0000000
#define USER_STACK_SIZE 0x1000

void welcome()
{
	printf("Illuminatrix Kernel!\n");
}

/* Parse "key=value" from a kernel command line string.
   Copies the value into buf (up to buf_size bytes) and returns 1 on success.
   Returns 0 if key is not found. */
static int
cmdline_find_param(const char *cmdline, const char *key,
		   char *buf, int buf_size)
{
	while (cmdline && *cmdline) {
		while (*cmdline == ' ')
			cmdline++;
		if (!*cmdline)
			break;

		const char *k = key;
		const char *c = cmdline;
		while (*k && *c && *k == *c) {
			k++;
			c++;
		}
		if (*k == '\0' && *c == '=') {
			c++;
			/* Copy value until next space or end */
			int i = 0;
			while (*c && *c != ' ' && i < buf_size - 1) {
				buf[i++] = *c;
				c++;
			}
			buf[i] = '\0';
			return 1;
		}

		while (*cmdline && *cmdline != ' ')
			cmdline++;
	}
	return 0;
}

static int
load_elf_from_vfs(const char *path, const char *name,
		  uint32_t **pdir_out, uint32_t *entry_out)
{
	struct vfs_inode *ino = vfs_resolve_path(path);
	uint32_t file_size;
	int num_pages;
	uint8_t *elf_buf;
	uint32_t *new_pdir;
	int i;
	uint32_t old_cr3;

	if (!ino) {
		printf("%s: vfs_resolve_path failed\n", name);
		return -1;
	}
	if (ino->i_type != VFS_IFILE) {
		printf("%s: %s is type %d, not file\n", name, path, ino->i_type);
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

	printf("%s: read %d bytes\n", name, (int)file_size);
	if (elf_validate((const struct elf32_ehdr *)elf_buf) < 0) {
		printf("%s: invalid ELF\n", name);
		return -1;
	}

	new_pdir = (uint32_t *)mm_frame_alloc();
	if (!new_pdir)
		return -1;
	memset(new_pdir, 0, 0x1000);

	extern uint32_t kernel_pdir[1024];
	for (i = 0; i < 4; i++)
		new_pdir[i] = kernel_pdir[i];

	printf("%s: loading ELF segments...\n", name);
	if (elf_load((const struct elf32_ehdr *)elf_buf, new_pdir, entry_out) < 0) {
		printf("%s: elf_load failed\n", name);
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

	/* Parse root= from bootloader command line */
	char root_buf[64];
	int has_root = 0;
	if ((mem_info_ptr->flags & (1 << 2)) && mem_info_ptr->cmdline)
		has_root = cmdline_find_param(
			(const char *)mem_info_ptr->cmdline, "root",
			root_buf, sizeof(root_buf));
	if (!has_root)
		panic("no root= parameter");
	printf("mount: root=%s\n", root_buf);

	/* Mount root at / via mount syscall */
	extern int sys_mount(const char *, const char *, const char *);
	sys_mount(root_buf, "/", "ext2");
	sys_mount(0, "/dev", "tmpfs");

	/* Parse init= from bootloader command line, or try defaults */
	char init_buf[64];
	int has_init = 0;
	if ((mem_info_ptr->flags & (1 << 2)) && mem_info_ptr->cmdline)
		has_init = cmdline_find_param(
			(const char *)mem_info_ptr->cmdline, "init",
			init_buf, sizeof(init_buf));

	if (has_init) {
		printf("init: %s\n", init_buf);
		if (load_elf_from_vfs(init_buf, "init", &pdir, &entry) < 0)
			panic("init not found or invalid");
	} else {
		const char *defaults[] = {"/init", "/sbin/init", "/bin/sh"};
		int found = 0;
		for (int i = 0; i < 3; i++) {
			if (load_elf_from_vfs(defaults[i], "init",
					      &pdir, &entry) == 0) {
				found = 1;
				break;
			}
		}
		if (!found)
			panic("no init found");
	}

	scheduler_init();
	struct task *init_task = setup_main_task(entry, pdir);

	pic_enable_irq(0);
	pic_enable_irq(1);

	while (1) {
		if (init_task->state == TASK_STATE_EXITED)
			panic("init process exited");
		asm volatile("hlt");
	}
}
