#include "block.h"
#include <string.h>

static struct block_device block_devices[BLOCK_MAX_DEVICES];
static int block_device_count;

int block_register_device(struct block_device *dev)
{
	if (block_device_count >= BLOCK_MAX_DEVICES || !dev)
		return -1;
	block_devices[block_device_count] = *dev;
	block_device_count++;
	return 0;
}

struct block_device *block_find_device(const char *name)
{
	int i;
	if (!name)
		return 0;
	for (i = 0; i < block_device_count; i++) {
		if (strncmp(block_devices[i].name, name,
			    BLOCK_NAME_LEN - 1) == 0)
			return &block_devices[i];
	}
	return 0;
}

int block_read(struct block_device *dev, uint32_t lba, uint32_t count,
	       void *buf)
{
	uint32_t i;
	uint8_t *p = (uint8_t *)buf;
	if (!dev || !dev->ops || !dev->ops->read_sector)
		return -1;
	for (i = 0; i < count; i++) {
		if (dev->ops->read_sector(dev, lba + i, p) < 0)
			return -1;
		p += BLOCK_SECTOR_SIZE;
	}
	return 0;
}

int block_write(struct block_device *dev, uint32_t lba, uint32_t count,
		const void *buf)
{
	uint32_t i;
	const uint8_t *p = (const uint8_t *)buf;
	if (!dev || !dev->ops || !dev->ops->write_sector)
		return -1;
	for (i = 0; i < count; i++) {
		if (dev->ops->write_sector(dev, lba + i, p) < 0)
			return -1;
		p += BLOCK_SECTOR_SIZE;
	}
	return 0;
}
