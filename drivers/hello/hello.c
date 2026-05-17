#include "vfs.h"
#include <string.h>

static int hello_read(struct vfs_inode *inode, uint32_t offset,
		      void *buf, uint32_t size)
{
	(void)inode;
	static const char content[] = "hello fs";
	int len = sizeof(content) - 1;

	if (offset >= (uint32_t)len)
		return 0;
	if (offset + size > (uint32_t)len)
		size = len - offset;

	memcpy(buf, content + offset, size);
	return (int)size;
}

static struct vfs_inode_ops hello_file_ops = {
	.read      = hello_read,
	.readdir   = 0,
	.lookup    = 0,
	.add_entry = 0,
};

void hello_driver_init(void)
{
	struct vfs_inode *inode = vfs_alloc_inode();
	if (!inode)
		return;

	inode->i_type = VFS_IFILE;
	inode->i_size = 8; /* "hello fs" */
	inode->ops = &hello_file_ops;
	inode->private_data = 0;

	vfs_register_by_path("/dev/hello", inode);
}
