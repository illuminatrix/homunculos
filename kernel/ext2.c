#include "ext2.h"
#include <string.h>
#include <stdio.h>

/* Shared block buffer — not reentrant, fine for single-threaded use */
static uint8_t ext2_buf[4096];

/* Forward declarations */
static int ext2_inode_read(struct vfs_inode *inode, uint32_t offset,
			   void *buf, uint32_t size);
static int ext2_readdir(struct vfs_inode *dir, uint32_t index,
			struct vfs_dirent *dent);
static struct vfs_inode *ext2_lookup(struct vfs_inode *dir,
				     const char *name);

static struct vfs_inode_ops ext2_file_ops = {
	.read      = ext2_inode_read,
	.write     = 0,
	.readdir   = 0,
	.lookup    = 0,
	.add_entry = 0,
};

static struct vfs_inode_ops ext2_dir_ops = {
	.read      = 0,
	.write     = 0,
	.readdir   = ext2_readdir,
	.lookup    = ext2_lookup,
	.add_entry = 0,
};

/* Per-inode private data for ext2 */
struct ext2_inode_data {
	uint32_t inode_no;
	struct ext2_fs *fs;
};

/* ---------------------------------------------------------------
 * Low-level block reads
 * --------------------------------------------------------------- */

static int ext2_read_block(struct ext2_fs *fs, uint32_t block_nr, void *buf)
{
	uint32_t lba;
	uint32_t sectors_per_block;

	sectors_per_block = fs->block_size / BLOCK_SECTOR_SIZE;
	lba = block_nr * sectors_per_block;

	return block_read(fs->dev, lba, sectors_per_block, buf);
}

/* ---------------------------------------------------------------
 * Block group descriptor access
 * --------------------------------------------------------------- */

static int ext2_read_bgdesc(struct ext2_fs *fs, uint32_t bg_idx,
			    struct ext2_bgdesc *bgd)
{
	uint32_t offset = bg_idx * sizeof(struct ext2_bgdesc);
	uint32_t block_nr = fs->bgdesc_block + (offset / fs->block_size);

	if (ext2_read_block(fs, block_nr, ext2_buf) < 0)
		return -1;

	offset %= fs->block_size;
	*bgd = *(const struct ext2_bgdesc *)(ext2_buf + offset);
	return 0;
}

/* ---------------------------------------------------------------
 * Inode read
 * --------------------------------------------------------------- */

static int ext2_read_inode(struct ext2_fs *fs, uint32_t inode_no,
			   struct ext2_inode *inode_out)
{
	uint32_t bg_idx;
	uint32_t inode_index;
	uint32_t inode_size;
	uint32_t inodes_per_group;
	uint32_t table_block;
	uint32_t byte_offset;
	uint32_t block_nr;

	if (inode_no == 0 || inode_no > fs->sb.inodes_count)
		return -1;

	inode_size = fs->sb.inode_size;
	if (inode_size == 0)
		inode_size = EXT2_GOOD_OLD_INODE_SIZE;

	inodes_per_group = fs->sb.inodes_per_group;
	bg_idx = (inode_no - 1) / inodes_per_group;
	inode_index = (inode_no - 1) % inodes_per_group;

	{
		struct ext2_bgdesc bgd;
		if (ext2_read_bgdesc(fs, bg_idx, &bgd) < 0)
			return -1;
		table_block = bgd.inode_table;
	}

	byte_offset = inode_index * inode_size;
	block_nr = table_block + (byte_offset / fs->block_size);

	if (ext2_read_block(fs, block_nr, ext2_buf) < 0)
		return -1;

	byte_offset %= fs->block_size;
	*inode_out = *(const struct ext2_inode *)(ext2_buf + byte_offset);
	return 0;
}

/* ---------------------------------------------------------------
 * Block pointer resolution (handles indirect blocks)
 * --------------------------------------------------------------- */

static uint32_t ext2_inode_bmap(struct ext2_fs *fs,
				const struct ext2_inode *inode,
				uint32_t block_idx)
{
	uint32_t ptrs_per_block;
	uint32_t direct_count = 12;
	uint32_t singly_count, doubly_count;
	uint32_t indirect_idx, indirect2_idx, indirect3_idx;

	ptrs_per_block = EXT2_PTRS_PER_BLOCK(fs->block_size);

	if (block_idx < direct_count)
		return inode->block[block_idx];

	block_idx -= direct_count;
	if (block_idx < ptrs_per_block) {
		if (ext2_read_block(fs, inode->block[12], ext2_buf) < 0)
			return 0;
		return ((uint32_t *)ext2_buf)[block_idx];
	}

	block_idx -= ptrs_per_block;
	singly_count = ptrs_per_block;
	doubly_count = ptrs_per_block * singly_count;

	if (block_idx < doubly_count) {
		indirect_idx = block_idx / singly_count;
		indirect2_idx = block_idx % singly_count;

		if (ext2_read_block(fs, inode->block[13], ext2_buf) < 0)
			return 0;

		if (ext2_read_block(fs,
				    ((uint32_t *)ext2_buf)[indirect_idx],
				    ext2_buf) < 0)
			return 0;
		return ((uint32_t *)ext2_buf)[indirect2_idx];
	}

	block_idx -= doubly_count;
	if (block_idx < ptrs_per_block * ptrs_per_block * ptrs_per_block) {
		indirect_idx = block_idx / (singly_count * singly_count);
		block_idx %= (singly_count * singly_count);
		indirect2_idx = block_idx / singly_count;
		indirect3_idx = block_idx % singly_count;

		if (ext2_read_block(fs, inode->block[14], ext2_buf) < 0)
			return 0;

		if (ext2_read_block(fs,
				    ((uint32_t *)ext2_buf)[indirect_idx],
				    ext2_buf) < 0)
			return 0;

		if (ext2_read_block(fs,
				    ((uint32_t *)ext2_buf)[indirect2_idx],
				    ext2_buf) < 0)
			return 0;
		return ((uint32_t *)ext2_buf)[indirect3_idx];
	}

	return 0;
}

/* ---------------------------------------------------------------
 * File inode read
 * --------------------------------------------------------------- */

static int ext2_inode_read(struct vfs_inode *inode, uint32_t offset,
			   void *buf, uint32_t size)
{
	struct ext2_inode_data *data;
	struct ext2_fs *fs;
	struct ext2_inode raw;
	uint32_t file_size;
	uint32_t block_size;
	uint32_t pos;
	uint32_t block_idx, block_offset, to_copy;
	uint8_t *dst = (uint8_t *)buf;

	data = (struct ext2_inode_data *)inode->private_data;
	if (!data)
		return -1;
	fs = data->fs;

	if (ext2_read_inode(fs, data->inode_no, &raw) < 0)
		return -1;

	file_size = raw.size;
	block_size = fs->block_size;

	if (offset >= file_size)
		return 0;
	if (offset + size > file_size)
		size = file_size - offset;

	pos = offset;
	while (size > 0) {
		block_idx = pos / block_size;
		block_offset = pos % block_size;

		uint32_t phys_block = ext2_inode_bmap(fs, &raw, block_idx);
		if (phys_block == 0) {
			memset(ext2_buf, 0, block_size);
		} else {
			if (ext2_read_block(fs, phys_block, ext2_buf) < 0)
				break;
		}

		to_copy = block_size - block_offset;
		if (to_copy > size)
			to_copy = size;

		memcpy(dst, ext2_buf + block_offset, to_copy);
		dst += to_copy;
		pos += to_copy;
		size -= to_copy;
	}

	return (int)(pos - offset);
}

/* ---------------------------------------------------------------
 * ext2_inode_data pool
 * --------------------------------------------------------------- */

static struct ext2_inode_data ext2_data_pool[64];
static int ext2_data_pool_next;

static struct ext2_inode_data *ext2_alloc_data(void)
{
	if (ext2_data_pool_next >= 64)
		return 0;
	return &ext2_data_pool[ext2_data_pool_next++];
}

/* ---------------------------------------------------------------
 * Directory operations
 * --------------------------------------------------------------- */

static int ext2_readdir(struct vfs_inode *dir, uint32_t index,
			struct vfs_dirent *dent)
{
	struct ext2_inode_data *data;
	struct ext2_fs *fs;
	struct ext2_inode raw;
	uint32_t offset, block_size;

	data = (struct ext2_inode_data *)dir->private_data;
	if (!data)
		return -1;
	fs = data->fs;

	if (ext2_read_inode(fs, data->inode_no, &raw) < 0)
		return -1;

	block_size = fs->block_size;

	offset = 0;
	while (offset < raw.size) {
		uint32_t block_idx = offset / block_size;
		uint32_t block_off = offset % block_size;

		uint32_t phys_block = ext2_inode_bmap(fs, &raw, block_idx);
		if (phys_block == 0) {
			offset += block_size - block_off;
			continue;
		}

		if (ext2_read_block(fs, phys_block, ext2_buf) < 0)
			return -1;

		while (block_off < block_size) {
			const struct ext2_dirent *de;
			de = (const struct ext2_dirent *)(ext2_buf + block_off);

			if (de->rec_len == 0)
				break;
			if (de->inode == 0) {
				block_off += de->rec_len;
				offset += de->rec_len;
				continue;
			}

			if (index == 0) {
				uint32_t name_len = de->name_len;
				if (name_len > 63)
					name_len = 63;
				dent->d_ino = de->inode;
				memcpy(dent->d_name, de->name, name_len);
				dent->d_name[name_len] = '\0';
				return 0;
			}

			index--;
			block_off += de->rec_len;
			offset += de->rec_len;
		}

		if (block_off >= block_size)
			offset = (block_idx + 1) * block_size;
	}

	return -1;
}

static struct vfs_inode *ext2_lookup(struct vfs_inode *dir, const char *name)
{
	struct ext2_inode_data *data;
	struct ext2_fs *fs;
	struct ext2_inode raw;
	uint32_t offset, block_size;

	data = (struct ext2_inode_data *)dir->private_data;
	if (!data)
		return 0;
	fs = data->fs;

	if (ext2_read_inode(fs, data->inode_no, &raw) < 0)
		return 0;

	block_size = fs->block_size;

	offset = 0;
	while (offset < raw.size) {
		uint32_t block_idx = offset / block_size;
		uint32_t block_off = offset % block_size;

		uint32_t phys_block = ext2_inode_bmap(fs, &raw, block_idx);
		if (phys_block == 0) {
			offset += block_size - block_off;
			continue;
		}

		if (ext2_read_block(fs, phys_block, ext2_buf) < 0)
			return 0;

		while (block_off < block_size) {
			const struct ext2_dirent *de;
			de = (const struct ext2_dirent *)(ext2_buf + block_off);

			if (de->rec_len == 0)
				break;
			if (de->inode == 0) {
				block_off += de->rec_len;
				offset += de->rec_len;
				continue;
			}

			if (de->name_len == strlen(name) &&
			    memcmp(de->name, name, de->name_len) == 0) {
				struct vfs_inode *child;
				struct ext2_inode_data *child_data;
				struct ext2_inode raw_child;
				uint32_t found_inode;

				/* Save before ext2_read_inode overwrites ext2_buf */
				found_inode = de->inode;

				if (ext2_read_inode(fs, found_inode,
						    &raw_child) < 0)
					return 0;

				child_data = ext2_alloc_data();
				if (!child_data)
					return 0;

				child = vfs_alloc_inode();
				if (!child)
					return 0;

				child->i_no = found_inode;
				child_data->inode_no = found_inode;
				child_data->fs = fs;
				child->private_data = child_data;
				child->i_size = raw_child.size;

				if ((raw_child.mode & EXT2_S_IFMT) ==
				    EXT2_S_IFDIR) {
					child->i_type = VFS_IDIR;
					child->ops = &ext2_dir_ops;
				} else {
					child->i_type = VFS_IFILE;
					child->ops = &ext2_file_ops;
				}

				return child;
			}

			block_off += de->rec_len;
			offset += de->rec_len;
		}

		if (block_off >= block_size)
			offset = (block_idx + 1) * block_size;
	}

	return 0;
}

/* ---------------------------------------------------------------
 * Mount
 * --------------------------------------------------------------- */

struct vfs_inode *ext2_mount(struct block_device *dev)
{
	struct ext2_fs *fs;
	struct vfs_inode *root;
	struct ext2_inode_data *root_data;
	struct ext2_inode raw_root;

	if (!dev)
		return 0;

#define EXT2_MAX_FS 1
	static struct ext2_fs fs_pool[EXT2_MAX_FS];
	static int fs_pool_next;

	if (fs_pool_next >= EXT2_MAX_FS)
		return 0;

	fs = &fs_pool[fs_pool_next++];

	/* Read superblock at byte offset 1024 (LBA 2 for 512-byte sectors) */
	if (block_read(dev, 2, 1, ext2_buf) < 0)
		return 0;

	fs->sb = *(const struct ext2_sb *)ext2_buf;

	if (fs->sb.magic != EXT2_MAGIC)
		return 0;

	fs->dev = dev;
	fs->block_size = 1024 << fs->sb.log_block_size;

	fs->bgdesc_count = (fs->sb.blocks_count + fs->sb.blocks_per_group - 1)
		/ fs->sb.blocks_per_group;

	if (fs->block_size == 1024)
		fs->bgdesc_block = 2;
	else
		fs->bgdesc_block = 1;

	root = vfs_alloc_inode();
	if (!root)
		return 0;

	root_data = ext2_alloc_data();
	if (!root_data) {
		vfs_free_inode(root);
		return 0;
	}

	root_data->inode_no = EXT2_ROOT_INO;
	root_data->fs = fs;

	root->i_type = VFS_IDIR;
	root->ops = &ext2_dir_ops;
	root->private_data = root_data;

	if (ext2_read_inode(fs, EXT2_ROOT_INO, &raw_root) < 0) {
		vfs_free_inode(root);
		return 0;
	}
	root->i_size = raw_root.size;

	return root;
}
