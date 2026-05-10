#include "vfs.h"

/* Linker symbols for .driver_init section */
extern vfs_driver_init_fn __driver_init_start[];
extern vfs_driver_init_fn __driver_init_end[];

struct file *vfs_stdin;
struct file *vfs_stdout;
struct file *vfs_stderr;

void vfs_init(void)
{
	vfs_driver_init_fn *fn;

	for (fn = __driver_init_start; fn < __driver_init_end; fn++)
		(*fn)();
}

struct file *vfs_get_stdin(void)
{
	return vfs_stdin;
}

struct file *vfs_get_stdout(void)
{
	return vfs_stdout;
}

struct file *vfs_get_stderr(void)
{
	return vfs_stderr;
}
