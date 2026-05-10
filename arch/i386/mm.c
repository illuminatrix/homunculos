#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "mm.h"

/* Identity-map 16MB (4 PTs, each covering 4MB) */
#define NUM_PT 4
#define IDENT_PAGES (NUM_PT * DIR_SIZE)

uint32_t kernel_pdir[DIR_SIZE] __attribute__((aligned(FRAME)));
uint32_t kernel_pt[NUM_PT][DIR_SIZE] __attribute__((aligned(FRAME)));

static uint32_t frame_bitmap[IDENT_PAGES / 32];

extern uint32_t __kernel_end;

void setup_identity_paging(void)
{
	uint32_t i, j;

	for (j = 0; j < NUM_PT; j++) {
		kernel_pdir[j] = ((uint32_t)kernel_pt[j]) | 0x3;
		for (i = 0; i < DIR_SIZE; i++)
			kernel_pt[j][i] = ((j * DIR_SIZE + i) * FRAME) | 0x3;
	}

	turn_on_paging();
}

static inline void bitmap_set(uint32_t frame)
{
	frame_bitmap[frame / 32] |= (1 << (frame % 32));
}

static inline void bitmap_clear(uint32_t frame)
{
	frame_bitmap[frame / 32] &= ~(1 << (frame % 32));
}

static inline int bitmap_test(uint32_t frame)
{
	return (frame_bitmap[frame / 32] >> (frame % 32)) & 1;
}

static void frame_bitmap_init(mmap_entry_t *mmap_addr, uint32_t len)
{
	uint32_t i;
	uint32_t addr;
	mmap_entry_t *entry;

	memset(frame_bitmap, 0xFF, sizeof(frame_bitmap));

	addr = (uint32_t)mmap_addr;
	while (addr < (uint32_t)mmap_addr + len) {
		entry = (mmap_entry_t *)addr;
		if (entry->type == MMAP_MEMORY_AVAILABLE) {
			uint32_t start = (uint32_t)(entry->addr / FRAME);
			uint32_t end = (uint32_t)((entry->addr + entry->len) / FRAME);
			if (end > IDENT_PAGES)
				end = IDENT_PAGES;
			for (i = start; i < end; i++)
				bitmap_clear(i);
		}
		addr += entry->size + 4;
	}

	uint32_t kend = ((uint32_t)&__kernel_end + FRAME - 1) / FRAME;
	for (i = 0; i < kend; i++)
		bitmap_set(i);
}

void init_mm(mmap_entry_t *mmap_addr, uint32_t len)
{
	setup_identity_paging();
	frame_bitmap_init(mmap_addr, len);
}

void turn_on_paging(void)
{
	__asm__ volatile ("mov %0, %%eax\n\t"
			  "mov %%eax, %%cr3\n\t"
			  "mov %%cr0, %%eax\n\t"
			  "or $0x80000000, %%eax\n\t"
			  "mov %%eax, %%cr0\n"
			  :: "p" (kernel_pdir)
			  : "ax");
}

void *mm_frame_alloc(void)
{
	uint32_t i;
	for (i = 0; i < IDENT_PAGES; i++) {
		if (!bitmap_test(i)) {
			bitmap_set(i);
			return (void *)(i * FRAME);
		}
	}
	return 0;
}

void mm_frame_free(void *addr)
{
	uint32_t frame = (uint32_t)addr / FRAME;
	if (frame < IDENT_PAGES)
		bitmap_clear(frame);
}

uint32_t *mm_clone_pdir(uint32_t *parent_pdir)
{
	uint32_t i, j;
	uint32_t *new_pdir;
	uint32_t *new_pt;
	uint32_t *parent_pt;
	void *new_frame;
	void *parent_frame;

	new_pdir = (uint32_t *)mm_frame_alloc();
	if (!new_pdir)
		return 0;

	for (i = 0; i < DIR_SIZE; i++) {
		if (parent_pdir[i] & 1) {
			new_pt = (uint32_t *)mm_frame_alloc();
			if (!new_pt) {
				for (j = 0; j < i; j++) {
					if (new_pdir[j] & 1) {
						uint32_t *pt = (uint32_t *)(new_pdir[j] & ~0xFFF);
						for (uint32_t k = 0; k < DIR_SIZE; k++) {
							if (pt[k] & 1)
								mm_frame_free((void *)(pt[k] & ~0xFFF));
						}
						mm_frame_free(pt);
					}
				}
				mm_frame_free(new_pdir);
				return 0;
			}

			parent_pt = (uint32_t *)(parent_pdir[i] & ~0xFFF);
			for (j = 0; j < DIR_SIZE; j++) {
				if (parent_pt[j] & 1) {
					new_frame = mm_frame_alloc();
					parent_frame = (void *)(parent_pt[j] & ~0xFFF);
					if (!new_frame) {
						for (uint32_t k = 0; k < j; k++) {
							if (new_pt[k] & 1)
								mm_frame_free((void *)(new_pt[k] & ~0xFFF));
						}
						mm_frame_free(new_pt);
						for (uint32_t k = 0; k < i; k++) {
							if (new_pdir[k] & 1) {
								uint32_t *pt2 = (uint32_t *)(new_pdir[k] & ~0xFFF);
								for (uint32_t l = 0; l < DIR_SIZE; l++) {
									if (pt2[l] & 1)
										mm_frame_free((void *)(pt2[l] & ~0xFFF));
								}
								mm_frame_free(pt2);
							}
						}
						mm_frame_free(new_pdir);
						return 0;
					}
					memcpy(new_frame, parent_frame, FRAME);
					new_pt[j] = ((uint32_t)new_frame) | (parent_pt[j] & 0xFFF);
				} else {
					new_pt[j] = parent_pt[j];
				}
			}
			new_pdir[i] = ((uint32_t)new_pt) | (parent_pdir[i] & 0xFFF);
		} else {
			new_pdir[i] = parent_pdir[i];
		}
	}

	return new_pdir;
}
