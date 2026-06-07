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
static int vfs_file_ioctl(struct file *f, int cmd, void *arg);

static struct vfs_ops vfs_file_ops = {
	.read  = vfs_file_read,
	.write = vfs_file_write,
	.ioctl = vfs_file_ioctl,
	.close = 0,
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
			vfs_inodes[i].i_mode = 0;
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
	int symlink_depth = 0;

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

		/* Follow symlinks (limit recursion to avoid loops) */
		while (current->i_type == VFS_ISYMLINK
		       && current->ops && current->ops->readlink) {
			char link_target[256];
			int n;
			if (symlink_depth++ > 8)
				return 0;
			n = current->ops->readlink(current, link_target,
						   sizeof(link_target));
			if (n < 0)
				return 0;
			link_target[n] = '\0';
			/* If absolute, restart resolution from root */
			if (link_target[0] == '/') {
				current = vfs_root_inode();
				if (*path == '/')
					path++;
				continue;
			}
			/* Relative: look up the link target in current dir */
			current = vfs_resolve_mount(current);
			current = current->ops->lookup(current, link_target);
			if (!current)
				return 0;
		}

		if (*path == '/')
			path++;
	}

	/* Resolve mount at the final inode too */
	current = vfs_resolve_mount(current);
	/* Follow trailing symlink */
	if (current->i_type == VFS_ISYMLINK
	    && current->ops && current->ops->readlink) {
		char link_target[256];
		int n;
		if (symlink_depth++ > 8)
			return 0;
		n = current->ops->readlink(current, link_target,
					   sizeof(link_target));
		if (n < 0)
			return 0;
		link_target[n] = '\0';
		if (link_target[0] == '/')
			current = vfs_resolve_path(link_target);
		else {
			current = vfs_root_inode();
			current = vfs_resolve_mount(current);
			current = current->ops->lookup(current, link_target);
			if (!current)
				return 0;
			current = vfs_resolve_mount(current);
		}
	}
	return current;
}

struct vfs_inode *vfs_resolve_path_no_follow(const char *path)
{
	struct vfs_inode *current = vfs_root_inode();
	char buf[64];
	int i;
	int symlink_depth = 0;

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

		/* Only follow symlinks for non-final (intermediate) components */
		if (*path == '/') {
			while (current->i_type == VFS_ISYMLINK
			       && current->ops && current->ops->readlink) {
				char link_target[256];
				int n;
				if (symlink_depth++ > 8)
					return 0;
				n = current->ops->readlink(current, link_target,
							   sizeof(link_target));
				if (n < 0)
					return 0;
				link_target[n] = '\0';
				/* If absolute, restart resolution from root */
				if (link_target[0] == '/') {
					current = vfs_root_inode();
					continue;
				}
				/* Relative: look up the link target in current dir */
				current = vfs_resolve_mount(current);
				current = current->ops->lookup(current, link_target);
				if (!current)
					return 0;
			}
			path++;
		}
	}

	/* Resolve mount at the final inode, but do NOT follow trailing symlink */
	current = vfs_resolve_mount(current);
	return current;
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

struct file *vfs_alloc_file(void)
{
	int i;
	for (i = 0; i < VFS_MAX_OPEN_FILES; i++) {
		if (vfs_open_files[i].ops == 0) {
			vfs_open_files[i].ops = 0;
			vfs_open_files[i].private_data = 0;
			vfs_open_files[i].pos = 0;
			vfs_open_files[i].refcount = 1;
			vfs_open_files[i].flags = 0;
			return &vfs_open_files[i];
		}
	}
	return 0;
}

struct file *vfs_open_file_flags(struct vfs_inode *inode, int flags)
{
	int i;
	if (!inode)
		return 0;
	for (i = 0; i < VFS_MAX_OPEN_FILES; i++) {
		if (vfs_open_files[i].ops == 0) {
			vfs_open_files[i].ops = &vfs_file_ops;
			vfs_open_files[i].private_data = inode;
			vfs_open_files[i].pos = 0;
			vfs_open_files[i].refcount = 1;
			vfs_open_files[i].flags = flags;
			return &vfs_open_files[i];
		}
	}
	printf("vfs_open_file_flags: out of slots!\n");
	return 0;
}

struct file *vfs_open_file(struct vfs_inode *inode)
{
	return vfs_open_file_flags(inode, O_RDONLY);
}

void vfs_close_file(struct file *f)
{
	if (!f)
		return;
	f->refcount--;
	if (f->refcount > 0)
		return;
	if (f->ops && f->ops->close)
		f->ops->close(f);
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

	if (inode->i_mode) {
		mode = inode->i_mode;
	} else {
		switch (inode->i_type) {
		case VFS_IFILE:
			mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
			break;
		case VFS_IDIR:
			mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
			break;
		case VFS_ISYMLINK:
			mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
			break;
		default:
			mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
			break;
		}
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
	int ret;
	struct vfs_inode *inode = (struct vfs_inode *)f->private_data;
	if (!inode || !inode->ops || !inode->ops->write)
		return -1;
	ret = inode->ops->write(inode, f->pos, buf, nbyte);
	if (ret > 0)
		f->pos += ret;
	return ret;
}

static int vfs_file_ioctl(struct file *f, int cmd, void *arg)
{
	struct vfs_inode *inode = (struct vfs_inode *)f->private_data;
	if (!inode || !inode->ops || !inode->ops->ioctl)
		return -1;
	return inode->ops->ioctl(inode, cmd, arg);
}

/* ----------------------------------------------------------------
 * Path manipulation helpers
 * ----------------------------------------------------------------*/

void vfs_split_path(const char *path, char *dir_out, int dir_size,
		    char *name_out, int name_size)
{
	const char *p;
	int i;

	if (!path)
		return;

	p = path;
	if (*p == '/')
		p++;

	/* Find last '/' */
	const char *last_slash = 0;
	const char *q = p;
	while (*q) {
		if (*q == '/')
			last_slash = q;
		q++;
	}

	if (last_slash) {
		/* Has parent dir */
		i = 0;
		if (dir_out && dir_size > 0)
			dir_out[i++] = '/';
		for (; &p[i - 1] < last_slash && i < dir_size - 1; i++)
			dir_out[i] = p[i - 1];
		if (dir_out)
			dir_out[i] = '\0';

		if (name_out) {
			i = 0;
			while (last_slash[1 + i] && i < name_size - 1) {
				name_out[i] = last_slash[1 + i];
				i++;
			}
			name_out[i] = '\0';
		}
	} else {
		/* No slash — single component, parent is root */
		if (dir_out && dir_size > 0) {
			dir_out[0] = '/';
			dir_out[1] = '\0';
		}
		if (name_out) {
			i = 0;
			while (p[i] && i < name_size - 1) {
				name_out[i] = p[i];
				i++;
			}
			name_out[i] = '\0';
		}
	}
}

struct vfs_inode *vfs_create_file(const char *path)
{
	char dir_path[256];
	char name[64];
	struct vfs_inode *parent;
	struct vfs_inode *inode;

	if (!path)
		return 0;

	vfs_split_path(path, dir_path, sizeof(dir_path),
		       name, sizeof(name));

	parent = vfs_resolve_path(dir_path);
	if (!parent || parent->i_type != VFS_IDIR
	    || !parent->ops || !parent->ops->add_entry)
		return 0;

	inode = vfs_alloc_inode();
	if (!inode)
		return 0;
	inode->i_type = VFS_IFILE;

	/* Filesystem fills in private_data via add_entry */
	if (parent->ops->add_entry(parent, name, inode) < 0) {
		vfs_free_inode(inode);
		return 0;
	}
	return inode;
}

int vfs_mkdir(const char *path)
{
	char dir_path[256];
	char name[64];
	struct vfs_inode *parent;

	if (!path)
		return -1;

	vfs_split_path(path, dir_path, sizeof(dir_path),
		       name, sizeof(name));

	parent = vfs_resolve_path(dir_path);
	if (!parent || parent->i_type != VFS_IDIR
	    || !parent->ops || !parent->ops->mkdir)
		return -1;

	return parent->ops->mkdir(parent, name);
}

int vfs_rmdir(const char *path)
{
	char dir_path[256];
	char name[64];
	struct vfs_inode *parent;

	if (!path)
		return -1;

	vfs_split_path(path, dir_path, sizeof(dir_path),
		       name, sizeof(name));

	parent = vfs_resolve_path(dir_path);
	if (!parent || parent->i_type != VFS_IDIR
	    || !parent->ops || !parent->ops->rmdir)
		return -1;

	return parent->ops->rmdir(parent, name);
}

int vfs_unlink(const char *path)
{
	char dir_path[256];
	char name[64];
	struct vfs_inode *parent;

	if (!path)
		return -1;

	vfs_split_path(path, dir_path, sizeof(dir_path),
		       name, sizeof(name));

	parent = vfs_resolve_path(dir_path);
	if (!parent || parent->i_type != VFS_IDIR
	    || !parent->ops || !parent->ops->unlink)
		return -1;

	return parent->ops->unlink(parent, name);
}

int vfs_symlink(const char *target, const char *path)
{
	char dir_path[256];
	char name[64];
	struct vfs_inode *parent;

	if (!target || !path)
		return -1;

	vfs_split_path(path, dir_path, sizeof(dir_path),
		       name, sizeof(name));

	parent = vfs_resolve_path(dir_path);
	if (!parent || parent->i_type != VFS_IDIR
	    || !parent->ops || !parent->ops->symlink)
		return -1;

	return parent->ops->symlink(parent, name, target);
}

int vfs_readlink(const char *path, char *buf, uint32_t size)
{
	struct vfs_inode *inode;

	if (!path || !buf)
		return -1;

		inode = vfs_resolve_path_no_follow(path);
	if (!inode || inode->i_type != VFS_ISYMLINK
	    || !inode->ops || !inode->ops->readlink)
		return -1;

	return inode->ops->readlink(inode, buf, size);
}

int vfs_chmod(const char *path, uint16_t mode)
{
	struct vfs_inode *inode;
	uint16_t full_mode;

	if (!path)
		return -1;
	inode = vfs_resolve_path(path);
	if (!inode)
		return -1;

	/* Preserve file type bits from existing i_mode, set permission bits */
	full_mode = (inode->i_mode & S_IFMT) | (mode & S_IPERM);
	inode->i_mode = full_mode;

	if (inode->ops && inode->ops->chmod)
		return inode->ops->chmod(inode, full_mode);

	return 0;
}

int vfs_rename(const char *old_path, const char *new_path)
{
	char old_dir[256], old_name[64];
	char new_dir[256], new_name[64];
	struct vfs_inode *old_parent, *new_parent;

	if (!old_path || !new_path)
		return -1;

	vfs_split_path(old_path, old_dir, sizeof(old_dir),
		       old_name, sizeof(old_name));
	vfs_split_path(new_path, new_dir, sizeof(new_dir),
		       new_name, sizeof(new_name));

	old_parent = vfs_resolve_path(old_dir);
	new_parent = vfs_resolve_path(new_dir);

	if (!old_parent || !new_parent)
		return -1;

	/* Same filesystem with dedicated rename op */
	if (old_parent->ops == new_parent->ops
	    && old_parent->ops && old_parent->ops->rename)
		return old_parent->ops->rename(old_parent, old_name,
					       new_parent, new_name);

	/* Generic fallback: add_entry + remove_entry */
	{
		struct vfs_inode *child;
		child = old_parent->ops->lookup(old_parent, old_name);
		if (!child)
			return -1;
		if (new_parent->ops->add_entry(new_parent, new_name,
					       child) < 0)
			return -1;
		if (old_parent->ops->remove_entry(old_parent,
						  old_name) < 0) {
			new_parent->ops->remove_entry(new_parent,
						      new_name);
			return -1;
		}
	}

	return 0;
}

int vfs_access(const char *path)
{
	struct vfs_inode *inode;
	if (!path)
		return -1;
	inode = vfs_resolve_path(path);
	return inode ? 0 : -1;
}
