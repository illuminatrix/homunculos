#ifndef MM_H
#define MM_H

#include <stdint.h>
#include <stddef.h>
#include "kernel.h"

#define FRAME 0x1000
#define DIR_SIZE (1<<10)
#define MAX_MEMORY_BYTES (256 * 1024 * 1024)
#define MAX_FRAMES (MAX_MEMORY_BYTES / FRAME)
#define FRAME_ALLOC_BITMAP_SIZE (MAX_FRAMES / 32)

/* Page table entry flags */
#define MM_PRESENT  0x001
#define MM_RW       0x002
#define MM_USER     0x004

void turn_on_paging(void);
void init_mm(mmap_entry_t *, uint32_t);
void setup_identity_paging(void);

void *mm_frame_alloc(void);
void mm_frame_free(void *addr);

uint32_t *mm_clone_pdir(uint32_t *parent_pdir);

/* Map existing physical page at virtual address in given page directory */
int mm_map_at(uint32_t *pdir, uint32_t va, uint32_t pa, uint32_t flags);

/* Allocate a frame and map it at virtual address in given page directory */
uint32_t mm_alloc_at(uint32_t *pdir, uint32_t va, uint32_t flags);

extern uint32_t kernel_pdir[DIR_SIZE];
extern uint32_t kernel_pt[][DIR_SIZE];

#endif
