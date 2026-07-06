#include "vfs.h"
#include "devtmpfs.h"
#include <string.h>
#include <stdio.h>

#define DEVTMPFS_MAX_ENTRIES 16
#define DEVTMPFS_MAX_DIRS 16
#define DEVTMPFS_MAX_SYMLINKS 16
#define DEVTMPFS_MAX_DEVICES 32

/* --- Device registration table (for late /dev population) --- */

#define DEVTMPFS_DEVICE_VFS   0
#define DEVTMPFS_DEVICE_INODE 1

struct devtmpfs_device {
	char name[64];
	int type;
	uint16_t i_type;
	dev_t dev;
	/* For DEVTMPFS_DEVICE_INODE type */
	const struct vfs_inode_ops *inode_ops;
	void *private_data;
	/* For DEVTMPFS_DEVICE_VFS type */
	const struct vfs_ops *vfs_ops;
	void *vfs_private;
};

static struct devtmpfs_device devtmpfs_devices[DEVTMPFS_MAX_DEVICES];
static int devtmpfs_device_count;

/* --- vfs_ops wrapper: converts file-level ops to inode-level --- */

static int devtmpfs_vfs_read(struct vfs_inode *inode, uint32_t offset,
			     void *buf, uint32_t size)
{
	struct devtmpfs_device *dev =
		(struct devtmpfs_device *)inode->private_data;
	struct file f;
	if (!dev->vfs_ops->read)
		return -1;
	f.ops = dev->vfs_ops;
	f.private_data = dev->vfs_private;
	f.pos = offset;
	return dev->vfs_ops->read(&f, buf, size);
}

static int devtmpfs_vfs_write(struct vfs_inode *inode, uint32_t offset,
			      const void *buf, uint32_t size)
{
	struct devtmpfs_device *dev =
		(struct devtmpfs_device *)inode->private_data;
	struct file f;
	if (!dev->vfs_ops->write)
		return -1;
	f.ops = dev->vfs_ops;
	f.private_data = dev->vfs_private;
	f.pos = offset;
	return dev->vfs_ops->write(&f, buf, size);
}

static int devtmpfs_vfs_ioctl(struct vfs_inode *inode, int cmd, void *arg)
{
	struct devtmpfs_device *dev =
		(struct devtmpfs_device *)inode->private_data;
	struct file f;
	if (!dev->vfs_ops->ioctl)
		return -1;
	f.ops = dev->vfs_ops;
	f.private_data = dev->vfs_private;
	f.pos = 0;
	return dev->vfs_ops->ioctl(&f, cmd, arg);
}

static struct vfs_inode_ops devtmpfs_vfs_inode_ops = {
	.read      = devtmpfs_vfs_read,
	.write     = devtmpfs_vfs_write,
	.ioctl     = devtmpfs_vfs_ioctl,
	.readdir   = 0,
	.lookup    = 0,
	.add_entry = 0,
	.remove_entry = 0,
	.mkdir     = 0,
	.rmdir     = 0,
	.unlink    = 0,
	.symlink   = 0,
	.link      = 0,
	.readlink  = 0,
};

/* Forward declaration for dir ops used by devtmpfs_mkdir */
static struct vfs_inode_ops devtmpfs_dir_ops;

/* --- Directory inode ops for devtmpfs --- */

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

static int devtmpfs_mkdir(struct vfs_inode *parent, const char *name, uint16_t mode)
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
	inode->i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	inode->ops = &devtmpfs_dir_ops;
	inode->private_data = new_dir;

	return devtmpfs_add_entry(parent, name, inode);
}

/* Null ops for device nodes created by mknod (no backing driver) */
static int devtmpfs_dev_null_read(struct vfs_inode *inode, uint32_t offset,
				  void *buf, uint32_t size)
{
	(void)inode; (void)offset; (void)buf; (void)size;
	return -1;
}

static int devtmpfs_dev_null_write(struct vfs_inode *inode, uint32_t offset,
				   const void *buf, uint32_t size)
{
	(void)inode; (void)offset; (void)buf; (void)size;
	return -1;
}

static struct vfs_inode_ops devtmpfs_dev_null_ops = {
	.read      = devtmpfs_dev_null_read,
	.write     = devtmpfs_dev_null_write,
	.readdir   = 0,
	.lookup    = 0,
	.add_entry = 0,
	.remove_entry = 0,
	.mkdir     = 0,
	.rmdir     = 0,
	.unlink    = 0,
	.symlink   = 0,
	.mknod     = 0,
	.link      = 0,
	.readlink  = 0,
};

static int devtmpfs_mknod(struct vfs_inode *parent, const char *name,
			  uint16_t mode, dev_t dev)
{
	struct devtmpfs_dir *td;
	struct vfs_inode *inode;

	if (!parent || !name)
		return -1;

	td = (struct devtmpfs_dir *)parent->private_data;
	if (td->count >= DEVTMPFS_MAX_ENTRIES)
		return -1;

	inode = vfs_alloc_inode();
	if (!inode)
		return -1;

	uint16_t file_type = mode & S_IFMT;
	if (file_type == S_IFCHR)
		inode->i_type = VFS_IFCHR;
	else if (file_type == S_IFBLK)
		inode->i_type = VFS_IFBLK;
	else {
		vfs_free_inode(inode);
		return -1;
	}

	inode->i_size = 0;
	inode->i_mode = mode;
	inode->i_rdev = dev;
	inode->ops = &devtmpfs_dev_null_ops;
	inode->private_data = 0;

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
	inode->i_mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
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
	.mknod     = devtmpfs_mknod,
	.link      = 0,
	.readlink  = devtmpfs_readlink_op,
};

/* --- Device registration API --- */

void devtmpfs_register_vfs(const char *name,
			   const struct vfs_ops *ops,
			   void *private_data,
			   dev_t dev)
{
	if (devtmpfs_device_count >= DEVTMPFS_MAX_DEVICES || !name || !ops)
		return;

	struct devtmpfs_device *d = &devtmpfs_devices[devtmpfs_device_count];
	int i;

	for (i = 0; name[i] && i < (int)sizeof(d->name) - 1; i++)
		d->name[i] = name[i];
	d->name[i] = '\0';

	d->type = DEVTMPFS_DEVICE_VFS;
	d->i_type = VFS_IFILE;
	d->dev = dev;
	d->vfs_ops = ops;
	d->vfs_private = private_data;
	devtmpfs_device_count++;
}

void devtmpfs_register_inode(const char *name,
			     const struct vfs_inode_ops *ops,
			     void *private_data,
			     uint16_t i_type,
			     dev_t dev)
{
	if (devtmpfs_device_count >= DEVTMPFS_MAX_DEVICES || !name)
		return;

	struct devtmpfs_device *d = &devtmpfs_devices[devtmpfs_device_count];
	int i;

	for (i = 0; name[i] && i < (int)sizeof(d->name) - 1; i++)
		d->name[i] = name[i];
	d->name[i] = '\0';

	d->type = DEVTMPFS_DEVICE_INODE;
	d->i_type = i_type;
	d->dev = dev;
	d->inode_ops = ops;
	d->private_data = private_data;
	devtmpfs_device_count++;
}

/* --- Create device inodes in a given devtmpfs directory --- */

static void populate_dir(struct vfs_inode *dev_dir)
{
	int i;

	if (!dev_dir || dev_dir->i_type != VFS_IDIR)
		return;

	for (i = 0; i < devtmpfs_device_count; i++) {
		struct devtmpfs_device *dev = &devtmpfs_devices[i];
		struct vfs_inode *inode = vfs_alloc_inode();

		if (!inode)
			continue;

		inode->i_type = dev->i_type;
		inode->i_size = 0;
		inode->i_mode = (dev->i_type == VFS_IDIR)
				? (S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) : (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		inode->i_rdev = dev->dev;

		if (dev->type == DEVTMPFS_DEVICE_VFS) {
			inode->ops = &devtmpfs_vfs_inode_ops;
			/* private_data points to the device entry so wrapper
			 * ops can find both vfs_ops and vfs_private */
			inode->private_data = dev;
		} else {
			inode->ops = dev->inode_ops;
			inode->private_data = dev->private_data;
		}

		devtmpfs_add_entry(dev_dir, dev->name, inode);
	}
}

void devtmpfs_create_nodes(void)
{
	struct vfs_inode *dev_dir = vfs_resolve_path("/dev");

	if (!dev_dir)
		return;

	populate_dir(dev_dir);
}

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
	root_inode->i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	root_inode->ops = &devtmpfs_dir_ops;
	root_inode->private_data = root_dir;

	/* Allocate /dev directory inode */
	struct vfs_inode *dev_inode = vfs_alloc_inode();
	if (!dev_inode)
		return;
	dev_inode->i_type = VFS_IDIR;
	dev_inode->i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	dev_inode->ops = &devtmpfs_dir_ops;
	dev_inode->private_data = dev_dir;

	/* Add "dev" entry to root */
	devtmpfs_add_entry(root_inode, "dev", dev_inode);

	/* Allocate /mnt directory inode */
	struct vfs_inode *mnt_inode = vfs_alloc_inode();
	if (mnt_inode) {
		mnt_inode->i_type = VFS_IDIR;
		mnt_inode->i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
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
	inode->i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	inode->ops = &devtmpfs_dir_ops;
	inode->private_data = &devtmpfs_devmount_root;

	populate_dir(inode);

	return inode;
}
