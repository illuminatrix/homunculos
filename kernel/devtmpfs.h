#ifndef DEVTMPFS_H
#define DEVTMPFS_H

void devtmpfs_init(void);
struct vfs_inode *devtmpfs_create_mount(void);

#endif
