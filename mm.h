#ifndef MM_H
#define MM_H

#include <stdint.h>
#include <stddef.h>
#include "kernel.h"

#define FRAME 0x1000
#define DIR_SIZE 1<<10
#define MAX_MEMORY_BYTES (256 * 1024 * 1024)
#define MAX_FRAMES (MAX_MEMORY_BYTES / FRAME)
#define FRAME_ALLOC_BITMAP_SIZE (MAX_FRAMES / 32)

void turn_on_paging(void);
void init_mm(mmap_entry_t *, uint32_t);
void setup_identity_paging(void);

void *mm_frame_alloc(void);
void mm_frame_free(void *addr);

uint32_t *mm_clone_pdir(uint32_t *parent_pdir);

extern uint32_t kernel_pdir[DIR_SIZE];
extern uint32_t kernel_pt[][DIR_SIZE];

#endif
