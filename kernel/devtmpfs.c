#include "vfs.h"
#include <string.h>
#include <stdio.h>

#define DEVTMPFS_MAX_ENTRIES 16
#define DEVTMPFS_MAX_DIRS 16
#define DEVTMPFS_MAX_SYMLINKS 16

struct devtmpfs_entry {
	char name[64];
	struct vfs_inode *inode;
};

struct devtmpfs_dir {
	struct devtmpfs_entry entries[DEVTMPFS_MAX_ENTRIES];
	int count;
};

/* Static pools */
static struct devtmpfs_dir devtmpfs_dirs[DEVTMPFS_MAX_DIRS];
static int devtmpfs_dir_next;
static char devtmpfs_symlink_targets[DEVTMPFS_MAX_SYMLINKS][64];
static int devtmpfs_symlink_next;

static struct devtmpfs_dir devtmpfs_root_data;
static struct devtmpfs_dir devtmpfs_dev_data;
static struct devtmpfs_dir devtmpfs_mnt_data;
static struct devtmpfs_dir devtmpfs_devmount_root;

/* --- Directory inode ops --- */

static int devtmpfs_readdir(struct vfs_inode *dir, uint32_t index,
			 struct vfs_dirent *dent)
{
	struct devtmpfs_dir *td = (struct devtmpfs_dir *)dir->private_data;

	if (index >= (uint32_t)td->count)
		return -1;

	dent->d_ino = td->entries[index].inode->i_no;
	dent->d_type = (td->entries[index].inode->i_type == VFS_IDIR)
			? 4 : 8;
	strncpy(dent->d_name, td->entries[index].name,
		sizeof(dent->d_name) - 1);
	dent->d_name[sizeof(dent->d_name) - 1] = '\0';
	return 0;
}

static struct vfs_inode *devtmpfs_lookup(struct vfs_inode *dir, const char *name)
{
	struct devtmpfs_dir *td = (struct devtmpfs_dir *)dir->private_data;
	int i;

	for (i = 0; i < td->count; i++) {
		if (strcmp(td->entries[i].name, name) == 0)
			return td->entries[i].inode;
	}
	return 0;
}

static int devtmpfs_add_entry(struct vfs_inode *dir, const char *name,
			   struct vfs_inode *entry)
{
	struct devtmpfs_dir *td = (struct devtmpfs_dir *)dir->private_data;
	int i;

	if (td->count >= DEVTMPFS_MAX_ENTRIES)
		return -1;

	for (i = 0; name[i] && i < (int)sizeof(td->entries[td->count].name) - 1; i++)
		td->entries[td->count].name[i] = name[i];
	td->entries[td->count].name[i] = '\0';
	td->entries[td->count].inode = entry;
	td->count++;
	return 0;
}

static int devtmpfs_remove_entry(struct vfs_inode *dir, const char *name)
{
	struct devtmpfs_dir *td = (struct devtmpfs_dir *)dir->private_data;
	int i;

	for (i = 0; i < td->count; i++) {
		if (strcmp(td->entries[i].name, name) == 0) {
			td->entries[i] = td->entries[td->count - 1];
			td->count--;
			return 0;
		}
	}
	return -1;
}

static int devtmpfs_mkdir(struct vfs_inode *parent, const char *name)
{
	struct devtmpfs_dir *td = (struct devtmpfs_dir *)parent->private_data;
	struct devtmpfs_dir *new_dir;

	if (td->count >= DEVTMPFS_MAX_ENTRIES)
		return -1;

	if (devtmpfs_dir_next >= DEVTMPFS_MAX_DIRS)
		return -1;

	new_dir = &devtmpfs_dirs[devtmpfs_dir_next++];
	memset(new_dir, 0, sizeof(*new_dir));

	struct vfs_inode *inode = vfs_alloc_inode();
	if (!inode)
		return -1;

	inode->i_type = VFS_IDIR;
	inode->ops = parent->ops;
	inode->private_data = new_dir;

	return devtmpfs_add_entry(parent, name, inode);
}

static int devtmpfs_unlink(struct vfs_inode *parent, const char *name)
{
	return devtmpfs_remove_entry(parent, name);
}

static int devtmpfs_rmdir(struct vfs_inode *parent, const char *name)
{
	struct devtmpfs_dir *td = (struct devtmpfs_dir *)parent->private_data;
	int i;

	for (i = 0; i < td->count; i++) {
		if (strcmp(td->entries[i].name, name) == 0) {
			struct devtmpfs_dir *child = (struct devtmpfs_dir *)
				td->entries[i].inode->private_data;
			if (child && child->count > 0)
				return -1;
			td->entries[i] = td->entries[td->count - 1];
			td->count--;
			return 0;
		}
	}
	return -1;
}

static int devtmpfs_symlink(struct vfs_inode *parent, const char *name,
			 const char *target)
{
	if (devtmpfs_symlink_next >= DEVTMPFS_MAX_SYMLINKS)
		return -1;

	char *target_copy = devtmpfs_symlink_targets[devtmpfs_symlink_next];
	memset(target_copy, 0, 64);
	int len = strlen(target);
	if (len > 63)
		len = 63;
	memcpy(target_copy, target, len);
	target_copy[len] = '\0';

	struct vfs_inode *inode = vfs_alloc_inode();
	if (!inode)
		return -1;

	inode->i_type = VFS_ISYMLINK;
	inode->ops = parent->ops;
	inode->private_data = target_copy;
	inode->i_size = len;
	devtmpfs_symlink_next++;

	return devtmpfs_add_entry(parent, name, inode);
}

static int devtmpfs_readlink_op(struct vfs_inode *inode, char *buf,
			     uint32_t size)
{
	char *target = (char *)inode->private_data;
	if (!target)
		return -1;

	int len = strlen(target);
	if ((int)size - 1 < len)
		len = size - 1;
	memcpy(buf, target, len);
	buf[len] = '\0';
	return len;
}

static struct vfs_inode_ops devtmpfs_dir_ops = {
	.read      = 0,
	.readdir   = devtmpfs_readdir,
	.lookup    = devtmpfs_lookup,
	.add_entry = devtmpfs_add_entry,
	.remove_entry = devtmpfs_remove_entry,
	.mkdir     = devtmpfs_mkdir,
	.rmdir     = devtmpfs_rmdir,
	.unlink    = devtmpfs_unlink,
	.symlink   = devtmpfs_symlink,
	.readlink  = devtmpfs_readlink_op,
};

/* --- Init --- */

void devtmpfs_init(void)
{
	struct devtmpfs_dir *root_dir = &devtmpfs_root_data;
	struct devtmpfs_dir *dev_dir = &devtmpfs_dev_data;

	root_dir->count = 0;
	dev_dir->count = 0;
	devtmpfs_mnt_data.count = 0;
	devtmpfs_dir_next = 0;
	devtmpfs_symlink_next = 0;

	/* Allocate root directory inode */
	struct vfs_inode *root_inode = vfs_alloc_inode();
	if (!root_inode)
		return;
	root_inode->i_type = VFS_IDIR;
	root_inode->ops = &devtmpfs_dir_ops;
	root_inode->private_data = root_dir;

	/* Allocate /dev directory inode */
	struct vfs_inode *dev_inode = vfs_alloc_inode();
	if (!dev_inode)
		return;
	dev_inode->i_type = VFS_IDIR;
	dev_inode->ops = &devtmpfs_dir_ops;
	dev_inode->private_data = dev_dir;

	/* Add "dev" entry to root */
	devtmpfs_add_entry(root_inode, "dev", dev_inode);

	/* Allocate /mnt directory inode */
	struct vfs_inode *mnt_inode = vfs_alloc_inode();
	if (mnt_inode) {
		mnt_inode->i_type = VFS_IDIR;
		mnt_inode->ops = &devtmpfs_dir_ops;
		mnt_inode->private_data = &devtmpfs_mnt_data;
		devtmpfs_add_entry(root_inode, "mnt", mnt_inode);
	}

	vfs_set_root_inode(root_inode);
}

struct vfs_inode *devtmpfs_create_mount(void)
{
	struct vfs_inode *inode = vfs_alloc_inode();
	if (!inode)
		return 0;

	devtmpfs_devmount_root.count = 0;
	inode->i_type = VFS_IDIR;
	inode->ops = &devtmpfs_dir_ops;
	inode->private_data = &devtmpfs_devmount_root;
	return inode;
}
