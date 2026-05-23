#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "task.h"
#include "scheduler.h"
#include "vfs.h"
#include "syscall.h"
#include "elf.h"
#include "mm.h"
#include "interrupt.h"
#include "pio.h"
#include "block.h"
#include "ext2.h"
#include "tmpfs.h"
#include "drivers/hello/hello.h"

uint32_t systemcall_table[255];

static void poweroff(void)
{
	disable_interrupts();

	/* QEMU */
	outw(0x604, 0x2000);

	/* Bochs */
	outw(0xB004, 0x2000);

	/* VirtualBox - full 32-bit write */
	outl(0x4004, 0x3400);

	/* Triple fault fallback */
	struct {
		uint16_t limit;
		uint32_t base;
	} __attribute__((packed)) null_idt = {0, 0};
	__asm__ volatile("lidt %0" : : "m"(null_idt));
	__asm__ volatile("int $3");

	while (1)
		__asm__ volatile("hlt");
}

int
sys_open(const char *path, int flags)
{
	struct vfs_inode *inode;
	struct file *f;
	int fd;
	struct task *current = scheduler_get_current();

	(void)flags;

	inode = vfs_resolve_path(path);
	if (!inode)
		return -1;

	f = vfs_open_file(inode);
	if (!f)
		return -1;

	if (!current) {
		/* early boot, no task — can't allocate an fd */
		return -1;
	}

	for (fd = 0; fd < VFS_MAX_FD; fd++) {
		if (current->fd_table[fd] == 0) {
			current->fd_table[fd] = f;
			return fd;
		}
	}

	vfs_close_file(f);
	return -1;
}

int
sys_close(int fd)
{
	struct task *current = scheduler_get_current();
	struct file *f;

	if (!current)
		return -1;
	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;
	if (fd < 3)
		return -1; /* don't close stdin/stdout/stderr */

	f = current->fd_table[fd];
	if (!f)
		return -1;

	current->fd_table[fd] = 0;
	vfs_close_file(f);
	return 0;
}

int
sys_write(int fd, const void *buf, size_t len)
{
	struct task *current = scheduler_get_current();
	struct file *f;

	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;

	if (!current) {
		static struct file *boot_out;
		if (!boot_out) {
			struct vfs_inode *ino = vfs_resolve_path("/dev/vga");
			if (ino)
				boot_out = vfs_open_file(ino);
		}
		f = boot_out;
	} else {
		f = current->fd_table[fd];
	}

	if (!f || !f->ops || !f->ops->write)
		return -1;

	return f->ops->write(f, buf, len);
}

int
sys_exit(int status)
{
	task_set_exit_status(status);
	task_exit();
	return 0;
}

int
sys_read(int fd, void *buf, size_t len)
{
	struct task *current = scheduler_get_current();
	struct file *f;

	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;

	if (!current) {
		static struct file *boot_in;
		if (!boot_in) {
			struct vfs_inode *ino = vfs_resolve_path("/dev/kbd");
			if (ino)
				boot_in = vfs_open_file(ino);
		}
		f = boot_in;
	} else {
		f = current->fd_table[fd];
	}

	if (!f || !f->ops || !f->ops->read)
		return -1;

	return f->ops->read(f, buf, len);
}

int
sys_yield(void)
{
	task_yield();
	return 0;
}

int
sys_reboot(void)
{
	poweroff();
	return 0;
}

int
sys_fork(void)
{
	uint32_t eip, cs, eflags, fp;

	__asm__ volatile(
		"movl 20(%%ebp), %0\n\t"
		"movl 24(%%ebp), %1\n\t"
		"movl 28(%%ebp), %2\n\t"
		"movl %%ebp, %3\n\t"
		: "=r"(eip), "=r"(cs), "=r"(eflags), "=r"(fp)
	);

	struct task *child = task_fork(eip, cs, eflags, fp);
	if (!child)
		return -1;

	return child->pid;
}

static void
copy_kernel_mappings(uint32_t *new_pdir)
{
	extern uint32_t kernel_pdir[1024];
	int i;
	for (i = 0; i < 4; i++)
		new_pdir[i] = kernel_pdir[i];
}

#define USER_STACK_TOP 0xC0000000
#define USER_STACK_SIZE 0x1000

int
sys_exec(const char *path)
{
	struct task *current = scheduler_get_current();
	struct vfs_inode *ino;
	uint32_t file_size, num_pages;
	uint8_t *elf_buf;
	const struct elf32_ehdr *ehdr;
	uint32_t entry;
	uint32_t *new_pdir;
	uint32_t va, user_esp, old_cr3;
	uint32_t stack_va;
	int cr3_saved = 0;
	int i;

	if (!current)
		return -1;

	ino = vfs_resolve_path(path);
	if (!ino)
		return -1;
	if (ino->i_type != VFS_IFILE)
		return -1;

	file_size = ino->i_size;
	num_pages = (file_size + 0xFFF) / 0x1000;

	elf_buf = 0;
	for (i = 0; i < (int)num_pages; i++) {
		uint8_t *page = (uint8_t *)mm_frame_alloc();
		if (!page)
			goto fail;
		if (i == 0)
			elf_buf = page;
	}

	if (ino->ops->read(ino, 0, elf_buf, file_size) < 0)
		goto fail;

	ehdr = (const struct elf32_ehdr *)elf_buf;
	if (elf_validate(ehdr) < 0)
		goto fail;

	new_pdir = (uint32_t *)mm_frame_alloc();
	if (!new_pdir)
		goto fail;
	memset(new_pdir, 0, 0x1000);

	copy_kernel_mappings(new_pdir);

	if (elf_load(ehdr, new_pdir, &entry) < 0) {
		mm_frame_free(new_pdir);
		goto fail;
	}

	stack_va = USER_STACK_TOP - USER_STACK_SIZE;
	for (va = stack_va; va < USER_STACK_TOP; va += 0x1000) {
		if (!mm_alloc_at(new_pdir, va,
				 MM_PRESENT | MM_RW | MM_USER)) {
			mm_frame_free(new_pdir);
			goto fail;
		}
	}

	asm volatile("mov %%cr3, %0" : "=r"(old_cr3));
	cr3_saved = 1;
	asm volatile("mov %0, %%cr3" :: "r"(new_pdir));

	elf_copy_segments(ehdr);

	for (i = 0; i < (int)num_pages; i++)
		mm_frame_free((void *)((uint32_t)elf_buf + i * 0x1000));

	user_esp = USER_STACK_TOP;
	user_esp -= 4;
	*(uint32_t *)user_esp = 0;

	uint32_t fp;
	asm volatile("movl %%ebp, %0" : "=r"(fp));
	*(uint32_t *)(fp + 20) = entry;
	*(uint32_t *)(fp + 32) = user_esp;

	current->pdir = new_pdir;
	return 0;

fail:
	if (cr3_saved)
		asm volatile("mov %0, %%cr3" :: "r"(old_cr3));
	if (elf_buf) {
		for (i = 0; i < (int)num_pages; i++)
			mm_frame_free((void *)((uint32_t)elf_buf
					       + i * 0x1000));
	}
	return -1;
}

int
sys_waitpid(int pid, int *status, int options)
{
	(void)pid;
	(void)options;
	return task_waitpid(pid, status);
}

static struct block_device *
mount_resolve_blockdev(const char *source)
{
	const char *name;

	if (!source)
		return 0;

	/* Extract basename from path (e.g. "/dev/hda1" -> "hda1") */
	name = source;
	{
		const char *p = source;
		while (*p) {
			if (*p == '/')
				name = p + 1;
			p++;
		}
	}

	return block_find_device(name);
}

int
sys_mount(const char *source, const char *target, const char *fstype)
{
	struct vfs_inode *target_inode;
	struct vfs_inode *fs_root;

	if (!target || !fstype)
		return -1;

	target_inode = vfs_resolve_path(target);
	if (!target_inode)
		return -1;

	if (strcmp(fstype, "ext2") == 0) {
		struct block_device *dev = mount_resolve_blockdev(source);
		if (!dev)
			return -1;
		fs_root = ext2_mount(dev);
		if (!fs_root)
			return -1;
		printf("ext2: mounted at %s\n", target);
	} else if (strcmp(fstype, "tmpfs") == 0) {
		fs_root = tmpfs_create_mount();
		if (!fs_root)
			return -1;
	} else {
		return -1;
	}

	if (!vfs_mount_create(target_inode, fs_root))
		return -1;

	/* Re-populate device nodes when tmpfs is mounted at /dev */
	if (strcmp(fstype, "tmpfs") == 0 && strcmp(target, "/dev") == 0) {
		vfs_create_device_nodes();
		hello_driver_init();
	}

	return 0;
}

int
sys_unmount(const char *target)
{
	if (!target)
		return -1;
	return vfs_unmount_path(target);
}

int
sys_getpid(void)
{
	struct task *current = scheduler_get_current();
	if (!current)
		return -1;
	return current->pid;
}

int
sys_getppid(void)
{
	struct task *current = scheduler_get_current();
	if (!current)
		return -1;
	return current->parent_pid;
}

void
syscall_init(void)
{
	systemcall_table[SYS_exit]        = (uint32_t)sys_exit;
	systemcall_table[SYS_fork]        = (uint32_t)sys_fork;
	systemcall_table[SYS_read]        = (uint32_t)sys_read;
	systemcall_table[SYS_write]       = (uint32_t)sys_write;
	systemcall_table[SYS_execve]      = (uint32_t)sys_exec;
	systemcall_table[SYS_waitpid]     = (uint32_t)sys_waitpid;
	systemcall_table[SYS_open]        = (uint32_t)sys_open;
	systemcall_table[SYS_close]       = (uint32_t)sys_close;
	systemcall_table[SYS_mount]       = (uint32_t)sys_mount;
	systemcall_table[SYS_umount]      = (uint32_t)sys_unmount;
	systemcall_table[SYS_sched_yield] = (uint32_t)sys_yield;
	systemcall_table[SYS_reboot]      = (uint32_t)sys_reboot;
	systemcall_table[SYS_getpid]      = (uint32_t)sys_getpid;
	systemcall_table[SYS_getppid]     = (uint32_t)sys_getppid;
}
