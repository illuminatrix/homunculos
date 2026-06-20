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
		kernel_pdir[j] = ((uint32_t)kernel_pt[j]) | (MM_PRESENT | MM_RW | MM_USER);
		for (i = 0; i < DIR_SIZE; i++)
			kernel_pt[j][i] = ((j * DIR_SIZE + i) * FRAME) | (MM_PRESENT | MM_RW | MM_USER);
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

static int clone_page_table(uint32_t parent_pde, uint32_t *new_pde_out)
{
	uint32_t *parent_pt = (uint32_t *)(parent_pde & ~0xFFF);
	uint32_t *new_pt = (uint32_t *)mm_frame_alloc();
	if (!new_pt)
		return -1;

	for (uint32_t j = 0; j < DIR_SIZE; j++) {
		if (parent_pt[j] & MM_PRESENT) {
			void *new_frame = mm_frame_alloc();
			if (!new_frame) {
				for (uint32_t k = 0; k < j; k++) {
					if (new_pt[k] & MM_PRESENT)
						mm_frame_free((void *)(new_pt[k] & ~0xFFF));
				}
				mm_frame_free(new_pt);
				return -1;
			}
			memcpy(new_frame, (void *)(parent_pt[j] & ~0xFFF), FRAME);
			new_pt[j] = ((uint32_t)new_frame) | (parent_pt[j] & 0xFFF);
		} else {
			new_pt[j] = parent_pt[j];
		}
	}

	*new_pde_out = ((uint32_t)new_pt) | (parent_pde & 0xFFF);
	return 0;
}

static void free_cloned_range(uint32_t *pdir, uint32_t start, uint32_t end)
{
	for (uint32_t i = start; i < end; i++) {
		if (!(pdir[i] & MM_PRESENT))
			continue;
		uint32_t *pt = (uint32_t *)(pdir[i] & ~0xFFF);
		for (uint32_t j = 0; j < DIR_SIZE; j++) {
			if (pt[j] & MM_PRESENT)
				mm_frame_free((void *)(pt[j] & ~0xFFF));
		}
		mm_frame_free(pt);
	}
}

uint32_t *mm_clone_pdir(uint32_t *parent_pdir)
{
	uint32_t *new_pdir = (uint32_t *)mm_frame_alloc();
	if (!new_pdir)
		return 0;

	memcpy(new_pdir, parent_pdir, 4 * sizeof(uint32_t));

	for (uint32_t i = 4; i < DIR_SIZE; i++) {
		if (parent_pdir[i] & MM_PRESENT) {
			uint32_t new_pde;
			if (clone_page_table(parent_pdir[i], &new_pde) < 0) {
				free_cloned_range(new_pdir, 4, i);
				mm_frame_free(new_pdir);
				return 0;
			}
			new_pdir[i] = new_pde;
		} else {
			new_pdir[i] = parent_pdir[i];
		}
	}

	return new_pdir;
}

int mm_map_at(uint32_t *pdir, uint32_t va, uint32_t pa, uint32_t flags)
{
	uint32_t pd_idx = va >> 22;
	uint32_t pt_idx = (va >> 12) & 0x3FF;
	uint32_t *pt;

	if (!(pdir[pd_idx] & MM_PRESENT)) {
		uint32_t *new_pt = (uint32_t *)mm_frame_alloc();
		if (!new_pt)
			return -1;
		memset(new_pt, 0, FRAME);
		pdir[pd_idx] = ((uint32_t)new_pt) | MM_PRESENT | MM_RW | MM_USER;
	}

	pt = (uint32_t *)(pdir[pd_idx] & ~0xFFF);

	if (pt[pt_idx] & MM_PRESENT)
		return -1;

	pt[pt_idx] = (pa & ~0xFFF) | flags;

	{
		uint32_t cr3;
		asm volatile("mov %%cr3, %0" : "=r"(cr3));
		if (cr3 == (uint32_t)pdir)
			asm volatile("invlpg %0" :: "m"(*(uint32_t *)va));
	}

	return 0;
}

uint32_t mm_alloc_at(uint32_t *pdir, uint32_t va, uint32_t flags)
{
	void *frame = mm_frame_alloc();
	if (!frame)
		return 0;

	memset(frame, 0, FRAME);

	if (mm_map_at(pdir, va, (uint32_t)frame, flags) < 0) {
		mm_frame_free(frame);
		return 0;
	}

	return va;
}

void mm_invlpg(uint32_t *pdir, uint32_t va)
{
	uint32_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	if (cr3 == (uint32_t)pdir)
		asm volatile("invlpg %0" :: "m"(*(uint32_t *)va));
}

int mm_unmap_at(uint32_t *pdir, uint32_t va)
{
	uint32_t pd_idx = va >> 22;
	uint32_t pt_idx = (va >> 12) & 0x3FF;
	uint32_t *pt;

	if (!(pdir[pd_idx] & MM_PRESENT))
		return -1;

	pt = (uint32_t *)(pdir[pd_idx] & ~0xFFF);
	if (!(pt[pt_idx] & MM_PRESENT))
		return -1;

	mm_frame_free((void *)(pt[pt_idx] & ~0xFFF));
	pt[pt_idx] = 0;

	mm_invlpg(pdir, va);
	return 0;
}

uint32_t mm_get_pte(uint32_t *pdir, uint32_t va)
{
	uint32_t pd_idx = va >> 22;
	uint32_t pt_idx = (va >> 12) & 0x3FF;

	if (!(pdir[pd_idx] & MM_PRESENT))
		return 0;

	uint32_t *pt = (uint32_t *)(pdir[pd_idx] & ~0xFFF);
	return pt[pt_idx];
}
