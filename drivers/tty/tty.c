#include "vfs.h"
#include "scheduler.h"
#include "task.h"
#include <string.h>

#define TTY_STDIN  0
#define TTY_STDOUT 1

static struct file *tty_get_stdin(void)
{
	struct task *current = scheduler_get_current();
	if (!current) {
		static struct file *boot_in;
		if (!boot_in) {
			struct vfs_inode *ino = vfs_resolve_path("/dev/kbd");
			if (ino)
				boot_in = vfs_open_file(ino);
		}
		return boot_in;
	}
	return current->fd_table[TTY_STDIN];
}

static struct file *tty_get_stdout(void)
{
	struct task *current = scheduler_get_current();
	if (!current) {
		static struct file *boot_out;
		if (!boot_out) {
			struct vfs_inode *ino = vfs_resolve_path("/dev/vga");
			if (ino)
				boot_out = vfs_open_file(ino);
		}
		return boot_out;
	}
	return current->fd_table[TTY_STDOUT];
}

static int tty_read(struct vfs_inode *inode, uint32_t offset,
		    void *buf, uint32_t size)
{
	(void)inode;
	(void)offset;
	struct file *f = tty_get_stdin();
	if (!f || !f->ops || !f->ops->read)
		return -1;
	return f->ops->read(f, buf, size);
}

static int tty_write(struct vfs_inode *inode, uint32_t offset,
		     const void *buf, uint32_t size)
{
	(void)inode;
	(void)offset;
	struct file *f = tty_get_stdout();
	if (!f || !f->ops || !f->ops->write)
		return -1;
	return f->ops->write(f, buf, size);
}

static int tty_ioctl(struct vfs_inode *inode, int cmd, void *arg)
{
	(void)inode;
	struct file *f = tty_get_stdout();
	if (!f || !f->ops || !f->ops->ioctl)
		return -1;
	return f->ops->ioctl(f, cmd, arg);
}

static struct vfs_inode_ops tty_file_ops = {
	.read      = tty_read,
	.write     = tty_write,
	.ioctl     = tty_ioctl,
	.readdir   = 0,
	.lookup    = 0,
	.add_entry = 0,
};

void tty_driver_init(void)
{
	struct vfs_inode *inode = vfs_alloc_inode();
	if (!inode)
		return;

	inode->i_type = VFS_IFILE;
	inode->i_size = 0;
	inode->ops = &tty_file_ops;
	inode->private_data = 0;

	vfs_register_by_path("/dev/tty", inode);
}
