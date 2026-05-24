#include "vfs.h"
#include <string.h>
#include <stdio.h>

/* Linker symbols for .driver_init section */
extern vfs_driver_init_fn __driver_init_start[];
extern vfs_driver_init_fn __driver_init_end[];

void vfs_init(void)
{
	vfs_driver_init_fn *fn;

	for (fn = __driver_init_start; fn < __driver_init_end; fn++)
		(*fn)();
}

/* === Inode Layer === */

static struct vfs_inode vfs_inodes[VFS_MAX_INODES];
static int vfs_next_ino = 1;

static struct file vfs_open_files[VFS_MAX_OPEN_FILES];
static int vfs_open_file_count;

static struct vfs_inode *vfs_root;

static int vfs_file_read(struct file *f, void *buf, size_t nbyte);
static int vfs_file_write(struct file *f, const void *buf, size_t nbyte);

static struct vfs_ops vfs_file_ops = {
	.read  = vfs_file_read,
	.write = vfs_file_write,
};

void vfs_inode_init(void)
{
	int i;
	for (i = 0; i < VFS_MAX_INODES; i++)
		vfs_inodes[i].i_no = 0;
	vfs_next_ino = 1;
	vfs_open_file_count = 0;
	vfs_root = 0;
}

struct vfs_inode *vfs_alloc_inode(void)
{
	int i;
	for (i = 0; i < VFS_MAX_INODES; i++) {
		if (vfs_inodes[i].i_no == 0) {
			vfs_inodes[i].i_no = vfs_next_ino++;
			vfs_inodes[i].i_size = 0;
			vfs_inodes[i].i_type = VFS_IFILE;
			vfs_inodes[i].ops = 0;
			vfs_inodes[i].private_data = 0;
			return &vfs_inodes[i];
		}
	}
	return 0;
}

void vfs_free_inode(struct vfs_inode *inode)
{
	if (!inode)
		return;
	inode->i_no = 0;
}

void vfs_set_root_inode(struct vfs_inode *inode)
{
	vfs_root = inode;
}

struct vfs_inode *vfs_root_inode(void)
{
	return vfs_root;
}

/* === Mount Abstraction === */

static struct vfs_mount vfs_mounts[VFS_MAX_MOUNTS];
static int vfs_mount_count;

struct vfs_mount *vfs_mount_create(struct vfs_inode *mount_point,
				   struct vfs_inode *fs_root)
{
	if (vfs_mount_count >= VFS_MAX_MOUNTS || !mount_point || !fs_root)
		return 0;
	vfs_mounts[vfs_mount_count].mount_point = mount_point;
	vfs_mounts[vfs_mount_count].root = fs_root;
	vfs_mount_count++;
	return &vfs_mounts[vfs_mount_count - 1];
}

void vfs_mount_destroy(struct vfs_mount *mnt)
{
	int i;
	if (!mnt)
		return;
	for (i = 0; i < vfs_mount_count; i++) {
		if (&vfs_mounts[i] == mnt) {
			vfs_mounts[i] = vfs_mounts[vfs_mount_count - 1];
			vfs_mount_count--;
			return;
		}
	}
}

int vfs_unmount_path(const char *path)
{
	struct vfs_inode *target = vfs_resolve_path(path);
	int i;

	if (!target)
		return -1;

	for (i = 0; i < vfs_mount_count; i++) {
		if (vfs_mounts[i].root == target) {
			vfs_mount_destroy(&vfs_mounts[i]);
			return 0;
		}
	}

	return -1;
}

struct vfs_inode *vfs_resolve_mount(struct vfs_inode *inode)
{
	int i;
	if (!inode)
		return 0;
	for (i = 0; i < vfs_mount_count; i++) {
		struct vfs_inode *mp = vfs_mounts[i].mount_point;
		/* Compare by inode number and ops table (filesystem type)
		 * instead of pointer, since filesystem lookups allocate new
		 * VFS inode objects for the same underlying resource */
		if (mp && mp->i_no == inode->i_no && mp->ops == inode->ops)
			return vfs_mounts[i].root;
	}
	return inode;
}

/* --------------------------------------------------------------- */

struct vfs_inode *vfs_resolve_path(const char *path)
{
	struct vfs_inode *current = vfs_root_inode();
	char buf[64];
	int i;

	if (!current || !path)
		return 0;

	if (*path == '/')
		path++;
	if (*path == '\0')
		return vfs_resolve_mount(current);

	while (*path) {
		i = 0;
		while (*path && *path != '/' && i < (int)sizeof(buf) - 1) {
			buf[i++] = *path++;
		}
		buf[i] = '\0';

		/* Before looking up, resolve mount if current is a mount point */
		current = vfs_resolve_mount(current);

		if (!current->ops || !current->ops->lookup)
			return 0;
		current = current->ops->lookup(current, buf);
		if (!current)
			return 0;

		if (*path == '/')
			path++;
	}

	/* Resolve mount at the final inode too */
	return vfs_resolve_mount(current);
}

int vfs_register_by_path(const char *path, struct vfs_inode *inode)
{
	const char *p;
	int i;

	if (!path || !inode)
		return -1;

	p = path;
	if (*p == '/')
		p++;
	if (*p == '\0')
		return -1;

	/* Find last '/' to split parent path from entry name */
	const char *last_slash = 0;
	const char *q = p;
	while (*q) {
		if (*q == '/')
			last_slash = q;
		q++;
	}

	if (last_slash) {
		/* Has parent path; build parent path string */
		char parent_path[64];
		int pi = 0;
		parent_path[pi++] = '/';
		for (i = 0; &p[i] < last_slash; i++)
			parent_path[pi++] = p[i];
		parent_path[pi] = '\0';

		struct vfs_inode *parent = vfs_resolve_path(parent_path);
		if (!parent || !parent->ops || !parent->ops->add_entry)
			return -1;

		return parent->ops->add_entry(parent, last_slash + 1, inode);
	} else {
		/* Single component — add to root */
		struct vfs_inode *root = vfs_root_inode();
		if (!root || !root->ops || !root->ops->add_entry)
			return -1;
		return root->ops->add_entry(root, p, inode);
	}
}

/* === Device Registration === */

static struct vfs_device vfs_devices[VFS_MAX_DEVICES];
static int vfs_device_count;

int vfs_register_device(const char *name, const struct vfs_ops *ops,
			void *private_data)
{
	if (vfs_device_count >= VFS_MAX_DEVICES || !name || !ops)
		return -1;
	vfs_devices[vfs_device_count].name = name;
	vfs_devices[vfs_device_count].ops = ops;
	vfs_devices[vfs_device_count].private_data = private_data;
	vfs_device_count++;
	return 0;
}

static int dev_inode_read(struct vfs_inode *inode, uint32_t offset,
			  void *buf, uint32_t size)
{
	struct vfs_device *dev = (struct vfs_device *)inode->private_data;
	struct file f;
	if (!dev->ops->read)
		return -1;
	f.ops = dev->ops;
	f.private_data = dev->private_data;
	f.pos = offset;
	return dev->ops->read(&f, buf, size);
}

static int dev_inode_write(struct vfs_inode *inode, uint32_t offset,
			   const void *buf, uint32_t size)
{
	struct vfs_device *dev = (struct vfs_device *)inode->private_data;
	struct file f;
	if (!dev->ops->write)
		return -1;
	f.ops = dev->ops;
	f.private_data = dev->private_data;
	f.pos = offset;
	return dev->ops->write(&f, buf, size);
}

static struct vfs_inode_ops vfs_dev_inode_ops = {
	.read      = dev_inode_read,
	.write     = dev_inode_write,
	.readdir   = 0,
	.lookup    = 0,
	.add_entry = 0,
};

void vfs_create_device_nodes(void)
{
	int i;
	for (i = 0; i < vfs_device_count; i++) {
		struct vfs_inode *inode = vfs_alloc_inode();
		char path[64];
		int pi;
		const char *src;

		if (!inode)
			continue;

		inode->i_type = VFS_IFILE;
		inode->i_size = 0;
		inode->ops = &vfs_dev_inode_ops;
		inode->private_data = &vfs_devices[i];

		pi = 0;
		src = "/dev/";
		while (*src && pi < (int)sizeof(path) - 1)
			path[pi++] = *src++;
		src = vfs_devices[i].name;
		while (*src && pi < (int)sizeof(path) - 1)
			path[pi++] = *src++;
		path[pi] = '\0';

		vfs_register_by_path(path, inode);
	}
}

struct file *vfs_open_file(struct vfs_inode *inode)
{
	int i;
	if (!inode)
		return 0;
	for (i = 0; i < VFS_MAX_OPEN_FILES; i++) {
		if (vfs_open_files[i].ops == 0) {
			vfs_open_files[i].ops = &vfs_file_ops;
			vfs_open_files[i].private_data = inode;
			vfs_open_files[i].pos = 0;
			return &vfs_open_files[i];
		}
	}
	return 0;
}

void vfs_close_file(struct file *f)
{
	if (!f)
		return;
	f->ops = 0;
	f->private_data = 0;
	f->pos = 0;
}

static int vfs_file_read(struct file *f, void *buf, size_t nbyte)
{
	struct vfs_inode *inode = (struct vfs_inode *)f->private_data;
	int ret;
	if (!inode || !inode->ops)
		return -1;

	if (inode->i_type == VFS_IDIR) {
		/* Pack directory entries as space-separated names */
		char *dst = (char *)buf;
		int written = 0;
		int idx = 0;
		struct vfs_dirent dent;
		if (!inode->ops->readdir)
			return -1;
		while (inode->ops->readdir(inode, idx++, &dent) == 0) {
			int len = strlen(dent.d_name);
			if (written + len + 1 > (int)nbyte)
				break;
			memcpy(dst + written, dent.d_name, len);
			written += len;
			dst[written++] = ' ';
		}
		return written;
	}

	if (!inode->ops->read)
		return -1;
	ret = inode->ops->read(inode, f->pos, buf, nbyte);
	if (ret > 0)
		f->pos += ret;
	return ret;
}

void vfs_inode_stat(struct vfs_inode *inode, struct vfs_stat *buf)
{
	unsigned short mode = 0;

	if (!inode || !buf)
		return;

	switch (inode->i_type) {
	case VFS_IFILE:
		mode = 0100644;  /* S_IFREG | S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH */
		break;
	case VFS_IDIR:
		mode = 040755;   /* S_IFDIR | S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH */
		break;
	default:
		mode = 0100644;
		break;
	}

	buf->st_dev     = 0;
	buf->st_ino     = inode->i_no;
	buf->st_mode    = mode;
	buf->st_nlink   = (inode->i_type == VFS_IDIR) ? 2 : 1;
	buf->st_uid     = 0;
	buf->st_gid     = 0;
	buf->st_rdev    = 0;
	buf->st_size    = inode->i_size;
	buf->st_blksize = 1024;
	buf->st_blocks  = (inode->i_size + 511) / 512;
	buf->st_atime   = 0;
	buf->st_mtime   = 0;
	buf->st_ctime   = 0;
}

static int vfs_file_write(struct file *f, const void *buf, size_t nbyte)
{
	struct vfs_inode *inode = (struct vfs_inode *)f->private_data;
	int ret;
	if (!inode || !inode->ops || !inode->ops->write)
		return -1;
	ret = inode->ops->write(inode, f->pos, buf, nbyte);
	if (ret > 0)
		f->pos += ret;
	return ret;
}
