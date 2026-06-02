#include "vfs.h"
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

void null_driver_init(void)
{
	struct vfs_inode *inode = vfs_alloc_inode();
	if (!inode)
		return;

	inode->i_type = VFS_IFILE;
	inode->i_size = 0;
	inode->ops = &null_file_ops;
	inode->private_data = 0;

	vfs_register_by_path("/dev/null", inode);
}
