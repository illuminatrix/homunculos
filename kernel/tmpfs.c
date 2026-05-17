#include "vfs.h"
#include <string.h>

#define TMPFS_MAX_ENTRIES 16

struct tmpfs_entry {
	char name[64];
	struct vfs_inode *inode;
};

struct tmpfs_dir {
	struct tmpfs_entry entries[TMPFS_MAX_ENTRIES];
	int count;
};

/* Static directory storage */
static struct tmpfs_dir tmpfs_root_data;
static struct tmpfs_dir tmpfs_dev_data;

/* --- Directory inode ops --- */

static int tmpfs_readdir(struct vfs_inode *dir, uint32_t index,
			 struct vfs_dirent *dent)
{
	struct tmpfs_dir *td = (struct tmpfs_dir *)dir->private_data;

	if (index >= (uint32_t)td->count)
		return -1;

	dent->d_ino = td->entries[index].inode->i_no;
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

static struct vfs_inode_ops tmpfs_dir_ops = {
	.read      = 0,
	.readdir   = tmpfs_readdir,
	.lookup    = tmpfs_lookup,
	.add_entry = tmpfs_add_entry,
};

/* --- Init --- */

void tmpfs_init(void)
{
	struct tmpfs_dir *root_dir = &tmpfs_root_data;
	struct tmpfs_dir *dev_dir = &tmpfs_dev_data;

	root_dir->count = 0;
	dev_dir->count = 0;

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

	vfs_set_root_inode(root_inode);
}
