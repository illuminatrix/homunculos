#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdint.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define VFS_MAX_FD    16

struct file;

struct vfs_ops {
	int (*write)(struct file *file, const void *buf, size_t nbyte);
	int (*read)(struct file *file, void *buf, size_t nbyte);
};

struct file {
	const struct vfs_ops *ops;
	void *private_data;
	uint32_t pos;
};

/* Driver init via linker section */
typedef void (*vfs_driver_init_fn)(void);

#define VFS_DRIVER_INIT(fn) \
	static vfs_driver_init_fn __vfs_drv_##fn \
		__attribute__((used, section(".driver_init"))) = fn

/* VFS core API */
void vfs_init(void);

/* === VFS Inode Layer === */

#define VFS_IFILE 0
#define VFS_IDIR  1

struct vfs_dirent {
	uint32_t d_ino;
	char d_name[64];
};

struct vfs_inode;
struct vfs_inode_ops {
	int (*read)(struct vfs_inode *inode, uint32_t offset,
		    void *buf, uint32_t size);
	int (*write)(struct vfs_inode *inode, uint32_t offset,
		     const void *buf, uint32_t size);
	int (*readdir)(struct vfs_inode *dir, uint32_t index,
		       struct vfs_dirent *dent);
	struct vfs_inode *(*lookup)(struct vfs_inode *dir,
				    const char *name);
	int (*add_entry)(struct vfs_inode *dir, const char *name,
			 struct vfs_inode *entry);
};

struct vfs_inode {
	uint32_t i_no;
	uint32_t i_size;
	uint16_t i_type;
	const struct vfs_inode_ops *ops;
	void *private_data;
};

#define VFS_MAX_INODES     256
#define VFS_MAX_OPEN_FILES 16
#define VFS_MAX_DEVICES    8

struct vfs_device {
	const char *name;
	const struct vfs_ops *ops;
	void *private_data;
};

void vfs_inode_init(void);
struct vfs_inode *vfs_alloc_inode(void);
void vfs_free_inode(struct vfs_inode *inode);
struct vfs_inode *vfs_root_inode(void);

void vfs_set_root_inode(struct vfs_inode *inode);
struct vfs_inode *vfs_resolve_path(const char *path);
int vfs_register_by_path(const char *path, struct vfs_inode *inode);
int vfs_register_device(const char *name, const struct vfs_ops *ops,
			void *private_data);
void vfs_create_device_nodes(void);
struct file *vfs_open_file(struct vfs_inode *inode);
void vfs_close_file(struct file *f);

/* === Mount Abstraction === */

#define VFS_MAX_MOUNTS 8

struct vfs_mount {
	struct vfs_inode *mount_point;
	struct vfs_inode *root;
};

struct vfs_mount *vfs_mount_create(struct vfs_inode *mount_point,
				   struct vfs_inode *fs_root);
void vfs_mount_destroy(struct vfs_mount *mnt);
struct vfs_inode *vfs_resolve_mount(struct vfs_inode *inode);

#endif
