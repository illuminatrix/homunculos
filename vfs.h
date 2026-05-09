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
};

/* Driver init via linker section */
typedef void (*vfs_driver_init_fn)(void);

#define VFS_DRIVER_INIT(fn) \
	static vfs_driver_init_fn __vfs_drv_##fn \
		__attribute__((used, section(".driver_init"))) = fn

/* Global file pointers (set by drivers during init) */
extern struct file *vfs_stdout;
extern struct file *vfs_stderr;

/* VFS core API */
void vfs_init(void);
struct file *vfs_get_stdout(void);
struct file *vfs_get_stderr(void);

#endif
