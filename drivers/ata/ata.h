#ifndef ATA_H
#define ATA_H

#include "block.h"

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

/* Detect and initialise ATA drives on all channels.
 * Registers detected drives as block devices named "hda", "hdb", etc. */
void ata_init(void);

#endif
