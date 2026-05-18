#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include "vfs.h"
#include "block.h"

#define EXT2_MAGIC           0xEF53
#define EXT2_ROOT_INO        2
#define EXT2_GOOD_OLD_INODE_SIZE 128

/* Superblock (at byte offset 1024) */
struct ext2_sb {
	uint32_t inodes_count;
	uint32_t blocks_count;
	uint32_t r_blocks_count;
	uint32_t free_blocks_count;
	uint32_t free_inodes_count;
	uint32_t first_data_block;
	uint32_t log_block_size;
	uint32_t log_frag_size;
	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;
	uint32_t mtime;
	uint32_t wtime;
	uint16_t mnt_count;
	uint16_t max_mnt_count;
	uint16_t magic;
	uint16_t state;
	uint16_t errors;
	uint16_t minor_rev;
	uint32_t lastcheck;
	uint32_t checkinterval;
	uint32_t creator_os;
	uint32_t rev_level;
	uint16_t def_resuid;
	uint16_t def_resgid;
	/* Extended (rev_level >= 1) */
	uint32_t first_ino;
	uint16_t inode_size;
	uint16_t block_group_nr;
	uint32_t feature_compat;
	uint32_t feature_incompat;
	uint32_t feature_ro_compat;
	uint8_t  uuid[16];
	char     volume_name[16];
} __attribute__((packed));

/* Block group descriptor (32 bytes) */
struct ext2_bgdesc {
	uint32_t block_bitmap;
	uint32_t inode_bitmap;
	uint32_t inode_table;
	uint16_t free_blocks_count;
	uint16_t free_inodes_count;
	uint16_t used_dirs_count;
	uint16_t pad;
	uint32_t reserved[3];
} __attribute__((packed));

/* Inode (128-byte old format) */
struct ext2_inode {
	uint16_t mode;
	uint16_t uid;
	uint32_t size;
	uint32_t atime;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t dtime;
	uint16_t gid;
	uint16_t links;
	uint32_t blocks;
	uint32_t flags;
	uint32_t osd1;
	uint32_t block[15];
	uint32_t generation;
	uint32_t file_acl;
	uint32_t dir_acl;
	uint32_t faddr;
	uint32_t osd2[3];
} __attribute__((packed));

/* Directory entry */
struct ext2_dirent {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t  name_len;
	uint8_t  file_type;
	char     name[];
} __attribute__((packed));

/* Inode mode bits for file type */
#define EXT2_S_IFMT   0xF000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFREG  0x8000

/* Filesystem context (per-mount) */
struct ext2_fs {
	struct block_device *dev;
	struct ext2_sb sb;
	uint32_t block_size;
	uint32_t bgdesc_block;      /* block containing bgdesc table */
	uint32_t bgdesc_count;
	int mounted;
};

/* Number of pointers per block (for indirection) */
#define EXT2_PTRS_PER_BLOCK(bs) ((bs) / 4)

/*
 * Mount an ext2 filesystem from a block device.
 * Returns a VFS inode for the root directory, or 0 on failure.
 * The returned inode uses ext2_vfs_inode_ops for read/readdir/lookup.
 */
struct vfs_inode *ext2_mount(struct block_device *dev);

#endif
