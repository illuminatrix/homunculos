#include "vfs.h"
#include <string.h>
#include <stdio.h>

#define TMPFS_MAX_ENTRIES 16
#define TMPFS_MAX_DIRS 16
#define TMPFS_MAX_SYMLINKS 16

struct tmpfs_entry {
	char name[64];
	struct vfs_inode *inode;
};

struct tmpfs_dir {
	struct tmpfs_entry entries[TMPFS_MAX_ENTRIES];
	int count;
};

/* Static pools */
static struct tmpfs_dir tmpfs_dirs[TMPFS_MAX_DIRS];
static int tmpfs_dir_next;
static char tmpfs_symlink_targets[TMPFS_MAX_SYMLINKS][64];
static int tmpfs_symlink_next;

static struct tmpfs_dir tmpfs_root_data;
static struct tmpfs_dir tmpfs_dev_data;
static struct tmpfs_dir tmpfs_mnt_data;
static struct tmpfs_dir tmpfs_devmount_root;

/* --- Directory inode ops --- */

static int tmpfs_readdir(struct vfs_inode *dir, uint32_t index,
			 struct vfs_dirent *dent)
{
	struct tmpfs_dir *td = (struct tmpfs_dir *)dir->private_data;

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

static struct vfs_inode *tmpfs_lookup(struct vfs_inode *dir, const char *name)
{
	struct tmpfs_dir *td = (struct tmpfs_dir *)dir->private_data;
	int i;

	for (i = 0; i < td->count; i++) {
		if (strcmp(td->entries[i].name, name) == 0)
			return td->entries[i].inode;
	}
	return 0;
}

static int tmpfs_add_entry(struct vfs_inode *dir, const char *name,
			   struct vfs_inode *entry)
{
	struct tmpfs_dir *td = (struct tmpfs_dir *)dir->private_data;
	int i;

	if (td->count >= TMPFS_MAX_ENTRIES)
		return -1;

	for (i = 0; name[i] && i < (int)sizeof(td->entries[td->count].name) - 1; i++)
		td->entries[td->count].name[i] = name[i];
	td->entries[td->count].name[i] = '\0';
	td->entries[td->count].inode = entry;
	td->count++;
	return 0;
}

static int tmpfs_remove_entry(struct vfs_inode *dir, const char *name)
{
	struct tmpfs_dir *td = (struct tmpfs_dir *)dir->private_data;
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

static int tmpfs_mkdir(struct vfs_inode *parent, const char *name)
{
	struct tmpfs_dir *td = (struct tmpfs_dir *)parent->private_data;
	struct tmpfs_dir *new_dir;

	if (td->count >= TMPFS_MAX_ENTRIES)
		return -1;

	if (tmpfs_dir_next >= TMPFS_MAX_DIRS)
		return -1;

	new_dir = &tmpfs_dirs[tmpfs_dir_next++];
	memset(new_dir, 0, sizeof(*new_dir));

	struct vfs_inode *inode = vfs_alloc_inode();
	if (!inode)
		return -1;

	inode->i_type = VFS_IDIR;
	inode->ops = parent->ops;
	inode->private_data = new_dir;

	return tmpfs_add_entry(parent, name, inode);
}

static int tmpfs_unlink(struct vfs_inode *parent, const char *name)
{
	return tmpfs_remove_entry(parent, name);
}

static int tmpfs_rmdir(struct vfs_inode *parent, const char *name)
{
	struct tmpfs_dir *td = (struct tmpfs_dir *)parent->private_data;
	int i;

	for (i = 0; i < td->count; i++) {
		if (strcmp(td->entries[i].name, name) == 0) {
			struct tmpfs_dir *child = (struct tmpfs_dir *)
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

static int tmpfs_symlink(struct vfs_inode *parent, const char *name,
			 const char *target)
{
	if (tmpfs_symlink_next >= TMPFS_MAX_SYMLINKS)
		return -1;

	char *target_copy = tmpfs_symlink_targets[tmpfs_symlink_next];
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
	tmpfs_symlink_next++;

	return tmpfs_add_entry(parent, name, inode);
}

static int tmpfs_readlink_op(struct vfs_inode *inode, char *buf,
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

static struct vfs_inode_ops tmpfs_dir_ops = {
	.read      = 0,
	.readdir   = tmpfs_readdir,
	.lookup    = tmpfs_lookup,
	.add_entry = tmpfs_add_entry,
	.remove_entry = tmpfs_remove_entry,
	.mkdir     = tmpfs_mkdir,
	.rmdir     = tmpfs_rmdir,
	.unlink    = tmpfs_unlink,
	.symlink   = tmpfs_symlink,
	.readlink  = tmpfs_readlink_op,
};

/* --- Init --- */

void tmpfs_init(void)
{
	struct tmpfs_dir *root_dir = &tmpfs_root_data;
	struct tmpfs_dir *dev_dir = &tmpfs_dev_data;

	root_dir->count = 0;
	dev_dir->count = 0;
	tmpfs_mnt_data.count = 0;
	tmpfs_dir_next = 0;
	tmpfs_symlink_next = 0;

	/* Allocate root directory inode */
	struct vfs_inode *root_inode = vfs_alloc_inode();
	if (!root_inode)
		return;
	root_inode->i_type = VFS_IDIR;
	root_inode->ops = &tmpfs_dir_ops;
	root_inode->private_data = root_dir;

	/* Allocate /dev directory inode */
	struct vfs_inode *dev_inode = vfs_alloc_inode();
	if (!dev_inode)
		return;
	dev_inode->i_type = VFS_IDIR;
	dev_inode->ops = &tmpfs_dir_ops;
	dev_inode->private_data = dev_dir;

	/* Add "dev" entry to root */
	tmpfs_add_entry(root_inode, "dev", dev_inode);

	/* Allocate /mnt directory inode */
	struct vfs_inode *mnt_inode = vfs_alloc_inode();
	if (mnt_inode) {
		mnt_inode->i_type = VFS_IDIR;
		mnt_inode->ops = &tmpfs_dir_ops;
		mnt_inode->private_data = &tmpfs_mnt_data;
		tmpfs_add_entry(root_inode, "mnt", mnt_inode);
	}

	vfs_set_root_inode(root_inode);
}

struct vfs_inode *tmpfs_create_mount(void)
{
	struct vfs_inode *inode = vfs_alloc_inode();
	if (!inode)
		return 0;

	tmpfs_devmount_root.count = 0;
	inode->i_type = VFS_IDIR;
	inode->ops = &tmpfs_dir_ops;
	inode->private_data = &tmpfs_devmount_root;
	return inode;
}
