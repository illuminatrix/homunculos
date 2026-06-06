#ifndef DEVTMPFS_H
#define DEVTMPFS_H

#include <stdint.h>

struct vfs_inode;
struct vfs_ops;
struct vfs_inode_ops;

/* Device registration — done during VFS_DRIVER_INIT, no inodes created yet */

/* Register a device with file-level (vfs_ops) interface, e.g. VGA, KBD, block devices */
void devtmpfs_register_vfs(const char *name,
			   const struct vfs_ops *ops,
			   void *private_data);

/* Register a device with inode-level (vfs_inode_ops) interface, e.g. hello, null, tty */
void devtmpfs_register_inode(const char *name,
			     const struct vfs_inode_ops *ops,
			     void *private_data,
			     uint16_t i_type);

/* Create all registered device nodes under /dev in the current VFS tree */
void devtmpfs_create_nodes(void);

void devtmpfs_init(void);
struct vfs_inode *devtmpfs_create_mount(void);

#endif
