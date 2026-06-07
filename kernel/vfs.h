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
	int (*ioctl)(struct file *file, int cmd, void *arg);
	int (*close)(struct file *file);
};

struct file {
	const struct vfs_ops *ops;
	void *private_data;
	uint32_t pos;
	int refcount;
	int flags;
};

/* Driver init via linker section */
typedef void (*vfs_driver_init_fn)(void);

#define VFS_DRIVER_INIT(fn) \
	static vfs_driver_init_fn __vfs_drv_##fn \
		__attribute__((used, section(".driver_init"))) = fn

/* VFS core API */
void vfs_init(void);

/* Kernel-side struct stat (matches libc <sys/stat.h> layout) */
struct vfs_stat {
	unsigned long st_dev;
	unsigned long st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	unsigned long st_rdev;
	unsigned long st_size;
	unsigned long st_blksize;
	unsigned long st_blocks;
	unsigned long st_atime;
	unsigned long st_mtime;
	unsigned long st_ctime;
};

/* === VFS Inode Layer === */

#define VFS_IFILE    0
#define VFS_IDIR     1
#define VFS_ISYMLINK 2

/* File type mode bits (POSIX-compatible, matches EXT2_S_IFxxx) */
#define S_IFMT   0xF000  /* type mask */
#define S_IFDIR  0x4000  /* directory */
#define S_IFREG  0x8000  /* regular file */
#define S_IFLNK  0xA000  /* symbolic link */
#define S_IPERM  0x0FFF  /* permission bits mask */

/* Permission bits (POSIX-compatible) */
#define S_IRWXU  00700  /* owner rwx */
#define S_IRUSR  00400  /* owner r */
#define S_IWUSR  00200  /* owner w */
#define S_IXUSR  00100  /* owner x */
#define S_IRWXG  00070  /* group rwx */
#define S_IRGRP  00040  /* group r */
#define S_IWGRP  00020  /* group w */
#define S_IXGRP  00010  /* group x */
#define S_IRWXO  00007  /* other rwx */
#define S_IROTH  00004  /* other r */
#define S_IWOTH  00002  /* other w */
#define S_IXOTH  00001  /* other x */

/* Open flags (Linux-compatible) */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT    64
#define O_TRUNC   512
#define O_APPEND 1024

struct vfs_dirent {
	uint32_t d_ino;
	char d_name[64];
	uint8_t d_type;
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
	int (*remove_entry)(struct vfs_inode *dir, const char *name);
	int (*mkdir)(struct vfs_inode *parent, const char *name);
	int (*rmdir)(struct vfs_inode *parent, const char *name);
	int (*unlink)(struct vfs_inode *parent, const char *name);
	int (*symlink)(struct vfs_inode *parent, const char *name,
		       const char *target);
	int (*readlink)(struct vfs_inode *inode, char *buf,
			uint32_t size);
	int (*ioctl)(struct vfs_inode *inode, int cmd, void *arg);
	int (*chmod)(struct vfs_inode *inode, uint16_t mode);
	int (*link)(struct vfs_inode *parent, const char *name,
		     struct vfs_inode *existing);
	int (*rename)(struct vfs_inode *old_parent, const char *old_name,
		      struct vfs_inode *new_parent, const char *new_name);
};

struct vfs_inode {
	uint32_t i_no;
	uint32_t i_size;
	uint16_t i_type;
	uint16_t i_mode;
	const struct vfs_inode_ops *ops;
	void *private_data;
};

#define VFS_MAX_INODES     256
#define VFS_MAX_OPEN_FILES 16

void vfs_inode_init(void);
struct vfs_inode *vfs_alloc_inode(void);
void vfs_free_inode(struct vfs_inode *inode);
struct vfs_inode *vfs_root_inode(void);

void vfs_set_root_inode(struct vfs_inode *inode);
struct vfs_inode *vfs_resolve_path(const char *path);
struct vfs_inode *vfs_resolve_path_no_follow(const char *path);
int vfs_register_by_path(const char *path, struct vfs_inode *inode);
void vfs_inode_stat(struct vfs_inode *inode, struct vfs_stat *buf);
struct file *vfs_alloc_file(void);
struct file *vfs_open_file(struct vfs_inode *inode);
struct file *vfs_open_file_flags(struct vfs_inode *inode, int flags);
void vfs_close_file(struct file *f);
struct vfs_inode *vfs_create_file(const char *path);
int vfs_mkdir(const char *path);
int vfs_rmdir(const char *path);
int vfs_unlink(const char *path);
int vfs_symlink(const char *target, const char *path);
int vfs_link(const char *old_path, const char *new_path);
int vfs_readlink(const char *path, char *buf, uint32_t size);
int vfs_access(const char *path);
int vfs_chmod(const char *path, uint16_t mode);
int vfs_inode_chmod(struct vfs_inode *inode, uint16_t mode);
int vfs_rename(const char *old_path, const char *new_path);
void vfs_split_path(const char *path, char *dir_out, int dir_size,
		    char *name_out, int name_size);

/* === Mount Abstraction === */

#define VFS_MAX_MOUNTS 8

struct vfs_mount {
	struct vfs_inode *mount_point;
	struct vfs_inode *root;
};

struct vfs_mount *vfs_mount_create(struct vfs_inode *mount_point,
				   struct vfs_inode *fs_root);
void vfs_mount_destroy(struct vfs_mount *mnt);
int vfs_unmount_path(const char *path);
struct vfs_inode *vfs_resolve_mount(struct vfs_inode *inode);

#endif
