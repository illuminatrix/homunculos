#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stddef.h>

#define BLOCK_SECTOR_SIZE 512
#define BLOCK_MAX_DEVICES 20
#define BLOCK_NAME_LEN    16

struct block_device;

struct block_device_ops {
	int (*read_sector)(struct block_device *dev, uint32_t lba, void *buf);
	int (*write_sector)(struct block_device *dev, uint32_t lba,
			    const void *buf);
};

struct block_device {
	char name[BLOCK_NAME_LEN];
	uint32_t block_size;
	uint32_t num_blocks;
	const struct block_device_ops *ops;
	void *private_data;
};

int block_register_device(struct block_device *dev);
struct block_device *block_find_device(const char *name);
int block_read(struct block_device *dev, uint32_t lba, uint32_t count,
	       void *buf);
int block_write(struct block_device *dev, uint32_t lba, uint32_t count,
		const void *buf);

#endif
