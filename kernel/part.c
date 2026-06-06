#include "part.h"
#include "block.h"
#include "vfs.h"
#include "devtmpfs.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Partition block device: wraps a parent block device with an LBA
 * offset so the partition appears as an independent block device.
 * --------------------------------------------------------------*/

#define MAX_PARTITIONS 16

struct part_priv {
	struct block_device *parent;
	uint32_t start_lba;
};

static struct block_device part_bdevs[MAX_PARTITIONS];
static struct part_priv part_privs[MAX_PARTITIONS];
static int part_count;

static int part_read_sector(struct block_device *dev, uint32_t lba,
			    void *buf)
{
	struct part_priv *priv = (struct part_priv *)dev->private_data;
	return priv->parent->ops->read_sector(priv->parent,
					      priv->start_lba + lba, buf);
}

static int part_write_sector(struct block_device *dev, uint32_t lba,
			     const void *buf)
{
	struct part_priv *priv = (struct part_priv *)dev->private_data;
	return priv->parent->ops->write_sector(priv->parent,
					       priv->start_lba + lba, buf);
}

static struct block_device_ops part_ops = {
	.read_sector  = part_read_sector,
	.write_sector = part_write_sector,
};

/* ----------------------------------------------------------------
 * Parse MBR on a block device and register partition block devices.
 * --------------------------------------------------------------*/

static void parse_mbr(struct block_device *parent)
{
	uint8_t mbr[BLOCK_SECTOR_SIZE];
	struct mbr_entry *entries;
	int i;

	if (parent->block_size < BLOCK_SECTOR_SIZE)
		return;
	if (block_read(parent, 0, 1, mbr) < 0)
		return;

	/* Check MBR signature */
	uint16_t sig = mbr[MBR_SIGNATURE_OFFSET] |
		       (mbr[MBR_SIGNATURE_OFFSET + 1] << 8);
	if (sig != MBR_SIGNATURE)
		return;

	entries = (struct mbr_entry *)(mbr + MBR_PART_TABLE_OFFSET);

	for (i = 0; i < 4; i++) {
		if (entries[i].type == 0 || entries[i].sector_count == 0)
			continue;
		if (part_count >= MAX_PARTITIONS)
			break;

		struct part_priv *priv = &part_privs[part_count];
		struct block_device *bdev = &part_bdevs[part_count];

		priv->parent = parent;
		priv->start_lba = entries[i].lba_start;

		memset(bdev, 0, sizeof(*bdev));
		bdev->block_size = BLOCK_SECTOR_SIZE;
		bdev->num_blocks = entries[i].sector_count;
		bdev->ops = &part_ops;
		bdev->private_data = priv;

		/* Build name: "hda1", "hda2", ... */
		int plen = strlen(parent->name);
		int k;
		for (k = 0; k < plen && k < BLOCK_NAME_LEN - 3; k++)
			bdev->name[k] = parent->name[k];
		bdev->name[k] = '0' + (i + 1);
		bdev->name[k + 1] = '\0';

		if (block_register_device(bdev) == 0)
			part_count++;
	}
}

/* ----------------------------------------------------------------
 * VFS file-level ops for block devices.
 * Translates byte-offset read/write to sector-based I/O.
 * --------------------------------------------------------------*/

static int block_file_read(struct file *f, void *buf, size_t nbyte)
{
	struct block_device *dev = (struct block_device *)f->private_data;
	uint32_t offset = f->pos;
	uint32_t dev_bytes = dev->num_blocks * BLOCK_SECTOR_SIZE;
	uint8_t *dst = (uint8_t *)buf;
	uint32_t total = 0;
	uint32_t todo;

	if (offset >= dev_bytes)
		return 0;
	if ((uint32_t)(offset + nbyte) > dev_bytes)
		nbyte = dev_bytes - offset;

	todo = nbyte;

	/* Partial first sector */
	if (offset % BLOCK_SECTOR_SIZE != 0) {
		uint8_t sector[BLOCK_SECTOR_SIZE];
		uint32_t lba = offset / BLOCK_SECTOR_SIZE;
		uint32_t skip = offset % BLOCK_SECTOR_SIZE;
		uint32_t chunk = BLOCK_SECTOR_SIZE - skip;

		if (chunk > todo)
			chunk = todo;
		if (block_read(dev, lba, 1, sector) < 0)
			return total > 0 ? (int)total : -1;
		memcpy(dst, sector + skip, chunk);
		dst += chunk;
		total += chunk;
		todo -= chunk;
	}

	/* Whole sectors */
	while (todo >= BLOCK_SECTOR_SIZE) {
		uint32_t lba = offset / BLOCK_SECTOR_SIZE + total / BLOCK_SECTOR_SIZE;
		if (block_read(dev, lba, 1, dst) < 0)
			return (int)total;
		dst += BLOCK_SECTOR_SIZE;
		total += BLOCK_SECTOR_SIZE;
		todo -= BLOCK_SECTOR_SIZE;
	}

	/* Partial last sector */
	if (todo > 0) {
		uint8_t sector[BLOCK_SECTOR_SIZE];
		uint32_t lba = offset / BLOCK_SECTOR_SIZE + total / BLOCK_SECTOR_SIZE;
		if (block_read(dev, lba, 1, sector) < 0)
			return (int)total;
		memcpy(dst, sector, todo);
		total += todo;
	}

	return (int)total;
}

static int block_file_write(struct file *f, const void *buf, size_t nbyte)
{
	struct block_device *dev = (struct block_device *)f->private_data;
	uint32_t offset = f->pos;
	uint32_t dev_bytes = dev->num_blocks * BLOCK_SECTOR_SIZE;
	const uint8_t *src = (const uint8_t *)buf;
	uint32_t total = 0;
	uint32_t todo;

	if (offset >= dev_bytes)
		return 0;
	if ((uint32_t)(offset + nbyte) > dev_bytes)
		nbyte = dev_bytes - offset;

	todo = nbyte;

	/* Partial first sector: read-modify-write */
	if (offset % BLOCK_SECTOR_SIZE != 0) {
		uint8_t sector[BLOCK_SECTOR_SIZE];
		uint32_t lba = offset / BLOCK_SECTOR_SIZE;
		uint32_t skip = offset % BLOCK_SECTOR_SIZE;
		uint32_t chunk = BLOCK_SECTOR_SIZE - skip;

		if (chunk > todo)
			chunk = todo;
		if (block_read(dev, lba, 1, sector) < 0)
			return total > 0 ? (int)total : -1;
		memcpy(sector + skip, src, chunk);
		if (block_write(dev, lba, 1, sector) < 0)
			return total > 0 ? (int)total : -1;
		src += chunk;
		total += chunk;
		todo -= chunk;
	}

	/* Whole sectors */
	while (todo >= BLOCK_SECTOR_SIZE) {
		uint32_t lba = offset / BLOCK_SECTOR_SIZE + total / BLOCK_SECTOR_SIZE;
		if (block_write(dev, lba, 1, src) < 0)
			return (int)total;
		src += BLOCK_SECTOR_SIZE;
		total += BLOCK_SECTOR_SIZE;
		todo -= BLOCK_SECTOR_SIZE;
	}

	/* Partial last sector: read-modify-write */
	if (todo > 0) {
		uint8_t sector[BLOCK_SECTOR_SIZE];
		uint32_t lba = offset / BLOCK_SECTOR_SIZE + total / BLOCK_SECTOR_SIZE;
		if (block_read(dev, lba, 1, sector) < 0)
			return (int)total;
		memcpy(sector, src, todo);
		if (block_write(dev, lba, 1, sector) < 0)
			return (int)total;
		total += todo;
	}

	return (int)total;
}

static struct vfs_ops block_dev_file_ops = {
	.read  = block_file_read,
	.write = block_file_write,
};

/* ----------------------------------------------------------------
 * Register a block device as a VFS device node under /dev/.
 * --------------------------------------------------------------*/

static void register_block_vfs(struct block_device *bdev)
{
	devtmpfs_register_vfs(bdev->name, &block_dev_file_ops, bdev);
}

/* ----------------------------------------------------------------
 * part_init — called after ATA init and before ext2 mount.
 * Parses MBRs on all registered block devices, creates partition
 * block devices, and registers every block device as a VFS node.
 * --------------------------------------------------------------*/

void part_init(void)
{
	char names[4][4] = {"hda", "hdb", "hdc", "hdd"};
	int i;

	/* Parse MBR on each ATA disk */
	for (i = 0; i < 4; i++) {
		struct block_device *dev = block_find_device(names[i]);
		if (dev)
			parse_mbr(dev);
	}

	/* Re-scan: register whole-disk devices as VFS nodes */
	for (i = 0; i < 4; i++) {
		struct block_device *dev = block_find_device(names[i]);
		if (dev)
			register_block_vfs(dev);
	}

	/* Register partition devices as VFS nodes */
	for (i = 0; i < part_count; i++)
		register_block_vfs(&part_bdevs[i]);
}

VFS_DRIVER_INIT(part_init);
