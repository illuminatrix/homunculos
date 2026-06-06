#include "vfs.h"
#include "pio.h"
#include "block.h"
#include <string.h>

/* ATA register offsets (primary base 0x1F0, secondary base 0x170) */
#define ATA_REG_DATA(base)       (base + 0)
#define ATA_REG_ERROR(base)      (base + 1)
#define ATA_REG_SECCOUNT(base)   (base + 2)
#define ATA_REG_LBA_LO(base)     (base + 3)
#define ATA_REG_LBA_MID(base)    (base + 4)
#define ATA_REG_LBA_HI(base)     (base + 5)
#define ATA_REG_DRIVE(base)      (base + 6)
#define ATA_REG_CMD(base)        (base + 7)
#define ATA_REG_ALTSTAT(base)    (base + 0x206)

/* ATA commands */
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC

/* Status register bits */
#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

/* Drive / head register */
#define ATA_DRIVE_MASTER 0xA0
#define ATA_DRIVE_SLAVE  0xB0
#define ATA_LBA_MODE     0xE0

struct ata_channel {
	uint16_t base;
	uint16_t ctrl;
	uint8_t  irq;
};

static const struct ata_channel ata_channels[2] = {
	{ 0x1F0, 0x3F6, 14 },
	{ 0x170, 0x376, 15 },
};

#define ATA_MAX_DRIVES 4
static struct ata_priv {
	uint16_t base;
	uint8_t  slave;
} ata_drives[ATA_MAX_DRIVES];

static struct block_device ata_bdevs[ATA_MAX_DRIVES];

/* --------------------------------------------------------------- */

static void io_delay(void)
{
	volatile int i;
	for (i = 0; i < 100; i++)
		(void)i;
}

static int ata_poll_bsy(uint16_t base)
{
	int timeout = 10000;
	while ((in(ATA_REG_CMD(base)) & ATA_SR_BSY) && timeout > 0) {
		io_delay();
		timeout--;
	}
	return timeout;
}

static int ata_poll_drq(uint16_t base)
{
	int timeout = 10000;
	while (!(in(ATA_REG_CMD(base)) & ATA_SR_DRQ) && timeout > 0) {
		io_delay();
		timeout--;
	}
	return timeout;
}

/* --------------------------------------------------------------- */

static int ata_read_sector(struct block_device *dev, uint32_t lba, void *buf)
{
	struct ata_priv *priv = (struct ata_priv *)dev->private_data;
	uint16_t base = priv->base;
	int i;

	ata_poll_bsy(base);

	out(ATA_REG_DRIVE(base), ATA_LBA_MODE | (priv->slave << 4) |
	    ((lba >> 24) & 0x0F));
	out(ATA_REG_SECCOUNT(base), 1);
	out(ATA_REG_LBA_LO(base), lba & 0xFF);
	out(ATA_REG_LBA_MID(base), (lba >> 8) & 0xFF);
	out(ATA_REG_LBA_HI(base), (lba >> 16) & 0xFF);
	out(ATA_REG_CMD(base), ATA_CMD_READ_SECTORS);

	if (ata_poll_drq(base) == 0)
		return -1;

	for (i = 0; i < 256; i++)
		((uint16_t *)buf)[i] = inw(ATA_REG_DATA(base));

	return 0;
}

static int ata_write_sector(struct block_device *dev, uint32_t lba,
			    const void *buf)
{
	struct ata_priv *priv = (struct ata_priv *)dev->private_data;
	uint16_t base = priv->base;
	int i;

	ata_poll_bsy(base);

	out(ATA_REG_DRIVE(base), ATA_LBA_MODE | (priv->slave << 4) |
	    ((lba >> 24) & 0x0F));
	out(ATA_REG_SECCOUNT(base), 1);
	out(ATA_REG_LBA_LO(base), lba & 0xFF);
	out(ATA_REG_LBA_MID(base), (lba >> 8) & 0xFF);
	out(ATA_REG_LBA_HI(base), (lba >> 16) & 0xFF);
	out(ATA_REG_CMD(base), ATA_CMD_WRITE_SECTORS);

	if (ata_poll_drq(base) == 0)
		return -1;

	for (i = 0; i < 256; i++)
		outw(ATA_REG_DATA(base), ((const uint16_t *)buf)[i]);

	out(ATA_REG_CMD(base), 0xE7);
	ata_poll_bsy(base);

	return 0;
}

static struct block_device_ops ata_ops = {
	.read_sector  = ata_read_sector,
	.write_sector = ata_write_sector,
};

/* --------------------------------------------------------------- */

static int ata_detect(uint16_t base, uint8_t slave, uint32_t *sector_count)
{
	int i;
	uint16_t ident[256];

	out(ATA_REG_DRIVE(base), 0xA0 | (slave << 4));
	out(ATA_REG_SECCOUNT(base), 0);
	out(ATA_REG_LBA_LO(base), 0);
	out(ATA_REG_LBA_MID(base), 0);
	out(ATA_REG_LBA_HI(base), 0);

	out(ATA_REG_CMD(base), ATA_CMD_IDENTIFY);

	if (in(ATA_REG_CMD(base)) == 0)
		return -1;

	if (ata_poll_bsy(base) == 0)
		return -1;

	if (in(ATA_REG_CMD(base)) & ATA_SR_ERR)
		return -1;

	if (ata_poll_drq(base) == 0)
		return -1;

	for (i = 0; i < 256; i++)
		ident[i] = inw(ATA_REG_DATA(base));

	*sector_count = (uint32_t)ident[60] | ((uint32_t)ident[61] << 16);
	return 0;
}

/* --------------------------------------------------------------- */

void ata_init(void)
{
	int c, s;
	int idx = 0;

	for (c = 0; c < 2 && idx < ATA_MAX_DRIVES; c++) {
		for (s = 0; s < 2 && idx < ATA_MAX_DRIVES; s++) {
			uint32_t sector_count;

			if (ata_detect(ata_channels[c].base, s,
				       &sector_count) < 0)
				continue;

			ata_drives[idx].base  = ata_channels[c].base;
			ata_drives[idx].slave = s;

			ata_bdevs[idx].block_size = 512;
			ata_bdevs[idx].num_blocks = sector_count;
			ata_bdevs[idx].ops = &ata_ops;
			ata_bdevs[idx].private_data = &ata_drives[idx];

			ata_bdevs[idx].name[0] = 'h';
			ata_bdevs[idx].name[1] = 'd';
			ata_bdevs[idx].name[2] = 'a' + idx;
			ata_bdevs[idx].name[3] = '\0';

			block_register_device(&ata_bdevs[idx]);
			idx++;
		}
	}
}

VFS_DRIVER_INIT(ata_init);
