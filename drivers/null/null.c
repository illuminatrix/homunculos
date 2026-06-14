#include "vfs.h"
#include "devtmpfs.h"
#include <string.h>

static int null_read(struct vfs_inode *inode, uint32_t offset,
		     void *buf, uint32_t size)
{
	(void)inode;
	(void)offset;
	(void)buf;
	(void)size;
	return 0;
}

static int null_write(struct vfs_inode *inode, uint32_t offset,
		      const void *buf, uint32_t size)
{
	(void)inode;
	(void)offset;
	(void)buf;
	return (int)size;
}

static struct vfs_inode_ops null_file_ops = {
	.read      = null_read,
	.write     = null_write,
	.readdir   = 0,
	.lookup    = 0,
	.add_entry = 0,
};

static void null_init(void)
{
	devtmpfs_register_inode("null", &null_file_ops, 0, VFS_IFCHR, MKDEV(1, 1));
}
VFS_DRIVER_INIT(null_init);
