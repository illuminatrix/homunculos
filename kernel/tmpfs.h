#ifndef TMPFS_H
#define TMPFS_H

void tmpfs_init(void);
struct vfs_inode *tmpfs_create_mount(void);

#endif
