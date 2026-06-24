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
#include "devtmpfs.h"
#include "pipe.h"
#include <dirent.h>
#include "devtmpfs.h"
#include "termios.h"
#include "signal.h"

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

static void normalize_path(char *path)
{
	char out[256];
	int o = 0, i = 0;

	strncpy(out, path, sizeof(out) - 1);
	out[sizeof(out) - 1] = '\0';

	if (out[0] != '/')
		return;

	o = 1;
	i = 1;

	while (out[i]) {
		while (out[i] == '/')
			i++;
		if (!out[i])
			break;

		int start = i;
		while (out[i] && out[i] != '/')
			i++;
		int len = i - start;

		if (len == 1 && out[start] == '.')
			continue;

		if (len == 2 && out[start] == '.' && out[start + 1] == '.') {
			if (o > 1) {
				o--;
				while (o > 0 && out[o - 1] != '/')
					o--;
			}
			continue;
		}

		if (out[o - 1] != '/')
			out[o++] = '/';
		memcpy(out + o, out + start, len);
		o += len;
	}

	out[o] = '\0';
	strcpy(path, out);
}

static struct vfs_inode *resolve_path(struct task *t, const char *path)
{
	if (!path || !t)
		return 0;
	if (path[0] == '/')
		return vfs_resolve_path(path);

	char buf[512];
	int len = strlen(t->cwd);
	memcpy(buf, t->cwd, len);
	if (buf[len - 1] != '/')
		buf[len++] = '/';
	strcpy(buf + len, path);
	normalize_path(buf);

	return vfs_resolve_path(buf);
}

static void build_abs_path(struct task *t, const char *path,
			   char *out, int out_size)
{
	if (!path || !t || !out || out_size <= 0)
		return;
	if (path[0] == '/') {
		strncpy(out, path, out_size - 1);
		out[out_size - 1] = '\0';
		normalize_path(out);
		return;
	}
	int len = strlen(t->cwd);
	if (len + strlen(path) + 2 > (uint32_t)out_size)
		return;
	memcpy(out, t->cwd, len);
	if (out[len - 1] != '/')
		out[len++] = '/';
	strcpy(out + len, path);
	normalize_path(out);
}

int
sys_open(const char *path, int flags)
{
	struct vfs_inode *inode;
	struct file *f;
	int fd;
	struct task *current = scheduler_get_current();
	char abs_path[512];

	if (!current || !path)
		return -1;

	build_abs_path(current, path, abs_path, sizeof(abs_path));

	inode = vfs_resolve_path(abs_path);
	if (!inode) {
		if (!(flags & O_CREAT))
			return -1;
		inode = vfs_create_file(abs_path);
		if (!inode)
			return -1;
	} else {
		if (flags & O_TRUNC) {
			if (inode->ops && inode->ops->truncate)
				inode->ops->truncate(inode, 0);
		}
	}

	f = vfs_open_file_flags(inode, flags);
	if (!f)
		return -1;

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
	/* Allow closing any fd (needed for pipe redirection) */

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
	struct task *current = scheduler_get_current();
	int i;

	if (current) {
		for (i = 0; i < VFS_MAX_FD; i++) {
			if (current->fd_table[i]) {
				vfs_close_file(current->fd_table[i]);
				current->fd_table[i] = 0;
			}
		}
	}

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

static void halt(void)
{
	disable_interrupts();
	while (1)
		__asm__ volatile("hlt");
}

static void restart(void)
{
	disable_interrupts();

	/* Pulse CPU reset line via keyboard controller */
	while (in(0x64) & 2)
		;
	out(0x64, 0xFE);

	/* Try ACPI reset */
	out(0xCF9, 0x06);

	/* Triple fault fallback */
	{
		struct {
			uint16_t limit;
			uint32_t base;
		} __attribute__((packed)) null_idt = {0, 0};
		__asm__ volatile("lidt %0" : : "m"(null_idt));
		__asm__ volatile("int $3");
	}

	while (1)
		__asm__ volatile("hlt");
}

int
sys_reboot(int magic1, int magic2, int cmd)
{
	if (magic1 != LINUX_REBOOT_MAGIC1)
		return -1;
	if (magic2 != LINUX_REBOOT_MAGIC2 &&
	    magic2 != LINUX_REBOOT_MAGIC2A &&
	    magic2 != LINUX_REBOOT_MAGIC2B &&
	    magic2 != LINUX_REBOOT_MAGIC2C)
		return -1;

	switch (cmd) {
	case LINUX_REBOOT_CMD_POWER_OFF:
		poweroff();
		break;
	case LINUX_REBOOT_CMD_RESTART:
	case LINUX_REBOOT_CMD_RESTART2:
		restart();
		break;
	case LINUX_REBOOT_CMD_HALT:
		halt();
		break;
	default:
		return -1;
	}
	return 0;
}

int
sys_fork(void)
{
	uint32_t eip, cs, eflags, fp;

	__asm__ volatile(
		"movl 32(%%ebp), %0\n\t"
		"movl 36(%%ebp), %1\n\t"
		"movl 40(%%ebp), %2\n\t"
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
#define MAX_EXEC_ARGS 16
#define MAX_EXEC_ENVS 16
#define MAX_EXEC_STRINGS 4096

int
sys_exec(const char *path, char **argv, char **envp)
{
	struct task *current = scheduler_get_current();
	struct vfs_inode *ino;
	uint32_t file_size, num_pages;
	uint8_t *elf_buf, *str_buf;
	const struct elf32_ehdr *ehdr;
	uint32_t entry;
	uint32_t *new_pdir;
	uint32_t va, user_esp, old_cr3;
	uint32_t stack_va;
	int cr3_saved = 0;
	int argc = 0, envc = 0;
	int i;
	uint32_t str_offset;
	uint32_t argv_offsets[MAX_EXEC_ARGS];
	uint32_t envp_offsets[MAX_EXEC_ENVS];
	uint32_t total_size;
	uint8_t *strings;
	uint32_t *arr;
	uint32_t fp;

	if (!current)
		return -1;

	ino = resolve_path(current, path);
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

	/*
	 * Copy argv/envp strings from user space to a kernel page
	 * BEFORE switching to the new pdir (user addresses become
	 * inaccessible after the switch).
	 */
	str_buf = (uint8_t *)mm_frame_alloc();
	if (!str_buf)
		goto fail;

	str_offset = 0;

	if (argv) {
		for (argc = 0; argc < MAX_EXEC_ARGS; argc++) {
			char *s = argv[argc];
			if (!s)
				break;
			int len = 0;
			while (s[len] && len < MAX_EXEC_STRINGS
			       - str_offset - 1)
				len++;
			if (str_offset + len + 1 > MAX_EXEC_STRINGS)
				break;
			argv_offsets[argc] = str_offset;
			memcpy(str_buf + str_offset, s, len + 1);
			str_offset += len + 1;
		}
	}

	if (envp) {
		for (envc = 0; envc < MAX_EXEC_ENVS; envc++) {
			char *s = envp[envc];
			if (!s)
				break;
			int len = 0;
			while (s[len] && len < MAX_EXEC_STRINGS
			       - str_offset - 1)
				len++;
			if (str_offset + len + 1 > MAX_EXEC_STRINGS)
				break;
			envp_offsets[envc] = str_offset;
			memcpy(str_buf + str_offset, s, len + 1);
			str_offset += len + 1;
		}
	}

	/* Switch to new pdir and copy ELF segments */
	asm volatile("mov %%cr3, %0" : "=r"(old_cr3));
	cr3_saved = 1;
	asm volatile("mov %0, %%cr3" :: "r"(new_pdir));

	elf_copy_segments(ehdr);

	current->brk_start = elf_brk_start(ehdr);
	current->program_break = current->brk_start;

	/* Free ELF buffer pages */
	for (i = 0; i < (int)num_pages; i++)
		mm_frame_free((void *)((uint32_t)elf_buf + i * 0x1000));

	/*
	 * Build user stack (Linux i386 ABI).
	 * Layout (low to high address):
	 *   argc (4 bytes)
	 *   argv[0..argc-1] + NULL terminator ((argc+1)*4 bytes)
	 *   envp[0..envc-1] + NULL terminator ((envc+1)*4 bytes)
	 *   string data (str_offset bytes)
	 */
	total_size = 4 + (argc + 1) * 4 + (envc + 1) * 4 + str_offset;
	user_esp = USER_STACK_TOP - total_size;

	strings = (uint8_t *)(user_esp + 4 + (argc + 1) * 4
			      + (envc + 1) * 4);
	memcpy(strings, str_buf, str_offset);

	arr = (uint32_t *)(user_esp + 4);
	for (i = 0; i < argc; i++)
		arr[i] = (uint32_t)(strings + argv_offsets[i]);
	arr[argc] = 0;

	arr = (uint32_t *)(user_esp + 4 + (argc + 1) * 4);
	for (i = 0; i < envc; i++)
		arr[i] = (uint32_t)(strings + envp_offsets[i]);
	arr[envc] = 0;

	*(uint32_t *)user_esp = (uint32_t)argc;

	mm_frame_free(str_buf);

	/* Reset signal handlers on exec (POSIX: exec resets to SIG_DFL) */
	signal_init_task(current);

	asm volatile("movl %%ebp, %0" : "=r"(fp));
	*(uint32_t *)(fp + 32) = entry;
	*(uint32_t *)(fp + 44) = user_esp;

	current->pdir = new_pdir;
	task_set_pdir(current, new_pdir);
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
	return task_waitpid(pid, status, options);
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
	} else if (strcmp(fstype, "devtmpfs") == 0) {
		fs_root = devtmpfs_create_mount();
		if (!fs_root)
			return -1;
	} else {
		return -1;
	}

	if (!vfs_mount_create(target_inode, fs_root))
		return -1;

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

int
sys_getuid32(void)
{
	return 0;
}

int
sys_getgid32(void)
{
	return 0;
}

int
sys_geteuid32(void)
{
	return 0;
}

int
sys_getegid32(void)
{
	return 0;
}

int
sys_setuid32(uint32_t uid)
{
	(void)uid;
	return 0;
}

int
sys_setgid32(uint32_t gid)
{
	(void)gid;
	return 0;
}

#define UTSNAME_LEN 65

struct sys_utsname {
	char sysname[UTSNAME_LEN];
	char nodename[UTSNAME_LEN];
	char release[UTSNAME_LEN];
	char version[UTSNAME_LEN];
	char machine[UTSNAME_LEN];
	char domainname[UTSNAME_LEN];
};

int
sys_uname(struct sys_utsname *buf)
{
	if (!buf)
		return -1;

	memset(buf->sysname, 0, UTSNAME_LEN);
	memset(buf->nodename, 0, UTSNAME_LEN);
	memset(buf->release, 0, UTSNAME_LEN);
	memset(buf->version, 0, UTSNAME_LEN);
	memset(buf->machine, 0, UTSNAME_LEN);
	memset(buf->domainname, 0, UTSNAME_LEN);

	strncpy(buf->sysname, "HomunculOS", UTSNAME_LEN - 1);
	strncpy(buf->nodename, "homunculos", UTSNAME_LEN - 1);
	strncpy(buf->release, "0.1.0", UTSNAME_LEN - 1);
	strncpy(buf->version, "#1 Sat May 23 2026", UTSNAME_LEN - 1);
	strncpy(buf->machine, "i686", UTSNAME_LEN - 1);
	strncpy(buf->domainname, "(none)", UTSNAME_LEN - 1);

	return 0;
}

int
sys_brk(void *addr)
{
	struct task *current = scheduler_get_current();
	uint32_t new_brk;

	if (!current)
		return -1;

	if (addr == 0)
		return current->program_break;

	new_brk = (uint32_t)addr;
	if (new_brk < current->brk_start)
		new_brk = current->brk_start;

	/* Expand heap if crossing a page boundary */
	uint32_t old_page = (current->program_break + 0xFFF) & ~0xFFF;
	uint32_t new_page = (new_brk + 0xFFF) & ~0xFFF;

	if (new_page > old_page) {
		for (uint32_t va = old_page; va < new_page; va += 0x1000) {
			if (!mm_alloc_at(current->pdir, va,
					 MM_PRESENT | MM_RW | MM_USER))
				return current->program_break;
		}
	}

	current->program_break = new_brk;
	return new_brk;
}

int
sys_lseek(int fd, int offset, int whence)
{
	struct task *current = scheduler_get_current();
	struct file *f;
	uint32_t new_pos;

	if (!current)
		return -1;
	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;

	f = current->fd_table[fd];
	if (!f)
		return -1;

	switch (whence) {
	case 0: /* SEEK_SET */
		new_pos = (uint32_t)offset;
		break;
	case 1: /* SEEK_CUR */
		new_pos = f->pos + offset;
		break;
	case 2: /* SEEK_END */
		return -1;
	default:
		return -1;
	}

	f->pos = new_pos;
	return (int)new_pos;
}

int
sys_stat(const char *path, struct vfs_stat *buf)
{
	struct vfs_inode *inode;

	if (!path || !buf)
		return -1;

	struct task *current = scheduler_get_current();

	inode = resolve_path(current, path);
	if (!inode)
		return -1;

	vfs_inode_stat(inode, buf);
	return 0;
}

int
sys_lstat(const char *path, struct vfs_stat *buf)
{
	/* No symlinks yet — same as stat */
	return sys_stat(path, buf);
}

int
sys_fstat(int fd, struct vfs_stat *buf)
{
	struct task *current = scheduler_get_current();
	struct file *f;
	struct vfs_inode *inode;

	if (!current)
		return -1;
	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;
	if (!buf)
		return -1;

	f = current->fd_table[fd];
	if (!f)
		return -1;

	inode = (struct vfs_inode *)f->private_data;
	if (!inode)
		return -1;

	vfs_inode_stat(inode, buf);
	return 0;
}

int
sys_chdir(const char *path)
{
	struct task *current = scheduler_get_current();
	struct vfs_inode *inode;
	char buf[512];

	if (!current || !path)
		return -1;

	if (path[0] == '/') {
		inode = vfs_resolve_path(path);
		if (!inode || inode->i_type != VFS_IDIR)
			return -1;
		strncpy(buf, path, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		normalize_path(buf);
		strncpy(current->cwd, buf, sizeof(current->cwd) - 1);
		current->cwd[sizeof(current->cwd) - 1] = '\0';
		return 0;
	}

	strcpy(buf, current->cwd);
	int len = strlen(buf);
	if (buf[len - 1] != '/')
		buf[len++] = '/';
	strcpy(buf + len, path);
	normalize_path(buf);

	inode = vfs_resolve_path(buf);
	if (!inode || inode->i_type != VFS_IDIR)
		return -1;

	strncpy(current->cwd, buf, sizeof(current->cwd) - 1);
	current->cwd[sizeof(current->cwd) - 1] = '\0';
	return 0;
}

int
sys_getcwd(char *buf, size_t size)
{
	struct task *current = scheduler_get_current();

	if (!current || !buf || !size)
		return -1;

	int len = strlen(current->cwd);
	if ((size_t)(len + 1) > size)
		return -1;

	memcpy(buf, current->cwd, len + 1);
	return len;
}

int
sys_getdents(int fd, struct dirent *dirp, int count)
{
	struct task *current = scheduler_get_current();
	struct file *f;
	struct vfs_inode *inode;

	if (!current)
		return -1;
	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;
	if (!dirp || count <= 0)
		return -1;

	f = current->fd_table[fd];
	if (!f)
		return -1;

	inode = (struct vfs_inode *)f->private_data;
	if (!inode || !inode->ops || !inode->ops->readdir)
		return -1;

	int written = 0;
	int idx = f->pos;
	struct vfs_dirent dent;

	while (1) {
		if (inode->ops->readdir(inode, idx, &dent) != 0)
			break;

		int namelen = strlen(dent.d_name);
		int reclen = sizeof(struct dirent) + namelen + 2;

		if (written + reclen > count)
			break;

		struct dirent *d = (struct dirent *)
					((char *)dirp + written);
		d->d_ino = dent.d_ino;
		d->d_off = 0;
		d->d_reclen = reclen;
		memcpy(d->d_name, dent.d_name, namelen + 1);
		/* d_type at d_reclen - 1 */
		*((char *)d + reclen - 1) = dent.d_type;

		written += reclen;
		idx++;
	}

	f->pos = idx;

	return written;
}

int
sys_ioctl(int fd, int cmd, void *arg)
{
	struct task *current = scheduler_get_current();
	struct file *f;

	if (!current)
		return -1;
	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;

	f = current->fd_table[fd];
	if (!f || !f->ops)
		return -1;

	if (!f->ops->ioctl)
		return -1;

	return f->ops->ioctl(f, cmd, arg);
}

int
sys_access(const char *path)
{
	struct task *current = scheduler_get_current();
	char abs_path[512];

	if (!current || !path)
		return -1;
	build_abs_path(current, path, abs_path, sizeof(abs_path));
	return vfs_access(abs_path);
}

int
sys_mkdir(const char *path)
{
	struct task *current = scheduler_get_current();
	char abs_path[512];

	if (!current || !path)
		return -1;
	build_abs_path(current, path, abs_path, sizeof(abs_path));
	return vfs_mkdir(abs_path);
}

int
sys_rmdir(const char *path)
{
	struct task *current = scheduler_get_current();
	char abs_path[512];

	if (!current || !path)
		return -1;
	build_abs_path(current, path, abs_path, sizeof(abs_path));
	return vfs_rmdir(abs_path);
}

int
sys_unlink(const char *path)
{
	struct task *current = scheduler_get_current();
	char abs_path[512];

	if (!current || !path)
		return -1;
	build_abs_path(current, path, abs_path, sizeof(abs_path));
	return vfs_unlink(abs_path);
}

int
sys_symlink(const char *target, const char *path)
{
	struct task *current = scheduler_get_current();
	char abs_path[512];

	if (!current || !target || !path)
		return -1;
	build_abs_path(current, path, abs_path, sizeof(abs_path));
	return vfs_symlink(target, abs_path);
}

int
sys_readlink(const char *path, char *buf, int size)
{
	struct task *current = scheduler_get_current();
	char abs_path[512];

	if (!current || !path || !buf)
		return -1;
	build_abs_path(current, path, abs_path, sizeof(abs_path));
	return vfs_readlink(abs_path, buf, (uint32_t)size);
}

/* --- sync --- */
int sys_sync(void)
{
	/* No write cache — nothing to flush */
	return 0;
}

/* --- fsync --- */
int sys_fsync(int fd)
{
	(void)fd;
	/* No write cache — nothing to flush */
	return 0;
}

/* --- dup --- */
int sys_dup(int oldfd)
{
	struct task *current = scheduler_get_current();
	int newfd;

	if (!current)
		return -1;
	if (oldfd < 0 || oldfd >= VFS_MAX_FD)
		return -1;
	if (!current->fd_table[oldfd])
		return -1;

	for (newfd = 0; newfd < VFS_MAX_FD; newfd++) {
		if (!current->fd_table[newfd])
			return sys_dup2(oldfd, newfd);
	}

	return -1;
}

/* --- times --- */
struct k_tms {
	int32_t tms_utime;
	int32_t tms_stime;
	int32_t tms_cutime;
	int32_t tms_cstime;
};

int sys_times(struct k_tms *buf)
{
	uint32_t ticks = scheduler_get_tick_count();

	if (buf) {
		buf->tms_utime = (int32_t)ticks;
		buf->tms_stime = 0;
		buf->tms_cutime = 0;
		buf->tms_cstime = 0;
	}

	return (int)ticks;
}

/* --- chmod --- */
int sys_chmod(const char *path, int mode)
{
	struct task *current = scheduler_get_current();
	char abs_path[512];

	if (!current || !path)
		return -1;

	build_abs_path(current, path, abs_path, sizeof(abs_path));
	return vfs_chmod(abs_path, (uint16_t)mode);
}

/* --- fchmod --- */
int sys_fchmod(int fd, int mode)
{
	struct task *current = scheduler_get_current();
	struct file *f;
	struct vfs_inode *inode;

	if (!current)
		return -1;
	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;

	f = current->fd_table[fd];
	if (!f)
		return -1;

	inode = (struct vfs_inode *)f->private_data;
	if (!inode)
		return -1;

	return vfs_inode_chmod(inode, (uint16_t)mode);
}

/* --- fcntl64 --- */
#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define FD_CLOEXEC 1

int sys_fcntl64(int fd, int cmd, int arg)
{
	struct task *current = scheduler_get_current();
	struct file *f;

	if (!current)
		return -1;
	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;

	f = current->fd_table[fd];
	if (!f)
		return -1;

	switch (cmd) {
	case F_DUPFD:
	{
		int new_fd;
		for (new_fd = arg; new_fd < VFS_MAX_FD; new_fd++) {
			if (!current->fd_table[new_fd]) {
				current->fd_table[new_fd] = f;
				f->refcount++;
				current->fd_flags[new_fd] = 0;
				return new_fd;
			}
		}
		return -1;
	}
	case F_GETFD:
		return current->fd_flags[fd];
	case F_SETFD:
		current->fd_flags[fd] = arg & FD_CLOEXEC;
		return 0;
	case F_GETFL:
		return f->flags;
	case F_SETFL:
		/* Allow setting O_APPEND (and eventually O_NONBLOCK) */
		f->flags = (f->flags & ~O_APPEND) | (arg & O_APPEND);
		return 0;
	default:
		return -1;
	}
}

/* --- rename --- */
int sys_rename(const char *old_path, const char *new_path)
{
	struct task *current = scheduler_get_current();
	char abs_old[512], abs_new[512];

	if (!current || !old_path || !new_path)
		return -1;

	build_abs_path(current, old_path, abs_old, sizeof(abs_old));
	build_abs_path(current, new_path, abs_new, sizeof(abs_new));

	return vfs_rename(abs_old, abs_new);
}

/* --- mknod --- */
int sys_mknod(const char *path, int mode, dev_t dev)
{
	struct task *current = scheduler_get_current();
	char abs_path[512];

	if (!current || !path)
		return -1;

	build_abs_path(current, path, abs_path, sizeof(abs_path));
	return vfs_mknod(abs_path, (uint16_t)mode, dev);
}

/* --- link --- */
int sys_link(const char *old_path, const char *new_path)
{
	struct task *current = scheduler_get_current();
	char abs_old[512], abs_new[512];

	if (!current || !old_path || !new_path)
		return -1;

	build_abs_path(current, old_path, abs_old, sizeof(abs_old));
	build_abs_path(current, new_path, abs_new, sizeof(abs_new));

	return vfs_link(abs_old, abs_new);
}

/* --- truncate --- */
int sys_truncate64(const char *path, unsigned long length)
{
	struct task *current = scheduler_get_current();
	char abs_path[512];
	struct vfs_inode *inode;

	if (!current || !path)
		return -1;

	build_abs_path(current, path, abs_path, sizeof(abs_path));
	inode = vfs_resolve_path(abs_path);
	if (!inode)
		return -1;

	if (!inode->ops || !inode->ops->truncate)
		return -1;

	return inode->ops->truncate(inode, (uint32_t)length);
}

int sys_ftruncate64(int fd, unsigned long length)
{
	struct task *current = scheduler_get_current();
	struct file *f;
	struct vfs_inode *inode;

	if (!current)
		return -1;
	if (fd < 0 || fd >= VFS_MAX_FD)
		return -1;

	f = current->fd_table[fd];
	if (!f)
		return -1;

	inode = (struct vfs_inode *)f->private_data;
	if (!inode)
		return -1;

	if (!inode->ops || !inode->ops->truncate)
		return -1;

	return inode->ops->truncate(inode, (uint32_t)length);
}

/* --- Memory mapping (mmap2/munmap/mprotect) --- */

#define MMAP_BASE      0x40000000
#define PROT_READ      1
#define PROT_WRITE     2
#define MAP_ANONYMOUS  32
#define MAP_FIXED      16
#define MAP_PRIVATE     2

static uint32_t vma_find_gap(struct task *t, uint32_t length)
{
	uint32_t candidate = MMAP_BASE;

	while (1) {
		int overlap = 0;
		uint32_t end = candidate + length;
		if (end < candidate)
			return 0;
		for (int i = 0; i < t->vma_count; i++) {
			if (candidate < t->vmas[i].end
			    && end > t->vmas[i].start) {
				candidate = t->vmas[i].end;
				overlap = 1;
				break;
			}
		}
		if (!overlap)
			return candidate;
		if (candidate + length < candidate)
			return 0;
	}
}

static int vma_add(struct task *t, uint32_t start, uint32_t end,
		   uint32_t prot, uint32_t flags)
{
	if (t->vma_count >= MAX_VMA)
		return -1;
	t->vmas[t->vma_count].start = start;
	t->vmas[t->vma_count].end = end;
	t->vmas[t->vma_count].prot = prot;
	t->vmas[t->vma_count].flags = flags;
	t->vma_count++;
	return 0;
}

static void vma_remove(struct task *t, uint32_t start, uint32_t end)
{
	for (int i = t->vma_count - 1; i >= 0; i--) {
		struct vm_area *v = &t->vmas[i];
		if (start >= v->end || end <= v->start)
			continue;
		v->start = 0;
		v->end = 0;
		v->prot = 0;
		v->flags = 0;
		t->vmas[i] = t->vmas[t->vma_count - 1];
		t->vma_count--;
	}
}

static void vma_update_prot(struct task *t, uint32_t start,
			    uint32_t end, uint32_t prot)
{
	for (int i = 0; i < t->vma_count; i++) {
		if (start < t->vmas[i].end
		    && end > t->vmas[i].start)
			t->vmas[i].prot = prot;
	}
}

static uint32_t prot_to_page_flags(int prot)
{
	if (prot == 0)
		return 0;
	uint32_t f = MM_PRESENT | MM_USER;
	if (prot & PROT_WRITE)
		f |= MM_RW;
	return f;
}

int sys_mmap2(uint32_t addr, uint32_t length, int prot,
	      int flags, int fd, uint32_t offset)
{
	struct task *current = scheduler_get_current();
	uint32_t va;
	uint32_t page_flags;
	int i, pages;

	(void)offset;

	if (!current)
		return -1;

	length = (length + 0xFFF) & ~0xFFF;
	if (length == 0)
		return -1;

	pages = length / FRAME;

	page_flags = prot_to_page_flags(prot);

	/* Only anonymous mappings supported for now */
	if (!(flags & MAP_ANONYMOUS))
		return -1;

	if (flags & MAP_FIXED) {
		va = addr & ~0xFFF;
		for (i = 0; i < current->vma_count; i++) {
			if (va < current->vmas[i].end
			    && va + length > current->vmas[i].start)
				return -1;
		}
	} else {
		va = vma_find_gap(current, length);
		if (va == 0)
			return -1;
	}

	for (i = 0; i < pages; i++) {
		if (!mm_alloc_at(current->pdir, va + i * FRAME,
				 page_flags))
			goto fail;
	}

	if (vma_add(current, va, va + length, prot, flags) < 0)
		goto fail;

	return (int)va;

fail:
	for (int j = 0; j < i; j++)
		mm_unmap_at(current->pdir, va + j * FRAME);
	return -1;
}

int sys_munmap(uint32_t addr, uint32_t length)
{
	struct task *current = scheduler_get_current();
	uint32_t va = addr & ~0xFFF;
	uint32_t end = ((addr + length + 0xFFF) & ~0xFFF);
	uint32_t page;

	if (!current)
		return -1;

	for (page = va; page < end; page += FRAME) {
		if (mm_get_pte(current->pdir, page) & MM_PRESENT)
			mm_unmap_at(current->pdir, page);
	}

	vma_remove(current, va, end);
	return 0;
}

int sys_mprotect(uint32_t addr, uint32_t length, int prot)
{
	struct task *current = scheduler_get_current();
	uint32_t va = addr & ~0xFFF;
	uint32_t end = ((addr + length + 0xFFF) & ~0xFFF);
	uint32_t page_flags = prot_to_page_flags(prot);
	uint32_t page;

	if (!current)
		return -1;

	for (page = va; page < end; page += FRAME) {
		uint32_t pte = mm_get_pte(current->pdir, page);
		if (pte & MM_PRESENT) {
			uint32_t pa = pte & ~0xFFF;
			uint32_t pd_idx = page >> 22;
			uint32_t pt_idx = (page >> 12) & 0x3FF;
			uint32_t *pt;
			pt = (uint32_t *)(current->pdir[pd_idx] & ~0xFFF);
			pt[pt_idx] = pa | page_flags;
			mm_invlpg(current->pdir, page);
		}
	}

	vma_update_prot(current, va, end, prot);
	return 0;
}

/* --- Time syscalls --- */

/* Kernel-side timeval/timespec (matches libc <sys/time.h>) */
struct k_timeval {
	int32_t tv_sec;
	int32_t tv_usec;
};

struct k_timespec {
	int32_t tv_sec;
	int32_t tv_nsec;
};

#define TICK_HZ 100

int
sys_gettimeofday(struct k_timeval *tv, void *tz)
{
	if (!tv)
		return -1;
	uint32_t ticks = scheduler_get_tick_count();
	tv->tv_sec = ticks / TICK_HZ;
	tv->tv_usec = (ticks % TICK_HZ) * (1000000 / TICK_HZ);
	return 0;
}

int
sys_time(int32_t *tloc)
{
	int32_t sec = scheduler_get_tick_count() / TICK_HZ;
	if (tloc)
		*tloc = sec;
	return sec;
}

int
sys_nanosleep(struct k_timespec *req, struct k_timespec *rem)
{
	struct task *current;

	if (!req)
		return -1;
	if (req->tv_sec < 0 || req->tv_nsec < 0)
		return -1;

	uint32_t total_ticks = req->tv_sec * TICK_HZ
		+ req->tv_nsec / (1000000000 / TICK_HZ);

	if (total_ticks == 0) {
		/* Minimum sleep = 1 tick (10ms) */
		total_ticks = 1;
	}

	current = scheduler_get_current();
	if (!current)
		return -1;

	current->wakeup_tick = scheduler_get_tick_count() + total_ticks;
	current->state = TASK_STATE_BLOCKED;

	while (scheduler_get_tick_count() < current->wakeup_tick) {
		/* Check for pending signals that interrupt sleep */
		uint32_t pending = current->pending_signals
			& ~current->signal_mask;
		if (pending) {
			if (rem) {
				uint32_t ticks_left
					= current->wakeup_tick
					- scheduler_get_tick_count();
				rem->tv_sec = ticks_left / TICK_HZ;
				rem->tv_nsec = (ticks_left % TICK_HZ)
					* (1000000000 / TICK_HZ);
			}
			current->state = TASK_STATE_RUNNING;
			return -1;
		}
		schedule();
		/* If schedule() returned (no other READY task), halt and wait */
		if (scheduler_get_tick_count() < current->wakeup_tick)
			__asm__ volatile("sti; hlt");
	}

	current->state = TASK_STATE_RUNNING;

	if (rem) {
		uint32_t ticks_left = current->wakeup_tick
			> scheduler_get_tick_count()
			? current->wakeup_tick - scheduler_get_tick_count()
			: 0;
		rem->tv_sec = ticks_left / TICK_HZ;
		rem->tv_nsec = (ticks_left % TICK_HZ)
			* (1000000000 / TICK_HZ);
	}
	return 0;
}

void
syscall_init(void)
{
	systemcall_table[SYS_exit]        = (uint32_t)sys_exit;
	systemcall_table[SYS_exit_group]  = (uint32_t)sys_exit;
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
	systemcall_table[SYS_uname]      = (uint32_t)sys_uname;
	systemcall_table[SYS_lseek]      = (uint32_t)sys_lseek;
	systemcall_table[SYS_brk]       = (uint32_t)sys_brk;
	systemcall_table[SYS_stat]      = (uint32_t)sys_stat;
	systemcall_table[SYS_lstat]     = (uint32_t)sys_lstat;
	systemcall_table[SYS_fstat]     = (uint32_t)sys_fstat;
	systemcall_table[SYS_getdents] = (uint32_t)sys_getdents;
	systemcall_table[SYS_chdir]   = (uint32_t)sys_chdir;
	systemcall_table[SYS_getcwd]  = (uint32_t)sys_getcwd;
	systemcall_table[SYS_ioctl]   = (uint32_t)sys_ioctl;
	systemcall_table[SYS_pipe]    = (uint32_t)sys_pipe;
	systemcall_table[SYS_dup2]    = (uint32_t)sys_dup2;
	systemcall_table[SYS_access] = (uint32_t)sys_access;
	systemcall_table[SYS_mkdir]  = (uint32_t)sys_mkdir;
	systemcall_table[SYS_rmdir]  = (uint32_t)sys_rmdir;
	systemcall_table[SYS_unlink] = (uint32_t)sys_unlink;
	systemcall_table[SYS_symlink]= (uint32_t)sys_symlink;
	systemcall_table[SYS_readlink]=(uint32_t)sys_readlink;
	systemcall_table[SYS_time]    = (uint32_t)sys_time;
	systemcall_table[SYS_gettimeofday]=(uint32_t)sys_gettimeofday;
	systemcall_table[SYS_nanosleep] =(uint32_t)sys_nanosleep;
	systemcall_table[SYS_sync]     = (uint32_t)sys_sync;
	systemcall_table[SYS_fsync]    = (uint32_t)sys_fsync;
	systemcall_table[SYS_dup]      = (uint32_t)sys_dup;
	systemcall_table[SYS_chmod]    = (uint32_t)sys_chmod;
	systemcall_table[SYS_fchmod]   = (uint32_t)sys_fchmod;
	systemcall_table[SYS_link]     = (uint32_t)sys_link;
	systemcall_table[SYS_fcntl64] = (uint32_t)sys_fcntl64;
	systemcall_table[SYS_truncate64]  = (uint32_t)sys_truncate64;
	systemcall_table[SYS_ftruncate64] = (uint32_t)sys_ftruncate64;
	systemcall_table[SYS_rename]   = (uint32_t)sys_rename;
	systemcall_table[SYS_mknod]    = (uint32_t)sys_mknod;
	systemcall_table[SYS_mmap2]   = (uint32_t)sys_mmap2;
	systemcall_table[SYS_munmap]  = (uint32_t)sys_munmap;
	systemcall_table[SYS_mprotect]= (uint32_t)sys_mprotect;
	systemcall_table[SYS_times]    = (uint32_t)sys_times;
	systemcall_table[SYS_kill]      = (uint32_t)sys_kill;
	systemcall_table[SYS_signal]    = (uint32_t)sys_signal;
	systemcall_table[SYS_rt_sigaction]  = (uint32_t)sys_rt_sigaction;
	systemcall_table[SYS_rt_sigreturn]  = (uint32_t)sys_rt_sigreturn;
	systemcall_table[SYS_rt_sigprocmask] = (uint32_t)sys_rt_sigprocmask;
	systemcall_table[SYS_getuid32]  = (uint32_t)sys_getuid32;
	systemcall_table[SYS_getgid32]  = (uint32_t)sys_getgid32;
	systemcall_table[SYS_geteuid32] = (uint32_t)sys_geteuid32;
	systemcall_table[SYS_getegid32] = (uint32_t)sys_getegid32;
	systemcall_table[SYS_setuid32]  = (uint32_t)sys_setuid32;
	systemcall_table[SYS_setgid32]  = (uint32_t)sys_setgid32;
}
