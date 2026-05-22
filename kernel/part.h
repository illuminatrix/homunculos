#ifndef PART_H
#define PART_H

#include <stdint.h>

/* MBR partition table entry (16 bytes) */
struct mbr_entry {
	uint8_t  status;
	uint8_t  chs_start[3];
	uint8_t  type;
	uint8_t  chs_end[3];
	uint32_t lba_start;
	uint32_t sector_count;
} __attribute__((packed));

/* MBR at LBA 0 */
#define MBR_PART_TABLE_OFFSET 446
#define MBR_SIGNATURE_OFFSET  510
#define MBR_SIGNATURE         0xAA55

void part_init(void);

#endif
