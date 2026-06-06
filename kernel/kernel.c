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
#include "devtmpfs.h"
#include "panic.h"
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
		  uint32_t **pdir_out, uint32_t *entry_out,
		  uint32_t *brk_out, uint32_t *user_esp_out)
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

	/* Set up minimal argc=0, argv=[NULL] on user stack */
	{
		uint32_t *usp = (uint32_t *)USER_STACK_TOP;
		*(--usp) = 0;	/* envp terminator (NULL) */
		*(--usp) = 0;	/* argv[0] = NULL */
		*(--usp) = 0;	/* argc = 0 */
		if (user_esp_out)
			*user_esp_out = (uint32_t)usp;
	}

	asm volatile("mov %0, %%cr3" :: "r"(old_cr3));

	if (brk_out)
		*brk_out = elf_brk_start((const struct elf32_ehdr *)elf_buf);

	*pdir_out = new_pdir;
	return 0;
}

static struct task *setup_main_task(uint32_t entry, uint32_t *pdir,
				    uint32_t brk_start, uint32_t user_esp)
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
	t->brk_start = brk_start;
	t->program_break = brk_start;
	task_update_context_user(t, entry, user_esp);
	scheduler_add_task(t);
	return t;
}

static void init_waiter(void *arg)
{
	(void)arg;
	task_waitpid(-1, 0, 0);
	panic("init process exited");
}

void kernel_main(multiboot_info_t *mem_info_ptr)
{
	extern void syscall_init(void);
	uint32_t entry;
	uint32_t *pdir;
	uint32_t brk = 0;

	syscall_init();
	pic_init();
	load_idt();
	vfs_init();
	vfs_inode_init();
	devtmpfs_init();

	/* Create device nodes in initial devtmpfs /dev for early boot output */
	devtmpfs_create_nodes();

	welcome();
	init_mm((mmap_entry_t *)mem_info_ptr->mmap_addr,
		mem_info_ptr->mmap_length);

	gdt_init();

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
	sys_mount(0, "/dev", "devtmpfs");

	/* Parse init= from bootloader command line, or try defaults */
	char init_buf[64];
	int has_init = 0;
	if ((mem_info_ptr->flags & (1 << 2)) && mem_info_ptr->cmdline)
		has_init = cmdline_find_param(
			(const char *)mem_info_ptr->cmdline, "init",
			init_buf, sizeof(init_buf));

	uint32_t user_esp;
	if (has_init) {
		printf("init: %s\n", init_buf);
		if (load_elf_from_vfs(init_buf, "init", &pdir, &entry, &brk,
				      &user_esp) < 0)
			panic("init not found or invalid");
	} else {
		const char *defaults[] = {"/init", "/sbin/init", "/bin/sh"};
		int found = 0;
		for (int i = 0; i < 3; i++) {
			if (load_elf_from_vfs(defaults[i], "init",
					      &pdir, &entry, &brk,
					      &user_esp) == 0) {
				found = 1;
				break;
			}
		}
		if (!found)
			panic("no init found");
	}

	scheduler_init();
	struct task *init_task = setup_main_task(entry, pdir, brk, user_esp);

	/* Create a kernel waiter that blocks until init exits */
	struct task *waiter = task_alloc();
	if (!waiter)
		panic("no task slot for waiter");
	strncpy(waiter->name, "waiter", 31);
	waiter->name[31] = '\0';
	waiter->parent_pid = -1;
	waiter->pdir = kernel_pdir;
	init_task->parent_pid = waiter->pid;
	task_init_context(waiter, init_waiter, 0);
	scheduler_add_task(waiter);

	pic_enable_irq(0);
	pic_enable_irq(1);

	asm volatile("sti");
	while (1)
		asm volatile("hlt");
}
