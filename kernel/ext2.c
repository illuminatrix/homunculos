#include "ext2.h"
#include <string.h>
#include <stdio.h>

/* Shared block buffer — not reentrant, fine for single-threaded use */
static uint8_t ext2_buf[4096];

/* Forward declarations */
static int ext2_inode_read(struct vfs_inode *inode, uint32_t offset,
			   void *buf, uint32_t size);
static int ext2_inode_write(struct vfs_inode *inode, uint32_t offset,
			    const void *buf, uint32_t size);
static int ext2_readdir(struct vfs_inode *dir, uint32_t index,
			struct vfs_dirent *dent);
static struct vfs_inode *ext2_lookup(struct vfs_inode *dir,
				     const char *name);
static int ext2_add_entry(struct vfs_inode *dir, const char *name,
			  struct vfs_inode *entry);
static int ext2_remove_entry(struct vfs_inode *dir, const char *name);
static int ext2_mkdir(struct vfs_inode *parent, const char *name);
static int ext2_unlink(struct vfs_inode *parent, const char *name);
static int ext2_rmdir(struct vfs_inode *parent, const char *name);
static int ext2_symlink(struct vfs_inode *parent, const char *name,
			const char *target);
static int ext2_mknod(struct vfs_inode *parent, const char *name,
		      uint16_t mode, dev_t dev);
static int ext2_readlink_op(struct vfs_inode *inode, char *buf,
			    uint32_t size);
static int ext2_chmod(struct vfs_inode *inode, uint16_t mode);
static int ext2_rename(struct vfs_inode *old_parent, const char *old_name,
		       struct vfs_inode *new_parent, const char *new_name);
static int ext2_link(struct vfs_inode *parent, const char *name,
		     struct vfs_inode *existing);
static int ext2_truncate(struct vfs_inode *inode, uint32_t length);

static struct vfs_inode_ops ext2_file_ops = {
	.read      = ext2_inode_read,
	.write     = ext2_inode_write,
	.readdir   = 0,
	.lookup    = 0,
	.add_entry = 0,
	.remove_entry = 0,
	.mkdir     = 0,
	.rmdir     = 0,
	.unlink    = 0,
	.symlink   = 0,
	.link      = 0,
	.readlink  = 0,
	.chmod     = ext2_chmod,
	.truncate  = ext2_truncate,
};

static struct vfs_inode_ops ext2_dir_ops = {
	.read      = 0,
	.write     = 0,
	.readdir   = ext2_readdir,
	.lookup    = ext2_lookup,
	.add_entry = ext2_add_entry,
	.remove_entry = ext2_remove_entry,
	.mkdir     = ext2_mkdir,
	.rmdir     = ext2_rmdir,
	.unlink    = ext2_unlink,
	.symlink   = ext2_symlink,
	.mknod     = ext2_mknod,
	.link      = ext2_link,
	.readlink  = 0,
	.chmod     = ext2_chmod,
	.rename    = ext2_rename,
};

static struct vfs_inode_ops ext2_symlink_ops = {
	.read      = 0,
	.write     = 0,
	.readdir   = 0,
	.lookup    = 0,
	.add_entry = 0,
	.remove_entry = 0,
	.mkdir     = 0,
	.rmdir     = 0,
	.unlink    = 0,
	.symlink   = 0,
	.link      = 0,
	.readlink  = ext2_readlink_op,
};

/* Per-inode private data for ext2 */
struct ext2_inode_data {
	uint32_t inode_no;
	struct ext2_fs *fs;
};

/* ---------------------------------------------------------------
 * Low-level block reads / writes
 * --------------------------------------------------------------- */

static int ext2_read_block(struct ext2_fs *fs, uint32_t block_nr, void *buf)
{
	uint32_t lba;
	uint32_t sectors_per_block;

	sectors_per_block = fs->block_size / BLOCK_SECTOR_SIZE;
	lba = block_nr * sectors_per_block;

	return block_read(fs->dev, lba, sectors_per_block, buf);
}

static int ext2_write_block(struct ext2_fs *fs, uint32_t block_nr,
			    const void *buf)
{
	uint32_t lba;
	uint32_t sectors_per_block;

	sectors_per_block = fs->block_size / BLOCK_SECTOR_SIZE;
	lba = block_nr * sectors_per_block;

	return block_write(fs->dev, lba, sectors_per_block, buf);
}

/* ---------------------------------------------------------------
 * Superblock / BGdesc write
 * --------------------------------------------------------------- */

static int ext2_write_sb(struct ext2_fs *fs)
{
	uint32_t sb_block = (fs->block_size == 1024) ? 1 : 0;

	memcpy(ext2_buf, &fs->sb, sizeof(fs->sb));
	return ext2_write_block(fs, sb_block, ext2_buf);
}

static int ext2_write_bgdesc(struct ext2_fs *fs, uint32_t bg_idx,
			     const struct ext2_bgdesc *bgd)
{
	uint32_t offset = bg_idx * sizeof(struct ext2_bgdesc);
	uint32_t block_nr = fs->bgdesc_block + (offset / fs->block_size);

	if (ext2_read_block(fs, block_nr, ext2_buf) < 0)
		return -1;

	offset %= fs->block_size;
	*(struct ext2_bgdesc *)(ext2_buf + offset) = *bgd;

	return ext2_write_block(fs, block_nr, ext2_buf);
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
 * Block allocator / deallocator
 * --------------------------------------------------------------- */

static uint32_t ext2_alloc_block(struct ext2_fs *fs)
{
	uint32_t bg_count = fs->bgdesc_count;
	uint32_t blocks_per_group = fs->sb.blocks_per_group;
	uint32_t i, bg;

	for (bg = 0; bg < bg_count; bg++) {
		struct ext2_bgdesc bgd;

		if (ext2_read_bgdesc(fs, bg, &bgd) < 0)
			continue;
		if (bgd.free_blocks_count == 0)
			continue;

		/* Read block bitmap */
		if (ext2_read_block(fs, bgd.block_bitmap, ext2_buf) < 0)
			continue;

		/* Scan for a free (0) bit */
		uint32_t max_bits = blocks_per_group;
		if (bg == bg_count - 1)
			max_bits = fs->sb.blocks_count - bg * blocks_per_group;
		for (i = 0; i < max_bits; i++) {
			uint32_t byte = i / 8;
			uint32_t bit = i % 8;
			if (!(ext2_buf[byte] & (1 << bit))) {
				/* Allocate the block */
				ext2_buf[byte] |= (1 << bit);
				if (ext2_write_block(fs, bgd.block_bitmap,
						     ext2_buf) < 0)
					return 0;

				bgd.free_blocks_count--;
				if (ext2_write_bgdesc(fs, bg, &bgd) < 0)
					return 0;

				fs->sb.free_blocks_count--;
				ext2_write_sb(fs);

				return bg * blocks_per_group + i;
			}
		}
	}
	return 0;
}

static void ext2_free_block(struct ext2_fs *fs, uint32_t block_nr)
{
	uint32_t blocks_per_group = fs->sb.blocks_per_group;
	uint32_t bg = block_nr / blocks_per_group;
	uint32_t offset = block_nr % blocks_per_group;
	struct ext2_bgdesc bgd;

	if (ext2_read_bgdesc(fs, bg, &bgd) < 0)
		return;
	if (ext2_read_block(fs, bgd.block_bitmap, ext2_buf) < 0)
		return;

	ext2_buf[offset / 8] &= ~(1 << (offset % 8));
	ext2_write_block(fs, bgd.block_bitmap, ext2_buf);

	bgd.free_blocks_count++;
	ext2_write_bgdesc(fs, bg, &bgd);

	fs->sb.free_blocks_count++;
	ext2_write_sb(fs);
}

/* ---------------------------------------------------------------
 * Inode allocator / deallocator
 * --------------------------------------------------------------- */

static uint32_t ext2_alloc_inode(struct ext2_fs *fs)
{
	uint32_t bg_count = fs->bgdesc_count;
	uint32_t inodes_per_group = fs->sb.inodes_per_group;
	uint32_t i, bg;

	for (bg = 0; bg < bg_count; bg++) {
		struct ext2_bgdesc bgd;

		if (ext2_read_bgdesc(fs, bg, &bgd) < 0)
			continue;
		if (bgd.free_inodes_count == 0)
			continue;
		if (ext2_read_block(fs, bgd.inode_bitmap, ext2_buf) < 0)
			continue;

		uint32_t max_bits = inodes_per_group;
		if (bg == bg_count - 1)
			max_bits = fs->sb.inodes_count - bg * inodes_per_group;
		for (i = 0; i < max_bits; i++) {
			uint32_t byte = i / 8;
			uint32_t bit = i % 8;
			if (!(ext2_buf[byte] & (1 << bit))) {
				ext2_buf[byte] |= (1 << bit);
				if (ext2_write_block(fs, bgd.inode_bitmap,
						     ext2_buf) < 0)
					return 0;

				bgd.free_inodes_count--;
				ext2_write_bgdesc(fs, bg, &bgd);

				fs->sb.free_inodes_count--;
				ext2_write_sb(fs);

				return bg * inodes_per_group + i + 1;
			}
		}
	}
	return 0;
}

static void ext2_free_inode(struct ext2_fs *fs, uint32_t inode_no)
{
	if (inode_no == 0)
		return;
	uint32_t inodes_per_group = fs->sb.inodes_per_group;
	uint32_t bg = (inode_no - 1) / inodes_per_group;
	uint32_t offset = (inode_no - 1) % inodes_per_group;
	struct ext2_bgdesc bgd;

	if (ext2_read_bgdesc(fs, bg, &bgd) < 0)
		return;
	if (ext2_read_block(fs, bgd.inode_bitmap, ext2_buf) < 0)
		return;

	ext2_buf[offset / 8] &= ~(1 << (offset % 8));
	ext2_write_block(fs, bgd.inode_bitmap, ext2_buf);

	bgd.free_inodes_count++;
	ext2_write_bgdesc(fs, bg, &bgd);

	fs->sb.free_inodes_count++;
	ext2_write_sb(fs);
}

/* ---------------------------------------------------------------
 * Inode write
 * --------------------------------------------------------------- */

static int ext2_write_inode(struct ext2_fs *fs, uint32_t inode_no,
			    const struct ext2_inode *inode)
{
	uint32_t bg_idx, inode_index;
	uint32_t inode_size;
	uint32_t inodes_per_group;
	uint32_t table_block, byte_offset, block_nr;

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
	*(struct ext2_inode *)(ext2_buf + byte_offset) = *inode;

	return ext2_write_block(fs, block_nr, ext2_buf);
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
 * Block pointer assignment (handles indirect blocks)
 * --------------------------------------------------------------- */

static int ext2_set_bmap(struct ext2_fs *fs, struct ext2_inode *inode,
			 uint32_t block_idx, uint32_t phys_block)
{
	uint32_t ptrs_per_block = EXT2_PTRS_PER_BLOCK(fs->block_size);

	if (block_idx < 12) {
		inode->block[block_idx] = phys_block;
		return 0;
	}

	block_idx -= 12;
	if (block_idx < ptrs_per_block) {
		uint32_t indir_block = inode->block[12];
		if (indir_block == 0) {
			if (phys_block == 0)
				return 0;
			indir_block = ext2_alloc_block(fs);
			if (indir_block == 0)
				return -1;
			inode->block[12] = indir_block;
		}
		if (ext2_read_block(fs, indir_block, ext2_buf) < 0)
			return -1;
		((uint32_t *)ext2_buf)[block_idx] = phys_block;
		return ext2_write_block(fs, indir_block, ext2_buf);
	}

	/* Double/triple indirect not implemented */
	return -1;
}

/* ---------------------------------------------------------------
 * File inode write (write data to an inode, allocating blocks)
 * --------------------------------------------------------------- */

static int ext2_inode_write(struct vfs_inode *inode, uint32_t offset,
			    const void *buf, uint32_t size)
{
	struct ext2_inode_data *data;
	struct ext2_fs *fs;
	struct ext2_inode raw;
	uint32_t block_size;
	uint32_t pos, block_idx, block_offset, to_copy;
	const uint8_t *src = (const uint8_t *)buf;

	if (inode->i_type == VFS_IFCHR || inode->i_type == VFS_IFBLK)
		return -1;

	data = (struct ext2_inode_data *)inode->private_data;
	if (!data)
		return -1;
	fs = data->fs;

	if (ext2_read_inode(fs, data->inode_no, &raw) < 0)
		return -1;

	block_size = fs->block_size;

	/* Truncation: if offset == 0 and buf == 0 / size == 0, free all */
	if (offset == 0 && size == 0) {
		uint32_t old_blocks = (raw.size + block_size - 1) / block_size;
		uint32_t i;
		for (i = 0; i < old_blocks; i++) {
			uint32_t b = ext2_inode_bmap(fs, &raw, i);
			if (b) {
				ext2_set_bmap(fs, &raw, i, 0);
				ext2_free_block(fs, b);
			}
		}
		raw.size = 0;
		raw.blocks = 0;
		/* Write inode back */
		ext2_write_inode(fs, data->inode_no, &raw);
		inode->i_size = 0;
		return 0;
	}

	pos = offset;
	while (size > 0) {
		block_idx = pos / block_size;
		block_offset = pos % block_size;

		uint32_t phys_block = ext2_inode_bmap(fs, &raw, block_idx);
		if (phys_block == 0) {
			phys_block = ext2_alloc_block(fs);
			if (phys_block == 0)
				break;
			if (ext2_set_bmap(fs, &raw, block_idx,
					  phys_block) < 0) {
				ext2_free_block(fs, phys_block);
				break;
			}
		}

		if (block_offset == 0 && size >= block_size) {
			/* Full block write */
			if (ext2_write_block(fs, phys_block, src) < 0)
				break;
			to_copy = block_size;
		} else {
			/* Partial block: read-modify-write */
			if (ext2_read_block(fs, phys_block, ext2_buf) < 0)
				break;
			to_copy = block_size - block_offset;
			if (to_copy > size)
				to_copy = size;
			memcpy(ext2_buf + block_offset, src, to_copy);
			if (ext2_write_block(fs, phys_block, ext2_buf) < 0)
				break;
		}

		src += to_copy;
		pos += to_copy;
		size -= to_copy;
	}

	uint32_t written = pos - offset;
	if (written > 0) {
		if (pos > raw.size)
			raw.size = pos;
		raw.blocks = (raw.size + block_size - 1) / block_size
			     * (block_size / 512);
		ext2_write_inode(fs, data->inode_no, &raw);
		inode->i_size = raw.size;
	}

	return (int)written;
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

	if (inode->i_type == VFS_IFCHR || inode->i_type == VFS_IFBLK)
		return -1;

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
 * File inode truncate
 * --------------------------------------------------------------- */

static int ext2_truncate(struct vfs_inode *inode, uint32_t length)
{
	struct ext2_inode_data *data;
	struct ext2_fs *fs;
	struct ext2_inode raw;
	uint32_t block_size;
	uint32_t old_block_count, new_block_count;

	if (inode->i_type == VFS_IFCHR || inode->i_type == VFS_IFBLK)
		return -1;

	data = (struct ext2_inode_data *)inode->private_data;
	if (!data)
		return -1;
	fs = data->fs;

	if (ext2_read_inode(fs, data->inode_no, &raw) < 0)
		return -1;

	block_size = fs->block_size;

	/* Extending beyond current size — just update size */
	if (length >= raw.size) {
		raw.size = length;
		raw.blocks = (length + block_size - 1) / block_size
			     * (block_size / 512);
		ext2_write_inode(fs, data->inode_no, &raw);
		inode->i_size = length;
		return 0;
	}

	/* Free blocks beyond the new size */
	old_block_count = (raw.size + block_size - 1) / block_size;
	new_block_count = length ? (length + block_size - 1) / block_size : 0;

	uint32_t i;
	for (i = new_block_count; i < old_block_count; i++) {
		uint32_t b = ext2_inode_bmap(fs, &raw, i);
		if (b) {
			ext2_set_bmap(fs, &raw, i, 0);
			ext2_free_block(fs, b);
		}
	}

	raw.size = length;
	raw.blocks = new_block_count * (block_size / 512);
	ext2_write_inode(fs, data->inode_no, &raw);
	inode->i_size = length;
	return 0;
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
				dent->d_type = (de->file_type == 2) ? 4 : 8;
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
				child->i_mode = raw_child.mode;

				if ((raw_child.mode & EXT2_S_IFMT) ==
				    EXT2_S_IFDIR) {
					child->i_type = VFS_IDIR;
					child->ops = &ext2_dir_ops;
				} else if ((raw_child.mode & EXT2_S_IFMT) ==
					   EXT2_S_IFLNK) {
					child->i_type = VFS_ISYMLINK;
					child->ops = &ext2_symlink_ops;
				} else if ((raw_child.mode & EXT2_S_IFMT) ==
					   EXT2_S_IFCHR) {
					child->i_type = VFS_IFCHR;
					child->ops = &ext2_file_ops;
					child->i_rdev = raw_child.block[0];
				} else if ((raw_child.mode & EXT2_S_IFMT) ==
					   EXT2_S_IFBLK) {
					child->i_type = VFS_IFBLK;
					child->ops = &ext2_file_ops;
					child->i_rdev = raw_child.block[0];
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
 * Directory entry operations
 * --------------------------------------------------------------- */

static int ext2_add_entry(struct vfs_inode *dir, const char *name,
			  struct vfs_inode *entry)
{
	struct ext2_inode_data *data;
	struct ext2_fs *fs;
	struct ext2_inode raw;
	uint32_t block_size;
	uint32_t offset;
	int name_len = strlen(name);
	int need_rec_len = sizeof(struct ext2_dirent) + name_len;
	/* Round up to 4-byte alignment */
	need_rec_len = (need_rec_len + 3) & ~3;
	if (need_rec_len < 12)
		need_rec_len = 12;

	data = (struct ext2_inode_data *)dir->private_data;
	if (!data) {
		printf("ext2_add_entry: no dir private_data\n");
		return -1;
	}
	fs = data->fs;

	if (ext2_read_inode(fs, data->inode_no, &raw) < 0) {
		printf("ext2_add_entry: read_inode failed\n");
		return -1;
	}

	block_size = fs->block_size;

	/* If entry is not yet backed by ext2, allocate an inode for it */
	if (entry && entry->private_data == 0) {
		uint32_t new_inode_no = ext2_alloc_inode(fs);
		if (new_inode_no == 0) {
			printf("ext2_add_entry: alloc_inode failed\n");
			return -1;
		}

		struct ext2_inode new_raw;
		memset(&new_raw, 0, sizeof(new_raw));
		new_raw.mode = entry->i_mode;
		new_raw.uid = 0;
		new_raw.gid = 0;
		new_raw.size = 0;
		new_raw.links = 1;
		new_raw.blocks = 0;

		if (ext2_write_inode(fs, new_inode_no, &new_raw) < 0) {
			printf("ext2_add_entry: write_inode failed\n");
			ext2_free_inode(fs, new_inode_no);
			return -1;
		}

		struct ext2_inode_data *child_data = ext2_alloc_data();
		if (!child_data) {
			printf("ext2_add_entry: alloc_data failed\n");
			ext2_free_inode(fs, new_inode_no);
			return -1;
		}
		child_data->inode_no = new_inode_no;
		child_data->fs = fs;
		entry->private_data = child_data;
		entry->i_no = new_inode_no;
		entry->ops = &ext2_file_ops;
	}

	uint32_t entry_inode;
	{
		struct ext2_inode_data *ed;
		ed = (struct ext2_inode_data *)entry->private_data;
		entry_inode = ed->inode_no;
	}

	/* Scan directory for space to insert the entry */
	offset = 0;
	while (offset < raw.size) {
		uint32_t block_idx = offset / block_size;
		uint32_t block_off = offset % block_size;

		uint32_t phys_block = ext2_inode_bmap(fs, &raw, block_idx);
		if (phys_block == 0) {
			printf("ext2_add_entry: block %d missing\n", block_idx);
			offset += block_size - block_off;
			continue;
		}

		if (ext2_read_block(fs, phys_block, ext2_buf) < 0) {
			printf("ext2_add_entry: read_block %d failed\n", phys_block);
			return -1;
		}

		while (block_off < block_size) {
			struct ext2_dirent *de;
			de = (struct ext2_dirent *)(ext2_buf + block_off);

			if (de->rec_len == 0)
				break;

			uint32_t used = sizeof(struct ext2_dirent)
					+ de->name_len;
			used = (used + 3) & ~3;
			uint32_t slack = de->rec_len - used;

			if (de->inode == 0 && slack >= (uint32_t)need_rec_len) {
				/* Free entry with enough space */
				de->inode = entry_inode;
				de->name_len = name_len;
				de->file_type = 1; /* regular file */
				memcpy(de->name, name, name_len);
				if (slack > (uint32_t)need_rec_len) {
					/* Split the entry */
					struct ext2_dirent *next;
					next = (struct ext2_dirent *)
						(ext2_buf + block_off
						 + need_rec_len);
					next->inode = 0;
					next->rec_len = de->rec_len
						 - need_rec_len;
					next->name_len = 0;
					de->rec_len = need_rec_len;
				}
				raw.size += need_rec_len;
				raw.blocks = (raw.size + block_size - 1)
					     / block_size * (block_size / 512);
				dir->i_size = raw.size;
				if (ext2_write_block(fs, phys_block,
						     ext2_buf) < 0)
					return -1;
				return ext2_write_inode(fs, data->inode_no,
							&raw);
			}

			if (de->rec_len > need_rec_len
			    && de->inode != 0) {
				/* Check if there's enough slack to split */
				if ((int)(de->rec_len - used) < (int)need_rec_len) {
					block_off += de->rec_len;
					offset += de->rec_len;
					continue;
				}
				/* Split this entry */
				uint32_t old_rec_len = de->rec_len;
				de->rec_len = used;

				struct ext2_dirent *new_de;
				new_de = (struct ext2_dirent *)
					(ext2_buf + block_off + used);
				new_de->inode = entry_inode;
				new_de->rec_len = old_rec_len - used;
				new_de->name_len = name_len;
				new_de->file_type = 1; /* regular file */
				memcpy(new_de->name, name, name_len);

				int _wr;
				_wr = ext2_write_block(fs, phys_block,
						       ext2_buf);
				if (_wr < 0)
					return _wr;
				raw.size += need_rec_len;
				raw.blocks = (raw.size + block_size - 1)
					     / block_size * (block_size / 512);
				dir->i_size = raw.size;
				return ext2_write_inode(fs, data->inode_no,
							&raw);
			}

			block_off += de->rec_len;
			offset += de->rec_len;
		}

		if (block_off >= block_size)
			offset = (block_idx + 1) * block_size;
	}

	/* Need to allocate a new block for the directory */
	{
		uint32_t block_idx = raw.size / block_size;
		uint32_t phys_block = ext2_alloc_block(fs);
		if (phys_block == 0) {
			printf("ext2_add_entry: alloc_block failed\n");
			return -1;
		}

		if (ext2_set_bmap(fs, &raw, block_idx, phys_block) < 0) {
			printf("ext2_add_entry: set_bmap failed\n");
			ext2_free_block(fs, phys_block);
			return -1;
		}

		memset(ext2_buf, 0, block_size);
		struct ext2_dirent *new_de;
		new_de = (struct ext2_dirent *)ext2_buf;
		new_de->inode = entry_inode;
		new_de->rec_len = block_size;
		new_de->name_len = name_len;
		new_de->file_type = 1;
		memcpy(new_de->name, name, name_len);

		if (ext2_write_block(fs, phys_block, ext2_buf) < 0) {
			printf("ext2_add_entry: write_block failed\n");
			return -1;
		}

		raw.size += block_size;
		raw.blocks = (raw.size + block_size - 1) / block_size
			     * (block_size / 512);
		dir->i_size = raw.size;
		ext2_write_inode(fs, data->inode_no, &raw);
		return 0;
	}

	return -1;
}

static int ext2_remove_entry(struct vfs_inode *dir, const char *name)
{
	struct ext2_inode_data *data;
	struct ext2_fs *fs;
	struct ext2_inode raw;
	uint32_t block_size;
	uint32_t offset;
	int name_len = strlen(name);

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
			struct ext2_dirent *de;
			de = (struct ext2_dirent *)(ext2_buf + block_off);

			if (de->rec_len == 0)
				break;
			if (de->inode == 0) {
				block_off += de->rec_len;
				offset += de->rec_len;
				continue;
			}

			if (de->name_len == (uint32_t)name_len
			    && memcmp(de->name, name, name_len) == 0) {
				de->inode = 0;
				return ext2_write_block(fs, phys_block,
						       ext2_buf);
			}

			block_off += de->rec_len;
			offset += de->rec_len;
		}

		if (block_off >= block_size)
			offset = (block_idx + 1) * block_size;
	}

	return -1;
}

static int ext2_mkdir(struct vfs_inode *parent, const char *name)
{
	struct ext2_inode_data *pdata;
	struct ext2_fs *fs;
	uint32_t new_inode_no;
	struct ext2_inode new_raw;
	uint32_t block_size;
	uint32_t phys_block;

	pdata = (struct ext2_inode_data *)parent->private_data;
	if (!pdata)
		return -1;
	fs = pdata->fs;
	block_size = fs->block_size;

	new_inode_no = ext2_alloc_inode(fs);
	if (new_inode_no == 0)
		return -1;

	memset(&new_raw, 0, sizeof(new_raw));
	new_raw.mode = EXT2_S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	new_raw.uid = 0;
	new_raw.gid = 0;
	new_raw.links = 2;  /* . and .. plus parent's entry */
	new_raw.size = block_size;

	phys_block = ext2_alloc_block(fs);
	if (phys_block == 0) {
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}
	new_raw.block[0] = phys_block;
	new_raw.blocks = block_size / 512;

	memset(ext2_buf, 0, block_size);

	/* "." entry */
	struct ext2_dirent *de = (struct ext2_dirent *)ext2_buf;
	de->inode = new_inode_no;
	de->rec_len = 12;
	de->name_len = 1;
	de->file_type = 2; /* directory */
	de->name[0] = '.';

	/* ".." entry */
	de = (struct ext2_dirent *)(ext2_buf + 12);
	de->inode = pdata->inode_no;
	de->rec_len = block_size - 12;
	de->name_len = 2;
	de->file_type = 2;
	de->name[0] = '.';
	de->name[1] = '.';

	if (ext2_write_block(fs, phys_block, ext2_buf) < 0) {
		ext2_free_block(fs, phys_block);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}

	if (ext2_write_inode(fs, new_inode_no, &new_raw) < 0) {
		ext2_free_block(fs, phys_block);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}

	/* Add entry to parent directory using VFS-level helper */
	struct vfs_inode *new_vfs = vfs_alloc_inode();
	if (!new_vfs) {
		ext2_free_block(fs, phys_block);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}
	new_vfs->i_type = VFS_IDIR;
	new_vfs->i_size = block_size;
	new_vfs->i_no = new_inode_no;
	new_vfs->ops = &ext2_dir_ops;

	struct ext2_inode_data *child_data = ext2_alloc_data();
	if (!child_data) {
		vfs_free_inode(new_vfs);
		ext2_free_block(fs, phys_block);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}
	child_data->inode_no = new_inode_no;
	child_data->fs = fs;
	new_vfs->private_data = child_data;

	if (ext2_add_entry(parent, name, new_vfs) < 0) {
		vfs_free_inode(new_vfs);
		ext2_free_block(fs, phys_block);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}

	/* Update parent link count */
	{
		struct ext2_inode parent_raw;
		if (ext2_read_inode(fs, pdata->inode_no, &parent_raw) == 0) {
			parent_raw.links++;
			ext2_write_inode(fs, pdata->inode_no, &parent_raw);
		}
	}

	return 0;
}

static int ext2_unlink(struct vfs_inode *parent, const char *name)
{
	struct ext2_inode_data *pdata;
	struct ext2_fs *fs;
	struct ext2_inode raw, child_raw;
	uint32_t block_size, offset;
	int name_len = strlen(name);

	pdata = (struct ext2_inode_data *)parent->private_data;
	if (!pdata)
		return -1;
	fs = pdata->fs;
	block_size = fs->block_size;

	if (ext2_read_inode(fs, pdata->inode_no, &raw) < 0)
		return -1;

	/* Find entry, get child inode number */
	uint32_t child_inode_no = 0;
	offset = 0;
	while (offset < raw.size && child_inode_no == 0) {
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
			struct ext2_dirent *de;
			de = (struct ext2_dirent *)(ext2_buf + block_off);
			if (de->rec_len == 0)
				break;
			if (de->inode != 0
			    && de->name_len == (uint32_t)name_len
			    && memcmp(de->name, name, name_len) == 0) {
				child_inode_no = de->inode;
				break;
			}
			block_off += de->rec_len;
			offset += de->rec_len;
		}
		if (block_off >= block_size)
			offset = (block_idx + 1) * block_size;
	}

	if (child_inode_no == 0)
		return -1;

	/* Remove directory entry */
	if (ext2_remove_entry(parent, name) < 0)
		return -1;

	/* Decrement link count and possibly free inode */
	if (ext2_read_inode(fs, child_inode_no, &child_raw) < 0)
		return -1;

	child_raw.links--;
	if (child_raw.links == 0) {
		/* Free all data blocks */
		uint32_t total_blocks = (child_raw.size + block_size - 1)
					/ block_size;
		uint32_t i;
		for (i = 0; i < total_blocks; i++) {
			uint32_t b = ext2_inode_bmap(fs, &child_raw, i);
			if (b) {
				ext2_set_bmap(fs, &child_raw, i, 0);
				ext2_free_block(fs, b);
			}
		}
		child_raw.size = 0;
		child_raw.blocks = 0;
		ext2_write_inode(fs, child_inode_no, &child_raw);
		ext2_free_inode(fs, child_inode_no);
	} else {
		ext2_write_inode(fs, child_inode_no, &child_raw);
	}

	return 0;
}

static int ext2_rmdir(struct vfs_inode *parent, const char *name)
{
	struct ext2_inode_data *pdata;
	struct ext2_fs *fs;
	struct ext2_inode raw;
	uint32_t block_size, offset;
	int name_len = strlen(name);

	pdata = (struct ext2_inode_data *)parent->private_data;
	if (!pdata)
		return -1;
	fs = pdata->fs;
	block_size = fs->block_size;

	if (ext2_read_inode(fs, pdata->inode_no, &raw) < 0)
		return -1;

	/* Find the child inode number */
	uint32_t child_inode_no = 0;
	offset = 0;
	while (offset < raw.size && child_inode_no == 0) {
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
			struct ext2_dirent *de;
			de = (struct ext2_dirent *)(ext2_buf + block_off);
			if (de->rec_len == 0)
				break;
			if (de->inode != 0
			    && de->name_len == (uint32_t)name_len
			    && memcmp(de->name, name, name_len) == 0) {
				child_inode_no = de->inode;
				break;
			}
			block_off += de->rec_len;
			offset += de->rec_len;
		}
		if (block_off >= block_size)
			offset = (block_idx + 1) * block_size;
	}

	if (child_inode_no == 0)
		return -1;

	/* Verify it's a directory and only has . and .. */
	{
		struct ext2_inode child_raw;
		if (ext2_read_inode(fs, child_inode_no, &child_raw) < 0)
			return -1;
		if ((child_raw.mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
			return -1;
		if (child_raw.links > 2)
			return -1; /* not empty (has entries beyond . and ..) */
		/* Check for actual entries beyond . and .. */
		uint32_t child_block = ext2_inode_bmap(fs, &child_raw, 0);
		if (child_block) {
			if (ext2_read_block(fs, child_block, ext2_buf) < 0)
				return -1;
			struct ext2_dirent *de;
			de = (struct ext2_dirent *)(ext2_buf + 12); /* skip . */
			if (de->rec_len > 12 && de->inode != 0)
				return -1; /* not empty */
		}
	}

	/* Remove the directory entry */
	if (ext2_remove_entry(parent, name) < 0)
		return -1;

	/* Free child inode and its blocks */
	{
		struct ext2_inode child_raw;
		if (ext2_read_inode(fs, child_inode_no, &child_raw) < 0)
			return -1;

		uint32_t total_blocks = (child_raw.size + block_size - 1)
					/ block_size;
		uint32_t i;
		for (i = 0; i < total_blocks; i++) {
			uint32_t b = ext2_inode_bmap(fs, &child_raw, i);
			if (b) {
				ext2_set_bmap(fs, &child_raw, i, 0);
				ext2_free_block(fs, b);
			}
		}
		ext2_free_inode(fs, child_inode_no);
	}

	/* Update parent link count */
	{
		struct ext2_inode parent_raw;
		if (ext2_read_inode(fs, pdata->inode_no, &parent_raw) == 0) {
			parent_raw.links--;
			ext2_write_inode(fs, pdata->inode_no, &parent_raw);
		}
	}

	return 0;
}

static int ext2_mknod(struct vfs_inode *parent, const char *name,
		      uint16_t mode, dev_t dev)
{
	struct ext2_inode_data *pdata;
	struct ext2_fs *fs;
	uint32_t new_inode_no;
	struct ext2_inode new_raw;
	struct vfs_inode *new_vfs;
	struct ext2_inode_data *child_data;

	pdata = (struct ext2_inode_data *)parent->private_data;
	if (!pdata)
		return -1;
	fs = pdata->fs;

	new_inode_no = ext2_alloc_inode(fs);
	if (new_inode_no == 0)
		return -1;

	memset(&new_raw, 0, sizeof(new_raw));
	new_raw.mode = mode;
	new_raw.uid = 0;
	new_raw.gid = 0;
	new_raw.links = 1;
	new_raw.size = 0;
	new_raw.block[0] = dev;

	if (ext2_write_inode(fs, new_inode_no, &new_raw) < 0) {
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}

	new_vfs = vfs_alloc_inode();
	if (!new_vfs) {
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}

	child_data = ext2_alloc_data();
	if (!child_data) {
		vfs_free_inode(new_vfs);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}
	child_data->inode_no = new_inode_no;
	child_data->fs = fs;

	uint16_t file_type = mode & S_IFMT;
	if (file_type == S_IFCHR)
		new_vfs->i_type = VFS_IFCHR;
	else if (file_type == S_IFBLK)
		new_vfs->i_type = VFS_IFBLK;
	else {
		vfs_free_inode(new_vfs);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}
	new_vfs->i_no = new_inode_no;
	new_vfs->i_size = 0;
	new_vfs->i_mode = mode;
	new_vfs->i_rdev = dev;
	new_vfs->ops = &ext2_file_ops;
	new_vfs->private_data = child_data;

	return ext2_add_entry(parent, name, new_vfs);
}

static int ext2_symlink(struct vfs_inode *parent, const char *name,
			const char *target)
{
	struct ext2_inode_data *pdata;
	struct ext2_fs *fs;
	uint32_t new_inode_no;
	struct ext2_inode new_raw;
	int target_len = strlen(target);
	uint32_t block_size;

	pdata = (struct ext2_inode_data *)parent->private_data;
	if (!pdata)
		return -1;
	fs = pdata->fs;
	block_size = fs->block_size;

	new_inode_no = ext2_alloc_inode(fs);
	if (new_inode_no == 0)
		return -1;

	memset(&new_raw, 0, sizeof(new_raw));
	new_raw.mode = EXT2_S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
	new_raw.uid = 0;
	new_raw.gid = 0;
	new_raw.links = 1;
	new_raw.size = target_len;

	if (target_len <= 60) {
		/* Fast symlink: store target in block pointers */
		memcpy(new_raw.block, target, target_len);
	} else {
		/* Slow symlink: store in a data block */
		uint32_t phys_block = ext2_alloc_block(fs);
		if (phys_block == 0) {
			ext2_free_inode(fs, new_inode_no);
			return -1;
		}
		new_raw.block[0] = phys_block;
		new_raw.blocks = block_size / 512;
		memset(ext2_buf, 0, block_size);
		memcpy(ext2_buf, target, target_len);
		if (ext2_write_block(fs, phys_block, ext2_buf) < 0) {
			ext2_free_block(fs, phys_block);
			ext2_free_inode(fs, new_inode_no);
			return -1;
		}
	}

	if (ext2_write_inode(fs, new_inode_no, &new_raw) < 0) {
		if (new_raw.block[0] && target_len > 60)
			ext2_free_block(fs, new_raw.block[0]);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}

	/* Add entry to parent */
	struct vfs_inode *new_vfs = vfs_alloc_inode();
	if (!new_vfs) {
		if (new_raw.block[0] && target_len > 60)
			ext2_free_block(fs, new_raw.block[0]);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}
	new_vfs->i_type = VFS_ISYMLINK;
	new_vfs->i_size = target_len;
	new_vfs->i_no = new_inode_no;
	new_vfs->ops = &ext2_symlink_ops;

	struct ext2_inode_data *child_data = ext2_alloc_data();
	if (!child_data) {
		vfs_free_inode(new_vfs);
		if (new_raw.block[0] && target_len > 60)
			ext2_free_block(fs, new_raw.block[0]);
		ext2_free_inode(fs, new_inode_no);
		return -1;
	}
	child_data->inode_no = new_inode_no;
	child_data->fs = fs;
	new_vfs->private_data = child_data;

	return ext2_add_entry(parent, name, new_vfs);
}

static int ext2_readlink_op(struct vfs_inode *inode, char *buf,
			    uint32_t size)
{
	struct ext2_inode_data *data;
	struct ext2_fs *fs;
	struct ext2_inode raw;

	data = (struct ext2_inode_data *)inode->private_data;
	if (!data)
		return -1;
	fs = data->fs;

	if (ext2_read_inode(fs, data->inode_no, &raw) < 0)
		return -1;

	uint32_t target_len = raw.size;
	if (target_len > size - 1)
		target_len = size - 1;

	if (target_len <= 60) {
		/* Fast symlink */
		memcpy(buf, raw.block, target_len);
	} else {
		/* Slow symlink */
		uint32_t phys_block = raw.block[0];
		if (phys_block == 0)
			return -1;
		if (ext2_read_block(fs, phys_block, ext2_buf) < 0)
			return -1;
		memcpy(buf, ext2_buf, target_len);
	}

	buf[target_len] = '\0';
	return (int)target_len;
}

/* ---------------------------------------------------------------
 * chmod
 * --------------------------------------------------------------- */

static int ext2_chmod(struct vfs_inode *inode, uint16_t mode)
{
	struct ext2_inode_data *data;
	struct ext2_inode raw;

	data = (struct ext2_inode_data *)inode->private_data;
	if (!data)
		return -1;

	if (ext2_read_inode(data->fs, data->inode_no, &raw) < 0)
		return -1;

	/* Preserve file type bits, set permission bits */
	raw.mode = (raw.mode & EXT2_S_IFMT) | (mode & ~EXT2_S_IFMT);

	if (ext2_write_inode(data->fs, data->inode_no, &raw) < 0)
		return -1;

	return 0;
}

/* ---------------------------------------------------------------
 * rename
 * --------------------------------------------------------------- */

static int ext2_rename(struct vfs_inode *old_parent, const char *old_name,
		       struct vfs_inode *new_parent, const char *new_name)
{
	struct ext2_inode_data *pdata, *npdata;
	struct vfs_inode *child;
	struct ext2_inode child_raw;
	int is_dir;

	pdata = (struct ext2_inode_data *)old_parent->private_data;
	npdata = (struct ext2_inode_data *)new_parent->private_data;
	if (!pdata || !npdata)
		return -1;

	/* Look up the child */
	child = ext2_lookup(old_parent, old_name);
	if (!child)
		return -1;

	is_dir = (child->i_type == VFS_IDIR);

	/* Same name in same directory is a no-op */
	if (old_parent == new_parent && strcmp(old_name, new_name) == 0)
		return 0;

	/* Add entry to new parent first */
	if (ext2_add_entry(new_parent, new_name, child) < 0)
		return -1;

	/* Remove from old parent */
	if (ext2_remove_entry(old_parent, old_name) < 0) {
		ext2_remove_entry(new_parent, new_name);
		return -1;
	}

	/* Directory-specific updates */
	if (is_dir && old_parent != new_parent) {
		struct ext2_inode parent_raw, new_parent_raw;

		/* Decrement old parent link count */
		if (ext2_read_inode(pdata->fs, pdata->inode_no,
				    &parent_raw) == 0) {
			parent_raw.links--;
			ext2_write_inode(pdata->fs, pdata->inode_no,
					 &parent_raw);
		}

		/* Increment new parent link count */
		if (ext2_read_inode(npdata->fs, npdata->inode_no,
				    &new_parent_raw) == 0) {
			new_parent_raw.links++;
			ext2_write_inode(npdata->fs, npdata->inode_no,
					 &new_parent_raw);
		}

		/* Update ".." entry in child directory */
		uint32_t child_inode_no;
		child_inode_no = ((struct ext2_inode_data *)
				  child->private_data)->inode_no;
		if (ext2_read_inode(pdata->fs, child_inode_no,
				    &child_raw) == 0) {
			uint32_t first_block = ext2_inode_bmap(
				pdata->fs, &child_raw, 0);
			if (first_block) {
				ext2_read_block(pdata->fs, first_block,
						ext2_buf);
				struct ext2_dirent *de;
				de = (struct ext2_dirent *)
					(ext2_buf + 12);
				de->inode = npdata->inode_no;
				ext2_write_block(pdata->fs, first_block,
						 ext2_buf);
			}
		}
	}

	return 0;
}

static int ext2_link(struct vfs_inode *parent, const char *name,
		     struct vfs_inode *existing)
{
	struct ext2_inode_data *edata;
	struct ext2_fs *fs;
	struct ext2_inode raw;

	if (!parent || !name || !existing)
		return -1;

	edata = (struct ext2_inode_data *)existing->private_data;
	if (!edata)
		return -1;
	fs = edata->fs;

	/* Increment link count on existing inode */
	if (ext2_read_inode(fs, edata->inode_no, &raw) < 0)
		return -1;
	raw.links++;
	if (ext2_write_inode(fs, edata->inode_no, &raw) < 0)
		return -1;

	/* Add directory entry pointing to the same inode */
	if (ext2_add_entry(parent, name, existing) < 0) {
		/* Roll back link count increment */
		raw.links--;
		ext2_write_inode(fs, edata->inode_no, &raw);
		return -1;
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
	root->i_mode = raw_root.mode;

	return root;
}
